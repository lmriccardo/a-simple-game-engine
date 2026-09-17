#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);
    LOG_INFO("Controls: UP/DOWN = adjust speed, SPACE = pause/resume, R = reset");

    asge::Application<PathFollowingDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - Path Following Demo" });
    app.Run();
    return 0;
}
