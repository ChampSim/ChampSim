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

  double cycles = 0;
  private:
    IDRAM* m_dram = nullptr;
    IDRAMController* m_controller = nullptr;
    IMemorySystem* m_system = nullptr;


  public:
    static std::vector<ChampSimStatsPlugin*> channel_plugins;
    std::map<std::string, double> stats;
    double rrb_miss = 0;
    double rrb_hits = 0;
    double wrb_miss = 0;
    double wrb_hits = 0;

    double refreshes = 0;
    double dbus_congestion = 0;
    double dbus_congested_cycles = 0;
    double dbus_packets = 0;

    void init() override
    {
      channel_plugins.push_back(this);
    };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {
      m_ctrl = cast_parent<IDRAMController>();
      m_dram = m_ctrl->m_dram;
      m_controller = m_ctrl;
      m_system = memory_system;

      stats["DBUS_CYCLE_CONGESTED"] = 0;
      stats["DBUS_COUNT_CONGESTED"] = 0;
      stats["REFRESH_CYCLES"]       = 0;
      stats["WQ_ROW_BUFFER_HIT"]   = 0;
      stats["WQ_ROW_BUFFER_MISS"] = 0;
      stats["RQ_ROW_BUFFER_HIT"]   = 0;
      stats["RQ_ROW_BUFFER_MISS"] = 0;

      //size in MB * # of ranks
      stats["SIZE"] = (m_dram->m_organization.density * (1ull << 20ull)) * m_dram->get_level_size("rank");
    };

    void update(bool request_found, ReqBuffer::iterator& req_it) override {
      if (request_found) {
        if(m_dram->m_command_meta(req_it->command).is_accessing)
        {
          if(req_it->type_id == Ramulator::Request::Type::Write)
          wrb_hits++;
          else if(req_it->type_id == Ramulator::Request::Type::Read)
          rrb_hits++;

          dbus_congested_cycles += (cycles - req_it->arrive) - 1;
          dbus_packets++;
         
        }
        if(m_dram->m_command_meta(req_it->command).is_opening && m_dram->m_command_scopes(req_it->command) == m_dram->m_levels("row")) //opened row
        {
          if(req_it->type_id == Ramulator::Request::Type::Write)
          {
            wrb_miss++;
          }
          else if(req_it->type_id == Ramulator::Request::Type::Read)
          {
            rrb_miss++;
          }
          dbus_congested_cycles -= cycles - req_it->arrive;
        }
        else if(m_dram->m_command_meta(req_it->command).is_refreshing) //refreshed
        {
          refreshes++;
        }
      }
      cycles++;
    };

    void finalize() override {
      stats["WQ_ROW_BUFFER_HIT"] = std::max(wrb_hits - wrb_miss,0.0);
      stats["WQ_ROW_BUFFER_MISS"] = wrb_miss;
      stats["RQ_ROW_BUFFER_HIT"] = std::max(rrb_hits - rrb_miss,0.0);
      stats["RQ_ROW_BUFFER_MISS"] = rrb_miss;
      stats["DBUS_CYCLE_CONGESTED"] = dbus_congested_cycles;
      stats["DBUS_COUNT_CONGESTED"] = dbus_packets;
      stats["REFRESH_CYCLES"] = refreshes;
    }
};

double get_ramulator_stat(std::string stat_name, int channel_no)
{
  return(ChampSimStatsPlugin::channel_plugins[channel_no]->stats[stat_name]);
}

std::vector<ChampSimStatsPlugin*> ChampSimStatsPlugin::channel_plugins;
}
