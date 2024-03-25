#include "transport/receiver/receiver.h"

#include <cassert>
#include <numeric>
#include <ranges>

#include "pc/peer_connection.h"
#include "rsfec/rsfec.h"
#include "tylib/time/timer.h"
// #include "rtp/rtcp/rtcp_nack.h"
#include "rtp/rtp_handler.h"

RtpReceiver::RtpReceiver(SSRCInfo& ssrcInfo) : belongingSSRCInfo_(ssrcInfo) {}

// TODO: do NACK
// void RtpReceiver::PushToJitter(RtpBizPacket&& rtpBizPacket) {
//   rtpBizPacket.enterJitterTimeMs = g_now_ms;
//   jitterBuffer_.emplace(rtpBizPacket.GetPowerSeq(), std::move(rtpBizPacket));
//   assert(rtpBizPacket.rtpRawPacket.empty());
// }

// have no CSRC, not use extension, use user defined:
// wide is 32B
// fec header:
// +++++++++++++++++++++++++++++++++++++++++++++++++++++
// | FEC number            |     FEC index (0-based)   |
// +++++++++++++++++++++++++++++++++++++++++++++++++++++
// |        protected ssrc                             |
// +++++++++++++++++++++++++++++++++++++++++++++++++++++
// | protected first seq   | protected last seq        |
// +++++++++++++++++++++++++++++++++++++++++++++++++++++
// | protected data pkt len ...         | length is (last - first + 1) * 2B
// +++++++++++++++++++++++++++++++++++++++++++++++++++++
// |    FEC payload ...                        |
// +++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// FEC number:  一组FEC一共几个包
// FEC index:  FEC组里当前的index
// protect ssrc:  保护的媒体的ssrc
// first protect seq number 和 last protect seq number:
// 这个FEC包是哪几个媒体包生成的
class FecHeader {
  uint16_t fecNum_;
  uint16_t fecIndex_;
  uint32_t protectedSSRC_;
  uint16_t protectedFirstSeq_;
  uint16_t protectedLastSeq_;

 public:
  uint16_t fecNum() const { return ntohs(fecNum_); }
  uint16_t fecIndex() const { return ntohs(fecIndex_); }
  uint32_t protectedSSRC() const { return ntohl(protectedSSRC_); }
  uint16_t protectedFirstSeq() const { return ntohs(protectedFirstSeq_); }
  uint16_t protectedLastSeq() const { return ntohs(protectedLastSeq_); }

  const char* getFecPayloadBegin() const {
    return reinterpret_cast<const char*>(this) + sizeof *this +
           2 * (protectedLastSeq() - protectedFirstSeq() + 1);
  }

  int getProtectedDataNum() const {
    return ((1 << 16) + protectedLastSeq() - protectedFirstSeq() + 1) %
           (1 << 16);
  }

  std::vector<int> computeProtectedDataLenList() const {
    const uint16_t* dataLenBegin = reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const char*>(this) + sizeof *this);
    const uint16_t* dataLenEnd =
        dataLenBegin + (protectedLastSeq() - protectedFirstSeq() + 1);

    // should not use assert if hack me :)
    assert(dataLenBegin < dataLenEnd);

    std::vector<int> dataLenList;
    for (const uint16_t* p = dataLenBegin; p != dataLenEnd; ++p) {
      dataLenList.push_back(ntohs(*p));
    }

    tylog("dataLenList=%s.", tylib::AnyToString(dataLenList).data());

    return dataLenList;
  }

  std::string ToString() const {
    return tylib::format_string(
        "{fecNum=%d, fecIndex=%d, protectedSSRC=%d, protectedFirstSeq=%d, "
        "protectedLastSeq=%d => protectedDataNum=%d}",
        fecNum(), fecIndex(), protectedSSRC(), protectedFirstSeq(),
        protectedLastSeq(), getProtectedDataNum());
  }
};

