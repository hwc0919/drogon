/**
 *
 *  @file RedisConnection.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "RedisConnection.h"
#include <drogon/nosql/RedisResult.h>
#include <future>

using namespace drogon::nosql;
RedisConnection::RedisConnection(const trantor::InetAddress &serverAddress,
                                 const std::string &password,
                                 const unsigned int db,
                                 trantor::EventLoop *loop)
    : serverAddr_(serverAddress), password_(password), db_(db), loop_(loop)
{
    assert(loop_);
    loop_->queueInLoop([this]() { startConnectionInLoop(); });
}

void RedisConnection::startConnectionInLoop()
{
    loop_->assertInLoopThread();
    assert(!redisContext_);

    redisContext_ =
        ::redisAsyncConnect(serverAddr_.toIp().c_str(), serverAddr_.toPort());
    status_ = ConnectStatus::kConnecting;
    if (redisContext_->err)
    {
        LOG_ERROR << "Error: " << redisContext_->errstr;

        if (disconnectCallback_)
        {
            disconnectCallback_(shared_from_this());
        }

        // Strange things have happend. In some kinds of connection errors, such
        // as setsockopt errors, hiredis already set redisContext_->c.fd to -1,
        // but the tcp connection stays in ESTABLISHED status. And there is no
        // way for us to obtain the fd of that socket nor close it. This
        // probably is a bug of hiredis.
        return;
    }

    redisContext_->ev.addWrite = addWrite;
    redisContext_->ev.delWrite = delWrite;
    redisContext_->ev.addRead = addRead;
    redisContext_->ev.delRead = delRead;
    redisContext_->ev.cleanup = cleanup;
    redisContext_->ev.data = this;

    channel_ = std::make_unique<trantor::Channel>(loop_, redisContext_->c.fd);
    channel_->setReadCallback([this]() { handleRedisRead(); });
    channel_->setWriteCallback([this]() { handleRedisWrite(); });
    redisAsyncSetConnectCallback(
        redisContext_, [](const redisAsyncContext *context, int status) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            if (status != REDIS_OK)
            {
                LOG_ERROR << "Failed to connect to "
                          << thisPtr->serverAddr_.toIpPort() << "! "
                          << context->errstr;
                thisPtr->handleDisconnect();
                if (thisPtr->disconnectCallback_)
                {
                    thisPtr->disconnectCallback_(thisPtr->shared_from_this());
                }
            }
            else
            {
                LOG_TRACE << "Connected successfully to "
                          << thisPtr->serverAddr_.toIpPort();
                if (thisPtr->password_.empty())
                {
                    if (thisPtr->db_ == 0)
                    {
                        thisPtr->status_ = ConnectStatus::kConnected;
                        if (thisPtr->connectCallback_)
                        {
                            thisPtr->connectCallback_(
                                thisPtr->shared_from_this());
                        }
                    }
                }
                else
                {
                    std::weak_ptr<RedisConnection> weakThisPtr =
                        thisPtr->shared_from_this();
                    thisPtr->sendCommand(
                        [weakThisPtr](const RedisResult &r) {
                            auto thisPtr = weakThisPtr.lock();
                            if (!thisPtr)
                                return;
                            if (r.asString() == "OK")
                            {
                                if (thisPtr->db_ == 0)
                                {
                                    thisPtr->status_ =
                                        ConnectStatus::kConnected;
                                    if (thisPtr->connectCallback_)
                                        thisPtr->connectCallback_(
                                            thisPtr->shared_from_this());
                                }
                            }
                            else
                            {
                                LOG_ERROR << r.asString();
                                thisPtr->disconnect();
                                thisPtr->status_ = ConnectStatus::kEnd;
                            }
                        },
                        [weakThisPtr](const std::exception &err) {
                            LOG_ERROR << err.what();
                            auto thisPtr = weakThisPtr.lock();
                            if (!thisPtr)
                                return;
                            thisPtr->disconnect();
                            thisPtr->status_ = ConnectStatus::kEnd;
                        },
                        "auth %s",
                        thisPtr->password_.data());
                }

                if (thisPtr->db_ != 0)
                {
                    LOG_TRACE << "redis db:" << thisPtr->db_;
                    std::weak_ptr<RedisConnection> weakThisPtr =
                        thisPtr->shared_from_this();
                    thisPtr->sendCommand(
                        [weakThisPtr](const RedisResult &r) {
                            auto thisPtr = weakThisPtr.lock();
                            if (!thisPtr)
                                return;
                            if (r.asString() == "OK")
                            {
                                thisPtr->status_ = ConnectStatus::kConnected;
                                if (thisPtr->connectCallback_)
                                {
                                    thisPtr->connectCallback_(
                                        thisPtr->shared_from_this());
                                }
                            }
                            else
                            {
                                LOG_ERROR << r.asString();
                                thisPtr->disconnect();
                                thisPtr->status_ = ConnectStatus::kEnd;
                            }
                        },
                        [weakThisPtr](const std::exception &err) {
                            LOG_ERROR << err.what();
                            auto thisPtr = weakThisPtr.lock();
                            if (!thisPtr)
                                return;
                            thisPtr->disconnect();
                            thisPtr->status_ = ConnectStatus::kEnd;
                        },
                        "select %u",
                        thisPtr->db_);
                }
            }
        });
    redisAsyncSetDisconnectCallback(
        redisContext_, [](const redisAsyncContext *context, int /*status*/) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);

            thisPtr->handleDisconnect();
            if (thisPtr->disconnectCallback_)
            {
                thisPtr->disconnectCallback_(thisPtr->shared_from_this());
            }

            LOG_TRACE << "Disconnected from "
                      << thisPtr->serverAddr_.toIpPort();
        });
}

