#include "rtp_rtcp.h"
#include <sys/time.h>
#include "string.h"
#include "netio.h"

RtpExtensionsHeaderOneByte::RtpExtensionsHeaderOneByte()
{

}

RtpExtensionsHeaderOneByte::~RtpExtensionsHeaderOneByte()
{

}

int RtpExtensionsHeaderOneByte::encode(uint8_t *buf, int size)
{
	uint32_t offset = 0;
	uint8_t id_len = 0;
	uint16_t len = 0;

	offset += write_uint16(buf, 0xBEDE);
	len += 2;
	int padding_count = (len % 4 == 0) ? 0 : (4 - len % 4);
	len += padding_count;
	offset += write_uint16(buf + offset, len / 4);
	id_len = (id & 0x0F) << 4 | 0x01;
	offset += write_uint8(buf + offset, id_len);
	offset += write_uint8(buf + offset, value);
	if (padding_count) 
	{
		memset(buf+offset, 0, padding_count);
		offset += padding_count;
	}
	return offset;
}

uint32_t RtpHeader::encode(uint8_t *buf, int size)
{
	uint8_t i = 0;
	uint8_t value8 = 0;
	uint32_t offset = 0;

	value8 = ((version << 6) & 0xC0) | ((p << 5) & 0x20) | ((x << 4) & 0x10) | (cc & 0x0F);

	offset += write_uint8(buf, value8);

	value8 = ((pt & 0x7F) | (m << 7));

	offset += write_uint8(buf + offset, value8);

	offset += write_uint16(buf + offset, seq);

	offset += write_uint32(buf + offset, ts);

	offset += write_uint32(buf + offset, ssrc);

	for (i = 0; i < cc; ++i) {
		if (offset > size - sizeof(uint32_t)) {
			offset = 0;
			break;
		}
		offset += write_uint32(buf + offset, csrc[i]);
	}
	if (x != 0)
	{
		offset += extension.encode(buf + offset, size - offset);
	}
	return offset;
}

RtpPacket::RtpPacket()
{
	rtpData = new uint8_t[RTP_RTCP_MTU];
	rtpDataLen = 0;
}

RtpPacket::~RtpPacket()
{
	if (rtpData != nullptr)
	{
		delete[] rtpData;
	}
	rtpData = nullptr;
}

void RtpPacket::setRtpVersion(uint8_t version)
{
	header.version = version;
}

void RtpPacket::setRtpPayloadType(uint8_t payloadType)
{
	header.pt = payloadType;
}

void RtpPacket::setRtpSSRC(uint32_t ssrc)
{
	header.ssrc = ssrc;
}

void RtpPacket::setRtpTimeStamp(uint32_t ts)
{
	header.ts = ts;
}

void RtpPacket::setRtpSeq(uint16_t seq)
{
	header.seq = seq;
}

void RtpPacket::setRtpExtension(uint8_t extensionflag, RtpExtensionsHeaderOneByte &extension)
{
	header.x = extensionflag;
	header.extension = extension;
}

void RtpPacket::setRtpPadding(uint8_t padding)
{
	header.p = padding;
}

void RtpPacket::setRtpCSRCCount(uint8_t csrc)
{
	header.cc = csrc;
}

void RtpPacket::setRtpMarker(uint8_t marker)
{
	header.m = marker;
}

void RtpPacket::setRtpPayload(uint8_t *data, uint32_t len)
{
	memcpy(rtpData, data, len);
	rtpDataLen = len;
}

uint8_t* RtpPacket::getRtpPayload(uint32_t &len)
{
	//len = rtp_packet_get_header_len(packet) + payloadLen;
	//return static_cast<uint8_t*>(rtp_packet_get_data(packet));
	len = rtpDataLen;
	return rtpData;
}

uint32_t RtpPacket::encode(uint8_t *buf, int size)
{
	uint8_t rtp_header[32];
	int headerLen;
	headerLen = header.encode(rtp_header, sizeof(rtp_header));
	memcpy(buf, rtp_header, headerLen);
	memcpy(buf + headerLen, rtpData, rtpDataLen);
	return headerLen + rtpDataLen;
	//memmove(rtpData + headerLen, rtpData, rtpDataLen);
	//memcpy(rtpData, rtp_header, headerLen);
	//rtpDataLen += headerLen;
}

