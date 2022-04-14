/**
 *
 *  @file SubscribeContext.h
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
#include <list>
#include <mutex>

namespace drogon::nosql
{
class SubscribeContext
{
  public:
    static std::shared_ptr<SubscribeContext> newContext()
    {
        return std::shared_ptr<SubscribeContext>(new SubscribeContext());
    }

    void addMessageCallback(RedisMessageCallback&& messageCallback)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messageCallbacks_.emplace_back(std::move(messageCallback));
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messageCallbacks_.clear();
    }

    void callMessageCallbacks(const std::string& channel,
                              const std::string& message)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& callback : messageCallbacks_)
        {
            callback(channel, message);
        }
    }

  private:
    SubscribeContext() = default;
    std::list<RedisMessageCallback> messageCallbacks_;
    std::mutex mutex_;
};

}  // namespace drogon::nosql
