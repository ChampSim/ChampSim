#ifndef SPP_H
#define SPP_H

#include "champsim_constants.h"
#include "util.h"

#include <array>
#include <bitset>
#include <cstdlib>
#include <tuple>
#include <vector>

// Signature table parameters
#define ST_SET 1
#define ST_WAY 256
#define ST_TAG_BIT 16
#define SIG_SHIFT 3
#define SIG_BIT 12
#define SIG_DELTA_BIT 7

// Pattern table parameters
#define PT_SET 512
#define PT_WAY 4
#define C_SIG_BIT 4
#define C_DELTA_BIT 4

// Prefetch filter parameters
#define FILTER_SET (1 << 8)
#define FILTER_WAY 1

// Global register parameters
#define GLOBAL_COUNTER_BIT 10
#define GLOBAL_ACCURACY_BIAS 127UL
#define MAX_GHR_ENTRY 8

enum confidence_t {REJECT, WEAKLY_ACCEPT, STRONGLY_ACCEPT}; // Request type for prefetch filter check

struct pfqueue_entry_t
{
    uint32_t sig;
    int32_t  delta;
    uint32_t depth;
    uint32_t confidence;

    pfqueue_entry_t(uint32_t sig, int32_t delta, uint32_t depth, uint32_t confidence)
        : sig(sig), delta(delta), depth(depth), confidence(confidence)
    {}
};

class SIGNATURE_TABLE
{
    private:
        struct sigtable_entry_t
        {
            bool     valid = false;
            uint32_t partial_page,
                     last_offset,
                     sig,
                     lru = 999999;

            uint32_t tag() const
            {
                return partial_page;
            }
        };

        std::array<std::array<sigtable_entry_t, ST_WAY>, ST_SET> sigtable;

    public:
        struct ghr_entry_t
        {
            bool     valid = false;
            uint32_t sig,
                     confidence = 0,
                     offset;
            int      delta;

            uint32_t tag() const
            {
                return offset;
            }

            bool operator< (const ghr_entry_t &other) const
            {
                is_valid<ghr_entry_t> validtest;
                return !validtest(*this) || (validtest(other) && this->confidence < other.confidence);
            }
        };

        // Global History Register (GHR) entries
        std::array<ghr_entry_t, MAX_GHR_ENTRY> page_bootstrap_table;

        SIGNATURE_TABLE() {};
        std::tuple<bool, uint32_t, uint32_t, int32_t> read_and_update_sig(uint64_t addr);
};

class PATTERN_TABLE
{
    private:
        struct pattable_entry_t
        {
            bool valid = false;
            int delta;
            unsigned int c_delta = 0;
            unsigned confidence = 0;

            int tag() const
            {
                return delta;
            }

            bool operator< (const pattable_entry_t &other) const
            {
                return this->c_delta < other.c_delta;
            }
        };

        struct pattable_set_t
        {
            std::array<pattable_entry_t, PT_WAY> ways;
            unsigned int c_sig = 0;
        };

        std::array<pattable_set_t, PT_SET> pattable;

    public:
        const unsigned int fill_threshold = 25;
        long long int global_accuracy = 0; // Alpha value in Section III. Equation 3

        PATTERN_TABLE() {}

        void update_pattern(uint32_t last_sig, int curr_delta);
        std::vector<pfqueue_entry_t> lookahead(uint32_t curr_sig, int delta);
};

class SPP_PREFETCH_FILTER
{
    private:
        struct filter_entry_t
        {
            uint64_t     page_no = 0;
            unsigned     lru = 9999999;
            std::bitset<PAGE_SIZE/BLOCK_SIZE> prefetched;
            std::bitset<PAGE_SIZE/BLOCK_SIZE> used;

            uint64_t tag() const
            {
                return page_no;
            }
        };

        std::array<std::array<filter_entry_t, FILTER_WAY>, FILTER_SET> prefetch_table;

    public:
        friend class is_valid<filter_entry_t>;

        // Global counters to calculate global prefetching accuracy
        unsigned int pf_useful = 0, pf_useless = 0;
        const unsigned int highconf_threshold = 40;

        SPP_PREFETCH_FILTER() {}

        confidence_t check(uint64_t pf_addr, uint32_t confidence);
        void update_demand(uint64_t pf_addr);
        void update_issue(uint64_t pf_addr);
        void update_evict(uint64_t pf_addr);
};

template <>
struct is_valid<SPP_PREFETCH_FILTER::filter_entry_t>
{
    bool operator() (const SPP_PREFETCH_FILTER::filter_entry_t &x)
    {
        return x.prefetched.any();
    }
};

template <typename T>
class tag_finder
{
    using argument_type = T;
    using check_t = decltype(std::declval<argument_type>().tag());
    const check_t to_check;

    public:
    explicit tag_finder(check_t chk) : to_check(chk) {}
    explicit tag_finder(const argument_type &chk) : to_check(chk.tag()) {}

    bool operator()(const argument_type &x)
    {
        is_valid<argument_type> validtest;
        return validtest(x) && x.tag() == to_check;
    }
};

uint64_t spp_hash(uint64_t key);
uint32_t spp_generate_signature(uint32_t last_sig, int32_t delta);

#endif

