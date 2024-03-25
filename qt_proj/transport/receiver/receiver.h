#ifndef TRANSPORT_RECEIVER_RECEIVER_H_
#define TRANSPORT_RECEIVER_RECEIVER_H_

#include <set>
#include <vector>

#include "rtp/rtp_parser.h"

struct SSRCInfo;

const PowerSeqT kShitRecvPowerSeqInitValue = -1;
const uint32_t kShitRecvTimestampInitValue = -1;

class RtpReceiver {
 public:
  explicit RtpReceiver(SSRCInfo& ssrcInfo);

  // void PushToJitter(RtpBizPacket&& rtpBizPacket);
  // std::vector<RtpBizPacket> PopOrderedPackets();
  // std::vector<RtpBizPacket> PopOrderedPacketsV2() ;
  std::vector<RtpBizPacket> PushAndPop(RtpBizPacket&& rtpBizPacket);
  int GetJitterSize() const;

  std::vector<RtpBizPacket> TryRecoverFEC(
      std::map<PowerSeqT, RtpBizPacket>& group);

  std::string ToString() const;

 public:
  SSRCInfo& belongingSSRCInfo_;

  // must be ordered, cannot be hashmap
  std::map<PowerSeqT, RtpBizPacket> jitterBuffer_;

  PowerSeqT lastPoppedPowerSeq_ = kShitRecvPowerSeqInitValue;
  uint32_t lastPoppedTs_ = kShitRecvTimestampInitValue;
};

#endif  //   TRANSPORT_RECEIVER_RECEIVER_H_
