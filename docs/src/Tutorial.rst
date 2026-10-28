.. _Tutorial:

================================================
Tutorial: Creating New Modules
================================================

This tutorial walks through creating modules for ChampSim from scratch, with detailed
line-by-line commentary. By the end, you will understand:

* How to create a prefetcher
* How to create a replacement policy
* How to create a branch predictor
* How the ``ModuleBuilder`` parameter system works
* How to define an entirely new module interface

.. contents:: Contents
   :local:
   :depth: 2

.. _Tutorial_Prefetcher:

------------------------------------------
Part 1: Creating a Prefetcher
------------------------------------------

We will build a stride prefetcher from scratch. A stride prefetcher tracks the
addresses accessed by each instruction pointer (IP). When it detects a repeating
stride pattern, it prefetches ahead in that pattern.

Step 1: Create the Directory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each module lives in its own directory under the interface directory it implements.
Prefetchers go under ``prefetcher/``::

    mkdir prefetcher/my_stride

The Makefile automatically discovers ``.cc`` files under ``prefetcher/``,
``replacement/``, ``branch/``, and ``btb/``, so there is no build system
configuration needed.

Step 2: Write the Header
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create ``prefetcher/my_stride/my_stride.h``:

.. code-block:: cpp
    :linenos:

    #ifndef MY_STRIDE_H
    #define MY_STRIDE_H

    // <map> gives us std::map for our tracking table.
    #include <map>

    // "address.h" provides champsim::address and champsim::block_number — ChampSim's
    // type-safe address types.  Raw uint64_t addresses are error-prone (byte vs block
    // vs page ambiguity), so ChampSim wraps them in distinct types.
    #include "address.h"

    // "modules.h" is the central module-system header.  It provides:
    //   - champsim::modules::prefetcher   (the interface we inherit from)
    //   - champsim::modules::ModuleBuilder (the constructor parameter)
    //   - champsim::modules::cache_module  (the parent type for prefetchers)
    //   - register_module<T>  (used to register our implementation)
    #include "modules.h"

    // --------------------------------------------------------------------------
    // my_stride — A stride prefetcher with configurable degree.
    //
    // Inherits from champsim::modules::prefetcher, which itself inherits from
    // module_base<prefetcher, cache_module>.  That template establishes two things:
    //   1. Prefetchers are attached to a cache_module (the "parent").
    //   2. The static registration/factory infrastructure (register_module,
    //      create_instance) is scoped to the prefetcher interface.
    // --------------------------------------------------------------------------
    struct my_stride : public champsim::modules::prefetcher {

        // --- Per-IP tracking state ---
        // We track each instruction pointer's last cache-line address and the
        // stride between the two most recent accesses.  When two consecutive
        // strides match, we have a pattern to prefetch.
        struct tracker_entry {
            champsim::block_number last_addr{};                 // cache-line address of last access
            champsim::block_number::difference_type stride{};   // last observed stride (in cache lines)
        };

        // --- Configurable fields ---
        // "degree" controls how many cache lines ahead we prefetch once a stride
        // pattern is detected.  It defaults to 3 but can be overridden from JSON.
        int degree = 3;

        // --- Parent cache pointer ---
        // We store a pointer to the parent cache_module so we can query runtime
        // state like MSHR occupancy.  This pointer is obtained from the
        // ModuleBuilder in the constructor.
        champsim::modules::cache_module* cache_ = nullptr;

        // --- Tracking table ---
        // Maps instruction pointers to their tracker entries.  Using std::map
        // here for simplicity; a real design might use a fixed-size LRU table
        // (see champsim::msl::lru_table in ip_stride).
        std::map<champsim::address, tracker_entry> table;

        // --- Constructor ---
        // Every module MUST accept exactly one argument: a ModuleBuilder.
        // The module system calls this constructor when it instantiates the
        // module, passing in a builder pre-populated with JSON parameters
        // and a reference to the parent module.
        my_stride(champsim::modules::ModuleBuilder builder);

        // --- Interface overrides ---
        // champsim::modules::prefetcher declares six pure virtual methods.
        // We MUST override all of them.  For methods we don't need, we provide
        // an empty body with "override {}".
        //
        // prefetcher_initialize: Called once before simulation begins.
        void prefetcher_initialize() override {}

        // prefetcher_cache_operate: Called on every cache access (hit or miss).
        // This is the main hook — it's where we detect strides and issue
        // prefetches.  Returns metadata to pass through to future callbacks.
        uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip,
            bool cache_hit, bool useful_prefetch, access_type type,
            uint32_t metadata_in) override;

        // prefetcher_cache_fill: Called when a cache line is filled (loaded into
        // the cache).  Useful for feedback-directed prefetching.  We don't need
        // it for a simple stride prefetcher, so we just pass through the metadata.
        uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way,
            bool prefetch, champsim::address evicted_addr, uint32_t metadata_in) override
        { return metadata_in; }

        // prefetcher_cycle_operate: Called once every cycle.  Useful if your
        // prefetcher needs to do work that isn't tied to a specific cache access
        // (e.g. draining a lookahead queue, as ip_stride does).
        void prefetcher_cycle_operate() override {}

        // prefetcher_final_stats: Called at the end of simulation.  Use it to
        // print custom statistics.
        void prefetcher_final_stats() override {}

        // prefetcher_branch_operate: Called on every branch instruction executed
        // by the core attached to this cache.  Some prefetchers (e.g. TAGE-based)
        // use branch outcomes.  We don't, so we stub it.
        void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type,
            champsim::address branch_target) override {}
    };

    #endif

**Why inherit from** ``champsim::modules::prefetcher``?  It is the interface that defines
the contract between the cache and its prefetcher(s).  Every cache expects its prefetcher
submodules to implement these six methods.  The ``module_base`` machinery behind
``prefetcher`` provides the static factory (``create_instance``, ``register_module``) that
the config system uses to instantiate your module by name.

