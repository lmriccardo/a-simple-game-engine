#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);
    LOG_INFO("Controls: WASD = pan camera, mouse wheel = zoom, R = reset camera");

    asge::Application<CameraDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - Camera Demo" });
    app.Run();
    return 0;
}
