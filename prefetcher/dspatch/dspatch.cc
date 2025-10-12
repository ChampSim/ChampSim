#include <cassert>
#include <algorithm>
#include <iomanip>
#include <iostream>

#include "dspatch.h"
#include "util.h"


const char* DSPatch_pref_candidate_string[] = {"NONE", "CovP", "AccP"};
const char* Map_DSPatch_pref_candidate(DSPatch_pref_candidate candidate)
{
	assert((uint32_t)candidate < DSPatch_pref_candidate::Num_DSPatch_pref_candidates);
	return DSPatch_pref_candidate_string[(uint32_t)candidate];
}

namespace config
{	
	uint32_t dspatch_log2_region_size = 11;
	uint32_t dspatch_num_cachelines_in_region = 1 << (dspatch_log2_region_size - 6);
	uint32_t dspatch_pb_size = 64;
	uint32_t dspatch_num_spt_entries = 256;
	uint32_t dspatch_compression_granularity = 2;
	uint32_t dspatch_pred_throttle_bw_thr = 3;
	uint32_t dspatch_bitmap_selection_policy = 3;
	uint32_t dspatch_sig_type = 1;
	uint32_t dspatch_sig_hash_type = 2;
	uint32_t dspatch_or_count_max = 3;
	uint32_t dspatch_measure_covP_max = 3;
	uint32_t dspatch_measure_accP_max = 3;
	uint32_t dspatch_acc_thr = 50;
	uint32_t dspatch_cov_thr = 50;
	bool     dspatch_enable_pref_buffer = true;
	uint32_t dspatch_pref_buffer_size = 256;
	uint32_t dspatch_pref_degree = 8;
	uint64_t TRC_CYCLES = 122; // Set this based on system
	uint64_t WINDOW_SIZE = 4 * TRC_CYCLES;
	uint64_t PEAK_ACCESS_COUNT = 15; // Define based on system
}

void dspatch::init_config()
{
	assert(config::dspatch_log2_region_size <= 12);
	assert(config::dspatch_num_cachelines_in_region * config::dspatch_compression_granularity <= 64);
}

void dspatch::init_stats()
{
	bzero(&stats, sizeof(stats));
}

DSPatch_pref_candidate dspatch::select_bitmap(DSPatch_SPTEntry *sptentry, Bitmap &bmp_selected)
{
	DSPatch_pref_candidate candidate = DSPatch_pref_candidate::NONE;
	switch(config::dspatch_bitmap_selection_policy)
	{
		case 1:
			/* always select coverage bitmap */
			bmp_selected = sptentry->bmp_cov;
			candidate = DSPatch_pref_candidate::COVP;
			break;
		case 2:
			/* always select accuracy bitmap */
			bmp_selected = sptentry->bmp_acc;
			candidate = DSPatch_pref_candidate::ACCP;
			break;
		case 3:
			/* hybrid selection */
			candidate = dyn_selection(sptentry, bmp_selected);
			break;
		default:
			std::cout << "invalid dspatch_bitmap_selection_policy " << config::dspatch_bitmap_selection_policy << std::endl;
			assert(false);
	}
	return candidate;
}

DSPatch_PBEntry* dspatch::search_pb(uint64_t page) {
	auto it = find_if(page_buffer.begin(), page_buffer.end(), 
		[page](DSPatch_PBEntry *pbentry){return pbentry->page == page;});
	return it != page_buffer.end() ? (*it) : NULL;
}

uint32_t dspatch::get_hash(uint32_t key)
{
	switch(config::dspatch_sig_hash_type)
	{
		case 1: 	return key;
		case 2: 	return HashZoo::jenkins(key);
		case 3: 	return HashZoo::knuth(key);
		case 4: 	return HashZoo::murmur3(key);
		case 5: 	return HashZoo::jenkins32(key);
		case 6: 	return HashZoo::hash32shift(key);
		case 7: 	return HashZoo::hash32shiftmult(key);
		case 8: 	return HashZoo::hash64shift(key);
		case 9: 	return HashZoo::hash5shift(key);
		case 10: 	return HashZoo::hash7shift(key);
		case 11: 	return HashZoo::Wang6shift(key);
		case 12: 	return HashZoo::Wang5shift(key);
		case 13: 	return HashZoo::Wang4shift(key);
		case 14: 	return HashZoo::Wang3shift(key);
		default: 	assert(false);
	}
}

