#include "autooffsets.h"
#include "generic.h"
#include "memory.h"
#include "engine.h"
#include "wrappers.h"
#include "log.h"
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdio>


static constexpr uint32_t kMaxSamples = 400;
static constexpr uint32_t kProbeBytes = 0x80;


struct ObjSample {
    uint32_t idx;
    byte*    ptr;
    uint8_t  buf[kProbeBytes];
};

static std::vector<ObjSample> CollectSamples()
{
    std::vector<ObjSample> out;
    out.reserve(kMaxSamples);

    const uint64_t imgEnd = Base + 0x40000000ULL;

    for (uint32_t i = 0; i < ObjObjects.NumElements && out.size() < kMaxSamples; i++)
    {
        byte* obj = ObjObjects.GetObjectPtr(i);
        if (!obj) continue;

        ObjSample s;
        s.idx = i;
        s.ptr = obj;
        memset(s.buf, 0, kProbeBytes);
        if (!Read(obj, s.buf, kProbeBytes)) continue;

        uint64_t vtbl = *reinterpret_cast<uint64_t*>(s.buf);
        if (vtbl < Base || vtbl >= imgEnd) continue;

        out.push_back(s);
    }
    return out;
}


static uint32_t FindIndexOffset(const std::vector<ObjSample>& samples, float* conf)
{
    std::unordered_map<uint32_t, int> votes;
    for (auto& s : samples)
        for (uint32_t off = 4; off + 4 <= kProbeBytes; off += 4) {
            uint32_t val = *reinterpret_cast<const uint32_t*>(s.buf + off);
            if (val == s.idx) votes[off]++;
        }

    if (votes.empty()) { *conf = 0.f; return 0; }
    auto best = std::max_element(votes.begin(), votes.end(),
        [](auto& a, auto& b){ return a.second < b.second; });
    *conf = (float)best->second / (float)samples.size();
    return best->first;
}


static uint32_t FindClassOffset(const std::vector<ObjSample>& samples,
                                 uint32_t skipOff, float* conf)
{
    const uint64_t imgEnd = Base + 0x40000000ULL;
    std::unordered_map<uint64_t, bool> vtblCache;

    auto IsValidObj = [&](uint64_t ptr) -> bool {
        if (!ptr || ptr < Base || ptr >= imgEnd) return false;
        auto it = vtblCache.find(ptr);
        if (it != vtblCache.end()) return it->second;
        uint64_t vtbl = Read<uint64_t>((void*)ptr);
        bool ok = (vtbl >= Base && vtbl < imgEnd);
        return vtblCache[ptr] = ok;
    };

    std::unordered_map<uint32_t, int> votes;
    for (auto& s : samples)
        for (uint32_t off = 0; off + 8 <= kProbeBytes; off += 8) {
            if (off == (skipOff & ~7u)) continue;
            uint64_t ptr = *reinterpret_cast<const uint64_t*>(s.buf + off);
            if (IsValidObj(ptr)) votes[off]++;
        }

    if (votes.empty()) { *conf = 0.f; return 0; }
    auto best = std::max_element(votes.begin(), votes.end(),
        [](auto& a, auto& b){ return a.second < b.second; });
    *conf = (float)best->second / (float)samples.size();
    return best->first;
}


static bool IsPrintableName(uint32_t idx)
{
    if (!idx || idx >= (uint32_t)GlobalNames.NumElements) return false;
    auto entry = UE_FNameEntry(GlobalNames.GetEntry(idx));
    if (!entry) return false;
    auto s = entry.String();
    if (s.empty() || s.size() > 256) return false;
    for (char c : s) if (!std::isprint((unsigned char)c)) return false;
    return true;
}

static uint32_t FindNameOffset(const std::vector<ObjSample>& samples,
                                uint32_t skipIndex, uint32_t skipClass,
                                float* conf)
{
    std::unordered_map<uint32_t, int> votes;
    for (auto& s : samples)
        for (uint32_t off = 4; off + 4 <= kProbeBytes; off += 4) {
            if (off == skipIndex) continue;
            if (off >= skipClass && off < skipClass + 8) continue;
            uint32_t val = *reinterpret_cast<const uint32_t*>(s.buf + off);
            if (IsPrintableName(val)) votes[off]++;
        }

    if (votes.empty()) { *conf = 0.f; return 0; }
    auto best = std::max_element(votes.begin(), votes.end(),
        [](auto& a, auto& b){ return a.second < b.second; });
    *conf = (float)best->second / (float)samples.size();
    return best->first;
}


