#include <fmt/core.h>
#include "wrappers.h"
#include "utils.h"
#include "memory.h"
#include "engine.h"
#include "config.h"
#include "progress.h"
#include "autofind.h"
#include "PatternScan.h"
#include "vtable_resolver.h"
#include "autooffsets.h"
#include "log.h"
#include <iostream>
#include <iomanip>
#include <unordered_set>
namespace fs = std::filesystem;

enum {
    SUCCESS,
    FAILED,
    WINDOW_NOT_FOUND,
    PROCESS_NOT_FOUND,
    READER_ERROR,
    CANNOT_GET_PROCNAME,
    ENGINE_ERROR,
    MODULE_NOT_FOUND,
    CANNOT_READ,
    INVALID_IMAGE,
    NAMES_NOT_FOUND,
    OBJECTS_NOT_FOUND,
    FILE_NOT_OPEN,
    ZERO_PACKAGES
};

class Dumper
{
protected:
    bool Full = true;
    bool Wait = false;
    fs::path Directory;
    fs::path ExeRoot;
    std::vector<byte>       Image;
    std::vector<FuncResult> FoundFuncs;
    ResolvedOffsets         OffsetInfo;

private:
    Dumper() {};

    static bool FindObjObjects(byte* start, byte* end,
                                const SigSpec& sig,
                                std::pair<byte*, size_t>& matchInfo)
    {
        const byte* sigBytes = sig.Bytes.data();
        size_t      sigLen   = sig.Bytes.size();

        byte* sigMatch = FindSignature(start, end,
            const_cast<byte*>(sigBytes), sigLen);
        if (!sigMatch) return false;

        byte* address = (byte*)FindPointer(start, end,
            const_cast<byte*>(sigBytes), sigLen,
            sig.PtrOffset, sig.PtrExtra);
        if (!address) return false;

        ObjObjects = *reinterpret_cast<decltype(ObjObjects)*>(address);
        if (!ObjObjects.NumElements) return false;

        matchInfo = { start, (size_t)(sigMatch - start) };
        LOG("  GObjects  found  @ section+0x{:08X}  ({} objects)\n",
            (uintptr_t)(address - start), ObjObjects.NumElements);
        return true;
    }

    static bool FindGlobalNames(byte* start, byte* end,
                                 const SigSpec& sig,
                                 std::pair<byte*, size_t>& matchInfo)
    {
        const byte* sigBytes = sig.Bytes.data();
        size_t      sigLen   = sig.Bytes.size();

        byte* sigMatch = FindSignature(start, end,
            const_cast<byte*>(sigBytes), sigLen);
        if (!sigMatch) return false;

        byte* address = (byte*)FindPointer(start, end,
            const_cast<byte*>(sigBytes), sigLen,
            sig.PtrOffset, sig.PtrExtra);
        if (!address) return false;

        Read(*reinterpret_cast<decltype(GlobalNames)**>(address), &GlobalNames, sizeof(GlobalNames));

        matchInfo = { start, (size_t)(sigMatch - start) };
        LOG("  GNames    found  @ section+0x{:08X}\n",
            (uintptr_t)(address - start));
        return true;
    }


    void SaveGeneratedSignatures(
        const std::pair<byte*, size_t>& gobjMatch,
        const std::pair<byte*, size_t>& gnamesMatch) const
    {
        File f(Directory / "signatures.txt", "w");
        if (!f) return;

        auto WriteEntry = [&](const char* label,
                               const std::pair<byte*, size_t>& match,
                               const SigSpec& sig)
        {
            const auto& bytes = sig.Bytes;
            fmt::print(f, "# {}\n", label);
            fmt::print(f, "# Section offset: 0x{:08X}\n", match.second);

            std::string primary;
            for (size_t i = 0; i < bytes.size(); i++) {
                if (!primary.empty()) primary += ' ';
                primary += (bytes[i] == 0x00) ? "??" : fmt::format("{:02X}", bytes[i]);
            }
            fmt::print(f, "Primary  [{}]:  {}\n", bytes.size(), primary);

            std::string extended = GenerateExtendedSig(
                match.first, match.second,
                bytes.data(), bytes.size(), 16);
            fmt::print(f, "Extended [{}]:  {}\n\n", bytes.size() + 16, extended);
        };

        fmt::print(f, "# Auto-generated signatures — {}\n\n", UEVersion);
        WriteEntry("GObjects", gobjMatch, ActiveGObjectsSig);
        WriteEntry("GNames",   gnamesMatch, ActiveGNamesSig);

        LOG("  Signatures saved to signatures.txt\n");
    }

