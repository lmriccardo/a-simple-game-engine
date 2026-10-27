"""Theory pages for the remaining subsystems: concept first, then how ASGE does it.

Type and function names are checked against src/ASGE. Each entry of PAGES is
(slug, title, summary, body-html); `src()` links a header on GitHub.
"""
import html

from _shared import GITHUB_URL, code_card

SRC = f"{GITHUB_URL}/blob/main/src/ASGE"


def src(path: str) -> str:
    return f'<a href="{SRC}/{path}" target="_blank" rel="noopener"><code>{html.escape(path)}</code></a>'


INPUT = """
<h1>Input</h1>
<p class="lede">Input turns raw key and mouse events into answers to the questions gameplay code actually asks:
is it held, did it just go down, how far did the mouse move.</p>

<h2>The concept</h2>
<p>The OS reports <em>events</em> ("key down", "key up"); a game wants <em>state</em>. Engines keep two snapshots,
the current and the previous frame, and derive three queries from them: <strong>down</strong> (held now),
<strong>pressed</strong> (up last frame, down now) and <strong>released</strong> (down last frame, up now). Edge
queries such as "pressed" are what make a jump fire once instead of every frame the key is held.</p>

<h2>How ASGE does it</h2>
<p><code>asge::input::InputSystem</code> is owned by <code>Application</code> and mirrors <code>VideoSystem</code>'s shape.
Every frame the loop calls <code>NewFrame()</code> to roll the current snapshot into the previous one, then feeds each
polled <code>SystemEvent</code> through <code>ProcessEvent()</code> (see <a href="events-signals.html">Events &amp; signals</a>).
It is not a second event source: <code>InputState</code> simply consumes the same stream that also reaches
<code>Game::OnSystemEvent</code>.</p>
<p>Games never touch the system. Each state's <code>Update</code> receives a read-only
<code>InputState const&amp;</code>:</p>
<ul>
  <li><code>IsKeyDown</code> / <code>IsKeyPressed</code> / <code>IsKeyReleased</code> with a <code>Keycode</code></li>
  <li><code>IsMouseButtonDown</code> / <code>IsMouseButtonPressed</code> / <code>IsMouseButtonReleased</code> with a <code>MouseButton</code> (<code>LEFT</code>, <code>MIDDLE</code>, <code>RIGHT</code>, <code>X1</code>, <code>X2</code>)</li>
  <li><code>GetMousePosition()</code> (window-relative), <code>GetMouseDelta()</code> and <code>GetScrollDelta()</code>, all <code>Float2</code>; the deltas accumulate since the last <code>NewFrame()</code></li>
</ul>
""" + code_card("state.cpp", "cpp", """void Update( float inDeltaTime, asge::input::InputState const& inInput ) override
{
    using asge::input::Keycode;
    using asge::input::MouseButton;

    if ( inInput.IsKeyPressed( Keycode::A ) ) { /* fires on the frame A went down */ }
    if ( inInput.IsKeyDown( Keycode::A ) )    { /* fires every frame A is held */ }

    if ( inInput.IsMouseButtonPressed( MouseButton::LEFT ) )
    {
        auto where = inInput.GetMousePosition();   // asge::math::Float2
    }
}""") + """
<h2>See also</h2>
<p>Source: """ + src("Input/InputSystem.hpp") + ", " + src("Input/InputState.hpp") + """.
<a href="../examples/input_demo.html">Input demo</a> &middot; <a href="game-loop.html">Game loop &amp; timing</a></p>
"""

