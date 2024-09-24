#ifndef __MEM_RUBY_NETWORK_SIMPLE_TDMSWITCH_HH__
#define __MEM_RUBY_NETWORK_SIMPLE_TDMSWITCH_HH__

#include <iostream>
#include <list>
#include <vector>

#include "mem/packet.hh"
#include "mem/ruby/common/TypeDefines.hh"
#include "mem/ruby/network/BasicRouter.hh"
#include "mem/ruby/network/simple/Switch.hh"
#include "mem/ruby/network/simple/TDMPerfectSwitch.hh"
#include "mem/ruby/network/simple/Throttle.hh"
#include "mem/ruby/protocol/MessageSizeType.hh"
#include "params/TDMSwitch.hh"

namespace gem5 {

namespace ruby {

class MessageBuffer;
class NetDest;
class SimpleNetwork;

class TDMSwitch : public Switch {
 public:
  typedef TDMSwitchParams Params;
  TDMSwitch(const Params& p);
  ~TDMSwitch() = default;
  void init();

  virtual void addInPort(const std::vector<MessageBuffer*>& in);
  virtual void addOutPort(const std::vector<MessageBuffer*>& out,
                  const NetDest& routing_table_entry, Cycles link_latency,
                  int bw_multiplier);

  virtual void resetStats();
  virtual void collateStats();
  virtual void regStats();
  virtual const statistics::Formula& getMsgCount(unsigned int type) const {
    return *(switchStats.m_msg_counts[type]);
  }

  virtual void print(std::ostream& out) const;
  virtual void init_net_ptr(SimpleNetwork* net_ptr) { m_network_ptr = net_ptr; }

  virtual bool functionalRead(Packet*);
  virtual bool functionalRead(Packet*, WriteMask&);
  virtual uint32_t functionalWrite(Packet*);

  void markLLCDone();
  void markLLCDone(bool wait_for_data);
  void markLLCDone(Cycles delay);
  int getROCCount(int set);
  void markTransactionDone();

 private:
  // Private copy constructor and assignment operator
  TDMSwitch(const TDMSwitch& obj);
  TDMSwitch& operator=(const TDMSwitch& obj);

  TDMPerfectSwitch perfectSwitch;
  SimpleNetwork* m_network_ptr;
  std::list<Throttle> throttles;

  unsigned m_num_connected_buffers;
  std::vector<MessageBuffer*> m_port_buffers;
  int m_ncore;
  bool m_roc;

 public:
 /*
  struct SwitchStats : public statistics::Group {
    SwitchStats(statistics::Group* parent);

    // Statistical variables
    statistics::Formula m_avg_utilization;
    statistics::Formula* m_msg_counts[MessageSizeType_NUM];
    statistics::Formula* m_msg_bytes[MessageSizeType_NUM];
  } switchStats; */
};

inline std::ostream& operator<<(std::ostream& out, const TDMSwitch& obj) {
  obj.print(out);
  out << std::flush;
  return out;
}
} // namespace ruby
} // namespace gem5

#endif  // __MEM_RUBY_NETWORK_SIMPLE_TDMSWITCH_HH__
