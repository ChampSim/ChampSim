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
#include "ptw.h"
#include "ooo_cpu.h"

//Define here
enum class event {
  // misc events
  BEGIN_PHASE,
  END,
  // ooo_cpu.cc pipeline events
  PRE_CYCLE,
  INITIALIZE,
  CHECK_DIB,
  START_FETCH,
  END_FETCH,
  START_DECODE,
  START_DISPATCH,
  START_SCHEDULE,
  END_SCHEDULE,
  START_EXECUTE,
  END_EXECUTE,
  RETIRE,
  // ooo_cpu.cc other events
  BRANCH,
  LOAD_DEPENDENCY,
  SQ,
  CSTORE,
  ELOAD,
  HANMEM,
  FINISH,
  // vmem.cc events
  VA_TO_PA,
  GET_PTE_PA,
  // channel.cc events
  ADD_RQ,
  ADD_WQ,
  ADD_PQ,
  // ptw.cc events
  PTW_HANDLE_READ,
  PTW_HANDLE_FILL,
  PTW_OPERATE,
  PTW_FINISH_PACKET,
  PTW_FINISH_PACKET_LAST_STEP,
  // cache.cc events
  CACHE_TRY_HIT
  // TODO: add rest of cache.cc events
};

// misc events

struct BEGIN_PHASE_data {
  bool is_warmup;
};

// ooo_cpu.cc pipeline events

struct PRE_CYCLE_data {
  std::deque<ooo_model_instr>* IFETCH_BUFFER;
  std::deque<ooo_model_instr>* DISPATCH_BUFFER;
  std::deque<ooo_model_instr>* DECODE_BUFFER;
  std::deque<ooo_model_instr>* ROB;
  std::vector<std::optional<LSQ_ENTRY>>* LQ;
  std::deque<LSQ_ENTRY>* SQ;
  std::deque<ooo_model_instr>* input_queue;
  long cycle;

  PRE_CYCLE_data(std::deque<ooo_model_instr>* if_buf, std::deque<ooo_model_instr>* dis_buf, std::deque<ooo_model_instr>* dec_buf, std::deque<ooo_model_instr>* rob_, std::vector<std::optional<LSQ_ENTRY>>* lq_, std::deque<LSQ_ENTRY>* sq_, std::deque<ooo_model_instr>* iq, long cycle_) : IFETCH_BUFFER(if_buf), DISPATCH_BUFFER(dis_buf), DECODE_BUFFER(dec_buf), ROB(rob_), LQ(lq_), SQ(sq_), input_queue(iq), cycle(cycle_) {}
};

struct INITIALIZE_data {
  std::deque<ooo_model_instr>::iterator begin;
  std::deque<ooo_model_instr>::iterator end;
  long cycle;

