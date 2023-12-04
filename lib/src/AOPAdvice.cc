/**
 *
 *  AOPAdvice.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "AOPAdvice.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"

namespace drogon
{

static void doAdvicesChain(
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>> &advices,
    size_t index,
    const HttpRequestImplPtr &req,
    std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &&callbackPtr);

void AopAdvice::passPreRoutingObservers(const HttpRequestImplPtr &req)
{
    if (!preRoutingObservers_.empty())
    {
        for (auto &observer : preRoutingObservers_)
        {
            observer(req);
        }
    }
}

void AopAdvice::passPreRoutingAdvices(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (preRoutingAdvices_.empty())
    {
        callback(nullptr);
        return;
    }

    auto callbackPtr =
        std::make_shared<std::decay_t<decltype(callback)>>(std::move(callback));
    doAdvicesChain(preRoutingAdvices_, 0, req, std::move(callbackPtr));
}

void AopAdvice::passPostRoutingObservers(const HttpRequestImplPtr &req)
{
    if (!postRoutingObservers_.empty())
    {
        for (auto &observer : postRoutingObservers_)
        {
            observer(req);
        }
    }
}

void AopAdvice::passPostRoutingAdvices(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (postRoutingAdvices_.empty())
    {
        callback(nullptr);
        return;
    }

    auto callbackPtr =
        std::make_shared<std::decay_t<decltype(callback)>>(std::move(callback));
    doAdvicesChain(postRoutingAdvices_, 0, req, std::move(callbackPtr));
}

void AopAdvice::passPreHandlingObservers(const HttpRequestImplPtr &req)
{
    if (!preHandlingObservers_.empty())
    {
        for (auto &observer : preHandlingObservers_)
        {
            observer(req);
        }
    }
}

void AopAdvice::passPreHandlingAdvices(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (preHandlingAdvices_.empty())
    {
        callback(nullptr);
        return;
    }

    auto callbackPtr =
        std::make_shared<std::decay_t<decltype(callback)>>(std::move(callback));
    doAdvicesChain(preHandlingAdvices_, 0, req, std::move(callbackPtr));
}

void AopAdvice::passPostHandlingAdvices(const HttpRequestImplPtr &req,
                                        const HttpResponsePtr &resp)
{
    for (auto &advice : postHandlingAdvices_)
    {
        advice(req, resp);
    }
}

static void doAdvicesChain(
    const std::vector<std::function<void(const HttpRequestPtr &,
                                         AdviceCallback &&,
                                         AdviceChainCallback &&)>> &advices,
    size_t index,
    const HttpRequestImplPtr &req,
    std::shared_ptr<const std::function<void(const HttpResponsePtr &)>>
        &&callbackPtr)
{
    if (index < advices.size())
    {
        auto &advice = advices[index];
        advice(
            req,
            [/*copy*/ callbackPtr](const HttpResponsePtr &resp) {
                (*callbackPtr)(resp);
            },
            [index, req, callbackPtr, &advices]() mutable {
                auto ioLoop = req->getLoop();
                if (ioLoop && !ioLoop->isInLoopThread())
                {
                    ioLoop->queueInLoop([index,
                                         req,
                                         callbackPtr = std::move(callbackPtr),
                                         &advices]() mutable {
                        doAdvicesChain(advices,
                                       index + 1,
                                       req,
                                       std::move(callbackPtr));
                    });
                }
                else
                {
                    doAdvicesChain(advices,
                                   index + 1,
                                   req,
                                   std::move(callbackPtr));
                }
            });
    }
    else
    {
        (*callbackPtr)(nullptr);
    }
}

}  // namespace drogon
