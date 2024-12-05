.. _Legacy_Modules:

====================================
The Legacy ChampSim Module System
====================================

Previous versions of ChampSim used the following module system.
Many modules exist in published artifacts, so documentation is included here.
New work should use the updated module system.

Legacy modules can be enabled by adding an empty file named "__legacy__" in the same directory as the module sources.
This is the preferred method.
Alternatively, ChampSim can be configured with (for example)::

    {
        "L2C": {
            "prefetcher" {
                "path": "../path/to/module",
                "legacy": true
            }
        }
    }

ChampSim uses four kinds of modules:

* Branch Direction Predictors
* Branch Target Predictors
* Memory Prefetchers
* Cache Replacement Policies

Each of these is implemented as a set of hook functions. Each hook must be implemented, or compilation will fail.

----------------------------
Branch Predictors
----------------------------

A branch predictor module must implement three functions.

::

  void O3_CPU::initialize_branch_predictor()

This function is called when the core is initialized. You can use it to initialize elements of dynamic structures, such as `std::vector` or `std::map`.

::

  uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)

This function is called when a prediction is needed. The parameters passed are:

* ip: The instruction pointer of the branch
* predicted_target: The predicted target of the branch. This is passed directly from the branch target predictor module and may be incorrect.
* always_taken: A boolean value. This parameter will be nonzero if the branch target predictor determines that the branch is always taken.
* branch_type: One of the following

  * `BRANCH_DIRECT_JUMP`: Direct non-call, unconditional branches, whose target is encoded in the instruction
  * `BRANCH_INDIRECT`: Indirect non-call, unconditional branches, whose target is stored in a register
  * `BRANCH_CONDITIONAL`: A direct conditional branch
  * `BRANCH_DIRECT_CALL`: A call to a procedure whose target is encoded in the instruction
  * `BRANCH_INDIRECT_CALL`: A call to a procedure whose target is stored in a register
  * `BRANCH_RETURN`: A return to a calling procedure
  * `BRANCH_OTHER`: If the branch type cannot be determined

::

  void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)


This function is called when a branch is resolved. The parameters are the same as in the previous hook, except that the last three are guaranteed to be correct.

-----------------------------------
Branch Target Buffers
-----------------------------------

A BTB module must implement three functions.

::

  void O3_CPU::initialize_btb()

This function is called when the core is initialized. You can use it to initialize elements of dynamic structures, such as `std::vector` or `std::map`.

::

  std::pair<uint64_t, bool> O3_CPU::btb_prediction(uint64_t ip)

This function is called when a prediction is needed. The parameters passed are:

* ip: The instruction pointer of the branch

The function should return a pair containing the predicted address and a boolean that describes if the branch is known to be always taken. If the prediction fails, the function should return a default-initialized address, e.g. `uint64_t{}`.

::

  void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)


This function is called when a branch is resolved. The parameters are:

* ip: The instruction pointer of the branch
* branch_target: The correct target of the branch.
* taken: A boolean value. This parameter will be nonzero if the branch was taken.
* branch_type: One of the following

  * `BRANCH_DIRECT_JUMP`: Direct non-call, unconditional branches, whose target is encoded in the instruction
  * `BRANCH_INDIRECT`: Indirect non-call, unconditional branches, whose target is stored in a register
  * `BRANCH_CONDITIONAL`: A direct conditional branch
  * `BRANCH_DIRECT_CALL`: A call to a procedure whose target is encoded in the instruction
  * `BRANCH_INDIRECT_CALL`: A call to a procedure whose target is stored in a register
  * `BRANCH_RETURN`: A return to a calling procedure
  * `BRANCH_OTHER`: If the branch type cannot be determined

-----------------------------------
Memory Prefetchers
-----------------------------------

A prefetcher module must implement five or six functions.

::

  void CACHE::prefetcher_initialize()

This function is called when the cache is initialized. You can use it to initialize elements of dynamic structures, such as `std::vector` or `std::map`.

::

  uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in);

This function is called when a tag is checked in the cache. The parameters passed are:

* addr: the address of the packet. If this is the first-level cache, the offset bits are included. Otherwise, the offset bits are zero. If the cache was configured with `"virtual_prefetch": true`, this address will be a virtual address. Otherwise, this is a physical address.
* ip: the address of the instruction that initiated the demand. If the packet is a prefetch from another level, this value will be 0.
* cache_hit: if this tag check is a hit, this value is nonzero. Otherwise, it is 0.
* useful_prefetch: if this tag check hit a prior prefetch, this value is true.
* type: the result of `static_cast<std::underlying_type_t<access_type>>(v)` for v in:
  * `access_type::LOAD`
  * `access_type::RFO`
  * `access_type::PREFETCH`
  * `access_type::WRITE`
  * `access_type::TRANSLATION`
