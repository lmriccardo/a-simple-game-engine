---
name: release-notes
description: Write a GitHub release description (markdown) for an ASGE version tag. Use when asked for release notes, a release description, or "write up vX.Y.Z for GitHub".
---

Output is a **markdown code block** the user copy-pastes into GitHub's release
editor — not a file, not an Artifact. Keep the whole thing roughly the length
of `template.md` in this folder; if a section is running longer than the
template's, you're writing changelog, not release notes.

## Gather material

```
git log <prev-tag>..<new-tag> --oneline      # commits in this release
git show -s --format=%B <commit>             # full body for each feat/fix commit
git log -1 <new-tag> --format=%ai            # release date
git log <prev-tag>..<new-tag> --format=%an | sort -u   # contributors (usually just lmriccardo)
```

Also check `docs/roadmap/*.md` for a milestone that just flipped to
**Done** — that's what the overview paragraph should name.

## Altitude — the one rule that matters

Commit bodies in this repo are deep and narrate bug stories, rationale, and
internal churn ("renamed X because Win32 macro Y", "fixed a typo in Z",
"discarded a `[[nodiscard]]` result"). **None of that belongs in release
notes.** A release note answers "what can I now do with ASGE, and why do I
care" — not "what did the commit history look like."

Cut, every time:
- Internal renames/refactors with no public-API or behavior effect
- Bug-fix narration for bugs introduced and fixed within the same release
  (never shipped broken, so nothing to explain)
- CI-only / roadmap-doc-only commits
- Enum/error-value lists, unless the type itself is the highlight
- Multi-sentence rationale for a naming choice — one clause, max, only if the
  name itself would otherwise be surprising to a caller

Keep: new/changed public types and methods (backtick them), what a new
example demonstrates, what test coverage now exists at a category level (not
per-test-case detail beyond a count).

## Structure

1. `# Release Notes — vX.Y.Z [YYYY-MM-DD]` — date from the tag's commit date.
2. One short paragraph (2–3 sentences). Always: what this release adds. If
   it completes a roadmap milestone (check `docs/roadmap/`), say what arc it
   closes and what the previous 1–2 releases contributed to it — this is the
   "overview" a milestone-completing release needs and a pure patch release
   doesn't.
3. `## Highlights` — one bullet per shipped feature, **bold lead-in — sentence**.
   Name the new types/methods in backticks. One clause on *why it matters*,
   not *how it was built*.
4. `## Testing` — bullets by category (unit coverage added, integration/
   pixel-verified proofs, full-suite status). Counts are fine ("21 cases"),
   per-case narration is not.
5. `## Contributors` — list from `git log --format=%an | sort -u`, filtered
   to human authors (skip `github-actions[bot]`).

See `template.md` for a fill-in-the-blanks version and `example-v0.2.0.md`
for a real one at the right altitude.
