/**
 *
 *  @file SubscribeContext.cc
 *  @author Nitromelon
 *
 *  Copyright 2022, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#include "SubscribeContext.h"
#include <stdio.h>
#include <string>

using namespace drogon::nosql;

static std::string formatSubscribeCommand(const std::string &channel)
{
    // Avoid using redisvFormatCommand, we don't want to emit unknown error
    static const char *redisSubFmt =
        "*2\r\n$9\r\nsubscribe\r\n$%zu\r\n%.*s\r\n";
    std::string command;
    if (channel.size() < 32)
    {
        char buf[64];
        size_t bufSize = sizeof(buf);
        int len = snprintf(buf,
                           bufSize,
                           redisSubFmt,
                           channel.size(),
                           (int)channel.size(),
                           channel.c_str());
        command = std::string(buf, len);
    }
    else
    {
        size_t bufSize = channel.size() + 64;
        char *buf = static_cast<char *>(malloc(bufSize));
        int len = snprintf(buf,
                           bufSize,
                           redisSubFmt,
                           channel.size(),
                           (int)channel.size(),
                           channel.c_str());
        command = std::string(buf, len);
        free(buf);
    }
    return command;
}

static std::string formatUnsubscribeCommand(const std::string &channel)
{
    // Avoid using redisvFormatCommand, we don't want to emit unknown error
    static const char *redisSubFmt =
        "*2\r\n$11\r\nunsubscribe\r\n$%zu\r\n%.*s\r\n";
    std::string command;
    if (channel.size() < 32)
    {
        char buf[64];
        size_t bufSize = sizeof(buf);
        int len = snprintf(buf,
                           bufSize,
                           redisSubFmt,
                           channel.size(),
                           (int)channel.size(),
                           channel.c_str());
        command = std::string(buf, len);
    }
    else
    {
        size_t bufSize = channel.size() + 64;
        char *buf = static_cast<char *>(malloc(bufSize));
        int len = snprintf(buf,
                           bufSize,
                           redisSubFmt,
                           channel.size(),
                           (int)channel.size(),
                           channel.c_str());
        command = std::string(buf, len);
        free(buf);
    }
    return command;
}

SubscribeContext::SubscribeContext(std::weak_ptr<RedisSubscriber> weakSub,
                                   std::string channel)
    : weakSub_(weakSub),
      channel_(channel),
      subscribeCommand_(formatSubscribeCommand(channel)),
      unsubscribeCommand_(formatUnsubscribeCommand(channel))
{
}
