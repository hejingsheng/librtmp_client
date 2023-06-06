#include <iostream>

#include "net/NetCore.h"
#include "base/logger.h"

using namespace NetCore;

class RtmpClient : public NetCore::ISocketCallback
{
public:
    void start() {
        client = new NetCore::TcpSocket(NETIOMANAGER->loop_);
        client->registerCallback(this);
        NetCore::IPAddr serveraddr;
        serveraddr.ip = "8.135.38.10";
        serveraddr.port = 8080;
        client->connectServer(serveraddr);
    }

protected:
    virtual int onConnect(int status, BaseSocket *pSock) {
        ILOG("connect success\n");
        pSock->sendData("hello", 5);
        return 0;
    }
    virtual int onRecvData(const char *data, int size, const struct sockaddr* addr, BaseSocket *pSock){
        return 0;
    }
    virtual int onClose(BaseSocket *pSock) {
        ILOG("close\n");
        return 0;
    }

private:
    NetCore::TcpSocket *client;
};

int main() {

    LogCore::Logger::instance()->startup();

    ILOG("rtmp client start...\n");

    NETIOMANAGER->init();

    RtmpClient *client = new RtmpClient();
    client->start();

    NETIOMANAGER->startup();

    LogCore::Logger::instance()->shutdown();
    return 0;
}
