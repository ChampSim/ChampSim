#include <deque>
#include <exception>
#include <functional>
#include <limits>

#include "cache.h"
#include "operable.h"

/*
 * A MemoryRequestConsumer that simply returns all packets on the next cycle
 */
class do_nothing_MRC : public champsim::operable
{
  struct packet : champsim::channel::request_type
  {
    uint64_t event_cycle = std::numeric_limits<uint64_t>::max();
  };
  std::deque<packet> packets, ready_packets;
  uint64_t ret_data = 0x11111111;
  const uint64_t latency = 0;

  public:
    champsim::channel queues{};
    std::deque<uint64_t> addresses{};
    do_nothing_MRC(uint64_t lat) : champsim::operable(1), latency(lat) {}
    do_nothing_MRC() : do_nothing_MRC(0) {}

    void operate() override {
      auto add_pkt = [&](auto pkt) {
        packet to_insert{pkt};
        to_insert.event_cycle = current_cycle + latency;
        to_insert.data = ++ret_data;
        addresses.push_back(to_insert.address);
        packets.push_back(to_insert);
      };

      std::for_each(std::begin(queues.RQ), std::end(queues.RQ), add_pkt);
      std::for_each(std::begin(queues.WQ), std::end(queues.WQ), add_pkt);
      std::for_each(std::begin(queues.PQ), std::end(queues.PQ), add_pkt);

      queues.RQ.clear();
      queues.WQ.clear();
      queues.PQ.clear();

      auto end = std::find_if_not(std::begin(packets), std::end(packets), [cycle=current_cycle](const auto &x){ return x.event_cycle <= cycle; });
      std::move(std::begin(packets), end, std::back_inserter(ready_packets));
      packets.erase(std::begin(packets), end);

      for (auto &pkt : ready_packets) {
        if (pkt.response_requested)
          queues.returned.push_back(champsim::channel::response_type{pkt});
      }
      ready_packets.clear();
    }

    std::size_t packet_count() const { return std::size(addresses); }
};

/*
 * A MemoryRequestConsumer that returns only a particular address
 */
class filter_MRC : public champsim::operable
{
  struct packet : champsim::channel::request_type
  {
    uint64_t event_cycle = std::numeric_limits<uint64_t>::max();
  };
  std::deque<packet> packets, ready_packets;
  const uint64_t ret_addr;
  const uint64_t latency = 0;
  std::size_t mpacket_count = 0;

  public:
    champsim::channel queues{};
    filter_MRC(uint64_t ret_addr_, uint64_t lat) : champsim::operable(1), ret_addr(ret_addr_), latency(lat) {}
    filter_MRC(uint64_t ret_addr_) : filter_MRC(ret_addr_, 0) {}

    void operate() override {
      auto add_pkt = [&](auto pkt) {
        if (pkt.address == ret_addr) {
          packet to_insert{pkt};
          to_insert.event_cycle = current_cycle + latency;
          packets.push_back(to_insert);
          ++mpacket_count;
        }
      };

      std::for_each(std::begin(queues.RQ), std::end(queues.RQ), add_pkt);
      std::for_each(std::begin(queues.WQ), std::end(queues.WQ), add_pkt);
      std::for_each(std::begin(queues.PQ), std::end(queues.PQ), add_pkt);

      queues.RQ.clear();
      queues.WQ.clear();
      queues.PQ.clear();

      auto end = std::find_if_not(std::begin(packets), std::end(packets), [cycle=current_cycle](const auto &x){ return x.event_cycle <= cycle; });
      std::move(std::begin(packets), end, std::back_inserter(ready_packets));

      for (auto &pkt : ready_packets) {
        if (pkt.response_requested)
          queues.returned.push_back(champsim::channel::response_type{pkt});
      }
      ready_packets.clear();
    }

    std::size_t packet_count() const { return mpacket_count; }
};

/*
 * A MemoryRequestConsumer that releases blocks when instructed to
 */
