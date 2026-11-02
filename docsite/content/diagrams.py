"""Inline SVG diagrams for the theory pages.

Colours and fonts come from classes in style.css (`.diagram .d-*`), which read the
site's CSS variables, so the figures follow the light/dark toggle. Names and
relationships are checked against src/ASGE (Application::Run, Game::Update, the
#include graph between the Core/Video/Input/Audio/Events/Game/Application folders).
The viewBox is 760 wide to fit the article column; min-width 640 keeps text legible
on phones, where the figure scrolls inside its own box.
"""

GAME_LOOP = """<figure class="diagram">
<div class="diagram-scroll">
<svg viewBox="0 0 760 292" role="img" aria-labelledby="gl-title gl-desc" style="min-width:640px">
<title id="gl-title">One frame of the ASGE game loop</title>
<desc id="gl-desc">Five steps run in order every frame: advance the clock, read input, update, render and present, then the loop returns to the first step.</desc>
<defs>
<marker id="gl-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 0L10 5L0 10z" class="d-head"/></marker>
<marker id="gl-arrow-b" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 0L10 5L0 10z" class="d-head-b"/></marker>
</defs>
<path class="d-arrow" d="M138 92H166" marker-end="url(#gl-arrow)"/>
<path class="d-arrow" d="M290 92H318" marker-end="url(#gl-arrow)"/>
<path class="d-arrow" d="M442 92H470" marker-end="url(#gl-arrow)"/>
<path class="d-arrow" d="M594 92H622" marker-end="url(#gl-arrow)"/>
<path class="d-arrow d-dash d-b" d="M684 128V196Q684 204 676 204H84Q76 204 76 196V132" marker-end="url(#gl-arrow-b)"/>
<rect x="316" y="194" width="128" height="20" class="d-mask"/>
<text x="380" y="208" text-anchor="middle" class="d-sub">next frame</text>
<rect x="14" y="56" width="124" height="72" rx="6" class="d-node"/>
<text x="76" y="88" text-anchor="middle" class="d-name">Advance clock</text>
<text x="76" y="108" text-anchor="middle" class="d-sub">Tick()</text>
<rect x="166" y="56" width="124" height="72" rx="6" class="d-node"/>
<text x="228" y="88" text-anchor="middle" class="d-name">Read input</text>
<text x="228" y="108" text-anchor="middle" class="d-sub">NewFrame()</text>
<rect x="318" y="56" width="124" height="72" rx="6" class="d-node d-focal"/>
<text x="380" y="88" text-anchor="middle" class="d-name">Update</text>
<text x="380" y="108" text-anchor="middle" class="d-sub">Update(dt)</text>
<rect x="470" y="56" width="124" height="72" rx="6" class="d-node"/>
<text x="532" y="88" text-anchor="middle" class="d-name">Render</text>
<text x="532" y="108" text-anchor="middle" class="d-sub">Render()</text>
<rect x="622" y="56" width="124" height="72" rx="6" class="d-node"/>
<text x="684" y="88" text-anchor="middle" class="d-name">Present</text>
<text x="684" y="108" text-anchor="middle" class="d-sub">Present()</text>
<rect x="14" y="248" width="14" height="14" rx="2" class="d-node d-focal"/>
<text x="36" y="260" class="d-sub">your game logic runs here</text>
<path class="d-arrow d-dash d-b" d="M280 255H316"/>
<text x="326" y="260" class="d-sub">the loop repeats</text>
</svg>
</div>
<figcaption>One frame of <code>Application::Run()</code>. ASGE uses a variable timestep: one <code>Update</code> per frame, with the scaled delta time.</figcaption>
</figure>"""

