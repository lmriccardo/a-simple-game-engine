"""Components reference: every component the engine ships, grouped on a few pages.

Field names, defaults and TOML table names come from src/ASGE/Game/Components and
src/ASGE/Core/ECS. Each component is a dict: name, header, toml, systems, text, fields.
"""
import html

from _shared import Page, register, render_page
from content.theory_systems import src

GROUP = "Components"

# (slug, title, summary, intro, [component, ...]); fields = [(field, default, meaning)]
CATEGORIES = [
    ("core", "Core components", "Name, Transform, Hierarchy and the Disabled marker: what almost every entity has.",
     "These describe an entity's identity and place in the world. Almost every scene entity carries a Transform.", [
        dict(name="Name", header="Game/Components/Name.hpp", toml="Name", systems="none (editor display)",
             text="A freeform display label. Empty means unnamed.",
             fields=[("m_Name", "empty", "The label shown in the editor's Entities tree.")]),
        dict(name="Transform", header="Game/Components/Transform.hpp", toml="Transform",
             systems="TransformPropagationSystem",
             text="Position, rotation and scale. Local values are relative to the parent (or absolute for a root); world values are what rendering and collision read, recomputed from the local ones by TransformPropagationSystem. Change a local value, then set <code>m_Dirty</code> to request a recompute.",
             fields=[("m_LocalCoordinates", "(0, 0)", "Position relative to the parent."),
                     ("m_LocalScale", "(1, 1)", "Scale relative to the parent."),
                     ("m_LocalRotation", "0", "Rotation in radians, relative to the parent."),
                     ("m_Locked", "false", "The editor refuses to move this entity; the engine does not enforce it."),
                     ("m_WorldCoordinates / m_WorldScale / m_WorldRotation", "derived", "Absolute values, written by the system. Not saved."),
                     ("m_Dirty", "false", "Request a world recompute.")]),
        dict(name="Hierarchy", header="Core/ECS/Hierarchy.hpp", toml="Hierarchy", systems="TransformPropagationSystem",
             text="Parent/child links. Children form a linked list, so an entity stores its parent, first and last child, and previous and next sibling. Use the helpers in the header rather than writing the links by hand: they refuse to create a cycle.",
             fields=[("m_Parent", "Null", "The parent, or Null for a root."),
                     ("m_FirstChild / m_LastChild", "Null", "Ends of this entity's children list."),
                     ("m_NextSibling / m_PrevSibling", "Null", "Neighbours under the same parent.")]),
        dict(name="Disabled", header="Core/ECS/Markers.hpp", toml="Disabled", systems="Registry views",
             text="A marker with no fields. Disabled entities are skipped by every view unless it asks for <code>IncludeDisabled()</code>. Disabling an entity disables everything under it; <code>Registry::IsDisabled</code> reports the derived state.",
             fields=[]),
    ]),
    ("rendering", "Rendering and animation components", "Sprite, RenderInfo, Camera and Animation.",
     "What gets drawn, in what order, and from where.", [
        dict(name="Sprite", header="Game/Components/Sprite.hpp", toml="Sprite", systems="RenderSystem",
             text="Draws a texture at the entity's world transform. The texture is loaded from a virtual path when assets are resolved.",
             fields=[("m_VirtualPath", "empty", "VFS path of the texture."),
                     ("m_SourceRect", "none", "Sub-region to draw; none draws the whole texture (used for spritesheets)."),
                     ("m_Tint", "white", "Colour modulation; the alpha channel fades the sprite."),
                     ("m_Texture", "null", "Resolved texture asset. Runtime only.")]),
        dict(name="RenderInfo", header="Game/Components/RenderInfo.hpp", toml="RenderInfo", systems="RenderSystem",
             text="Decides draw order. Higher layers draw on top; within a layer, Y-sort orders by bottom edge so things lower on screen draw in front.",
             fields=[("m_Layer", "0", "Draw-order bucket."),
                     ("m_YSort", "false", "Sort by bottom-edge Y within the layer."),
                     ("m_ScreenSpace", "false", "Draw in screen space on top of the world; propagates to children."),
                     ("m_InheritSortFromParent", "false", "Adopt the nearest ancestor's layer and Y-sort."),
                     ("m_LocalOrder", "0", "Tie-break among entities sharing a sort owner; -1 draws behind the parent."),
                     ("m_SortOffsetY", "0", "Added to the Y-sort key in world units.")]),
        dict(name="Camera", header="Game/Components/Camera.hpp", toml="Camera", systems="CameraSystem",
             text="Makes an entity a camera that follows its Transform. The active camera is held in the <code>ActiveCamera</code> resource.",
             fields=[("m_Zoom", "1", "Uniform zoom."),
                     ("m_Smoothing", "0", "Catch-up rate toward the target; 0 snaps immediately, higher eases in faster.")]),
        dict(name="Animation", header="Game/Components/Animation.hpp", toml="Animation", systems="AnimationSystem",
             text="Plays a spritesheet clip by writing each frame into the entity's <code>Sprite::m_SourceRect</code>. The frame list lives in a shared, VFS-loaded FrameTable, so many entities can play one clip. While playing, the Animation owns the Sprite's source rect and restores it afterwards.",
             fields=[("m_ClipPath", "empty", "Virtual path of the clip's FrameTable TOML."),
                     ("m_Clips", "empty", "Named clips (idle, run, ...) mapped to clip paths; switch with <code>PlayClip</code>."),
                     ("m_ClipFrameDurations", "empty", "Optional per-clip frame duration in seconds, keyed by clip name."),
                     ("m_FrameDuration", "0.1", "Seconds each frame is shown, unless the FrameTable gives a frame its own duration."),
                     ("m_PlaybackSpeed", "1", "Playback-rate multiplier; non-positive stops advancing."),
                     ("m_Loop", "true", "Wrap to frame 0 at the end instead of stopping."),
                     ("m_StartPlaying", "true", "Whether a freshly loaded scene starts it playing."),
                     ("m_Playing / m_CurrentFrame / m_ElapsedTime", "runtime", "Live playback state. Not saved.")]),
    ]),
    ("physics", "Physics components", "Collider, Rigidbody and Velocity.",
     "Collision shapes and motion. See the <a href='../theory/physics.html'>Physics theory page</a> for how they are used.", [
        dict(name="Collider", header="Game/Components/Collider.hpp", toml="Collider", systems="CollisionSystem",
             text="A rectangle or circle, offset from the entity's Transform. A Solid collider is pushed out of what it overlaps; a Trigger never blocks and fires signals instead. Layer and mask decide who can collide with whom.",
             fields=[("m_LocalBounds", "default Rect", "A <code>Rect</code> or <code>Circle</code> plus a local offset."),
                     ("m_Resolution", "Solid", "<code>Solid</code> (push-out), <code>Trigger</code> (sensor) or <code>Unknown</code> (ignored)."),
                     ("m_Layer", "1", "Bit(s) this collider occupies."),
                     ("m_Mask", "all bits", "Layers it is willing to collide with."),
                     ("m_OnEnter / m_OnStay / m_OnExit", "lazy signals", "Per-entity trigger signals, called with the other entity.")]),
        dict(name="Rigidbody", header="Game/Components/Rigidbody.hpp", toml="Rigidbody", systems="PhysicsSystem",
             text="Makes an entity respond to physics.",
             fields=[("m_Mass", "1", "Mass."),
                     ("m_AffectedByGravity", "true", "Whether gravity accelerates it.")]),
        dict(name="Velocity", header="Game/Components/Velocity.hpp", toml="Velocity", systems="MovementSystem, PhysicsSystem",
             text="Linear velocity in world units per second.",
             fields=[("m_DX", "0", "Change in X per second."), ("m_DY", "0", "Change in Y per second.")]),
    ]),
    ("audio", "Audio components", "AudioSource.",
     "Sound playback. See the <a href='../theory/audio.html'>Audio theory page</a>.", [
        dict(name="AudioSource", header="Game/Components/AudioSource.hpp", toml="AudioSource", systems="AudioSystem",
             text="Plays an audio clip on the machine's output device. Start it with <code>PlayAudioSource</code>; AudioSystem consumes the restart request once.",
             fields=[("m_VirtualClipPath", "empty", "VFS path of the clip."),
                     ("m_Volume", "1", "This source's own gain."),
                     ("m_Loop", "false", "Restart the clip when it runs out."),
                     ("m_Playing", "false", "Whether AudioSystem advances playback."),
                     ("m_Restart", "false", "Set by <code>PlayAudioSource</code> to force an immediate (re)start."),
                     ("m_Clip / m_Stream", "null", "Resolved clip and its device stream. Runtime only.")]),
    ]),
    ("path", "Path components", "Path and PathFollow.",
     "A route and a follower are two entities. The Path lives on the route entity; the follower is its child and finds the Path through its parent, so the scene file needs no entity references. Authoring: <a href='../../editor/follow-a-path.html'>Make an entity follow a path</a>.", [
        dict(name="Path", header="Game/Components/Path.hpp", toml="Path", systems="asset::Resolver&lt;Path&gt;",
             text="A route through its waypoints, local to the route entity's Transform: a smooth Catmull-Rom curve, or straight segments with sharp corners when <code>m_Linear</code> is set.",
             fields=[("m_Waypoints", "empty", "Control points, in order, local to the entity."),
                     ("m_Resolution", "32", "Arc-length samples per segment. Unused when <code>m_Linear</code> is set."),
                     ("m_Linear", "false", "Straight segments between waypoints instead of a curve."),
                     ("m_Loop", "false", "Wrap a follower to the start at the end instead of stopping."),
                     ("m_Path", "built", "The <code>CatmullRomSpline</code> built from the waypoints. Runtime only.")]),
        dict(name="PathFollow", header="Game/Components/PathFollow.hpp", toml="PathFollow", systems="PathFollowingSystem",
             text="Moves its entity along the parent's Path, writing the sampled point plus the offset into its local transform. Followers with no parent, or a parent without a Path, are skipped.",
             fields=[("m_Speed", "1", "Units per second along the path."),
                     ("m_OffsetH / m_OffsetV", "0", "Offset added to the sampled point."),
                     ("m_AlignRotation", "true", "Turn to face the path tangent; off leaves rotation alone."),
                     ("m_Traveled / m_Finished", "runtime", "Distance walked and whether a non-looping path ended. Not saved."),
                     ("m_Heading", "(1, 0)", "Unit path tangent in the parent's space, updated every frame it moves (also when <code>m_AlignRotation</code> is off), e.g. to pick an animation clip. Changes abruptly at corners in linear mode. Not saved."),
                     ("m_Paused", "false", "Set by <code>PathFollowPause</code> and cleared by <code>PathFollowResume</code>; the system skips the follower and it resumes from the same distance. Not saved.")]),
    ]),
    ("ui", "UI components", "UIRect, Interactable, UIButton, UILabel, UICheckbox, UISlider, UIProgressBar, UIPanel, UILayoutItem, UIDebugOverlay.",
     "A UI widget is an ordinary entity: a <code>UIRect</code> footprint plus the components for what it does. Build them with the <code>asge::game::ui::Create*</code> functions rather than by hand. See the <a href='../theory/ui.html'>UI theory page</a> and <a href='../../editor/arrange-the-ui.html'>Arrange the UI</a>.", [
        dict(name="UIRect", header="Game/Components/UI/Common.hpp", toml="UIRect", systems="RenderSystem, UILayoutSystem",
             text="The rectangular footprint of a widget, positioned by the entity's Transform (top-left, like Sprite).",
             fields=[("m_Size", "(80, 24)", "Width and height in Transform units.")]),
        dict(name="Interactable", header="Game/Components/UI/Common.hpp", toml="Interactable", systems="UIInteractionSystem",
             text="Pointer state for a widget with a UIRect. Recomputed every frame; <code>m_Clicked</code> is true only on the frame a press completes while still hovered.",
             fields=[("m_Enabled", "true", "False opts the widget out of hit-testing (greyed out)."),
                     ("m_Hovered / m_Held / m_Clicked", "false", "Runtime state. Not saved.")]),
        dict(name="UIButton", header="Game/Components/UI/UIButton.hpp", toml="UIButton", systems="UIInteractionSystem, RenderSystem",
             text="A clickable button that changes colour with hover and press.",
             fields=[("m_Colors", "light button palette", "Normal, hover and pressed fill colours."),
                     ("m_OnClick", "signal", "Fired by UIInteractionSystem on click.")]),
        dict(name="UILabel", header="Game/Components/UI/UILabel.hpp", toml="UILabel", systems="RenderSystem",
             text="Text drawn with a baked font. A label with text but no font path is rejected with <code>UIWidgetError::TextWithNoFont</code>.",
             fields=[("m_Text", "\"Placeholder\"", "The string."),
                     ("m_FontPath", "empty", "VFS path of the font."),
                     ("m_FontPixelHeight", "16", "Font size in pixels."),
                     ("m_Color", "black", "Text colour."),
                     ("m_Align / m_VerticalAlign", "Left / Center", "Horizontal (<code>Left</code>, <code>Center</code>, <code>Right</code>) and vertical (<code>Top</code>, <code>Center</code>, <code>Bottom</code>) alignment."),
                     ("m_AutoSize", "true", "Fit the rect to the text."),
                     ("m_WordWrap", "false", "Wrap to the rect's width; needs AutoSize off.")]),
        dict(name="UICheckbox", header="Game/Components/UI/UICheckbox.hpp", toml="UICheckbox", systems="UIInteractionSystem, RenderSystem",
             text="A toggle box.",
             fields=[("m_BoxColors", "light button palette", "Box fill colours by state."),
                     ("m_CheckColor", "black", "Colour of the check mark."),
                     ("m_Checked", "false", "Current state."),
                     ("m_OnToggled", "signal&lt;bool&gt;", "Fires with the new state.")]),
        dict(name="UISlider", header="Game/Components/UI/UISlider.hpp", toml="UISlider", systems="UIInteractionSystem, RenderSystem",
             text="A draggable thumb on a track.",
             fields=[("m_Min / m_Max", "0 / 1", "Range."),
                     ("m_Value", "0", "Current value."),
                     ("m_TrackColor", "gray", "Track colour."),
                     ("m_ThumbColor", "light button palette", "Thumb colours by state."),
                     ("m_OnValueChanged", "signal&lt;float&gt;", "Fires with the new value while dragging.")]),
        dict(name="UIProgressBar", header="Game/Components/UI/UIProgressBar.hpp", toml="UIProgressBar", systems="RenderSystem",
             text="A bar that fills between a minimum and a maximum. Advance it with <code>Step(bar)</code> or <code>Step(bar, amount)</code>: the value is clamped, then <code>m_OnValueChange</code> fires, and <code>m_OnMaxReached</code> fires once it is full.",
             fields=[("m_Direction", "LeftToRight", "<code>LeftToRight</code>, <code>RightToLeft</code>, <code>TopToBottom</code> or <code>BottomToTop</code>."),
                     ("m_Min / m_Max / m_Start", "0 / 1 / 0", "Range and starting value."),
                     ("m_DefaultStep", "0.1", "Amount added by <code>Step(bar)</code>."),
                     ("m_BackgroundColor / m_FillColor / m_BorderColor", "dark gray / magenta / light gray", "Colours."),
                     ("m_CurrValue / m_IsAtMax", "runtime", "Current value and whether it is full. Not saved."),
                     ("m_OnValueChange / m_OnMaxReached", "signals", "Value change (with the new value) and reaching the maximum.")]),
        dict(name="UIPanel", header="Game/Components/UI/UIPanel.hpp", toml="UIPanel", systems="UILayoutSystem, RenderSystem",
             text="A container that draws a background and lays out its children (the entities under it in the Hierarchy).",
             fields=[("m_Layout", "Absolute", "<code>Absolute</code> (children keep their position), <code>Grid</code> (rows, cols), <code>VStack</code> (rows) or <code>HStack</code> (cols)."),
                     ("m_Background", "shadow black", "Fill colour."),
                     ("m_Border / m_BorderColor", "true / light gray", "Draw a border, and its colour."),
                     ("m_Padding / m_Margin / m_Spacing", "(0, 0)", "Inner padding, outer margin and gap between children.")]),
        dict(name="UILayoutItem", header="Game/Components/UI/UILayoutItem.hpp", toml="UILayoutItem", systems="UILayoutSystem",
             text="Optional, on a child of a panel: how it uses its layout slot.",
             fields=[("m_FillX / m_FillY", "true", "Stretch to fill the slot on that axis."),
                     ("m_AlignX / m_AlignY", "Start", "<code>Start</code>, <code>Center</code> or <code>End</code> within the slot.")]),
        dict(name="UIDebugOverlay", header="Game/Components/UI/UIDebugOverlay.hpp", toml="(never saved)", systems="UIDebugOverlaySystem",
             text="Marks a UILabel as a live min/avg/max FPS and entity-count readout. Create it with <code>CreateDebugOverlay</code>.",
             fields=[("m_RefreshInterval", "0.25", "Seconds between text updates; also the FPS window."),
                     ("m_Accumulated / m_Frames / m_MinDelta / m_MaxDelta", "runtime", "Counters for the current window.")]),
    ]),
]