RENDERING = """
<h1>Rendering</h1>
<p class="lede">Rendering gets pixels onto a window. ASGE keeps this layer thin: an abstract renderer, a window and
a texture, with SDL3 as the only backend today.</p>

<h2>The concept</h2>
<p>A 2D renderer exposes a small drawing vocabulary (clear, shapes, textures, text) and a way to <em>present</em>
the finished frame. Hiding it behind an interface keeps game code independent of the graphics API, so a different
backend could be added later without touching gameplay. Cameras convert <em>world space</em> (where things are
in the level) into <em>screen space</em> (pixels in the window).</p>

<h2>How ASGE does it</h2>
<h3>Video system and backends</h3>
<p><code>asge::video::VideoSystem</code> brings up a window and a renderer for a chosen <code>GraphicsBackend</code>
through a factory and hands them out as <code>IWindow&amp;</code> and <code>IRenderer&amp;</code>. Only
<code>GraphicsBackend::SDL</code> exists; OpenGL, Vulkan and DirectX are future possibilities, which is why
everything above this layer talks to the interfaces.</p>

<h3>IRenderer</h3>
<p>The drawing calls are all <code>const</code> methods on <code>IRenderer</code>:</p>
<ul>
  <li><code>Clear</code>, <code>DrawRect</code>, <code>DrawLine</code>, <code>DrawCircle</code> for primitives, then <code>Present()</code> to show the frame.</li>
  <li><code>DrawTexture</code> in several forms: scaled into a rect, at native size, a source sub-rect, plus <code>DrawTexture9Grid</code> (nine-slice scaling), <code>DrawTextureTiled</code> and <code>DrawTextureAffine</code> for arbitrary transforms.</li>
  <li><code>DrawString</code> renders text from a baked <code>media::Font</code> atlas (a fixed ASCII range).</li>
  <li><code>CreateTexture(media::Image const&amp;)</code> uploads pixels and returns a <code>std::unique_ptr&lt;ITexture&gt;</code>.</li>
</ul>
<p>An <code>ITexture</code> reports its <code>Size()</code> and supports a colour modulation (<code>SetColorMod</code>).</p>

<h3>Camera</h3>
<p><code>video::Camera</code> is a 2D world position plus a uniform <code>m_Zoom</code>. <code>IRenderer::SetCamera</code>
makes every following draw call go through it, and helpers such as <code>TransformRect</code> and
<code>VisibleWorldRect</code> convert between world and screen space. The ECS-side rendering (sprites, layers, Y-sort)
is the render pipeline that <code>Game::Render</code> runs each frame.</p>
""" + code_card("render.cpp", "cpp", """// image: an asge::media::Image decoded earlier; color: an asge::graphics::RGBA_Color
auto texture = inRenderer.CreateTexture( image );          // std::unique_ptr<ITexture>

inRenderer.Clear( color );
inRenderer.DrawTexture( *texture, asge::math::Float2{ 32.0f, 32.0f } );   // native size at (32, 32)
inRenderer.Present();""") + """
<h2>See also</h2>
<p>Source: """ + src("Video/VideoSystem.hpp") + ", " + src("Video/Graphics/Renderer.hpp") + ", " + src("Video/Graphics/Camera.hpp") + """.
<a href="../examples/shapes_demo.html">Shapes demo</a> &middot; <a href="../examples/texture_demo.html">Texture demo</a> &middot;
<a href="../examples/camera_demo.html">Camera demo</a> &middot; <a href="math.html">Math</a></p>
"""

