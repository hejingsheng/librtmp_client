#ifndef _NETIO_H_
#define _NETIO_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * \fn set_netdata
 *	写入序列化的整数类型
 *
 * \param[out] buffer 缓存空间
 * \param d			要写入的整数
 *
 * \return			写入数据字节数
 */

int write_uint8(uint8_t* buffer, uint8_t d);
int write_uint16(uint8_t* buffer,uint16_t d);
int write_uint32(uint8_t* buffer,uint32_t d);
int write_int8(uint8_t* buffer,int8_t d);
int write_int16(uint8_t* buffer,int16_t d);
int write_int32(uint8_t* buffer,int32_t d);


/**
 * \fn read_netdata
 *  从序列化的数据流中读出整数
 *
 * \param[out] buffer	数据缓存
 * \param[out] d			用来接收的整数
 *
 * \return			读出整数的字节大小
 */

int read_uint8(const uint8_t* buffer, uint8_t* d);
int read_uint16(const uint8_t* buffer, uint16_t* d);
int read_uint32(const uint8_t* buffer, uint32_t* d);
int read_int8(const uint8_t* buffer, int8_t* d);
int read_int16(const uint8_t* buffer, int16_t* d);
int read_int32(const uint8_t* buffer, int32_t* d);


#if SUPPORT_INT64
int write_uint64(uint8_t* buffer,uint64_t d);
int write_int64(uint8_t* buffer,int64_t d);
int read_uint64(const uint8_t* buffer,uint64_t* d);
int read_int64(const uint8_t* buffer,int64_t* d);
#endif

#ifdef __cplusplus
}
#endif

#endif



