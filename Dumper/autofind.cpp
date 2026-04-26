#include "autofind.h"
#include "generic.h"
#include "wrappers.h"
#include "memory.h"
#include "engine.h"
#include "utils.h"
#include <ctime>
#include <cstdio>
#include <cstring>



struct KnownSig {
    std::vector<uint8_t> bytes;
    int32_t ptrExtra;    
    const char* label;
};

static const KnownSig kGObjectsSigs[] = {

    { { 0x4C,0x8B,0x15, 0,0,0,0, 0x8B,0x5D,0x10 },                          3, "UE4-pre25-A  4C 8B 15 ... 8B 5D 10" },
    { { 0x4C,0x8B,0x15, 0,0,0,0, 0x44,0x8B,0x4D },                          3, "UE4-pre25-B  4C 8B 15 ... 44 8B 4D" },
    // ----- UE4.25+ compact-FName -----
    { { 0x4C,0x8B,0x0D, 0,0,0,0, 0x4C,0x8B,0xC3 },                          3, "UE4-comp-A   4C 8B 0D ... 4C 8B C3" },
    { { 0x4C,0x8B,0x0D, 0,0,0,0, 0x48,0x8B,0x0C,0xC8 },                     3, "UE4-comp-B   4C 8B 0D ... 48 8B 0C C8" },
    { { 0x4C,0x8B,0x0D, 0,0,0,0, 0x8B,0x41,0x0C },                          3, "UE4-comp-C   4C 8B 0D ... 8B 41 0C" },
    { { 0x4C,0x8B,0x0D, 0,0,0,0, 0x4C,0x8B,0xD3 },                          3, "UE4-comp-D   4C 8B 0D ... 4C 8B D3" },
    { { 0x4C,0x8B,0x0D, 0,0,0,0, 0x48,0x8B,0xD1 },                          3, "UE4-comp-E   4C 8B 0D ... 48 8B D1" },
    { { 0x4C,0x8B,0x0D, 0,0,0,0, 0x4D,0x85,0xC9 },                          3, "UE4/5-comp-F 4C 8B 0D ... 4D 85 C9" },
    { { 0x48,0x8B,0x0D, 0,0,0,0, 0x48,0x85,0xC9,0x74 },                     3, "UE4-comp-G   48 8B 0D ... 48 85 C9 74" },
    // ----- UE4.26+ / Fortnite variants -----
    { { 0x4C,0x8B,0x05, 0,0,0,0, 0x4C,0x8B,0xC3 },                          3, "UE4-26A      4C 8B 05 ... 4C 8B C3" },
    { { 0x4C,0x8B,0x05, 0,0,0,0, 0x4D,0x85,0xC0 },                          3, "UE4-26B      4C 8B 05 ... 4D 85 C0" },
    { { 0x48,0x89,0x05, 0,0,0,0, 0x48,0x85,0xC0,0x74 },                     3, "UE4-26C      48 89 05 ... 48 85 C0 74" },
    // ----- UE5.0 – 5.3 -----
    { { 0x4C,0x8B,0x15, 0,0,0,0, 0x4D,0x85,0xD2 },                          3, "UE5-A        4C 8B 15 ... 4D 85 D2" },
    { { 0x4C,0x8B,0x15, 0,0,0,0, 0x4C,0x8B,0xCA },                          3, "UE5-B        4C 8B 15 ... 4C 8B CA" },
    { { 0x4C,0x8B,0x0D, 0,0,0,0, 0x49,0x8B,0x41 },                          3, "UE5-C        4C 8B 0D ... 49 8B 41" },
    { { 0x4C,0x8B,0x0D, 0,0,0,0, 0x4D,0x8B,0x41 },                          3, "UE5-D        4C 8B 0D ... 4D 8B 41" },
};

