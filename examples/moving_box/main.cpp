#include <iostream>
#include "MovingBoxGame.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<MovingBoxGame> app(asge::ApplicationConfig{});
    app.Run();
    return 0;
}
