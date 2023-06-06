#include <sstream>
#include <openssl/hmac.h>
#include "string.h"
#include "stun.h"
#include "logger.h"

#define LITTLEENDIAN_ARCH 1

static int write_int8(void* dest, const void* src) {
	*(uint8_t*)dest = *(uint8_t*)src;
	return sizeof(uint8_t);
}

#if BIGENDIAN_ARCH
static int write_int16(void* _dest, void* _src) {
	register uint8_t* dest = _dest;
	register uint8_t* src = _src;
	dest[0] = src[0];
	dest[1] = src[1];
	return sizeof(uint16_t);
}
static int write_int32(void* _dest, void* _src) {
	register uint8_t* dest = _dest;
	register uint8_t* src = _src;
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
	dest[3] = src[3];
	return sizeof(uint32_t);
}
#endif
#if LITTLEENDIAN_ARCH
static int write_int16(void* _dest, const void* _src) {
	register uint8_t* dest = (uint8_t*)_dest; 
	register uint8_t* src = (uint8_t*)_src;
	dest[0] = src[1];
	dest[1] = src[0];
	return sizeof(uint16_t);
}
static int write_int32(void* _dest, const void* _src) {
	register uint8_t* dest = (uint8_t*)_dest; 
	register uint8_t* src = (uint8_t*)_src;
	dest[0] = src[3];
	dest[1] = src[2];
	dest[2] = src[1];
	dest[3] = src[0];
	return sizeof(uint32_t);
}
static int write_int64(void* _dest, const void* _src) {
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


static std::string general_random_str(int len)
{
	static std::string random_table = "01234567890123456789012345678901234567890123456789abcdefghijklmnopqrstuvwxyz";

	std::string ret;
	ret.reserve(len);
	for (int i = 0; i < len; ++i) {
		ret.append(1, random_table[rand() % random_table.size()]);
	}
	return ret;
}
static unsigned char SWAP_UINT8(unsigned char* p)
{
	if (!p)
		return 0;
	return (unsigned char)(*p);
}
static unsigned short SWAP_UINT16(unsigned char* p)
{
	unsigned short a = 0;
	unsigned char* pa = (unsigned char*)&a;
	pa[1] = SWAP_UINT8(p++);
	pa[0] = SWAP_UINT8(p);
	return a;
}
static unsigned int SWAP_UINT32(unsigned char* p)
{
	unsigned int a = 0;
	unsigned char* pa = (unsigned char*)&a;
	pa[3] = SWAP_UINT8(p++);
	pa[2] = SWAP_UINT8(p++);
	pa[1] = SWAP_UINT8(p++);
	pa[0] = SWAP_UINT8(p);
	return a;
}

static int hmac_encode(const std::string& algo, const char* key, const int& key_length,
	const char* input, const int input_length, char* output, unsigned int& output_length)
{
	int err = 0;

	const EVP_MD* engine = NULL;
	if (algo == "sha512") {
		engine = EVP_sha512();
	}
	else if (algo == "sha256") {
		engine = EVP_sha256();
	}
	else if (algo == "sha1") {
		engine = EVP_sha1();
	}
	else if (algo == "md5") {
		engine = EVP_md5();
	}
	else if (algo == "sha224") {
		engine = EVP_sha224();
	}
	else if (algo == "sha384") {
		engine = EVP_sha384();
	}
	else {
		return -1;
	}

	HMAC_CTX* ctx = HMAC_CTX_new();
	if (ctx == NULL) {
		return -1;
	}

	if (HMAC_Init_ex(ctx, key, key_length, engine, NULL) < 0) {
		HMAC_CTX_free(ctx);
		return -1;
	}

	if (HMAC_Update(ctx, (const unsigned char*)input, input_length) < 0) {
		HMAC_CTX_free(ctx);
		return -1;
	}

	if (HMAC_Final(ctx, (unsigned char*)output, &output_length) < 0) {
		HMAC_CTX_free(ctx);
		return -1;
	}

	HMAC_CTX_free(ctx);

	return err;
}

static const uint32_t crc32Table[] =
{
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
		0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
		0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
		0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
		0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
		0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
		0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
		0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
		0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
		0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
		0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
		0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
		0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
		0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
		0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
		0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
		0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
		0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
		0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
		0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
		0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
		0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
		0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
		0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
		0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
		0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
		0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
		0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
		0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
		0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
		0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

inline uint32_t GetCRC32(const uint8_t *data, size_t size) {
	uint32_t crc{ 0xFFFFFFFF };
	const uint8_t *p = data;

	while (size--) {
		crc = crc32Table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	}

	return crc ^ ~0U;
}

StunMsgPacket::StunMsgPacket()
{
	stunMsgType = 0;
	stunMsgLenght = 0;
	stunMagicCookie = 0;
}

StunMsgPacket::~StunMsgPacket()
{

}

void StunMsgPacket::encode(char *data, int size, std::string &password)
{
	char tmp[1024] = {0};
	char *p = data;
	char *q = tmp;
	uint16_t tmplen;
	uint16_t temptype;

	std::map<uint16_t, std::string>::iterator it;
	for (it = stunAttribute.begin(); it != stunAttribute.end(); it++)
	{
		uint16_t type = it->first;
		uint16_t len = it->second.length();
		uint16_t padding = 0;
		switch (type)
		{
		case Username:
		case Software:
			q += write_int16(q, (void*)&type);
			q += write_int16(q, (void*)&len);
			memcpy(q, it->second.c_str(), len);
			q += len;
			if (len % 4 != 0)
			{
				padding = 4 - (len % 4);
				memset(q, 0x00, padding);
				q += padding;
			}
			break;
		case MappedAddress:
			{
				size_t pos = it->second.find(":");
				uint16_t port;
				struct in_addr addr;
				std::string ipaddr;
				if (pos != std::string::npos)
				{
					q += write_int16(q, (void*)&type);
					len = 8;
					q += write_int16(q, (void*)&len);
					*q = 0x00;
					q += sizeof(uint8_t);
					*q = 0x01;
					q += sizeof(uint8_t);
					ipaddr = it->second.substr(0, pos);
					//inet_aton(ipaddr.c_str(), &addr);
					addr.s_addr = inet_addr(ipaddr.c_str());
					addr.s_addr = htonl(addr.s_addr);
					port = atoi(it->second.substr(pos + 1).c_str());
					q += write_int16(q, (void*)&port);
					q += write_int32(q, (void*)&(addr.s_addr));
				}
			}
			break;
		case XorMappedAddress:
			{
				size_t pos = it->second.find(":");
				uint16_t port;
				struct in_addr addr;
				std::string ipaddr;
				if (pos != std::string::npos)
				{
					q += write_int16(q, (void*)&type);
					len = 8;
					q += write_int16(q, (void*)&len);
					*q = 0x00;
					q += sizeof(uint8_t);
					*q = 0x01;
					q += sizeof(uint8_t);
					ipaddr = it->second.substr(0, pos);
					addr.s_addr = inet_addr(ipaddr.c_str());
					addr.s_addr = htonl(addr.s_addr);
					port = atoi(it->second.substr(pos + 1).c_str());
					port = port ^ ((stunMagicCookie & 0xffff0000) >> 16);
					q += write_int16(q, (void*)&port);
					uint32_t tmp = addr.s_addr ^ stunMagicCookie;
					q += write_int32(q, (void*)&tmp);
				}
			}
			break;
		case ChangeRequest:
			{
				len = 4;
				uint32_t value = atoi(it->second.c_str());
				q += write_int16(q, (void*)&type);
				q += write_int16(q, (void*)&len);
				q += write_int32(q, (void*)&value);
			}
			break;
		case IceControlling:
			{
				len = 8;
				uint64_t value = atoll(it->second.c_str());
				q += write_int16(q, (void*)&type);
				q += write_int16(q, (void*)&len);
				q += write_int64(q, (void*)&value);
			}
			break;
		case Priority:
			{
				len = 4;
				uint32_t value = atoi(it->second.c_str());
				q += write_int16(q, (void*)&type);
				q += write_int16(q, (void*)&len);
				q += write_int32(q, (void*)&value);
			}
			break;
		case UseCandidate:
			{
				len = 0;
				q += write_int16(q, (void*)&type);
				q += write_int16(q, (void*)&len);
			}
		default:
			break;
		}
	}
	stunAttribute.clear();
	p += write_int16(p, (void*)&stunMsgType);
	stunMsgLenght = q - tmp;
	p += write_int16(p, (void*)&stunMsgLenght);
	p += write_int32(p, (void*)&stunMagicCookie);
	memcpy(p, (void*)stunTranId, 12);
	p += 12;
	memcpy(p, tmp, stunMsgLenght);
	p += stunMsgLenght;

	data[2] = ((stunMsgLenght + 24) & 0x0000FF00) >> 8;
	data[3] = ((stunMsgLenght + 24) & 0x000000FF);
	char hmac_buf[20] = { 0 };
	unsigned int hmac_buf_len = 0;
	int err = 0;
	if ((err = hmac_encode("sha1", password.c_str(), password.size(), data, 20 + stunMsgLenght, hmac_buf, hmac_buf_len)) != 0) {
		return;
	}
	temptype = MessageIntegrity;
	p += write_int16(p, (void*)&temptype);
	tmplen = 20;
	p += write_int16(p, (void*)&tmplen);
	memcpy(p, hmac_buf, hmac_buf_len);
	p += hmac_buf_len;
	stunMsgLenght += 24;

	data[2] = ((stunMsgLenght + 8) & 0x0000FF00) >> 8;
	data[3] = ((stunMsgLenght + 8) & 0x000000FF);
	uint32_t crc32 = GetCRC32((const uint8_t *)data, 20 + stunMsgLenght) ^ 0x5354554E;
	temptype = Fingerprint;
	p += write_int16(p, (void*)&temptype);
	tmplen = 4;
	p += write_int16(p, (void*)&tmplen);
	p += write_int32(p, (void*)&crc32);
	stunMsgLenght += 8;
}

void StunMsgPacket::decode(const char *buf, int len)
{
	if (len < 20)
	{
		//error len
		return;
	}
	std::ostringstream ostr;
	const char *p = buf;
	int index = 0;
		
	stunMsgType = (p[0] << 8 | p[1]);
	stunMsgLenght = (p[2] << 8 | p[3]);
	stunMagicCookie |= ((p[4] & 0x000000ff) << 24);
	stunMagicCookie |= ((p[5] & 0x000000ff) << 16);
	stunMagicCookie |= ((p[6] & 0x000000ff) << 8);
	stunMagicCookie |= (p[7] & 0x000000ff);
	memcpy(stunTranId, p + 8, 12);
	index = 20;
	if (stunMsgType == StunBindRespones)
	{
		while (index < len)
		{
			uint16_t type = p[index] << 8 | p[index + 1];
			index += 2;
			uint16_t len = p[index] << 8 | p[index + 1];
			index += 2;
			std::string val = std::string(p + index, len);
			if (len % 4 != 0)
			{
				len = ((len / 4) + 1) * 4;
			}
			index += len;
			switch (type)
			{
			case Username:
			{
				stunAttribute.insert(std::make_pair(Username, val));
			}
			break;
			case XorMappedAddress:
			{
				char reserved = val.at(0);
				char protocolFamily = val.at(1);
				uint16_t port = ((val.at(2) & 0x00ff) << 8 | (val.at(3) & 0x00ff)) ^ ((stunMagicCookie & 0xffff0000) >> 16);
				uint32_t ipaddr = ((val.at(4) & 0x000000ff) << 24 | (val.at(5) & 0x000000ff) << 16 | (val.at(6) & 0x000000ff) << 8 | (val.at(7) & 0x000000ff)) ^ stunMagicCookie;
				ipaddr = ntohl(ipaddr);
				struct in_addr addr;
				addr.s_addr = ipaddr;
				char *tmp = inet_ntoa(addr);
				ostr << tmp << ":" << port;
				stunAttribute.insert(std::make_pair(XorMappedAddress, ostr.str()));
				ostr.str("");
			}
			break;
			case MappedAddress:
			{
				char reserved = val.at(0);
				char protocolFamily = val.at(1);
				uint16_t port = ((val.at(2) & 0x00ff) << 8 | (val.at(3) & 0x00ff));
				uint32_t ipaddr = ((val.at(4) & 0x000000ff) << 24 | (val.at(5) & 0x000000ff) << 16 | (val.at(6) & 0x000000ff) << 8 | (val.at(7) & 0x000000ff));
				ipaddr = ntohl(ipaddr);
				struct in_addr addr;
				addr.s_addr = ipaddr;
				char *tmp = inet_ntoa(addr);
				ostr << tmp << ":" << port;
				stunAttribute.insert(std::make_pair(MappedAddress, ostr.str()));
				ostr.str("");
			}
			break;
			case ChangedAddress:
			case OtherAddress:
			{
				char reserved = val.at(0);
				char protocolFamily = val.at(1);
				uint16_t port = ((val.at(2) & 0x00ff) << 8 | (val.at(3) & 0x00ff));
				uint32_t ipaddr = ((val.at(4) & 0x000000ff) << 24 | (val.at(5) & 0x000000ff) << 16 | (val.at(6) & 0x000000ff) << 8 | (val.at(7) & 0x000000ff));
				ipaddr = ntohl(ipaddr);
				struct in_addr addr;
				addr.s_addr = ipaddr;
				char *tmp = inet_ntoa(addr);
				ostr << tmp << ":" << port;
				stunAttribute.insert(std::make_pair(ChangedAddress, ostr.str()));
				ostr.str("");
			}
			break;
			default:
				//not deal 
				break;
			}
		}
	}
	else if (stunMsgType == StunBindError)
	{
			
	}
	else
	{

	}
}

STUNBase::STUNBase()
{

}

STUNBase::~STUNBase()
{
	stunCb_ = nullptr;
}

void STUNBase::init(IStunCallback *callback)
{
	stunCb_ = callback;
}

void STUNBase::onRecvStunData(const char *data, int len)
{
	packet.decode(data, len);
}

void STUNBase::stopStun()
{

}

STUNServer::STUNServer(std::string password) : password_(password)
{

}

STUNServer::~STUNServer()
{

}

void STUNServer::responseStun(NetCore::IPAddr &addr)
{
	char msg[1024] = { 0 };
	int len;

	buildBindResponse(msg, sizeof(msg), len, addr);
	if (stunCb_)
	{
		stunCb_->onStunSendData(msg, len);
	}
}

void STUNServer::requestStun()
{
	return;
}

void STUNServer::setChangeRequest(int changeValue)
{

}

void STUNServer::setUseCandidate()
{

}

void STUNServer::clearChangeRequest()
{

}

void STUNServer::buildBindResponse(char *data, int size, int &len, NetCore::IPAddr &addr)
{
	StunMsgPacket stunMsg;
	stunMsg.stunMsgType = StunBindRespones;
	stunMsg.stunMagicCookie = 0x2112a442;
	for (int i = 0; i < 12; i++)
	{
		stunMsg.stunTranId[i] = packet.stunTranId[i];
	}
	if (packet.stunAttribute.find(Username) != packet.stunAttribute.end())
	{
		stunMsg.stunAttribute.insert(std::make_pair(Username, packet.stunAttribute.find(Username)->second));
	}
	stunMsg.stunAttribute.insert(std::make_pair(XorMappedAddress, addr.toStr()));
	//stunMsg.stunAttribute.insert(std::make_pair(MappedAddress, addr.toStr()));
	//stunMsg.stunAttribute.insert(std::make_pair(Software, "Libuv App StunServer(HeJingsheng)"));
	stunMsg.encode(data, size, password_);
	len = stunMsg.stunMsgLenght + 20;
}

STUNClient::STUNClient(uv_loop_t *loop, std::string username, std::string password) : username_(username), password_(password)
{
	//retryTimer_ = new uv::Timer(loop, 1000, 0, std::bind(&STUNClient::startRetransmitTimer, this));
	retryTimer_ = new uv_timer_t;
	retryTimer_->data = static_cast<void*>(this);
	uv_timer_init(loop, retryTimer_);
	maxRetryNum = 5;
	stunCb_ = nullptr;
	initRtt = 500;
}

STUNClient::~STUNClient()
{
	if (retryTimer_)
	{
		delete retryTimer_;
		retryTimer_ = nullptr;
	}
	stunCb_ = nullptr;
	DLOG("destroy stun client\n");
}

void STUNClient::stunRequest()
{
	char msg[1024] = { 0 };
	int len;

	buildBindRequest(msg, sizeof(msg), len);
	if (stunCb_)
	{
		stunCb_->onStunSendData(msg, len);
	}
	//retryTimer_->stop();
	//retryTimer_->setTimeout(initRtt);
	//retryTimer_->start();
	uv_timer_stop(retryTimer_);
	uv_timer_start(retryTimer_, &STUNClient::onTimer, initRtt, 0);
	stop_ = false;
}

void STUNClient::startRetransmit()
{
	if (stunCb_)
	{
		maxRetryNum--;
		if (maxRetryNum <= 0)
		{
			stunCb_->onStunFail(StunErrorTimeOut);
		}
		else
		{
			initRtt = initRtt * 2;
			stunRequest();
		}
	}
}

void STUNClient::stopTimer()
{
	if (uv_is_active((uv_handle_t*)retryTimer_))
	{
		uv_timer_stop(retryTimer_);
	}
	if (uv_is_closing((uv_handle_t*)retryTimer_) == 0)
	{
		uv_close((uv_handle_t*)retryTimer_,
			[](uv_handle_t* handle)
		{
			auto ptr = static_cast<STUNClient*>(handle->data);
			//ptr->closeComplete();
			ptr->onStopTimer();
			//delete handle;
		});
	}
	else
	{
		//closeComplete();
		//close();
		onStopTimer();
	}
}

void STUNClient::onStopTimer()
{
	if (stop_)
	{
		stunCb_->onStunFail(StunErrorCancel);
	}
	else if (maxRetryNum <= 0)
	{
		stunCb_->onStunFail(StunErrorTimeOut);
	}
}

void STUNClient::buildBindRequest(char *data, int size, int &len)
{
	StunMsgPacket stunMsg;
	stunMsg.stunMsgType = StunBindRequest;
	stunMsg.stunMagicCookie = 0x2112a442;
	std::string transportId = general_random_str(12);
	transportIdVec_.push_back(transportId);
	for (int i = 0; i < 12; i++)
	{
		stunMsg.stunTranId[i] = transportId.at(i);
	}
	if (!username_.empty())
	{
		stunMsg.stunAttribute.insert(std::make_pair(Username, username_));
		//stunMsg.stunAttribute.insert(std::make_pair(XorMappedAddress, "192.168.0.200:10000"));
		//stunMsg.stunAttribute.insert(std::make_pair(MappedAddress, "8.135.38.10:5000"));
	}
	std::map<uint16_t, std::string>::iterator iter;
	for (iter = attribute.begin(); iter != attribute.end(); iter++)
	{
		stunMsg.stunAttribute.insert(std::make_pair(iter->first, iter->second));
	}
	//stunMsg.stunAttribute.insert(std::make_pair(ChangeRequest, " "));
	//stunMsg.stunAttribute.insert(std::make_pair(UseCandidate, ""));
	//stunMsg.stunAttribute.insert(std::make_pair(Software, "Libuv App Stun(HeJingsheng)"));
	stunMsg.stunAttribute.insert(std::make_pair(IceControlling, "88888888888"));
	stunMsg.stunAttribute.insert(std::make_pair(Priority, "1853824767"));
	stunMsg.encode(data, size, password_);
	len = stunMsg.stunMsgLenght + 20;
}

void STUNClient::onTimer(uv_timer_t *handle)
{
	auto data = static_cast<STUNClient*>(handle->data);
	data->startRetransmit();
}

void STUNClient::requestStun()
{
	maxRetryNum = 5;
	initRtt = 500;
	stunRequest();
}

void STUNClient::responseStun(NetCore::IPAddr &addr)
{
	return;
}

void STUNClient::onRecvStunData(const char *data, int len)
{
	STUNBase::onRecvStunData(data, len);
	if (packet.stunAttribute.find(XorMappedAddress) == packet.stunAttribute.end() && packet.stunAttribute.find(MappedAddress) == packet.stunAttribute.end())
	{
		stunCb_->onStunFail(StunErrorServer);
	}
	else
	{	
		std::string value;
		if (packet.stunAttribute.find(XorMappedAddress) != packet.stunAttribute.end()) 
		{
			value = packet.stunAttribute.find(XorMappedAddress)->second;
		}
		else
		{
			value = packet.stunAttribute.find(XorMappedAddress)->second;
		}
		size_t pos = value.find(":");
		uint16_t port;
		std::string ipaddr;

		ipaddr = value.substr(0, pos);
		const char *t = value.substr(pos + 1).c_str();
		port = atoi(value.substr(pos + 1).c_str());
		NetCore::IPAddr sockaddr;
		NetCore::IPAddr changeAddr;
		sockaddr.ip = std::move(ipaddr);
		sockaddr.port = port;
		if (packet.stunAttribute.find(ChangedAddress) != packet.stunAttribute.end())
		{
			value = packet.stunAttribute.find(ChangedAddress)->second;
			pos = value.find(":");
			ipaddr = value.substr(0, pos);
			port = atoi(value.substr(pos + 1).c_str());
			changeAddr.ip = std::move(ipaddr);
			changeAddr.port = port;
		}
		else
		{
			changeAddr.ip = "";
			changeAddr.port = -1;
		}
		stunCb_->onStunNatMap(sockaddr, changeAddr);
		//retryTimer_->stop();
		uv_timer_stop(retryTimer_);
	}
}

void STUNClient::setChangeRequest(int changeValue)
{
	attribute.insert(std::make_pair(ChangeRequest, std::to_string(changeValue)));
}

void STUNClient::setUseCandidate()
{
	attribute.insert(std::make_pair(UseCandidate, ""));
}

void STUNClient::clearChangeRequest()
{
	attribute.clear();
}

void STUNClient::stopStun()
{
	stop_ = true;
	stopTimer();
}