//
// Created by hejingsheng on 2023/6/9.
//

#include "rtmp_stack_packet.h"
#include "autofree.h"
#include "netio.h"
#include "logger.h"

RtmpHeader::RtmpHeader()
{
    chunk_type = 0;
    chunk_stream_id = RTMP_CID_OverConnection;
    timestamp = 0;
    msg_length = 0;
    msg_stream_id = 0;
    msg_type_id = 0;
}

RtmpHeader::~RtmpHeader()
{

}

int RtmpHeader::encode(uint8_t *data, int size)
{
    if (chunk_type == RTMP_CHUNK_FMT0_TYPE)
    {
        return encode_fmt0_chunk_header(data, size);
    }
    else if (chunk_type == RTMP_CHUNK_FMT1_TYPE)
    {
        return encode_fmt1_chunk_header(data, size);
    }
    else if (chunk_type == RTMP_CHUNK_FMT2_TYPE)
    {
        return encode_fmt2_chunk_header(data, size);
    }
    else if (chunk_type == RTMP_CHUNK_FMT3_TYPE)
    {
        return encode_fmt3_chunk_header(data, size);
    }
    else
    {
        return -1;
    }
}

int RtmpHeader::decode(uint8_t *data, int len)
{
    int ret;
    int offset = 0;
    uint8_t fmt;
    uint32_t cs_id;

    ret = decode_base_header(data+offset, len-offset, fmt, cs_id);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    chunk_type = fmt;
    chunk_stream_id = cs_id;
    ret = decode_msg_header(data+offset, len-offset);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    return offset;
}

int RtmpHeader::decode_msg_header(uint8_t *data, int length)
{
    int offset = 0;

    if (chunk_type == RTMP_CHUNK_FMT0_TYPE)
    {
        // fmt0 12 bytes
        if (length < 11)
        {
            return -1;
        }
        timestamp += data[offset++] << 16;
        timestamp += data[offset++] << 8;
        timestamp += data[offset++];
//        if (timestamp == RTMP_EXTENDED_TIMESTAMP)
//        {
//            if (length < 15)
//            {
//                return -1;
//            }
//            timestamp = 0;
//        }
        msg_length += data[offset++] << 16;
        msg_length += data[offset++] << 8;
        msg_length += data[offset++];
        msg_type_id = data[offset++];
        msg_stream_id += data[offset++] << 24;
        msg_stream_id += data[offset++] << 16;
        msg_stream_id += data[offset++] << 8;
        msg_stream_id += data[offset++];
//        if (timestamp == RTMP_EXTENDED_TIMESTAMP)
//        {
//            timestamp += data[offset++] << 24;
//            timestamp += data[offset++] << 16;
//            timestamp += data[offset++] << 8;
//            timestamp += data[offset++];
//        }
    }
    else if (chunk_type == RTMP_CHUNK_FMT1_TYPE)
    {
        // fmt1 8 bytes
        if (length < 7)
        {
            return -1;
        }
        timestamp += data[offset++] << 16;
        timestamp += data[offset++] << 8;
        timestamp += data[offset++];
//        if (timestamp == RTMP_EXTENDED_TIMESTAMP)
//        {
//            if (length < 11)
//            {
//                return -1;
//            }
//            timestamp = 0;
//        }
        msg_length += data[offset++] << 16;
        msg_length += data[offset++] << 8;
        msg_length += data[offset++];
        msg_type_id = data[offset++];
//        if (timestamp == RTMP_EXTENDED_TIMESTAMP)
//        {
//            timestamp += data[offset++] << 24;
//            timestamp += data[offset++] << 16;
//            timestamp += data[offset++] << 8;
//            timestamp += data[offset++];
//        }
    }
    else if (chunk_type == RTMP_CHUNK_FMT2_TYPE)
    {
        // fmt2 4 bytes
        if (length < 3)
        {
            return -1;
        }
        timestamp += data[offset++] << 16;
        timestamp += data[offset++] << 8;
        timestamp += data[offset++];
//        if (timestamp == RTMP_EXTENDED_TIMESTAMP)
//        {
//            if (length < 7)
//            {
//                return -1;
//            }
//            timestamp = 0;
//            timestamp += data[offset++] << 24;
//            timestamp += data[offset++] << 16;
//            timestamp += data[offset++] << 8;
//            timestamp += data[offset++];
//        }
    }
    else //
    {
        // fmt3 1 bytes
    }
    return offset;
}

int RtmpHeader::decode_base_header(uint8_t *data, int length, uint8_t &fmt, uint32_t &cs_id)
{
    uint8_t value;
    int offset = 0;
    value = (uint8_t)data[offset];
    offset++;
    fmt = (value & 0x03) >> 6;
    cs_id = value & 0x3f;
    if (cs_id > 1)
    {
        return offset;
    }
    else if (cs_id == 0)
    {
        if (length < 2)
        {
            return -1;
        }
        value = (uint8_t)data[offset];
        offset++;
        cs_id = value+64;
        return offset;
    }
    else
    {
        int32_t tmp = 0;
        if (length < 3)
        {
            return -1;
        }
        value = (uint8_t)data[offset];
        offset++;
        tmp += value;
        value = (uint8_t)data[offset];
        offset++;
        tmp += value * 256;
        cs_id = tmp+64;
        return offset;
    }
}

