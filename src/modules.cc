/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "modules.h"

#include "cache.h"
namespace champsim::modules {

  bool prefetcher::prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const
  {
    return intern_->prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
  }
  bool champsim::modules::prefetcher::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const
  {
    return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
  }

  void prefetcher::prefetcher_initialize_impl() {
    prefetcher_initialize_arb<0>();
  }

  uint32_t prefetcher::prefetcher_cache_operate_impl(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch,
                                            access_type type, uint32_t metadata_in) {
    std::pair<uint32_t,bool> val = prefetcher_cache_operate_arb<0>(addr,ip,cache_hit,useful_prefetch,type,metadata_in);
    if(val.second)
      return val.first;
    
    val = prefetcher_cache_operate_arb<1>(addr,ip,(uint8_t)cache_hit,useful_prefetch,type,metadata_in);
    if(val.second)
      return val.first;

    val = prefetcher_cache_operate_arb<2>(addr,ip,cache_hit,useful_prefetch,champsim::to_underlying(type),metadata_in);
    if(val.second)
      return val.first;
    
    val = prefetcher_cache_operate_arb<3>(addr.to<uint64_t>(), ip.to<uint64_t>(), cache_hit, champsim::to_underlying(type),metadata_in);
    if(val.second)
      return val.first;

    return(metadata_in);
  }

  uint32_t prefetcher::prefetcher_cache_fill_impl(champsim::address addr, long set, long way, bool prefetch, 
                                            champsim::address evicted_addr, uint32_t metadata_in) {

    std::pair<uint32_t,bool> val = prefetcher_cache_fill_arb<0>(addr,set,way,prefetch,evicted_addr,metadata_in);
    if(val.second)
      return val.first;

    val = prefetcher_cache_fill_arb<1>(addr,set,way,(uint8_t)prefetch,evicted_addr,metadata_in);
    if(val.second)
      return val.first;

    val = prefetcher_cache_fill_arb<2>(addr.to<uint64_t>(),set,way,prefetch,evicted_addr.to<uint64_t>(),metadata_in);
    if(val.second)
      return val.first;

   
    return(metadata_in);
  }

  void prefetcher::prefetcher_cycle_operate_impl() {prefetcher_cycle_operate_arb<0>();}
  void prefetcher::prefetcher_final_stats_impl() {prefetcher_final_stats_arb<0>();}

  void prefetcher::prefetcher_branch_operate_impl(champsim::address ip, uint8_t branch_type, champsim::address branch_target) {

    bool val = prefetcher_branch_operate_arb<0>(ip,branch_type,branch_target);
    if(val)
      return;
    val = prefetcher_branch_operate_arb<1>(ip.to<uint64_t>(),branch_type,branch_target.to<uint64_t>());
    if(val)
      return;

  }

  void replacement::initialize_replacement_impl() {initialize_replacement_arb<0>();}

  long replacement::find_victim_impl(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                                  champsim::address full_addr, access_type type) {

    std::pair<long,bool> val = find_victim_arb<0>(triggering_cpu,instr_id,set,current_set,ip,full_addr,type);
    if(val.second)
      return(val.first);
    val = find_victim_arb<1>(triggering_cpu,instr_id,set,current_set,ip,full_addr,champsim::to_underlying(type));
    if(val.second)
      return(val.first);
    val = find_victim_arb<2>(triggering_cpu,instr_id,set,current_set,ip.to<uint64_t>(),full_addr.to<uint64_t>(),type);
    if(val.second)
      return(val.first);
    val = find_victim_arb<3>(triggering_cpu,instr_id,set,current_set,ip.to<uint64_t>(),full_addr.to<uint64_t>(),champsim::to_underlying(type));
    if(val.second)
      return(val.first);

    //we should never get here
    return(-1);
  }

  void replacement::update_replacement_state_impl(uint32_t triggering_cpu, long set, long way, champsim::address full_addr,
                                              champsim::address ip, champsim::address victim_addr, access_type type, bool hit) {
    if(hit || is_any_active(replacement_interface::CACHE_FILL)) {
      auto new_victim_addr =  hit ? champsim::address{} : victim_addr;

      bool val = update_replacement_state_arb<0>(triggering_cpu,set,way,full_addr,ip,type,hit);
      if(val)
        return;
      val = update_replacement_state_arb<1>(triggering_cpu,set,way,full_addr,ip,new_victim_addr,type,hit);
      if(val)
        return;
      val = update_replacement_state_arb<2>(triggering_cpu,set,way,full_addr,ip,new_victim_addr,type,(uint8_t)hit);
      if(val)
        return;
      val = update_replacement_state_arb<3>(triggering_cpu,set,way,full_addr,ip,new_victim_addr,champsim::to_underlying(type),hit);
      if(val)
        return;
      val = update_replacement_state_arb<4>(triggering_cpu,set,way,full_addr.to<uint64_t>(),ip.to<uint64_t>(),new_victim_addr.to<uint64_t>(),champsim::to_underlying(type),hit);
    }
  }

  void replacement::replacement_cache_fill_impl(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, 
                                              champsim::address ip, champsim::address victim_addr, access_type type) {

    if (!replacement_cache_fill_arb<0>(triggering_cpu,set,way,full_addr,ip,victim_addr,type))
      intern_->impl_update_replacement_state(triggering_cpu,set,way,full_addr,ip,victim_addr,type,false);
  }

  void replacement::replacement_final_stats_impl() {replacement_final_stats_arb<0>();}

  
  void branch_predictor::initialize_branch_predictor_impl(){initialize_branch_predictor_arb<0>();}

  void branch_predictor::last_branch_result_impl(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type) {

    bool val = last_branch_result_arb<0>(ip,target,taken,branch_type);
    if(val)
      return;
    val = last_branch_result_arb<1>(ip.to<uint64_t>(),target.to<uint64_t>(),taken,branch_type);
  }

  bool branch_predictor::predict_branch_impl(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type) {
    std::pair<bool,bool> val = predict_branch_arb<0>(ip,predicted_target,always_taken,branch_type);
    if(val.second)
      return val.first;
    val = predict_branch_arb<1>(ip);
    if(val.second)
      return val.first;
    val = predict_branch_arb<2>(ip.to<uint64_t>(),predicted_target.to<uint64_t>(),always_taken,branch_type);
    if(val.second)
      return val.first;
    val = predict_branch_arb<3>(ip.to<uint64_t>());
    if(val.second)
      return val.first;
    

    return false;
  }


  void btb::initialize_btb_impl(){initialize_btb_arb<0>();}

  void btb::update_btb_impl(champsim::address ip, champsim::address predicted_target, bool taken, uint8_t branch_type) {

    bool val = update_btb_arb<0>(ip,predicted_target,taken,branch_type);
    if(val)
      return;
    val = update_btb_arb<1>(ip.to<uint64_t>(),predicted_target.to<uint64_t>(),taken,branch_type);
  }

  std::pair<champsim::address, bool> btb::btb_prediction_impl(champsim::address ip, uint8_t branch_type) {
    std::pair<champsim::address,bool> result{};
    
    std::pair<std::pair<champsim::address,bool>,bool> val = btb_prediction_arb<0>(ip,branch_type);
    if(val.second)
      return val.first;
    val = btb_prediction_arb<1>(ip);
    if(val.second)
      return val.first;
    val = btb_prediction_arb<2>(ip.to<uint64_t>(),branch_type);
    if(val.second)
      return val.first;
    val = btb_prediction_arb<3>(ip.to<uint64_t>());
    if(val.second)
      return val.first;
    
    return std::pair<champsim::address,bool>{};
  }

}
// LCOV_EXCL_STOP
