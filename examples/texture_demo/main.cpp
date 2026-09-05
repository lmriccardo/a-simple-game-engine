#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<TextureDemoGame> app(asge::ApplicationConfig{});
    app.Run();
    return 0;
}
