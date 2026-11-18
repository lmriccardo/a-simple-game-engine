#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);

    asge::Application<CompositingDemoGame> app(asge::ApplicationConfig{ .s_Title = "Orb Rush" });
    app.Run();
    return 0;
}
