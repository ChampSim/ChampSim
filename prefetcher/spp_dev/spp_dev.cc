#include "spp_dev.h"

#include <cassert>
#include <iostream>

void spp_dev::prefetcher_initialize()
{
  std::cout << "Initialize SIGNATURE TABLE" << std::endl;
  std::cout << "ST_SET: " << ST_SET << std::endl;
  std::cout << "ST_WAY: " << ST_WAY << std::endl;
  std::cout << "ST_TAG_BIT: " << ST_TAG_BIT << std::endl;

  std::cout << std::endl << "Initialize PATTERN TABLE" << std::endl;
  std::cout << "PT_SET: " << PT_SET << std::endl;
  std::cout << "PT_WAY: " << PT_WAY << std::endl;
  std::cout << "SIG_DELTA_BIT: " << SIG_DELTA_BIT << std::endl;
  std::cout << "C_SIG_BIT: " << C_SIG_BIT << std::endl;
  std::cout << "C_DELTA_BIT: " << C_DELTA_BIT << std::endl;

  std::cout << std::endl << "Initialize PREFETCH FILTER" << std::endl;
  std::cout << "FILTER_SET: " << FILTER_SET << std::endl;

  // pass pointers
  ST._parent = this;
  PT._parent = this;
  FILTER._parent = this;
  GHR._parent = this;
}

void spp_dev::prefetcher_cycle_operate() {}

