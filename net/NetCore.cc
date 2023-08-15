#include "NetCore.h"
#include "logger.h"
#include <sstream>
#include "string.h"
//#include "ProConfig.h"

namespace NetCore
{
	const std::string kCRLF = "\r\n";

	static Http404Handle http404handle;
	static unsigned char ToHex(unsigned char x)
	{
		return  x > 9 ? x + 55 : x + 48;
	}

	static std::string UrlEncode(const std::string& str)
	{
		std::string strTemp = "";
		size_t length = str.length();
		for (size_t i = 0; i < length; i++)
		{
			if (isalnum((unsigned char)str[i]) ||
				(str[i] == '-') ||
				(str[i] == '_') ||
				(str[i] == '.') ||
				(str[i] == '~'))
				strTemp += str[i];
			else if (str[i] == ' ')
				strTemp += "+";
			else
			{
				strTemp += '%';
				strTemp += ToHex((unsigned char)str[i] >> 4);
				strTemp += ToHex((unsigned char)str[i] % 16);
			}
		}
		return strTemp;
	}

	void IPAddr::transform(struct sockaddr_in *addr)
	{
		uv_ip4_addr(ip.c_str(), port, addr);
	}

	uint64_t IPAddr::generalKey()
	{
		struct sockaddr_in *addr_t = (struct sockaddr_in *)&addr;
		uint64_t key = ((uint64_t)(addr_t->sin_addr.s_addr)) << 16;
		key |= addr_t->sin_port;
		return key;
	}

	std::string IPAddr::toStr()
	{
		return ip + ":" + std::to_string(port);
	}
	void IPAddr::operator=(struct IPAddr &other)
	{
		ip = std::move(other.ip);
		port = other.port;
		addr = other.addr;
	}

	BaseSocket::BaseSocket(uv_loop_t *loop, uint16_t port) : loop_(loop), bindPort(port), socketType(-1)
	{

	}

	BaseSocket::~BaseSocket()
	{
		if (buf_)
		{
			delete[] buf_;
			buf_ = nullptr;
		}
		sockCb_ = nullptr;
	}

	void BaseSocket::init(int type)
	{
		buf_ = new char[MAX_DATA_LEN];
		socketType = type;
	}

	void BaseSocket::registerCallback(ISocketCallback *callback)
	{
		sockCb_ = callback;
	}

	void BaseSocket::onClosed()
	{
		sockCb_->onClose(this);
		delete this;
	}

	char* BaseSocket::getBuf(size_t &len, size_t suggested_size)
	{
		if (buf_ == nullptr)
		{
			buf_ = new char[MAX_DATA_LEN];
		}
		len = MAX_DATA_LEN;
		return buf_;
	}

