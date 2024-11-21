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

class CACHE;
class O3_CPU;
namespace champsim::modules {


template<typename B>
struct module_base {

    using function_type = typename std::function<std::unique_ptr<B>()>;
    static std::map<std::string,function_type> module_map;
    static std::map<std::string,std::vector<std::unique_ptr<B>>> instance_map;

    static void add_module(std::string name, function_type module_constructor) {
        if(module_map.find(name) != module_map.end()) {
            fmt::print("[MODULE] ERROR: duplicate module name used: {}\n", name);
            exit(-1);
        }
        module_map[name] = module_constructor;
    }

    static B* create_instance(std::string name) {
        if(module_map.find(name) == module_map.end()) {
            fmt::print("[MODULE] ERROR: specified module {} does not exist\n",name);
            exit(-1);
        }
        return instance_map[name].emplace_back(module_map[name]()).get();
    }

    template<typename D> 
    struct register_module {
    register_module(std::string module_name) {
        function_type create_module([](){return std::unique_ptr<B>(new D());});
        add_module(module_name,create_module);
    }
};


};

  struct prefetcher: public module_base<prefetcher> {

      CACHE* intern_;
      void bind(CACHE* cache) {intern_ = cache;}

      virtual void prefetcher_initialize() {}
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] bool cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                    [[maybe_unused]] access_type type, [[maybe_unused]] uint32_t metadata_in) { return metadata_in;}
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] champsim::address addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] bool prefetch, 
                                                [[maybe_unused]] champsim::address evicted_addr,[[maybe_unused]] uint32_t metadata_in) { return metadata_in;}
      virtual void prefetcher_cycle_operate() {}
      virtual void prefetcher_final_stats() {}
      virtual void prefetcher_branch_operate([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] champsim::address branch_target) {}
      bool prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;

      bool prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
  };



  struct replacement: public module_base<replacement> {
    CACHE* intern_;
    void bind(CACHE* cache) {intern_ = cache;}

    virtual void initialize_replacement() {}
    virtual long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                                    champsim::address full_addr, access_type type) = 0;
    virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type, [[maybe_unused]] bool hit) {};
    virtual void replacement_cache_fill([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr, 
                                                [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type) {};
    virtual void replacement_final_stats() {};
  };

  struct branch_predictor: public module_base<branch_predictor> {
    O3_CPU* intern_;
    void bind(O3_CPU* cpu) {intern_ = cpu;}
    virtual void initialize_branch_predictor() {};
    virtual void last_branch_result([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {};
    virtual bool predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type) = 0;
  };

  struct btb: public module_base<btb> {
    O3_CPU* intern_;
    void bind(O3_CPU* cpu) {intern_ = cpu;}
    virtual void initialize_btb() {};
    virtual void update_btb([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address predicted_target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {};
    virtual std::pair<champsim::address, bool> btb_prediction(champsim::address ip, uint8_t branch_type) = 0;
  };

  template<typename B>
  std::map<std::string,std::function<std::unique_ptr<B>()>> module_base<B>::module_map;
  template<typename B>
  std::map<std::string,std::vector<std::unique_ptr<B>>> module_base<B>::instance_map;
}





#endif
