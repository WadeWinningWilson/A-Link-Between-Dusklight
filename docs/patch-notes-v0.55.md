# Patch Notes for v0.55

**A Link Between Dusklight** — changes since [v0.5](../README.md) (`ALBW-Dusklight-v0.5`).

---

## New features

### Extra Item Slot (optional)

- New toggle under **Pause → ALBW Settings → Extra Item Slot** (default: off).
- When enabled:
  - **Talk to Midna** moves to the **left D-pad** cross direction.
  - The **Z button** accepts a third inventory item (assign from the item wheel with **Z**, same as X/Y).
  - The Z bubble stays visible for the assigned item; Midna’s icon sits on the left cross arm instead of on Z.
- Wolf Link is unchanged: Z still uses vanilla wolf abilities when transformed.

### Death Recovery Orb (optional)

- New **Death Recovery Orb** toggle under ALBW Settings (default: off).
- After Talo is rescued, dying can halve your wallet and leave a **Tear of Light** at the death spot to recover part of the lost rupees.
- Item strip and meter refill on death are unaffected when this is off.

### Wolf Link combat (optional)

- New **Wolf Link Combat** toggle under ALBW Settings (default: off).
- ALBW-style wolf biting: charge system for Midna field attacks, twilight vs non-twilight damage split, non-twilight stun, and low-HP bite healing.
- Wolf form remains outside the ALBW energy meter.

### Enemy Death Rupees (optional)

- New **Enemy Death Rupees** toggle under ALBW Settings (default: off).
- Credits rupees directly to your wallet on enemy kills and boss fight victories.
- Shows a native **"+n"** popup beside the rupee counter.
- Vanilla drop tables (hearts, jars, ground rupees) are unchanged.

---

## Improvements

- ALBW mod settings are grouped under a single **ALBW Settings** section in the pause menu.
- Magic jar pickups behave more reliably when collected from the field.
- Postman rental shop documentation and README updated for current feature set.

---

## Bug fixes

### Extra Item Slot & HUD

- Z-slot item icons now overlay the Z button correctly (including after moving items from X/Y).
- Midna’s HUD icon stays visible on the left cross arm when Extra Item Slot is on, and returns to Z when the setting is off.
- Gale Boomerang and other Z-slot items no longer vanish from the HUD after a few seconds (including during boomerang lock-on / R-button status updates).
- Wolf scent collection no longer overwrites the Z item assignment when Extra Item Slot is enabled.

### Death Recovery Orb

- Recovery orb spawns at death height instead of snapping to the ground.
- Tear-of-light particle load is cancelled cleanly on scene change (avoids orphaned effects).
- Orb emitter retry and BashHud session cleanup improved for dungeon deaths and reloads.

### Postman shop

- Rental item icons align correctly at widescreen aspect ratios.

---

## Known limitations (unchanged from v0.5)

- Postman shop footer polish (analog-stick hint, tagline) is still WIP.
- Colossal Wallet / final pricing pass is still pending.

---

## Upstream

Built on [Dusklight](https://github.com/TwilitRealm/dusklight). Provide your own legal copy of _Twilight Princess_.

For full gameplay and build instructions, see **[README](../README.md)** and **[docs/albw-port.md](albw-port.md)**.