	void BaseSocket::onAllocBuf(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
	{
		auto hand = (BaseSocket*)((handle->data));
		buf->base = hand->getBuf(buf->len, suggested_size);
	}

	void BaseSocket::onRecvMsg(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf)
	{
		if (nread == 0)
		{
			perror("nread=0\n");
			return;
		}
		auto handle = static_cast<BaseSocket*>(client->data);
		handle->onMessage(buf->base, nread, nullptr, 0);
	}

	void BaseSocket::onRecvMsg(uv_udp_t* client, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
	{
		auto handle = static_cast<BaseSocket*>(client->data);
		handle->onMessage(buf->base, nread, addr, flags);
	}

	TcpSocket::TcpSocket(uv_loop_t *loop, uint16_t port) : BaseSocket(loop, port)
	{
		tcp_ = new uv_tcp_t;
		conn_req_ = new uv_connect_t;
		conn_req_->data = this;
		tcp_->data = this;
		init(0);
		//rbuf_ = new DataRingBuf();
	}

	TcpSocket::~TcpSocket()
	{
		DLOG("destroy tcp socket\n");
		if (conn_req_)
		{
			delete conn_req_;
			conn_req_ = nullptr;
		}
		if (tcp_)
		{
			delete tcp_;
			tcp_ = nullptr;
		}
		//if (rbuf_)
		//{
		//	delete rbuf_;
		//	rbuf_ = nullptr;
		//}
	}

	int TcpSocket::connectServer(IPAddr &addr)
	{
		struct sockaddr_in client_addr, server_addr;
		uv_ip4_addr("0.0.0.0", bindPort, &client_addr);
		addr.transform(&server_addr);

		uv_tcp_init(loop_, tcp_);
		uv_tcp_bind(tcp_, (const struct sockaddr*)&client_addr, 0);
		uv_tcp_connect(conn_req_, tcp_, (const sockaddr*)&server_addr, [](uv_connect_t *conn, int status) {
			auto handle = (TcpSocket*)((conn->data));
			handle->onConnect(status);
		});
		return 0;
	}

	int TcpSocket::writeData(const char *data, int len)
	{
		uv_write_t *req;
		uv_buf_t buf;
		buf.base = const_cast<char*>(data);
		buf.len = len;
		req = new uv_write_t;
		uv_write(req, (uv_stream_t*)tcp_, &buf, 1, [](uv_write_t* req, int status) {
			if (req)
			{
				delete req;
			}
			if (status == UV_ECANCELED)
			{
				//write error;
			}
		});

		return 0;
	}

	int TcpSocket::sendData(const char *data, int len)
	{
		if (std::this_thread::get_id() == NETIOMANAGER->mainLoopThreadId_)
		{
			writeData(data, len);
		}
		else
		{
			DLOG("not loop thread\n");
			sendDataByRawSocket(data, len);
			//std::string str(data, len);
			//NETIOMANAGER->postMainLoop([this, str]() {
			//	this->writeData(str.data(), str.length());
			//});
		}
		return 0;
	}

	int TcpSocket::sendDataByRawSocket(const char *data, int len)
	{
		int tcpSocketFd;
		int ret = 0;
		uv_fileno((uv_handle_t*)tcp_, &tcpSocketFd);
		ret = send(tcpSocketFd, data, len, 0);
		return ret;
	}

	// must call by main loop thread
	int TcpSocket::close()
	{
		if (uv_is_active((uv_handle_t*)tcp_))
		{
			uv_read_stop((uv_stream_t*)tcp_);
		}
		if (uv_is_closing((uv_handle_t*)tcp_) == 0)
		{
			//libuv ��loop��ѯ�л���رվ����delete�ᵼ�³����쳣�˳���
			uv_close((uv_handle_t*)tcp_, [](uv_handle_t* handle) {
				auto data = static_cast<TcpSocket*>(handle->data);
				data->onClosed();
			});
		}
		else
		{
			onClosed();
		}
		return 0;
	}

	void TcpSocket::onConnect(int status)
	{
		if (status == 0)
		{
			uv_read_start((uv_stream_t*)tcp_, &BaseSocket::onAllocBuf, &BaseSocket::onRecvMsg);
			sockCb_->onConnect(0, this);
		}
		else
		{
			sockCb_->onConnect(-1, this);
		}
	}

	void TcpSocket::onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags)
	{
		if (size > 0)
		{
			sockCb_->onRecvData(buf, size, nullptr, this);
		}
		else if (size < 0)
		{
			close();
		}
		else
		{

		}
	}

	WebSocketClient::WebSocketClient(uv_loop_t *loop, uint16_t port): TcpSocket(loop, port), wsProtocol_(nullptr), pingPeriod_(20), path_(""), host_("")
	{
		wsProtocol_ = new WebSocketProtocolClient();
		pingTimer_ = new uv_timer_t;
		pingTimer_->data = static_cast<void*>(this);
		uv_timer_init(loop, pingTimer_);
		cacheMsg_ = "";
		msgLen_ = 0;
	}

	WebSocketClient::~WebSocketClient()
	{
		DLOG("destroy websocket client\n");
		if (wsProtocol_ != nullptr)
		{
			delete wsProtocol_;
			wsProtocol_ = nullptr;
		}
		pingTimer_ = nullptr;
	}

	void WebSocketClient::connect(IPAddr &addr, std::string path)
	{
		connectServer(addr);
		this->path_ = path;
		this->host_ = addr.ip + ":" + std::to_string(addr.port);
	}

	void WebSocketClient::setPingPeriod(int period)
	{
		this->pingPeriod_ = period;
	}

	void WebSocketClient::sendWs(const char *data, int len, bool text)
	{
		if (wsProtocol_->isConnected())
		{
			std::string dest = "";
			WsOpCode opcode = text ? TEXT_FRAME : BINARY_FRAME;
			wsProtocol_->encodeData(data, len, opcode, dest);
			//DLOG("send len data {}, len {} opcode {}", data, len, opcode);
			sendData(dest.data(), dest.length());
			//sendDataByRawSocket(dest.data(), dest.length());
		}
	}

	void WebSocketClient::closeWs()
	{
		wsProtocol_->close();
		uv_timer_stop(pingTimer_);
		uv_close((uv_handle_t*)pingTimer_,
			[](uv_handle_t* handle)
		{
			auto ptr = static_cast<WebSocketClient*>(handle->data);
			//ptr->closeComplete();
			ptr->close();
			delete handle;
		});
	}

	void WebSocketClient::onConnect(int status)
	{
		if (status >= 0)
		{
			std::string handshake;
			std::string extensions = "ws rtc client";
			uv_read_start((uv_stream_t*)tcp_, &BaseSocket::onAllocBuf, &BaseSocket::onRecvMsg);
			wsProtocol_->initParam(0, path_, host_, extensions);
			wsProtocol_->doHandShake(handshake);
			sendData(handshake.c_str(), handshake.length());
			//client_->write(handshake.data(), handshake.length(), nullptr);
		}
		else
		{
			ELOG("connect server fail\n");
			if (sockCb_)
			{
				sockCb_->onConnect(-1, this);
			}
		}
	}

	void WebSocketClient::onMessage(char* data, ssize_t size, const struct sockaddr* addr, unsigned flags)
	{
		if (size > 0)
		{
			if (wsProtocol_->isConnected())
			{
				//int index = 0;
				//do
				//{
				//	std::string dest = "";
				//	bool finish;
				//	WsOpCode opcode;
				//	int hlen = 0;
				//	int len = wsProtocol_->decodeData(data + index, size, dest, finish, opcode, hlen);
				//	index += (len + hlen);
				//	//DLOG("cur {} size {} hlen {}", index, size, hlen);
				//	if (len == 0)
				//	{
				//		if (opcode == CLOSE_FRAME)
				//		{
				//			len = wsProtocol_->encodeData("EOF", strlen("EOF"), CLOSE_FRAME, dest);
				//			sendData(dest.c_str(), dest.length());
				//			break;
				//		}
				//		//else if (opcode == PONG_FRAME) 
				//		//{
				//		//	TLOG("recv pong resp");
				//		//}
				//	}
				//	else
				//	{
				//		DLOG("recv data len: %d:%d\n", dest.length(),len);
				//		if (sockCb_)
				//		{
				//			sockCb_->onRecvData(dest.data(), len, nullptr, this);
				//		}
				//	}
				//} while (index < size);
				process(data, size);
			}
			else
			{
				std::string response(data, size);
				int status;
				int ret = wsProtocol_->doResponse(response);
				if (ret < 0)
				{
					close();
					status = -1;
				}
				else
				{
					uv_timer_start(pingTimer_, &WebSocketClient::onTimer, pingPeriod_ * 1000, pingPeriod_ * 1000);
					//pingTimer_->start();
					status = 0;
				}
				if (sockCb_)
				{
					sockCb_->onConnect(status, this);
				}
			}
		}
		else
		{
			close();
		}
	}


	void WebSocketClient::onTimer(uv_timer_t *handle)
	{
		auto data = static_cast<WebSocketClient*>(handle->data);
		data->sendPingRequest();
	}

	void WebSocketClient::sendPingRequest()
	{
		std::string dest = "";
		int ret = wsProtocol_->encodeData(NULL, 0, PING_FRAME, dest);
		//sendData(dest.data(), dest.length());
	}

	void WebSocketClient::process(char *data, ssize_t size)
	{
		int index = 0;
		do
		{
			if (cacheMsg_.length() > 0)
			{
				int i = 0;
				int currLen = cacheMsg_.length();
				while (i < (msgLen_ - currLen) && i < size)
				{
					//cacheMsg_.append(1, data[i]);
					cacheMsg_.push_back(data[i]);
					i++;
				}
				if (cacheMsg_.length() == msgLen_)
				{
					if (sockCb_)
					{
						sockCb_->onRecvData(cacheMsg_.data(), msgLen_, nullptr, this);
					}
					cacheMsg_.clear();
					msgLen_ = 0;
				}
				index += i;
			}
			else
			{
				std::string dest = "";
				bool finish;
				WsOpCode opcode;
				int hlen = 0;
				int len = wsProtocol_->decodeData(data + index, size - index, dest, finish, opcode, hlen);
				index += (len + hlen);
				//DLOG("cur {} size {} hlen {}", index, size, hlen);
				if (len == 0)
				{
					if (opcode == CLOSE_FRAME)
					{
						len = wsProtocol_->encodeData("EOF", strlen("EOF"), CLOSE_FRAME, dest);
						sendData(dest.c_str(), dest.length());
						break;
					}
					//else if (opcode == PONG_FRAME) 
					//{
					//	TLOG("recv pong resp");
					//}
				}
				else
				{
					if (dest.length() < len)
					{
						DLOG("recv len:%d < msg len:%d cache current msg wait next packet\n", dest.length(), len);
						cacheMsg_.append(dest);
						msgLen_ = len;
						dest.clear();
					}
					else
					{
						if (sockCb_)
						{
							sockCb_->onRecvData(dest.data(), len, nullptr, this);
						}
					}
				}
			}
		} while (index < size);
	}

	HttpClient::HttpClient(uv_loop_t *loop, uint16_t port) : TcpSocket(loop, port)
	{
		http_parser_init(&parser, HTTP_RESPONSE);
		parser.data = static_cast<void*>(this);
		memset(&settings, 0, sizeof(settings));
		settings.on_message_begin = on_message_begin;
		settings.on_url = on_url;
		settings.on_header_field = on_header_field;
		settings.on_header_value = on_header_value;
		settings.on_headers_complete = on_headers_complete;
		settings.on_body = on_body;
		settings.on_message_complete = on_message_complete;
		headerComplete = false;
		p_body_start_ = nullptr;
		bodyLen = 0;
		header_key = header_val = "";
	}

	HttpClient::~HttpClient()
	{
		p_body_start_ = nullptr;
		DLOG("destroy http client\n");
	}

	void HttpClient::PostData(std::string url, std::string params)
	{
		httpParams_ = params;
		parseHttpUrl(url);
		method = 1;
		buildConnect();
	}

	void HttpClient::GetData(std::string url, std::unordered_map<std::string, std::string> params)
	{
		std::ostringstream os;
		os << "?";
		for (auto iter = params.begin(); iter != params.end(); iter++)
		{
			std::string val = iter->second;
			os << iter->first << "=" << UrlEncode(val) << "&";
		}
		std::string data = os.str();
		httpParams_ = data.substr(0, data.length() - 1);
		//ILOG("http get param %s\n", httpParams_.c_str());
		method = 0;
		parseHttpUrl(url);
		buildConnect();
	}

	void HttpClient::onConnect(int status)
	{
		if (status >= 0)
		{
			std::string data;
			buildHttpData(data);
			httpHeader_.clear();
			uv_read_start((uv_stream_t*)tcp_, &BaseSocket::onAllocBuf, &BaseSocket::onRecvMsg);
			sendData(data.data(), data.length());
		}
		else
		{
			ELOG("connect server fail\n");
			if (sockCb_)
			{
				sockCb_->onConnect(-1, this);
			}
		}
	}

	void HttpClient::onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags)
	{
		if (size > 0)
		{
			int parsed = http_parser_execute(&parser, &settings, buf, size);
			enum http_errno code;
			if ((code = HTTP_PARSER_ERRNO(&parser)) != HPE_OK)
			{
				ELOG("http response parsed {%d} err {%d}/{%s} {%s}\n", (int)parsed, code, http_errno_name(code), http_errno_description(code));
				return;
			}
			if (headerComplete)
			{
				DLOG("http header parse complete\n");
				if (sockCb_)
				{
					if (p_body_start_ != nullptr && bodyLen != 0)
					{
						sockCb_->onRecvData(p_body_start_, bodyLen, nullptr, this);
					}
				}
				//close();
			}
		}
		else
		{
			close();
		}
	}

