#include <catch.hpp>
#include <string>
#include <vector>

#include "hook.h"

namespace
{
// Declared the way a real hook is: a registration object, named once, beside its emitter.
champsim::hook<void(int)> counter_hook{"test_counter"};
champsim::hook<void(const std::string&, int)> pair_hook{"test_pair"};
} // namespace

TEST_CASE("A hook with no subscribers ignores its emissions")
{
  REQUIRE(counter_hook.subscriber_count() == 0);
  counter_hook.emit(7); // must not crash, must not do anything
  REQUIRE(counter_hook.subscriber_count() == 0);
}

TEST_CASE("A subscriber receives the emitted payload")
{
  std::vector<int> seen;
  auto sub = counter_hook.subscribe([&](const int& value) { seen.push_back(value); });

  REQUIRE(counter_hook.subscriber_count() == 1);
  counter_hook.emit(1);
  counter_hook.emit(2);

  REQUIRE(seen == std::vector<int>{1, 2});
}

TEST_CASE("Every subscriber is notified, in subscription order")
{
  std::vector<std::string> seen;
  auto first = counter_hook.subscribe([&](const int& value) { seen.push_back("first:" + std::to_string(value)); });
  auto second = counter_hook.subscribe([&](const int& value) { seen.push_back("second:" + std::to_string(value)); });

  REQUIRE(counter_hook.subscriber_count() == 2);
  counter_hook.emit(3);

  REQUIRE(seen == std::vector<std::string>{"first:3", "second:3"});
}

TEST_CASE("A multi-argument hook forwards every argument")
{
  std::string seen_name;
  int seen_value = 0;
  auto sub = pair_hook.subscribe([&](const std::string& name, const int& value) {
    seen_name = name;
    seen_value = value;
  });

  pair_hook.emit("L1D", 42);

  REQUIRE(seen_name == "L1D");
  REQUIRE(seen_value == 42);
}

TEST_CASE("A subscription that goes out of scope stops being called")
{
  int calls = 0;
  {
    auto sub = counter_hook.subscribe([&](const int&) { ++calls; });
    counter_hook.emit(0);
    REQUIRE(calls == 1);
  }

  REQUIRE(counter_hook.subscriber_count() == 0);
  counter_hook.emit(0);
  REQUIRE(calls == 1); // the destroyed handle took the callback with it
}

TEST_CASE("A subscription can be cancelled early, and cancelling twice is harmless")
{
  int calls = 0;
  auto sub = counter_hook.subscribe([&](const int&) { ++calls; });
  REQUIRE(sub.active());

  sub.cancel();
  REQUIRE_FALSE(sub.active());
  counter_hook.emit(0);
  REQUIRE(calls == 0);

  sub.cancel(); // no owner left; must be a no-op
  REQUIRE(counter_hook.subscriber_count() == 0);
}

TEST_CASE("Cancelling one subscription leaves the others attached")
{
  std::vector<std::string> seen;
  auto first = counter_hook.subscribe([&](const int&) { seen.emplace_back("first"); });
  auto second = counter_hook.subscribe([&](const int&) { seen.emplace_back("second"); });

  first.cancel();
  counter_hook.emit(0);

  REQUIRE(seen == std::vector<std::string>{"second"});
  REQUIRE(counter_hook.subscriber_count() == 1);
}

TEST_CASE("A moved-from subscription no longer owns the callback")
{
  int calls = 0;
  auto sub = counter_hook.subscribe([&](const int&) { ++calls; });
  {
    auto moved = std::move(sub);
    REQUIRE(moved.active());
    counter_hook.emit(0);
    REQUIRE(calls == 1);
  } // the moved-to handle cancels here

  REQUIRE(counter_hook.subscriber_count() == 0);
  counter_hook.emit(0);
  REQUIRE(calls == 1);
}

TEST_CASE("A hook knows whether anyone is listening")
{
  REQUIRE_FALSE(counter_hook.listening());
  {
    auto sub = counter_hook.subscribe([](const int&) {});
    REQUIRE(counter_hook.listening());
    {
      auto second = counter_hook.subscribe([](const int&) {});
      REQUIRE(counter_hook.listening());
    }
    REQUIRE(counter_hook.listening()); // one remains
  }
  REQUIRE_FALSE(counter_hook.listening()); // the last one went away
}

TEST_CASE("Declared hooks are registered under their names")
{
  const auto& hooks = champsim::hook_registry::hooks();
  REQUIRE(hooks.count("test_counter") == 1);
  REQUIRE(hooks.count("test_pair") == 1);
  REQUIRE(hooks.at("test_counter")->name() == "test_counter");
}
