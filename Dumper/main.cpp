#include <fmt/core.h>
#include "wrappers.h"
#include "utils.h"
#include "memory.h"
#include "engine.h"
#include "config.h"
#include "progress.h"
#include "autofind.h"
#include "PatternScan.h"
#include <iostream>
#include <iomanip>
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
        fmt::print("  GObjects  found  @ section+0x{:08X}  ({} objects)\n",
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
        fmt::print("  GNames    found  @ section+0x{:08X}\n",
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

        fmt::print("  Signatures saved to signatures.txt\n");
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

        std::vector<byte> image;
        std::vector<std::pair<byte*, byte*>> sections;
        {
            auto [base, size] = GetModuleInfo(pid, processName);
            if (!(base && size)) { return MODULE_NOT_FOUND; }

            image.resize(size);
            if (!Read(base, image.data(), size)) { return CANNOT_READ; }
            sections = GetExSections(image.data());
            if (!sections.size()) { return INVALID_IMAGE; }
            Base = reinterpret_cast<uint64_t>(base);
        }


        {
            auto detected = DetectUEVersion(image);
            if (!detected.empty()) {
                if (UEVersion == "Unknown" || UEVersion == "4.x" || UEVersion == "5.x")
                    UEVersion = detected;
                fmt::print("  Detected UE version : {}\n", UEVersion);
            }
            fmt::print("  Game                : {}\n", processName.string());
            fmt::print("  UE5 mode            : {}\n\n", IsUE5 ? "yes" : "no");
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
                fmt::print("  Primary GObjects sig failed — running auto-scan ({} patterns)...\n",
                    16); 
                for (auto& [s, e] : sections) {
                    autoGobj = AutoFindGObjects(s, e);
                    if (autoGobj.found) {
                        byte* sigMatch = FindSignature(s, e,
                            const_cast<byte*>(autoGobj.sig.Bytes.data()),
                            autoGobj.sig.Bytes.size());
                        gobjMatch = { s, sigMatch ? (size_t)(sigMatch - s) : 0u };
                        fmt::print("  Auto-found GObjects via: {}\n", autoGobj.label);
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
                fmt::print("  Primary GNames sig failed — running auto-scan ({} patterns)...\n",
                    13);
                for (auto& [s, e] : sections) {
                    autoGnames = AutoFindGNames(s, e);
                    if (autoGnames.found) {
                        byte* sigMatch = FindSignature(s, e,
                            const_cast<byte*>(autoGnames.sig.Bytes.data()),
                            autoGnames.sig.Bytes.size());
                        gnamesMatch = { s, sigMatch ? (size_t)(sigMatch - s) : 0u };
                        fmt::print("  Auto-found GNames   via: {}\n", autoGnames.label);
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
            fmt::print("  Total names : {}\n\n", size);
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
            fmt::print("  Total objects : {}\n\n", size);
        }

        if (!Full) { return SUCCESS; }

        {
            size_t total   = packages.size();
            size_t erased  = std::erase_if(packages,
                [](auto& pkg) { return pkg.second.size() < 2; });
            fmt::print("  Packages : {}  (wiped {} empty)\n\n", packages.size(), erased);
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
            fmt::print("  Saved : {}/{}\n", saved, packages.size());

            if (!unsaved.empty()) {
                unsaved.erase(unsaved.size() - 2);
                fmt::print("  Empty packages (skipped) : [ {} ]\n", unsaved);
            }
        }

        return SUCCESS;
    }
};

int main(int argc, char* argv[])
{
    puts("=== UE Dumper ===\n");
    auto dumper = Dumper::GetInstance();

    switch (dumper->Init(argc, argv))
    {
    case WINDOW_NOT_FOUND:    puts("Error: can't find UnrealWindow"); return FAILED;
    case PROCESS_NOT_FOUND:   puts("Error: can't get process ID");   return FAILED;
    case READER_ERROR:        puts("Error: can't open process");      return FAILED;
    case CANNOT_GET_PROCNAME: puts("Error: can't get process name");  return FAILED;
    case ENGINE_ERROR:        puts("Error: no offset profile found for this game\n"
                                   "       Add it to games.ini next to the exe"); return FAILED;
    case MODULE_NOT_FOUND:    puts("Error: can't enumerate modules (protected process?)"); return FAILED;
    case CANNOT_READ:         puts("Error: can't read process memory"); return FAILED;
    case INVALID_IMAGE:       puts("Error: can't parse PE sections");   return FAILED;
    case OBJECTS_NOT_FOUND:   puts("Error: GObjects not found (primary + all auto-scan patterns failed)\n"
                                   "       Add a custom GObjects.Sig to games.ini for this game"); return FAILED;
    case NAMES_NOT_FOUND:     puts("Error: GNames not found (primary + all auto-scan patterns failed)\n"
                                   "       Add a custom GNames.Sig to games.ini for this game");   return FAILED;
    case SUCCESS:             break;
    default:                  return FAILED;
    }

    puts("");
    switch (dumper->Dump())
    {
    case FILE_NOT_OPEN:  puts("Error: can't open output file"); return FAILED;
    case ZERO_PACKAGES:  puts("Error: zero packages found");    return FAILED;
    case SUCCESS:        break;
    default:             return FAILED;
    }

    puts("\nDone.");
    return SUCCESS;
}
