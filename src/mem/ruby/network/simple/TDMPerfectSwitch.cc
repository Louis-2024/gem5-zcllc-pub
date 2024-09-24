#include "mem/ruby/network/simple/TDMPerfectSwitch.hh"

#include <algorithm>
#include <set>
#include <map>

#include "base/cast.hh"
#include "base/cprintf.hh"
#include "base/logging.hh"
#include "base/random.hh"
#include "base/trace.hh"
#include "debug/TDMSwitch.hh"
#include "debug/TDM.hh"
#include "debug/TDMOrder.hh"
#include "debug/ROC.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/simple/SimpleNetwork.hh"
#include "mem/ruby/network/simple/TDMSwitch.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#include "mem/ruby/protocol/RequestMsg.hh"

#include "xyz/SlotManager.hh"
#include "sim/cur_tick.hh"
#include "sim/clocked_object.hh"

// protocol specific messages
#include "mem/ruby/protocol/RequestMsg.hh"
#include "mem/ruby/protocol/ResponseMsg.hh"

namespace gem5
{

namespace ruby
{

const int PRIORITY_SWITCH_LIMIT = 128;

TDMPerfectSwitch::TDMPerfectSwitch(SwitchID sid, TDMSwitch* sw,
                                   uint32_t virt_nets, int ncore,
                                   bool enforce_roc, bool work_conserving,
                                   bool subslot_opt, bool split_bus,
                                   int response_bus_latency, int slot_width, int llc_latency)
    : Consumer(sw),
      m_switch_id(sid),
      m_switch(sw),
      m_tdm_slot_owner(0),
      m_enforce_roc(enforce_roc) /*, m_slot_width(128) */,
      m_work_conserving(work_conserving),
      m_subslot_opt(subslot_opt),
      m_split_bus(split_bus),
      m_slot_manager(*sw, *this, {}, work_conserving, subslot_opt, split_bus,
                     enforce_roc, Cycles(slot_width), ncore),
      m_llc_end_event(
          [this] {
            DPRINTF(TDMSwitch, "setting llc finish signal to true... wait for data? %d\n", this->m_slot_manager.m_llc_finish_wait_for_data_response);
            this->m_slot_manager.m_llc_finish_signal = true;
          },
          "Consumer Event", false),
      m_roc_queue(enforce_roc) {
  // panic(".%d %d", slot_width, llc_latency);
  m_round_robin_start = 0;
  m_wakeups_wo_switch = 0;
  m_virtual_networks = virt_nets;
  m_ncore = ncore;
  // m_end_delay = Cycles(5);
  m_end_delay = Cycles(llc_latency);
  m_response_bus_latency = Cycles(response_bus_latency);
  // assert(virt_nets == 4);
  // m_slot_width = Cycles(128);
  m_schedule = std::vector<int>(ncore);
  // TODO: make this alternative scheme
  std::iota(m_schedule.begin(), m_schedule.end(), 0);

  m_slot_manager.m_schedule = m_schedule;

  if(m_split_bus) {
    this->m_response_order = new std::list<int>;
  }  else {
    this->m_response_order = nullptr;
  }

  DPRINTF(TDMSwitch, "Configuration: wc %d, subslot %d, splitbus %b\n", m_work_conserving, m_subslot_opt, m_split_bus);
  // initialize vnet message limit
}

void
TDMPerfectSwitch::init(SimpleNetwork *network_ptr)
{
    m_network_ptr = network_ptr;

    for (int i = 0;i < m_virtual_networks;++i) {
        m_pending_message_count.push_back(0);
    }
}

void
TDMPerfectSwitch::addInPort(const std::vector<MessageBuffer*>& in)
{
    
    NodeID port = m_in.size();
    m_in.push_back(in);
    m_cycles.push_back(Cycles(0));

    for (int i = 0; i < in.size(); ++i) {
      DPRINTF(TDMSwitch, "Added in port! %p\n", in[i]);
      if (in[i] != nullptr) {
        in[i]->setConsumer(this);
        in[i]->setIncomingLink(port);
        in[i]->setVnet(i);
      }
    }
}

void
TDMPerfectSwitch::addOutPort(const std::vector<MessageBuffer*>& out,
                          const NetDest& routing_table_entry)
{
    // Setup link order
    LinkOrder l;
    l.m_value = 0;
    l.m_link = m_out.size();
    m_link_order.push_back(l);

    // Add to routing table
    m_out.push_back(out);
    m_routing_table.push_back(routing_table_entry);
    DPRINTF(TDMSwitch, "Adding out, routing_table_entry: %s\n", routing_table_entry);
}

TDMPerfectSwitch::~TDMPerfectSwitch()
{
}

int TDMPerfectSwitch::getCurrentSlotOwner() const {
  // return this->m_tdm_slot_owner;
  return this->m_cur_tdm_slot_owner;
}
bool TDMPerfectSwitch::isStartOfSlot() const {
  // 1. starting a new request
  // or 2. from a prior scheduled slot
  return (this->curCycle() - this->m_last_slot_start) >= this->m_slot_width;
}

Cycles TDMPerfectSwitch::getNextCycleForCore(int c) const {
  auto currentSlotStart =
    this->m_switch->curCycle() / this->m_slot_width * this->m_slot_width;
    panic_if((currentSlotStart % this->m_slot_width != 0), "Slot start should be divided");
    auto so = getCurrentSlotOwner();
    auto delta = (c - so + m_ncore) % m_ncore;
    if(c == so)
        delta = m_ncore;
    // if inlinks() == 5, it's 4 core
    panic_if(currentSlotStart + delta * m_slot_width < this->m_switch->curCycle(), "Should be scheduled later, c: %d, ncore: %d", c, m_ncore);
     return Cycles(
        currentSlotStart + delta * m_slot_width - this->m_switch->curCycle());
}
void
TDMPerfectSwitch::operateVnet(int vnet) {
    // This is for round-robin scheduling
    int incoming = m_round_robin_start;
    m_round_robin_start++;
    if (m_round_robin_start >= m_in.size()) {
        m_round_robin_start = 0;
    }

    if (m_pending_message_count[vnet] > 0) {
        // for all input ports, use round robin scheduling
        for (int counter = 0; counter < m_in.size(); counter++) {
            // Round robin scheduling
            incoming++;
            if (incoming >= m_in.size()) {
                incoming = 0;
            }
            operateVnet(vnet, incoming, -1);
        }
    }
}

void
TDMPerfectSwitch::operateVnet(int vnet, int message_limit) {
    // This is for round-robin scheduling
    int incoming = m_round_robin_start;
    m_round_robin_start++;
    if (m_round_robin_start >= m_in.size()) {
        m_round_robin_start = 0;
    }

    if (m_pending_message_count[vnet] > 0) {
        // for all input ports, use round robin scheduling
        for (int counter = 0; counter < m_in.size(); counter++) {
            // Round robin scheduling
            incoming++;
            if (incoming >= m_in.size()) {
                incoming = 0;
            }
            operateVnet(vnet, incoming, message_limit);
        }
    }
}

void
TDMPerfectSwitch::operateVnet(int vnet, int deviceIdx, int message_limit) {
    // Is there a message waiting?
    int incoming = deviceIdx;

    if (m_in[incoming].size() <= vnet) {
        return;
    }

    MessageBuffer *buffer = m_in[incoming][vnet];
    if (buffer == nullptr) {
        return;
    }

    operateMessageBuffer(buffer, incoming, vnet, message_limit);
}

void
TDMPerfectSwitch::operateMessageBuffer(MessageBuffer *buffer, int incoming,
                                    int vnet, int message_limit)
{
    MsgPtr msg_ptr;
    Message *net_msg_ptr = NULL;

    // temporary vectors to store the routing results
    std::vector<LinkID> output_links;
    std::vector<NetDest> output_link_destinations;
    Tick current_time = m_switch->clockEdge();

    int sent_messages = 0;
    while (buffer->isReady(current_time) && ((message_limit < 0) || sent_messages < message_limit) ) {
        DPRINTF(RubyNetwork, "incoming: %d\n", incoming);

        // Peek at message
        msg_ptr = buffer->peekMsgPtr();
        net_msg_ptr = msg_ptr.get();
        DPRINTF(RubyNetwork, "Message: %s\n", (*net_msg_ptr));

        output_links.clear();
        output_link_destinations.clear();
        NetDest msg_dsts = net_msg_ptr->getDestination();

        // Unfortunately, the token-protocol sends some
        // zero-destination messages, so this assert isn't valid
        // assert(msg_dsts.count() > 0);

        assert(m_link_order.size() == m_routing_table.size());
        assert(m_link_order.size() == m_out.size());

        if (m_network_ptr->getAdaptiveRouting()) {
            if (m_network_ptr->isVNetOrdered(vnet)) {
                // Don't adaptively route
                for (int out = 0; out < m_out.size(); out++) {
                    m_link_order[out].m_link = out;
                    m_link_order[out].m_value = 0;
                }
            } else {
                // Find how clogged each link is
                for (int out = 0; out < m_out.size(); out++) {
                    int out_queue_length = 0;
                    for (int v = 0; v < m_virtual_networks; v++) {
                        out_queue_length += m_out[out][v]->getSize(current_time);
                    }
                    int value =
                        (out_queue_length << 8) |
                        random_mt.random(0, 0xff);
                    m_link_order[out].m_link = out;
                    m_link_order[out].m_value = value;
                }

                // Look at the most empty link first
                sort(m_link_order.begin(), m_link_order.end());
            }
        }

        for (int i = 0; i < m_routing_table.size(); i++) {
            // pick the next link to look at
            int link = m_link_order[i].m_link;
            NetDest dst = m_routing_table[link];
            DPRINTF(RubyNetwork, "dst: %s\n", dst);

            if (!msg_dsts.intersectionIsNotEmpty(dst))
                continue;

            // Remember what link we're using
            output_links.push_back(link);

            // Need to remember which destinations need this message in
            // another vector.  This Set is the intersection of the
            // routing_table entry and the current destination set.  The
            // intersection must not be empty, since we are inside "if"
            output_link_destinations.push_back(msg_dsts.AND(dst));

            // Next, we update the msg_destination not to include
            // those nodes that were already handled by this link
            msg_dsts.removeNetDest(dst);
        }

        assert(msg_dsts.count() == 0);

        // Check for resources - for all outgoing queues
        bool enough = true;
        for (int i = 0; i < output_links.size(); i++) {
            int outgoing = output_links[i];

            if (!m_out[outgoing][vnet]->areNSlotsAvailable(1, current_time))
                enough = false;

            DPRINTF(RubyNetwork, "Checking if node is blocked ..."
                    "outgoing: %d, vnet: %d, enough: %d\n",
                    outgoing, vnet, enough);
        }

        // There were not enough resources
        if (!enough) {
            scheduleEvent(Cycles(1));
            DPRINTF(RubyNetwork, "Can't deliver message since a node "
                    "is blocked\n");
            DPRINTF(RubyNetwork, "Message: %s\n", (*net_msg_ptr));
            break; // go to next incoming port
        }

        MsgPtr unmodified_msg_ptr;

        if (output_links.size() > 1) {
            // If we are sending this message down more than one link
            // (size>1), we need to make a copy of the message so each
            // branch can have a different internal destination we need
            // to create an unmodified MsgPtr because the MessageBuffer
            // enqueue func will modify the message

            // This magic line creates a private copy of the message
            unmodified_msg_ptr = msg_ptr->clone();
        }

        // Dequeue msg
        buffer->dequeue(current_time);
        m_pending_message_count[vnet]--;

        // Enqueue it - for all outgoing queues
        for (int i=0; i<output_links.size(); i++) {
            int outgoing = output_links[i];

            if (i > 0) {
                // create a private copy of the unmodified message
                msg_ptr = unmodified_msg_ptr->clone();
            }

            // Change the internal destination set of the message so it
            // knows which destinations this link is responsible for.
            net_msg_ptr = msg_ptr.get();
            net_msg_ptr->getDestination() = output_link_destinations[i];

            // Enqeue msg
            DPRINTF(RubyNetwork, "Enqueuing net msg from "
                    "inport[%d][%d] to outport [%d][%d].\n",
                    incoming, vnet, outgoing, vnet);

            m_out[outgoing][vnet]->enqueue(msg_ptr, current_time,
                                           m_switch->cyclesToTicks(Cycles(1)));
        }
        sent_messages ++;
        DPRINTF(RubyNetwork, "Msg "
                    "inport[%d][%d]. %d\n",
                    incoming, vnet, sent_messages);
    }
}

/**
 * Get the request from ROCVnet and put it in ROC buffer
*/
void
TDMPerfectSwitch::enqueueROCQueue(int llc_idx, bool is_front) {
    DPRINTF(TDMSwitch, "Before: %s\n", this->m_roc_queue);
    const int ROCVnet = 3;
    // if failure something is wrong
    auto buffer = m_in[llc_idx][ROCVnet];
    panic_if(!buffer->isReady(curTick()), "ROC buffer should be ready");

    Tick current_time = m_switch->clockEdge();
    m_pending_message_count[ROCVnet]--;

    MsgPtr ptr = buffer->peekMsgPtr();
    buffer->dequeue(current_time);

    DPRINTF(TDMSwitch, "enqueueROCQueue: idx: %d\n", this->m_slot_manager.getCoreIDFromRequest(&*ptr));
    if(is_front) {
        DPRINTF(TDMSwitch, "enqueueROCQueue front\n");
        this->m_roc_queue.enqueueFrontROCRequest(this->m_slot_manager.getSlotOwner(),
                                          ptr);
    } else {
        DPRINTF(TDMSwitch, "enqueueROCQueue back\n");
        this->m_roc_queue.enqueueROCRequest(this->m_slot_manager.getSlotOwner(),
                                          ptr);
    }
    DPRINTF(TDMSwitch, "After: %s\n", this->m_roc_queue);
}

void TDMPerfectSwitch::dequeuROCQueue() {
    DPRINTF(TDMSwitch, "> Before: %s\n", this->m_roc_queue);
    DPRINTF(TDMSwitch, "dequeuROCQueue\n");
    const int RequestVnet = 0;
    int core = this->m_slot_manager.getSlotOwner();
    int llc_idx = this->m_slot_manager.getLLCDeviceId();
    // if failure something is wrong
    auto buffer = m_out[llc_idx][RequestVnet];

    Tick current_time = m_switch->clockEdge();

    // TODO: fix
    MsgPtr ptr = this->m_roc_queue.dequeueROCRequest(core);

    buffer->enqueue(ptr, current_time, m_switch->cyclesToTicks(Cycles(1)));

    DPRINTF(TDMSwitch, "> After: %s\n", this->m_roc_queue);
}

void
TDMPerfectSwitch::wakeup() {
    /* There are several possibilities when wakeup is called
     * that we model:
     * 1. vanilla TDM (!m_work_conserving && !m_subslot_opt )
     * 2. Work conserving (m_work_conserving && !m_subslot_opt)
     * 3. WC + Subslot (m_work_conserving && m_subslot_opt)
     * In 1 and 2, once a slot starts, it will always be slot width
     * In 3, the slot length will be variable, depending on whether it hits or misses in the next level cache
     */

    /* there are 3 sources that can wakeup the bus
     * 1. the caches: (a) issue new request (b) send BI response
     * 2. the LLC: (a) send response, (b) send request for issuing later, and we need to maintain order for it
     *             (c) send back-invalidation
     * 3. the bus: self-scheduled wake-up - (a) send requests in ROC order
     * 
     * 1.a and 3.a are subject to delays and may be scheduled in futher cycles
     * 
     * 1.b, 2.a, 2.b, 2.c, 3.a can be processed immediately, however, these should only happen
     * for the bus owner - we don't have dedicated data buses anyways
     */

    // Check SlotManager.hh for the rationale of how the bus handle events

    m_slot_manager.updateBusStatus();
    auto actions = m_slot_manager.getNextEvent(
        this->m_in,
        this->m_roc_queue,
        this->m_response_order,
        this->m_llc_finish_signal
    );
    // bool scheduled = false;

    for(auto action : actions) {
        if(action.act == action.OpVnet)  {
            // for response this would be different
            panic_if(action.OpVnet == SlotManager::SplitResponseVnet, "SplitResposneVnet operates asynchronously, and should not be accessed by the slot manager");
            operateVnet(action.vnet, action.deviceIdx, action.messageLimit);
        } else if(action.act == action.ROCQueue) {
            // ROC
            enqueueROCQueue(action.deviceIdx, false);
        } else if(action.act == action.ROCQueueFront) {
            enqueueROCQueue(action.deviceIdx, true);
        } else if(action.act == action.ROCDequeue) {
            dequeuROCQueue();
        } else if(action.act == action.SchedEnd) {
            panic("SchedEnd is not implemented");
        } else {
            panic("Unrecognized action");
        }
    }

    if(this->is_split_bus()) {
        // for filtering past requests which may have been satisfied through c2c interconnect
        // we need to remove that from the response order arbitration
        handleSplitBusResponse();
    }


    // panic_if(!scheduled, "TDM bus is woken up but no action is scheduled, this could lead to deadlock");
}

void
TDMPerfectSwitch::handleSplitBusResponse() {
    // if this is split bus instance, the response bus needs to operate asynchronously
    // response bus occupies vnet 6 only
    // Three cases of request path:
    // 1. Request [C -> D Push Req] (if splitBus then markLLCDone) -> Response [D -> C Pop Req]
    // 2. Request BI [C -> D Push Req] -> BIReq [D -> C' Push Req] -> BIResponse [C' -> D PushReq] -> (MemResponse) -> Response [D-> C Pop Req]
    // 3. WB [C -> D Push Req] -> Resposne [C -> D Pop Req]
    // 4. Request [C -> D Push Req] -> FwdReq [D -> C' Push Req] -> FwdResponse [C' -> D PushReq]
    // Responses are ordered by the initial request
    auto llc_device_idx = 0;
    panic_if(m_in[llc_device_idx].size() <= SlotManager::SplitResponseVnet, "Cannot find split response vnet on LLC, perhaps there's an error in configuration");

    auto* llc_response_mb = m_in[llc_device_idx][SlotManager::SplitResponseVnet];
    auto current_time = curTick();

    // get the cores that have a inward or outward response message 
    // the indices are core indices
    const int dir_to_core = 0;
    const int core_to_dir = 1;
    // [arbitration, [buffer_idx, dir]]
    std::map<int, std::tuple<int, int> > current_message_destinations;

    // get the # of msgs at this time
    auto n_message = llc_response_mb->getSize(curTick());
    // collect all responses over the split bus
    // LLC response
    // Since LLC response always signifies the end of a transaction
    // the destination is the slot that we will occupy
    for(auto msg : llc_response_mb->m_prio_heap) {
        if(!mbIsMsgReady(msg, current_time)) continue;
        // this must be the response message from the LLC
        auto destinations /*: NetDest*/ = mbGetDestination<ResponseMsg>(msg);
        int any_message = 0;
        for(int i = 0; i < m_ncore; i++) {
            if(destinations.isElement(MachineID(MachineType::MachineType_L1Cache, i))) {
                int device_idx = m_slot_manager.coreToDeviceId(i);
                current_message_destinations.insert({device_idx, {0, dir_to_core} });
                any_message++;
            }
        }
        panic_if(any_message > 1, "Cannot check message into llc_response");
        // msg is ready
    }
    // BI responses or FwdGetM responses
    for(int device_idx = 1; device_idx < m_in.size(); device_idx++) {
        panic_if(m_in[device_idx].size() <= SlotManager::SplitResponseVnet, "Split bus enabled on LLC but it is not connected");
        auto core_response_mb = m_in[device_idx][SlotManager::SplitResponseVnet];
        if(!core_response_mb->isReady(current_time)) continue;
        auto msg = core_response_mb->peekMsgPtr();
        auto originRequestor = this->mbGetOriginalRequestor<ResponseMsg>(msg);
        auto originalRequestorDeviceIdx = m_slot_manager.coreToDeviceId(originRequestor.getNum());
        current_message_destinations.insert({originalRequestorDeviceIdx, {device_idx, core_to_dir}});
    }

    DPRINTF(TDMSwitch, "current_message_destinations (response split bus): %d entries\n", current_message_destinations.size());
    for(auto it : current_message_destinations) {
        auto [device_idx, dir] = it.second;
        DPRINTF(TDMSwitch, "device_idx (slot to occupy): %d, (buffer to operate): %d, dir: %d\n", it.first, device_idx, dir);
    }

    DPRINTF(TDMSwitch, "current response order: ");
    for(auto it : *m_response_order) {
        DPRINTF(TDMSwitch, "%d \n", it);
    }

    // dispatch the collected messages
    // with work-conserving arbitration
    // selected = false;
    for(auto it = this->m_response_order->begin(); it != this->m_response_order->end(); it++) {
        auto device_idx = *it;
        auto core = m_slot_manager.deviceIdToCore(device_idx);
        panic_if(m_in[device_idx].size() <= SlotManager::SplitResponseVnet, "Cannot find split response vnet on private L2, perhaps there's an error in configuration");
        auto it_dir = current_message_destinations.find(device_idx);
        if(it_dir == current_message_destinations.end()) continue;

        auto [buffer_device_idx, dir] = it_dir->second;

        // now we know that there is ONE message that we could handle, either it is in the LLC response buffer, or it is in the core response buffer
        if(dir == dir_to_core) {
            // okay but we need to delay the head for later scheduling
            auto head_msg_ptr = llc_response_mb->peekMsgPtr();
            auto initial_value = head_msg_ptr.get();
            auto destinations /*: NetDest*/ = mbGetDestination<ResponseMsg>(head_msg_ptr);
            // delay until we find the correct head
            while(!destinations.isElement(MachineID(MachineType::MachineType_L1Cache, core))) {
                // delay head
                llc_response_mb->delayHead(curTick(), getObject()->cyclesToTicks(m_response_bus_latency));

                // ok and we move to next message
                head_msg_ptr = llc_response_mb->peekMsgPtr();
                destinations /*: NetDest*/ = mbGetDestination<ResponseMsg>(head_msg_ptr);

                panic_if(initial_value == head_msg_ptr.get(), "Cannot find the correct head after iterating through the whole llc response buffer");
            }
            // int head_device_idx = ;
            this->operateVnet(SlotManager::SplitResponseVnet, m_slot_manager.getLLCDeviceId(), 1);
            DPRINTF(TDMSwitch, "Split bus: LLC -> Core %d, deviceIdx = %d\n", core, m_slot_manager.getLLCDeviceId());
            m_response_order->erase(it);
        } else {  // core to LLC
            this->operateVnet(SlotManager::SplitResponseVnet, buffer_device_idx, 1);
            int buffer_core = m_slot_manager.deviceIdToCore(buffer_device_idx);
            DPRINTF(TDMSwitch, "Split bus: Core %d -> LLC, using slot of Core %d\n", buffer_core, core);
        }
        break;
    }
    if(current_message_destinations.size() > 1) {
        scheduleEvent(Cycles(m_response_bus_latency));
    }
}

void
TDMPerfectSwitch::storeEventInfo(int info)
{
    m_pending_message_count[info]++;
}

void
TDMPerfectSwitch::clearStats()
{
}
void
TDMPerfectSwitch::collateStats()
{
}


void
TDMPerfectSwitch::print(std::ostream& out) const
{
    out << "[TDMPerfectSwitch " << m_switch_id << "]";
}

void
TDMPerfectSwitch::scheduleNextSlot() {
  // this is to prevent the slot being scheduled too many times
  if (m_last_scheduled_slot <= curCycle()) {
    auto nxt_slot = m_last_slot_start + this->m_slot_width;
    scheduleEvent(Cycles(nxt_slot - curCycle()));
    DPRINTF(TDM, "WCRR scheduling next slot to %d from %d!\n", nxt_slot, curCycle());
    m_last_scheduled_slot = Cycles(nxt_slot);
  }
}

Cycles TDMPerfectSwitch::curCycle() const { return this->m_switch->curCycle(); }
bool TDMPerfectSwitch::coreHasPendingRequest(int c) const {
  // whether a core has pending request
  // core c, vnet 0
  // incoming?
  return c == m_order.front() ||
         (m_in[c + 1][0] != nullptr && !m_in[c + 1][0]->isEmpty());
}

void TDMPerfectSwitch::markLLCDone() {
    // we immediately schedule at next cycle when the LLC tells us that it's done
    ClockedObject* em = this->getObject();
    if(m_llc_end_event.scheduled()) {
        DPRINTF(TDM, "LLC done, RE-scheduling sometime later\n");
        em->reschedule(m_llc_end_event, em->clockEdge(m_end_delay), true);
    } else {
        DPRINTF(TDM, "LLC done, scheduling sometime later\n");
        em->schedule(m_llc_end_event, em->clockEdge(m_end_delay));
    }
    scheduleEvent(m_end_delay + Cycles(1));
    // The LLC should be finished at the momment
}

void TDMPerfectSwitch::markLLCDone(bool wait_for_data) {
    markLLCDone();
    this->m_slot_manager.m_llc_finish_wait_for_data_response = wait_for_data;
}

// useful when we account for the response bus latency!
void TDMPerfectSwitch::markLLCDone(Cycles delay) {
    // we immediately schedule at next cycle when the LLC tells us that it's done
    ClockedObject* em = this->getObject();
    if(m_llc_end_event.scheduled()) {
        DPRINTF(TDM, "LLC done, RE-scheduling sometime later\n");
        em->reschedule(m_llc_end_event, em->clockEdge(delay), true);
    } else {
        DPRINTF(TDM, "LLC done, scheduling sometime later\n");
        em->schedule(m_llc_end_event, em->clockEdge(delay));
    }
    scheduleEvent(delay + Cycles(1));
    // The LLC should be finished at the momment
}

void TDMPerfectSwitch::markTransactionDone() {
    panic_if(true, "markTransactionDone is not implemented");
}

} // namespace ruby
} // namespace gem5
