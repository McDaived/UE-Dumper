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
