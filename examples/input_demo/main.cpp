#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    InputDemoGame game;
    asge::Application app(game, asge::ApplicationConfig{ .s_Title = "ASGE - Input Demo" });
    app.Run();
    return 0;
}
