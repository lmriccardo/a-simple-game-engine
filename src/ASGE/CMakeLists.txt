add_library(ASGE STATIC
    # Core components of the Engine
    Application/Application.cpp

    # Event system components of the Engine
    Events/Translation/SDL_TranslationUnit.cpp
    Events/Events.cpp

    # Queryable input state, built on top of the event system
    Input/InputState.cpp
    Input/InputSystem.cpp

    # Video and Graphic portion with Renderers and others
    Video/VideoSystem.cpp
    Video/Graphics/GraphicsFactory.cpp
    Video/Graphics/Rendering/SDL/SDLRenderer.cpp
    Video/Graphics/Rendering/SDL/SDLTexture.cpp
    Video/Graphics/Rendering/SDL/SDLPixelFormat.cpp
    Video/Graphics/Windowing/SDL/SDLWindow.cpp

    # Engine Logger
    Core/Logger/Logger.cpp

    # Timers or timing utils for the engine
    Core/Time/TimeUtils.cpp
    Core/Time/Time.cpp

    # Math utilities implementation
    Core/Math/Geometry/Circle.cpp
    Core/Math/Geometry/Collision.cpp

    # Core graphics utilities (backend-agnostic value types: Color, Image, ...)
    # PixelFormat is header-only now (PixelFormatInfoFor is inline constexpr)
    Core/Graphics/Image.cpp
    Core/Graphics/Font.cpp

    # Concurrency utilities
    Core/Concurrent/Thread.cpp
    Core/Concurrent/Context.cpp
    Core/Concurrent/ThreadPool.cpp

    # Allocators utilities
    Core/Memory/LinearAllocator.cpp

    # Configuration utilities
    Core/Configuration/TOML_Parser.cpp
    Core/Configuration/ConfigurationManager.cpp

    # String utilities
    Core/Strings.cpp

    # Filesystem utilities
    Core/Filesystem/FileIO.cpp
    Core/Filesystem/FileMetadata.cpp
    Core/Filesystem/FileWatcher.cpp
    Core/Filesystem/VirtualFileSystem.cpp

    # Profiling utilities
    Core/Profiling/TimingProfiler.cpp

    # Entity-Component-System
    Core/ECS/Registry.cpp

    # Gameplay systems built on top of the ECS
    Game/Systems/PhysicsSystem.cpp
    Game/Systems/RenderSystem.cpp

    # Asset Manager
    Game/Assets/AssetManager.cpp

    # Scene management
    Game/Scene/SceneSerializer.cpp
    Game/Scene/SceneManager.cpp
)

add_library(ASGE::ASGE ALIAS ASGE)

target_link_libraries(ASGE PUBLIC SDL3::SDL3)
target_link_libraries(ASGE PRIVATE stb::stb)
if(APPLE)
    target_link_libraries(ASGE PRIVATE "-framework CoreServices")
endif()
target_include_directories(ASGE
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

if(ASGE_INSTALL)
    install(
        TARGETS ASGE stb
        EXPORT ASGETargets
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ASGE"
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()