LAYERS = """<figure class="diagram">
<div class="diagram-scroll">
<svg viewBox="0 0 760 420" role="img" aria-labelledby="ly-title ly-desc" style="min-width:640px">
<title id="ly-title">ASGE engine layers</title>
<desc id="ly-desc">Four layers stacked top to bottom: Application, Game, a row of subsystems (Video, Input, Audio, Events) and Core. Each layer uses the ones below it.</desc>
<defs>
<marker id="ly-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 0L10 5L0 10z" class="d-head"/></marker>
</defs>
<path class="d-arrow" d="M30 36V348" marker-end="url(#ly-arrow)"/>
<rect x="6" y="176" width="48" height="20" class="d-mask"/>
<text x="30" y="190" text-anchor="middle" class="d-sub">uses</text>
<rect x="64" y="24" width="684" height="336" rx="10" class="d-node"/>
<rect x="66" y="110" width="680" height="80" class="d-node d-focal"/>
<path class="d-rule" d="M66 108H746M66 192H746M66 276H746"/>
<text x="82" y="71" class="d-sub">L4</text>
<text x="120" y="72" class="d-name d-lg">Application</text>
<text x="732" y="71" text-anchor="end" class="d-sub">Application&lt;TGame&gt;::Run()</text>
<text x="82" y="155" class="d-sub">L3</text>
<text x="120" y="156" class="d-name d-lg">Game</text>
<text x="732" y="155" text-anchor="end" class="d-sub">states, scenes, systems, assets</text>
<text x="82" y="239" class="d-sub">L2</text>
<text x="120" y="240" class="d-name d-lg">Subsystems</text>
<rect x="250" y="212" width="92" height="44" rx="6" class="d-node"/>
<text x="296" y="239" text-anchor="middle" class="d-name">Video</text>
<rect x="354" y="212" width="92" height="44" rx="6" class="d-node"/>
<text x="400" y="239" text-anchor="middle" class="d-name">Input</text>
<rect x="458" y="212" width="92" height="44" rx="6" class="d-node"/>
<text x="504" y="239" text-anchor="middle" class="d-name">Audio</text>
<rect x="562" y="212" width="92" height="44" rx="6" class="d-node"/>
<text x="608" y="239" text-anchor="middle" class="d-name">Events</text>
<text x="82" y="323" class="d-sub">L1</text>
<text x="120" y="324" class="d-name d-lg">Core</text>
<text x="732" y="323" text-anchor="end" class="d-sub">ECS, math, memory, filesystem, time</text>
<rect x="64" y="380" width="14" height="14" rx="2" class="d-node d-focal"/>
<text x="86" y="392" class="d-sub">where your game code lives</text>
</svg>
</div>
<figcaption>Application sits on Game, Game builds on the subsystems, and everything rests on Core. Folders under <code>src/ASGE/</code>.</figcaption>
</figure>"""


# --- helpers for the figures below -------------------------------------------------

def _node(x, y, w, h, name, sub="", focal=False, ghost=False):
    cx, cy = x + w // 2, y + h // 2
    cls = "d-node d-focal" if focal else "d-node d-ghost" if ghost else "d-node"
    out = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" class="{cls}"/>'
    if sub:
        out += f'<text x="{cx}" y="{cy - 4}" text-anchor="middle" class="d-name">{name}</text>'
        out += f'<text x="{cx}" y="{cy + 16}" text-anchor="middle" class="d-sub">{sub}</text>'
    else:
        out += f'<text x="{cx}" y="{cy + 5}" text-anchor="middle" class="d-name">{name}</text>'
    return out


def _legend(y, focal_text, dash_text=None):
    out = (f'<rect x="14" y="{y}" width="14" height="14" rx="2" class="d-node d-focal"/>'
           f'<text x="36" y="{y + 12}" class="d-sub">{focal_text}</text>')
    if dash_text:
        out += (f'<path class="d-arrow d-dash d-b" d="M300 {y + 7}H336"/>'
                f'<text x="346" y="{y + 12}" class="d-sub">{dash_text}</text>')
    return out