    void SaveFuncAddresses() const
    {
        File f(Directory / "funcs.txt", "w");
        if (!f) return;

        fmt::print(f,
            "; ============================================================\n"
            "; UE Function addresses — {}\n"
            "; Base: 0x{:X}\n"
            ";\n"
            "; RVA = offset from module base (stable across game restarts)\n"
            "; VA  = Base + RVA              (actual runtime address)\n"
            "; ============================================================\n\n",
            UEVersion, Base);

        size_t found = 0;
        for (auto& r : FoundFuncs) {
            if (r.found) {
                fmt::print(f, "{:<35}  RVA=0x{:08X}   VA=0x{:X}\n",
                    r.name, r.rva, Base + r.rva);
                fmt::print(f, "  ; source: [{}] {}\n\n", r.source, r.label);
                found++;
            } else {
                fmt::print(f, "{:<35}  NOT FOUND — sig needs update for this game/version\n\n",
                    r.name);
            }
        }

        LOG("  Function addresses saved ({}/{}) -> funcs.txt\n", found, FoundFuncs.size());
    }

    void DumpVTables() const
    {
        File f(Directory / "vtables.txt", "w");
        if (!f) return;

        const uint64_t imageEnd = Base + Image.size();

        fmt::print(f,
            "; ============================================================\n"
            "; VTable dump — {}\n"
            "; Base: 0x{:X}\n"
            ";\n"
            "; All addresses are RVAs (offset from module base).\n"
            "; Reconstruct VA at runtime: Base + RVA\n"
            "; ============================================================\n\n",
            UEVersion, Base);

        std::unordered_set<uintptr_t> seen;
        size_t count = 0;

        ObjObjects.Dump([&](byte* object) {
            uintptr_t vtbl = Read<uintptr_t>((void*)object);
            if (!vtbl || vtbl < Base || vtbl >= imageEnd) return;

            uintptr_t vtbl_rva = vtbl - Base;
            if (!seen.insert(vtbl_rva).second) return;

            UE_UObject obj(object);
            UE_UClass  cls = obj.GetClass();
            std::string className = cls ? cls.GetFullName() : "Unknown";
            uint32_t    objIndex  = obj.GetIndex();

            std::vector<uintptr_t> slots;
            slots.reserve(64);
            for (int i = 0; i < 2048; i++) {
                uintptr_t slot = Read<uintptr_t>((void*)(vtbl + (uintptr_t)i * 8));
                if (slot < Base || slot >= imageEnd) break;
                slots.push_back(slot - Base);
            }

            fmt::print(f, "[{:0>6}] {}\n  vtable=0x{:X}  slots={}\n",
                objIndex, className, vtbl_rva, slots.size());
            for (size_t i = 0; i < slots.size(); i++)
                fmt::print(f, "  [{:3}] 0x{:X}\n", i, slots[i]);
            fmt::print(f, "\n");
            count++;
        });

        LOG("  VTables dumped: {} unique vtables -> vtables.txt\n", count);
    }

public:

    static Dumper* GetInstance() {
        static Dumper dumper;
        return &dumper;
    }

    int Init(int argc, char* argv[]) {

        for (auto i = 1; i < argc; i++) {
            auto arg = argv[i];
            if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
                puts("  -p   partial dump (Names + Objects only, no SDK)\n"
                     "  -w   wait for input before processing");
                return FAILED;
            }
            else if (!strcmp(arg, "-p")) { Full = false; }
            else if (!strcmp(arg, "-w")) { Wait = true; }
        }

        if (Wait) { system("pause"); }


        uint32_t pid = 0;
        {
            HWND hWnd = FindWindowA("UnrealWindow", nullptr);
            if (!hWnd) { return WINDOW_NOT_FOUND; };
            GetWindowThreadProcessId(hWnd, reinterpret_cast<DWORD*>(&pid));
            if (!pid) { return PROCESS_NOT_FOUND; };
        }

        if (!ReaderInit(pid)) { return READER_ERROR; };

        fs::path processName;
        {
            wchar_t processPath[MAX_PATH]{};
            if (!GetProccessPath(pid, processPath, MAX_PATH)) { return CANNOT_GET_PROCNAME; };
            processName = fs::path(processPath).filename();
        }

