#include <glib.h>
#include "netio.h"
#define SUPPORT_INT64 1

static int rawio_netdata_8(void* dest,const void* src) {
	*(uint8_t*)dest = *(uint8_t*)src;
	return sizeof(uint8_t);
}

#if G_BYTE_ORDER == G_BIG_ENDIAN
static int rawio_netdata_16(void* _dest,void* _src) {
	register uint8_t* dest = _dest;
	register uint8_t* src = _src;
	dest[0] = src[0];
	dest[1] = src[1];
	return sizeof(uint16_t);
}
static int rawio_netdata_32(void* _dest,void* _src) {
	register uint8_t* dest = _dest;
	register uint8_t* src = _src;
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
	dest[3] = src[3];
	return sizeof(uint32_t);
}

#if SUPPORT_INT64
static INLINE int rawio_netdata_64(void* _dest,const void* _src) {
	register T_UINT8* dest = _dest; register T_UINT8* src = _src;
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
	dest[3] = src[3];
	dest[4] = src[4];
	dest[5] = src[5];
	dest[6] = src[6];
	dest[7] = src[7];
	return sizeof(uint64_t);	
}
#endif
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
static int rawio_netdata_16(void* _dest,const void* _src) {
	register uint8_t* dest = (uint8_t*)_dest; 
	register uint8_t* src = (uint8_t*)_src;
	dest[0] = src[1];
	dest[1] = src[0];
	return sizeof(uint16_t);
}
static int rawio_netdata_32(void* _dest,const void* _src) {
	register uint8_t* dest = (uint8_t*)_dest; 
	register uint8_t* src = (uint8_t*)_src;
	dest[0] = src[3];
	dest[1] = src[2];
	dest[2] = src[1];
	dest[3] = src[0];
	return sizeof(uint32_t);
}

#if SUPPORT_INT64
static int rawio_netdata_64(void* _dest,const void* _src) {
	register uint8_t* dest = (uint8_t*)_dest;
	register uint8_t* src = (uint8_t*)_src;
	dest[0] = src[7];
	dest[1] = src[6];
	dest[2] = src[5];
	dest[3] = src[4];
	dest[4] = src[3];
	dest[5] = src[2];
	dest[6] = src[1];
	dest[7] = src[0];
	return sizeof(uint64_t);	
}
#endif
#else
#error "G_BYTE_ORDER should be big or little endian."
#endif

#if SUPPORT_INT64
int write_uint64(uint8_t* buffer,uint64_t d) {
	return rawio_netdata_64(buffer,&d);
}

int write_int64(uint8_t* buffer,int64_t d) {
	return rawio_netdata_64(buffer,&d);
}

int read_uint64(const uint8_t* buffer,uint64_t* d) {
	return rawio_netdata_64(d,buffer);
}

int read_int64(const uint8_t* buffer,int64_t* d) {
	return rawio_netdata_64(d,buffer);
}
#endif


int write_uint8(uint8_t* buffer, uint8_t d) {
	return rawio_netdata_8(buffer,&d);
}

int write_uint16(uint8_t* buffer, uint16_t d) {
	return rawio_netdata_16(buffer,&d);
}

int write_uint32(uint8_t* buffer, uint32_t d) {
	return rawio_netdata_32(buffer,&d);
}



int write_int8(uint8_t* buffer,int8_t d) {
	return rawio_netdata_8(buffer,&d);
}

int write_int16(uint8_t* buffer,int16_t d) {
	return rawio_netdata_16(buffer,&d);
}

int write_int32(uint8_t* buffer,int32_t d) {
	return rawio_netdata_32(buffer,&d);
}


int read_uint8(const uint8_t* buffer,uint8_t* d) {
	return rawio_netdata_8(d,buffer);
}

int read_uint16(const uint8_t* buffer,uint16_t* d) {
	return rawio_netdata_16(d,(void*)buffer);
}

int read_uint32(const uint8_t* buffer,uint32_t* d) {
	return rawio_netdata_32(d,(void*)buffer);
}


int read_int8(const uint8_t* buffer,int8_t* d) {
	return rawio_netdata_8(d,(void*)buffer);
}

int read_int16(const uint8_t* buffer,int16_t* d) {
	return rawio_netdata_16(d,(void*)buffer);
}

int read_int32(const uint8_t* buffer,int32_t* d) {
	return rawio_netdata_32(d,(void*)buffer);
}