def _slug(name: str) -> str:
    return name.lower()


def _component(c: dict) -> str:
    rows = "".join(f"<tr><td><code>{f}</code></td><td>{d}</td><td>{m}</td></tr>" for f, d, m in c["fields"])
    table = (f"<table><tr><th>Field</th><th>Default</th><th>Meaning</th></tr>{rows}</table>"
             if rows else "<p>No fields.</p>")
    toml = c["toml"] if c["toml"].startswith("(") else f'<code>[{c["toml"]}]</code>'
    return (
        f'<h2 id="{_slug(c["name"])}">{c["name"]}</h2>\n'
        f'<p>{c["text"]}</p>\n'
        f'<p>TOML table: {toml} &middot; Used by: {c["systems"]} &middot; Source: {src(c["header"])}</p>\n'
        f'{table}\n'
    )


INDEX_PAGE = register(Page(
    "engine/components/index.html", "Components", "engine", GROUP,
    "Every component the ASGE engine provides, what it does and its fields.",
))
PAGES = {
    slug: register(Page(f"engine/components/{slug}.html", title, "engine", GROUP, summary))
    for slug, title, summary, _, _ in CATEGORIES
}

INDEX_BODY = """
<h1>Components</h1>
<p class="lede">An entity in ASGE is just an id; its components are the data that gives it behaviour. These are all the
components the engine ships. Each is plain data, saved to a scene as a TOML table of the same name, and acted on by a
system. Mix them freely: a sprite that animates and collides is an entity with a Transform, Sprite, Animation and Collider.</p>
<p>New to the model? Read <a href="../theory/ecs.html">Entity-Component-System</a> first. Add components in the
<a href="../../editor/add-entities-and-components.html">level editor</a> or from code through the <code>Registry</code>.</p>
""" + "\n".join(
    f'<h2><a href="{slug}.html">{html.escape(title)}</a></h2><p>{intro}</p><p>'
    + " &middot; ".join(f'<a href="{slug}.html#{_slug(c["name"])}"><code>{c["name"]}</code></a>' for c in comps)
    + "</p>"
    for slug, title, _, intro, comps in CATEGORIES
)


def build() -> dict[str, str]:
    out = {"engine/components/index.html": render_page(page=INDEX_PAGE, body=INDEX_BODY)}
    for slug, title, _, intro, comps in CATEGORIES:
        body = f'<h1>{title}</h1>\n<p class="lede">{intro}</p>\n' + "\n".join(_component(c) for c in comps)
        out[f"engine/components/{slug}.html"] = render_page(page=PAGES[slug], body=body)
    return out
