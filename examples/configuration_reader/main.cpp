#include <iostream>
#include <ASGE/Core/Configuration/ConfigurationManager.hpp>

using namespace asge::config;

int main()
{
    ConfigurationManager cm( asge::concurrent::WithCancel() );
    bool load_result = cm.Load( "C:\\Users\\ricca\\Desktop\\dev\\asge\\examples\\configuration_reader\\conf.toml" );

    auto w = cm.Get<int>("Window.Width");
    auto h = cm.Get<int>("Window.Height");
    auto fps = cm.Get<int>("Rendering.Target_Fps");

    std::cout << "Window.Width  = " << *w << "\n";
    std::cout << "Window.Height = " << *h << "\n";
    std::cout << "Rendering.Target_Fps = " << *fps << std::endl;

    return 0;
}