#include <iostream>
#include <chrono>
#include <thread>
#include <ASGE/Core/Configuration/ConfigurationManager.hpp>

using namespace asge::config;

namespace
{

void PrintIntSetting(ConfigurationManager const& cm, std::string const& inPath)
{
    auto result = cm.Get<int>(inPath);
    if (!result)
    {
        auto const err = result.Error();
        LOG_ERROR("Error reading ", inPath, ": ", err);
        return;
    }

    std::cout << inPath << " = " << result.Value() << "\n";
}

}

int main()
{
    ConfigurationManager cm( asge::concurrent::WithCancel() );
    auto load_result = cm.Load( "C:\\Users\\ricca\\Desktop\\dev\\asge\\examples\\configuration_reader\\conf.toml" );
    if (!load_result)
    {
        auto const err = load_result.Error();
        LOG_ERROR("Error loading conf: ", err);
        return 1;
    }

    PrintIntSetting(cm, "Window.Width");
    PrintIntSetting(cm, "Window.Height");
    PrintIntSetting(cm, "Rendering.Target_Fps");

    std::this_thread::sleep_for( std::chrono::seconds(10) );

    std::cout << "After Reload" << std::endl;

    PrintIntSetting(cm, "Window.Width");
    PrintIntSetting(cm, "Window.Height");
    PrintIntSetting(cm, "Rendering.Target_Fps");

    return 0;
}
