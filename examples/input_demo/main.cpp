#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<InputDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - Input Demo" });
    app.Run();
    return 0;
}
