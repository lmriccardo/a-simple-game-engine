"""Engine theory section: the concept behind each subsystem, then how ASGE does it.

Four subsystems get full pages (ECS, game loop & timing, events & signals,
memory & threading); every other implemented subsystem is a short stub.
All type and function names below are checked against src/ASGE.
"""
import html

from _shared import GITHUB_URL, Page, code_card, register, render_page

SRC = f"{GITHUB_URL}/blob/main/src/ASGE"


def src(path: str) -> str:
    return f'<a href="{SRC}/{path}" target="_blank" rel="noopener"><code>{html.escape(path)}</code></a>'


# --- full pages --------------------------------------------------------------

ECS_SNIPPET = """struct Health { int m_Hp{ 100 }; };

asge::ecs::Registry registry;

auto entity = registry.CreateEntity();          // Result<Entity>
if ( !entity ) return;
registry.AddComponent( entity.Value(), Health{ 50 } );

// Every entity that has a Health; components arrive as std::reference_wrapper<T>.
for ( auto [e, health] : registry.View<Health>() )
{
    health.get().m_Hp -= 1;
}"""

ECS = """
<h1>Entity-Component-System</h1>
<p class="lede">An ECS splits a game object into <em>identity</em>, <em>data</em> and <em>behaviour</em>,
so behaviour can be written once and applied to every object that has the right data.</p>

<h2>The concept</h2>
<ul>
  <li><strong>Entity</strong> - just an id. It carries no data and no code.</li>
  <li><strong>Component</strong> - a plain bundle of data (a position, a sprite, a collider) attached to an entity.</li>
  <li><strong>System</strong> - a function that walks every entity owning a given set of components and acts on them.</li>
</ul>
<p>Storing each component type in its own densely packed array keeps iteration cache-friendly, and
composing objects from components avoids deep inheritance hierarchies.</p>

<h2>How ASGE does it</h2>
<h3>Generational entity handles</h3>
<p>An <code>asge::ecs::Entity</code> is an index plus a generation counter. When an entity is destroyed its
slot is recycled and its generation is bumped, so any stale handle still held elsewhere fails
<code>IsAlive()</code> instead of silently pointing at an unrelated new entity. A slot whose generation
reaches its maximum is retired rather than reused. Indices come from a fixed-capacity
<code>EntityAllocator</code> built on <code>mem::FreeList</code> (see
<a href="memory-threading.html">Memory &amp; threading</a>).</p>

<h3>Sparse-set component pools</h3>
<p>Each component type lives in its own <code>ComponentPool&lt;T&gt;</code>, a sparse set: a sparse array maps an
entity index to a slot in a dense array of components. <code>Insert</code>, <code>Get</code>, <code>Remove</code>
and <code>Contains</code> are O(1), and iteration only touches live components. Removal is swap-and-pop, so
iteration order may change after a removal.</p>

<h3>The Registry</h3>
<p><code>asge::ecs::Registry</code> is the single entry point. It owns the allocator and one pool per component
type, created lazily on the first <code>AddComponent&lt;T&gt;</code>. <code>DestroyEntity()</code> strips the entity
from every pool. The registry also holds per-registry <em>resources</em> (<code>SetResource&lt;T&gt;</code> /
<code>GetResource&lt;T&gt;</code>): at most one value of a type that is not tied to any entity.</p>

<h3>Views</h3>
<p><code>Registry::View&lt;Ts...&gt;()</code> returns a lazy, non-owning <code>View</code> over every entity that has
all of <code>Ts</code>. It iterates the smallest contributing pool and filters the rest. Entities carrying the
<code>Disable</code> marker, or whose ancestor in the <code>Hierarchy</code> does, are skipped unless you call
<code>IncludeDisabled()</code>. Use <code>View&lt;T const&gt;</code> for read-only access.</p>

""" + code_card("ecs.cpp", "cpp", ECS_SNIPPET) + """

<h3>Systems in the Game layer</h3>
<p>Systems are plain functions over a <code>Registry</code>, such as <code>MovementSystem</code>,
<code>PathFollowingSystem</code> and <code>TransformPropagationSystem</code>. <code>Game</code> runs them in one
fixed order every frame - see <a href="game-loop.html">Game loop &amp; timing</a>.</p>

<h2>Things to know</h2>
<ul>
  <li>A view built for a component type that has never been used is permanently empty, even if the type is added later.</li>
  <li>Views hold raw pointers into the pools and must not outlive the <code>Registry</code>.</li>
  <li>A pool is not told when an entity dies; <code>Registry::DestroyEntity</code> handles that, so destroy through the registry.</li>
</ul>

<h2>See also</h2>
<p>Source: """ + src("Core/ECS/Registry.hpp") + ", " + src("Core/ECS/View.hpp") + ", " + src("Core/ECS/ComponentPool.hpp") + """.
<a href="../examples/ecs_demo.html">ECS demo</a> &middot; <a href="../api/index.html">API reference</a></p>
"""

