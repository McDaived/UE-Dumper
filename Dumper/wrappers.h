#pragma once
#include "generic.h"
#include <filesystem>

namespace fs = std::filesystem;


class File {
private:
	FILE* file;
public:
	File(fs::path path, const char* mode) { fopen_s(&file, path.string().c_str(), mode); }
	~File() { if (file) { fclose(file); } }
	operator bool() const { return file != nullptr; }
	operator FILE* () { return file; }
};


class UE_FNameEntry {
protected:
	byte* object;
public:
	UE_FNameEntry(byte* object) : object(object) {}
	UE_FNameEntry() : object(nullptr) {}


	std::string String() const;


	void String(char* buf, uint16_t len) const;
	operator bool() const { return object != nullptr; }

};

class UE_FName {
protected:
	byte* object;
public:
	UE_FName(byte* object) : object(object) {}
	UE_FName() : object(nullptr) {}

	std::string GetName() const;
};

class UE_UClass;

class UE_UObject
{
protected:
	byte* object;
public:
	UE_UObject(byte* object) : object(object) {}
	UE_UObject() : object(nullptr) {}
	bool operator==(const UE_UObject& obj) const { return obj.object == object; };
	bool operator!=(const UE_UObject& obj) const { return obj.object != object; };
	uint32_t GetIndex() const;
	UE_UClass GetClass() const;
	UE_UObject GetOuter() const;
	UE_UObject GetPackageObject() const;
	std::string GetName() const;
	std::string GetFullName() const;
	std::string GetCppName() const;
	void* GetAddress() const { return object; }
	operator byte* () const { return object; };
	operator bool() const { return object != nullptr; }

	template<typename Base>
	Base Cast() const { return Base(object); }

	template<typename T>
	bool IsA() const;

	static UE_UClass StaticClass();
};

class UE_AActor : public UE_UObject
{
public:
	static UE_UClass StaticClass();
};

class UE_UField : public UE_UObject
{
public:
	using UE_UObject::UE_UObject;
	UE_UField GetNext() const;
	static UE_UClass StaticClass();
};

class UE_UStruct : public UE_UField
{
public:
	using UE_UField::UE_UField;

	UE_UStruct GetSuper() const;
	UE_UField GetChildren() const;
	int32_t GetSize() const;
	static UE_UClass StaticClass();
};

enum EFunctionFlags : uint32_t
{

	FUNC_None = 0x00000000,

	FUNC_Final = 0x00000001,	
	FUNC_RequiredAPI = 0x00000002,	
	FUNC_BlueprintAuthorityOnly = 0x00000004,   
	FUNC_BlueprintCosmetic = 0x00000008,   
	FUNC_Net = 0x00000040,   
	FUNC_NetReliable = 0x00000080,   
	FUNC_NetRequest = 0x00000100,	
	FUNC_Exec = 0x00000200,	
	FUNC_Native = 0x00000400,	
	FUNC_Event = 0x00000800,   
	FUNC_NetResponse = 0x00001000,   
	FUNC_Static = 0x00002000,   
	FUNC_NetMulticast = 0x00004000,	
	FUNC_UbergraphFunction = 0x00008000,   
	FUNC_MulticastDelegate = 0x00010000,	
	FUNC_Public = 0x00020000,	
	FUNC_Private = 0x00040000,	
	FUNC_Protected = 0x00080000,	
	FUNC_Delegate = 0x00100000,	
	FUNC_NetServer = 0x00200000,	
	FUNC_HasOutParms = 0x00400000,	
	FUNC_HasDefaults = 0x00800000,	
	FUNC_NetClient = 0x01000000,	
	FUNC_DLLImport = 0x02000000,	
	FUNC_BlueprintCallable = 0x04000000,	
	FUNC_BlueprintEvent = 0x08000000,	
	FUNC_BlueprintPure = 0x10000000,	
	FUNC_EditorOnly = 0x20000000,	
	FUNC_Const = 0x40000000,	
	FUNC_NetValidate = 0x80000000,	

	FUNC_AllFlags = 0xFFFFFFFF,
};

class UE_UFunction : public UE_UStruct
{
public:
	using UE_UStruct::UE_UStruct;
	uint64_t GetFunc() const;
	std::string GetFunctionFlags() const;
	static UE_UClass StaticClass();
};

class UE_UScriptStruct : public UE_UStruct
{
public:
	using UE_UStruct::UE_UStruct;
	static UE_UClass StaticClass();
};

class UE_UClass : public UE_UStruct {
public:
	using UE_UStruct::UE_UStruct;
	static UE_UClass StaticClass();
};

class UE_UEnum : public UE_UField
{
public:
	using UE_UField::UE_UField;
	TArray GetNames() const;
	static UE_UClass StaticClass();
};

enum class PropertyType {
	Unknown,
	StructProperty,
	ObjectProperty,
	SoftObjectProperty,
	FloatProperty,
	ByteProperty,
	BoolProperty,
	IntProperty,
	Int8Property,
	Int16Property,
	Int64Property,
	UInt16Property,
	UInt32Property,
	UInt64Property,
	NameProperty,
	DelegateProperty,
	SetProperty,
	ArrayProperty,
	WeakObjectProperty,
	StrProperty,
	TextProperty,
	MulticastSparseDelegateProperty,
	EnumProperty,
	DoubleProperty,
	MulticastDelegateProperty,
	ClassProperty,
	MulticastInlineDelegateProperty,
	MapProperty,
	InterfaceProperty
};

