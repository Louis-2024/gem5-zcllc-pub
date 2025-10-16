#ifndef __MEM_RUBY_STRUCTURES_XYZCACHEMEMORY_HH__
#define __MEM_RUBY_STRUCTURES_XYZCACHEMEMORY_HH__

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>
#include <utility>
#include "debug/ZIVCache.hh"

#include "base/statistics.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/ruby/common/AccessRecordsList.hh"
#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/protocol/CacheRequestType.hh"
#include "mem/ruby/protocol/CacheResourceType.hh"
#include "mem/ruby/protocol/RubyRequest.hh"
#include "mem/ruby/slicc_interface/AbstractCacheEntry.hh"
#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"
#include "mem/ruby/structures/BankedArray.hh"
#include "mem/ruby/structures/CacheMemory.hh"
#include "mem/ruby/system/CacheRecorder.hh"
// #include "params/RubyCache.hh"
#include "params/XYZCache.hh"
#include "sim/sim_object.hh"

#define SHORT_DISTANCE_ACCESS 0b0001
#define MID_DISTANCE_ACCESS 0b0010
#define LONG_DISTANCE_ACCESS 0b0100

#define RVQ_SIZE 2048
#define TIME_BIN_SIZE 512

namespace gem5 {

namespace ruby {

class XYZCacheMemory : public CacheMemory {
    struct Location {
        int row;
        int loc;
    };
public:
    typedef XYZCacheParams XYZParams;
    // typedef std::shared_ptr<replacement_policy::ReplacementData> ReplData;
    XYZCacheMemory(const XYZParams& p);
    ~XYZCacheMemory();
    virtual void deallocate(Addr address);
    // This was the original thing
    Addr addressToCacheSet(Addr address) const {
        return Addr(CacheMemory::addressToCacheSet(address));
    }
    int getLHS() const {
        // get the left hand side of the vacancy invariant
        return getNumBlocks() - getNotSync() - getPrv();
    }
    bool calculateVIwithDelta(int delta_lhs, int delta_rhs, int private_capacity) {
        if(pri_tot  == -1) pri_tot = private_capacity;
        // return getLHS() + delta_lhs >= pri_tot - totalPrivateCache + delta_rhs;
        DPRINTF(ZIVCache, "%d %d >=? %d %d\n", getNumBlocks(), getNotSync(), pri_tot, delta_rhs);
        // if(getNotSync() == 0) return true;
        return getNumBlocks() - getNotSync() >= pri_tot + delta_rhs;
    }
    int getNotSync() const { return Q.size(); }
    int getPrv() const { return P.size(); }
    bool isRelocated(Addr lineAddress) {
        return relocation_table.find(lineAddress) != relocation_table.end();
    }

    virtual AbstractCacheEntry* allocate(Addr address, AbstractCacheEntry* new_entry);
    // deallocate uses lookup which we hooked, so no need to change
    // virtual void deallocate(Addr address);
    virtual AbstractCacheEntry* lookup(Addr address);
    virtual const AbstractCacheEntry* lookup(Addr address) const;
    virtual void init();

    uint16_t calculate_pattern(Addr address) {
        // implement maths to calculate pattern
        return 1;
    }

    int calculate_priority(Addr address) {
        // implement maths to calculate priority
        return 1;
    }

    void addToQ(Addr address) {
        assert(Q.find(address) == Q.end());

        Q.insert(address);
        addQAccessRecordsByAddr(address);
        Q_patterns[address] = calculate_pattern(address);
        Q_priority[address] = calculate_priority(address);
    }

    void removeFromQ(Addr address) {
        assert(Q.find(address) != Q.end());

        Q.erase(address);
        removeQAccessRecordsByAddr(address);
        Q_patterns.erase(address);
        Q_priority.erase(address);
    }

    void removeFromQAsVictim(Addr address) {
        assert(Q.find(address) != Q.end());

        Q_RVQ.push_back({address, Q_patterns[address]});
        if (Q_RVQ.size() > RVQ_SIZE) {
            Q_RVQ.pop_front();
        }

        Q.erase(address);
        removeQAccessRecordsByAddr(address);
        Q_patterns.erase(address);
        Q_priority.erase(address);

        assert(Q_patterns.size() == Q.size());
        assert(Q_priority.size() == Q.size());
        assert(Q_RVQ.size() <= RVQ_SIZE);
    }

