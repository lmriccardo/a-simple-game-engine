# Simple C++ Game Engine Starter Checklist

---

## 1. Platform Layer
Build the lowest-level code that talks to the operating system.

- [ ] Create an application entry point
- [ ] Open a window
- [ ] Create a main loop
- [ ] Handle window close event
- [ ] Track delta time per frame
- [ ] Add basic logging to console
- [ ] Add error reporting helpers
- [ ] Add assertions for debugging

### Nice to have
- [ ] Command line argument handling
- [ ] High-resolution timer
- [ ] Fullscreen/windowed toggle
- [ ] Window resize handling

---

## 2. Core Engine Systems
These are the basic systems everything else depends on.

- [ ] `Application` class
- [ ] `Engine` or `Game` class
- [ ] `Layer` or `State` system
- [ ] `Time` system
- [ ] `Logger` system
- [ ] Configuration/settings system
- [ ] Basic memory ownership rules
  - [ ] Decide where to use stack objects
  - [ ] Decide where to use smart pointers
- [ ] Utility helpers
  - [ ] string helpers
  - [ ] file helpers
  - [ ] math helpers

### Suggested classes
- [ ] `Application`
- [ ] `Window`
- [ ] `Timer`
- [ ] `Log`
- [ ] `Input`
- [ ] `FileSystem`

---

## 3. Input System
You need a way to receive player actions.

- [ ] Keyboard input
- [ ] Mouse position
- [ ] Mouse buttons
- [ ] Mouse wheel
- [ ] Input polling each frame
- [ ] Key pressed / released detection
- [ ] Mouse pressed / released detection
- [ ] Abstract input API
  - [ ] `IsKeyDown()`
  - [ ] `IsMouseButtonDown()`
  - [ ] `GetMousePosition()`

### Nice to have
- [ ] Input mapping system
  - [ ] map "Jump" to Space
  - [ ] map "MoveLeft" to A
- [ ] Gamepad support later

---

## 4. Math Library
A small math layer is essential.

- [ ] `Vec2`
- [ ] `Vec3`
- [ ] `Vec4`
- [ ] `Mat4`
- [ ] Basic operations
  - [ ] add
  - [ ] subtract
  - [ ] multiply
  - [ ] scalar operations
- [ ] Dot product
- [ ] Length / normalize
- [ ] Transform matrices
  - [ ] translation
  - [ ] rotation
  - [ ] scale
- [ ] Orthographic projection
- [ ] Perspective projection

### Nice to have
- [ ] Quaternion support later
- [ ] Geometry helpers
  - [ ] rectangles
  - [ ] circles
  - [ ] AABB

---

## 5. Rendering System
Start simple. For a first engine, 2D rendering is enough.

- [ ] Pick a graphics API or library
  - [ ] SDL2 renderer
  - [ ] SFML
  - [ ] OpenGL
- [ ] Clear screen each frame
- [ ] Draw a colored triangle or rectangle
- [ ] Render loop integrated into main loop
- [ ] Basic renderer abstraction
- [ ] Camera system
- [ ] Transform system
- [ ] Draw sprites or textured quads
- [ ] Load textures from files
- [ ] Render text to screen

### Minimum rendering milestones
- [ ] Open window
- [ ] Clear background color
- [ ] Draw one shape
- [ ] Move shape on screen
- [ ] Draw texture
- [ ] Draw multiple objects

### Nice to have
- [ ] Sprite batching
- [ ] Shader abstraction
- [ ] Material system
- [ ] Render commands queue

---

## 6. Resource / Asset Management
You will need a safe way to load files.

- [ ] File loading utility
- [ ] Texture loader
- [ ] Shader loader
- [ ] Font loader
- [ ] Asset manager class
- [ ] Avoid loading same asset twice
- [ ] Add asset path conventions
- [ ] Add missing-file fallback behavior

### Suggested classes
- [ ] `AssetManager`
- [ ] `Texture`
- [ ] `Shader`
- [ ] `Font`

---

## 7. Entity / Object System
At first, keep it simple.

- [ ] Create a `GameObject` or `Entity` class
- [ ] Add transform data
  - [ ] position
  - [ ] rotation
  - [ ] scale
- [ ] Add name / ID
- [ ] Add update function
- [ ] Add render function
- [ ] Add parent-child relationship later if needed

### Start simple with
- [ ] `Entity`
- [ ] `TransformComponent`
- [ ] `SpriteComponent`

### Later
- [ ] ECS (Entity Component System)
- [ ] Component registration
- [ ] Systems processing entities

---

## 8. Scene Management
You need to organize what is currently loaded.

- [ ] Create a `Scene` class
- [ ] Store a list of entities
- [ ] Update all entities
- [ ] Render all entities
- [ ] Add scene loading/unloading
- [ ] Add scene switching
  - [ ] menu scene
  - [ ] gameplay scene
  - [ ] pause scene

### Nice to have
- [ ] Scene serialization later
- [ ] Prefab system later

---

## 9. Game Loop
This is one of the most important engine parts.

- [ ] Process input
- [ ] Update game logic
- [ ] Render frame
- [ ] Repeat until exit
- [ ] Use delta time
- [ ] Add fixed timestep option for physics later

### Basic loop order
- [ ] Poll events
- [ ] Update input state
- [ ] Update game objects
- [ ] Render scene
- [ ] Present frame

### Nice to have
- [ ] FPS counter
- [ ] Frame limiting
- [ ] VSync toggle

---

