#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);
    LOG_INFO("Controls: SPACE = replay blip, L = toggle looping ambient hum");

    asge::Application<AudioDemoGame> app(asge::ApplicationConfig{});
    app.Run();
    return 0;
}
