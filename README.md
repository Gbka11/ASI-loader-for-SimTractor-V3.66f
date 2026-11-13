# SimTractor 3.66f – ASI Loader

**Author:** Sponge (Gbka)

A small plugin loader for **SimTractor V3.66f**. / Mały loader wtyczek dla **SimTractor V3.66f**.

📥 **Download:** see **[Releases](../../releases)** (right side of this page). / **Pobieranie:** zakładka **Releases** (po prawej).

---

## 🇬🇧 English

### What it is
A small plugin loader for SimTractor 3.5 (V3.66f). It replaces `d3d9.dll` in the game folder, forwards `Direct3DCreate9` to the real Windows `d3d9.dll` and loads every `*.asi` file from the `mods` folder (and from the game folder). Each mod is a separate `.asi` file (+ optional `.ini` next to it).

It also shows `ModLoader active: <mods>` next to the version text in the menu (`asi_loader.ini`: `ShowOverlay=0` turns it off) and writes `asi_loader.log`.

### Install
1. Back up your existing `d3d9.dll` in the game folder (if you have one, e.g. ReShade).
2. Copy `d3d9.dll` next to `SimTractor.exe`.
3. Create a `mods` folder there and put `.asi` mods in it (e.g. [Widescreen Fix](https://github.com/Gbka11/Simtractor-3.66f-Widescreen-Fix)).
4. Start the game. Check `asi_loader.log` if a mod does not load.

### Uninstall
Delete `d3d9.dll` and the `mods` folder (restore your old `d3d9.dll` if you had one).

### Notes
- Tested only with SimTractor V3.66f (32-bit).
- Another `d3d9.dll` wrapper (ReShade etc.) in the same folder will conflict – only one `d3d9.dll` can exist.
- Antivirus may flag any proxy `d3d9.dll` as suspicious (false positive). Full source is in `src`.
- The loader does not modify any game files.
- Known issue: Windows must be installed on drive `C:`.

---

## 🇵🇱 Polski

### Co to jest
Mały loader wtyczek dla SimTractor (V3.66f). Zastępuje `d3d9.dll` w folderze gry, przekazuje `Direct3DCreate9` do prawdziwego `d3d9.dll` z Windows i ładuje wszystkie pliki `*.asi` z folderu `mods` (oraz z folderu gry). Każdy mod to osobny plik `.asi` (+ opcjonalnie `.ini` obok).

W menu przy wersji pokazuje napis `ModLoader active: <lista modów>` (`asi_loader.ini`: `ShowOverlay=0` wyłącza) i zapisuje `asi_loader.log`.

### Instalacja
1. Zrób kopię obecnego `d3d9.dll` w folderze gry (jeśli jakiś masz, np. ReShade).
2. Skopiuj `d3d9.dll` obok `SimTractor.exe`.
3. Utwórz tam folder `mods` i wrzuć do niego mody `.asi` (np. [Widescreen Fix](https://github.com/Gbka11/Simtractor-3.66f-Widescreen-Fix)).
4. Uruchom grę. Jeśli mod się nie ładuje, sprawdź `asi_loader.log`.

### Odinstalowanie
Usuń `d3d9.dll` i folder `mods` (przywróć stary `d3d9.dll`, jeśli go miałeś).

### Uwagi
- Testowane tylko na SimTractor V3.66f (32-bit).
- Inny wrapper `d3d9.dll` (ReShade itp.) w tym samym folderze będzie się gryzł – może być tylko jeden `d3d9.dll`.
- Antywirus może oznaczyć każdy proxy `d3d9.dll` jako podejrzany (false positive). Pełny kod źródłowy w `src`.
- Loader nie zmienia żadnych plików gry.
- Znany problem: Windows musi być zainstalowany na dysku `C:`.

---

## ⚙️ Settings / Ustawienia (`asi_loader.ini`)

| Option | EN | PL |
|---|---|---|
| `ShowMessage=1` | message box with loaded mods at start | okienko z listą modów przy starcie |
| `ShowOverlay=0` | hide the text in the game menu | wyłącza napis w menu gry |
| `TickApi=0` | disable the per-frame mod API | wyłącza API modów (co klatkę) |
| `WarnMs=50` | log a warning when a mod is slow (0 = off) | ostrzeżenie w logu, gdy mod spowalnia grę (0 = wył.) |

## 🛠️ For modders / Dla twórców modów

Mods may optionally export / Mod może opcjonalnie eksportować:

```cpp
extern "C" __declspec(dllexport) void __cdecl ModOnInit(void* ckContext);        // once, before the first frame
extern "C" __declspec(dllexport) void __cdecl ModOnPreProcess(void* ckContext);  // every frame, before CKContext::Process
extern "C" __declspec(dllexport) void __cdecl ModOnPostProcess(void* ckContext); // every frame, after CKContext::Process
```

Callbacks run on the game thread. Details in the header of `src/asi_loader.cpp`.

### Build / Kompilacja
Open **x86 Native Tools Command Prompt for VS** (32-bit!) and run `build.bat`.

