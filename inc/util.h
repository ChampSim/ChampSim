#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

constexpr unsigned lg2(uint64_t n) { return n < 2 ? 0 : 1 + lg2(n / 2); }

constexpr uint64_t bitmask(std::size_t begin, std::size_t end = 0) { return ((1ull << (begin - end)) - 1) << end; }

constexpr uint64_t splice_bits(uint64_t upper, uint64_t lower, std::size_t bits) { return (upper & ~bitmask(bits)) | (lower & bitmask(bits)); }

template <typename T>
struct is_valid {
  using argument_type = T;
  is_valid() {}
  bool operator()(const argument_type& test) { return test.valid; }
};

template <typename T>
struct is_valid<std::optional<T>> {
  bool operator()(const std::optional<T>& test) { return test.has_value(); }
};

template <typename T, typename = void>
struct eq_addr
{
    using argument_type = T;
    using addr_type = decltype(T::address);
    const addr_type match_addr;
    const std::size_t shamt;

    explicit eq_addr(addr_type addr, std::size_t shamt = 0) : match_addr(addr), shamt(shamt) {}
    explicit eq_addr(const argument_type &elem, std::size_t shamt = 0) : eq_addr(elem.address, shamt) {}

    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && (test.address >> shamt) == (match_addr >> shamt);
    }
};

// Specialization for types that include a member T::asid
template <typename T>
struct eq_addr<T, std::void_t<decltype(T::asid)>>
{
    using argument_type = T;
    using addr_type = decltype(T::address);
    using asid_type = decltype(T::asid);

    const asid_type match_asid;
    const addr_type match_addr;
    const std::size_t shamt;

    eq_addr(asid_type asid, addr_type addr, std::size_t shamt = 0) : match_asid(asid), match_addr(addr), shamt(shamt) {}
    explicit eq_addr(const argument_type &elem, std::size_t shamt = 0) : eq_addr(elem.asid, elem.address, shamt) {}

    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && test.asid == match_asid && (test.address >> shamt) == (match_addr >> shamt);
    }
};

template <typename T, typename BIN, typename U = T, typename UN_T = is_valid<T>, typename UN_U = is_valid<U>>
struct invalid_is_minimal {
  bool operator()(const T& lhs, const U& rhs)
  {
    UN_T lhs_unary;
    UN_U rhs_unary;
    BIN cmp;

    return !lhs_unary(lhs) || (rhs_unary(rhs) && cmp(lhs, rhs));
  }
};

template <typename T, typename BIN, typename U = T, typename UN_T = is_valid<T>, typename UN_U = is_valid<U>>
struct invalid_is_maximal {
  bool operator()(const T& lhs, const U& rhs)
  {
    UN_T lhs_unary;
    UN_U rhs_unary;
    BIN cmp;

    return !rhs_unary(rhs) || (lhs_unary(lhs) && cmp(lhs, rhs));
  }
};

template <typename T, typename U = T>
struct cmp_event_cycle {
  bool operator()(const T& lhs, const U& rhs) { return lhs.event_cycle < rhs.event_cycle; }
};

template <typename T>
struct min_event_cycle : invalid_is_maximal<T, cmp_event_cycle<T>> {
};

template <typename T, typename U = T>
struct ord_event_cycle {
  using first_argument_type = T;
  using second_argument_type = U;
  bool operator()(const first_argument_type& lhs, const second_argument_type& rhs)
  {
    is_valid<first_argument_type> first_validtest;
    is_valid<second_argument_type> second_validtest;
    return !second_validtest(rhs) || (first_validtest(lhs) && lhs.event_cycle < rhs.event_cycle);
  }
};

namespace champsim
{
  template <typename T, typename SetProj, typename TagProj>
  class lru_table
  {
    public:
    using value_type = T;

    private:
    struct block_t {
      uint64_t last_used = 0;
      value_type data;
    };

    SetProj set_projection;
    TagProj tag_projection;

    const std::size_t NUM_SET, NUM_WAY;
    uint64_t access_count = 0;
    std::vector<block_t> block{NUM_SET*NUM_WAY};

    auto get_set_span(const value_type &elem)
    {
      auto set_idx = set_projection(elem) & bitmask(lg2(NUM_SET));
      auto set_begin = std::next(std::begin(block), set_idx*NUM_WAY);
      auto set_end = std::next(set_begin, NUM_WAY);
      return std::pair{set_begin, set_end};
    }

    auto match_func(const value_type &elem)
    {
      return [tag = tag_projection(elem), proj = this->tag_projection](const block_t &x) {
        return x.last_used > 0 && proj(x.data) == tag;
      };
    }

    public:
    std::optional<value_type> check_hit(const value_type &elem)
    {
      auto [set_begin, set_end] = get_set_span(elem);
      auto hit = std::find_if(set_begin, set_end, match_func(elem));

      if (hit == set_end)
        return std::nullopt;

      hit->last_used = ++access_count;
      return hit->data;
    }

    void fill(const value_type &elem)
    {
      auto [set_begin, set_end] = get_set_span(elem);
      auto hit = std::find_if(set_begin, set_end, match_func(elem));

      if (hit == set_end)
        hit = std::min_element(set_begin, set_end, [](auto x, auto y) { return x.last_used < y.last_used; });

      *hit = {++access_count, elem};
    }

    std::optional<value_type> invalidate(const value_type &elem)
    {
      auto [set_begin, set_end] = get_set_span(elem);
      auto hit = std::find_if(set_begin, set_end, match_func(elem));

      if (hit == set_end)
        return std::nullopt;

      auto oldval = std::exchange(*hit, {});
      return oldval.data;
    }

    lru_table(std::size_t sets, std::size_t ways, SetProj set_proj, TagProj tag_proj) : set_projection(set_proj), tag_projection(tag_proj), NUM_SET(sets), NUM_WAY(ways)
    {
      assert(sets == (1ull << lg2(sets)));
    }

    lru_table(std::size_t sets, std::size_t ways, SetProj set_proj) : lru_table(sets, ways, set_proj, {}) {}
    lru_table(std::size_t sets, std::size_t ways) : lru_table(sets, ways, {}, {}) {}
  };
} // namespace champsim

#endif
