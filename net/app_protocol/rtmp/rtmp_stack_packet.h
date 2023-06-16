//
// Created by hejingsheng on 2023/6/9.
//

#ifndef RTMP_CLIENT_RTMP_STACK_PACKET_H
#define RTMP_CLIENT_RTMP_STACK_PACKET_H

#include <string>
#include <vector>
#include "app_protocol/rtmp/rtmp_stack_amf0.h"

#define RTMP_AMF0_NUMBER  0x00
#define RTMP_AMF0_BOOLEAN 0x01
#define RTMP_AMF0_STRING  0x02
#define RTMP_AMF0_OBJECT  0x03
#define RTMP_AMF0_MOVIE   0x04
#define RTMP_AMF0_NULL    0x05
#define RTMP_AMF0_UNDEFINE  0x06
#define RTMP_AMF0_REFERENCE  0x07
#define RTMP_AMF0_ECMAARRAY  0x08
#define RTMP_AMF0_OBJECTEND  0x09
#define RTMP_AMF0_STRICTARRAY  0x0a
#define RTMP_AMF0_DATE  0x0b
#define RTMP_AMF0_LONGSTRING  0x0c
#define RTMP_AMF0_UNSUPPORT  0x0d
#define RTMP_AMF0_RECORD  0x0e
#define RTMP_AMF0_XMLDOC  0x0f
#define RTMP_AMF0_TYPEOBJ  0x10
#define RTMP_AMF0_AVMPLUSOBJ  0x11
#define RTMP_AMF0_ORIGINSTRICTARRAY  0x20
#define RTMP_AMF0_INVALID  0x3f

#define RTMP_CHUNK_FMT0_TYPE 0
#define RTMP_CHUNK_FMT1_TYPE 1
#define RTMP_CHUNK_FMT2_TYPE 2
#define RTMP_CHUNK_FMT3_TYPE 3

#define RTMP_CHUNK_FMT0_HEADER_MAX_SIZE 16
#define RTMP_CHUNK_FMT1_HEADER_MAX_SIZE 12
#define RTMP_CHUNK_FMT2_HEADER_MAX_SIZE 8
#define RTMP_CHUNK_FMT3_HEADER_MAX_SIZE 5

#define RTMP_EXTENDED_TIMESTAMP    0xffffff

// 5. Protocol Control Messages
// RTMP reserves message type IDs 1-7 for protocol control messages.
// These messages contain information needed by the RTM Chunk Stream
// protocol or RTMP itself. Protocol messages with IDs 1 & 2 are
// reserved for usage with RTM Chunk Stream protocol. Protocol messages
// with IDs 3-6 are reserved for usage of RTMP. Protocol message with ID
// 7 is used between edge server and origin server.
#define RTMP_MSG_SetChunkSize                   0x01
#define RTMP_MSG_AbortMessage                   0x02
#define RTMP_MSG_Acknowledgement                0x03
#define RTMP_MSG_UserControlMessage             0x04
#define RTMP_MSG_WindowAcknowledgementSize      0x05
#define RTMP_MSG_SetPeerBandwidth               0x06
#define RTMP_MSG_EdgeAndOriginServerCommand     0x07
// 3. Types of messages
// The server and the client send messages over the network to
// communicate with each other. The messages can be of any type which
// includes audio messages, video messages, command messages, shared
// object messages, data messages, and user control messages.
// 3.1. Command message
// Command messages carry the AMF-encoded commands between the client
// and the server. These messages have been assigned message type value
// of 20 for AMF0 encoding and message type value of 17 for AMF3
// encoding. These messages are sent to perform some operations like
// connect, createStream, publish, play, pause on the peer. Command
// messages like onstatus, result etc. are used to inform the sender
// about the status of the requested commands. A command message
// consists of command name, transaction ID, and command object that
// contains related parameters. A client or a server can request Remote
// Procedure Calls (RPC) over streams that are communicated using the
// command messages to the peer.
#define RTMP_MSG_AMF3CommandMessage             17 // 0x11
#define RTMP_MSG_AMF0CommandMessage             20 // 0x14
// 3.2. Data message
// The client or the server sends this message to send Metadata or any
// user data to the peer. Metadata includes details about the
// data(audio, video etc.) like creation time, duration, theme and so
// on. These messages have been assigned message type value of 18 for
// AMF0 and message type value of 15 for AMF3.
#define RTMP_MSG_AMF0DataMessage                18 // 0x12
#define RTMP_MSG_AMF3DataMessage                15 // 0x0F
// 3.3. Shared object message
// A shared object is a Flash object (a collection of name value pairs)
// that are in synchronization across multiple clients, instances, and
// so on. The message types kMsgContainer=19 for AMF0 and
// kMsgContainerEx=16 for AMF3 are reserved for shared object events.
// Each message can contain multiple events.
#define RTMP_MSG_AMF3SharedObject               16 // 0x10
#define RTMP_MSG_AMF0SharedObject               19 // 0x13
// 3.4. Audio message
// The client or the server sends this message to send audio data to the
// peer. The message type value of 8 is reserved for audio messages.
#define RTMP_MSG_AudioMessage                   8 // 0x08
// 3.5. Video message
// The client or the server sends this message to send video data to the
// peer. The message type value of 9 is reserved for video messages.
// These messages are large and can delay the sending of other type of
// messages. To avoid such a situation, the video message is assigned
// The lowest priority.
#define RTMP_MSG_VideoMessage                   9 // 0x09
// 3.6. Aggregate message
// An aggregate message is a single message that contains a list of submessages.
// The message type value of 22 is reserved for aggregate
// messages.
#define RTMP_MSG_AggregateMessage               22 // 0x16
// The chunk stream id used for some under-layer message,
// For example, the PC(protocol control) message.
#define RTMP_CID_ProtocolControl                0x02
// The AMF0/AMF3 command message, invoke method and return the result, over NetConnection.
// generally use 0x03.
#define RTMP_CID_OverConnection                 0x03
// The AMF0/AMF3 command message, invoke method and return the result, over NetConnection,
// The midst state(we guess).
// rarely used, e.g. onStatus(NetStream.Play.Reset).
#define RTMP_CID_OverConnection2                0x04
// The stream message(amf0/amf3), over NetStream.
// generally use 0x05.
#define RTMP_CID_OverStream                     0x05
// The stream message(amf0/amf3), over NetStream, the midst state(we guess).
// rarely used, e.g. play("mp4:mystram.f4v")
#define RTMP_CID_OverStream2                    0x08
// The stream message(video), over NetStream
// generally use 0x06.
#define RTMP_CID_Video                          0x06
// The stream message(audio), over NetStream.
// generally use 0x07.
#define RTMP_CID_Audio                          0x07

