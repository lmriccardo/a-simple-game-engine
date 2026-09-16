#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);
    LOG_INFO("Controls: LEFT/RIGHT = adjust spin speed, SPACE = pause/resume, R = reset");

    asge::Application<RotationDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - Rotation Demo" });
    app.Run();
    return 0;
}
