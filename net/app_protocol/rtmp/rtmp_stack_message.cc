//
// Created by hejingsheng on 2023/6/9.
//

#include "rtmp_stack_message.h"

//rtmp_message::rtmp_message()
//{
//    payload = nullptr;
//    size = 0;
//}
//
//rtmp_message::~rtmp_message()
//{
//    if (payload)
//    {
//        delete[] payload;
//    }
//}
//
//void rtmp_message::create_payload(int size)
//{
//    if (payload)
//    {
//        delete[] payload;
//        payload = nullptr;
//    }
//    payload = new uint8_t[size];
//    this->size = size;
//}
//
//int rtmp_message::create_msg(rtmp_message_header *h, uint8_t *body, int bodylen)
//{
//    if (payload)
//    {
//        delete[] payload;
//    }
//    header = *h;
//    payload = body;
//    size = bodylen;
//    return 0;
//}