#include "engine.h"
#include <unordered_map>
#include <functional>
#include <cstring>

using namespace std;

Offsets     defs;
bool        IsUE5 = false;
std::string UEVersion = "Unknown";
SigSpec     ActiveGObjectsSig;
SigSpec     ActiveGNamesSig;


struct SeaOfThieves {
	struct { uint16_t HeaderSize = 0xC; }  FNameEntry;
	struct { uint16_t Index = 0x14; uint16_t Class = 0x8; uint16_t Name = 0x24; uint16_t Outer = 0x18; } UObject;
	struct { uint16_t Next = 0x28; }       UField;
	struct { uint16_t SuperStruct = 0x30; uint16_t Children = 0x38; uint16_t PropertiesSize = 0x40; } UStruct;
	struct { uint16_t Names = 0x40; uint16_t NamesElementSize = 0xC; }  UEnum;
	struct { uint16_t FunctionFlags = 0x88; uint16_t Func = 0xB0; }     UFunction;
	struct { uint16_t ArrayDim = 0x40; uint16_t ElementSize = 0x46; uint16_t PropertyFlags = 0x30; uint16_t Offset = 0x48; uint16_t Size = 0x70; } UProperty;
};
static_assert(sizeof(Offsets) == sizeof(SeaOfThieves));

struct ContraReboot {
	struct { uint16_t HeaderSize = 0x10; } FNameEntry;
	struct { uint16_t Index = 0xC; uint16_t Class = 0x10; uint16_t Name = 0x18; uint16_t Outer = 0x20; } UObject;
	struct { uint16_t Next = 0x28; }       UField;
	struct { uint16_t SuperStruct = 0x30; uint16_t Children = 0x38; uint16_t PropertiesSize = 0x40; } UStruct;
	struct { uint16_t Names = 0x40; uint16_t NamesElementSize = 0x10; } UEnum;
	struct { uint16_t FunctionFlags = 0x88; uint16_t Func = 0xB0; }     UFunction;
	struct { uint16_t ArrayDim = 0x38; uint16_t ElementSize = 0x3E; uint16_t PropertyFlags = 0x30; uint16_t Offset = 0x48; uint16_t Size = 0x70; } UProperty;
};
static_assert(sizeof(Offsets) == sizeof(ContraReboot));

struct GenericUE4Compact {
	struct { uint16_t HeaderSize = 0x2; }  FNameEntry;
	struct { uint16_t Index = 0xC; uint16_t Class = 0x10; uint16_t Name = 0x18; uint16_t Outer = 0x20; } UObject;
	struct { uint16_t Next = 0x28; }       UField;
	struct { uint16_t SuperStruct = 0x30; uint16_t Children = 0x38; uint16_t PropertiesSize = 0x40; } UStruct;
	struct { uint16_t Names = 0x40; uint16_t NamesElementSize = 0x10; } UEnum;
	struct { uint16_t FunctionFlags = 0x88; uint16_t Func = 0xB0; }     UFunction;
	struct { uint16_t ArrayDim = 0x38; uint16_t ElementSize = 0x3E; uint16_t PropertyFlags = 0x30; uint16_t Offset = 0x4C; uint16_t Size = 0x70; } UProperty;
};
static_assert(sizeof(Offsets) == sizeof(GenericUE4Compact));

struct Fortnite {
	struct { uint16_t HeaderSize = 0x2; }  FNameEntry;
	struct { uint16_t Index = 0xC; uint16_t Class = 0x10; uint16_t Name = 0x18; uint16_t Outer = 0x20; } UObject;
	struct { uint16_t Next = 0x28; }       UField;
	struct { uint16_t SuperStruct = 0x30; uint16_t Children = 0x38; uint16_t PropertiesSize = 0x40; } UStruct;
	struct { uint16_t Names = 0x40; uint16_t NamesElementSize = 0x10; } UEnum;
	struct { uint16_t FunctionFlags = 0x88; uint16_t Func = 0xB0; }     UFunction;
	struct { uint16_t ArrayDim = 0x38; uint16_t ElementSize = 0x3E; uint16_t PropertyFlags = 0x30; uint16_t Offset = 0x4C; uint16_t Size = 0x78; } UProperty;
};
static_assert(sizeof(Offsets) == sizeof(Fortnite));

