#include "Chat.h"

#include <memory>
#include <unordered_set>

struct ClientContext
{
    std::string room_;
    std::shared_ptr<nosql::RedisSubscriber> subscriber_;
};

void Chat::handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,
                            std::string&& message,
                            const WebSocketMessageType& type)
{
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
    LOG_DEBUG << "WsClient new message from "
              << wsConnPtr->peerAddr().toIpPort();

    auto context = wsConnPtr->getContext<ClientContext>();

    int operation = 0;
    std::string room;
    if (message.compare(0, 6, "ENTER ") == 0)
    {
        room = message.substr(6);
        operation = 1;
    }
    else if (message == "QUIT")
    {
        operation = 2;
    }

    switch (operation)
    {
        case 0:  // Message
            if (context->room_.empty())
            {
                wsConnPtr->send("ERROR: not in room");
                return;
            }
            drogon::app().getRedisClient()->execCommandAsync(
                [wsConnPtr, room = context->room_, message](
                    const nosql::RedisResult&) {},
                [wsConnPtr](const nosql::RedisException& ex) {
                    wsConnPtr->send(std::string("ERROR: ") + ex.what());
                },
                "publish %s %s",
                context->room_.c_str(),
                message.c_str());
            break;
        case 1:  // Enter
            if (!context->room_.empty())
            {
                context->subscriber_->unsubscribe(context->room_);
                wsConnPtr->send("INFO: Quit room " + context->room_);
            }
            wsConnPtr->send("INFO: Enter room " + room);
            context->subscriber_->subscribe(
                [wsConnPtr](const std::string& room, const std::string& msg) {
                    wsConnPtr->send("[" + room + "]: " + msg);
                },
                room);
            context->room_ = room;
            break;
        case 2:  // QUIT
            if (!context->room_.empty())
            {
                context->subscriber_->unsubscribe(context->room_);
                wsConnPtr->send("INFO: Quit room " + context->room_);
                context->room_.clear();
            }
            else
            {
                wsConnPtr->send("ERROR: Not in room");
            }
            break;
        default:
            break;
    }
}

void Chat::handleNewConnection(const HttpRequestPtr&,
                               const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_DEBUG << "WsClient new connection from "
              << wsConnPtr->peerAddr().toIpPort();
    std::shared_ptr<ClientContext> context = std::make_shared<ClientContext>();
    context->subscriber_ = drogon::app().getRedisClient()->newSubscriber();
    wsConnPtr->setContext(context);
}

void Chat::handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_DEBUG << "WsClient close connection from "
              << wsConnPtr->peerAddr().toIpPort();
    // Channels will be auto unsubscribed when subscriber destructed.
    // auto context = wsConnPtr->getContext<ClientContext>();
    // for (auto& channel : context->channels_)
    // {
    //     context->subscriber_->unsubscribe(channel);
    // }
    wsConnPtr->clearContext();
}
