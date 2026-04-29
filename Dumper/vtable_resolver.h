#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>


constexpr int kDiscoverIndex = -1; 

struct VTblHint {
    const uint8_t* bytes      = nullptr;
    size_t         size       = 0;
    size_t         scanWindow = 0;  // scanWindow > 0: scan within [funcRva, funcRva + scanWindow) // scanWindow = 0: match at funcRva
};

constexpr VTblHint kNoHint{}; 

struct VTblFuncDef {
    const char* funcName;
    const char* className;
    int         index        = kDiscoverIndex;
    int         minIndex     = 3;
    VTblHint    hints[4]     = {};
};

enum class VTblStatus : uint8_t {
    FOUND_IN_VTABLE,
    NOT_VIRTUAL,
    CLASS_NOT_FOUND,
    NO_SEED,
};

struct VTblFuncResult {
    std::string  funcName;
    std::string  resolvedClass;
    int          vtableIndex;
    VTblStatus   status;
    uintptr_t    rva;
    uintptr_t    slotRva;
    int          thunkDepth;
    int          validated;
    bool         heuristic;
    uintptr_t    patternRva  = 0;
    int          minDiscIdx  = 0;
};

const std::vector<VTblFuncDef>& GetVTblFuncDefs();

std::vector<VTblFuncResult> ResolveVTableFunctions(
    const std::vector<VTblFuncDef>&                   defs,
    uint64_t                                           base,
    const uint8_t*                                     imageBytes,
    size_t                                             imageSize,
    const std::unordered_map<std::string, uintptr_t>& patternRvas);

void SaveVTableFuncResults(
    const std::vector<VTblFuncResult>& results,
    const std::string& path,
    uint64_t base);