int RtmpHeader::encode_fmt0_chunk_header(uint8_t *data, int size)
{
    uint8_t *p = data;
    uint8_t *q;
    if (size < RTMP_CHUNK_FMT0_HEADER_MAX_SIZE)
    {
        return -1;
    }
    // base header(chunk type + chunk stream id) 1 byte
    *p++ = 0x00 | (((uint8_t)chunk_stream_id) & 0x3f);
    // timestame big endian 3 bytes
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        *p++ = 0xff;
        *p++ = 0xff;
        *p++ = 0xff;
    }
    else
    {
        q = (uint8_t*)&timestamp;
        *p++ = q[2];
        *p++ = q[1];
        *p++ = q[0];
    }
    // message length big endian 3 bytes
    q = (uint8_t*)&msg_length;
    *p++ = q[2];
    *p++ = q[1];
    *p++ = q[0];
    // message type 1 byte
    *p++ = msg_type_id;
    // message stream id 4 bytes
    q = (uint8_t*)&msg_stream_id;
    *p++ = q[0];
    *p++ = q[1];
    *p++ = q[2];
    *p++ = q[3];
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        q = (uint8_t*)&timestamp;
        *p++ = q[3];
        *p++ = q[2];
        *p++ = q[1];
        *p++ = q[0];
    }
    return (int)(p-data);
}

int RtmpHeader::encode_fmt1_chunk_header(uint8_t *data, int size)
{
    uint8_t *p = data;
    uint8_t *q;
    if (size < RTMP_CHUNK_FMT1_HEADER_MAX_SIZE)
    {
        return -1;
    }
    // base header(chunk type + chunk stream id) 1 byte
    *p++ = 0x40 | (((uint8_t)chunk_stream_id) & 0x3f);
    // timestame big endian 3 bytes
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        *p++ = 0xff;
        *p++ = 0xff;
        *p++ = 0xff;
    }
    else
    {
        q = (uint8_t*)&timestamp;
        *p++ = q[2];
        *p++ = q[1];
        *p++ = q[0];
    }
    // message length big endian 3 bytes
    q = (uint8_t*)&msg_length;
    *p++ = q[2];
    *p++ = q[1];
    *p++ = q[0];
    // message type 1 byte
    *p++ = msg_type_id;
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        q = (uint8_t*)&timestamp;
        *p++ = q[3];
        *p++ = q[2];
        *p++ = q[1];
        *p++ = q[0];
    }
    return (int)(p-data);
}

int RtmpHeader::encode_fmt2_chunk_header(uint8_t *data, int size)
{
    uint8_t *p = data;
    uint8_t *q;
    if (size < RTMP_CHUNK_FMT2_HEADER_MAX_SIZE)
    {
        return -1;
    }
    // base header(chunk type + chunk stream id) 1 byte
    *p++ = 0x80 | (((uint8_t)chunk_stream_id) & 0x3f);
    // timestame big endian 3 bytes
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        *p++ = 0xff;
        *p++ = 0xff;
        *p++ = 0xff;
    }
    else
    {
        q = (uint8_t*)&timestamp;
        *p++ = q[2];
        *p++ = q[1];
        *p++ = q[0];
    }
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        q = (uint8_t*)&timestamp;
        *p++ = q[3];
        *p++ = q[2];
        *p++ = q[1];
        *p++ = q[0];
    }
    return (int)(p-data);
}

int RtmpHeader::encode_fmt3_chunk_header(uint8_t *data, int size)
{
    uint8_t *p = data;
    uint8_t *q;
    if (size < RTMP_CHUNK_FMT3_HEADER_MAX_SIZE)
    {
        return -1;
    }
    // base header(chunk type + chunk stream id) 1 byte
    *p++ = 0xC0 | (((uint8_t)chunk_stream_id) & 0x3f);
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
    {
        q = (uint8_t*)&timestamp;
        *p++ = q[3];
        *p++ = q[2];
        *p++ = q[1];
        *p++ = q[0];
    }
    return (int)(p-data);
}

int RtmpBasePacket::encode(uint8_t *&payload, int &size)
{
    int ret;
    int length = get_pkg_len();
    uint8_t *buffer = NULL;

    if (length > 0)
    {
        buffer = new uint8_t[length];
        ret = encode_pkg(buffer, length);
        if (ret < 0)
        {
            delete[] buffer;
            return -1;
        }
    }
    payload = buffer;
    size = length;
    return 0;
}

RtmpChunkData::RtmpChunkData()
{
    payload = nullptr;
    current_payload_len = 0;
}

RtmpChunkData::~RtmpChunkData()
{
    if (payload)
    {
        delete[] payload;
    }
}

void RtmpChunkData::create_payload(int length)
{
    if (payload)
    {
        delete[] payload;
    }
    payload = new uint8_t[length];
    current_payload_len = 0;
}

void RtmpChunkData::copy_payload(uint8_t *data, int len)
{
    memcpy(payload+current_payload_len, data, len);
    current_payload_len += len;
}

RtmpAudioPacket::RtmpAudioPacket()
{

}

RtmpAudioPacket::~RtmpAudioPacket()
{

}

uint32_t RtmpAudioPacket::getTimestamp()
{
    return timestamp;
}

int RtmpAudioPacket::encode_pkg(uint8_t *payload, int size)
{
    int offset = 0;
    /*
     *  ++++    ++   +     +
     * |format|rate|bits|channel
     * pcmu  0x08 (4bits)
     * rate  5.5k 0  11k 1  22k  2  44k  3 (2bits)
     * bits  8bits 0  16bits 1 (1bits)
     * channel  0 mono  1 stereo  (1bits)
     */
    payload[offset++] = 0x82;  //1000 00 1 0
    memcpy(payload+offset, data, datalen);
    offset += datalen;
    return 0;
}

int RtmpAudioPacket::decode(uint8_t *data, int len)
{
    return 0;
}

int RtmpAudioPacket::get_pkg_len()
{
    // pcmu format
    return 1 + datalen;
}
int RtmpAudioPacket::get_cs_id()
{
    return RTMP_CID_Audio;
}

int RtmpAudioPacket::get_msg_type()
{
    return RTMP_MSG_AudioMessage;
}

