#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<AnimatorDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - Animator Demo" });
    app.Run();
    return 0;
}
