#include <iostream>
#include <filesystem>
#include <ASGE/Core/Configuration/ConfigurationManager.hpp>
#include <ASGE/Core/Filesystem/Filesystem.hpp>

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
    // Work on a throwaway copy of the shipped conf.toml so this example can
    // freely Set/Save without mutating the checked-in fixture.
#ifdef _WIN32
    asge::filesystem::Path const sourcePath = "C:\\Users\\ricca\\Desktop\\dev\\asge\\examples\\configuration_reader\\conf.toml";
#else
    asge::filesystem::Path const sourcePath = "/home/ricca/personal/a-simple-game-engine/examples/configuration_reader/conf.toml";
#endif
    asge::filesystem::Path const workingPath = std::filesystem::temp_directory_path() / "asge_configuration_reader_example.toml";

    auto copy_result = asge::filesystem::Copy( sourcePath, workingPath );
    if (!copy_result)
    {
        auto const err = copy_result.Error();
        LOG_ERROR("Error copying conf: ", err);
        return 1;
    }

    ConfigurationManager cm( asge::concurrent::WithCancel() );
    auto load_result = cm.Load( workingPath );
    if (!load_result)
    {
        auto const err = load_result.Error();
        LOG_ERROR("Error loading conf: ", err);
        return 1;
    }

    std::cout << "Before Set" << std::endl;
    PrintIntSetting(cm, "Window.Width");
    PrintIntSetting(cm, "Window.Height");
    PrintIntSetting(cm, "Rendering.Target_Fps");
    cm.SetHotReloadEnabled( true );

    std::this_thread::sleep_for( std::chrono::seconds(2) );

    // auto set_result = cm.Set<int>("Window.Width", 1024);
    // if (!set_result)
    // {
    //     auto const err = set_result.Error();
    //     LOG_ERROR("Error setting Window.Width: ", err);
    //     return 1;
    // }

    // auto save_result = cm.Save();
    // if (!save_result)
    // {
    //     auto const err = save_result.Error();
    //     LOG_ERROR("Error saving conf: ", err);
    //     return 1;
    // }

    // std::cout << "After Set+Save" << std::endl;
    // PrintIntSetting(cm, "Window.Width");
    // PrintIntSetting(cm, "Window.Height");
    // PrintIntSetting(cm, "Rendering.Target_Fps");

    return 0;
}
