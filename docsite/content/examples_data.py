"""Metadata for the runnable examples (matches examples/ on main -- see examples/CMakeLists.txt)."""

EXAMPLES = [
    dict(
        slug="shapes_demo", title="Shapes", tagline="Every primitive ASGE can draw",
        tags=["Rendering"], kind="window", playable=False,
        description=[
            "A single screen that exercises every primitive on <code>IRenderer</code> — "
            "filled and outlined rectangles, circles, lines and points — including a line "
            "swept through a full rotation to show angle handling.",
            "There's no game logic here: <code>shapes_demo</code> exists purely as a visual "
            "reference for the rendering-primitives phase of the roadmap.",
        ],
        controls=[], source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="moving_box", title="Moving Box", tagline="The smallest playable game",
        tags=["Rendering", "Input"], kind="window", playable=False,
        description=[
            "The example linked from the project README's own quick-start: a single "
            "WASD-controlled box drawn with <code>IRenderer::DrawRect</code>.",
            "It's deliberately minimal — one entity, no ECS, no assets — to show the "
            "shortest path from <code>Application&lt;TGame&gt;</code> to something on "
            "screen that responds to a player.",
        ],
        controls=[("W A S D", "Move the box")],
        source_files=["MovingBox.hpp", "MovingBox.cpp", "MovingBoxGame.cpp"],
    ),
    dict(
        slug="texture_demo", title="Textures", tagline="Loading and drawing images",
        tags=["Rendering", "Assets"], kind="window", playable=False,
        description=[
            "Loads a checker-pattern texture and a frame texture from disk through the "
            "asset pipeline, then draws them with <code>IRenderer::DrawTexture</code> and "
            "<code>DrawTextureAffine</code> — the latter continuously rotated to demonstrate "
            "affine (rotated/scaled) texture drawing rather than plain axis-aligned blits.",
        ],
        controls=[], source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="text_demo", title="Text Rendering", tagline="Fonts, sizes and color",
        tags=["Rendering", "Assets"], kind="window", playable=False,
        description=[
            "Bakes the same font at three different pixel heights — each ASGE "
            "<code>Font</code> is rasterized at one fixed size, so multiple on-screen "
            "sizes means loading it multiple times, each with its own atlas texture.",
            "A headline drawn alongside them continuously cycles through hue space, "
            "exercising per-draw color tinting on top of the baked glyph atlas.",
        ],
        controls=[], source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="animation_demo", title="Sprite Animation", tagline="Spritesheets via a shared frame table",
        tags=["Rendering", "ECS", "Assets"], kind="window", playable=False,
        description=[
            "Showcase for <code>components::Animation</code> and "
            "<code>systems::AnimationSystem</code>/<code>RenderPipeline</code>: playing back "
            "a spritesheet through per-entity frame state, with the frame list itself loaded "
            "as a shared <code>asset::FrameTable</code> (<code>walk.toml</code>) rather than "
            "embedded per-entity.",
        ],
        controls=[
            ("Left Click", "Spawn another animated sprite at the cursor"),
            ("SPACE", "Play/pause every sprite's animation"),
            ("R", "Reset"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="ecs_demo", title="ECS", tagline="Entities, components and systems in motion",
        tags=["ECS", "Rendering"], kind="window", playable=False,
        description=[
            "A player entity and a handful of sprite entities, all built through the ECS "
            "<code>Registry</code> rather than hand-written classes.",
            "Sprite/texture attachment is deferred to the first <code>Render()</code> call, "
            "since a live <code>IRenderer</code> doesn't exist yet when <code>IGame</code> is "
            "constructed — a pattern every sprite-driven example after this one follows.",
        ],
        controls=[("W A S D", "Move the player entity")],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="camera_demo", title="Camera & Viewport", tagline="Panning, zooming, and a picture-in-picture minimap",
        tags=["Rendering"], kind="window", playable=False,
        description=[
            "Showcases <code>IRenderer</code>'s <code>Camera</code>/<code>Viewport</code> pair "
            "directly (no ECS involved): a checkerboard world bigger than the window, panned "
            "and zoomed by a controllable <code>Camera</code>, plus a picture-in-picture "
            "minimap — a second <code>Camera</code>, zoomed to fit the whole world, rendered "
            "through a small <code>Viewport</code> in the corner with a live outline tracking "
            "the main camera's visible area.",
        ],
        controls=[
            ("W A S D", "Pan the camera"),
            ("Scroll", "Zoom in/out"),
            ("R", "Reset the camera"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="camera_follow_demo", title="Camera Follow", tagline="ECS camera-follow with off-screen culling",
        tags=["Rendering", "ECS"], kind="window", playable=False,
        description=[
            "The ECS-driven counterpart to <code>camera_demo</code>: a player entity "
            "(<code>Transform</code> + <code>Velocity</code> + <code>Camera</code>, set as "
            "<code>resources::ActiveCamera</code>) WASD-driven through a 40×30-tile world, "
            "1280×960 — well beyond the 800×600 window.",
            "<code>systems::CameraSystem</code> smoothly follows the player every frame, and "
            "<code>RenderSystem</code> culls tiles outside the visible area "
            "(<code>video::VisibleWorldRect</code>) rather than submitting a draw call for all "
            "1,200 of them. Unlike <code>camera_demo</code>, nothing here ever calls "
            "<code>IRenderer::SetCamera</code> directly — <code>CameraSystem</code> does that "
            "from the active entity's own components, the way a real game would.",
        ],
        controls=[
            ("W A S D", "Move the player"),
            ("↑ / ↓", "Adjust zoom"),
            ("F", "Toggle camera smoothing (instant-snap vs. eased follow)"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="rotation_demo", title="Sprite Rotation", tagline="Transform::m_Rotation, applied for real",
        tags=["Rendering"], kind="window", playable=False,
        description=[
            "Showcases <code>Transform::m_Rotation</code> actually being applied by "
            "<code>RenderSystem</code>: a plain rotating sprite (the whole-texture "
            "<code>DrawTextureAffine</code> path) alongside a rotating <em>animated</em> sprite "
            "(the source-rect <code>DrawTextureAffine</code> overload — rotated and cropped to "
            "its current animation frame at once).",
            "Both entities just accumulate their own angular speed into their own "
            "<code>Transform::m_Rotation</code> each <code>Update()</code> — there's no "
            "engine-side \"spin\" component, it's ordinary per-entity gameplay state living in "
            "the demo itself.",
        ],
        controls=[
            ("LEFT / RIGHT", "Adjust the plain sprite's spin speed (can reverse direction)"),
            ("SPACE", "Pause/resume both sprites"),
            ("R", "Reset"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="input_demo", title="Input System", tagline="Polling InputState end-to-end",
        tags=["Input"], kind="window", playable=False,
        description=[
            "Step 3 of the input-system roadmap: a small <code>InputState</code>/"
            "<code>InputSystem</code> showcase.",
            "Everything here is driven by polling <code>InputState</code> inside "
            "<code>Update()</code> — movement, an edge-triggered toggle, a mouse cursor, "
            "held vs. edge-triggered mouse buttons, and scroll — with "
            "<code>OnSystemEvent</code> left completely empty, proving the polling API is "
            "enough on its own for a real game loop.",
        ],
        controls=[
            ("W A S D", "Move the box"),
            ("SPACE", "Toggle light/dark background (edge-triggered)"),
            ("Left Click (hold)", "Shown as held while down"),
            ("Right Click", "Drop a mark at the cursor (edge-triggered)"),
            ("Scroll", "Shown live"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="scene_demo", title="Scene Management", tagline="A whole level loaded from TOML",
        tags=["Scenes", "ECS", "Assets"], kind="window", playable=False,
        description=[
            "Loads its whole entity set from a hand-authored TOML scene file "
            "(<code>assets/scene.toml</code>) via <code>SceneManager::LoadScene</code> "
            "instead of spawning entities in code — see <code>ecs_demo</code> for the "
            "code-driven equivalent this replaces.",
            "Ties together every ASGE subsystem at its current state: the virtual "
            "filesystem loads the TOML, the asset pipeline resolves textures, the ECS "
            "registry holds the resulting entities, and <code>SceneManager</code> can swap "
            "the whole active scene at runtime or snapshot it back out to disk.",
        ],
        controls=[
            ("W A S D", "Move the player entity"),
            ("P", "Save a snapshot of the current scene back to TOML (edge-triggered)"),
            ("L", "Swap to the other scene file, alternating back and forth"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="physics_demo", title="Physics", tagline="Gravity, collision response and triggers",
        tags=["Physics", "ECS"], kind="window", playable=False,
        description=[
            "Showcase for <code>Rigidbody</code>, gravity, mass-weighted collision "
            "response, and Trigger colliders, all driven through "
            "<code>systems::PhysicsUpdate</code>.",
            "A floor and two side walls are static Solid Colliders — deliberately no "
            "<code>Velocity</code>/<code>Rigidbody</code>, so collision response treats "
            "them as immovable. A yellow-outlined zone acts as a Trigger, reporting overlap "
            "without any physical push-back.",
        ],
        controls=[("Click", "Drop a new box at the cursor"), ("R", "Reset the scene")],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="path_following_demo", title="Path Following", tagline="A car driving a closed, curved street",
        tags=["Physics", "ECS", "Assets", "Rendering"], kind="window", playable=False,
        description=[
            "End-to-end showcase for <code>components::PathFollow</code> and "
            "<code>systems::PathFollowingSystem</code>: a car drives a closed, curved street "
            "loop built from <code>math::CatmullRomSpline</code> — 13 waypoints (the first "
            "repeated at the end, since a Catmull-Rom chain doesn't wrap on its own) resolved "
            "once at spawn time, then walked every frame at a player-adjustable speed, with the "
            "car oriented to face the road's tangent direction as it turns.",
            "The street itself is drawn straight from the car's own resolved "
            "<code>PathFollow::m_Path</code> — <code>IRenderer</code> has no curve/polygon-fill "
            "primitive, so it's painted as a dense trail of overlapping filled circles (via "
            "<code>PointAtDistance</code>) with a dashed centerline, rather than needing a "
            "second, separately-authored road asset.",
        ],
        controls=[
            ("UP / DOWN", "Adjust speed while held"),
            ("SPACE", "Pause/resume"),
            ("R", "Reset to the start of the loop"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="audio_demo", title="Audio", tagline="One-shot and looping playback",
        tags=["Audio"], kind="window", playable=False,
        description=[
            "Demonstrates <code>AudioSource</code>/<code>AudioSystem</code>: replaying a "
            "short one-shot \"blip\" immediately even while it's still sounding, toggling a "
            "looping ambient hum, adjusting the audio device's master gain, and detaching a "
            "source's stream back to the shared pool.",
        ],
        controls=[
            ("SPACE", "Replay the one-shot blip"),
            ("L", "Toggle the looping ambient hum"),
            ("↑ / ↓", "Master volume"),
            ("D", "Detach the blip's stream back to the pool"),
        ],
        source_files=["Game.hpp", "Game.cpp"],
    ),
    dict(
        slug="background_changer", title="Background Changer", tagline="About the smallest ASGE program",
        tags=["Rendering", "Getting Started"], kind="window", playable=False,
        description=[
            "About as small as an ASGE program gets: one component holding an RGBA color, "
            "cleared to the screen every frame, randomized on a keypress.",
            "A good first file to read before anything involving ECS or the asset pipeline.",
        ],
        controls=[("SPACE", "Randomize the background color")],
        source_files=["Game.hpp", "Game.cpp", "Component.hpp"],
    ),
    dict(
        slug="configuration_reader", title="Configuration Reader",
        tagline="Reading typed values out of a TOML config",
        tags=["Configuration"], kind="console", playable=False,
        description=[
            "Opens a TOML configuration file through <code>ConfigurationManager</code> and "
            "reads back typed settings (<code>Get&lt;int&gt;</code>, etc.) with explicit "
            "error handling on missing or mistyped keys, printing each to stdout.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
    dict(
        slug="configuration_writer", title="Configuration Writer",
        tagline="Building a TOML document from code",
        tags=["Configuration"], kind="console", playable=False,
        description=[
            "The write-side counterpart to <code>configuration_reader</code>: assembles a "
            "TOML document in memory via <code>toml::TOML_Builder</code>, then reads it "
            "straight back through <code>ConfigurationManager</code> to confirm the "
            "round-trip.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
    dict(
        slug="file_watching", title="File Watching", tagline="Filesystem change notifications",
        tags=["Filesystem"], kind="console", playable=False,
        description=[
            "Starts a <code>FileWatcher</code> on a directory and prints every "
            "<code>FileEvent</code> (created/modified/removed) it receives, cancelling "
            "itself on the first event — a minimal example of ASGE's cross-platform "
            "filesystem-watching API.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
    dict(
        slug="image_info", title="Image Info", tagline="Loading an image and inspecting its pixels",
        tags=["Assets", "Media"], kind="console", playable=False,
        description=[
            "Loads a bitmap through <code>Image::Load</code> and prints its dimensions and "
            "raw pixel data — the smallest possible use of the image-loading half of the "
            "asset pipeline, with no rendering involved at all.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
    dict(
        slug="scoped_profiler", title="Scoped Profiler", tagline="Nested scope timing",
        tags=["Profiling", "Tooling"], kind="console", playable=False,
        description=[
            "Times nested code paths with <code>PROFILE_SCOPE</code>, showing that scopes "
            "can nest inside each other the same way a real per-frame <code>Update()</code> "
            "would, then prints the resulting timing tree.",
        ],
        controls=[], source_files=["main.cpp"],
    ),
]

EXAMPLES_BY_SLUG = {e["slug"]: e for e in EXAMPLES}