RtmpAVCPacket::RtmpAVCPacket() {

}

RtmpAVCPacket::~RtmpAVCPacket() {

}

int RtmpAVCPacket::encode_pkg(uint8_t *payload, int size) {
    int offset = 0;
    payload[offset++] = 0x17;
    payload[offset++] = 0x00;
    payload[offset++] = 0x00;
    payload[offset++] = 0x00;
    payload[offset++] = 0x00;

    // avc struct sps
    payload[offset++] = 0x01;
    payload[offset++] = sps[1];
    payload[offset++] = sps[2];
    payload[offset++] = sps[3];
    payload[offset++] = 0xfc | 3;
    payload[offset++] = 0xe0 | 1;
    offset += write_int16(payload+offset, spslen);
    memcpy(payload+offset, sps, spslen);
    offset += spslen;
    // avc struct pps
    payload[offset++] = 0x01;
    offset += write_int16(payload+offset, ppslen);
    memcpy(payload+offset, pps, ppslen);
    offset += ppslen;
    return 0;
}

int RtmpAVCPacket::decode(uint8_t *data, int len) {
    return 0;
}

int RtmpAVCPacket::get_pkg_len() {
    return 1+1+3+6+2+spslen+1+2+ppslen;
}

int RtmpAVCPacket::get_cs_id() {
    return RTMP_CID_Video;
}

int RtmpAVCPacket::get_msg_type() {
    return RTMP_MSG_VideoMessage;
}

RtmpVideoPacket::RtmpVideoPacket() {

}

RtmpVideoPacket::~RtmpVideoPacket() {

}

uint32_t RtmpVideoPacket::getTimestamp() {
    return timestamp;
}

int RtmpVideoPacket::encode_pkg(uint8_t *payload, int size)
{
    int offset = 0;
    uint8_t *q;
    int time = 40;

    if (keyframe) {
        payload[offset++] = 0x17;
    }
    else {
        payload[offset++] = 0x27;
    }
    payload[offset++] = 0x01;
//    q = (uint8_t*)&timestamp;
//    payload[offset++] = q[2];
//    payload[offset++] = q[1];
//    payload[offset++] = q[0];
    payload[offset++] = 0x00;
    payload[offset++] = 0x00;
    payload[offset++] = 0x00;
    offset += write_uint32(payload+offset, nalulen);
    memcpy(payload+offset, nalu, nalulen);
    offset += nalulen;
    return 0;
}

int RtmpVideoPacket::decode(uint8_t *data, int len)
{
    return 0;
}

int RtmpVideoPacket::get_pkg_len()
{
    return 1 + 1 + 3 + 4 + nalulen;
}

int RtmpVideoPacket::get_cs_id()
{
    return RTMP_CID_Video;
}

int RtmpVideoPacket::get_msg_type()
{
    return RTMP_MSG_VideoMessage;
}

RtmpConnectPacket::RtmpConnectPacket()
{
    command_name = "connect";
    number = 1;
    command_object = RtmpAmf0Any::object();
}

RtmpConnectPacket::~RtmpConnectPacket()
{
    delete command_object;
}

int RtmpConnectPacket::encode_pkg(uint8_t *payload, int size)
{
    int offset = 0;
    int ret;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    ret = command_object->write(payload+offset, size-offset);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    return 0;
}

int RtmpConnectPacket::decode(uint8_t *data, int len)
{
    int ret;
    int offset = 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0 || command_name.empty() || command_name != "connect")
    {
        goto error;
    }
    offset += ret;
    ret = rtmp_amf0_read_number(data+offset, len-offset, number);
    if (ret < 0 || number != 1.0)
    {
        goto error;
    }
    offset += ret;
    ret = command_object->read(data+offset, len-offset);
    if (ret < 0)
    {
        goto error;
    }
    offset += ret;
    return 0;
error:
    return ret;
}

int RtmpConnectPacket::get_pkg_len()
{
    int length = 0;

    length += RtmpAmf0Size::str(command_name);
    length += RtmpAmf0Size::number();
    length += RtmpAmf0Size::object(command_object);

    return length;
}

int RtmpConnectPacket::get_cs_id()
{
    return RTMP_CID_OverConnection;
}

int RtmpConnectPacket::get_msg_type()
{
    return RTMP_MSG_AMF0CommandMessage;
}

RtmpConnectResponsePacket::RtmpConnectResponsePacket()
{
    command_name = "_result";
    number = 1;
    object = RtmpAmf0Any::object();
    info = RtmpAmf0Any::object();
}

RtmpConnectResponsePacket::~RtmpConnectResponsePacket()
{
    delete object;
    delete info;
}

int RtmpConnectResponsePacket::decode(uint8_t *data, int len)
{
    int ret;
    int offset = 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0)
    {
        return -1;
    }
    if (command_name.empty() || command_name != "_result")
    {
        return -2;
    }
    offset += ret;
    ret = rtmp_amf0_read_number(data+offset, len-offset, number);
    if (ret < 0)
    {
        return -1;
    }
    offset += ret;
    if (offset < len)
    {
        RtmpAmf0Any *p = nullptr;
        ret = rtmp_amf0_read_any(data+offset, len-offset, &p);
        if (ret < 0)
        {
            return -1;
        }
        offset += ret;
        if (!p->is_object())
        {
            delete p;
        }
        else
        {
            if (object)
            {
                delete object;
            }
            object = p->to_object();
        }
    }
    ret = info->read(data+offset, len-offset);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    return offset;
}

int RtmpConnectResponsePacket::get_pkg_len()
{
    return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number() + RtmpAmf0Size::object(object) + RtmpAmf0Size::object(info);
}

int RtmpConnectResponsePacket::get_cs_id()
{
    return RTMP_CID_OverConnection;
}

