#ifndef _NET_CORE_H_
#define _NET_CORE_H_

#include "singleton.h"

#include "uv.h"
#include <string>
#include <thread>
#include <unordered_map>
#include "DataBuf.h"
#include "AsyncEvent.h"

#include "app_protocol/wsProtocol.h"
#include "app_protocol/tls.h"
#include "app_protocol/http_parser.h"

namespace NetCore
{
	struct IPAddr
	{
		std::string ip = "";
		uint16_t port = 0;
		struct sockaddr addr;

		void transform(struct sockaddr_in *addr);
		uint64_t generalKey();
		std::string toStr();
		void operator=(struct IPAddr &other);
	};
	class TcpSocketConn;
	using OnRecvDataCallback = std::function<void(const char *, ssize_t, uint64_t, TcpSocketConn*)>;
	using OnConnCloseCallback = std::function<void(uint64_t, TcpSocketConn*)>;
	class BaseSocket;
	class ISocketCallback
	{
	public:
		virtual int onConnect(int status, BaseSocket *pSock) = 0;
		virtual int onRecvData(const char *data, int size, const struct sockaddr* addr, BaseSocket *pSock) = 0;
		virtual int onClose(BaseSocket *pSock) = 0;
		ISocketCallback() = default;
		virtual ~ISocketCallback() = default;
	};

	class BaseSocket
	{
	public:
		BaseSocket(uv_loop_t *loop, uint16_t port);
		virtual ~BaseSocket();

	protected:
		void init(int type);

	public:
		virtual int connectServer(IPAddr &addr) = 0;
		virtual int sendData(const char *data, int len) = 0;
		virtual int sendDataByRawSocket(const char *data, int len) = 0;
		virtual int close() = 0;

	public:
		void registerCallback(ISocketCallback *callback);

	protected:
		virtual void onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags) = 0;
		void onClosed();
		char* getBuf(size_t &len, size_t suggested_size);

	protected:
		static void onAllocBuf(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
		static void onRecvMsg(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf);
		static void onRecvMsg(uv_udp_t* client, ssize_t nread, const uv_buf_t* rcvbuf, const struct sockaddr* addr, unsigned flags);

	protected:
		uv_loop_t *loop_;
		uint16_t bindPort;
		char *buf_;
		ISocketCallback *sockCb_;

	private:
		int socketType; // 0 tcp 1 udp
	};

	class TcpSocket : public BaseSocket
	{
	public:
		TcpSocket(uv_loop_t *loop, uint16_t port = 0);
		virtual ~TcpSocket();

	public:
		virtual int connectServer(IPAddr &addr);
		virtual int sendData(const char *data, int len);
		virtual int sendDataByRawSocket(const char *data, int len);
		virtual int close();

	protected:
		virtual void onConnect(int status);
		virtual void onMessage(char* data, ssize_t size, const struct sockaddr* addr, unsigned flags);

	private:
		int writeData(const char *data, int len);

	protected:
		uv_tcp_t *tcp_;
		uv_connect_t *conn_req_;
	};

	class WebSocketClient : public TcpSocket
	{
	public:
		WebSocketClient(uv_loop_t *loop, uint16_t port = 0);
		virtual ~WebSocketClient();

	public:
		void connect(IPAddr &addr, std::string path);
		void setPingPeriod(int period);
		void sendWs(const char *data, int len, bool text);
		void closeWs();

	protected:
		virtual void onConnect(int status);
		virtual void onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags);

	protected:
		static void onTimer(uv_timer_t *handle);
		void sendPingRequest();
		void process(char *data, ssize_t size);

