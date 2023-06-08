//
// Created by hejingsheng on 2023/6/7.
//

#ifndef RTMP_CLIENT_RTMP_STACK_H
#define RTMP_CLIENT_RTMP_STACK_H

#include <string>

enum schema_type {
    schema_type0 = 0,
    schema_type1 = 1,
};

class rtmp_handshake_key {

    /*
     C1、S1 key：764bytes
     random data：（offset）bytes
     key data：（128bytes） --可以秘钥
     random data（764-offset-128-4）bytes
     offset：4bytes（大端对应在0-764的正整数）
     */
public:
    // offset
    char *random0;
    int random0_size;
    //key
    char key[128];
    // 764 - offset - 128 - 4
    char *random1;
    int random1_size;
    // offset
    uint32_t offset;

public:
    rtmp_handshake_key();
    virtual ~rtmp_handshake_key();

public:
    int parse_key(const char *data, int size);

private:
    int calc_offset();
};

class rtmp_handshake_digest {

    /*
    digest： 764bytes
    offset: 4bytes
    random-data: (offset)bytes
    digest-data: 32bytes
    random-data: (764-4-offset-32)bytes
     */
public:
    // offset
    char *random0;
    int random0_size;
    //key
    char digest[32];
    // 764 - offset - 32 - 4
    char *random1;
    int random1_size;
    // offset
    uint32_t offset;

public:
    rtmp_handshake_digest();
    virtual ~rtmp_handshake_digest();

public:
    int parse_digest(const char *data, int size);
    int calc_c1_digest(char *joinbytes, int len);

private:
    int calc_offset();

};

class rtmp_handshake {

public:
    char *c0c1; // 1 + 1536
    char *s0s1s2; // 1 + 1536 + 1536;
    char *c2; // 1536
    int index;

public:
    rtmp_handshake();
    virtual ~rtmp_handshake();

public:
    // client handshake
    int create_c0c1(schema_type schema);
    int create_c2();
    int process_s0s1s2(const char *data, int len);

private:
    int build_time(char *payload);
    int build_version(char *payload);
    int build_key(char *payload);
    int build_digest(char *payload, bool with_digest);
    int general_c0c1(schema_type schema);

private:
    uint32_t time;
    uint32_t version;
    rtmp_handshake_key key;
    rtmp_handshake_digest digest;
};


#endif //RTMP_CLIENT_RTMP_STACK_H
