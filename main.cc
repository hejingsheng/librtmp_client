#include <iostream>

#include "net/NetCore.h"
#include "base/logger.h"
#include "rtmpclient.h"

int main() {

    LogCore::Logger::instance()->startup();

    ILOG("rtmp client start...\n");

    NETIOMANAGER->init();

    RtmpPublishClient *client = new RtmpPublishClient("rtmp://8.135.38.10:1935/live/live1");
    NetCore::IPAddr addr;
    addr.ip = "8.135.38.10";
    addr.port = 1935;
    client->start();

    NETIOMANAGER->startup();

    LogCore::Logger::instance()->shutdown();
    return 0;
}
