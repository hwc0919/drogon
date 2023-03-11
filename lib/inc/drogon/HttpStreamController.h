/**
 *
 *  HttpSimpleController.h
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

#pragma once

#include <drogon/DrObject.h>
#include <drogon/utils/HttpConstraint.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/ControllerMacro.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <string>
#include <vector>

namespace drogon
{
/**
 * @brief The abstract base class for HTTP simple controllers.
 *
 */
class HttpStreamControllerBase : public virtual DrObjectBase
{
  public:
    /**
     * @brief The function is called when a HTTP request is routed to the
     * controller.
     *
     * @param req The HTTP request.
     * @param callback The callback via which a response is returned.
     * If the response is nullptr, means you are ready to receive stream body
     */
    virtual void onRequestHeaders(
        const HttpRequestPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback) = 0;
    virtual void onReceiveMessage(const char *data, size_t len, bool last);
    virtual ~HttpStreamControllerBase()
    {
    }
};

/**
 * @brief The reflection base class template for HTTP simple controllers
 *
 * @tparam T The type of the implementation class
 * @tparam AutoCreation The flag for automatically creating, user can set this
 * flag to false for classes that have nondefault constructors.
 */
template <typename T, bool AutoCreation = true>
class HttpStreamController : public DrObject<T>, public HttpStreamControllerBase
{
  public:
    static const bool isAutoCreation = AutoCreation;
    virtual ~HttpStreamController()
    {
    }

  protected:
    HttpStreamController()
    {
    }
    static void registerSelf__(
        const std::string &path,
        const std::vector<internal::HttpConstraint> &filtersAndMethods)
    {
        LOG_TRACE << "register simple controller("
                  << HttpStreamController<T, AutoCreation>::classTypeName()
                  << ") on path:" << path;
        app().registerHttpStreamController(
            path,
            HttpStreamController<T, AutoCreation>::classTypeName(),
            filtersAndMethods);
    }

  private:
    class pathRegistrator
    {
      public:
        pathRegistrator()
        {
            if (AutoCreation)
            {
                T::initPathRouting();
            }
        }
    };
    friend pathRegistrator;
    static pathRegistrator registrator_;
    virtual void *touch()
    {
        return &registrator_;
    }
};
template <typename T, bool AutoCreation>
typename HttpStreamController<T, AutoCreation>::pathRegistrator
    HttpStreamController<T, AutoCreation>::registrator_;

}  // namespace drogon
