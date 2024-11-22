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

#ifndef MODULES_H
#define MODULES_H

#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <cassert>

#include "access_type.h"
#include "address.h"
#include "block.h"
#include "champsim.h"
#include <any>

class CACHE;
class O3_CPU;
namespace champsim::modules {


template<typename B, typename C>
struct module_base {
    std::string NAME;
    C* intern_;
    using function_type = typename std::function<std::unique_ptr<B>()>;

    private:
    static std::map<std::string,std::any> module_map;
    static std::map<std::string,std::vector<std::unique_ptr<B>>> instance_map;

    template<typename... Params>
    static void add_module(std::string name, std::function<std::unique_ptr<B>(Params...)> module_constructor) {
        if(module_map.find(name) != module_map.end()) {
            fmt::print("[MODULE] ERROR: duplicate module name used: {}\n", name);
            exit(-1);
        }
        module_map[name] = module_constructor;
    }

    public:
    void bind(C* bind_arg) {intern_ = bind_arg;};

    template<typename T, typename... Params>
    static B* create_instance(std::string name, T* bind_arg, Params... parameters) {
        if(module_map.find(name) == module_map.end()) {
            fmt::print("[MODULE] ERROR: specified module {} does not exist\n",name);
            exit(-1);
        }
        try {
          B* instance_ptr = instance_map[name].emplace_back(std::any_cast<std::function<std::unique_ptr<B>(Params...)>>(module_map[name])(parameters...)).get();
          instance_ptr->NAME = name;
          instance_ptr->bind(bind_arg);
          return(instance_ptr);
        }
        catch(const std::bad_any_cast& caught) {
          fmt::print("[MODULE] ERROR: Casting failed while constructing {}, are your registration and instance calls consistent?\n",name);
          exit(-1);
        }
    }

    template<typename D, typename... Params> 
    struct register_module {
      register_module(std::string module_name) {
          
          std::function<std::unique_ptr<B>(Params...)> create_module([](Params... parameters){return std::unique_ptr<B>(new D(parameters...));});
          add_module(module_name,create_module);
      }
    };

};

  struct prefetcher: public module_base<prefetcher,CACHE>{

      bool prefetcher_initialize_used = true;

      bool prefetcher_cache_operate0_used = true;
      bool prefetcher_cache_operate1_used = true;
      bool prefetcher_cache_operate2_used = true;
      bool prefetcher_cache_operate3_used = true;

      bool prefetcher_cache_fill0_used = true;
      bool prefetcher_cache_fill1_used = true;
      bool prefetcher_cache_fill2_used = true;

      bool prefetcher_cycle_operate_used = true;
      bool prefetcher_final_stats_used = true;

      bool prefetcher_branch_operate0_used = true;
      bool prefetcher_branch_operate1_used = true;

      void prefetcher_initialize_impl();
      virtual void prefetcher_initialize() {prefetcher_initialize_used = false;}