struct HogwartsLegacy {
	struct { uint16_t HeaderSize = 0x2; }  FNameEntry;
	struct { uint16_t Index = 0xC; uint16_t Class = 0x10; uint16_t Name = 0x18; uint16_t Outer = 0x20; } UObject;
	struct { uint16_t Next = 0x28; }       UField;
	struct { uint16_t SuperStruct = 0x30; uint16_t Children = 0x38; uint16_t PropertiesSize = 0x40; } UStruct;
	struct { uint16_t Names = 0x40; uint16_t NamesElementSize = 0x10; } UEnum;
	struct { uint16_t FunctionFlags = 0x88; uint16_t Func = 0xB0; }     UFunction;
	struct { uint16_t ArrayDim = 0x38; uint16_t ElementSize = 0x3E; uint16_t PropertyFlags = 0x30; uint16_t Offset = 0x4C; uint16_t Size = 0x70; } UProperty;
};
static_assert(sizeof(Offsets) == sizeof(HogwartsLegacy));

struct DeadIsland2 {
	struct { uint16_t HeaderSize = 0x2; }  FNameEntry;
	struct { uint16_t Index = 0xC; uint16_t Class = 0x10; uint16_t Name = 0x18; uint16_t Outer = 0x20; } UObject;
	struct { uint16_t Next = 0x28; }       UField;
	struct { uint16_t SuperStruct = 0x30; uint16_t Children = 0x38; uint16_t PropertiesSize = 0x40; } UStruct;
	struct { uint16_t Names = 0x40; uint16_t NamesElementSize = 0x10; } UEnum;
	struct { uint16_t FunctionFlags = 0x88; uint16_t Func = 0xB0; }     UFunction;
	struct { uint16_t ArrayDim = 0x38; uint16_t ElementSize = 0x3E; uint16_t PropertyFlags = 0x30; uint16_t Offset = 0x4C; uint16_t Size = 0x70; } UProperty;
};
static_assert(sizeof(Offsets) == sizeof(DeadIsland2));

struct Chivalry2 {
	struct { uint16_t HeaderSize = 0x2; }  FNameEntry;
	struct { uint16_t Index = 0xC; uint16_t Class = 0x10; uint16_t Name = 0x18; uint16_t Outer = 0x20; } UObject;
	struct { uint16_t Next = 0x28; }       UField;
	struct { uint16_t SuperStruct = 0x30; uint16_t Children = 0x38; uint16_t PropertiesSize = 0x40; } UStruct;
	struct { uint16_t Names = 0x40; uint16_t NamesElementSize = 0x10; } UEnum;
	struct { uint16_t FunctionFlags = 0x88; uint16_t Func = 0xB0; }     UFunction;
	struct { uint16_t ArrayDim = 0x38; uint16_t ElementSize = 0x3E; uint16_t PropertyFlags = 0x30; uint16_t Offset = 0x4C; uint16_t Size = 0x70; } UProperty;
};
static_assert(sizeof(Offsets) == sizeof(Chivalry2));

struct GenericUE5 {
	struct { uint16_t HeaderSize = 0x2; }  FNameEntry;
	struct { uint16_t Index = 0xC; uint16_t Class = 0x10; uint16_t Name = 0x18; uint16_t Outer = 0x20; } UObject;
	struct { uint16_t Next = 0x28; }       UField;
	struct { uint16_t SuperStruct = 0x30; uint16_t Children = 0x38; uint16_t PropertiesSize = 0x40; } UStruct;
	struct { uint16_t Names = 0x40; uint16_t NamesElementSize = 0x18; } UEnum;
	struct { uint16_t FunctionFlags = 0x88; uint16_t Func = 0xB0; }     UFunction;
	struct { uint16_t ArrayDim = 0x38; uint16_t ElementSize = 0x3E; uint16_t PropertyFlags = 0x30; uint16_t Offset = 0x4C; uint16_t Size = 0x70; } UProperty;
};
static_assert(sizeof(Offsets) == sizeof(GenericUE5));


static const SigSpec kDefaultGObjectsSig = {
	{ 0x4C, 0x8B, 0x15, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x5D, 0x10 },
	-4, 3
};

static const SigSpec kDefaultGNamesSig = {
	{ 0x48, 0x89, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x5C, 0x24, 0x20, 0x48, 0x83, 0xC4, 0x28 },
	4, 3
};

static const SigSpec kCompactGObjectsSig = {
	{ 0x4C, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x8B, 0xC3 },
	-4, 3
};


