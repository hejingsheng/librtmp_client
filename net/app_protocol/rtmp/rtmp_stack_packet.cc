//
// Created by hejingsheng on 2023/6/9.
//

#include "rtmp_stack_packet.h"
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
    *p++ = q[3];
    *p++ = q[2];
    *p++ = q[1];
    *p++ = q[0];
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
