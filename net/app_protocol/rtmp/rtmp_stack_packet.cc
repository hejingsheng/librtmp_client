//
// Created by hejingsheng on 2023/6/9.
//

#include "rtmp_stack_packet.h"
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
    if (chunk_type == 0)
    {
        return encode_12byte_chunk_header(data, size);
    }
    else if (chunk_type == 1)
    {
        return encode_8byte_chunk_header(data, size);
    }
    else if (chunk_type == 2)
    {
        return encode_4byte_chunk_header(data, size);
    }
    else if (chunk_type == 3)
    {
        return encode_1byte_chunk_header(data, size);
    }
    else
    {
        return -1;
    }
}

int RtmpHeader::decode(uint8_t *data, int len)
{
    return 0;
}

int RtmpHeader::encode_12byte_chunk_header(uint8_t *data, int size)
{
    uint8_t *p = data;
    uint8_t *q;
    if (size < RTMP_CHUNK_FMT0_HEADER_MAX_SIZE)
    {
        return -1;
    }
    // base header(chunk type + chunk stream id) 1 byte
    *p++ = 0x00 | (chunk_stream_id & 0x3f);
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

int RtmpHeader::encode_8byte_chunk_header(uint8_t *data, int size)
{
    uint8_t *p = data;
    uint8_t *q;
    if (size < RTMP_CHUNK_FMT1_HEADER_MAX_SIZE)
    {
        return -1;
    }
    // base header(chunk type + chunk stream id) 1 byte
    *p++ = 0x40 | (chunk_stream_id & 0x3f);
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

int RtmpHeader::encode_4byte_chunk_header(uint8_t *data, int size)
{
    uint8_t *p = data;
    uint8_t *q;
    if (size < RTMP_CHUNK_FMT2_HEADER_MAX_SIZE)
    {
        return -1;
    }
    // base header(chunk type + chunk stream id) 1 byte
    *p++ = 0x80 | (chunk_stream_id & 0x3f);
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

int RtmpHeader::encode_1byte_chunk_header(uint8_t *data, int size)
{
    uint8_t *p = data;
    uint8_t *q;
    if (size < RTMP_CHUNK_FMT3_HEADER_MAX_SIZE)
    {
        return -1;
    }
    // base header(chunk type + chunk stream id) 1 byte
    *p++ = 0xC0 | (chunk_stream_id & 0x3f);
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

RtmpHeader * RtmpBasePacket::get_header()
{
    return &header;
}

void RtmpBasePacket::set_header(RtmpHeader *h)
{
    header = *h;
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