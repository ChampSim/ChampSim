#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <math.h>
#include <random>
#include <string>
#include <vector>

#include "base/base.h"
#include "dram_controller/controller.h"
#include "dram_controller/plugin.h"

namespace Ramulator
{
class ChampSimStatsPlugin : public IControllerPlugin, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IControllerPlugin, ChampSimStatsPlugin, "ChampSimStats", "Collects DRAM statistics for ChampSim")

  double rrb_miss = 0;
  double rrb_hits = 0;
  double wrb_miss = 0;
  double wrb_hits = 0;

  double refreshes = 0;
  double dbus_congestion = 0;
  private:
    IDRAM* m_dram = nullptr;
    IDRAMController* m_controller = nullptr;
    IMemorySystem* m_system = nullptr;


  public:
    void init() override
    {
      
    };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {
      m_ctrl = cast_parent<IDRAMController>();
      m_dram = m_ctrl->m_dram;
      m_controller = m_ctrl;
      m_system = memory_system;

      register_stat(rrb_miss).name("read_rowbuffer_misses");
      register_stat(rrb_hits).name("read_rowbuffer_hits");
      register_stat(wrb_miss).name("write_rowbuffer_misses");
      register_stat(wrb_hits).name("write_rowbuffer_hits");
      register_stat(refreshes).name("total_refreshes");
      register_stat(dbus_congestion).name("dbus_congestion");
    };

    void update(bool request_found, ReqBuffer::iterator& req_it) override {
      if (request_found) {
        int type = req_it->type_id == Ramulator::Request::Type::Write ? 1 : 0;
        if(m_dram->m_command_meta(req_it->command).is_accessing)
        {
          if(req_it->type_id == Ramulator::Request::Type::Write)
          wrb_hits++;
          else if(req_it->type_id == Ramulator::Request::Type::Read)
          rrb_hits++;

          //check for readiness of command
         
        }
        if(m_dram->m_command_meta(req_it->command).is_opening && m_dram->m_command_scopes(req_it->command) == m_dram->m_levels("row")) //opened row
        {
          if(req_it->type_id == Ramulator::Request::Type::Write)
          {
            wrb_miss++;
            wrb_hits--;
          }
          else if(req_it->type_id == Ramulator::Request::Type::Read)
          {
            rrb_miss++;
            rrb_hits--;
          }
        }
        else if(m_dram->m_command_meta(req_it->command).is_refreshing && m_dram->m_command_scopes(req_it->command) == m_dram->m_levels("rank")) //refreshed
        {
          refreshes++;
        }
      }
    };

    void finalize() override {
    }

};
}