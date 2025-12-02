#include "CARE.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>

#include "champsim.h"

namespace
{
constexpr double low_threshold_ratio = 0.005;
constexpr double high_threshold_ratio = 0.05;
}

CARE::CARE(CACHE* cache)
    : replacement(cache),
      num_sets(cache->NUM_SET),
      num_ways(cache->NUM_WAY),
      blocks(static_cast<std::size_t>(cache->NUM_SET * cache->NUM_WAY)),
      sampled_sets(static_cast<std::size_t>(cache->NUM_SET), false),
      sht(NUM_CPUS)
{
  // Initialize the SHT entries to a neutral state so the first few insertions do not
  // bias the predictor toward either extreme reuse or cost. CARE uses 3-bit counters
  // for RC and PD, so the midpoint provides a balanced starting prediction.
  for (auto& cpu_table : sht) {
    for (auto& entry : cpu_table) {
      auto rc_mid = (entry.rc.maximum + entry.rc.minimum) / 2;
      auto pd_mid = (entry.pd.maximum + entry.pd.minimum) / 2;
      entry.rc = rc_mid;
      entry.pd = pd_mid;
    }
  }

  std::vector<long> set_ids(static_cast<std::size_t>(num_sets));
  std::iota(set_ids.begin(), set_ids.end(), 0);
  std::mt19937_64 rng{1};
  std::shuffle(set_ids.begin(), set_ids.end(), rng);

  auto sample_count = std::max<std::size_t>(1, static_cast<std::size_t>(num_sets) / sample_divisor);
  sample_count = std::min<std::size_t>(sample_count, set_ids.size());
  for (std::size_t i = 0; i < sample_count; ++i)
    sampled_sets[static_cast<std::size_t>(set_ids[i])] = true;
}

std::size_t CARE::block_index(long set, long way) const
{
  return static_cast<std::size_t>(set) * static_cast<std::size_t>(num_ways) +
         static_cast<std::size_t>(way);
}

bool CARE::is_sampled(long set) const
{
  auto s = static_cast<std::size_t>(set);
  return s < sampled_sets.size() && sampled_sets[s];
}

uint16_t CARE::make_signature(champsim::address ip, bool is_prefetch) const
{
  constexpr uint16_t base_mask = static_cast<uint16_t>((std::size_t{1} << (signature_bits - 1)) - 1);
  auto raw = ip.to<uint64_t>();
  auto hashed = static_cast<uint16_t>(((raw >> 2) ^ raw) & base_mask);
  return static_cast<uint16_t>((hashed << 1) | static_cast<uint16_t>(is_prefetch));
}

std::size_t CARE::signature_index(uint16_t signature) const
{
  return static_cast<std::size_t>(signature) & (sht_entries - 1);
}

CARE::sht_entry& CARE::entry_for(uint32_t cpu, uint16_t signature)
{
  if (sht.empty())
    throw std::out_of_range("SHT not initialized");
  auto cpu_index = std::min<std::size_t>(cpu, sht.size() - 1);
  return sht.at(cpu_index).at(signature_index(signature));
}

const CARE::sht_entry& CARE::entry_for(uint32_t cpu, uint16_t signature) const
{
  if (sht.empty())
    throw std::out_of_range("SHT not initialized");
  auto cpu_index = std::min<std::size_t>(cpu, sht.size() - 1);
  return sht.at(cpu_index).at(signature_index(signature));
}

CARE::reuse_prediction CARE::predict_reuse(uint32_t cpu, uint16_t signature) const
{
  const auto& entry = entry_for(cpu, signature);
  if (entry.rc.value() == entry.rc.minimum)
    return reuse_prediction::low;
  if (entry.rc.value() == entry.rc.maximum)
    return reuse_prediction::high;
  return reuse_prediction::moderate;
}

CARE::cost_prediction CARE::predict_cost(uint32_t cpu, uint16_t signature) const
{
  const auto& entry = entry_for(cpu, signature);
  if (entry.pd.value() == entry.pd.minimum)
    return cost_prediction::low;
  if (entry.pd.value() == entry.pd.maximum)
    return cost_prediction::high;
  return cost_prediction::medium;
}

uint8_t CARE::select_epv(reuse_prediction reuse, cost_prediction cost, access_type type) const
{
  if (access_type{type} == access_type::WRITE)
    return max_epv;

  if (reuse == reuse_prediction::high)
    return 0;

  if (reuse == reuse_prediction::low)
    return max_epv;

  if (cost == cost_prediction::low)
    return max_epv;
  if (cost == cost_prediction::high)
    return 0;
  return 2;
}

