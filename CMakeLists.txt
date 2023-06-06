cmake_minimum_required(VERSION 3.16)
project(rtmp_client)

set(CMAKE_OSX_ARCHITECTURES "arm64")

set(CMAKE_CXX_STANDARD 11)

link_directories(/usr/local/lib)

include_directories(/usr/local/include)
include_directories(/usr/local/include/glib-2.0)
include_directories(/usr/local/lib/glib-2.0/include)

include_directories(base)
include_directories(net)
include_directories(net/app_protocol)
include_directories(net/app_protocol/librtcp)
include_directories(net/ssl)

add_executable(rtmp_client
        base/autofree.h
        base/Base64.cc
        base/Base64.h
        base/logger.cc
        base/logger.h
        base/netio.c
        base/netio.h
        base/our_md5.c
        base/our_md5.h
        base/our_md5hl.c
        base/singleton.h
        base/utils.cc
        base/utils.h
        net/app_protocol/librtcp/rtcp.c
        net/app_protocol/librtcp/rtcp.h
        net/app_protocol/BitVector.cc
        net/app_protocol/BitVector.h
        net/app_protocol/DigestAuthentication.cc
        net/app_protocol/DigestAuthentication.h
        net/app_protocol/http_parser.c
        net/app_protocol/http_parser.h
        net/app_protocol/rtp_rtcp.cc
        net/app_protocol/rtp_rtcp.h
        net/app_protocol/RTSPCommon.cc
        net/app_protocol/RTSPCommon.h
        net/app_protocol/sha1.cc
        net/app_protocol/sha1.h
        net/app_protocol/stun.cc
        net/app_protocol/stun.h
        net/app_protocol/tls.cc
        net/app_protocol/tls.h
        net/app_protocol/wsProtocol.cc
        net/app_protocol/wsProtocol.h
        net/ssl/Dtls.cc
        net/ssl/Dtls.h
        net/ssl/SSLCertificate.cc
        net/ssl/SSLCertificate.h
        net/AsyncEvent.cc
        net/AsyncEvent.h
        net/DataBuf.cc
        net/DataBuf.h
        net/NetCommon.h
        net/NetCore.cc
        net/NetCore.h
        main.cc)

target_link_libraries(rtmp_client -lpthread -lcrypto -lssl -luv -lsrtp2 -lzlog -lglib-2.0 -lgthread-2.0)