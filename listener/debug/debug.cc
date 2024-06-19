#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "debug.h"

void debug::process_event(event eventType, void* data) {
  // from ooo_cpu pipeline events
  if (eventType == event::PRE_CYCLE) {
    PRE_CYCLE_data* p_data = static_cast<PRE_CYCLE_data *>(data);
    fmt::print("[PRE_CYCLE] cycle: {} IFETCH_BUFFER cap.: {} DECODE_BUFFER cap.: {}, DISPATCH_BUFFER cap.: {} ROB cap.: {} LQ cap.: {}, SQ cap.: {}\n", p_data->cycle, p_data->IFETCH_BUFFER->size(), p_data->DECODE_BUFFER->size(), p_data->DISPATCH_BUFFER->size(), p_data->ROB->size(), p_data->LQ->size(), p_data->SQ->size());
  } else if (eventType == event::INITIALIZE) {
    INITIALIZE_data* i_data = static_cast<INITIALIZE_data *>(data);
    fmt::print("[INITIALIZE] instrs:");
    for (auto it = i_data->begin; it != i_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::CHECK_DIB) {
    CHECK_DIB_data* c_data = static_cast<CHECK_DIB_data *>(data);
    fmt::print("[CHECK_DIB] instrs:");
    for (auto it = c_data->begin; it != c_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_FETCH) {
    START_FETCH_data* s_data = static_cast<START_FETCH_data *>(data);
    fmt::print("[START_FETCH] successful: {} instrs:", s_data->success);
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::END_FETCH) {
    END_FETCH_data* e_data = static_cast<END_FETCH_data *>(data);
    fmt::print("[END_FETCH] instrs:");
    for (auto it = e_data->begin; it != e_data->end; ++it) {
      fmt::print(" {}", (*it)->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_DECODE) {
    START_DECODE_data* s_data = static_cast<START_DECODE_data *>(data);
    fmt::print("[START_DECODE] instrs:");
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_DISPATCH) {
    START_DISPATCH_data* s_data = static_cast<START_DISPATCH_data *>(data);
    fmt::print("[START_DISPATCH] instrs:");
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_SCHEDULE) {
    START_SCHEDULE_data* s_data = static_cast<START_SCHEDULE_data *>(data);
    fmt::print("[START_SCHEDULE] instrs:");
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::END_SCHEDULE) {
    END_SCHEDULE_data* e_data = static_cast<END_SCHEDULE_data *>(data);
    fmt::print("[END_SCHEDULE] instrs:");
    for (auto it = e_data->begin; it != e_data->end; ++it) {
      fmt::print(" {}", (*it)->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_EXECUTE) {
    START_EXECUTE_data* e_data = static_cast<START_EXECUTE_data *>(data);
    fmt::print("[START_EXECUTE] instrs:");
    for (auto it = e_data->begin; it != e_data->end; ++it) {
      fmt::print(" {}", (*it)->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::END_EXECUTE) {
    END_EXECUTE_data* e_data = static_cast<END_EXECUTE_data *>(data);
    fmt::print("[END_EXECUTE] instrs:");
    for (auto it = e_data->begin; it != e_data->end; ++it) {
      fmt::print(" {}", (*it)->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::RETIRE) {
    RETIRE_data* s_data = static_cast<RETIRE_data *>(data);
    fmt::print("[RETIRE] instrs:");
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  }
  // from ooo_cpu.cc
  if (eventType == event::BRANCH) {
    BRANCH_data* b_data = static_cast<BRANCH_data *>(data);
    fmt::print("[BRANCH] do_predict_branch instr_id: {} ip: {} taken: {}\n", b_data->instr->instr_id, b_data->instr->ip, b_data->instr->branch_taken);
  } else if (eventType == event::LOAD_DEPENDENCY) {
    LOAD_DEPENDENCY_data* l_data = static_cast<LOAD_DEPENDENCY_data *>(data);
    fmt::print("[LOAD_DEPENDENCY] instr_id: {} waits on: {}\n", l_data->sink->instr_id, l_data->source->instr_id);
  } else if (eventType == event::SQ) {
    SQ_data* sq_data = static_cast<SQ_data *>(data);
     fmt::print("[SQ] {} instr_id: {} vaddr: {}\n", __func__, sq_data->instr->instr_id, sq_data->instr->virtual_address);
  } else if (eventType == event::CSTORE) {
    CSTORE_data* cs_data = static_cast<CSTORE_data *>(data);
     fmt::print("[CSTORE] {} instr_id: {} vaddr: {}\n", __func__, cs_data->instr->instr_id, cs_data->instr->virtual_address);
  } else if (eventType == event::ELOAD) {
    ELOAD_data* eload_data = static_cast<ELOAD_data *>(data);
    fmt::print("[EXELOAD] {} instr_id: {} vaddr: {}\n", __func__, eload_data->instr->instr_id, eload_data->instr->virtual_address);
  } else if (eventType == event::HANMEM) {
    HANMEM_data* hanmem_data = static_cast<HANMEM_data *>(data);
    fmt::print("[HANMEM] {} instr_id: {} fetch completed\n", __func__, hanmem_data->instr->instr_id);
  } else if (eventType == event::FINISH) {
    FINISH_data* finish_data = static_cast<FINISH_data *>(data);
    fmt::print("[LSQ] {} instr_id: {} full_address: {} remain_mem_ops: {}\n", __func__, finish_data->rob_entry->instr_id, finish_data->virtual_address, finish_data->rob_entry->num_mem_ops() - finish_data->rob_entry->completed_mem_ops);
  // from vmem.cc
  } else if (eventType == event::VA_TO_PA) {
    VA_TO_PA_data* v_data = static_cast<VA_TO_PA_data *>(data);
    fmt::print("[VMEM] va_to_pa paddr: {} vaddr: {} fault: {}\n", v_data->paddr, v_data->vaddr, v_data->fault);
  } else if (eventType == event::GET_PTE_PA) {
    GET_PTE_PA_data* g_data = static_cast<GET_PTE_PA_data *>(data);
    fmt::print("[VMEM] get_pta_pa paddr: {} vaddr: {} pt_page_offset: {} translation_level: {} fault: {}\n", g_data->paddr, g_data->vaddr, g_data->offset, g_data->level, g_data->fault);
  // from channel.cc
  } else if (eventType == event::ADD_RQ) {
    ADD_RQ_data* a_data = static_cast<ADD_RQ_data *>(data);
    fmt::print("[channel_rq] add_rq instr_id: {} address: {} v_address: {} type: {}\n", a_data->instr_id, a_data->address, a_data->v_address,
               access_type_names.at(champsim::to_underlying(a_data->type)));
  } else if (eventType == event::ADD_WQ) {
    ADD_WQ_data* a_data = static_cast<ADD_WQ_data *>(data);
    fmt::print("[channel_wq] add_wq instr_id: {} address: {} v_address: {} type: {}\n", a_data->instr_id, a_data->address, a_data->v_address,
               access_type_names.at(champsim::to_underlying(a_data->type)));
  } else if (eventType == event::ADD_PQ) {
    ADD_PQ_data* a_data = static_cast<ADD_PQ_data *>(data);
    fmt::print("[channel_pq] add_pq instr_id: {} address: {} v_address: {} type: {}\n", a_data->instr_id, a_data->address, a_data->v_address,
               access_type_names.at(champsim::to_underlying(a_data->type)));
  // from ptw.cc
  } else if (eventType == event::PTW_HANDLE_READ) {
    PTW_HANDLE_READ_data* p_data = static_cast<PTW_HANDLE_READ_data *>(data);
    fmt::print("[handle_read] handle_read address: {} v_address: {} pt_page_offset: {} translation_level: {} cycle: {}\n", p_data->address, p_data->v_address, p_data->pt_page_offset, p_data->translation_level, p_data->cycle);
  } else if (eventType == event::PTW_HANDLE_FILL) {
    PTW_HANDLE_FILL_data* p_data = static_cast<PTW_HANDLE_FILL_data *>(data);
    fmt::print("[handle_fill] handle_fill address: {} v_address: {} data: {} pt_page_offset: {} translation_level: {} cycle: {}\n", p_data->address, p_data->v_address, p_data->data, p_data->pt_page_offset, p_data->translation_level, p_data->cycle);
  } else if (eventType == event::PTW_OPERATE) {
    PTW_OPERATE_data* p_data = static_cast<PTW_OPERATE_data *>(data);
    fmt::print("[operate] operate MSHR contents: {} cycle: {}\n", p_data->mshr_addresses, p_data->cycle);
  } else if (eventType == event::PTW_FINISH_PACKET) {
    PTW_FINISH_PACKET_data* p_data = static_cast<PTW_FINISH_PACKET_data *>(data);
    fmt::print("[finish_packet] finish_packet address: {} v_address: {} data: {} translation_level: {} cycle: {} penalty: {}\n", p_data->address, p_data->v_address, p_data->data, p_data->translation_level, p_data->cycle, p_data->penalty);
  } else if (eventType == event::PTW_FINISH_PACKET_LAST_STEP) {
    PTW_FINISH_PACKET_LAST_STEP_data* p_data = static_cast<PTW_FINISH_PACKET_LAST_STEP_data *>(data);
    fmt::print("[complete_packet] complete_packet address: {} v_address: {} data: {} translation_level: {} cycle: {} penalty: {}\n", p_data->address, p_data->v_address, p_data->data, p_data->translation_level, p_data->cycle, p_data->penalty);
  // from cache.cc
  } else if (eventType == event::CACHE_MERGE) {
    CACHE_MERGE_data* c_data = static_cast<CACHE_MERGE_data *>(data);
    if (c_data->successor.type == access_type::PREFETCH) {
      fmt::print("[MSHR] merge address {} type: {} into address {} type: {}\n", c_data->successor.address, access_type_names.at(champsim::to_underlying(c_data->successor.type)), c_data->predecessor.address, access_type_names.at(champsim::to_underlying(c_data->successor.type)));
    } else {
      fmt::print("[MSHR] merge address {} type: {} into address {} type: {}\n", c_data->predecessor.address, access_type_names.at(champsim::to_underlying(c_data->predecessor.type)), c_data->successor.address, access_type_names.at(champsim::to_underlying(c_data->successor.type)));
    }
  } else if (eventType == event::CACHE_HANDLE_FILL) {
    CACHE_HANDLE_FILL_data* c_data = static_cast<CACHE_HANDLE_FILL_data *>(data);
    fmt::print("[{}] handle_fill instr_id: {} address: {} v_address: {} set: {} way: {} type: {} prefetch_metadata: {} cycle_enqueued: {} cycle: {}\n", c_data->NAME, c_data->mshr.instr_id, c_data->mshr.address, c_data->mshr.v_address, c_data->set, c_data->way, access_type_names.at(champsim::to_underlying(c_data->mshr.type)), c_data->mshr.data_promise->pf_metadata, c_data->cycle_enqueued, c_data->cycle);
  } else if (eventType == event::CACHE_HANDLE_WRITEBACK) {
    CACHE_HANDLE_WRITEBACK_data* c_data = static_cast<CACHE_HANDLE_WRITEBACK_data *>(data);
    fmt::print("[{}] handle_writeback evict address: {} v_address: {} prefetch_metadata: {}\n", c_data->NAME, c_data->address, c_data->v_address, c_data->mshr.data_promise->pf_metadata);
  } else if (eventType == event::CACHE_TRY_HIT) {
    CACHE_TRY_HIT_data* c_data = static_cast<CACHE_TRY_HIT_data *>(data);
    fmt::print("[{}] try_hit instr_id: {} address: {} v_address: {} data: {} set: {} way: {} ({}) type: {} cycle: {}\n", c_data->NAME, c_data->instr_id, c_data->address, c_data->v_address, c_data->data, c_data->set, c_data->way, c_data->hit ? "HIT" : "MISS", access_type_names.at(champsim::to_underlying(c_data->type)), c_data->cycle);
  } else if (eventType == event::CACHE_HANDLE_MISS) {
    CACHE_HANDLE_MISS_data* c_data = static_cast<CACHE_HANDLE_MISS_data *>(data);
    fmt::print("[{}] handle_miss instr_id: {} address: {} v_address: {} type: {} local_prefetch: {} cycle: {}\n", c_data->NAME, c_data->instr_id, c_data->address, c_data->v_address, access_type_names.at(champsim::to_underlying(c_data->type)), c_data->prefetch_from_this, c_data->cycle);
  } else if (eventType == event::CACHE_HANDLE_WRITE) {
    CACHE_HANDLE_WRITE_data* c_data = static_cast<CACHE_HANDLE_WRITE_data *>(data);
    fmt::print("[{}] handle_write instr_id: {} address: {} v_address: {} type: {} local_prefetch: {} cycle: {}\n", c_data->NAME, c_data->instr_id, c_data->address, c_data->v_address, access_type_names.at(champsim::to_underlying(c_data->type)), c_data->prefetch_from_this, c_data->cycle);
  } else if (eventType == event::CACHE_INITIATE_TAG_CHECK) {
    CACHE_INITIATE_TAG_CHECK_data* c_data = static_cast<CACHE_INITIATE_TAG_CHECK_data *>(data);
    fmt::print("[TAG] initiate_tag_check instr_id: {} address: {} v_address: {} type: {} response_requested: {}\n", c_data->instr_id, c_data->address, c_data->v_address, access_type_names.at(champsim::to_underlying(c_data->type)), c_data->response_requested);
  } else if (eventType == event::CACHE_OPERATE) {
    CACHE_OPERATE_data* c_data = static_cast<CACHE_OPERATE_data *>(data);
    fmt::print("[{}] operate cycle completed: {} tags checked: {} remaining: {} stash consumed: {} remaining: {} channel consumed: {} pq consumed {} unused consume bw {}\n", c_data->NAME, c_data->cycle, c_data->tags_checked, c_data->tags_remaining, c_data->stash_consumed, c_data->stash_remaining, c_data->channel_consumed, c_data->pq_consumed, c_data->unused_bw);
  } else if (eventType == event::CACHE_FINISH_PACKET) {
    CACHE_FINISH_PACKET_data* c_data = static_cast<CACHE_FINISH_PACKET_data *>(data);
    fmt::print("[{}_MSHR] finish_packet instr_id: {} address: {} data: {} type: {} current: {}\n", c_data->NAME, c_data->mshr.instr_id, c_data->mshr.address, c_data->mshr.data_promise->data, access_type_names.at(champsim::to_underlying(c_data->mshr.type)), c_data->cycle);
  } else if (eventType == event::CACHE_FINISH_TRANSLATION) {
    CACHE_FINISH_TRANSLATION_data* c_data = static_cast<CACHE_FINISH_TRANSLATION_data *>(data);
    fmt::print("[{}_TRANSLATE] finish_translation old: {} paddr: {} vaddr: {} type: {} cycle: {}\n", c_data->NAME, c_data->old_addr, c_data->p_addr, c_data->v_addr, access_type_names.at(champsim::to_underlying(c_data->type)), c_data->cycle);
  } else if (eventType == event::CACHE_ISSUE_TRANSLATION) {
    CACHE_ISSUE_TRANSLATION_data* c_data = static_cast<CACHE_ISSUE_TRANSLATION_data *>(data);
    fmt::print("[TRANSLATE] do_issue_translation instr_id: {} paddr: {} vaddr: {} type: {}\n", c_data->instr_id, c_data->p_addr, c_data->v_addr, access_type_names.at(champsim::to_underlying(c_data->type)));
  }
}
