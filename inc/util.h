#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "msl/bits.h"
#include "msl/lru_table.h"

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

    explicit eq_addr(addr_type addr, std::size_t shift_bits = 0) : match_addr(addr), shamt(shift_bits) {}
    explicit eq_addr(const argument_type &elem, std::size_t shift_bits = 0) : eq_addr(elem.address, shift_bits) {}

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

    eq_addr(asid_type asid, addr_type addr, std::size_t shift_bits = 0) : match_asid(asid), match_addr(addr), shamt(shift_bits) {}
    explicit eq_addr(const argument_type &elem, std::size_t shift_bits = 0) : eq_addr(elem.asid, elem.address, shift_bits) {}

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
using msl::bitmask;
using msl::lg2;
using msl::lru_table;
using msl::splice_bits;
} // namespace champsim

#endif