class RtmpHeader
{
public:
    // base header 1 byte
    uint8_t chunk_type;   // 2 bits
    uint32_t chunk_stream_id;  // 6 bits
    // timestamp 3 bytes;
    uint32_t timestamp;  // 3 bytes
    // message header 8 bytes
    uint32_t msg_length; // 3 bytes
    uint8_t msg_type_id; // 1 byte
    uint32_t msg_stream_id; // 4 bytes

public:
    RtmpHeader();
    virtual ~RtmpHeader();

public:
    int encode(uint8_t *data, int size);
    int decode(uint8_t *data, int len);

private:
    int decode_base_header(uint8_t *data, int length, uint8_t &fmt, uint32_t &cs_id);
    int decode_msg_header(uint8_t *data, int length);

private:
    int encode_fmt0_chunk_header(uint8_t *data, int size);
    int encode_fmt1_chunk_header(uint8_t *data, int size);
    int encode_fmt2_chunk_header(uint8_t *data, int size);
    int encode_fmt3_chunk_header(uint8_t *data, int size);
};

class RtmpChunkData
{

public:
    RtmpChunkData();
    virtual ~RtmpChunkData();

public:
    void create_payload(int length);
    void copy_payload(uint8_t *data, int len);
    uint32_t get_current_len() const { return current_payload_len; }
    void reset() { current_payload_len = 0; payload = nullptr;}
    uint8_t* getPaylaod() const { return payload; }

public:
    RtmpHeader h;
    uint32_t time_delta;

private:
    uint8_t *payload;
    uint32_t current_payload_len;
};

class RtmpBasePacket
{
public:
    RtmpBasePacket() {

    }
    virtual ~RtmpBasePacket() {

    }

public:
    virtual int encode(uint8_t *&payload, int &size) ;

public:
    virtual int encode_pkg(uint8_t *payload, int size) = 0;
    virtual int decode(uint8_t *data, int len) = 0;
    virtual int get_pkg_len() = 0;

public:
    virtual int get_cs_id() = 0;
    virtual int get_msg_type() = 0;
};

class RtmpConnectPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Object *command_object;

public:
    RtmpConnectPacket();
    virtual ~RtmpConnectPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpConnectResponsePacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Object *object;
    RtmpAmf0Object *info;

public:
    RtmpConnectResponsePacket();
    virtual ~RtmpConnectResponsePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpSetWindowAckSizePacket : public RtmpBasePacket
{
public:
    int32_t window_ack_size;

public:
    RtmpSetWindowAckSizePacket();
    virtual ~RtmpSetWindowAckSizePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpAcknowledgementPacket : public RtmpBasePacket
{
public:
    uint32_t sequence_number;

public:
    RtmpAcknowledgementPacket();
    virtual ~RtmpAcknowledgementPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);

};