	void HttpClient::parseHttpUrl(std::string url)
	{
		int index;
		index = url.find("http://");
		if (index == std::string::npos)
		{
			return;
		}
		std::string temp = url.substr(index + 7, url.length());
		host_ = temp.substr(0, temp.find(":"));
		std::string sport = temp.substr(temp.find(":") + 1, temp.find("/") - temp.find(":") - 1);
		port = atoi(sport.c_str());
		path_ = temp.substr(temp.find("/"), temp.length() - temp.find("/"));
	}

	void HttpClient::initHttpHeader()
	{
		httpHeader_["User-Agent"] = "Fiddler";
		httpHeader_["Content-Type"] = "application/json;charset=utf-8";
		httpHeader_["HOST"] = host_ + ":" + std::to_string(port);
		httpHeader_["Connection"] = "close";
		if (method == 0)
		{
			//GET Method
			httpHeader_["Content-Length"] = "0";
		}
		else if (method == 1)
		{
			//POST Method
			httpHeader_["Content-Length"] = std::to_string(httpParams_.length());
		}
	}

	void HttpClient::buildHttpData(std::string &data)
	{
		std::ostringstream os;
		if (method == 0)
		{
			os << "GET " << path_ << httpParams_ << " HTTP/1.1" << kCRLF;
		}
		else
		{
			os << "POST " << path_ << " HTTP/1.1" << kCRLF;
		}
		for (auto iter = httpHeader_.begin(); iter != httpHeader_.end(); iter++)
		{
			os << iter->first << ": " << iter->second << kCRLF;
		}
		os << kCRLF;
		if (method == 1)
		{
			os << httpParams_;
		}
		data = os.str();
	}

	void HttpClient::buildConnect()
	{
		IPAddr addr;
		addr.ip = host_;
		addr.port = port;
		initHttpHeader();
		connectServer(addr);
	}