// 1, data is all complete, no need do recover
// 2, data is not complete, but can recover
// 3, data is not complete, connot recover
const std::string kAllComplete = "AllComplete";
const std::string kNotCompleteCanRecover = "NotCompleteCanRecover";
const std::string kNotCompleteCannotRecover = "NotCompleteCannotRecover";

// @brief group [in] a group of data and FEC
std::vector<RtpBizPacket> RtpReceiver::TryRecoverFEC(
    std::map<PowerSeqT, RtpBizPacket>& group) {
  assert(!group.empty());

  int recvFecNum{};
  int recvDataNum{};

  auto firstFecIt = group.end();

  // PowerSeqT expectSeq = this->lastPoppedPowerSeq_ ==
  // kShitRecvPowerSeqInitValue ? group->begin()->first :
  // this->lastPoppedPowerSeq_ + 1;
  for (auto it = group.begin(); it != group.end(); ++it) {
    tylog("powerSeq=%lld, pkt=%s.", it->first, it->second.ToString().data());

    const RtpHeader& rtp =
        *reinterpret_cast<const RtpHeader*>(it->second.rtpRawPacket.data());
    if (rtp.isFEC()) {
      const FecHeader& fecHeader =
          *reinterpret_cast<const FecHeader*>(rtp.getHeaderExt());
      tylog("it's %dth FEC: powerSeq=%d, fec=%s.", recvFecNum, it->first,
            fecHeader.ToString().data());

      ++recvFecNum;

      if (firstFecIt == group.end()) {
        firstFecIt = it;
      }
    } else {
      ++recvDataNum;
    }
  }

  tylog("recvDataNum=%d, recvFecNum=%d.", recvDataNum, recvFecNum);

  if (recvFecNum == 0) {
    // bug: if first data pkt is lost, should check in outer function
    assert(!"not recv fec, todo");
    return {};
  } else {
    assert(firstFecIt != group.end());

    const FecHeader& firstFecHeader = *reinterpret_cast<const FecHeader*>(
        firstFecIt->second.rtpRawPacket.data() + kRtpHeaderLenByte);
    int expectDataNum = firstFecHeader.getProtectedDataNum();
    int expectFecNum = firstFecHeader.fecNum();

    tylog("recvDataNum=%d, expectDataNum=%d, recvAll=%zu.", recvDataNum,
          expectDataNum, group.size());
    assert(recvDataNum <= expectDataNum);

    if (recvDataNum == expectDataNum && recvFecNum == expectFecNum) {
      tylog("result=%s.", kAllComplete.data());

      // only for test!!!
      rsfec::CRSFec fec;
      fec.SetVandermondeMatrix();
      const int N = expectDataNum;
      int ret = fec.SetNM(N, firstFecHeader.fecNum());
      if (ret) {
        tylog("fec set NM ret=%d.", ret);

        return {};
      }

      // arg 1
      auto dataLenList = firstFecHeader.computeProtectedDataLenList();
      const int pkgSize =
          *std::max_element(dataLenList.begin(), dataLenList.end());
      tylog("fec pkgSize=%d.", pkgSize);

      // arg 2
      std::vector<int> inputIds(N - 1);
      std::iota(inputIds.begin(), inputIds.end(), 1);
      tylog("input ids=%s.", tylib::AnyToString(inputIds).data());

      // arg 3
      std::vector<int> outputIds = {0};

      // arg 4
      std::vector<void*> bufferIn(N);
      std::vector<char> oneData(pkgSize);
      std::vector<std::vector<char>> supplement(N);
      bufferIn[0] = oneData.data();

      auto it = ++group.begin();
      for (int i = 1; i < N; ++i, ++it) {
        if (it->second.rtpRawPacket.size() == pkgSize) {
          bufferIn[i] = it->second.rtpRawPacket.data();
        } else {
          assert(static_cast<int>(it->second.rtpRawPacket.size()) < pkgSize);

          supplement[i] = it->second.rtpRawPacket;
          supplement[i].resize(pkgSize);
          bufferIn[i] = supplement[i].data();
        }
      }
      assert(it == firstFecIt);

      // arg 5
      std::vector<void*> bufferOut = {
          const_cast<char*>(firstFecHeader.getFecPayloadBegin())};
      assert(bufferOut.size() == 1);

      ret = fec.RecoveryFEC(pkgSize, inputIds, outputIds, bufferIn, bufferOut);
      if (ret) {
        tylog("recover fec ret=%d.", ret);
        return {};
      }

      assert(dataLenList.front() <= pkgSize);
      oneData.resize(dataLenList.front());

      assert(oneData.size() == group.begin()->second.rtpRawPacket.size());
      assert(oneData == group.begin()->second.rtpRawPacket);

      std::vector<RtpBizPacket> v;
      v.push_back({std::move(oneData), group.begin()->second.cycle});
      tylog("recover a data=%s.", tylib::AnyToString(v).data());
      for (auto it = ++group.begin(); it != firstFecIt; ++it) {
        v.push_back(std::move(it->second));
      }
      assert(static_cast<int>(v.size()) == expectDataNum);
      return v;
    } else if (expectDataNum <= group.size()) {
      tylog("result=%s.", kNotCompleteCanRecover.data());

      rsfec::CRSFec fec;
      fec.SetVandermondeMatrix();
      const int N = expectDataNum;
      int ret = fec.SetNM(N, firstFecHeader.fecNum());
      if (ret) {
        tylog("fec set NM ret=%d.", ret);
        assert(!"set NM should not fail :)");

        return {};
      }

      // arg 1
      auto dataLenList = firstFecHeader.computeProtectedDataLenList();
      const int pkgSize =
          *std::max_element(dataLenList.begin(), dataLenList.end());
      tylog("fec pkgSize=%d.", pkgSize);

      // arg 2

      return {};  // TODO
    } else {
      tylog("result=%s.", kNotCompleteCannotRecover.data());
      assert(!"todo");
      return {};
    }
  }
}

