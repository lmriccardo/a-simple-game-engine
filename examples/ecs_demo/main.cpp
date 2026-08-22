#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    EcsDemoGame game;
    asge::Application app(game, asge::ApplicationConfig{ .s_Title = "ASGE - ECS Demo" });
    app.Run();
    return 0;
}