LOOP_SNIPPET = """// asge::Application<TGame>::Run(), abridged
while ( m_Running )
{
    GET_TIMESYSTEM.Tick();                       // advance the clock
    m_InputSys.NewFrame();
    while ( SDL_PollEvent( &event ) )            // drain OS events
        processEvent( &event );
    m_Game->Update( time::DeltaTime(), m_InputSys.GetState() );
    m_Game->Render( m_VideoSys.GetRenderer() );
    m_VideoSys.GetRenderer().Present();
}"""

TIMING_SNIPPET = """asge::time::Scale( 0.5f );            // slow motion
asge::time::Paused();                    // true while paused
float dt  = asge::time::DeltaTime();     // scaled, zero while paused
float raw = asge::time::UnscaledDeltaTime();

void Update()
{
    PROFILE_SCOPE( "Update" );           // times this scope into a named bucket
    // ...
}"""

LOOP = """
<h1>Game loop &amp; timing</h1>
<p class="lede">The game loop is the heartbeat of a game: read input, advance the simulation by the time that
passed, draw, repeat.</p>

<h2>The concept</h2>
<p>Each iteration of the loop is a <em>frame</em>. Simulation code must be told how long the previous frame took
(the <em>delta time</em>) so movement stays the same speed on a 30 Hz and a 144 Hz display. Engines differ on
whether the simulation uses a variable step (one update per frame with the real delta) or a fixed step
(update at a constant rate, possibly several times per frame); a variable step is simpler, a fixed step is
more deterministic.</p>

<h2>How ASGE does it</h2>
<h3>Application</h3>
<p><code>asge::Application&lt;TGame&gt;</code> owns the video, input and audio systems and constructs your
<code>TGame</code> in place. <code>Run()</code> executes the loop below and stops on an OS quit event or when a game
state requests <code>TransitionKind::Quit</code>.</p>
""" + code_card("Application.hpp", "cpp", LOOP_SNIPPET) + """
<p>ASGE uses a <strong>variable timestep</strong>: one <code>Update</code> per frame, with the scaled delta time.
The target frame rate comes from <code>ApplicationConfig::s_TargetFps</code>.</p>

<h3>One frame inside Game</h3>
<p><code>Game&lt;TStateId&gt;::Update</code> runs the systems in the only valid order:</p>
<ol>
  <li>UI interaction</li>
  <li>the active state's own <code>Update()</code> (your game logic) - states never call systems themselves</li>
  <li>gravity, movement, path following</li>
  <li>UI layout, then one transform propagation</li>
  <li>collision detection and resolution, then trigger events</li>
  <li>audio</li>
</ol>
<p><code>Render</code> then resolves pending assets, clears with the visible state's colour, draws the scene through
the render pipeline, and finally lets the states draw on top.</p>

<h3>Game states</h3>
<p>Screens are <code>IGameState</code> objects on a <code>GameStateStack</code>. A state's <code>Update</code>
may return a <code>Transition</code> to <code>Push</code>, <code>Pop</code>, <code>Replace</code> or <code>Quit</code>.
Your <code>Game</code> subclass implements <code>CreateState()</code> to build a state from its id.</p>

<h3>Time</h3>
<p>The <code>asge::time</code> namespace exposes the clock: <code>DeltaTime()</code> (scaled and pause-aware),
<code>UnscaledDeltaTime()</code>, <code>ElapsedTime()</code>, <code>FrameIndex()</code>, <code>Scale()</code>,
<code>Paused()</code>; <code>Scale(float)</code>, <code>Pause(bool)</code> and <code>TargetFPS(fps)</code> are the setters. Pausing makes the scaled delta zero while the unscaled one keeps running,
which is what you want for a pause menu.</p>

<h3>Profiling</h3>
<p><code>PROFILE_SCOPE("name")</code> times the enclosing scope with a <code>ScopedTimer</code> and records it into
a named <code>TimingBucket</code> held by the <code>TimingProfiler</code> singleton.</p>
""" + code_card("timing.cpp", "cpp", TIMING_SNIPPET) + """

<h2>See also</h2>
<p>Source: """ + src("Application/Application.hpp") + ", " + src("Game/Game.hpp") + ", " + src("Core/Time/Time.hpp") + """.
<a href="../examples/scoped_profiler.html">Scoped profiler example</a> &middot; <a href="../examples/moving_box.html">Moving box</a></p>
"""