void RedisConnection::handleDisconnect()
{
    LOG_TRACE << "handleDisconnect";
    loop_->assertInLoopThread();
    while ((!resultCallbacks_.empty()) && (!exceptionCallbacks_.empty()))
    {
        if (exceptionCallbacks_.front())
        {
            exceptionCallbacks_.front()(
                RedisException(RedisErrorCode::kConnectionBroken,
                               "Connection is broken"));
        }
        resultCallbacks_.pop();
        exceptionCallbacks_.pop();
    }
    status_ = ConnectStatus::kEnd;
    channel_->disableAll();
    channel_->remove();
    redisContext_->ev.addWrite = nullptr;
    redisContext_->ev.delWrite = nullptr;
    redisContext_->ev.addRead = nullptr;
    redisContext_->ev.delRead = nullptr;
    redisContext_->ev.cleanup = nullptr;
    redisContext_->ev.data = nullptr;
}
void RedisConnection::addWrite(void *userData)
{
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->enableWriting();
}
void RedisConnection::delWrite(void *userData)
{
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->disableWriting();
}
void RedisConnection::addRead(void *userData)
{
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->enableReading();
}
void RedisConnection::delRead(void *userData)
{
    auto thisPtr = static_cast<RedisConnection *>(userData);
    assert(thisPtr->channel_);
    thisPtr->channel_->disableReading();
}
void RedisConnection::cleanup(void * /*userData*/)
{
    LOG_TRACE << "cleanup";
}

void RedisConnection::handleRedisRead()
{
    if (status_ != ConnectStatus::kEnd)
    {
        redisAsyncHandleRead(redisContext_);
    }
}

void RedisConnection::handleRedisWrite()
{
    if (status_ != ConnectStatus::kEnd)
    {
        redisAsyncHandleWrite(redisContext_);
    }
}

void RedisConnection::sendCommandInLoop(
    const std::string &command,
    RedisResultCallback &&resultCallback,
    RedisExceptionCallback &&exceptionCallback)
{
    resultCallbacks_.emplace(std::move(resultCallback));
    exceptionCallbacks_.emplace(std::move(exceptionCallback));

    redisAsyncFormattedCommand(
        redisContext_,
        [](redisAsyncContext *context, void *r, void * /*userData*/) {
            auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
            thisPtr->handleResult(static_cast<redisReply *>(r));
        },
        nullptr,
        command.c_str(),
        command.length());
}

void RedisConnection::handleResult(redisReply *result)
{
    auto commandCallback = std::move(resultCallbacks_.front());
    resultCallbacks_.pop();
    auto exceptionCallback = std::move(exceptionCallbacks_.front());
    exceptionCallbacks_.pop();
    if (result && result->type != REDIS_REPLY_ERROR)
    {
        commandCallback(RedisResult(result));
    }
    else
    {
        if (result)
        {
            exceptionCallback(
                RedisException(RedisErrorCode::kRedisError,
                               std::string{result->str, result->len}));
        }
        else
        {
            exceptionCallback(RedisException(RedisErrorCode::kConnectionBroken,
                                             "Network failure"));
        }
    }
    if (resultCallbacks_.empty())
    {
        assert(exceptionCallbacks_.empty());
        if (idleCallback_)
        {
            idleCallback_(shared_from_this());
        }
    }
}

void RedisConnection::disconnect()
{
    std::promise<int> pro;
    auto f = pro.get_future();
    auto thisPtr = shared_from_this();
    loop_->runInLoop([thisPtr, &pro]() {
        redisAsyncDisconnect(thisPtr->redisContext_);
        pro.set_value(1);
    });
    f.get();
}