class RtmpSetChunkSizePacket : public RtmpBasePacket
{
public:
    int32_t chunk_size;

public:
    RtmpSetChunkSizePacket();
    virtual ~RtmpSetChunkSizePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

enum RtmpPeerBandwidthType
{
    RtmpPeerBandwidthHard = 0,
    RtmpPeerBandwidthSoft = 1,
    RtmpPeerBandwidthDynamic = 0,
};

class RtmpSetPeerBandwidthPacket : public RtmpBasePacket
{
public:
    int32_t bandwidth;
    uint8_t type;

public:
    RtmpSetPeerBandwidthPacket();
    virtual ~RtmpSetPeerBandwidthPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

enum RtmpPCUCEventType
{
    RtmpPCUCStreamBegin = 0x00,
    RtmpPCUCStreamEof = 0x01,
    RtmpPCUCStreamDry = 0x02,
    RtmpPCUCSetBufferLength = 0x03,
    RtmpPCUCStreamIsRecorded = 0x04,
    RtmpPCUCPingRequest = 0x06,
    RtmpPCUCPingResponse = 0x07,
    RtmpPCUCFmsEvent0 = 0x1a,
};

class RtmpUserControlPacket : public RtmpBasePacket
{
public:
    int16_t event_type;
    int32_t event_data;
    int32_t extra_data;

public:
    RtmpUserControlPacket();
    virtual ~RtmpUserControlPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpCallPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    RtmpAmf0Any *args;

public:
    RtmpCallPacket();
    virtual ~RtmpCallPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpCallResponsePacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    RtmpAmf0Any *response;

public:
    RtmpCallResponsePacket(double id);
    virtual ~RtmpCallResponsePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpCreateStreamPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;

public:
    RtmpCreateStreamPacket();
    virtual ~RtmpCreateStreamPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpCreateStreamResponsePacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    double streamid_;

public:
    RtmpCreateStreamResponsePacket(double id, double streamid);
    virtual ~RtmpCreateStreamResponsePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpCloseStreamPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;

public:
    RtmpCloseStreamPacket();
    virtual ~RtmpCloseStreamPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpFMLEStartPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    std::string stream_name;

public:
    RtmpFMLEStartPacket();
    virtual ~RtmpFMLEStartPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);

public:
    static RtmpFMLEStartPacket* create_release_stream(std::string stream);
    static RtmpFMLEStartPacket* create_FC_publish(std::string stream);
};

class RtmpFMLEStartResponsePacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    RtmpAmf0Any *args;

public:
    RtmpFMLEStartResponsePacket(double id);
    virtual ~RtmpFMLEStartResponsePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpPublishPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    std::string stream_name;
    std::string type;

public:
    RtmpPublishPacket();
    virtual ~RtmpPublishPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpPausePacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    bool pause;
    double time;

public:
    RtmpPausePacket();
    virtual ~RtmpPausePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpPlayPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    std::string stream_name;
    double start;
    double duration;
    bool reset;

public:
    RtmpPlayPacket();
    virtual ~RtmpPlayPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpPlayResponsePacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *object;
    RtmpAmf0Object *desc;

public:
    RtmpPlayResponsePacket();
    virtual ~RtmpPlayResponsePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpOnBWDonePacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *args;

public:
    RtmpOnBWDonePacket();
    virtual ~RtmpOnBWDonePacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpOnStatusCallPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    double number;
    RtmpAmf0Any *args;
    RtmpAmf0Object *data;

public:
    RtmpOnStatusCallPacket();
    virtual ~RtmpOnStatusCallPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpOnStatusDataPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    RtmpAmf0Object *data;

public:
    RtmpOnStatusDataPacket();
    virtual ~RtmpOnStatusDataPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpSampleAccessPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    bool video_sample_access;
    bool audio_sample_access;

public:
    RtmpSampleAccessPacket();
    virtual ~RtmpSampleAccessPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

class RtmpOnMetaDataPacket : public RtmpBasePacket
{
public:
    std::string command_name;
    RtmpAmf0Object *metadata;

public:
    RtmpOnMetaDataPacket();
    virtual ~RtmpOnMetaDataPacket();

public:
    virtual int decode(uint8_t *data, int len);
    virtual int get_pkg_len();
    virtual int get_cs_id();
    virtual int get_msg_type();

public:
    virtual int encode_pkg(uint8_t *payload, int size);
};

#endif //RTMP_CLIENT_RTMP_STACK_PACKET_H
