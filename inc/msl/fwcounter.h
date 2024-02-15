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

#ifndef FWCOUNTER_HPP
#define FWCOUNTER_HPP

#include <algorithm>
#include <cstdlib>

namespace champsim::msl
{
/**
 * A fixed-width saturating counter.
 * All arithmetic operations on this value are clamped between the maximum and minimum values.
 *
 * \tparam val_type The underlying representation of the value.
 * \tparam MAXVAL The maximum to saturate at.
 * \tparam MINVAL The minimum to saturate at.
 */
template <typename val_type, val_type MAXVAL, val_type MINVAL>
class base_fwcounter
{
public:
  using value_type = val_type;

protected:
  val_type _value{};

  static val_type clamp(val_type val) { return std::clamp(val, MINVAL, MAXVAL); }

public:
  constexpr static val_type minimum = MINVAL;
  constexpr static val_type maximum = MAXVAL;

  base_fwcounter() {}
  explicit base_fwcounter(val_type value) : _value(std::move(value)) {}

  template <typename Numeric>
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator=(Numeric);

  /**
   * Increment the value, saturating at the maximum value.
   */
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator++();
  base_fwcounter<val_type, MAXVAL, MINVAL> operator++(int);

  /**
   * Decrement the value, saturating at the minimum value.
   */
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator--();
  base_fwcounter<val_type, MAXVAL, MINVAL> operator--(int);

  /**
   * Add the other operand to the wrapped value, saturating at the minimum and maximum.
   */
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator+=(base_fwcounter<val_type, MAXVAL, MINVAL>);
  template <typename Numeric>
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator+=(Numeric);

  /**
   * Subtract the other operand from the wrapped value, saturating at the minimum and maximum.
   */
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator-=(base_fwcounter<val_type, MAXVAL, MINVAL>);
  template <typename Numeric>
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator-=(Numeric);

  /**
   * Multiply the wrapped value by the other operand, saturating at the minimum and maximum.
   */
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator*=(base_fwcounter<val_type, MAXVAL, MINVAL>);
  template <typename Numeric>
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator*=(Numeric);

  /**
   * Divide the wrapped value by the other operand, saturating at the minimum and maximum.
   */
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator/=(base_fwcounter<val_type, MAXVAL, MINVAL>);
  template <typename Numeric>
  base_fwcounter<val_type, MAXVAL, MINVAL>& operator/=(Numeric);

  /**
   * Detect whether the counter is saturated at its maximum.
   */
  bool is_max() const { return _value == maximum; }

  /**
   * Detect whether the counter is saturated at its minimum.
   */
  bool is_min() const { return _value == minimum; }

  // operator val_type const ();

