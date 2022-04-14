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
#include <utility>

namespace drogon::nosql
{
class SubscribeContext
{
  public:
    static std::shared_ptr<SubscribeContext> newContext(
        const std::weak_ptr<RedisSubscriber>& weakSub)
    {
        return std::shared_ptr<SubscribeContext>(new SubscribeContext(weakSub));
    }

    void addMessageCallback(RedisMessageCallback&& messageCallback)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        messageCallbacks_.emplace_back(std::move(messageCallback));
    }

    void disable()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        disabled_ = true;
        messageCallbacks_.clear();
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

    bool alive() const
    {
        return !disabled_ && weakSub_.lock() != nullptr;
    }

  private:
    explicit SubscribeContext(std::weak_ptr<RedisSubscriber> weakSub)
        : weakSub_(std::move(weakSub))
    {
    }
    std::list<RedisMessageCallback> messageCallbacks_;
    std::weak_ptr<RedisSubscriber> weakSub_;
    std::mutex mutex_;
    bool disabled_{false};
};

}  // namespace drogon::nosql
