#include "cpu/testers/rubytest/SyntheticBenchmark.hh"

#include "base/intmath.hh"
#include "base/random.hh"
#include "base/trace.hh"
#include "cpu/testers/rubytest/Check.hh"
#include "debug/RubyTest.hh"

#define DATA_SIZE 131072 // 128 * 1024

namespace gem5
{

SyntheticBenchmark::SyntheticBenchmark(int _num_writers, int _num_readers, RubyTester* _tester)
    : m_num_writers(_num_writers), m_num_readers(_num_readers),
      m_tester_ptr(_tester)
{
    const int base_physical = 1000;
    for (int i = 0; i < DATA_SIZE / 16; i++) {
        for (int j = 0; j < 16; j++) {
            addCheck(base_physical + CHECK_SIZE * (j * DATA_SIZE / 16 + i));
        }
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

    float weight_extra_long_reuse = (m_current_index < 4096) ? (0.125 * m_current_index) : 512;
    float weight_long_reuse = (m_current_index < 2048) ? (0.25 * m_current_index) : 512;
    float weight_mid_reuse = (m_current_index < 1024) ? (m_current_index) : 1024;
    float weight_short_reuse = (m_current_index < 512) ? (2 * m_current_index) : 1024;
    float weight_extra_short_reuse = (m_current_index < 256) ? (4 * m_current_index) : 1024;
    float weight_new_data = 12288;

    float random_between_0_and_1 = random_mt.random<float>();
    float weight_total = weight_extra_long_reuse + weight_long_reuse + weight_mid_reuse + weight_short_reuse + weight_extra_short_reuse + weight_new_data;

    float prob_extra_long_reuse = weight_extra_long_reuse / weight_total;
    float prob_long_reuse = weight_long_reuse / weight_total;
    float prob_mid_reuse = weight_mid_reuse / weight_total;
    float prob_short_reuse = weight_short_reuse / weight_total;
    float prob_extra_short_reuse = weight_extra_short_reuse / weight_total;
    float prob_new_data = weight_new_data / weight_total;

    const int cycle_size = 16384;
    const int pattern_size = 4096;

    const int iter_size_0 = 2048; // 2 * 2k 
    const int iter_size_1 = 768; // 6 * 768
    const int iter_size_2 = 4096; // 1 * 4k
    const int iter_size_3 = 1024; // 4 * 1k

    uint32_t out_index = m_current_index;

    if (random_between_0_and_1 < prob_extra_long_reuse) { // extra long reuse 
        uint32_t lower_bound = (m_current_index < 4096) ? 0 : (m_current_index - 4096);
        out_index = random_mt.random<unsigned>(lower_bound, m_current_index - 1);
    } else if (random_between_0_and_1 < (prob_extra_long_reuse + prob_long_reuse)) { // long reuse 
        uint32_t lower_bound = (m_current_index < 2048) ? 0 : (m_current_index - 2048);
        out_index = random_mt.random<unsigned>(lower_bound, m_current_index - 1);
    } else if (random_between_0_and_1 < (prob_extra_long_reuse + prob_long_reuse + prob_mid_reuse)) { // mid reuse
        uint32_t lower_bound = (m_current_index < 1024) ? 0 : (m_current_index - 1024);
        out_index = random_mt.random<unsigned>(lower_bound, m_current_index - 1);
    } else if (random_between_0_and_1 < (prob_extra_long_reuse + prob_long_reuse + prob_mid_reuse + prob_short_reuse)) { // short reuse
        uint32_t lower_bound = (m_current_index < 512) ? 0 : (m_current_index - 512);
        out_index = random_mt.random<unsigned>(lower_bound, m_current_index - 1);
    } else if (random_between_0_and_1 < (prob_extra_long_reuse + prob_long_reuse + prob_mid_reuse + prob_short_reuse + prob_extra_short_reuse)) { // extra short reuse
        uint32_t lower_bound = (m_current_index < 256) ? 0 : (m_current_index - 256);
        out_index = random_mt.random<unsigned>(lower_bound, m_current_index - 1);
    } else {
        // proceed with traversal
        int current_cycle = m_current_index / cycle_size;
        int current_cycle_base = current_cycle * cycle_size;

        int current_pattern = (m_current_index - current_cycle_base) / pattern_size;
        int current_pattern_base = current_cycle_base + current_pattern * pattern_size;

        if (current_pattern == 0) {
            int current_index = (m_current_index - current_pattern_base) % iter_size_0;
            if (current_index % 32 < 8) { // 25%
                out_index = current_pattern_base + current_index % 32;
            } else if ((8 <= current_index % 64) && (current_index % 64 < 16)) { // 12.5%
                out_index = current_pattern_base + current_index % 64;
            } else if ((16 <= current_index % 128) && (current_index % 128 < 32)) { // 12.5% 
                out_index = current_pattern_base + current_index % 128;
            } else if ((104 <= current_index % 256) && (current_index % 256 < 128)) { // 9.375%
                out_index = current_pattern_base + current_index % 256;
            } else if (((208 <= current_index % 512) && (current_index % 512 < 224)) || ((464 <= current_index % 512) && (current_index % 512 < 480))) { // 6.25%
                out_index = current_pattern_base + current_index % 512;
            } else if (((40 <= current_index % 1024) && (current_index % 1024 < 64)) || ((552 <= current_index % 1024) && (current_index % 1024 < 576))) { // 4.6875%
                out_index = current_pattern_base + current_index % 1024;
            }
        } else if (current_pattern == 1) {
            int current_index = (m_current_index - current_pattern_base) % iter_size_1;
            if (current_index % 32 < 8) { // 25%
                out_index = current_pattern_base + current_index % 32;
            } else if ((8 <= current_index % 64) && (current_index % 64 < 16)) { // 12.5%
                out_index = current_pattern_base + current_index % 64;
            } else if ((16 <= current_index % 128) && (current_index % 128 < 32)) { // 12.5% 
                out_index = current_pattern_base + current_index % 128;
            } else if ((104 <= current_index % 256) && (current_index % 256 < 128)) { // 9.375%
                out_index = current_pattern_base + current_index % 256;
            } else if (((208 <= current_index % 512) && (current_index % 512 < 224)) || ((464 <= current_index % 512) && (current_index % 512 < 480))) { // 6.25%
                out_index = current_pattern_base + current_index % 512;
            }
        } else if (current_pattern == 2) {
            int current_index = (m_current_index - current_pattern_base) % iter_size_2;
            if (current_index % 32 < 8) { // 25%
                out_index = current_pattern_base + current_index % 32;
            } else if ((8 <= current_index % 64) && (current_index % 64 < 16)) { // 12.5%
                out_index = current_pattern_base + current_index % 64;
            } else if ((16 <= current_index % 128) && (current_index % 128 < 32)) { // 12.5% 
                out_index = current_pattern_base + current_index % 128;
            } else if ((104 <= current_index % 256) && (current_index % 256 < 128)) { // 9.375%
                out_index = current_pattern_base + current_index % 256;
            } else if (((208 <= current_index % 512) && (current_index % 512 < 224)) || ((464 <= current_index % 512) && (current_index % 512 < 480))) { // 6.25%
                out_index = current_pattern_base + current_index % 512;
            } else if (((40 <= current_index % 1024) && (current_index % 1024 < 64)) || ((552 <= current_index % 1024) && (current_index % 1024 < 576))) { // 4.6875%
                out_index = current_pattern_base + current_index % 1024;
            }
        } else if (current_pattern == 3) {
            int current_index = (m_current_index - current_pattern_base) % iter_size_3;
            if (current_index % 32 < 8) { // 25%
                out_index = current_pattern_base + current_index % 32;
            } else if ((8 <= current_index % 64) && (current_index % 64 < 16)) { // 12.5%
                out_index = current_pattern_base + current_index % 64;
            } else if ((16 <= current_index % 128) && (current_index % 128 < 32)) { // 12.5% 
                out_index = current_pattern_base + current_index % 128;
            } else if ((104 <= current_index % 256) && (current_index % 256 < 128)) { // 9.375%
                out_index = current_pattern_base + current_index % 256;
            } else if (((208 <= current_index % 512) && (current_index % 512 < 224)) || ((464 <= current_index % 512) && (current_index % 512 < 480))) { // 6.25%
                out_index = current_pattern_base + current_index % 512;
            }
        }

        // cross-pattern reuse
        if ((424 <= m_current_index % 512) && (m_current_index % 512 < 448)) { // 4.6875%
            out_index = current_cycle_base + m_current_index % 512;
        } else if ((680 <= m_current_index % 1024) && (m_current_index % 1024 < 704)) { // 2.34375%
            out_index = current_cycle_base + m_current_index % 1024;
        } else if (((336 <= m_current_index % 1536) && (m_current_index % 1536 < 352)) || ((488 <= m_current_index % 1536) && (m_current_index % 1536 < 512))) { // 2.60417%
            out_index = current_cycle_base + m_current_index % 1536;
        } else if (((168 <= m_current_index % 2048) && (m_current_index % 2048 < 192)) || ((232 <= m_current_index % 2048) && (m_current_index % 2048 < 256))) { // 2.34375%
            out_index = current_cycle_base + m_current_index % 2048;
        }
    }

    DPRINTF(RubyTest, "m_current_index = %d \n", m_current_index);
    DPRINTF(RubyTest, "out_index = %d \n", out_index);
    
    m_current_index = (m_current_index < DATA_SIZE - 1) ? (m_current_index + 1) : 0;
    return m_check_vector[out_index];
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
