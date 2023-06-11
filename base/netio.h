#ifndef _NETIO_H_
#define _NETIO_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * \fn set_netdata
 *	д�����л�����������
 *
 * \param[out] buffer ����ռ�
 * \param d			Ҫд�������
 *
 * \return			д�������ֽ���
 */

int write_uint8(uint8_t* buffer, uint8_t d);
int write_uint16(uint8_t* buffer,uint16_t d);
int write_uint32(uint8_t* buffer,uint32_t d);
int write_uint64(uint8_t* buffer,uint64_t d);
int write_int8(uint8_t* buffer,int8_t d);
int write_int16(uint8_t* buffer,int16_t d);
int write_int32(uint8_t* buffer,int32_t d);
int write_int64(uint8_t* buffer,int64_t d);


/**
 * \fn read_netdata
 *  �����л����������ж�������
 *
 * \param[out] buffer	���ݻ���
 * \param[out] d			�������յ�����
 *
 * \return			�����������ֽڴ�С
 */

int read_uint8(const uint8_t* buffer, uint8_t* d);
int read_uint16(const uint8_t* buffer, uint16_t* d);
int read_uint32(const uint8_t* buffer, uint32_t* d);
int read_uint64(const uint8_t* buffer, uint64_t* d);
int read_int8(const uint8_t* buffer, int8_t* d);
int read_int16(const uint8_t* buffer, int16_t* d);
int read_int32(const uint8_t* buffer, int32_t* d);
int read_int64(const uint8_t* buffer, int64_t* d);


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



