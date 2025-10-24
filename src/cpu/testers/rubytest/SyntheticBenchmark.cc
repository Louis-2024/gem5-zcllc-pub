#include "cpu/testers/rubytest/SyntheticBenchmark.hh"

#include "base/intmath.hh"
#include "base/random.hh"
#include "base/trace.hh"
#include "cpu/testers/rubytest/Check.hh"
#include "debug/RubyTest.hh"

#define DATA_SIZE 11264

namespace gem5
{

SyntheticBenchmark::SyntheticBenchmark(int _num_writers, int _num_readers, RubyTester* _tester)
    : m_num_writers(_num_writers), m_num_readers(_num_readers),
      m_tester_ptr(_tester)
{
    const int base_physical = 1000;
    for (int i = 0; i < DATA_SIZE / 2; i++) {
        for (int j = 0; j < 2; j++) {
            addCheck(base_physical + 8 * CHECK_SIZE * (j * DATA_SIZE / 2 + i));
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

    const int iter1 = 1;
    const int iter2 = 4;
    const int iter3 = 2;
    const int iter4 = 2;
    const int iter5 = 1;

    const int size1 = 2048;
    const int size2 = 512;
    const int size3 = 1024;
    const int size4 = 2048;
    const int size5 = 1024;
    
    const int base_1 = 0;
    const int base_2 = 2048;
    const int base_3 = 4096;
    const int base_4 = 6144;
    const int base_5 = 10240;

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
        if ((0 <= m_current_index) && (m_current_index < (base_1 + size1 * iter1))) { // reuse in data pattern 1
            if (m_current_index % 32 < 8) { // 25%
                out_index = base_1 + m_current_index % 32;
            } else if ((8 <= m_current_index % 64) && (m_current_index % 64 < 16)) { // 12.5%
                out_index = base_1 + m_current_index % 64;
            } else if ((16 <= m_current_index % 128) && (m_current_index % 128 < 32)) { // 12.5% 
                out_index = base_1 + m_current_index % 128;
            } else if ((80 <= m_current_index % 256) && (m_current_index % 256 < 96)) { // 6.25%
                out_index = base_1 + m_current_index % 256;
            } else if (((208 <= m_current_index % 512) && (m_current_index % 512 < 224)) || ((464 <= m_current_index % 512) && (m_current_index % 512 < 480))) { // 6.25%
                out_index = base_1 + m_current_index % 512;
            } else if (((40 <= m_current_index % 1024) && (m_current_index % 1024 < 64)) || ((552 <= m_current_index % 1024) && (m_current_index % 1024 < 576))) { // 4.6875%
                out_index = base_1 + m_current_index % 1024;
            }
        } else if ((base_2 <= m_current_index) && (m_current_index < (base_2 + size2 * iter2))) { // reuse in data pattern 2
            if (m_current_index % 32 < 8) { // 25%
                out_index = base_2 + m_current_index % 32;
            } else if ((8 <= m_current_index % 64) && (m_current_index % 64 < 16)) { // 12.5%
                out_index = base_2 + m_current_index % 64;
            } else if ((16 <= m_current_index % 128) && (m_current_index % 128 < 24)) { // 6.25% 
                out_index = base_2 + m_current_index % 128;
            } else if ((80 <= m_current_index % 256) && (m_current_index % 256 < 96)) { // 6.25%
                out_index = base_2 + m_current_index % 256;
            }
        } else if ((base_3 <= m_current_index) && (m_current_index < (base_3 + size3 * iter3))) { // reuse in data pattern 3
            if (m_current_index % 64 < 16) { // 25%
                out_index = base_3 + m_current_index % 64;
            } else if ((16 <= m_current_index % 128) && (m_current_index % 128 < 32)) { // 12.5%
                out_index = base_3 + m_current_index % 128;
            } else if (((80 <= m_current_index % 256) && (m_current_index % 256 < 104)) || ((208 <= m_current_index % 256) && (m_current_index % 256 < 224))) { // 15.625 % 
                out_index = base_3 + m_current_index % 256;
            } else if ((36 <= m_current_index % 512) && (m_current_index % 512 < 64)) { // 6.25%
                out_index = base_3 + m_current_index % 512;
            }
        } else if ((base_4 <= m_current_index) && (m_current_index < (base_4 + size4 * iter4))) { // reuse in data pattern 4
            if (m_current_index % 32 < 8) { // 25%
                out_index = base_4 + m_current_index % 32;
            } else if ((8 <= m_current_index % 64) && (m_current_index % 64 < 16)) { // 12.5%
                out_index = base_4 + m_current_index % 64;
            } else if ((16 <= m_current_index % 128) && (m_current_index % 128 < 32)) { // 12.5% 
                out_index = base_4 + m_current_index % 128;
            } else if ((80 <= m_current_index % 256) && (m_current_index % 256 < 96)) { // 6.25%
                out_index = base_4 + m_current_index % 256;
            } else if (((208 <= m_current_index % 512) && (m_current_index % 512 < 224)) || ((464 <= m_current_index % 512) && (m_current_index % 512 < 480))) { // 6.25%
                out_index = base_4 + m_current_index % 512;
            } else if (((40 <= m_current_index % 1024) && (m_current_index % 1024 < 64)) || ((552 <= m_current_index % 1024) && (m_current_index % 1024 < 576))) { // 4.6875%
                out_index = base_4 + m_current_index % 1024;
            }
        } else if ((base_5 <= m_current_index) && (m_current_index < (base_5 + size5 * iter5))) { // reuse in data pattern 5
            if (m_current_index % 64 < 16) { // 25%
                out_index = base_5 + m_current_index % 64;
            } else if ((16 <= m_current_index % 128) && (m_current_index % 128 < 32)) { // 12.5%
                out_index = base_5 + m_current_index % 128;
            } else if (((80 <= m_current_index % 256) && (m_current_index % 256 < 104)) || ((208 <= m_current_index % 256) && (m_current_index % 256 < 224))) { // 15.625 % 
                out_index = base_5 + m_current_index % 256;
            } else if ((36 <= m_current_index % 512) && (m_current_index % 512 < 64)) { // 6.25%
                out_index = base_5 + m_current_index % 512;
            }
        } else {
            // cross-pattern reuse
            if ((424 <= m_current_index % 512) && (m_current_index % 512 < 448)) { // 4.6875%
                out_index = m_current_index % 512;
            } else if ((104 <= m_current_index % 1024) && (m_current_index % 1024 < 128)) { // 2.34375%
                out_index = m_current_index % 1024;
            } else if (((296 <= m_current_index % 1536) && (m_current_index % 1536 < 320)) || ((360 <= m_current_index % 1536) && (m_current_index % 1536 < 384))) { // 3.125%
                out_index = m_current_index % 1536;
            } else if (((168 <= m_current_index % 2048) && (m_current_index % 2048 < 192)) || ((232 <= m_current_index % 2048) && (m_current_index % 2048 < 256))) { // 2.34375%
                out_index = m_current_index % 2048;
            }
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
