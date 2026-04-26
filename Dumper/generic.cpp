#include "wrappers.h"
#include "memory.h"
#include "engine.h"


byte* TNameEntryArray::GetEntry(uint32_t id) const
{
	if (IsUE5) {
		uint32_t blockIndex  = id >> 16;
		uint32_t blockOffset = (id & 0xFFFF) * 2u;
		byte* block = reinterpret_cast<byte*>(Chunks[blockIndex]);
		return block + blockOffset;
	}

	uint32_t ChunkIndex       = id / 16384;
	uint32_t WithinChunkIndex = id % 16384;
	byte** Chunk              = Chunks[ChunkIndex];
	return Read<byte*>(Chunk + WithinChunkIndex);
}

void TNameEntryArray::Dump(std::function<void(std::string_view name, uint32_t id)> callback) const
{
	if (IsUE5) {
		for (int blockIdx = 0; blockIdx < 128; blockIdx++) {
			byte* blockPtr = reinterpret_cast<byte*>(Chunks[blockIdx]);
			if (!blockPtr) break;

			uint32_t offset = 0;
			while (offset + 2 <= 65536) {
				uint16_t header = Read<uint16_t>(blockPtr + offset);
				if (header == 0) break; 

				bool     bIsWide = (header & 1) != 0;
				uint16_t len     = header >> 1;
				if (len == 0) break;

				uint32_t id = ((uint32_t)blockIdx << 16) | (offset / 2);
				auto entry  = UE_FNameEntry(blockPtr + offset);
				auto name   = entry.String();
				callback(name, id);

				uint32_t entryBytes = 2u + (bIsWide ? len * 2u : (uint32_t)len);
				entryBytes = (entryBytes + 1u) & ~1u; 
				offset += entryBytes;
			}
		}
		return;
	}


	for (auto i = 0; i < NumElements; i++) {
		auto entry = UE_FNameEntry(GetEntry(i));
		if (!entry) { continue; }
		auto name = entry.String();
		callback(name, i);
	}
}


byte* TUObjectArray::GetObjectPtr(uint64_t id) const
{
	if (id >= NumElements) { return nullptr; }
	return Read<byte*>(ObjObjects.Objects + id * 24);
}

void TUObjectArray::Dump(std::function<void(byte*)> callback) const
{
	for (uint32_t i = 0u; i < NumElements; i++)
	{
		byte* object = GetObjectPtr(i);
		if (!object) { continue; }
		callback(object);
	}
}

UE_UClass TUObjectArray::FindObject(const std::string& name) const
{
	for (uint32_t i = 0u; i < NumElements; i++)
	{
		UE_UClass object = GetObjectPtr(i);
		if (object && object.GetFullName() == name) { return object; }
	}
	return nullptr;
}

TNameEntryArray GlobalNames;
TUObjectArray   ObjObjects;