* metadata_in: the metadata carried along by the packet.

The function should return metadata that will be stored alongside the block.

::

  uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_way, uint8_t prefetch, uint32_t metadata_in);

This function is called when a miss is filled in the cache. The parameters passed are:

* addr: the address of the packet. If this is the first-level cache, the offset bits are included. Otherwise, the offset bits are zero. If the cache was configured with `"virtual_prefetch": true`, this address will be a virtual address. Otherwise, this is a physical address.
* set: the set that the fill occurred in
* way: the way that the fill occurred in, or `this->NUM_WAY` if a bypass occurred
* prefetch: if this tag check hit a prior prefetch, this value is true.
* metadata_in: the metadata carried along by the packet.

The function should return metadata that will be stored alongside the block.

::

  void CACHE::prefetcher_cycle_operate();


This function is called each cycle, after all other operation has completed.

::

  void CACHE::prefetcher_final_stats();


This function is called at the end of the simulation and can be used to print statistics.


::

  void CACHE::prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target);


This function must be implemented by instruction prefetchers. The parameters passed are:

* ip: The instruction pointer of the branch
* branch_type: One of the following

  * `BRANCH_DIRECT_JUMP`: Direct non-call, unconditional branches, whose target is encoded in the instruction
  * `BRANCH_INDIRECT`: Indirect non-call, unconditional branches, whose target is stored in a register
  * `BRANCH_CONDITIONAL`: A direct conditional branch
  * `BRANCH_DIRECT_CALL`: A call to a procedure whose target is encoded in the instruction
  * `BRANCH_INDIRECT_CALL`: A call to a procedure whose target is stored in a register
  * `BRANCH_RETURN`: A return to a calling procedure
  * `BRANCH_OTHER`: If the branch type cannot be determined

* branch_target: The instruction pointer of the target

-----------------------------------
Replacement Policies
-----------------------------------

A replacement policy module must implement four functions.

::

  void CACHE::initialize_replacement()

This function is called when the cache is initialized. You can use it to initialize elements of dynamic structures, such as `std::vector` or `std::map`.

::

  uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t addr, uint32_t type);

This function is called when a tag is checked in the cache. The parameters passed are:

* triggering_cpu: the core index that initiated this fill
* instr_id: an instruction count that can be used to examine the program order of requests.
* set: the set that the fill occurred in.
* current_set: a pointer to the beginning of the set being accessed.
* ip: the address of the instruction that initiated the demand. If the packet is a prefetch from another level, this value will be 0.
* addr: the address of the packet. If this is the first-level cache, the offset bits are included. Otherwise, the offset bits are zero. If the cache was configured with `"virtual_prefetch": true`, this address will be a virtual address. Otherwise, this is a physical address.
* type: the result of `static_cast<std::underlying_type_t<access_type>>(v)` for v in:

  * `access_type::LOAD`
  * `access_type::RFO`
  * `access_type::PREFETCH`
  * `access_type::WRITE`
  * `access_type::TRANSLATION`

The function should return the way index that should be evicted, or `this->NUM_WAY` to indicate that a bypass should occur.

::

  void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t addr, uint64_t ip, uint64_t victim_addr, uint8_t hit);

This function is called when a hit occurs or a miss is filled in the cache. The parameters passed are:

* triggering_cpu: the core index that initiated this fill
* set: the set that the fill occurred in.
* way: the way that the fill occurred in.
* addr: the address of the packet. If this is the first-level cache, the offset bits are included. Otherwise, the offset bits are zero. If the cache was configured with `"virtual_prefetch": true`, this address will be a virtual address. Otherwise, this is a physical address.
* ip: the address of the instruction that initiated the demand. If the packet is a prefetch from another level, this value will be 0.
* victim_addr: the address of the evicted block, if this is a miss. If this is a hit, the value is 0.
* type: the result of `static_cast<std::underlying_type_t<access_type>>(v)` for v in:

  * `access_type::LOAD`
  * `access_type::RFO`
  * `access_type::PREFETCH`
  * `access_type::WRITE`
  * `access_type::TRANSLATION`

The function should return metadata that will be stored alongside the block.

::

  void CACHE::replacement_final_stats();


This function is called at the end of the simulation and can be used to print statistics.