SIGNAL_SNIPPET = """asge::signals::Signal<int> onScore;

// Slot signature must match the signal's: void(int)
auto connection = onScore.Connect( []( int points ) { /* update HUD */ } );

onScore.Emit( 10 );        // runs every connected slot
connection.Disconnect();   // or pass a lifetime token to Connect() for automatic cleanup"""

EVENTS_SNIPPET = """void OnSystemEvent( asge::event::SystemEvent const& inSysEvent ) override
{
    if ( auto const* key = inSysEvent.TryGet<asge::event::KeyboardEvent>() )
    {
        // react to the key press
    }
}"""

EVENTS = """
<h1>Events &amp; signals</h1>
<p class="lede">Decoupled communication lets one part of the program announce that something happened without
knowing who cares.</p>

<h2>The concept</h2>
<p>Two common patterns: an <strong>event stream</strong>, where the platform produces a sequence of typed
messages that code polls or receives, and <strong>signals and slots</strong>, where a producer exposes a typed
signal and any number of listeners (slots) connect callbacks to it. Both remove direct dependencies between
the sender and its receivers.</p>

<h2>How ASGE does it</h2>
<h3>System events</h3>
<p>SDL events are translated once, in <code>event::_internal::ProcessEvent</code>, into
<code>asge::event::SystemEvent</code>: a <code>std::variant</code> over <code>QuitEvent</code>,
<code>WindowEvent</code>, <code>KeyboardEvent</code>, <code>MouseMotionEvent</code>, <code>MouseButtonEvent</code>,
<code>MouseWheelEvent</code> and <code>UnknownEvent</code>. Game code never sees SDL types; it asks for the case it cares
about with <code>TryGet&lt;EventT&gt;()</code>, which returns a pointer or null. <code>Application</code> forwards each event
to the input system and then to <code>Game::OnSystemEvent</code>.</p>
""" + code_card("state.cpp", "cpp", EVENTS_SNIPPET) + """

<h3>Signals</h3>
<p><code>asge::signals::Signal&lt;Args...&gt;</code> is a thread-safe, type-checked signal: every slot must have the
signature of the signal. <code>Connect()</code> returns a <code>Connection</code>. A connection ends when you call
<code>Disconnect()</code> or when the optional <em>lifetime token</em> (a weak pointer you pass to <code>Connect()</code>)
expires. Dead connections are pruned automatically on the next <code>Emit()</code>.</p>
""" + code_card("signals.cpp", "cpp", SIGNAL_SNIPPET) + """

<h2>Choosing</h2>
<p>System events carry what the OS reports; signals are for your own code, for example a gameplay system
announcing a score change to the HUD.</p>

<h2>See also</h2>
<p>Source: """ + src("Events/Events.hpp") + ", " + src("Core/Patterns/Signal.hpp") + """.
<a href="../examples/input_demo.html">Input demo</a></p>
"""