  /**
   * Unpack the wrapped value.
   */
  val_type value() const { return _value; }
};

/*
 * Unsigned template specialization.
 *
 * \tparam WIDTH the bit-width of the value
 */
template <std::size_t WIDTH>
using fwcounter = base_fwcounter<signed long long int, (1 << WIDTH) - 1, 0>;

/*
 * Signed template specialization.
 *
 * \tparam WIDTH the bit-width of the value
 */
template <std::size_t WIDTH>
using sfwcounter = base_fwcounter<signed long long int, (1 << (WIDTH - 1)) - 1, -(1 << (WIDTH - 1))>;

/*
 * Fundamental operations
 */
template <typename val_type, val_type MAXVAL, val_type MINVAL>
template <typename Numeric>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator=(Numeric rhs)
{
  _value = clamp(rhs);
  return *this;
}

template <typename val_type, val_type MAXVAL, val_type MINVAL>
template <typename Numeric>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator+=(Numeric rhs)
{
  _value = clamp(_value + rhs);
  return *this;
}

template <typename val_type, val_type MAXVAL, val_type MINVAL>
template <typename Numeric>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator-=(Numeric rhs)
{
  _value = clamp(_value - rhs);
  return *this;
}

template <typename val_type, val_type MAXVAL, val_type MINVAL>
template <typename Numeric>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator*=(Numeric rhs)
{
  _value = clamp(_value * rhs);
  return *this;
}

template <typename val_type, val_type MAXVAL, val_type MINVAL>
template <typename Numeric>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator/=(Numeric rhs)
{
  _value = clamp(_value / rhs);
  return *this;
}

/*
 * Prefix unary operators forward to the binary assignment operator with value one
 */

template <typename val_type, val_type MAXVAL, val_type MINVAL>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator++()
{
  return (*this += 1);
}

template <typename val_type, val_type MAXVAL, val_type MINVAL>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator--()
{
  return (*this += 1);
}

/*
 * Postfix unary operators forwward to the prefix unary operator of the same type
 */
template <typename val_type, val_type MAXVAL, val_type MINVAL>
base_fwcounter<val_type, MAXVAL, MINVAL> base_fwcounter<val_type, MAXVAL, MINVAL>::operator++(int)
{
  base_fwcounter<val_type, MAXVAL, MINVAL> result(*this);
  operator++();
  return result;
}

template <typename val_type, val_type MAXVAL, val_type MINVAL>
base_fwcounter<val_type, MAXVAL, MINVAL> base_fwcounter<val_type, MAXVAL, MINVAL>::operator--(int)
{
  base_fwcounter<val_type, MAXVAL, MINVAL> result(*this);
  operator--();
  return result;
}

/*
 * Binary arithmetic operators forward to the assignment operators of the same type
 */
template <typename vt, vt mxvl, vt mnvl, typename Numeric>
base_fwcounter<vt, mxvl, mnvl> operator+(base_fwcounter<vt, mxvl, mnvl> lhs, Numeric rhs)
{
  lhs += rhs;
  return lhs;
}

template <typename vt, vt mxvl, vt mnvl, typename Numeric>
base_fwcounter<vt, mxvl, mnvl> operator-(base_fwcounter<vt, mxvl, mnvl> lhs, Numeric rhs)
{
  lhs -= rhs;
  return lhs;
}

template <typename vt, vt mxvl, vt mnvl, typename Numeric>
base_fwcounter<vt, mxvl, mnvl> operator*(base_fwcounter<vt, mxvl, mnvl> lhs, Numeric rhs)
{
  lhs *= rhs;
  return lhs;
}

template <typename vt, vt mxvl, vt mnvl, typename Numeric>
base_fwcounter<vt, mxvl, mnvl> operator/(base_fwcounter<vt, mxvl, mnvl> lhs, Numeric rhs)
{
  lhs /= rhs;
  return lhs;
}

/*
 * Base comparators
 */
template <typename vt, vt mxvl, vt mnvl, typename Numeric>
bool operator<(const base_fwcounter<vt, mxvl, mnvl>& lhs, Numeric rhs)
{
  return lhs.value() < rhs;
}

template <typename vt, vt mxvl, vt mnvl, typename Numeric>
bool operator==(const base_fwcounter<vt, mxvl, mnvl>& lhs, Numeric rhs)
{
  return lhs.value() == rhs;
}

/*
 * Other comparators forward to the bases
 */
template <typename vt, vt mxvl, vt mnvl, typename Numeric>
bool operator>(const base_fwcounter<vt, mxvl, mnvl>& lhs, Numeric rhs)
{
  return !(lhs == rhs || lhs < rhs);
}

template <typename vt, vt mxvl, vt mnvl, typename Numeric>
bool operator>=(const base_fwcounter<vt, mxvl, mnvl>& lhs, Numeric rhs)
{
  return !(lhs < rhs);
}

template <typename vt, vt mxvl, vt mnvl, typename Numeric>
bool operator<=(const base_fwcounter<vt, mxvl, mnvl>& lhs, Numeric rhs)
{
  return !(lhs > rhs);
}

template <typename vt, vt mxvl, vt mnvl, typename Numeric>
bool operator!=(const base_fwcounter<vt, mxvl, mnvl>& lhs, Numeric rhs)
{
  return !(lhs == rhs);
}

/*
 * Arithmentic operators for two fwcounters forward to the value operators
 */

template <typename val_type, val_type MAXVAL, val_type MINVAL>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator+=(base_fwcounter<val_type, MAXVAL, MINVAL> rhs)
{
  return (*this += rhs._value);
}

template <typename val_type, val_type MAXVAL, val_type MINVAL>
base_fwcounter<val_type, MAXVAL, MINVAL>& base_fwcounter<val_type, MAXVAL, MINVAL>::operator-=(base_fwcounter<val_type, MAXVAL, MINVAL> rhs)
{
  return (*this -= rhs._value);
}

template <typename vt, vt mxvl, vt mnvl>
base_fwcounter<vt, mxvl, mnvl> operator+(base_fwcounter<vt, mxvl, mnvl> lhs, const base_fwcounter<vt, mxvl, mnvl>& rhs)
{
  return lhs + rhs.value();
}

template <typename vt, vt mxvl, vt mnvl>
base_fwcounter<vt, mxvl, mnvl> operator-(base_fwcounter<vt, mxvl, mnvl> lhs, const base_fwcounter<vt, mxvl, mnvl>& rhs)
{
  return lhs - rhs.value();
}

/*
 * Comparison operators for two fwcounters forward to the value comparators
 */
template <typename vt, vt mxvl, vt mnvl>
bool operator<(const base_fwcounter<vt, mxvl, mnvl>& lhs, const base_fwcounter<vt, mxvl, mnvl>& rhs)
{
  return lhs < rhs.value();
}

template <typename vt, vt mxvl, vt mnvl>
bool operator>(const base_fwcounter<vt, mxvl, mnvl>& lhs, const base_fwcounter<vt, mxvl, mnvl>& rhs)
{
  return lhs > rhs.value();
}

template <typename vt, vt mxvl, vt mnvl>
bool operator<=(const base_fwcounter<vt, mxvl, mnvl>& lhs, const base_fwcounter<vt, mxvl, mnvl>& rhs)
{
  return lhs <= rhs.value();
}

template <typename vt, vt mxvl, vt mnvl>
bool operator>=(const base_fwcounter<vt, mxvl, mnvl>& lhs, const base_fwcounter<vt, mxvl, mnvl>& rhs)
{
  return lhs >= rhs.value();
}

template <typename vt, vt mxvl, vt mnvl>
bool operator==(const base_fwcounter<vt, mxvl, mnvl>& lhs, const base_fwcounter<vt, mxvl, mnvl>& rhs)
{
  return lhs == rhs.value();
}

template <typename vt, vt mxvl, vt mnvl>
bool operator!=(const base_fwcounter<vt, mxvl, mnvl>& lhs, const base_fwcounter<vt, mxvl, mnvl>& rhs)
{
  return lhs != rhs.value();
}
} // namespace champsim::msl

#endif