static const KnownSig kGNamesSigs[] = {
    // ----- UE4 (all FName compactness variants) -----
    { { 0x48,0x89,0x1D, 0,0,0,0, 0x48,0x8B,0x5C,0x24,0x20,0x48,0x83,0xC4,0x28 }, 3, "UE4-A  48 89 1D ... 48 8B 5C 24 20 48 83 C4 28" },
    { { 0x48,0x89,0x1D, 0,0,0,0, 0x48,0x8B,0x5C,0x24, 0, 0x48,0x83,0xC4 },       3, "UE4-B  48 89 1D ... 48 8B 5C 24 ?? 48 83 C4" },
    { { 0x48,0x89,0x1D, 0,0,0,0, 0x48,0x8B,0x74,0x24 },                           3, "UE4-C  48 89 1D ... 48 8B 74 24" },
    { { 0x48,0x89,0x1D, 0,0,0,0, 0xEB },                                           3, "UE4-D  48 89 1D ... EB" },
    { { 0x48,0x8B,0x1D, 0,0,0,0, 0x48,0x85,0xDB,0x75 },                           3, "UE4-E  48 8B 1D ... 48 85 DB 75" },
    { { 0x48,0x8B,0x1D, 0,0,0,0, 0x48,0x85,0xDB,0x74 },                           3, "UE4-F  48 8B 1D ... 48 85 DB 74" },
    { { 0x48,0x89,0x35, 0,0,0,0, 0x48,0x8B,0x5C,0x24 },                           3, "UE4-G  48 89 35 ... 48 8B 5C 24" },
    { { 0x48,0x89,0x3D, 0,0,0,0, 0x48,0x8B,0x5C,0x24 },                           3, "UE4-H  48 89 3D ... 48 8B 5C 24" },
    // ----- UE4.25+ -----
    { { 0x48,0x8D,0x05, 0,0,0,0, 0x48,0x89,0x1D, 0,0,0,0 },                      3, "UE4-25A 48 8D 05 ... 48 89 1D" },
    { { 0x48,0x89,0x1D, 0,0,0,0, 0xE8, 0,0,0,0 },                                 3, "UE4-25B 48 89 1D ... E8 ??" },
    // ----- UE5.0 – 5.3 -----
    { { 0x48,0x8D,0x1D, 0,0,0,0, 0x48,0x89,0x1D, 0,0,0,0 },                      3, "UE5-A  48 8D 1D ... 48 89 1D" },
    { { 0x48,0x89,0x1D, 0,0,0,0, 0x48,0x83,0xC4,0x28,0xC3 },                     3, "UE5-B  48 89 1D ... 48 83 C4 28 C3" },
    { { 0x48,0x8B,0x1D, 0,0,0,0, 0x48,0x85,0xDB,0x0F,0x85 },                     3, "UE5-C  48 8B 1D ... 48 85 DB 0F 85" },
};


static bool ValidateGObjects(byte* address) {
    if (!address) return false;
    TUObjectArray arr;
    memcpy(&arr, address, sizeof(arr));
    if (arr.NumElements < 5000   || arr.NumElements > 2'000'000) return false;
    if (arr.MaxElements < arr.NumElements)                        return false;
    if (!arr.Objects)                                             return false;
    byte* firstPtr = Read<byte*>(arr.Objects);
    return firstPtr != nullptr;
}


static bool ValidateGNames(byte* address) {
    if (!address) return false;
    auto* gamePtr = *reinterpret_cast<decltype(GlobalNames)**>(address);
    if (!gamePtr) return false;

    TNameEntryArray names;
    if (!Read(gamePtr, &names, sizeof(names))) return false;
    if (names.NumElements < 500 || names.NumElements > 5'000'000) return false;


    auto saved = GlobalNames;
    GlobalNames = names;
    bool ok = false;
    {
        auto entry = UE_FNameEntry(GlobalNames.GetEntry(0));
        if (entry) {
            auto s = entry.String();
            ok = (s == "None" || s == "none");
        }
    }
    GlobalNames = saved;
    return ok;
}


static bool TryOneSig(byte* start, byte* end,
                       const KnownSig& ks, int32_t ptrOffset,
                       bool isGNames,
                       AutoFindResult& out)
{
    byte* address = (byte*)FindPointer(start, end,
        const_cast<byte*>(ks.bytes.data()), ks.bytes.size(),
        ptrOffset, ks.ptrExtra);
    if (!address) return false;

    bool valid = isGNames ? ValidateGNames(address) : ValidateGObjects(address);
    if (!valid) return false;

    if (isGNames) {
        auto* gamePtr = *reinterpret_cast<decltype(GlobalNames)**>(address);
        Read(gamePtr, &GlobalNames, sizeof(GlobalNames));
    } else {
        ObjObjects = *reinterpret_cast<TUObjectArray*>(address);
    }

    out.found    = true;
    out.sig      = { std::vector<uint8_t>(ks.bytes.begin(), ks.bytes.end()),
                     ptrOffset, ks.ptrExtra };
    out.label    = std::string(ks.label) +
                   (ptrOffset == -4 ? "  [off=-4]" : "  [off=+4]");
    return true;
}



