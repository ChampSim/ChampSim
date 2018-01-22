#ifndef SPP_H
#define SPP_H

// SPP functional knobs
#define LOOKAHEAD_ON
//#define FILTER_ON
//#define GHR_ON
#define SPP_SANITY_CHECK

//#define SPP_DEBUG_PRINT
#ifdef SPP_DEBUG_PRINT
#define SPP_DP(x) x
#else
#define SPP_DP(x)
#endif

// Signature table parameters
#define ST_SET 1
#define ST_WAY 256
#define ST_PRIME 1
#define ST_TAG_BIT 16
#define ST_TAG_MASK ((1 << ST_TAG_BIT) - 1)
#define SIG_SHIFT 3
#define SIG_BIT 12
#define SIG_MASK ((1 << SIG_BIT) - 1)
#define DELTA_BIT 7

// Pattern table parameters
#define PT_SET 512
#define PT_WAY 4
#define PT_PRIME 509
#define C_SIG_BIT 4
#define C_DELTA_BIT 4
#define C_SIG_MAX ((1 << C_SIG_BIT) - 1)
#define C_DELTA_MAX ((1 << C_DELTA_BIT) - 1)

// Prefetch filter parameters
#define QUOTIENT_BIT  10
#define REMAINDER_BIT 6
#define HASH_BIT (QUOTIENT_BIT + REMAINDER_BIT + 1)
#define FILTER_SET (1 << QUOTIENT_BIT)
#define FILL_THRESHOLD 90
#define PF_THRESHOLD 25

// Global register parameters
#define GHR_COUNTER_BIT 10
#define GHR_COUNTER_MAX ((1 << GHR_COUNTER_BIT) - 1) 

enum FILTER_REQUEST {SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT}; // Request type for prefetch filter
uint64_t get_hash(uint64_t key);

class SIGNATURE_TABLE {
  public:
    bool     valid[ST_SET][ST_WAY];
    uint32_t tag[ST_SET][ST_WAY],
             last_offset[ST_SET][ST_WAY],
             sig[ST_SET][ST_WAY],
             lru[ST_SET][ST_WAY];

    SIGNATURE_TABLE() {
        cout << endl << "Initialize SIGNATURE TABLE" << endl;
        cout << "ST_SET: " << ST_SET << endl;
        cout << "ST_WAY: " << ST_WAY << endl;
        cout << "ST_TAG_BIT: " << ST_TAG_BIT << endl;
        cout << "ST_TAG_MASK: " << hex << ST_TAG_MASK << dec << endl;

        for (uint32_t set = 0; set < ST_SET; set++)
            for (uint32_t way = 0; way < ST_WAY; way++) {
                valid[set][way] = 0;
                tag[set][way] = 0;
                last_offset[set][way] = 0;
                sig[set][way] = 0;
                lru[set][way] = way;
            }
    };

    void read_and_update_sig(uint64_t page, uint32_t offset, uint32_t &last_sig, uint32_t &curr_sig, int32_t &delta);
};

class PATTERN_TABLE {
  public:
    int      delta[PT_SET][PT_WAY];
    uint32_t c_delta[PT_SET][PT_WAY],
             c_sig[PT_SET];

    PATTERN_TABLE() {
        cout << endl << "Initialize PATTERN TABLE" << endl;
        cout << "PT_SET: " << PT_SET << endl;
        cout << "PT_WAY: " << PT_WAY << endl;
        cout << "DELTA_BIT: " << DELTA_BIT << endl;
        cout << "C_SIG_BIT: " << C_SIG_BIT << endl;
        cout << "C_DELTA_BIT: " << C_DELTA_BIT << endl;

        for (uint32_t set = 0; set < PT_SET; set++) {
            for (uint32_t way = 0; way < PT_WAY; way++) {
                delta[set][way] = 0;
                c_delta[set][way] = 0;
            }
            c_sig[set] = 0;
        }
    }

    void update_pattern(uint32_t last_sig, int curr_delta),
         read_pattern(uint32_t curr_sig, int *prefetch_delta, uint32_t *confidence_q, uint32_t &lookahead_way, uint32_t &lookahead_conf, uint32_t &pf_q_tail, uint32_t &depth);
};

class PREFETCH_FILTER {
  public:
    uint64_t remainder_tag[FILTER_SET];
    bool     valid[FILTER_SET],  // Consider this as "prefetched"
             useful[FILTER_SET]; // Consider this as "used"

    PREFETCH_FILTER() {
        cout << endl << "Initialize PREFETCH FILTER" << endl;
        cout << "FILTER_SET: " << FILTER_SET << endl;

        for (uint32_t set = 0; set < FILTER_SET; set++) {
            remainder_tag[set] = 0;
            valid[set] = 0;
            useful[set] = 0;
        }

    }

    bool     check(uint64_t pf_addr, FILTER_REQUEST filter_request);
};

class GLOBAL_REGISTER {
  public:
    uint64_t pf_useful,
             pf_issued,
             global_conf;

    GLOBAL_REGISTER() {
        pf_useful = 0;
        pf_issued = 0;
        global_conf = 0;
    }
};

#endif