int RtmpConnectResponsePacket::get_msg_type()
{
    return RTMP_MSG_AMF0CommandMessage;
}

int RtmpConnectResponsePacket::encode_pkg(uint8_t *payload, int size)
{
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    ret = object->write(payload+offset, size-offset);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    ret = info->write(payload+offset, size-offset);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpSetWindowAckSizePacket::RtmpSetWindowAckSizePacket()
{
    window_ack_size = 0;
}

RtmpSetWindowAckSizePacket::~RtmpSetWindowAckSizePacket()
{

}

int RtmpSetWindowAckSizePacket::decode(uint8_t *data, int len)
{
    int offset = 0;
    if (len < 4)
    {
        return -1;
    }
    offset += read_int32(data+offset, &window_ack_size);
    return offset;
}

int RtmpSetWindowAckSizePacket::get_pkg_len()
{
    return 4;
}

int RtmpSetWindowAckSizePacket::get_cs_id()
{
    return RTMP_CID_ProtocolControl;
}

int RtmpSetWindowAckSizePacket::get_msg_type()
{
    return RTMP_MSG_WindowAcknowledgementSize;
}

int RtmpSetWindowAckSizePacket::encode_pkg(uint8_t *payload, int size)
{
    int offset = 0;
    if (size < 4)
    {
        return -1;
    }
    offset += write_int32(payload+offset, window_ack_size);
    return offset;
}

RtmpAcknowledgementPacket::RtmpAcknowledgementPacket()
{
    sequence_number = 0;
}

RtmpAcknowledgementPacket::~RtmpAcknowledgementPacket()
{

}

int RtmpAcknowledgementPacket::decode(uint8_t *data, int len)
{
    int offset = 0;

    if (len < 4)
    {
        return -1;
    }
    offset += read_uint32(data, &sequence_number);
    return offset;
}

int RtmpAcknowledgementPacket::get_pkg_len()
{
    return 4;
}

int RtmpAcknowledgementPacket::get_cs_id()
{
    return RTMP_CID_ProtocolControl;
}

int RtmpAcknowledgementPacket::get_msg_type()
{
    return RTMP_MSG_Acknowledgement;
}

int RtmpAcknowledgementPacket::encode_pkg(uint8_t *payload, int size)
{
    int offset = 0;

    if (size < 4)
    {
        return -1;
    }
    offset += write_int32(payload+offset, sequence_number);
    return offset;
}

RtmpSetChunkSizePacket::RtmpSetChunkSizePacket()
{
    chunk_size = 128;
}

RtmpSetChunkSizePacket::~RtmpSetChunkSizePacket()
{

}

int RtmpSetChunkSizePacket::decode(uint8_t *data, int len)
{
    int offset = 0;

    if (len < 4)
    {
        return -1;
    }
    offset += read_int32(data, &chunk_size);
    return offset;
}

int RtmpSetChunkSizePacket::get_pkg_len()
{
    return 4;
}

int RtmpSetChunkSizePacket::get_cs_id()
{
    return RTMP_CID_ProtocolControl;
}

int RtmpSetChunkSizePacket::get_msg_type()
{
    return RTMP_MSG_SetChunkSize;
}

int RtmpSetChunkSizePacket::encode_pkg(uint8_t *payload, int size)
{
    int offset = 0;

    if (size < 4)
    {
        return -1;
    }
    offset += write_int32(payload+offset, chunk_size);
    return offset;
}

RtmpSetPeerBandwidthPacket::RtmpSetPeerBandwidthPacket()
{
    bandwidth = 0;
    type = RtmpPeerBandwidthDynamic;
}

RtmpSetPeerBandwidthPacket::~RtmpSetPeerBandwidthPacket()
{

}

int RtmpSetPeerBandwidthPacket::decode(uint8_t *data, int len)
{
    int offset = 0;

    if (len < 5)
    {
        return -1;
    }
    offset += read_int32(data+offset, &bandwidth);
    offset += read_uint8(data+offset, &type);
    return offset;
}

int RtmpSetPeerBandwidthPacket::get_pkg_len()
{
    return 5;
}

int RtmpSetPeerBandwidthPacket::get_cs_id()
{
    return RTMP_CID_ProtocolControl;
}

int RtmpSetPeerBandwidthPacket::get_msg_type()
{
    return RTMP_MSG_SetPeerBandwidth;
}

int RtmpSetPeerBandwidthPacket::encode_pkg(uint8_t *payload, int size)
{
    int offset = 0;

    if (size < 5)
    {
        return -1;
    }
    offset += write_int32(payload+offset, bandwidth);
    offset += write_uint8(payload+offset, type);
    return offset;
}

RtmpUserControlPacket::RtmpUserControlPacket()
{
    event_type = 0;
    event_data = 0;
    extra_data = 0;
}

RtmpUserControlPacket::~RtmpUserControlPacket()
{

}

int RtmpUserControlPacket::decode(uint8_t *data, int len)
{
    return 0;
}

int RtmpUserControlPacket::get_pkg_len()
{
    int length = 2;

    if (event_type == RtmpPCUCFmsEvent0)
    {
        length += 1;
    }
    else
    {
        length += 4;
    }
    if (event_type == RtmpPCUCSetBufferLength)
    {
        length += 4;
    }
    return length;
}

int RtmpUserControlPacket::get_cs_id()
{
    return RTMP_CID_ProtocolControl;
}

int RtmpUserControlPacket::get_msg_type()
{
    return RTMP_MSG_UserControlMessage;
}

int RtmpUserControlPacket::encode_pkg(uint8_t *payload, int size)
{
    int offset = 0;

    if (size < get_pkg_len())
    {
        return -1;
    }
    offset += write_int16(payload+offset, event_type);
    if (event_type == RtmpPCUCFmsEvent0)
    {
        offset += write_int8(payload+offset, event_data);
    }
    else
    {
        offset += write_int32(payload+offset, event_data);
    }
    if (event_type == RtmpPCUCSetBufferLength)
    {
        offset += write_int32(payload+offset, extra_data);
    }
    return offset;
}

RtmpCallPacket::RtmpCallPacket() {
    command_name = "";
    number = 0;
    object = nullptr;
    args = nullptr;
}

RtmpCallPacket::~RtmpCallPacket() {
    if (object) {
        delete object;
    }
    if (args) {
        delete args;
    }
}

int RtmpCallPacket::decode(uint8_t *data, int len) {
    int ret;
    int offset =- 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    if (command_name.empty()) {
        return -2;
    }
    ret = rtmp_amf0_read_number(data+offset, len-offset, number);
    if (ret < 0) {
        return -1;
    }
    offset += ret;
    if (object) {
        delete object;
    }
    ret = RtmpAmf0Any::discovery(data+offset, len-offset, &object);
    if (ret < 0) {
        return ret;
    }
    ret = object->read(data+offset, len-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    if (offset < len) {
        if (args) {
            delete args;
        }
        ret = RtmpAmf0Any::discovery(data+offset, len-offset, &args);
        if (ret < 0) {
            return ret;
        }
        ret = args->read(data+offset, len-offset);
        if (ret < 0) {
            return ret;
        }
        offset += ret;
    }
    return offset;
}

int RtmpCallPacket::get_pkg_len() {
    int size = 0;

    size += RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number();
    if (object) {
        size += RtmpAmf0Size::any(object);
    }
    if (args) {
        size += RtmpAmf0Size::any(args);
    }
    return size;
}

int RtmpCallPacket::get_cs_id() {
    return RTMP_CID_OverConnection;
}

int RtmpCallPacket::get_msg_type() {
    return RTMP_MSG_AMF0CommandMessage;
}

int RtmpCallPacket::encode_pkg(uint8_t *payload, int size) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return -1;
    }
    offset += ret;

    ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0) {
        return -1;
    }
    offset += ret;

    if (object) {
        ret = object->write(payload+offset, size-offset);
        if (ret < 0) {
            return -1;
        }
        offset += ret;
    }

    if (args) {
        ret = args->write(payload+offset, size-offset);
        if (ret < 0) {
            return -1;
        }
        offset += ret;
    }
    return offset;
}

