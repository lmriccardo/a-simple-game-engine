#include "CppProject.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{

namespace fs = std::filesystem;

std::string ReadAll(fs::path const& inPath)
{
    std::ifstream in(inPath, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

class CppProjectTest : public ::testing::Test
{
protected:
    fs::path m_Root;
    fs::path m_ProjectFile;

    void SetUp() override
    {
        m_Root = fs::temp_directory_path()
            / ("asge_cpp_project_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        fs::create_directories(m_Root);
        m_ProjectFile = m_Root / "demo.asgeproject";
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(m_Root, ec);
    }

    CppProjectInput Input(std::vector<std::string> const& inSceneNames) const
    {
        CppProjectInput input;
        input.m_ProjectFile = m_ProjectFile;
        for (auto const& name : inSceneNames) input.m_Scenes.push_back({ name, m_Root / "scenes" / (name + ".asgescene") });
        input.m_MountDirs = { m_Root / "assets" };
        return input;
    }

    fs::path Code() const { return m_Root / "code"; }
};

// ─── CppStateName ────────────────────────────────────────────────────────────

TEST(CppStateNameTest, SpacesAndPunctuation_BecomePascalCase)
{
    EXPECT_EQ(CppStateName("Main Menu"), "MainMenu");
    EXPECT_EQ(CppStateName("level_one-final"), "LevelOneFinal");
}

TEST(CppStateNameTest, LeadingDigit_GetsScenePrefix)
{
    EXPECT_EQ(CppStateName("2nd level"), "Scene2ndLevel");
}

TEST(CppStateNameTest, NoIdentifierCharacters_FallsBackToScene)
{
    EXPECT_EQ(CppStateName("###"), "Scene");
}

// ─── CreateCppProject ────────────────────────────────────────────────────────

TEST_F(CppProjectTest, Create_WritesGeneratedFilesAndOneStatePerScene)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main Menu", "Level 1" }), "C:/dev/asge").IsOk());

    for (auto const* file : { "CMakeLists.txt", "main.cpp", "Game.hpp", "Game.cpp", "StateId.hpp",
                              "SceneState.hpp", "project_files.cmake" })
    {
        EXPECT_TRUE(fs::exists(Code() / file)) << file;
    }
    for (auto const* file : { "MainMenuState.hpp", "MainMenuState.cpp", "Level1State.hpp", "Level1State.cpp" })
    {
        EXPECT_TRUE(fs::exists(Code() / "states" / file)) << file;
    }
    EXPECT_TRUE(IsCppProjectLinked(m_ProjectFile));
}

TEST_F(CppProjectTest, Create_CMakeListsPointsAtTheAsgeFolder)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    auto const cmake = ReadAll(Code() / "CMakeLists.txt");
    EXPECT_NE(cmake.find("set(ASGE_SOURCE_DIR \"C:/dev/asge\""), std::string::npos);
    EXPECT_NE(cmake.find("project(demo LANGUAGES CXX)"), std::string::npos);
    EXPECT_NE(cmake.find("CMAKE_SIZEOF_VOID_P EQUAL 8"), std::string::npos); // 32-bit kits fail later with a confusing SDL3 message
    EXPECT_NE(cmake.find("-E make_directory"), std::string::npos); // project/ must exist before files are copied into it
}

TEST_F(CppProjectTest, Create_GameMapsEveryStateIdToItsSceneStem)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main Menu", "Level 1" }), "C:/dev/asge").IsOk());

    auto const ids = ReadAll(Code() / "StateId.hpp");
    EXPECT_NE(ids.find("    MainMenu,\n    Level1,\n"), std::string::npos);

    auto const game = ReadAll(Code() / "Game.cpp");
    EXPECT_NE(game.find("\"Main Menu\","), std::string::npos);
    EXPECT_NE(game.find("case StateId::Level1: return std::make_unique<Level1State>( MakeContext( kSceneStems[1] ) );"),
              std::string::npos);
}

TEST_F(CppProjectTest, Create_GameReadsTheWindowSizeFromTheProjectAndMainUsesIt)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    auto const game = ReadAll(Code() / "Game.cpp");
    EXPECT_NE(game.find("config.s_Width = project.Value().m_TargetWidth"), std::string::npos);
    EXPECT_NE(game.find("asge::ApplicationConfig config{ .s_Title = \"demo\" }"), std::string::npos);
    EXPECT_NE(ReadAll(Code() / "main.cpp").find("MakeApplicationConfig()"), std::string::npos);
}

