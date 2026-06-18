# Commit and push — ALBW-Dusklight fork

**Audience:** You and Cursor agents. This fork is **`WadeWinningWilson/ALBW-Dusklight`**, not `TwilitRealm/dusklight`.

**Related:** [build-fps-guidelines.md](build-fps-guidelines.md) (build before commit if gameplay changed), [performance-handoff.md](performance-handoff.md) (perf investigation only).

---

## Remotes

| Remote | URL | Use |
|--------|-----|-----|
| **`upstream`** | `https://github.com/WadeWinningWilson/ALBW-Dusklight.git` | **Push here** — your ALBW fork |
| `origin` | `https://github.com/TwilitRealm/dusklight.git` | Upstream Dusklight reference; **do not push** unless you have TwilitRealm write access |

```powershell
git remote -v   # confirm upstream points at ALBW-Dusklight
```

---

## Never commit

These stay local (see `.gitignore`):

| Category | Examples |
|----------|----------|
| **Drive automation** | `src/dusk/drive.cpp`, `include/dusk/drive.h`, `local_dev_backup/` |
| **CoNavigate** | `src/dusk/conavigate.cpp`, `include/dusk/conavigate.h`, `docs/conavigate.md`, `build_conavigate.bat` |
| **Temp debug dumps** | `albw_*_debug.txt` (runtime **output** only — not required in repo) |
| **Build artifacts** | `build/`, `*.exe`, `build_stdout.txt`, `build_stderr.txt` |
| **Local backups** | `albw_backup_*/`, `.claude/` |

Drive/CoNavigate tooling belongs in `local_dev_backup/` (gitignored) or on another machine.

---

## Before commit

1. **Only commit when you explicitly want to** (or after playtest approval).
2. If gameplay/HUD changed, rebuild and spot-check:

```powershell
cmd /c build_run.bat
# Exe: build\windows-msvc-relwithdebinfo\dusklight.exe
```

3. Review scope:

```powershell
git status
git diff
git diff --staged
```

---

## Stage

```powershell
git add src/ include/ files.cmake CMakeLists.txt .gitignore docs/
# Add README or other paths only when intentionally changed
```

**Do not** `git add .` without checking — debug dumps and build logs must stay out.

---

## Commit

Use a short title + body (why, not just what):

```powershell
git commit -m "Short summary of why" -m "Optional detail: what changed and what was excluded."
```

---

## Push (ALBW fork)

If remote moved ahead (e.g. README edit on GitHub), merge first — **avoid rebase** if submodule conflicts appear:

```powershell
git fetch upstream
git merge upstream/main -m "Merge remote ALBW-Dusklight main"
git push upstream main
```

**Never** `git push origin main` for routine ALBW work unless you intend to push to TwilitRealm.

---

## Verify

```powershell
git status          # clean working tree (ignored files OK)
git log -1 --oneline
```

On GitHub: confirm latest commit on `WadeWinningWilson/ALBW-Dusklight` → `main`.

---

## Quick reference

```powershell
# Full happy path (after review)
git add <paths>
git commit -m "..."
git fetch upstream
git merge upstream/main    # if push rejected
git push upstream main
```