void RedisConnection::sendSubscribe(
    const std::shared_ptr<SubscribeContext> &subCtx,
    bool subscribe)
{
    if (loop_->isInLoopThread())
    {
        sendSubscribeInLoop(subCtx, subscribe);
    }
    else
    {
        loop_->queueInLoop([this, subCtx, subscribe]() {
            sendSubscribeInLoop(subCtx, subscribe);
        });
    }
}

void RedisConnection::sendSubscribeInLoop(
    const std::shared_ptr<SubscribeContext> &subCtx,
    bool subscribe)
{
    if (subscribe)
    {
        if (!subCtx->alive())
        {
            // Unsub-ed by somewhere else
            return;
        }
        subscribeContexts_.emplace(subCtx->channel(), subCtx);
        redisAsyncFormattedCommand(
            redisContext_,
            [](redisAsyncContext *context, void *r, void *subCtx) {
                auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
                thisPtr->handleSubscribeResult(static_cast<redisReply *>(r),
                                               static_cast<SubscribeContext *>(
                                                   subCtx));
            },
            subCtx.get(),
            subCtx->subscribeCommand().c_str(),
            subCtx->subscribeCommand().size());
    }
    else
    {
        // There is a Hiredis issue here
        // handleUnsubscribeResult() may not be called, and
        // handleSubscribeResult() may be called instead, with
        // first element in result as "unsubscribe".
        // This problem is fixed in 2021-12-02 commit da5a4ff, but have not been
        // released as a tag.
        // For now, we have to deal with unsub logic in both callbacks.
        redisAsyncFormattedCommand(
            redisContext_,
            [](redisAsyncContext *context, void *r, void *subCtx) {
                auto thisPtr = static_cast<RedisConnection *>(context->ev.data);
                thisPtr->handleUnsubscribeResult(
                    static_cast<redisReply *>(r),
                    static_cast<SubscribeContext *>(subCtx));
            },
            subCtx.get(),
            subCtx->unsubscribeCommand().c_str(),
            subCtx->unsubscribeCommand().size());
    }
}

void RedisConnection::handleSubscribeResult(redisReply *result,
                                            SubscribeContext *subCtx)
{
    if (!result)
    {
        LOG_ERROR
            << "Subscribe callback receive empty result (means disconnect?)";
    }
    else if (result->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR << "Subscribe callback receive error result: " << result->str;
    }
    else
    {
        assert(result->type == REDIS_REPLY_ARRAY && result->elements == 3);
        if (strcasecmp(result->element[0]->str, "message") == 0)
        {
            std::string channel(result->element[1]->str,
                                result->element[1]->len);
            std::string message(result->element[2]->str,
                                result->element[2]->len);
            if (!subCtx->alive())
            {
                LOG_ERROR << "Subscribe receive message, but context is no "
                             "longer alive"
                          << ", channel: " << channel
                          << ", message: " << message;
            }
            else
            {
                subCtx->callMessageCallbacks(channel, message);
            }
            // Message callback, no need to call idleCallback_
            return;
        }
        else if (strcmp(result->element[0]->str, "subscribe") == 0)
        {
            std::string channel(result->element[1]->str,
                                result->element[1]->len);
            LOG_INFO << "Subscribe success, channel " << channel;
        }
        // Hiredis issue: the unsub callback doesn't work
        else if (strcmp(result->element[0]->str, "unsubscribe") == 0)
        {
            std::string channel(result->element[1]->str,
                                result->element[1]->len);
            LOG_INFO << "Unsubscribe success, channel " << channel;
            subscribeContexts_.erase(channel);
        }
        else
        {
            LOG_ERROR << "Unknown redis response: " << result->element[0]->str;
        }
    }

    if (idleCallback_)
    {
        idleCallback_(shared_from_this());
    }
}

void RedisConnection::handleUnsubscribeResult(redisReply *result,
                                              SubscribeContext *subCtx)
{
    if (!result)
    {
        LOG_ERROR
            << "Unsubscribe callback receive empty result (means disconnect?)";
    }
    else if (result->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR << "Unsubscribe callback receive error result: "
                  << result->str;
    }
    else
    {
        assert(result->type == REDIS_REPLY_ARRAY && result->elements == 3 &&
               strcmp(result->element[0]->str, "unsubscribe") == 0);

        std::string channel(result->element[1]->str, result->element[1]->len);
        assert(channel == subCtx->channel());
        if (subCtx->alive())
        {
            LOG_ERROR
                << "Unsubscribe callback called, but context is still alive";
        }
        subscribeContexts_.erase(channel);
    }

    if (idleCallback_)
    {
        idleCallback_(shared_from_this());
    }
}
