#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#undef GetObject

uint32_t GetProcessId(std::wstring name);
std::pair<byte*, uint32_t> GetModuleInfo(uint32_t pid, std::wstring name);
bool Compare(byte* data, byte* sig, size_t size);
byte* FindSignature(byte* start, byte* end, byte* sig, size_t size);
void* FindPointer(byte* start, byte* end, byte* sig, size_t size, int32_t addition = 0, int32_t abcdefg = 0);
std::vector<std::pair<byte*, byte*>> GetExSections(byte* data);
uint32_t GetProccessPath(uint32_t pid, wchar_t* processName, uint32_t size);


std::string DetectUEVersion(const std::vector<byte>& image);
std::string GenerateExtendedSig(const byte* sectionStart, size_t matchOffset,
                                 const byte* sig, size_t sigLen,
                                 size_t prefixBytes = 16);
