/*! \file    rtcp.c
 * \author   Lorenzo Miniero <lorenzo@meetecho.com>
 * \copyright GNU General Public License v3
 * \brief    RTCP processing
 * \details  Implementation (based on the oRTP structures) of the RTCP
 * messages. RTCP messages coming through the server are parsed and,
 * if needed (according to http://tools.ietf.org/html/draft-ietf-straw-b2bua-rtcp-00),
 * fixed before they are sent to the peers (e.g., to fix SSRCs that may
 * have been changed by the server). Methods to generate FIR messages
 * and generate/cap REMB messages are provided as well.
 *
 * \ingroup protocols
 * \ref protocols
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
//#include <bits/time.h>
#include <sys/time.h>
#include <sys/types.h>
#include "rtcp.h"

/* Transport CC statuses */
typedef enum rtp_packet_status {
	rtp_packet_status_notreceived = 0,
	rtp_packet_status_smalldelta = 1,
	rtp_packet_status_largeornegativedelta = 2,
	rtp_packet_status_reserved = 3
} rtp_packet_status;
static const char *rtp_packet_status_description(rtp_packet_status status) {
	switch(status) {
		case rtp_packet_status_notreceived: return "notreceived";
		case rtp_packet_status_smalldelta: return "smalldelta";
		case rtp_packet_status_largeornegativedelta: return "largeornegativedelta";
		case rtp_packet_status_reserved: return "reserved";
		default: break;
	}
	return NULL;
}

guint32 push_bits(guint32 word, size_t num, guint32 val) {
	return (word << num) | (val & (0xFFFFFFFF >> (32 - num)));
}

void set1(guint8 *data, size_t i, guint8 val) {
	data[i] = val;
}

void set2(guint8 *data, size_t i, guint32 val) {
	data[i + 1] = (guint8)(val);
	data[i] = (guint8)(val >> 8);
}

void set3(guint8 *data, size_t i, guint32 val) {
	data[i + 2] = (guint8)(val);
	data[i + 1] = (guint8)(val >> 8);
	data[i] = (guint8)(val >> 16);
}

void set4(guint8 *data, size_t i, guint32 val) {
	data[i + 3] = (guint8)(val);
	data[i + 2] = (guint8)(val >> 8);
	data[i + 1] = (guint8)(val >> 16);
	data[i] = (guint8)(val >> 24);
}


gint64 get_monotonic_time(void) {
	struct timespec ts;
	clock_gettime(1, &ts);
	return (ts.tv_sec*G_GINT64_CONSTANT(1000000)) + (ts.tv_nsec / G_GINT64_CONSTANT(1000));
}

gboolean is_rtcp(char *buf, guint len) {
	return (len >= 12) && (buf[0] & 0x80) && (buf[1] >= 192 && buf[1] <= 223);
}

int rtcp_parse(rtcp_context *ctx, char *packet, int len) {
	return rtcp_fix_ssrc(ctx, packet, len, 0, 0, 0);
}

guint32 rtcp_get_sender_ssrc(char *packet, int len) {
	if(packet == NULL || len == 0)
		return 0;
	rtcp_header *rtcp = (rtcp_header *)packet;
	int pno = 0, total = len;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			break;
		if(rtcp->version != 2)
			break;
		pno++;
		switch(rtcp->type) {
			case RTCP_SR: {
				/* SR, sender report */
				rtcp_sr *sr = (rtcp_sr *)rtcp;
				return ntohl(sr->ssrc);
			}
			case RTCP_RR: {
				/* RR, receiver report */
				rtcp_rr *rr = (rtcp_rr *)rtcp;
				return ntohl(rr->ssrc);
			}
			case RTCP_RTPFB: {
				/* RTPFB, Transport layer FB message (rfc4585) */
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				return ntohl(rtcpfb->ssrc);
			}
			case RTCP_PSFB: {
				/* PSFB, Payload-specific FB message (rfc4585) */
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				return ntohl(rtcpfb->ssrc);
			}
			case RTCP_XR: {
				/* XR, extended reports (rfc3611) */
				rtcp_xr *xr = (rtcp_xr *)rtcp;
				return ntohl(xr->ssrc);
			}
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0) {
			break;
		}
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return 0;
}

guint32 rtcp_get_receiver_ssrc(char *packet, int len) {
	if(packet == NULL || len == 0)
		return 0;
	rtcp_header *rtcp = (rtcp_header *)packet;
	int pno = 0, total = len;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			break;
		if(rtcp->version != 2)
			break;
		pno++;
		switch(rtcp->type) {
			case RTCP_SR: {
				/* SR, sender report */
				if (!rtcp_check_sr(rtcp, total))
					break;
				rtcp_sr *sr = (rtcp_sr *)rtcp;
				if(sr->header.rc > 0) {
					return ntohl(sr->rb[0].ssrc);
				}
				break;
			}
			case RTCP_RR: {
				/* RR, receiver report */
				if (!rtcp_check_rr(rtcp, total))
					break;
				rtcp_rr *rr = (rtcp_rr *)rtcp;
				if(rr->header.rc > 0) {
					return ntohl(rr->rb[0].ssrc);
				}
				break;
			}
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0) {
			break;
		}
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return 0;
}

void rtcp_swap_report_blocks(char *packet, int len, uint32_t rtx_ssrc) {
	if(packet == NULL || len == 0)
		return;
	rtcp_header *rtcp = (rtcp_header *)packet;
	int pno = 0, total = len;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			break;
		if(rtcp->version != 2)
			break;
		pno++;
		switch(rtcp->type) {
			case RTCP_SR: {
				/* SR, sender report */
				if (!rtcp_check_sr(rtcp, total))
					break;
				rtcp_sr *sr = (rtcp_sr *)rtcp;
				if(sr->header.rc >= 2 && ntohl(sr->rb[0].ssrc) == rtx_ssrc) {
					report_block rb0_copy = sr->rb[0];
					sr->rb[0] = sr->rb[1];
					sr->rb[1] = rb0_copy;
				}
				break;
			}
			case RTCP_RR: {
				/* RR, receiver report */
				if (!rtcp_check_rr(rtcp, total))
					break;
				rtcp_rr *rr = (rtcp_rr *)rtcp;
				if(rr->header.rc >= 2 && ntohl(rr->rb[0].ssrc) == rtx_ssrc) {
					report_block rb0_copy = rr->rb[0];
					rr->rb[0] = rr->rb[1];
					rr->rb[1] = rb0_copy;
					//printf("Switched incoming RTCP Report Blocks %"SCNu32"(rtx) <--> %"SCNu32"\n", rtx_ssrc, ntohl(rr->rb[0].ssrc));
				}
				break;
			}
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0) {
			break;
		}
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
}

/* Helper to handle an incoming SR: triggered by a call to rtcp_fix_ssrc with a valid context pointer */
static void rtcp_incoming_sr(rtcp_context *ctx, rtcp_sr *sr) {
	if(ctx == NULL)
		return;
	/* Update the context with info on the monotonic time of last SR received */
	ctx->lsr_ts = get_monotonic_time();
	/* Compute the last SR received as well */
	uint64_t ntp = ntohl(sr->si.ntp_ts_msw);
	ntp = (ntp << 32) | ntohl(sr->si.ntp_ts_lsw);
	ctx->lsr = (ntp >> 16);
}

