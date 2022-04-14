#include "WsClient.h"

#include <memory>
#include <unordered_set>

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
        wsConnPtr->send("Channel not found");
        wsConnPtr->shutdown();
        return;
    }

    auto subChannels = wsConnPtr->getContext<std::unordered_set<std::string>>();
    if (subChannels->find(channel) != subChannels->end())
    {
        wsConnPtr->send("Already subscribed to channel " + channel);
        return;
    }

    drogon::app().getRedisClient()->subscribeAsync(
        [channel, wsConnPtr](const std::string& subChannel,
                             const std::string& subMessage) {
            assert(subChannel == channel);
            LOG_INFO << "Receive channel message " << subMessage;
            std::string resp = "{\"channel\":\"" + subChannel +
                               "\",\"message\":\"" + subMessage + "\"}";
            wsConnPtr->send(resp);
        },
        channel);

    subChannels->insert(channel);
    wsConnPtr->send("Subscribe to channel: " + channel);
}

void WsClient::handleNewConnection(const HttpRequestPtr&,
                                   const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_INFO << "WsClient new connection from "
             << wsConnPtr->peerAddr().toIpPort();
    std::shared_ptr<std::unordered_set<std::string>> subChannels =
        std::make_shared<std::unordered_set<std::string>>();
    wsConnPtr->setContext(subChannels);
}

void WsClient::handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_INFO << "WsClient close connection from "
             << wsConnPtr->peerAddr().toIpPort();
    // TODO: unsubscribe channels
    auto subChannels = wsConnPtr->getContext<std::unordered_set<std::string>>();
    for (auto& channel : *subChannels)
    {
        LOG_INFO << "Need to unsubscribe channel " << channel;
    }
}