TEST_F(CppProjectTest, Create_SceneStateOffersLookupConnectAndTransitionHelpers)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    auto const base = ReadAll(Code() / "SceneState.hpp");
    for (auto const* member : { "virtual void OnSceneLoaded()", "virtual void OnUpdate(", "Registry& GetRegistry() const", "FindByName(", "T* Find(", "void Connect(",
                                "void Replace(", "void Push(", "void Pop()", "void Quit()", "void OnExit() override" })
    {
        EXPECT_NE(base.find(member), std::string::npos) << member;
    }
}

TEST_F(CppProjectTest, Create_StateFilesOverrideOnSceneLoadedAndOnUpdate)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    auto const header = ReadAll(Code() / "states" / "MainState.hpp");
    EXPECT_NE(header.find("void OnSceneLoaded() override;"), std::string::npos);
    EXPECT_NE(header.find("void OnUpdate( float inDeltaTime, asge::input::InputState const& inInput ) override;"), std::string::npos);
    auto const source = ReadAll(Code() / "states" / "MainState.cpp");
    EXPECT_NE(source.find("void MainState::OnSceneLoaded()"), std::string::npos);
    EXPECT_NE(source.find("void MainState::OnUpdate("), std::string::npos);
    EXPECT_EQ(source.find("SceneState::Update"), std::string::npos); // the base Update() runs OnUpdate() itself
}

TEST_F(CppProjectTest, Create_SceneStateUpdateRunsOnUpdateBeforeHandingOverTheRequestedTransition)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    auto const base = ReadAll(Code() / "SceneState.hpp");
    auto const onUpdate = base.find("OnUpdate( inDeltaTime, inInput );");
    auto const exchange = base.find("return std::exchange( m_Pending, std::nullopt );");
    ASSERT_NE(onUpdate, std::string::npos);
    ASSERT_NE(exchange, std::string::npos);
    EXPECT_LT(onUpdate, exchange);
}

TEST_F(CppProjectTest, Create_DuplicateSanitizedNames_GetNumericSuffixes)
{
    ASSERT_TRUE(CreateCppProject(Input({ "my scene", "My-Scene" }), "C:/dev/asge").IsOk());

    EXPECT_TRUE(fs::exists(Code() / "states" / "MySceneState.cpp"));
    EXPECT_TRUE(fs::exists(Code() / "states" / "MyScene2State.cpp"));
}

TEST_F(CppProjectTest, Create_EverythingInsideTheProjectFolder_IsPortableAndListsWhatToCopy)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    auto const files = ReadAll(Code() / "project_files.cmake");
    EXPECT_NE(files.find("set(ASGE_PROJECT_PORTABLE ON)"), std::string::npos);
    EXPECT_NE(files.find("\"demo.asgeproject\" \"scenes\" \"assets\""), std::string::npos);
}

TEST_F(CppProjectTest, Create_MountOutsideTheProjectFolder_IsNotPortable)
{
    auto input = Input({ "Main" });
    input.m_MountDirs = { fs::path("Z:/elsewhere/assets") };

    ASSERT_TRUE(CreateCppProject(input, "C:/dev/asge").IsOk());

    EXPECT_NE(ReadAll(Code() / "project_files.cmake").find("set(ASGE_PROJECT_PORTABLE OFF)"), std::string::npos);
}

TEST_F(CppProjectTest, Create_WritesVSCodeSettingsForTheProjectRoot_UnlessOneExists)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());
    auto const settings = m_Root / ".vscode" / "settings.json";
    EXPECT_NE(ReadAll(settings).find("\"cmake.sourceDirectory\": \"${workspaceFolder}/code\""), std::string::npos);
}

TEST_F(CppProjectTest, Create_KeepsAnExistingVSCodeSettingsFile)
{
    fs::create_directories(m_Root / ".vscode");
    std::ofstream(m_Root / ".vscode" / "settings.json") << "{ \"mine\": true }";

    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    EXPECT_EQ(ReadAll(m_Root / ".vscode" / "settings.json"), "{ \"mine\": true }");
}

TEST_F(CppProjectTest, Create_AlreadyLinked_ReturnsFileExistsAndTouchesNothing)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());
    std::ofstream(Code() / "main.cpp", std::ios::trunc) << "// mine";

    auto result = CreateCppProject(Input({ "Main" }), "C:/dev/asge");

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::file_exists));
    EXPECT_EQ(ReadAll(Code() / "main.cpp"), "// mine");
}

