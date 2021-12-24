#pragma once

#include <drogon/utils/string_view.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace drogon
{
class QueryParser
{
  public:
    void parseParameters(const drogon::string_view& query);

  private:
    std::unordered_map<std::string, std::vector<std::string>> parameters_;
};

}  // namespace drogon