#include "tls.h"
#include "logger.h"

SecurityTransport::SecurityTransport(TlsCallback *cb)
{
	callback_ = cb;
	bConencted_ = false;
	ctx_ = NULL;
	ssl_ = NULL;
	rbio_ = wbio_ = NULL;
}

SecurityTransport::~SecurityTransport()
{
	if (ctx_ != NULL)
	{
		SSL_CTX_free(ctx_);
		ctx_ = NULL;
	}
	if (ssl_ != NULL)
	{
		SSL_free(ssl_);
		ssl_ = NULL;
	}
	//if (rbio_)
	//{
	//	BIO_free(rbio_);
	//	rbio_ = NULL;
	//}
	//if (wbio_)
	//{
	//	BIO_free(wbio_);
	//	wbio_ = NULL;
	//}
}

int SecurityTransport::init(std::string cert, std::string key, TlsRole r)
{
	int ret;
	bool usetls1_3 = true;
	role = r;
	if (role == TLS_ROLE_SERVER)
	{
#if (OPENSSL_VERSION_NUMBER < 0x10002000L) // v1.0.2
		ctx_ = SSL_CTX_new(TLS_method());
#else
		if (usetls1_3)
		{
			ctx_ = SSL_CTX_new(SSLv23_method());
		}
		else
		{
			ctx_ = SSL_CTX_new(TLSv1_2_method());
		}
#endif
		if (ctx_ == NULL)
		{
			ERR_print_errors_fp(stdout);
			ret = -1;
			goto ERROR;
		}
		SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, NULL);

		if (0 == SSL_CTX_use_certificate_file(ctx_, cert.c_str(), SSL_FILETYPE_PEM))
		{
			ELOG("load cert fail\n");
			ret = -1;
			goto ERROR;
		}

		if (0 == SSL_CTX_use_PrivateKey_file(ctx_, key.c_str(), SSL_FILETYPE_PEM))
		{
			ELOG("load key fail\n");
			ret = -1;
			goto ERROR;
		}

		if (!SSL_CTX_check_private_key(ctx_))
		{
			ELOG("load cert fail\n");
			ret = -1;
			goto ERROR;
		}

		ret = SSL_CTX_set_cipher_list(ctx_, "ALL");
		if (ret != 1)
		{
			ELOG("Private key does not match the certificate public key\n");
			ret = -1;
			goto ERROR;
		}
		SSL_CTX_set_mode(ctx_, SSL_MODE_AUTO_RETRY);
		ssl_ = SSL_new(ctx_);
		rbio_ = BIO_new(BIO_s_mem());
		if (rbio_ == NULL)
		{
			ELOG("rbio create fail\n");
			ret = -1;
			goto ERROR;
		}
		wbio_ = BIO_new(BIO_s_mem());
		if (wbio_ == NULL)
		{
			ELOG("wbio create fail\n");
			ret = -1;
			goto ERROR;
		}
		SSL_set_bio(ssl_, rbio_, wbio_);
		SSL_set_accept_state(ssl_);
		SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
	}
	else
	{
#if (OPENSSL_VERSION_NUMBER < 0x10002000L) // v1.0.2
		ctx_ = SSL_CTX_new(TLS_method());
#else
		if (usetls1_3)
		{
			ctx_ = SSL_CTX_new(SSLv23_client_method());
		}
		else
		{
			ctx_ = SSL_CTX_new(TLSv1_2_client_method());
		}
#endif
		if (ctx_ == NULL)
		{
			ELOG("create ssl ctx fail no memery\n");
			return -1;
		}
		ssl_ = SSL_new(ctx_);
		if (ssl_ == NULL)
		{
			ELOG("create ssl fail no memery\n");
			return -1;
		}
		rbio_ = BIO_new(BIO_s_mem());
		wbio_ = BIO_new(BIO_s_mem());
		SSL_set_bio(ssl_, rbio_, wbio_);
		SSL_set_connect_state(ssl_);
		ret = SSL_connect(ssl_);
		if (ret == 1)
		{
			bConencted_ = true;
			return 0;
		}
		else
		{
			send_bio_data();
			int err = SSL_get_error(ssl_, ret);
			if (err == SSL_ERROR_WANT_WRITE)
			{
				send_bio_data();
				ret = 1;
			}
			else if (err == SSL_ERROR_WANT_READ)
			{
				//send_bio_data();
				ret = 1;
			}
			else
			{
				//ERR_print_errors(errBio);
				ELOG("ssl connect fail\n");
				ret = -1;
			}
		}
	}
ERROR:
	return ret;
}

int SecurityTransport::writeData(const char * buf, int len)
{
	int err;
	char *data = NULL;
	int length;
	if (bConencted_)
	{
		SSL_write(ssl_, buf, len);
		length = BIO_get_mem_data(wbio_, &data);
		if (length > 0 && data != NULL)
		{
			callback_->onTlsWriteData(data, length);
			BIO_reset(wbio_);
		}
	}
	return 0;
}

int SecurityTransport::readData(const char * buf, int len)
{
	if (bConencted_)
	{
		int ret;
		ret = BIO_write(rbio_, buf, len);
		ret = SSL_read(ssl_, (void*)buf, len);
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
				bConencted_ = false;
				callback_->onTlsConnectStatus(0);
			}
		}
		else
		{
			callback_->onTlsReadData((char*)buf, ret);
		}
	}
	else
	{
		int r0;
		int r1;
		BIO_write(rbio_, buf, len);
		r0 = SSL_do_handshake(ssl_);
		r1 = SSL_get_error(ssl_, r0);
		if (r0 != 1)
		{
			switch (r1)
			{
			case SSL_ERROR_NONE: //0
			case SSL_ERROR_SSL:  // 1
				ERR_print_errors_fp(stderr);
				//don't break, flush data first
			case SSL_ERROR_WANT_READ: // 2
			case SSL_ERROR_WANT_WRITE: // 3
			case SSL_ERROR_WANT_X509_LOOKUP:  // 4
			{
				send_bio_data();
			}
			break;
			case SSL_ERROR_ZERO_RETURN: // 5
			case SSL_ERROR_SYSCALL: //6
			case SSL_ERROR_WANT_CONNECT: //7
			case SSL_ERROR_WANT_ACCEPT: //8
				ERR_print_errors_fp(stderr);
			default:
				break;
			}
		}
		if (r0 == 1)
		{
			send_bio_data();
			bConencted_ = true;
			callback_->onTlsConnectStatus(1);
		}
	}
	return 0;
}

void SecurityTransport::send_bio_data()
{
	char *data = NULL;
	int len;

	len = BIO_get_mem_data(wbio_, &data);
	if (data != NULL && len > 0)
	{
		BIO_reset(rbio_);
		callback_->onTlsWriteData(data, len);
		BIO_reset(wbio_);
	}
}


