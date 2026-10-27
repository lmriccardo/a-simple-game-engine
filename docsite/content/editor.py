"""Editor section: an overview plus a task-based guide to asge-editor.

Rewritten from the former per-topic pages; screenshots under assets/img/editor/
are reused. Tasks without a screenshot (marked below) ship text-only.
"""
import html

from _shared import GITHUB_URL, Page, code_card, register, render_page

IMG = "../assets/img/editor"


def shot(name: str, alt: str) -> str:
    return f'<img src="{IMG}/{name}" alt="{html.escape(alt)}" loading="lazy"/>'


def kv_table(rows: list[tuple[str, str]], head: tuple[str, str] = ("Item", "What it does")) -> str:
    body = "".join(f"<tr><td>{a}</td><td>{b}</td></tr>" for a, b in rows)
    return f"<table><tr><th>{head[0]}</th><th>{head[1]}</th></tr>{body}</table>"


BUILD_SNIPPET = """cmake --preset windows-editor-debug
cmake --build --preset windows-editor-debug --target asge-editor
# binary lands at bin/Debug/asge-editor.exe"""

# (slug, title, summary, body). Each body starts with an h1.
TASKS = [
    ("create-a-project", "Create a project",
     "Create or open a project and save its scenes from the File menu.",
     f"""
<h1>Create a project</h1>
<p class="lede">A project is the durable unit; a scene is one of its levels. Everything project- and scene-related lives under the File menu.</p>
{shot("file-menu.png", "The File menu with Create a Project and Create a Scene expanded")}
<h2>Steps</h2>
<ol>
  <li>Choose <strong>File &rarr; New &rarr; Create a Project...</strong>.</li>
  <li>Enter a name and a folder, the grid spacing and the game window size.</li>
  <li>Confirm. The editor creates the project and saves an <em>Empty Scene</em> inside it.</li>
</ol>
<p><strong>Result:</strong> an <code>.asgeproject</code> file plus one <code>.asgescene</code>, both open in the editor.</p>
<h2>File menu reference</h2>
{kv_table([
    ("New &rarr; Create a Project...", "Name, folder, grid spacing and game window size; auto-creates and saves an &quot;Empty Scene&quot;."),
    ("New &rarr; Create a Scene...", "Adds a new scene to the current project (disabled without one)."),
    ("Save / <kbd>Ctrl+P</kbd>", "Saves only the project file (mounts, assets, view, scene list) - never scene data, even a dirty scene."),
    ("Save As...", "Copies the project and every scene file to a new folder."),
    ("Save Scene / <kbd>Ctrl+S</kbd>", "Saves the active scene's entities."),
    ("Open... / Open Scene...", "Opens an existing <code>.asgeproject</code> / adds an existing <code>.asgescene</code> to the current project."),
], ("Item", "What it does"))}
<p>Every scene-affecting item is disabled while no project is open, and scene-specific ones stay disabled until a scene is active: there is nowhere for a new entity or asset import to persist to otherwise.</p>
<h2>What is stored where</h2>
<ul>
  <li><code>.asgeproject</code> - VFS mounts, known asset paths, view settings (grid spacing, target game window size) and the list of scene files.</li>
  <li><code>.asgescene</code> - a plain TOML file with one level's entities, the same format <code>SceneManager::SaveScene/LoadScene</code> use anywhere in the engine.</li>
  <li><code>asge.session</code> - remembers which project and scene were active; auto-saved every 30 seconds and restored on the next launch. It lives in your per-user data folder (<code>%APPDATA%\\ASGE\\Editor</code> on Windows), not next to the executable, so it works when the editor is installed somewhere read-only such as <code>C:\\Program Files</code>.</li>
</ul>
<p>Next: <a href="work-with-scenes.html">Work with scenes</a>.</p>
"""),
    ("work-with-scenes", "Work with scenes",
     "Switch between a project's scenes, spot unsaved changes and rename a scene.",
     f"""
<h1>Work with scenes</h1>
<p class="lede">A project can hold many scenes; the editor keeps exactly one active at a time.</p>
{shot("project-scene-hud.png", "The Project and Scene HUD next to the viewport")}
<h2>Steps</h2>
<ol>
  <li>Find the HUD panel next to the viewport's mouse-position readout. It shows <code>Project: &lt;name&gt;</code> and a <strong>Scene</strong> dropdown listing every scene the project knows about.</li>
  <li>Pick another scene. If the current one has unsaved changes (a filled dot next to the dropdown), it is saved first.</li>
  <li>To rename the active scene, use the Scene panel. This moves its <code>.asgescene</code> file on disk to match; no copy is left behind under the old name.</li>
</ol>
<p><strong>Result:</strong> selection, dirty state and asset resolution are reset for the incoming scene.</p>
<h2>How switching works</h2>
<p><code>SceneManager</code> keeps one scene's entities live at a time. Switching away suspends the outgoing scene into an in-memory snapshot instead of destroying it, and switching back to a visited scene restores that snapshot instead of re-reading from disk. Repeat switching is fast and a stale scene's entities never bleed into rendering, picking or the entity list.</p>
<p>Previous: <a href="create-a-project.html">Create a project</a> &middot; Next: <a href="navigate-the-viewport.html">Navigate the viewport</a>.</p>
"""),
    ("navigate-the-viewport", "Navigate the viewport",
     "Move entities, pan, zoom and set up the reference grid.",
     f"""
<h1>Navigate the viewport</h1>
<p class="lede">The viewport is a real, resizable window with the engine's own camera and viewport math underneath, so every overlay pans and zooms exactly like the scene.</p>
{shot("viewport-grid.png", "The viewport with its reference grid")}
{kv_table([
    ("Left-drag", "Move the clicked entity (or drag a gizmo arm to constrain to one axis)."),
    ("Middle-drag", "Pan the camera."),
    ("Scroll", "Zoom, anchored on the point under the cursor."),
    ("Restore View", "Reset pan and zoom to the default."),
], ("Input", "Effect"))}
<h2>Set the grid spacing</h2>
{shot("grid-settings.png", "The Grid settings dialog")}
<ol>
  <li>Open <strong>View &rarr; Grid</strong> (disabled without a project).</li>
  <li>Set the world-space spacing between the reference grid lines drawn behind every sprite.</li>
</ol>
<p>The value is saved in the project's own <code>[View]</code> settings, not in scene data.</p>
<p>Previous: <a href="work-with-scenes.html">Work with scenes</a> &middot; Next: <a href="configure-the-game-window.html">Configure the game window</a>.</p>
"""),
    ("configure-the-game-window", "Configure the game window",
     "Set the target game window size and preview what each camera frames.",
     f"""
<h1>Configure the game window</h1>
<p class="lede">Tell the editor what size a real build of the game runs at, so the previews match.</p>
{shot("game-window-settings.png", "The Game Window settings dialog")}
<h2>Steps</h2>
<ol>
  <li>Open <strong>View &rarr; Game Window</strong> (disabled without a project).</li>
  <li>Set the target width and height. They are saved in the project's <code>[View]</code> settings.</li>
</ol>
<h2>Previews</h2>
<ul>
  <li>Every entity with a <code>Camera</code> component gets a live preview box: a world-space rectangle showing exactly what that camera frames at the configured game window size, centred on the entity's visual midpoint (its sprite's bounds, not just its transform's corner) at that camera's zoom. It uses the same math as <code>systems::CameraSystem</code> at runtime but is never applied to the editor's own view.</li>
  <li>A separate preview anchored at the world origin shows the letterboxed game window itself, regardless of any camera, dimming everything outside it like a real letterbox.</li>
</ul>
<p>Previous: <a href="navigate-the-viewport.html">Navigate the viewport</a> &middot; Next: <a href="add-entities-and-components.html">Add entities and components</a>.</p>
"""),
    ("add-entities-and-components", "Add entities and components",
     "Create entities, edit their components in the Inspector and organise them in a hierarchy.",
     f"""
<h1>Add entities and components</h1>
<p class="lede">Every entity in the active scene is listed on the right; click one, or click it in the viewport, to edit it live in the Inspector.</p>
{shot("add-component.png", "The Inspector's Add Component list")}
<h2>Steps</h2>
<ol>
  <li>Click <strong>Create Entity</strong> in the Entities panel (or <strong>Create UI Element</strong>, see <a href="arrange-the-ui.html">Arrange the UI</a>).</li>
  <li>Select it. The Inspector shows one collapsible section per component the entity has.</li>
  <li>Use <strong>Add Component</strong> to attach more. Edit fields directly: there is no intermediate copy and no Apply step.</li>
  <li>The trailing <strong>x</strong> on a section removes that component. <strong>Duplicate</strong> copies the entity with everything under it into the same parent; <strong>Delete</strong> removes just that entity and its children become roots.</li>
</ol>
<h2>Component types</h2>
<p>A fixed, explicit set rather than generic reflection; each has a hand-written editor next to its <code>Serializer&lt;T&gt;</code>.</p>
{kv_table([
    ("Name / Transform", "Display name; position, rotation, scale."),
    ("Sprite", "Virtual texture path (picked from known assets, not typed), layer, Y-sort."),
    ("Collider", "Rect or circle shape, drawable on the viewport: <a href='add-a-collider.html'>Add a collider</a>."),
    ("Camera", "Zoom and follow-smoothing; previewed live in the viewport."),
    ("AudioSource", "Clip path (picked from known audio), volume, loop."),
    ("Animation", "Clip path, frame duration and a Play/Stop button. It starts stopped so nothing auto-plays when added."),
    ("PathFollow", "Waypoints authored on the viewport: <a href='follow-a-path.html'>Make an entity follow a path</a>."),
    ("Velocity / Rigidbody", "Plain physics data: linear velocity, mass and drag."),
    ("RenderInfo", "Layer, Y-sort, screen space and sort inheritance. Added automatically, once, whenever a Sprite is."),
    ("UI components", "UIRect, UIButton, UILabel, UICheckbox, UISlider, UIPanel and UILayoutItem."),
], ("Component", "Fields"))}
<h2>Parent entities</h2>
<p>The Entities panel is a tree: roots at the top level, children nested behind a collapsible arrow. A selected child shows a read-only <em>Parent</em> line. A child's transform is relative to its parent, so moving the parent moves everything under it.</p>
{kv_table([
    ("New Child", "Right-click a row: creates a new entity with just a Transform, attached under that row."),
    ("Detach", "Right-click a child row to make it a root again (disabled on a root)."),
    ("Remove", "Right-click a row to destroy the entity and its whole subtree."),
    ("Reparent", "Drag one row onto another to make it that entity's child. Dropping onto itself or its own descendant does nothing."),
], ("Action", "How"))}
<h2>Disable an entity</h2>
<p>The <strong>Disabled</strong> checkbox at the top of the Inspector turns an entity off, along with everything under it. A child disabled only because of its parent shows an <em>(inherited from parent)</em> hint. A closed-eye icon marks a disabled row, and every ancestor of a disabled entity too, so it stays visible when its branch is collapsed. Disabled entities stop drawing collider, camera and waypoint overlays.</p>
<h2>Unsaved changes</h2>
<p>Every field edit, component add/remove, or duplicate/delete marks the scene dirty (the dot next to the Scene selector). Dragging a scalar does not re-resolve assets on each intermediate value, so it stays cheap. Adding a Sprite, or a component with an asset-path field (Animation, AudioSource), resolves it immediately, so a newly picked texture or clip shows up without changing selection.</p>
<p>Previous: <a href="configure-the-game-window.html">Configure the game window</a> &middot; Next: <a href="arrange-the-ui.html">Arrange the UI</a>.</p>
"""),
    ("arrange-the-ui", "Arrange the UI",
     "Create buttons, labels, checkboxes, sliders and panels and let a panel lay them out.",
     f"""
<h1>Arrange the UI</h1>
<p class="lede">Build HUDs and menus without writing TOML. Widgets are assembled through the same <code>ui::Create*</code> functions a game uses, so what you see is what a game gets.</p>
<h2>Create a widget</h2>
<ol>
  <li>Click <strong>Create UI Element</strong> in the Entities panel (disabled until a project is open).</li>
  <li>Pick a type, then fill in its details. <strong>Back</strong> returns to the type list; <strong>Discard</strong> closes without creating.</li>
  <li>In the <strong>Panel Parent</strong> section at the end of every form, choose a panel (default <code>(none)</code>) to create the widget as that panel's child. A panel can parent another panel, which is how you nest layouts.</li>
</ol>
{kv_table([
    ("Button", "Name, position, size, enabled flag, Color / Hover / Pressed colours, optional caption."),
    ("Label", "Text, font, size, alignment and colour. Auto Size fits the rect to the text; turn it off for a fixed size."),
    ("Checkbox", "Box colours, the check colour and whether it starts checked."),
    ("Slider", "Min, Max and starting Value, the track colour and the thumb's Color / Hover / Pressed colours."),
    ("Panel", "Background and border colours, margin, padding, spacing and a layout for its children."),
], ("Type", "Settings"))}
<p>Every type also takes a Name, a Position and a <em>Screen Space</em> flag (on by default, so HUD elements stay put while the viewport pans and zooms). Fonts are picked from those loaded in the Assets panel. Afterwards each widget's components appear as Inspector sections (UIRect, Interactable, UIButton, UILabel, UICheckbox, UISlider, UIPanel) without an <strong>x</strong>: delete the whole entity instead. You can also drag an entity's row onto a panel in the Entities tree to move it in later.</p>
<h2>Panel layouts</h2>
<p>A panel's <strong>Layout</strong> decides where its children go, applied live in the viewport.</p>
{kv_table([
    ("Absolute", "Children keep whatever position and size they have."),
    ("Grid (Rows, Columns)", "Equal cells, filled row by row in sibling order."),
    ("VStack (Rows)", "Equal-height rows, each as wide as the panel, top to bottom."),
    ("HStack (Columns)", "Equal-width columns, each as tall as the panel, left to right."),
], ("Layout", "Behaviour"))}
<p>Slots sit inside the panel's rect, inset by its Margin and Padding, with Spacing between neighbours. A child takes the next slot in sibling order (the order in the Entities tree) and only children with a Transform count; children past the last slot keep their own position.</p>
<h2>Per-child options</h2>
<p>By default a child stretches to fill its slot on both axes. Add a <code>UILayoutItem</code> from the Inspector's Add Component list to change that.</p>
{kv_table([
    ("Fill X / Fill Y", "Stretch the child to the slot on that axis; off keeps the child's own size."),
    ("Align X / Align Y", "Where a non-filling axis sits in the slot: Start, Center or End."),
], ("Field", "What it does"))}
<p>Sprites draw at their texture's size and auto-sized labels fit their text, so neither is ever stretched, even with Fill on, but both can be aligned.</p>
<h2>Good to know</h2>
<ul>
  <li><strong>The layout wins.</strong> A child of a Grid/VStack/HStack panel has its position (and size, unless a UILayoutItem says otherwise) recomputed every frame, so dragging it or editing its Transform won't stick. Edit the panel, or set it to Absolute.</li>
  <li><strong>Duplicate keeps the layout.</strong> Duplicating a child puts the copy in the same panel as its next slot; duplicating a panel copies everything inside it.</li>
  <li><strong>Not interactive here.</strong> The editor only draws UI: buttons, checkboxes and sliders react to clicks only when run in a game.</li>
  <li><strong>Panels sit behind.</strong> New panels draw on a layer behind ordinary widgets so their contents stay visible.</li>
</ul>
<p>Previous: <a href="add-entities-and-components.html">Add entities and components</a> &middot; Next: <a href="import-and-inspect-assets.html">Import and inspect assets</a>.</p>
"""),
    ("import-and-inspect-assets", "Import and inspect assets",
     "Mount asset folders, load assets and preview them in the Asset Inspector.",
     f"""
<h1>Import and inspect assets</h1>
<p class="lede">Assets are addressed by virtual path everywhere in ASGE. The editor gives you a panel for the mounts that back those paths and one for browsing what they resolve to.</p>
{shot("asset-inspector-texture.png", "The Asset Inspector showing a texture")}
<h2>Steps</h2>
<ol>
  <li>In the <strong>Virtual File System</strong> panel, add a mount (a virtual root pointing at a real directory) with the native folder picker.</li>
  <li>In the <strong>Assets</strong> panel, use <strong>Load Asset...</strong> to bring in a texture, animation clip, audio clip or <code>.ttf</code> font. The panel lists everything the editor knows about: assets some entity references, plus those you loaded explicitly (so an unused import still shows up).</li>
  <li>Click an entry to open the <strong>Asset Inspector</strong>: absolute path, mountpoint and file size, plus a preview for the kind.</li>
</ol>
{kv_table([
    ("Texture", "Pixel dimensions and a scaled thumbnail."),
    ("Audio clip", "Duration, size, and working Play/Pause and Rewind controls."),
    ("Animation clip", "Every FrameTable field plus a live, cycling preview of its sliced frames."),
    ("Font", "A sample string rendered with the font's own glyph metrics. Fonts feed a label's Font Path; changing a label's font size re-bakes it."),
], ("Kind", "Preview"))}
<p>Next: <a href="animate-a-sprite.html">Animate a sprite</a> turns a spritesheet texture into a playable clip.</p>
"""),
    ("add-a-collider", "Add a collider",
     "Attach a Collider and draw its rect or circle directly on the viewport.",
     f"""
<h1>Add a collider</h1>
<p class="lede">Collider shapes get their own viewport interaction mode instead of raw number fields.</p>
{shot("collider-added.png", "A Collider component just added to an entity")}
<h2>Steps</h2>
<ol>
  <li>Select an entity and add a <code>Collider</code> from <strong>Add Component</strong>.</li>
  <li>Pick the shape in the <strong>Collider Shape</strong> dropdown (<em>Rect</em> or <em>Circle</em>, default Rect). The editor remembers each shape's own last dimensions per entity, so switching back and forth loses neither.{shot("collider-shape-dropdown.png", "The Collider Shape dropdown")}</li>
  <li>Click <strong>Draw Collider</strong>, then press to anchor the shape and drag to grow it: a rect defines opposite corners, a circle's drag sets its radius from the anchor. Release to commit; <kbd>Esc</kbd> restores the previous shape.{shot("collider-drawn.png", "A collider drawn on the viewport")}</li>
  <li><strong>Reset Collider</strong> zeroes the current shape's dimensions.</li>
</ol>
<p>Drawing edits the live Collider component, so the translucent overlay every collider already has shows it updating in real time.</p>
<p>Previous: <a href="arrange-the-ui.html">Arrange the UI</a> &middot; Next: <a href="animate-a-sprite.html">Animate a sprite</a>.</p>
"""),
    ("animate-a-sprite", "Animate a sprite",
     "Slice a spritesheet into a clip and play it with an Animation component.",
     f"""
<h1>Animate a sprite</h1>
<p class="lede">Turn a spritesheet into something an Animation component can play, without an external tool or hand-written TOML.</p>
{shot("create-clip-modal.png", "The Create Clip modal with a grid overlay")}
<h2>Steps</h2>
<ol>
  <li>Open the spritesheet texture in the <a href="import-and-inspect-assets.html">Asset Inspector</a> and click <strong>Create Clip</strong>.</li>
  <li>In the modal, set <strong>Rows</strong> and <strong>Columns</strong>. A bigger thumbnail shows a live grid overlay and the computed per-cell pixel size.</li>
  <li>Click <strong>Apply</strong>. The editor writes <code>&lt;image&gt;_clip.toml</code> next to the texture (creating or overwriting it), then registers and selects it as a new animation clip.</li>
  <li>Add an <code>Animation</code> component to an entity, choose the clip path and frame duration, and press <strong>Play</strong>.</li>
</ol>
{shot("animation-clip-preview.png", "The animation clip preview cycling through frames")}
<p>A clip records which texture it slices (<code>m_OriginAsset</code>), so once one exists the texture's button becomes <strong>Open Clip</strong> and jumps straight to it. Importing a <code>.toml</code> by hand via Load Asset... that doesn't identify its own texture this way is rejected rather than imported half-configured.</p>
<p>Previous: <a href="add-a-collider.html">Add a collider</a> &middot; Next: <a href="follow-a-path.html">Make an entity follow a path</a>.</p>
"""),
    ("follow-a-path", "Make an entity follow a path",
     "Author PathFollow waypoints by clicking on the viewport and preview the spline.",
     f"""
<h1>Make an entity follow a path</h1>
<p class="lede">A PathFollow's waypoints are placed on the viewport rather than typed as numbers.</p>
{shot("pathfollow-waypoints.png", "Waypoints placed on the viewport")}
<h2>Steps</h2>
<ol>
  <li>Add a <code>PathFollow</code> component to the entity.</li>
  <li>Click <strong>Select Waypoints</strong> in its Inspector section. The viewport becomes a click-to-place surface.</li>
  <li>Edit the path with the mouse, then click <strong>End Selection</strong> to commit and leave the mode.</li>
</ol>
{kv_table([
    ("Click", "Append a waypoint at the cursor's world position."),
    ("Drag a point", "Move it (a fixed screen-pixel pick radius, so it is grabbable at any zoom)."),
    ("Right-click a point", "Remove it."),
    ("<kbd>Esc</kbd>", "Discard everything placed this session, restoring what was there before."),
    ("End Selection", "Commit and exit the mode."),
], ("Input", "Effect"))}
{shot("pathfollow-spline.png", "The Catmull-Rom spline overlay")}
<p>Waypoints stay visible as pin markers after selection ends. Outside edit mode, a real Catmull-Rom curve, the same interpolation <code>PathFollowingSystem</code> uses at runtime, overlays the straight point-to-point lines, sampled at the component's own <em>Resolution</em>, so it previews the path the entity will actually follow.</p>
<p>Previous: <a href="animate-a-sprite.html">Animate a sprite</a>.</p>
"""),
]

