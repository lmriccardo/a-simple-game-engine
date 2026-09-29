#include "Game.hpp"

int main(int, char**)
{
    LOG_INSTANCE().SetLogLevel(asge::logger::LogLevel::Debug);
    LOG_INFO("Click either button with the left mouse button -- watch the console.");

    // Taller than the 800x600 default -- four widget rows plus margins no
    // longer fit in 600px.
    asge::Application<UIDemoGame> app(asge::ApplicationConfig{
        .s_Height = 700, .s_Title = "ASGE - UI Demo"
    });
    app.Run();
    return 0;
}
