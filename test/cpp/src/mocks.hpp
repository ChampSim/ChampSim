#include <deque>
#include <limits>

#include "cache.h"
#include "memory_class.h"
#include "operable.h"

/*
 * A MemoryRequestConsumer that simply returns all packets on the next cycle
 */
class do_nothing_MRC : public MemoryRequestConsumer, public champsim::operable
{
  std::deque<PACKET> packets, ready_packets;
  champsim::address ret_data_base{0x11111111};
  int steps = 0;
  const uint64_t latency = 0;

  void add(PACKET pkt) {
    pkt.event_cycle = current_cycle + latency;
    pkt.data = ret_data_base + steps;
    ++steps;
    addresses.push_back(pkt.address);
    packets.push_back(pkt);
  }

  public:
    std::deque<champsim::address> addresses{};
    do_nothing_MRC(uint64_t lat) : MemoryRequestConsumer(), champsim::operable(1), latency(lat) {}
    do_nothing_MRC() : do_nothing_MRC(0) {}

    void operate() override {
      auto end = std::find_if_not(std::begin(packets), std::end(packets), [cycle=current_cycle](const PACKET &x){ return x.event_cycle <= cycle; });
      std::move(std::begin(packets), end, std::back_inserter(ready_packets));
      packets.erase(std::begin(packets), end);

      for (PACKET &pkt : ready_packets) {
        for (auto ret : pkt.to_return)
          ret->return_data(pkt);
      }
      ready_packets.clear();
    }

    bool add_rq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_wq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_pq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_ptwq(const PACKET &pkt) override { add(pkt); return true; }

    std::size_t get_occupancy(uint8_t, champsim::address) const override { return std::size(packets); }
    std::size_t get_size(uint8_t, champsim::address) const override { return std::numeric_limits<uint32_t>::max(); }

    std::size_t packet_count() const { return std::size(addresses); }
};

/*
 * A MemoryRequestConsumer that returns only a particular address
 */
class filter_MRC : public MemoryRequestConsumer, public champsim::operable
{
  std::deque<PACKET> packets, ready_packets;
  const champsim::address ret_addr;
  const uint64_t latency = 0;
  std::size_t mpacket_count = 0;

  void add(PACKET pkt) {
    if (pkt.address == ret_addr) {
      pkt.event_cycle = current_cycle + latency;
      packets.push_back(pkt);
      ++mpacket_count;
    }
  }

  public:
    filter_MRC(champsim::address ret_addr_, uint64_t lat) : MemoryRequestConsumer(), champsim::operable(1), ret_addr(ret_addr_), latency(lat) {}
    filter_MRC(champsim::address ret_addr_) : filter_MRC(ret_addr_, 0) {}

    void operate() override {
      auto end = std::find_if_not(std::begin(packets), std::end(packets), [cycle=current_cycle](const PACKET &x){ return x.event_cycle <= cycle; });
      std::move(std::begin(packets), end, std::back_inserter(ready_packets));

      for (PACKET &pkt : ready_packets) {
        for (auto ret : pkt.to_return)
          ret->return_data(pkt);
      }
      ready_packets.clear();
    }

    bool add_rq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_wq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_pq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_ptwq(const PACKET &pkt) override { add(pkt); return true; }

    std::size_t get_occupancy(uint8_t, champsim::address) const override { return std::size(packets); }
    std::size_t get_size(uint8_t, champsim::address) const override { return std::numeric_limits<uint32_t>::max(); }

    std::size_t packet_count() const { return mpacket_count; }
};

/*
 * A MemoryRequestConsumer that releases blocks when instructed to
 */
class release_MRC : public MemoryRequestConsumer, public champsim::operable
{
  std::deque<PACKET> packets;
  std::size_t mpacket_count = 0;

  void add(PACKET pkt) {
      packets.push_back(pkt);
      ++mpacket_count;
  }

  public:
    release_MRC() : MemoryRequestConsumer(), champsim::operable(1) {}

    void operate() override {}

    bool add_rq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_wq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_pq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_ptwq(const PACKET &pkt) override { add(pkt); return true; }

    std::size_t get_occupancy(uint8_t, champsim::address) const override { return std::size(packets); }
    std::size_t get_size(uint8_t, champsim::address) const override { return std::numeric_limits<uint32_t>::max(); }

    std::size_t packet_count() const { return mpacket_count; }

    void release(champsim::address addr)
    {
        auto pkt_it = std::find_if(std::begin(packets), std::end(packets), [addr](auto x){ return x.address == addr; });
        if (pkt_it != std::end(packets)) {
            for (auto ret : pkt_it->to_return) {
                ret->return_data(*pkt_it);
            }
        }
        packets.erase(pkt_it);
    }
};

/*
 * A MemoryRequestProducer that counts how many returns it receives
 */
class counting_MRP : public MemoryRequestProducer
{
  public:
  unsigned count = 0;

  counting_MRP() : MemoryRequestProducer(nullptr) {}

  void return_data(const PACKET&) override {
    count++;
  }
};

template <typename MRC, typename Fun>
class queue_issue_MRP : public MemoryRequestProducer, public champsim::operable
{
  public:
    struct result_data {
      PACKET pkt;
      uint64_t issue_time;
      uint64_t return_time;
    };
    std::deque<result_data> packets;

    Fun issue_func;

    queue_issue_MRP(MRC* ll, Fun func) : MemoryRequestProducer(ll), champsim::operable(1), issue_func(func) {}

    void operate() override {}

    bool issue(const PACKET &pkt) {
      auto copy = pkt;
      copy.to_return = {this};
      packets.push_back({copy, current_cycle, 0});
      return issue_func(*static_cast<MRC*>(lower_level), copy);
    }

    void return_data(const PACKET &pkt) override {
      auto it = std::find_if(std::rbegin(packets), std::rend(packets), [addr=pkt.address](auto x) {
          return x.pkt.address == addr;
          });
      it->return_time = current_cycle;
    }
};

/*
 * A MemoryRequestProducer that sends its packets to the write queue and notes when packets are returned
 */
template <typename MRC>
class to_wq_MRP : public queue_issue_MRP<MRC, decltype(std::mem_fn(&MRC::add_wq))>
{
  using super_type = queue_issue_MRP<MRC, decltype(std::mem_fn(&MRC::add_wq))>;
  public:
    explicit to_wq_MRP(MRC* ll) : super_type(ll, std::mem_fn(&MRC::add_wq)) {}
};

/*
 * A MemoryRequestProducer that sends its packets to the read queue and notes when packets are returned
 */
template <typename MRC>
class to_rq_MRP : public queue_issue_MRP<MRC, decltype(std::mem_fn(&MRC::add_rq))>
{
  using super_type = queue_issue_MRP<MRC, decltype(std::mem_fn(&MRC::add_rq))>;
  public:
    explicit to_rq_MRP(MRC* ll) : super_type(ll, std::mem_fn(&MRC::add_rq)) {}
};

/*
 * A MemoryRequestProducer that sends its packets to the read queue and notes when packets are returned
 */
template<typename MRC>
class to_pq_MRP : public queue_issue_MRP<MRC, decltype(std::mem_fn(&MRC::add_pq))>
{
  using super_type = queue_issue_MRP<MRC, decltype(std::mem_fn(&MRC::add_pq))>;
  public:
    explicit to_pq_MRP(MRC* ll) : super_type(ll, std::mem_fn(&MRC::add_pq)) {}
};

