# UE Dumper

A Windows tool that extracts Unreal Engine game data from a running process and generates a full C++ SDK — class hierarchies, structs, enums, function signatures, and member offsets 

Supports **UE4** (4.20 – 4.27) and **UE5** (5.0 – 5.3).

---

## Features

### Core Dumping
| Feature | Description |
|---|---|
| **Names dump** | Exports every FName string with its ID to `Names.txt` |
| **Objects dump** | Exports every UObject with its full class path and address to `Objects.txt` |
| **Full SDK generation** | Generates one `_classes.h` + `_struct.h` per package, with correct inheritance, member offsets, sizes, and padding |
| **Function parameter dumping** | Each function is output with full return type, parameter types, `const`/`out` modifiers, and array notation |
| **Bitfield & padding support** | Gaps between members are filled with `UnknownData_XX` padding; bitfields are shown with `: N` notation |

### Signature System
| Feature | Description |
|---|---|
| **Auto sig-finder** | On an unknown game, automatically find known GObjects/GNames patterns and validates each hit — no manual sig hunting needed |
| **Signature generator** | After every successful run writes `signatures.txt` to the output folder with the primary sig + a 16-byte extended version for robustness |
| **`<GameName>_sigs.txt`** | Always written next to the exe — contains the working sigs in `games.ini`-ready format so they can be reused or shared |

### Configuration
| Feature | Description |
|---|---|
| **`games.ini` config file** | All offsets and signatures live in a plain-text file next to the exe — add a new game without recompiling |
| **Built-in game profiles** | 8 profiles pre-loaded in code as fallback when `games.ini` has no entry |
| **Auto UE version detection** | Scans the module image for embedded version strings (e.g. `"Release-4.27"`) and prints the detected version |

### UE5 Support
| Feature | Description |
|---|---|
| **UE5 FName pool** | Reads the compact 2-byte-header, variable-length, 64KB-block pool used in UE5 |
| **Wide string support** | Handles both narrow (`ANSICHAR`) and wide (`WIDECHAR`) FName entries |
| **UE5 FName number** | Reads `Number` at the correct `+0x4` offset instead of the UE4 `-0x4` convention |

### VTable Function Resolver
| Feature | Description |
|---|---|
| **Fully dynamic vtable discovery** | Resolves virtual function slot indices at runtime — no hardcoded indices, works across engine builds and games |
| **Hint-based heuristic scan (primary)** | Each virtual function carries a byte fingerprint; the resolver walks the target class vtable and matches the fingerprint against each function body using the local image copy |
| **Pattern-seeded fallback** | If the heuristic scan finds no match, the resolver falls back to the pattern-scan RVA and searches vtable slots for a function within `0x10` bytes of it |
| **Multi-hint per entry** | Up to 4 alternate fingerprints per function; the first match at any slot wins — covers different MSVC prologue variants (PR-A, PR-B, etc.) without changing resolver logic |
| **Thunk / adjustor unwinding** | Follows `JMP rel32`, `JMP rel8`, `SUB/ADD RCX,imm8 + JMP` (single-inheritance adjustor), `SUB/ADD RCX,imm32 + JMP` (large adjustor), and stops at `JMP [RIP+rel32]` import stubs |
| **Post-thunk voting** | Votes on the real function RVA *after* thunk resolution — objects that reach the same function through different adjustor stubs now converge on one winner instead of splitting votes |
| **minIndex guard** | Each entry has a minimum acceptable slot index; slots 0–1 (MSVC destructors) and any suspiciously low index are rejected before they can produce false positives |
| **Source-tagged `funcs.txt`** | Every entry in `funcs.txt` is now labelled `[vtable>pattern]`, `[vtable]`, or `[pattern]` — vtable result is the primary source of truth for virtual functions |
| **`vtable_funcs.txt` output** | Dedicated file for vtable-resolved functions — shows discovered index, `FOUND_IN_VTABLE [heuristic]` or `FOUND_IN_VTABLE`, RVA, VA, SlotRVA, thunk depth, and validation count |
| **Tracked virtual functions** | `UObject::ProcessEvent`, `UGameViewportClient::PostRender`, `UNetDriver::ProcessRemoteFunction` — all discovered dynamically |

