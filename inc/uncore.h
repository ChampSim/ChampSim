#ifndef UNCORE_H
#define UNCORE_H

#include "champsim.h"
#include "cache.h"
#include "dram_controller.h"
#include "DRAMSim.h"
//#include "drc_controller.h"

//#define DRC_MSHR_SIZE 48

// uncore
class UNCORE {
  public:

    // LLC
    CACHE LLC{"LLC", LLC_SET, LLC_WAY, LLC_SET*LLC_WAY, LLC_WQ_SIZE, LLC_RQ_SIZE, LLC_PQ_SIZE, LLC_MSHR_SIZE};

    // DRAM
    MEMORY_CONTROLLER DRAM{"DRAM"}; 

    //DRAMSim2
    DRAMSim::MultiChannelMemorySystem *mem = DRAMSim::getMemorySystemInstance("ini/DDR2_micron_16M_8b_x8_sg3E.ini", "system.ini", "./DRAMSim2", "example_app", 16384);

    UNCORE(); 
};

extern UNCORE uncore;

#endif