uint8_t CARE::quantize(double pmc, double low, double high) const
{
  if (pmc <= low)
    return low_pmcs_state;
  if (pmc >= high)
    return high_pmcs_state;

  const auto midpoint = (low + high) / 2.0;
  return (pmc < midpoint) ? 1 : 2;
}

void CARE::update_dtrm(bool costly)
{
  ++miss_counter;
  if (costly)
    ++costly_counter;

  if (miss_counter < dtrm_period)
    return;

  const auto low_threshold = static_cast<std::size_t>(low_threshold_ratio * static_cast<double>(dtrm_period));
  const auto high_threshold = static_cast<std::size_t>(high_threshold_ratio * static_cast<double>(dtrm_period));

  if (costly_counter < low_threshold) {
    pmc_low = std::max(0.0, pmc_low - 10.0);
    pmc_high = std::max(pmc_low + 1.0, pmc_high - 70.0);
  } else if (costly_counter > high_threshold) {
    pmc_low += 10.0;
    pmc_high = std::max(pmc_low + 1.0, pmc_high + 70.0);
  }

  miss_counter = 0;
  costly_counter = 0;
}

void CARE::update_history_on_eviction(std::size_t block_idx)
{
  if (block_idx >= blocks.size())
    return;

  auto set = static_cast<long>(block_idx / static_cast<std::size_t>(num_ways));
  if (!is_sampled(set))
    return;

  auto& meta = blocks.at(block_idx);
  if (!meta.valid)
    return;

  auto& entry = entry_for(meta.owner_cpu, meta.signature);
  if (!meta.referenced)
    entry.rc -= 1;
  if (meta.pmcs == low_pmcs_state)
    entry.pd -= 1;
  else if (meta.pmcs == high_pmcs_state)
    entry.pd += 1;

  meta.valid = false;
}

long CARE::find_victim(uint32_t,
                      uint64_t,
                      long set,
                      const champsim::cache_block* current_set,
                      champsim::address,
                      champsim::address,
                      access_type)
{
  for (long way = 0; way < num_ways; ++way) {
    if (!current_set[way].valid)
      return way;
  }

  const auto begin = blocks.begin() + static_cast<std::ptrdiff_t>(set * num_ways);
  const auto end = begin + num_ways;

  while (true) {
    auto victim = std::find_if(begin, end, [](const auto& meta) { return meta.epv == max_epv; });
    if (victim != end)
      return std::distance(begin, victim);

    std::for_each(begin, end, [](auto& meta) {
      if (meta.epv < max_epv)
        ++meta.epv;
    });
  }
}

void CARE::update_replacement_state(uint32_t triggering_cpu,
                                   long set,
                                   long way,
                                   champsim::address,
                                   champsim::address,
                                   champsim::address,
                                   access_type type,
                                   uint8_t hit)
{
  if (!hit)
    return;
  if (access_type{type} == access_type::WRITE)
    return;
  if (access_type{type} == access_type::PREFETCH)
    return;

  auto idx = block_index(set, way);
  auto& meta = blocks.at(idx);
  meta.owner_cpu = triggering_cpu;

  if (!meta.referenced) {
    meta.referenced = true;
    if (is_sampled(set)) {
      auto& entry = entry_for(triggering_cpu, meta.signature);
      entry.rc += 1;
    }
  }

  if (meta.prefetched) {
    meta.prefetched = false;
    meta.epv = max_epv;
    return;
  }

  const auto reuse = predict_reuse(triggering_cpu, meta.signature);
  if (reuse == reuse_prediction::low) {
    if (meta.epv > 0)
      --meta.epv;
  } else {
    meta.epv = 0;
  }
}

void CARE::replacement_cache_fill(uint32_t triggering_cpu,
                                 long set,
                                 long way,
                                 champsim::address,
                                 champsim::address ip,
                                 champsim::address,
                                 access_type type,
                                 double pmc)
{
  auto idx = block_index(set, way);
  update_history_on_eviction(idx);

  auto signature = make_signature(ip, access_type{type} == access_type::PREFETCH);
  auto pmc_low_snapshot = pmc_low;
  auto pmc_high_snapshot = pmc_high;
  auto pmcs_value = quantize(pmc, pmc_low_snapshot, pmc_high_snapshot);
  update_dtrm(pmc > pmc_high_snapshot);

  auto reuse = predict_reuse(triggering_cpu, signature);
  auto cost = predict_cost(triggering_cpu, signature);
  auto epv_value = select_epv(reuse, cost, type);

  auto& meta = blocks.at(idx);
  meta.valid = true;
  meta.epv = std::min<uint8_t>(epv_value, max_epv);
  meta.pmcs = pmcs_value;
  meta.referenced = false;
  meta.prefetched = (access_type{type} == access_type::PREFETCH);
  meta.signature = signature;
  meta.owner_cpu = triggering_cpu;
}
