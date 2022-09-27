#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <cstdint>
#include <functional>
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
  namespace detail
  {
    template <typename T> struct hash;

    template <typename T1, typename T2>
    struct hash<std::pair<T1, T2>>
    {
      std::size_t operator()(std::pair<T1, T2> val)
      {
        return std::hash<T1>{}(val.first) ^ std::hash<T2>{}(val.second);
      }
    };
  }

  template <typename T, typename Proj, typename Hash=std::hash<std::invoke_result_t<Proj, const T&>>>
  class lru_table
  {
    struct block_t {
      uint64_t last_used = 0;
      T data;
    };

    const std::size_t NUM_SET, NUM_WAY, shamt;
    uint64_t access_count = 0;
    std::vector<block_t> block{NUM_SET * NUM_WAY};
    using iter_type = typename std::vector<block_t>::iterator;

    Proj projection;
    using index_type = std::invoke_result_t<Proj, const T&>;

    static Hash hash;

    auto get_set_span(index_type index)
    {
      auto set_idx = (index >> shamt) & bitmask(lg2(NUM_SET));
      auto set_begin = std::next(std::begin(block), set_idx * NUM_WAY);
      auto set_end = std::next(set_begin, NUM_WAY);

      return std::pair{set_begin, set_end};
    }

    auto match_func(index_type index)
    {
      return [index, shamt = this->shamt, proj = this->projection](const block_t &x) {
        return x.last_used > 0 && (hash(proj(x.data)) >> shamt) == (index >> shamt);
      };
    }

    protected:
    std::optional<T> check_hit(const T &elem)
    {
      auto index = hash(projection(elem));
      auto [set_begin, set_end] = get_set_span(index);
      auto hit = std::find_if(set_begin, set_end, match_func(index));

      if (hit == set_end)
        return std::nullopt;

      hit->last_used = ++access_count;
      return hit->data;
    }

    void fill(const T &elem)
    {
      auto index = hash(projection(elem));
      auto [set_begin, set_end] = get_set_span(index);
      auto hit = std::find_if(set_begin, set_end, match_func(index));

      if (hit == set_end)
        hit = std::min_element(set_begin, set_end, [](auto x, auto y) { return x.last_used < y.last_used; });

      *hit = {++access_count, elem};
    }

    std::optional<T> invalidate(const T &elem)
    {
      auto index = hash(projection(elem));
      auto [set_begin, set_end] = get_set_span(index);
      auto hit = std::find_if(set_begin, set_end, match_func(index));

      if (hit == set_end)
        return std::nullopt;

      auto oldval = std::exchange(*hit, {});
      return oldval.data;
    }

    public:
    using value_type = T;
    lru_table(std::size_t sets, std::size_t ways, std::size_t shamt, Proj &&proj) : NUM_SET(sets), NUM_WAY(ways), shamt(shamt), projection(proj) {}
    lru_table(std::size_t sets, std::size_t ways, std::size_t shamt) : NUM_SET(sets), NUM_WAY(ways), shamt(shamt) {}
  };

  template <typename T, typename I=T>
  class simple_lru_table : lru_table<std::pair<I,T>, std::function<I(const std::pair<I,T>&)>>
  {
    using super_type = lru_table<std::pair<I,T>, std::function<I(const std::pair<I,T>&)>>;

    public:
    simple_lru_table(std::size_t sets, std::size_t ways, std::size_t shamt) : super_type(sets, ways, shamt, [](auto x){ return x.first; }) {}

    std::optional<T> check_hit(I index)
    {
      auto hit = super_type::check_hit({index, 0});
      if (!hit.has_value())
        return std::nullopt;

      return hit->second;
    }

    void fill_cache(I index, T data)
    {
      super_type::fill({index, data});
    }

    std::optional<T> invalidate(I index)
    {
      auto inv = super_type::invalidate({index, 0});
      if (!inv.has_value())
        return std::nullopt;

      return inv->second;
    }
  };
} // namespace champsim

#endif
