#ifndef __MEM_RUBY_STRUCTURES_XYZSTATSOBJECT_HH__
#define __MEM_RUBY_STRUCTURES_XYZSTATSOBJECT_HH__

#include "params/XYZStatsObject.hh"
#include "sim/sim_object.hh"
#include "sim/clocked_object.hh"
#include "base/statistics.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/XYZInfo.hh"

namespace gem5::ruby {
    class XYZStatsObject: public ClockedObject 
    {
        using Base = ClockedObject;
    public:
        XYZStatsObject(const XYZStatsObjectParams &p);


        gem5::statistics::Distribution latencies;
        gem5::statistics::Scalar total_repl;
        gem5::statistics::Scalar total_repl_shared;
        gem5::statistics::Scalar total_repl_owned;
        gem5::statistics::Scalar total_repl_llc;
        gem5::statistics::Scalar total_put;
        gem5::statistics::Scalar total_putm;
        gem5::statistics::Scalar total_puts;
        gem5::statistics::Scalar total_put_as_wt;
        gem5::statistics::Scalar total_puts_as_wt;
        gem5::statistics::Scalar total_putm_as_wt;
        gem5::statistics::Formula wt_putRatio;
        gem5::statistics::Formula wt_putsRatio;
        gem5::statistics::Formula wt_putmRatio;

        gem5::statistics::Scalar l0_hits;
        gem5::statistics::Scalar l1_hits;
        gem5::statistics::Scalar llc_hits;
        gem5::statistics::Scalar mem_hits;

        Cycles w;

        uint64_t wcl_bound;

        void tic(Addr address) {
            w = curCycle();
            DPRINTF(XYZInfo, "Start timing %#x at: %lld\n", address, w);
        }

        void toc(Addr address) {
            latencies.sample(curCycle() - w);
            DPRINTF(XYZInfo, "Finish timing %#x at: %lld\n", address, curCycle());
            DPRINTF(XYZInfo, "Samping latency: %lld\n", curCycle() - w);
            if(curCycle() - w > wcl_bound) {
                DPRINTF(XYZInfo, "Worst case latency bound reached: %lld\n", curCycle() - w);
                panic("Worst case latency bound reached: %lld\n", curCycle() - w);
            }
        }

        void regStats() override;        
        void recordReplShared() {
            total_repl++;
            total_repl_shared++;
        }
        void recordReplOwned() {
            total_repl++;
            total_repl_owned++;
        }
        void recordReplLLC() {
            total_repl++;
            total_repl_llc++;
        }
        
        void recordL0Hits() {
            l0_hits++;
        }
        void recordL1Hits() {
            l1_hits++;
        }
        void recordLLCHits() {
            llc_hits++;
        }
        void recordMemHits() {
            mem_hits++;
        }
    };
};


#endif