**Why must the constructor take** ``ModuleBuilder``?  The registration macro stores a
factory lambda ``[](ModuleBuilder b){ return std::make_unique<T>(b); }``.  If your
constructor has a different signature, that lambda won't compile and registration will
fail.

Step 3: Implement the Module
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create ``prefetcher/my_stride/my_stride.cc``:

.. code-block:: cpp
    :linenos:

    #include "my_stride.h"

    // "cache.h" provides the concrete CACHE class.  We don't strictly need it here
    // because we only interact through the cache_module interface, but some modules
    // may need it for additional helpers.
    #include "cache.h"

    // =========================================================================
    // REGISTRATION
    //
    // This single line registers our class with the module system under the name
    // "my_stride".  When a config file says "model": "my_stride", the factory
    // will call our constructor.
    //
    // The variable name (my_stride_reg) is arbitrary — it just needs to be a
    // global so it runs at static-initialization time.
    //
    // Breaking down the type:
    //   champsim::modules::prefetcher           — the interface we implement
    //   ::register_module<my_stride>            — template helper on module_base
    //   my_stride_reg("my_stride")             — constructor arg = model name
    // =========================================================================
    champsim::modules::prefetcher::register_module<my_stride>
        my_stride_reg("my_stride");

    // =========================================================================
    // CONSTRUCTOR
    //
    // The ModuleBuilder is pre-populated by the config system before we see it.
    // It contains:
    //   - A pointer to the parent module (a cache_module*)
    //   - All JSON parameters as a string->std::any map
    //   - The module name and model name
    //
    // get_parameter<T>(name, optional, default):
    //   - name:     JSON key to look up
    //   - optional: if true, return default_value when key is missing;
    //               if false, exit with an error when key is missing
    //   - default:  value to use when key is missing (only if optional=true)
    //
    // get_parent<T>():
    //   - Returns a T* to the parent module.  For prefetchers and replacement
    //     policies, T is cache_module.  For branch predictors and BTBs, T is
    //     core_module.
    // =========================================================================
    my_stride::my_stride(champsim::modules::ModuleBuilder builder)
        : degree(builder.get_parameter<int>("degree", true, 3)),
          cache_(builder.get_parent<champsim::modules::cache_module>())
    {
        // At this point:
        //   degree == the "degree" value from JSON, or 3 if absent
        //   cache_ == pointer to the parent cache (never null if config is valid)
    }

    // =========================================================================
    // MAIN HOOK: prefetcher_cache_operate
    //
    // Called by the cache on every access (loads, stores, prefetch fills).
    //
    // Parameters:
    //   addr           — full byte address of the access
    //   ip             — instruction pointer that caused the access
    //   cache_hit      — true if the access was a cache hit
    //   useful_prefetch— true if a prior prefetch for this address was used
    //   type           — LOAD, RFO (store), PREFETCH, WRITE, or TRANSLATION
    //   metadata_in    — metadata propagated from a prior prefetcher in the chain
    //
    // Returns: metadata to propagate to the next prefetcher in the chain.
    // =========================================================================
    uint32_t my_stride::prefetcher_cache_operate(
        champsim::address addr, champsim::address ip,
        bool cache_hit, bool useful_prefetch, access_type type,
        uint32_t metadata_in)
    {
        // Convert the byte address to a cache-line (block) number.
        // champsim::block_number strips the low bits (the block offset),
        // giving us a cache-line-granularity address for stride computation.
        champsim::block_number cl_addr{addr};

        // Look up whether we've seen this IP before.
        auto it = table.find(ip);
        if (it != table.end()) {
            // We've seen this IP before.  Compute the stride: the difference
            // between the current cache-line and the last one this IP accessed.
            // champsim::offset(a, b) returns b - a in block-number units.
            auto stride = champsim::offset(it->second.last_addr, cl_addr);

            // Only prefetch if:
            //   1. stride != 0   — the IP didn't access the same line twice
            //   2. stride == last_stride — the pattern is consistent
            if (stride != 0 && stride == it->second.stride) {
                for (int i = 0; i < degree; ++i) {
                    // Compute the prefetch address: current line + (stride * (i+1))
                    champsim::address pf_addr{cl_addr + (stride * (i + 1))};

                    // Decide whether to fill this level or a lower level.
                    // When MSHRs are less than 50% full, fill this cache level
                    // (L2C).  When they're busy, fill the level below (LLC)
                    // to avoid polluting the closer cache.
                    bool fill_this_level = cache_->get_mshr_occupancy_ratio() < 0.5;

                    // prefetch_line() is provided by the prefetcher base class.
                    // It issues the prefetch through the parent cache.
                    // Arguments: address, fill_this_level, metadata.
                    prefetch_line(pf_addr, fill_this_level, metadata_in);
                }
            }

            // Update the tracker for next time.
            it->second.stride = stride;
            it->second.last_addr = cl_addr;
        } else {
            // First time seeing this IP.  Create a tracker entry with stride 0
            // (we need at least two accesses to compute a stride).
            table[ip] = {cl_addr, 0};
        }

        // Pass metadata through unchanged.
        return metadata_in;
    }

Step 4: Build
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Rebuild ChampSim.  The Makefile automatically discovers source files under
``prefetcher/``, ``replacement/``, ``branch/``, and ``btb/``::

    make -j$(nproc)

If you get a linker error about duplicate symbols, make sure your ``register_module``
variable name is unique across all modules.

Step 5: Configure and Run
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Legacy config** — the simplest option, just name the model::

    {
        "L2C": { "prefetcher": "my_stride" }
    }

