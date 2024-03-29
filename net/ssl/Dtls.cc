/*
Author: hejingsheng@qq.com

Date: 2021/11/26
*/
#include "Dtls.h"
#include "SSLCertificate.h"
#include "string.h"
#include "srtp2/srtp.h"
#include "logger.h"

namespace Dtls
{

	/* DTLS alert callback */
	void uv_dtls_callback(const SSL *ssl, int where, int ret) 
	{
		/* We only care about alerts */
		if (!(where & SSL_CB_ALERT)) 
		{
			return;
		}
		DtlsBase *dtls = (DtlsBase*)SSL_get_ex_data(ssl, 0);
		if (!dtls) 
		{
			return;
		}
		std::string alert_type = SSL_alert_type_string_long(ret);
		std::string alert_desc = SSL_alert_desc_string_long(ret);
		//uv::LogWriter::Instance()->error("dtls alert");
		dtls->alertCallback(alert_type, alert_desc);
		return;
	}

	int uv_verify_callback(int isOk, X509_STORE_CTX *ctx)
	{
		// Always OK, we don't check the certificate of client,
		// because we allow client self-sign certificate.
		if (isOk == 1)
		{
			// check success
		}
		else
		{
			// isOk == 0  check fail
		}
		return 1;
	}

	DtlsBase::DtlsBase(IDtlsCallback *callback) : dtlsCallback_(callback)
	{
		ctx_ = NULL;
		ssl_ = NULL;
		rbio_ = NULL;
		wbio_ = NULL;
	}

	DtlsBase::~DtlsBase()
	{
		DLOG("destroy dtls base %p,%p\n", ctx_, ssl_);
		
		if (ssl_ != NULL)
		{
			SSL_free(ssl_);
			ssl_ = NULL;
		}
		if (ctx_ != NULL)
		{
			SSL_CTX_free(ctx_);
			ctx_ = NULL;
		}
		dtlsCallback_ = nullptr;
	}

	int DtlsBase::init(std::string cert, std::string key, DtlsRole role)
	{
		int ret = 0;
		role_ = role;
		if (role == DtlsRoleClient)
		{
#if OPENSSL_VERSION_NUMBER < 0x10002000L //v1.0.2
			ctx_ = SSL_CTX_new(DTLSv1_method());
#else
			ctx_ = SSL_CTX_new(DTLS_client_method());
#endif
		}
		else if (role == DtlsRoleServer)
		{
#if OPENSSL_VERSION_NUMBER < 0x10002000L //v1.0.2
			ctx_ = SSL_CTX_new(DTLSv1_method());
#else
			ctx_ = SSL_CTX_new(DTLS_server_method());
#endif
		}
		else
		{
			return -1;
		}
		if (ctx_ == NULL)
		{
			//uv::LogWriter::Instance()->error("create ssl ctx fail no memery");
			return -1;
		}
		if (SSLCertificate::getSSLCert()->isecdsa) // By ECDSA, https://stackoverflow.com/a/6006898
		{ 
#if OPENSSL_VERSION_NUMBER >= 0x10002000L // v1.0.2
			// For ECDSA, we could set the curves list.
			// @see https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_set1_curves_list.html
			SSL_CTX_set1_curves_list(ctx_, "P-521:P-384:P-256");
#endif

			// For openssl <1.1, we must set the ECDH manually.
			// @see https://stackoverrun.com/cn/q/10791887
#if OPENSSL_VERSION_NUMBER < 0x10100000L // v1.1.x
#if OPENSSL_VERSION_NUMBER < 0x10002000L // v1.0.2
			SSL_CTX_set_tmp_ecdh(ctx_, ssl_certificate.get_ssl_eckey());
#else
			SSL_CTX_set_ecdh_auto(ctx_, 1);
#endif
#endif
		}
		ret = SSL_CTX_set_cipher_list(ctx_, "ALL");
		if (ret != 1)
		{
			//uv::LogWriter::Instance()->error("SSL_CTX_set_cipher_list fail");
			return -1;
		}
		if (cert.empty() || key.empty())
		{
			//加载证书和私钥
			if (0 == SSL_CTX_use_certificate(ctx_, SSLCertificate::getSSLCert()->get_ssl_cert()))
			{
				ERR_print_errors_fp(stderr);
				return -1;
			}
			if (0 == SSL_CTX_use_PrivateKey(ctx_, SSLCertificate::getSSLCert()->get_ssl_pkey()))
			{
				ERR_print_errors_fp(stderr);
				return -1;
			}
		}
		else
		{
			//加载证书和私钥
			if (0 == SSL_CTX_use_certificate_file(ctx_, cert.c_str(), SSL_FILETYPE_PEM))
			{
				ERR_print_errors_fp(stderr);
				return -1;
			}
			if (0 == SSL_CTX_use_PrivateKey_file(ctx_, key.c_str(), SSL_FILETYPE_PEM))
			{
				ERR_print_errors_fp(stderr);
				return -1;
			}
		}
		if (!SSL_CTX_check_private_key(ctx_))
		{
			//uv::LogWriter::Instance()->error("Private key does not match the certificate public key");
			return -1;
		}
		SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, uv_verify_callback);
		SSL_CTX_set_verify_depth(ctx_, 4);
		SSL_CTX_set_read_ahead(ctx_, 1);

