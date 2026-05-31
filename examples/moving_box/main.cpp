#include <iostream>
#include "MovingBoxGame.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    MovingBoxGame game;
    asge::Application app(game, asge::ApplicationConfig{});
    app.Run();
    return 0;
}