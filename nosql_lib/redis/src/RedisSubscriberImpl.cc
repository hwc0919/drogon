/**
 *
 *  @file RedisSubscriberImpl.cpp
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

#include "RedisSubscriberImpl.h"

using namespace drogon::nosql;

RedisSubscriberImpl::~RedisSubscriberImpl()
{
    RedisConnectionPtr conn;
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_)
    {
        conn.swap(conn_);
        conn->getLoop()->runInLoop([conn]() {
            // Run in self loop to avoid blocking
            conn->disconnect();
        });
    }
}

void RedisSubscriberImpl::subscribeAsync(RedisMessageCallback &&messageCallback,
                                         const std::string &channel) noexcept
{
    LOG_TRACE << "Subscribe " << channel;

    std::shared_ptr<SubscribeContext> subCtx;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (subscribes_.find(channel) != subscribes_.end())
        {
            subCtx = subscribes_.at(channel);
        }
        else
        {
            subCtx = SubscribeContext::newContext(shared_from_this());
            subscribes_.emplace(channel, subCtx);
        }
        subCtx->addMessageCallback(std::move(messageCallback));
    }

    RedisConnectionPtr connPtr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connPtr = conn_;
    }

    std::string command = RedisConnection::formatSubscribeCommand(channel);
    LOG_INFO << "Subscribe command: " << command;
    if (connPtr)
    {
        connPtr->sendSubscribe(std::move(command), subCtx);
    }
    else
    {
        LOG_TRACE
            << "no subscribe connection available, push command to buffer";
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.emplace_back(
            std::make_shared<std::function<void(const RedisConnectionPtr &)>>(
                [subCtx, command = std::move(command)](
                    const RedisConnectionPtr &connPtr) mutable {
                    connPtr->sendSubscribe(std::move(command), subCtx);
                }));
    }
}

void RedisSubscriberImpl::unsubscribe(const std::string &channel) noexcept
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto iter = subscribes_.find(channel);
        if (iter == subscribes_.end())
        {
            LOG_DEBUG << "Attempt to unsubscribe from unknown channel "
                      << channel;
            return;
        }
        iter->second->disable();
        subscribes_.erase(iter);
    }

    // TODO: exec subscribe command
}

void RedisSubscriberImpl::subscribeNext(const RedisConnectionPtr &connPtr)
{
    std::shared_ptr<std::function<void(const RedisConnectionPtr &)>> taskPtr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!tasks_.empty())
        {
            taskPtr = std::move(tasks_.front());
            tasks_.pop_front();
        }
    }
    if (taskPtr && (*taskPtr))
    {
        (*taskPtr)(connPtr);
    }
}

void RedisSubscriberImpl::subscribeAll()
{
}
