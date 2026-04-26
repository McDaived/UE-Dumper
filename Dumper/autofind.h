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


AutoFindResult AutoFindGObjects(byte* start, byte* end);
AutoFindResult AutoFindGNames(byte* start, byte* end);

void SaveAutoFoundSigs(const std::string& exeDir,
                        const std::string& gameName,
                        const AutoFindResult& gobj,
                        const AutoFindResult& gnames,
                        const std::string& ueVersion);
