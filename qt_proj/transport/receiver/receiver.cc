#include "transport/receiver/receiver.h"

#include <cassert>
#include <ranges>

#include "tylib/time/timer.h"
#include "rsfec/rsfec.h"

#include "pc/peer_connection.h"
// #include "rtp/rtcp/rtcp_nack.h"
#include "rtp/rtp_handler.h"

RtpReceiver::RtpReceiver(SSRCInfo& ssrcInfo) : belongingSSRCInfo_(ssrcInfo) {}

// TODO: do NACK
// void RtpReceiver::PushToJitter(RtpBizPacket&& rtpBizPacket) {
//   rtpBizPacket.enterJitterTimeMs = g_now_ms;
//   jitterBuffer_.emplace(rtpBizPacket.GetPowerSeq(), std::move(rtpBizPacket));
//   assert(rtpBizPacket.rtpRawPacket.empty());
// }

const std::string kAllComplete = "AllComplete";
const std::string kNotCompleteCanRecover = "NotCompleteCanRecover";
const std::string kNotCompleteCannotRecover = "NotCompleteCannotRecover";

// 1, data is all complete, no need recover
// 2, data is not complete, but can recover
// 3, data is not complete, connot recover
std::string RtpReceiver::shoudDoDecodeFEC(const std::map<PowerSeqT, RtpBizPacket> &group) {
  assert(!group.empty());

  int recvFecNum{};
  int recvDataNum{};
  int expectDataNum{};

  // PowerSeqT expectSeq = this->lastPoppedPowerSeq_ == kShitRecvPowerSeqInitValue ? group->begin()->first : this->lastPoppedPowerSeq_ + 1;
  for (auto it = group.begin(); it != group.end(); ++it) {
    const RtpHeader& rtp = *reinterpret_cast<const RtpHeader*>(it->second.rtpRawPacket.data());
    if (rtp.isFEC()) {
      ++recvFecNum;
    } else {
      ++recvDataNum;
    }
  }

}

// @brief io_group [in & out] is a group of data and FEC
void RtpReceiver::TryRecoverFEC(std::map<PowerSeqT, RtpBizPacket> *io_group) {

  for (auto const& [seq, pkt] : *io_group | std::views::reverse) {
    tylog("seq=%lld, pkt=%s.", seq, pkt.ToString().data());
  }

  rsfec::CRSFec fec;
  int ret = fec.SetNM(1, 1);
  if (ret) {
    return;
  }

}

std::vector<RtpBizPacket> RtpReceiver::PushAndPop(RtpBizPacket&& rtpBizPacket) {
  rtpBizPacket.enterJitterTimeMs = g_now_ms;

  const RtpHeader& rtp =
      *reinterpret_cast<const RtpHeader*>(rtpBizPacket.rtpRawPacket.data());
  // only for test FEC, fec pkt may lost !!!
  // OPT: cooperate with NACK
  if (rtp.isFEC() && rtp.getMarker() == 1) {
    std::map<PowerSeqT, RtpBizPacket> groupBeforeRecover;
    for (auto it = jitterBuffer_.begin();
         it != jitterBuffer_.end() && it->first < rtpBizPacket.GetPowerSeq();) {
      tylog("pop from jitter rtp=%s.", it->second.ToString().data());

      groupBeforeRecover.emplace(it->first, std::move(it->second));
      assert(it->second.rtpRawPacket.empty());

      it = jitterBuffer_.erase(it);
    }
    groupBeforeRecover.emplace(rtpBizPacket.GetPowerSeq(),  std::move(rtpBizPacket) );

    tylog("now jitter=%s.", ToString().data());
    assert(rtpBizPacket.rtpRawPacket.empty());

    TryRecoverFEC(&groupBeforeRecover);

    std::vector<RtpBizPacket> recoveredGroup;
    for (auto & [_, pkt] : groupBeforeRecover) {
      recoveredGroup.emplace_back(std::move(pkt));
    }

    return recoveredGroup;
  } else {
    jitterBuffer_.emplace(rtpBizPacket.GetPowerSeq(), std::move(rtpBizPacket));
    assert(rtpBizPacket.rtpRawPacket.empty());

    return {};
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
