add_library(ramulator-controller OBJECT)

target_sources(
  ramulator-controller PRIVATE
  bh_controller.h 
  bh_scheduler.h 
  controller.h 
  scheduler.h 
  plugin.h
  refresh.h

  impl/bh_dram_controller.cpp
  impl/dummy_controller.cpp
  impl/generic_dram_controller.cpp
  
  impl/scheduler/bh_scheduler.cpp
  impl/scheduler/blocking_scheduler.cpp
  impl/scheduler/generic_scheduler.cpp

  impl/refresh/all_bank_refresh.cpp

  # impl/plugin/trr.cpp
  impl/plugin/trace_recorder.cpp
  impl/plugin/cmd_counter.cpp
  impl/plugin/para.cpp
  impl/plugin/graphene.cpp
  impl/plugin/oracle_rh.cpp
  impl/plugin/twice.cpp
  impl/plugin/hydra.cpp
  impl/plugin/rrs.cpp
  impl/plugin/champsim_stats.cpp

  impl/plugin/blockhammer/blockhammer_throttler.h 
  impl/plugin/blockhammer/blockhammer_util.h 
  impl/plugin/blockhammer/blockhammer.cpp 
  impl/plugin/blockhammer/blockhammer.h 
)

target_link_libraries(
  ramulator
  PRIVATE
  ramulator-controller
)