/* Helper to handle an incoming transport-cc feedback: triggered by a call to rtcp_fix_ssrc a valid context pointer */
static void rtcp_incoming_transport_cc(rtcp_context *ctx, rtcp_fb *twcc, int total) {
	if(ctx == NULL || twcc == NULL || total < 20)
		return;
	if(!rtcp_check_fci((rtcp_header *)twcc, total, 4))
		return;
	/* Parse the header first */
	uint8_t *data = (uint8_t *)twcc->fci;
	uint16_t base_seq = 0, status_count = 0;
	uint32_t reference = 0;
	uint8_t fb_pkt = 0;
	memcpy(&base_seq, data, sizeof(uint16_t));
	base_seq = ntohs(base_seq);
	memcpy(&status_count, data+2, sizeof(uint16_t));
	status_count = ntohs(status_count);
	memcpy(&reference, data+4, sizeof(uint32_t));
	reference = ntohl(reference) >> 8;
	fb_pkt = *(data+7);
	//printf("[TWCC] seq=%"SCNu16", psc=%"SCNu16", ref=%"SCNu32", fbpc=%"SCNu8"\n", base_seq, status_count, reference, fb_pkt);
	/* Now traverse the feedback: packet chunks first, and then recv deltas */
	total -= 20;
	data += 8;
	uint16_t psc = status_count;
	uint16_t chunk = 0;
	uint8_t t = 0, ss = 0, s = 0, length = 0;
	/* Iterate on all packet chunks */
	//printf("[TWCC] Chunks:\n");
	uint16_t num = 0;
	GList *list = NULL;
	while(psc > 0 && total > 1) {
		num++;
		memcpy(&chunk, data, sizeof(uint16_t));
		chunk = ntohs(chunk);
		t = (chunk & 0x8000) >> 15;
		if(t == 0) {
			/* Run length */
			s = (chunk & 0x6000) >> 13;
			length = (chunk & 0x1FFF);
			//printf("  [%"SCNu16"] t=run-length, s=%s, l=%"SCNu8"\n", num, rtp_packet_status_description(s), length);
			while(length > 0 && psc > 0) {
				list = g_list_prepend(list, GUINT_TO_POINTER(s));
				length--;
				psc--;
			}
		} else {
			/* Status vector */
			ss = (chunk & 0x4000) >> 14;
			length = (s ? 7 : 14);
			printf("  [%"SCNu16"] t=status-vector, ss=%s, l=%"SCNu8"\n", num,
				s ? "2-bit" : "bit", length);
			while(length > 0 && psc > 0) {
				if(!ss)
					s = (chunk & (1 << (length-1))) ? rtp_packet_status_smalldelta : rtp_packet_status_notreceived;
				else
					s = (chunk & (3 << (2*length-2))) >> (2*length-2);
				list = g_list_prepend(list, GUINT_TO_POINTER(s));
				length--;
				psc--;
			}
		}
		total -= 2;
		data += 2;
	}
	if(psc > 0) {
		/* Incomplete feedback? Drop... */
		g_list_free(list);
		return;
	}
	list = g_list_reverse(list);
	/* Iterate on all recv deltas */
	printf("[TWCC] Recv Deltas (%d/%"SCNu16"):\n", g_list_length(list), status_count);
	num = 0;
	uint16_t delta = 0;
	uint32_t delta_us = 0;
	GList *iter = list;
	while(iter != NULL && total > 0) {
		num++;
		delta = 0;
		s = GPOINTER_TO_UINT(iter->data);
		if(s == rtp_packet_status_smalldelta) {
			/* Small delta = 1 byte */
			delta = *data;
			total--;
			data++;
		} else if(s == rtp_packet_status_largeornegativedelta) {
			/* Large or negative delta = 2 bytes */
			if(total < 2)
				break;
			memcpy(&delta, data, sizeof(uint16_t));
			delta = ntohs(delta);
			total -= 2;
			data += 2;
		}
		delta_us = delta*250;
		/* Print summary */
		printf("  [%02"SCNu16"][%"SCNu16"] %s (%"SCNu32"us)\n", num, base_seq+num-1,
			rtp_packet_status_description(s), delta_us);
		iter = iter->next;
	}
	/* TODO Update the context with the feedback we got */
	g_list_free(list);
}

/* Link quality estimate filter coefficient */
#define LINK_QUALITY_FILTER_K 3.0

static double rtcp_link_quality_filter(double last, double in) {
	/* Note: the last!=last is there to check for NaN */
	if(last == 0 || last == in || last != last) {
		return in;
	} else {
		return (1.0 - 1.0/LINK_QUALITY_FILTER_K) * last + (1.0/LINK_QUALITY_FILTER_K) * in;
	}
}

/* Update link quality stats based on RR */
static void rtcp_rr_update_stats(rtcp_context *ctx, report_block rb) {
	int64_t ts = get_monotonic_time();
	int64_t delta_t = ts - ctx->rr_last_ts;
	if(delta_t < 2*G_USEC_PER_SEC) {
		return;
	}
	ctx->rr_last_ts = ts;
	uint32_t total_lost = ntohl(rb.flcnpl) & 0x00FFFFFF;
	if (ctx->rr_last_ehsnr != 0) {
		uint32_t sent = g_atomic_int_get(&ctx->sent_packets_since_last_rr);
		uint32_t expect = ntohl(rb.ehsnr) - ctx->rr_last_ehsnr;
		int32_t nacks = g_atomic_int_get(&ctx->nack_count) - ctx->rr_last_nack_count;
		double link_q = !sent ? 0 : 100.0 - (100.0 * nacks / (double)sent);
		ctx->out_link_quality = rtcp_link_quality_filter(ctx->out_link_quality, link_q);
		int32_t lost = total_lost - ctx->rr_last_lost;
		if(lost < 0) {
			lost = 0;
		}
		double media_link_q = !expect ? 0 : 100.0 - (100.0 * lost / (double)expect);
		ctx->out_media_link_quality = rtcp_link_quality_filter(ctx->out_media_link_quality, media_link_q);
		printf("Out link quality=%"SCNu32", media link quality=%"SCNu32"\n", rtcp_context_get_out_link_quality(ctx), rtcp_context_get_out_media_link_quality(ctx));
	}
	ctx->rr_last_ehsnr = ntohl(rb.ehsnr);
	ctx->rr_last_lost = total_lost;
	ctx->rr_last_nack_count = g_atomic_int_get(&ctx->nack_count);
	g_atomic_int_set(&ctx->sent_packets_since_last_rr, 0);
}

/* Helper to handle an incoming RR: triggered by a call to rtcp_fix_ssrc with fixssrc=0 */
static void rtcp_incoming_rr(rtcp_context *ctx, rtcp_rr *rr) {
	if(ctx == NULL)
		return;
	/* FIXME Check the Record Blocks */
	if(rr->header.rc > 0) {
		double jitter = (double)ntohl(rr->rb[0].jitter);
		uint32_t fraction = ntohl(rr->rb[0].flcnpl) >> 24;
		uint32_t total = ntohl(rr->rb[0].flcnpl) & 0x00FFFFFF;
		printf("jitter=%f, fraction=%"SCNu32", loss=%"SCNu32"\n", jitter, fraction, total);
		ctx->lost_remote = total;
		ctx->jitter_remote = jitter;
		rtcp_rr_update_stats(ctx, rr->rb[0]);
		/* FIXME Compute round trip time */
		uint32_t lsr = ntohl(rr->rb[0].lsr);
		uint32_t dlsr = ntohl(rr->rb[0].delay);
		if(lsr == 0)	/* Not enough info yet */
			return;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		uint32_t s = tv.tv_sec + 2208988800u;
		uint32_t u = tv.tv_usec;
		uint32_t f = (u << 12) + (u << 8) - ((u * 3650) >> 6);
		uint32_t ntp_ts_msw = s;
		uint32_t ntp_ts_lsw = f;
		uint64_t temp = ((uint64_t)ntp_ts_msw << 32 ) | ntp_ts_lsw;
		uint32_t a = (uint32_t)(temp >> 16);
		uint32_t rtt = a - lsr - dlsr;
		uint32_t rtt_msw = (rtt & 0xFFFF0000) >> 16;
		uint32_t rtt_lsw = rtt & 0x0000FFFF;
		tv.tv_sec = rtt_msw;
		tv.tv_usec = (rtt_lsw * 15625) >> 10;
		ctx->rtt = tv.tv_sec*1000 + tv.tv_usec/1000;	/* We need milliseconds */
		printf("rtt=%"SCNu32"\n", ctx->rtt);
	}
}

gboolean rtcp_check_len(rtcp_header *rtcp, int len) {
	if (len < (int)sizeof(rtcp_header) + (int)sizeof(uint32_t)) {
		printf("Packet size is too small (%d bytes) to contain RTCP\n", len);
		return FALSE;
	}
	int header_def_len = 4*(int)ntohs(rtcp->length) + 4;
	if (len < header_def_len) {
		printf("Invalid RTCP packet defined length, expected %d bytes > actual %d bytes\n", header_def_len, len);
		return FALSE;
	}
	return TRUE;
}

