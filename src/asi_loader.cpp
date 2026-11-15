// asi_loader.cpp  -  small .asi plugin loader for SimTractor (poses as d3d9.dll)
//
// What it does:
//   * the game already loads d3d9.dll from the folder next to SimTractor.exe - this file does the same (forwards Direct3DCreate9
//     to the real Windows d3d9.dll), and on top of that loads every *.asi file from the "mods" subfolder
//     (and from the game folder, next to the exe),
//   * each mod is a separate .asi file (+ optionally its own .ini in the same folder),
//   * adds "ModLoader active: <mod list>" in the game (next to the version line in the bottom right of the menu) (asi_loader.ini: ShowOverlay=0 turns it off),
//   * writes asi_loader.log next to the loader: what was loaded and what failed (with error code).
//   * mod API on the GAME THREAD: the loader hooks the ?Process@CKContext@@QAEHXZ (CK2.dll) import in SimTractor.exe's IAT,
//     called once per frame from the CustomPlayer loop (SimTractor.exe FUN_00403a00). A mod can (optionally) export:
//        extern "C" __declspec(dllexport) void __cdecl ModOnInit(void* ckContext);        // once, before the first Process
//        extern "C" __declspec(dllexport) void __cdecl ModOnPreProcess(void* ckContext);  // every frame, before Process
//        extern "C" __declspec(dllexport) void __cdecl ModOnPostProcess(void* ckContext); // every frame, after Process
//     These callbacks may call the CK2 API (same thread as the engine). SEH exceptions from a mod are caught and the mod
//     is then disabled (logged). asi_loader.ini: [General] TickApi=0 disables the hook.
//     Stability: no callbacks when ctx==NULL or on re-entry into Process; a warning is logged when
//     a callback takes longer than [General] WarnMs (default 50 ms, 0 = off).
//
// Build (x86 Native Tools Command Prompt for VS):
//   cl /nologo /LD /O2 /MT asi_loader.cpp /Fe:d3d9.dll

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#pragma comment(lib, "user32.lib")
#pragma comment(linker, "/export:Direct3DCreate9=C:\\Windows\\System32\\d3d9.Direct3DCreate9")

static char g_dir[MAX_PATH];        // folder this loader (and the exe) is in
static char g_log[MAX_PATH];