struct BuiltinProfile {
	Offsets offsets;
	bool ue5;
	std::string version;
	SigSpec gobjSig;
	SigSpec gnamesSig;
};

template<typename T>
static Offsets MakeOffsets() {
	T buf{};
	static_assert(sizeof(buf) == sizeof(Offsets));
	Offsets out;
	memcpy(&out, &buf, sizeof(out));
	return out;
}

static unordered_map<string, BuiltinProfile> builtinProfiles = {
	{ "SoTGame",                         { MakeOffsets<SeaOfThieves>(),   false, "4.x",  kDefaultGObjectsSig,  kDefaultGNamesSig } },
	{ "ContraReboot-Win64-Shipping",      { MakeOffsets<ContraReboot>(),   false, "4.x",  kDefaultGObjectsSig,  kDefaultGNamesSig } },
	{ "FortniteClient-Win64-Shipping",    { MakeOffsets<Fortnite>(),       false, "4.27", kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "HogwartsLegacy-Win64-Shipping",    { MakeOffsets<HogwartsLegacy>(), false, "4.27", kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "DeadIsland2-Win64-Shipping",       { MakeOffsets<DeadIsland2>(),    false, "4.27", kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "Chivalry2-Win64-Shipping",         { MakeOffsets<Chivalry2>(),      false, "4.26", kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "Generic-UE4",                      { MakeOffsets<GenericUE4Compact>(), false, "4.x", kCompactGObjectsSig, kDefaultGNamesSig } },
	{ "Generic-UE5",                      { MakeOffsets<GenericUE5>(),     true,  "5.x",  kDefaultGObjectsSig,  kDefaultGNamesSig } },
};

// -----------------------------------------------------------------------

static void ApplyConfig(const GameConfig& cfg) {
	defs.FNameEntry.HeaderSize     = cfg.FNameEntry_HeaderSize;
	defs.UObject.Index             = cfg.UObject_Index;
	defs.UObject.Class             = cfg.UObject_Class;
	defs.UObject.Name              = cfg.UObject_Name;
	defs.UObject.Outer             = cfg.UObject_Outer;
	defs.UField.Next               = cfg.UField_Next;
	defs.UStruct.SuperStruct       = cfg.UStruct_SuperStruct;
	defs.UStruct.Children          = cfg.UStruct_Children;
	defs.UStruct.PropertiesSize    = cfg.UStruct_PropertiesSize;
	defs.UEnum.Names               = cfg.UEnum_Names;
	defs.UEnum.NamesElementSize    = cfg.UEnum_NamesElementSize;
	defs.UFunction.FunctionFlags   = cfg.UFunction_FunctionFlags;
	defs.UFunction.Func            = cfg.UFunction_Func;
	defs.UProperty.ArrayDim        = cfg.UProperty_ArrayDim;
	defs.UProperty.ElementSize     = cfg.UProperty_ElementSize;
	defs.UProperty.PropertyFlags   = cfg.UProperty_PropertyFlags;
	defs.UProperty.Offset          = cfg.UProperty_Offset;
	defs.UProperty.Size            = cfg.UProperty_Size;

	IsUE5      = cfg.IsUE5;
	UEVersion  = cfg.UEVersion.empty() ? (cfg.IsUE5 ? "5.x" : "4.x") : cfg.UEVersion;

	ActiveGObjectsSig = cfg.GObjects.Bytes.empty() ? kDefaultGObjectsSig : cfg.GObjects;
	ActiveGNamesSig   = cfg.GNames.Bytes.empty()   ? kDefaultGNamesSig   : cfg.GNames;
}

bool EngineInit(std::string game, const std::string& configDir) {
	if (!configDir.empty()) {
		auto iniPath = configDir + "games.ini";
		auto cfgs = LoadGameConfigs(iniPath);
		for (auto& c : cfgs) {
			if (c.Name == game) {
				ApplyConfig(c);
				printf("[config] Loaded '%s' from games.ini  (UE%s%s)\n",
					game.c_str(), UEVersion.c_str(), IsUE5 ? " UE5-mode" : "");
				return true;
			}
		}
	}

	auto it = builtinProfiles.find(game);
	if (it == builtinProfiles.end()) return false;
	auto& p = it->second;
	defs              = p.offsets;
	IsUE5             = p.ue5;
	UEVersion         = p.version;
	ActiveGObjectsSig = p.gobjSig;
	ActiveGNamesSig   = p.gnamesSig;
	return true;
}
