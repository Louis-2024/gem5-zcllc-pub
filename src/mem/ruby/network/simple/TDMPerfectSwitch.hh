#ifndef __MEM_RUBY_NETWORK_SIMPLE_TDMPERFECTSWITCH_HH__
#define __MEM_RUBY_NETWORK_SIMPLE_TDMPERFECTSWITCH_HH__

#include <iostream>
#include <string>
#include <vector>
#include <list>

#include "sim/clocked_object.hh"
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/common/TypeDefines.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#include "xyz/SlotManager.hh"
#include "xyz/ROCQueue.hh"

namespace gem5
{

namespace ruby
{

class MessageBuffer;
class NetDest;
class SimpleNetwork;
class TDMSwitch;

struct LinkOrder;
/*
{
    int m_link;
    int m_value;
};
*/

bool operator<(const LinkOrder& l1, const LinkOrder& l2);


class TDMPerfectSwitch : public Consumer
{
  public:
    TDMPerfectSwitch(SwitchID sid, TDMSwitch * sw, uint32_t virt_nets, 
        int ncore, bool enforce_roc, bool work_conserving, bool subslot_opt, bool split_bus, int response_bus_latency, int slot_width, int llc_latency);
    ~TDMPerfectSwitch();

    std::string name()
    { return csprintf("TDMPerfectSwitch-%i", m_switch_id); }

    void init(SimpleNetwork *);
    void addInPort(const std::vector<MessageBuffer*>& in);
    void addOutPort(const std::vector<MessageBuffer*>& out,
                    const NetDest& routing_table_entry);

    int getInLinks() const { return m_in.size(); }
    int getOutLinks() const { return m_out.size(); }
    virtual int getCurrentSlotOwner() const;
    virtual bool isStartOfSlot() const;
    virtual int getRequestVNet() const { return 0; }
    virtual Cycles getNextCycleForCore(int c) const;
    virtual void scheduleNextSlot();
    virtual void gotoNextSlotOwner() {
        m_tdm_slot_owner_idx = m_tdm_slot_owner_idx + 1;
        if(m_tdm_slot_owner_idx >= m_schedule.size()) m_tdm_slot_owner_idx = 0;
        m_tdm_slot_owner = m_schedule[m_tdm_slot_owner_idx];
    }

    virtual void wakeup();
    void storeEventInfo(int info);

    void clearStats();
    void collateStats();
    void print(std::ostream& out) const;

    Cycles curCycle() const; 
    bool coreHasPendingRequest(int c) const;

    // denote that the bus in not active,
    // used by caches to indicate that their operations are finish'd
    void deactivate() { m_bus_active = false; }

    void markLLCDone();
    void markLLCDone(bool wait_for_data);
    void markLLCDone(Cycles delay);
    void markTransactionDone();
    int getROCCount(int set) { return this->m_roc_queue.count(); }

    int m_slot_width = 128;

   private:
    // Private copy constructor and assignment operator
    TDMPerfectSwitch(const TDMPerfectSwitch& obj);
    TDMPerfectSwitch& operator=(const TDMPerfectSwitch& obj);

    void operateVnet(int vnet);
    void operateVnet(int vnet, int message_limit);
    void operateVnet(int vnet, int deviceIdx, int message_limit);
    void operateMessageBuffer(MessageBuffer *b, int incoming, int vnet, int message_limit);
    void enqueueROCQueue(int llc_idx, bool is_front);
    void dequeuROCQueue();

    void handleSplitBusResponse();

    // mb* methods are used in conjunction with the message buffer
    // for invasive probes
    bool mbIsMsgReady(MsgPtr& msg, Tick current_time) const {
        return (msg->getLastEnqueueTime() <= current_time);
    }
    // get the destination of the message
    // used 
    template<typename MsgType>
    NetDest mbGetDestination(MsgPtr& msg) {
        const MsgType* req = dynamic_cast<const MsgType*>(msg.get());
        return req->getDestination();
    }
    template<typename MsgType>
    MachineID mbGetSrc(MsgPtr& msg) {
        const MsgType* req = dynamic_cast<const MsgType*>(msg.get());
        return req->getSender();
    }

    template<typename MsgType>
    MachineID mbGetOriginalRequestor(MsgPtr& msg) {
        const MsgType* req = dynamic_cast<const MsgType*>(msg.get());
        return req->getOriginalRequestor();
    }

    // void operateVnet_deprecated(int vnet);
    // void operateMessageBuffer_deprecated(MessageBuffer *b, int incoming, int vnet);
    void operateMessageBuffer(MessageBuffer *b, int incoming, int vnet, bool& sent);


    bool is_vanilla_tdm() const { return !this->m_work_conserving && !this->m_subslot_opt; }
    bool is_work_conserving() const { return this->m_work_conserving && !this->m_subslot_opt; }
    bool is_subslot_opt() const { return this->m_work_conserving && this->m_subslot_opt; }
    bool is_split_bus() const { return this->m_split_bus; }

    const SwitchID m_switch_id;
    TDMSwitch * const m_switch;

    // vector of queues from the components
    std::vector<std::vector<MessageBuffer*> > m_in;
    std::vector<std::vector<MessageBuffer*> > m_out;

    std::vector<NetDest> m_routing_table;
    std::vector<LinkOrder> m_link_order;
    std::vector<Cycles> m_cycles;

    uint32_t m_virtual_networks;
    int m_round_robin_start;
    int m_wakeups_wo_switch;

    int m_tdm_slot_owner;
    int m_cur_tdm_slot_owner;
    int m_tdm_slot_owner_idx;

    SimpleNetwork* m_network_ptr;
    std::vector<int> m_pending_message_count;

    std::vector<int> m_schedule;

    // Store the ordering
    std::list<int> m_order;
    // fast checking of whether a core is ordered
    std::set<int>    m_core_ordered;
    int m_ordered_slot = -1;
    Cycles m_last_opererated_cycle = Cycles(0);

    Cycles m_last_insertion_cycle = Cycles(0);

    Cycles m_last_slot_start = Cycles(0);
    Cycles m_last_scheduled_slot = Cycles(0);
    Cycles m_response_bus_latency;

    int m_ncore = -1;
    bool m_enforce_roc;

    bool m_work_conserving;
    bool m_subslot_opt;
    // whether the bus is active handling a request
    bool m_bus_active;

    bool m_split_bus;
    SlotManager m_slot_manager;
    ROCQueue m_roc_queue;
    bool m_llc_finish_signal;
    // How long does it take for the LLC signal to stop propogate to
    // the bus, this value should be greater than the network latency
    // b/c we want to ensure that the bus first propagate the LLC responses
    Cycles m_end_delay;
    EventFunctionWrapper m_llc_end_event;

    // a queue that record the request order of incoming requests, where we will handle the response corresponding to the request
    // m_request_order[i] == core means that a response is pending from OR TO core
    // this is because it is possible that a back-invalidation or a voluntary write-back are pending

    // differences b/w m_roc_queue and m_response_order
    // roc queue is only enqueued when a request cannot finish within its slot
    // and an entry is removed as soon as a request is "done" by LLC
    // but response queue is used even if a request can finish within its first attempt
    // and it is used to throttle the response bus (in shared bus, there will be slot width allocated for response bus so no need there for arbitration)
    std::list<int>* m_response_order;
};

inline std::ostream&
operator<<(std::ostream& out, const TDMPerfectSwitch& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_SIMPLE_PERFECTSWITCH_HH__
