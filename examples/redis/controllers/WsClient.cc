#include "WsClient.h"

#include <memory>
#include <unordered_set>

struct ClientContext
{
    std::unordered_set<std::string> channels_;
    std::shared_ptr<nosql::RedisSubscriber> subscriber_;
};

void WsClient::handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,
                                std::string&& message,
                                const WebSocketMessageType& type)
{
    LOG_INFO << "WsClient new message from "
             << wsConnPtr->peerAddr().toIpPort();
    if (type == WebSocketMessageType::Ping ||
        type == WebSocketMessageType::Pong ||
        type == WebSocketMessageType::Close)
    {
        return;
    }

    if (type != WebSocketMessageType::Text)
    {
        LOG_ERROR << "Unsupported message type " << (int)type;
        return;
    }

    std::string channel = std::move(message);
    if (channel.empty())
    {
        wsConnPtr->send("Channel not provided");
        return;
    }

    bool subscribe = true;
    if (channel.compare(0, 6, "unsub ") == 0)
    {
        channel = channel.substr(6);
        subscribe = false;
    }

    auto context = wsConnPtr->getContext<ClientContext>();

    if (subscribe)
    {
        if (context->channels_.find(channel) != context->channels_.end())
        {
            wsConnPtr->send("Already subscribed to channel " + channel);
            return;
        }

        context->subscriber_->subscribeAsync(
            [channel, wsConnPtr](const std::string& subChannel,
                                 const std::string& subMessage) {
                assert(subChannel == channel);
                LOG_INFO << "Receive channel message " << subMessage;
                std::string resp = "{\"channel\":\"" + subChannel +
                                   "\",\"message\":\"" + subMessage + "\"}";
                wsConnPtr->send(resp);
            },
            channel);

        context->channels_.insert(channel);
        wsConnPtr->send("Subscribe to channel: " + channel);
    }
    else
    {
        if (context->channels_.find(channel) == context->channels_.end())
        {
            wsConnPtr->send("Channel not subscribed.");
            return;
        }
        context->channels_.erase(channel);
        context->subscriber_->unsubscribe(channel);
        wsConnPtr->send("Unsubscribe from channel: " + channel);
    }
}

void WsClient::handleNewConnection(const HttpRequestPtr&,
                                   const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_INFO << "WsClient new connection from "
             << wsConnPtr->peerAddr().toIpPort();

    std::shared_ptr<ClientContext> context = std::make_shared<ClientContext>();
    context->subscriber_ = drogon::app().getRedisClient()->newSubscriber();
    wsConnPtr->setContext(context);
}

void WsClient::handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_INFO << "WsClient close connection from "
             << wsConnPtr->peerAddr().toIpPort();
    // TODO: unsubscribe channels
    auto context = wsConnPtr->getContext<ClientContext>();
    for (auto& channel : context->channels_)
    {
        LOG_INFO << "Need to unsubscribe channel " << channel;
        context->subscriber_->unsubscribe(channel);
    }
    wsConnPtr->clearContext();
}
