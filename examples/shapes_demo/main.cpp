#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<ShapesDemoGame> app(asge::ApplicationConfig{});
    app.Run();
    return 0;
}