class release_MRC : public champsim::operable
{
  std::deque<champsim::channel::request_type> packets;
  std::size_t mpacket_count = 0;

  public:
    champsim::channel queues{};
    release_MRC() : champsim::operable(1) {}

    void operate() override {
      auto add_pkt = [&](auto pkt) {
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

    std::size_t packet_count() const { return mpacket_count; }

    void release_all()
    {
      for (const auto &pkt : packets) {
        if (pkt.response_requested)
          queues.returned.push_back(champsim::channel::response_type{pkt});
      }
      packets.clear();
    }

    void release(uint64_t addr)
    {
        auto pkt_it = std::partition(std::begin(packets), std::end(packets), [addr](auto x){ return x.address != addr; });
        std::for_each(pkt_it, std::end(packets), [&](const auto& pkt) {
          if (pkt.response_requested)
            queues.returned.push_back(champsim::channel::response_type{pkt});
        });
        packets.erase(pkt_it, std::end(packets));
    }
};

/*
 * A MemoryRequestProducer that counts how many returns it receives
 */
struct counting_MRP
{
  std::deque<champsim::channel::response_type> returned{};

  std::size_t count = 0;

  void operate() {
    count += std::size(returned);
    returned.clear();
  }
};

struct queue_issue_MRP : public champsim::operable
{
  using request_type = typename champsim::channel::request_type;
  using response_type = typename champsim::channel::response_type;

  std::deque<response_type> returned{};
  champsim::channel queues{};

  struct result_data {
    request_type pkt;
    uint64_t issue_time;
    uint64_t return_time;
  };
  std::deque<result_data> packets;

  using func_type = std::function<bool(request_type, response_type)>;
  func_type top_finder;

  queue_issue_MRP() : queue_issue_MRP([](auto x, auto y){ return x.address == y.address; }) {}
  explicit queue_issue_MRP(func_type finder) : champsim::operable(1), top_finder(finder) {}

  void operate() override {
    auto finder = [&](response_type to_find, result_data candidate) { return top_finder(candidate.pkt, to_find); };

    for (auto pkt : queues.returned) {
      auto it = std::partition(std::begin(packets), std::end(packets), std::not_fn(std::bind(finder, pkt, std::placeholders::_1)));
      if (it == std::end(packets))
        throw std::invalid_argument{"Packet returned which was not sent"};
      std::for_each(it, std::end(packets), [cycle=current_cycle](auto& x){ return x.return_time = cycle; });
    }
    queues.returned.clear();
  }
};

/*
 * A MemoryRequestProducer that sends its packets to the write queue and notes when packets are returned
 */
struct to_wq_MRP : public queue_issue_MRP
{
  using queue_issue_MRP::queue_issue_MRP;
  using request_type = typename queue_issue_MRP::request_type;
  bool issue(const queue_issue_MRP::request_type &pkt) {
    packets.push_back({pkt, current_cycle, 0});
    return queues.add_wq(pkt);
  }
};

/*
 * A MemoryRequestProducer that sends its packets to the read queue and notes when packets are returned
 */
struct to_rq_MRP : public queue_issue_MRP
{
  using queue_issue_MRP::queue_issue_MRP;
  using request_type = typename queue_issue_MRP::request_type;
  bool issue(const queue_issue_MRP::request_type &pkt) {
    packets.push_back({pkt, current_cycle, 0});
    return queues.add_rq(pkt);
  }
};

/*
 * A MemoryRequestProducer that sends its packets to the read queue and notes when packets are returned
 */
struct to_pq_MRP : public queue_issue_MRP
{
  using queue_issue_MRP::queue_issue_MRP;
  using request_type = typename queue_issue_MRP::request_type;
  bool issue(const queue_issue_MRP::request_type &pkt) {
    packets.push_back({pkt, current_cycle, 0});
    return queues.add_pq(pkt);
  }
};

