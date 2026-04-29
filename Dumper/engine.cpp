#include "engine.h"
#include "log.h"
#include <unordered_map>
#include <functional>
#include <cstring>
#include <fstream>

using namespace std;

Offsets     defs;
bool        IsUE5 = false;
bool        FNameReversed = false;
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
	bool fnameReversed;
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
	{ "SoTGame",                         { MakeOffsets<SeaOfThieves>(),      false, true,  "4.x",  kDefaultGObjectsSig,  kDefaultGNamesSig } },
	{ "ContraReboot-Win64-Shipping",      { MakeOffsets<ContraReboot>(),      false, false, "4.x",  kDefaultGObjectsSig,  kDefaultGNamesSig } },
	{ "FortniteClient-Win64-Shipping",    { MakeOffsets<Fortnite>(),          false, false, "4.27", kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "HogwartsLegacy-Win64-Shipping",    { MakeOffsets<HogwartsLegacy>(),    false, false, "4.27", kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "DeadIsland2-Win64-Shipping",       { MakeOffsets<DeadIsland2>(),       false, false, "4.27", kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "Chivalry2-Win64-Shipping",         { MakeOffsets<Chivalry2>(),         false, false, "4.26", kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "Generic-UE4",                      { MakeOffsets<GenericUE4Compact>(), false, false, "4.x",  kCompactGObjectsSig,  kDefaultGNamesSig } },
	{ "Generic-UE5",                      { MakeOffsets<GenericUE5>(),        true,  false, "5.x",  kDefaultGObjectsSig,  kDefaultGNamesSig } },
};

static void SyncBuiltinProfiles(const std::string& iniPath) {
	static const unordered_map<string, string> kDesc = {
		{ "SoTGame",                       "Sea of Thieves (Xbox Game Pass, UE4 non-compact FName)" },
		{ "ContraReboot-Win64-Shipping",    "Contra: Rebooted (UE4)"                                },
		{ "FortniteClient-Win64-Shipping",  "Fortnite (UE4.27, compact GObjects)"                   },
		{ "HogwartsLegacy-Win64-Shipping",  "Hogwarts Legacy (UE4.27, compact GObjects)"             },
		{ "DeadIsland2-Win64-Shipping",     "Dead Island 2 (UE4.27, compact GObjects)"               },
		{ "Chivalry2-Win64-Shipping",       "Chivalry 2 (UE4.26, compact GObjects)"                  },
		{ "Generic-UE4",                    "Generic UE4 (compact FName, standard layout)"            },
		{ "Generic-UE5",                    "Generic UE5"                                             },
	};

	{
		ifstream rf(iniPath);
		if (rf.is_open()) return;
	}

	ofstream wf(iniPath);
	if (!wf.is_open()) return;

	static const char* kHeader =
		"; ============================================================\n"
		"; UE Dumper - Game Profiles\n"
		"; Place this file next to Dumper.exe\n"
		";\n"
		"; Format:\n"
		";   [ExeNameWithoutExtension]\n"
		";   UE               = 4.27      ; UE version string (sets IsUE5 if \"5.x\")\n"
		";   IsUE5            = false     ; override UE5 mode explicitly\n"
		";   FNameReversed    = false     ; set true if FName layout is {Number, ComparisonIndex}\n"
		";   <Struct>.<Field> = 0xHEX     ; memory offsets\n"
		";   GObjects.Sig     = XX XX ??  ; IDA-style hex, ?? = wildcard\n"
		";   GObjects.PtrOffset = -4      ; signed byte offset from match to rel32\n"
		";   GObjects.PtrExtra  = 3       ; bytes after rel32 in RIP calculation\n"
		";   GNames.Sig       = ...\n"
		";   GNames.PtrOffset = ...\n"
		";   GNames.PtrExtra  = ...\n"
		"; ============================================================\n";

	static const char* kSep =
		"; -----------------------------------------------------------------------\n";

	wf << kHeader;

	auto kv = [&](const char* key, const string& val) {
		char buf[128];
		snprintf(buf, sizeof(buf), "%-25s= %s\n", key, val.c_str());
		wf << buf;
	};
	auto hex = [](uint16_t v) { char b[8]; snprintf(b, sizeof(b), "0x%X", v); return string(b); };
	auto WriteSig = [&](const char* prefix, const SigSpec& sig) {
		string s;
		for (size_t i = 0; i < sig.Bytes.size(); i++) {
			if (i) s += ' ';
			if (sig.Bytes[i] == 0) s += "??";
			else { char b[4]; snprintf(b, sizeof(b), "%02X", sig.Bytes[i]); s += b; }
		}
		char pk[32], po[32], pe[32];
		snprintf(pk, sizeof(pk), "%s.Sig",       prefix);
		snprintf(po, sizeof(po), "%s.PtrOffset", prefix);
		snprintf(pe, sizeof(pe), "%s.PtrExtra",  prefix);
		kv(pk, s);
		kv(po, to_string(sig.PtrOffset));
		kv(pe, to_string(sig.PtrExtra));
	};

	int added = 0;
	for (auto& [name, p] : builtinProfiles) {
		auto& o = p.offsets;

		auto it  = kDesc.find(name);
		string desc = (it != kDesc.end()) ? it->second : name;

		wf << "\n" << kSep;
		wf << "; " << desc << "\n";
		wf << kSep;
		wf << "[" << name << "]\n";
		kv("UE", p.version);
		if (p.ue5)           kv("IsUE5",         "true");
		if (p.fnameReversed) kv("FNameReversed",  "true");
		kv("FNameEntry.HeaderSize",   hex(o.FNameEntry.HeaderSize));
		kv("UObject.Index",           hex(o.UObject.Index));
		kv("UObject.Class",           hex(o.UObject.Class));
		kv("UObject.Name",            hex(o.UObject.Name));
		kv("UObject.Outer",           hex(o.UObject.Outer));
		kv("UField.Next",             hex(o.UField.Next));
		kv("UStruct.SuperStruct",     hex(o.UStruct.SuperStruct));
		kv("UStruct.Children",        hex(o.UStruct.Children));
		kv("UStruct.PropertiesSize",  hex(o.UStruct.PropertiesSize));
		kv("UEnum.Names",             hex(o.UEnum.Names));
		kv("UEnum.NamesElementSize",  hex(o.UEnum.NamesElementSize));
		kv("UFunction.FunctionFlags", hex(o.UFunction.FunctionFlags));
		kv("UFunction.Func",          hex(o.UFunction.Func));
		kv("UProperty.ArrayDim",      hex(o.UProperty.ArrayDim));
		kv("UProperty.ElementSize",   hex(o.UProperty.ElementSize));
		kv("UProperty.PropertyFlags", hex(o.UProperty.PropertyFlags));
		kv("UProperty.Offset",        hex(o.UProperty.Offset));
		kv("UProperty.Size",          hex(o.UProperty.Size));
		WriteSig("GObjects", p.gobjSig);
		WriteSig("GNames",   p.gnamesSig);
		added++;
	}

	if (added)
		LOG("[config] games.ini: added {} new builtin profile(s)\n", added);
}

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

	IsUE5         = cfg.IsUE5;
	FNameReversed = cfg.FNameReversed;
	UEVersion     = cfg.UEVersion.empty() ? (cfg.IsUE5 ? "5.x" : "4.x") : cfg.UEVersion;

	ActiveGObjectsSig = cfg.GObjects.Bytes.empty() ? kDefaultGObjectsSig : cfg.GObjects;
	ActiveGNamesSig   = cfg.GNames.Bytes.empty()   ? kDefaultGNamesSig   : cfg.GNames;
}

bool EngineInit(std::string game, const std::string& configDir) {
	if (!configDir.empty()) {
		auto iniPath = configDir + "games.ini";
		SyncBuiltinProfiles(iniPath);
		auto cfgs = LoadGameConfigs(iniPath);
		for (auto& c : cfgs) {
			if (c.Name == game) {
				ApplyConfig(c);
				LOG("[config] Loaded '{}' from games.ini  (UE{}{})\n",
					game, UEVersion, IsUE5 ? " UE5-mode" : "");
				return true;
			}
		}
	}

	auto it = builtinProfiles.find(game);
	if (it == builtinProfiles.end()) return false;
	auto& p = it->second;
	defs              = p.offsets;
	IsUE5             = p.ue5;
	FNameReversed     = p.fnameReversed;
	UEVersion         = p.version;
	ActiveGObjectsSig = p.gobjSig;
	ActiveGNamesSig   = p.gnamesSig;
	return true;
}
