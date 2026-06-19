# HUD mockup reference images

These PNGs live **inside the git repo** so agents and collaborators can open them by path (Cursor project `assets/` paths are not reliable across sessions).

## LoP HUD layout

| File | Description |
|------|-------------|
| [`dusklight-hud-lop-layout-v3-shield-icons-mockup.png`](dusklight-hud-lop-layout-v3-shield-icons-mockup.png) | **Current target** — v3 layout: hearts + green ALBW top-left, rupees top-right, X/Y items + Ordon **shield icons** (not spurs) + yellow durability bar bottom-left, B sword + wolves bottom-right. No gem row, no mini A/B/X/Y row. |
| *(not in repo)* | v1/v2/vanilla variants were generated in chat only; use v3 as the source of truth. |

**Implementation brief:** [`../albw-hud-lop-layout-v3-instructions.md`](../albw-hud-lop-layout-v3-instructions.md)

**Shield row + durability tuning (for Claude):** [`../albw-hud-lop-shield-row-tuning.md`](../albw-hud-lop-shield-row-tuning.md)

| File | Description |
|------|-------------|
| [`lop-hud-in-game-progress.png`](lop-hud-in-game-progress.png) | Current in-game build (overlap / wrong scale) — compare against v3 target. |

## Boss health bar

| File | Description |
|------|-------------|
| [`dusklight-boss-hp-bar-tuning-mockup.png`](dusklight-boss-hp-bar-tuning-mockup.png) | Thin crimson bar, translucent charcoal empty track, small “Armogohma” name tight above bar. |

**Tuning brief:** [`../albw-boss-hp-hud-tuning-brief.md`](../albw-boss-hp-hud-tuning-brief.md)

## For agents

Open mockups with the Read tool using repo-relative paths, e.g.:

```
docs/mockups/dusklight-hud-lop-layout-v3-shield-icons-mockup.png
```

Do **not** use `assets/` at the workspace root or `.cursor/projects/.../assets/` — those paths may be missing on another machine or session.
