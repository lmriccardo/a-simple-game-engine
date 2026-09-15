---
name: docsite-fix
description: Land a fix or improvement to the ASGE documentation site (docsite/, its Doxygen API reference, or .github/workflows/docs.yml) onto the docs/gh-pages-site branch — never straight into main. Use whenever asked to fix, adjust, restyle, or improve anything about the docs site, the GitHub Pages deploy, or the API reference theme.
---

Standing instruction from the repo owner: documentation-site fixes accumulate
on `docs/gh-pages-site` and get PR'd into `main` only in a batch, on the
owner's explicit go-ahead ("PR into main now" or equivalent) — never
automatically after a fix. `docs.yml` only deploys on push to `main`, so
fixes sitting on `docs/gh-pages-site` intentionally don't go live yet; that's
the point of batching them.

## 1. Get onto docs/gh-pages-site

```
git fetch origin
```

Then:
- If `origin/docs/gh-pages-site` exists: `git checkout -B docs/gh-pages-site origin/docs/gh-pages-site`.
- If it doesn't (the owner's last batch got merged into `main` and GitHub
  deleted the branch): recreate it fresh from `origin/main` —
  `git checkout -B docs/gh-pages-site origin/main`.

Never branch a one-off `fix/...` name off `main` for this kind of change —
that was the mistake this skill exists to prevent.

## 2. Make the fix

Same as any other change: read the actual affected file(s) under `docsite/`
or `.github/workflows/docs.yml` first, don't guess. If it touches the
Doxygen theme or generated output, rebuild locally and check it visually
(a portable `doxygen`/`dot` binary + headless Chrome screenshot, as done for
the original docsite build and its follow-up fixes — don't spin up Docker or
another VM for this, it's unnecessary) before pushing anything.

## 3. Commit and push to docs/gh-pages-site

Follow `CLAUDE.md`'s gitmoji + Conventional Commits format. Push straight to
the branch — no PR yet:

```
git push -u origin docs/gh-pages-site
```

If the branch already existed remotely and diverged (rare — normally you're
just adding a commit on top), reconcile with a merge or rebase as
appropriate rather than force-pushing over someone else's work.

## 4. Stop — report, don't PR

Tell the owner what landed on `docs/gh-pages-site` and that it's staged,
not live. Do **not** run `gh pr create --base main` for this. Only open that
PR when the owner explicitly says so in a later message; when they do, PR
`docs/gh-pages-site` into `main` (not a fresh branch).
