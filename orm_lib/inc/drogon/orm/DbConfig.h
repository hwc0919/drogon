/**
 *
 *  @file DbConfig.h
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <utility>

namespace drogon::orm
{

struct DbConfig
{
  public:
    virtual std::string buildConnectString() const = 0;
};

using DbConfigPtr = std::shared_ptr<DbConfig>;

struct PostgresConfig : public DbConfig
{
    PostgresConfig(std::string name,
                   std::string host,
                   unsigned short port,
                   std::string databaseName,
                   std::string username,
                   std::string password,
                   size_t connectionNumber,
                   bool isFast,
                   std::string characterSet,
                   double timeout,
                   bool autoBatch,
                   std::unordered_map<std::string, std::string> connectOptions)
        : name(std::move(name)),
          host(std::move(host)),
          port(port),
          databaseName(std::move(databaseName)),
          username(std::move(username)),
          password(std::move(password)),
          connectionNumber(connectionNumber),
          isFast(isFast),
          characterSet(std::move(characterSet)),
          timeout(timeout),
          autoBatch(autoBatch),
          connectOptions(std::move(connectOptions))
    {
    }

    std::string name;
    std::string host;
    unsigned short port;
    std::string databaseName;
    std::string username;
    std::string password;
    size_t connectionNumber;
    bool isFast;
    std::string characterSet;
    double timeout;
    bool autoBatch;
    std::unordered_map<std::string, std::string> connectOptions;

    std::string buildConnectString() const override;
};

struct MysqlConfig : public DbConfig
{
    MysqlConfig(const std::string &name,
                const std::string &host,
                unsigned short port,
                const std::string &databaseName,
                const std::string &username,
                const std::string &password,
                size_t connectionNumber,
                bool isFast,
                const std::string &characterSet,
                double timeout)
        : name(name),
          host(host),
          port(port),
          databaseName(databaseName),
          username(username),
          password(password),
          connectionNumber(connectionNumber),
          isFast(isFast),
          characterSet(characterSet),
          timeout(timeout)
    {
    }

    std::string name;
    std::string host;
    unsigned short port;
    std::string databaseName;
    std::string username;
    std::string password;
    size_t connectionNumber;
    bool isFast;
    std::string characterSet;
    double timeout;

    std::string buildConnectString() const override;
};

struct Sqlite3Config : public DbConfig
{
    Sqlite3Config(const std::string &name,
                  const std::string &filename,
                  size_t connectionNumber,
                  double timeout)
        : name(name),
          filename(filename),
          connectionNumber(connectionNumber),
          timeout(timeout)
    {
    }

    std::string name;
    std::string filename;
    size_t connectionNumber;
    double timeout;

    std::string buildConnectString() const override;
};

}  // namespace drogon::orm

//{
//    auto connStr =
//        utils::formattedString("host=%s port=%u dbname=%s user=%s",
//                               escapeConnString(cfg.host).c_str(),
//                               cfg.port,
//                               escapeConnString(cfg.databaseName).c_str(),
//                               escapeConnString(cfg.username).c_str());
//    if (!cfg.password.empty())
//    {
//        connStr += " password=";
//        connStr += escapeConnString(cfg.password);
//    }
//    std::string type = cfg.dbType;
//    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c)
//    {
//        return tolower(c);
//    });
//    if (!cfg.characterSet.empty())
//    {
//        connStr += " client_encoding=";
//        connStr += escapeConnString(cfg.characterSet);
//    }
//    DbInfo info;
//    info.connectionInfo_ = connStr;
//    info.connectionNumber_ = cfg.connectionNumber;
//    info.isFast_ = cfg.isFast;
//    info.name_ = cfg.name;
//    info.timeout_ = cfg.timeout;
//    info.autoBatch_ = cfg.autoBatch;
//
//    if (type == "postgresql" || type == "postgres")
//    {
// #if USE_POSTGRESQL
//        // For valid connection options, see:
//        //
//        https://www.postgresql.org/docs/16/libpq-connect.html#LIBPQ-CONNECT-OPTIONS
//        if (!cfg.connectOptions.empty())
//        {
//            std::string optionStr = " options='";
//            for (auto const &[key, value] : cfg.connectOptions)
//            {
//                optionStr += " -c ";
//                optionStr += escapeConnString(key);
//                optionStr += "=";
//                optionStr += escapeConnString(value);
//            }
//            optionStr += "'";
//            info.connectionInfo_ += optionStr;
//        }
//        info.dbType_ = orm::ClientType::PostgreSQL;
//        dbInfos_.push_back(info);
// #else
//        std::cout
//            << "The PostgreSQL is not supported by drogon, please install "
//               "the development library first."
//            << std::endl;
//        exit(1);
// #endif
//    }
//    else if (type == "mysql")
//    {
// #if USE_MYSQL
//        info.dbType_ = orm::ClientType::Mysql;
//        dbInfos_.push_back(info);
// #else
//        std::cout << "The Mysql is not supported by drogon, please install the
//        "
//                     "development library first."
//                  << std::endl;
//        exit(1);
// #endif
//    }
//    else if (type == "sqlite3")
//    {
// #if USE_SQLITE3
//        std::string sqlite3ConnStr = "filename=" + cfg.filename;
//        info.connectionInfo_ = sqlite3ConnStr;
//        info.dbType_ = orm::ClientType::Sqlite3;
//        dbInfos_.push_back(info);
// #else
//        std::cout
//            << "The Sqlite3 is not supported by drogon, please install the "
//               "development library first."
//            << std::endl;
//        exit(1);
// #endif
//    }
//}