	int HttpClient::on_message_begin(http_parser* parser)
	{
		ILOG("message begin\n");
		return 0;
	}

	int HttpClient::on_headers_complete(http_parser* parser)
	{
		ILOG("header complete\n");
		//HttpClient *obj = (HttpClient*)(parser->data);
		//obj->headerComplete = true;
		return 0;
	}

	int HttpClient::on_message_complete(http_parser* parser)
	{
		ILOG("message complete\n");
		HttpClient *obj = (HttpClient*)(parser->data);
		obj->headerComplete = true; // message complete
		return 0;
	}

	int HttpClient::on_url(http_parser* parser, const char* at, size_t length)
	{
		std::string url(at, length);
		ILOG("on url is %d\n", length);
		return 0;
	}

	int HttpClient::on_header_field(http_parser* parser, const char* at, size_t length)
	{
		HttpClient *obj = (HttpClient*)(parser->data);
		if (!obj->header_val.empty())
		{
			obj->httpHeader_[obj->header_key] = obj->header_val;
			obj->header_key = obj->header_val = "";
		}
		if (length > 0)
		{
			obj->header_key.append(at, length);
		}
		ILOG("Header key(%d bytes)\n", (int)length);
		return 0;
	}

	int HttpClient::on_header_value(http_parser* parser, const char* at, size_t length)
	{
		HttpClient *obj = (HttpClient*)(parser->data);
		if (length > 0)
		{
			obj->header_val.append(at, length);
		}
		ILOG("Header value(%d bytes)\n", (int)length);
		return 0;
	}

	int HttpClient::on_body(http_parser* parser, const char* at, size_t length)
	{
		HttpClient *obj = (HttpClient*)(parser->data);
		ILOG("Http on body %d\n", length);
		if (length > 0)
		{
			obj->p_body_start_ = at;
			obj->bodyLen = length;
		}
		return 0;
	}

	HttpConn::HttpConn(HttpServer * server, uint64_t key)
	{
		http_parser_init(&parser, HTTP_REQUEST);
		parser.data = static_cast<void*>(this);
		memset(&settings, 0, sizeof(settings));
		settings.on_message_begin = on_message_begin;
		settings.on_url = on_url;
		settings.on_header_field = on_header_field;
		settings.on_header_value = on_header_value;
		settings.on_headers_complete = on_headers_complete;
		settings.on_body = on_body;
		settings.on_message_complete = on_message_complete;
        headerComplete = false;
		messageComplete = false;
		httpBody = "";
		server_ = server;
		responseWriter = new HttpResponseWriter(key);
	}

	HttpConn::~HttpConn()
	{
		DLOG("destroy http connection\n");
	    delete responseWriter;
	}

	void HttpConn::onRecvData(const char * data, ssize_t len)
	{
		if (len > 0)
		{
			int parsed = http_parser_execute(&parser, &settings, data, len);
			enum http_errno code;
			if ((code = HTTP_PARSER_ERRNO(&parser)) != HPE_OK)
			{
				ELOG("http response parsed {%d} err {%d}/{%s} {%s}\n", (int)parsed, code, http_errno_name(code), http_errno_description(code));
				return;
			}
			if (messageComplete)
			{
				ILOG("http message parse complete\n");
                time_t now = time(0);
                char* dt = ctime(&now);
                std::string date(dt, strlen(dt) - 1);
				responseWriter->set("Access-Control-Allow-Origin", "*");
				responseWriter->set("Access-Control-Allow-Methods", "GET, POST, HEAD, PUT, DELETE, OPTIONS");
				responseWriter->set("Access-Control-Expose-Headers", "Server,range,Content-Length,Content-Range");
				responseWriter->set("Access-Control-Allow-Headers", "origin,range,accept-encoding,referer,Cache-Control,X-Proxy-Authorization,X-Requested-With,Content-Type");
				responseWriter->set("Date", date);
				responseWriter->set("Server", "Libnet(hejingsheng)/v0.0.1");
				if (parser.method == HTTP_POST)
				{
					server_->onHttpPost(url, httpBody, responseWriter);
				}
				else if (parser.method == HTTP_GET)
				{
					std::string param = "";
					int index = url.find_first_of('?', 0);
					if (index != std::string::npos)
					{
					    std::string tmpurl = url.substr(0, index);
						param = url.substr(index+1);
						server_->onHttpGet(tmpurl, param, responseWriter);
					}
					else
					{
						server_->onHttpGet(url, param, responseWriter);
					}
				}
			}
		}
	}

	int HttpConn::on_message_begin(http_parser* parser)
	{
		ILOG("message begin\n");
		return 0;
	}

	int HttpConn::on_headers_complete(http_parser* parser)
	{
		ILOG("header complete\n");
		HttpConn *obj = (HttpConn*)(parser->data);
		obj->headerComplete = true;
		return 0;
	}

	int HttpConn::on_message_complete(http_parser* parser)
	{
		ILOG("message complete\n");
		HttpConn *obj = (HttpConn*)(parser->data);
		obj->messageComplete = true;
		return 0;
	}

	int HttpConn::on_url(http_parser* parser, const char* at, size_t length)
	{
		std::string url(at, length);
		HttpConn *obj = (HttpConn*)(parser->data);
		obj->url = url;
		ILOG("on url is %s\n", url.c_str());
		return 0;
	}

	int HttpConn::on_header_field(http_parser* parser, const char* at, size_t length)
	{
		HttpConn *obj = (HttpConn*)(parser->data);
		if (!obj->header_val.empty())
		{
			obj->httpHeader_[obj->header_key] = obj->header_val;
			obj->header_key = obj->header_val = "";
		}
		if (length > 0)
		{
			obj->header_key.append(at, length);
		}
		ILOG("Header key(%d bytes)\n", (int)length);
		return 0;
	}

	int HttpConn::on_header_value(http_parser* parser, const char* at, size_t length)
	{
		HttpConn *obj = (HttpConn*)(parser->data);
		if (length > 0)
		{
			obj->header_val.append(at, length);
		}
		ILOG("Header value(%d bytes)\n", (int)length);
		return 0;
	}