gboolean rtcp_check_sr(rtcp_header *rtcp, int len) {
	if (len < (int)sizeof(rtcp_header) + (int)sizeof(uint32_t) + (int)sizeof(sender_info)) {
		printf("RTCP Packet is too small (%d bytes) to contain SR\n", len);
		return FALSE;
	}
	int header_rb_len = (int)(rtcp->rc)*(int)sizeof(report_block);
	int actual_rb_len = len - (int)sizeof(rtcp_header) - (int)sizeof(uint32_t) - (int)sizeof(sender_info);
	if (actual_rb_len < header_rb_len) {
		printf("SR got %d RB count, expected %d bytes > actual %d bytes\n", rtcp->rc, header_rb_len, actual_rb_len);
		return FALSE;
	}
	return TRUE;
}

gboolean rtcp_check_rr(rtcp_header *rtcp, int len) {
	int header_rb_len = (int)(rtcp->rc)*(int)sizeof(report_block);
	int actual_rb_len = len - (int)sizeof(rtcp_header) - (int)sizeof(uint32_t);
	if (actual_rb_len < header_rb_len) {
		printf("RR got %d RB count, expected %d bytes > actual %d bytes\n", rtcp->rc, header_rb_len, actual_rb_len);
		return FALSE;
	}
	return TRUE;
}

gboolean rtcp_check_fci(rtcp_header *rtcp, int len, int sizeof_fci) {
	/* At least one sizeof_fci bytes FCI */
	if (len < (int)sizeof(rtcp_header) + 2*(int)sizeof(uint32_t) + sizeof_fci) {
		printf("RTCP Packet is too small (%d bytes) to contain at least one %d bytes FCI\n", len, sizeof_fci);
		return FALSE;
	}
	/* Evaluate fci total size */
	int fci_size = len - (int)sizeof(rtcp_header) - 2*(int)sizeof(uint32_t);
	/*  The length of the feedback message is set to 2+(sizeof_fci/4)*N where
		N is the number of FCI entries */
	int fcis;
	switch(sizeof_fci) {
		case 0:
			fcis = 0;
			break;
		case 4:
			fcis = (int)ntohs(rtcp->length) - 2;
			break;
		case 8:
			fcis = ((int)ntohs(rtcp->length) - 2) >> 1;
			break;
		default:
			fcis = ((int)ntohs(rtcp->length)- 2) / (sizeof_fci >> 2);
			break;
	}
	/* Every FCI is sizeof_fci bytes */
	if (fci_size < sizeof_fci*fcis) {
		printf("Got %d FCI count, expected %d bytes > actual %d bytes\n", fcis, sizeof_fci*fcis, fci_size);
		return FALSE;
	}
	return TRUE;
}

gboolean rtcp_check_remb(rtcp_header *rtcp, int len) {
	/* At least 1 SSRC feedback */
	if (len < (int)sizeof(rtcp_header) + 2*(int)sizeof(uint32_t) + 3*(int)sizeof(uint32_t)) {
		printf("Packet is too small (%d bytes) to contain REMB\n", len);
		return FALSE;
	}
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	uint8_t numssrc = *(rtcpfb->fci+4);
	/* Evaluate ssrcs total size */
	int ssrc_size = len - (int)sizeof(rtcp_header) - 2*(int)sizeof(uint32_t);
	/* Every SSRC is 4 bytes */
	if (ssrc_size < 4*numssrc) {
		printf("REMB got %d SSRC count, expected %d bytes > actual %d bytes\n", numssrc, 4*numssrc, ssrc_size);
		return FALSE;
	}
	return TRUE;
}

