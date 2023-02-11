//
// Created by wanchen.he on 2023/2/11.
//
#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <thread>
#include <chrono>

using namespace drogon;
using namespace std::chrono_literals;

int main()
{
    HttpClientPtr client;
    drogon::app().getLoop()->queueInLoop([&client]() {
        std::this_thread::sleep_for(1s);
        client = HttpClient::newHttpClient("http://127.0.0.1:55555");
        client->setPipeliningDepth(64);

        for (int i = 1; i <= 2; ++i)
        {
            auto req = HttpRequest::newHttpRequest();
            req->setPath("/error");
            req->setBody(std::to_string(i));
            LOG_INFO << "Send request " << i;
            client->sendRequest(
                req,
                [i](ReqResult r, const HttpResponsePtr& resp) {
                    LOG_INFO << "callback " << i << " res: " << r;
                    if (r == ReqResult::Ok)
                    {
                        LOG_INFO << "response " << i << ": " << resp->body();
                    }
                    if (r == ReqResult::Timeout)
                    {
                        LOG_INFO << "Haha, I find the bug.";
                    }
                },
                5);
        }
        LOG_INFO << "Finish sending requests.";
    });

    drogon::app().addListener("0.0.0.0", 55555);
    drogon::app().run();
    return 0;
}