INDEX = f"""
<h1>Level editor</h1>
<p class="lede"><code>asge-editor</code> is a native ImGui-based editor for authoring ASGE scenes visually: place and configure
entities, wire up components, manage textures, animations and audio, and organise a project's scenes, without hand-writing TOML.
It is a plain executable built from the same CMake tree as the engine.</p>
{shot("hero.png", "The asge-editor window")}
<p>Native installers for Windows (<code>.exe</code>), Linux (<code>.deb</code>) and macOS (<code>.dmg</code>) ship with every
tagged release: see <a href="../releases.html">Releases</a>.</p>
<h2>Build from source</h2>
{code_card("build just the editor", "bash", BUILD_SNIPPET)}
<p>The editor never links against or depends on <code>Game</code>/<code>IGameState</code>: it drives <code>asge::ecs::Registry</code>,
<code>SceneManager</code>, <code>AssetManager</code> and <code>IRenderer</code> directly. See
<a href="../engine/getting-started.html">Getting Started</a> for the full build setup.</p>
<h2>Guide</h2>
<div class="grid">
""" + "\n".join(
    f'<a class="card" href="{slug}.html"><h3>{html.escape(title)}</h3><p>{html.escape(summary)}</p></a>'
    for slug, title, summary, _ in TASKS
) + "\n</div>\n"

PAGES = {"editor/index.html": register(Page(
    "editor/index.html", "Editor overview", "editor", "Guide",
    "asge-editor is a native ImGui-based editor for authoring ASGE scenes visually.",
))}
for _slug, _title, _summary, _ in TASKS:
    PAGES[f"editor/{_slug}.html"] = register(Page(f"editor/{_slug}.html", _title, "editor", "Guide", _summary))


def build() -> dict[str, str]:
    out = {"editor/index.html": render_page(page=PAGES["editor/index.html"], body=INDEX)}
    for slug, _title, _summary, body in TASKS:
        path = f"editor/{slug}.html"
        out[path] = render_page(page=PAGES[path], body=body)
    return out
