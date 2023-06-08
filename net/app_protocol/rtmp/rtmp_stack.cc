//
// Created by hejingsheng on 2023/6/7.
//

#include "rtmp_stack.h"
#include <string>
#include "autofree.h"
#include "logger.h"
#include "netio.h"
#include "string.h"

// for hmacsha256
#include "openssl/evp.h"
#include "openssl/hmac.h"
#include "openssl/dh.h"

#define RTMP_SIG_HANDSHAKR "RTMP_LIB(0.0.1)"

uint8_t FMSKey[] = {
    0x47, 0x65, 0x6e, 0x75, 0x69, 0x6e, 0x65, 0x20,
    0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x46, 0x6c,
    0x61, 0x73, 0x68, 0x20, 0x4d, 0x65, 0x64, 0x69,
    0x61, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
    0x20, 0x30, 0x30, 0x31, // Genuine Adobe Flash Media Server 001
    0xf0, 0xee, 0xc2, 0x4a, 0x80, 0x68, 0xbe, 0xe8,
    0x2e, 0x00, 0xd0, 0xd1, 0x02, 0x9e, 0x7e, 0x57,
    0x6e, 0xec, 0x5d, 0x2d, 0x29, 0x80, 0x6f, 0xab,
    0x93, 0xb8, 0xe6, 0x36, 0xcf, 0xeb, 0x31, 0xae
}; // 68

uint8_t FPKey[] = {
    0x47, 0x65, 0x6E, 0x75, 0x69, 0x6E, 0x65, 0x20,
    0x41, 0x64, 0x6F, 0x62, 0x65, 0x20, 0x46, 0x6C,
    0x61, 0x73, 0x68, 0x20, 0x50, 0x6C, 0x61, 0x79,
    0x65, 0x72, 0x20, 0x30, 0x30, 0x31, // Genuine Adobe Flash Player 001
    0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8,
    0x2E, 0x00, 0xD0, 0xD1, 0x02, 0x9E, 0x7E, 0x57,
    0x6E, 0xEC, 0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB,
    0x93, 0xB8, 0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE
}; // 62

//rand()随机数生成
void random_generate(char* bytes, int size)
{
    static bool _random_initialized = false;
    if (!_random_initialized) {
        srand(0);
        _random_initialized = true;
    }

    for (int i = 0; i < size; i++) {
        // the common value in [0x0f, 0xf0]
        bytes[i] = 0x0f + (rand() % (256 - 0x0f - 0x0f));
    }
}

int do_openssl_HMACsha256(HMAC_CTX *ctx, char *data, int data_size, char *digest, unsigned int *digest_size)
{
    int ret = 0;
    if (HMAC_Update(ctx, (unsigned char *)data, data_size) < 0)
    {
        return -1;
    }
    if (HMAC_Final(ctx, (unsigned char *)digest, digest_size) < 0)
    {
        return -1;
    }
    return ret;
}

int openssl_HMACsha256(unsigned char *key, int key_size, char *data, int data_size, char *digest)
{
    int ret;
    unsigned int digest_size = 0;
    HMAC_CTX *ctx = HMAC_CTX_new();
    if (ctx == NULL)
    {
        return -1;
    }
    if (HMAC_Init_ex(ctx, key, key_size, EVP_sha256(), NULL) < 0)
    {
        HMAC_CTX_free(ctx);
        return -1;
    }
    ret = do_openssl_HMACsha256(ctx, data, data_size, digest, &digest_size);
    HMAC_CTX_free(ctx);
    if (ret != 0)
    {
        return -1;
    }
    if (digest_size != 32)
    {
        return -1;
    }
    return ret;
}

rtmp_handshake_key::rtmp_handshake_key()
{
    offset = rand();
    random0 = random1 = nullptr;

    int valid_offset = calc_offset();
    random0_size = valid_offset;
    if (random0_size > 0) {
        random0 = new char[random0_size];
        random_generate(random0, random0_size);
        snprintf(random0, random0_size, "%s", RTMP_SIG_HANDSHAKR);
    }
    random_generate(key, sizeof(key));
    random1_size = 764 - valid_offset - 128 - 4;
    if (random1_size > 0) {
        random1 = new char[random1_size];
        random_generate(random1, random1_size);
        snprintf(random1, random1_size, "%s", RTMP_SIG_HANDSHAKR);
    }
}