    virtual void addSharer(Addr address) {
        if(!m_ziv) return;
        assert(isTagPresent(address));
        if(Q.find(address) != Q.end()) {
            removeFromQ(address);
            assert(P.find(address) == P.end());
        }
        if(P.find(address) == P.end()) P[address] = 0;
        P[address] += 1;
        addSharerRecords(address, curTick(), P[address]);
        totalPrivateCache += 1;
        markEntryNotCRE(address);
        reportInvariant(address);
    }
    void markEntryNotCRE(Addr address) {
        if(!m_ziv) return;
        auto e = lookup(address);
        if(isCRE[e->getSet()][e->getWay()]) {
            isCRE[e->getSet()][e->getWay()] = false;
            CRECountPerSet[e->getSet()] -= 1;
            CRETotal -= 1;
        }
        
    }
    virtual void markOwner(Addr address) {
        if(!m_ziv) return;
        assert(isTagPresent(address));
        if(Q.find(address) != Q.end()) {
            removeFromQ(address);
            assert(P.find(address) == P.end());
        }
        if(P.find(address) != P.end()) {
            totalPrivateCache -= P[address];
            P[address] = 0;
        }
        P[address] = 1;
        addSharerRecords(address, curTick(), 1);
        totalPrivateCache += 1;

        markEntryNotCRE(address);
        reportInvariant(address);
    }
    virtual void removeSharer(Addr address) {
        if(!m_ziv) return;
        assert(isTagPresent(address));
        assert(Q.find(address) == Q.end());
        auto it = P.find(address);
        assert(it != P.end());
        assert(it->second > 0);
        it->second--;
        if(it->second == 0) {
            addToQ(address);
            P.erase(it);
        }
        addSharerRecords(address, curTick(), it->second);
        totalPrivateCache -= 1;
        reportInvariant(address);
    }
    virtual void clearSharer(Addr address) {
        if(!m_ziv) return;
        assert(isTagPresent(address));
        auto it = P.find(address);
        assert(it != P.end());
        totalPrivateCache -= it->second;
        P.erase(it);
        addToQ(address);
        addSharerRecords(address, curTick(), 0);
        reportInvariant(address);
    }
    // must be called afterwards
    // called manually
    virtual void markCRE(Addr address) {
        if(!m_ziv) return;
        DPRINTF(ZIVCache, "ZIV: marking %#x ad CRE\n", address);
        assert(isTagPresent(address));
        assert(P.find(address) == P.end());
        removeFromQAsVictim(address);
        removeSharerRecordsByAddr(address);
        auto e = lookup(address);
        auto row = e->getSet();
        auto way = e->getWay();
        if(!isCRE[row][way]) {
            isCRE[row][way] = true;
            CRECountPerSet[row] += 1;
            CRETotal += 1;
        }

        reportInvariant(address);
    }

    // Whether a CRE is available
    virtual bool xyzCREAvail(Addr address) const {
        if(!m_ziv) return cacheAvail(address);
        return CRETotal > 0;
    }
    bool checkCRE(Addr address) {
        panic_if(!m_ziv, "Relocation when ziv is not used");
        assert(isTagPresent(address));
        if(P.find(address) == P.end()) {
            return false;
        } else if(Q.find(address) == Q.end()) {
            return false;
        } else {
            return true;
        }
    }
    Location locateCRE(Addr address) {
        // We could use a more intelligent policy
        // NOTE: the state in the LLC is either invalid or LLCOnly
        panic_if(!m_ziv, "Relocation when ziv is not used");
        assert(CRETotal > 0);
        int64_t cacheSet = addressToCacheSet(address);
        // prioritize CREs in the same set
        if(CRECountPerSet[cacheSet]) {
            for(auto j = 0; j < m_cache_assoc; j++) {
                    if(isCRE[cacheSet][j]) {
                        return { (int)cacheSet, j };
                    }
                }
        }
        for(auto i = 0; i < m_cache_num_sets; i++) {
            if(CRECountPerSet[i] > 0) {
                for(auto j = 0; j < m_cache_assoc; j++) {
                    if(isCRE[i][j]) {
                        return { i, j };
                    }
                }
            }
        }
        panic("CRETotal is greater than 0 but no CRE found");
    }
    void reportInvariant(Addr address);
    void relocateVictim(AbstractCacheEntry* entry, Location targetLocation);
    AbstractCacheEntry* cacheProbeEntry(Addr address) const {
        // just return an entry instead of an address, used to find a victim to relocate
        assert(address == makeLineAddress(address));
        assert(!cacheAvail(address));

        int64_t cacheSet = addressToCacheSet(address);
        std::vector<ReplaceableEntry*> candidates;
        for (int i = 0; i < m_cache_assoc; i++) {
            candidates.push_back(static_cast<ReplaceableEntry*>(
                                                        m_cache[cacheSet][i]));
        }
        return m_cache[cacheSet][m_replacementPolicy_ptr->
                            getVictim(candidates)->getWay()];
    }

    // functions to track last access time by address
    Tick getLastAccessByAddr(Addr address) {
        auto entry = lookup(address);
        if(entry) {
            return entry->getLastAccess();
        } else {
            return 0;
        }
    }

    // functions to track access records by address
    std::vector<Tick> getAccessRecordsByAddr(Addr address) {
        auto entry = lookup(address);
        if(entry) {
            return entry->getAccessRecord();
        } else {
            return std::vector<Tick>();
        }
    }
    
