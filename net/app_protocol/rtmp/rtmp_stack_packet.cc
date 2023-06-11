//
// Created by hejingsheng on 2023/6/9.
//

#include "rtmp_stack_packet.h"

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

RtmpConnectPacket::RtmpConnectPacket()
{
    command_name = "connect";
    number = 1;
    command_object = RtmpAmf0Any::object();
}

RtmpConnectPacket::~RtmpConnectPacket()
{

}

int RtmpConnectPacket::encode(char *payload, int size)
{
    return 0;
}

int RtmpConnectPacket::decode(char *data, int len)
{
    return 0;
}

int RtmpConnectPacket::get_cs_id()
{
    return 0;
}

int RtmpConnectPacket::get_msg_type()
{
    return 0;
}

int RtmpConnectPacket::get_stream_id()
{
    return 0;
}