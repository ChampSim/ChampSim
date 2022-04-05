#include <deque>
#include <limits>

#include "memory_class.h"
#include "operable.h"

/*
 * A MemoryRequestConsumer that simply returns all packets on the next cycle
 */
class do_nothing_MRC : public MemoryRequestConsumer, public champsim::operable
{
  std::deque<PACKET> packets, ready_packets;
  uint64_t ret_data = 0x11111111;
  const uint64_t latency = 0;
  unsigned mpacket_count = 0;

  void add(PACKET pkt) {
    pkt.event_cycle = current_cycle + latency;
    packets.push_back(pkt);
    ++mpacket_count;
  }

  public:
    do_nothing_MRC(uint64_t latency) : MemoryRequestConsumer(), champsim::operable(1), latency(latency) {}
    do_nothing_MRC() : do_nothing_MRC(0) {}

    void operate() {
      auto end = std::find_if_not(std::begin(packets), std::end(packets), [cycle=current_cycle](const PACKET &x){ return x.event_cycle <= cycle; });
      std::move(std::begin(packets), end, std::back_inserter(ready_packets));

      for (PACKET &pkt : ready_packets) {
        pkt.data = ++ret_data;
        for (auto ret : pkt.to_return)
          ret->return_data(pkt);
      }
      ready_packets.clear();
    }

    bool add_rq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_wq(const PACKET &pkt) override { add(pkt); return true; }
    bool add_pq(const PACKET &pkt) override { add(pkt); return true; }

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override { return std::size(packets); }
    uint32_t get_size(uint8_t queue_type, uint64_t address) override { return std::numeric_limits<uint32_t>::max(); }

    unsigned packet_count() const { return mpacket_count; }
};

/*
 * A MemoryRequestProducer that counts how many returns it receives
 */
class counting_MRP : public MemoryRequestProducer
{
  public:
  unsigned count = 0;

  counting_MRP() : MemoryRequestProducer(nullptr) {}

  void return_data(const PACKET &pkt) override {
    count++;
  }
};

/*
 * A MemoryRequestProducer that sends its packets to the write queue and notes when packets are returned
 */
class to_wq_MRP : public MemoryRequestProducer, public champsim::operable
{
  public:
    struct result_data {
      PACKET pkt;
      uint64_t issue_time;
      uint64_t return_time;
    };
    std::deque<result_data> packets;

    to_wq_MRP(MemoryRequestConsumer* ll) : MemoryRequestProducer(ll), champsim::operable(1) {}

    void operate() override {}

    bool issue(const PACKET &pkt) {
      packets.push_back({pkt, current_cycle, 0});
      return lower_level->add_wq(pkt);
    }

    void return_data(const PACKET &pkt) override {
      auto it = std::find_if(std::rbegin(packets), std::rend(packets), [addr=pkt.address](auto x) {
          return x.pkt.address == addr;
          });
      it->return_time = current_cycle;
    }
};

/*
 * A MemoryRequestProducer that sends its packets to the read queue and notes when packets are returned
 */
class to_rq_MRP : public MemoryRequestProducer, public champsim::operable
{
  public:
    struct result_data {
      PACKET pkt;
      uint64_t issue_time;
      uint64_t return_time;
    };
    std::deque<result_data> packets;

    to_rq_MRP(MemoryRequestConsumer* ll) : MemoryRequestProducer(ll), champsim::operable(1) {}

    void operate() override {}

    bool issue(const PACKET &pkt) {
      packets.push_back({pkt, current_cycle, 0});
      return lower_level->add_rq(pkt);
    }

    void return_data(const PACKET &pkt) override {
      auto it = std::find_if(std::rbegin(packets), std::rend(packets), [addr=pkt.address](auto x) {
          return x.pkt.address == addr;
          });
      it->return_time = current_cycle;
    }
};

/*
 * A MemoryRequestProducer that sends its packets to the read queue and notes when packets are returned
 */
class to_pq_MRP : public MemoryRequestProducer, public champsim::operable
{
  public:
    struct result_data {
      PACKET pkt;
      uint64_t issue_time;
      uint64_t return_time;
    };
    std::deque<result_data> packets;

    to_pq_MRP(MemoryRequestConsumer* ll) : MemoryRequestProducer(ll), champsim::operable(1) {}

    void operate() override {}

    bool issue(const PACKET &pkt) {
      packets.push_back({pkt, current_cycle, 0});
      return lower_level->add_pq(pkt);
    }

    void return_data(const PACKET &pkt) override {
      auto it = std::find_if(std::rbegin(packets), std::rend(packets), [addr=pkt.address](auto x) {
          return x.pkt.address == addr;
          });
      it->return_time = current_cycle;
    }
};
