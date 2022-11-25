#ifndef VBERTI_H_
#define VBERTI_H_

/*
 * Berti: an Accurate Local-Delta Data Prefetcher
 *  
 * 55th ACM/IEEE International Conference on Microarchitecture (MICRO 2022),
 * October 1-5, 2022, Chicago, Illinois, USA.
 * 
 * @Authors: Agustín Navarro-Torres, Biswabandan Panda, J. Alastruey-Benedé, 
 *           Pablo Ibáñez, Víctor Viñals-Yúfera, and Alberto Ros
 * @Email: agusnt@unizar.es
 * @Date: 22/11/2022
 * 
 * This code is an update from the original code to work with the new version
 * of ChampSim: https://github.com/agusnt/Berti-Artifact
 * 
 * Maybe fine-tuning is required to get the optimal performance/accuracy.
 * 
 * Please note that this version of ChampSim has noticeable differences with 
 * the used for the paper, so results can varies.
 * 
 * Cite this:
 * 
 * A. Navarro-Torres, B. Panda, J. Alastruey-Benedé, P. Ibáñez, 
 * V. Viñals-Yúfera and A. Ros, 
 * "Berti: an Accurate Local-Delta Data Prefetcher," 
 * 2022 55th IEEE/ACM International Symposium on Microarchitecture (MICRO), 
 * 2022, pp. 975-991, doi: 10.1109/MICRO56248.2022.00072.
 * 
 * @INPROCEEDINGS{9923806,  author={Navarro-Torres, Agustín and Panda, 
 * Biswabandan and Alastruey-Benedé, Jesús and Ibáñez, Pablo and Viñals-Yúfera,
 * Víctor and Ros, Alberto},  booktitle={2022 55th IEEE/ACM International 
 * Symposium on Microarchitecture (MICRO)},   title={Berti: an Accurate 
 * Local-Delta Data Prefetcher},   year={2022},  volume={},  number={},  
 * pages={975-991},  doi={10.1109/MICRO56248.2022.00072}}
 */

#include "berti_parameters.h"
#include "cache.h"

#include <algorithm>
#include <stdlib.h>
#include <vector>
#include <time.h>
#include <cstdio>
#include <tuple>
#include <queue>
#include <cmath>
#include <map>

/*****************************************************************************
 *                              Stats                                        *
 *****************************************************************************/
// Get average latency: Welford's method
typedef struct welford
{
  uint64_t num = 0; 
  float average = 0.0;
} welford_t;

welford_t average_latency;

// Get more info
uint64_t pf_to_l1 = 0;
uint64_t pf_to_l2 = 0;
uint64_t pf_to_l2_bc_mshr = 0;
uint64_t cant_track_latency = 0;
uint64_t cross_page = 0;
uint64_t no_cross_page = 0;
uint64_t no_found_berti = 0;
uint64_t found_berti = 0;

/*****************************************************************************
 *                      General Structs                                      *
 *****************************************************************************/

typedef struct Delta {
  uint64_t conf;
  int64_t  delta;
  uint8_t  rpl;
  Delta(): conf(0), delta(0), rpl(BERTI_R) {};
} delta_t; 

/*****************************************************************************
 *                      Berti structures                                     *
 *****************************************************************************/
class LatencyTable
{
  /* Latency table simulate the modified PQ and MSHR */
  private:
    struct latency_table {
      uint64_t addr = 0; // Addr
      uint64_t tag  = 0; // IP-Tag 
      uint64_t time = 0; // Event cycle
      bool     pf   = false;   // Is the entry accessed by a demand miss
    };
    int size;
    
    latency_table *latencyt;

  public:
    LatencyTable(const int size) : size(size)
    {
      latencyt = new latency_table[size];
    }
    ~LatencyTable() { delete latencyt;}

    uint8_t  add(uint64_t addr, uint64_t tag, bool pf, uint64_t cycle);
    uint64_t get(uint64_t addr);
    uint64_t del(uint64_t addr);
    uint64_t get_tag(uint64_t addr);
};

class ShadowCache
{
  /* Shadow cache simulate the modified L1D Cache */
  private:
    struct shadow_cache {
      uint64_t addr = 0; // Addr
      uint64_t lat  = 0;  // Latency
      bool     pf   = false;   // Is a prefetch 
    }; // This struct is the vberti table

    int sets;
    int ways;
    shadow_cache **scache;

  public:
    ShadowCache(const int sets, const int ways)
    {
      scache = new shadow_cache*[sets];
      for (int i = 0; i < sets; i++) scache[i] = new shadow_cache[ways];

      this->sets = sets;
      this->ways = ways;
    }

    ~ShadowCache()
    {
      for (int i = 0; i < sets; i++) delete scache[i];
      delete scache;
    }

    bool add(uint32_t set, uint32_t way, uint64_t addr, bool pf, uint64_t lat);
    bool get(uint64_t addr);
    void set_pf(uint64_t addr, bool pf);
    bool is_pf(uint64_t addr);
    uint64_t get_latency(uint64_t addr);
};

class HistoryTable
{
  /* History Table */
  private:
    struct history_table {
      uint64_t tag  = 0; // IP Tag
      uint64_t addr = 0; // IP @ accessed
      uint64_t time = 0; // Time where the line is accessed
    }; // This struct is the history table

    const int sets = HISTORY_TABLE_SETS;
    const int ways = HISTORY_TABLE_WAYS;

    history_table **historyt;
    history_table **history_pointers;

    uint16_t get_aux(uint32_t latency, uint64_t tag, uint64_t act_addr,
        uint64_t *tags, uint64_t *addr, uint64_t cycle);
  public:
    HistoryTable()
    {
      history_pointers = new history_table*[sets];
      historyt = new history_table*[sets];

      for (int i = 0; i < sets; i++) historyt[i] = new history_table[ways];
      for (int i = 0; i < sets; i++) history_pointers[i] = historyt[i];
    }

    ~HistoryTable()
    {
      for (int i = 0; i < sets; i++) delete historyt[i];
      delete historyt;

      delete history_pointers;
    }

    int get_ways();
    void add(uint64_t tag, uint64_t addr, uint64_t cycle);
    uint16_t get(uint32_t latency, uint64_t tag, uint64_t act_addr, 
        uint64_t *tags, uint64_t *addr, uint64_t cycle);
};

class Berti 
{
  /* Berti Table */
  private:
    struct berti {
      delta_t  deltas[BERTI_TABLE_DELTA_SIZE];
      uint64_t conf;
      uint64_t total_used;
    };

    std::map<uint64_t, berti*> bertit;
    std::queue<uint64_t> bertit_queue;

    bool static compare_greater_delta(delta_t a, delta_t b);

    void increase_conf_tag(uint64_t tag);
    void conf_tag(uint64_t tag);
    void add(uint64_t tag, int64_t delta);

  public:
    void find_and_update(uint64_t latency, uint64_t tag, uint64_t cycle, 
        uint64_t line_addr);
    uint8_t get(uint64_t tag, delta_t res[BERTI_TABLE_DELTA_SIZE]);
};

LatencyTable *latencyt;
ShadowCache *scache;
HistoryTable *historyt;
Berti *berti;
#endif