// DSPatch organizes the SPT as a 256-entry tagless direct-mapped structure. 
// A simple folded-XOR hash of the PC is used to index into this structure.
uint32_t dspatch::get_spt_index(uint64_t signature)
{
	uint32_t folded_sig = folded_xor(signature, 2);
	/* apply hash */
	uint32_t hashed_index = get_hash(folded_sig);
	return hashed_index % config::dspatch_num_spt_entries;
}

uint64_t dspatch::create_signature(uint64_t pc, uint64_t page, uint32_t offset)
{
	uint64_t signature = 0;
	switch(config::dspatch_sig_type)
	{
		case 1:
			signature = pc;
			break;
		default:
			std::cout << "invalid dspatch_sig_type " << config::dspatch_sig_type << std::endl;
			assert(false);
	}
	return signature;
}

void dspatch::add_to_spt(DSPatch_PBEntry *pbentry)
{
	stats.spt.called++;
	Bitmap bmp_real, bmp_cov, bmp_acc;
	bmp_real = pbentry->bmp_real;
	uint64_t trigger_pc = pbentry->trigger_pc;
	uint32_t trigger_offset = pbentry->trigger_offset;

	uint64_t signature = create_signature(trigger_pc, 0xdeadbeef, trigger_offset);
	uint32_t spt_index = get_spt_index(signature);
	assert(spt_index < config::dspatch_num_spt_entries);
	DSPatch_SPTEntry *sptentry = spt[spt_index];

	bmp_real = BitmapHelper::rotate_right(bmp_real, trigger_offset, 
		config::dspatch_num_cachelines_in_region);
	bmp_cov  = BitmapHelper::decompress(sptentry->bmp_cov, 
		config::dspatch_compression_granularity, 
		config::dspatch_num_cachelines_in_region);
	bmp_acc  = BitmapHelper::decompress(sptentry->bmp_acc, 
		config::dspatch_compression_granularity, 
		config::dspatch_num_cachelines_in_region);

	uint32_t pop_count_bmp_real = BitmapHelper::count_bits_set(bmp_real);
	uint32_t pop_count_bmp_cov  = BitmapHelper::count_bits_set(bmp_cov);
	uint32_t pop_count_bmp_acc  = BitmapHelper::count_bits_set(bmp_acc);
	uint32_t same_count_bmp_cov = BitmapHelper::count_bits_same(bmp_cov, bmp_real);
	uint32_t same_count_bmp_acc = BitmapHelper::count_bits_same(bmp_acc, bmp_real);

	uint32_t cov_bmp_cov = 100 * (float)same_count_bmp_cov / pop_count_bmp_real;
	uint32_t acc_bmp_cov = 100 * (float)same_count_bmp_cov / pop_count_bmp_cov;
	uint32_t cov_bmp_acc = 100 * (float)same_count_bmp_acc / pop_count_bmp_real;
	uint32_t acc_bmp_acc = 100 * (float)same_count_bmp_acc / pop_count_bmp_acc;

	/* Update CovP counters */
	if(BitmapHelper::count_bits_diff(bmp_real, bmp_cov) != 0)
		sptentry->or_count.incr(config::dspatch_or_count_max);
	if(acc_bmp_cov < config::dspatch_acc_thr || cov_bmp_cov < config::dspatch_cov_thr)
		sptentry->measure_covP.incr(config::dspatch_measure_covP_max);

	/* Update CovP */
	if(sptentry->measure_covP.value() == config::dspatch_measure_covP_max) {
		if(bw_bucket == 3 || cov_bmp_cov < 50) { /* WARNING: hardcoded values */
			sptentry->bmp_cov = 
				BitmapHelper::compress(bmp_real, config::dspatch_compression_granularity);
			sptentry->or_count.reset();
			stats.spt.bmp_cov_reset++;
		}
	}
	else
	{
		sptentry->bmp_cov = 
			BitmapHelper::compress(BitmapHelper::bitwise_or(bmp_cov, bmp_real), 
				config::dspatch_compression_granularity);
	}

	/* Update AccP counter(s) */
	if(acc_bmp_acc < 50) /* WARNING: hardcoded value */
		sptentry->measure_accP.incr();
	else
		sptentry->measure_accP.decr();

	/* Update AccP */
	sptentry->bmp_acc = 
		BitmapHelper::bitwise_and(bmp_real, 
			BitmapHelper::decompress(sptentry->bmp_cov, 
				config::dspatch_compression_granularity, 
				config::dspatch_num_cachelines_in_region));
	sptentry->bmp_acc = BitmapHelper::compress(sptentry->bmp_acc,
		config::dspatch_compression_granularity);
}

