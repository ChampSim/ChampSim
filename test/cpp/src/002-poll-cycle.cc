#include <catch.hpp>

#include "operable.h"

namespace
{
// An operable whose poll_cycle() answer is scripted per local cycle.
struct polling_operable : champsim::operable {
  using operable::operable;
  int operate_count = 0;
  int poll_count = 0;
  long skip_answer = 0;

  long poll_cycle() override
  {
    ++poll_count;
    return skip_answer;
  }

  long operate() override
  {
    ++operate_count;
    return 1;
  }
};

// Restores the global skip switch when a test section ends.
struct skip_guard {
  bool saved = champsim::operable::skip_enabled();
  ~skip_guard() { champsim::operable::set_skip_enabled(saved); }
};
} // namespace

TEST_CASE("A poll_cycle returning 0 does not affect operation")
{
  skip_guard guard;
  champsim::operable::set_skip_enabled(true);

  champsim::chrono::clock global_clock{};
  champsim::chrono::clock::duration period{100};
  constexpr int num_cycles = 50;
  polling_operable uut{period};
  uut.skip_answer = 0;

  for (int i = 0; i < num_cycles; ++i) {
    global_clock.tick(period);
    uut.operate_on(global_clock);
  }

  REQUIRE(uut.operate_count == num_cycles);
  REQUIRE(uut.poll_count == num_cycles);
}

TEST_CASE("A poll_cycle returning 1 skips exactly that cycle")
{
  skip_guard guard;
  champsim::operable::set_skip_enabled(true);

  champsim::chrono::clock global_clock{};
  champsim::chrono::clock::duration period{100};
  polling_operable uut{period};
  uut.skip_answer = 1;

  for (int i = 0; i < 10; ++i) {
    global_clock.tick(period);
    uut.operate_on(global_clock);
  }

  REQUIRE(uut.operate_count == 0);
  REQUIRE(uut.poll_count == 10);
  // Time still advances one period per cycle while skipping
  REQUIRE(uut.current_time.time_since_epoch() == 10 * period);

  // Waking up resumes normal operation with no lost time
  uut.skip_answer = 0;
  global_clock.tick(period);
  uut.operate_on(global_clock);
  REQUIRE(uut.operate_count == 1);
  REQUIRE(uut.current_time.time_since_epoch() == 11 * period);
}

TEST_CASE("A poll_cycle returning n sleeps through n local cycles")
{
  skip_guard guard;
  champsim::operable::set_skip_enabled(true);

  champsim::chrono::clock global_clock{};
  champsim::chrono::clock::duration period{100};
  constexpr long sleep_cycles = 7;
  polling_operable uut{period};
  uut.skip_answer = sleep_cycles;

  // First tick: one poll answers "sleep 7"; current_time leaps 7 periods.
  global_clock.tick(period);
  uut.operate_on(global_clock);
  REQUIRE(uut.poll_count == 1);
  REQUIRE(uut.operate_count == 0);
  REQUIRE(uut.current_time.time_since_epoch() == sleep_cycles * period);

  // The operable is not reconsidered until the global clock catches up.
  uut.skip_answer = 0;
  for (int i = 1; i < sleep_cycles; ++i) {
    global_clock.tick(period);
    uut.operate_on(global_clock);
  }
  REQUIRE(uut.poll_count == 1);
  REQUIRE(uut.operate_count == 0);

  // One more tick passes its wake time: it operates again.
  global_clock.tick(period);
  uut.operate_on(global_clock);
  REQUIRE(uut.operate_count == 1);
  REQUIRE(uut.current_time.time_since_epoch() == (sleep_cycles + 1) * period);
}

TEST_CASE("Disabling cycle skip forces operation despite poll_cycle")
{
  skip_guard guard;
  champsim::operable::set_skip_enabled(false);

  champsim::chrono::clock global_clock{};
  champsim::chrono::clock::duration period{100};
  constexpr int num_cycles = 25;
  polling_operable uut{period};
  uut.skip_answer = 1;

  for (int i = 0; i < num_cycles; ++i) {
    global_clock.tick(period);
    uut.operate_on(global_clock);
  }

  REQUIRE(uut.poll_count == 0);
  REQUIRE(uut.operate_count == num_cycles);
}

TEST_CASE("poll_cycle observes the same end-of-cycle timestamp operate would")
{
  skip_guard guard;
  champsim::operable::set_skip_enabled(true);

  struct time_probe : champsim::operable {
    using operable::operable;
    champsim::chrono::clock::time_point seen_at_poll{};
    champsim::chrono::clock::time_point seen_at_operate{};
    bool skip_next = true;

    long poll_cycle() override
    {
      seen_at_poll = current_time;
      return skip_next ? 1 : 0;
    }
    long operate() override
    {
      seen_at_operate = current_time;
      return 1;
    }
  };

  champsim::chrono::clock global_clock{};
  champsim::chrono::clock::duration period{100};
  time_probe uut{period};

  global_clock.tick(period);
  uut.operate_on(global_clock);
  REQUIRE(uut.seen_at_poll.time_since_epoch() == period); // end-of-cycle view

  uut.skip_next = false;
  global_clock.tick(period);
  uut.operate_on(global_clock);
  REQUIRE(uut.seen_at_poll == uut.seen_at_operate);
}
