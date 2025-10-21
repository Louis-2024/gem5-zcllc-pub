#include "cpu/testers/rubytest/SyntheticBenchmark.hh"

#include "base/intmath.hh"
#include "base/random.hh"
#include "base/trace.hh"
#include "cpu/testers/rubytest/Check.hh"
#include "debug/RubyTest.hh"

namespace gem5
{

SyntheticBenchmark::SyntheticBenchmark(int _num_writers, int _num_readers, RubyTester* _tester)
    : m_num_writers(_num_writers), m_num_readers(_num_readers),
      m_tester_ptr(_tester)
{
    Addr physical = 0;

    const int size1 = 4096;
    const int size2 = 8192;

    DPRINTF(RubyTest, "Adding false sharing checks\n");
    // The first set is to get some false sharing
    physical = 1000;
    for (int i = 0; i < size1; i++) {
        // Setup linear addresses
        addCheck(physical);
        physical += CHECK_SIZE;
    }

    DPRINTF(RubyTest, "Adding cache conflict checks\n");
    // The next two sets are to get some limited false sharing and
    // cache conflicts
    physical = 1000;
    for (int i = 0; i < size2; i++) {
        // Setup linear addresses
        addCheck(physical);
        physical += 256;
    }

    DPRINTF(RubyTest, "Adding cache conflict checks2\n");
    physical = 1000 + CHECK_SIZE;
    for (int i = 0; i < size2; i++) {
        // Setup linear addresses
        addCheck(physical);
        physical += 256;
    }
}

SyntheticBenchmark::~SyntheticBenchmark()
{
    int size = m_check_vector.size();
    for (int i = 0; i < size; i++)
        delete m_check_vector[i];
}

void
SyntheticBenchmark::addCheck(Addr address)
{
    if (floorLog2(CHECK_SIZE) != 0) {
        if (ruby::bitSelect(address, 0, CHECK_SIZE_BITS - 1) != 0) {
            panic("Check not aligned");
        }
    }

    for (int i = 0; i < CHECK_SIZE; i++) {
        if (m_lookup_map.count(address+i)) {
            // A mapping for this byte already existed, discard the
            // entire check
            return;
        }
    }

    DPRINTF(RubyTest, "Adding check for address: %s\n", address);

    Check* check_ptr = new Check(address, 100 + m_check_vector.size(),
                                 m_num_writers, m_num_readers, m_tester_ptr);
    for (int i = 0; i < CHECK_SIZE; i++) {
        // Insert it once per byte
        m_lookup_map[address + i] = check_ptr;
    }
    m_check_vector.push_back(check_ptr);
}

Check*
SyntheticBenchmark::getRandomCheck()
{
    assert(m_check_vector.size() > 0);
    return m_check_vector[random_mt.random<unsigned>(0, m_check_vector.size() - 1)];
}

Check*
SyntheticBenchmark::getCheck(const Addr address)
{
    DPRINTF(RubyTest, "Looking for check by address: %s\n", address);

    auto i = m_lookup_map.find(address);

    if (i == m_lookup_map.end())
        return NULL;

    Check* check = i->second;
    assert(check != NULL);
    return check;
}

void
SyntheticBenchmark::print(std::ostream& out) const
{
}

} // namespace gem5