To pass parameters to the prefetcher, use the object form (see
:ref:`Passing Parameters to Submodules <Legacy_Submodule_Params>`)::

    {
        "L2C": { "prefetcher": {"model": "my_stride", "degree": 4} }
    }

**Explicit config** — the prefetcher is a child of the cache module::

    {
        "name": "cpu0_L2C_prefetcher",
        "module": "prefetcher",
        "model": "my_stride",
        "degree": 4
    }

**Run**::

    bin/champsim --config my_config.json \
        --warmup-instructions 200000000 \
        --simulation-instructions 500000000 \
        trace.champsimtrace.xz

**Verify with** ``--dump`` — add ``--dump`` to see every module's parameters and
connections, confirming your prefetcher was constructed with the right degree::

    bin/champsim --config my_config.json --dump

.. _Tutorial_Replacement:

------------------------------------------
Part 2: Creating a Replacement Policy
------------------------------------------

This section builds a simple LRU (Least Recently Used) replacement policy.  It
follows the same three-step pattern as the prefetcher: inherit, override, register.

**Header** (``replacement/my_lru/my_lru.h``):

.. code-block:: cpp
    :linenos:

    #ifndef MY_LRU_H
    #define MY_LRU_H

    #include <vector>
    #include "modules.h"

    // -------------------------------------------------------------------------
    // my_lru — Least Recently Used replacement policy.
    //
    // Inherits from champsim::modules::replacement, which is
    // module_base<replacement, cache_module>.  Like prefetchers, replacement
    // policies are children of a cache_module.
    //
    // The replacement interface has five pure virtual methods:
    //   initialize_replacement()       — one-time setup
    //   find_victim(...)               — pick a way to evict
    //   update_replacement_state(...)  — called on every access (hit or miss)
    //   replacement_cache_fill(...)    — called when a new line is filled
    //   replacement_final_stats()      — end-of-simulation hook
    // -------------------------------------------------------------------------
    struct my_lru : public champsim::modules::replacement {

        // Number of ways per set — we read this from the parent cache at
        // construction time so our flat vector is correctly sized.
        long num_ways;

        // Flat vector of last-used timestamps, indexed as [set * num_ways + way].
        // A flat vector is faster than a map<pair<long,long>, ...> because the
        // access pattern is dense and predictable.
        std::vector<uint64_t> last_used;

        // Monotonically increasing timestamp.  We increment it on every access,
        // giving us a total ordering of recency.
        uint64_t cycle = 0;

        // Constructor — reads cache geometry from the parent.
        my_lru(champsim::modules::ModuleBuilder builder);

        // --- Interface overrides ---

        void initialize_replacement() override {}

        // find_victim: Return the way index within "set" that should be evicted.
        // The cache calls this when it needs to make room for a new line.
        long find_victim(champsim::origin origin, uint64_t instr_id, long set,
            const champsim::cache_block* current_set, champsim::address ip,
            champsim::address full_addr, access_type type) override;

        // update_replacement_state: Called on every cache access (hit or miss).
        // We use it to update the recency timestamp.
        void update_replacement_state(champsim::origin origin, long set, long way,
            champsim::address full_addr, champsim::address ip,
            champsim::address victim_addr, access_type type, bool hit) override;

        // replacement_cache_fill: Called when a new line is physically written
        // into the cache.  We also update the timestamp here.
        void replacement_cache_fill(champsim::origin origin, long set, long way,
            champsim::address full_addr, champsim::address ip,
            champsim::address victim_addr, access_type type) override;

        void replacement_final_stats() override {}
    };

    #endif

**Implementation** (``replacement/my_lru/my_lru.cc``):

.. code-block:: cpp
    :linenos:

    #include "my_lru.h"
    #include <algorithm>

    // Register under the name "my_lru".
    champsim::modules::replacement::register_module<my_lru>
        my_lru_reg("my_lru");

    // -------------------------------------------------------------------------
    // Constructor
    //
    // We query the parent cache for its geometry (num_sets, num_ways) so we can
    // size our data structures correctly.  This is the typical pattern for
    // replacement policies: they need to know how big the cache is.
    // -------------------------------------------------------------------------
    my_lru::my_lru(champsim::modules::ModuleBuilder builder)
        : num_ways(static_cast<long>(
              builder.get_parent<champsim::modules::cache_module>()->num_ways())),
          last_used(static_cast<std::size_t>(
              builder.get_parent<champsim::modules::cache_module>()->num_sets()
              * num_ways), 0)
    {
        // last_used is now a flat vector of size (sets * ways), all initialized
        // to 0.  Way indices within each set are at offsets [set*ways .. set*ways+ways).
    }

    // -------------------------------------------------------------------------
    // find_victim: Scan the last_used timestamps for this set and return the
    // way with the smallest (oldest) timestamp.
    // -------------------------------------------------------------------------
    long my_lru::find_victim(champsim::origin /*origin*/, uint64_t /*instr_id*/,
        long set, const champsim::cache_block* /*current_set*/,
        champsim::address /*ip*/, champsim::address /*full_addr*/,
        access_type /*type*/)
    {
        // Point to the start of this set's slice of the flat vector.
        auto begin = std::next(last_used.begin(), set * num_ways);
        auto end = std::next(begin, num_ways);

        // std::min_element finds the way with the smallest last-used time.
        auto victim = std::min_element(begin, end);
        return std::distance(begin, victim);
    }

    // -------------------------------------------------------------------------
    // update_replacement_state: Stamp the accessed way with the current time.
    //
    // We skip writeback hits because writebacks are not demand accesses — they
    // are cache maintenance operations.  Promoting a line on a writeback would
    // artificially extend its lifetime.
    // -------------------------------------------------------------------------
    void my_lru::update_replacement_state(champsim::origin /*origin*/, long set,
        long way, champsim::address /*full_addr*/, champsim::address /*ip*/,
        champsim::address /*victim_addr*/, access_type type, bool hit)
    {
        if (hit && type != access_type::WRITE)
            last_used.at(static_cast<std::size_t>(set * num_ways + way)) = cycle++;
    }

    // -------------------------------------------------------------------------
    // replacement_cache_fill: A new line was just written into set/way.
    // Mark it as recently used.
    // -------------------------------------------------------------------------
    void my_lru::replacement_cache_fill(champsim::origin /*origin*/, long set,
        long way, champsim::address /*full_addr*/, champsim::address /*ip*/,
        champsim::address /*victim_addr*/, access_type /*type*/)
    {
        last_used.at(static_cast<std::size_t>(set * num_ways + way)) = cycle++;
    }

