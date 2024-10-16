.. _Modules:

====================================
The ChampSim Module System
====================================

ChampSim uses four kinds of modules:

* Branch Direction Predictors
* Branch Target Predictors
* Memory Prefetchers
* Cache Replacement Policies

Modules are implemented as C++ objects.
The module should inherit from one of the following classes:

* ``champsim::modules::branch_predictor``
* ``champsim::modules::btb``
* ``champsim::modules::prefetcher``
* ``champsim::modules::replacement``

The module must be constructible with a ``O3_CPU*`` (for branch predictors and BTBs) or a ``CACHE*`` (for prefetchers and replacement policies).
Such a constructor must call the superclass constructor of the same kind, for example::

    class my_pref : champsim::modules::prefetcher
    {
        public:
        explicit my_pref(CACHE* cache) : champsim::modules::prefetcher(cache) {}
    };

One way to do this, if your module's constructor has an empty body, is to inherit the constructors of the superclass, as with::

    class my_pref : champsim::modules::prefetcher
    {
        public:
        using champsim::modules::prefetcher::prefetcher;
    };

A module may implement any of the listed member functions.
If a member function has overloads listed, any of them may be implemented, and the simulator will select the first candidate overload in the list.

----------------------------
Branch Predictors
----------------------------

A branch predictor module may implement three functions.

.. cpp:function:: void initialize_branch_predictor()

   This function is called when the core is initialized. You can use it to initialize elements of dynamic structures, such as ``std::vector`` or ``std::map``.

.. cpp:function:: bool predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type)
.. cpp:function:: bool predict_branch(uint64_t ip, uint64_t predicted_target, bool always_taken, uint8_t branch_type)
.. cpp:function:: bool predict_branch(champsim::address ip)
.. cpp:function:: bool predict_branch(uint64_t ip)

   This function is called when a prediction is needed.

   :param ip: The instruction pointer of the branch
   :param predicted_target: The predicted target of the branch.
       This is passed directly from the branch target predictor module and may be incorrect.
   :param always_taken: A boolean value.
       This parameter will be nonzero if the branch target predictor determines that the branch is always taken.
   :param branch_type: One of the following

     * ``BRANCH_DIRECT_JUMP``: Direct non-call, unconditional branches, whose target is encoded in the instruction
     * ``BRANCH_INDIRECT``: Indirect non-call, unconditional branches, whose target is stored in a register
     * ``BRANCH_CONDITIONAL``: A direct conditional branch
     * ``BRANCH_DIRECT_CALL``: A call to a procedure whose target is encoded in the instruction
     * ``BRANCH_INDIRECT_CALL``: A call to a procedure whose target is stored in a register
     * ``BRANCH_RETURN``: A return to a calling procedure
     * ``BRANCH_OTHER``: If the branch type cannot be determined

   :return: This function must return true if the branch is predicted taken, and false otherwise.

.. cpp:function:: void last_branch_result(champsim::address ip, champsim::address branch_target, uint8_t taken, uint8_t branch_type)
.. cpp:function:: void last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)

   This function is called when a branch is resolved. The parameters are the same as in the previous hook, except that the last three are guaranteed to be correct.

-----------------------------------
Branch Target Buffers
-----------------------------------

A BTB module may implement three functions.

.. cpp:function:: void initialize_btb()

   This function is called when the core is initialized. You can use it to initialize elements of dynamic structures, such as ``std::vector`` or ``std::map``.

.. cpp:function:: std::pair<champsim::address, bool> btb_prediction(champsim::address ip, uint8_t branch_type)
.. cpp:function:: std::pair<champsim::address, bool> btb_prediction(champsim::address ip)
.. cpp:function:: std::pair<champsim::address, bool> btb_prediction(uint64_t ip, uint8_t branch_type)
.. cpp:function:: std::pair<champsim::address, bool> btb_prediction(uint64_t ip)

   This function is called when a prediction is needed.

   :param ip: The instruction pointer of the branch
   :param branch_type: One of the following

     * ``BRANCH_DIRECT_JUMP``: Direct non-call, unconditional branches, whose target is encoded in the instruction
     * ``BRANCH_INDIRECT``: Indirect non-call, unconditional branches, whose target is stored in a register
     * ``BRANCH_CONDITIONAL``: A direct conditional branch
     * ``BRANCH_DIRECT_CALL``: A call to a procedure whose target is encoded in the instruction
     * ``BRANCH_INDIRECT_CALL``: A call to a procedure whose target is stored in a register
     * ``BRANCH_RETURN``: A return to a calling procedure
     * ``BRANCH_OTHER``: If the branch type cannot be determined

   :return: The function should return a pair containing the predicted address and a boolean that describes if the branch is known to be always taken.
       If the prediction fails, the function should return a default-initialized address, e.g. ``champsim::address{}``.