static uint32_t FindOuterOffset(const std::vector<ObjSample>& samples,
                                 uint32_t skipIndex, uint32_t skipClass,
                                 uint32_t nameOff, float* conf)
{
    const uint64_t imgEnd = Base + 0x40000000ULL;
    std::unordered_map<uint64_t, bool> vtblCache;

    auto IsValidOrNull = [&](uint64_t ptr) -> bool {
        if (!ptr) return true;
        if (ptr < Base || ptr >= imgEnd) return false;
        auto it = vtblCache.find(ptr);
        if (it != vtblCache.end()) return it->second;
        uint64_t vtbl = Read<uint64_t>((void*)ptr);
        bool ok = (vtbl >= Base && vtbl < imgEnd);
        return vtblCache[ptr] = ok;
    };

    uint32_t guess = nameOff + 8;
    if (guess + 8 <= kProbeBytes) {
        int gv = 0;
        for (auto& s : samples) {
            uint64_t ptr = *reinterpret_cast<const uint64_t*>(s.buf + guess);
            if (IsValidOrNull(ptr)) gv++;
        }
        float gc = (float)gv / (float)samples.size();
        if (gc >= kMinFieldConfidence) { *conf = gc; return guess; }
    }

    std::unordered_map<uint32_t, int> votes;
    for (auto& s : samples)
        for (uint32_t off = 0; off + 8 <= kProbeBytes; off += 8) {
            if (off == (skipIndex & ~7u) || off == skipClass) continue;
            if (off >= nameOff && off < nameOff + 8) continue;
            uint64_t ptr = *reinterpret_cast<const uint64_t*>(s.buf + off);
            if (IsValidOrNull(ptr)) votes[off]++;
        }

    if (votes.empty()) { *conf = 0.f; return 0; }
    auto best = std::max_element(votes.begin(), votes.end(),
        [](auto& a, auto& b){ return a.second < b.second; });
    *conf = (float)best->second / (float)samples.size();
    return best->first;
}


AutoOffsets AutoDiscoverOffsets()
{
    AutoOffsets r;
    if (!ObjObjects.NumElements || !GlobalNames.NumElements) return r;

    LOG("  [AutoOffsets] Collecting samples ({} max)...\n", kMaxSamples);
    auto samples = CollectSamples();

    if (samples.size() < 20) {
        LOGW("  [AutoOffsets] Only {} valid samples — skipping discovery\n", samples.size());
        return r;
    }
    LOG("  [AutoOffsets] Probing {} objects x 0x{:X} bytes\n", samples.size(), kProbeBytes);

    r.Index = FindIndexOffset(samples, &r.confIndex);
    LOG("  [AutoOffsets]   Index  0x{:02X}  ({:.0f}%)\n", r.Index, r.confIndex * 100.f);
    if (r.confIndex < kMinFieldConfidence) {
        LOGW("  [AutoOffsets] Index confidence too low — aborting\n");
        return r;
    }

    r.Class = FindClassOffset(samples, r.Index, &r.confClass);
    LOG("  [AutoOffsets]   Class  0x{:02X}  ({:.0f}%)\n", r.Class, r.confClass * 100.f);
    if (r.confClass < kMinFieldConfidence) {
        LOGW("  [AutoOffsets] Class confidence too low — aborting\n");
        return r;
    }

    r.Name = FindNameOffset(samples, r.Index, r.Class, &r.confName);
    LOG("  [AutoOffsets]   Name   0x{:02X}  ({:.0f}%)\n", r.Name, r.confName * 100.f);

    if (r.confName >= kMinFieldConfidence) {
        r.Outer = FindOuterOffset(samples, r.Index, r.Class, r.Name, &r.confOuter);
        LOG("  [AutoOffsets]   Outer  0x{:02X}  ({:.0f}%)\n", r.Outer, r.confOuter * 100.f);
    }

    int fields = 2;
    float total = r.confIndex + r.confClass;
    if (r.confName  >= kMinFieldConfidence) { total += r.confName;  fields++; }
    if (r.confOuter >= kMinFieldConfidence) { total += r.confOuter; fields++; }
    r.overall = total / (float)fields;
    r.valid   = true;

    LOG("  [AutoOffsets] Overall confidence {:.0f}%\n", r.overall * 100.f);
    return r;
}

