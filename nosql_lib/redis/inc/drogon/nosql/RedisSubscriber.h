/**
 *
 *  @file RedisSubscriber.h
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

#include <string>
#include <drogon/nosql/RedisResult.h>

namespace drogon::nosql
{
class RedisSubscriber
{
  public:
    virtual void subscribeAsync(RedisMessageCallback &&messageCallback,
                                const std::string &channel) noexcept = 0;

    virtual void unsubscribe(const std::string &channel) noexcept = 0;

    virtual ~RedisSubscriber() = default;
};

}  // namespace drogon::nosql
