#include "utils.h"
#include <Psapi.h>
#include <algorithm>
#include <cstdio>

uint32_t GetProcessId(std::wstring name)
{
    uint32_t pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W entry = { sizeof(entry) };
        while (Process32NextW(snapshot, &entry)) { if (name == entry.szExeFile) { pid = entry.th32ProcessID; break; } }
        CloseHandle(snapshot);
    }
    return pid;
}
std::pair<byte*, uint32_t> GetModuleInfo(uint32_t pid, std::wstring name)
{
    std::pair<byte*, uint32_t> info;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32W modEntry = { sizeof(modEntry) };
        while (Module32NextW(snapshot, &modEntry))
        {
            if (name == modEntry.szModule) { info = { modEntry.modBaseAddr, modEntry.modBaseSize }; break; }
        }
    }
    return info;
}

bool Compare(byte* data, byte* sig, size_t size)
{
    for (size_t i = 0; i < size; i++) { if (data[i] != sig[i] && sig[i] != 0x00) { return false; } }
    return true;
}

byte* FindSignature(byte* start, byte* end, byte* sig, size_t size)
{
    for (byte* it = start; it < end - size; it++) { if (Compare(it, sig, size)) { return it; }; }
    return 0;
}
void* FindPointer(byte* start, byte* end, byte* sig, size_t size, int32_t addition, int32_t abcdefg)
{
    byte* address = FindSignature(start, end, sig, size);
    if (!address) return nullptr;
    address = address + abcdefg;
    int32_t offset = *reinterpret_cast<int32_t*>(address);
    return address + offset + addition;
}

std::vector<std::pair<byte*, byte*>> GetExSections(byte* data)
{
    std::vector<std::pair<byte*, byte*>> sections;
    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(data);
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(data + dos->e_lfanew);
    auto s = IMAGE_FIRST_SECTION(nt);
    for (auto i = 0u; i < nt->FileHeader.NumberOfSections; i++, s++) {
        if (s->Characteristics & IMAGE_SCN_CNT_CODE)
        {
            auto start = data + s->PointerToRawData;
            auto end = start + s->SizeOfRawData;
            sections.push_back({ start, end });
        }
    }
    return sections;
}

uint32_t GetProccessPath(uint32_t pid, wchar_t* processName, uint32_t size)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
    if (!QueryFullProcessImageNameW(hProcess, 0, processName, reinterpret_cast<DWORD*>(&size))) { size = 0; };
    CloseHandle(hProcess);
    return size;
}

std::string DetectUEVersion(const std::vector<byte>& image)
{
    const char* needle = "Release-";
    const size_t nLen  = 8;
    for (size_t i = 0; i + nLen + 3 < image.size(); i++) {
        if (memcmp(image.data() + i, needle, nLen) != 0) continue;
        const char* p = reinterpret_cast<const char*>(image.data() + i + nLen);
        if (!isdigit((unsigned char)*p)) continue;
        std::string ver;
        while (*p && (isdigit((unsigned char)*p) || *p == '.') && ver.size() < 8)
            ver += *p++;
        while (!ver.empty() && ver.back() == '.') ver.pop_back();
        if (ver.size() >= 3) return ver; 
    }

    for (const char* tag : { "++UE5+", "++UE4+" }) {
        std::string t(tag);
        auto it = std::search(image.begin(), image.end(), t.begin(), t.end());
        if (it != image.end())
            return (tag[2] == '5') ? "5.x" : "4.x";
    }
    return "";
}

std::string GenerateExtendedSig(const byte* sectionStart, size_t matchOffset,
                                  const byte* sig, size_t sigLen,
                                  size_t prefixBytes)
{
    size_t pre   = (matchOffset >= prefixBytes) ? prefixBytes : matchOffset;
    const byte* start = sectionStart + matchOffset - pre;
    size_t totalLen   = pre + sigLen;

    std::string result;
    for (size_t i = 0; i < totalLen; i++) {
        if (!result.empty()) result += ' ';
        bool isWild = false;
        if (i >= pre) {
            size_t sigPos = i - pre;
            isWild = (sigPos < sigLen && sig[sigPos] == 0x00);
        }
        if (isWild) {
            result += "??";
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X", start[i]);
            result += buf;
        }
    }
    return result;
}
