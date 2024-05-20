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
    for (auto &dbInfo : dbInfos_)
    {
        if (dbInfo.isFast_)
        {
            if (dbInfo.dbType_ == drogon::orm::ClientType::Sqlite3)
            {
                LOG_ERROR << "Sqlite3 don't support fast mode";
                abort();
            }
            if (dbInfo.dbType_ == drogon::orm::ClientType::PostgreSQL ||
                dbInfo.dbType_ == drogon::orm::ClientType::Mysql)
            {
                dbFastClientsMap_[dbInfo.name_] =
                    IOThreadStorage<orm::DbClientPtr>();
                dbFastClientsMap_[dbInfo.name_].init([&](orm::DbClientPtr &c,
                                                         size_t idx) {
                    assert(idx == ioloops[idx]->index());
                    LOG_TRACE << "create fast database client for the thread "
                              << idx;
                    c = std::shared_ptr<orm::DbClient>(
                        new drogon::orm::DbClientLockFree(
                            dbInfo.connectionInfo_,
                            ioloops[idx],
                            dbInfo.dbType_,
#if LIBPQ_SUPPORTS_BATCH_MODE
                            dbInfo.connectionNumber_,
                            dbInfo.autoBatch_));
#else
                            dbInfo.connectionNumber_));
#endif
                    if (dbInfo.timeout_ > 0.0)
                    {
                        c->setTimeout(dbInfo.timeout_);
                    }
                });
            }
        }
        else
        {
            if (dbInfo.dbType_ == drogon::orm::ClientType::PostgreSQL)
            {
#if USE_POSTGRESQL
                dbClientsMap_[dbInfo.name_] =
                    drogon::orm::DbClient::newPgClient(dbInfo.connectionInfo_,
                                                       dbInfo.connectionNumber_,
                                                       dbInfo.autoBatch_);
                if (dbInfo.timeout_ > 0.0)
                {
                    dbClientsMap_[dbInfo.name_]->setTimeout(dbInfo.timeout_);
                }
#endif
            }
            else if (dbInfo.dbType_ == drogon::orm::ClientType::Mysql)
            {
#if USE_MYSQL
                dbClientsMap_[dbInfo.name_] =
                    drogon::orm::DbClient::newMysqlClient(
                        dbInfo.connectionInfo_, dbInfo.connectionNumber_);
                if (dbInfo.timeout_ > 0.0)
                {
                    dbClientsMap_[dbInfo.name_]->setTimeout(dbInfo.timeout_);
                }
#endif
            }
            else if (dbInfo.dbType_ == drogon::orm::ClientType::Sqlite3)
            {
#if USE_SQLITE3
                dbClientsMap_[dbInfo.name_] =
                    drogon::orm::DbClient::newSqlite3Client(
                        dbInfo.connectionInfo_, dbInfo.connectionNumber_);
                if (dbInfo.timeout_ > 0.0)
                {
                    dbClientsMap_[dbInfo.name_]->setTimeout(dbInfo.timeout_);
                }
#endif
            }
        }
    }
}

