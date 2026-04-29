#pragma once
#include <cstdint>
#include <string>

constexpr float kMinFieldConfidence = 0.65f;

// -- Discovery results 

struct AutoOffsets {
    uint32_t Index  = 0;
    uint32_t Class  = 0;
    uint32_t Name   = 0;
    uint32_t Outer  = 0;

    float confIndex = 0.f;
    float confClass = 0.f;
    float confName  = 0.f;
    float confOuter = 0.f;
    float overall   = 0.f;

    bool valid = false;
};

// -- Per field annotation (for offsets.txt)

enum class OffsetSource : uint8_t {
    Profile,    
    AutoDetect, 
    Missing,    
};

struct OffsetEntry {
    uint16_t     value  = 0;
    OffsetSource source = OffsetSource::Missing;
    float        conf   = 0.f;  
};


struct ResolvedOffsets {
    OffsetEntry FNameEntry_HeaderSize;
    OffsetEntry UObject_Index;
    OffsetEntry UObject_Class;
    OffsetEntry UObject_Name;
    OffsetEntry UObject_Outer;
    OffsetEntry UField_Next;
    OffsetEntry UStruct_SuperStruct;
    OffsetEntry UStruct_Children;
    OffsetEntry UStruct_PropertiesSize;
    OffsetEntry UEnum_Names;
    OffsetEntry UEnum_NamesElementSize;
    OffsetEntry UFunction_FunctionFlags;
    OffsetEntry UFunction_Func;
    OffsetEntry UProperty_ArrayDim;
    OffsetEntry UProperty_ElementSize;
    OffsetEntry UProperty_PropertyFlags;
    OffsetEntry UProperty_Offset;
    OffsetEntry UProperty_Size;
};


AutoOffsets AutoDiscoverOffsets();
ResolvedOffsets ApplyAutoOffsets(const AutoOffsets& ao);

void SaveOffsetsTxt(const ResolvedOffsets& r,
                    const std::string& path,
                    const std::string& gameName,
                    const std::string& ueVersion);