AUDIO = """
<h1>Audio</h1>
<p class="lede">Audio plays sound clips on the machine's output device, several at once, driven by components
on entities.</p>

<h2>The concept</h2>
<p>A game needs many sounds playing concurrently, each with its own volume and looping, without allocating while
the game runs. Engines open one output device, keep a bounded pool of <em>streams</em> bound to it, and feed decoded
clip data into those streams.</p>

<h2>How ASGE does it</h2>
<h3>AudioDevice and AudioStream</h3>
<p><code>asge::audio::AudioDevice</code> owns SDL's audio subsystem, one physical playback device and a fixed pool
of <strong>32</strong> <code>AudioStream</code>s. <code>Initialize()</code> opens the default device;
<code>CreateStream(AudioClip&amp;)</code> returns a <code>std::shared_ptr&lt;AudioStream&gt;</code> matching the clip's
format, and <code>DetachStream()</code> gives the slot back. <code>SetGain()</code> sets the device volume. An
<code>AudioStream</code> wraps one <code>SDL_AudioStream</code>: <code>PutData()</code> queues a clip,
<code>ClearData()</code> empties it, <code>SetAudioGain()</code> sets its own volume. The shared pointer stays valid
even if another detach relocates its slot.</p>
<p><code>Application</code> initializes the device alongside the video system and passes it into your game.</p>

<h3>AudioSource and AudioSystem</h3>
<p>Gameplay does not touch streams. An entity gets an <code>AudioSource</code> component (clip path, volume, loop),
and you use <code>PlayAudioSource</code>, <code>StopAudioSource</code> and <code>DetachAudioSource</code> to change
its state. <code>AudioSystem</code>, the last system in <code>Game::Update</code>'s fixed order (see
<a href="game-loop.html">Game loop &amp; timing</a>), starts newly playing sources, restarts looping ones whose queued audio ran out, stops finished
non-looping ones, and clears the data of stopped ones. A source keeps the same stream for its whole lifetime, so replaying
it never creates a new one. Clips are loaded through the
<a href="assets.html">asset manager</a> as <code>media::AudioClip</code>.</p>
""" + code_card("audio.cpp", "cpp", """using namespace asge::game::components;

auto source = registry.GetComponent<AudioSource>( entity );   // Result<reference_wrapper<AudioSource>>
if ( source )
{
    PlayAudioSource( source.Value().get(), /* inLoop */ true );   // AudioSystem picks it up this frame
    // StopAudioSource( ... ) stops it; DetachAudioSource( device, ... ) frees its pool slot
}""") + """
<h2>See also</h2>
<p>Source: """ + src("Audio/AudioDevice.hpp") + ", " + src("Game/Components/AudioSource.hpp") + ", " + src("Game/Systems/AudioSystem.hpp") + """.
<a href="../examples/audio_demo.html">Audio demo</a></p>
"""

FILESYSTEM = """
<h1>Filesystem &amp; VFS</h1>
<p class="lede">Assets are referred to by <em>virtual</em> paths, so a game's data does not depend on where it is
installed.</p>

<h2>The concept</h2>
<p>Hard-coding <code>C:\\Users\\me\\game\\hero.png</code> breaks the moment the game moves. A virtual file system maps
short, stable names (<code>textures/hero.png</code>) onto real directories chosen at startup. The same game data then
works from a dev checkout, an installed build or an archive, and tools can remap folders without touching scenes.</p>

<h2>How ASGE does it</h2>
<h3>VirtualFileSystem</h3>
<p><code>asge::filesystem::VirtualFileSystem</code> keeps a list of <em>mounts</em>, each binding a virtual root to a
real directory:</p>
<ul>
  <li><code>Mount(root, realPath)</code> and <code>Unmount()</code> manage bindings; a root can be mounted more than once.</li>
  <li><code>Resolve(virtualPath)</code> returns the real <code>Path</code>, trying each mount whose root prefixes the path in registration order.</li>
  <li><code>Exists()</code>, <code>IsMounted()</code>, <code>ListMounts()</code> and <code>SplitRoot()</code> inspect state.</li>
  <li><code>ToVirtualPath(realPath)</code> goes the other way, which the editor uses when you pick a file on disk.</li>
</ul>
<p>Paths are normalized before use, and fallible calls return <code>Result</code>/<code>BoolResult</code> instead of
throwing.</p>
""" + code_card("vfs.cpp", "cpp", """asge::filesystem::VirtualFileSystem vfs;
vfs.Mount( "textures", "assets/textures" );          // BoolResult

auto real = vfs.Resolve( "textures/hero.png" );      // Result<Path>: assets/textures/hero.png
bool ok   = vfs.Exists( "textures/hero.png" );""") + """
<h3>File IO and watching</h3>
<p>The <code>filesystem</code> namespace also provides <code>ReadText</code>, <code>WriteText</code>,
<code>ReadBinary</code> and <code>Copy</code>, plus file metadata. <code>FileWatcher</code> runs on its own
<code>concurrent::Thread</code> and reports <code>FileEvent</code>s (created, deleted, modified, moved) to an
<code>IFileEventHandler</code>, with a native implementation per platform.</p>

<h2>See also</h2>
<p>Source: """ + src("Core/Filesystem/VirtualFileSystem.hpp") + ", " + src("Core/Filesystem/FileWatcher.hpp") + ", " + src("Core/Filesystem/FileIO.hpp") + """.
<a href="../examples/file_watching.html">File watching example</a> &middot; <a href="assets.html">Assets</a></p>
"""

