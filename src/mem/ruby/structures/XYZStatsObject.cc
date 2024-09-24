
#include "xyz/XYZStatsObject.hh"

#include <iostream>

namespace gem5::ruby
{

XYZStatsObject::XYZStatsObject(const XYZStatsObjectParams &params) :
    ClockedObject(params)
{
    std::cout << "XYZStatsObject !" << std::endl;
    wcl_bound = params.worst_case_latency_bound;
}

void XYZStatsObject::regStats() {
    Base::regStats();
    latencies.init(0, 100000, 50000);
    latencies.name(name() + "latency").desc("Mem access latency");
    total_put.name(name() + ".total_put").desc("Total number of put (PutM + PutS)");
    total_puts.name(name() + ".total_puts").desc("Total number of PutS");
    total_putm.name(name() + ".total_putm").desc("Total number of PutM");
    total_repl.name(name() + ".total_repl").desc("Total Replacement");
    total_repl_shared.name(name() + ".total_repl_shared").desc("Total Replacement of shared line");
    total_repl_owned.name(name() + ".total_repl_owned").desc("Total Replacement of owned line");
    total_repl_llc.name(name() + ".total_repl_llc").desc("Total Replacement of LLC line");
    total_put_as_wt.name(name() + ".total_put_as_wt").desc("Total number of put as write through");
    total_puts_as_wt.name(name() + ".total_puts_as_wt").desc("Total number of PutS as write through");
    total_putm_as_wt.name(name() + ".total_putm_as_wt").desc("Total number of PutM as write through");
    wt_putRatio.name(name() + ".wt_putRatio").desc("WT / Put");
    wt_putsRatio.name(name() + ".wt_putsRatio").desc("PutS WT / PutS");
    wt_putmRatio.name(name() + ".wt_putmRatio").desc("PutM WT / PutM");
    wt_putRatio = total_put_as_wt / total_put;
    wt_putsRatio = total_puts_as_wt / total_puts;
    wt_putmRatio = total_putm_as_wt / total_putm;
}

} // namespace gem5
