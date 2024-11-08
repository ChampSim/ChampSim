#ifndef TEST_MATCHERS_H
#define TEST_MATCHERS_H

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include "address.h"

namespace champsim::test {
template <typename Addr>
struct StrideMatcher : Catch::Matchers::MatcherGenericBase {
  typename Addr::difference_type stride;

  explicit StrideMatcher(typename Addr::difference_type s) : stride(s) {}

    template<typename Range>
    bool match(const Range& range) const {
      std::vector<decltype(stride)> diffs;
      return std::adjacent_find(std::cbegin(range), std::cend(range), [stride=stride](const auto& x, const auto& y){ return champsim::offset(Addr{x}, Addr{y}) != stride; }) == std::cend(range);
    }

    std::string describe() const override {
        return "has stride " + std::to_string(stride);
    }
};


struct RelativeReturnedMatcher : Catch::Matchers::MatcherGenericBase {
  long issue_time;
  long epsilon;

  RelativeReturnedMatcher(long issue_time_, long epsilon_) : issue_time(issue_time_), epsilon(epsilon_) {}

  template <typename T>
  RelativeReturnedMatcher(const T& other, long cycles, long epsilon_) : issue_time(other.issue_time + cycles), epsilon(epsilon_) {}

  template <typename T>
  bool match(const T& to_match) const {
    bool not_early = (to_match.return_time >= issue_time - epsilon);
    bool not_late = (to_match.return_time <= issue_time + epsilon);
    return not_early && not_late;
  }

  std::string describe() const override {
    if (epsilon > 0) {
      return "Returned between cycle " + std::to_string(issue_time-epsilon) + " and " + std::to_string(issue_time+epsilon);
    }
    return "Returned at cycle " + std::to_string(issue_time);
  }
};

struct ReturnedMatcher : Catch::Matchers::MatcherGenericBase {
  long cycles;
  long epsilon;

  ReturnedMatcher(long cycles_, long epsilon_) : cycles(cycles_), epsilon(epsilon_) {}

  template <typename T>
  bool match(const T& to_match) const {
    bool not_early = (to_match.return_time >= to_match.issue_time + cycles - epsilon);
    bool not_late = (to_match.return_time <= to_match.issue_time + cycles + epsilon);
    return not_early && not_late;
  }

  std::string describe() const override {
    if (epsilon > 0) {
      return "Returned after between cycles " + std::to_string(cycles-epsilon) + " and " + std::to_string(cycles+epsilon);
    }
    return "Returned after cycles " + std::to_string(cycles);
  }
};

template <typename Range>
struct DisjunctMatcher : Catch::Matchers::MatcherGenericBase
{
  DisjunctMatcher(Range const& range_) : range{ range_ } {}

  template<typename OtherRange>
    bool match(OtherRange const& other) const {
      return std::none_of(std::begin(other), std::end(other), [&](const auto other_elem){
          return std::find(std::begin(range), std::end(range), other_elem) != std::end(range);
      });
    }

  std::string describe() const override {
    return "Contains none of: " + Catch::rangeToString(range);
  }

  private:
  Range const& range;
};

struct MonotonicallyIncreasingMatcher : Catch::Matchers::MatcherGenericBase {
    template<typename Range>
    bool match(Range const& range) const {
        return std::adjacent_find(std::begin(range), std::end(range), std::greater_equal<typename Range::value_type>{}) == std::end(range);
    }

    std::string describe() const override {
        return "Increases monotonically";
    }
};
}

#endif
