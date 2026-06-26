# Trailer production — handoff & asset index

**Purpose:** Preserve trailer image work from the long Cursor chat so you **do not need to reopen that session**. Start new chats with:

> Continue trailer work — read `docs/trailer-handoff.md` first.

**Related:** In-game boss HP HUD (not trailer) → [boss-fights-handoff.md](boss-fights-handoff.md), [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md).

---

## Is a handoff doc advised here?

**Yes.** Trailer work is mostly **generated PNG assets + style rules** — easy to lose if you only rely on chat history. Original mega-session **`9818a7d2-ba3f-4e7e-bc65-4fced422559c`** also covered shop, death orb, and perf work — see linked docs below. Session **deleted 2026-06-15** after archival.

**Related handoffs from same session:** [albw-shop-icon-alignment.md](albw-shop-icon-alignment.md), [albw-death-recovery-orb-brief.md](albw-death-recovery-orb-brief.md), [future-performance-leaning.md](future-performance-leaning.md).

---

## Asset location

```
trailer/assets/          ← repo-local copies (gitignored; .gitkeep only in git)
```

All `dusklight-*.png` files from the original Cursor session folder were copied here (33 PNGs). They stay on disk for editing and trailer assembly but are **not committed** — see `.gitignore` (`trailer/assets/*` with `!trailer/assets/.gitkeep`).

Original source (no longer needed unless re-exporting):

```
%USERPROFILE%\.cursor\projects\<workspace-hash>\assets\
```

---

## Visual style — caption banners

Match the official **Dusklight / Twilight Princess trailer** caption plaques (reference: “Enhanced Resolution” style from base Dusklight trailer screenshots).

| Element | Spec |
|---------|------|
| **Shape** | Wide horizontal plaque (~6:1), notched concave gold corners |
| **Fill** | Dark chocolate brown, subtle center glow |
| **Border** | Thick gold double frame |
| **Text** | White bold serif, title case, centered on brown panel |
| **Shadow** | Soft black drop shadow, down-right |
| **Background** | **Transparent** outside the plaque (alpha PNG) |
| **Quotation marks** | **Never** in final banner text (user copies text *inside* quotes only) |

### Editor workflow (Premiere / DaVinci)

1. Import PNG (pre-rendered text) **or** blank base + add your own text layer.
2. Scale to taste; banners are wide.
3. For blank base: use same font treatment as above.

**Caveat:** Banners are **AI recreations** matched to Dusklight screenshots, not ripped game assets. Good enough for mod trailer; not pixel-perfect extraction.

---

## Feature caption banners (16 + extras)

### Core set (numbered)

| # | On-screen text | Filename |
|---|----------------|----------|
| 1 | Refined Boss Combat | `dusklight-trailer-banner-01-refined-boss-combat.png` |
| 2 | Boss Health Bars | `dusklight-trailer-banner-02-boss-health-bars.png` |
| 3 | Boss Rush | `dusklight-trailer-banner-03-boss-rush.png` |
| 4 | **Wolf Link Combat Overhaul** | `dusklight-trailer-banner-04-refined-wolf-link-combat.png` |
| 5 | Enemy HP Multiplier | `dusklight-trailer-banner-05-enemy-hp-multiplier.png` |
| 6 | Parry System | `dusklight-trailer-banner-06-parry-system.png` |
| 7 | Shield Durability | `dusklight-trailer-banner-07-shield-durability.png` |
| 8 | Rental Shop | `dusklight-trailer-banner-08-rental-shop.png` |
| 9 | RPG Leveling | `dusklight-trailer-banner-09-rpg-leveling.png` |
| 10 | Enemy Wealth | `dusklight-trailer-banner-10-enemy-wealth.png` |
| 11 | Rupee Loss/Recovery Orb | `dusklight-trailer-banner-11-rupee-loss-recovery-orb.png` |
| 12 | On Death | `dusklight-trailer-banner-12-on-death.png` |
| 13 | **Z-Item Slot** | `dusklight-trailer-banner-13-z-item-slot.png` |
| 14 | Magic Armor Rework | `dusklight-trailer-banner-14-magic-armor-rework.png` |
| 15 | Risk and Reward | `dusklight-trailer-banner-15-risk-and-reward.png` |
| 16 | **Z-Target Switching** | `dusklight-trailer-banner-16-z-target-switching.png` |

