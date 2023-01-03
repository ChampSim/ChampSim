#include <deque>
#include <exception>
#include <functional>
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
  uint64_t ret_data = 0x11111111;
  const uint64_t latency = 0;

  public:
    champsim::channel queues{};
    std::deque<uint64_t> addresses{};
    do_nothing_MRC(uint64_t lat) : MemoryRequestConsumer(), champsim::operable(1), latency(lat) {
      queues.sim_stats.emplace_back();
    }
    do_nothing_MRC() : do_nothing_MRC(0) {}

    void operate() override {
      auto add_pkt = [&](PACKET pkt) {
        pkt.event_cycle = current_cycle + latency;
        pkt.data = ++ret_data;
        addresses.push_back(pkt.address);
        packets.push_back(pkt);
      };

      std::for_each(std::begin(queues.RQ), std::end(queues.RQ), add_pkt);
      std::for_each(std::begin(queues.WQ), std::end(queues.WQ), add_pkt);
      std::for_each(std::begin(queues.PQ), std::end(queues.PQ), add_pkt);

      queues.RQ.clear();
      queues.WQ.clear();
      queues.PQ.clear();

      auto end = std::find_if_not(std::begin(packets), std::end(packets), [cycle=current_cycle](const PACKET &x){ return x.event_cycle <= cycle; });
      std::move(std::begin(packets), end, std::back_inserter(ready_packets));
      packets.erase(std::begin(packets), end);

      for (PACKET &pkt : ready_packets) {
        for (auto ret : pkt.to_return)
          ret->push_back(pkt);
      }
      ready_packets.clear();
    }

    std::size_t get_occupancy(uint8_t, uint64_t) override { return std::size(packets); }
    std::size_t get_size(uint8_t, uint64_t) override { return std::numeric_limits<uint32_t>::max(); }

    std::size_t packet_count() const { return std::size(addresses); }
};

/*
 * A MemoryRequestConsumer that returns only a particular address
 */
class filter_MRC : public MemoryRequestConsumer, public champsim::operable
{
  std::deque<PACKET> packets, ready_packets;
  const uint64_t ret_addr;
  const uint64_t latency = 0;
  std::size_t mpacket_count = 0;

  public:
    champsim::channel queues{};
    filter_MRC(uint64_t ret_addr_, uint64_t lat) : MemoryRequestConsumer(), champsim::operable(1), ret_addr(ret_addr_), latency(lat) {
      queues.sim_stats.emplace_back();
    }
    filter_MRC(uint64_t ret_addr_) : filter_MRC(ret_addr_, 0) {}

    void operate() override {
      auto add_pkt = [&](PACKET pkt) {
        if (pkt.address == ret_addr) {
          pkt.event_cycle = current_cycle + latency;
          packets.push_back(pkt);
          ++mpacket_count;
        }
      };

      std::for_each(std::begin(queues.RQ), std::end(queues.RQ), add_pkt);
      std::for_each(std::begin(queues.WQ), std::end(queues.WQ), add_pkt);
      std::for_each(std::begin(queues.PQ), std::end(queues.PQ), add_pkt);

      queues.RQ.clear();
      queues.WQ.clear();
      queues.PQ.clear();

      auto end = std::find_if_not(std::begin(packets), std::end(packets), [cycle=current_cycle](const PACKET &x){ return x.event_cycle <= cycle; });
      std::move(std::begin(packets), end, std::back_inserter(ready_packets));

      for (PACKET &pkt : ready_packets) {
        for (auto ret : pkt.to_return)
          ret->push_back(pkt);
      }
      ready_packets.clear();
    }

    std::size_t get_occupancy(uint8_t, uint64_t) override { return std::size(packets); }
    std::size_t get_size(uint8_t, uint64_t) override { return std::numeric_limits<uint32_t>::max(); }

    std::size_t packet_count() const { return mpacket_count; }
};

/*
 * A MemoryRequestConsumer that releases blocks when instructed to
 */