RtmpCallResponsePacket::RtmpCallResponsePacket(double id) {
    command_name = "_result";
    number = id;
    object = nullptr;
    response = nullptr;
}

RtmpCallResponsePacket::~RtmpCallResponsePacket() {
    if (object) {
        delete object;
    }
    if (response) {
        delete response;
    }
}

int RtmpCallResponsePacket::decode(uint8_t *data, int len) {
    return 0;
}

int RtmpCallResponsePacket::get_pkg_len() {
    int size = 0;

    size += RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number();
    if (object) {
        size += RtmpAmf0Size::any(object);
    }
    if (response) {
        size += RtmpAmf0Size::any(response);
    }
    return size;
}

int RtmpCallResponsePacket::get_cs_id() {
    return RTMP_CID_OverConnection;
}

int RtmpCallResponsePacket::get_msg_type() {
    return RTMP_MSG_AMF0CommandMessage;
}

int RtmpCallResponsePacket::encode_pkg(uint8_t *payload, int size) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;

    ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;

    if (object) {
        ret = object->write(payload+offset, size-offset);
        if (ret < 0) {
            return ret;
        }
        offset += ret;
    }
    if (response) {
        ret = response->write(payload+offset, size-offset);
        if (ret < 0) {
            return ret;
        }
        offset += ret;
    }
    return offset;
}

RtmpCreateStreamPacket::RtmpCreateStreamPacket() {
    command_name = "createStream";
    number = 2;
    object = RtmpAmf0Any::null();
}

RtmpCreateStreamPacket::~RtmpCreateStreamPacket() {
    if (object) {
        delete object;
    }
}