      uint32_t prefetcher_cache_operate_impl(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch,
                                                access_type type, uint32_t metadata_in);
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] bool cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] access_type type, [[maybe_unused]] uint32_t metadata_in) {prefetcher_cache_operate0_used = false; return metadata_in;}
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] access_type type, [[maybe_unused]] uint32_t metadata_in) {prefetcher_cache_operate1_used = false; return metadata_in;}
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] bool cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] uint32_t metadata_in) {prefetcher_cache_operate2_used = false; return metadata_in;}
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] uint64_t addr, [[maybe_unused]] uint64_t ip, [[maybe_unused]] bool cache_hit,[[maybe_unused]] std::underlying_type_t<access_type> type, 
                                                [[maybe_unused]] uint32_t metadata_in) {prefetcher_cache_operate3_used = false; return metadata_in;}
 
      uint32_t prefetcher_cache_fill_impl(champsim::address addr, long set, long way, bool prefetch, 
                                                champsim::address evicted_addr, uint32_t metadata_in);
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] champsim::address addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] bool prefetch, 
                                                [[maybe_unused]] champsim::address evicted_addr, [[maybe_unused]] uint32_t metadata_in) {prefetcher_cache_fill0_used = false; return metadata_in;}
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] champsim::address addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] uint8_t prefetch,
                                             [[maybe_unused]] champsim::address evicted_addr, [[maybe_unused]] uint32_t metadata_in) {prefetcher_cache_fill1_used = false; return metadata_in;}
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] uint64_t addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] bool prefetch, 
                                                [[maybe_unused]] uint64_t evicted_addr, [[maybe_unused]] uint32_t metadata_in) {prefetcher_cache_fill2_used = false; return metadata_in;}
      

      void prefetcher_cycle_operate_impl();
      virtual void prefetcher_cycle_operate() {prefetcher_cycle_operate_used = false;}

      void prefetcher_final_stats_impl();
      virtual void prefetcher_final_stats() {prefetcher_final_stats_used = false;}

      void prefetcher_branch_operate_impl([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] champsim::address branch_target);
      virtual void prefetcher_branch_operate([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] champsim::address branch_target) {prefetcher_branch_operate0_used = false;}
      virtual void prefetcher_branch_operate([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] uint64_t branch_target) {prefetcher_branch_operate1_used = false;}

      bool prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
      bool prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
  };


  struct replacement: public module_base<replacement,CACHE> {

      bool replacement_initialize_used = true;

      bool replacement_find_victim0_used = true;
      bool replacement_find_victim1_used = true;
      bool replacement_find_victim2_used = true;
      bool replacement_find_victim3_used = true;

      bool replacement_update_replacement_state0_used = true;
      bool replacement_update_replacement_state1_used = true;
      bool replacement_update_replacement_state2_used = true;
      bool replacement_update_replacement_state3_used = true;
      bool replacement_update_replacement_state4_used = true;

      bool replacement_cache_fill_used = true;

      bool replacement_final_stats_used = true;

      void initialize_replacement_impl();
      virtual void initialize_replacement() {replacement_initialize_used = false;}

      long find_victim_impl(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                                      champsim::address full_addr, access_type type);
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] champsim::address ip,
                                      [[maybe_unused]] champsim::address full_addr, [[maybe_unused]] access_type type) { replacement_find_victim0_used = false; return -1;}
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] champsim::address ip,
                                      [[maybe_unused]] champsim::address full_addr, [[maybe_unused]] std::underlying_type_t<access_type> type) { replacement_find_victim1_used = false; return -1;};
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] uint64_t ip,
                                      [[maybe_unused]] uint64_t full_addr, [[maybe_unused]] access_type type) {replacement_find_victim2_used = false; return -1;}
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] uint64_t ip,
                                      [[maybe_unused]] uint64_t full_addr, [[maybe_unused]] std::underlying_type_t<access_type> type) { replacement_find_victim3_used = false; return -1;};

      void update_replacement_state_impl(uint32_t triggering_cpu, long set, long way, champsim::address full_addr,
                                                  champsim::address ip, champsim::address victim_addr, access_type type, bool hit);
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] access_type type, [[maybe_unused]] bool hit) {replacement_update_replacement_state0_used = false;}
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type, [[maybe_unused]] bool hit) {replacement_update_replacement_state1_used = false;}
            virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type, [[maybe_unused]] uint8_t hit) {replacement_update_replacement_state2_used = false;}
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] bool hit) {replacement_update_replacement_state3_used = false;}
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] uint64_t full_addr,
                                                  [[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t victim_addr, [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] bool hit) {replacement_update_replacement_state4_used = false;}

      void replacement_cache_fill_impl(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, 
                                                  champsim::address ip, champsim::address victim_addr, access_type type);
      virtual void replacement_cache_fill([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr, 
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type) {replacement_cache_fill_used = false;}

      void replacement_final_stats_impl();
      virtual void replacement_final_stats() {replacement_final_stats_used = false;}
  };

  struct branch_predictor: public module_base<branch_predictor,O3_CPU> {

      bool branch_predictor_initialize_used = true;

      bool branch_predictor_last_branch_result0_used = true;
      bool branch_predictor_last_branch_result1_used = true;

      bool branch_predictor_predict_branch0_used = true;
      bool branch_predictor_predict_branch1_used = true;
      bool branch_predictor_predict_branch2_used = true;
      bool branch_predictor_predict_branch3_used = true;

    void initialize_branch_predictor_impl();
    virtual void initialize_branch_predictor() {branch_predictor_initialize_used = false;}

    void last_branch_result_impl(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type);
    virtual void last_branch_result([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {branch_predictor_last_branch_result0_used = false;}
    virtual void last_branch_result([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {branch_predictor_last_branch_result1_used = false;}

    bool predict_branch_impl(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type);
    virtual bool predict_branch([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address predicted_target, [[maybe_unused]] bool always_taken, [[maybe_unused]] uint8_t branch_type) {branch_predictor_predict_branch0_used = false; return false;}
    virtual bool predict_branch([[maybe_unused]] champsim::address ip) {branch_predictor_predict_branch1_used = false; return false;}
    virtual bool predict_branch([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t predicted_target, [[maybe_unused]] bool always_taken, [[maybe_unused]] uint8_t branch_type) {branch_predictor_predict_branch2_used = false; return false;}
    virtual bool predict_branch([[maybe_unused]] uint64_t ip) {branch_predictor_predict_branch3_used = false; return false;}
  };

  struct btb: public module_base<btb,O3_CPU> {

    bool btb_initialize_used = true;
    bool btb_update_btb0_used = true;
    bool btb_update_btb1_used = true;

    bool btb_btb_prediction0_used = true;
    bool btb_btb_prediction1_used = true;
    bool btb_btb_prediction2_used = true;
    bool btb_btb_prediction3_used = true;

    void initialize_btb_impl();
    virtual void initialize_btb() {btb_initialize_used = false;}

    void update_btb_impl(champsim::address ip, champsim::address predicted_target, bool taken, uint8_t branch_type);
    virtual void update_btb([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address predicted_target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {btb_update_btb0_used = false;}
    virtual void update_btb([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t predicted_target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {btb_update_btb1_used = false;}

    std::pair<champsim::address, bool> btb_prediction_impl(champsim::address ip, uint8_t branch_type);
    virtual std::pair<champsim::address, bool> btb_prediction([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type) {btb_btb_prediction0_used = false; return std::pair<champsim::address, bool>{};}
    virtual std::pair<champsim::address, bool> btb_prediction([[maybe_unused]] champsim::address ip) {btb_btb_prediction1_used = false; return std::pair<champsim::address, bool>{};}
    virtual std::pair<uint64_t, bool> btb_prediction([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint8_t branch_type) {btb_btb_prediction2_used = false; return std::pair<uint64_t, bool>{};}
    virtual std::pair<uint64_t, bool> btb_prediction([[maybe_unused]] uint64_t ip) {btb_btb_prediction3_used = false; return std::pair<uint64_t, bool>{};}
  };

  template<typename B, typename C>
  std::map<std::string,std::any> module_base<B,C>::module_map;
  template<typename B, typename C>
  std::map<std::string,std::vector<std::unique_ptr<B>>> module_base<B,C>::instance_map;
}





#endif