uint32_t spp_dev::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                           uint32_t metadata_in)
{
  champsim::page_number page{addr};
  uint32_t last_sig = 0, curr_sig = 0, depth = 0;
  std::vector<uint32_t> confidence_q(intern_->MSHR_SIZE);

  typename spp_dev::offset_type::difference_type delta = 0;
  std::vector<typename spp_dev::offset_type::difference_type> delta_q(intern_->MSHR_SIZE);

  for (uint32_t i = 0; i < intern_->MSHR_SIZE; i++) {
    confidence_q[i] = 0;
    delta_q[i] = 0;
  }
  confidence_q[0] = 100;
  GHR.global_accuracy = GHR.pf_issued ? ((100 * GHR.pf_useful) / GHR.pf_issued) : 0;

  if constexpr (SPP_DEBUG_PRINT) {
    std::cout << std::endl << "[ChampSim] " << __func__ << " addr: " << addr;
    std::cout << " page: " << page << std::endl;
  }

  // Stage 1: Read and update a sig stored in ST
  // last_sig and delta are used to update (sig, delta) correlation in PT
  // curr_sig is used to read prefetch candidates in PT
  ST.read_and_update_sig(addr, last_sig, curr_sig, delta);

  // Also check the prefetch filter in parallel to update global accuracy counters
  FILTER.check(addr, spp_dev::L2C_DEMAND);

  // Stage 2: Update delta patterns stored in PT
  if (last_sig)
    PT.update_pattern(last_sig, delta);

  // Stage 3: Start prefetching
  auto base_addr = addr;
  uint32_t lookahead_conf = 100, pf_q_head = 0, pf_q_tail = 0;
  uint8_t do_lookahead = 0;

  do {
    uint32_t lookahead_way = PT_WAY;
    PT.read_pattern(curr_sig, delta_q, confidence_q, lookahead_way, lookahead_conf, pf_q_tail, depth);

    do_lookahead = 0;
    for (uint32_t i = pf_q_head; i < pf_q_tail; i++) {
      if (confidence_q[i] >= PF_THRESHOLD) {
        champsim::address pf_addr{champsim::block_number{base_addr} + delta_q[i]};

        if (champsim::page_number{pf_addr} == page) { // Prefetch request is in the same physical page
          if (FILTER.check(pf_addr, ((confidence_q[i] >= FILL_THRESHOLD) ? spp_dev::SPP_L2C_PREFETCH : spp_dev::SPP_LLC_PREFETCH))) {
            prefetch_line(pf_addr, (confidence_q[i] >= FILL_THRESHOLD), 0); // Use addr (not base_addr) to obey the same physical page boundary

            if (confidence_q[i] >= FILL_THRESHOLD) {
              GHR.pf_issued++;
              if (GHR.pf_issued > GLOBAL_COUNTER_MAX) {
                GHR.pf_issued >>= 1;
                GHR.pf_useful >>= 1;
              }
              if constexpr (SPP_DEBUG_PRINT) {
                std::cout << "[ChampSim] SPP L2 prefetch issued GHR.pf_issued: " << GHR.pf_issued << " GHR.pf_useful: " << GHR.pf_useful << std::endl;
              }
            }

            if constexpr (SPP_DEBUG_PRINT) {
              std::cout << "[ChampSim] " << __func__ << " base_addr: " << base_addr << " pf_addr: " << pf_addr;
              std::cout << " prefetch_delta: " << delta_q[i] << " confidence: " << confidence_q[i];
              std::cout << " depth: " << i << std::endl;
            }
          }
        } else { // Prefetch request is crossing the physical page boundary
          if constexpr (GHR_ON) {
            // Store this prefetch request in GHR to bootstrap SPP learning when
            // we see a ST miss (i.e., accessing a new page)
            GHR.update_entry(curr_sig, confidence_q[i], spp_dev::offset_type{pf_addr}, delta_q[i]);
          }
        }

        do_lookahead = 1;
        pf_q_head++;
      }
    }

    // Update base_addr and curr_sig
    if (lookahead_way < PT_WAY) {
      uint32_t set = get_hash(curr_sig) % PT_SET;
      base_addr += (PT.delta[set][lookahead_way] << LOG2_BLOCK_SIZE);

      // PT.delta uses a 7-bit sign magnitude representation to generate
      // sig_delta
      // int sig_delta = (PT.delta[set][lookahead_way] < 0) ? ((((-1) *
      // PT.delta[set][lookahead_way]) & 0x3F) + 0x40) :
      // PT.delta[set][lookahead_way];
      auto sig_delta = (PT.delta[set][lookahead_way] < 0) ? (((-1) * PT.delta[set][lookahead_way]) + (1 << (SIG_DELTA_BIT - 1))) : PT.delta[set][lookahead_way];
      curr_sig = ((curr_sig << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
    }

    if constexpr (SPP_DEBUG_PRINT) {
      std::cout << "Looping curr_sig: " << std::hex << curr_sig << " base_addr: " << base_addr << std::dec;
      std::cout << " pf_q_head: " << pf_q_head << " pf_q_tail: " << pf_q_tail << " depth: " << depth << std::endl;
    }
  } while (LOOKAHEAD_ON && do_lookahead);

  return metadata_in;
}

uint32_t spp_dev::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  if constexpr (FILTER_ON) {
    if constexpr (SPP_DEBUG_PRINT) {
      std::cout << std::endl;
    }
    FILTER.check(evicted_addr, spp_dev::L2C_EVICT);
  }

  return metadata_in;
}

void spp_dev::prefetcher_final_stats() {}

// TODO: Find a good 64-bit hash function
uint64_t spp_dev::get_hash(uint64_t key)
{
  // Robert Jenkins' 32 bit mix function
  key += (key << 12);
  key ^= (key >> 22);
  key += (key << 4);
  key ^= (key >> 9);
  key += (key << 10);
  key ^= (key >> 2);
  key += (key << 7);
  key ^= (key >> 12);

  // Knuth's multiplicative method
  key = (key >> 3) * 2654435761;

  return key;
}

void spp_dev::SIGNATURE_TABLE::read_and_update_sig(champsim::address addr, uint32_t& last_sig, uint32_t& curr_sig, typename offset_type::difference_type& delta)
{
  auto set = get_hash(champsim::page_number{addr}.to<uint64_t>()) % ST_SET;
  auto match = ST_WAY;
  tag_type partial_page{addr};
  offset_type page_offset{addr};
  uint8_t ST_hit = 0;
  long sig_delta = 0;

  if constexpr (SPP_DEBUG_PRINT) {
    std::cout << "[ST] " << __func__ << " page: " << champsim::page_number{addr} << " partial_page: " << std::hex << partial_page << std::dec << std::endl;
  }

  // Case 2: Invalid
  if (match == ST_WAY) {
    for (match = 0; match < ST_WAY; match++) {
      if (valid[set][match] && (tag[set][match] == partial_page)) {
        last_sig = sig[set][match];
        delta = champsim::offset(last_offset[set][match], page_offset);

        if (delta) {
          // Build a new sig based on 7-bit sign magnitude representation of delta
          // sig_delta = (delta < 0) ? ((((-1) * delta) & 0x3F) + 0x40) : delta;
          sig_delta = (delta < 0) ? (((-1) * delta) + (1 << (SIG_DELTA_BIT - 1))) : delta;
          sig[set][match] = ((last_sig << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
          curr_sig = sig[set][match];
          last_offset[set][match] = page_offset;

          if constexpr (SPP_DEBUG_PRINT) {
            std::cout << "[ST] " << __func__ << " hit set: " << set << " way: " << match;
            std::cout << " valid: " << valid[set][match] << " tag: " << std::hex << tag[set][match];
            std::cout << " last_sig: " << last_sig << " curr_sig: " << curr_sig;
            std::cout << " delta: " << std::dec << delta << " last_offset: " << page_offset << std::endl;
          }
        } else
          last_sig = 0; // Hitting the same cache line, delta is zero

        ST_hit = 1;
        break;
      }
    }
  }

  // Case 2: Invalid
  if (match == ST_WAY) {
    for (match = 0; match < ST_WAY; match++) {
      if (valid[set][match] == 0) {
        valid[set][match] = 1;
        tag[set][match] = partial_page;
        sig[set][match] = 0;
        curr_sig = sig[set][match];
        last_offset[set][match] = page_offset;

        if constexpr (SPP_DEBUG_PRINT) {
          std::cout << "[ST] " << __func__ << " invalid set: " << set << " way: " << match;
          std::cout << " valid: " << valid[set][match] << " tag: " << std::hex << partial_page;
          std::cout << " sig: " << sig[set][match] << " last_offset: " << std::dec << page_offset << std::endl;
        }

        break;
      }
    }
  }

  if constexpr (SPP_SANITY_CHECK) {
    // Assertion
    if (match == ST_WAY) {
      for (match = 0; match < ST_WAY; match++) {
        if (lru[set][match] == ST_WAY - 1) { // Find replacement victim
          tag[set][match] = partial_page;
          sig[set][match] = 0;
          curr_sig = sig[set][match];
          last_offset[set][match] = page_offset;

          if constexpr (SPP_DEBUG_PRINT) {
            std::cout << "[ST] " << __func__ << " miss set: " << set << " way: " << match;
            std::cout << " valid: " << valid[set][match] << " victim tag: " << std::hex << tag[set][match] << " new tag: " << partial_page;
            std::cout << " sig: " << sig[set][match] << " last_offset: " << std::dec << page_offset << std::endl;
          }

          break;
        }
      }

      // Assertion
      if (match == ST_WAY) {
        std::cout << "[ST] Cannot find a replacement victim!" << std::endl;
        assert(0);
      }
    }
  }

  if constexpr (GHR_ON) {
    if (ST_hit == 0) {
      uint32_t GHR_found = _parent->GHR.check_entry(page_offset);
      if (GHR_found < MAX_GHR_ENTRY) {
        sig_delta = (_parent->GHR.delta[GHR_found] < 0) ? (((-1) * _parent->GHR.delta[GHR_found]) + (1 << (SIG_DELTA_BIT - 1))) : _parent->GHR.delta[GHR_found];
        sig[set][match] = ((_parent->GHR.sig[GHR_found] << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
        curr_sig = sig[set][match];
      }
    }
  }

  // Update LRU
  for (uint32_t way = 0; way < ST_WAY; way++) {
    if (lru[set][way] < lru[set][match]) {
      lru[set][way]++;

      if constexpr (SPP_SANITY_CHECK) {
        // Assertion
        if (lru[set][way] >= ST_WAY) {
          std::cout << "[ST] LRU value is wrong! set: " << set << " way: " << way << " lru: " << lru[set][way] << std::endl;
          assert(0);
        }
      }
    }
  }

  lru[set][match] = 0; // Promote to the MRU position
}

void spp_dev::PATTERN_TABLE::update_pattern(uint32_t last_sig, typename offset_type::difference_type curr_delta)
{
  // Update (sig, delta) correlation
  uint32_t set = get_hash(last_sig) % PT_SET, match = 0;

  // Case 1: Hit
  for (match = 0; match < PT_WAY; match++) {
    if (delta[set][match] == curr_delta) {
      c_delta[set][match]++;
      c_sig[set]++;
      if (c_sig[set] > C_SIG_MAX) {
        for (uint32_t way = 0; way < PT_WAY; way++)
          c_delta[set][way] >>= 1;
        c_sig[set] >>= 1;
      }

      if constexpr (SPP_DEBUG_PRINT) {
        std::cout << "[PT] " << __func__ << " hit sig: " << std::hex << last_sig << std::dec << " set: " << set << " way: " << match;
        std::cout << " delta: " << delta[set][match] << " c_delta: " << c_delta[set][match] << " c_sig: " << c_sig[set] << std::endl;
      }

      break;
    }
  }

  // Case 2: Miss
  if (match == PT_WAY) {
    uint32_t victim_way = PT_WAY, min_counter = C_SIG_MAX;

    for (match = 0; match < PT_WAY; match++) {
      if (c_delta[set][match] < min_counter) { // Select an entry with the minimum c_delta
        victim_way = match;
        min_counter = c_delta[set][match];
      }
    }

    delta[set][victim_way] = curr_delta;
    c_delta[set][victim_way] = 0;
    c_sig[set]++;
    if (c_sig[set] > C_SIG_MAX) {
      for (uint32_t way = 0; way < PT_WAY; way++)
        c_delta[set][way] >>= 1;
      c_sig[set] >>= 1;
    }

    if constexpr (SPP_DEBUG_PRINT) {
      std::cout << "[PT] " << __func__ << " miss sig: " << std::hex << last_sig << std::dec << " set: " << set << " way: " << victim_way;
      std::cout << " delta: " << delta[set][victim_way] << " c_delta: " << c_delta[set][victim_way] << " c_sig: " << c_sig[set] << std::endl;
    }

    if constexpr (SPP_SANITY_CHECK) {
      // Assertion
      if (victim_way == PT_WAY) {
        std::cout << "[PT] Cannot find a replacement victim!" << std::endl;
        assert(0);
      }
    }
  }
}

void spp_dev::PATTERN_TABLE::read_pattern(uint32_t curr_sig, std::vector<typename offset_type::difference_type>& delta_q, std::vector<uint32_t>& confidence_q,
                                          uint32_t& lookahead_way, uint32_t& lookahead_conf, uint32_t& pf_q_tail, uint32_t& depth)
{
  // Update (sig, delta) correlation
  uint32_t set = get_hash(curr_sig) % PT_SET, local_conf = 0, pf_conf = 0, max_conf = 0;

  if (c_sig[set]) {
    for (uint32_t way = 0; way < PT_WAY; way++) {
      local_conf = (100 * c_delta[set][way]) / c_sig[set];
      pf_conf = depth ? (_parent->GHR.global_accuracy * c_delta[set][way] / c_sig[set] * lookahead_conf / 100) : local_conf;

      if (pf_conf >= PF_THRESHOLD) {
        confidence_q[pf_q_tail] = pf_conf;
        delta_q[pf_q_tail] = delta[set][way];

        // Lookahead path follows the most confident entry
        if (pf_conf > max_conf) {
          lookahead_way = way;
          max_conf = pf_conf;
        }
        pf_q_tail++;

        if constexpr (SPP_DEBUG_PRINT) {
          std::cout << "[PT] " << __func__ << " HIGH CONF: " << pf_conf << " sig: " << std::hex << curr_sig << std::dec << " set: " << set << " way: " << way;
          std::cout << " delta: " << delta[set][way] << " c_delta: " << c_delta[set][way] << " c_sig: " << c_sig[set];
          std::cout << " conf: " << local_conf << " depth: " << depth << std::endl;
        }
      } else {
        if constexpr (SPP_DEBUG_PRINT) {
          std::cout << "[PT] " << __func__ << "  LOW CONF: " << pf_conf << " sig: " << std::hex << curr_sig << std::dec << " set: " << set << " way: " << way;
          std::cout << " delta: " << delta[set][way] << " c_delta: " << c_delta[set][way] << " c_sig: " << c_sig[set];
          std::cout << " conf: " << local_conf << " depth: " << depth << std::endl;
        }
      }
    }
    pf_q_tail++;

    lookahead_conf = max_conf;
    if (lookahead_conf >= PF_THRESHOLD)
      depth++;

    if constexpr (SPP_DEBUG_PRINT) {
      std::cout << "global_accuracy: " << _parent->GHR.global_accuracy << " lookahead_conf: " << lookahead_conf << std::endl;
    }
  } else {
    confidence_q[pf_q_tail] = 0;
  }
}

bool spp_dev::PREFETCH_FILTER::check(champsim::address check_addr, FILTER_REQUEST filter_request)
{
  champsim::block_number cache_line{check_addr};
  auto hash = get_hash(cache_line.to<uint64_t>());
  auto quotient = (hash >> REMAINDER_BIT) & ((1 << QUOTIENT_BIT) - 1);
  auto remainder = hash % (1 << REMAINDER_BIT);

  if constexpr (SPP_DEBUG_PRINT) {
    std::cout << "[FILTER] check_addr: " << check_addr << " hash: " << hash << " quotient: " << quotient << " remainder: " << remainder << std::endl;
  }

  switch (filter_request) {
  case spp_dev::SPP_L2C_PREFETCH:
    if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) {
      if constexpr (SPP_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__ << " line is already in the filter check_addr: " << check_addr << " cache_line: " << cache_line;
        std::cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << std::endl;
      }

      return false; // False return indicates "Do not prefetch"
    } else {
      valid[quotient] = 1;  // Mark as prefetched
      useful[quotient] = 0; // Reset useful bit
      remainder_tag[quotient] = remainder;

      if constexpr (SPP_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__ << " set valid for check_addr: " << check_addr << " cache_line: " << cache_line;
        std::cout << " quotient: " << quotient << " remainder_tag: " << remainder_tag[quotient] << " valid: " << valid[quotient]
                  << " useful: " << useful[quotient] << std::endl;
      }
    }
    break;

  case spp_dev::SPP_LLC_PREFETCH:
    if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) {
      if constexpr (SPP_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__ << " line is already in the filter check_addr: " << check_addr << " cache_line: " << cache_line;
        std::cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << std::endl;
      }

      return false; // False return indicates "Do not prefetch"
    } else {
      // NOTE: SPP_LLC_PREFETCH has relatively low confidence (FILL_THRESHOLD <= SPP_LLC_PREFETCH < PF_THRESHOLD)
      // Therefore, it is safe to prefetch this cache line in the large LLC and save precious L2C capacity
      // If this prefetch request becomes more confident and SPP eventually issues SPP_L2C_PREFETCH,
      // we can get this cache line immediately from the LLC (not from DRAM)
      // To allow this fast prefetch from LLC, SPP does not set the valid bit for SPP_LLC_PREFETCH

      // valid[quotient] = 1;
      // useful[quotient] = 0;

      if constexpr (SPP_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__ << " don't set valid for check_addr: " << check_addr << " cache_line: " << cache_line;
        std::cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << std::endl;
      }
    }
    break;

  case spp_dev::L2C_DEMAND:
    if ((remainder_tag[quotient] == remainder) && (useful[quotient] == 0)) {
      useful[quotient] = 1;
      if (valid[quotient])
        _parent->GHR.pf_useful++; // This cache line was prefetched by SPP and actually used in the program

      if constexpr (SPP_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__ << " set useful for check_addr: " << check_addr << " cache_line: " << cache_line;
        std::cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient];
        std::cout << " GHR.pf_issued: " << _parent->GHR.pf_issued << " GHR.pf_useful: " << _parent->GHR.pf_useful << std::endl;
      }
    }
    break;

  case spp_dev::L2C_EVICT:
    // Decrease global pf_useful counter when there is a useless prefetch (prefetched but not used)
    if (valid[quotient] && !useful[quotient] && _parent->GHR.pf_useful)
      _parent->GHR.pf_useful--;

    // Reset filter entry
    valid[quotient] = 0;
    useful[quotient] = 0;
    remainder_tag[quotient] = 0;
    break;

  default:
    // Assertion
    std::cout << "[FILTER] Invalid filter request type: " << filter_request << std::endl;
    assert(0);
  }

  return true;
}

void spp_dev::GLOBAL_REGISTER::update_entry(uint32_t pf_sig, uint32_t pf_confidence, offset_type pf_offset, typename offset_type::difference_type pf_delta)
{
  // NOTE: GHR implementation is slightly different from the original paper
  // Instead of matching (last_offset + delta), GHR simply stores and matches the pf_offset
  uint32_t min_conf = 100, victim_way = MAX_GHR_ENTRY;

  if constexpr (SPP_DEBUG_PRINT) {
    std::cout << "[GHR] Crossing the page boundary pf_sig: " << std::hex << pf_sig << std::dec;
    std::cout << " confidence: " << pf_confidence << " pf_offset: " << pf_offset << " pf_delta: " << pf_delta << std::endl;
  }

  for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
    // if (sig[i] == pf_sig) { // TODO: Which one is better and consistent?
    //  If GHR already holds the same pf_sig, update the GHR entry with the latest info
    if (valid[i] && (offset[i] == pf_offset)) {
      // If GHR already holds the same pf_offset, update the GHR entry with the latest info
      sig[i] = pf_sig;
      confidence[i] = pf_confidence;
      // offset[i] = pf_offset;
      delta[i] = pf_delta;

      if constexpr (SPP_DEBUG_PRINT) {
        std::cout << "[GHR] Found a matching index: " << i << std::endl;
      }

      return;
    }

    // GHR replacement policy is based on the stored confidence value
    // An entry with the lowest confidence is selected as a victim
    if (confidence[i] < min_conf) {
      min_conf = confidence[i];
      victim_way = i;
    }
  }

  // Assertion
  if (victim_way >= MAX_GHR_ENTRY) {
    std::cout << "[GHR] Cannot find a replacement victim!" << std::endl;
    assert(0);
  }

  if constexpr (SPP_DEBUG_PRINT) {
    std::cout << "[GHR] Replace index: " << victim_way << " pf_sig: " << std::hex << sig[victim_way] << std::dec;
    std::cout << " confidence: " << confidence[victim_way] << " pf_offset: " << offset[victim_way] << " pf_delta: " << delta[victim_way] << std::endl;
  }

  valid[victim_way] = 1;
  sig[victim_way] = pf_sig;
  confidence[victim_way] = pf_confidence;
  offset[victim_way] = pf_offset;
  delta[victim_way] = pf_delta;
}

uint32_t spp_dev::GLOBAL_REGISTER::check_entry(offset_type page_offset)
{
  uint32_t max_conf = 0, max_conf_way = MAX_GHR_ENTRY;

  for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
    if ((offset[i] == page_offset) && (max_conf < confidence[i])) {
      max_conf = confidence[i];
      max_conf_way = i;
    }
  }

  return max_conf_way;
}