### Rewording history (locked in)

| Original | Final |
|----------|-------|
| Refined Wolf Link Combat | **Wolf Link Combat Overhaul** |
| Z Item Slot | **Z-Item Slot** |
| Z Target Switching | **Z-Target Switching** |

### Early / extra banners

| Text | Filename |
|------|----------|
| *(blank base)* | `dusklight-trailer-text-banner-blank.png` |
| ALBW Meter | `dusklight-trailer-banner-albw-meter.png` |
| Parry and Durability System | `dusklight-trailer-banner-parry-durability.png` |
| And More! | `dusklight-trailer-banner-and-more.png` |
| Better HUD Hider | `dusklight-trailer-banner-better-hud-hider.png` |

---

## Title cards

| Text | Filename | Notes |
|------|----------|-------|
| In Progress... | `dusklight-trailer-in-progress-title-v2.png` | **Use v2** — muted gray-white, no quotes |
| In Progress... (v1) | `dusklight-trailer-in-progress-title.png` | Bright white, had quotes — superseded |

---

## Boss health bar mockups (trailer + HUD reference)

Generated for trailer B-roll and to guide the **in-game** Boss Health Bars setting. Typography matches caption banners (white serif + drop shadow).

| Asset | Filename | Layout |
|-------|----------|--------|
| Usurper King Zant (typo) | `dusklight-boss-healthbar-ursurper-king-zant.png` | Elden Ring–style, name left-aligned |
| Usurper King Zant (correct spelling) | `dusklight-boss-healthbar-lies-of-p-zant-full.png` | **Lies of P** — name centered above bar, full red fill |
| Armogohma tuning mockup | `dusklight-boss-hp-bar-tuning-mockup.png` | In-game screenshot edit for HUD polish |
| LoP layout variants | `dusklight-hud-lop-layout*.png` | Layout exploration |

**In-game implementation:** `d_albw_boss_hp_hud.cpp` — see [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md).

---

## Shop footer mockup (trailer + shop UI target)

Approved layout reference for Postman shop footer (also used in trailer context):

| Version | Filename | Notes |
|---------|----------|-------|
| v1 | `dusklight-shop-footer-mockup.png` | Initial |
| v2–v3 | `dusklight-shop-footer-mockup-v2.png`, `-v3.png` | Quoted tagline fixes |
| **v4 (approved target)** | `dusklight-shop-footer-mockup-v4.png` | Use for shop implementation |

**Footer target:**

| Zone | Content |
|------|---------|
| Left | White nunchuk stick + up/down arrows (inside footer bar only) |
| Center | `"Hylians are raving about our service!"` (quotes in-game string) |
| Right | `A: Buy/ B: Exit` |

**Code:** `drawShopFooter()` in `d_albw_shop.cpp` — see shop brief in chat transcript / [albw-shop-icon-alignment.md](albw-shop-icon-alignment.md).

---

## Suggested trailer feature order (draft)

Use the 16 banners as chapter cards between gameplay clips. Typical grouping:

1. Core ALBW — ALBW Meter, On Death, Rental Shop  
2. Combat — Parry System, Shield Durability, Refined Boss Combat  
3. Economy — Enemy Wealth, Rupee Loss/Recovery Orb, Enemy HP Multiplier  
4. QoL — Boss Health Bars, Boss Rush, Z-Item Slot, Z-Target Switching, Better HUD Hider  
5. Closer — And More!

Adjust order to match your edit rhythm.

---

## Requesting new banners

In a **new** chat, provide:

1. Exact on-screen text (no quotation marks in the image)  
2. Reference: “same style as `dusklight-trailer-banner-albw-meter.png`”  
3. Note if long text needs smaller font or two-line layout  

Example:

> Generate a trailer banner: `Death Recovery Orb` — same gold-plaque style as the existing set.

---

## Chat archive

| Field | Value |
|-------|-------|
| Transcript ID | `9818a7d2-ba3f-4e7e-bc65-4fced422559c` |
| Scope | Trailer assets + shop + orb + perf (mixed session) |
| Reopen in UI? | **No** — deleted 2026-06-15; use this doc + `trailer/assets/` |

Session deleted from Cursor DB after handoff docs verified.