        {
            ExeRoot = fs::path(argv[0]); ExeRoot.remove_filename();
            auto game = processName.stem();
            Directory = ExeRoot / "Games" / game;
            fs::create_directories(Directory);
            if (!EngineInit(game.string(), ExeRoot.string())) { return ENGINE_ERROR; };
        }

        std::vector<std::pair<byte*, byte*>> sections;
        {
            auto [base, size] = GetModuleInfo(pid, processName);
            if (!(base && size)) { return MODULE_NOT_FOUND; }

            Image.resize(size);
            if (!Read(base, Image.data(), size)) { return CANNOT_READ; }
            sections = GetExSections(Image.data());
            if (!sections.size()) { return INVALID_IMAGE; }
            Base = reinterpret_cast<uint64_t>(base);
        }


        {
            auto detected = DetectUEVersion(Image);
            if (!detected.empty()) {
                if (UEVersion == "Unknown" || UEVersion == "4.x" || UEVersion == "5.x")
                    UEVersion = detected;
                LOG("  Detected UE version : {}\n", UEVersion);
            }
            LOG("  Game                : {}\n", processName.string());
            LOG("  UE5 mode            : {}\n\n", IsUE5 ? "yes" : "no");
        }


        std::pair<byte*, size_t> gobjMatch, gnamesMatch;
        AutoFindResult autoGobj, autoGnames;
        std::string gameName = processName.stem().string();


        {
            bool found = false;
            for (auto& [s, e] : sections) {
                if (FindObjObjects(s, e, ActiveGObjectsSig, gobjMatch)) { found = true; break; }
            }
            if (!found) {
                LOGW("  Primary GObjects sig failed — running auto-scan ({} patterns)...\n", 16);
                for (auto& [s, e] : sections) {
                    autoGobj = AutoFindGObjects(s, e);
                    if (autoGobj.found) {
                        byte* sigMatch = FindSignature(s, e,
                            const_cast<byte*>(autoGobj.sig.Bytes.data()),
                            autoGobj.sig.Bytes.size());
                        gobjMatch = { s, sigMatch ? (size_t)(sigMatch - s) : 0u };
                        LOG("  Auto-found GObjects via: {}\n", autoGobj.label);
                        break;
                    }
                }
                if (!autoGobj.found) { return OBJECTS_NOT_FOUND; }
            }
        }

        {
            bool found = false;
            for (auto& [s, e] : sections) {
                if (FindGlobalNames(s, e, ActiveGNamesSig, gnamesMatch)) { found = true; break; }
            }
            if (!found) {
                LOGW("  Primary GNames sig failed — running auto-scan ({} patterns)...\n", 13);
                for (auto& [s, e] : sections) {
                    autoGnames = AutoFindGNames(s, e);
                    if (autoGnames.found) {
                        byte* sigMatch = FindSignature(s, e,
                            const_cast<byte*>(autoGnames.sig.Bytes.data()),
                            autoGnames.sig.Bytes.size());
                        gnamesMatch = { s, sigMatch ? (size_t)(sigMatch - s) : 0u };
                        LOG("  Auto-found GNames   via: {}\n", autoGnames.label);
                        break;
                    }
                }
                if (!autoGnames.found) { return NAMES_NOT_FOUND; }
            }
        }

        SaveGeneratedSignatures(gobjMatch, gnamesMatch);

        if (!autoGobj.found)   autoGobj   = { true, ActiveGObjectsSig, "primary (from config/built-in)" };
        if (!autoGnames.found) autoGnames = { true, ActiveGNamesSig,   "primary (from config/built-in)" };
        SaveAutoFoundSigs(ExeRoot.string(), gameName, autoGobj, autoGnames, UEVersion);

        {
            LOG("  Running runtime offset discovery...\n");
            auto ao  = AutoDiscoverOffsets();
            OffsetInfo = ApplyAutoOffsets(ao);
        }

        {
            LOG("  Scanning for UE engine functions...\n");
            for (auto& [s, e] : sections)
                AutoFindAllFunctions(Image.data(), s, e, FoundFuncs);

            size_t found = 0;
            for (auto& r : FoundFuncs) {
                if (r.found) {
                    LOG("  [{:>35}]  RVA=0x{:X}  ({})\n", r.name, r.rva, r.label);
                    found++;
                } else {
                    LOGW("  [{:>35}]  NOT FOUND\n", r.name);
                }
            }
            LOG("  Functions found: {}/{}\n", found, FoundFuncs.size());
        }