int rtcp_fix_ssrc(rtcp_context *ctx, char *packet, int len, int fixssrc, uint32_t newssrcl, uint32_t newssrcr) {
	if(packet == NULL || len <= 0)
		return -1;
	rtcp_header *rtcp = (rtcp_header *)packet;
	int pno = 0, total = len;
	printf("   Parsing compound packet (total of %d bytes)\n", total);
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			return -2;
		if(rtcp->version != 2)
			return -2;
		pno++;
		/* TODO Should we handle any of these packets ourselves, or just relay them? */
		switch(rtcp->type) {
			case RTCP_SR: {
				/* SR, sender report */
				printf("     #%d SR (200)\n", pno);
				if (!rtcp_check_sr(rtcp, total))
					return -2;
				rtcp_sr *sr = (rtcp_sr *)rtcp;
				/* If an RTCP context was provided, update it with info on this SR */
				rtcp_incoming_sr(ctx, sr);
				if(fixssrc && newssrcl) {
					sr->ssrc = htonl(newssrcl);
					if (sr->header.rc > 0) {
						sr->rb[0].ssrc = htonl(newssrcr);
					}
				}
				break;
			}
			case RTCP_RR: {
				/* RR, receiver report */
				printf("     #%d RR (201)\n", pno);
				if (!rtcp_check_rr(rtcp, total))
					return -2;
				rtcp_rr *rr = (rtcp_rr *)rtcp;
				/* If an RTCP context was provided, update it with info on this RR */
				rtcp_incoming_rr(ctx, rr);
				if(fixssrc && newssrcl) {
					rr->ssrc = htonl(newssrcl);
					if (rr->header.rc > 0) {
						rr->rb[0].ssrc = htonl(newssrcr);
					}
				}
				break;
			}
			case RTCP_SDES: {
				/* SDES, source description */
				printf("     #%d SDES (202)\n", pno);
				rtcp_sdes *sdes = (rtcp_sdes *)rtcp;
				//~ printf("       -- SSRC: %u\n", ntohl(sdes->chunk.ssrc));
				if(fixssrc && newssrcl) {
					sdes->chunk.ssrc = htonl(newssrcl);
				}
				break;
			}
			case RTCP_BYE: {
				/* BYE, goodbye */
				printf("     #%d BYE (203)\n", pno);
				rtcp_bye *bye = (rtcp_bye *)rtcp;
				//~ printf("       -- SSRC: %u\n", ntohl(bye->ssrc[0]));
				if(fixssrc && newssrcl) {
					bye->ssrc[0] = htonl(newssrcl);
				}
				break;
			}
			case RTCP_APP: {
				/* APP, application-defined */
				printf("     #%d APP (204)\n", pno);
				rtcp_app *app = (rtcp_app *)rtcp;
				//~ printf("       -- SSRC: %u\n", ntohl(app->ssrc));
				if(fixssrc && newssrcl) {
					app->ssrc = htonl(newssrcl);
				}
				break;
			}
			case RTCP_FIR: {
				/* FIR, rfc2032 */
				printf("     #%d FIR (192)\n", pno);
				break;
			}
			case RTCP_RTPFB: {
				/* RTPFB, Transport layer FB message (rfc4585) */
				//~ printf("     #%d RTPFB (205)\n", pno);
				gint fmt = rtcp->rc;
				//~ printf("       -- FMT: %u\n", fmt);
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				//~ printf("       -- SSRC: %u\n", ntohl(rtcpfb->ssrc));
				if(fmt == 1) {
					printf("     #%d NACK -- RTPFB (205)\n", pno);
					/* NACK FCI size is 4 bytes */
					if (!rtcp_check_fci(rtcp, total, 4))
						return -2;
					if(fixssrc && newssrcr) {
						rtcpfb->media = htonl(newssrcr);
					}
					int nacks = ntohs(rtcp->length)-2;	/* Skip SSRCs */
					if(nacks > 0) {
						printf("        Got %d nacks\n", nacks);
						rtcp_nack *nack = NULL;
						uint16_t pid = 0;
						uint16_t blp = 0;
						int i=0, j=0;
						char bitmask[20];
						for(i=0; i< nacks; i++) {
							nack = (rtcp_nack *)rtcpfb->fci + i;
							pid = ntohs(nack->pid);
							blp = ntohs(nack->blp);
							memset(bitmask, 0, 20);
							for(j=0; j<16; j++) {
								bitmask[j] = (blp & ( 1 << j )) >> j ? '1' : '0';
							}
							bitmask[16] = '\n';
							printf("[%d] %"SCNu16" / %s\n", i, pid, bitmask);
						}
					}
				} else if(fmt == 3) {	/* rfc5104 */
					/* TMMBR: http://tools.ietf.org/html/rfc5104#section-4.2.1.1 */
					printf("     #%d TMMBR -- RTPFB (205)\n", pno);
					if(fixssrc && newssrcr) {
						/* TMMBR FCI size is 8 bytes */
						if (!rtcp_check_fci(rtcp, total, 8))
							return -2;
						uint32_t *ssrc = (uint32_t *)rtcpfb->fci;
						*ssrc = htonl(newssrcr);
					}
				} else if(fmt == 15) {	/* transport-cc */
					/* If an RTCP context was provided, parse this transport-cc feedback */
					rtcp_incoming_transport_cc(ctx, rtcpfb, total);
				} else {
					printf("     #%d ??? -- RTPFB (205, fmt=%d)\n", pno, fmt);
				}
				if(fixssrc && newssrcl) {
					rtcpfb->ssrc = htonl(newssrcl);
				}
				break;
			}
			case RTCP_PSFB: {
				/* PSFB, Payload-specific FB message (rfc4585) */
				//~ printf("     #%d PSFB (206)\n", pno);
				gint fmt = rtcp->rc;
				//~ printf("       -- FMT: %u\n", fmt);
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				//~ printf("       -- SSRC: %u\n", ntohl(rtcpfb->ssrc));
				if(fmt == 1) {
					printf("     #%d PLI -- PSFB (206)\n", pno);
					/* PLI does not require parameters.  Therefore, the length field MUST be
						2, and there MUST NOT be any Feedback Control Information. */
					if(fixssrc && newssrcr) {
						if (!rtcp_check_fci(rtcp, total, 0))
							return -2;
						rtcpfb->media = htonl(newssrcr);
					}
				} else if(fmt == 2) {
					printf("     #%d SLI -- PSFB (206)\n", pno);
				} else if(fmt == 3) {
					printf("     #%d RPSI -- PSFB (206)\n", pno);
				} else if(fmt == 4) {	/* rfc5104 */
					/* FIR: http://tools.ietf.org/html/rfc5104#section-4.3.1.1 */
					printf("     #%d FIR -- PSFB (206)\n", pno);
					if(fixssrc && newssrcr) {
						/* FIR FCI size is 8 bytes */
						if (!rtcp_check_fci(rtcp, total, 8))
							return -2;
						rtcpfb->media = htonl(newssrcr);
						uint32_t *ssrc = (uint32_t *)rtcpfb->fci;
						*ssrc = htonl(newssrcr);
					}
				} else if(fmt == 5) {	/* rfc5104 */
					/* TSTR: http://tools.ietf.org/html/rfc5104#section-4.3.2.1 */
					printf("     #%d PLI -- TSTR (206)\n", pno);
				} else if(fmt == 15) {
					//~ printf("       -- This is a AFB!\n");
					rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
					if(fixssrc && newssrcr) {
						/* AFB FCI size is variable, check just media SSRC */
						if (!rtcp_check_fci(rtcp, total, 0))
							return -2;
						rtcpfb->ssrc = htonl(newssrcr);
						rtcpfb->media = 0;
					}
					rtcp_fb_remb *remb = (rtcp_fb_remb *)rtcpfb->fci;
					if(rtcp_check_remb(rtcp, total) && remb->id[0] == 'R' && remb->id[1] == 'E' && remb->id[2] == 'M' && remb->id[3] == 'B') {
						printf("     #%d REMB -- PSFB (206)\n", pno);
						if(fixssrc && newssrcr) {
							remb->ssrc[0] = htonl(newssrcr);
						}
						/* FIXME From rtcp_utility.cc */
						unsigned char *_ptrRTCPData = (unsigned char *)remb;
						_ptrRTCPData += 4;	// Skip unique identifier and num ssrc
						//~ printf(" %02X %02X %02X %02X\n", _ptrRTCPData[0], _ptrRTCPData[1], _ptrRTCPData[2], _ptrRTCPData[3]);
						uint8_t numssrc = (_ptrRTCPData[0]);
						uint8_t brExp = (_ptrRTCPData[1] >> 2) & 0x3F;
						uint32_t brMantissa = (_ptrRTCPData[1] & 0x03) << 16;
						brMantissa += (_ptrRTCPData[2] << 8);
						brMantissa += (_ptrRTCPData[3]);
						uint32_t bitRate = (uint64_t)brMantissa << brExp;
						printf("       -- -- -- REMB: %u * 2^%u = %"SCNu32" (%d SSRCs, %u)\n",
							brMantissa, brExp, bitRate, numssrc, ntohl(remb->ssrc[0]));
					} else {
						printf("     #%d AFB ?? -- PSFB (206)\n", pno);
					}
				} else {
					printf("     #%d ?? -- PSFB (206, fmt=%d)\n", pno, fmt);
				}
				if(fixssrc && newssrcl) {
					rtcpfb->ssrc = htonl(newssrcl);
				}
				break;
			}
			case RTCP_XR: {
				/* XR, extended reports (rfc3611) */
				rtcp_xr *xr = (rtcp_xr *)rtcp;
				if(fixssrc && newssrcl) {
					xr->ssrc = htonl(newssrcl);
				}
				/* TODO Fix report blocks too, once we support them */
				break;
			}
			default:
				printf("     Unknown RTCP PT %d\n", rtcp->type);
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		printf("       RTCP PT %d, length: %d bytes\n", rtcp->type, length*4+4);
		if(length == 0) {
			//~ printf("  0-length, end of compound packet\n");
			break;
		}
		total -= length*4+4;
		//~ printf("     Packet has length %d (%d bytes, %d remaining), moving to next one...\n", length, length*4+4, total);
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return 0;
}

char *rtcp_filter(char *packet, int len, int *newlen) {
	if(packet == NULL || len <= 0 || newlen == NULL)
		return NULL;
	*newlen = 0;
	rtcp_header *rtcp = (rtcp_header *)packet;
	char *filtered = NULL;
	int total = len, length = 0, bytes = 0;
	/* Iterate on the compound packets */
	gboolean keep = TRUE;
	gboolean error = FALSE;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total)) {
			error = TRUE;
			break;
		}
		if(rtcp->version != 2) {
			error = TRUE;
			break;
		}
		keep = TRUE;
		length = ntohs(rtcp->length);
		if(length == 0)
			break;
		bytes = length*4+4;
		switch(rtcp->type) {
			case RTCP_SR:
			case RTCP_RR:
			case RTCP_SDES:
				/* These are packets we generate ourselves, so remove them */
				keep = FALSE;
				break;
			case RTCP_BYE:
			case RTCP_APP:
			case RTCP_FIR:
			case RTCP_PSFB:
				break;
			case RTCP_RTPFB:
				if(rtcp->rc == 1) {
					/* We handle NACKs ourselves as well, remove this too */
					keep = FALSE;
					break;
				} else if(rtcp->rc == 15) {
					/* We handle Transport Wide CC ourselves as well, remove this too */
					keep = FALSE;
					break;
				}
				break;
			case RTCP_XR:
				/* FIXME We generate RR/SR ourselves, so remove XR */
				keep = FALSE;
				break;
			default:
				printf("Unknown RTCP PT %d\n", rtcp->type);
				/* FIXME Should we allow this to go through instead? */
				keep = FALSE;
				break;
		}
		if(keep) {
			/* Keep this packet */
			if(filtered == NULL)
				filtered = g_malloc0(total);
			memcpy(filtered+*newlen, (char *)rtcp, bytes);
			*newlen += bytes;
		}
		total -= bytes;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	if (error) {
		g_free(filtered);
		filtered = NULL;
		*newlen = 0;
	}
	return filtered;
}


