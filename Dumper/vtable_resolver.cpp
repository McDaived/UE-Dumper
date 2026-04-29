#include "vtable_resolver.h"
#include "generic.h"
#include "wrappers.h"
#include "memory.h"
#include "log.h"
#include <fmt/core.h>
#include <unordered_map>
#include <algorithm>
#include <cstdio>
#include <cstring>


// ProcessEvent — PE-A prologue (SoT / UE4.26+)
static const uint8_t kProcessEventHint[] = {
    0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,
    0x48,0x81,0xEC,0x00,0x00,0x00,0x00,   // sub rsp, ?
    0x48,0x8D,0x6C,0x24,0x00              // lea rbp, [rsp+?]
};

// PostRender — two complementary hints covering both MSVC prologue styles.

static const uint8_t kPostRenderHint0[] = {
    0x48,0x81,0xEC,0x00,0x00,0x00,0x00,  // sub rsp, ?         (large frame)
    0x48,0x8B,0x05,0x00,0x00,0x00,0x00,  // mov rax, [rip+?]   (__security_cookie)
    0x48,0x33,0xC4                        // xor rax, rsp
};

// PR-B prologue: MOV [RSP+8],RBX 
static const uint8_t kPostRenderHint1[] = {
    0x48,0x89,0x5C,0x24,0x00,            // mov [rsp+?], rbx
    0x55,0x56,0x57,                       // push rbp, rsi, rdi
    0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57, // push r12–r15
    0x48,0x8D,0x6C,0x24,0x00,            // lea rbp, [rsp+?]
    0x48,0x81,0xEC,0x00,0x00,0x00,0x00   // sub rsp, ?
};

// ProcessRemoteFunction — save of UFunction* (3rd param = R8) into R12.
static const uint8_t kPRFHint[] = {
    0x4D,0x8B,0xE0   // mov r12, r8
};

static const std::vector<VTblFuncDef> kVTblFuncDefs =
{
    {
        "ProcessEvent", "UObject",
        kDiscoverIndex, /*minIndex=*/ 3,
        {
            { kProcessEventHint, sizeof(kProcessEventHint), 0 },
        }
    },
    {
        "PostRender", "UGameViewportClient",
        kDiscoverIndex, /*minIndex=*/ 3,
        {
            { kPostRenderHint0, sizeof(kPostRenderHint0), 64 }, 
            { kPostRenderHint1, sizeof(kPostRenderHint1),  0 }, 
        }
    },
    {
        "ProcessRemoteFunction", "UNetDriver",
        kDiscoverIndex, /*minIndex=*/ 0x40,
        {
            { kPRFHint, sizeof(kPRFHint), 256 },
        }
    },
};

const std::vector<VTblFuncDef>& GetVTblFuncDefs() { return kVTblFuncDefs; }

static uintptr_t FollowThunks(uintptr_t rva, const uint8_t* img, size_t sz,
                               int maxDepth = 6)
{
    for (int d = 0; d < maxDepth; d++) {
        if (rva >= sz) break;
        const uint8_t* p = img + rva;
        size_t rem = sz - rva;

        if (rem>=5  && p[0]==0xE9)
            { uintptr_t t=rva+5 +*(const int32_t*)(p+1); if(t<sz){rva=t;continue;} break; }
        if (rem>=2  && p[0]==0xEB)
            { uintptr_t t=rva+2 +(int8_t)p[1];           if(t<sz){rva=t;continue;} break; }
        if (rem>=6  && p[0]==0xFF && p[1]==0x25) break;
        if (rem>=9  && p[0]==0x48&&p[1]==0x83&&(p[2]==0xE9||p[2]==0xC1)&&p[4]==0xE9)
            { uintptr_t t=rva+9 +*(const int32_t*)(p+5); if(t<sz){rva=t;continue;} break; }
        if (rem>=12 && p[0]==0x48&&p[1]==0x81&&(p[2]==0xE9||p[2]==0xC1)&&p[7]==0xE9)
            { uintptr_t t=rva+12+*(const int32_t*)(p+8); if(t<sz){rva=t;continue;} break; }
        break;
    }
    return rva;
}

