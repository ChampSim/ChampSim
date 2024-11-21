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
    std::string NAME;
    using function_type = typename std::function<std::unique_ptr<B>()>;

    private:
    static std::map<std::string,function_type> module_map;
    static std::map<std::string,std::vector<std::unique_ptr<B>>> instance_map;

    static void add_module(std::string name, function_type module_constructor) {
        if(module_map.find(name) != module_map.end()) {
            fmt::print("[MODULE] ERROR: duplicate module name used: {}\n", name);
            exit(-1);
        }
        module_map[name] = module_constructor;
    }

    public:

    template<typename T>
    static B* create_instance(std::string name, T* bind_arg) {
        if(module_map.find(name) == module_map.end()) {
            fmt::print("[MODULE] ERROR: specified module {} does not exist\n",name);
            exit(-1);
        }
        B* instance_ptr = instance_map[name].emplace_back(module_map[name]()).get();
        instance_ptr->NAME = name;
        instance_ptr->bind(bind_arg);
        return(instance_ptr);
    }

    template<typename D> 
    struct register_module {
    register_module(std::string module_name) {
        function_type create_module([](){return std::unique_ptr<B>(new D());});
        add_module(module_name,create_module);
    }
};

};
  template <typename T>
  struct bound_to {
    T* intern_;
    void bind(T* bind_arg) { intern_ = bind_arg; }
  };

  struct prefetcher: public module_base<prefetcher>, public bound_to<CACHE> {

      virtual void prefetcher_initialize() {}
      virtual uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch,
                                                access_type type, uint32_t metadata_in) = 0;
      virtual uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, 
                                                champsim::address evicted_addr, uint32_t metadata_in) = 0;
      virtual void prefetcher_cycle_operate() = 0;
      virtual void prefetcher_final_stats() = 0;
      virtual void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) = 0;
      bool prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
  };


  struct replacement: public module_base<replacement>, public bound_to<CACHE> {

    virtual void initialize_replacement() = 0;
    virtual long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                                    champsim::address full_addr, access_type type) = 0;
    virtual void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr,
                                                champsim::address ip, champsim::address victim_addr, access_type type, bool hit) = 0;
    virtual void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, 
                                                champsim::address ip, champsim::address victim_addr, access_type type) = 0;
    virtual void replacement_final_stats() = 0;
  };

  struct branch_predictor: public module_base<branch_predictor>, public bound_to<O3_CPU> {

    virtual void initialize_branch_predictor() = 0;
    virtual void last_branch_result(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type) = 0;
    virtual bool predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type) = 0;
  };

  struct btb: public module_base<btb>, public bound_to<O3_CPU> {

    virtual void initialize_btb() {};
    virtual void update_btb(champsim::address ip, champsim::address predicted_target, bool taken, uint8_t branch_type) = 0;
    virtual std::pair<champsim::address, bool> btb_prediction(champsim::address ip, uint8_t branch_type) = 0;
  };

  template<typename B>
  std::map<std::string,std::function<std::unique_ptr<B>()>> module_base<B>::module_map;
  template<typename B>
  std::map<std::string,std::vector<std::unique_ptr<B>>> module_base<B>::instance_map;
}





#endif