## 10. Audio System
Not required on day one, but useful early.

- [ ] Load sound effect
- [ ] Play sound effect
- [ ] Load music
- [ ] Play/stop/pause music
- [ ] Volume control
- [ ] Simple audio manager

### Nice to have
- [ ] Separate music and SFX channels
- [ ] Looping audio
- [ ] 2D positional audio later

---

## 11. Collision and Physics
Keep this basic at first.

- [ ] AABB collision detection
- [ ] Circle collision detection
- [ ] Collision response for simple objects
- [ ] Velocity
- [ ] Gravity
- [ ] Basic movement integration

### Build in order
- [ ] Position
- [ ] Velocity
- [ ] Move each frame using delta time
- [ ] Detect overlap
- [ ] Stop or separate objects on collision

### Nice to have
- [ ] Tile collisions
- [ ] Physics layers
- [ ] Rigid body system later

---

## 12. UI / Debug Tools
This will help a lot while building.

- [ ] Debug text on screen
- [ ] FPS display
- [ ] Toggle debug mode
- [ ] Show object positions
- [ ] Draw collision boxes
- [ ] Console log categories
- [ ] Error messages with file/line

### Nice to have
- [ ] Dear ImGui integration
- [ ] Runtime inspector
- [ ] Asset browser
- [ ] Scene hierarchy

---

## 13. Saving / Loading
Useful for settings and game state.

- [ ] Save settings to file
- [ ] Load settings at startup
- [ ] Save simple game data
- [ ] Choose format
  - [ ] plain text
  - [ ] JSON
  - [ ] binary later

### Nice to have
- [ ] Level save/load
- [ ] Player progress save system

---

## 14. Scripting / Data-Driven Content
Do this later, not at the beginning.

- [ ] Define game objects from data files
- [ ] Load levels from text/JSON
- [ ] Add simple scripting later
  - [ ] Lua
  - [ ] custom script callbacks
- [ ] Expose engine functions to scripts later

---

## 15. Editor Tools
Only after your engine can already run a game.

- [ ] Level editor
- [ ] Drag-and-drop entity placement
- [ ] Inspector panel
- [ ] Scene hierarchy
- [ ] Asset browser
- [ ] Save edited scenes

---

# Suggested Build Order

## Phase 1: Bare Minimum Engine
- [ ] Create project
- [ ] Open window
- [ ] Main loop
- [ ] Logging
- [ ] Input handling
- [ ] Draw something simple

## Phase 2: Make It a Tiny 2D Engine
- [ ] Texture loading
- [ ] Sprite rendering
- [ ] Camera
- [ ] Basic scene system
- [ ] Game objects with transforms

## Phase 3: Make It Usable for a Small Game
- [ ] Audio
- [ ] Collision
- [ ] Simple physics
- [ ] Text rendering
- [ ] Asset manager

## Phase 4: Improve Architecture
- [ ] Better scene management
- [ ] Components
- [ ] Serialization
- [ ] Debug tools
- [ ] Performance cleanup

## Phase 5: Advanced Extras
- [ ] ECS
- [ ] Editor
- [ ] Scripting
- [ ] Better renderer
- [ ] Animation system
- [ ] Particles

---

# First Tiny Goal
Your first success target should be:

- [ ] Launch engine
- [ ] Open a window
- [ ] Show a colored background
- [ ] Draw a movable rectangle
- [ ] Control it with keyboard
- [ ] Display FPS

If you can do that, you already have the beginning of a real engine.

---

# First Practical Classes to Implement
- [ ] `Application`
- [ ] `Window`
- [ ] `Renderer`
- [ ] `Input`
- [ ] `Timer`
- [ ] `Entity`
- [ ] `Scene`
- [ ] `Texture`
- [ ] `AssetManager`
- [ ] `Camera2D`

---

# Suggested Folder Structure
- [ ] `src/core`
- [ ] `src/platform`
- [ ] `src/renderer`
- [ ] `src/input`
- [ ] `src/math`
- [ ] `src/scene`
- [ ] `src/audio`
- [ ] `src/physics`
- [ ] `src/assets`
- [ ] `src/debug`
- [ ] `assets/textures`
- [ ] `assets/shaders`
- [ ] `assets/fonts`
- [ ] `assets/audio`

---

# Good Beginner Rule
- [ ] Build the simplest version first
- [ ] Avoid making everything generic too early
- [ ] Make it work before making it elegant
- [ ] Test each subsystem independently
- [ ] Finish small milestones
- [ ] Build one tiny game with the engine as soon as possible

---

# Optional Milestone Checklist

## Milestone 1: Window + Loop
- [ ] Window appears
- [ ] App closes correctly
- [ ] Delta time works
- [ ] Console logging works

## Milestone 2: Input + Rendering
- [ ] Keyboard input works
- [ ] Mouse input works
- [ ] Rectangle renders
- [ ] Rectangle moves

## Milestone 3: Basic Game Objects
- [ ] Entity class works
- [ ] Transform works
- [ ] Scene updates entities
- [ ] Scene renders entities

## Milestone 4: Assets
- [ ] Texture loads from file
- [ ] Sprite displays correctly
- [ ] Missing files handled safely

## Milestone 5: Basic Game Features
- [ ] Collision works
- [ ] Sound plays
- [ ] Text displays
- [ ] FPS shown onscreen

## Milestone 6: Engine Usability
- [ ] Config file works
- [ ] Debug drawing works
- [ ] Scene switching works
- [ ] Basic save/load works