/*
Author: hejingsheng@qq.com

Date: 2021/11/26
*/
#ifndef _DTLS_H_
#define _DTLS_H_

extern "C"
{
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "uv.h"
}
#include <string>

namespace Dtls
{

	enum DtlsState {
		DtlsStateInit, // Start.
		DtlsStateClientHello, // Should start ARQ thread.
		DtlsStateServerHello, // We are in the first ARQ state.
		DtlsStateClientCertificate, // Should start ARQ thread again.
		DtlsStateServerDone, // We are in the second ARQ state.
		DtlsStateClientDone, // Done.
	};

	enum DtlsRole {
		DtlsRoleServer,
		DtlsRoleClient,
	};

	class IDtlsCallback
	{
	public:
		virtual void onDtlsHandShakeDone() = 0;
		virtual void onDtlsRecvData(const char *data, unsigned int len) = 0;
		virtual void onDtlsSendData(const char *data, unsigned int len) = 0;
		virtual void onDtlsAlert(std::string type, std::string desc) = 0;

		virtual ~IDtlsCallback() = default;
	};

	class DtlsBase
	{
	public:
		DtlsBase(IDtlsCallback *callback);
		virtual ~DtlsBase();

	public:
		virtual int init(std::string cert, std::string key, DtlsRole role);
		virtual void onMessage(const char *buf, unsigned int size) = 0;
		virtual int startHandShake() = 0;
		virtual void writeData(const char *buf, unsigned int size);
		virtual void alertCallback(std::string type, std::string desc);

	public:
		void checkRemoteCertificate(std::string &fingerprint);
		void getSrtpKey(std::string &recv_key, std::string &send_key);

	protected:
		void send_bio_data();

	protected:
		SSL_CTX *ctx_;
		SSL *ssl_;
		BIO *rbio_;
		BIO *wbio_;

		DtlsRole role_;
		IDtlsCallback *dtlsCallback_;
	};

	class DtlsServer : public DtlsBase
	{
	public:
		DtlsServer(IDtlsCallback *callback);
		virtual ~DtlsServer();

	public:
		virtual int init(std::string cert, std::string key, DtlsRole role);
		virtual void onMessage(const char *buf, unsigned int size);
		virtual int startHandShake();

	private:
		bool dtlsConnect_ = false;
		DtlsState dtlsStatus_;

	};

	class DtlsClient: public DtlsBase
	{
	public:
		DtlsClient(uv_loop_t *loop, IDtlsCallback *callback);
		virtual ~DtlsClient();

	public:
		virtual int init(std::string cert, std::string key, DtlsRole role);
		virtual void onMessage(const char *buf, unsigned int size);
		virtual int startHandShake();

	private:
		void stopNegotiation();
		void onStopNegotiation();
		void startRetransmitTimer();
		static void onTimer(uv_timer_t *handle);

	private:
		bool dtlsConnect_;
		DtlsState dtlsStatus_;
		int retryTime;

		uv_timer_t *arqTimer_;
		uint64_t arqTimeout_;
	};

}

#endif