rtmp_handshake_key::~rtmp_handshake_key()
{
    if (random0)
    {
        delete[] random0;
    }
    if (random1)
    {
        delete[] random1;
    }
}

int rtmp_handshake_key::parse_key(const char *data, int size)
{
    int index = 0;
    if (size < 764)
    {
        return -1;
    }
    //offset = data[763] << 24 | data[762] << 16 | data[761] << 8 | data[760];
    read_uint32((uint8_t*)data+760, &offset);
    int valid_offset = calc_offset();
    random0_size = valid_offset;
    if (random0_size > 0)
    {
        if (random0)
        {
            delete[] random0;
        }
        random0 = new char[random0_size];
        memcpy(random0, data+index, random0_size);
        index += random0_size;
    }
    memcpy(key, data+index, 128);
    index += 128;
    random1_size = 764 - valid_offset - 128 - 4;
    if (random1_size > 0)
    {
        if (random1)
        {
            delete[] random1;
        }
        random1 = new char[random1_size];
        memcpy(random1, data+index, random1_size);
        index += random1_size;
    }
    return index+4;
}

int rtmp_handshake_key::calc_offset()
{
    int max_offset_size = (764 - 128 - 4);
    int valid_offset = 0;
    uint8_t *pp = (uint8_t*)&offset;
    valid_offset += *pp++;
    valid_offset += *pp++;
    valid_offset += *pp++;
    valid_offset += *pp++;
    return valid_offset % max_offset_size;
}

rtmp_handshake_digest::rtmp_handshake_digest()
{
    offset = rand();
    random0 = random1 = nullptr;

    int valid_offset = calc_offset();
    random0_size = valid_offset;
    if (random0_size > 0) {
        random0 = new char[random0_size];
        random_generate(random0, random0_size);
        snprintf(random0, random0_size, "%s", RTMP_SIG_HANDSHAKR);
    }
    random_generate(digest, sizeof(digest));
    random1_size = 764 - 4 - valid_offset - 32;
    if (random1_size > 0) {
        random1 = new char[random1_size];
        random_generate(random1, random1_size);
        snprintf(random1, random1_size, "%s", RTMP_SIG_HANDSHAKR);
    }
}

rtmp_handshake_digest::~rtmp_handshake_digest()
{
    if (random0)
    {
        delete[] random0;
    }
    if (random1)
    {
        delete[] random1;
    }
}

int rtmp_handshake_digest::parse_digest(const char *data, int size)
{
    int index = 0;
    if (size < 764)
    {
        return -1;
    }
    index += read_uint32((uint8_t*)data+index, &offset);
    int valid_offset = calc_offset();
    random0_size = valid_offset;
    if (random0_size > 0)
    {
        if (random0)
        {
            delete[] random0;
        }
        random0 = new char[random0_size];
        memcpy(random0, data+index, random0_size);
        index += random0_size;
    }
    memcpy(digest, data+index, 32);
    index += 32;
    random1_size = 764 - 4 - valid_offset - 32;
    if (random1_size > 0)
    {
        if (random1)
        {
            delete[] random1;
        }
        random1 = new char[random1_size];
        memcpy(random1, data+index, random1_size);
        index += random1_size;
    }
    return index;
}

int rtmp_handshake_digest::calc_c1_digest(char *joinbytes, int len)
{
    int ret;
    char *c1_digest;

    c1_digest = new char[128];
    AutoFreeA(char, c1_digest);
    ret = openssl_HMACsha256(FPKey, 30, joinbytes, len, c1_digest);
    if (ret < 0)
    {
        return -1;
    }
    memcpy(digest, c1_digest, 32);
    return 0;
}

int rtmp_handshake_digest::calc_offset()
{
    int max_offset_size = (764 - 32 - 4);
    int valid_offset = 0;
    uint8_t *pp = (uint8_t*)&offset;
    valid_offset += *pp++;
    valid_offset += *pp++;
    valid_offset += *pp++;
    valid_offset += *pp++;
    return valid_offset % max_offset_size;
}

rtmp_handshake::rtmp_handshake()
{
    c0c1 = s0s1s2 = c2 = nullptr;
    index = 0;
}