class UE_UProperty : public UE_UField
{
public:
	using UE_UField::UE_UField;

	int32_t GetArrayDim() const;
	int32_t GetSize() const;
	int32_t GetOffset() const;
	uint64_t GetPropertyFlags() const;
	std::pair<PropertyType, std::string> GetType() const;
};

class UE_UByteProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;

	UE_UEnum GetEnum() const;
	std::string GetType() const;

	static UE_UClass StaticClass();
};



class UE_UInt16Property : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};


class UE_UInt32Property : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};


class UE_UInt64Property : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_Int8Property : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_Int16Property : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_IntProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_Int64Property : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_FloatProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_DoubleProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_BoolProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	uint8_t GetFieldMask() const;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_ObjectPropertyBase : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	UE_UClass GetPropertyClass() const;
	static UE_UClass StaticClass();
};


class UE_ObjectProperty : public UE_ObjectPropertyBase
{
public:
	using UE_ObjectPropertyBase::UE_ObjectPropertyBase;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_ClassProperty : public UE_ObjectProperty
{
public:
	using UE_ObjectProperty::UE_ObjectProperty;
	UE_UClass GetMetaClass() const;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_InterfaceProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	UE_UClass GetInterfaceClass() const;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_WeakObjectProperty : public UE_ObjectPropertyBase
{
public:
	using UE_ObjectPropertyBase::UE_ObjectPropertyBase;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_LazyObjectProperty : public UE_ObjectPropertyBase
{
public:
	using UE_ObjectPropertyBase::UE_ObjectPropertyBase;
	std::string GetType() const;

	static UE_UClass StaticClass();
};

class UE_AssetObjectProperty : public UE_ObjectPropertyBase
{
public:
	using UE_ObjectPropertyBase::UE_ObjectPropertyBase;
	std::string GetType() const;

	static UE_UClass StaticClass();
};

class UE_AssetClassProperty : public UE_AssetObjectProperty
{
public:
	using UE_AssetObjectProperty::UE_AssetObjectProperty;

	UE_UClass GetMetaClass() const;
	std::string GetType() const;

	static UE_UClass StaticClass();
};

class UE_NameProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;

	static UE_UClass StaticClass();
};

class UE_StructProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;

	UE_UStruct GetStruct() const;
	std::string GetType() const;

	static UE_UClass StaticClass();
};

class UE_StrProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;

	static UE_UClass StaticClass();
};

class UE_TextProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;

	static UE_UClass StaticClass();
};

class UE_ArrayProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;

	UE_UProperty GetInner() const;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_MapProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;

	UE_UProperty GetKeyProp() const;
	UE_UProperty GetValueProp() const;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_DelegateProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	UE_UFunction GetSignatureFunction() const;
	std::string GetType() const;
	static UE_UClass StaticClass();
};

class UE_MulticastDelegateProperty : public UE_UProperty
{
public:
	using UE_UProperty::UE_UProperty;
	std::string GetType() const;
	static UE_UClass StaticClass();
};


template<typename T>
bool UE_UObject::IsA() const
{
	auto cmp = T::StaticClass();
	if (!cmp) { return false; }

	for (auto super = GetClass(); super; super = super.GetSuper().Cast<UE_UClass>())
	{
		if (super.object == cmp.object)
		{
			return true;
		}
	}

	return false;
}

class UE_UPackage
{
private:
	struct Member
	{
		std::string Name;
		int32_t Offset = 0;
		int32_t Size = 0;
	};
	struct Function
	{
		std::string FullName;
		std::string CppName;
		std::string Params;
		std::string Flags;
		uint64_t Func;
	};
	struct Struct
	{
		std::string FullName;
		std::string CppName;
		int32_t Inherited = 0;
		int32_t Size = 0;
		std::vector<Member> Members;
		std::vector<Function> Functions;
	};
	struct Enum
	{
		std::string FullName;
		std::string CppName;
		std::vector<std::string> Members;
	};
private:
	std::pair<byte* const, std::vector<UE_UObject>>* Package;
	std::vector<Struct> Classes;
	std::vector<Struct> Structures;
	std::vector<Enum> Enums;
private:
	static void GenerateBitPadding(std::vector<Member>& members, int32_t offset, int16_t bitOffset, int16_t size);
	static void GeneratePadding(std::vector<Member>& members, int32_t& minOffset, int32_t& bitOffset, int32_t maxOffset);
	static void GenerateStruct(UE_UStruct object, std::vector<Struct>& arr);
	static void GenerateEnum(UE_UEnum object, std::vector<Enum>& arr);
	static void SaveStruct(std::vector<Struct>& arr, FILE* file);
	static void SaveEnum(std::vector<Enum>& arr, FILE* file);
public:
	UE_UPackage(std::pair<byte* const, std::vector<UE_UObject>>& package) : Package(&package) {};
	void Process();
	bool Save(const fs::path& dir);
	UE_UObject GetObject() const;
};
