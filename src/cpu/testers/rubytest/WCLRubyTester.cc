
#include "cpu/testers/rubytest/WCLRubyTester.hh"
#include "params/WCLRubyTester.hh"

#include "base/logging.hh"
#include "base/trace.hh"
#include "cpu/testers/rubytest/Check.hh"
#include "mem/ruby/common/SubBlock.hh"
#include "sim/sim_exit.hh"
#include "sim/system.hh"

namespace gem5
{

WCLRubyTester::WCLRubyTester(const Params &p)
  : RubyTester(p)
{
    
}

// WCLRubyTester::~WCLRubyTester()
// {
// }

// WCLRubyTester* WCLRubyTesterParams::create() const {
//     return new WCLRubyTester(*this);
// }


} // namespace gem5
