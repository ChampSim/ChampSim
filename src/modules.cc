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

  void prefetcher::prefetcher_initialize_impl() {if (prefetcher_initialize_used) prefetcher_initialize();}
  uint32_t prefetcher::prefetcher_cache_operate_impl(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch,
                                            access_type type, uint32_t metadata_in) {
    if(prefetcher_cache_operate0_used) {
      uint32_t meta = prefetcher_cache_operate(addr,ip,cache_hit,useful_prefetch,type,metadata_in);
      if(prefetcher_cache_operate0_used)
        return meta;
    }    
    if(prefetcher_cache_operate1_used) {
      uint32_t meta = prefetcher_cache_operate(addr,ip,(uint8_t)cache_hit,useful_prefetch,type,metadata_in);
      if(prefetcher_cache_operate1_used)
        return meta;
    }
    if(prefetcher_cache_operate2_used) {
      uint32_t meta = prefetcher_cache_operate(addr,ip,cache_hit,useful_prefetch,champsim::to_underlying(type),metadata_in);
      if(prefetcher_cache_operate2_used)
        return meta;
    }
    if(prefetcher_cache_operate3_used) {
      uint32_t meta =  prefetcher_cache_operate(addr.to<uint64_t>(), ip.to<uint64_t>(), cache_hit, champsim::to_underlying(type),metadata_in);
      if(prefetcher_cache_operate3_used)
        return meta;
    }
    return(metadata_in);
  }

  uint32_t prefetcher::prefetcher_cache_fill_impl(champsim::address addr, long set, long way, bool prefetch, 
                                            champsim::address evicted_addr, uint32_t metadata_in) {
    if(prefetcher_cache_fill0_used) {
      uint32_t meta = prefetcher_cache_fill(addr,set,way,prefetch,evicted_addr,metadata_in);
      if(prefetcher_cache_fill0_used)
        return meta;
    }
    if(prefetcher_cache_fill1_used) {
      uint32_t meta = prefetcher_cache_fill(addr,set,way,(uint8_t)prefetch,evicted_addr,metadata_in);
      if(prefetcher_cache_fill1_used)
        return meta;
    }
    if(prefetcher_cache_fill2_used) {
      uint32_t meta = prefetcher_cache_fill(addr.to<uint64_t>(),set,way,prefetch,evicted_addr.to<uint64_t>(),metadata_in);
      if(prefetcher_cache_fill2_used)
        return meta;
    }
    return(metadata_in);
  }

  void prefetcher::prefetcher_cycle_operate_impl() {if(prefetcher_cycle_operate_used) prefetcher_cycle_operate();}
  void prefetcher::prefetcher_final_stats_impl() {if(prefetcher_final_stats_used) prefetcher_final_stats();}

  void prefetcher::prefetcher_branch_operate_impl(champsim::address ip, uint8_t branch_type, champsim::address branch_target) {
    if(prefetcher_branch_operate0_used) {
      prefetcher_branch_operate(ip,branch_type,branch_target);
      if(prefetcher_branch_operate0_used)
        return;
    }
    if(prefetcher_branch_operate1_used) {
      prefetcher_branch_operate(ip.to<uint64_t>(),branch_type,branch_target.to<uint64_t>());
      if(prefetcher_branch_operate1_used)
        return;
    }
  }

  void replacement::initialize_replacement_impl() {initialize_replacement();}

  long replacement::find_victim_impl(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                                  champsim::address full_addr, access_type type) {
    assert(replacement_find_victim0_used || replacement_find_victim1_used || replacement_find_victim2_used);

    if(replacement_find_victim0_used){
      long temp_victim = find_victim(triggering_cpu,instr_id,set,current_set,ip,full_addr,type);
      if(replacement_find_victim0_used)
        return temp_victim;
    }
    if(replacement_find_victim1_used){
      long temp_victim = find_victim(triggering_cpu,instr_id,set,current_set,ip,full_addr,champsim::to_underlying(type));
      if(replacement_find_victim1_used)
        return temp_victim;
    } 
    if(replacement_find_victim2_used){
      long temp_victim = find_victim(triggering_cpu,instr_id,set,current_set,ip.to<uint64_t>(),full_addr.to<uint64_t>(),type);
      if(replacement_find_victim2_used)
        return temp_victim;
    }
    if(replacement_find_victim3_used){
      long temp_victim = find_victim(triggering_cpu,instr_id,set,current_set,ip.to<uint64_t>(),full_addr.to<uint64_t>(),champsim::to_underlying(type));
      if(replacement_find_victim3_used)
        return temp_victim;
    }

    return(-1);
  }

  void replacement::update_replacement_state_impl(uint32_t triggering_cpu, long set, long way, champsim::address full_addr,
                                              champsim::address ip, champsim::address victim_addr, access_type type, bool hit) {
    if(hit || replacement_cache_fill_used) {
      auto new_victim_addr =  hit ? champsim::address{} : victim_addr;
      if(replacement_update_replacement_state0_used) {
        update_replacement_state(triggering_cpu,set,way,full_addr,ip,type,hit);
        if(replacement_update_replacement_state0_used)
          return;
      }
      if(replacement_update_replacement_state1_used) {
        update_replacement_state(triggering_cpu,set,way,full_addr,ip,new_victim_addr,type,hit);
        if(replacement_update_replacement_state1_used)
          return;
      }
      if(replacement_update_replacement_state2_used) {
        update_replacement_state(triggering_cpu,set,way,full_addr,ip,new_victim_addr,type,(uint8_t)hit);
        if(replacement_update_replacement_state2_used)
          return;
      }
      if(replacement_update_replacement_state3_used) {
        update_replacement_state(triggering_cpu,set,way,full_addr,ip,new_victim_addr,champsim::to_underlying(type),hit);
        if(replacement_update_replacement_state3_used)
          return;
      }
      if(replacement_update_replacement_state4_used) {
        update_replacement_state(triggering_cpu,set,way,full_addr.to<uint64_t>(),ip.to<uint64_t>(),new_victim_addr.to<uint64_t>(),champsim::to_underlying(type),hit);
        if(replacement_update_replacement_state4_used)
          return;
      }
    }
  }

  void replacement::replacement_cache_fill_impl(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, 
                                              champsim::address ip, champsim::address victim_addr, access_type type) {
    //always call, need to call update_replacement_state
    if(replacement_cache_fill_used) {
      replacement_cache_fill(triggering_cpu,set,way,full_addr,ip,victim_addr,type);
      if(replacement_cache_fill_used)
      return;
    }
    if(!replacement_cache_fill_used) {
      intern_->impl_update_replacement_state(triggering_cpu,set,way,full_addr,ip,victim_addr,type,false);
      return;
    }
  }

  void replacement::replacement_final_stats_impl() {if (replacement_final_stats_used) replacement_final_stats();}

  
  void branch_predictor::initialize_branch_predictor_impl(){if(branch_predictor_initialize_used) initialize_branch_predictor();}

  void branch_predictor::last_branch_result_impl(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type) {
    if(branch_predictor_last_branch_result0_used) {
      last_branch_result(ip,target,taken,branch_type);
      if(branch_predictor_last_branch_result0_used)
        return;
    }
    if(branch_predictor_last_branch_result1_used) {
      last_branch_result(ip.to<uint64_t>(),target.to<uint64_t>(),taken,branch_type);
      if(branch_predictor_last_branch_result1_used)
        return;
    }
  }

  bool branch_predictor::predict_branch_impl(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type) {
    if(branch_predictor_predict_branch0_used) {
      bool prediction = predict_branch(ip,predicted_target,always_taken,branch_type);
      if(branch_predictor_predict_branch0_used)
        return prediction;
    }
    if(branch_predictor_predict_branch1_used) {
      bool prediction = predict_branch(ip);
      if(branch_predictor_predict_branch1_used)
        return prediction;
    }
    if(branch_predictor_predict_branch2_used) {
      bool prediction = predict_branch(ip.to<uint64_t>(),predicted_target.to<uint64_t>(),always_taken,branch_type);
      if(branch_predictor_predict_branch2_used)
        return prediction;
    }
    if(branch_predictor_predict_branch3_used) {
      bool prediction = predict_branch(ip.to<uint64_t>());
      if(branch_predictor_predict_branch3_used)
        return prediction;
    }
    return false;
  }


  void btb::initialize_btb_impl(){initialize_btb();}

  void btb::update_btb_impl(champsim::address ip, champsim::address predicted_target, bool taken, uint8_t branch_type) {
    if(btb_update_btb0_used) {
      update_btb(ip,predicted_target,taken,branch_type);
      if(btb_update_btb0_used)
        return;
    }
    if(btb_update_btb1_used) {
      update_btb(ip.to<uint64_t>(),predicted_target.to<uint64_t>(),taken,branch_type);
      if(btb_update_btb1_used)
        return;
    }
  }

  std::pair<champsim::address, bool> btb::btb_prediction_impl(champsim::address ip, uint8_t branch_type) {
    std::pair<champsim::address,bool> result{};
    
    if(btb_btb_prediction0_used) {
      std::pair<champsim::address,bool> temp_result = btb_prediction(ip,branch_type);
      if(btb_btb_prediction0_used)
        return(temp_result);
    }
    if(btb_btb_prediction1_used) {
      std::pair<champsim::address,bool> temp_result = btb_prediction(ip);
      if(btb_btb_prediction1_used)
        return(temp_result);
    }
    if(btb_btb_prediction2_used) {
      std::pair<champsim::address,bool> temp_result{btb_prediction(ip.to<uint64_t>(),branch_type)};
      if(btb_btb_prediction2_used)
        return(temp_result);
    }
    if(btb_btb_prediction3_used) {
      std::pair<champsim::address,bool> temp_result{btb_prediction(ip.to<uint64_t>())};
      if(btb_btb_prediction3_used)
        return(temp_result);
    }
    return result;
  }

}
// LCOV_EXCL_STOP
