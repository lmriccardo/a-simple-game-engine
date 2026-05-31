#include <iostream>
#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    BackgroundChangingGame game;
    asge::Application app(game, asge::ApplicationConfig{});
    app.Run();
    return 0;
}