### Runtime Offset Discovery
| Feature | Description |
|---|---|
| **Probe-based UObject layout** | After GObjects/GNames are found, probes up to 400 live objects to vote on `Index`, `Class`, `Name`, and `Outer` field offsets — no profile needed |
| **`game.ini` stays authoritative** | Profile values are **never overwritten** — autodetect only fills fields the profile left at zero; if both disagree a diagnostic warning is printed |
| **Per-field confidence scoring** | Each discovered offset gets an individual confidence score; fields below 65 % are skipped and left to the profile or flagged as missing |
| **`offsets.txt` debug file** | Written every full dump — lists every struct field offset, its value, and which source provided it (`game.ini`, `autodetect (N%)`, or `MISSING`), plus a summary count at the bottom |

### UX
| Feature | Description |
|---|---|
| **Progress bars** | Names, Objects, and Packages phases all show `[###   ] 45%  ETA 12s  3.1s` live bars |
| **Detailed console output** | Prints game name, detected UE version, UE5 mode flag, found addresses, and counts |
| **Helpful error messages** | Every error code prints message and a suggestion (e.g. which `games.ini` key to add) |

---

## Supported Games (built-in profiles)

| Game | Executable stem | UE Version |
|---|---|---|
| Sea of Thieves | `SoTGame` | 4.x |
| Contra: Rebooted | `ContraReboot-Win64-Shipping` | 4.x |
| Fortnite | `FortniteClient-Win64-Shipping` | 4.27 |
| Hogwarts Legacy | `HogwartsLegacy-Win64-Shipping` | 4.27 |
| Dead Island 2 | `DeadIsland2-Win64-Shipping` | 4.27 |
| Chivalry 2 | `Chivalry2-Win64-Shipping` | 4.26 |
| Generic UE4 template | `Generic-UE4` | 4.25+ |
| Generic UE5 template | `Generic-UE5` | 5.x |

Any game **not** in the list is still handled by the auto sig-finder — it will scan and write a `<GameName>_sigs.txt` file so you can add the entry to `games.ini` for next time.

---

## How to Use App

- The target game must be **running** before you launch UE Dumper
- Run UE Dumper as **Administrator** (needed for `PROCESS_ALL_ACCESS`)


---

## Source Code

### 1. Build
Open `UE Dumper.sln` in Visual Studio 2022, select **Release | x64**, and build.  
The exe lands at `bin/Release/Dumper.exe`.

### 2. Launch your game
Start the game and wait until you are in the main menu or in-game (the UE object pool must be fully populated).

### 3. Run UE Dumper
```
Dumper.exe           # full dump — Names, Objects, and SDK
Dumper.exe -p        # partial dump — Names and Objects only (fast)
Dumper.exe -w        # wait for a keypress before starting (gives time to inject mods)
```

---

## Adding a New Game

### Option A — Auto-finder (zero effort)
Just run UE Dumper against the game. The auto-finder will find all signature pattern and validates thats works. If it works, it writes `<GameName>_sigs.txt` next to the exe with the correct sigs and a template for the offsets.

### Option B — Manual via `games.ini`
Open `games.ini` (next to the exe) and add a section, Example:

```ini
[MyGame-Win64-Shipping]
UE = 4.27
IsUE5 = false

FNameEntry.HeaderSize     = 0x2
UObject.Index             = 0xC
UObject.Class             = 0x10
UObject.Name              = 0x18
UObject.Outer             = 0x20
UField.Next               = 0x28
UStruct.SuperStruct       = 0x30
UStruct.Children          = 0x38
UStruct.PropertiesSize    = 0x40
UEnum.Names               = 0x40
UEnum.NamesElementSize    = 0x10
UFunction.FunctionFlags   = 0x88
UFunction.Func            = 0xB0
UProperty.ArrayDim        = 0x38
UProperty.ElementSize     = 0x3E
UProperty.PropertyFlags   = 0x30
UProperty.Offset          = 0x4C
UProperty.Size            = 0x70

GObjects.Sig        = 4C 8B 0D ?? ?? ?? ?? 4C 8B C3
GObjects.PtrOffset  = -4
GObjects.PtrExtra   = 3
GNames.Sig          = 48 89 1D ?? ?? ?? ?? 48 8B 5C 24 20 48 83 C4 28
GNames.PtrOffset    = 4
GNames.PtrExtra     = 3
```

