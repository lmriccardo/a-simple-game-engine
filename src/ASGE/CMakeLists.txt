add_library(ASGE STATIC
    # Event system components of the Engine
    Events/Translation/SDL_TranslationUnit.cpp
    Events/Events.cpp

    # Queryable input state, built on top of the event system
    Input/InputState.cpp
    Input/InputSystem.cpp

    # Video and Graphic portion with Renderers and others
    Video/VideoSystem.cpp
    Video/Graphics/GraphicsFactory.cpp
    Video/Graphics/Camera.cpp
    Video/Graphics/Rendering/SDL/SDLRenderer.cpp
    Video/Graphics/Rendering/SDL/SDLTexture.cpp
    Video/Graphics/Rendering/SDL/SDLPixelFormat.cpp
    Video/Graphics/Windowing/SDL/SDLWindow.cpp

    # Audio Device initialization
    Audio/AudioStream.cpp
    Audio/AudioDevice.cpp

    # Engine Logger
    Core/Logger/Logger.cpp

    # Timers or timing utils for the engine
    Core/Time/TimeUtils.cpp
    Core/Time/Time.cpp

    # Math utilities implementation
    Core/Math/Geometry/Circle.cpp
    Core/Math/Geometry/Collision.cpp
    Core/Math/Geometry/CatmullRomSpline.cpp

    # Core graphics utilities (backend-agnostic value types: Color, Image, ...)
    # PixelFormat is header-only now (PixelFormatInfoFor is inline constexpr)
    Core/Media/Image.cpp
    Core/Media/Font.cpp
    Core/Media/AudioClip.cpp

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

    # ECS components and their TOML (de)serialization
    Game/Components/Serialize.cpp
    Game/Components/Transform.cpp
    Game/Components/Velocity.cpp
    Game/Components/Rigidbody.cpp
    Game/Components/Collider.cpp
    Game/Components/Sprite.cpp
    Game/Components/Animation.cpp
    Game/Components/AudioSource.cpp
    Game/Components/Camera.cpp
    Game/Components/PathFollow.cpp

    # Gameplay systems built on top of the ECS
    Game/Systems/PhysicsSystem.cpp
    Game/Systems/PhysicsDebugSystem.cpp
    Game/Systems/RenderSystem.cpp
    Game/Systems/AudioSystem.cpp

    # Asset Manager
    Game/Assets/AssetManager.cpp
    Game/Assets/AssetResolver.cpp
    Game/Assets/FrameTable.cpp

    # Scene management
    Game/Scene/SceneSerializer.cpp
    Game/Scene/SceneManager.cpp
    Game/Scene/IdContext.cpp

    # Game Events
    Game/Events.cpp
)

add_library(ASGE::ASGE ALIAS ASGE)

target_link_libraries(ASGE PUBLIC SDL3::SDL3)
target_link_libraries(ASGE PRIVATE stb::stb)
target_link_libraries(ASGE PRIVATE stb::stb_vorbis)

if(APPLE)
    target_link_libraries(ASGE PRIVATE "-framework CoreServices")
endif()
target_include_directories(ASGE
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

if(ASGE_INSTALL)
    install(
        TARGETS ASGE stb stb_vorbis
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