static int CountHops(uintptr_t from, uintptr_t to, const uint8_t* img, size_t sz)
{
    int hops = 0;
    for (int i = 0; i < 6 && from != to; i++) {
        if (from >= sz) break;
        const uint8_t* p = img + from; size_t rem = sz - from;
        if (rem>=5  && p[0]==0xE9) { from=from+5 +*(const int32_t*)(p+1); hops++; continue; }
        if (rem>=2  && p[0]==0xEB) { from=from+2 +(int8_t)p[1];           hops++; continue; }
        if (rem>=9  && p[0]==0x48&&p[1]==0x83&&(p[2]==0xE9||p[2]==0xC1)&&p[4]==0xE9)
            { from=from+9 +*(const int32_t*)(p+5); hops++; continue; }
        if (rem>=12 && p[0]==0x48&&p[1]==0x81&&(p[2]==0xE9||p[2]==0xC1)&&p[7]==0xE9)
            { from=from+12+*(const int32_t*)(p+8); hops++; continue; }
        break;
    }
    return hops;
}

static bool MatchHint(const VTblHint& hint, uintptr_t funcRva,
                      const uint8_t* img, size_t imgSz)
{
    if (!hint.bytes || hint.size == 0) return false;
    if (funcRva + hint.size > imgSz)   return false;

    uintptr_t end = funcRva + hint.scanWindow;
    if (end + hint.size > imgSz) end = imgSz - hint.size;
    if (end < funcRva) end = funcRva;

    for (uintptr_t off = funcRva; off <= end; off++) {
        bool ok = true;
        for (size_t j = 0; j < hint.size; j++) {
            if (hint.bytes[j] != 0x00 && img[off + j] != hint.bytes[j])
                { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}


static std::string ExtractShortName(const std::string& full)
{
    auto dot = full.rfind('.');
    if (dot != std::string::npos && dot+1 < full.size()) return full.substr(dot+1);
    auto sp  = full.rfind(' ');
    if (sp  != std::string::npos && sp +1 < full.size()) return full.substr(sp +1);
    return full;
}

static const char* StripUEPrefix(const char* n)
{
    if (!n || !*n) return n;
    char c = *n;
    if ((c=='U'||c=='A'||c=='F'||c=='E'||c=='I'||c=='T') && *(n+1)) return n+1;
    return n;
}

static bool ClassNameMatches(const std::string& s, const char* t)
{ return s == StripUEPrefix(t); }


static constexpr int      kDiscoveryMaxSlots = 2048;
static constexpr uintptr_t kDiscoveryDelta   = 0x10; 

std::vector<VTblFuncResult> ResolveVTableFunctions(
    const std::vector<VTblFuncDef>&                   defs,
    uint64_t                                           base,
    const uint8_t*                                     imageBytes,
    size_t                                             imageSize,
    const std::unordered_map<std::string, uintptr_t>& patternRvas)
{
    const uint64_t imageEnd = base + imageSize;

    struct IdxVote { int count = 0; uintptr_t rva = 0; uintptr_t slotRva = 0; };
    struct Accum {
        std::unordered_map<int, IdxVote> heuristicVotes;
        std::unordered_map<int, IdxVote> patternVotes;
        bool classFound = false;
    };
    std::vector<Accum> acc(defs.size());

    ObjObjects.Dump([&](byte* object)
    {
        UE_UObject obj(object);
        UE_UClass  cls = obj.GetClass();
        if (!cls) return;

        std::string shortN = ExtractShortName(cls.GetFullName());

        uintptr_t vtbl = Read<uintptr_t>((void*)object);
        if (!vtbl || vtbl < base || vtbl >= imageEnd) return;

        for (size_t i = 0; i < defs.size(); i++)
        {
            if (!ClassNameMatches(shortN, defs[i].className)) continue;
            acc[i].classFound = true;

            bool heuristicHit = false;

            {
                bool hasHint = false;
                for (auto& h : defs[i].hints) if (h.bytes && h.size) { hasHint = true; break; }

                if (hasHint)
                {
                    for (int slot = defs[i].minIndex; slot < kDiscoveryMaxSlots; slot++)
                    {
                        uintptr_t fn = Read<uintptr_t>((void*)(vtbl + (uintptr_t)slot * 8));
                        if (fn < base || fn >= imageEnd) break;

                        uintptr_t slotRva = fn - base;
                        uintptr_t realRva = FollowThunks(slotRva, imageBytes, imageSize);

                        bool matched = false;
                        for (auto& h : defs[i].hints) {
                            if (!h.bytes || !h.size) break;
                            if (MatchHint(h, realRva, imageBytes, imageSize))
                                { matched = true; break; }
                        }

                        if (matched)
                        {
                            auto& v = acc[i].heuristicVotes[slot];
                            if (v.count == 0) { v.rva = realRva; v.slotRva = slotRva; }
                            v.count++;
                            heuristicHit = true;
                            break;
                        }
                    }
                }
            }

            if (!heuristicHit)
            {
                auto pit = patternRvas.find(defs[i].funcName);
                if (pit != patternRvas.end())
                {
                    uintptr_t targetRva = pit->second;
                    for (int slot = defs[i].minIndex; slot < kDiscoveryMaxSlots; slot++)
                    {
                        uintptr_t fn = Read<uintptr_t>((void*)(vtbl + (uintptr_t)slot * 8));
                        if (fn < base || fn >= imageEnd) break;

                        uintptr_t slotRva = fn - base;
                        uintptr_t realRva = FollowThunks(slotRva, imageBytes, imageSize);
                        uintptr_t delta   = realRva > targetRva
                                            ? realRva - targetRva : targetRva - realRva;
                        if (delta <= kDiscoveryDelta)
                        {
                            auto& v = acc[i].patternVotes[slot];
                            if (v.count == 0) { v.rva = realRva; v.slotRva = slotRva; }
                            v.count++;
                            break;
                        }
                    }
                }
            }
        }
    });

    auto BestIdx = [](const std::unordered_map<int, IdxVote>& m)
        -> std::unordered_map<int, IdxVote>::const_iterator
    {
        return std::max_element(m.begin(), m.end(),
            [](auto& a, auto& b){ return a.second.count < b.second.count; });
    };

    std::vector<VTblFuncResult> results;
    results.reserve(defs.size());

    for (size_t i = 0; i < defs.size(); i++)
    {
        VTblFuncResult r;
        r.funcName      = defs[i].funcName;
        r.resolvedClass = defs[i].className;
        r.vtableIndex   = kDiscoverIndex;
        r.status        = VTblStatus::NOT_VIRTUAL;
        r.rva = r.slotRva = 0;
        r.thunkDepth = r.validated = 0;
        r.heuristic  = false;
        r.minDiscIdx = defs[i].minIndex;

        if (!acc[i].classFound) {
            r.status = VTblStatus::CLASS_NOT_FOUND;
            results.push_back(std::move(r)); continue;
        }

        bool hasHeuristic = !acc[i].heuristicVotes.empty();
        bool hasPattern   = !acc[i].patternVotes.empty();

        if (!hasHeuristic && !hasPattern) {
            bool anyHint = false;
            for (auto& h : defs[i].hints) if (h.bytes && h.size) { anyHint = true; break; }
            bool noSeed = !anyHint
                       && patternRvas.find(defs[i].funcName) == patternRvas.end();
            r.status = noSeed ? VTblStatus::NO_SEED : VTblStatus::NOT_VIRTUAL;
            auto pit = patternRvas.find(defs[i].funcName);
            if (pit != patternRvas.end()) r.patternRva = pit->second;
            results.push_back(std::move(r)); continue;
        }

        auto& votes    = hasHeuristic ? acc[i].heuristicVotes : acc[i].patternVotes;
        auto   best    = BestIdx(votes);

        r.status      = VTblStatus::FOUND_IN_VTABLE;
        r.vtableIndex = best->first;
        r.rva         = best->second.rva;
        r.slotRva     = best->second.slotRva;
        r.thunkDepth  = CountHops(r.slotRva, r.rva, imageBytes, imageSize);
        r.validated   = best->second.count;
        r.heuristic   = hasHeuristic;

        results.push_back(std::move(r));
    }

    return results;
}

void SaveVTableFuncResults(
    const std::vector<VTblFuncResult>& results,
    const std::string& path,
    uint64_t base)
{
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (!f) return;

    fprintf(f,
        "; ============================================================\n"
        "; VTable-resolved function addresses — fully dynamic\n"
        "; Base: 0x%llX\n"
        ";\n"
        "; All indices discovered at runtime \n"
        ";\n"
        "; RVA / VA     real function entry point \n"
        "; SlotRVA      raw vtable slot value\n"
        "; [heuristic]  index found via function-body fingerprint scan\n"
        ";\n"
        ";\n"
        "; Status:\n"
        ";   FOUND_IN_VTABLE   resolved, validated across N live objects\n"
        ";   NOT_VIRTUAL       no slot matched in [minIndex, max) range\n"
        ";   CLASS_NOT_FOUND   no live instance found in GObjects\n"
        ";   NO_SEED           neither hint nor pattern RVA available\n"
        "; ============================================================\n\n",
        (unsigned long long)base);

    size_t found = 0;
    for (auto& r : results)
    {
        if (r.status == VTblStatus::FOUND_IN_VTABLE)
        {
            fprintf(f, "%-38s  [%-28s vtable[0x%X]]  FOUND_IN_VTABLE%s",
                r.funcName.c_str(), r.resolvedClass.c_str(), r.vtableIndex,
                r.heuristic ? " [heuristic]" : "");
            fprintf(f, "  RVA=0x%08llX  VA=0x%llX",
                (unsigned long long)r.rva, (unsigned long long)(base + r.rva));
            if (r.thunkDepth > 0)
                fprintf(f, "  SlotRVA=0x%08llX  thunk_depth=%d",
                    (unsigned long long)r.slotRva, r.thunkDepth);
            fprintf(f, "  (validated: %d)\n", r.validated);

            fprintf(f, "  ; %-36s index = 0x%X  [%s]\n\n",
                r.funcName.c_str(), r.vtableIndex,
                r.heuristic ? "heuristic" : "pattern-seeded");

            found++;
        }
        else
        {
            const char* tag = r.status == VTblStatus::NOT_VIRTUAL  ? "NOT_VIRTUAL"    :
                              r.status == VTblStatus::NO_SEED       ? "NO_SEED"        :
                                                                       "CLASS_NOT_FOUND";
            fprintf(f, "%-38s  [%-28s vtable[discovery]]  %s\n",
                r.funcName.c_str(), r.resolvedClass.c_str(), tag);

            if (r.status == VTblStatus::NOT_VIRTUAL) {
                if (r.patternRva)
                    fprintf(f,
                        "  ; hint+pattern both tried — pattern found RVA=0x%08llX"
                        " but no vtable slot [0x%X..0x%X] matched\n",
                        (unsigned long long)r.patternRva, r.minDiscIdx, kDiscoveryMaxSlots);
                else
                    fprintf(f,
                        "  ; hint scan found no match in [0x%X..0x%X]"
                        " — update hint bytes or add a pattern\n",
                        r.minDiscIdx, kDiscoveryMaxSlots);
            } else if (r.status == VTblStatus::NO_SEED) {
                fprintf(f, "  ; add a VTblHint or a pattern entry for this function\n");
            } else {
                fprintf(f, "  ; no live %s instance in GObjects\n", r.resolvedClass.c_str());
            }

            fputc('\n', f);
        }
    }

    fprintf(f, "; Resolved: %zu / %zu\n", found, results.size());
    fclose(f);

    LOG("  VTable functions resolved ({}/{}) -> vtable_funcs.txt\n", found, results.size());
}
