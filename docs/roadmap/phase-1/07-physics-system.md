# Physics System — Phase 1

*Priority: Tier 2 — first system that makes gameplay feel like gameplay.
See [README](../README.md). Other phases:
[Phase 2](../phase-2/07-physics-system.md) (CCD, raycasts, character controller),
[Phase 3](../phase-3/07-physics-system.md) (soft body, vehicle physics).*

## Goals
Basic 2D collision and gravity — enough that entities can bump into each other and
fall, without a general-purpose 2D physics engine.

## Current state
No physics code exists anywhere in the engine yet. What it hooks into already does:
[03-ECS Phase 1](../phase-1/03-entity-component-system.md) already stubs `Collider`
(shape + bounds) and `Rigidbody` (velocity/mass) as placeholder components
specifically for this system to fill in — Phase 1 here is what gives those
components an actual system operating on them.

## Design notes
- **AABB only, not arbitrary shapes.** Matches this engine's 2D scope
  ([00-overview](../00-overview.md)) — axis-aligned bounding boxes cover the common
  2D gameplay case (platformers, top-down) without the complexity of a general
  narrowphase for rotated/convex shapes.
- **A system, not a class hierarchy.** Following [03-ECS](03-entity-component-system.md)'s
  own convention: a `PhysicsSystem` is a plain function operating on a view of
  `Transform`+`Collider`+`Rigidbody`, the same shape as the existing
  `MovementSystem`/`RenderSystem`, not a `Rigidbody` base class with virtual methods.
- **Gravity as a constant force applied to `Rigidbody.m_Velocity`**, integrated the
  same way `MovementSystem` already integrates velocity into `Transform.m_Position`
  — reusing that integration step rather than building a second one.

## Tasks
- [ ] AABB collision detection (`Collider` vs `Collider`, from
      [03-ECS](../phase-1/03-entity-component-system.md)'s existing component)
- [ ] Gravity: constant acceleration applied to `Rigidbody.m_Velocity` each frame
- [ ] Collision response: basic resolution (stop/push out on overlap) — not full
      impulse-based rigid body dynamics
- [ ] `PhysicsSystem` as a plain function, following `MovementSystem`'s existing shape

## Deliverables
- `PhysicsSystem` detecting AABB overlaps between entities with `Collider`
- Gravity affecting any entity with `Rigidbody`
- At least one example demonstrating a falling/colliding entity

## Explicitly out of scope for Phase 1
Continuous collision detection, raycasts, character controller
([Phase 2](../phase-2/07-physics-system.md)); soft body and vehicle physics
([Phase 3](../phase-3/07-physics-system.md)); broadphase beyond a naive all-pairs
check (revisit only if [13-Optimization](13-optimization.md) profiling shows it's
needed — no entity count in this engine justifies it yet).
