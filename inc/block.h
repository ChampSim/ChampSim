#ifndef BLOCK_H
#define BLOCK_H

#include "champsim.h"
#include "instruction.h"
#include "circular_buffer.hpp"

#include <algorithm>
#include <vector>

class MemoryRequestProducer;
class LSQ_ENTRY;

// message packet
class PACKET {
  public:
    bool scheduled = false,
         returned  = false;

    uint8_t type = 0,
            fill_level = 0,
            pf_origin_level = 0;

    uint16_t asid = std::numeric_limits<uint16_t>::max();

    uint32_t pf_metadata;
    uint32_t cpu = NUM_CPUS;

    uint64_t address = 0,
             v_address = 0,
             data = 0,
             instr_id = 0,
             ip = 0,
             event_cycle = std::numeric_limits<uint64_t>::max(),
             cycle_enqueued = 0;

    std::vector<std::vector<LSQ_ENTRY>::iterator> lq_index_depend_on_me = {}, sq_index_depend_on_me = {};
    std::vector<champsim::circular_buffer<ooo_model_instr>::iterator> instr_depend_on_me;
    std::vector<MemoryRequestProducer*> to_return;

	uint8_t translation_level = 0, init_translation_level = 0; 
};

template <>
struct is_valid<PACKET>
{
    is_valid() {}
    bool operator()(const PACKET &test)
    {
        return test.address != 0;
    }
};

template <>
struct eq_addr<PACKET>
{
    using argument_type = PACKET;
    using addr_type = decltype(argument_type::address);
    using asid_type = decltype(argument_type::asid);

    const asid_type match_asid;
    const addr_type match_addr;
    const std::size_t shamt;

    eq_addr(asid_type asid, addr_type addr, std::size_t shamt = 0) : match_asid(asid), match_addr(addr), shamt(shamt) {}

    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && test.asid == match_asid && (test.address >> shamt) == (match_addr >> shamt);
    }
};

template <typename LIST>
void packet_dep_merge(LIST &dest, LIST &src)
{
    dest.reserve(std::size(dest) + std::size(src));
    auto middle = std::end(dest);
    dest.insert(middle, std::begin(src), std::end(src));
    std::inplace_merge(std::begin(dest), middle, std::end(dest));
    auto uniq_end = std::unique(std::begin(dest), std::end(dest));
    dest.erase(uniq_end, std::end(dest));
}

// load/store queue
struct LSQ_ENTRY {
    uint64_t instr_id = 0,
             producer_id = std::numeric_limits<uint64_t>::max(),
             virtual_address = 0,
             physical_address = 0,
             ip = 0,
             event_cycle = 0;

    champsim::circular_buffer<ooo_model_instr>::iterator rob_index;

    uint8_t translated = 0,
            fetched = 0;

    uint16_t asid = std::numeric_limits<uint16_t>::max();
};

template <>
class is_valid<LSQ_ENTRY>
{
    public:
        bool operator() (const LSQ_ENTRY &test)
        {
            return test.virtual_address != 0;
        }
};

#endif

