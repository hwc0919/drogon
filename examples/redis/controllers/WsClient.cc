#include "WsClient.h"

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

    drogon::app().getRedisClient()->subscribeAsync(
        [channel, wsConnPtr](const nosql::RedisResult&) {
            LOG_INFO << "Successfully Subscribed to " << channel;
            wsConnPtr->send("SUBSCRIBE " + channel);
        },
        [channel, wsConnPtr](const nosql::RedisException& ex) {
            LOG_ERROR << "Failed to subscribe to " << channel << ":"
                      << ex.what();
            wsConnPtr->send("FAILED TO SUBSCRIBE " + channel + ": " +
                            ex.what());
        },
        [channel, wsConnPtr](const std::string& subChannel,
                             const std::string& subMessage) {
            assert(subChannel == channel);
            LOG_INFO << "Receive channel message " << subMessage;
            std::string resp = "{\"channel\":\"" + subChannel +
                               "\",\"message\":\"" + subMessage + "\"}";
            wsConnPtr->send(resp);
        },
        channel);
}

void WsClient::handleNewConnection(const HttpRequestPtr&,
                                   const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_INFO << "WsClient new connection from "
             << wsConnPtr->peerAddr().toIpPort();
}

void WsClient::handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_INFO << "WsClient close connection from "
             << wsConnPtr->peerAddr().toIpPort();
}