        return SUCCESS;
    }

    int Dump() {
        {
            File file(Directory / "Names.txt", "w");
            if (!file) { return FILE_NOT_OPEN; }
            size_t size = 0;
            {
                ProgressBar bar("Names", (uint64_t)GlobalNames.NumElements);
                GlobalNames.Dump([&](std::string_view name, uint32_t id) {
                    fmt::print(file, "[{:0>6}] {}\n", id, name);
                    bar.Tick();
                    size++;
                });
                bar.Finish();
            }
            LOG("  Total names : {}\n\n", size);
        }

        std::unordered_map<byte*, std::vector<UE_UObject>> packages;
        {
            File file(Directory / "Objects.txt", "w");
            if (!file) { return FILE_NOT_OPEN; }
            size_t size = 0;
            {
                ProgressBar bar("Objects", (uint64_t)ObjObjects.NumElements);
                if (Full) {
                    ObjObjects.Dump([&](UE_UObject object) {
                        fmt::print(file, "[{:0>6}] <{}> {}\n",
                            object.GetIndex(), object.GetAddress(), object.GetFullName());
                        if (object.IsA<UE_UStruct>() || object.IsA<UE_UEnum>())
                            packages[object.GetPackageObject()].push_back(object);
                        bar.Tick();
                        size++;
                    });
                } else {
                    ObjObjects.Dump([&](UE_UObject object) {
                        fmt::print(file, "[{:0>6}] <{}> {}\n",
                            object.GetIndex(), object.GetAddress(), object.GetFullName());
                        bar.Tick();
                        size++;
                    });
                }
                bar.Finish();
            }
            LOG("  Total objects : {}\n\n", size);
        }

        if (!Full) { return SUCCESS; }

        {
            size_t total   = packages.size();
            size_t erased  = std::erase_if(packages,
                [](auto& pkg) { return pkg.second.size() < 2; });
            LOG("  Packages : {}  (wiped {} empty)\n\n", packages.size(), erased);
        }

        if (!packages.size()) { return ZERO_PACKAGES; }

        {
            auto path = Directory / "SOT SDK";
            fs::create_directories(path);

            int saved = 0;
            int i = 1;
            std::string unsaved;

            ProgressBar bar("Packages", (uint64_t)packages.size());
            for (UE_UPackage package : packages) {
                bar.Update(i++);
                package.Process();
                if (package.Save(path)) {
                    saved++;
                } else {
                    unsaved += package.GetObject().GetName() + ", ";
                }
            }
            bar.Finish();
            LOG("  Saved : {}/{}\n", saved, packages.size());

            if (!unsaved.empty()) {
                unsaved.erase(unsaved.size() - 2);
                LOGW("  Empty packages (skipped) : [ {} ]\n", unsaved);
            }
        }

        {
            constexpr uintptr_t kCloseThreshold = 0x1000;

            std::unordered_map<std::string, uintptr_t> patternRvas;
            for (auto& fr : FoundFuncs)
                if (fr.found && fr.source == "pattern")
                    patternRvas[fr.name] = fr.rva;

            auto vresults = ResolveVTableFunctions(
                GetVTblFuncDefs(), Base,
                Image.data(), Image.size(),
                patternRvas);

            for (auto& vr : vresults)
            {
                if (vr.status != VTblStatus::FOUND_IN_VTABLE) continue;

                bool merged = false;
                for (auto& fr : FoundFuncs)
                {
                    if (fr.name != vr.funcName) continue;
                    merged = true;

                    if (fr.found)
                    {
                        uintptr_t oldRva = fr.rva;
                        uintptr_t delta  = oldRva > vr.rva
                                           ? oldRva - vr.rva
                                           : vr.rva - oldRva;

                        if (delta == 0) {
                            LOG("  [vtable=pattern]  {:<35}  RVA=0x{:X}\n",
                                fr.name, vr.rva);
                        } else if (delta < kCloseThreshold) {
                            LOG("  [vtable>pattern~] {:<35}  vtable=0x{:X}  pattern=0x{:X}  delta=0x{:X}\n",
                                fr.name, vr.rva, oldRva, delta);
                        } else {
                            LOGW("  [vtable>pattern!] {:<35}  vtable=0x{:X}  pattern=0x{:X}  delta=0x{:X}\n",
                                fr.name, vr.rva, oldRva, delta);
                        }

                        fr.rva    = vr.rva;
                        fr.source = "vtable>pattern";
                        fr.label  = fmt::format(
                            "{} vtable[0x{:X}] (pattern was 0x{:X}, delta=0x{:X})",
                            vr.resolvedClass, vr.vtableIndex, oldRva, delta);
                    }
                    else
                    {
                        LOG("  [vtable only]     {:<35}  RVA=0x{:X}  (pattern not found)\n",
                            fr.name, vr.rva);
                        fr.found  = true;
                        fr.rva    = vr.rva;
                        fr.source = "vtable";
                        fr.label  = fmt::format(
                            "{} vtable[0x{:X}]", vr.resolvedClass, vr.vtableIndex);
                    }
                    break;
                }

                LOG("  [vtable index]    {:<35}  index=0x{:X}  [{}]\n",
                    vr.funcName, vr.vtableIndex,
                    vr.heuristic ? "heuristic" : "pattern-seeded");

                if (!merged)
                {
                    LOG("  [vtable new]      {:<35}  RVA=0x{:X}\n", vr.funcName, vr.rva);
                    FuncResult nr;
                    nr.found  = true;
                    nr.rva    = vr.rva;
                    nr.source = "vtable";
                    nr.label  = fmt::format(
                        "{} vtable[0x{:X}]", vr.resolvedClass, vr.vtableIndex);
                    nr.name   = vr.funcName;
                    FoundFuncs.push_back(std::move(nr));
                }
            }

            SaveVTableFuncResults(vresults,
                (Directory / "vtable_funcs.txt").string(), Base);
        }

        SaveFuncAddresses();
        DumpVTables();
        SaveOffsetsTxt(OffsetInfo,
            (Directory / "offsets.txt").string(),
            Directory.filename().string(),
            UEVersion);

        return SUCCESS;
    }
};

