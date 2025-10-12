#ifndef __MEM_RUBY_COMMON_ACCESSRECORDSLIST_HH__
#define __MEM_RUBY_COMMON_ACCESSRECORDSLIST_HH__

#pragma once
#include <set>
#include <utility>
#include <functional>
#include <cstddef>

template <class Addr, class Tick>
class AccessRecordsList {
public:
    struct Entry {
        Addr addr;
        Tick tick;
    };

private:
    struct Cmp {
        bool operator()(const Entry& a, const Entry& b) const noexcept {
            return a.tick < b.tick;
        }
    };

    using Set = std::multiset<Entry, Cmp>;
    Set data_;

public:
    using value_type      = Entry;
    using iterator        = typename Set::iterator;
    using const_iterator  = typename Set::const_iterator;

    AccessRecordsList() = default;

    iterator insert(const Addr& addr, const Tick& tick) {
        return data_.insert(Entry{addr, tick});
    }
    iterator insert(Addr&& addr, Tick&& tick) {
        return data_.insert(Entry{std::move(addr), std::move(tick)});
    }

    Entry pop_front() {
        auto it = data_.begin();
        Entry e = *it;
        data_.erase(it);
        return e;
    }

    void erase(iterator it)                 { data_.erase(it); }
    std::size_t erase(const Addr& a, const Tick& t) {
        auto range = data_.equal_range(Entry{Addr{}, t});
        for (auto it = range.first; it != range.second; ++it) {
            if (std::equal_to<Addr>{}(it->addr, a)) {
                data_.erase(it);
                return 1;
            }
        }
        return 0;
    }
    std::size_t erase(const Addr& a) {
        std::size_t removed = 0;
        for (auto it = data_.begin(); it != data_.end(); ) {
            if (std::equal_to<Addr>{}(it->addr, a)) {
                it = data_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    bool empty() const noexcept             { return data_.empty(); }
    std::size_t size() const noexcept       { return data_.size(); }

    const Entry& front() const              { return *data_.begin(); }
    const Entry& back()  const              { return *data_.rbegin(); }

    iterator begin()                        { return data_.begin(); }
    iterator end()                          { return data_.end(); }
    const_iterator begin() const            { return data_.begin(); }
    const_iterator end() const              { return data_.end(); }
    const_iterator cbegin() const           { return data_.cbegin(); }
    const_iterator cend() const             { return data_.cend(); }

    const_iterator lower_bound_tick(const Tick& t) const {
        return data_.lower_bound(Entry{Addr{}, t});
    }
    const_iterator upper_bound_tick(const Tick& t) const {
        return data_.upper_bound(Entry{Addr{}, t});
    }

    void clear()                            { data_.clear(); }
};

#endif // __MEM_RUBY_COMMON_ACCESSRECORDSLIST_HH__