def _figure(slug, title, desc, height, body, caption, top=0):
    return f"""<figure class="diagram">
<div class="diagram-scroll">
<svg viewBox="0 {top} 760 {height}" role="img" aria-labelledby="{slug}-title {slug}-desc" style="min-width:640px">
<title id="{slug}-title">{title}</title>
<desc id="{slug}-desc">{desc}</desc>
<defs>
<marker id="{slug}-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 0L10 5L0 10z" class="d-head"/></marker>
<marker id="{slug}-arrow-b" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M0 0L10 5L0 10z" class="d-head-b"/></marker>
</defs>
{body}
</svg>
</div>
<figcaption>{caption}</figcaption>
</figure>"""


def _arrow(slug, d, teal=False, dash=False):
    cls = "d-arrow" + (" d-b" if teal else "") + (" d-dash" if dash else "")
    suffix = "-b" if teal else ""
    return f'<path class="{cls}" d="{d}" marker-end="url(#{slug}-arrow{suffix})"/>'


# --- ECS: Registry -> View -> System ---------------------------------------------

ECS = _figure(
    "ecs", "How the ECS Registry feeds a system",
    "The Registry owns an entity allocator and one component pool per type. A View iterates the pools that hold the requested components and hands the matching entities to a system.",
    280,
    '<rect x="14" y="24" width="330" height="224" rx="10" class="d-node"/>'
    '<text x="32" y="52" class="d-name d-lg">Registry</text>'
    '<text x="332" y="50" text-anchor="end" class="d-sub">the single entry point</text>'
    + _node(34, 68, 290, 64, "Entity allocator", "index + generation")
    + _node(34, 152, 290, 76, "Component pool, one per type", "sparse array to dense array", focal=True)
    + _arrow("ecs", "M324 190H372Q380 190 380 182V164Q380 156 388 156H404")
    + _node(404, 104, 140, 72, "View", "View&lt;Ts...&gt;")
    + _arrow("ecs", "M544 140H604")
    + _node(604, 104, 142, 72, "System", "MovementSystem")
    + _legend(256, "where components live"),
    "A view iterates the smallest pool among the requested types and filters the rest. Systems keep no state of their own.",
)

# --- Events and signals: two separate paths ---------------------------------------

EVENTS = _figure(
    "ev", "System events and signals in ASGE",
    "Top lane: an SDL event is translated once into a SystemEvent, then forwarded to the input system and to Game::OnSystemEvent. Bottom lane: a gameplay system emits a Signal, which calls every connected slot.",
    324,
    '<text x="14" y="26" class="d-lane">From the OS</text>'
    + _node(14, 68, 120, 64, "SDL event")
    + _arrow("ev", "M134 100H170")
    + _node(170, 68, 140, 64, "SystemEvent", "std::variant", focal=True)
    + _arrow("ev", "M310 84H370Q378 84 378 76V72Q378 64 386 64H420")
    + _arrow("ev", "M310 116H370Q378 116 378 124V128Q378 136 386 136H420")
    + _node(420, 36, 326, 56, "Input system", "gets each event first")
    + _node(420, 108, 326, 56, "Game::OnSystemEvent", "TryGet&lt;KeyboardEvent&gt;()")
    + '<path class="d-rule" d="M14 188H746"/>'
    + '<text x="14" y="214" class="d-lane">Your own code</text>'
    + _node(14, 232, 160, 64, "Gameplay system", "Emit(10)")
    + _arrow("ev", "M174 264H250")
    + _node(250, 232, 160, 64, "Signal&lt;int&gt;", "Connect()")
    + _arrow("ev", "M410 264H486")
    + _node(486, 232, 260, 64, "Slot", "void(int), for example the HUD")
    + _legend(304, "where SDL types stop"),
    "System events carry what the OS reports; signals are for your own code to announce things.",
)

# --- Scenes: file <-> serializer <-> registry <-> snapshots ------------------------

