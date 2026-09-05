#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<EcsDemoGame> app(asge::ApplicationConfig{ .s_Title = "ASGE - ECS Demo" });
    app.Run();
    return 0;
}
