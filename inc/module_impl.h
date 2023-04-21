#ifndef MODULE_IMPL_H
#define MODULE_IMPL_H

namespace champsim {

namespace detail
{
template <typename T>
struct take_last {
  T operator()(T, T last) const { return last; }
};
} // namespace detail

} // namespace champsim

#endif