AutoFindResult AutoFindGObjects(byte* start, byte* end)
{
    AutoFindResult result;
    for (auto& sig : kGObjectsSigs) {
        if (TryOneSig(start, end, sig, -4, false, result)) return result;
        if (TryOneSig(start, end, sig, +4, false, result)) return result;
    }
    return result; 
}

AutoFindResult AutoFindGNames(byte* start, byte* end)
{
    AutoFindResult result;
    for (auto& sig : kGNamesSigs) {
        if (TryOneSig(start, end, sig, +4, true, result)) return result;
        if (TryOneSig(start, end, sig, -4, true, result)) return result;
    }
    return result; 
}


static std::string SigToHexString(const std::vector<uint8_t>& bytes) {
    std::string r;
    for (auto b : bytes) {
        if (!r.empty()) r += ' ';
        if (b == 0x00) r += "??";
        else { char buf[4]; snprintf(buf, sizeof(buf), "%02X", b); r += buf; }
    }
    return r;
}

void SaveAutoFoundSigs(const std::string& exeDir,
                        const std::string& gameName,
                        const AutoFindResult& gobj,
                        const AutoFindResult& gnames,
                        const std::string& ueVersion)
{
    if (!gobj.found && !gnames.found) return;

    std::string path = exeDir + gameName + "_sigs.txt";
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (!f) return;

    time_t now = time(nullptr);
    char timeBuf[64]{};
    struct tm tms {};
    localtime_s(&tms, &now);
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tms);

    fprintf(f,
        "; ============================================================\n"
        "; By Daived \n"
        "; Github: https://github.com/McDaived \n"
        "; ============================================================\n"
        "; Auto-discovered signatures for: %s\n"
        "; UE Version: %s\n"
        "; Generated: %s\n"
        "; ============================================================\n"
        ";\n"
        "; Copy the block below into [%s] in games.ini\n"
        "; (create the section if it doesn't exist)\n"
        ";\n",
        gameName.c_str(), ueVersion.c_str(), timeBuf, gameName.c_str());

    fprintf(f, "[%s]\n", gameName.c_str());
    fprintf(f, "UE = %s\n", ueVersion.c_str());
    fprintf(f, "IsUE5 = %s\n", IsUE5 ? "true" : "false");
    fprintf(f, "\n");
    fprintf(f, "; Offsets – fill these in manually or copy from a known profile in games.ini\n");
    fprintf(f, "; FNameEntry.HeaderSize  = 0x??\n");
    fprintf(f, "; UObject.Index          = 0x??\n");
    fprintf(f, "\n");

    if (gobj.found) {
        fprintf(f, "; Pattern matched: %s\n", gobj.label.c_str());
        fprintf(f, "GObjects.Sig       = %s\n",
            SigToHexString(gobj.sig.Bytes).c_str());
        fprintf(f, "GObjects.PtrOffset = %d\n", gobj.sig.PtrOffset);
        fprintf(f, "GObjects.PtrExtra  = %d\n", gobj.sig.PtrExtra);
    } else {
        fprintf(f, "; GObjects: not found — try adding GObjects.Sig manually\n");
    }

    fprintf(f, "\n");

    if (gnames.found) {
        fprintf(f, "; Pattern matched: %s\n", gnames.label.c_str());
        fprintf(f, "GNames.Sig         = %s\n",
            SigToHexString(gnames.sig.Bytes).c_str());
        fprintf(f, "GNames.PtrOffset   = %d\n", gnames.sig.PtrOffset);
        fprintf(f, "GNames.PtrExtra    = %d\n", gnames.sig.PtrExtra);
    } else {
        fprintf(f, "; GNames: not found — try adding GNames.Sig manually\n");
    }

    fprintf(f, "\n"
        "; ============================================================\n"
        "; Once you have the offsets set, remove the comment (;) from the\n"
        "; offset lines and this file will be auto-loaded next run.\n"
        "; ============================================================\n");

    fclose(f);
    printf("  Auto-found sigs saved -> %s\n", path.c_str());
}
