#include "cache.h"
#include <cstdlib>
#include <ctime>

#define maxRRPV 3
#define SHCT_SIZE  16384
#define SHCT_PRIME 16381
#define SAMPLER_SET (256*NUM_CPUS)
#define SAMPLER_WAY LLC_WAY
#define SHCT_MAX 7

uint32_t rrpv[LLC_SET][LLC_WAY];

// sampler structure
class SAMPLER_class
{
  public:
    uint8_t valid,
            type,
            used;

    uint64_t tag, cl_addr, ip;
    
    uint32_t lru;

    SAMPLER_class() {
        valid = 0;
        type = 0;
        used = 0;

        tag = 0;
        cl_addr = 0;
        ip = 0;

        lru = 0;
    };
};

// sampler
uint32_t rand_sets[SAMPLER_SET];
SAMPLER_class sampler[SAMPLER_SET][SAMPLER_WAY];

// prediction table structure
class SHCT_class {
  public:
    uint32_t counter;

    SHCT_class() {
        counter = 0;
    };
};
SHCT_class SHCT[NUM_CPUS][SHCT_SIZE];

// initialize replacement state
void CACHE::llc_initialize_replacement()
{
    cout << "Initialize SHIP state" << endl;

    for (int i=0; i<LLC_SET; i++) {
        for (int j=0; j<LLC_WAY; j++) {
            rrpv[i][j] = maxRRPV;
        }
    }

    // initialize sampler
    for (int i=0; i<SAMPLER_SET; i++) {
        for (int j=0; j<SAMPLER_WAY; j++) {
            sampler[i][j].lru = j;
        }
    }

    // randomly selected sampler sets
    srand(time(NULL));
    unsigned long rand_seed = 1;
    unsigned long max_rand = 1048576;
    uint32_t my_set = LLC_SET;
    int do_again = 0;
    for (int i=0; i<SAMPLER_SET; i++)
    {
        do 
        {
            do_again = 0;
            rand_seed = rand_seed * 1103515245 + 12345;
            rand_sets[i] = ((unsigned) ((rand_seed/65536) % max_rand)) % my_set;
            printf("Assign rand_sets[%d]: %u  LLC: %u\n", i, rand_sets[i], my_set);
            for (int j=0; j<i; j++) 
            {
                if (rand_sets[i] == rand_sets[j]) 
                {
                    do_again = 1;
                    break;
                }
            }
        } while (do_again);
        printf("rand_sets[%d]: %d\n", i, rand_sets[i]);
    }
}

// check if this set is sampled
uint32_t is_it_sampled(uint32_t set)
{
    for (int i=0; i<SAMPLER_SET; i++)
        if (rand_sets[i] == set)
            return i;

    return SAMPLER_SET;
}

