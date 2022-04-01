#include <deque>
#include <limits>

#include "memory_class.h"
#include "operable.h"

/*
 * A MemoryRequestConsumer that simply returns all packets on the next cycle
 */
class do_nothing_MRC : public MemoryRequestConsumer, public champsim::operable
{
  std::deque<PACKET> packets;
  public:
    do_nothing_MRC() : MemoryRequestConsumer(), champsim::operable(1) {}

    void operate() {
      for (const PACKET &pkt : packets)
        for (auto ret : pkt.to_return)
          ret->return_data(pkt);
      packets.clear();
    }

    bool add_rq(const PACKET &pkt) override { packets.push_back(pkt); return true; }
    bool add_wq(const PACKET &pkt) override { packets.push_back(pkt); return true; }
    bool add_pq(const PACKET &pkt) override { packets.push_back(pkt); return true; }

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override { return std::size(packets); }
    uint32_t get_size(uint8_t queue_type, uint64_t address) override { return std::numeric_limits<uint32_t>::max(); }
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