	int HttpConn::on_body(http_parser* parser, const char* at, size_t length)
	{
		HttpConn *obj = (HttpConn*)(parser->data);
		ILOG("Http on body %d\n", length);
		if (length > 0)
		{
			obj->httpBody.append(at, length);
		}
		return 0;
	}

	HttpResponseWriter::HttpResponseWriter(uint64_t clientkey)
	{
		key_ = clientkey;
		code = 200;
		headerMap_.clear();
		http_body = "";
		content_len = -1;
	}

	HttpResponseWriter::~HttpResponseWriter()
	{
		DLOG("destroy http response writer\n");
		headerMap_.clear();
	}

	void HttpResponseWriter::set(std::string key, std::string value)
	{
		headerMap_[key] = value;
	}

	void HttpResponseWriter::set_response_code(int code)
	{
		this->code = code;
	}

	void HttpResponseWriter::set_body(std::string &body)
	{
	    if (!body.empty())
	    {
            http_body = body;
        }
	}

    void HttpResponseWriter::set_content_len(int len)
    {
        content_len = len;
    }

	void HttpResponseWriter::write(TcpSocketServer *server)
	{
	    std::stringstream ss;
        std::string data;
		char bodylen[64];
		bool chunked = false;
		memset(bodylen, 0, sizeof(bodylen));
	    ss << "HTTP/1.1 " << code << kCRLF;
	    for (auto iter = headerMap_.begin(); iter != headerMap_.end(); ++iter)
        {
	        ss << iter->first << ": " << iter->second << kCRLF;
        }
	    if (content_len == -1)
        {
	        ss << "Transfer-Encoding: chunked" << kCRLF;
	        chunked = true;
        }
	    else
        {
            ss << "Content-Length: " << content_len << kCRLF;
        }
	    if (headerMap_.find("Connection") == headerMap_.end())
        {
	        ss << "Connection: Keep-Alive" << kCRLF;
        }
	    ss << kCRLF;
	    if (!http_body.empty())
        {
	        if (chunked)
            {
                ss << http_body.length() << kCRLF;
                sprintf(bodylen, "%x", http_body.length());
            }
	        ss << http_body << kCRLF;
        }
	    data = ss.str();
	    server->sendData(key_, data.c_str(), data.length());
	}

	int Http404Handle::http_server_handle(HttpResponseWriter *writer, const std::string &data)
	{
		writer->set("Content-Type", "text/plain; charset=utf-8");
		writer->set("Connection", "Close");
		writer->set_content_len(data.length());
		writer->set_body(const_cast<std::string&>(data));
		return 0;
	}

	HttpServer::HttpServer(uv_loop_t * loop, uint16_t port, bool https)
	{
		server_ = new TcpSocketServer(loop, port, false, https);
	}

	HttpServer::~HttpServer()
	{
		delete server_;
	}

