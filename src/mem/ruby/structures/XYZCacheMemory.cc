
#include "xyz/XYZCacheMemory.hh"

#include "base/compiler.hh"
#include "base/intmath.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/HtmMem.hh"
// #include "debug/RubyCache.hh"
#include "debug/RubyCacheTrace.hh"
#include "debug/RubyResourceStalls.hh"
#include "debug/RubyStats.hh"
#include "debug/ZIVCache.hh"
#include "mem/cache/replacement_policies/weighted_lru_rp.hh"
#include "mem/ruby/protocol/AccessPermission.hh"
#include "mem/ruby/system/RubySystem.hh"
#include <sstream>

namespace gem5 {

namespace ruby {

XYZCacheMemory::XYZCacheMemory(const XYZCacheParams &p)
    : CacheMemory(p) {
        DPRINTF(ZIVCache, "Created XYZ cache\n");
        m_use_vi = p.use_vi;
        m_ziv = p.ziv;
        if(m_use_vi) {
            panic_if(!m_ziv, "ziv must be set when use_vi is enabled");
        }
        pri_tot = p.pri_tot;

}

void XYZCacheMemory::init() {
    CacheMemory::init();
    // initialize additional data structures
    CRECountPerSet.resize(m_cache_num_sets, m_cache_assoc);
    // initially all clear entry
    isCRE.resize(m_cache_num_sets,
                    std::vector<bool>(m_cache_assoc, 1));
    CRETotal = getNumBlocks();

    loadCacheAccessMap("cache_access_map.txt");
}


XYZCacheMemory::~XYZCacheMemory() {}


// looks an address up in the cache
AbstractCacheEntry*
XYZCacheMemory::lookup(Addr address)
{
    if(!m_ziv) return CacheMemory::lookup(address);
    // first check whether the address is relocated
    // m_tag_index will be invalid if the cache line is in relocation_table
    auto it = relocation_table.find(address);
    if(it != relocation_table.end()) {
        return m_cache[it->second.row][it->second.loc];
    } else {
        return CacheMemory::lookup(address);
    }
}

// looks an address up in the cache
const AbstractCacheEntry*
XYZCacheMemory::lookup(Addr address) const
{
    if(!m_ziv) return CacheMemory::lookup(address);
    auto it = relocation_table.find(address);
    if(it != relocation_table.end()) {
        return m_cache[it->second.row][it->second.loc];
    } else {
        return CacheMemory::lookup(address);
    }
}


AbstractCacheEntry*
XYZCacheMemory::allocate(Addr address, AbstractCacheEntry *entry)
{
    if(!m_ziv) return CacheMemory::allocate(address, entry);
    DPRINTF(ZIVCache, "ZIV cache is allocating space for %#x\n", address);

    // TODO: need changes
    // m_tag_index for relocation, we want lookup in parent class
    // to always fail for relocated cache lines to
    // fall back on our version
    assert(address == makeLineAddress(address));
    assert(!isTagPresent(address));
    // xyzCREAvail must be checked within the SM
    assert(xyzCREAvail(address));
    DPRINTF(ZIVCache, "XYZ Allocating entry to address: %#x\n", address);
    AbstractCacheEntry* new_entry = nullptr;
    if(cacheAvail(address)) {

        DPRINTF(ZIVCache, "ZIV cache uses traditional allocate\n", address);
        new_entry = CacheMemory::allocate(address, entry);
    } else {
        // relocate a cache line from current set to a set with CRE, and then call allocate
        auto targetLocation = locateCRE(address);
        auto e = cacheProbeEntry(address);
        relocateVictim(e, targetLocation);

        assert(cacheAvail(address));
        new_entry = CacheMemory::allocate(address, entry);
    }
    // the cache line is technical a CRE when allocate ends because it is
    // not cached by any core, later, the SM will call the AddSharers, MakeOwners etc to make this line non CRE
    // if it was a vacan entry, allocate would not modify that entry
    // otherwise, the relocation will leave the entry fresh
    assert(isCRE[new_entry->getSet()][new_entry->getWay()]);
    // CRECountPerSet[new_entry->getSet()] -= 1;
    // CRETotal -= 1;
    return new_entry;
}
void XYZCacheMemory::deallocate(Addr address) {
    if(!m_ziv) { CacheMemory::deallocate(address); return; }
    // deallocate only happens when relocating a victim to a target location
    // This target location must be a CRE even in ZIV because no back-invalidation is there
    // the LLC state machine should never call deallocate
    assert(P.find(address) == P.end());
    assert(Q.find(address) == Q.end()); // should be clean and safe to deallocate
    DPRINTF(ZIVCache, "ZIV cache is deallocating %#x with cacheMemory method\n", address);
    CacheMemory::deallocate(address);
    // This must be after deallocate since it uses lookup to find the location
    auto it = relocation_table.find(address);
    if(it != relocation_table.end()) {
        DPRINTF(ZIVCache, "ZIV cache is deallocating a relocated line %#x\n", address);
        relocation_table.erase(it);
    }
    DPRINTF(ZIVCache, "Deallocating the line done \n");
}

void XYZCacheMemory::reportInvariant(Addr address) {
    if(!m_ziv) return;
    DPRINTF(ZIVCache, "INVCHK >> M = %d, |P| = %d, |Q| = %d | N*T = %d, sum(|C|) = %d, CRETotal = %d\n",
        getNumBlocks(), getPrv(), getNotSync(),
        pri_tot, totalPrivateCache, CRETotal
    );

    if(P.find(address) != P.end()) {
        DPRINTF(ZIVCache, "AFTER CHANGE>>P[%#x] = %d\n", address, P[address]);
    } else {
        DPRINTF(ZIVCache, "AFTER CHANGE>>P[%#x] = 0\n", address);
    }
    DPRINTF(ZIVCache, "AFTER CHANGE>>Q[%#x]? = %d\n", address, Q.find(address) != Q.end());
}
void XYZCacheMemory::relocateVictim(AbstractCacheEntry* entry, Location targetLocation) {
    panic_if(!m_ziv, "Calling relocation when ziv is not available");
    // consider implementing these functions..
    // findTagInSet only used in lookup, m_tag_index will be invalidate after relocation
    // m_tag_index only used in allocate/deallocate/findTagInSet* we need to hook those
    auto row = targetLocation.row;
    auto loc = targetLocation.loc;
    auto orig_row = entry->getSet();
    auto orig_loc = entry->getWay();
    int is_original_entry_cre = isCRE[orig_row][orig_loc] ? 1 : 0;
    DPRINTF(ZIVCache, "ZIV relocating %#x to row %d, loc %d\n", entry->m_Address, row, loc);

    // clean up the target location, it is important that this comes first so that replacementData is released
    auto targetEntry = m_cache[row][loc];
    // This cache line must be clean and can be deleted
    assert(isCRE[row][loc]);

    // The total number will not change
    // This should also work for self relocation
    isCRE[orig_row][orig_loc] = true;
    CRECountPerSet[orig_row] += (1 - is_original_entry_cre);
    CRETotal += (1 - is_original_entry_cre);

    isCRE[row][loc] = is_original_entry_cre;
    CRECountPerSet[row] -= (1 - is_original_entry_cre);
    CRETotal -= (1 - is_original_entry_cre);




    // could be relocating to self
    if(targetEntry != nullptr) {
        bool selfRelocation = targetEntry == entry;
        if(selfRelocation) {
            assert(orig_row == row && orig_loc == loc);
            DPRINTF(ZIVCache, "ZIV: self-relocation\n");

        }
        deallocate(targetEntry->m_Address);
        // should now be released
        assert(m_cache[row][loc] == nullptr);
        if(selfRelocation) {
            // replacing to self
            // this line should have been deallocated
            return; // done already some space in the orignal location
        }
    }

    m_cache[orig_row][orig_loc] = nullptr;
    // move the entry to targetLocation
    // relocated line
    m_tag_index[entry->m_Address] = -2; // -2 meaning a cache line is relocated, check the relocation table
    entry->setPosition(row, loc);
    entry->replacementData = replacement_data[row][loc];
    m_replacementPolicy_ptr->reset(entry->replacementData);
    m_cache[row][loc] = entry;
    relocation_table[entry->m_Address] = targetLocation;


    // other maintain

    // The relocation process should be a subset of the following allocate function
    /*
    set[i] = entry;  // Init entry
    set[i]->m_Address = address;
    set[i]->m_Permission = AccessPermission_Invalid;
    DPRINTF(RubyCache, "Allocate clearing lock for addr: %x\n",
            address);
    set[i]->m_locked = -1;
    m_tag_index[address] = i;
    set[i]->setPosition(cacheSet, i);
    set[i]->replacementData = replacement_data[cacheSet][i];
    set[i]->setLastAccess(curTick());

    // Call reset function here to set initial value for different
    // replacement policies.
    m_replacementPolicy_ptr->reset(entry->replacementData);
    */
}

void XYZCacheMemory::loadCacheAccessMap(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        DPRINTF(ZIVCache, "Cache access map file %s not found, skipping load\n", filename);
        return;
    }

    std::string line;
    int line_count = 0;
    int loaded_entries = 0;

    while (std::getline(file, line)) {
        line_count++;

        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(iss, token, ',')) {
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }

        Addr address = std::stoull(tokens[0], nullptr, 16);
        std::vector<Tick> access_times;
        for (size_t i = 1; i < tokens.size(); i++) {
            Tick access_time = std::stoull(tokens[i]);
            access_times.push_back(access_time);
        }

        cache_access_map[address] = access_times;
        loaded_entries++;
    }

    file.close();
    DPRINTF(ZIVCache, "Loaded %d entries from cache access map file %s\n",
            loaded_entries, filename);
}

const std::vector<Tick>& XYZCacheMemory::getAccessTimes(Addr address) const {
    static const std::vector<Tick> empty_vector;
    auto it = cache_access_map.find(address);
    if (it != cache_access_map.end()) {
        return it->second;
    }
    return empty_vector;
}

bool XYZCacheMemory::hasAccessTimes(Addr address) const {
    return cache_access_map.find(address) != cache_access_map.end();
}

}; // namespace ruby
}; // namespace gem5