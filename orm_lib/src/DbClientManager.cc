/**
 *
 *  @file DbClientManager.cc
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

#include "../../lib/src/DbClientManager.h"
#include "DbClientLockFree.h"
#include <drogon/config.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/utils/Utilities.h>
#include <algorithm>

using namespace drogon::orm;
using namespace drogon;

inline std::string escapeConnString(const std::string &str)
{
    bool beQuoted = str.empty() || (str.find(' ') != std::string::npos);

    std::string escaped;
    escaped.reserve(str.size());
    for (auto ch : str)
    {
        if (ch == '\'')
            escaped.push_back('\\');
        else if (ch == '\\')
            escaped.push_back('\\');
        escaped.push_back(ch);
    }

    if (beQuoted)
        return "'" + escaped + "'";
    return escaped;
}

void DbClientManager::createDbClients(
    const std::vector<trantor::EventLoop *> &ioloops)
{
    assert(dbClientsMap_.empty());
    assert(dbFastClientsMap_.empty());
    for (auto &configPtr : dbConfigs_)
    {
#if USE_POSTGRESQL
        if (auto cfg = std::dynamic_pointer_cast<PostgresConfig>(configPtr))
        {
            if (cfg->isFast)
            {
                dbFastClientsMap_[cfg->name] =
                    IOThreadStorage<orm::DbClientPtr>();
                dbFastClientsMap_[cfg->name].init([&](orm::DbClientPtr &c,
                                                      size_t idx) {
                    assert(idx == ioloops[idx]->index());
                    LOG_TRACE << "create fast database client for the thread "
                              << idx;
                    c = std::shared_ptr<orm::DbClient>(
                        new drogon::orm::DbClientLockFree(
                            cfg->buildConnectString(),
                            ioloops[idx],
                            ClientType::PostgreSQL,
#if LIBPQ_SUPPORTS_BATCH_MODE
                            cfg->connectionNumber,
                            cfg->autoBatch));
#else
                            pgCfg->connectionNumber));
#endif
                    if (cfg->timeout > 0.0)
                    {
                        c->setTimeout(cfg->timeout);
                    }
                });
            }
            else
            {
                dbClientsMap_[cfg->name] = drogon::orm::DbClient::newPgClient(
                    cfg->buildConnectString(),
                    cfg->connectionNumber,
                    cfg->autoBatch);
                if (cfg->timeout > 0.0)
                {
                    dbClientsMap_[cfg->name]->setTimeout(cfg->timeout);
                }
            }
        }
#endif  // USE_POSTGRESQL

#if USE_MYSQL
        if (auto cfg = std::dynamic_pointer_cast<MysqlConfig>(configPtr))
        {
            if (cfg->isFast)
            {
                dbFastClientsMap_[cfg->name] =
                    IOThreadStorage<orm::DbClientPtr>();
                dbFastClientsMap_[cfg->name].init([&](orm::DbClientPtr &c,
                                                      size_t idx) {
                    assert(idx == ioloops[idx]->index());
                    LOG_TRACE << "create fast database client for the thread "
                              << idx;
                    c = std::shared_ptr<orm::DbClient>(
                        new drogon::orm::DbClientLockFree(
                            cfg->buildConnectString(),
                            ioloops[idx],
                            ClientType::Mysql,
#if LIBPQ_SUPPORTS_BATCH_MODE
                            cfg->connectionNumber,
                            false));
#else
                            pgCfg->connectionNumber));
#endif
                });
            }
            else
            {
                dbClientsMap_[cfg->name] =
                    drogon::orm::DbClient::newMysqlClient(
                        cfg->buildConnectString(), cfg->connectionNumber);
                if (cfg->timeout > 0.0)
                {
                    dbClientsMap_[cfg->name]->setTimeout(cfg->timeout);
                }
            }
        }
#endif  // USE_MYSQL

#if USE_SQLITE3
        if (auto cfg = std::dynamic_pointer_cast<Sqlite3Config>(configPtr))
        {
            dbClientsMap_[cfg->name] = drogon::orm::DbClient::newSqlite3Client(
                cfg->buildConnectString(), cfg->connectionNumber);
            if (cfg->timeout > 0.0)
            {
                dbClientsMap_[cfg->name]->setTimeout(cfg->timeout);
            }
        }
#endif
    }
}

void DbClientManager::addDbClient(const DbConfigPtr &cfg)
{
    dbConfigs_.push_back(cfg);
}

bool DbClientManager::areAllDbClientsAvailable() const noexcept
{
    for (auto const &pair : dbClientsMap_)
    {
        if (!(pair.second)->hasAvailableConnections())
            return false;
    }
    auto loop = trantor::EventLoop::getEventLoopOfCurrentThread();
    if (loop && loop->index() < app().getThreadNum())
    {
        for (auto const &pair : dbFastClientsMap_)
        {
            if (!(*(pair.second))->hasAvailableConnections())
                return false;
        }
    }
    return true;
}

DbClientManager::~DbClientManager()
{
    for (auto &pair : dbClientsMap_)
    {
        pair.second->closeAll();
    }
    for (auto &pair : dbFastClientsMap_)
    {
        pair.second.init([](DbClientPtr &clientPtr, size_t index) {
            // the main loop;
            std::promise<void> p;
            auto f = p.get_future();
            drogon::getIOThreadStorageLoop(index)->runInLoop(
                [&clientPtr, &p]() {
                    clientPtr->closeAll();
                    p.set_value();
                });
            f.get();
        });
    }
}