	private:
		std::string path_;
		std::string host_;
		int pingPeriod_;
		uv_timer_t *pingTimer_;
		WebSocketProtocolBase *wsProtocol_;
		std::string cacheMsg_;
		uint32_t msgLen_;
	};

	class HttpClient : public TcpSocket
	{
	public:
		HttpClient(uv_loop_t *loop, uint16_t port = 0);
		virtual ~HttpClient();

	public:
		void PostData(std::string url, std::string params);
		void GetData(std::string url, std::unordered_map<std::string, std::string> params);

	protected:
		virtual void onConnect(int status);
		virtual void onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags);

	private:
		void parseHttpUrl(std::string url);
		void initHttpHeader();
		void buildHttpData(std::string &data);
		void buildConnect();

	private:
		std::unordered_map<std::string, std::string> httpHeader_;
		std::string httpParams_;
		std::string host_;
		std::string path_;
		uint16_t port;
		int method;

	private:
		http_parser_settings settings;
		http_parser parser;
		std::string header_key;
		std::string header_val;
		bool headerComplete;
		const char *p_body_start_;
		int bodyLen;

	private:
		static int on_message_begin(http_parser* parser);
		static int on_headers_complete(http_parser* parser);
		static int on_message_complete(http_parser* parser);
		static int on_url(http_parser* parser, const char* at, size_t length);
		static int on_header_field(http_parser* parser, const char* at, size_t length);
		static int on_header_value(http_parser* parser, const char* at, size_t length);
		static int on_body(http_parser* parser, const char* at, size_t length);
	};

	class TcpSocketServer;
	class HttpServer;
	class HttpResponseWriter;
	class HttpConn
	{
	public:
		HttpConn(HttpServer *server, uint64_t key);
		virtual ~HttpConn();

	public:
		void onRecvData(const char *data, ssize_t len);

	private:
		HttpServer *server_;
		http_parser_settings settings;
		http_parser parser;
		std::string header_key;
		std::string header_val;
		bool headerComplete;
		bool messageComplete;
		std::string url;
		std::string httpBody;
		const char *p_body_start_;
		char bodyLen;
        HttpResponseWriter *responseWriter;
		std::unordered_map<std::string, std::string> httpHeader_;

	private:
		static int on_message_begin(http_parser* parser);
		static int on_headers_complete(http_parser* parser);
		static int on_message_complete(http_parser* parser);
		static int on_url(http_parser* parser, const char* at, size_t length);
		static int on_header_field(http_parser* parser, const char* at, size_t length);
		static int on_header_value(http_parser* parser, const char* at, size_t length);
		static int on_body(http_parser* parser, const char* at, size_t length);
	};

	class HttpResponseWriter
	{
	public:
		HttpResponseWriter(uint64_t clientkey);
		virtual ~HttpResponseWriter();

	public:
		void set(std::string key, std::string value);
		void set_response_code(int code);
		void set_body(std::string &body);
		void set_content_len(int len);
		int get_content_len() const {return content_len;}
		void write(TcpSocketServer *server);

	private:
		uint64_t key_;
		int code;
		std::string http_body;
		int content_len;
		std::unordered_map<std::string, std::string> headerMap_;
	};

	class IHttpServerHandle
	{
	public:
		IHttpServerHandle() = default;
		virtual ~IHttpServerHandle() = default;
		virtual bool is_404() { return false; }
		virtual int http_server_handle(HttpResponseWriter *writer, const std::string &data) = 0;
	};

	class Http404Handle : public IHttpServerHandle
    {
    public:
        Http404Handle() {;}
        ~Http404Handle() {;}

    public:
        virtual bool is_404() {
            return true;
        }

		virtual int http_server_handle(HttpResponseWriter *writer, const std::string &data);
    };

	class HttpServer
	{
	public:
		HttpServer(uv_loop_t *loop, uint16_t port, bool https = false);
		virtual ~HttpServer();

	public:
		void start();
		void registerHandle(const std::string &pattern, IHttpServerHandle *handle);
		
	public:
		void onHttpPost(std::string &url, std::string &body, HttpResponseWriter *writer);
		void onHttpGet(std::string &url, std::string &param, HttpResponseWriter *writer);

	private:
	    void find_handle(std::string &pattern, IHttpServerHandle **handle);

	private:
		void onRecvData(const char *data, ssize_t len, uint64_t key, TcpSocketConn *conn);
		void onClosed(uint64_t key, TcpSocketConn *conn);

	private:
		TcpSocketServer *server_;
		std::unordered_map<uint64_t, HttpConn*> httpConnectionMap_;

	private:
		std::unordered_map<std::string, IHttpServerHandle*> httpHandle_;
	};

	class TcpSocketConn : public BaseSocket, public TlsCallback
	{
	public:
		TcpSocketConn(uv_loop_t *loop, uv_tcp_t *tcp, bool ws=false, bool tls=false);
		virtual ~TcpSocketConn();

	public:
		virtual int connectServer(IPAddr &addr);
		virtual int sendData(const char *data, int len);
		virtual int sendDataByRawSocket(const char *data, int len);
		virtual int close();

	public:
		void onTlsConnectStatus(int status);
		void onTlsReadData(char *buf, int size);
		void onTlsWriteData(char *data, int len);

	public:
		void setRecvDataCallback(OnRecvDataCallback callback);
		void setCloseCallback(OnConnCloseCallback callback);

	private:
		int onConnClosed();
		int writeData(const char *data, int len);
		int writeData(const std::string &data);

	protected:
		virtual void onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags);

	private:
		uv_tcp_t *tcp_;
		uint64_t selfkey;
		OnRecvDataCallback onRecvDataCb_;
		OnConnCloseCallback onCloseCb_;

		WebSocketProtocolBase *wsProtocol_;
		SecurityTransport *tlsTranport_;
	};

	class TcpSocketServer
	{
	public:
		TcpSocketServer(uv_loop_t *loop, uint16_t port, bool ws = false, bool tls = false);
		virtual ~TcpSocketServer();

	public:
		void bindAndStart();
		void sendData(uint64_t key, const char *data, int len);
		void close(uint64_t key);
		void setRecvDataCallback(OnRecvDataCallback callback);
		void setCloseCallback(OnConnCloseCallback callback);

	private:
		void createNewClient(uv_stream_t* server);
		void removeClient(uint64_t key);
		void onConnection(uv_stream_t* server);
		void onRecvData(const char *data, ssize_t len, uint64_t key, TcpSocketConn *conn);
		void onClosed(uint64_t key, TcpSocketConn *conn);
		static void on_connection_cb(uv_stream_t* server, int status);

	private:
		uv_loop_t *loop_;
		uv_tcp_t *tcpServer_;
		uint16_t listenPort;
		bool ws_;
		bool tls_;

		OnRecvDataCallback onRecvDataCb_;
		OnConnCloseCallback onCloseCb_;

		std::unordered_map<uint64_t, TcpSocketConn*> clientMap;
	};

	class UdpSocket : public BaseSocket
	{
	public:
		UdpSocket(uv_loop_t *loop, uint16_t port = 0);
		virtual ~UdpSocket();

	public:
		virtual int connectServer(IPAddr &addr);
		virtual int sendData(const char *data, int len);
		virtual int sendDataByRawSocket(const char *data, int len);
		virtual int close();

	protected:
		virtual void onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags);

	private:
		int writeData(const char *data, int len);

	private:
		uv_udp_t *udp_;
		struct sockaddr_in server_addr;
	};

	using OnUdpRecvDataCallback = std::function<void(const char *, ssize_t, const struct sockaddr*)>;
	class UdpSocketServer : public BaseSocket
	{
	public:
		UdpSocketServer(uv_loop_t *loop, uint16_t port);
		virtual ~UdpSocketServer();

	public:
		void bindAndStart();
		void sendDataByRawSocket(const char *data, int len, IPAddr &addr);

	protected:
		virtual int connectServer(IPAddr &addr);
		virtual int sendData(const char *data, int len);
		virtual int sendDataByRawSocket(const char *data, int len);
		virtual int close();

	protected:
		virtual void onMessage(char* buf, ssize_t size, const struct sockaddr* addr, unsigned flags);

	private:
		int writeData(const char *data, int len, struct sockaddr_in server_addr);

	private:
		uv_udp_t *udpServer_;
	};

	class NetIoManager : public core::Singleton<NetIoManager>
	{
	public:
		void init();
		void startup(bool runInMain = true);
		void shutdown();
		void postMainLoop(AsyncCallback callback);

	private:
		void mainLoop();

	public:
		uv_loop_t *loop_;
		std::thread::id mainLoopThreadId_;
	private:
		AsyncCore *async_;
		std::thread mainLoopThread_;
	};
}

#define NETIOMANAGER  NetCore::NetIoManager::instance()

#endif