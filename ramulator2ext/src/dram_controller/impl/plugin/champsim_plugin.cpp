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
#include "addr_mapper/addr_mapper.h"
#include "memory_system/memory_system.h"
namespace Ramulator
{
class ChampSimPlugin : public IControllerPlugin, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IControllerPlugin, ChampSimPlugin, "ChampSimPlugin", "Collects DRAM information for ChampSim")

  double cycles = 0;
  long   progress = 0;
  private:
    IDRAM* m_dram = nullptr;
    IDRAMController* m_controller = nullptr;
    IMemorySystem* m_system = nullptr;
    IAddrMapper* m_mapper = nullptr;


  public:
    static std::vector<ChampSimPlugin*> channel_plugins;
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
      m_mapper = m_system->get_ifce<IAddrMapper>();

      stats["DBUS_CYCLE_CONGESTED"] = 0;
      stats["DBUS_COUNT_CONGESTED"] = 0;
      stats["REFRESH_CYCLES"]       = 0;
      stats["WQ_ROW_BUFFER_HIT"]   = 0;
      stats["WQ_ROW_BUFFER_MISS"] = 0;
      stats["RQ_ROW_BUFFER_HIT"]   = 0;
      stats["RQ_ROW_BUFFER_MISS"] = 0;
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
        else if(m_dram->m_command_meta(req_it->command).is_opening && m_dram->m_command_meta(req_it->final_command).is_accessing) //opened row
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
      progress++;
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

    size_t get_field(std::string field, Request& req)
    {
      m_mapper->apply(req);

      //champsim doesn't know about bankgroups, so we need to do some
      //math here to merge banks and bankgroups together
      if(field == "bank")
      {
        if(m_dram->m_organization.count.size() > 5)
        {
          uint64_t bank_count = m_dram->get_level_size("bank");
          return(req.addr_vec[m_dram->m_levels("bank")] + bank_count*req.addr_vec[m_dram->m_levels("bankgroup")]);
        }
        else
          return(req.addr_vec[m_dram->m_levels(field)]);
      }
      else
        return(req.addr_vec[m_dram->m_levels(field)]);
    }

    size_t get_field_size(std::string field)
    {
      if(field == "bank")
        return(m_dram->get_level_size(field) * m_dram->get_level_size("bankgroup"));
      else
        return(m_dram->get_level_size(field));
    }

    long get_progress()
    {
      long temp_progress = progress;
      progress = 0;
      return(temp_progress);
    }

    uint64_t get_size()
    {
      uint64_t chips = m_dram->m_channel_width / m_dram->m_organization.dq;
      return((m_dram->m_organization.density * (1ull << 20ull)) * chips * m_dram->get_level_size("rank") / 8);
    }

    uint64_t get_channel_width()
    {
      return(m_dram->m_channel_width/8);
    }
};

double get_ramulator_stat(std::string stat_name, size_t channel_no)
{
  return(ChampSimPlugin::channel_plugins[channel_no]->stats[stat_name]);
}

size_t translate_to_ramulator_addr_field (std::string field, int64_t addr)
{
  Request req = {addr, int(Request::Type::Read), 0, nullptr};
  return(ChampSimPlugin::channel_plugins[0]->get_field(field,req));
}

size_t get_ramulator_field_size (std::string field)
{
  return(ChampSimPlugin::channel_plugins[0]->get_field_size(field));
}

uint64_t get_ramulator_size()
{
  uint64_t dram_size = 0;
  for(auto chan : ChampSimPlugin::channel_plugins)
    dram_size += chan->get_size();
  return(dram_size);
}

uint64_t get_ramulator_channel_width()
{
  return(ChampSimPlugin::channel_plugins[0]->get_channel_width());
}

long get_ramulator_progress()
{
  long progress = 0;
  for(auto plugin : ChampSimPlugin::channel_plugins)
    progress += plugin->get_progress();
  
  return(progress);
}
std::vector<ChampSimPlugin*> ChampSimPlugin::channel_plugins;
}
