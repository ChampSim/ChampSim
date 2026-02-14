#include <catch.hpp>
#include <typeinfo>
#include <iostream>

#include "listeners/heartbeat.h"
#include "instruction.h"
#include "trace_instruction.h"

namespace
{

TEST_CASE("The heartbeat listener prints one line after 10M instructions retired") {
    std::ostringstream stdout{};
    Heartbeat uut{&stdout};
    
    // begin phase event
    bool in_warmup = false;
    uut.handle_event<Event::BEGIN_PHASE>(in_warmup);
    
    for (int i = 0; i < 5000000; ++i) {
        std::deque<ooo_model_instr> fake_instructions{{ooo_model_instr(0, input_instr()), ooo_model_instr(0, input_instr())}};
        uint32_t cpu = 0;
        uint64_t curr_cycles = i;
        auto cb = std::cbegin(fake_instructions);
        auto ce = std::cend(fake_instructions);
        uut.handle_event<Event::RETIRE>(cpu, cb, ce, curr_cycles);
    }
    
    std::string res = stdout.str();
    
    std::string rest = res.substr(res.find('\n')+1);
    
    REQUIRE_THAT(res, Catch::Matchers::StartsWith("Heartbeat CPU 0 instructions: 10000000 cycles: 4999999 heartbeat IPC: 2 cumulative IPC: 2 "));
    REQUIRE(rest.length() < 2);
}

TEST_CASE("The heartbeat listener prints cumulative and heartbeat IPC correctly after a phase change") {
    std::ostringstream stdout{};
    Heartbeat uut{&stdout};
    
    // begin phase event
    bool in_warmup = true;
    uut.handle_event<Event::BEGIN_PHASE>(in_warmup);
    
    for (int i = 0; i <11000000; ++i) {
        
        // phase change to simulation
        if (i == 5000000) {
          in_warmup = false;
          uut.handle_event<Event::BEGIN_PHASE>(in_warmup);
        }
      
        // warmup behavior (4 IPC)
        if (i < 4000000) {
          std::deque<ooo_model_instr> fake_instructions{{ooo_model_instr(0, input_instr()), ooo_model_instr(0, input_instr()), ooo_model_instr(0, input_instr()), ooo_model_instr(0, input_instr())}};
          uint32_t cpu = 0;
          uint64_t curr_cycles = i;
          auto cb = std::cbegin(fake_instructions);
          auto ce = std::cend(fake_instructions);
          uut.handle_event<Event::RETIRE>(cpu, cb, ce, curr_cycles);
        } else {
          // simulation behavior (2 IPC)
          std::deque<ooo_model_instr> fake_instructions{{ooo_model_instr(0, input_instr()), ooo_model_instr(0, input_instr())}};
          uint32_t cpu = 0;
          uint64_t curr_cycles = i;
          auto cb = std::cbegin(fake_instructions);
          auto ce = std::cend(fake_instructions);
          uut.handle_event<Event::RETIRE>(cpu, cb, ce, curr_cycles);
        }
    }
    
    std::string res = stdout.str();
    
    std::string l1 = res.substr(0, res.find('\n')+1);
    std::string rest = res.substr(res.find('\n')+1);
    std::string l2 = rest.substr(0, rest.find('\n')+1);
    rest = rest.substr(rest.find('\n')+1);
    std::string l3 = rest.substr(0, rest.find('\n')+1);
    rest = rest.substr(rest.find('\n')+1);
    
    REQUIRE_THAT(l1, Catch::Matchers::StartsWith("Heartbeat CPU 0 instructions: 10000000 cycles: 2499999 heartbeat IPC: 4 cumulative IPC: 4 "));
    REQUIRE_THAT(l2, Catch::Matchers::StartsWith("Heartbeat CPU 0 instructions: 20000000 cycles: 5999999 heartbeat IPC: 2.857 cumulative IPC: 2 "));
    REQUIRE_THAT(l3, Catch::Matchers::StartsWith("Heartbeat CPU 0 instructions: 30000000 cycles: 10999999 heartbeat IPC: 2 cumulative IPC: 2 "));
    REQUIRE(rest.length() < 2);
}

TEST_CASE("The heartbeat listener prints correctly with multiple CPUs") {
    std::ostringstream stdout{};
    Heartbeat uut{&stdout};
    
    // begin phase event
    bool in_warmup = true;
    uut.handle_event<Event::BEGIN_PHASE>(in_warmup);
    
    for (int i = 0; i < 5000000; ++i) {
        
        // simulation behavior (2 IPC)
        std::deque<ooo_model_instr> fake_instructions{{ooo_model_instr(0, input_instr()), ooo_model_instr(0, input_instr())}};
        uint32_t cpu = 0;
        uint64_t curr_cycles = i;
        auto cb = std::cbegin(fake_instructions);
        auto ce = std::cend(fake_instructions);
        uut.handle_event<Event::RETIRE>(cpu, cb, ce, curr_cycles);
        cpu++;
        uut.handle_event<Event::RETIRE>(cpu, cb, ce, curr_cycles);
        cpu++;
        uut.handle_event<Event::RETIRE>(cpu, cb, ce, curr_cycles);
        cpu++;
        uut.handle_event<Event::RETIRE>(cpu, cb, ce, curr_cycles);
        
    }
    
    std::string res = stdout.str();
    
    std::string l1 = res.substr(0, res.find('\n')+1);
    std::string rest = res.substr(res.find('\n')+1);
    std::string l2 = rest.substr(0, rest.find('\n')+1);
    rest = rest.substr(rest.find('\n')+1);
    std::string l3 = rest.substr(0, rest.find('\n')+1);
    rest = rest.substr(rest.find('\n')+1);
    std::string l4 = rest.substr(0, rest.find('\n')+1);
    rest = rest.substr(rest.find('\n')+1);
    
    REQUIRE_THAT(l1, Catch::Matchers::StartsWith("Heartbeat CPU 0 instructions: 10000000 cycles: 4999999 heartbeat IPC: 2 cumulative IPC: 2 "));
    REQUIRE_THAT(l2, Catch::Matchers::StartsWith("Heartbeat CPU 1 instructions: 10000000 cycles: 4999999 heartbeat IPC: 2 cumulative IPC: 2 "));
    REQUIRE_THAT(l3, Catch::Matchers::StartsWith("Heartbeat CPU 2 instructions: 10000000 cycles: 4999999 heartbeat IPC: 2 cumulative IPC: 2 "));
    REQUIRE_THAT(l4, Catch::Matchers::StartsWith("Heartbeat CPU 3 instructions: 10000000 cycles: 4999999 heartbeat IPC: 2 cumulative IPC: 2 "));
    REQUIRE(rest.length() < 2);
}

}