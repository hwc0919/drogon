#include <drogon/HttpSimpleController.h>
#include <vector>
#include <mutex>
#include <json/value.h>

using namespace drogon;
class PipeliningErrorController
    : public drogon::HttpSimpleController<PipeliningErrorController>
{
  public:
    virtual void asyncHandleHttpRequest(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) override
    {
        mtx_.lock();
        ++idx_;
        LOG_INFO << "Receive " << std::to_string(idx_);

        if (std::to_string(idx_) != req->getBody())
        {
            LOG_ERROR << "Should not reach here, test condition not meet.";
        }

        decltype(callback) cb1{nullptr};
        unsigned long long idx1{0};

        if (callbacks_.empty())
        {
            LOG_INFO << "Receive " + std::to_string(idx_) + ", cache callback ";
            callbacks_.emplace_back(std::move(callback), idx_);
        }
        else
        {
            auto item = std::move(callbacks_.back());
            callbacks_.pop_back();
            assert(callbacks_.empty());

            cb1 = item.first;
            idx1 = item.second;
        }
        mtx_.unlock();
        LOG_INFO << "Mutex unlock";

        if (cb1)
        {
            Json::Value json;
            auto resp1 = HttpResponse::newHttpResponse();
            resp1->setBody("Receive " + std::to_string(idx_) + ", callback " +
                           std::to_string(idx1));
            LOG_INFO << "Receive " + std::to_string(idx_) + ", callback " +
                            std::to_string(idx1);
            cb1(resp1);

            auto resp2 = HttpResponse::newHttpResponse();
            resp2->setBody("Receive " + std::to_string(idx_) + ", callback " +
                           std::to_string(idx_));
            LOG_INFO << "Receive " + std::to_string(idx_) + ", callback " +
                            std::to_string(idx_);
            callback(resp2);
        }
    }
    PATH_LIST_BEGIN
    // list path definitions here;
    PATH_ADD("/error", Get);
    PATH_LIST_END

  private:
    std::mutex mtx_;
    unsigned long long idx_{0};
    std::vector<std::pair<std::function<void(const HttpResponsePtr &)>,
                          unsigned long long>>
        callbacks_;
};