.. cpp:function:: void update_btb(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
.. cpp:function:: void update_btb(uint64_t ip, uint64_t branch_target, bool taken, uint8_t branch_type)

   This function is called when a branch is resolved.

   :param ip: The instruction pointer of the branch
   :param branch_target: The correct target of the branch.
   :param taken: A boolean value. This parameter will be nonzero if the branch was taken.
   :param branch_type: One of the following

     * ``BRANCH_DIRECT_JUMP``: Direct non-call, unconditional branches, whose target is encoded in the instruction
     * ``BRANCH_INDIRECT``: Indirect non-call, unconditional branches, whose target is stored in a register
     * ``BRANCH_CONDITIONAL``: A direct conditional branch
     * ``BRANCH_DIRECT_CALL``: A call to a procedure whose target is encoded in the instruction
     * ``BRANCH_INDIRECT_CALL``: A call to a procedure whose target is stored in a register
     * ``BRANCH_RETURN``: A return to a calling procedure
     * ``BRANCH_OTHER``: If the branch type cannot be determined

-----------------------------------
Memory Prefetchers
-----------------------------------

A prefetcher module may implement five or six functions.

.. cpp:function:: void prefetcher_initialize()

   This function is called when the cache is initialized. You can use it to initialize elements of dynamic structures, such as ``std::vector`` or ``std::map``.

.. cpp:function:: uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in)
.. cpp:function:: uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
.. cpp:function:: uint32_t prefetcher_cache_operate(uint64_t addr, uint64_t ip, bool cache_hit, uint8_t type, uint32_t metadata_in)

   This function is called when a tag is checked in the cache.

   :param addr: the address of the packet.
       If this is the first-level cache, the offset bits are included.
       Otherwise, the offset bits are zero.
       If the cache was configured with ``"virtual_prefetch": true``, this address will be a virtual address.
       Otherwise, this is a physical address.
   :param ip: the address of the instruction that initiated the demand.
       If the packet is a prefetch from another level, this value will be 0.
   :param cache_hit: if this tag check is a hit, this value is true.
   :param useful_prefetch: if this tag check hit a prior prefetch, this value is true.
   :param type: one of the following

     * ``access_type::LOAD``
     * ``access_type::RFO``
     * ``access_type::PREFETCH``
     * ``access_type::WRITE``
     * ``access_type::TRANSLATION``

   :param metadata_in: the metadata carried along by the packet.

   :return: The function should return metadata that will be stored alongside the block.

.. cpp:function:: uint32_t prefetcher_cache_fill(champsim::address addr, uint32_t set, uint32_way, bool prefetch, champsim::address evicted_address, uint32_t metadata_in)
.. cpp:function:: uint32_t prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_way, bool prefetch, uint64_t evicted_address, uint32_t metadata_in)

   This function is called when a miss is filled in the cache.

   :param addr: the address of the packet.
       If this is the first-level cache, the offset bits are included.
       Otherwise, the offset bits are zero.
       If the cache was configured with ``"virtual_prefetch": true``, this address will be a virtual address.
       Otherwise, this is a physical address.
   :param set: the set that the fill occurred in
   :param way: the way that the fill occurred in, or ``this->NUM_WAY`` if a bypass occurred
   :param prefetch: if this tag check hit a prior prefetch, this value is true.
   :param evicted_address: the address of the evicted block.
       If the fill was a bypass, this value will be default-constructed.
       If the cache was configured with ``"virtual_prefetch": true``, this address will be a virtual address.
       Otherwise, this is a physical address.
   :param metadata_in: the metadata carried along by the packet.

   :return: The function should return metadata that will be stored alongside the block.

.. cpp:function:: void prefetcher_cycle_operate()


   This function is called each cycle, after all other operation has completed.

.. cpp:function:: void prefetcher_final_stats()


   This function is called at the end of the simulation and can be used to print statistics.


.. cpp:function:: void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target)
.. cpp:function:: void prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)


   This function may be implemented by instruction prefetchers.

   :param ip: The instruction pointer of the branch
   :param branch_type: One of the following

     * ``BRANCH_DIRECT_JUMP``: Direct non-call, unconditional branches, whose target is encoded in the instruction
     * ``BRANCH_INDIRECT``: Indirect non-call, unconditional branches, whose target is stored in a register
     * ``BRANCH_CONDITIONAL``: A direct conditional branch
     * ``BRANCH_DIRECT_CALL``: A call to a procedure whose target is encoded in the instruction
     * ``BRANCH_INDIRECT_CALL``: A call to a procedure whose target is stored in a register
     * ``BRANCH_RETURN``: A return to a calling procedure
     * ``BRANCH_OTHER``: If the branch type cannot be determined

   :param branch_target: The instruction pointer of the target

