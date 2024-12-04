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


//Module base, defining the base type B for the module and component type C that it is used by
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
    //bind the internal pointer to its managing component
    void bind(C* bind_arg) {intern_ = bind_arg;};

    //create an instance of the module, which will be stored in this base-module-type's static list
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

    //register a derived type D of base type B and constructor with arguments Params with the module system
    //this is necessary to be able to create instances
    template<typename D, typename... Params> 
    struct register_module {
      register_module(std::string module_name) {
          
          std::function<std::unique_ptr<B>(Params...)> create_module([](Params... parameters){return std::unique_ptr<B>(new D(parameters...));});
          add_module(module_name,create_module);
      }
    };

};

  struct prefetcher: public module_base<prefetcher,CACHE> {

      //prefetcher initialize
      virtual void prefetcher_initialize() {}

      //prefetcher cache operate
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] bool cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] access_type type, [[maybe_unused]] uint32_t metadata_in) {
        return prefetcher_cache_operate(addr,ip,(uint8_t)cache_hit,useful_prefetch,type,metadata_in);
      }
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] access_type type, [[maybe_unused]] uint32_t metadata_in) {
        return prefetcher_cache_operate(addr,ip,cache_hit,useful_prefetch,champsim::to_underlying<access_type>(type),metadata_in);
      }
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] bool cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] uint32_t metadata_in) {
        return prefetcher_cache_operate(addr.to<uint64_t>(),ip.to<uint64_t>(),cache_hit,type,metadata_in);
      }
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] uint64_t addr, [[maybe_unused]] uint64_t ip, [[maybe_unused]] bool cache_hit,[[maybe_unused]] std::underlying_type_t<access_type> type, 
                                                [[maybe_unused]] uint32_t metadata_in) {return metadata_in;}

      //prefetcher cache fill
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] champsim::address addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] bool prefetch, 
                                                [[maybe_unused]] champsim::address evicted_addr, [[maybe_unused]] uint32_t metadata_in) {
        return prefetcher_cache_fill(addr,set,way,(uint8_t)prefetch,evicted_addr,metadata_in);
      }
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] champsim::address addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] uint8_t prefetch,
                                             [[maybe_unused]] champsim::address evicted_addr, [[maybe_unused]] uint32_t metadata_in) {
        return prefetcher_cache_fill(addr.to<uint64_t>(), set, way, prefetch, evicted_addr.to<uint64_t>(), metadata_in);
      }
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] uint64_t addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] bool prefetch, 
                                                [[maybe_unused]] uint64_t evicted_addr, [[maybe_unused]] uint32_t metadata_in) {return metadata_in;}

      //prefetcher cycle operate
      virtual void prefetcher_cycle_operate() {}

      //prefetcher final stats
      virtual void prefetcher_final_stats() {}

      //prefetcher branch operate
      virtual void prefetcher_branch_operate([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] champsim::address branch_target) {
        prefetcher_branch_operate(ip.to<uint64_t>(), branch_type, branch_target.to<uint64_t>());
      }
      virtual void prefetcher_branch_operate([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] uint64_t branch_target) {}

      bool prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
      bool prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
  };


  struct replacement: public module_base<replacement,CACHE> {

      //initialize replacement
      virtual void initialize_replacement() {}

      //find victim
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] champsim::address ip,
                                      [[maybe_unused]] champsim::address full_addr, [[maybe_unused]] access_type type) {
        return find_victim(triggering_cpu,instr_id,set,current_set,ip,full_addr,champsim::to_underlying<access_type>(type));
      }
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] champsim::address ip,
                                      [[maybe_unused]] champsim::address full_addr, [[maybe_unused]] std::underlying_type_t<access_type> type) {
        return find_victim(triggering_cpu, instr_id, set, current_set, ip.to<uint64_t>(), full_addr.to<uint64_t>(), static_cast<access_type>(type));
      }
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] uint64_t ip,
                                      [[maybe_unused]] uint64_t full_addr, [[maybe_unused]] access_type type) {
        return find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, champsim::to_underlying<access_type>(type));
      }
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] uint64_t ip,
                                      [[maybe_unused]] uint64_t full_addr, [[maybe_unused]] std::underlying_type_t<access_type> type) { return -1;};

      //update replacement state
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type, [[maybe_unused]] bool hit) {
        champsim::address repl_victim = hit ? champsim::address{} : victim_addr;
        update_replacement_state(triggering_cpu,set,way,full_addr,ip,repl_victim,type,(uint8_t)hit);
      }
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type, [[maybe_unused]] uint8_t hit) {
        update_replacement_state(triggering_cpu,set,way,full_addr,ip,victim_addr,champsim::to_underlying<access_type>(type),hit);
      }
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] bool hit) {
        update_replacement_state(triggering_cpu,set,way,full_addr.to<uint64_t>(),ip.to<uint64_t>(),victim_addr.to<uint64_t>(),type,hit);
      }
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] uint64_t full_addr,
                                                  [[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t victim_addr, [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] bool hit) {
        update_replacement_state(triggering_cpu,set,way,champsim::address{full_addr},champsim::address{ip},static_cast<access_type>(type),hit);
      }
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] access_type type, [[maybe_unused]] bool hit) {}


      //replacement cache fill
      virtual void replacement_cache_fill([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr, 
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type);

      //replacement final stats
      virtual void replacement_final_stats() {}

  };

  struct branch_predictor: public module_base<branch_predictor,O3_CPU> {

    //initialize branch predictor
    virtual void initialize_branch_predictor() {}

    //last branch result
    virtual void last_branch_result([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {
      last_branch_result(ip.to<uint64_t>(),target.to<uint64_t>(),taken,branch_type);
    }
    virtual void last_branch_result([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {}

    //predict branch
    virtual bool predict_branch([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address predicted_target, [[maybe_unused]] bool always_taken, [[maybe_unused]] uint8_t branch_type) {
      return predict_branch(ip.to<uint64_t>(),predicted_target.to<uint64_t>(),always_taken,branch_type);
    }
    virtual bool predict_branch([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t predicted_target, [[maybe_unused]] bool always_taken, [[maybe_unused]] uint8_t branch_type) {
      return predict_branch(champsim::address{ip});
    }
    virtual bool predict_branch([[maybe_unused]] champsim::address ip) {
      return predict_branch(ip.to<uint64_t>());
    }
    virtual bool predict_branch([[maybe_unused]] uint64_t ip) {return false;}

  };

  struct btb: public module_base<btb,O3_CPU> {

    //initialize btb
    virtual void initialize_btb() {}

    //update btb
    virtual void update_btb([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address predicted_target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {
      update_btb(ip.to<uint64_t>(),predicted_target.to<uint64_t>(),taken,branch_type);
    }
    virtual void update_btb([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t predicted_target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {}

    //btb prediction
    virtual std::pair<champsim::address, bool> btb_prediction([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type) {
      return std::pair<champsim::address, bool>{btb_prediction(ip.to<uint64_t>(),branch_type)};
    }
    virtual std::pair<uint64_t, bool> btb_prediction([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint8_t branch_type) {
      std::pair<champsim::address, bool> result = btb_prediction(champsim::address{ip});
      return std::pair<uint64_t, bool>{result.first.to<uint64_t>(),result.second};
    }
    virtual std::pair<champsim::address, bool> btb_prediction([[maybe_unused]] champsim::address ip) {
      return std::pair<champsim::address, bool>{btb_prediction(ip.to<uint64_t>())};  
    }
    virtual std::pair<uint64_t, bool> btb_prediction([[maybe_unused]] uint64_t ip) {return std::pair<uint64_t, bool>{};}
  };

  template<typename B, typename C>
  std::map<std::string,std::any> module_base<B,C>::module_map;
  template<typename B, typename C>
  std::map<std::string,std::vector<std::unique_ptr<B>>> module_base<B,C>::instance_map;
}





#endif