SCENES = _figure(
    "sc", "How ASGE loads, saves and switches scenes",
    "A scene file is read by the SceneSerializer into the single active Registry. Switching scenes stores the active registry as an in-memory snapshot and restores it later without re-reading the file.",
    216,
    _node(8, 56, 124, 72, "Scene file", ".asgescene")
    + _node(220, 56, 124, 72, "Serializer", "SceneSerializer")
    + _node(432, 56, 124, 72, "Registry", "active scene", focal=True)
    + _node(644, 56, 108, 72, "Snapshots", "in memory")
    + _arrow("sc", "M132 80H220") + _arrow("sc", "M220 104H132", dash=True, teal=True)
    + _arrow("sc", "M344 80H432") + _arrow("sc", "M432 104H344", dash=True, teal=True)
    + _arrow("sc", "M556 80H644") + _arrow("sc", "M644 104H556", dash=True, teal=True)
    + '<text x="176" y="68" text-anchor="middle" class="d-sub">LoadScene</text>'
    + '<text x="176" y="124" text-anchor="middle" class="d-sub">SaveScene</text>'
    + '<text x="388" y="68" text-anchor="middle" class="d-sub">populate</text>'
    + '<text x="388" y="124" text-anchor="middle" class="d-sub">write</text>'
    + '<text x="600" y="68" text-anchor="middle" class="d-sub">snapshot</text>'
    + '<text x="600" y="124" text-anchor="middle" class="d-sub">restore</text>'
    + _legend(176, "the one live Registry", "going back"),
    "<code>SceneManager</code> owns the live registry and the snapshots, so switching back to a visited scene is instant.",
)

# --- Physics: the collision pipeline ----------------------------------------------

PHYSICS = _figure(
    "ph", "The ASGE collision pipeline",
    "Each frame the QuadTree broad phase finds candidate pairs, the narrow phase tests them exactly, and the result splits into solid pairs that are pushed apart and trigger pairs that fire events after the walk.",
    236,
    _node(14, 56, 136, 72, "Broad phase", "QuadTree", focal=True)
    + _arrow("ph", "M150 92H170")
    + _node(170, 56, 136, 72, "Find pairs", "ForEachPair")
    + _arrow("ph", "M306 92H326")
    + _node(326, 56, 136, 72, "Narrow phase", "PenetrationVector")
    + _arrow("ph", "M462 80H482Q490 80 490 72V68Q490 60 498 60H514")
    + _arrow("ph", "M462 104H482Q490 104 490 112V140Q490 148 498 148H514")
    + _node(514, 28, 232, 64, "Push apart", "Solid: mass-weighted")
    + _node(514, 116, 232, 64, "Trigger events", "Enter/Stay/Exit after the walk")
    + _legend(204, "rebuilt every frame"),
    "Gravity and movement run first. Pairs whose layer and mask do not match, or whose resolution is Unknown, are skipped in the narrow phase.",
)

# --- Rendering: everything above talks to IRenderer --------------------------------

RENDERING = _figure(
    "rd", "How a frame reaches the screen",
    "Game::Render runs the render pipeline, which draws through the IRenderer interface. The only backend today is SDL; OpenGL, Vulkan and DirectX are possible future backends. The Camera transforms every draw call.",
    300,
    _node(398, 12, 140, 56, "Camera", "SetCamera()")
    + _arrow("rd", "M468 68V96")
    + _node(14, 96, 140, 72, "Game::Render", "once per frame")
    + _arrow("rd", "M154 132H206")
    + _node(206, 96, 140, 72, "Render pipeline", "sprites, Y-sort")
    + _arrow("rd", "M346 132H398")
    + _node(398, 96, 140, 72, "IRenderer", "Clear, Draw*", focal=True)
    + _arrow("rd", "M538 132H590")
    + _node(590, 96, 140, 72, "SDL backend", "the only one yet")
    + _arrow("rd", "M468 168V224Q468 232 476 232H590", teal=True, dash=True)
    + _node(590, 204, 140, 56, "Future backends", "OpenGL, Vulkan", ghost=True)
    + _legend(276, "the interface everything uses", "possible later"),
    "Everything above <code>IRenderer</code> talks to the interface, so a new backend would not touch game code.",
)