-----------------------------------
Replacement Policies
-----------------------------------

A replacement policy module may implement five functions.

.. cpp:function:: void initialize_replacement()

   This function is called when the cache is initialized. You can use it to initialize elements of dynamic structures, such as ``std::vector`` or ``std::map``.

.. cpp:function:: uint32_t find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t addr, access_type type)
.. cpp:function:: uint32_t find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t addr, uint32_t type)

   This function is called when a tag is checked in the cache.

   :param triggering_cpu: the core index that initiated this fill
   :param instr_id: an instruction count that can be used to examine the program order of requests.
   :param set: the set that the fill occurred in.
   :param current_set: a pointer to the beginning of the set being accessed.
   :param ip: the address of the instruction that initiated the demand.
       If the packet is a prefetch from another level, this value will be 0.
   :param addr: the address of the packet.
       If this is the first-level cache, the offset bits are included.
       Otherwise, the offset bits are zero.
       If the cache was configured with ``"virtual_prefetch": true``, this address will be a virtual address.
       Otherwise, this is a physical address.
   :param type: one of the following

     * ``access_type::LOAD``
     * ``access_type::RFO``
     * ``access_type::PREFETCH``
     * ``access_type::WRITE``
     * ``access_type::TRANSLATION``

   :return: The function should return the way index that should be evicted, or ``this->NUM_WAY`` to indicate that a bypass should occur.

.. cpp:function:: void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr, access_type type)

    This function is called when a block is filled in the cache.
    It is called with the same timing as ``find_victim()``, but is additionally called when filling an invalid way.
    Use of this function should be careful not to misinterpret fills to invalid ways.

   :param triggering_cpu: the core index that initiated this fill
   :param set: the set that the fill occurred in.
   :param way: the way that the fill occurred in.
   :param full_addr: the address of the packet.
       If this is the first-level cache, the offset bits are included.
       Otherwise, the offset bits are zero.
       If the cache was configured with ``"virtual_prefetch": true``, this address will be a virtual address.
       Otherwise, this is a physical address.
   :param ip: the address of the instruction that initiated the demand.
       If the packet is a prefetch from another level, this value will be 0.
   :param victim_addr: the address of the evicted block, if this is a miss.
       If this is a hit, the value is 0.
   :param type: one of the following

     * ``access_type::LOAD``
     * ``access_type::RFO``
     * ``access_type::PREFETCH``
     * ``access_type::WRITE``
     * ``access_type::TRANSLATION``

.. cpp:function:: void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address addr, champsim::address ip, access_type type, bool hit)
.. cpp:function:: void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address addr, champsim::address ip, champsim::address victim_addr, access_type type, bool hit)
.. cpp:function:: void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address addr, champsim::address ip, champsim::address victim_addr, uint32_t type, bool hit)
.. cpp:function:: void update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t addr, uint64_t ip, uint64_t victim_addr, bool hit)

   This function has different behavior depending on whether ``replacement_cache_fill()`` is defined.
   If it is defined, this function is called when a tag check completes, whether the check is a hit or a miss.
   If it is not defined, this function is called on hits and when a miss is filled (that is, with the same timing as ``replacement_cache_fill()``).

   :param triggering_cpu: the core index that initiated this fill
   :param set: the set that the fill occurred in.
   :param way: the way that the fill occurred in.
   :param addr: the address of the packet.
       If this is the first-level cache, the offset bits are included.
       Otherwise, the offset bits are zero.
       If the cache was configured with ``"virtual_prefetch": true``, this address will be a virtual address.
       Otherwise, this is a physical address.
   :param ip: the address of the instruction that initiated the demand.
       If the packet is a prefetch from another level, this value will be 0.
   :param victim_addr: This value will be 0 unless ``replacement_cache_fill()`` is not defined and ``hit`` is false.
       If so, this parameter is the address of the evicted block.
   :param type: one of the following

     * ``access_type::LOAD``
     * ``access_type::RFO``
     * ``access_type::PREFETCH``
     * ``access_type::WRITE``
     * ``access_type::TRANSLATION``
   :param hit: true if the packet hit the cache, false otherwise.

.. cpp:function:: void replacement_final_stats()

   This function is called at the end of the simulation and can be used to print statistics.