THREAD_SNIPPET = """asge::concurrent::ThreadPool pool;               // defaults to the hardware concurrency

auto result = pool.Submit( []( int n ) { return n * 2; }, 21 );   // std::future<int>
int value = result.get();                                         // 42

pool.Submit( [] { /* void work: no future is returned */ } );"""

MEMORY = """
<h1>Memory &amp; threading</h1>
<p class="lede">Games allocate and free a lot of small things every frame; predictable allocation and safe
parallel work keep the frame time steady.</p>

<h2>The concept</h2>
<p>General-purpose <code>new</code>/<code>delete</code> can be slow and fragment memory. Game engines replace it, where it
matters, with allocators that match an allocation <em>pattern</em>: bump pointers for per-frame scratch data,
pools for many same-sized objects, stacks for strictly nested lifetimes. For CPU parallelism, a thread pool
reuses a fixed set of worker threads instead of spawning one per task.</p>

<h2>How ASGE does it</h2>
<h3>Allocators (<code>asge::mem</code>)</h3>
<table>
  <tr><th>Type</th><th>Pattern</th><th>Notes</th></tr>
  <tr><td><code>LinearAllocator</code></td><td>Arena / bump pointer</td><td>O(1) alloc over a buffer you provide; <code>Reset()</code> frees everything, <code>GetMarker()</code> + <code>Rewind()</code> roll back; no individual frees.</td></tr>
  <tr><td><code>PoolAllocator&lt;T, N&gt;</code></td><td>Fixed-size slots</td><td>O(1) alloc and free from a free-list; returns an owning pointer whose deleter gives the slot back. Created through <code>Create()</code> as a <code>shared_ptr</code>.</td></tr>
  <tr><td><code>StackAllocator&lt;N&gt;</code></td><td>LIFO</td><td>Frees must be in reverse order (asserted in debug); <code>GetMarker()</code> / <code>FreeToMarker()</code> unwind and run destructors.</td></tr>
  <tr><td><code>FreeList&lt;N&gt;</code></td><td>Index allocator</td><td>Hands out indices in <code>[0, N)</code> with double-free detection. The ECS uses it to issue entity indices.</td></tr>
</table>

<h3>Thread pool (<code>asge::concurrent</code>)</h3>
<p><code>ThreadPool</code> owns a fixed set of <code>Worker</code> threads and a shared lock-free queue. <code>Submit()</code>
wraps a callable and its arguments in a job and returns a <code>std::future&lt;T&gt;</code>, or nothing for a callable that
returns <code>void</code>. The pool is neither copyable nor movable because workers hold references to its queue.</p>
""" + code_card("threads.cpp", "cpp", THREAD_SNIPPET) + """

<h2>See also</h2>
<p>Source: """ + src("Core/Memory/") + ", " + src("Core/Concurrent/ThreadPool.hpp") + """.
<a href="ecs.html">ECS</a> shows the free-list in use.</p>
"""

FULL_PAGES = [
    ("ecs", "Entity-Component-System", "Entities, components, systems and views, and how ASGE's Registry implements them.", ECS),
    ("game-loop", "Game loop & timing", "The main loop, the per-frame system order, game states, the clock and profiling.", LOOP),
    ("events-signals", "Events & signals", "SDL event translation, SystemEvent and the type-safe Signal/Connection pattern.", EVENTS),
    ("memory-threading", "Memory & threading", "Linear, pool and stack allocators, the free-list and the ThreadPool.", MEMORY),
]

