#include "HttpQuery.h"
#include <drogon/utils/Utilities.h>

using namespace drogon;

void QueryParser::parseParameters(const drogon::string_view &query)
{
    if (!query.empty())
    {
        string_view::size_type pos = 0;
        while ((query[pos] == '?' || isspace(query[pos])) &&
               pos < query.length())
        {
            ++pos;
        }
        auto value = query.substr(pos);
        while ((pos = value.find('&')) != string_view::npos)
        {
            auto coo = value.substr(0, pos);
            auto epos = coo.find('=');
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    ++cpos;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                parameters_[keydecode].push_back(std::move(pdecode));
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            auto &coo = value;
            auto epos = coo.find('=');
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    ++cpos;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                parameters_[keydecode].push_back(std::move(pdecode));
            }
        }
    }

    query = contentView();
    if (query.empty())
        return;
    std::string type = getHeaderBy("content-type");
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    if (type.empty() ||
        type.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        string_view::size_type pos = 0;
        while ((query[pos] == '?' || isspace(query[pos])) &&
               pos < query.length())
        {
            ++pos;
        }
        auto value = query.substr(pos);
        while ((pos = value.find('&')) != string_view::npos)
        {
            auto coo = value.substr(0, pos);
            auto epos = coo.find('=');
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    ++cpos;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                parameters_[keydecode] = pdecode;
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            auto &coo = value;
            auto epos = coo.find('=');
            if (epos != string_view::npos)
            {
                auto key = coo.substr(0, epos);
                string_view::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    ++cpos;
                key = key.substr(cpos);
                auto pvalue = coo.substr(epos + 1);
                std::string pdecode = utils::urlDecode(pvalue);
                std::string keydecode = utils::urlDecode(key);
                parameters_[keydecode] = pdecode;
            }
        }
    }
}