    // functions to track sharer records by address
    std::vector<std::pair<Tick, int>> getSharerRecordsByAddr (Addr address) {
        auto sharers = Q_sharer_records.find(address);
        if (sharers != Q_sharer_records.end()) {
            return sharers->second;
        } else {
            return  std::vector<std::pair<Tick, int>>();
        }
    }
    void addSharerRecords (Addr address, Tick tick, int num_sharers) {
        Q_sharer_records[address].push_back({tick, num_sharers});
    }
    void removeSharerRecordsByAddr (Addr address) {
        Q_sharer_records.erase(address);
    }

    // functions to track access records of all Q lines
    AccessRecordsList<Addr, Tick> getQAccessRecords() {
        return Q_access_records;
    }
    void addQAccessRecordsByAddr (Addr address) {
        std::vector<Tick> records = getAccessRecordsByAddr(address);
        for (const Tick& tick : records) {
            Q_access_records.insert(address, tick);
        }
    }
    void removeQAccessRecordsByAddr (Addr address) {
        Q_access_records.erase(address);
    }

    // function to track access index by address
    std::unordered_map<Addr, std::vector<int>> getQAccessIndices () {
        std::unordered_map<Addr, std::vector<int>> Q_access_indices;
        int index = (int) (Q_access_records.size() - 1);
        for (const auto& record : Q_access_records) {
            Q_access_indices[record.addr].push_back(index);
            index -= 1;
        }
        return Q_access_indices;
    }

    // functions to select Q victim for CRE
    Addr xyzCacheProbe(Addr address) {
        if(!m_ziv) return cacheProbe(address);
        assert(!xyzCREAvail(address));
        assert(Q.size() > 0);
        
        
        // step 1: find the min priority
        uint16_t min_priority = UINT16_MAX;
        for (const Addr& addr : Q) {
            if (Q_priority[addr] < min_priority) {
                min_priority = Q_priority[addr];
            }
        }
        // step 2: reduce min priority to 0
        for (auto& pair : Q_priority) {
            pair.second = pair.second - min_priority;
        }
        // step 3: find the LRU entry of Q with 0 priority
        Addr victim = 0x0000;
        Tick victim_tick = UINT64_MAX;
        for (const Addr& addr : Q) {
            if ((Q_priority[addr] == 0) && (getLastAccessByAddr(addr) < victim_tick)) {
                victim = addr;
                victim_tick = getLastAccessByAddr(addr);
            }
        }
        
        
        assert(isTagPresent(victim));
        return victim;
    }
    Addr simpleProbe(Addr address) {
        if(!m_ziv) return cacheProbe(address);
        assert(Q.size() > 0);


        // step 1: find the min priority
        uint16_t min_priority = UINT16_MAX;
        for (const Addr& addr : Q) {
            if (Q_priority[addr] < min_priority) {
                min_priority = Q_priority[addr];
            }
        }
        // step 2: reduce min priority to 0
        for (auto& pair : Q_priority) {
            pair.second = pair.second - min_priority;
        }
        // step 3: find the LRU entry of Q with 0 priority
        Addr victim = 0x0000;
        Tick victim_tick = UINT64_MAX;
        for (const Addr& addr : Q) {
            if ((Q_priority[addr] == 0) && (getLastAccessByAddr(addr) < victim_tick)) {
                victim = addr;
                victim_tick = getLastAccessByAddr(addr);
            }
        }


        assert(isTagPresent(victim));
        return victim;
    }

    bool vi_enabled() const {
        return m_use_vi;
    }
    bool ziv_enabled() const {
        return m_ziv;
    }
    

protected:
    bool m_use_vi;
    bool m_ziv;
    // Store all relocated cache line
    std::unordered_map<Addr, Location> relocation_table;
    // Store the number of sharers for cache lines cached
    std::unordered_map<Addr, int> P; // the P set for maintaining P
    
    std::unordered_set<Addr> Q; // the lines that are dirty but not privately cached
    std::unordered_map<Addr, std::uint16_t> Q_patterns; // second: access pattern index
    std::unordered_map<Addr, std::uint16_t> Q_priority; // second: priority index

    std::deque<std::pair<Addr, std::uint16_t>> Q_RVQ; // list of recent Q victims, max size = RVQ_SIZE
    std::array<uint16_t, 4> Q_RVQ_reuse_counter; // [0]: SDA; [1]: MDA; [2]: LDA; [3]: default

    AccessRecordsList<Addr, Tick> Q_access_records;
    std::unordered_map<Addr, std::vector<std::pair<Tick, int>>> Q_sharer_records;

    std::vector<int> CRECountPerSet; // The number of CRE per set, initialized to be all zeros
    std::vector<std::vector<bool>> isCRE;
    int CRETotal = 0; // Total number of CREs
    int pri_tot = -1;

    int totalPrivateCache = 0;

private:
    // We don't need to copy this
    XYZCacheMemory(const CacheMemory& obj);
    XYZCacheMemory& operator=(const XYZCacheMemory& obj);
};

}  // namespace ruby
}  // namespace gem5

#endif  // __MEM_RUBY_STRUCTURES_CACHEMEMORY_HH__
