# 0008 — Split the documentation into a landing page and `docs/`

**Status** — Accepted

## Context

`README.md` had grown to **103,568 characters on 599 lines**, with a single H1 and no table
of contents. It was accurate and complete, and unusable: a new owner could not find where to
start, a returning owner could not find one binding, and every reader had to scroll past
hardware, thirty modes' worth of prose, and build instructions to reach anything.

The content was not the problem. The container was.

## Decision

- `README.md` becomes a **landing page**: what the instrument is, hardware in one table, the
  three mode families in one table, a link tree, build in three lines.
- The existing prose is **moved, not rewritten**, into `docs/` by subject:
  `modes/`, `generators/`, `reference/`, plus `hardware`, `architecture`, `build`,
  `extending`.
- New material is authored where the old page had no home: a [glossary](../glossary.md),
  [cheat sheet](../cheatsheet.md), [panel map](../panel-map.md),
  [feature status](../status.md), [recipes](../recipes/index.md), [FAQ](../faq.md),
  [limitations](../limitations.md), [ADRs](README.md) and a
  [style guide](../style-guide.md).
- Movement is verbatim. Rewriting while moving would have introduced behaviour the firmware
  does not have, and the old page was the only accurate record of some of it.

## Consequences

- Every page now has one H1, a breadcrumb, and a depth limit of three headings.
- **Links became a maintenance cost.** Renaming a heading breaks an anchor, so the style
  guide requires a link check before committing a rename.
- Readers who bookmarked README anchors lost them; the docs map is the replacement.
- `AGENTS.md` now points at `docs/` instead of `README.md`, and the definition of "keep the
  docs in sync" expanded: behaviour changes touch the page, the status matrix, the changelog
  and `TODO.md`.
- Splitting again is cheap: the tree is by task, so a new subject is a new folder, not a new
  section in a monolith.
