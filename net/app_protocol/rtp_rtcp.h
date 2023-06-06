#ifndef _RTP_H_
#define _RTP_H_

#include <string>
#include <vector>
//#include "rtp-packet.h"
//#include "rtcp-packet.h"
extern "C"
{
#include "rtcp.h"
}

const int RTP_RTCP_MTU = 1500;

class RtpExtensionsHeaderOneByte // for audio level
{
public:
	RtpExtensionsHeaderOneByte();
	virtual ~RtpExtensionsHeaderOneByte();
	int encode(uint8_t *buf, int size);

public:
	uint8_t id;
	uint8_t value;
};


class RtpHeader
{
public:
	uint8_t version;
	uint8_t p;
	uint8_t x;
	uint8_t cc;
	uint8_t m;
	uint8_t pt;
	uint16_t seq;
	uint32_t ts;
	uint32_t ssrc;
	uint32_t csrc[16];
	RtpExtensionsHeaderOneByte extension;

	uint32_t encode(uint8_t *buf, int size);
};

class RtpPacket
{
public:
	RtpPacket();
	virtual ~RtpPacket();

public:
	void setRtpVersion(uint8_t version);
	void setRtpPayloadType(uint8_t payloadType);
	uint8_t getRtpPayloadType() const { return header.pt; }
	void setRtpSSRC(uint32_t ssrc);
	uint32_t getRtpSSRC() const { return header.ssrc; }
	void setRtpTimeStamp(uint32_t ts);
	uint32_t getRtpTimeStamp() const { return header.ts; }
	void setRtpSeq(uint16_t seq);
	uint16_t getRtpSeq() const { return header.seq; }
	void setRtpExtension(uint8_t extensionflag, RtpExtensionsHeaderOneByte &extension);
	void setRtpPadding(uint8_t padding);
	void setRtpCSRCCount(uint8_t csrc);
	void setRtpMarker(uint8_t marker);
	uint8_t getRtpMarker() { return header.m; }
	void setRtpPayload(uint8_t *data, uint32_t len);
	uint8_t* getRtpPayload(uint32_t &len);
	uint32_t encode(uint8_t *buf, int size);
	int decode(uint8_t *data, int len);
	int decodePs(uint8_t *data, int len);

private:
	RtpHeader header;
	uint8_t *rtpData;
	uint32_t rtpDataLen;
};

class RtcpHeader
{
public:
	uint8_t version;
	uint8_t p;
	uint8_t ic_fmt;
	uint8_t pt;
	uint16_t length;

	uint32_t encode(uint8_t *buf, int size);
	uint32_t decode(uint8_t *data, int len);
};

class RtcpPacket
{
public:
	RtcpPacket();
	RtcpPacket(uint8_t *data, int len);
	virtual ~RtcpPacket();

public:
	int updateRtcpCtx(rtcp_context *rtcp_ctx);

public:
	uint32_t getRtcpRecvSSRC();
	uint32_t getRtcpSendSSRC();
	uint32_t getRtcpBitrate();
	std::vector<uint16_t> getRtcpNacks();
	bool is_pli();
	bool is_fir();

public:
	void rtcpAddSR(uint32_t ssrc, uint32_t ts, uint32_t packets_sent, uint32_t bytes_sent);
	void rtcpAddRR(uint32_t ssrc, rtcp_context *rtcp_ctx);
	void rtcpAddNacks(std::vector<uint16_t> nacks);
	void rtcpAddRemb(uint32_t bitrate);
	//uint8_t* getRtcpData(uint32_t &len);
	uint32_t encode(uint8_t *buf, int size);

public:
	uint8_t dir;

private:
	uint8_t *dataptr;
	int datalen;

};


#endif