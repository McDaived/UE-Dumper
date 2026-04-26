#pragma once
#include <string>
#include <vector>
#include <cstdint>


struct SigSpec {
    std::vector<uint8_t> Bytes;
    int32_t PtrOffset = 0;
    int32_t PtrExtra  = 0;
};

struct GameConfig {
    std::string Name;
    std::string UEVersion;
    bool IsUE5 = false;

    uint16_t FNameEntry_HeaderSize     = 0;
    uint16_t UObject_Index             = 0;
    uint16_t UObject_Class             = 0;
    uint16_t UObject_Name              = 0;
    uint16_t UObject_Outer             = 0;
    uint16_t UField_Next               = 0;
    uint16_t UStruct_SuperStruct       = 0;
    uint16_t UStruct_Children          = 0;
    uint16_t UStruct_PropertiesSize    = 0;
    uint16_t UEnum_Names               = 0;
    uint16_t UEnum_NamesElementSize    = 0;
    uint16_t UFunction_FunctionFlags   = 0;
    uint16_t UFunction_Func            = 0;
    uint16_t UProperty_ArrayDim        = 0;
    uint16_t UProperty_ElementSize     = 0;
    uint16_t UProperty_PropertyFlags   = 0;
    uint16_t UProperty_Offset          = 0;
    uint16_t UProperty_Size            = 0;

    SigSpec GObjects;
    SigSpec GNames;
    bool Valid = false;
};


std::vector<uint8_t> ParseSigString(const std::string& hex);
std::vector<GameConfig> LoadGameConfigs(const std::string& path);
std::string FormatSigBytes(const uint8_t* bytes, size_t len,
                           const std::vector<size_t>& wildcardIdx);