void dspatch::buffer_prefetch(std::vector<uint64_t> pref_addr)
{
	uint32_t count = 0;
	for(uint32_t index = 0; index < pref_addr.size(); ++index)
	{
		if(pref_buffer.size() >= config::dspatch_pref_buffer_size)
		{
			break;
		}
		pref_buffer.push_back(pref_addr[index]);
		count++;
	}
	stats.pref_buffer.buffered += count;
	stats.pref_buffer.spilled += (pref_addr.size() - count);
}

void dspatch::issue_prefetch(std::vector<uint64_t> &pref_addr)
{
	uint32_t count = 0;
	while(!pref_buffer.empty() && count < config::dspatch_pref_degree)
	{
		pref_addr.push_back(pref_buffer.front());
		pref_buffer.pop_front();
		count++;
	}
	stats.pref_buffer.issued += pref_addr.size();
}


void dspatch::invoke_prefetcher(uint64_t pc, uint64_t address, uint8_t cache_hit, uint8_t type, std::vector<uint64_t> &pref_addr) {
	uint64_t page = address >> config::dspatch_log2_region_size;
	uint32_t offset = (address >> LOG2_BLOCK_SIZE) & ((1ull << (config::dspatch_log2_region_size - LOG2_BLOCK_SIZE)) - 1);

	//std::cout<< "FAKE address: " << address << "  , " << "page: " << page << "  , " << "offset: " << offset << std::endl;

	DSPatch_PBEntry *pbentry = NULL;
	pbentry = search_pb(page);
	stats.pb.lookup++;
	if(pbentry)
	{
		/* record the access */
		pbentry->bmp_real[offset] = true;
		stats.pb.hit++;

		/* trigger prefetch */
		generate_prefetch(pc, page, offset, address, pref_addr);

		if(config::dspatch_enable_pref_buffer)
		{
			buffer_prefetch(pref_addr);

			pref_addr.clear();
		}

		add_to_spt(pbentry);

	}
	else /* page buffer miss, prefetch trigger opportunity */
	{
		/* insert the new page buffer entry */
		if(page_buffer.size() >= config::dspatch_pb_size)
		{
			pbentry = page_buffer.front();
			page_buffer.pop_front();
			add_to_spt(pbentry);

			delete pbentry;
			stats.pb.evict++;
		}
		
		pbentry = new DSPatch_PBEntry();
		pbentry->page = page;
		pbentry->trigger_pc = pc;
		pbentry->trigger_offset = offset;
		pbentry->bmp_real[offset] = true;
		page_buffer.push_back(pbentry);
		stats.pb.insert++;
		
	}

	/* slowly inject prefetches at every demand access, if buffer is turned on */
	if(config::dspatch_enable_pref_buffer)
	{
		issue_prefetch(pref_addr);
	}
}

void dspatch::generate_prefetch(uint64_t pc, uint64_t page, uint32_t offset, uint64_t address, std::vector<uint64_t> &pref_addr)
{
	Bitmap bmp_cov, bmp_acc, bmp_pred;
	uint64_t signature = 0xdeadbeef;
	DSPatch_pref_candidate candidate = DSPatch_pref_candidate::NONE;
	DSPatch_SPTEntry *sptentry = NULL;

	stats.gen_pref.called++;
	signature = create_signature(pc, page, offset);
	uint32_t spt_index = get_spt_index(signature);
	assert(spt_index < config::dspatch_num_spt_entries);
	
	sptentry = spt[spt_index];
	candidate = select_bitmap(sptentry, bmp_pred);
	stats.gen_pref.selection_dist[candidate]++;

	/* decompress and rotate back the bitmap */
	bmp_pred = BitmapHelper::decompress(bmp_pred, config::dspatch_compression_granularity, config::dspatch_num_cachelines_in_region);
	bmp_pred = BitmapHelper::rotate_left(bmp_pred, offset, config::dspatch_num_cachelines_in_region);

	/* Throttling predictions incase of predicting with bmp_acc and b/w is high */
	if(bw_bucket >= config::dspatch_pred_throttle_bw_thr && candidate == DSPatch_pref_candidate::ACCP)
	{
		bmp_pred.reset();
		stats.gen_pref.reset++;
	}
	
	/* generate prefetch requests */
	for(uint32_t index = 0; index < config::dspatch_num_cachelines_in_region; ++index)
	{
		if(bmp_pred[index] && index != offset)
		{
			uint64_t addr = (page << config::dspatch_log2_region_size) + (index << LOG2_BLOCK_SIZE);
			pref_addr.push_back(addr);
		}
	}
	stats.gen_pref.total += pref_addr.size();
}

