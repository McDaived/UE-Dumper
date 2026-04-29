#pragma once
#include <Windows.h>
#include "config.h"
#include <string>
#include <vector>
#include <cstdint>

struct AutoFindResult {
    bool     found   = false;
    SigSpec  sig;
    std::string label;
};

struct FuncResult {
    bool        found  = false;
    uintptr_t   rva    = 0;
    std::string label;
    std::string name;   
    std::string source = "pattern"; 
};


AutoFindResult AutoFindGObjects(byte* start, byte* end);
AutoFindResult AutoFindGNames(byte* start, byte* end);

void AutoFindAllFunctions(byte* imageBase, byte* start, byte* end,
                          std::vector<FuncResult>& results);

void SaveAutoFoundSigs(const std::string& exeDir,
                        const std::string& gameName,
                        const AutoFindResult& gobj,
                        const AutoFindResult& gnames,
                        const std::string& ueVersion);