# --- Assets: path-keyed cache over the VFS ---------------------------------------

ASSETS = _figure(
    "as", "How the AssetManager loads an asset",
    "Your code asks the AssetPool for a virtual path. On a cache hit the pool returns the existing asset. On a miss the path is resolved through the VirtualFileSystem, decoded, stored in the pool and returned.",
    226,
    _node(14, 96, 140, 72, "Your code", "GetImage(path)")
    + _arrow("as", "M154 132H250")
    + _node(250, 96, 140, 72, "AssetPool", "path-keyed cache", focal=True)
    + _arrow("as", "M390 132H506")
    + '<text x="448" y="122" text-anchor="middle" class="d-sub">hit</text>'
    + _node(506, 96, 140, 72, "Asset", "Asset&lt;Image&gt;")
    + _arrow("as", "M320 168V204", teal=True, dash=True)
    + '<text x="332" y="190" class="d-sub">miss</text>'
    + _node(240, 204, 160, 48, "VirtualFileSystem", "")
    + _arrow("as", "M400 228H506", teal=True, dash=True)
    + '<text x="453" y="220" text-anchor="middle" class="d-sub">real path</text>'
    + _node(506, 204, 140, 48, "Decode", "")
    + _arrow("as", "M576 204V172", teal=True, dash=True)
    + '<text x="588" y="192" class="d-sub">cached</text>'
    + _legend(268, "loaded once per path", "only on a miss"),
    "A second <code>GetImage</code> for the same path returns the cached asset without decoding again.",
    top=64,
)

# --- Input: one event stream, two consumers ----------------------------------------

INPUT = _figure(
    "in", "How input reaches your game state",
    "Each polled SystemEvent goes to the InputSystem, which updates the InputState that your state's Update receives as a read-only reference. The same event also reaches Game::OnSystemEvent.",
    312,
    _node(14, 60, 148, 72, "SystemEvent", "from the poll loop")
    + _arrow("in", "M162 96H208")
    + _node(208, 60, 148, 72, "InputSystem", "ProcessEvent()", focal=True)
    + _arrow("in", "M356 96H402")
    + _node(402, 60, 148, 72, "InputState", "read-only snapshot")
    + _arrow("in", "M550 96H596")
    + _node(596, 60, 148, 72, "State::Update", "Update(dt, input)")
    + _arrow("in", "M88 132V190", teal=True, dash=True)
    + '<text x="100" y="166" class="d-sub">same event</text>'
    + _node(14, 190, 148, 72, "OnSystemEvent", "raw events")
    + _legend(284, "where input is tracked", "the other consumer"),
    "<code>NewFrame()</code> rolls the current snapshot into the previous one before the events are fed in, which is what makes IsKeyPressed true for a single frame.",
)

# --- Audio: component -> system -> stream -> device --------------------------------

AUDIO = _figure(
    "au", "How a sound gets played",
    "Your code sets an AudioSource component to playing. The AudioSystem, last in the frame, starts it on an AudioStream taken from the AudioDevice's pool.",
    184,
    _node(14, 40, 124, 72, "Your code", "PlayAudioSource")
    + _arrow("au", "M138 76H166")
    + _node(166, 40, 124, 72, "AudioSource", "component")
    + _arrow("au", "M290 76H318")
    + _node(318, 40, 124, 72, "AudioSystem", "last system", focal=True)
    + _arrow("au", "M442 76H470")
    + _node(470, 40, 124, 72, "AudioStream", "PutData()")
    + _arrow("au", "M594 76H622")
    + _node(622, 40, 124, 72, "AudioDevice", "32 streams")
    + _legend(148, "runs once per frame"),
    "A source keeps the same stream for its whole lifetime, so replaying it never creates a new one.",
)
