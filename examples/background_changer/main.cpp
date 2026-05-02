#include <iostream>
#include "src/Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::DEBUG);

    BackgroundChangingGame game;
    asge::Application app(game, asge::ApplicationConfig{});
    app.Run();
    return 0;
}
