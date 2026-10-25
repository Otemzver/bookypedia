#include "bookypedia.h"
#include <iostream>
#include "menu/menu.h"
#include "ui/view.h"

namespace bookypedia {
using namespace std::literals;

Application::Application(const AppConfig& config)
    : db_{pqxx::connection{config.db_url}} {}

void Application::Run() {
    menu::Menu menu{std::cin, std::cout};
    menu.AddAction("Help"s, {}, "Show instructions"s, [&menu](std::istream&) {
        menu.ShowInstructions(); return true;
    });
    menu.AddAction("Exit"s, {}, "Exit program"s, [](std::istream&) { return false; });
    ui::View view{menu, db_, std::cin, std::cout};
    menu.Run();
}
}  // namespace bookypedia
