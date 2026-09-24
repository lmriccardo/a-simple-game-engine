#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);
    LOG_INFO("Click either button with the left mouse button -- watch the console.");

    asge::Application<UIDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - UI Demo" });
    app.Run();
    return 0;
}
