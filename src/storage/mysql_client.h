#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace game {

using QueryRow = std::unordered_map<std::string, std::string>;
using QueryResult = std::vector<QueryRow>;

class MysqlClient {
public:
    bool Connect();
    bool Execute(const std::string& sql);
    QueryResult Query(const std::string& sql);
};

} // namespace game

