#ifndef _TLS_H_
#define _TLS_H_

#include <string>

#include "openssl/ssl.h"
#include "openssl/err.h"

enum TlsRole
{
	TLS_ROLE_CLIENT = 0,
	TLS_ROLE_SERVER = 1,
};

class TlsCallback
{
public:
	virtual void onTlsConnectStatus(int status) = 0;
	virtual void onTlsReadData(char *buf, int size) = 0;
	virtual void onTlsWriteData(char *data, int len) = 0;

	virtual ~TlsCallback() = default;
};

class SecurityTransport
{
public:
	SecurityTransport(TlsCallback *cb);
	virtual ~SecurityTransport();

public:
	int init(std::string cert, std::string key, TlsRole r);
	int writeData(const char *buf, int len);
	int readData(const char *buf, int len);
	bool isConnected() const { return bConencted_; }

private:
	void send_bio_data();

private:
	SSL_CTX *ctx_;
	SSL *ssl_;
	BIO *rbio_;
	BIO *wbio_;
	bool bConencted_;
	TlsRole role;  //0 client  1 server
	TlsCallback *callback_;
};

#endif