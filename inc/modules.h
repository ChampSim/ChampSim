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

  //this interface manager identifies and invokes
  //multiple overloads of hooks
  class interface_manager {
    bool is_default_interface = false;
    std::map<std::size_t, std::vector<bool>> usage_map;

    protected:

    //checks to see if specific interface inter (overload _id) is being used
    template <std::size_t inter, std::size_t _id>
    bool is_active() {
      if(auto found = usage_map.find(inter); found != usage_map.end()) {
        if(_id >= std::size(found->second))
          return true;
        return found->second.at(_id);
      }
      return true;
    }

    //invoked by the default interface to identify an unused interface
    void is_default() {
      is_default_interface = true;
    }

    //checks to see if any interface of a specific type is active
    bool is_any_active(std::size_t inter) {
      if(auto found = usage_map.find(inter); found != usage_map.end()) {
        return(std::any_of(found->second.begin(),found->second.end(),[](auto const entry) {return entry;}));
      }
      return true;
    }

    //checks to see if the given interface is active and invokes it if so,
    //Will return a pair of <return_type,bool> indicating success or failure to invoke an active interface
    //Template parameters are: the interface being invoked, the id of said interface overload, the return type of
    //the interface, the interface function call, and its arguments
    template<std::size_t inter, std::size_t _id, typename return_type, typename func_type, typename... Args>
      std::pair<return_type,bool> arbitrate_interface(func_type func, Args... arguments) {
        is_default_interface = false;
        if (is_active<inter,_id>()) {
          //call interface
          return_type temp = func(arguments...);

          //check if default bit was set. If so, update
          if(is_default_interface) {
            if(auto found = usage_map.find(inter); found != usage_map.end()) {
              if(found->second.size() <= _id)
                found->second.resize(_id+1);
              found->second.at(_id) = false;
            }
            else {
              usage_map[inter] = std::vector<bool>(_id+1,true);
              usage_map[inter].at(_id) = false;
            }
            return std::pair<return_type,bool>{};
          }
          //else return variable
          return(std::pair<return_type,bool>{temp,true});
        }
        //else return nothing
        return std::pair<return_type,bool>{};
      }
    //Overload to invoke an interface that returns nothing (void)
    template<std::size_t inter, std::size_t _id, typename func_type, typename... Args>
      bool arbitrate_interface(func_type func, Args... arguments) {
        is_default_interface = false;
        if (is_active<inter,_id>()) {
          //call interface
          func(arguments...);

          //check if default bit was set. If so, update
          if(is_default_interface) {
            if(auto found = usage_map.find(inter); found != usage_map.end()) {
              if(found->second.size() <= _id)
                found->second.resize(_id+1);
              found->second.at(_id) = false;
            }
            else {
              usage_map[inter] = std::vector<bool>(_id+1,true);
              usage_map[inter].at(_id) = false;
            }
            return false;
          }
          //else return variable
          return true;
        }
        //else return nothing
        return false;
      }
  
  };

  struct prefetcher: public module_base<prefetcher,CACHE>, public interface_manager{
      enum prefetch_interface {
        INITIALIZE,
        CACHE_OPERATE,
        CACHE_FILL,
        CYCLE_OPERATE,
        FINAL_STATS,
        BRANCH_OPERATE,
      };
      //prefetcher initialize
      void prefetcher_initialize_impl();
      virtual void prefetcher_initialize() {is_default();}

      //prefetcher cache operate
      uint32_t prefetcher_cache_operate_impl(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch,
                                                access_type type, uint32_t metadata_in);
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] bool cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] access_type type, [[maybe_unused]] uint32_t metadata_in) {is_default(); return metadata_in;}
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] access_type type, [[maybe_unused]] uint32_t metadata_in) {is_default(); return metadata_in;}
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] champsim::address addr, [[maybe_unused]] champsim::address ip, [[maybe_unused]] bool cache_hit, [[maybe_unused]] bool useful_prefetch,
                                                [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] uint32_t metadata_in) {is_default(); return metadata_in;}
      virtual uint32_t prefetcher_cache_operate([[maybe_unused]] uint64_t addr, [[maybe_unused]] uint64_t ip, [[maybe_unused]] bool cache_hit,[[maybe_unused]] std::underlying_type_t<access_type> type, 
                                                [[maybe_unused]] uint32_t metadata_in) {is_default(); return metadata_in;}

      //prefetcher cache fill
      uint32_t prefetcher_cache_fill_impl(champsim::address addr, long set, long way, bool prefetch, 
                                                champsim::address evicted_addr, uint32_t metadata_in);
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] champsim::address addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] bool prefetch, 
                                                [[maybe_unused]] champsim::address evicted_addr, [[maybe_unused]] uint32_t metadata_in) {is_default(); return metadata_in;}
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] champsim::address addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] uint8_t prefetch,
                                             [[maybe_unused]] champsim::address evicted_addr, [[maybe_unused]] uint32_t metadata_in) {is_default(); return metadata_in;}
      virtual uint32_t prefetcher_cache_fill([[maybe_unused]] uint64_t addr, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] bool prefetch, 
                                                [[maybe_unused]] uint64_t evicted_addr, [[maybe_unused]] uint32_t metadata_in) {is_default(); return metadata_in;}

      //prefetcher cycle operate
      void prefetcher_cycle_operate_impl();
      virtual void prefetcher_cycle_operate() {is_default();}

      //prefetcher final stats
      void prefetcher_final_stats_impl();
      virtual void prefetcher_final_stats() {is_default();}

      //prefetcher branch operate
      void prefetcher_branch_operate_impl([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] champsim::address branch_target);
      virtual void prefetcher_branch_operate([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] champsim::address branch_target) {is_default();}
      virtual void prefetcher_branch_operate([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint8_t branch_type, [[maybe_unused]] uint64_t branch_target) {is_default();}

      bool prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
      bool prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const;
  };


  struct replacement: public module_base<replacement,CACHE>, public interface_manager {
      enum replacement_interface {
        INITIALIZE,
        FIND_VICTIM,
        UPDATE_REPLACEMENT_STATE,
        CACHE_FILL,
        FINAL_STATS,
      };
      //initialize replacement
      void initialize_replacement_impl();
      virtual void initialize_replacement() {is_default();}

      //find victim
      long find_victim_impl(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                                      champsim::address full_addr, access_type type);
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] champsim::address ip,
                                      [[maybe_unused]] champsim::address full_addr, [[maybe_unused]] access_type type) { is_default(); return -1;}
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] champsim::address ip,
                                      [[maybe_unused]] champsim::address full_addr, [[maybe_unused]] std::underlying_type_t<access_type> type) { is_default(); return -1;};
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] uint64_t ip,
                                      [[maybe_unused]] uint64_t full_addr, [[maybe_unused]] access_type type) {is_default(); return -1;}
      virtual long find_victim([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] uint64_t instr_id, [[maybe_unused]] long set, [[maybe_unused]] const champsim::cache_block* current_set, [[maybe_unused]] uint64_t ip,
                                      [[maybe_unused]] uint64_t full_addr, [[maybe_unused]] std::underlying_type_t<access_type> type) { is_default(); return -1;};

      //update replacement state
      void update_replacement_state_impl(uint32_t triggering_cpu, long set, long way, champsim::address full_addr,
                                                  champsim::address ip, champsim::address victim_addr, access_type type, bool hit);
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] access_type type, [[maybe_unused]] bool hit) {is_default();}
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type, [[maybe_unused]] bool hit) {is_default();}
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type, [[maybe_unused]] uint8_t hit) {is_default();}
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr,
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] bool hit) {is_default();}
      virtual void update_replacement_state([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] uint64_t full_addr,
                                                  [[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t victim_addr, [[maybe_unused]] std::underlying_type_t<access_type> type, [[maybe_unused]] bool hit) {is_default();}

      //replacement cache fill
      void replacement_cache_fill_impl(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, 
                                                  champsim::address ip, champsim::address victim_addr, access_type type);
      virtual void replacement_cache_fill([[maybe_unused]] uint32_t triggering_cpu, [[maybe_unused]] long set, [[maybe_unused]] long way, [[maybe_unused]] champsim::address full_addr, 
                                                  [[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address victim_addr, [[maybe_unused]] access_type type) {is_default();}

      //replacement final stats
      void replacement_final_stats_impl();
      virtual void replacement_final_stats() {is_default();}

  };

  struct branch_predictor: public module_base<branch_predictor,O3_CPU>, public interface_manager {

    enum bp_interface {
      INITIALIZE,
      LAST_BRANCH_RESULT,
      PREDICT_BRANCH
    };

    //initialize branch predictor
    void initialize_branch_predictor_impl();
    virtual void initialize_branch_predictor() {is_default();}

    //last branch result
    void last_branch_result_impl(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type);
    virtual void last_branch_result([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {is_default();}
    virtual void last_branch_result([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {is_default();}

    //predict branch
    bool predict_branch_impl(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type);
    virtual bool predict_branch([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address predicted_target, [[maybe_unused]] bool always_taken, [[maybe_unused]] uint8_t branch_type) {is_default(); return false;}
    virtual bool predict_branch([[maybe_unused]] champsim::address ip) {is_default(); return false;}
    virtual bool predict_branch([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t predicted_target, [[maybe_unused]] bool always_taken, [[maybe_unused]] uint8_t branch_type) {is_default(); return false;}
    virtual bool predict_branch([[maybe_unused]] uint64_t ip) {is_default(); return false;}

  };

  struct btb: public module_base<btb,O3_CPU>, public interface_manager {

    enum btb_interface {
      INITIALIZE,
      UPDATE_BTB,
      BTB_PREDICTION
    };
    //initialize btb
    void initialize_btb_impl();
    virtual void initialize_btb() {is_default();}

    //update btb
    void update_btb_impl(champsim::address ip, champsim::address predicted_target, bool taken, uint8_t branch_type);
    virtual void update_btb([[maybe_unused]] champsim::address ip, [[maybe_unused]] champsim::address predicted_target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {is_default();}
    virtual void update_btb([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint64_t predicted_target, [[maybe_unused]] bool taken, [[maybe_unused]] uint8_t branch_type) {is_default();}

    //btb prediction
    std::pair<champsim::address, bool> btb_prediction_impl(champsim::address ip, uint8_t branch_type);
    virtual std::pair<champsim::address, bool> btb_prediction([[maybe_unused]] champsim::address ip, [[maybe_unused]] uint8_t branch_type) {is_default(); return std::pair<champsim::address, bool>{};}
    virtual std::pair<champsim::address, bool> btb_prediction([[maybe_unused]] champsim::address ip) {is_default(); return std::pair<champsim::address, bool>{};}
    virtual std::pair<uint64_t, bool> btb_prediction([[maybe_unused]] uint64_t ip, [[maybe_unused]] uint8_t branch_type) {is_default(); return std::pair<uint64_t, bool>{};}
    virtual std::pair<uint64_t, bool> btb_prediction([[maybe_unused]] uint64_t ip) {is_default(); return std::pair<uint64_t, bool>{};}
  };

  template<typename B, typename C>
  std::map<std::string,std::any> module_base<B,C>::module_map;
  template<typename B, typename C>
  std::map<std::string,std::vector<std::unique_ptr<B>>> module_base<B,C>::instance_map;
}





#endif