ResolvedOffsets ApplyAutoOffsets(const AutoOffsets& ao)
{
    ResolvedOffsets res;

    auto Classify = [&](const char* name,
                        uint16_t& dest,
                        uint32_t autoVal, float autoConf,
                        OffsetEntry& entry)
    {
        if (dest != 0)
        {
            entry.value  = dest;
            entry.source = OffsetSource::Profile;
            entry.conf   = 0.f;

            if (ao.valid && autoConf >= kMinFieldConfidence
                && autoVal != 0 && (uint16_t)autoVal != dest)
            {
                LOGW("  [AutoOffsets]   {:22s} profile={:#04x}  discovered={:#04x}"
                     "  (keeping profile)\n", name, (uint32_t)dest, autoVal);
            }
        }
        else if (ao.valid && autoConf >= kMinFieldConfidence && autoVal != 0)
        {
            dest = (uint16_t)autoVal;
            entry.value  = dest;
            entry.source = OffsetSource::AutoDetect;
            entry.conf   = autoConf;
            LOG("  [AutoOffsets]   {:22s} = {:#04x}  (autodetect {:.0f}%)\n",
                name, autoVal, autoConf * 100.f);
        }
        else
        {
            entry.value  = 0;
            entry.source = OffsetSource::Missing;
            entry.conf   = 0.f;
            if (!dest)
                LOGW("  [AutoOffsets]   {:22s} MISSING (no profile, no autodetect)\n", name);
        }
    };

    auto ProfileOnly = [](uint16_t val, OffsetEntry& entry) {
        entry.value  = val;
        entry.source = val ? OffsetSource::Profile : OffsetSource::Missing;
        entry.conf   = 0.f;
    };

    LOG("  [AutoOffsets] Applying offsets (profile wins, autodetect fills gaps):\n");

    Classify("UObject.Index", defs.UObject.Index, ao.Index, ao.confIndex, res.UObject_Index);
    Classify("UObject.Class", defs.UObject.Class, ao.Class, ao.confClass, res.UObject_Class);
    Classify("UObject.Name",  defs.UObject.Name,  ao.Name,  ao.confName,  res.UObject_Name);
    Classify("UObject.Outer", defs.UObject.Outer, ao.Outer, ao.confOuter, res.UObject_Outer);

    ProfileOnly(defs.FNameEntry.HeaderSize,   res.FNameEntry_HeaderSize);
    ProfileOnly(defs.UField.Next,             res.UField_Next);
    ProfileOnly(defs.UStruct.SuperStruct,     res.UStruct_SuperStruct);
    ProfileOnly(defs.UStruct.Children,        res.UStruct_Children);
    ProfileOnly(defs.UStruct.PropertiesSize,  res.UStruct_PropertiesSize);
    ProfileOnly(defs.UEnum.Names,             res.UEnum_Names);
    ProfileOnly(defs.UEnum.NamesElementSize,  res.UEnum_NamesElementSize);
    ProfileOnly(defs.UFunction.FunctionFlags, res.UFunction_FunctionFlags);
    ProfileOnly(defs.UFunction.Func,          res.UFunction_Func);
    ProfileOnly(defs.UProperty.ArrayDim,      res.UProperty_ArrayDim);
    ProfileOnly(defs.UProperty.ElementSize,   res.UProperty_ElementSize);
    ProfileOnly(defs.UProperty.PropertyFlags, res.UProperty_PropertyFlags);
    ProfileOnly(defs.UProperty.Offset,        res.UProperty_Offset);
    ProfileOnly(defs.UProperty.Size,          res.UProperty_Size);

    return res;
}

