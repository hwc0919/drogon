/**
 *
 *  @file SubscribeCallbacks.h
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
#pragma once

#include <drogon/nosql/RedisClient.h>

namespace drogon::nosql
{

struct SubscribeCallbacks
{
    RedisResultCallback resultCallback_;
    RedisExceptionCallback exceptionCallback_;
    RedisMessageCallback messageCallback_;
    SubscribeCallbacks(RedisResultCallback &&resultCallback,
                       RedisExceptionCallback &&exceptionCallback,
                       RedisMessageCallback &&messageCallback)
        : resultCallback_(std::move(resultCallback)),
          exceptionCallback_(std::move(exceptionCallback)),
          messageCallback_(std::move(messageCallback))
    {
    }
};

}  // namespace drogon::nosql
