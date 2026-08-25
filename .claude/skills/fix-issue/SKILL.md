---
name: fix-issue
description: End-to-end workflow for fixing a GitHub issue in this repo — pull the issue, branch, implement the fix, document-and-test it, and open a PR into main. Use when asked to "fix issue #N", "solve issue #N", or "work issue #N".
---

Requires `gh` authenticated (`gh auth status` — if it fails, tell the user to
run `gh auth login -h github.com` and stop). Takes one argument: an issue
number.

## 1. Pull the issue

```
gh issue view <N> --json number,title,body,labels,url
```

Read the body in full — reproduction, root cause, and any suggested fix are
usually spelled out there. Don't start editing code until you understand the
actual defect, not just the title.

## 2. Branch

Check for uncommitted changes first (`git status --short`); if there are any
unrelated to the fix, leave them alone (don't stash/discard) and remember not
to stage them later. Branch from an up-to-date `main`:

```
git fetch origin main
git checkout -b fix/<short-kebab-slug> main
```

Name the slug from the issue's subject, not `issue-N` — e.g.
`fix/rendersystem-destrect-sourcecrop`, not `fix/issue-35`.

## 3. Implement the fix

Make the minimal change that addresses the root cause described in the
issue — prefer the issue's own suggested fix when it's sound; verify it
against the actual code first rather than pasting it blindly. Check any
doc comments on the code you touch (declaration-site Doxygen blocks per
`CLAUDE.md`) for claims the fix now invalidates and update them in the same
edit, even before running document-and-test.

## 4. Document and test

Invoke the `document-and-test` skill, pointing it at the changed
class/function and describing the bug briefly so its tests target the actual
regression, not just generic coverage:

```
Skill(document-and-test, "<Component> (<path>) — focus on documenting and
testing the fix for issue #<N>: <one-line description of the bug>.")
```

That skill adds/updates Doxygen comments and writes a GoogleTest suite
following the repo's conventions, then builds and runs it plus the full
`ctest` suite. Don't skip its verify step — a fix without a red-then-green
test run for the regression isn't done.

## 5. Commit

Stage only the fix + its docs/tests — never sweep in unrelated pre-existing
changes sitting in the working tree (check `git status --short` again before
`git add`). Follow the commit message convention in `CLAUDE.md` (gitmoji +
Conventional Commits, `fix(<scope>): :bug: ...` for a bug fix), and end the
body with `Fixes #<N>` so GitHub auto-closes the issue on merge.

## 6. PR

```
git push -u origin fix/<short-kebab-slug>
gh pr create --base main --head fix/<short-kebab-slug> \
  --title "fix(<scope>): <short imperative summary>" \
  --body "..."
```

PR body: a Summary naming the fixed issue (`Fixes #<N>`), a Changes list
(one bullet per file, what changed and why), and a Testing section stating
what was actually run (e.g. `ctest --preset windows-debug` pass count) —
never claim a test run that didn't happen.

Report the PR URL back to the user; don't merge it yourself.