rtmp_handshake::~rtmp_handshake()
{
    delete[] c0c1;
    delete[] s0s1s2;
    delete[] c2;
}

int rtmp_handshake::create_c0c1(schema_type type)
{
    char *c1_payload;
    int offset = 0;
    c1_payload = new char[1536-32];
    AutoFreeA(char, c1_payload);

    time = (uint32_t)::time(NULL);
    version = 0x80000702;
    offset = build_time(c1_payload);
    offset += build_version(c1_payload+offset);
    // left 1537 - 9 = 1528 bytes
    if (schema_type0 == type)
    {
        // time + version + key + digest
        offset += build_key(c1_payload+offset);
        offset += build_digest(c1_payload+offset, false);
    }
    else
    {
        // time + version + digest + key
        offset += build_digest(c1_payload+offset, false);
        offset += build_key(c1_payload+offset);
    }
    digest.calc_c1_digest(c1_payload, 1536-32);
    general_c0c1(type);
    return 0;
}

int rtmp_handshake::create_c2()
{
    int ret = 0;

    c2 = new char[1536];
    memcpy(c2, s0s1s2+1, 1536);
//    index = 0;
//    random_generate(c2, 1536);
//
//    index += write_uint32((uint8_t*)c2+index, (uint32_t)::time(NULL));
//    memcpy(c2+index, s0s1s2+1, 4);
//    index += 4;
//    char temp_key[128];
//    ret = openssl_HMACsha256(FPKey, 62, digest.digest, 32, temp_key);
//    if (ret != 0)
//    {
//        return -1;
//    }
//    char temp_digest[128];
//    ret = openssl_HMACsha256(temp_key, 32, c2, )

    return ret;
}

int rtmp_handshake::process_s0s1s2(const char *data, int len)
{
    char ver;
    int offset = 0;
    if (len < 3073)
    {
        return -1;
    }
    s0s1s2 = new char[3073];
    memcpy(s0s1s2, data, len);
    ver = data[offset];
    offset += 1;
    if (ver != 0x03)
    {
        ELOG("version error handshake fail\n");
        return -2;
    }
    offset += read_uint32((uint8_t*)data+offset, &time);
    offset += read_uint32((uint8_t*)data+offset, &version);
    offset += digest.parse_digest(data+offset, 764);
    offset += key.parse_key(data+offset+764, 764);
    return offset;
}

int rtmp_handshake::build_time(char *payload)
{
    return write_uint32((uint8_t*)payload, time);
}

int rtmp_handshake::build_version(char *payload)
{
    return write_uint32((uint8_t*)payload, version);
}

int rtmp_handshake::build_key(char *payload)
{
    int i = 0;
    if (key.random0_size > 0)
    {
        memcpy(payload+i, key.random0, key.random0_size);
        i += key.random0_size;
    }
    memcpy(payload+i, key.key, 128);
    i += 128;
    if (key.random1_size > 0)
    {
        memcpy(payload+i, key.random1, key.random1_size);
        i += key.random1_size;
    }
    i += write_uint32((uint8_t*)payload+i, key.offset);
    return i;
}

int rtmp_handshake::build_digest(char *payload, bool with_digest)
{
    int i = 0;
    i += write_uint32((uint8_t*)payload+i, digest.offset);
    if (digest.random0_size > 0)
    {
        memcpy(payload+i, digest.random0, digest.random0_size);
        i += digest.random0_size;
    }
    if (with_digest)
    {
        memcpy(payload + i, digest.digest, 32);
        i += 32;
    }
    if (key.random1_size > 0)
    {
        memcpy(payload+i, digest.random1, digest.random1_size);
        i += digest.random1_size;
    }
    return i;
}

int rtmp_handshake::general_c0c1(schema_type type)
{
    index = 0;
    c0c1 = new char[1+1536];
    c0c1[index] = 0x03;
    index += 1;
    index += build_time(c0c1+index);
    index += build_version(c0c1+index);
    if (schema_type0 == type)
    {
        // time + version + key + digest
        index += build_key(c0c1+index);
        index += build_digest(c0c1+index, true);

    }
    else
    {
        // time + version + digest + key
        index += build_digest(c0c1+index, true);
        index += build_key(c0c1+index);
    }
    return index;
}