ASSETS = """
<h1>Assets</h1>
<p class="lede">Loading an image or a sound is slow; loading it twice is wasteful. An asset layer loads each file once
and shares it.</p>

<h2>The concept</h2>
<p>Assets (textures, fonts, audio, animation data) are loaded by <em>name</em> and cached, so the second request for
the same asset is a lookup. Components then hold a cheap reference or a path instead of a copy, and a scene file can
store just the path.</p>

<h2>How ASGE does it</h2>
<h3>AssetManager and AssetPool</h3>
<p><code>asset::AssetManager</code> is the one entry point. It is built over a
<a href="filesystem.html">VirtualFileSystem</a> and owns a path-keyed <code>AssetPool</code> per asset type, each a cache of
<code>std::shared_ptr&lt;Asset&lt;T&gt;&gt;</code>:</p>
<ul>
  <li><code>GetImage</code>, <code>GetAudio</code>, <code>GetFrameTable</code> by virtual path; <code>GetFont</code> by path <em>and</em> pixel height, since a font is baked at a size.</li>
  <li><code>GetTexture</code> uploads an image to the GPU once, caches the <code>ITexture</code> by virtual path and keeps ownership; <code>UnloadTexture</code> drops it.</li>
  <li>Loads fail with a <code>Result</code> error when the path does not resolve through the VFS or the file cannot be decoded.</li>
</ul>
""" + code_card("assets.cpp", "cpp", """asge::game::asset::AssetManager assets( vfs );

auto image = assets.GetImage( "textures/hero.png" );   // Result<shared_ptr<Asset<Image>>>
auto again = assets.GetImage( "textures/hero.png" );   // cached: same asset, no second decode""") + """
<h3>Resolving components</h3>
<p>A <code>Sprite</code> stores its <code>m_VirtualPath</code> and <code>m_SourceRect</code>; those are what round-trip
through a scene file. Each frame <code>ResolveAssets</code> (called from <code>Game::Render</code>) turns the paths of
sprites and animations back into live textures. A sprite's texture therefore points into the manager's cache and must
not outlive it.</p>

<h3>Spritesheets and animation clips</h3>
<p>A <code>FrameTable</code> is a named sequence of source rects into a spritesheet, loaded from a small TOML file.
A <code>GridSpec</code> describes a contiguous range of cells over a regular grid (row-major or column-major), and
<code>MakeGridFrames</code> does the grid math. An optional <code>frame_durations</code> array gives frames their own
display time.</p>

<h2>See also</h2>
<p>Source: """ + src("Game/Assets/AssetManager.hpp") + ", " + src("Game/Assets/AssetPool.hpp") + ", " + src("Game/Assets/FrameTable.hpp") + """.
<a href="../examples/texture_demo.html">Texture demo</a> &middot; <a href="../examples/animation_demo.html">Animation demo</a></p>
"""

