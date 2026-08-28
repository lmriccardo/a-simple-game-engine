#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    SceneDemoGame game;
    asge::Application app(game, asge::ApplicationConfig{ .s_Title = "ASGE - Scene Demo" });
    app.Run();
    return 0;
}
