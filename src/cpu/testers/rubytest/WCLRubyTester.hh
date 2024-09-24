#ifndef __CPU_WCLRUBYTEST_RUBYTESTER_HH__
#define __CPU_WCLRUBYTEST_RUBYTESTER_HH__

#include <iostream>
#include <string>
#include <vector>

#include "cpu/testers/rubytest/CheckTable.hh"
#include "cpu/testers/rubytest/RubyTester.hh"
#include "mem/packet.hh"
#include "mem/port.hh"
#include "mem/ruby/common/SubBlock.hh"
#include "mem/ruby/common/TypeDefines.hh"
#include "params/RubyTester.hh"
#include "params/WCLRubyTester.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

class WCLRubyTester : public RubyTester
{
  
public:
    typedef WCLRubyTesterParams Params;
    WCLRubyTester(const Params &p);
    ~WCLRubyTester() = default;

private:
    // Private copy constructor and assignment operator
    WCLRubyTester(const WCLRubyTester& obj);
    WCLRubyTester& operator=(const WCLRubyTester& obj);
};

inline std::ostream&
operator<<(std::ostream& out, const WCLRubyTester& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace gem5

#endif // __CPU_WCLRUBYTEST_RUBYTESTER_HH__