SCENES = """
<h1>Scenes</h1>
<p class="lede">A scene is one level's worth of entities, saved to a file and loaded back.</p>

<h2>The concept</h2>
<p>Because an ECS world is just entities and plain-data components, a level can be serialized by writing out every
entity's components and rebuilt by reading them back. That makes levels data rather than code, and lets tools such as
the <a href="../../editor/index.html">level editor</a> author them. A scene manager decides which scene is active and how
to switch between them.</p>

<h2>How ASGE does it</h2>
<h3>SceneSerializer</h3>
<p><code>SceneSerializer</code> writes every alive entity of a <code>Registry</code> to a TOML scene file
(<code>.asgescene</code>) and populates a registry from one. Each component type has its own
<code>Serializer&lt;T&gt;</code> specialization. Asset references are stored as virtual paths, which is why scenes survive
being moved: see <a href="filesystem.html">Filesystem &amp; VFS</a>.</p>

<h3>SceneManager</h3>
<p><code>SceneManager</code> owns one <code>Registry</code> holding only the <em>active</em> scene's entities.
<code>LoadScene(virtualPath)</code> makes a scene active, <code>SaveScene(path)</code> saves the active one,
<code>UnloadScene()</code> clears it and <code>RenameActiveScene()</code> retags it for "Save As". Every other visited scene
stays resident as an in-memory <em>snapshot</em>, so switching back is instant and does not re-read the file;
<code>EvictCachedScene()</code> and <code>ClearCache()</code> drop snapshots. Entity handles change when a snapshot is
restored, which <code>TakeRestoredEntities()</code> reports as an old-to-new map.</p>
""" + code_card("scenes.cpp", "cpp", """asge::game::scene::SceneManager scenes( vfs );

scenes.LoadScene( "scenes/level1.asgescene" );          // BoolResult
asge::ecs::Registry& registry = scenes.GetRegistry();   // the active scene's entities""") + """
<p><code>Game</code> owns a scene manager and exposes loading through the project API, so a typical game loads a project
and scene instead of building one by hand.</p>

<h2>See also</h2>
<p>Source: """ + src("Game/Scene/SceneManager.hpp") + ", " + src("Game/Scene/SceneSerializer.hpp") + """.
<a href="../examples/scene_demo.html">Scene demo</a> &middot; <a href="ecs.html">ECS</a></p>
"""

PHYSICS = """
<h1>Physics</h1>
<p class="lede">2D physics here means three things: moving things, noticing when they overlap, and deciding what
happens next.</p>

<h2>The concept</h2>
<p>A simple engine splits collision into a <em>broad</em> step (which pairs might touch), a <em>narrow</em> step
(do these two shapes overlap, and by how much) and <em>resolution</em> (push them apart, or just report it). Shapes carry
a <em>layer</em> and a <em>mask</em> so, for example, bullets can ignore other bullets.</p>

<h2>How ASGE does it</h2>
<h3>Components</h3>
<ul>
  <li><code>Velocity</code> and <code>Transform</code>: position changes by velocity each frame.</li>
  <li><code>Collider</code>: a <code>Rect</code> or <code>Circle</code> hitbox, independent of the sprite, with a <code>m_Resolution</code> of <code>Solid</code> (default, pushed out), <code>Trigger</code> (sensor, fires events, never pushed) or <code>Unknown</code> (ignored), plus <code>m_Layer</code> and <code>m_Mask</code> bitmasks. Two colliders only interact if each one's layer is in the other's mask.</li>
  <li><code>Rigidbody</code>: marks an entity as physics-driven (gravity and push-out); an entity needs both <code>Velocity</code> and <code>Rigidbody</code> to be movable, otherwise it acts as static level geometry.</li>
</ul>

<h3>Systems</h3>
<p>These run inside <code>Game::Update</code> in this order (see <a href="game-loop.html">Game loop &amp; timing</a>):</p>
<ol>
  <li><code>GravitySystem</code> and <code>MovementSystem</code>: plain Euler integration, <code>position += velocity * dt</code>.</li>
  <li><code>DetectCollisions</code>: a naive all-pairs check that skips pairs whose layer/mask do not match and dispatches on the actual shape pair, returning a list of <code>CollisionContact</code>.</li>
  <li><code>ResolveCollisions</code>: pushes apart every non-trigger contact using the penetration vector from <code>math::PenetrationVector</code>.</li>
  <li><code>DispatchTriggerEvents</code>: fires <code>OnCollisionTriggerEnter</code>, <code>Stay</code> and <code>Exit</code> for trigger contacts, using <code>PhysicsState</code> to remember last frame's pairs.</li>
</ol>
""" + code_card("physics.cpp", "cpp", """using namespace asge::game::components;

auto& collider = registry.GetOrAddComponent<Collider>( entity ).get();
collider.m_Resolution = ResolutionType::Trigger;   // sensor: reports overlaps, never pushes
collider.m_Layer      = 0b01;                      // I live on layer 1...
collider.m_Mask       = 0b10;                      // ...and only care about layer 2""") + """
<p>The all-pairs check is simple and fine for small scenes; a broad-phase structure is the natural upgrade if
entity counts grow.</p>

<h2>See also</h2>
<p>Source: """ + src("Game/Systems/PhysicsSystem.hpp") + ", " + src("Game/Components/Collider.hpp") + ", " + src("Core/Math/Geometry/Collision.hpp") + """.
<a href="../examples/physics_demo.html">Physics demo</a> &middot; <a href="math.html">Math</a></p>
"""

