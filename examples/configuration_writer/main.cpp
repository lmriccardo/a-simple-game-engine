#include <iostream>
#include <filesystem>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/Configuration/ConfigurationManager.hpp>

using namespace asge::config;

namespace
{

template<typename T>
void PrintSetting(ConfigurationManager const& cm, std::string const& inPath)
{
    auto result = cm.Get<T>(inPath);
    if (!result)
    {
        auto const err = result.Error();
        LOG_ERROR("Error reading ", inPath, ": ", err);
        return;
    }

    std::cout << inPath << " = " << result.Value() << "\n";
}

template<typename T>
void PrintArraySetting(ConfigurationManager const& cm, std::string const& inPath)
{
    auto result = cm.Get<std::vector<T>>(inPath);
    if (!result)
    {
        auto const err = result.Error();
        LOG_ERROR("Error reading ", inPath, ": ", err);
        return;
    }

    std::cout << inPath << " = [";
    auto const& values = result.Value();
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0) std::cout << ", ";
        std::cout << values[i];
    }
    std::cout << "]\n";
}

}

int main()
{
    auto const outPath = std::filesystem::temp_directory_path() / "asge_configuration_writer_example.toml";

    // Build a fresh document from scratch — no existing conf.toml required.
    // Set()/SetArray() create each key as they're called; Table() descends
    // into (creating) a subtable and scopes the calls chained onto it.
    TOMLBuilder builder;
    builder.Set<std::string>("title", "Configuration Writer Demo")
           .Set<int>("version", 1);

    builder.Table("Window")
           .Set<int>("Width", 800)
           .Set<int>("Height", 600)
           .Set<bool>("Fullscreen", false);

    builder.Table("Rendering")
           .Set<int>("Target_Fps", 120)
           .SetArray<std::string>("Enabled_Layers", {"opaque", "transparent", "ui"});

    builder.Table("Audio")
           .Set<int>("Master_Volume", 80)
           .Set<bool>("Muted", false);

    builder.Table("Physics")
           .Set<double>("Fixed_Timestep", 1.0 / 60.0)
           .SetArray<double>("Gravity", {0.0, -9.81});

    // A dotted path descends through (creating) as many intermediate tables
    // as needed in one call — here Input.Keyboard and Input.Mouse are both
    // subtables of an Input table that's never explicitly opened itself.
    builder.Table("Input.Keyboard")
           .SetArray<std::string>("Move_Bindings", {"W", "A", "S", "D"});

    builder.Table("Input.Mouse")
           .Set<double>("Sensitivity", 1.5)
           .Set<bool>("Invert_Y", false);

    std::cout << "--- Built document ---\n" << builder.ToString() << "\n";

    auto save_result = builder.SaveToFile(outPath);
    if (!save_result)
    {
        auto const err = save_result.Error();
        LOG_ERROR("Error saving built config: ", err);
        return 1;
    }

    // Load it back through the normal ConfigurationManager path to prove
    // the written file is a well-formed, re-parseable TOML document.
    ConfigurationManager cm;
    auto load_result = cm.Load(outPath);
    if (!load_result)
    {
        auto const err = load_result.Error();
        LOG_ERROR("Error reloading written config: ", err);
        return 1;
    }

    std::cout << "--- Reloaded via ConfigurationManager ---\n";
    PrintSetting<std::string>(cm, "title");
    PrintSetting<int>(cm, "Window.Width");
    PrintSetting<int>(cm, "Window.Height");
    PrintSetting<int>(cm, "Rendering.Target_Fps");
    PrintArraySetting<std::string>(cm, "Rendering.Enabled_Layers");
    PrintSetting<int>(cm, "Audio.Master_Volume");
    PrintSetting<double>(cm, "Physics.Fixed_Timestep");
    PrintArraySetting<double>(cm, "Physics.Gravity");
    PrintArraySetting<std::string>(cm, "Input.Keyboard.Move_Bindings");
    PrintSetting<double>(cm, "Input.Mouse.Sensitivity");

    return 0;
}
