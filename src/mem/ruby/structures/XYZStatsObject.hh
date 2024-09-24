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

        Cycles w;

        uint64_t wcl_bound;

        void tic() {
            w = curCycle();
        }

        void toc() {
            latencies.sample(curCycle() - w);
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
        void recordPutM(bool is_write_through) {
            total_put++;
            total_putm++;
            if(is_write_through) {
                total_putm_as_wt++;
                total_put_as_wt++;
            }
        };
        void recordPutS(bool is_write_through) {
            total_put++;
            total_puts++;
            if(is_write_through) {
                total_puts_as_wt++;
                total_put_as_wt++;
            }
        };
    };
};


#endif