int RtpPacket::decode(uint8_t *data, int len)
{
	uint32_t offset = 0;
	uint8_t i = 0;
	uint8_t value8 = 0;
	uint16_t bede = 0;

	offset += read_uint8(data + offset, &value8);
	header.version = ((value8 & 0xC0) >> 6);
	header.p = ((value8 & 0x20) >> 5);
	header.x = ((value8 & 0x10) >> 4);
	header.cc = (value8 & 0xF);

	if (len < (sizeof(uint32_t) * header.cc + 12))
		return -1;

	offset += read_uint8(data + offset, &value8);
	header.m = ((value8 & 0x80) >> 7);
	header.pt = (value8 & 0x7F);

	offset += read_uint16(data + offset, &(header.seq));
	offset += read_uint32(data + offset, &(header.ts));
	offset += read_uint32(data + offset, &(header.ssrc));
	for (i = 0; i < header.cc; ++i) {
		offset += read_uint32(data + offset, &(header.csrc[i]));
	}
	// Extensions
	read_uint16(data + offset, &bede);
	if (bede == 0xBEDE)
	{
		offset += 2;
		uint16_t extlen;
		offset += read_uint16(data + offset, &extlen);
		offset += extlen * 4;
	}
	memcpy(rtpData, data + offset, len - offset);
	rtpDataLen = (len - offset);
	return 0;
}

int RtpPacket::decodePs(uint8_t *data, int len)
{
	uint32_t offset = 0;
	uint8_t i = 0;
	uint8_t value8 = 0;
	uint16_t bede = 0;

	offset += read_uint8(data + offset, &value8);
	header.version = ((value8 & 0xC0) >> 6);
	header.p = ((value8 & 0x20) >> 5);
	header.x = ((value8 & 0x10) >> 4);
	header.cc = (value8 & 0xF);

	if (len < (sizeof(uint32_t) * header.cc + 12))
		return -1;

	offset += read_uint8(data + offset, &value8);
	header.m = ((value8 & 0x80) >> 7);
	header.pt = (value8 & 0x7F);

	offset += read_uint16(data + offset, &(header.seq));
	offset += read_uint32(data + offset, &(header.ts));
	offset += read_uint32(data + offset, &(header.ssrc));
	//for (i = 0; i < header.cc; ++i) {
	//	offset += read_uint32(data + offset, &(header.csrc[i]));
	//}
	memcpy(rtpData, data + offset, len - offset);
	rtpDataLen = (len - offset);
	return 0;
}

uint32_t RtcpHeader::encode(uint8_t *buf, int size)
{
	uint8_t value8 = 0;
	uint32_t offset = 0;

	value8 = ((version << 6) & 0xC0) | ((p << 5) & 0x20) | (ic_fmt & 0x1F);

	offset += write_uint8(buf, value8);

	value8 = pt;

	offset += write_uint8(buf + offset, value8);

	offset += write_uint16(buf + offset, length);

	return offset;
}

uint32_t RtcpHeader::decode(uint8_t *data, int len)
{
	uint32_t offset = 0;
	uint8_t i = 0;
	uint8_t value8 = 0;

	offset += read_uint8(data + offset, &value8);
	version = ((value8 & 0xC0) >> 6);
	p = ((value8 & 0x20) >> 5);
	ic_fmt = (value8 & 0x1F);
	offset += read_uint8(data + offset, &pt);
	offset += read_uint16(data + offset, &length);
	return offset;
}

RtcpPacket::RtcpPacket()
{
	dataptr = new uint8_t[RTP_RTCP_MTU];
	datalen = 0;
	dir = 0;
}

RtcpPacket::RtcpPacket(uint8_t *data, int len)
{
	dataptr = new uint8_t[RTP_RTCP_MTU];
	memcpy(dataptr, data, len);
	datalen = len;
	dir = 1;
}