# --- stubs (only subsystems present in src/ASGE) -----------------------------

STUBS = [
    ("input", "Input", "Keyboard and mouse state tracked every frame from the event stream.",
     "Input/InputSystem.hpp", "../examples/input_demo.html"),
    ("rendering", "Rendering", "The thin video layer over SDL3: window, renderer, textures, cameras and primitives.",
     "Video/VideoSystem.hpp", "../examples/shapes_demo.html"),
    ("audio", "Audio", "The audio device and streams, driven by AudioSource components.",
     "Audio/AudioDevice.hpp", "../examples/audio_demo.html"),
    ("filesystem", "Filesystem & VFS", "File IO, file watching and the virtual file system.",
     "Core/Filesystem/VirtualFileSystem.hpp", "../examples/file_watching.html"),
    ("assets", "Assets", "The AssetManager, typed asset pools and spritesheet frame tables.",
     "Game/Assets/AssetManager.hpp", "../examples/texture_demo.html"),
    ("scenes", "Scenes", "SceneManager and the scene serializer that loads and saves a Registry.",
     "Game/Scene/SceneManager.hpp", "../examples/scene_demo.html"),
    ("physics", "Physics", "Rigidbody and Collider components with collision detection and resolution systems.",
     "Game/Systems/PhysicsSystem.hpp", "../examples/physics_demo.html"),
    ("ui", "UI", "Anchored UI components with interaction and layout systems.",
     "Game/UI.hpp", "../examples/index.html"),
    ("math", "Math", "Linear algebra, geometry and interpolation helpers.",
     "Core/Math/Math.hpp", "../examples/rotation_demo.html"),
]


def _stub_body(title: str, summary: str, header: str, example: str) -> str:
    return f"""
<h1>{html.escape(title)}</h1>
<p><span class="badge badge-progress">In progress</span></p>
<p class="lede">{html.escape(summary)}</p>
<p>A full write-up of the concept and ASGE's implementation is still to come.
Meanwhile: source {src(header)}, <a href="{example}">a related example</a>, and the
<a href="../api/index.html">API reference</a>.</p>
"""


INDEX = """
<h1>Engine theory</h1>
<p class="lede">What the main parts of a game engine do, and how ASGE implements the ones it has today.
Each page first explains the concept, then shows the ASGE types that realise it.</p>
<div class="grid">
""" + "\n".join(
    f'<a class="card" href="{slug}.html"><h3>{html.escape(title)}</h3><p>{html.escape(summary)}</p></a>'
    for slug, title, summary, _ in FULL_PAGES
) + """
</div>
<h2>Shorter overviews</h2>
<ul>
""" + "\n".join(f'<li><a href="{slug}.html">{html.escape(title)}</a> - {html.escape(summary)}</li>' for slug, title, summary, *_ in STUBS) + """
</ul>
"""

PAGES = {"engine/theory/index.html": register(Page(
    "engine/theory/index.html", "Engine theory", "engine", "Theory",
    "What the main parts of a game engine do, and how ASGE implements them.",
))}
for _slug, _title, _summary, *_ in FULL_PAGES + STUBS:
    PAGES[f"engine/theory/{_slug}.html"] = register(Page(
        f"engine/theory/{_slug}.html", _title, "engine", "Theory", _summary))


def build() -> dict[str, str]:
    out = {"engine/theory/index.html": render_page(page=PAGES["engine/theory/index.html"], body=INDEX)}
    for slug, _title, _summary, body in FULL_PAGES:
        path = f"engine/theory/{slug}.html"
        out[path] = render_page(page=PAGES[path], body=body)
    for slug, title, summary, header, example in STUBS:
        path = f"engine/theory/{slug}.html"
        out[path] = render_page(page=PAGES[path], body=_stub_body(title, summary, header, example))
    return out