.. _Tutorial_BranchPredictor:

------------------------------------------
Part 3: Creating a Branch Predictor
------------------------------------------

Branch predictors follow the same pattern, but they are children of ``core_module``
instead of ``cache_module``.

The ``branch_predictor`` interface has three pure virtual methods:

* ``initialize_branch_predictor()`` — one-time setup
* ``predict_branch(ip, predicted_target, always_taken, branch_type)`` — return ``true``
  for taken, ``false`` for not-taken
* ``last_branch_result(ip, target, taken, branch_type)`` — called after the branch is
  resolved so you can update predictor state

**A simple bimodal predictor** (``branch/my_bimodal/my_bimodal.h``):

.. code-block:: cpp
    :linenos:

    #ifndef MY_BIMODAL_H
    #define MY_BIMODAL_H

    #include <vector>
    #include "modules.h"

    // -------------------------------------------------------------------------
    // my_bimodal — A simple bimodal branch predictor.
    //
    // Uses a table of 2-bit saturating counters indexed by the low bits of the
    // instruction pointer.  This is the simplest dynamic branch predictor.
    //
    // Inherits from champsim::modules::branch_predictor, which is
    // module_base<branch_predictor, core_module>.  Branch predictors are
    // children of a core_module.
    // -------------------------------------------------------------------------
    struct my_bimodal : public champsim::modules::branch_predictor {

        // Number of index bits — controls table size (2^index_bits entries).
        int index_bits;

        // Table of 2-bit saturating counters.  Each entry is 0–3:
        //   0,1 = predict not-taken
        //   2,3 = predict taken
        // We use int8_t to save space.
        std::vector<int8_t> table;

        // Constructor reads table_size from JSON config.
        my_bimodal(champsim::modules::ModuleBuilder builder);

        void initialize_branch_predictor() override {}

        // predict_branch: Return true (taken) or false (not-taken).
        bool predict_branch(champsim::address ip, champsim::address predicted_target,
            bool always_taken, uint8_t branch_type) override;

        // last_branch_result: Update the counter after the branch is resolved.
        void last_branch_result(champsim::address ip, champsim::address target,
            bool taken, uint8_t branch_type) override;
    };

    #endif

**Implementation** (``branch/my_bimodal/my_bimodal.cc``):

.. code-block:: cpp
    :linenos:

    #include "my_bimodal.h"

    // Register as "my_bimodal".
    champsim::modules::branch_predictor::register_module<my_bimodal>
        my_bimodal_reg("my_bimodal");

    // -------------------------------------------------------------------------
    // Constructor
    //
    // Read table_size from JSON (default 4096 entries = 12 index bits).
    // Initialize all counters to 2 (weakly taken), a common starting point.
    // -------------------------------------------------------------------------
    my_bimodal::my_bimodal(champsim::modules::ModuleBuilder builder)
        : index_bits(builder.get_parameter<int>("index_bits", true, 12)),
          table(1 << index_bits, 2)  // 2^index_bits entries, init to "weakly taken"
    {
    }

    // -------------------------------------------------------------------------
    // predict_branch
    //
    // Index into the counter table using the low bits of the IP.
    // Counter >= 2 means "predict taken".
    // -------------------------------------------------------------------------
    bool my_bimodal::predict_branch(champsim::address ip,
        champsim::address /*predicted_target*/, bool /*always_taken*/,
        uint8_t /*branch_type*/)
    {
        // Use ip.to<std::size_t>() to extract the raw integer value.
        // Mask with (table_size - 1) to get an index.
        std::size_t index = ip.to<std::size_t>() & ((1u << index_bits) - 1);
        return table[index] >= 2;
    }

    // -------------------------------------------------------------------------
    // last_branch_result
    //
    // Increment the counter if the branch was taken, decrement if not-taken.
    // Saturate at 0 and 3 (2-bit counter).
    // -------------------------------------------------------------------------
    void my_bimodal::last_branch_result(champsim::address ip,
        champsim::address /*target*/, bool taken, uint8_t /*branch_type*/)
    {
        std::size_t index = ip.to<std::size_t>() & ((1u << index_bits) - 1);
        if (taken && table[index] < 3)
            table[index]++;
        else if (!taken && table[index] > 0)
            table[index]--;
    }

.. _Tutorial_ModuleBuilder:

------------------------------------------
Part 4: Understanding ModuleBuilder
------------------------------------------

``ModuleBuilder`` is the object the module system passes to your constructor.  It carries
all the information your module needs to initialize itself.

Reading Parameters
^^^^^^^^^^^^^^^^^^^^^

``get_parameter<T>(name, optional, default_value)`` reads a JSON value:

