#pragma once

/**
 * @mainpage ASGE API Reference
 *
 * ASGE ("A Simple Game Engine") is a modular, lightweight, cross-platform 2D
 * game engine written in C++23 on top of SDL3. This reference is generated
 * from the doc comments on every public and internal declaration under
 * `src/ASGE/` — use the sidebar to browse by namespace or class.
 *
 * For an overview, installation instructions, and runnable examples with
 * screenshots and source, see the rest of the
 * <a href="../index.html">documentation site</a>.
 *
 * Pull in the whole engine through this umbrella header:
 * @code
 * #include <ASGE/ASGE.hpp>
 * @endcode
 */

// General Application
#include <ASGE/Application/Application.hpp>
#include <ASGE/Application/ApplicationConfig.hpp>

// ASGE System Events
#include <ASGE/Events/Enums.hpp>
#include <ASGE/Events/Events.hpp>
#include <ASGE/Input/Keycode.hpp>
#include <ASGE/Input/MouseButton.hpp>
#include <ASGE/Input/InputState.hpp>
#include <ASGE/Input/InputSystem.hpp>

// ASGE Game
#include <ASGE/Game/Game.hpp>
#include <ASGE/Game/Components.hpp>
#include <ASGE/Game/Systems.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Events.hpp>
#include <ASGE/Game/Resources/ActiveCamera.hpp>

// ASGE Core ECS functionalities
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/ECS/Entity.hpp>

// ASGE Core Graphics
#include <ASGE/Core/Media/Color.hpp>
#include <ASGE/Core/Media/Font.hpp>
#include <ASGE/Core/Media/Image.hpp>

// ASGE Core Logger
#include <ASGE/Core/Logger/Logger.hpp>

// ASGE Core Timing and Profiling
#include <ASGE/Core/Time/Time.hpp>
#include <ASGE/Core/Profiling/ScopedTimer.hpp>
#include <ASGE/Core/Profiling/TimingProfiler.hpp>

// ASGE Core Math
#include <ASGE/Core/Math/Math.hpp>

// ASGE Core Patterns
#include <ASGE/Core/Patterns/Signal.hpp>

// ASGE Core Concurrent
#include <ASGE/Core/Concurrent/Thread.hpp>
#include <ASGE/Core/Concurrent/Context.hpp>
#include <ASGE/Core/Concurrent/ThreadPool.hpp>

// ASGE Core Memory Management
#include <ASGE/Core/Memory/LinearAllocator.hpp>
#include <ASGE/Core/Memory/PoolAllocator.hpp>
#include <ASGE/Core/Memory/StackAllocator.hpp>

// ASGE Core Filesystem
#include <ASGE/Core/Filesystem/Filesystem.hpp>

// ASGE Video 
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Video/Graphics/Window.hpp>
#include <ASGE/Video/GraphicsBackend.hpp>