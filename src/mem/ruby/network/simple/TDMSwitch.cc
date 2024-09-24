
#include <numeric>

#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/simple/SimpleNetwork.hh"
#include "mem/ruby/network/simple/TDMSwitch.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/TDMSwitch.hh"

namespace gem5 {

namespace ruby {

using stl_helpers::operator<<;

TDMSwitch::TDMSwitch(const Params& p)
    : Switch(p),
      perfectSwitch(m_id, this, p.virt_nets, p.ncore, p.enforce_roc, p.work_conserving, p.subslot_opt, p.split_bus, p.response_bus_latency, p.slot_width, p.llc_latency),
      m_num_connected_buffers(0) /*,
      switchStats(this) */ {
  m_ncore = p.ncore;
  m_port_buffers.reserve(p.port_buffers.size());
  for (auto& buffer : p.port_buffers) {
    m_port_buffers.emplace_back(buffer);
  }
}

void TDMSwitch::init() {
  BasicRouter::init();
  perfectSwitch.init(m_network_ptr);
}

void TDMSwitch::addInPort(const std::vector<MessageBuffer*>& in) {
  perfectSwitch.addInPort(in);
}

void TDMSwitch::addOutPort(const std::vector<MessageBuffer*>& out,
                           const NetDest& routing_table_entry,
                           Cycles link_latency, int bw_multiplier) {
  // Create a throttle
  
  throttles.emplace_back(m_id, m_network_ptr->params().ruby_system,
                         throttles.size(), link_latency, bw_multiplier,
                         m_network_ptr->getEndpointBandwidth(), this);
                         

  // Create one buffer per vnet (these are intermediaryQueues)
  std::vector<MessageBuffer*> intermediateBuffers;

  for (int i = 0; i < out.size(); ++i) {
    assert(m_num_connected_buffers < m_port_buffers.size());
    MessageBuffer* buffer_ptr = m_port_buffers[m_num_connected_buffers];
    m_num_connected_buffers++;
    intermediateBuffers.push_back(buffer_ptr);
  }

  // Hook the queues to the PerfectSwitch
  perfectSwitch.addOutPort(intermediateBuffers, routing_table_entry);
  // Directly hooking to out?
  // perfectSwitch.addOutPort(out, routing_table_entry);

  // Hook the queues to the Throttle
  throttles.back().addLinks(intermediateBuffers, out);
}

void TDMSwitch::regStats() {
  BasicRouter::regStats();

  
  for (auto& throttle : throttles) {
    throttle.regStats();
  }

  for (const auto& throttle : throttles) {
    switchStats.m_avg_utilization += throttle.getUtilization();
  }
  
  switchStats.m_avg_utilization /= statistics::constant(throttles.size());
  // switchStats.m_avg_utilization += statistics::constant(1);

  
   for (unsigned int type = MessageSizeType_FIRST; type < MessageSizeType_NUM;
        ++type) {
     switchStats.m_msg_counts[type] = new statistics::Formula(
         &switchStats, csprintf("msg_count.%s",
                                MessageSizeType_to_string(MessageSizeType(type)))
                           .c_str());
     switchStats.m_msg_counts[type]->flags(statistics::nozero);

     switchStats.m_msg_bytes[type] = new statistics::Formula(
         &switchStats, csprintf("msg_bytes.%s",
                                MessageSizeType_to_string(MessageSizeType(type)))
                           .c_str());
     switchStats.m_msg_bytes[type]->flags(statistics::nozero);

     for (const auto& throttle : throttles) {
       *(switchStats.m_msg_counts[type]) += throttle.getMsgCount(type);
     }

     
     *(switchStats.m_msg_bytes[type]) =
         *(switchStats.m_msg_counts[type]) *
         statistics::constant(
             Network::MessageSizeType_to_int(MessageSizeType(type)));
             
   }
   
}

void TDMSwitch::resetStats() {
  perfectSwitch.clearStats();
  for (auto& throttle : throttles) {
    throttle.clearStats();
  }
}

void TDMSwitch::collateStats() {
  perfectSwitch.collateStats();
  for (auto& throttle : throttles) {
    throttle.collateStats();
  }
}

void TDMSwitch::print(std::ostream& out) const {
  // FIXME printing
  out << "[Switch]";
}

bool TDMSwitch::functionalRead(Packet* pkt) {
  for (unsigned int i = 0; i < m_port_buffers.size(); ++i) {
    if (m_port_buffers[i]->functionalRead(pkt)) return true;
  }
  DPRINTF(TDMSwitch, "Huh????? %s\n", *this);
  for (unsigned int i = 0; i < m_port_buffers.size(); ++i) {
    DPRINTF(TDMSwitch, "Buffer: %s\n", *m_port_buffers[i]);
  }
  return false;
}

bool TDMSwitch::functionalRead(Packet* pkt, WriteMask& mask) {
  bool read = false;
  for (unsigned int i = 0; i < m_port_buffers.size(); ++i) {
    if (m_port_buffers[i]->functionalRead(pkt, mask)) read = true;
  }
  return read;
}

uint32_t TDMSwitch::functionalWrite(Packet* pkt) {
  // Access the buffers in the switch for performing a functional write
  uint32_t num_functional_writes = 0;
  for (unsigned int i = 0; i < m_port_buffers.size(); ++i) {
    num_functional_writes += m_port_buffers[i]->functionalWrite(pkt);
  }
  return num_functional_writes;
}

void TDMSwitch::markLLCDone() {
    this->perfectSwitch.markLLCDone();
}
void TDMSwitch::markLLCDone(bool wait_for_data) {
    this->perfectSwitch.markLLCDone(wait_for_data);
}
void TDMSwitch::markLLCDone(Cycles delay) {
    this->perfectSwitch.markLLCDone(delay);
}

void TDMSwitch::markTransactionDone() {
    this->perfectSwitch.markTransactionDone();
}

int TDMSwitch::getROCCount(int set) {
    return this->perfectSwitch.getROCCount(set);
}

/*
TDMSwitch::SwitchStats::SwitchStats(statistics::Group* parent)
    : statistics::Group(parent),
      m_avg_utilization(this, "percent_links_utilized") {} */

}  // namespace ruby
}  // namespace gem5