// update sampler
void update_sampler(uint32_t cpu, uint32_t s_idx, uint64_t address, uint64_t ip, uint8_t type)
{
    SAMPLER_class *s_set = sampler[s_idx];
    uint64_t tag = address / (64*LLC_SET); 
    int match = -1;

    // check hit
    for (match=0; match<SAMPLER_WAY; match++)
    {
        if (s_set[match].valid && (s_set[match].tag == tag))
        {
            uint32_t SHCT_idx = s_set[match].ip % SHCT_PRIME;
            if (SHCT[cpu][SHCT_idx].counter > 0)
                SHCT[cpu][SHCT_idx].counter--;

            /*
            if (draw_transition)
                printf("cycle: %lu SHCT: %d ip: 0x%llX SAMPLER_HIT cl_addr: 0x%llX page: 0x%llX block: %ld set: %d\n", 
                ooo_cpu[cpu].current_cycle, SHCT[cpu][SHCT_idx].dead, s_set[match].ip, address>>6, address>>12, (address>>6) & 0x3F, s_idx);
            */

            //s_set[match].ip = ip; // SHIP does not update ip on sampler hit
            s_set[match].type = type; 
            s_set[match].used = 1;
            //D(printf("sampler hit  cpu: %d  set: %d  way: %d  tag: %x  ip: %lx  type: %d  lru: %d\n",
            //            cpu, rand_sets[s_idx], match, tag, ip, type, s_set[match].lru));

            break;
        }
    }

    // check invalid
    if (match == SAMPLER_WAY)
    {
        for (match=0; match<SAMPLER_WAY; match++)
        {
            if (s_set[match].valid == 0)
            {
                s_set[match].valid = 1;
                s_set[match].tag = tag;
                s_set[match].ip = ip;
                s_set[match].type = type;
                s_set[match].used = 0;

                //D(printf("sampler invalid  cpu: %d  set: %d  way: %d  tag: %x  ip: %lx  type: %d  lru: %d\n",
                //            cpu, rand_sets[s_idx], match, tag, ip, type, s_set[match].lru));
                break;
            }
        }
    }

    // miss
    if (match == SAMPLER_WAY)
    {
        for (match=0; match<SAMPLER_WAY; match++)
        {
            if (s_set[match].lru == (SAMPLER_WAY-1)) // Sampler uses LRU replacement
            {
                if (s_set[match].used == 0)
                {
                    uint32_t SHCT_idx = s_set[match].ip % SHCT_PRIME;
                    if (SHCT[cpu][SHCT_idx].counter < SHCT_MAX)
                        SHCT[cpu][SHCT_idx].counter++;

                    /*
                    if (draw_transition)
                        printf("cycle: %lu SHCT: %d ip: 0x%llX SAMPLER_MISS cl_addr: 0x%llX page: 0x%llX block: %ld set: %d\n", 
                        ooo_cpu[cpu].current_cycle, SHCT[cpu][SHCT_idx].dead, s_set[match].ip, address>>6, address>>12, (address>>6) & 0x3F, s_idx);
                    */
                }

                s_set[match].tag = tag;
                s_set[match].ip = ip;
                s_set[match].type = type;
                s_set[match].used = 0;

                //D(printf("sampler miss  cpu: %d  set: %d  way: %d  tag: %x  ip: %lx  type: %d  lru: %d\n",
                //            cpu, rand_sets[s_idx], match, tag, ip, type, s_set[match].lru));
                break;
            }
        }
    }

    // update LRU state
    uint32_t curr_position = s_set[match].lru;
    for (int i=0; i<SAMPLER_WAY; i++)
    {
        if (s_set[i].lru < curr_position)
            s_set[i].lru++;
    }
    s_set[match].lru = 0;
}

// find replacement victim
uint32_t CACHE::llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    // look for the maxRRPV line
    while (1)
    {
        for (int i=0; i<LLC_WAY; i++)
            if (rrpv[set][i] == maxRRPV)
                return i;

        for (int i=0; i<LLC_WAY; i++)
            rrpv[set][i]++;
    }

    // WE SHOULD NOT REACH HERE
    assert(0);
    return 0;
}

// called on every cache hit and cache fill
void CACHE::llc_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    string TYPE_NAME;
    if (type == LOAD)
        TYPE_NAME = "LOAD";
    else if (type == RFO)
        TYPE_NAME = "RFO";
    else if (type == PREFETCH)
        TYPE_NAME = "PF";
    else if (type == WRITEBACK)
        TYPE_NAME = "WB";
    else
        assert(0);

    if (hit)
        TYPE_NAME += "_HIT";
    else
        TYPE_NAME += "_MISS";

    if ((type == WRITEBACK) && ip)
        assert(0);

    //cout << "CPU: " << cpu << "  LLC " << setw(9) << TYPE_NAME << " set: " << setw(5) << set << " way: " << setw(2) << way;
    //cout << hex << " paddr: " << setw(12) << full_addr << " ip: " << setw(8) << ip << " victim_addr: " << victim_addr << dec << endl;
    
    // handle writeback access
    if (type == WRITEBACK) {
        if (hit)
            return;
        else {
            rrpv[set][way] = maxRRPV-1;
            return;
        }
    }

    // update sampler
    uint32_t s_idx = is_it_sampled(set);
    if (s_idx < SAMPLER_SET)
        update_sampler(cpu, s_idx, full_addr, ip, type);

    if (hit)
        rrpv[set][way] = 0;
    else {
        // SHIP prediction
        uint32_t SHCT_idx = ip % SHCT_PRIME;

        // sanity check
        if (SHCT_idx >= SHCT_PRIME)
            assert(0);

        rrpv[set][way] = maxRRPV-1;
        if (SHCT[cpu][SHCT_idx].counter == SHCT_MAX)
            rrpv[set][way] = maxRRPV;
    }
}

// use this function to print out your own stats at the end of simulation
void CACHE::llc_replacement_final_stats()
{

}
