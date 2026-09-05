#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<AnimationDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - Animation Demo" });
    app.Run();
    return 0;
}
