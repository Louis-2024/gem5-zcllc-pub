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

#define WB_SIZE 8 // should be larger than or equal to the number of cores

namespace gem5 {

namespace ruby {

class XYZCacheMemory : public CacheMemory {
    struct Location {
        int row;
        int loc;
    };
    struct WBEntry {
        Addr addr;
        DataBlock DataBlk;
        MachineID Sender;
        int MemAckOutstanding;
        int WritebackRequestSent;
        WBEntry() : addr(0), DataBlk(), Sender(), MemAckOutstanding(0), WritebackRequestSent(0) {}
        WBEntry(Addr addr, const DataBlock& DataBlk, MachineID Sender, int MemAckOutstanding): addr(addr), DataBlk(DataBlk), Sender(Sender), MemAckOutstanding(MemAckOutstanding), WritebackRequestSent(0) {}
        ~WBEntry() {}
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
    int getNotSync() const { return LLC_Only_dirty.size(); }
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

    virtual void addSharer(Addr address) {
        if(!m_ziv) return;
        assert(isTagPresent(address));
        if(LLC_Only_dirty.find(address) != LLC_Only_dirty.end()) {
            assert(P_dirty.find(address) == P_dirty.end());
            LLC_Only_dirty.erase(address);
            P_dirty.insert(address);
        }
        if(P.find(address) == P.end()) {
            P[address] = 0;
        }
        P[address] += 1;
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
        if(LLC_Only_dirty.find(address) != LLC_Only_dirty.end()) {
            assert(P_dirty.find(address) == P_dirty.end());
            LLC_Only_dirty.erase(address);
            P_dirty.insert(address);
        }
        if(P.find(address) != P.end()) {
            totalPrivateCache -= P[address];
            P[address] = 0;
        }
        P[address] = 1;
        totalPrivateCache += 1;

        markEntryNotCRE(address);
        reportInvariant(address);
    }
    virtual void removeSharer(Addr address) {
        if(!m_ziv) return;
        assert(isTagPresent(address));
        auto it = P.find(address);
        assert(it != P.end());
        assert(it->second > 0);
        it->second--;
        if(it->second == 0) {
            P.erase(it);
            if (P_dirty.find(address) != P_dirty.end()) {
                LLC_Only_dirty.insert(address);
                P_dirty.erase(address);
            }
        }
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
        if (P_dirty.find(address) != P_dirty.end()) {
            LLC_Only_dirty.insert(address);
            P_dirty.erase(address);
        }
        reportInvariant(address);
    }
    void markDirty(Addr address) {
        assert(P.find(address) != P.end());
        if (P_dirty.find(address) == P_dirty.end()) {
            P_dirty.insert(address);
        }
    }
    bool checkDirty(Addr address) {
        return (LLC_Only_dirty.find(address) != LLC_Only_dirty.end()) || (P_dirty.find(address) != P_dirty.end());
    }
    // must be called afterwards
    // called manually
    virtual void markCRE(Addr address) {
        if(!m_ziv) return;
        DPRINTF(ZIVCache, "ZIV: marking %#x ad CRE\n", address);
        assert(isTagPresent(address));
        assert(P.find(address) == P.end());
        if(LLC_Only_dirty.find(address) != LLC_Only_dirty.end()) {
            LLC_Only_dirty.erase(address);
        }
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
        } else if(LLC_Only_dirty.find(address) == LLC_Only_dirty.end()) {
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
    void relocateVictimToWB(AbstractCacheEntry* entry, MachineID Sender, int MemAckOutstanding);
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

    // functions to select Q victim for CRE
    Addr xyzCacheProbe(Addr address) {
        if(!m_ziv) return cacheProbe(address);
        assert(!xyzCREAvail(address));
        assert(LLC_Only_dirty.size() > 0);

        Addr victim = *LLC_Only_dirty.begin();
        Tick last_access_time = curTick();
        for (const Addr& Q_addr : LLC_Only_dirty) {
            AbstractCacheEntry* entry = lookup(Q_addr);
            assert(entry != nullptr);
            Tick Q_access_time = entry->getLastAccess();
            if (Q_access_time < last_access_time) {
                last_access_time = Q_access_time;
                victim = Q_addr;
            }
        }
        
        assert(isTagPresent(victim));
        return victim;
    }
    Addr simpleProbe(Addr address) {
        if(!m_ziv) return cacheProbe(address);
        assert(LLC_Only_dirty.size() > 0);
        
        Addr victim = *LLC_Only_dirty.begin();
        Tick last_access_time = curTick();
        for (const Addr& Q_addr : LLC_Only_dirty) {
            AbstractCacheEntry* entry = lookup(Q_addr);
            assert(entry != nullptr);
            Tick Q_access_time = entry->getLastAccess();
            if (Q_access_time < last_access_time) {
                last_access_time = Q_access_time;
                victim = Q_addr;
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

    bool existWBEntryToWriteback() {
        for (const auto& [addr, index] : m_WB_index) {
            assert(m_cache_WB[index] != nullptr);
            if (m_cache_WB[index]->WritebackRequestSent == 0) {
                return true;
            }
        }
        return false;
    }

    int getWBEntryIndexToWriteback() {
        for (const auto& [addr, index] : m_WB_index) {
            assert(m_cache_WB[index] != nullptr);
            if (m_cache_WB[index]->WritebackRequestSent == 0) {
                return index;
            }
        }
        return -1;
    }

    Addr getAddrByIndex(int index) {
        WBEntry* entry = m_cache_WB[index];
        assert(entry != nullptr);
        return (entry->addr);
    }

    DataBlock getDataBlkByIndex(int index) {
        WBEntry* entry = m_cache_WB[index];
        assert(entry != nullptr);
        return (entry->DataBlk);
    }

    MachineID getSenderByIndex(int index) {
        WBEntry* entry = m_cache_WB[index];
        assert(entry != nullptr);
        return (entry->Sender);
    }

    int getMemAckOutstandingByIndex(int index) {
        WBEntry* entry = m_cache_WB[index];
        assert(entry != nullptr);
        return (entry->MemAckOutstanding);
    }

    void setMemAckOutstandingByIndex(int index, int value) {
        WBEntry* entry = m_cache_WB[index];
        assert(entry != nullptr);
        entry->MemAckOutstanding = value;
    }

    bool checkWBEntryByAddr(Addr addr) {
        return (m_WB_index.find(addr) != m_WB_index.end());
    }

    int getIndexByAddr(Addr addr) {
        if (checkWBEntryByAddr(addr)) {
            return m_WB_index[addr];
        } else {
            return -1;
        }
    }

    void removeWBEntryByAddr(Addr addr) {
        if (checkWBEntryByAddr(addr)) {
            // remove from m_cache_WB
            delete m_cache_WB[m_WB_index[addr]];
            m_cache_WB[m_WB_index[addr]] = nullptr;
            // remove from m_WB_index
            m_WB_index.erase(addr);
        }
    }

    int getWBSize() {
        return m_WB_index.size();
    }

    void setWritebackRequestSentByIndex(int index) {
        WBEntry* entry = m_cache_WB[index];
        assert(entry != nullptr);
        entry->WritebackRequestSent = 1;
    }
    
protected:
    bool m_use_vi;
    bool m_ziv;
    // Store all relocated cache line
    std::unordered_map<Addr, Location> relocation_table;
    // Store the number of sharers for cache lines cached
    std::unordered_map<Addr, int> P; // the P set for maintaining P
    std::unordered_set<Addr> P_dirty;
    std::unordered_set<Addr> LLC_Only_dirty;
    // Store the Q lines for writeback in WB
    std::unordered_map<Addr, int> m_WB_index;
    std::vector<WBEntry*> m_cache_WB;

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
