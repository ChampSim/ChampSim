#ifndef EVENT_COUNTER_H
#define EVENT_COUNTER_H

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <type_traits>
#include <vector>

namespace champsim::stats
{
template <typename Key>
class event_counter
{
public:
  using key_type = std::remove_cv_t<Key>;
  using value_type = long;

private:
  std::vector<key_type> keys{};
  std::vector<value_type> values{};

  auto get_iter(key_type key)
  {
    auto key_iter = std::lower_bound(std::begin(keys), std::end(keys), key);
    auto value_iter = std::next(std::begin(values), std::distance(std::begin(keys), key_iter));
    return std::pair{key_iter, value_iter};
  }

  auto get_iter(key_type key) const
  {
    auto key_iter = std::lower_bound(std::begin(keys), std::end(keys), key);
    auto value_iter = std::next(std::begin(values), std::distance(std::begin(keys), key_iter));
    return std::pair{key_iter, value_iter};
  }

public:
  void allocate(key_type key)
  {
    auto [key_iter, value_iter] = get_iter(key);
    if (key_iter == std::end(keys) || *key_iter != key) {
      keys.insert(key_iter, key);
      values.insert(value_iter, value_type{});
    }
  }

  void deallocate(key_type key)
  {
    auto [key_iter, value_iter] = get_iter(key);
    keys.erase(key_iter);
    values.erase(value_iter);
  }

  void increment(key_type key)
  {
    allocate(key);
    auto [key_iter, value_iter] = get_iter(key);
    (*value_iter)++;
  }

  void set(key_type key, value_type val)
  {
    allocate(key);
    auto [key_iter, value_iter] = get_iter(key);
    *value_iter = val;
  }

  auto at(key_type key) const
  {
    auto [key_iter, value_iter] = get_iter(key);
    return *value_iter;
  }

  auto value_or(key_type key, value_type val) const
  {
    auto [key_iter, value_iter] = get_iter(key);
    if (key_iter == std::end(keys) || *key_iter != key) {
      return val;
    }
    return *value_iter;
  }

  auto total() const { return std::accumulate(std::begin(values), std::end(values), value_type{}); }

  std::vector<key_type> get_keys() const { return keys; }

  event_counter<key_type>& operator+=(const event_counter<key_type>& rhs)
  {
    std::transform(std::begin(values), std::end(values), std::cbegin(keys), std::begin(values),
                   [&rhs](auto val, auto key) { return val + rhs.value_or(key, value_type{}); });
    return *this;
  }

  friend auto operator+(event_counter<key_type> lhs, const event_counter<key_type>& rhs)
  {
    lhs += rhs;
    return lhs;
  }

  event_counter<key_type>& operator-=(const event_counter<key_type>& rhs)
  {
    std::transform(std::begin(values), std::end(values), std::cbegin(keys), std::begin(values),
                   [&rhs](auto val, auto key) { return val - rhs.value_or(key, value_type{}); });
    return *this;
  }

  friend auto operator-(event_counter<key_type> lhs, const event_counter<key_type>& rhs)
  {
    lhs -= rhs;
    return lhs;
  }
};
} // namespace champsim::stats

#endif