		//if (SSL_CTX_set_tlsext_use_srtp(ctx_, "SRTP_AES128_CM_SHA1_80:SRTP_AEAD_AES_256_GCM:SRTP_AEAD_AES_128_GCM") != 0)
		if (SSL_CTX_set_tlsext_use_srtp(ctx_, "SRTP_AES128_CM_SHA1_80") != 0)
		{
			return -1;
		}

		ssl_ = SSL_new(ctx_);
		if (ssl_ == NULL)
		{
			//uv::LogWriter::Instance()->warn("create ssl fail no memery");
			return -1;
		}

		SSL_set_ex_data(ssl_, 0, this);
		SSL_set_info_callback(ssl_, uv_dtls_callback);
		SSL_set_options(ssl_, SSL_OP_NO_QUERY_MTU);
		SSL_set_mtu(ssl_, 1500);

		rbio_ = BIO_new(BIO_s_mem());
		wbio_ = BIO_new(BIO_s_mem());
		SSL_set_bio(ssl_, rbio_, wbio_);
		return 0;
	}

	void DtlsBase::writeData(const char *buf, unsigned int size)
	{
		int err;
		char *data = NULL;
		int len;
		SSL_write(ssl_, buf, size);
		len = BIO_get_mem_data(wbio_, &data);
		if (len > 0 && data != NULL)
		{
			if (dtlsCallback_)
			{
				dtlsCallback_->onDtlsSendData(data, len);
			}
			BIO_reset(wbio_);
		}
		else
		{

		}
	}

	void DtlsBase::alertCallback(std::string type, std::string desc)
	{
		if (dtlsCallback_)
		{
			dtlsCallback_->onDtlsAlert(type, desc);
		}
	}

	void DtlsBase::checkRemoteCertificate(std::string &fingerprint)
	{
		X509 *cert;
		unsigned int rsize;
		unsigned char rfingerprint[EVP_MAX_MD_SIZE] = { 0 };
		char remote_fingerprint[160] = { 0 };
		char *rfp = (char *)&remote_fingerprint;
		int ret;

		if (ssl_ != nullptr)
		{
			cert = SSL_get_peer_certificate(ssl_);
			if (cert != nullptr)
			{
				ret = X509_cmp_current_time(X509_get_notAfter(cert));
				if (ret < 0)
				{
					alertCallback("warning", "certificate expired");
				}
				X509_digest(cert, EVP_sha256(), (unsigned char *)rfingerprint, &rsize);
				X509_free(cert);
				cert = NULL;
				for (unsigned int i = 0; i < rsize; i++)
				{
					snprintf(rfp, 4, "%.2X:", rfingerprint[i]);
					rfp += 3;
				}
				*(rfp - 1) = 0;
				fingerprint.assign(remote_fingerprint, strlen(remote_fingerprint));
			}
		}
	}

	const int SRTP_MASTER_KEY_KEY_LEN = 16;
	const int SRTP_MASTER_KEY_SALT_LEN = 14;
	void DtlsBase::getSrtpKey(std::string &recv_key, std::string &send_key)
	{
		unsigned char material[SRTP_MASTER_KEY_LEN * 2] = { 0 };  // client(SRTP_MASTER_KEY_KEY_LEN + SRTP_MASTER_KEY_SALT_LEN) + server
		static const std::string dtls_srtp_lable = "EXTRACTOR-dtls_srtp";

		if (SSL_export_keying_material(ssl_, material, SRTP_MASTER_KEY_LEN * 2, dtls_srtp_lable.c_str(), dtls_srtp_lable.length(), NULL, 0, 0) != 1)
		{
			return;
		}
		size_t offset = 0;

		std::string client_master_key(reinterpret_cast<char*>(material), SRTP_MASTER_KEY_KEY_LEN);
		offset += SRTP_MASTER_KEY_KEY_LEN;
		std::string server_master_key(reinterpret_cast<char*>(material + offset), SRTP_MASTER_KEY_KEY_LEN);
		offset += SRTP_MASTER_KEY_KEY_LEN;
		std::string client_master_salt(reinterpret_cast<char*>(material + offset), SRTP_MASTER_KEY_SALT_LEN);
		offset += SRTP_MASTER_KEY_SALT_LEN;
		std::string server_master_salt(reinterpret_cast<char*>(material + offset), SRTP_MASTER_KEY_SALT_LEN);

		if (role_ == DtlsRoleClient) {
			recv_key = server_master_key + server_master_salt;
			send_key = client_master_key + client_master_salt;
		}
		else {
			recv_key = client_master_key + client_master_salt;
			send_key = server_master_key + server_master_salt;
		}
	}

	void DtlsBase::send_bio_data()
	{
		char *data = NULL;
		int len;

		len = BIO_get_mem_data(wbio_, &data);
		if (data != NULL && len > 0)
		{
			BIO_reset(rbio_);
			if (dtlsCallback_)
			{
				dtlsCallback_->onDtlsSendData(data, len);
			}
			BIO_reset(wbio_);
		}
	}

	DtlsServer::DtlsServer(IDtlsCallback *callback) : DtlsBase(callback)
	{
		dtlsStatus_ = DtlsStateInit;
	}

	DtlsServer::~DtlsServer()
	{

	}

	int DtlsServer::init(std::string cert, std::string key, DtlsRole role)
	{
		int ret;
		ret = DtlsBase::init(cert, key, role);

		if (ret < 0)
		{
			//uv::LogWriter::Instance()->error("dtls server init fail");
			return -1;
		}

		SSL_set_accept_state(ssl_);
		return 0;
	}

	void DtlsServer::onMessage(const char *buf, unsigned int size)
	{
		int len;
		int r0, r1;
		len = BIO_write(rbio_, buf, size);
		if (!dtlsConnect_)
		{
			r0 = SSL_do_handshake(ssl_);
			r1 = SSL_get_error(ssl_, r0);
			if (r0 != 1)
			{
				send_bio_data();
				dtlsStatus_ = DtlsStateServerHello;
			}
			else if (r0 == 1)
			{
				send_bio_data();
				//uv::LogWriter::Instance()->info("DTLS connect success");
				if (r1 == SSL_ERROR_NONE)
				{
					dtlsConnect_ = true;
					dtlsStatus_ = DtlsStateServerDone;
					if (dtlsCallback_)
					{
						dtlsCallback_->onDtlsHandShakeDone();
					}
				}
			}
		}
		else
		{
			int ret;
			ret = SSL_read(ssl_, (void*)buf, size);
			if (ret <= 0)
			{
				int err = SSL_get_error(ssl_, ret);
				if (err == SSL_ERROR_WANT_READ)
				{
					// 在read回调函数中读取数据
				}
				else if (err == SSL_ERROR_WANT_WRITE)
				{
					send_bio_data();
				}
				else
				{
					// closed
				}
			}
			else
			{
				if (dtlsCallback_)
				{
					dtlsCallback_->onDtlsRecvData(buf, ret);
				}
			}
		}
	}

	int DtlsServer::startHandShake()
	{
		// wait client send handshake request
		return 0;
	}

	DtlsClient::DtlsClient(uv_loop_t *loop, IDtlsCallback *callback) : DtlsBase(callback)
	{
		dtlsConnect_ = false;
		dtlsStatus_ = DtlsStateInit;
		retryTime = 0;
		arqTimeout_ = 100;
		//arqTimer_ = new uv::Timer(loop, 100, 0, std::bind(&DtlsClient::startRetransmitTimer, this));
		arqTimer_ = new uv_timer_t;
		arqTimer_->data = static_cast<void*>(this);
		uv_timer_init(loop, arqTimer_);
	}

	DtlsClient::~DtlsClient()
	{
		DLOG("destroy dtls client\n");
		if (arqTimer_)
		{
			delete arqTimer_;
			arqTimer_ = nullptr;
		}
	}

	int DtlsClient::init(std::string cert, std::string key, DtlsRole role)
	{
		int ret;
		ret = DtlsBase::init(cert, key, role);
		if (ret < 0)
		{
			//uv::LogWriter::Instance()->error("dtls client init fail");
			return -1;
		}

		SSL_set_connect_state(ssl_);
		SSL_set_max_send_fragment(ssl_, 1500);
		return ret;
	}

	int DtlsClient::startHandShake()
	{
		int ret;

		ret = SSL_do_handshake(ssl_);
		if (ret == 1)
		{
			//uv::LogWriter::Instance()->warn("connect success");
			return 0;
		}
		else
		{
			int err = SSL_get_error(ssl_, ret);
			if (err == SSL_ERROR_WANT_WRITE)
			{
				ret = 1;
			}
			else if (err == SSL_ERROR_WANT_READ)
			{
				send_bio_data();
				dtlsStatus_ = DtlsStateClientHello;
				//arqTimer_->start();
				uv_timer_start(arqTimer_, &DtlsClient::onTimer, arqTimeout_, 0);
				ret = 1;
			}
			else
			{
				//uv::LogWriter::Instance()->error("ssl connect fail");
				ret = -1;
			}
		}
		return ret;
	}

	void DtlsClient::onMessage(const char *buf, unsigned int size)
	{
		int len;
		int r0, r1;
		len = BIO_write(rbio_, buf, size);
		if (!dtlsConnect_)
		{
			r0 = SSL_do_handshake(ssl_);
			r1 = SSL_get_error(ssl_, r0);
			if (r0 != 1)
			{
				if (r1 == SSL_ERROR_WANT_WRITE)
				{

				}
				else if (r1 == SSL_ERROR_WANT_READ)
				{
					send_bio_data();
				}
				if (dtlsStatus_ == DtlsStateClientHello)
				{
					dtlsStatus_ = DtlsStateClientCertificate;
				}
				//arqTimer_->stop();
				uv_timer_stop(arqTimer_);
				uv_timer_start(arqTimer_, &DtlsClient::onTimer, arqTimeout_, 0);
				//arqTimer_->setTimeout(100);
				//arqTimer_->start();
			}
			else if (r0 == 1)
			{
				//uv::LogWriter::Instance()->info("DTLS connect success");
				dtlsConnect_ = true;
				dtlsStatus_ = DtlsStateClientDone;
				//close arq timer
				//arqTimer_->close(nullptr);
				stopNegotiation();
			}
		}
		else
		{
			int ret;
			ret = SSL_read(ssl_, (void*)buf, size);
			if (ret <= 0)
			{
				int err = SSL_get_error(ssl_, ret);
				if (err == SSL_ERROR_WANT_READ)
				{
					// 在read回调函数中读取数据
				}
				else if (err == SSL_ERROR_WANT_WRITE)
				{
					send_bio_data();
				}
				else
				{
					// closed
				}
			}
			else
			{
				if (dtlsCallback_)
				{
					dtlsCallback_->onDtlsRecvData(buf, ret);
					//std::string f;
					//getRemoteFingerprint(f);
					//checkRemoteCertificate(f);
				}
			}
		}
	}

	void DtlsClient::stopNegotiation()
	{
		uv_timer_stop(arqTimer_);
		uv_close((uv_handle_t*)arqTimer_,
			[](uv_handle_t* handle)
		{
			auto ptr = static_cast<DtlsClient*>(handle->data);
			ILOG("dtls timer close finish\n");
			ptr->onStopNegotiation();
		});
	}

	void DtlsClient::onStopNegotiation()
	{
		if (dtlsStatus_ != DtlsStateClientDone)
		{
			if (dtlsCallback_)
			{
				dtlsCallback_->onDtlsAlert("timeout", "");
			}
		}
		else
		{
			if (dtlsCallback_)
			{
				dtlsCallback_->onDtlsHandShakeDone();
			}
		}
	}

	void DtlsClient::startRetransmitTimer()
	{
		if (dtlsStatus_ != DtlsStateClientCertificate && dtlsStatus_ != DtlsStateClientHello)
		{
			return;
		}
		if (retryTime >= 10)
		{
			//uv::LogWriter::Instance()->error("try 10 times not success close socket");
			//arqTimer_->stop();
			stopNegotiation();
			retryTime = 0;
			return;
		}

		int r0 = 0;
		int r1;
		struct timeval to = { 0 };
		r0 = DTLSv1_get_timeout(ssl_, &to);
		if (r0 == 0)
		{
			// get time out fail
			uv_timer_stop(arqTimer_);
			uv_timer_start(arqTimer_, &DtlsClient::onTimer, arqTimeout_, 0);
			return;
		}
		uint64_t timeout = to.tv_sec * 1000 + to.tv_usec / 1000;
		if (timeout == 0)
		{
			r0 = BIO_reset(wbio_); 
			int r1 = SSL_get_error(ssl_, r0);
			if (r0 != 1) {
				//uv::LogWriter::Instance()->error("Bio reset error");
				return;
			}
			r0 = DTLSv1_handle_timeout(ssl_);
			send_bio_data();
		}
		else
		{
			//arqTimer_->setTimeout(timeout);
			arqTimeout_ = timeout;
		}
		uv_timer_stop(arqTimer_);
		uv_timer_start(arqTimer_, &DtlsClient::onTimer, arqTimeout_, 0);
		retryTime++;
	}

	void DtlsClient::onTimer(uv_timer_t *handle)
	{
		auto data = static_cast<DtlsClient*>(handle->data);
		data->startRetransmitTimer();
	}

}