.. code-block:: cpp

    // REQUIRED: exits with an error if "sets" is not in the JSON.
    // The third argument is ignored when optional=false, but you must provide it
    // due to the template signature.
    int sets = builder.get_parameter<int>("sets", false, 0);

    // OPTIONAL: returns 3 if "degree" is not in the JSON.
    int degree = builder.get_parameter<int>("degree", true, 3);

    // STRING parameter:
    auto mode = builder.get_parameter<std::string>("mode", true, "normal");

    // SIZE parameter — uses std::size_t, which is 64-bit unsigned.
    // numeric_any_cast handles converting JSON integers (stored as int64_t)
    // to std::size_t automatically.
    auto sz = builder.get_parameter<std::size_t>("table_size", true, 256);

The parameter name corresponds to a key in the module's JSON config entry.
See :ref:`Passing Parameters to Submodules <Legacy_Submodule_Params>` and the
:ref:`Explicit Configuration Format <Explicit_Config>` for how these values
get into the ``ModuleBuilder``.

Accessing the Parent Module
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``get_parent<T>()`` returns a pointer to the module that owns this submodule:

.. code-block:: cpp

    // Prefetchers and replacement policies → parent is cache_module
    auto* cache = builder.get_parent<champsim::modules::cache_module>();

    // Branch predictors and BTBs → parent is core_module
    auto* core = builder.get_parent<champsim::modules::core_module>();

Store the pointer in a member variable if you need it after construction.  Methods
available on ``cache_module`` include:

* ``get_mshr_occupancy_ratio()`` — MSHR load as a fraction (0.0 to 1.0)
* ``get_mshr_size()`` — total MSHR capacity
* ``num_sets()`` / ``num_ways()`` — cache geometry
* ``is_virtual_prefetch()`` — whether prefetch addresses are virtual
* ``get_rq_occupancy()`` / ``get_wq_occupancy()`` / ``get_pq_occupancy()`` — queue loads

Querying Module Name and Model
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    // The instance name (e.g. "cpu0_L2C_prefetcher"):
    std::string name = builder.get_name();

    // The model name (e.g. "my_stride"):
    std::string model = builder.get_model();

.. _Tutorial_Debugging:

------------------------------------------
Part 5: Debugging Module Construction
------------------------------------------

Use ``--dump`` to see every module that was constructed, its parameters, and its
connections::

    bin/champsim --config my_config.json --dump

Example output:

.. code-block:: text

    [cpu0_L2C_prefetcher] degree = 4 (set)
    [cpu0_L2C_prefetcher] created_module = my_stride (set)
    [cpu0_L2C_replacement] created_module = lru (set)

This is invaluable for verifying that:

* Your module was instantiated (look for ``created_module = your_model_name``)
* Parameters have the expected values (``(set)`` vs ``(default)``)
* Parent/child connections are correct

.. _Tutorial_NewInterface:

------------------------------------------
Part 6: Defining a New Interface
------------------------------------------

ChampSim ships with built-in interfaces for prefetchers, replacement policies, branch
predictors, and BTBs.  The module system is fully extensible — you can define your own
interface types, attach them to existing parent modules, and the config system will
construct them for you from JSON.

This section walks through adding a **cache_indexer** interface: a submodule of a
cache that owns the address → set-index mapping.  The default mapping in ChampSim
is a simple bit-slice of the block address, which is a common source of conflict
misses on pathological access patterns (power-of-two strides, aliasing page tables).
A pluggable indexer lets you swap the mapping (bit-slice, XOR hashing, prime
modulo, skewed associativity) at runtime from the config file without touching
the cache core.

The end state is:

* an interface header ``inc/cache_indexer.h``,
* an interface registration in ``src/modules.cc``,
* a hook inside ``CACHE::get_set_index`` that consults the indexer when one is attached,
* two implementations (``bit_slice`` and ``xor_hash``) under ``indexer/``,
* and a JSON config entry that attaches the indexer as a child of a cache module.

Step 1: Define the Interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create ``inc/cache_indexer.h``:

.. code-block:: cpp
    :linenos:

    #ifndef CHAMPSIM_CACHE_INDEXER_H
    #define CHAMPSIM_CACHE_INDEXER_H

    #include "modules.h"

    namespace champsim::modules {

    // -------------------------------------------------------------------------
    // cache_indexer — A submodule of cache_module that computes the set index
    // for a given address.
    //
    // Inherits from module_base<cache_indexer, cache_module>.
    //
    // The first template argument is the interface type itself (CRTP).  This
    // keeps the static module_map / instance_map unique to this interface so a
    // cache_indexer registered as "bit_slice" won't collide with a prefetcher
    // registered as "bit_slice".  The second argument is the parent type —
    // because indexers live under a cache, that's cache_module.
    //
    // module_base provides (inherited):
    //   - register_module<T>   : register an implementation by model name
    //   - register_interface   : register this interface type by string name
    //   - create_instance(...) : factory used by the config system
    // -------------------------------------------------------------------------
    struct cache_indexer : public module_base<cache_indexer, cache_module> {

      virtual ~cache_indexer() = default;

      // compute_set_index — map a byte address to a set index in
      // [0, num_sets()).  The cache asserts the return value is in-range.
      //
      // If you want the cache-line-granularity address, strip
      // get_parent<cache_module>()->get_offset_bits() bits off the bottom.
      virtual long compute_set_index(champsim::address addr) = 0;
    };

    } // namespace champsim::modules

    #endif

**Why CRTP?**  The first template parameter to ``module_base`` is always the
interface class itself.  ``module_base<cache_indexer, cache_module>`` instantiates a
set of static registries that are unique to the ``cache_indexer`` interface.  A
module registered as ``"lru"`` under ``replacement`` will not be visible through
``cache_indexer``'s registry and vice-versa.

Step 2: Register the Interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The explicit config parser looks up interfaces by string name so that
``"module": "cache_indexer"`` in JSON resolves to our ``module_base``
specialization.  Add a single line to ``src/modules.cc`` (where the other
interfaces — prefetcher, replacement, branch, btb — are registered):