int RtmpCreateStreamPacket::decode(uint8_t *data, int len) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    if (command_name.empty() || command_name != "createStream") {
        return -2;
    }
    ret = rtmp_amf0_read_number(data+offset, len-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_null(data+offset, len-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

int RtmpCreateStreamPacket::get_pkg_len() {
    return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number() + RtmpAmf0Size::null();
}

int RtmpCreateStreamPacket::get_cs_id() {
    return RTMP_CID_OverConnection;
}

int RtmpCreateStreamPacket::get_msg_type() {
    return RTMP_MSG_AMF0CommandMessage;
}

int RtmpCreateStreamPacket::encode_pkg(uint8_t *payload, int size) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;

    ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;

    ret = rtmp_amf0_write_null(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpCreateStreamResponsePacket::RtmpCreateStreamResponsePacket(double id, double streamid) {
    command_name = "_result";
    number = id;
    object = RtmpAmf0Any::null();
    streamid_ = streamid;
}

RtmpCreateStreamResponsePacket::~RtmpCreateStreamResponsePacket() {
    if (object) {
        delete object;
    }
}

int RtmpCreateStreamResponsePacket::decode(uint8_t *data, int len) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    if (command_name.empty() || command_name != "_result") {
        return -2;
    }
    offset += ret;
    ret = rtmp_amf0_read_number(data+offset, len-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_null(data+offset, len-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_number(data+offset, len-offset, streamid_);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

int RtmpCreateStreamResponsePacket::get_pkg_len() {
    return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number() + RtmpAmf0Size::null() + RtmpAmf0Size::number();
}

int RtmpCreateStreamResponsePacket::get_cs_id() {
    return RTMP_CID_OverConnection;
}

int RtmpCreateStreamResponsePacket::get_msg_type() {
    return RTMP_MSG_AMF0CommandMessage;
}

int RtmpCreateStreamResponsePacket::encode_pkg(uint8_t *payload, int size) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_null(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_number(payload+offset, size-offset, streamid_);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpCloseStreamPacket::RtmpCloseStreamPacket() {
    command_name = "closeStream";
    number = 0;
    object = RtmpAmf0Any::null();
}

RtmpCloseStreamPacket::~RtmpCloseStreamPacket() {
    if (object) {
        delete object;
    }
}

int RtmpCloseStreamPacket::decode(uint8_t *data, int len) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_number(data+offset, len-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_null(data+offset, len-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

int RtmpCloseStreamPacket::get_pkg_len() {
    return 0;
}

int RtmpCloseStreamPacket::get_cs_id() {
    return 0;
}

int RtmpCloseStreamPacket::get_msg_type() {
    return 0;
}

int RtmpCloseStreamPacket::encode_pkg(uint8_t *payload, int size) {
    return 0;
}

RtmpFMLEStartPacket::RtmpFMLEStartPacket() {
    command_name = "releaseStream";
    number = 0;
    object = RtmpAmf0Any::null();
}

RtmpFMLEStartPacket::~RtmpFMLEStartPacket() {
    if (object) {
        delete object;
    }
}

int RtmpFMLEStartPacket::decode(uint8_t *data, int len) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    bool command_name_invalid = (command_name != "releaseStream" && command_name != "FCPublish" && command_name != "FCUnpublish");
    if (command_name.empty() || command_name_invalid) {
        return -2;
    }
    offset += ret;
    ret = rtmp_amf0_read_number(data+offset, len-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_null(data+offset, len-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_string(data+offset, len-offset, stream_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

int RtmpFMLEStartPacket::get_pkg_len() {
    return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number() + RtmpAmf0Size::null() + RtmpAmf0Size::str(stream_name);
}

int RtmpFMLEStartPacket::get_cs_id() {
    return RTMP_CID_OverConnection;
}

int RtmpFMLEStartPacket::get_msg_type() {
    return RTMP_MSG_AMF0CommandMessage;
}

int RtmpFMLEStartPacket::encode_pkg(uint8_t *payload, int size) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_null(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_string(payload+offset, size-offset, stream_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpFMLEStartPacket *RtmpFMLEStartPacket::create_release_stream(std::string stream) {
    RtmpFMLEStartPacket *packet = new RtmpFMLEStartPacket();
    packet->command_name = "releaseStream";
    packet->number = 2;
    packet->stream_name = stream;
    return packet;
}

RtmpFMLEStartPacket *RtmpFMLEStartPacket::create_FC_publish(std::string stream) {
    RtmpFMLEStartPacket *packet = new RtmpFMLEStartPacket();
    packet->command_name = "FCPublish";
    packet->number = 3;
    packet->stream_name = stream;
    return packet;
}

RtmpFMLEStartResponsePacket::RtmpFMLEStartResponsePacket(double id) {
    command_name = "_result";
    number = id;
    object = RtmpAmf0Any::null();
    args = RtmpAmf0Any::undefined();
}

RtmpFMLEStartResponsePacket::~RtmpFMLEStartResponsePacket() {
    if (object) {
        delete object;
    }
    if (args) {
        delete args;
    }
}

int RtmpFMLEStartResponsePacket::decode(uint8_t *data, int len) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    if (command_name.empty() || command_name != "_result") {
        return -2;
    }
    offset += ret;
    ret = rtmp_amf0_read_number(data+offset, len-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_null(data+offset, len-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_read_undefined(data+offset, len-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

int RtmpFMLEStartResponsePacket::get_pkg_len() {
    return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number() + RtmpAmf0Size::null() + RtmpAmf0Size::undefined();
}

int RtmpFMLEStartResponsePacket::get_cs_id() {
    return RTMP_CID_OverConnection;
}

int RtmpFMLEStartResponsePacket::get_msg_type() {
    return RTMP_MSG_AMF0CommandMessage;
}

int RtmpFMLEStartResponsePacket::encode_pkg(uint8_t *payload, int size) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_null(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_undefined(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpPublishPacket::RtmpPublishPacket() {
	command_name = "publish";
	number = 0;
	object = RtmpAmf0Any::null();
	type = "live";
}

RtmpPublishPacket::~RtmpPublishPacket() {
	if (object) {
		delete object;
	}
}

int RtmpPublishPacket::decode(uint8_t *data, int len) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_read_string(data + offset, len - offset, command_name);
	if (ret < 0) {
		return ret;
	}
	if (command_name.empty() || command_name != "publish") {
		return -2;
	}
	offset += ret;
	ret = rtmp_amf0_read_number(data + offset, len - offset, number);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_read_null(data + offset, len - offset);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_read_string(data + offset, len - offset, stream_name);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	if (offset < len) {
		ret = rtmp_amf0_read_string(data + offset, len - offset, type);
		if (ret < 0) {
			return ret;
		}
		offset += ret;
	}
	return offset;
}

int RtmpPublishPacket::get_pkg_len() {
	return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number()
		+ RtmpAmf0Size::null() + RtmpAmf0Size::str(stream_name)
		+ RtmpAmf0Size::str(type);
}

int RtmpPublishPacket::get_cs_id() {
    return RTMP_CID_OverStream;
}

int RtmpPublishPacket::get_msg_type() {
    return RTMP_MSG_AMF0CommandMessage;
}

int RtmpPublishPacket::encode_pkg(uint8_t *payload, int size) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_write_string(payload + offset, size - offset, command_name);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_number(payload + offset, size - offset, number);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_null(payload + offset, size - offset);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_string(payload + offset, size - offset, stream_name);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_string(payload+offset,size-offset, type);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpPausePacket::RtmpPausePacket() {
	command_name = "pause";
	number = 0;
	object = RtmpAmf0Any::null();

	time = 0;
	pause = true;
}

RtmpPausePacket::~RtmpPausePacket() {
	if (object) {
		delete object;
	}
}

int RtmpPausePacket::decode(uint8_t *data, int len) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_read_string(data + offset, len - offset, command_name);
	if (ret < 0) {
		return ret;
	}
	if (command_name.empty() || command_name != "pause") {
		return -2;
	}
	offset += ret;
	ret = rtmp_amf0_read_number(data + offset, len - offset, number);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_read_null(data + offset, len - offset);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_read_boolean(data + offset, len - offset, pause);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_read_number(data + offset, len - offset, time);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	return offset;
}

int RtmpPausePacket::get_pkg_len() {
    return 0;
}

int RtmpPausePacket::get_cs_id() {
    return 0;
}

int RtmpPausePacket::get_msg_type() {
    return 0;
}

int RtmpPausePacket::encode_pkg(uint8_t *payload, int size) {
    return 0;
}

RtmpPlayPacket::RtmpPlayPacket() {
	command_name = "play";
	number = 0;
	object = RtmpAmf0Any::null();

	start = -2;
	duration = -1;
	reset = true;
}

RtmpPlayPacket::~RtmpPlayPacket() {
	if (object) {
		delete object;
	}
}

int RtmpPlayPacket::decode(uint8_t *data, int len) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_read_string(data + offset, len - offset, command_name);
	if (ret < 0) {
		return ret;
	}
	if (command_name.empty() || command_name != "play") {
		return -2;
	}
	offset += ret;
	ret = rtmp_amf0_read_number(data + offset, len - offset, number);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_read_null(data + offset, len - offset);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_read_string(data + offset, len - offset, stream_name);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	if (offset < len) {
		ret = rtmp_amf0_read_number(data + offset, len - offset, start);
		if (ret < 0) {
			return ret;
		}
		offset += ret;
	}
	if (offset < len) {
		ret = rtmp_amf0_read_number(data + offset, len - offset, duration);
		if (ret < 0) {
			return ret;
		}
		offset += ret;
	}
	if (offset < len) {
		RtmpAmf0Any *reset_value = nullptr;
		ret = rtmp_amf0_read_any(data + offset, len - offset, &reset_value);
		if (ret < 0) {
			return ret;
		}
		offset += ret;
		AutoFree(RtmpAmf0Any, reset_value);
		if (reset_value) {
			if (reset_value->is_boolean()) {
				reset = reset_value->to_boolean();
			}
			else if (reset_value->is_number()) {
				reset = (reset_value->to_number() != 0);
			}
			else {
				return -2;
			}
		}
	}
	return offset;
}

int RtmpPlayPacket::get_pkg_len() {
	int size = RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number()
		+ RtmpAmf0Size::null() + RtmpAmf0Size::str(stream_name);

	if (start != -2 || duration != -1 || !reset) {
		size += RtmpAmf0Size::number();
	}

	if (duration != -1 || !reset) {
		size += RtmpAmf0Size::number();
	}

	if (!reset) {
		size += RtmpAmf0Size::boolean();
	}

	return size;
}

int RtmpPlayPacket::get_cs_id() {
	return RTMP_CID_OverStream;
}

int RtmpPlayPacket::get_msg_type() {
	return RTMP_MSG_AMF0CommandMessage;
}

int RtmpPlayPacket::encode_pkg(uint8_t *payload, int size) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_write_string(payload + offset, size - offset, command_name);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_number(payload + offset, size - offset, number);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_null(payload + offset, size - offset);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_string(payload + offset, size - offset, stream_name);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	if (start != -2 || duration != -1 || !reset) {
		ret = rtmp_amf0_write_number(payload + offset, size - offset, start);
		if (ret < 0) {
			return ret;
		}
		offset += ret;
	}
	if (duration != -1 || !reset) {
		ret = rtmp_amf0_write_number(payload + offset, size - offset, duration);
		if (ret < 0) {
			return ret;
		}
		offset += ret;
	}
	if (!reset) {
		ret = rtmp_amf0_write_boolean(payload + offset, size - offset, reset);
		if (ret < 0) {
			return ret;
		}
		offset += ret;
	}
	return offset;
}

RtmpPlayResponsePacket::RtmpPlayResponsePacket() {
	command_name = "_result";
	number = 0;
	object = RtmpAmf0Any::null();
	desc = RtmpAmf0Any::object();
}

RtmpPlayResponsePacket::~RtmpPlayResponsePacket() {
	if (object) {
		delete object;
	}
	if (desc) {
		delete desc;
	}
}

int RtmpPlayResponsePacket::decode(uint8_t *data, int len) {
	return 0;
}

int RtmpPlayResponsePacket::get_pkg_len() {
	return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number()
		+ RtmpAmf0Size::null() + RtmpAmf0Size::object(desc);
}

int RtmpPlayResponsePacket::get_cs_id() {
	return RTMP_CID_OverStream;
}

int RtmpPlayResponsePacket::get_msg_type() {
	return RTMP_MSG_AMF0CommandMessage;
}

int RtmpPlayResponsePacket::encode_pkg(uint8_t *payload, int size) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_write_string(payload + offset, size - offset, command_name);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_number(payload + offset, size - offset, number);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_null(payload + offset, size - offset);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	ret = desc->write(payload + offset, size - offset);
	if (ret < 0) {
		return ret;
	}
	offset += ret;
	return offset;
}

RtmpOnBWDonePacket::RtmpOnBWDonePacket() {
    command_name = "onBWDone";
    number = 0;
    args = RtmpAmf0Any::null();
}

RtmpOnBWDonePacket::~RtmpOnBWDonePacket() {
    if (args) {
        delete args;
    }
}

int RtmpOnBWDonePacket::decode(uint8_t * data, int len) {
	return 0;
}

int RtmpOnBWDonePacket::get_pkg_len() {
	return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number() + RtmpAmf0Size::null();
}

int RtmpOnBWDonePacket::get_cs_id() {
	return RTMP_CID_OverConnection;
}

int RtmpOnBWDonePacket::get_msg_type() {
	return RTMP_MSG_AMF0CommandMessage;
}

int RtmpOnBWDonePacket::encode_pkg(uint8_t * payload, int size) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
	if (ret < 0) {
	    return ret;
	}
	offset += ret;

	ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
	if (ret < 0) {
	    return ret;
	}
	offset += ret;

	ret = rtmp_amf0_write_null(payload+offset, size-offset);
	if (ret < 0) {
	    return ret;
	}
	offset += ret;
	return offset;
}

RtmpOnStatusCallPacket::RtmpOnStatusCallPacket() {
    command_name = "onStatus";
    number = 0;
    args = RtmpAmf0Any::null();
    data = RtmpAmf0Any::object();
}

RtmpOnStatusCallPacket::~RtmpOnStatusCallPacket() {
    if (args) {
        delete args;
    }
    if (data) {
        delete data;
    }
}

int RtmpOnStatusCallPacket::decode(uint8_t * data, int len) {
	return 0;
}

int RtmpOnStatusCallPacket::get_pkg_len() {
	return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::number() + RtmpAmf0Size::null() + RtmpAmf0Size::object(data);
}

int RtmpOnStatusCallPacket::get_cs_id() {
	return RTMP_CID_OverConnection;
}

int RtmpOnStatusCallPacket::get_msg_type() {
	return RTMP_MSG_AMF0CommandMessage;
}

int RtmpOnStatusCallPacket::encode_pkg(uint8_t * payload, int size) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
	if (ret < 0) {
	    return ret;
	}
	offset += ret;
	ret = rtmp_amf0_write_number(payload+offset, size-offset, number);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_null(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = data->write(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpOnStatusDataPacket::RtmpOnStatusDataPacket() {
    command_name = "onStatus";
    data = RtmpAmf0Any::object();
}

RtmpOnStatusDataPacket::~RtmpOnStatusDataPacket() {
    if (data) {
        delete data;
    }
}

int RtmpOnStatusDataPacket::decode(uint8_t * data, int len) {
	return 0;
}

int RtmpOnStatusDataPacket::get_pkg_len() {
	return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::object(data);
}

int RtmpOnStatusDataPacket::get_cs_id() {
	return RTMP_CID_OverStream;
}

int RtmpOnStatusDataPacket::get_msg_type() {
	return RTMP_MSG_AMF0DataMessage;
}

int RtmpOnStatusDataPacket::encode_pkg(uint8_t * payload, int size) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = data->write(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpSampleAccessPacket::RtmpSampleAccessPacket() {
    command_name = "|RtmpSampleAccess";
    video_sample_access = false;
    audio_sample_access = false;
}

RtmpSampleAccessPacket::~RtmpSampleAccessPacket() {
}

int RtmpSampleAccessPacket::decode(uint8_t * data, int len) {
	return 0;
}

int RtmpSampleAccessPacket::get_pkg_len() {
	return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::boolean() + RtmpAmf0Size::boolean();
}

int RtmpSampleAccessPacket::get_cs_id() {
	return RTMP_CID_OverStream;
}

int RtmpSampleAccessPacket::get_msg_type() {
	return RTMP_MSG_AMF0DataMessage;
}

int RtmpSampleAccessPacket::encode_pkg(uint8_t * payload, int size) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_boolean(payload+offset, size-offset, video_sample_access);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = rtmp_amf0_write_boolean(payload+offset, size-offset, audio_sample_access);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpOnMetaDataPacket::RtmpOnMetaDataPacket() {
    command_name = "onMetaData";
    metadata = RtmpAmf0Any::object();
}

RtmpOnMetaDataPacket::~RtmpOnMetaDataPacket() {
    if (metadata) {
        delete metadata;
    }
}

int RtmpOnMetaDataPacket::decode(uint8_t * data, int len) {
    int ret;
    int offset = 0;

    ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    if (command_name == "@setDataFrame") {
        ret = rtmp_amf0_read_string(data+offset, len-offset, command_name);
        if (ret < 0) {
            return ret;
        }
        offset += ret;
    }
    if (offset < len) {
        RtmpAmf0Any *any = nullptr;
        ret = rtmp_amf0_read_any(data+offset, len-offset, &any);
        if (ret < 0) {
            return ret;
        }
        offset += ret;
        if (any->is_object()) {
            if (metadata) {
                delete metadata;
            }
            metadata = any->to_object();
            return offset;
        }
        AutoFree(RtmpAmf0Any, any);
        if (any->is_ecma_array()) {
            RtmpAmf0EcmaArray *arr = any->to_ecma_array();

            for (int i = 0; i < arr->count(); i++) {
                metadata->set(arr->key_at(i), arr->value_at(i)->copy());
            }
        }
    }
	return offset;
}

int RtmpOnMetaDataPacket::get_pkg_len() {
	return RtmpAmf0Size::str(command_name) + RtmpAmf0Size::object(metadata);
}

int RtmpOnMetaDataPacket::get_cs_id() {
	return RTMP_CID_OverConnection2;
}

int RtmpOnMetaDataPacket::get_msg_type() {
	return RTMP_MSG_AMF0DataMessage;
}

int RtmpOnMetaDataPacket::encode_pkg(uint8_t * payload, int size) {
	int ret;
	int offset = 0;

	ret = rtmp_amf0_write_string(payload+offset, size-offset, command_name);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    ret = metadata->write(payload+offset, size-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