static void Log(const char* fmt, ...)
{
    FILE* f = NULL;
    fopen_s(&f, g_log, "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fclose(f);
}

// ------------------------------------------------------------------ mod callbacks (game thread)
typedef void (__cdecl *FnModCb)(void* ctx);
struct ModCb { char name[64]; FnModCb init, pre, post; int dead; };
#define MAX_MODS 64
static ModCb g_cb[MAX_MODS];
static int   g_cbCount = 0;

static void RegisterCallbacks(HMODULE m, const char* file)
{
    FnModCb init = (FnModCb)GetProcAddress(m, "ModOnInit");
    FnModCb pre  = (FnModCb)GetProcAddress(m, "ModOnPreProcess");
    FnModCb post = (FnModCb)GetProcAddress(m, "ModOnPostProcess");
    if (!init && !pre && !post) return;
    if (g_cbCount >= MAX_MODS) { Log("tick API: too many mods, skipping %s", file); return; }
    ModCb& c = g_cb[g_cbCount++];
    strncpy_s(c.name, sizeof(c.name), file, _TRUNCATE);
    c.init = init; c.pre = pre; c.post = post; c.dead = 0;
    Log("tick API: %s (init=%d pre=%d post=%d)", file, init != 0, pre != 0, post != 0);
}

static int g_count = 0;
static char g_names[1024] = "";
static char g_short[512] = "";       // mod names without ".asi", comma separated (for the in-game text)

static void LoadFrom(const char* dir)
{
    char pattern[MAX_PATH];
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s*.asi", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char path[MAX_PATH];
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s%s", dir, fd.cFileName);
        HMODULE m = LoadLibraryA(path);
        if (m) {
            Log("loaded: %s", path);
            RegisterCallbacks(m, fd.cFileName);
            g_count++;
            strncat_s(g_names, sizeof(g_names), "\n - ", _TRUNCATE);
            strncat_s(g_names, sizeof(g_names), fd.cFileName, _TRUNCATE);
            {
                char nm[MAX_PATH]; strncpy_s(nm, sizeof(nm), fd.cFileName, _TRUNCATE);
                char* dot = strrchr(nm, '.'); if (dot) *dot = 0;
                if (g_short[0]) strncat_s(g_short, sizeof(g_short), ", ", _TRUNCATE);
                strncat_s(g_short, sizeof(g_short), nm, _TRUNCATE);
            }
        }
        else   Log("LOAD ERROR: %s (code %lu)", path, GetLastError());
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// Mods are loaded SYNCHRONOUSLY in DllMain (.asi plugins do nothing heavy in their DllMain - they only start threads),
// so they are loaded before the game continues (needed e.g. by the widescreen mod, which changes the game settings).
static void LoadAll()
{
    Log("---- asi_loader start ----");
    char mods[MAX_PATH];
    _snprintf_s(mods, sizeof(mods), _TRUNCATE, "%smods\\", g_dir);
    LoadFrom(mods);      // mods subfolder first
    LoadFrom(g_dir);     // then the game folder
    Log("---- asi_loader end ---- (mods: %d)", g_count);
}

// Startup message box in a separate thread (does not block the game).
static DWORD WINAPI LoaderThread(LPVOID)
{

    // startup message (turned off in asi_loader.ini: [General] ShowMessage=0), closes by itself after a few seconds
    char ini[MAX_PATH];
    _snprintf_s(ini, sizeof(ini), _TRUNCATE, "%sasi_loader.ini", g_dir);
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES)
        WritePrivateProfileStringA("General", "ShowMessage", "0", ini);
    if (GetPrivateProfileIntA("General", "ShowMessage", 1, ini)) {
        char text[1400];
        _snprintf_s(text, sizeof(text), _TRUNCATE, "SimTractor ModLoader is running.\nLoaded mods: %d%s", g_count, g_names);
        typedef int (WINAPI *MBT)(HWND, LPCSTR, LPCSTR, UINT, WORD, DWORD);
        HMODULE u = LoadLibraryA("user32.dll");
        MBT mbt = u ? (MBT)GetProcAddress(u, "MessageBoxTimeoutA") : NULL;
        if (mbt) mbt(NULL, text, "ModLoader", MB_OK | MB_ICONINFORMATION | MB_TOPMOST, 0, 4000);
        else     MessageBoxA(NULL, text, "ModLoader", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    }
    return 0;
}

// ------------------------------------------------------------------ in-game "ModLoader" text
// The game draws menu text through Interface.dll (text drawing function, RVA 0x16270, thiscall, 8 args, ret 0x20;
// 3rd arg = string). The info lines with the game version ("V3.66f | FPS = ... ") get an extra line about the ModLoader.
typedef int (__thiscall *FnDrawText)(void* self, int a1, int a2, const char* str, int a4, int a5, int a6, int a7, int a8);
static BYTE* g_drawTramp = NULL;
static char  g_overlayMsg[600] = "";

static int __fastcall HookDrawText(void* self, void* /*edx*/, int a1, int a2, const char* str, int a4, int a5, int a6, int a7, int a8)
{
    char buf[1400];
    const char* s = str;
    if (str && str[0] == 'V' && str[1] == '3' && str[2] == '.' && g_overlayMsg[0]) {
        size_t n = strlen(str);
        if (n < 700) {
            memcpy(buf, str, n);
            buf[n] = '\n';
            strncpy_s(buf + n + 1, sizeof(buf) - n - 1, g_overlayMsg, _TRUNCATE);
            s = buf;
        }
    }
    return ((FnDrawText)g_drawTramp)(self, a1, a2, s, a4, a5, a6, a7, a8);
}

static void PatchOverlay(HMODULE m)
{
    BYTE* f = (BYTE*)m + 0x16270;
    static const BYTE sig[7] = { 0x83,0xEC,0x58, 0x8B,0x44,0x24,0x60 };
    if (memcmp(f, sig, 7) != 0) { Log("in-game text: different Interface.dll version - skipping"); return; }
    BYTE* t = (BYTE*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return;
    memcpy(t, sig, 7);
    t[7] = 0xE9; *(int*)(t + 8) = (int)((f + 7) - (t + 12));
    g_drawTramp = t;
    BYTE j[7]; j[0] = 0xE9; *(int*)(j + 1) = (int)((BYTE*)&HookDrawText - (f + 5)); j[5] = 0x90; j[6] = 0x90;
    DWORD old;
    if (VirtualProtect(f, 7, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy(f, j, 7);
        VirtualProtect(f, 7, old, &old);
        FlushInstructionCache(GetCurrentProcess(), f, 7);
        Log("in-game text: hook ok");
    } else Log("in-game text: WRITE ERROR");
}

static DWORD WINAPI OverlayThread(LPVOID)
{
    for (int i = 0; i < 120000; i++) {              // Interface.dll loads later - wait up to ~2 min
        HMODULE m = GetModuleHandleA("Interface.dll");
        if (m) { PatchOverlay(m); return 0; }
        Sleep(1);
    }
    Log("in-game text: Interface.dll was not loaded");
    return 0;
}

// ------------------------------------------------------------------ IAT hook: CKContext::Process
// thiscall int CKContext::Process(void) -> __fastcall(ecx=this, edx=garbage), no stack args.
typedef int (__fastcall *FnProcess)(void* ctx, void* edx);
static FnProcess g_origProcess = NULL;
static int       g_initDone = 0;

// separate function without C++ objects (required by __try)
static int SafeCall(FnModCb f, void* ctx, DWORD* code)
{
    __try { f(ctx); return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { *code = GetExceptionCode(); return 0; }
}

// callback timing: log a warning when a mod exceeds WarnMs (asi_loader.ini, default 50 ms);
// max 5 warnings per mod so the log is not flooded. API behavior unchanged.
static LARGE_INTEGER g_qpf;
static int g_warnMs = 50;
static int g_warnCount[MAX_MODS];
static volatile LONG g_inHook = 0;      // re-entry guard (e.g. Process called from inside a callback)

static void RunCallbacks(int which, void* ctx)
{
    for (int i = 0; i < g_cbCount; i++) {
        ModCb& c = g_cb[i];
        if (c.dead) continue;
        FnModCb f = which == 0 ? c.init : which == 1 ? c.pre : c.post;
        if (!f) continue;
        DWORD code = 0;
        LARGE_INTEGER t0, t1; QueryPerformanceCounter(&t0);
        int ok = SafeCall(f, ctx, &code);
        QueryPerformanceCounter(&t1);
        if (!ok) {
            c.dead = 1;
            Log("tick API: exception 0x%08lX in %s (%s) - mod disabled",
                code, c.name, which == 0 ? "ModOnInit" : which == 1 ? "ModOnPreProcess" : "ModOnPostProcess");
        }
        if (g_qpf.QuadPart && g_warnMs > 0) {
            double ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)g_qpf.QuadPart;
            if (ms > g_warnMs && which != 0 && g_warnCount[i] < 5) {
                g_warnCount[i]++;
                Log("tick API: %s took %.1f ms in one frame (%s)", c.name, ms, which == 1 ? "pre" : "post");
            }
        }
    }
}

static int __fastcall HookProcess(void* ctx, void* edx)
{
    // no context or re-entry: original only (no callbacks)
    if (!ctx || InterlockedCompareExchange(&g_inHook, 1, 0) != 0)
        return g_origProcess(ctx, edx);
    if (!g_initDone) { g_initDone = 1; Log("tick API: first frame, CKContext=%p", ctx); RunCallbacks(0, ctx); }
    RunCallbacks(1, ctx);
    int r = g_origProcess(ctx, edx);
    RunCallbacks(2, ctx);
    InterlockedExchange(&g_inHook, 0);
    return r;
}

static void InstallProcessHook()
{
    BYTE* base = (BYTE*)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) { Log("tick API: exe has no imports"); return; }
    for (IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); d->Name; d++) {
        if (_stricmp((const char*)(base + d->Name), "CK2.dll") != 0) continue;
        if (!d->OriginalFirstThunk) { Log("tick API: no OriginalFirstThunk for CK2.dll"); return; }
        IMAGE_THUNK_DATA* names = (IMAGE_THUNK_DATA*)(base + d->OriginalFirstThunk);
        IMAGE_THUNK_DATA* iat   = (IMAGE_THUNK_DATA*)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            const char* fn = (const char*)((IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData))->Name;
            if (strncmp(fn, "?Process@CKContext@@", 20) != 0) continue;
            DWORD old;
            if (!VirtualProtect(&iat->u1.Function, sizeof(DWORD), PAGE_READWRITE, &old)) {
                Log("tick API: VirtualProtect IAT ERROR"); return;
            }
            g_origProcess = (FnProcess)iat->u1.Function;
            iat->u1.Function = (DWORD)(ULONG_PTR)&HookProcess;
            VirtualProtect(&iat->u1.Function, sizeof(DWORD), old, &old);
            Log("tick API: hook %s ok (orig %p)", fn, g_origProcess);
            return;
        }
    }
    Log("tick API: CKContext::Process import not found - callbacks inactive");
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        GetModuleFileNameA(hInst, g_dir, MAX_PATH);
        char* slash = strrchr(g_dir, '\\');
        if (slash) *(slash + 1) = 0;
        _snprintf_s(g_log, sizeof(g_log), _TRUNCATE, "%sasi_loader.log", g_dir);
        LoadAll();
        HANDLE t = CreateThread(NULL, 0, LoaderThread, NULL, 0, NULL);
        if (t) CloseHandle(t);
        {
            char ini[MAX_PATH];
            _snprintf_s(ini, sizeof(ini), _TRUNCATE, "%sasi_loader.ini", g_dir);
            if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES)
                WritePrivateProfileStringA("General", "ShowMessage", "0", ini);
            if (GetPrivateProfileIntA("General", "ShowOverlay", -1, ini) == -1)
                WritePrivateProfileStringA("General", "ShowOverlay", "1", ini);
            if (GetPrivateProfileIntA("General", "TickApi", -1, ini) == -1)
                WritePrivateProfileStringA("General", "TickApi", "1", ini);
            g_warnMs = (int)GetPrivateProfileIntA("General", "WarnMs", 50, ini);
            QueryPerformanceFrequency(&g_qpf);
            if (GetPrivateProfileIntA("General", "TickApi", 1, ini) && g_cbCount > 0)
                InstallProcessHook();
            if (GetPrivateProfileIntA("General", "ShowOverlay", 1, ini)) {
                _snprintf_s(g_overlayMsg, sizeof(g_overlayMsg), _TRUNCATE, "ModLoader active%s%s",
                            g_short[0] ? ": " : "", g_short);
                HANDLE o = CreateThread(NULL, 0, OverlayThread, NULL, 0, NULL);
                if (o) CloseHandle(o);
            }
        }
    }
    return TRUE;
}
