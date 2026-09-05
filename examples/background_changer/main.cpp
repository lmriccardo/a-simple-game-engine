#include <iostream>
#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<BackgroundChangingGame> app(asge::ApplicationConfig{});
    app.Run();
    return 0;
}