int main(int argc, char* argv[])
{
    {
        fs::path logDir = fs::path(argc > 0 ? argv[0] : ".").remove_filename();
        if (!logDir.empty()) fs::create_directories(logDir);
        LogInit((logDir.empty() ? fs::path(".") : logDir).string() + "logs.txt");
    }

    LOG("=== UE Dumper ===\n\n");
    auto dumper = Dumper::GetInstance();

    switch (dumper->Init(argc, argv))
    {
    case WINDOW_NOT_FOUND:    LOGE("Error: can't find UnrealWindow\n");                                                   LogClose(); return FAILED;
    case PROCESS_NOT_FOUND:   LOGE("Error: can't get process ID\n");                                                      LogClose(); return FAILED;
    case READER_ERROR:        LOGE("Error: can't open process\n");                                                         LogClose(); return FAILED;
    case CANNOT_GET_PROCNAME: LOGE("Error: can't get process name\n");                                                    LogClose(); return FAILED;
    case ENGINE_ERROR:        LOGE("Error: no offset profile found for this game\n       Add it to games.ini next to the exe\n"); LogClose(); return FAILED;
    case MODULE_NOT_FOUND:    LOGE("Error: can't enumerate modules (protected process?)\n");                              LogClose(); return FAILED;
    case CANNOT_READ:         LOGE("Error: can't read process memory\n");                                                 LogClose(); return FAILED;
    case INVALID_IMAGE:       LOGE("Error: can't parse PE sections\n");                                                   LogClose(); return FAILED;
    case OBJECTS_NOT_FOUND:   LOGE("Error: GObjects not found (primary + all auto-scan patterns failed)\n       Add a custom GObjects.Sig to games.ini for this game\n"); LogClose(); return FAILED;
    case NAMES_NOT_FOUND:     LOGE("Error: GNames not found (primary + all auto-scan patterns failed)\n       Add a custom GNames.Sig to games.ini for this game\n");    LogClose(); return FAILED;
    case SUCCESS:             break;
    default:                  LogClose(); return FAILED;
    }

    LOG("\n");
    switch (dumper->Dump())
    {
    case FILE_NOT_OPEN:  LOGE("Error: can't open output file\n"); LogClose(); return FAILED;
    case ZERO_PACKAGES:  LOGE("Error: zero packages found\n");    LogClose(); return FAILED;
    case SUCCESS:        break;
    default:             LogClose(); return FAILED;
    }

    LOG("\nDone.\n");
    LogClose();
    return SUCCESS;
}
