#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    PhysicsDemoGame game;
    asge::Application app(game, asge::ApplicationConfig{ .s_Title = "ASGE - Physics Demo (click: drop box, R: reset)" });
    app.Run();
    return 0;
}