	void HttpServer::start()
	{
		server_->setRecvDataCallback(std::bind(&HttpServer::onRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		server_->setCloseCallback(std::bind(&HttpServer::onClosed, this, std::placeholders::_1, std::placeholders::_2));
		server_->bindAndStart();
	}

	void HttpServer::registerHandle(const std::string &pattern, IHttpServerHandle *handle)
	{
		httpHandle_[pattern] = handle;
	}

	void HttpServer::onHttpPost(std::string &url, std::string &body, HttpResponseWriter *writer)
	{
        IHttpServerHandle *handle = nullptr;

        find_handle(url, &handle);
        if (handle == nullptr)
        {
            handle = &http404handle;
        }
        if (handle->is_404())
        {
            writer->set_response_code(404);
			handle->http_server_handle(writer, "not found");
        }
        else
        {
            handle->http_server_handle(writer, body);
        }
        writer->write(server_);
	}

	void HttpServer::onHttpGet(std::string &url, std::string &param, HttpResponseWriter *writer)
	{
		
	}

    void HttpServer::find_handle(std::string &pattern, IHttpServerHandle **handle)
    {
	    if (httpHandle_.find(pattern) != httpHandle_.end())
        {
	        *handle = httpHandle_[pattern];
        }
	    else
        {
	        *handle = nullptr;
        }
    }

	void HttpServer::onRecvData(const char * data, ssize_t len, uint64_t key, TcpSocketConn *conn)
	{
		HttpConn *httpconnection = nullptr;
		if (httpConnectionMap_.find(key) == httpConnectionMap_.end())
		{
			httpconnection = new HttpConn(this, key);
			httpConnectionMap_.insert(std::make_pair(key, httpconnection));
		}
		else
		{
			httpconnection = httpConnectionMap_[key];
		}
		httpconnection->onRecvData(data, len);
	}

	void HttpServer::onClosed(uint64_t key, TcpSocketConn *conn)
	{
		HttpConn *httpconnection = nullptr;
		if (httpConnectionMap_.find(key) == httpConnectionMap_.end())
		{
			ELOG("not find http connection\n");
		}
		else
		{
			httpconnection = httpConnectionMap_[key];
			delete httpconnection;
			httpConnectionMap_.erase(key);
		}
	}

    TcpSocketConn::TcpSocketConn(uv_loop_t *loop, uv_tcp_t *tcp, bool ws, bool tls) : BaseSocket(loop, 0), tcp_(tcp)
	{
		tcp_->data = this;
		if (ws)
		{
			wsProtocol_ = new WebSocketProtocolServer();
		}
		else
		{
			wsProtocol_ = nullptr;
		}
		if (tls)
		{
			tlsTranport_ = new SecurityTransport(this);
		}
		else
		{
			tlsTranport_ = nullptr;
		}
		init(0);
	}

	TcpSocketConn::~TcpSocketConn()
	{
		onCloseCb_ = nullptr;
		onRecvDataCb_ = nullptr;
		if (tcp_)
		{
			delete tcp_;
			tcp_ = nullptr;
		}
		if (wsProtocol_)
		{
			delete wsProtocol_;
			wsProtocol_ = nullptr;
		}
		if (tlsTranport_)
		{
			delete tlsTranport_;
			tlsTranport_ = nullptr;
		}
	}

	int TcpSocketConn::connectServer(IPAddr &addr)
	{
		int len;

		len = sizeof(struct sockaddr);
		uv_tcp_getpeername(tcp_, (struct sockaddr *)&addr.addr, &len);
		struct sockaddr_in *tmp = (struct sockaddr_in *)&addr.addr;
		addr.ip = (inet_ntoa(tmp->sin_addr));
		addr.port = htons(tmp->sin_port);
		selfkey = addr.generalKey();
		uv_read_start((uv_stream_t*)tcp_, &BaseSocket::onAllocBuf, &BaseSocket::onRecvMsg);
		if (tlsTranport_)
		{
			std::string cert = "";
			std::string key = "";
			tlsTranport_->init(cert, key, TLS_ROLE_SERVER);
		}
		return 0;
	}

	int TcpSocketConn::sendData(const char *data, int len)
	{
		std::string dest = "";
		if (tlsTranport_ && tlsTranport_->isConnected())
		{
			if (wsProtocol_ && wsProtocol_->isConnected())
			{
				wsProtocol_->encodeData(data, len, TEXT_FRAME, dest);
				tlsTranport_->writeData(dest.c_str(), dest.length());
			}
			else
			{
				tlsTranport_->writeData(data, len);
			}
		}
		else
		{
			if (wsProtocol_ && wsProtocol_->isConnected())
			{
				wsProtocol_->encodeData(data, len, TEXT_FRAME, dest);
				if (std::this_thread::get_id() == NETIOMANAGER->mainLoopThreadId_)
				{
					writeData(dest.c_str(), dest.length());
				}
				else
				{
					NETIOMANAGER->postMainLoop([this, dest]() {
						this->writeData(dest.c_str(), dest.length());
					});
				}
			}
			else
			{
				if (std::this_thread::get_id() == NETIOMANAGER->mainLoopThreadId_)
				{
					writeData(data, len);
				}
				else
				{
					NETIOMANAGER->postMainLoop([this, data, len]() {
						this->writeData(data, len);
					});
				}
			}
		}
		return 0;
	}

	int TcpSocketConn::sendDataByRawSocket(const char *data, int len)
	{
		return 0;
	}

	int TcpSocketConn::close()
	{
		if (wsProtocol_ != nullptr)
		{
			wsProtocol_->close();
		}
		if (uv_is_active((uv_handle_t*)tcp_))
		{
			uv_read_stop((uv_stream_t*)tcp_);
		}
		if (uv_is_closing((uv_handle_t*)tcp_) == 0)
		{
			//libuv ��loop��ѯ�л���رվ����delete�ᵼ�³����쳣�˳���
			uv_close((uv_handle_t*)tcp_, [](uv_handle_t* handle) {
				auto data = static_cast<TcpSocketConn*>(handle->data);
				data->onConnClosed();
			});
		}
		else
		{
			onConnClosed();
		}
		return 0;
	}

	void TcpSocketConn::onTlsConnectStatus(int status)
	{
		ILOG("tls status %d", status);
		if (status == 0)
		{
			close();
		}
	}

	void TcpSocketConn::onTlsReadData(char *buf, int size)
	{
		if (size > 0)
		{
			if (wsProtocol_ != nullptr)
			{
				if (!wsProtocol_->isConnected())
				{
					std::string handshake(buf, size);
					if (wsProtocol_->doHandShake(handshake) == 0)
					{
						std::string response;
						wsProtocol_->doResponse(response);
						//sendData(response.data(), response.length());
						tlsTranport_->writeData(response.c_str(), response.length());
					}
					else
					{
						WLOG("wait other data\n");
					}
				}
				else
				{
					std::string data = "";
					bool finish;
					int len;
					int hlen;
					WsOpCode opcode;
					len = wsProtocol_->decodeData(buf, size, data, finish, opcode, hlen);
					if (len == 0)
					{
						if (opcode == CLOSE_FRAME)
						{
							DLOG("recv close msg\n");
							len = wsProtocol_->encodeData("����˽��շ�������EOF", strlen("����˽��շ�������EOF"), CLOSE_FRAME, data);
							//sendData(data.c_str(), len);
							tlsTranport_->writeData(data.c_str(), data.length());
							close();
						}
						else if (opcode == PING_FRAME)
						{
							int ret = wsProtocol_->encodeData(NULL, 0, PONG_FRAME, data);
							tlsTranport_->writeData(data.c_str(), data.length());
						}
					}
					else
					{
						DLOG("recv %s data\n", data.c_str());
						if (onRecvDataCb_)
						{
							onRecvDataCb_(data.data(), data.size(), selfkey, this);
						}
						data.clear();
						//len = wsProtocol_->encodeData("recv", 4, data);
						//sendData(data.c_str(), len);
					}
				}
			}
			else
			{
				if (onRecvDataCb_)
				{
					onRecvDataCb_(buf, size, selfkey, this);
				}
			}
		}
	}

	void TcpSocketConn::onTlsWriteData(char *data, int len)
	{
		if (std::this_thread::get_id() == NETIOMANAGER->mainLoopThreadId_)
		{
			writeData(data, len);
		}
		else
		{
			NETIOMANAGER->postMainLoop([this, data, len]() {
				this->writeData(data, len);
			});
		}
	}

	void TcpSocketConn::setRecvDataCallback(OnRecvDataCallback callback)
	{
		onRecvDataCb_ = callback;
	}

	void TcpSocketConn::setCloseCallback(OnConnCloseCallback callback)
	{
		onCloseCb_ = callback;
	}

	int TcpSocketConn::onConnClosed()
	{
		if (onCloseCb_)
		{
			onCloseCb_(selfkey, this);
		}
	}

	int TcpSocketConn::writeData(const char *data, int len)
	{
		uv_write_t *req;
		uv_buf_t buf;
		buf.base = const_cast<char*>(data);
		buf.len = len;
		req = new uv_write_t;
		uv_write(req, (uv_stream_t*)tcp_, &buf, 1, [](uv_write_t* req, int status) {
			if (req)
			{
				delete req;
			}
			if (status == UV_ECANCELED)
			{
				//write error;
			}
		});

		return 0;
	}

	int TcpSocketConn::writeData(const std::string &data)
	{
		uv_write_t *req;
		uv_buf_t buf;
		buf.base = const_cast<char*>(data.c_str());
		buf.len = data.length();
		req = new uv_write_t;
		uv_write(req, (uv_stream_t*)tcp_, &buf, 1, [](uv_write_t* req, int status) {
			if (req)
			{
				delete req;
			}
			if (status == UV_ECANCELED)
			{
				//write error;
			}
		});

		return 0;
	}

	void TcpSocketConn::onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags)
	{
		if (size > 0)
		{
			if (tlsTranport_ != nullptr)
			{
				// tls
				tlsTranport_->readData(buf, size);
			}
			else
			{
				if (wsProtocol_ != nullptr)
				{
					if (!wsProtocol_->isConnected())
					{
						std::string handshake(buf, size);
						if (wsProtocol_->doHandShake(handshake) == 0)
						{
							std::string response;
							wsProtocol_->doResponse(response);
							//sendData(response.data(), response.length());
							writeData(response);
						}
						else
						{
							WLOG("wait other data\n");
						}
					}
					else
					{
						std::string data = "";
						bool finish;
						int len;
						int hlen;
						WsOpCode opcode;
						len = wsProtocol_->decodeData(buf, size, data, finish, opcode, hlen);
						if (len == 0)
						{
							if (opcode == CLOSE_FRAME)
							{
								DLOG("recv close msg\n");
								len = wsProtocol_->encodeData("����˽��շ�������EOF", strlen("����˽��շ�������EOF"), CLOSE_FRAME, data);
								//sendData(data.c_str(), len);
								writeData(data);
								close();
							}
							else if (opcode == PING_FRAME)
							{
								int ret = wsProtocol_->encodeData(NULL, 0, PONG_FRAME, data);
								writeData(data);
							}
						}
						else
						{
							DLOG("recv %s data\n", data.c_str());
							if (onRecvDataCb_)
							{
								onRecvDataCb_(data.data(), data.size(), selfkey, this);
							}
							data.clear();
							//len = wsProtocol_->encodeData("recv", 4, data);
							//sendData(data.c_str(), len);
						}
					}
				}
				else
				{
					if (onRecvDataCb_)
					{
						onRecvDataCb_(buf, size, selfkey, this);
					}
				}
			}
		}
		else if (size < 0)
		{
			// socket �쳣��Ͽ� �Ͽ��ɹ����ϱ�״̬���ɣ��Լ������н���״̬ά��
			close();
		}
		else
		{

		}
	}

	TcpSocketServer::TcpSocketServer(uv_loop_t *loop, uint16_t port, bool ws, bool tls) : loop_(loop), tcpServer_(new uv_tcp_t), listenPort(port), ws_(ws), tls_(tls)
	{
		tcpServer_->data = this;
	}

	TcpSocketServer::~TcpSocketServer()
	{

	}

	void TcpSocketServer::bindAndStart()
	{
		struct sockaddr_in addr;
		int r;

		uv_ip4_addr("0.0.0.0", listenPort, &addr);
		uv_tcp_init(loop_, tcpServer_);
		uv_tcp_bind(tcpServer_, (const sockaddr*)&addr, 0);
		uv_listen((uv_stream_t*)tcpServer_, SOMAXCONN, &TcpSocketServer::on_connection_cb);
	}

	void TcpSocketServer::sendData(uint64_t key, const char *data, int len)
	{
		if (clientMap.find(key) != clientMap.end())
		{
			clientMap[key]->sendData(data, len);
		}
	}

	void TcpSocketServer::close(uint64_t key)
	{
		if (clientMap.find(key) != clientMap.end())
		{
			clientMap[key]->close();
		}
	}

	void TcpSocketServer::setRecvDataCallback(OnRecvDataCallback callback)
	{
		onRecvDataCb_ = callback;
	}

	void TcpSocketServer::setCloseCallback(OnConnCloseCallback callback)
	{
		onCloseCb_ = callback;
	}

	void TcpSocketServer::createNewClient(uv_stream_t* server)
	{
		int ret;
		uv_tcp_t *client = new uv_tcp_t;
		uv_tcp_init(loop_, client);
		ret = uv_accept(server, (uv_stream_t*)client);
		if (ret == 0)
		{
			IPAddr addr;
			TcpSocketConn *conn = new TcpSocketConn(loop_, client, ws_, tls_);
			conn->connectServer(addr);
			conn->setRecvDataCallback(std::bind(&TcpSocketServer::onRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
			conn->setCloseCallback(std::bind(&TcpSocketServer::onClosed, this, std::placeholders::_1, std::placeholders::_2));
			uint64_t key = addr.generalKey();
			clientMap.insert(std::make_pair(key, conn));
		}
		else
		{
			uv_close((uv_handle_t*)client, nullptr);
		}
	}

	void TcpSocketServer::onConnection(uv_stream_t* server)
	{
		createNewClient(server);
	}

	void TcpSocketServer::removeClient(uint64_t key)
	{
		auto iter = clientMap.find(key);
		if (iter != clientMap.end())
		{
			TcpSocketConn *tmp = iter->second;
			delete tmp;
			clientMap.erase(key);
		}
	}

	void TcpSocketServer::onRecvData(const char *data, ssize_t len, uint64_t key, TcpSocketConn *conn)
	{
		DLOG("recv data %d from %ld\n", len, key);
		//conn->sendData(data, len);
		if (onRecvDataCb_)
		{
			onRecvDataCb_(data, len, key, nullptr);
		}
	}

	void TcpSocketServer::onClosed(uint64_t key, TcpSocketConn *conn)
	{
		DLOG("recv close from %ld\n", key);
		removeClient(key);
		if (onCloseCb_)
		{
			onCloseCb_(key, nullptr);
		}
	}

	void TcpSocketServer::on_connection_cb(uv_stream_t* server, int status)
	{
		if (status == 0)
		{
			auto data = static_cast<TcpSocketServer*>(server->data);
			data->onConnection(server);
		}
		else
		{
			ELOG("connect error %d\n", status);
		}
	}

	UdpSocket::UdpSocket(uv_loop_t *loop, uint16_t port) : BaseSocket(loop, port)
	{
		udp_ = new uv_udp_t;
		udp_->data = this;
		init(1);
	}

	UdpSocket::~UdpSocket()
	{
		DLOG("destroy udp socket\n");
		if (udp_)
		{
			delete udp_;
			udp_ = nullptr;
		}
	}

	int UdpSocket::connectServer(IPAddr &addr)
	{
		int ret;
		struct sockaddr_in client_addr;
		addr.transform(&server_addr);

		uv_udp_init(loop_, udp_);
		if (bindPort != 0)
		{
			//uv_ip4_addr("0.0.0.0", htons(bindPort), &client_addr);
			uv_ip4_addr("0.0.0.0", bindPort, &client_addr);
			ret = uv_udp_bind(udp_, (const struct sockaddr*)&client_addr, 0);
			if (ret < 0)
			{
				return -1;
			}
		}
		uv_udp_recv_start(udp_, &BaseSocket::onAllocBuf, &BaseSocket::onRecvMsg);
		return 0;
	}

	int UdpSocket::sendData(const char *data, int len)
	{
		if (std::this_thread::get_id() == NETIOMANAGER->mainLoopThreadId_)
		{
			writeData(data, len);
		}
		else
		{
			//NETIOMANAGER->postMainLoop([this, data, len]() {
			//	this->writeData(data, len);
			//});
			sendDataByRawSocket(data, len);
		}
		return 0;
	}

	int UdpSocket::sendDataByRawSocket(const char *data, int len)
	{
		int udpSocketFd;
		socklen_t addr_len = sizeof(struct sockaddr_in);
		int ret = 0;
		uv_fileno((uv_handle_t*)udp_, &udpSocketFd);
		ret = sendto(udpSocketFd, data, len, 0, (const struct sockaddr*)&server_addr, addr_len);
		return ret;
	}

	// must call by main loop thread
	int UdpSocket::close()
	{
		if (uv_is_active((uv_handle_t*)udp_))
		{
			uv_udp_recv_stop(udp_);
		}
		if (uv_is_closing((uv_handle_t*)udp_) == 0)
		{
			uv_close((uv_handle_t*)udp_, [](uv_handle_t* handle) {
				auto data = static_cast<UdpSocket*>(handle->data);
				data->onClosed();
			});
		}
		else
		{
			onClosed();
		}
		return 0;
	}

	void UdpSocket::onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags)
	{
		if (size > 0)
		{
			sockCb_->onRecvData(buf, size, addr, this);
		}
	}

	int UdpSocket::writeData(const char *data, int len)
	{
		uv_udp_send_t *req;
		uv_buf_t buf;
		buf.base = const_cast<char*>(data);
		buf.len = len;
		req = new uv_udp_send_t;
		uv_udp_send(req, udp_, &buf, 1, (const struct sockaddr*)&server_addr, [](uv_udp_send_t* req, int status) {
			if (req)
			{
				delete req;
			}
			if (status == UV_ECANCELED)
			{
				//write error;
			}
		});

		return 0;
	}

	UdpSocketServer::UdpSocketServer(uv_loop_t *loop, uint16_t port) : BaseSocket(loop, port)
	{
		udpServer_ = new uv_udp_t;
		udpServer_->data = static_cast<void*>(this);
		init(1);
	}

	UdpSocketServer::~UdpSocketServer()
	{
		if (udpServer_)
		{
			delete udpServer_;
			udpServer_ = nullptr;
		}
	}

	void UdpSocketServer::bindAndStart()
	{
		struct sockaddr_in addr;
		
		uv_udp_init(loop_, udpServer_);
		uv_ip4_addr("0.0.0.0", bindPort, &addr);
		int ret = uv_udp_bind(udpServer_, (const struct sockaddr*)&addr, 0);
		if (ret < 0)
		{
			return;
		}
		uv_udp_recv_start(udpServer_, &BaseSocket::onAllocBuf, &BaseSocket::onRecvMsg);
	}

	void UdpSocketServer::sendDataByRawSocket(const char *data, int len, IPAddr &addr)
	{
		struct sockaddr_in remote_addr;
		addr.transform(&remote_addr);
		int udpSocketFd;
		socklen_t addr_len = sizeof(struct sockaddr_in);
		int ret = 0;
		uv_fileno((uv_handle_t*)udpServer_, &udpSocketFd);
		sendto(udpSocketFd, data, len, 0, (const struct sockaddr*)&remote_addr, addr_len);
	}

	int UdpSocketServer::connectServer(IPAddr &addr)
	{
		return 0;
	}

	int UdpSocketServer::sendData(const char *data, int len)
	{
		return 0;
	}

	int UdpSocketServer::sendDataByRawSocket(const char *data, int len)
	{
		return 0;
	}

	int UdpSocketServer::writeData(const char *data, int len, struct sockaddr_in server_addr)
	{
		uv_udp_send_t *req;
		uv_buf_t buf;
		buf.base = const_cast<char*>(data);
		buf.len = len;
		req = new uv_udp_send_t;
		uv_udp_send(req, udpServer_, &buf, 1, (const struct sockaddr*)&server_addr, [](uv_udp_send_t* req, int status) {
			if (req)
			{
				delete req;
			}
			if (status == UV_ECANCELED)
			{
				//write error;
			}
		});

		return 0;
	}

	int UdpSocketServer::close()
	{
		return 0;
	}

	void UdpSocketServer::onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags)
	{
		if (size > 0)
		{ 
			sockCb_->onRecvData(buf, size, addr, this);
		}
	}

	void NetIoManager::init()
	{
		loop_ = new uv_loop_t();
		uv_loop_init(loop_);
		async_ = new AsyncCore(loop_);
	}

	void NetIoManager::startup(bool runInMain)
	{
		if (runInMain)
		{
			mainLoop();
		}
		else
		{
			mainLoopThread_ = std::thread(&NetIoManager::mainLoop, this);
		}
	}

	void NetIoManager::shutdown()
	{
		if (loop_)
		{
			uv_loop_close(loop_);
			free(loop_);
		}
	}

	void NetIoManager::postMainLoop(AsyncCallback callback)
	{
		async_->postAsyncEvent(callback);
	}

	void NetIoManager::mainLoop()
	{
		mainLoopThreadId_ = std::this_thread::get_id();
		uv_run(loop_, UV_RUN_DEFAULT);
	}
}