add_library(ASGE STATIC
    # Core components of the Engine
    Core/Application.cpp

    # Event system components of the Engine
    Events/Translation/SDL_TranslationUnit.cpp
    Events/Events.cpp

    # Video and Graphic portion with Renderers and others
    Video/VideoSystem.cpp
    Video/Graphics/SDLRenderer.cpp

    # Engine Logger
    Logger/Logger.cpp

    # Timers or timing utils for the engine
    Time/TimeUtils.cpp

    # Math utilities implementation
)

target_link_libraries(ASGE PUBLIC SDL3::SDL3)
target_include_directories(ASGE 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}/..")