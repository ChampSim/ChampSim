#include "dram_interface.h"

void DRAM_CONTROLLER::read_complete(unsigned id, uint64_t address, uint64_t clock_cycle)
{
    PACKET packet;
    packet.address = address;
    int index = check_dram_queue(&RQ, &packet);

    if (index == -1)
        abort();

    uint32_t op_cpu = RQ.entry[index].cpu;
    upper_level_dcache[op_cpu]->return_data(&RQ.entry[index]);
    RQ.remove_queue(&RQ.entry[index]);

}

void DRAM_CONTROLLER::write_complete(unsigned id, uint64_t address, uint64_t clock_cycle)
{
    PACKET packet;
    packet.address = address;
    int index = check_dram_queue(&WQ, &packet);

    if (index == -1)
        abort();

    WQ.remove_queue(&RQ.entry[index]);

}

int DRAM_CONTROLLER::check_dram_queue(PACKET_QUEUE *queue, PACKET *packet)
{
    // search queue
    for (uint32_t index=0; index<queue->SIZE; index++) {
        if (queue->entry[index].address == packet->address) {
            return index;
        }
    }

    return -1;
}
void DRAM_CONTROLLER::operate()
{
    mem->update();
}

int DRAM_CONTROLLER::add_rq(PACKET *packet)
{
    int index;

    // simply return read requests with dummy response before the warmup
    if (all_warmup_complete < NUM_CPUS) {
        if (packet->instruction)
            upper_level_icache[packet->cpu]->return_data(packet);
        else // data
            upper_level_dcache[packet->cpu]->return_data(packet);

        return -1;
    }

    // search for the empty index
    for (index=0; index<DRAM_RQ_SIZE; index++) {
        if (RQ.entry[index].address == 0) {
            memcpy(&RQ.entry[index], packet, sizeof(PACKET));
            RQ.occupancy++;
            break;
        }
    }

    //Call DRAMSim2
    mem->addTransaction(false, packet->address);
    return index;
}

int DRAM_CONTROLLER::add_wq(PACKET *packet)
{
    int index;

    // simply drop write requests before the warmup
    if (all_warmup_complete < NUM_CPUS)
        return -1;

    // search for the empty index
    for (index=0; index<DRAM_WQ_SIZE; index++) {
        if (WQ.entry[index].address == 0) {
            memcpy(&WQ.entry[index], packet, sizeof(PACKET));
            WQ.occupancy++;
            break;
        }
    }

    //Call DRAMSim2
    mem->addTransaction(true, packet->address);
    return index;
}

int DRAM_CONTROLLER::add_pq(PACKET *packet)
{
    return -1;
}

void DRAM_CONTROLLER::return_data(PACKET *packet)
{

}

uint32_t DRAM_CONTROLLER::get_occupancy(uint8_t queue_type, uint64_t address)
{
    return -1;
}

uint32_t DRAM_CONTROLLER::get_size(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 1)
        return RQ.SIZE;
    else if (queue_type == 2)
        return WQ.SIZE;

    return 0;
}

void DRAM_CONTROLLER::increment_WQ_FULL(uint64_t address)
{

}
