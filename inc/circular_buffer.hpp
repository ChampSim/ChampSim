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

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <cassert>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

#include <type_traits>

namespace champsim
{

template <typename T>
class circular_buffer_iterator
{
protected:
  using cbuf_type = T;
  using self_type = circular_buffer_iterator<T>;

public:
  cbuf_type* buf;
  typename cbuf_type::size_type pos;

public:
  using difference_type = typename cbuf_type::difference_type;
  using value_type = typename cbuf_type::value_type;
  using pointer = value_type*;
  using reference = value_type&;
  using iterator_category = std::random_access_iterator_tag;

  friend class circular_buffer_iterator<typename std::remove_const<T>::type>;
  friend class circular_buffer_iterator<typename std::add_const<T>::type>;

  circular_buffer_iterator() : buf(NULL), pos(0) {}
  circular_buffer_iterator(cbuf_type* buf, typename cbuf_type::size_type pos) : buf(buf), pos(pos) {}

  circular_buffer_iterator(const circular_buffer_iterator<typename std::remove_const<T>::type>& other) : buf(other.buf), pos(other.pos) {}

  reference operator*() { return (*buf)[pos]; }
  pointer operator->() { return &(operator*()); }

  self_type& operator+=(difference_type n)
  {
    pos = cbuf_type::circ_inc(pos, n, *buf);
    return *this;
  }
  self_type operator+(difference_type n)
  {
    self_type r(*this);
    r += n;
    return r;
  }
  self_type& operator-=(difference_type n)
  {
    operator+=(-n);
    return *this;
  }
  self_type operator-(difference_type n)
  {
    self_type r(*this);
    r -= n;
    return r;
  }

  self_type& operator++() { return operator+=(1); }
  self_type operator++(int)
  {
    self_type r(*this);
    operator++();
    return r;
  }
  self_type& operator--() { return operator-=(1); }
  self_type operator--(int)
  {
    self_type r(*this);
    operator--();
    return r;
  }

  difference_type operator-(const self_type& other) const;
  reference operator[](difference_type n) { return *(*this + n); }

  bool operator<(const self_type& other) const { return buf == other.buf && (other - *this) > 0; }
  bool operator>(const self_type& other) const { return other.operator<(*this); }
  bool operator>=(const self_type& other) const { return !operator<(other); }
  bool operator<=(const self_type& other) const { return !operator>(other); }
  bool operator==(const self_type& other) const { return operator<=(other) && operator>=(other); }
  bool operator!=(const self_type& other) const { return !operator==(other); }
};

/***
 * This class implements a deque-like interface with fixed (maximum) size over
 * contiguous memory. Iterators to this structure are never invalidated, unless
 * the element it refers to is popped.
 */
template <typename T>
class circular_buffer
{
protected:
  using buffer_t = std::vector<T>;

public:
  using value_type = typename buffer_t::value_type;
  using size_type = typename buffer_t::size_type;
  using difference_type = typename buffer_t::difference_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = circular_buffer_iterator<circular_buffer<T>>;
  using const_iterator = circular_buffer_iterator<const circular_buffer<T>>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

protected:
  friend iterator;
  friend const_iterator;
  friend reverse_iterator;
  friend const_reverse_iterator;

  const size_type sz_;

  buffer_t entry_ = {};
  size_type head_ = 0;
  size_type tail_ = 0;

  reference operator[](size_type n) { return entry_.at(n); }
  const_reference operator[](size_type n) const { return entry_.at(n); }

  static size_type circ_inc(size_type base, difference_type inc, const circular_buffer<T>& buf);

public:
  explicit circular_buffer(std::size_t N) : sz_(N), entry_(N + 1) {}

  constexpr size_type size() const noexcept { return sz_; }
  size_type occupancy() const noexcept { return std::distance(begin(), end()); };
  bool empty() const noexcept { return occupancy() == 0; }
  bool full() const noexcept { return occupancy() == size(); }
  constexpr size_type max_size() const noexcept { return static_cast<size_type>(std::numeric_limits<difference_type>::max() - 1); }

  reference front() { return operator[](head_); }
  reference back() { return operator[](circ_inc(tail_, -1, *this)); }
  const_reference front() const { return operator[](head_); }
  const_reference back() const { return operator[](circ_inc(tail_, -1, *this)); }

  iterator begin() noexcept { return iterator(this, head_); }
  iterator end() noexcept { return iterator(this, tail_); }
  const_iterator begin() const noexcept { return const_iterator(this, head_); }
  const_iterator end() const noexcept { return const_iterator(this, tail_); }
  const_iterator cbegin() const noexcept { return const_iterator(this, head_); }
  const_iterator cend() const noexcept { return const_iterator(this, tail_); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

  void clear() { head_ = tail_ = 0; }
  void push_back(const T& item)
  {
    assert(!full());
    operator[](tail_) = item;
    tail_ = circ_inc(tail_, 1, *this);
  }
  void push_back(const T&& item)
  {
    assert(!full());
    operator[](tail_) = std::move(item);
    tail_ = circ_inc(tail_, 1, *this);
  }
  void pop_front()
  {
    assert(!empty());
    head_ = circ_inc(head_, 1, *this);
  }
};

template <typename T>
auto circular_buffer<T>::circ_inc(size_type base, difference_type inc, const circular_buffer<T>& buf) -> size_type
{
  difference_type signed_new_base = base + inc;
  const difference_type max_size = buf.entry_.size();

  // Adjust from the negative direction
  while (signed_new_base < 0)
    signed_new_base += max_size;

  // Adjust from the positive direction
  while (signed_new_base >= max_size)
    signed_new_base -= max_size;

  return static_cast<size_type>(signed_new_base);
}

template <typename T>
auto circular_buffer_iterator<T>::operator-(const self_type& other) const -> difference_type
{
  difference_type diff = pos - other.pos;

  // Adjust for the cases where the tail has wrapped, but the head has not.
  // In the positive direction
  if (pos < buf->head_ && buf->head_ <= other.pos)
    diff = buf->entry_.size() + diff;

  // In the negative direction
  else if (other.pos < buf->head_ && buf->head_ <= pos)
    diff = buf->entry_.size() - diff;

  return diff;
}

} // namespace champsim

#endif
