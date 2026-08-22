# Networking — Phase 1

*Priority: Tier 4 — deferred. MVP is single-player; revisit only if multiplayer
becomes an actual goal, not speculatively. See [README](../README.md).
Other phases: [Phase 2](../phase-2/12-networking.md) (prediction/interpolation),
[Phase 3](../phase-3/12-networking.md) (dedicated servers, rollback).*

## Goals
LAN multiplayer — the minimum that makes "more than one player" real, if this
system ever gets built at all.

## Current state
No networking code exists anywhere in the engine, and nothing currently depends on
it. [00-overview](../00-overview.md) doesn't list multiplayer as a goal.

## Design notes
This entire system, including this "Phase 1" scoping, is speculative — written so
that *if* multiplayer becomes an actual goal, there's a starting scope already
thought through, not because any part of it is scheduled. Don't start building
against this doc without first confirming multiplayer is an actual, current goal.

## Tasks
- [ ] Client/server architecture — even LAN-only needs a decision on
      authoritative-server vs. peer-to-peer
- [ ] Basic state replication for [03-ECS](03-entity-component-system.md) entities
      over a LAN connection

## Deliverables
- Two instances of an example on the same LAN seeing each other's entities move

## Explicitly out of scope for Phase 1
Everything beyond LAN — see [Phase 2](../phase-2/12-networking.md) and
[Phase 3](../phase-3/12-networking.md). This whole system stays out of scope
entirely unless multiplayer becomes an actual goal.
