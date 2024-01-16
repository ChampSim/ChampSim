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

#ifndef UTIL_UNITS_H
#define UTIL_UNITS_H

#include <map>
#include <numeric>
#include <ratio>
#include <string_view>
#include <fmt/core.h>
#include <fmt/ostream.h>

#include "champsim.h"
#include "bits.h"
#include "ratio.h"

namespace champsim
{
  namespace data
  {
    template <typename Rep, typename Unit>
    class size
    {
      public:
      using rep = Rep;
      using unit = typename Unit::type;
      constexpr static auto byte_multiple{unit::num};

      private:
      rep m_count{};

      template <typename Rep2, typename Unit2>
      friend class size;

      public:
      size() = default;
      size(const size<Rep, Unit>& other) = default;
      size<Rep, Unit>& operator=(const size<Rep, Unit>& other) = default;

      template <typename Rep2>
      constexpr explicit size(const Rep2& other) : m_count(other) {}

      template <typename Rep2, typename Unit2>
      constexpr size(const size<Rep2, Unit2>& other)
      {
        using conversion = typename std::ratio_divide<Unit2, Unit>::type;
        m_count = other.m_count * conversion::num / conversion::den;
      }

      auto count() const { return m_count; }

      constexpr size<Rep, Unit> operator+() const
      {
        return *this;
      }

      constexpr size<Rep, Unit> operator-() const
      {
        return size<Rep, Unit>{-m_count};
      }

      constexpr size<Rep, Unit>& operator+=(const size<Rep, Unit>& other)
      {
        m_count += other.count();
        return *this;
      }

      constexpr size<Rep, Unit>& operator-=(const size<Rep, Unit>& other)
      {
        m_count -= other.count();
        return *this;
      }

      constexpr size<Rep, Unit>& operator*=(const rep& other)
      {
        m_count *= other;
        return *this;
      }

      constexpr size<Rep, Unit>& operator/=(const rep& other)
      {
        m_count /= other;
        return *this;
      }

      constexpr size<Rep, Unit>& operator%=(const rep& other)
      {
        m_count %= other;
        return *this;
      }

      constexpr size<Rep, Unit>& operator%=(const size<Rep, Unit>& other)
      {
        m_count %= other.count();
        return *this;
      }
    };

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator+(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      using result_type = typename std::common_type<decltype(lhs), decltype(rhs)>::type;
      result_type retval{lhs};
      retval += rhs;
      return retval;
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator-(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      using result_type = typename std::common_type<decltype(lhs), decltype(rhs)>::type;
      result_type retval{lhs};
      retval -= rhs;
      return retval;
    }

    template<class Rep1, class Unit1, class Rep2>
    auto constexpr operator*(const size<Rep1,Unit1>& lhs, const Rep2& rhs)
    {
      using result_type = size<typename std::common_type<Rep1, Rep2>::type, Unit1>;
      result_type retval{lhs};
      retval *= rhs;
      return retval;
    }

    template<class Rep1, class Rep2, class Unit2>
    auto constexpr operator*(const Rep1& lhs, const size<Rep2,Unit2>& rhs)
    {
      return rhs * lhs;
    }

    template<class Rep1, class Unit1, class Rep2>
    auto constexpr operator/(const size<Rep1,Unit1>& lhs, const Rep2& rhs)
    {
      using result_type = size<typename std::common_type<Rep1, Rep2>::type, Unit1>;
      result_type retval{lhs};
      retval /= rhs;
      return retval;
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator/(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      using result_type = typename std::common_type<decltype(lhs), decltype(rhs)>::type;
      result_type retval{lhs};
      retval /= rhs;
      return retval;
    }

    template<class Rep1, class Unit1, class Rep2>
    auto constexpr operator%(const size<Rep1,Unit1>& lhs, const Rep2& rhs)
    {
      using result_type = size<typename std::common_type<Rep1, Rep2>::type, Unit1>;
      result_type retval{lhs};
      retval %= result_type{rhs};
      return retval;
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator%(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      using result_type = typename std::common_type<decltype(lhs), decltype(rhs)>::type;
      result_type retval{lhs};
      retval %= rhs;
      return retval;
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator==(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      using comparison_type = typename std::common_type<decltype(lhs), decltype(rhs)>::type;
      return comparison_type{lhs}.count() == comparison_type{rhs}.count();
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator!=(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      return !(lhs == rhs);
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator<(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      using comparison_type = typename std::common_type<decltype(lhs), decltype(rhs)>::type;
      return comparison_type{lhs}.count() < comparison_type{rhs}.count();
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator<=(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      return lhs < rhs || lhs == rhs;
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator>(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      return !(lhs <= rhs);
    }

    template<class Rep1, class Unit1, class Rep2, class Unit2>
    auto constexpr operator>=(const size<Rep1,Unit1>& lhs, const size<Rep2,Unit2>& rhs)
    {
      return !(lhs < rhs);
    }

    namespace data_literals
    {
      champsim::data::bytes     operator""_B  (unsigned long long val);
      champsim::data::kibibytes operator""_kiB(unsigned long long val);
      champsim::data::mebibytes operator""_MiB(unsigned long long val);
      champsim::data::gibibytes operator""_GiB(unsigned long long val);
      champsim::data::tebibytes operator""_TiB(unsigned long long val);
    }
  }
}

template<class Rep1, class Unit1, class Rep2, class Unit2>
struct std::common_type<champsim::data::size<Rep1, Unit1>, champsim::data::size<Rep2, Unit2>>
{
  using type = champsim::data::size<
    typename std::common_type<Rep1, Rep2>::type,
    typename std::ratio<
      std::gcd(Unit1::num, Unit2::num),
      std::lcm(Unit1::den, Unit2::den)
    >::type
  >;
};

template <typename Rep, typename Unit>
struct fmt::formatter<champsim::data::size<Rep, Unit>> {
  using value_type = champsim::data::size<Rep, Unit>;
  constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
    // Check if reached the end of the range:
    if (ctx.begin() != ctx.end()) ctx.on_error("invalid format");

    // Return an iterator past the end of the parsed range:
    return ctx.end();
  }

  auto format(const value_type& val, format_context& ctx) const -> format_context::iterator {
    const static std::map<unsigned long long, std::string_view> suffix_map{
      {champsim::data::bytes::byte_multiple, std::string_view{"B"}},
      {champsim::data::kibibytes::byte_multiple, std::string_view{"kiB"}},
      {champsim::data::mebibytes::byte_multiple, std::string_view{"MiB"}},
      {champsim::data::gibibytes::byte_multiple, std::string_view{"GiB"}},
      {champsim::data::tebibytes::byte_multiple, std::string_view{"TiB"}}
    };

    auto suffix_it = suffix_map.find(value_type::byte_multiple);
    std::string_view suffix{};
    if (suffix_it != std::end(suffix_map)) {
      suffix = suffix_it->second;
    }
    return fmt::format_to(ctx.out(), "{}{}", val.count(), suffix);
  }
};

template <typename Ostr, typename Rep, typename Unit>
auto operator<<(Ostr& os, champsim::data::size<Rep, Unit>& val) -> decltype(os)
{
  fmt::print(os, "{}", val);
  return os;
}

#endif
