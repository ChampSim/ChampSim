/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DELAY_QUEUE_H
#define DELAY_QUEUE_H

#include <algorithm>
#include <iostream>
#include <iterator>
#include <utility>

#include "circular_buffer.hpp"
#include <type_traits>

namespace champsim
{

/***
 * A fixed-size queue that releases its members only after a delay.
 *
 * This class forwards most of its functionality on to a
 *champsim::circular_buffer<>, but introduces some boilerplate code to wrap
 *members in a type that counts down the cycles until the member is ready to be
 *released.
 *
 * The `end_ready()` member function (and related functions) are provided to
 *permit iteration over only ready members.
 *
 * No checking is done when members are popped. A recommended way to operate on
 *and to pop multiple members is:
 *
 *     delay_queue<int> dq(10, 1);
 *     std::size_t bandwidth = 4;
 *     auto end = std::min(dq.end_ready(), std::next(dq.begin(), bandwidth));
 *     for (auto it = dq.begin(); it != end; it = dq.begin()) {
 *         perform_task(*it);
 *         dq.pop_front();
 *     }
 *
 * If no bandwidth constraints are needed, the terminal condition can simply be
 *`it != dq.end_ready()`.
 ***/
template <typename T>
class delay_queue
{
private:
  template <typename U>
  using buffer_t = circular_buffer<U>;

public:
  delay_queue(std::size_t size, unsigned latency) : sz(size), _buf(size), _delays(size), _latency(latency) {}

  /***
   * These types provided for compatibility with standard containers.
   ***/
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const reference;
  using pointer = value_type*;
  using const_pointer = const pointer;
  using iterator = typename buffer_t<value_type>::iterator;
  using const_iterator = typename buffer_t<value_type>::const_iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr size_type size() const noexcept { return sz; }
  size_type occupancy() const noexcept { return _buf.occupancy(); };
  // size_type occupancy() const noexcept          { return _buf.size(); };
  bool empty() const noexcept { return occupancy() == 0; }
  bool full() const noexcept { return _buf.full(); }
  bool has_ready() const noexcept { return begin() != end_ready(); }
  constexpr size_type max_size() const noexcept { return _buf.max_size(); }

  /***
   * Note: there is no guarantee that either the front or back element is ready.
   ***/
  reference front() { return _buf.front(); }
  reference back() { return _buf.back(); }
  const_reference front() const { return _buf.front(); }
  const_reference back() const { return _buf.back(); }

  /***
   * Note: there is no guarantee that either begin() or end() points to a ready
   *member. end_ready() will always point to the first non-ready member.
   ***/
  iterator begin() noexcept { return _buf.begin(); }
  iterator end() noexcept { return _buf.end(); }
  iterator end_ready() noexcept { return _end_ready; }
  const_iterator begin() const noexcept { return _buf.begin(); }
  const_iterator end() const noexcept { return _buf.end(); }
  const_iterator end_ready() const noexcept { return _end_ready; }
  const_iterator cbegin() const noexcept { return _buf.cbegin(); }
  const_iterator cend() const noexcept { return _buf.cend(); }
  const_iterator cend_ready() const noexcept { return _end_ready; }

  reverse_iterator rbegin() noexcept { return _buf.rbegin(); }
  reverse_iterator rend() noexcept { return _buf.rend(); }
  reverse_iterator rend_ready() noexcept { return reverse_iterator(end_ready()); }
  const_reverse_iterator rbegin() const noexcept { return _buf.rbegin(); }
  const_reverse_iterator rend() const noexcept { return _buf.rend(); }
  const_reverse_iterator rend_ready() const noexcept { return reverse_iterator(end_ready()); }
  const_reverse_iterator crbegin() const noexcept { return _buf.crbegin(); }
  const_reverse_iterator crend() const noexcept { return _buf.crend(); }
  const_reverse_iterator crend_ready() const noexcept { return reverse_iterator(end_ready()); }

  void clear() { _buf.clear(); }

  /***
   * Push an element into the queue, delayed by the fixed amount.
   ***/
  void push_back(const T& item)
  {
    _buf.push_back(item);
    _delays.push_back(_latency);
  }
  void push_back(const T&& item)
  {
    _buf.push_back(std::forward<T>(item));
    _delays.push_back(_latency);
  }

  /***
   * Pops the element off of the front.
   * Note, no checking is performed. Guard all accesses with calls to
   *has_ready()
   ***/
  void pop_front()
  {
    _buf.pop_front();
    _delays.pop_front();
  }

  /***
   * These functions add an element that is immediately ready. Elements are
   *still popped in order.
   ***/
  void push_back_ready(const T& item)
  {
    _buf.push_back(item);
    _delays.push_back(0);
  }
  void push_back_ready(const T&& item)
  {
    _buf.push_back(std::forward<T>(item));
    _delays.push_back(0);
  }

  /***
   * This function must be called once every cycle.
   ***/
  void operate()
  {
    for (auto& x : _delays)
      --x; // The delay may go negative, this is permitted.

    auto delay_it = std::partition_point(_delays.begin(), _delays.end(), [](long long int x) { return x <= 0; });
    _end_ready = std::next(_buf.begin(), std::distance(_delays.begin(), delay_it));
  }

private:
  const size_type sz;
  buffer_t<value_type> _buf{sz};
  buffer_t<long long int> _delays{sz};
  const long long int _latency;
  iterator _end_ready = _buf.end();
};

} // namespace champsim

#endif
