#ifndef UTIL_EQ_ADDR_H
#define UTIL_EQ_ADDR_H

#include <optional>

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

template <typename T>
struct eq_addr {
  using argument_type = T;
  const decltype(argument_type::address) match_val;
  const std::size_t shamt = 0;

  explicit eq_addr(decltype(argument_type::address) val) : match_val(val) {}
  eq_addr(decltype(argument_type::address) val, std::size_t shift_bits) : match_val(val), shamt(shift_bits) {}

  bool operator()(const argument_type& test)
  {
    is_valid<argument_type> validtest;
    return validtest(test) && (test.address >> shamt) == (match_val >> shamt);
  }
};

#endif