.. code-block:: cpp

    #include "cache_indexer.h"

    static champsim::modules::cache_indexer::register_interface
        cache_indexer_iface_reg("cache_indexer");

This runs at static-initialization time and makes
``interface_registry::create("cache_indexer", builder, parent)`` available to the
explicit environment.  The string ``"cache_indexer"`` is exactly what you will use
as the ``"module"`` key in the JSON config.

Step 3: Hook the Interface into the Parent
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

An interface with no consumer does nothing.  The cache needs to construct the
indexer submodule from its builder and consult it on every set lookup.  Two
surgical edits to ``CACHE`` are required.

First, in ``inc/cache.h``, store an indexer pointer and populate it in the
cache constructor from the builder's submodules.  We treat the indexer as a
*required* submodule: every cache must declare at least one ``cache_indexer``
child in its JSON config.

.. code-block:: cpp

    // In the CACHE class body, alongside pref_module_pimpl / repl_module_pimpl:
    champsim::modules::cache_indexer* indexer_module_pimpl = nullptr;

    // In the CACHE constructor, alongside the existing submodule loops.
    // get_submodules defaults to optional=false, so a missing
    // "cache_indexer" child causes the simulator to exit with an error
    // rather than silently producing an unindexed cache.
    for (const auto& sub : builder.get_submodules("cache_indexer"))
      indexer_module_pimpl = champsim::modules::cache_indexer::create_instance(
          sub, static_cast<champsim::modules::cache_module*>(this));

The loop form is intentional.  ``get_submodules`` returns a vector; writing it
as a loop handles any-N submodules uniformly.  For this interface we keep only
the last one because a cache has a single address → set mapping; another
interface (e.g. a listener) would ``push_back`` into a vector here instead.
If you wanted to permit caches without an indexer, you would pass
``optional=true`` as the second argument — ``get_submodules`` would then return
an empty vector instead of erroring out, and the loop body would simply not
run.

Second, in ``src/cache.cc``, route ``get_set_index`` through the indexer:

.. code-block:: cpp

    long CACHE::get_set_index(champsim::address address) const
    {
      long idx = indexer_module_pimpl->compute_set_index(address);
      assert(idx >= 0 && static_cast<uint32_t>(idx) < NUM_SET);
      return idx;
    }

Step 4: Write Implementations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Implementations live under ``indexer/<model_name>/`` so the Makefile can discover
them.  (Add ``indexer`` to the ``find`` list in the Makefile's
``module_sources`` line alongside ``branch btb prefetcher replacement`` — the
build system rules follow the same pattern as the other module directories.)

The default bit-slice, made explicit (``indexer/bit_slice/bit_slice.h``):

.. code-block:: cpp
    :linenos:

    #ifndef BIT_SLICE_INDEXER_H
    #define BIT_SLICE_INDEXER_H

    #include "cache_indexer.h"
    #include "champsim.h"

    struct bit_slice : public champsim::modules::cache_indexer {
      long num_sets;
      champsim::data::bits offset_bits;

      bit_slice(champsim::modules::ModuleBuilder builder);
      long compute_set_index(champsim::address addr) override;
    };

    #endif

``indexer/bit_slice/bit_slice.cc``:

.. code-block:: cpp
    :linenos:

    #include "bit_slice.h"
    #include "cache.h"

    // Register under the model name "bit_slice".
    champsim::modules::cache_indexer::register_module<bit_slice>
        bit_slice_reg("bit_slice");

    bit_slice::bit_slice(champsim::modules::ModuleBuilder builder)
      : num_sets(static_cast<long>(
          builder.get_parent<champsim::modules::cache_module>()->num_sets())),
        offset_bits(
          builder.get_parent<champsim::modules::cache_module>()->get_offset_bits())
    {}

    long bit_slice::compute_set_index(champsim::address addr)
    {
      // Canonical bit-slice: use champsim::address::slice with a
      // dynamic_extent describing [offset_bits, offset_bits + lg2(num_sets)),
      // then unwrap the strongly-typed slice with .to<long>(). This is the
      // same expression CACHE::get_set_index uses for its built-in fallback.
      return addr
          .slice(champsim::dynamic_extent{offset_bits, champsim::lg2(num_sets)})
          .to<long>();
    }

A one-line XOR-folding hash that reduces conflict misses on power-of-two strides
(``indexer/xor_hash/xor_hash.cc``):

.. code-block:: cpp
    :linenos:

    #include "xor_hash.h"
    #include "cache.h"

    champsim::modules::cache_indexer::register_module<xor_hash>
        xor_hash_reg("xor_hash");

    xor_hash::xor_hash(champsim::modules::ModuleBuilder builder)
      : num_sets(static_cast<long>(
          builder.get_parent<champsim::modules::cache_module>()->num_sets())),
        index_bits(champsim::lg2(num_sets)),
        offset_bits(
          builder.get_parent<champsim::modules::cache_module>()->get_offset_bits())
    {}

    long xor_hash::compute_set_index(champsim::address addr)
    {
      // Fold two adjacent index_bits-wide fields of the block address
      // together. Each field is pulled with champsim::address::slice + a
      // dynamic_extent whose second argument is the slice width in bits, so
      // we stay inside the strongly-typed address API.
      const auto low_bits  = champsim::dynamic_extent{offset_bits, index_bits};
      const auto high_bits = champsim::dynamic_extent{
          offset_bits + champsim::data::bits{index_bits}, index_bits};
      const auto low  = addr.slice(low_bits).to<unsigned long>();
      const auto high = addr.slice(high_bits).to<unsigned long>();
      return static_cast<long>(low ^ high);
    }