RtcpPacket::~RtcpPacket()
{
	if (dataptr)
	{
		delete[] dataptr;
	}
	dataptr = nullptr;
}

int RtcpPacket::updateRtcpCtx(rtcp_context *rtcp_ctx)
{
	return rtcp_parse(rtcp_ctx, (char*)dataptr, datalen);
}

uint32_t RtcpPacket::getRtcpRecvSSRC()
{
	return rtcp_get_receiver_ssrc((char*)dataptr, datalen);
}

uint32_t RtcpPacket::getRtcpSendSSRC()
{
	return rtcp_get_sender_ssrc((char*)dataptr, datalen);
}

uint32_t RtcpPacket::getRtcpBitrate()
{
	return rtcp_get_remb((char*)dataptr, datalen);
}

std::vector<uint16_t> RtcpPacket::getRtcpNacks()
{
	GSList *nacks = rtcp_get_nacks((char*)dataptr, datalen);
	guint nacks_count = g_slist_length(nacks);
	std::vector<uint16_t> seqs;
	if (nacks_count)
	{
		GSList *list = nacks;
		while (list) 
		{
			uint16_t seqnr = GPOINTER_TO_UINT(list->data);
			seqs.push_back(seqnr);
			list = list->next;
		}
	}
	return seqs;
}

bool RtcpPacket::is_pli()
{
	return rtcp_has_pli((char*)dataptr, datalen);
}

bool RtcpPacket::is_fir()
{
	return rtcp_has_fir((char*)dataptr, datalen);
}

void RtcpPacket::rtcpAddSR(uint32_t ssrc, uint32_t ts, uint32_t packets_sent, uint32_t bytes_sent)
{
	memset(dataptr, 0, RTP_RTCP_MTU);
	rtcp_sr *sr = (rtcp_sr*)dataptr;
	int srlen = 28;
	sr->header.version = 2;
	sr->header.type = RTCP_SR;
	sr->header.rc = 0;
	sr->header.length = htons((srlen / 4) - 1);
	sr->ssrc = ssrc;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	uint32_t s = tv.tv_sec + 2208988800u;
	uint32_t u = tv.tv_usec;
	uint32_t f = (u << 12) + (u << 8) - ((u * 3650) >> 6);
	sr->si.ntp_ts_msw = htonl(s);
	sr->si.ntp_ts_lsw = htonl(f);
	sr->si.rtp_ts = ts;
	sr->si.s_packets = packets_sent;
	sr->si.s_octets = bytes_sent;
	datalen = srlen;
}

void RtcpPacket::rtcpAddRR(uint32_t ssrc, rtcp_context *rtcp_ctx)
{
	memset(dataptr, 0, RTP_RTCP_MTU);
	rtcp_rr *rr = (rtcp_rr *)dataptr;
	int rrlen = 32;
	rr->header.version = 2;
	rr->header.type = RTCP_RR;
	rr->header.rc = 1;
	rr->header.length = htons((rrlen / 4) - 1);
	rr->ssrc = htonl(ssrc);
	rtcp_report_block(rtcp_ctx, &rr->rb[0]);
	datalen = rrlen;
}

void RtcpPacket::rtcpAddNacks(std::vector<uint16_t> nacks)
{
	memset(dataptr, 0, RTP_RTCP_MTU);
	GSList *list = NULL;
	for (int i = 0; i < nacks.size(); i++)
	{
		list = g_slist_append(list, GUINT_TO_POINTER(nacks[i]));
	}
	datalen = rtcp_nacks((char*)dataptr, RTP_RTCP_MTU, list);
}

void RtcpPacket::rtcpAddRemb(uint32_t bitrate)
{
	memset(dataptr, 0, RTP_RTCP_MTU);
	datalen = rtcp_remb((char*)dataptr, RTP_RTCP_MTU, bitrate);
}

//uint8_t* RtcpPacket::getRtcpData(uint32_t &len)
//{
//	len = datalen;
//	return dataptr;
//}

uint32_t RtcpPacket::encode(uint8_t *buf, int size)
{
	memcpy(buf, dataptr, datalen);
	return datalen;
}

