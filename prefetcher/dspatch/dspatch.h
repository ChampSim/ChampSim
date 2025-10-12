#ifndef PREFETCHER_DSPATCH_H
#define PREFETCHER_DSPATCH_H

#include <cstdint>
#include <deque>
#include <vector>
#include <limits.h>

#include "champsim.h"
#include "modules.h"
#include "bitmap.h"

#define DSPATCH_MAX_BW_LEVEL 4

enum DSPatch_pref_candidate
{
	NONE = 0,
	COVP,
	ACCP,
	Num_DSPatch_pref_candidates
};

const char* Map_DSPatch_pref_candidate(DSPatch_pref_candidate candidate);

class DSPatch_counter
{
private:
	uint32_t counter;
public:
	DSPatch_counter() : counter(0) {}
	~DSPatch_counter(){}
	inline void incr(uint32_t max = UINT_MAX) {counter = (counter < max ? counter+1 : counter);}
	inline void decr(uint32_t min = 0) {counter = (counter > min ? counter-1 : counter);}
	inline uint32_t value(){return counter;}
	inline void reset(){counter = 0;}
};

class DSPatch_PBEntry
{
public:
	uint64_t page;
	uint64_t trigger_pc;
	uint32_t trigger_offset;
	Bitmap bmp_real;

	DSPatch_PBEntry() : page(0xdeadbeef), trigger_pc(0xdeadbeef), trigger_offset(0)
	{
		bmp_real.reset();
	}
	~DSPatch_PBEntry(){}
};

class DSPatch_SPTEntry
{
public:
	uint64_t signature;
	Bitmap bmp_cov;
	Bitmap bmp_acc;
	DSPatch_counter measure_covP, measure_accP;
	DSPatch_counter or_count;
	/* TODO: add confidence counters */

	DSPatch_SPTEntry() : signature(0xdeadbeef)
	{
		bmp_cov.reset();
		bmp_acc.reset();
		measure_covP.reset();
		measure_accP.reset();
		or_count.reset();
	}
	~DSPatch_SPTEntry(){}
};

class dspatch : public champsim::modules::prefetcher
{
public:
	using prefetcher::prefetcher;

	uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in);
	uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
	// void prefetcher_final_stats() {}
	void prefetcher_initialize();
	void prefetcher_cycle_operate();

private:

    std::deque<DSPatch_PBEntry*> page_buffer;
	DSPatch_SPTEntry **spt;
	std::deque<uint64_t> pref_buffer;

	/* 0 => b/w is less than 25% of peak
	 * 1 => b/w is more than 25% and less than 50% of peak
	 * 2 => b/w is more than 50% and less than 75% of peak
	 * 3 => b/w is more than 75% of peak
	 */
	uint8_t bw_bucket;

	uint64_t current_cycle;
	uint64_t dram_access_count;
	uint64_t last_reset_cycle;

	/* stats */
	struct
	{
		struct
		{
			uint64_t lookup;
			uint64_t hit;
			uint64_t evict;
			uint64_t insert;
		} pb;
		
		struct
		{
			uint64_t called;
			uint64_t selection_dist[DSPatch_pref_candidate::Num_DSPatch_pref_candidates];
			uint64_t reset;
			uint64_t total;
		} gen_pref;

		struct
		{
			uint64_t called;
			uint64_t none;
			uint64_t accp_reason1;
			uint64_t accp_reason2;
			uint64_t covp_reason1;
			uint64_t covp_reason2;
		} dyn_selection;

		struct
		{
			uint64_t called;
			uint64_t or_count_incr;
			uint64_t measure_covP_incr;
			uint64_t bmp_cov_reset;
			uint64_t bmp_cov_update;
			uint64_t measure_accP_incr;
			uint64_t measure_accP_decr;
			uint64_t bmp_acc_update;
		} spt;

		struct
		{
			uint64_t called;
			uint64_t bw_histogram[DSPATCH_MAX_BW_LEVEL];
		} bw;

		struct
		{
			uint64_t spilled;
			uint64_t buffered;
			uint64_t issued;
		} pref_buffer;
	} stats;


private:
  void invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr);
  void init_config();
  void init_stats();
  DSPatch_pref_candidate select_bitmap(DSPatch_SPTEntry* sptentry, Bitmap& bmp_selected);
  DSPatch_PBEntry* search_pb(uint64_t page);
  uint32_t get_hash(uint32_t key);
  uint32_t get_spt_index(uint64_t signature);
  uint64_t create_signature(uint64_t pc, uint64_t page, uint32_t offset);
  void add_to_spt(DSPatch_PBEntry* pbentry);
  void buffer_prefetch(std::vector<uint64_t> pref_addr);
  void issue_prefetch(std::vector<uint64_t>& pref_addr);
  void generate_prefetch(uint64_t pc, uint64_t page, uint32_t offset, uint64_t address, std::vector<uint64_t>& pref_addr);
  void update_bw();
  void record_dram_access();
  DSPatch_pref_candidate dyn_selection(DSPatch_SPTEntry* sptentry, Bitmap& bmp_selected);
};

#endif