.. note::

   Prefer ``champsim::address::slice`` (combined with ``champsim::dynamic_extent``
   or one of the named extents in ``inc/extent.h``) over manually converting the
   address to a ``uint64_t`` and shifting.  The slice API keeps the offset and
   width checked by the type system, composes with ``champsim::address::splice``
   and ``slice_upper`` for more complex hashes, and — importantly — is what the
   rest of ChampSim uses, so your indexer reads like the surrounding code.

Step 5: Configure It
^^^^^^^^^^^^^^^^^^^^^^^^

In the explicit config, the indexer is a child of a cache module, right next to
the prefetcher and replacement submodules::

    {
      "name": "LLC", "module": "cache", "model": "DEFAULT_CACHE",
      "num_sets": 2048, "num_ways": 16,
      "children": [
        {"name": "llc_pf",   "module": "prefetcher",    "model": "no"},
        {"name": "llc_repl", "module": "replacement",   "model": "lru"},
        {"name": "llc_idx",  "module": "cache_indexer", "model": "xor_hash"}
      ]
    }

Run ``bin/champsim --config my_config.json --dump`` and confirm the line::

    [llc_idx] created_module = xor_hash (set)

appears before the cache itself is constructed.  If the ``cache_indexer``
child is omitted from the config, ``get_submodules("cache_indexer")`` exits
with a ``required submodules of interface cache_indexer not found`` error,
making the missing wiring impossible to overlook.

Summary
^^^^^^^^^^

Defining a new interface is four pieces:

1. **Interface class** inheriting ``module_base<YourInterface, ParentType>``.
2. **Interface registration** via ``register_interface("name")`` in a ``.cc`` file.
3. **Implementation class(es)** inheriting the interface, registered with
   ``register_module<Impl>("model_name")``.
4. **Parent integration** — the parent module pulls submodule builders out via
   ``builder.get_submodules("name")``, creates instances with
   ``YourInterface::create_instance(sub, this)``, and calls the virtual methods at
   the relevant points.

The ``module_base`` template provides all the factory and registration
infrastructure.  You define the virtual methods and the integration point inside
the parent module.

.. _Tutorial_Migration:

------------------------------------------
Part 7: Migrating from Legacy Modules
------------------------------------------

If you have modules written for an older version of ChampSim that used free-function
hooks (e.g. ``void CACHE::prefetcher_initialize()``), here is how to update them:

1. **Create a class** inheriting from the appropriate interface
   (``champsim::modules::prefetcher``, ``::replacement``, ``::branch_predictor``, or
   ``::btb``).

2. **Move your free functions** into the class as method overrides:

   * Remove the ``CACHE::`` or ``O3_CPU::`` prefix.
   * Update parameter types: ``uint64_t`` addresses become ``champsim::address``,
     ``uint8_t type`` becomes ``access_type type``, etc.
   * Add ``override`` to each method.

3. **Move global state** into member variables.  Each module instance gets its own
   state — you no longer need to key state by cache/CPU pointer.

4. **Add a constructor** that takes ``champsim::modules::ModuleBuilder``.  If you need
   access to the parent cache or core, call ``builder.get_parent<T>()``.

5. **Add a registration line** in your ``.cc`` file::

       champsim::modules::prefetcher::register_module<my_pref>("my_pref");

6. **Remove the** ``__legacy__`` **file** if one exists in your module directory.

7. **Stub any new pure virtual methods** that didn't exist in the old interface (e.g.
   ``prefetcher_branch_operate``, ``replacement_cache_fill``) with ``override {}``.

Example diff for a prefetcher::

    // OLD (free-function style):
    void CACHE::prefetcher_initialize() { ... }
    uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, ...) { ... }

    // NEW (class-based style):
    struct my_pref : champsim::modules::prefetcher {
        my_pref(champsim::modules::ModuleBuilder builder) {}
        void prefetcher_initialize() override { ... }
        uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, ...) override { ... }
        // ... all other pure virtuals stubbed or implemented
    };
    champsim::modules::prefetcher::register_module<my_pref>("my_pref");

.. _Tutorial_Modernization:

------------------------------------------
Part 8: Modernizing the Default Config
------------------------------------------

The default ``champsim_config.json`` shipped with the repository is a
documentation artifact.  Its own header says so:

.. code-block:: json

    "_description": "Sample/documentation config. Does not represent a
     reasonable system; intended only to illustrate config file structure."

Running experiments against it and reporting the results as representative of a
contemporary CPU is a common source of bad numbers.  The defaults were
reasonable in the mid-2010s; they are not in 2026.  This section walks through
upgrading the default config to something closer to an Intel **Redwood Cove**
(Meteor Lake / Granite Rapids P-core) system and explains what each change
buys you.  The approximate specs we are targeting:

* ~5 GHz core clock, ~6-wide front-end, ~6-wide allocate, 12-wide (MATH + LOAD + STORE) execute.
* 512-entry ROB, ~280-entry INT / ~332-entry FP register files, ~205-entry scheduler,
  ~192-entry load queue, ~114-entry store queue.
* 48 KB 12-way L1D, ~5 cycle load-to-use at 5 GHz.
* 64 KB 8-way L1I (doubled vs. Golden Cove — one of Redwood Cove's visible
  front-end changes).
* 2 MB private L2 (client parts; Granite Rapids doubles this to 4 MB).
* ~3 MB / core 12-way shared L3 slice (client).
* DDR5-5600, 2 channels (client) — Granite Rapids uses DDR5-6400 / MCR-DIMM.

The goal here is not perfect fidelity (ChampSim's core model is abstract,
and does not model a specific ISA or microarchitecture) but **realistic values** 
so that replacement/prefetch comparisons are not dominated by
obvious model artifacts (e.g. a ROB that empties in a single cycle).

Where to Direct Attention
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When cloning ``champsim_config.json`` into an experimental config, audit these
parameters in roughly decreasing order of impact:

