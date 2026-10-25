#pragma once
#include <pqxx/pqxx>
#include <string>
#include "postgres/postgres.h"

namespace bookypedia {
struct AppConfig { std::string db_url; };

class Application {
public:
    explicit Application(const AppConfig& config);
    void Run();
private:
    postgres::Database db_;
};
}  // namespace bookypedia
