---
name: roadmap-milestone
description: Stand up GitHub project scaffolding from a docs/roadmap phase-N doc — a branch, a milestone (summary + tasks in the description), and one labeled/assigned issue per task. Use when asked to "create a milestone/branch/issues for" a roadmap file, or to kick off work on a roadmap phase doc.
---

Turns one `docs/roadmap/phase-N/<NN.P>-<name>.md` file into working GitHub
scaffolding: a branch, a milestone, and one issue per task. Requires `gh`
authenticated (`gh auth status` — if it fails, tell the user to run
`gh auth login -h github.com` and stop; don't attempt any of the steps below
unauthenticated).

## 0. Read the doc

Read the target file in full first — the description and every issue body
are drawn from its Goals/Current state/Design notes/Tasks sections, not
guessed. Note the `owner/repo` via `gh repo view --json nameWithOwner`.

## 1. Derive names

- **Version**: the leading number in the filename, e.g. `05.1-asset-pipeline.md`
  → `0.5.0` (the `.1` is the *phase* suffix, not part of the version — phase 1
  is always `0.<N>.0`, phase 2/3 docs are stretch/future work, not scaffolded
  by this skill unless explicitly asked).
- **Title**: Title Case of the filename's name segment, e.g. `asset-pipeline`
  → `Asset Pipeline`.
- **Milestone title**: `"0.<N>.0 - <Title> Phase <P>"`, matching the existing
  convention on this repo's milestones (e.g. `0.4.0 - Input System Phase 1`).
- **Branch name**: `mils/0.<N>.0-<kebab-title>`, e.g. `mils/0.5.0-asset-pipeline`
  — full kebab-case of the title, not an abbreviation.

## 2. Branch

From a clean, up-to-date `main`:

```
git checkout -b mils/0.<N>.0-<kebab-title>
git push -u origin mils/0.<N>.0-<kebab-title>
git checkout main
```

## 3. Milestone

Write a **summary**, not the raw file — 1–2 short paragraphs covering what
the phase adds and why (Goals + the sharpest Design notes), and what's
explicitly deferred, followed by a literal `## Tasks` checklist mirroring the
doc's own `## Tasks` section (unchecked boxes). Put it in a temp file, then:

```
gh api repos/{owner}/{repo}/milestones -X POST \
  -f title="0.<N>.0 - <Title> Phase <P>" \
  -F description=@<tmpfile>
```

## 4. One issue per task

For each item in the doc's `## Tasks` list, one issue:

```
gh issue create \
  --title "<short task name, no leading verb-list noise>" \
  --milestone "<milestone title from step 3>" \
  --label "enhancement" \
  --assignee "lmriccardo" \
  --body "..."
```

Body shape (matches this repo's existing issues — see any closed milestone's
issues for the pattern):

```
## Context

Part of [`<filename>.md`](docs/roadmap/phase-N/<filename>.md). <1-3 sentences
grounding *why* this task exists, pulled from the doc's Design notes/Current
state — not boilerplate.>

## Scope

- [ ] <task broken into concrete, checkable sub-steps>
- [ ] <...>
- [ ] Unit tests for <the thing>, if the doc's Deliverables call for it

**Branch:** `mils/0.<N>.0-<kebab-title>`
```

Note dependency order between issues in prose at the bottom ("Depends on
the <X> task") when one task's doc position clearly builds on another's.

### Why the branch is a text line, not a native GitHub link

`gh issue develop` and the `createLinkedBranch` GraphQL mutation only
**create a brand-new branch** tied to one issue — verified empirically:
pointing `createLinkedBranch` at a branch name that already exists returns
`{"linkedBranch": null}` with no error, i.e. a silent no-op. There is no API
path to attach one already-existing shared branch to several issues, which
is what a milestone-wide branch needs. The `**Branch:** \`...\`` line in each
issue body is the practical substitute — don't spend time re-attempting the
native link for a shared branch.

## 5. Report

List the branch, the milestone URL, and each issue URL back to the user.
