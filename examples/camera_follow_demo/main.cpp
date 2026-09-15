#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);
    LOG_INFO("Controls: WASD = move player, UP/DOWN = zoom, F = toggle camera smoothing");

    asge::Application<CameraFollowDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - Camera Follow Demo" });
    app.Run();
    return 0;
}
