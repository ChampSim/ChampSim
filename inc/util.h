#ifndef UTIL_H
#define UTIL_H

#include <cstdint>

constexpr uint64_t lg2(uint64_t n)
{
    return n < 2 ? 0 : 1+lg2(n/2);
}

template <typename T>
struct is_valid
{
    using argument_type = T;
    is_valid() {}
    bool operator()(const argument_type &test)
    {
        return test.valid;
    }
};

template <typename T>
struct eq_addr
{
    using argument_type = T;
    const decltype(argument_type::address) val;
    eq_addr(decltype(argument_type::address) val) : val(val) {}
    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && test.address == val;
    }
};

template <typename T, typename U>
struct lru_comparator
{
    using first_argument_type = T;
    using second_argument_type = U;
    bool operator()(const first_argument_type &lhs, const second_argument_type &rhs)
    {
        is_valid<first_argument_type> first_validtest;
        is_valid<second_argument_type> second_validtest;
        return !second_validtest(rhs) || (first_validtest(lhs) && lhs.lru < rhs.lru);
    }
};


#endif

