#ifndef __CPU_RUBYTEST_SYNTHETICBENCHMARK_HH__
#define __CPU_RUBYTEST_SYNTHETICBENCHMARK_HH__

#include <iostream>
#include <unordered_map>
#include <vector>

#include "mem/ruby/common/Address.hh"

namespace gem5
{

class Check;
class RubyTester;

class SyntheticBenchmark
{
  public:
    SyntheticBenchmark(int _num_writers, int _num_readers, RubyTester* _tester);
    ~SyntheticBenchmark();

    Check* getRandomCheck();
    Check* getCheck(Addr address);

    //  bool isPresent(const Address& address) const;
    //  void removeCheckFromTable(const Address& address);
    //  bool isTableFull() const;
    // Need a method to select a check or retrieve a check

    void print(std::ostream& out) const;

  private:
    void addCheck(Addr address);
    
    // Private copy constructor and assignment operator
    SyntheticBenchmark(const SyntheticBenchmark& obj);
    SyntheticBenchmark& operator=(const SyntheticBenchmark& obj);

    std::vector<Check*> m_check_vector;
    std::unordered_map<Addr, Check*> m_lookup_map;

    uint32_t m_current_index = 0;

    int m_num_writers;
    int m_num_readers;
    RubyTester* m_tester_ptr;
};

inline std::ostream&
operator<<(std::ostream& out, const SyntheticBenchmark& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace gem5

#endif // __CPU_RUBYTEST_SYNTHETICBENCHMARK_HH__
