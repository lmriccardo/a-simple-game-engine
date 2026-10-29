"""Engine theory section: the concept behind each subsystem, then how ASGE does it.

The core pages (ECS, game loop & timing, events & signals, memory & threading) live
here; the other subsystems are in theory_systems.py. All type and function names
are checked against src/ASGE.
"""
import html

from _shared import Page, code_card, register, render_page
from content.theory_systems import PAGES as SYSTEM_PAGES
from content.theory_systems import src

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

<h3>Why not inheritance?</h3>
<p>The classic alternative is a class tree: <code>Enemy</code> derives from <code>Actor</code>, a flying enemy from
<code>Enemy</code>, and so on. It works until a new object needs a mix of branches (a flying, shooting, destructible
crate) and the tree has to be reshuffled. With components, that crate is just an entity carrying the right bundle of
data; no class had to change.</p>
<h3>Data-oriented layout</h3>
<p>CPUs are fast at reading memory in order and slow at chasing pointers. A system that touches only
<code>Position</code> and <code>Velocity</code> reads two tightly packed arrays and never loads the sprite, health or
audio data of the same entities. That is the main performance argument for an ECS, and it grows with the entity count.</p>
<h3>Systems and ordering</h3>
<p>Systems hold no state of their own; everything they need is in components or shared resources. That makes them
easy to test and to reason about, but it means <em>order</em> matters: gravity must run before movement, and movement
before collision, or objects react to last frame's positions. Engines therefore fix the system order in one place.</p>
<h3>Trade-offs</h3>
<p>The price is indirection. Finding "the one entity with component X" means a lookup, behaviour is spread across
systems instead of living in one class, and removing an entity while iterating needs care. For small, singular objects
(the player controller, say) plain code is often clearer; the ECS earns its keep for the many similar things in a scene.</p>

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

<h3>Anatomy of a frame</h3>
<p>Every frame does the same four jobs in the same order: <strong>input</strong> (drain the OS event queue and update
input state), <strong>update</strong> (advance the simulation), <strong>render</strong> (draw the result) and
<strong>present</strong> (swap the finished image onto the screen). Doing input first means the update sees the freshest
state; presenting last is what makes a frame visible at all.</p>
<h3>Delta time</h3>
<p>Without delta time, <code>x += 5</code> each frame moves twice as far at 120 FPS as at 60 FPS. Multiplying by the
seconds since the last frame, <code>x += speed * dt</code>, makes motion depend on <em>time</em> and not on frame rate. A
very long frame (a breakpoint, a window drag) produces a huge delta that can push objects through walls, which is why
engines often clamp it.</p>
<h3>Variable versus fixed step</h3>
<p>A variable step is the simplest option and fine for most games. A fixed step runs the simulation at a constant rate
(say 60 times a second) and accumulates leftover real time, so results are reproducible and physics stays stable; the
cost is more machinery, and rendering then often needs to interpolate between two simulation states.</p>
<h3>Pacing the frame</h3>
<p>Without a limit a loop runs as fast as the machine allows, burning CPU and battery for no visible gain. Engines either
wait for the display's vertical sync or sleep until the target frame time has passed. Time can also be scaled
(slow motion) or paused, which is why a clock distinguishes <em>scaled</em> game time from <em>unscaled</em> real time:
a pause menu still needs real time to animate.</p>

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

<h3>Why decouple?</h3>
<p>If the player code calls the HUD directly, the player depends on the HUD, and the HUD cannot be removed or tested
without it. With an announcement instead ("score changed"), the producer neither knows nor cares who listens: the HUD, an
achievement tracker and a sound effect can all react, and none of them is named in the player code.</p>
<h3>Polling versus callbacks</h3>
<p>An event stream is usually <em>pulled</em>: the loop drains a queue once per frame and handles each message in
order, so all reactions happen at a predictable point. A signal is <em>pushed</em>: the slots run immediately, inside the
call that emitted it. Push is convenient but means arbitrary code runs in the middle of the emitter's work, so a slot that
modifies the very collection being iterated can cause trouble.</p>
<h3>Lifetimes</h3>
<p>The classic bug of callbacks is the dangling listener: an object is destroyed but its callback is still registered,
and the next emit calls into freed memory. Any signal design therefore needs a way to disconnect, either explicitly or
automatically when the listener goes away.</p>

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

<h3>Why general allocation hurts</h3>
<p>A general-purpose allocator has to handle any size and any lifetime, so it searches for free blocks, takes locks in
multithreaded programs and leaves gaps behind as blocks of different sizes come and go (<em>fragmentation</em>). Its cost
is also unpredictable, which shows up as an occasional slow frame. Knowing the allocation pattern lets you replace all that
with a few instructions.</p>
<h3>Matching pattern to allocator</h3>
<p>A <em>linear</em> (arena) allocator just bumps a pointer and frees everything at once: ideal for scratch data that
dies at the end of a frame. A <em>pool</em> keeps same-sized slots on a free list: ideal for many identical objects that
come and go in any order. A <em>stack</em> allows frees only in reverse order, matching strictly nested lifetimes. The
restriction is the point; each gives up some flexibility in exchange for speed and predictability.</p>
<h3>Threads in a game</h3>
<p>Creating a thread costs far more than calling a function, so spawning one per small task loses the benefit. A
thread pool starts its workers once and feeds them jobs from a queue. The hard part of parallel code is not starting
work but sharing data safely: jobs should either own their data or touch disjoint parts of it, since locks reintroduce the
stalls parallelism was meant to remove.</p>

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
    ("ecs", "Entity-Component-System", "Why a game object is split into identity, data and behaviour, and how ASGE's Registry stores entities and components, iterates them through views, and runs systems over them each frame.", ECS),
    ("game-loop", "Game loop & timing", "How the main loop reads input, updates the simulation with a delta time and renders, the order in which systems run every frame, and how game states, the clock and the profiler fit around it.", LOOP),
    ("events-signals", "Events & signals", "How raw SDL events are translated into engine events, and how the type-safe Signal/Connection pattern lets one part of the program announce something without knowing who is listening.", EVENTS),
    ("memory-threading", "Memory & threading", "The custom allocators (linear, pool, stack and free-list) that avoid per-frame heap traffic, when each one fits, and how the ThreadPool spreads work across cores.", MEMORY),
] + SYSTEM_PAGES

INDEX = """
<h1>Engine theory</h1>
<p class="lede">What the main parts of a game engine do, and how ASGE implements each one.
Every page first explains the concept, then shows the ASGE types that realise it, with a short snippet.</p>
<div class="grid">
""" + "\n".join(
    f'<a class="card" href="{slug}.html"><h3>{html.escape(title)}</h3><p>{html.escape(summary)}</p></a>'
    for slug, title, summary, _ in FULL_PAGES
) + """
</div>
"""

PAGES ={"engine/theory/index.html": register(Page(
    "engine/theory/index.html", "Engine theory", "engine", "Theory",
    "What the main parts of a game engine do, and how ASGE implements them.",
))}
for _slug, _title, _summary, _ in FULL_PAGES:
    PAGES[f"engine/theory/{_slug}.html"] = register(Page(
        f"engine/theory/{_slug}.html", _title, "engine", "Theory", _summary))


def build() -> dict[str, str]:
    out = {"engine/theory/index.html": render_page(page=PAGES["engine/theory/index.html"], body=INDEX)}
    for slug, _title, _summary, body in FULL_PAGES:
        path = f"engine/theory/{slug}.html"
        out[path] = render_page(page=PAGES[path], body=body)
    return out