void dspatch::update_bw() {
    // Check if the window period (4 * tRC) has passed
    if (current_cycle - last_reset_cycle >= config::WINDOW_SIZE) {
        // Calculate utilization
        double utilization = (double)dram_access_count / config::PEAK_ACCESS_COUNT;

        // Assign to quartile buckets
        if (utilization < 0.25)
            bw_bucket = 0;
        else if (utilization < 0.50)
            bw_bucket = 1;
        else if (utilization < 0.75)
            bw_bucket = 2;
        else
            bw_bucket = 3;

        // Reset for next window
        dram_access_count = 0;
        last_reset_cycle = current_cycle;

		stats.bw.called++;
	 	stats.bw.bw_histogram[bw_bucket]++;
    }
}


void dspatch::record_dram_access() {
    dram_access_count++;
    update_bw();
}

DSPatch_pref_candidate dspatch::dyn_selection(DSPatch_SPTEntry *sptentry, Bitmap &bmp_selected)
{
	stats.dyn_selection.called++;
	DSPatch_pref_candidate candidate = DSPatch_pref_candidate::NONE;

	std::cout<< "bw: " << (int)bw_bucket << std::endl;

	if(bw_bucket == 3)
	{
		if(sptentry->measure_accP.value() == config::dspatch_measure_accP_max)
		{
			/* no prefetch */
			bmp_selected.reset();
			candidate = DSPatch_pref_candidate::NONE;
			stats.dyn_selection.none++;
		}
		else
		{
			/* Prefetch with accP */
			bmp_selected = sptentry->bmp_acc;
			candidate = DSPatch_pref_candidate::ACCP;
			stats.dyn_selection.accp_reason1++;
		}
	}
	else if(bw_bucket == 2)
	{
		if(sptentry->measure_covP.value() == config::dspatch_measure_covP_max)
		{
			/* Prefetch with accP */
			bmp_selected = sptentry->bmp_acc;
			candidate = DSPatch_pref_candidate::ACCP;			
			stats.dyn_selection.accp_reason2++;
		}
		else
		{
			/* Prefetch with covP */
			bmp_selected = sptentry->bmp_cov;
			candidate = DSPatch_pref_candidate::COVP;
			stats.dyn_selection.covp_reason1++;
		}
	}
	else
	{
		/* Prefetch with covP */
		bmp_selected = sptentry->bmp_cov;
		candidate = DSPatch_pref_candidate::COVP;		
		stats.dyn_selection.covp_reason2++;
	}

	return candidate;
}

uint32_t dspatch::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                      uint32_t metadata_in)
{
	std::vector<uint64_t> pref_addr;
	uint64_t addrsss = static_cast<uint64_t>(addr.to<uint64_t>());
	uint64_t ins = static_cast<uint64_t>(ip.to<uint64_t>());
	uint8_t t = (uint8_t)type;

	invoke_prefetcher(ins, addrsss, cache_hit, t, pref_addr);
	
	for(uint32_t index = 0; index < pref_addr.size(); ++index)
	{
		prefetch_line(champsim::address{pref_addr[index]}, true, metadata_in);
	}
	return metadata_in;
}

uint32_t dspatch::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
	record_dram_access();
  	return metadata_in;
}

void dspatch::prefetcher_cycle_operate(){
	current_cycle++;
}

void dspatch::prefetcher_initialize(){
	init_config();
	init_stats();

	/* init cycle number to zero*/
	current_cycle = 0;

	/* init bw to lowest value */
	bw_bucket = 0;

	/* init SPT */
	spt = (DSPatch_SPTEntry**)calloc(config::dspatch_num_spt_entries, sizeof(DSPatch_SPTEntry**));
	assert(spt);
	for(uint32_t index = 0; index < config::dspatch_num_spt_entries; ++index)
	{
		spt[index] = new DSPatch_SPTEntry();
	}
}
