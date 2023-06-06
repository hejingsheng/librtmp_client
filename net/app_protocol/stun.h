#ifndef _STUN_H_
#define _STUN_H_

#include <map>
#include <vector>

#include "uv.h"
#include "NetCore.h"

enum StunMessageType
{
	StunBindRequest         = 0x0001,
	StunBindRespones        = 0x0101,
	StunBindError           = 0x0111,
	StunShareSecretRequest  = 0x0002,
	StunShareSecretResponse = 0x0102,
	StunShareSecretError    = 0x0112,
};

enum StunMessageAttribute
{
	// see @ https://tools.ietf.org/html/rfc3489#section-11.2
	MappedAddress = 0x0001,
	ResponseAddress = 0x0002,
	ChangeRequest = 0x0003,
	SourceAddress = 0x0004,
	ChangedAddress = 0x0005,
	Username = 0x0006,
	Password = 0x0007,
	MessageIntegrity = 0x0008,
	ErrorCode = 0x0009,
	UnknownAttributes = 0x000A,
	ReflectedFrom = 0x000B,

	// see @ https://tools.ietf.org/html/rfc5389#section-18.2
	Realm = 0x0014,
	Nonce = 0x0015,
	XorMappedAddress = 0x0020,
	Software = 0x8022,
	AlternateServer = 0x8023,
	Fingerprint = 0x8028,

	Priority = 0x0024,
	UseCandidate = 0x0025,
	IceControlled = 0x8029,
	IceControlling = 0x802A,
	OtherAddress = 0x802C,
};

enum StunChangeMsgAttr
{
	StunChangePort = 1 << 1,
	StunChangeIp = 1 << 2,
};

enum StunCallbackErrCode
{
	StunErrorNone = 0,
	StunErrorTimeOut = -1,
	StunErrorServer = -2,
	StunErrorCancel = -3,
};

class IStunCallback
{
public:
	virtual void onStunFail(StunCallbackErrCode err) = 0;
	virtual void onStunNatMap(NetCore::IPAddr &mapAddr, NetCore::IPAddr &changeAddr) = 0;
	virtual void onStunSendData(const char *data, int len) = 0;

	virtual ~IStunCallback() = default;
};

class StunMsgPacket
{
public:
	StunMsgPacket();
	virtual ~StunMsgPacket();

public:
	uint16_t stunMsgType;
	uint16_t stunMsgLenght;
	uint32_t stunMagicCookie;
	uint8_t stunTranId[12];
	std::map<uint16_t, std::string> stunAttribute;

public:
	void encode(char *buf, int size, std::string &password);
	void decode(const char *data, int len);
};

class STUNBase
{
public:
	STUNBase();
	virtual ~STUNBase();

public:
	virtual void init(IStunCallback *callback);
	virtual void onRecvStunData(const char *data, int len);
	virtual void stopStun();

public:
	virtual void requestStun() = 0;
	virtual void responseStun(NetCore::IPAddr &addr) = 0;
	virtual void setChangeRequest(int changeValue) = 0;
	virtual void setUseCandidate() = 0;
	virtual void clearChangeRequest() = 0;

protected:
	IStunCallback *stunCb_;
	StunMsgPacket packet;
};

class STUNServer : public STUNBase
{
public:
	STUNServer(std::string password);
	virtual ~STUNServer();

public:
	virtual void responseStun(NetCore::IPAddr &addr);
	virtual void requestStun();
	virtual void setChangeRequest(int changeValue);
	virtual void setUseCandidate();
	virtual void clearChangeRequest();

private:
	void buildBindResponse(char *data, int size, int &len, NetCore::IPAddr &addr);

private:
	std::string password_;
		
};

class STUNClient : public STUNBase
{
public:
	STUNClient(uv_loop_t *loop, std::string username, std::string password);
	virtual ~STUNClient();

public:
	virtual void requestStun();
	virtual void responseStun(NetCore::IPAddr &addr);
	virtual void onRecvStunData(const char *data, int len);
	virtual void setChangeRequest(int changeValue);
	virtual void setUseCandidate();
	virtual void clearChangeRequest();
	virtual void stopStun();

private:
	void stunRequest();
	void startRetransmit();
	void stopTimer();
	void onStopTimer();
	void buildBindRequest(char *data, int size, int &len);
	static void onTimer(uv_timer_t *handle);

private:
	uv_timer_t *retryTimer_;

	int maxRetryNum;
	int initRtt;
	bool stop_;
	std::string username_;
	std::string password_;
	std::vector<std::string> transportIdVec_;
	std::map<uint16_t, std::string> attribute;
};

#endif