void SaveOffsetsTxt(const ResolvedOffsets& r,
                    const std::string& path,
                    const std::string& gameName,
                    const std::string& ueVersion)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (!f) return;

    auto SourceTag = [](const OffsetEntry& e) -> const char* {
        switch (e.source) {
            case OffsetSource::Profile:    return "game.ini";
            case OffsetSource::AutoDetect: return "autodetect";
            case OffsetSource::Missing:    return "MISSING";
        }
        return "?";
    };

    auto Row = [&](const char* field, const OffsetEntry& e) {
        if (e.source == OffsetSource::AutoDetect)
            fprintf(f, "  %-38s 0x%04X   %-12s (%.0f%%)\n",
                field, e.value, SourceTag(e), e.conf * 100.f);
        else
            fprintf(f, "  %-38s 0x%04X   %s\n",
                field, e.value, SourceTag(e));
    };

    fprintf(f,
        "; ============================================================\n"
        "; Struct field offsets — %s — %s\n"
        ";\n"
        "; Sources:\n"
        ";   game.ini    = explicit profile\n"
        ";   autodetect  = discovered at runtime by probing GObjects\n"
        ";   MISSING     = not found in either source\n"
        "; ============================================================\n\n",
        gameName.c_str(), ueVersion.c_str());

    fprintf(f, "  %-38s %-8s %s\n", "; Field", "Offset", "Source");
    fprintf(f, "  %-38s %-8s %s\n", "; -----", "------", "------");

    Row("FNameEntry.HeaderSize",    r.FNameEntry_HeaderSize);
    fputc('\n', f);

    Row("UObject.Index",            r.UObject_Index);
    Row("UObject.Class",            r.UObject_Class);
    Row("UObject.Name",             r.UObject_Name);
    Row("UObject.Outer",            r.UObject_Outer);
    fputc('\n', f);

    Row("UField.Next",              r.UField_Next);
    fputc('\n', f);

    Row("UStruct.SuperStruct",      r.UStruct_SuperStruct);
    Row("UStruct.Children",         r.UStruct_Children);
    Row("UStruct.PropertiesSize",   r.UStruct_PropertiesSize);
    fputc('\n', f);

    Row("UEnum.Names",              r.UEnum_Names);
    Row("UEnum.NamesElementSize",   r.UEnum_NamesElementSize);
    fputc('\n', f);

    Row("UFunction.FunctionFlags",  r.UFunction_FunctionFlags);
    Row("UFunction.Func",           r.UFunction_Func);
    fputc('\n', f);

    Row("UProperty.ArrayDim",       r.UProperty_ArrayDim);
    Row("UProperty.ElementSize",    r.UProperty_ElementSize);
    Row("UProperty.PropertyFlags",  r.UProperty_PropertyFlags);
    Row("UProperty.Offset",         r.UProperty_Offset);
    Row("UProperty.Size",           r.UProperty_Size);

    int profileCount = 0, autoCount = 0, missingCount = 0;
    auto Count = [&](const OffsetEntry& e) {
        switch (e.source) {
            case OffsetSource::Profile:    profileCount++;    break;
            case OffsetSource::AutoDetect: autoCount++;       break;
            case OffsetSource::Missing:    missingCount++;    break;
        }
    };
    Count(r.FNameEntry_HeaderSize);
    Count(r.UObject_Index);   Count(r.UObject_Class);
    Count(r.UObject_Name);    Count(r.UObject_Outer);
    Count(r.UField_Next);
    Count(r.UStruct_SuperStruct);  Count(r.UStruct_Children);
    Count(r.UStruct_PropertiesSize);
    Count(r.UEnum_Names);     Count(r.UEnum_NamesElementSize);
    Count(r.UFunction_FunctionFlags); Count(r.UFunction_Func);
    Count(r.UProperty_ArrayDim);  Count(r.UProperty_ElementSize);
    Count(r.UProperty_PropertyFlags); Count(r.UProperty_Offset);
    Count(r.UProperty_Size);

    fprintf(f, "\n; game.ini=%d  autodetect=%d  MISSING=%d\n",
        profileCount, autoCount, missingCount);

    fclose(f);
    LOG("  Offsets written -> offsets.txt  "
        "(game.ini:{} autodetect:{} missing:{})\n",
        profileCount, autoCount, missingCount);
}
