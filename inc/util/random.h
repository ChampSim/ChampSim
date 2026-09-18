#ifndef UTIL_RANDOM_H
#define UTIL_RANDOM_H

#include <istream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace champsim
{

// Only defined for integral types
template <class IntType, class Enable = void>
class uniform_int_distribution
{
public:
  uniform_int_distribution() = delete;
};

/**
 * Custom re-implementation of std::uniform_int_distribution.
 *
 * This is needed because std::uniform_int_distribution is implemented
 * differently across platforms/compilers/etc. Therefore, it can generate different
 * values on different platforms.
 *
 * Unlike std::uniform_int_distribution, this version should generate identical
 * values on every platform.
 *
 * \tparam IntType The type of the values to generate.
 */
template <class IntType>
class uniform_int_distribution<IntType, std::enable_if_t<std::is_integral<IntType>::value>>
{
public:
  using result_type = IntType;

  class param_type
  {
  private:
    IntType min;
    IntType max;

  public:
    using distribution_type = uniform_int_distribution<IntType>;

    param_type() : param_type(0) {}

    explicit param_type(IntType min_, IntType max_ = std::numeric_limits<IntType>::max()) : min(min_), max(max_)
    {
      if (min > max) {
        throw std::invalid_argument("Invalid range");
      }
    }

    constexpr IntType a() const noexcept { return min; }
    constexpr IntType b() const noexcept { return max; }

    friend constexpr bool operator==(const param_type& left, const param_type& right) noexcept { return left.min == right.min && left.max == right.max; }
    friend constexpr bool operator!=(const param_type& left, const param_type& right) noexcept { return !(left == right); }

    template <class OStr>
    friend OStr& operator<<(OStr& os, const param_type& p)
    {
      os << p.min << ' ' << p.max;
      return os;
    }

    template <class IStr>
    friend IStr& operator>>(IStr& is, param_type& p)
    {
      IntType min_, max_;
      if (is >> min_ >> std::ws >> max_) {
        try {
          p = param_type(min_, max_);
        } catch (const std::invalid_argument& e) {
          is.setstate(std::ios::failbit);
        }
      }
      return is;
    }
  };

  uniform_int_distribution() : uniform_int_distribution(0) {}

  explicit uniform_int_distribution(IntType a, IntType b = std::numeric_limits<IntType>::max()) : uniform_int_distribution(param_type(a, b)) {}

  explicit uniform_int_distribution(const param_type& params_) : params(params_) {}

  constexpr result_type a() const noexcept { return params.a(); }

  constexpr result_type b() const noexcept { return params.b(); }

  constexpr result_type min() const noexcept { return params.a(); }

  constexpr result_type max() const noexcept { return params.b(); }

  constexpr param_type param() const noexcept { return params; }

  void param(const param_type& params_) { params = params_; }

  template <class URBG>
  result_type operator()(URBG& g)
  {
    return generate(g, params);
  }

  template <class URBG>
  result_type operator()(URBG& g, const param_type& params_)
  {
    return generate(g, params_);
  }

  friend constexpr bool operator==(const uniform_int_distribution& left, const uniform_int_distribution& right) noexcept { return left.params == right.params; }

  friend constexpr bool operator!=(const uniform_int_distribution& left, const uniform_int_distribution& right) noexcept { return !(left == right); }

  template <class OStr>
  friend OStr& operator<<(OStr& os, const uniform_int_distribution& d)
  {
    os << d.params;
    return os;
  }

  template <class IStr>
  friend IStr& operator>>(IStr& is, uniform_int_distribution& d)
  {
    param_type params_;
    if (is >> params_) {
      d.param(params_);
    }
    return is;
  }

private:
  param_type params;

  /**
   * Sample a random value from the distribution.
   *
   * \tparam URBG The type of the uniform random bit generator (URBG)
   * \param g The random number generator.
   * \param params The distribution parameters.
   * \return A random integral value sampled uniformly from the range [min, max]
   */
  template <class URBG>
  static result_type generate(URBG& g, const param_type& params)
  {
    // Use an unsigned result type to avoid signed overflow
    // This unsigned type is common to both the URBG and distribution.
    using u_result_type = typename std::common_type<typename URBG::result_type,                    // URBG result type
                                                    typename std::make_unsigned<result_type>::type // (Unsigned) distrubtion result type
                                                    >::type;

    // Get the ranges of the URBG and distribution
    static_assert(URBG::max() > URBG::min(), "URBG must have a range greater than zero");
    constexpr u_result_type range_g = static_cast<u_result_type>(URBG::max() - URBG::min());
    const u_result_type range_d = static_cast<u_result_type>(params.b() - params.a() + 1);

    if (range_g < range_d) {
      throw std::invalid_argument("URBG range is too small for the distribution");
    }

    // Use rejection sampling to avoid modulo bias
    const u_result_type reject_limit{range_g % range_d};
    u_result_type sample{g() - URBG::min()};
    while (sample <= reject_limit) {
      sample = g() - URBG::min();
    }

    return static_cast<result_type>((sample % range_d) + params.a());
  }
};
} // namespace champsim

#endif // UTIL_RANDOM_H