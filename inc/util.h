#ifndef UTIL_H
#define UTIL_H

#include <cstdint>

constexpr unsigned lg2(uint64_t n)
{
    return n < 2 ? 0 : 1+lg2(n/2);
}

constexpr uint64_t bitmask(std::size_t begin, std::size_t end = 0)
{
    return ((1ull << (begin - end))-1) << end;
}

constexpr uint64_t splice_bits(uint64_t upper, uint64_t lower, std::size_t bits)
{
    return (upper & ~bitmask(bits)) | (lower & bitmask(bits));
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

template <typename T, typename = void>
struct eq_addr
{
    using argument_type = T;
    using addr_type = decltype(T::address);
    const addr_type match_addr;
    const std::size_t shamt;

    eq_addr(const argument_type &elem, std::size_t shamt = 0) : match_addr(elem.address), shamt(shamt) {}
    eq_addr(addr_type addr, std::size_t shamt = 0) : match_addr(addr), shamt(shamt) {}

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

    eq_addr(const argument_type &elem, std::size_t shamt = 0) : match_asid(elem.asid), match_addr(elem.address), shamt(shamt) {}
    eq_addr(asid_type asid, addr_type addr, std::size_t shamt = 0) : match_asid(asid), match_addr(addr), shamt(shamt) {}

    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && test.asid == match_asid && (test.address >> shamt) == (match_addr >> shamt);
    }
};

template <typename T, typename BIN, typename U = T, typename UN_T = is_valid<T>, typename UN_U = is_valid<U>>
struct invalid_is_minimal
{
    bool operator() (const T &lhs, const U &rhs)
    {
        UN_T lhs_unary;
        UN_U rhs_unary;
        BIN  cmp;

        return !lhs_unary(lhs) || (rhs_unary(rhs) && cmp(lhs, rhs));
    }
};

template <typename T, typename BIN, typename U = T, typename UN_T = is_valid<T>, typename UN_U = is_valid<U>>
struct invalid_is_maximal
{
    bool operator() (const T &lhs, const U &rhs)
    {
        UN_T lhs_unary;
        UN_U rhs_unary;
        BIN  cmp;

        return !rhs_unary(rhs) || (lhs_unary(lhs) && cmp(lhs, rhs));
    }
};

template <typename T, typename U=T>
struct cmp_event_cycle
{
    bool operator() (const T &lhs, const U &rhs)
    {
        return lhs.event_cycle < rhs.event_cycle;
    }
};

template <typename T>
struct min_event_cycle : invalid_is_maximal<T, cmp_event_cycle<T>> {};

template <typename T, typename U=T>
struct cmp_lru
{
    bool operator() (const T &lhs, const U &rhs)
    {
        return lhs.lru < rhs.lru;
    }
};

/*
 * A comparator to determine the LRU element. To use this comparator, the type must have a member
 * variable named "lru" and have a specialization of is_valid<>.
 *
 * To use:
 *     auto lru_elem = std::max_element(std::begin(set), std::end(set), lru_comparator<BLOCK>());
 *
 * The MRU element can be found using std::min_element instead.
 */
template <typename T, typename U=T>
struct lru_comparator : invalid_is_maximal<T, cmp_lru<T,U>, U>
{
    using first_argument_type = T;
    using second_argument_type = U;
};

/*
 * A functor to reorder elements to a new LRU order.
 * The type must have a member variable named "lru".
 *
 * To use:
 *     std::for_each(std::begin(set), std::end(set), lru_updater<BLOCK>(hit_element));
 */
template <typename T>
struct lru_updater
{
    const decltype(T::lru) val;
    explicit lru_updater(decltype(T::lru) val) : val(val) {}

    template <typename U>
    explicit lru_updater(U iter) : val(iter->lru) {}

    void operator()(T &x)
    {
        if (x.lru == val) x.lru = 0;
        else ++x.lru;
    }
};

template <typename T, typename U=T>
struct ord_event_cycle
{
    using first_argument_type = T;
    using second_argument_type = U;
    bool operator() (const first_argument_type &lhs, const second_argument_type &rhs)
    {
        is_valid<first_argument_type> first_validtest;
        is_valid<second_argument_type> second_validtest;
        return !second_validtest(rhs) || (first_validtest(lhs) && lhs.event_cycle < rhs.event_cycle);
    }
};

#endif

