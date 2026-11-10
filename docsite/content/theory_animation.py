"""Animation theory page: concept first, then how ASGE does it.

Names are checked against src/ASGE/Game (Animator, AnimatorSystem, AnimationStateMachine,
AnimationClipSet). Imported by theory_systems.py and listed in its PAGES.
"""
from _shared import code_card
from content.diagrams import ANIMATION as ANIMATION_DIAGRAM

ANIMATION_BODY = """
<h1>Animation</h1>
<p class="lede">A 2D character is a spritesheet and a rule for which part of it to show. Animation is that rule, kept
out of gameplay code.</p>

<h2>The concept</h2>
<p>Sprite animation is flipbook playback: a list of frames (rectangles on a spritesheet) shown one after another, each for
a short time. Playing one clip is easy. The hard part is <em>choosing</em> the clip: idle becomes run when the player
moves, run becomes attack on a button press, attack returns to idle when it finishes. Written as <code>if</code>
chains in gameplay code, that logic spreads and breaks.</p>

<h3>State machines</h3>
<p>Engines model it as a <strong>state machine</strong>: each state plays one clip, and <strong>transitions</strong> between
states fire on conditions. Gameplay code stops saying "play run" and only reports facts; the machine decides. The facts come
in two kinds:</p>
<ul>
  <li><strong>Flags</strong> are persistent conditions that stay true until changed ("running").</li>
  <li><strong>Triggers</strong> are one-shot events ("attack pressed") that wait until a transition consumes them, so a press is never lost or handled twice.</li>
</ul>
<h3>Ending, priority and direction</h3>
<p>A clip needs a rule for its last frame: loop, freeze on it, or give the sprite back its original image. Transitions are
tested in priority order, so "hurt" can override everything else. And a character that walks in several directions needs
one set of frames per direction, chosen from the way it is facing.</p>

<h2>How ASGE does it</h2>
""" + ANIMATION_DIAGRAM + """
<h3>Three assets, one component</h3>
<p>The data is split so each piece is reused independently:</p>
<ul>
  <li>A <code>FrameTable</code> (TOML) is the list of source rects for one clip, as described on <a href="assets.html">Assets</a>.</li>
  <li>An <code>asset::AnimationStateMachine</code> (<code>.animsm</code> XML) declares the flags, triggers, clip names, states and transitions. Up to 32 flags and 32 triggers, each a bit of a mask.</li>
  <li>An <code>asset::AnimationClipSet</code> (<code>.animset</code> XML) names its machine and binds each clip name to its frame table(s) and a frame duration.</li>
</ul>
<p>A <code>components::Animator</code> stores only the clip set path, a playback speed and a start-playing flag; those are
what a scene file saves. <code>AssetManager::ResolveAssets</code> loads the set, its machine and the frame tables once,
shared by every animator that names them. Until then <code>SetFlag</code> and <code>Fire</code> do nothing and return false.
Everything else on the component (state, frame, flags, triggers, heading) is runtime state.</p>
""" + code_card("player.animsm", "xml", """<AnimStateMachine initial="idle">
  <Flags><Flag name="running"/></Flags>
  <Triggers><Trigger name="attack"/><Trigger name="hurt"/></Triggers>
  <Clips><Clip name="idle"/><Clip name="run"/><Clip name="attack"/><Clip name="hurt"/></Clips>

  <State name="idle" clip="idle" end="loop">
    <Transition to="hurt"   trigger="hurt" priority="10"/>
    <Transition to="attack" trigger="attack"/>
    <Transition to="run"    when="running"/>
  </State>
  <State name="attack" clip="attack" end="hold">
    <Transition to="run"  atEnd="true" when="running" priority="1"/>
    <Transition to="idle" atEnd="true"/>
  </State>
  <!-- run and hurt omitted -->
</AnimStateMachine>""") + """
<h3>What a frame of AnimatorSystem does</h3>
<p><code>systems::AnimatorSystem</code> visits every entity with an <code>Animator</code> and a <code>Sprite</code>. For
each one, in order:</p>
<ol>
  <li><strong>Update the heading</strong> from a moving <code>PathFollow</code>, else a nonzero <code>Velocity</code>, else keep the last one.</li>
  <li><strong>Start</strong> on first sight: enter the machine's initial state, or stay stopped if <code>m_StartPlaying</code> is false.</li>
  <li><strong>Advance time</strong> by <code>dt</code> times the playback speed, stepping through frames whose duration (the frame table's own, else the clip's) has elapsed.</li>
  <li><strong>Try transitions</strong> when something changed: the first transition of the current state, highest priority first, whose conditions hold is taken and its trigger consumed.</li>
  <li><strong>Write the frame</strong> into <code>Sprite::m_SourceRect</code>.</li>
</ol>
<p>The animator never draws anything: <code>RenderSystem</code> just sees whatever source rect was last written.
<code>RenderPipeline</code> runs <code>AnimatorSystem</code>, <code>CameraSystem</code> then <code>RenderSystem</code> in that order.</p>

<h3>Transitions and end modes</h3>
<p>A transition can require flags to be set (<code>when="running"</code>) or clear (<code>when="!running"</code>), a pending
trigger, and optionally that the clip has finished (<code>atEnd</code>). A state's <code>end</code> picks what happens on its
last frame:</p>
<ul>
  <li><code>loop</code> wraps to the first frame.</li>
  <li><code>hold</code> stays on the last frame and marks the animator finished; an <code>atEnd</code> transition can then leave.</li>
  <li><code>restore</code> gives the sprite its original source rect back.</li>
</ul>
<p>Each finished playback emits <code>events::OnAnimationFinished()</code> with the entity, a <a href="events-signals.html">signal</a>
gameplay code can connect to. While playing, the animator owns the sprite's source rect; <code>Stop</code> releases it and
<code>Resume</code> takes it back.</p>

<h3>Directional clips</h3>
<p>Instead of a <code>path</code>, a clip of the clip set can hold 1 to 8 <code>&lt;Variant path&gt;</code> children, one frame
table per direction, spread evenly clockwise from north: four variants are N/E/S/W, eight add the diagonals. All variants
share the clip's frame duration. <code>Animator::m_Heading</code> (a screen-space vector, south until set) picks the nearest
one; a small hysteresis stops a heading sitting on a boundary from flickering. A heading change swaps the frame table
<em>without restarting</em> the clip, so the walk cycle keeps its frame. Set the heading by hand with
<code>components::SetHeading</code>; a zero vector is ignored, so an entity that stops keeps facing the way it moved.</p>
""" + code_card("hero.animset", "xml", """<AnimClipSet machine="anim/humanoid.animsm">
  <Clip name="run" frameDuration="0.10">
    <Variant path="anim/hero_run_n.toml"/>
    <Variant path="anim/hero_run_e.toml"/>
    <Variant path="anim/hero_run_s.toml"/>
    <Variant path="anim/hero_run_w.toml"/>
  </Clip>
  <Clip name="death" path="anim/hero_death.toml" frameDuration="0.10"/>
</AnimClipSet>""") + code_card("animator.cpp", "cpp", """SetFlag( animator, "running", moving );          // persistent: keeps its value until set again
if ( attackPressed ) Fire( animator, "attack" ); // one-shot: consumed by the transition that listens""") + """
<h2>See also</h2>
<p><a href="../examples/animator_demo.html">Animator demo</a> &middot; <a href="assets.html">Assets</a> &middot;
<a href="../components/index.html">Components reference</a></p>
"""