  INITIALIZE_data(std::deque<ooo_model_instr>::iterator begin_, std::deque<ooo_model_instr>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct CHECK_DIB_data {
  std::deque<ooo_model_instr>::iterator begin;
  std::deque<ooo_model_instr>::iterator end;
  long cycle;

  CHECK_DIB_data(std::deque<ooo_model_instr>::iterator begin_, std::deque<ooo_model_instr>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct START_FETCH_data {
  std::deque<ooo_model_instr>::iterator begin;
  std::deque<ooo_model_instr>::iterator end;
  bool success;
  long cycle;

  START_FETCH_data(std::deque<ooo_model_instr>::iterator begin_, std::deque<ooo_model_instr>::iterator end_, bool success_, long cycle_) : begin(begin_), end(end_), success(success_), cycle(cycle_) {}
};

struct END_FETCH_data {
  std::vector<ooo_model_instr*>::iterator begin;
  std::vector<ooo_model_instr*>::iterator end;
  long cycle;

  END_FETCH_data(std::vector<ooo_model_instr*>::iterator begin_, std::vector<ooo_model_instr*>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct START_DECODE_data {
  std::deque<ooo_model_instr>::iterator begin;
  std::deque<ooo_model_instr>::iterator end;
  long cycle;

  START_DECODE_data(std::deque<ooo_model_instr>::iterator begin_, std::deque<ooo_model_instr>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct START_DISPATCH_data {
  std::deque<ooo_model_instr>::iterator begin;
  std::deque<ooo_model_instr>::iterator end;
  long cycle;

  START_DISPATCH_data(std::deque<ooo_model_instr>::iterator begin_, std::deque<ooo_model_instr>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct START_SCHEDULE_data {
  std::deque<ooo_model_instr>::iterator begin;
  std::deque<ooo_model_instr>::iterator end;
  long cycle;

  START_SCHEDULE_data(std::deque<ooo_model_instr>::iterator begin_, std::deque<ooo_model_instr>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct END_SCHEDULE_data {
  std::vector<ooo_model_instr*>::iterator begin;
  std::vector<ooo_model_instr*>::iterator end;
  long cycle;

  END_SCHEDULE_data(std::vector<ooo_model_instr*>::iterator begin_, std::vector<ooo_model_instr*>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct START_EXECUTE_data {
  std::vector<ooo_model_instr*>::iterator begin;
  std::vector<ooo_model_instr*>::iterator end;
  long cycle;

  START_EXECUTE_data(std::vector<ooo_model_instr*>::iterator begin_, std::vector<ooo_model_instr*>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct END_EXECUTE_data {
  std::vector<ooo_model_instr*>::iterator begin;
  std::vector<ooo_model_instr*>::iterator end;
  long cycle;

  END_EXECUTE_data(std::vector<ooo_model_instr*>::iterator begin_, std::vector<ooo_model_instr*>::iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

struct RETIRE_data {
  std::deque<ooo_model_instr>::const_iterator begin;
  std::deque<ooo_model_instr>::const_iterator end;
  long cycle;

  RETIRE_data(std::deque<ooo_model_instr>::const_iterator begin_, std::deque<ooo_model_instr>::const_iterator end_, long cycle_) : begin(begin_), end(end_), cycle(cycle_) {}
};

// ooo_cpu.cc other events

struct BRANCH_data {
  ooo_model_instr* instr;

  BRANCH_data(ooo_model_instr* instr_) : instr(instr_) {}
};

struct LOAD_DEPENDENCY_data {
  ooo_model_instr* sink;
  LSQ_ENTRY* source;

  LOAD_DEPENDENCY_data(ooo_model_instr* sink_, LSQ_ENTRY* source_) : sink(sink_), source(source_) {}
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

struct FINISH_data {
  ooo_model_instr* rob_entry;
  champsim::address virtual_address;

  FINISH_data() {
    rob_entry = nullptr;
  }
};

// vmem.cc events

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

// channel.cc events

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

// ptw.cc events

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
  std::vector<uint64_t> instr_depend_on_me;
  long cycle;
  long penalty;

  PTW_FINISH_PACKET_data(std::string NAME_, champsim::address address_, champsim::address v_address_, champsim::address data_, std::size_t translation_level_, std::vector<uint64_t> instr_depend_on_me_, long cycle_, long penalty_)
    : NAME(NAME_), address(address_), v_address(v_address_), data(data_), translation_level(translation_level_), instr_depend_on_me(instr_depend_on_me_), cycle(cycle_), penalty(penalty_) {}
};

struct PTW_FINISH_PACKET_LAST_STEP_data {
  std::string NAME;
  champsim::address address;
  champsim::address v_address;
  champsim::page_number data;
  std::size_t translation_level;
  std::vector<uint64_t> instr_depend_on_me;
  long cycle;
  long penalty;

  PTW_FINISH_PACKET_LAST_STEP_data(std::string NAME_, champsim::address address_, champsim::address v_address_, champsim::page_number data_, std::size_t translation_level_, std::vector<uint64_t> instr_depend_on_me_, long cycle_, long penalty_)
    : NAME(NAME_), address(address_), v_address(v_address_), data(data_), translation_level(translation_level_), instr_depend_on_me(instr_depend_on_me_), cycle(cycle_), penalty(penalty_) {}
};

// cache.cc events

struct CACHE_TRY_HIT_data {
  std::string NAME;
  uint64_t instr_id = std::numeric_limits<uint64_t>::max();
  bool hit;
  long set;
  long way;
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