> **Tip:** Copy `[Generic-UE4]` or `[Generic-UE5]` from `games.ini` as a starting point — most UE4.25+ games share the same offsets.

### For UE5 games
Set `IsUE5 = true` (or `UE = 5.x`) and use the `[Generic-UE5]` offsets as a base.  
The FName pool reader switches to the UE5 block-based layout automatically.

---

## Troubleshooting

| Error | Fix |
|---|---|
| `can't find UnrealWindow` | Make sure the game is running and is a UE title |
| `can't enumerate modules (protected process?)` | Run UE Dumper as Administrator |
| `no offset profile found for this game` | Add a `[GameName]` section to `games.ini` |
| `GObjects not found (all patterns failed)` | Find the sig manually with IDA/x64dbg and add it to `games.ini` |
| `GNames not found (all patterns failed)` | Same as above for the GNames sig |
| `zero packages found` | The offsets are likely wrong — cross-check with `[Generic-UE4]` values |
| Names/Objects look garbled | `FNameEntry.HeaderSize` is wrong — try `0x2` (UE4.25+) or `0xC`/`0x10` (older) |
| vtable index is 0 or wrong | The hint bytes for that function don't match this build — check `vtable_funcs.txt` for the `NOT_VIRTUAL` diagnostic line and update the hint |
| `offsets.txt` shows MISSING fields | The profile is incomplete and autodetect failed — add the missing keys to `games.ini` manually |

---

## Changelog

```diff
## v1.1

# VTable Function Resolver (vtable_resolver.h/cpp)
+ Added fully dynamic vtable-based function resolver — no hardcoded slot indices
+ Heuristic scan (function body fingerprint) is primary; pattern-seeded discovery is fallback
+ Multi-hint support per entry: up to 4 alternate byte fingerprints tried in order, covers MSVC prologue variants
+ Thunk and adjustor-thunk unwinding before voting: JMP rel32, JMP rel8, SUB/ADD RCX + JMP (imm8 and imm32), import stub detection
+ Voting happens after thunk resolution different adjustor thunks routing to the same real function now converge correctly
+ minIndex guard per entry rejects destructor slots and suspiciously shallow indices
+ Result tags: FOUND_IN_VTABLE [heuristic] (fingerprint matched) vs FOUND_IN_VTABLE (pattern-seeded)
+ Diagnostic output for failures: NOT_VIRTUAL includes the pattern RVA that was searched and the slot range that was scanned
+ New output file: vtable_funcs.txt - vtable resolved functions with index, RVA, VA, SlotRVA, thunk depth, and validation count

# Virtual Functions Tracked
+ UObject::ProcessEvent - heuristic discovered (correct index varies per build)
+ UGameViewportClient::PostRender - heuristic discovered, two hints cover PR-A and PR-B prologue variants
+ UNetDriver::ProcessRemoteFunction - heuristic discovered with minIndex = 0x40 new pattern signatures added (PRF-A, PRF-B, PRF-C)

# funcs.txt Improvements
+ VTable results are now the primary source of truth for virtual functions; pattern scan is fallback
+ Every entry is labelled with its source: [vtable>pattern], [vtable], or [pattern
+ Cross validation: range-based comparison (threshold 0x1000) replaces exact equality wrapper/override differences are logged as [vtable>pattern~] instead of hard mismatches

# Runtime Offset Discovery (autooffsets.h/cpp)
+ Added probe-based UObject field offset discovery: samples up to 400 live objects and votes on Index, Class, Name, Outer positions
+ Priority enforced: game.ini values are authoritative and never overwritten - autodetect fills only zero-value fields
+ Per-field confidence scoring fields below 65 % threshold are skipped
+ Diagnostic warning printed when profile and autodetect disagree on a value
+ New output file: offsets.txT every struct field with value and source (game.ini, autodetect (N%), or MISSING), summary counts at bottom
```