int rtcp_process_incoming_rtp(rtcp_context *ctx, char *packet, int len,
		gboolean rfc4588_pkt, gboolean rfc4588_enabled, gboolean retransmissions_disabled,
		uint32_t rate) {
	if(ctx == NULL || packet == NULL || len < 1)
		return -1;

	/* First of all, let's check if we need to change the timestamp base */
	rtp_header *rtp = (rtp_header *)packet;
	int pt = rtp->type;
	uint32_t clock_rate = rate;
	if(clock_rate > 0 && ctx->tb != clock_rate)
		ctx->tb = clock_rate;
	/* Now parse this RTP packet header and update the rtcp_context instance */
	uint16_t seq_number = ntohs(rtp->seq_number);
	if(ctx->base_seq == 0 && ctx->seq_cycle == 0)
		ctx->base_seq = seq_number;

	int64_t now = get_monotonic_time();
	if (!rfc4588_pkt) {
		/* Non-RTX packet */
		if ((int16_t)(seq_number - ctx->max_seq_nr) > 0) {
			/* In-order packet */
			ctx->received++;

			if(seq_number < ctx->max_seq_nr)
				ctx->seq_cycle++;
			ctx->max_seq_nr = seq_number;
			uint32_t rtp_expected = 0x0;
			if(ctx->seq_cycle > 0) {
				rtp_expected = ctx->seq_cycle;
				rtp_expected = rtp_expected << 16;
			}
			rtp_expected = rtp_expected + 1 + ctx->max_seq_nr - ctx->base_seq;
			ctx->expected = rtp_expected;

			int64_t arrival = (now * ctx->tb) / 1000000;
			int64_t transit = arrival - ntohl(rtp->timestamp);
			if (ctx->transit != 0) {
				int64_t d = transit - ctx->transit;
				if (d < 0) d = -d;
				ctx->jitter += (1./16.) * ((double)d  - ctx->jitter);
			}

			ctx->transit = transit;
			ctx->rtp_last_inorder_ts = ntohl(rtp->timestamp);
			ctx->rtp_last_inorder_time = now;
		} else {
			/* Out-of-order packet */
			if (rfc4588_enabled) {
				/* Just an out-of-order packet in a stream that is using a dedicated RTX channel */
				ctx->received++;
			} else {
				/* The stream does not have a rtx channel dedicated to retransmissions */
				if (!retransmissions_disabled) {
					/* The stream does have a retransmission mechanism (e.g. video w/ NACKs) */
					/* Try to detect the retransmissions */
					/* TODO We have to accomplish this in a smarter way */
					int32_t rtp_diff = ntohl(rtp->timestamp) - ctx->rtp_last_inorder_ts;
					int32_t ms_diff = (abs(rtp_diff) * 1000) / ctx->tb;
					if (ms_diff > 120)
						ctx->retransmitted++;
					else
						ctx->received++;
				} else {
					/* The stream does not have a retransmission mechanism (e.g. audio wo/ NACKs) */
					ctx->received++;
				}
			}
		}
	} else {
		/* RTX packet, just increase retransmitted count */
		ctx->retransmitted++;
	}

	/* RTP packet received: it means we can start sending RR */
	ctx->rtp_recvd = 1;

	return 0;
}


uint32_t rtcp_context_get_rtt(rtcp_context *ctx) {
	return ctx ? ctx->rtt : 0;
}

uint32_t rtcp_context_get_in_link_quality(rtcp_context *ctx) {
	return ctx ? (uint32_t)(ctx->in_link_quality + 0.5) : 0;
}

uint32_t rtcp_context_get_in_media_link_quality(rtcp_context *ctx) {
	return ctx ? (uint32_t)(ctx->in_media_link_quality + 0.5) : 0;
}

uint32_t rtcp_context_get_out_link_quality(rtcp_context *ctx) {
	return ctx ? (uint32_t)(ctx->out_link_quality + 0.5) : 0;
}

uint32_t rtcp_context_get_out_media_link_quality(rtcp_context *ctx) {
	return ctx ? (uint32_t)(ctx->out_media_link_quality + 0.5) : 0;
}

uint32_t rtcp_context_get_lost_all(rtcp_context *ctx, gboolean remote) {
	if(ctx == NULL)
		return 0;
	return remote ? ctx->lost_remote : ctx->lost;
}

static uint32_t rtcp_context_get_lost(rtcp_context *ctx) {
	if(ctx == NULL)
		return 0;
	uint32_t lost;
	if(ctx->lost > 0x7FFFFF) {
		lost = 0x7FFFFF;
	} else {
		lost = ctx->lost;
	}
	return lost;
}

static uint32_t rtcp_context_get_lost_fraction(rtcp_context *ctx) {
	if(ctx == NULL)
		return 0;
	uint32_t expected_interval = ctx->expected - ctx->expected_prior;
	uint32_t received_interval = ctx->received - ctx->received_prior;
	int32_t lost_interval = expected_interval - received_interval;
	uint32_t fraction;
	if(expected_interval == 0 || lost_interval <=0)
		fraction = 0;
	else
		fraction = (lost_interval << 8) / expected_interval;
	return fraction << 24;
}

uint32_t rtcp_context_get_jitter(rtcp_context *ctx, gboolean remote) {
	if(ctx == NULL || ctx->tb == 0)
		return 0;
	return (uint32_t) floor((remote ? ctx->jitter_remote : ctx->jitter) / (ctx->tb/1000));
}

static void rtcp_estimate_in_link_quality(rtcp_context *ctx) {
	uint32_t expected_interval = ctx->expected - ctx->expected_prior;
	uint32_t received_interval = ctx->received - ctx->received_prior;
	uint32_t retransmitted_interval = ctx->retransmitted - ctx->retransmitted_prior;

	/* Link lost is calculated without considering any retransmission */
	int32_t link_lost = expected_interval - received_interval;
	if (link_lost < 0) {
		link_lost = 0;
	}
	double link_q = !expected_interval ? 0 : 100.0 - (100.0 * (double)link_lost / (double)expected_interval);
	ctx->in_link_quality = rtcp_link_quality_filter(ctx->in_link_quality, link_q);

	/* Media lost is calculated considering also retransmitted packets */
	int32_t media_lost = expected_interval - (received_interval + retransmitted_interval);
	if (media_lost < 0) {
		media_lost = 0;
	}
	double media_link_q = !expected_interval ? 0 : 100.0 - (100.0 * (double)media_lost / (double)expected_interval);
	ctx->in_media_link_quality = rtcp_link_quality_filter(ctx->in_media_link_quality, media_link_q);

	printf("In link quality=%"SCNu32", media link quality=%"SCNu32"\n", rtcp_context_get_in_link_quality(ctx), rtcp_context_get_in_media_link_quality(ctx));
}

int rtcp_report_block(rtcp_context *ctx, report_block *rb) {
	if(ctx == NULL || rb == NULL)
		return -1;
	gint64 now = get_monotonic_time();
	rb->jitter = htonl((uint32_t) ctx->jitter);
	rb->ehsnr = htonl((((uint32_t) 0x0 + ctx->seq_cycle) << 16) + ctx->max_seq_nr);
	uint32_t expected_interval = ctx->expected - ctx->expected_prior;
	uint32_t received_interval = ctx->received - ctx->received_prior;
	int32_t lost_interval = 0;
	if (expected_interval > received_interval) {
		lost_interval = expected_interval - received_interval;
	}
	ctx->lost += lost_interval;
	uint32_t reported_lost = rtcp_context_get_lost(ctx);
	uint32_t reported_fraction = rtcp_context_get_lost_fraction(ctx);
	rtcp_estimate_in_link_quality(ctx);
	ctx->expected_prior = ctx->expected;
	ctx->received_prior = ctx->received;
	ctx->retransmitted_prior = ctx->retransmitted;
	rb->flcnpl = htonl(reported_lost | reported_fraction);
	if(ctx->lsr > 0) {
		rb->lsr = htonl(ctx->lsr);
		rb->delay = htonl(((now - ctx->lsr_ts) << 16) / 1000000);
	} else {
		rb->lsr = 0;
		rb->delay = 0;
	}
	ctx->last_sent = now;
	return 0;
}