UI = """
<h1>UI</h1>
<p class="lede">Menus and HUDs in ASGE are ordinary entities with UI components, laid out and clicked by two systems.</p>

<h2>The concept</h2>
<p>Game UI has two jobs besides drawing: <em>interaction</em> (is the pointer over this, was it clicked) and
<em>layout</em> (where does each widget go, so you do not position everything by hand). Treating widgets as entities means
the same scene format, hierarchy and editor tooling cover UI too.</p>

<h2>How ASGE does it</h2>
<h3>Widgets</h3>
<p>Each widget is a bundle of components over a <code>UIRect</code>: <code>UIButton</code>, <code>UILabel</code>,
<code>UICheckbox</code>, <code>UISlider</code> and <code>UIPanel</code>. You do not assemble these by hand; the
<code>asge::game::ui::Create*</code> functions take a descriptor and return a <code>Result&lt;ecs::Entity&gt;</code>:
<code>CreateLabel</code>, <code>CreateButton</code>, <code>CreateCheckbox</code>, <code>CreateSlider</code> and
<code>CreatePanel</code>. The editor uses the same functions, so what it builds is what a game gets.</p>
""" + code_card("ui.cpp", "cpp", """asge::game::ui::ButtonDesc desc;
desc.m_Name     = "Play";
desc.m_Position = asge::math::Float2{ 16.0f, 16.0f };
desc.m_OnClick  = [] { /* start the game */ };

auto button = asge::game::ui::CreateButton( registry, desc );   // Result<ecs::Entity>""") + """
<h3>Interaction and layout</h3>
<ul>
  <li><code>UIInteractionSystem</code> runs first in a frame (before game logic) and updates every interactable's hovered, held and clicked flags from the <a href="input.html">input state</a>; a button's <code>m_OnClick</code> fires when it is released while hovered.</li>
  <li><code>UILayoutSystem</code> places each panel's children. A panel's layout is <code>Absolute</code> (children keep their position), <code>Grid</code> (rows and columns), <code>VStack</code> or <code>HStack</code>; a child may carry a <code>UILayoutItem</code> to choose whether it fills its slot and how it aligns.</li>
  <li>Widgets are children of panels through the normal entity <code>Hierarchy</code>; a screen-space flag keeps HUD elements fixed while the camera moves.</li>
</ul>
<p>Text uses a baked <code>Font</code> loaded through the <a href="assets.html">asset manager</a>; a label with text but no
font path is rejected with <code>UIWidgetError::TextWithNoFont</code>.</p>

<h2>See also</h2>
<p>Source: """ + src("Game/UI.hpp") + ", " + src("Game/Systems/UISystem.hpp") + ", " + src("Game/Components/UI/UIPanel.hpp") + """.
<a href="../../editor/arrange-the-ui.html">Arrange the UI in the editor</a></p>
"""