class release_MRC : public MemoryRequestConsumer, public champsim::operable
{
  std::deque<PACKET> packets;
  std::size_t mpacket_count = 0;

  public:
    champsim::channel queues{};
    release_MRC() : MemoryRequestConsumer(), champsim::operable(1) {
      queues.sim_stats.emplace_back();
    }

    void operate() override {
      auto add_pkt = [&](PACKET pkt) {
        packets.push_back(pkt);
        ++mpacket_count;
      };

      std::for_each(std::begin(queues.RQ), std::end(queues.RQ), add_pkt);
      std::for_each(std::begin(queues.WQ), std::end(queues.WQ), add_pkt);
      std::for_each(std::begin(queues.PQ), std::end(queues.PQ), add_pkt);

      queues.RQ.clear();
      queues.WQ.clear();
      queues.PQ.clear();
    }

    std::size_t get_occupancy(uint8_t, uint64_t) override { return std::size(packets); }
    std::size_t get_size(uint8_t, uint64_t) override { return std::numeric_limits<uint32_t>::max(); }

    std::size_t packet_count() const { return mpacket_count; }

    void release(uint64_t addr)
    {
        auto pkt_it = std::find_if(std::begin(packets), std::end(packets), [addr](auto x){ return x.address == addr; });
        if (pkt_it != std::end(packets)) {
            for (auto ret : pkt_it->to_return) {
                ret->push_back(*pkt_it);
            }
        }
        packets.erase(pkt_it);
    }
};

/*
 * A MemoryRequestProducer that counts how many returns it receives
 */
struct counting_MRP : public MemoryRequestProducer
{
  std::deque<PACKET> returned{};

  unsigned count = 0;

  void operate() {
    count += std::size(returned);
    returned.clear();
  }
};

struct queue_issue_MRP : public MemoryRequestProducer, public champsim::operable
{
  champsim::channel queues{};
  std::deque<PACKET> returned{};

  struct result_data {
    PACKET pkt;
    uint64_t issue_time;
    uint64_t return_time;
  };
  std::deque<result_data> packets;

  std::function<bool(PACKET, PACKET)> top_finder;

  queue_issue_MRP() : queue_issue_MRP([](PACKET x, PACKET y){ return x.address == y.address; }) {}
  explicit queue_issue_MRP(std::function<bool(PACKET, PACKET)> finder) : champsim::operable(1), top_finder(finder) {
    queues.sim_stats.emplace_back();
  }

  void operate() override {
    auto finder = [&](PACKET to_find, result_data candidate) { return top_finder(candidate.pkt, to_find); };

    for (auto pkt : returned) {
      auto it = std::find_if(std::rbegin(packets), std::rend(packets), std::bind(finder, pkt, std::placeholders::_1));
      if (it == std::rend(packets))
        throw std::invalid_argument{"Packet returned which was not sent"};
      it->return_time = current_cycle;
    }
    returned.clear();
  }

  protected:
  PACKET mark_packet(PACKET pkt)
  {
    pkt.to_return = {&returned};
    packets.push_back({pkt, current_cycle, 0});
    return pkt;
  }
};

/*
 * A MemoryRequestProducer that sends its packets to the write queue and notes when packets are returned
 */
struct to_wq_MRP : public queue_issue_MRP
{
  using queue_issue_MRP::queue_issue_MRP;
  bool issue(const PACKET &pkt) { return queues.add_wq(mark_packet(pkt)); }
};

/*
 * A MemoryRequestProducer that sends its packets to the read queue and notes when packets are returned
 */
struct to_rq_MRP : public queue_issue_MRP
{
  using queue_issue_MRP::queue_issue_MRP;
  bool issue(const PACKET &pkt) { return queues.add_rq(mark_packet(pkt)); }
};

/*
 * A MemoryRequestProducer that sends its packets to the read queue and notes when packets are returned
 */
struct to_pq_MRP : public queue_issue_MRP
{
  using queue_issue_MRP::queue_issue_MRP;
  bool issue(const PACKET &pkt) { return queues.add_pq(mark_packet(pkt)); }
};