int rtcp_fix_report_data(char *packet, int len, uint32_t base_ts, uint32_t base_ts_prev, uint32_t ssrc_peer, uint32_t ssrc_local, uint32_t ssrc_expected, gboolean video) {
	if(packet == NULL || len <= 0)
		return -1;
	/* Parse RTCP compound packet */
	rtcp_header *rtcp = (rtcp_header *)packet;
	int pno = 0, total = len, status = 0;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			return -2;
		if(rtcp->version != 2)
			return -2;
		pno++;
		switch(rtcp->type) {
			case RTCP_RR: {
				if (!rtcp_check_rr(rtcp, total))
					return -2;
				rtcp_rr *rr = (rtcp_rr *)rtcp;
				rr->ssrc = htonl(ssrc_peer);
				status++;
				if (rr->header.rc > 0) {
					rr->rb[0].ssrc = htonl(ssrc_local);
					status++;
					/* FIXME we need to fix the extended highest sequence number received */
					/* FIXME we need to fix the cumulative number of packets lost */
					break;
				}
				break;
			}
			case RTCP_SR: {
				if (!rtcp_check_sr(rtcp, total))
					return -2;
				rtcp_sr *sr = (rtcp_sr *)rtcp;
				uint32_t recv_ssrc = ntohl(sr->ssrc);
				if (recv_ssrc != ssrc_expected) {
					if(ssrc_expected != 0) {
						printf("Incoming RTCP SR SSRC (%"SCNu32") does not match the expected one (%"SCNu32") video=%d\n", recv_ssrc, ssrc_expected, video);
					}
					return -3;
				}
				sr->ssrc = htonl(ssrc_peer);
				/* FIXME we need to fix the sender's packet count */
				/* FIXME we need to fix the sender's octet count */
				uint32_t sr_ts = ntohl(sr->si.rtp_ts);
				uint32_t fix_ts = (sr_ts - base_ts) + base_ts_prev;
				sr->si.rtp_ts = htonl(fix_ts);
				status++;
				if (sr->header.rc > 0) {
					sr->rb[0].ssrc = htonl(ssrc_local);
					status++;
					/* FIXME we need to fix the extended highest sequence number received */
					/* FIXME we need to fix the cumulative number of packets lost */
					break;
				}
				break;
			}
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return status;
}

gboolean rtcp_has_bye(char *packet, int len) {
	/* Parse RTCP compound packet */
	rtcp_header *rtcp = (rtcp_header *)packet;
	int pno = 0, total = len;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			break;
		if(rtcp->version != 2)
			break;
		pno++;
		switch(rtcp->type) {
			case RTCP_BYE:
				return TRUE;
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return FALSE;
}

gboolean rtcp_has_fir(char *packet, int len) {
	/* Parse RTCP compound packet */
	rtcp_header *rtcp = (rtcp_header *)packet;
	int pno = 0, total = len;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			break;
		if(rtcp->version != 2)
			break;
		pno++;
		switch(rtcp->type) {
			case RTCP_FIR:
				return TRUE;
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return FALSE;
}

gboolean rtcp_has_pli(char *packet, int len) {
	/* Parse RTCP compound packet */
	rtcp_header *rtcp = (rtcp_header *)packet;
	int pno = 0, total = len;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			break;
		if(rtcp->version != 2)
			break;
		pno++;
		switch(rtcp->type) {
			case RTCP_PSFB: {
				gint fmt = rtcp->rc;
				if(fmt == 1)
					return TRUE;
				break;
			}
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return FALSE;
}

GSList *rtcp_get_nacks(char *packet, int len) {
	if(packet == NULL || len == 0)
		return NULL;
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* FIXME Get list of sequence numbers we should send again */
	GSList *list = NULL;
	int total = len;
	gboolean error = FALSE;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total)) {
			error = TRUE;
			break;
		}
		if (rtcp->version != 2) {
			error = TRUE;
			break;
		}
		if(rtcp->type == RTCP_RTPFB) {
			gint fmt = rtcp->rc;
			if(fmt == 1) {
				/* NACK FCI size is 4 bytes */
				if (!rtcp_check_fci(rtcp, total, 4)) {
					error = TRUE;
					break;
				}
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				int nacks = ntohs(rtcp->length)-2;	/* Skip SSRCs */
				if(nacks > 0) {
					printf("        Got %d nacks\n", nacks);
					rtcp_nack *nack = NULL;
					uint16_t pid = 0;
					uint16_t blp = 0;
					int i=0, j=0;
					char bitmask[20];
					for(i=0; i< nacks; i++) {
						nack = (rtcp_nack *)rtcpfb->fci + i;
						pid = ntohs(nack->pid);
						list = g_slist_append(list, GUINT_TO_POINTER(pid));
						blp = ntohs(nack->blp);
						memset(bitmask, 0, 20);
						for(j=0; j<16; j++) {
							bitmask[j] = (blp & ( 1 << j )) >> j ? '1' : '0';
							if((blp & ( 1 << j )) >> j)
								list = g_slist_append(list, GUINT_TO_POINTER(pid+j+1));
						}
						bitmask[16] = '\n';
						printf("[%d] %"SCNu16" / %s\n", i, pid, bitmask);
					}
				}
				break;
			}
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	if (error && list) {
		g_slist_free(list);
		list = NULL;
	}
	return list;
}

int rtcp_remove_nacks(char *packet, int len) {
	if(packet == NULL || len == 0)
		return len;
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Find the NACK message */
	char *nacks = NULL;
	int total = len, nacks_len = 0;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total)) {
			break;
		}
		if(rtcp->version != 2)
			break;
		if(rtcp->type == RTCP_RTPFB) {
			gint fmt = rtcp->rc;
			if(fmt == 1) {
				nacks = (char *)rtcp;
				if (!rtcp_check_fci(rtcp, total, 4)) {
					break;
				}
			}
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		if(nacks != NULL) {
			nacks_len = length*4+4;
			break;
		}
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	if(nacks != NULL) {
		total = len - ((nacks-packet)+nacks_len);
		if(total < 0) {
			/* FIXME Should never happen, but you never know: do nothing */
			return len;
		} else if(total == 0) {
			/* NACK was the last compound packet, easy enough */
			return len-nacks_len;
		} else {
			/* NACK is between two compound packets, move them around */
			int i=0;
			for(i=0; i<total; i++)
				*(nacks+i) = *(nacks+nacks_len+i);
			return len-nacks_len;
		}
	}
	return len;
}

/* Query an existing REMB message */
uint32_t rtcp_get_remb(char *packet, int len) {
	if(packet == NULL || len == 0)
		return 0;
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Get REMB bitrate, if any */
	int total = len;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			break;
		if(rtcp->version != 2)
			break;
		if(rtcp->type == RTCP_PSFB) {
			gint fmt = rtcp->rc;
			if(fmt == 15) {
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				rtcp_fb_remb *remb = (rtcp_fb_remb *)rtcpfb->fci;
				if(rtcp_check_remb(rtcp, total) && remb->id[0] == 'R' && remb->id[1] == 'E' && remb->id[2] == 'M' && remb->id[3] == 'B') {
					/* FIXME From rtcp_utility.cc */
					unsigned char *_ptrRTCPData = (unsigned char *)remb;
					_ptrRTCPData += 4;	/* Skip unique identifier and num ssrc */
					//~ printf(" %02X %02X %02X %02X\n", _ptrRTCPData[0], _ptrRTCPData[1], _ptrRTCPData[2], _ptrRTCPData[3]);
					uint8_t brExp = (_ptrRTCPData[1] >> 2) & 0x3F;
					uint32_t brMantissa = (_ptrRTCPData[1] & 0x03) << 16;
					brMantissa += (_ptrRTCPData[2] << 8);
					brMantissa += (_ptrRTCPData[3]);
					uint32_t bitrate = (uint64_t)brMantissa << brExp;
					printf("Got REMB bitrate %"SCNu32"\n", bitrate);
					return bitrate;
				}
			}
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return 0;
}

/* Change an existing REMB message */
int rtcp_cap_remb(char *packet, int len, uint32_t bitrate) {
	if(packet == NULL || len == 0)
		return -1;
	rtcp_header *rtcp = (rtcp_header *)packet;
	if(bitrate == 0)
		return 0;	/* No need to cap */
	/* Cap REMB bitrate */
	int total = len;
	while(rtcp) {
		if (!rtcp_check_len(rtcp, total))
			return -2;
		if(rtcp->version != 2)
			return -2;
		if(rtcp->type == RTCP_PSFB) {
			gint fmt = rtcp->rc;
			if(fmt == 15) {
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				rtcp_fb_remb *remb = (rtcp_fb_remb *)rtcpfb->fci;
				if(rtcp_check_remb(rtcp, total) && remb->id[0] == 'R' && remb->id[1] == 'E' && remb->id[2] == 'M' && remb->id[3] == 'B') {
					/* FIXME From rtcp_utility.cc */
					unsigned char *_ptrRTCPData = (unsigned char *)remb;
					_ptrRTCPData += 4;	/* Skip unique identifier and num ssrc */
					//~ printf(" %02X %02X %02X %02X\n", _ptrRTCPData[0], _ptrRTCPData[1], _ptrRTCPData[2], _ptrRTCPData[3]);
					uint8_t brExp = (_ptrRTCPData[1] >> 2) & 0x3F;
					uint32_t brMantissa = (_ptrRTCPData[1] & 0x03) << 16;
					brMantissa += (_ptrRTCPData[2] << 8);
					brMantissa += (_ptrRTCPData[3]);
					uint32_t origbitrate = (uint64_t)brMantissa << brExp;
					printf("Got REMB bitrate %"SCNu32", need to cap it to %"SCNu32"\n", origbitrate, bitrate);
					printf("  >> %u * 2^%u = %"SCNu32"\n", brMantissa, brExp, origbitrate);
					/* bitrate --> brexp/brmantissa */
					uint8_t b = 0;
					uint8_t newbrexp = 0;
					uint32_t newbrmantissa = 0;
					for(b=0; b<32; b++) {
						if(bitrate <= ((uint32_t) 0x3FFFF << b)) {
							newbrexp = b;
							break;
						}
					}
					if(b > 31)
						b = 31;
					newbrmantissa = bitrate >> b;
					printf("new brexp:      %"SCNu8"\n", newbrexp);
					printf("new brmantissa: %"SCNu32"\n", newbrmantissa);
					/* FIXME From rtcp_sender.cc */
					_ptrRTCPData[1] = (uint8_t)((newbrexp << 2) + ((newbrmantissa >> 16) & 0x03));
					_ptrRTCPData[2] = (uint8_t)(newbrmantissa >> 8);
					_ptrRTCPData[3] = (uint8_t)(newbrmantissa);
				}
			}
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return 0;
}

/* Generate a new SDES message */
int rtcp_sdes_cname(char *packet, int len, const char *cname, int cnamelen) {
	if(packet == NULL || len <= 0 || cname == NULL || cnamelen <= 0)
		return -1;
	memset(packet, 0, len);
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_SDES;
	rtcp->rc = 1;
	int plen = 8;	/* Header + chunk + item header */
	plen += cnamelen+3; /* cname item header(2) + cnamelen + terminator(1) */
	/* calculate padding length. assume that plen is shorter than 65535 */
	plen = (plen + 3) & 0xFFFC;
	if(len < plen) {
		printf("Buffer too small for SDES message: %d < %d\n", len, plen);
		return -1;
	}
	rtcp->length = htons((plen/4)-1);
	/* Now set SDES stuff */
	rtcp_sdes *rtcpsdes = (rtcp_sdes *)rtcp;
	rtcpsdes->item.type = 1;
	rtcpsdes->item.len = cnamelen;
	memcpy(rtcpsdes->item.content, cname, cnamelen);
	return plen;
}

/* Generate a new REMB message */
int rtcp_remb(char *packet, int len, uint32_t bitrate) {
	/* By default we assume a single SSRC will be set */
	return rtcp_remb_ssrcs(packet, len, bitrate, 1);
}

int rtcp_remb_ssrcs(char *packet, int len, uint32_t bitrate, uint8_t numssrc) {
	if(packet == NULL || numssrc == 0)
		return -1;
	int min_len = 20 + numssrc*4;
	if(len < min_len)
		return -1;
	memset(packet, 0, len);
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_PSFB;
	rtcp->rc = 15;
	rtcp->length = htons((min_len/4)-1);
	/* Now set REMB stuff */
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	rtcp_fb_remb *remb = (rtcp_fb_remb *)rtcpfb->fci;
	remb->id[0] = 'R';
	remb->id[1] = 'E';
	remb->id[2] = 'M';
	remb->id[3] = 'B';
	/* bitrate --> brexp/brmantissa */
	uint8_t b = 0;
	uint8_t newbrexp = 0;
	uint32_t newbrmantissa = 0;
	for(b=0; b<32; b++) {
		if(bitrate <= ((uint32_t) 0x3FFFF << b)) {
			newbrexp = b;
			break;
		}
	}
	if(b > 31)
		b = 31;
	newbrmantissa = bitrate >> b;
	/* FIXME From rtcp_sender.cc */
	unsigned char *_ptrRTCPData = (unsigned char *)remb;
	_ptrRTCPData += 4;	/* Skip unique identifier */
	_ptrRTCPData[0] = numssrc;
	_ptrRTCPData[1] = (uint8_t)((newbrexp << 2) + ((newbrmantissa >> 16) & 0x03));
	_ptrRTCPData[2] = (uint8_t)(newbrmantissa >> 8);
	_ptrRTCPData[3] = (uint8_t)(newbrmantissa);
	printf("[REMB] bitrate=%"SCNu32" (%d bytes)\n", bitrate, 4*(ntohs(rtcp->length)+1));
	return min_len;
}

/* Generate a new FIR message */
int rtcp_fir(char *packet, int len, int *seqnr) {
	if(packet == NULL || len != 20 || seqnr == NULL)
		return -1;
	memset(packet, 0, len);
	rtcp_header *rtcp = (rtcp_header *)packet;
	*seqnr = *seqnr + 1;
	if(*seqnr < 0 || *seqnr >= 256)
		*seqnr = 0;	/* Reset sequence number */
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_PSFB;
	rtcp->rc = 4;	/* FMT=4 */
	rtcp->length = htons((len/4)-1);
	/* Now set FIR stuff */
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	rtcp_fb_fir *fir = (rtcp_fb_fir *)rtcpfb->fci;
	fir->seqnr = htonl(*seqnr << 24);	/* FCI: Sequence number */
	printf("[FIR] seqnr=%d (%d bytes)\n", *seqnr, 4*(ntohs(rtcp->length)+1));
	return 20;
}

/* Generate a new PLI message */
int rtcp_pli(char *packet, int len) {
	if(packet == NULL || len != 12)
		return -1;
	memset(packet, 0, len);
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_PSFB;
	rtcp->rc = 1;	/* FMT=1 */
	rtcp->length = htons((len/4)-1);
	return 12;
}

/* Generate a new NACK message */
int rtcp_nacks(char *packet, int len, GSList *nacks) {
	if(packet == NULL || len < 16 || nacks == NULL)
		return -1;
	memset(packet, 0, len);
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_RTPFB;
	rtcp->rc = 1;	/* FMT=1 */
	/* Now set NACK stuff */
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	rtcp_nack *nack = (rtcp_nack *)rtcpfb->fci;
	/* FIXME We assume the GSList list is already ordered... */
	guint16 pid = GPOINTER_TO_UINT(nacks->data);
	nack->pid = htons(pid);
	nacks = nacks->next;
	int words = 3;
	while(nacks) {
		guint16 npid = GPOINTER_TO_UINT(nacks->data);
		if(npid-pid < 1) {
			printf("Skipping PID to NACK (%"SCNu16" already added)...\n", npid);
		} else if(npid-pid > 16) {
			/* We need a new block: this sequence number will be its root PID */
			printf("Adding another block of NACKs (%"SCNu16"-%"SCNu16" > 16)...\n", npid, pid);
			words++;
			if(len < (words*4+4)) {
				printf("Buffer too small: %d < %d (at least %d NACK blocks needed)\n", len, words*4+4, words);
				return -1;
			}
			char *new_block = packet + words*4;
			nack = (rtcp_nack *)new_block;
			pid = GPOINTER_TO_UINT(nacks->data);
			nack->pid = htons(pid);
		} else {
			uint16_t blp = ntohs(nack->blp);
			blp |= 1 << (npid-pid-1);
			nack->blp = htons(blp);
		}
		nacks = nacks->next;
	}
	rtcp->length = htons(words);
	return words*4+4;
}

int rtcp_transport_wide_cc_feedback(char *packet, size_t size, guint32 ssrc, guint32 media, guint8 feedback_packet_count, GQueue *transport_wide_cc_stats) {
	if(packet == NULL || size < sizeof(rtcp_header) || transport_wide_cc_stats == NULL || g_queue_is_empty(transport_wide_cc_stats))
		return -1;

	memset(packet, 0, size);
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_RTPFB;
	rtcp->rc = 15;
	/* Now set FB stuff */
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	rtcpfb->ssrc = htonl(ssrc);
	rtcpfb->media = htonl(media);

	/* Get first packet */
	rtcp_transport_wide_cc_stats *stat = (rtcp_transport_wide_cc_stats *) g_queue_pop_head (transport_wide_cc_stats);
	/* Calculate temporal info */
	guint16 base_seq_num = stat->transport_seq_num;
	gboolean first_received	= FALSE;
	guint64 reference_time = 0;
	guint packet_status_count = g_queue_get_length(transport_wide_cc_stats) + 1;

	/*
		0                   1                   2                   3
		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|      base sequence number     |      packet status count      |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                 reference time                | fb pkt. count |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 */

	/* The packet as unsigned */
	guint8 *data = (guint8 *)packet;
	/* The start of the feedback data */
	size_t len = sizeof(rtcp_header) + 8;

	/* Set header data */
	set2(data, len, base_seq_num);
	set2(data, len+2, packet_status_count);
	/* Set3 referenceTime when first received */
	size_t reference_time_pos = len + 4;
	set1(data, len+7, feedback_packet_count);

	/* Next byte */
	len += 8;

	/* Initial time in us */
	guint64 timestamp = 0;

	/* Store delta array */
	GQueue *deltas = g_queue_new();
	GQueue *statuses = g_queue_new();
	rtp_packet_status last_status = rtp_packet_status_reserved;
	rtp_packet_status max_status = rtp_packet_status_notreceived;
	gboolean all_same = TRUE;

	/* For each packet  */
	while (stat != NULL) {
		rtp_packet_status status = rtp_packet_status_notreceived;

		/* If got packet */
		if (stat->timestamp) {
			int delta = 0;
			/* If first received */
			if (!first_received) {
				/* Got it  */
				first_received = TRUE;
				/* Set it */
				reference_time = stat->timestamp / 64000;
				/* Get initial time */
				timestamp = reference_time * 64000;
				/* also in buffer */
				/* (use only 23 bits of reference_time) */
				set3(data, reference_time_pos, (reference_time & 0x007FFFFF));
			}

			/* Get delta */
			if (stat->timestamp>timestamp)
				delta = (stat->timestamp-timestamp)/250;
			else
				delta = -(int)((timestamp-stat->timestamp)/250);
			/* If it is negative or too big */
			if (delta<0 || delta> 255) {
				/* Big one */
				status = rtp_packet_status_largeornegativedelta;
			} else {
				/* Small */
				status = rtp_packet_status_smalldelta;
			}
			/* Store delta */
			/* Overflows are possible here */
			g_queue_push_tail(deltas, GINT_TO_POINTER(delta));
			/* Set last time */
			timestamp = stat->timestamp;
		}

		/* Check if all previoues ones were equal and this one the first different */
		if (all_same && last_status!=rtp_packet_status_reserved && status!=last_status) {
			/* How big was the same run */
			if (g_queue_get_length(statuses)>7) {
				guint32 word = 0;
				/* Write run! */
				/*
					0                   1
					0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					|T| S |       Run Length        |
					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					T = 0
				 */
				word = push_bits(word, 1, 0);
				word = push_bits(word, 2, last_status);
				word = push_bits(word, 13, g_queue_get_length(statuses));
				/* Write word */
				set2(data, len, word);
				len += 2;
				/* Remove all statuses */
				g_queue_clear(statuses);
				/* Reset status */
				last_status = rtp_packet_status_reserved;
				max_status = rtp_packet_status_notreceived;
				all_same = TRUE;
			} else {
				/* Not same */
				all_same = FALSE;
			}
		}

		/* Push back statuses, it will be handled later */
		g_queue_push_tail(statuses, GUINT_TO_POINTER(status));

		/* If it is bigger */
		if (status>max_status) {
			/* Store it */
			max_status = status;
		}
		/* Store las status */
		last_status = status;

		/* Check if we can still be enqueuing for a run */
		if (!all_same) {
			/* Check  */
			if (!all_same && max_status==rtp_packet_status_largeornegativedelta && g_queue_get_length(statuses)>6) {
				guint32 word = 0;
				/*
					0                   1
					0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					|T|S|        Symbols            |
					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					T = 1
					S = 1
				 */
				word = push_bits(word, 1, 1);
				word = push_bits(word, 1, 1);
				/* Set next 7 */
				size_t i = 0;
				for (i=0;i<7;++i) {
					/* Get status */
					rtp_packet_status status = (rtp_packet_status) GPOINTER_TO_UINT(g_queue_pop_head (statuses));
					/* Write */
					word = push_bits(word, 2, (guint8)status);
				}
				/* Write word */
				set2(data, len, word);
				len += 2;
				/* Reset */
				last_status = rtp_packet_status_reserved;
				max_status = rtp_packet_status_notreceived;
				all_same = TRUE;

				/* We need to restore the values, as there may be more elements on the buffer */
				for (i=0; i<g_queue_get_length(statuses); ++i) {
					/* Get status */
					status = (rtp_packet_status) GPOINTER_TO_UINT(g_queue_peek_nth(statuses, i));
					/* If it is bigger */
					if (status>max_status) {
						/* Store it */
						max_status = status;
					}
					//Check if it is the same */
					if (all_same && last_status!=rtp_packet_status_reserved && status!=last_status) {
						/* Not the same */
						all_same = FALSE;
					}
					/* Store las status */
					last_status = status;
				}
			} else if (!all_same && g_queue_get_length(statuses)>13) {
				guint32 word = 0;
				/*
					0                   1
					0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					|T|S|       symbol list         |
					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
					T = 1
					S = 0
				 */
				word = push_bits(word, 1, 1);
				word = push_bits(word, 1, 0);
				/* Set next 7 */
				guint32 i = 0;
				for (i=0;i<14;++i) {
					/* Get status */
					rtp_packet_status status = (rtp_packet_status) GPOINTER_TO_UINT(g_queue_pop_head (statuses));
					/* Write */
					word = push_bits(word, 1, (guint8)status);
				}
				/* Write word */
				set2(data, len, word);
				len += 2;
				/* Reset */
				last_status = rtp_packet_status_reserved;
				max_status = rtp_packet_status_notreceived;
				all_same = TRUE;
			}
		}
		/* Free mem */
		g_free(stat);

		/* Get next packet stat */
		stat = (rtcp_transport_wide_cc_stats *) g_queue_pop_head (transport_wide_cc_stats);
	}

	/* Get status len */
	size_t statuses_len = g_queue_get_length(statuses);

	/* If not finished yet */
	if (statuses_len>0) {
		/* How big was the same run */
		if (all_same) {
			guint32 word = 0;
			/* Write run! */
			word = push_bits(word, 1, 0);
			word = push_bits(word, 2, last_status);
			word = push_bits(word, 13, statuses_len);
			/* Write word */
			set2(data, len, word);
			len += 2;
		} else if (max_status == rtp_packet_status_largeornegativedelta) {
			guint32 word = 0;
			/* Write chunk */
			word = push_bits(word, 1, 1);
			word = push_bits(word, 1, 1);
			/* Write all the statuses */
			unsigned int i = 0;
			for (i=0;i<statuses_len;i++) {
				/* Get each status */
				rtp_packet_status status = (rtp_packet_status) GPOINTER_TO_UINT(g_queue_pop_head (statuses));
				/* Write */
				word = push_bits(word, 2, (guint8)status);
			}
			/* Write pending */
			word = push_bits(word, 14-statuses_len*2, 0);
			/* Write word */
			set2(data , len, word);
			len += 2;
		} else {
			guint32 word = 0;
			/* Write chunck */
			word = push_bits(word, 1, 1);
			word = push_bits(word, 1, 0);
			/* Write all the statuses */
			unsigned int i = 0;
			for (i=0;i<statuses_len;i++) {
				/* Get each status */
				rtp_packet_status status = (rtp_packet_status) GPOINTER_TO_UINT(g_queue_pop_head (statuses));
				/* Write */
				word = push_bits(word, 1, (guint8)status);
			}
			/* Write pending */
			word = push_bits(word, 14-statuses_len, 0);
			/* Write word */
			set2(data, len, word);
			len += 2;
		}
	}

	/* Write now the deltas */
	while (!g_queue_is_empty(deltas)) {
		/* Get next delta */
		gint delta = GPOINTER_TO_INT(g_queue_pop_head (deltas));
		/* Check size */
		if (delta<0 || delta>255) {
			short reported_delta = (short)delta;
			/* Overflow */
			if (reported_delta != delta) {
				reported_delta = delta > 0 ? SHRT_MAX : SHRT_MIN;
				printf("Delta value (%d) too large, reporting it as %d\n", delta, reported_delta);
			}
			/* 2 bytes */
			set2(data, len, reported_delta);
			/* Inc */
			len += 2;
		} else {
			/* 1 byte */
			set1(data, len, (guint8)delta);
			/* Inc */
			len ++;
		}
	}

	/* Clean mem */
	g_queue_free(statuses);
	g_queue_free(deltas);

	/* Add zero padding */
	while (len%4) {
		/* Add padding */
		set1(data, len++, 0);
	}

	/* Set RTCP Len */
	rtcp->length = htons((len/4)-1);

	/* Done */
	return len;
}