std::vector<RtpBizPacket> RtpReceiver::PushAndPop(RtpBizPacket&& rtpBizPacket) {
  rtpBizPacket.enterJitterTimeMs = g_now_ms;

  const RtpHeader& rtp = rtpBizPacket.GetRtpHeaderRef();
  // only for test FEC, fec pkt may lost !!!
  // OPT: cooperate with NACK
  if (rtp.isFEC() && rtp.getMarker() == 1) {
    std::map<PowerSeqT, RtpBizPacket> recvGroup;
    for (auto it = jitterBuffer_.begin();
         it != jitterBuffer_.end() && it->first < rtpBizPacket.GetPowerSeq();) {
      tylog("pop from jitter rtp=%s.", it->second.ToString().data());

      if (it->second.GetRtpHeaderRef().getTimestamp() == rtp.getTimestamp()) {
        recvGroup.emplace(it->first, std::move(it->second));
        assert(it->second.rtpRawPacket.empty());
      } else {
        tylog("warning: the above pkt ts(%u) != fec(%u), discard it.",
              it->second.GetRtpHeaderRef().getTimestamp(), rtp.getTimestamp());
      }

      it = jitterBuffer_.erase(it);
    }

    tylog("add current pkt rtp=%s.", rtpBizPacket.ToString().data());
    recvGroup.emplace(rtpBizPacket.GetPowerSeq(), std::move(rtpBizPacket));
    assert(rtpBizPacket.rtpRawPacket.empty());

    lastPoppedPowerSeq_ = rtp.getSeqNumber();
    lastPoppedTs_ = rtp.getTimestamp();

    tylog("now receiver=%s.", ToString().data());

    // check tS
    // NOTE: ORIGINAL RTP HAS BEEN MOVED, WHILE ADDRESS IS SAME. The usage is
    // dangerous.
    auto ts = rtp.getTimestamp();
    for (const auto& [_, pkt] : recvGroup) {
      if (pkt.GetRtpHeaderRef().getTimestamp() != ts) {
        tylog("pkt ts is not %u: pkt=%s.", ts, pkt.ToString().data());
        assert(!"ts is not same");
      }
    }

    return TryRecoverFEC(recvGroup);
  } else {
    jitterBuffer_.emplace(rtpBizPacket.GetPowerSeq(), std::move(rtpBizPacket));
    assert(rtpBizPacket.rtpRawPacket.empty());

    const RtpBizPacket& firstRtp = jitterBuffer_.begin()->second;
    tylog("jitter firstRtp=%s.", firstRtp.ToString().data());
    const uint32_t firstTs = firstRtp.GetRtpHeaderRef().getTimestamp();
    const int kUnorderWaitMs = 50;
    // OPT: check if data packets are complete, no need wait fec last pkt
    if (rtp.getTimestamp() == firstTs ||
        firstRtp.WaitTimeMs() < kUnorderWaitMs) {
      return {};
    }

    std::map<PowerSeqT, RtpBizPacket> recvGroup;
    for (auto it = jitterBuffer_.begin();
         it->second.GetRtpHeaderRef().getTimestamp() == firstTs;) {
      lastPoppedPowerSeq_ = it->second.GetRtpHeaderRef().getSeqNumber();
      lastPoppedTs_ = it->second.GetRtpHeaderRef().getTimestamp();

      recvGroup.emplace(it->first, std::move(it->second));
      it = jitterBuffer_.erase(it);
      assert(it != jitterBuffer_.end());
    }

    return TryRecoverFEC(recvGroup);
  }
}