1. **Core width and window sizes** (``rob_size``, ``lq_size``, ``sq_size``,
   ``scheduler_size``, ``*_width``).  The default ROB of 352 and LQ of 128 are
   usably close to Redwood Cove, but the 4-wide execute / 2-wide LQ are too
   narrow for a modern OoO core and silently gate MLP.
2. **L1D latency and size**.  The default ``5/64/12`` is fine; double-check it
   matches the accuracy you want.
3. **L2 size**.  The default 512 KB / 8-way L2 is ~4× smaller than Redwood
   Cove's 2 MB / 16-way private L2 (client parts).  Undersized L2s push more
   traffic to LLC and DRAM and distort prefetcher evaluations.
4. **LLC size**.  The default 2048 sets × 16 ways × 64 B = **2 MB**.  Modern
   server parts are 30 MB+; client parts are 30 MB+.  Per-core LLC slices are
   in the 1.5–3 MB range.  Running with 2 MB shared LLC for any benchmark with
   a working set larger than ~1 MB is a DRAM-bound experiment by construction.
5. **DRAM speed**.  The default ``data_rate: 3200`` (DDR4-3200) is two
   generations behind the DDR5-5600 (client) / DDR5-6400 + MCR-DIMM (server)
   that ships with Redwood Cove systems.
6. **Prefetchers**.  The defaults are ``no`` at L1D/L1I and ``spp_dev`` at L2C.
   This asymmetry is a deliberate documentation choice; for a realistic
   comparison baseline you typically want ``ip_stride`` or ``va_ampm_lite``
   at **L1D** (both are virtually-addressed L1D prefetchers in this repo —
   ``va_ampm_lite`` is page-region AMPM on VA), ``next_line`` at L1I, and
   ``spp_dev`` at L2C.
7. **Memory timings** (``nCAS``, ``nRCD``, ``nRP``, ``nRAS``).  The defaults
   target DDR4; DDR5 timings are different in absolute cycles.
8. **MSHR sizes**.  L1D MSHR = 16 is acceptable; Redwood Cove sustains
   ~16 outstanding demand misses. However, L2C MSHR = 32 is undersized.
   Redwood Cove has 64 MSHRs in its L2C. An undersized MSHR caps effective MLP.


Step 1: Clone and Comment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Copy ``champsim_config.json`` to ``redwood_cove.json``.  Immediately replace the
``_description`` field with a real citation of the specs you are targeting and
the date you snapshotted them — future-you will thank you.

Step 2: Core Parameters
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: json

    "ooo_cpu": [{
      "frequency": 5000,
      "ifetch_buffer_size": 128,
      "decode_buffer_size": 64,
      "dispatch_buffer_size": 64,
      "register_file_size": 192,
      "rob_size": 512,
      "lq_size": 192,
      "sq_size": 114,
      "fetch_width": 6,
      "decode_width": 6,
      "dispatch_width": 6,
      "execute_width": 12,
      "lq_width": 3,
      "sq_width": 2,
      "retire_width": 8,
      "mispredict_penalty": 1,
      "scheduler_size": 205,
      "decode_latency": 1,
      "dispatch_latency": 1,
      "schedule_latency": 0,
      "execute_latency": 0,
      "branch_predictor": "hashed_perceptron",
      "btb": "basic_btb"
    }]


Step 3: Cache Hierarchy
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: json

    "L1I": { "sets": 128, "ways": 8,  "mshr_size": 16, "latency": 3,
             "prefetcher": "next_line" },
    "L1D": { "sets": 64,  "ways": 12, "mshr_size": 16, "latency": 5,
             "prefetcher": "ip_stride" },
    "L2C": { "sets": 2048, "ways": 16, "mshr_size": 64, "latency": 14,
             "prefetcher": "spp_dev" },
    "LLC": { "sets": 4096, "ways": 12, "mshr_size": 96, "latency": 40,
             "prefetcher": "no", "replacement": "srrip" }

Sizes: 64 KB / 48 KB / 2 MB / ~3 MB (per core) with 64 B lines.
Latencies are in **cache cycles**.  Note that ChampSim's single-LLC
model does not distinguish slice access time from interconnect hops; ``40`` is
the aggregate LLC hit latency in LLC cycles.

The defaults omit ``replacement`` at all levels except LLC; for a modern
baseline, ``srrip`` at LLC is a reasonable starting point.  Leave L1/L2 at the
built-in LRU by omitting the key.

Step 4: DRAM
^^^^^^^^^^^^^^^

.. code-block:: json

    "physical_memory": {
      "data_rate": 5600,
      "channels": 2,
      "ranks": 1,
      "bankgroups": 8,
      "banks": 4,
      "bank_rows": 65536,
      "bank_columns": 2048,
      "channel_width": 4,
      "wq_size": 96,
      "rq_size": 96,
      "nCAS": 40,
      "nRCD": 40,
      "nRP": 40,
      "nRAS": 88,
      "refresh_period": 32,
      "refreshes_per_period": 8192
    }

The timings above are approximate DDR5-5600 values scaled into ChampSim's
DRAM-cycle unit.  Two channels doubles the achievable BW; many single-channel
ChampSim runs are silently bottlenecked there.

Step 5: Validate with --dump
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Every time you change a config parameter, confirm it took effect:

.. code-block:: bash

    bin/champsim --config redwood_cove.json --dump | less

Look for ``(default)`` tags — those indicate a parameter you thought you set
but didn't (common after renames or typos).

Step 6: Why the Defaults Are What They Are
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The shipped defaults are optimized for **readability of the config file**, not
realism.  Small numbers (8-way, 64-set) make it obvious which field is which
when you're learning the format.

When in doubt, start from ``champsim_config.json`` for **structure**, refer
to **published values** for your config, and ``--dump`` after every edit to
confirm the settings landed where you expected.
