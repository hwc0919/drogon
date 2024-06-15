/**
 *
 *  @file RequestStreamHandler.cc
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "MultipartStreamParser.h"
#include "HttpRequestImpl.h"

#include <drogon/RequestStream.h>
#include <variant>

namespace drogon
{
class RequestStreamImpl : public RequestStream
{
  public:
    RequestStreamImpl(const HttpRequestImplPtr &req) : weakReq_(req)
    {
    }

    ~RequestStreamImpl() override
    {
        if (isSet_.exchange(true))
        {
            return;
        }

        // Drop all data if no reader is set
        if (auto req = weakReq_.lock())
        {
            setHandlerInLoop(req, RequestStreamHandler::newNullHandler());
        }
    }

    void setStreamHandler(RequestStreamHandlerPtr handler) override
    {
        if (isSet_.exchange(true))
        {
            return;
        }

        if (auto req = weakReq_.lock())
        {
            setHandlerInLoop(req, std::move(handler));
        }
    }

    void setHandlerInLoop(const HttpRequestImplPtr &req,
                          RequestStreamHandlerPtr handler)
    {
        if (!req->isStreamMode())
        {
            return;
        }
        auto loop = req->getLoop();
        if (loop->isInLoopThread())
        {
            req->setStreamHandler(std::move(handler));
        }
        else
        {
            loop->queueInLoop(
                [req = std::move(req), handler = std::move(handler)]() mutable {
                    req->setStreamHandler(std::move(handler));
                });
        }
    }

  private:
    std::weak_ptr<HttpRequestImpl> weakReq_;
    std::atomic_bool isSet_{false};
};

namespace internal
{
RequestStreamPtr createRequestStream(const HttpRequestPtr &req)
{
    auto reqImpl = std::static_pointer_cast<HttpRequestImpl>(req);
    if (!reqImpl->isStreamMode())
    {
        return nullptr;
    }
    return std::make_shared<RequestStreamImpl>(
        std::static_pointer_cast<HttpRequestImpl>(req));
}
}  // namespace internal

/**
 * A default implementation for convenience
 */
class DefaultStreamHandler : public RequestStreamHandler
{
  public:
    DefaultStreamHandler(StreamDataCallback dataCb,
                         StreamFinishCallback finishCb)
        : dataCb_(std::move(dataCb)), finishCb_(std::move(finishCb))
    {
    }

    void onStreamData(const char *data, size_t length) override
    {
        dataCb_(data, length);
    }

    void onStreamFinish(std::exception_ptr ex) override
    {
        finishCb_(std::move(ex));
    }

  private:
    StreamDataCallback dataCb_;
    StreamFinishCallback finishCb_;
};

/**
 * Drops all data
 */
class NullStreamHandler : public RequestStreamHandler
{
  public:
    void onStreamData(const char *, size_t length) override
    {
    }

    void onStreamFinish(std::exception_ptr) override
    {
    }
};

/**
 * Parse multipart data and return actual content
 */
class MultipartStreamHandler : public RequestStreamHandler
{
  public:
    MultipartStreamHandler(const std::string &contentType,
                           MultipartHeaderCallback headerCb,
                           StreamDataCallback dataCb,
                           StreamFinishCallback finishCb)
        : parser_(contentType),
          headerCb_(std::move(headerCb)),
          dataCb_(std::move(dataCb)),
          finishCb_(std::move(finishCb))
    {
    }

    void onStreamData(const char *data, size_t length) override
    {
        if (!parser_.isValid() || parser_.isFinished())
        {
            return;
        }
        parser_.parse(data, length, headerCb_, dataCb_);
        if (!parser_.isValid())
        {
            // TODO: should we mix stream error and user error?
            finishCb_(std::make_exception_ptr(
                std::runtime_error("invalid multipart data")));
        }
        else if (parser_.isFinished())
        {
            finishCb_({});
        }
    }

    void onStreamFinish(std::exception_ptr ex) override
    {
        if (!parser_.isValid() || parser_.isFinished())
        {
            return;
        }

        if (!ex)
        {
            finishCb_(std::make_exception_ptr(
                std::runtime_error("incomplete multipart data")));
        }
        else
        {
            finishCb_(std::move(ex));
        }
    }

  private:
    MultipartStreamParser parser_;
    MultipartHeaderCallback headerCb_;
    StreamDataCallback dataCb_;
    StreamFinishCallback finishCb_;
};

RequestStreamHandlerPtr RequestStreamHandler::newHandler(
    StreamDataCallback dataCb,
    StreamFinishCallback finishCb)
{
    return std::make_shared<DefaultStreamHandler>(std::move(dataCb),
                                                  std::move(finishCb));
}

RequestStreamHandlerPtr RequestStreamHandler::newNullHandler()
{
    return std::make_shared<NullStreamHandler>();
}

RequestStreamHandlerPtr RequestStreamHandler::newMultipartHandler(
    const HttpRequestPtr &req,
    MultipartHeaderCallback headerCb,
    StreamDataCallback dataCb,
    StreamFinishCallback finishCb)
{
    return std::make_shared<MultipartStreamHandler>(req->getHeader(
                                                        "content-type"),
                                                    std::move(headerCb),
                                                    std::move(dataCb),
                                                    std::move(finishCb));
}

}  // namespace drogon