int RtpReceiver::GetJitterSize() const { return jitterBuffer_.size(); }

std::string RtpReceiver::ToString() const {
  return tylib::format_string("{jitterSize=%zu, lastPoppedPowerSeq_=%s}",
                              jitterBuffer_.size(),
                              PowerSeqToString(lastPoppedPowerSeq_).data());
}

/*
// OPT: use not wait stategy
std::vector<RtpBizPacket> RtpReceiver::PopOrderedPackets() {
  std::vector<RtpBizPacket> orderedPackets;

  for (auto it = jitterBuffer_.begin();
       it != jitterBuffer_.end() &&
       (kShitRecvPowerSeqInitValue == lastPoppedPowerSeq_ ||
        it->first == lastPoppedPowerSeq_ + 1);) {
    tylog("pop from jitter rtp=%s.", it->second.ToString().data());
    lastPoppedPowerSeq_ = it->first;

    orderedPackets.emplace_back(std::move(it->second));
    assert(it->second.rtpRawPacket.empty());

    it = jitterBuffer_.erase(it);
  }

  // OPT: move constant to config
  if (jitterBuffer_.size() >= 10000) {
    std::string err = tylib::format_string(
        "bug, jitterBuffer_ size too large=%zu.", jitterBuffer_.size());
    tylog("%s", err.data());
    assert(!"jitter buffer size too large");
  }

  tylog("orderedPackets size=%zu, jitter size=%zu.", orderedPackets.size(),
        jitterBuffer_.size());

  if (!orderedPackets.empty()) {
    return orderedPackets;
  }

  assert(!jitterBuffer_.empty());  // already push to jitter

  const RtpBizPacket& firstPacket = jitterBuffer_.begin()->second;
  const RtpHeader& firstRtpHeader =
      *reinterpret_cast<const RtpHeader*>(firstPacket.rtpRawPacket.data());
  // const bool bAudioType = firstRtpHeader.GetMediaType() == kMediaTypeAudio;
  tylog(
      "jitter detect out-of-order or lost, cannot out packets, last out "
      "packet powerSeq=%" PRId64 "[%s], jitter.size=%zu, jitter first rtp=%s.",
      lastPoppedPowerSeq_, PowerSeqToString(lastPoppedPowerSeq_).data(),
      jitterBuffer_.size(), firstPacket.ToString().data());

  const int64_t waitMs = firstPacket.WaitTimeMs();

  // const int64_t kRtcpRoundTripTimeMs = 40;

  // OPT: move constant to config, related to RTT
  const int64_t kNackTimeMs = 0;
  // const int64_t kPLITimeMs = kRtcpRoundTripTimeMs + 5;
  const int64_t kPLITimeMs = 300;
  // buffer packet len, OPT: distinguish audio and video
  const size_t kJitterMaxSize = 15;

  if (waitMs < kNackTimeMs) {
    tylog(
        "We wait short time %ldms, do nothing. Sometime not real lost, just "
        "out-of-order.",
        waitMs);

    return {};
  }

  /*
  if (kNackTimeMs <= waitMs && waitMs < kPLITimeMs &&
      jitterBuffer_.size() <= kJitterMaxSize) {
    tylog("wait %ldms, to nack", waitMs);
    std::set<int> nackSeqs;
    // should req all not received packets in jitter
    for (int i = lastPoppedPowerSeq_ + 1; i < firstPacket.GetPowerSeq(); ++i) {
      nackSeqs.insert(SplitPowerSeq(i).second);
    }
    tylog("uplink this=%s, firstPacket=%s, nack seqs=%s.", ToString().data(),
          firstPacket.ToString().data(), tylib::AnyToString(nackSeqs).data());
    assert(!nackSeqs.empty());

    const uint32_t kSelfRtcpSSRC = 1;
    const uint32_t kMediaSrcSSRC =
        bAudioType ? this->belongingSSRCInfo_.belongingRtpHandler.upAudioSSRC
                   : this->belongingSSRCInfo_.belongingRtpHandler.upVideoSSRC;
    assert(0 != kMediaSrcSSRC &&
           kMediaSrcSSRC == firstRtpHeader.getSSRC());  // already recv

    int ret = this->belongingSSRCInfo_.belongingRtpHandler
                  .belongingPeerConnection_.rtcpHandler_.CreateNackReportSend(
                      nackSeqs, kSelfRtcpSSRC, kMediaSrcSSRC);

    if (ret) {
      tylog("createNackReportSend ret=%d", ret);

      return {};
    }

  }

  // wait too long for audio, pop all
  if (bAudioType) {
    tylog("audio wait too long %ldms, not wait, pop all", waitMs);
    // OPT: should pop only first, and pop remaining in normal way
    for (auto it = jitterBuffer_.begin(); it != jitterBuffer_.end();) {
      tylog("pop from jitter rtp=%s.", it->second.ToString().data());
      lastPoppedPowerSeq_ = it->first;

      orderedPackets.emplace_back(std::move(it->second));
      assert(it->second.rtpRawPacket.empty());

      it = jitterBuffer_.erase(it);
    }
    return orderedPackets;
  }

  // wait too long for video, clear and do PLI
  tylog("PLI, first packet waitMs=%ld too long, packet=%s.", waitMs,
        firstPacket.ToString().data());

  const uint32_t kSelfRtcpSSRC = 1;
  const uint32_t kMediaSrcSSRC =
      belongingSSRCInfo_.belongingRtpHandler.upVideoSSRC;
  assert(0 != kMediaSrcSSRC);
  int ret = belongingSSRCInfo_.belongingRtpHandler.belongingPeerConnection_
                .rtcpHandler_.CreatePLIReportSend(kSelfRtcpSSRC, kMediaSrcSSRC);
  if (ret) {
    tylog("createPLIReportSend ret=%d", ret);
    // not return
  }

  jitterBuffer_.clear();
  lastPoppedPowerSeq_ = kShitRecvPowerSeqInitValue;

  return {};
}
*/
