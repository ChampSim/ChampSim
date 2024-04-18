#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#ifndef EVENT_LISTENER_H
#define EVENT_LISTENER_H

#include <vector>
#include <iostream>

#include "champsim.h"
#include "instruction.h"
#include "channel.h"
#include "access_type.h"

#include "ooo_cpu.h"
//Define here
enum class event {
  CYCLE_BEGIN,
  BRANCH,
  DIB,
  FETCH,
  DECODE,
  RETIRE,
  EXE,
  LOAD_DEPENDENCY,
  DISPATCH_MEM,
  SQ,
  CSTORE,
  ELOAD,
  HANMEM,
  VA_TO_PA,
  GET_PTE_PA,
  ADD_RQ,
  ADD_WQ,
  ADD_PQ,
  BEGIN_PHASE,
  END,
  PTW_HANDLE_READ,
  PTW_HANDLE_FILL,
  PTW_OPERATE,
  PTW_FINISH_PACKET,
  PTW_FINISH_PACKET_LAST_STEP,
  FINISH
};

struct CYCLE_BEGIN_data {};

struct BRANCH_data {
  ooo_model_instr* instr;

  BRANCH_data() {
    instr = nullptr;
  }
};

struct DIB_data {
  ooo_model_instr* instr;
  long cycle;
  
  DIB_data() {
    instr = nullptr;
    cycle = 0;
  }
};

struct FETCH_data {
  ooo_model_instr* begin;
  long cycle;
  std::vector<long unsigned int> instr_depend_on_me;
  
  
  FETCH_data() {
    
    cycle = 0;
  }
};

struct DECODE_data {
  ooo_model_instr* instr;
  long cycle;
  
  DECODE_data() {
    instr = nullptr;
    cycle = 0;
  }
};

struct RETIRE_data {
  long cycle;
  std::vector<ooo_model_instr> instrs;
  std::deque<ooo_model_instr>* ROB;

  RETIRE_data() {
    cycle = 0;
    ROB = nullptr;
  }
};

struct EXE_data {
  ooo_model_instr* instr;
  long cycle;
  
  EXE_data() {
    instr = nullptr;
    cycle = 0;
  }
};

struct LOAD_DEPENDENCY_data {
  ooo_model_instr* sink;
  LSQ_ENTRY* source;

  LOAD_DEPENDENCY_data(ooo_model_instr* sink_, LSQ_ENTRY* source_) : sink(sink_), source(source_) {}
};

struct DISPATCH_MEM_data {
  ooo_model_instr* instr;
  long cycle;
  
  DISPATCH_MEM_data() {
    instr = nullptr;
    cycle = 0;
  }
};

struct SQ_data {
  const LSQ_ENTRY* instr;
  
  SQ_data() {
    instr = nullptr;
  }
};

struct CSTORE_data {
  const LSQ_ENTRY* instr;
  
  CSTORE_data() {
    instr = nullptr;
  }
};

struct ELOAD_data {
  const LSQ_ENTRY* instr;
  
  ELOAD_data() {
    instr = nullptr;
  }
};

struct HANMEM_data {
  ooo_model_instr* instr;
  
  HANMEM_data() {
    instr = nullptr;
  }
};

struct VA_TO_PA_data {
  champsim::page_number paddr;
  champsim::page_number vaddr;
  bool fault;
};

struct GET_PTE_PA_data {
  champsim::address paddr;
  champsim::page_number vaddr;
  uint64_t offset;
  std::size_t level;
  bool fault;
};

struct ADD_RQ_data {
  uint64_t instr_id;
  champsim::address address;
  champsim::address v_address;
  access_type type;
};

struct ADD_WQ_data {
  uint64_t instr_id;
  champsim::address address;
  champsim::address v_address;
  access_type type;
};

struct ADD_PQ_data {
  uint64_t instr_id;
  champsim::address address;
  champsim::address v_address;
  access_type type;
};

struct BEGIN_PHASE_data {
  bool is_warmup;
};

struct PTW_HANDLE_READ_data {
  std::string NAME;
  champsim::address address;
  champsim::address v_address;
  std::vector<uint64_t> instr_depend_on_mshr;
  int pt_page_offset;
  std::size_t translation_level;
  long cycle;

  PTW_HANDLE_READ_data(std::string NAME_, champsim::address address_, champsim::address v_address_, std::vector<uint64_t>& instr_depend_on_mshr_, int pt_page_offset_, std::size_t translation_level_, long cycle_)
    : NAME(NAME_), address(address_), v_address(v_address_), instr_depend_on_mshr(instr_depend_on_mshr_), pt_page_offset(pt_page_offset_), translation_level(translation_level_), cycle(cycle_) {}
};

struct PTW_HANDLE_FILL_data {
  std::string NAME;
  champsim::address address;
  champsim::address v_address;
  champsim::address data;
  int pt_page_offset;
  std::size_t translation_level;
  long cycle;

  PTW_HANDLE_FILL_data(std::string NAME_, champsim::address address_, champsim::address v_address_, champsim::address data_, int pt_page_offset_, std::size_t translation_level_, long cycle_)
    : NAME(NAME_), address(address_), v_address(v_address_), data(data_), pt_page_offset(pt_page_offset_), translation_level(translation_level_), cycle(cycle_) {}
};

struct PTW_OPERATE_data {
  std::string NAME;
  std::vector<champsim::address> mshr_addresses;
  long cycle;

  PTW_OPERATE_data(std::string NAME_, std::vector<champsim::address>& mshr_addresses_, long cycle_)
    : NAME(NAME_), mshr_addresses(mshr_addresses_), cycle(cycle_) {}
};

struct PTW_FINISH_PACKET_data {
  std::string NAME;
  champsim::address address;
  champsim::address v_address;
  champsim::address data;
  std::size_t translation_level;
  long cycle;
  long penalty;

  PTW_FINISH_PACKET_data(std::string NAME_, champsim::address address_, champsim::address v_address_, champsim::address data_, std::size_t translation_level_, long cycle_, long penalty_)
    : NAME(NAME_), address(address_), v_address(v_address_), data(data_), translation_level(translation_level_), cycle(cycle_), penalty(penalty_) {}
};

struct PTW_FINISH_PACKET_LAST_STEP_data {
  std::string NAME;
  champsim::address address;
  champsim::address v_address;
  champsim::page_number data;
  std::size_t translation_level;
  long cycle;
  long penalty;

  PTW_FINISH_PACKET_LAST_STEP_data(std::string NAME_, champsim::address address_, champsim::address v_address_, champsim::page_number data_, std::size_t translation_level_, long cycle_, long penalty_)
    : NAME(NAME_), address(address_), v_address(v_address_), data(data_), translation_level(translation_level_), cycle(cycle_), penalty(penalty_) {}
};

struct FINISH_data {
  ooo_model_instr* rob_entry;
  champsim::address virtual_address;

  FINISH_data() {
    rob_entry = nullptr;
    
  }
};

class EventListener {
public:
  virtual void process_event(event eventType, void* data);
};

namespace champsim {
  extern std::vector<EventListener*> event_listeners;
}

extern void call_event_listeners(event eventType, void* data);
extern void init_event_listeners();
extern void cleanup_event_listeners();

#endif

#ifdef SET_ASIDE_CHAMPSIM_MODULE
#undef SET_ASIDE_CHAMPSIM_MODULE
#define CHAMPSIM_MODULE
#endif