TEST_F(CppProjectTest, Create_NoScenes_ReturnsInvalidArgument)
{
    auto result = CreateCppProject(Input({}), "C:/dev/asge");

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::invalid_argument));
    EXPECT_FALSE(IsCppProjectLinked(m_ProjectFile));
}

// ─── DeleteCppProject ────────────────────────────────────────────────────────

TEST_F(CppProjectTest, Delete_RemovesTheCodeFolderAndUnlinksTheProject)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    ASSERT_TRUE(DeleteCppProject(m_ProjectFile).IsOk());

    EXPECT_FALSE(fs::exists(Code()));
    EXPECT_FALSE(IsCppProjectLinked(m_ProjectFile));
}

TEST_F(CppProjectTest, Delete_LeavesEverythingOutsideCodeAlone)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());
    fs::create_directories(m_Root / "scenes");
    std::ofstream(m_Root / "scenes" / "Main.asgescene") << "scene";
    std::ofstream(m_ProjectFile) << "project";

    ASSERT_TRUE(DeleteCppProject(m_ProjectFile).IsOk());

    EXPECT_EQ(ReadAll(m_Root / "scenes" / "Main.asgescene"), "scene");
    EXPECT_EQ(ReadAll(m_ProjectFile), "project");
    EXPECT_TRUE(fs::exists(m_Root / ".vscode" / "settings.json"));
}

TEST_F(CppProjectTest, Delete_ProjectWithoutACppProject_ReturnsNoSuchFileAndTouchesNothing)
{
    fs::create_directories(Code() / "unrelated");
    std::ofstream(Code() / "unrelated" / "keep.txt") << "keep"; // a code/ folder we did not generate: no CMakeLists.txt

    auto result = DeleteCppProject(m_ProjectFile);

    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::no_such_file_or_directory));
    EXPECT_EQ(ReadAll(Code() / "unrelated" / "keep.txt"), "keep");
}

TEST_F(CppProjectTest, Delete_ThenCreate_StartsFromScratch)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());
    std::ofstream(Code() / "states" / "MainState.cpp", std::ios::trunc) << "// my gameplay";
    ASSERT_TRUE(DeleteCppProject(m_ProjectFile).IsOk());

    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    EXPECT_EQ(ReadAll(Code() / "states" / "MainState.cpp").find("my gameplay"), std::string::npos);
}

// ─── UpdateCppProject ────────────────────────────────────────────────────────

TEST_F(CppProjectTest, Update_NewScene_GetsAStateAndAStateIdEntry)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());

    ASSERT_TRUE(UpdateCppProject(Input({ "Main", "Boss Fight" })).IsOk());

    EXPECT_TRUE(fs::exists(Code() / "states" / "BossFightState.cpp"));
    EXPECT_NE(ReadAll(Code() / "StateId.hpp").find("    BossFight,\n"), std::string::npos);
    EXPECT_NE(ReadAll(Code() / "Game.cpp").find("case StateId::BossFight:"), std::string::npos);
}

TEST_F(CppProjectTest, Update_NeverOverwritesAnExistingStateFile)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());
    std::ofstream(Code() / "states" / "MainState.cpp", std::ios::trunc) << "// my gameplay";

    ASSERT_TRUE(UpdateCppProject(Input({ "Main", "Other" })).IsOk());

    EXPECT_EQ(ReadAll(Code() / "states" / "MainState.cpp"), "// my gameplay");
}

TEST_F(CppProjectTest, Update_RemovedScene_KeepsItsStateFileButDropsItFromTheEnum)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main", "Gone" }), "C:/dev/asge").IsOk());

    ASSERT_TRUE(UpdateCppProject(Input({ "Main" })).IsOk());

    EXPECT_TRUE(fs::exists(Code() / "states" / "GoneState.cpp"));
    EXPECT_EQ(ReadAll(Code() / "StateId.hpp").find("Gone"), std::string::npos);
}

TEST_F(CppProjectTest, Update_DoesNotTouchCMakeListsOrMain)
{
    ASSERT_TRUE(CreateCppProject(Input({ "Main" }), "C:/dev/asge").IsOk());
    std::ofstream(Code() / "CMakeLists.txt", std::ios::trunc) << "# edited";

    ASSERT_TRUE(UpdateCppProject(Input({ "Main" })).IsOk());

    EXPECT_EQ(ReadAll(Code() / "CMakeLists.txt"), "# edited");
}

}