MATH = """
<h1>Math</h1>
<p class="lede">The small maths toolbox the rest of the engine leans on: vectors, shapes, collision tests and
interpolation.</p>

<h2>The concept</h2>
<p>2D games are mostly vectors and rectangles. Keeping them in one dependency-free module, rather than reaching into SDL
types, means gameplay, physics, rendering and the editor all speak the same small set of types.</p>

<h2>How ASGE does it</h2>
<h3>Linear algebra</h3>
<p><code>asge::math::Vec2&lt;T&gt;</code> (and <code>Vec3</code>), built on a shared <code>VecN</code>, with the common
aliases <code>Float2</code>, <code>Double2</code>, <code>Int2</code> and <code>Int642</code>. Arithmetic operators are
provided; positions, sizes and mouse deltas throughout the API are <code>Float2</code> or <code>Int2</code>.</p>

<h3>Geometry and collision</h3>
<p><code>Rect</code> and <code>Circle</code> are the two collider shapes. <code>Collision.hpp</code> has the overlap tests
(<code>AabbOverlap</code>, <code>CircleOverlap</code>) and <code>PenetrationVector</code> overloads for every
rect/circle pairing, returning an <code>std::optional&lt;Float2&gt;</code> that says how to push the first shape out of the
second. <a href="physics.html">Physics</a> is built on these.</p>

<h3>Interpolation</h3>
<ul>
  <li><code>Easing</code> and <code>Ease(easing, t)</code>: <code>Linear</code>, <code>InQuad</code>, <code>OutQuad</code>, <code>InOutQuad</code>, <code>OutCubic</code>; the input is clamped to [0, 1].</li>
  <li><code>Tween&lt;T&gt;</code>: moves a float, <code>Float2</code> or similar from one value to another over a duration; call <code>Update(dt)</code> each frame and read <code>Value()</code>.</li>
  <li><code>CatmullRomSpline</code>: a smooth curve through waypoints, with arc-length sampling so motion along it is at constant speed. <code>PathFollowingSystem</code> and the editor's path preview use it.</li>
</ul>
""" + code_card("math.cpp", "cpp", """using asge::math::Easing;

asge::math::Tween<float> slide( 0.0f, 100.0f, /* seconds */ 2.0f, Easing::OutQuad );

// each frame:
slide.Update( dt );
float x = slide.Value();       // eased value; slide.IsFinished() once the duration has elapsed""") + """
<h2>See also</h2>
<p>Source: """ + src("Core/Math/Math.hpp") + ", " + src("Core/Math/Geometry/Collision.hpp") + ", " + src("Core/Math/Interpolation/Tween.hpp") + """.
<a href="../examples/rotation_demo.html">Rotation demo</a> &middot; <a href="../examples/path_following_demo.html">Path following demo</a></p>
"""

PAGES = [
    ("input", "Input", "Keyboard and mouse state, with down/pressed/released queries derived from the event stream.", INPUT),
    ("rendering", "Rendering", "The thin video layer: VideoSystem, IRenderer, textures, cameras and the SDL3 backend.", RENDERING),
    ("audio", "Audio", "The audio device and its stream pool, AudioSource components and the AudioSystem.", AUDIO),
    ("filesystem", "Filesystem & VFS", "Virtual paths mounted onto real directories, file IO and file watching.", FILESYSTEM),
    ("assets", "Assets", "The AssetManager's path-keyed caches, sprite resolution and spritesheet frame tables.", ASSETS),
    ("scenes", "Scenes", "SceneSerializer and SceneManager: saving, loading and switching between levels.", SCENES),
    ("physics", "Physics", "Colliders, layers and masks, and the movement, collision and trigger systems.", PHYSICS),
    ("ui", "UI", "Widgets as entities, the Create* functions, and the interaction and layout systems.", UI),
    ("math", "Math", "Vectors, shapes, collision tests, easing, tweens and splines.", MATH),
]