void DbClientManager::addDbClient(const DbConfig &config)
{
    if (std::holds_alternative<PostgresConfig>(config))
    {
#if USE_POSTGRESQL
        auto &cfg = std::get<PostgresConfig>(config);
        auto connStr =
            utils::formattedString("host=%s port=%u dbname=%s user=%s",
                                   escapeConnString(cfg.host).c_str(),
                                   cfg.port,
                                   escapeConnString(cfg.databaseName).c_str(),
                                   escapeConnString(cfg.username).c_str());
        if (!cfg.password.empty())
        {
            connStr += " password=";
            connStr += escapeConnString(cfg.password);
        }
        if (!cfg.characterSet.empty())
        {
            connStr += " client_encoding=";
            connStr += escapeConnString(cfg.characterSet);
        }
        DbInfo info;
        info.connectionInfo_ = connStr;
        info.connectionNumber_ = cfg.connectionNumber;
        info.isFast_ = cfg.isFast;
        info.name_ = cfg.name;
        info.timeout_ = cfg.timeout;
        info.autoBatch_ = cfg.autoBatch;
        // For valid connection options, see:
        // https://www.postgresql.org/docs/16/libpq-connect.html#LIBPQ-CONNECT-OPTIONS
        if (!cfg.connectOptions.empty())
        {
            std::string optionStr = " options='";
            for (auto const &[key, value] : cfg.connectOptions)
            {
                optionStr += " -c ";
                optionStr += escapeConnString(key);
                optionStr += "=";
                optionStr += escapeConnString(value);
            }
            optionStr += "'";
            info.connectionInfo_ += optionStr;
        }
        info.dbType_ = orm::ClientType::PostgreSQL;
        dbInfos_.push_back(info);
#else
        std::cout
            << "The PostgreSQL is not supported by drogon, please install "
               "the development library first."
            << std::endl;
        exit(1);
#endif
    }
    else if (std::holds_alternative<MysqlConfig>(config))
    {
#if USE_MYSQL
        auto cfg = std::get<MysqlConfig>(config);
        auto connStr =
            utils::formattedString("host=%s port=%u dbname=%s user=%s",
                                   escapeConnString(cfg.host).c_str(),
                                   cfg.port,
                                   escapeConnString(cfg.databaseName).c_str(),
                                   escapeConnString(cfg.username).c_str());
        if (!cfg.password.empty())
        {
            connStr += " password=";
            connStr += escapeConnString(cfg.password);
        }
        if (!cfg.characterSet.empty())
        {
            connStr += " client_encoding=";
            connStr += escapeConnString(cfg.characterSet);
        }
        DbInfo info;
        info.connectionInfo_ = connStr;
        info.connectionNumber_ = cfg.connectionNumber;
        info.isFast_ = cfg.isFast;
        info.name_ = cfg.name;
        info.timeout_ = cfg.timeout;
        info.dbType_ = orm::ClientType::Mysql;
        dbInfos_.push_back(info);
#else
        std::cout << "The Mysql is not supported by drogon, please install the "
                     "development library first."
                  << std::endl;
        exit(1);
#endif
    }
    else if (std::holds_alternative<Sqlite3Config>(config))
    {
#if USE_SQLITE3
        auto cfg = std::get<Sqlite3Config>(config);
        std::string sqlite3ConnStr = "filename=" + cfg.filename;
        DbInfo info;
        info.connectionInfo_ = sqlite3ConnStr;
        info.connectionNumber_ = cfg.connectionNumber;
        info.name_ = cfg.name;
        info.timeout_ = cfg.timeout;
        info.dbType_ = orm::ClientType::Sqlite3;
        dbInfos_.push_back(info);
#else
        std::cout
            << "The Sqlite3 is not supported by drogon, please install the "
               "development library first."
            << std::endl;
        exit(1);
#endif
    }
    else
    {
        assert(false && "Should not happen, unknown database type");
    }
}

void DbClientManager::addDbClient(const DbGeneralConfig &cfg)
{
    auto connStr =
        utils::formattedString("host=%s port=%u dbname=%s user=%s",
                               escapeConnString(cfg.host).c_str(),
                               cfg.port,
                               escapeConnString(cfg.databaseName).c_str(),
                               escapeConnString(cfg.username).c_str());
    if (!cfg.password.empty())
    {
        connStr += " password=";
        connStr += escapeConnString(cfg.password);
    }
    std::string type = cfg.dbType;
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
        return tolower(c);
    });
    if (!cfg.characterSet.empty())
    {
        connStr += " client_encoding=";
        connStr += escapeConnString(cfg.characterSet);
    }
    DbInfo info;
    info.connectionInfo_ = connStr;
    info.connectionNumber_ = cfg.connectionNumber;
    info.isFast_ = cfg.isFast;
    info.name_ = cfg.name;
    info.timeout_ = cfg.timeout;
    info.autoBatch_ = cfg.autoBatch;

    if (type == "postgresql" || type == "postgres")
    {
#if USE_POSTGRESQL
        // For valid connection options, see:
        // https://www.postgresql.org/docs/16/libpq-connect.html#LIBPQ-CONNECT-OPTIONS
        if (!cfg.connectOptions.empty())
        {
            std::string optionStr = " options='";
            for (auto const &[key, value] : cfg.connectOptions)
            {
                optionStr += " -c ";
                optionStr += escapeConnString(key);
                optionStr += "=";
                optionStr += escapeConnString(value);
            }
            optionStr += "'";
            info.connectionInfo_ += optionStr;
        }
        info.dbType_ = orm::ClientType::PostgreSQL;
        dbInfos_.push_back(info);
#else
        std::cout
            << "The PostgreSQL is not supported by drogon, please install "
               "the development library first."
            << std::endl;
        exit(1);
#endif
    }
    else if (type == "mysql")
    {
#if USE_MYSQL
        info.dbType_ = orm::ClientType::Mysql;
        dbInfos_.push_back(info);
#else
        std::cout << "The Mysql is not supported by drogon, please install the "
                     "development library first."
                  << std::endl;
        exit(1);
#endif
    }
    else if (type == "sqlite3")
    {
#if USE_SQLITE3
        std::string sqlite3ConnStr = "filename=" + cfg.filename;
        info.connectionInfo_ = sqlite3ConnStr;
        info.dbType_ = orm::ClientType::Sqlite3;
        dbInfos_.push_back(info);
#else
        std::cout
            << "The Sqlite3 is not supported by drogon, please install the "
               "development library first."
            << std::endl;
        exit(1);
#endif
    }
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
