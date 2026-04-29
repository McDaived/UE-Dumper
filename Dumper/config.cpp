#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>

static std::string Trim(const std::string& s) {
    auto l = s.find_first_not_of(" \t\r\n");
    auto r = s.find_last_not_of(" \t\r\n");
    return (l == std::string::npos) ? "" : s.substr(l, r - l + 1);
}

static uint16_t ParseU16(const std::string& s) {
    return (uint16_t)std::stoul(s, nullptr, 0);
}

std::vector<uint8_t> ParseSigString(const std::string& hex) {
    std::vector<uint8_t> result;
    std::istringstream ss(hex);
    std::string tok;
    while (ss >> tok) {
        if (tok == "??" || tok == "?")
            result.push_back(0x00);
        else
            result.push_back((uint8_t)std::stoul(tok, nullptr, 16));
    }
    return result;
}

std::string FormatSigBytes(const uint8_t* bytes, size_t len,
                            const std::vector<size_t>& wildcardIdx) {
    std::string r;
    for (size_t i = 0; i < len; i++) {
        if (!r.empty()) r += ' ';
        bool wild = false;
        for (auto w : wildcardIdx) if (w == i) { wild = true; break; }
        if (wild) {
            r += "??";
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X", bytes[i]);
            r += buf;
        }
    }
    return r;
}

std::vector<GameConfig> LoadGameConfigs(const std::string& path) {
    std::vector<GameConfig> configs;
    std::ifstream file(path);
    if (!file.is_open()) return configs;

    GameConfig cur;
    auto Flush = [&]() {
        if (!cur.Name.empty()) { cur.Valid = true; configs.push_back(cur); }
        cur = GameConfig{};
    };

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[') {
            Flush();
            auto end = line.find(']');
            if (end != std::string::npos)
                cur.Name = Trim(line.substr(1, end - 1));
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = Trim(line.substr(0, eq));
        auto val = Trim(line.substr(eq + 1));
        if (key.empty() || val.empty()) continue;

        // strip inline comments
        for (char c : { ';', '#' }) {
            auto pos = val.find(c);
            if (pos != std::string::npos) val = Trim(val.substr(0, pos));
        }

        if      (key == "UE")                        { cur.UEVersion = val; cur.IsUE5 = (val[0] == '5'); }
        else if (key == "IsUE5")                     { cur.IsUE5 = (val == "true" || val == "1"); }
        else if (key == "FNameReversed")             { cur.FNameReversed = (val == "true" || val == "1"); }
        else if (key == "FNameEntry.HeaderSize")     cur.FNameEntry_HeaderSize    = ParseU16(val);
        else if (key == "UObject.Index")             cur.UObject_Index            = ParseU16(val);
        else if (key == "UObject.Class")             cur.UObject_Class            = ParseU16(val);
        else if (key == "UObject.Name")              cur.UObject_Name             = ParseU16(val);
        else if (key == "UObject.Outer")             cur.UObject_Outer            = ParseU16(val);
        else if (key == "UField.Next")               cur.UField_Next              = ParseU16(val);
        else if (key == "UStruct.SuperStruct")       cur.UStruct_SuperStruct      = ParseU16(val);
        else if (key == "UStruct.Children")          cur.UStruct_Children         = ParseU16(val);
        else if (key == "UStruct.PropertiesSize")    cur.UStruct_PropertiesSize   = ParseU16(val);
        else if (key == "UEnum.Names")               cur.UEnum_Names              = ParseU16(val);
        else if (key == "UEnum.NamesElementSize")    cur.UEnum_NamesElementSize   = ParseU16(val);
        else if (key == "UFunction.FunctionFlags")   cur.UFunction_FunctionFlags  = ParseU16(val);
        else if (key == "UFunction.Func")            cur.UFunction_Func           = ParseU16(val);
        else if (key == "UProperty.ArrayDim")        cur.UProperty_ArrayDim       = ParseU16(val);
        else if (key == "UProperty.ElementSize")     cur.UProperty_ElementSize    = ParseU16(val);
        else if (key == "UProperty.PropertyFlags")   cur.UProperty_PropertyFlags  = ParseU16(val);
        else if (key == "UProperty.Offset")          cur.UProperty_Offset         = ParseU16(val);
        else if (key == "UProperty.Size")            cur.UProperty_Size           = ParseU16(val);
        else if (key == "GObjects.Sig")              cur.GObjects.Bytes           = ParseSigString(val);
        else if (key == "GObjects.PtrOffset")        cur.GObjects.PtrOffset       = std::stoi(val, nullptr, 0);
        else if (key == "GObjects.PtrExtra")         cur.GObjects.PtrExtra        = std::stoi(val, nullptr, 0);
        else if (key == "GNames.Sig")                cur.GNames.Bytes             = ParseSigString(val);
        else if (key == "GNames.PtrOffset")          cur.GNames.PtrOffset         = std::stoi(val, nullptr, 0);
        else if (key == "GNames.PtrExtra")           cur.GNames.PtrExtra          = std::stoi(val, nullptr, 0);
    }
    Flush();
    return configs;
}
