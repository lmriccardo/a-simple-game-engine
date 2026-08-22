# Documentation — Phase 1

*Priority: Tier 4, but continuous — light and ongoing rather than a single pass
bolted on at the end. See [README](../README.md). Other phases:
[Phase 2](../phase-2/14-documentation.md) (beginner tutorials),
[Phase 3](../phase-3/14-documentation.md) (full engine manual).*

## Goals
A place for docs to live, and the habit of keeping them current — not a
comprehensive reference yet.

## Current state
This roadmap itself (`docs/roadmap/`) is the documentation that currently exists,
plus inline doc comments on public headers (see `CLAUDE.md`'s doc-comment
guideline: every function gets a short comment on its declaration). There's no
generated API reference or docs site yet.

## Design notes
- **Same shape as [13-Optimization](13-optimization.md): continuous, not a
  terminal step.** Keep doc comments and this roadmap current as code changes,
  rather than deferring all documentation to a single pass at the end — that pass
  tends to never actually happen, or happens against stale code.
- **"Setup docs site" means picking a generator that can consume the doc comments
  already being written** (e.g. Doxygen), not authoring new prose from scratch —
  the raw material is already accumulating per `CLAUDE.md`'s guideline.

## Tasks
- [ ] Pick and configure a docs-site generator that consumes existing doc comments
- [ ] Publish the generated reference somewhere reachable (even just a build
      artifact/local output, to start)
- [ ] Keep this roadmap's per-system docs current as each system's actual state
      changes — already the working pattern established across every phase file
      in this directory

## Deliverables
- A generated API reference from existing doc comments, even if minimal
- This roadmap continuing to reflect actual code state as it's extended

## Explicitly out of scope for Phase 1
Beginner tutorials ([Phase 2](../phase-2/14-documentation.md)); a full engine
manual ([Phase 3](../phase-3/14-documentation.md)) — both need more finished
systems to document than currently exist.
