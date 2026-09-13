.. _Explicit_Config:

================================================
Explicit Configuration Format
================================================

The explicit configuration format gives full control over every module in the ChampSim
hierarchy. Unlike the :ref:`legacy format <Creating_Config>`, which uses shorthand keys
and auto-generates modules, the explicit format requires you to define every module
(channels, caches, cores, DRAM, etc.) individually.

To use the explicit format, set ``"environment": "ENVIRONMENT"`` at the root of
your JSON configuration file. All modules are defined in a flat ``"children"`` array.

----------------------------------
Basic Structure
----------------------------------

An explicit configuration file has the following top-level structure::

    {
        "environment": "ENVIRONMENT",
        "block_size": 64,
        "page_size": 4096,
        "num_cores": 1,

        "children": [
            { ... module definitions ... }
        ]
    }

Each entry in the ``"children"`` array defines one module instance. Every module has at
minimum three keys:

``name``
    A unique identifier for this module instance (e.g. ``"cpu0_L1D"``).

``module``
    The interface type this module implements (e.g. ``"cache"``, ``"core"``,
    ``"memory_controller"``, ``"channel"``, ``"prefetcher"``, ``"replacement"``,
    ``"branch_predictor"``, ``"btb"``, ``"vmem"``, ``"page_table_walker"``).

``model``
    The registered model name of the implementation to use (e.g. ``"DEFAULT_CACHE"``,
    ``"DEFAULT_CORE"``, ``"lru"``, ``"ip_stride"``).

Additional keys are module-specific parameters.

----------------------------------
Module References
----------------------------------

Modules refer to each other using the ``@name`` syntax. For example, a cache connects to
its lower-level channel via::

    "lower_level": "@cpu0_L1D_cpu0_L2C_channel"

This tells the simulator to look up the module named ``cpu0_L1D_cpu0_L2C_channel`` and
connect it as the lower-level channel for this cache.

References are resolved immediately as each module in the ``"children"`` array is
constructed, by looking up the referenced name among the modules built so far. Definition
order therefore matters: a module must be defined **before** any module that references
it. In particular, channels must be declared before the caches, cores, and memory
controllers that connect to them (a forward reference to a not-yet-constructed module
aborts with an ``[ENVIRONMENT] ERROR: @-reference ... not found``).

----------------------------------
Typed Parameter Objects
----------------------------------

Some parameters use typed wrapper objects instead of plain values:

``{"time": "250p"}``
    A time duration. Suffixes: ``p`` (picoseconds), ``n`` (nanoseconds), ``u`` (microseconds).

``{"frequency": "4G"}``
    A frequency value. Suffixes: ``G`` (GHz), ``M`` (MHz).

``{"bits": "6"}``
    A bit-width value.

``{"bytes": "8"}`` or ``{"bytes": "4Ki"}``
    A byte size. ``Ki``, ``Mi``, ``Gi`` for binary prefixes.

``{"bandwidth": 2}``
    A bandwidth value (operations per cycle).

``{"access_types": ["LOAD", "PREFETCH"]}``
    A bitmask of access types.

``{"null": "channel"}``
    A null reference for an optional module connection.

``{"optional_uint64": 1}``
    An optional 64-bit unsigned integer.

----------------------------------
Submodules (Children)
----------------------------------

Modules like caches and cores can have submodules defined inline using a nested
``"children"`` array. This is how prefetchers, replacement policies, branch predictors,
and BTBs are attached to their parent::

    {
        "name": "cpu0_L2C",
        "module": "cache",
        "model": "DEFAULT_CACHE",
        "num_sets": 1024,
        "num_ways": 8,
        ...
        "children": [
            {"name": "cpu0_L2C_prefetcher", "module": "prefetcher", "model": "ip_stride", "degree": 4},
            {"name": "cpu0_L2C_replacement", "module": "replacement", "model": "lru"}
        ]
    }

Custom parameters (like ``"degree": 4`` above) are passed to the module's constructor
via ``ModuleBuilder::get_parameter<T>()``.

.. _Explicit_Submodule_Params:

Passing Parameters to Submodules
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the explicit format, submodule parameters are **inline keys** in the child's JSON
object.  Any key that is not one of the three reserved keys (``"name"``, ``"module"``,
``"model"``) is treated as a parameter and forwarded to the ``ModuleBuilder``.

For example, the following gives the prefetcher two custom parameters::

    {
        "name": "cpu0_L2C_pref",
        "module": "prefetcher",
        "model": "ip_stride",
        "degree": 6,
        "tracker_sets": 512
    }

Inside the module's constructor, these are read as:

.. code-block:: cpp

    int degree = builder.get_parameter<int>("degree", true, 3);          // returns 6
    auto sets = builder.get_parameter<std::size_t>("tracker_sets", true, 256); // returns 512

**Parameter types** — JSON values are mapped to C++ types as follows:

* JSON integers → ``int64_t`` (use ``numeric_any_cast`` for ``int``, ``std::size_t``, etc.)
* JSON floats → ``double``
* JSON booleans → ``bool``
* JSON strings → ``std::string``

The ``get_parameter<T>()`` method handles type conversion automatically via
``numeric_any_cast`` for all arithmetic types, so requesting ``int`` from a value stored
as ``int64_t`` works seamlessly.

**Debugging parameters** — use ``--dump`` to verify that your parameters were set
correctly.  Values read from JSON show as ``(set)``; default values show as ``(default)``.

----------------------------------
Channels
----------------------------------

Channels connect modules together. A channel definition looks like::

    {
        "name": "cpu0_L1D_cpu0_L2C_channel",
        "module": "channel",
        "model": "DEFAULT_CHANNEL",
        "rq_size": 32,
        "wq_size": 32,
        "pq_size": 16,
        "offset_bits": {"bits": "6"},
        "match_offset_bits": false
    }

----------------------------------
Cores and Instruction Producers
----------------------------------

A core attaches its branch predictor, BTB, and one or more instruction producers as
``children``. An ``INSTRUCTION_PRODUCER`` reads instructions from the trace named by its
``trace_file`` parameter::

    {
        "name": "cpu0",
        "module": "core",
        "model": "DEFAULT_CORE",
        "fetch_queues": "@cpu0_cpu0_L1I_channel",
        "data_queues": "@cpu0_cpu0_L1D_channel",
        "l1i": "@cpu0_L1I",
        ...
        "children": [
            {"name": "cpu0_bp",  "module": "branch_predictor", "model": "hashed_perceptron"},
            {"name": "cpu0_btb", "module": "btb", "model": "basic_btb"},
            {"name": "cpu0_trace", "module": "instruction_producer", "model": "INSTRUCTION_PRODUCER",
             "trace_file": "$trace0"}
        ]
    }

A core may hold more than one producer. By default each producer gets its own
framework-assigned id (its own address space); to place several producers under one shared
id, give them a matching ``producer_group`` label::

    "children": [
        {"name": "cpu0_bp",  "module": "branch_predictor", "model": "hashed_perceptron"},
        {"name": "cpu0_btb", "module": "btb", "model": "basic_btb"},
        {"name": "cpu0_t0",  "module": "instruction_producer", "model": "INSTRUCTION_PRODUCER",
         "trace_file": "$trace0", "producer_group": "shared"},
        {"name": "cpu0_t1",  "module": "instruction_producer", "model": "INSTRUCTION_PRODUCER",
         "trace_file": "$trace1", "producer_group": "shared"}
    ]

----------------------------------
Orchestration Modules
----------------------------------

Phase controllers and listeners are ordinary top-level children::

    {
        "name": "pc",
        "module": "phase_controller",
        "model": "PHASE_CONTROLLER",
        "deadlock_cycles": 500,
        "warmup_length": "$warmup_instructions",
        "simulation_length": "$simulation_instructions"
    }

Any number of phase controllers may be declared (each optionally governing a subset of
consumers), a controller may define an arbitrary phase list including unmeasured
fast-forward phases, and the root-level keys ``"cycle_skip"`` and ``"heartbeat_frequency"``
tune the orchestration defaults. (Event listeners such as the heartbeat are compile-time
instrumentation, not config modules — see :ref:`Orchestration`.) The full contracts,
parameters, and composition rules are documented in :ref:`Orchestration`.

----------------------------------
A Minimal Example
----------------------------------

The following is a stripped-down explicit config showing the essential structure. See
``champsim_config_explicit.json`` in the repository root for a complete example with
all default modules::

    {
        "environment": "ENVIRONMENT",
        "block_size": 64,
        "page_size": 4096,
        "num_cores": 1,

        "children": [
            {
                "name": "LLC_DRAM_channel",
                "module": "channel",
                "model": "DEFAULT_CHANNEL",
                "rq_size": 64, "wq_size": 64, "pq_size": 64,
                "offset_bits": {"bits": "6"},
                "match_offset_bits": false
            },
            {
                "name": "DRAM",
                "module": "memory_controller",
                "model": "DEFAULT_MEMORY_CONTROLLER",
                "channels": 1,
                "ul_channels": ["@LLC_DRAM_channel"],
                ...
            },
            {
                "name": "LLC",
                "module": "cache",
                "model": "DEFAULT_CACHE",
                "num_sets": 2048, "num_ways": 16,
                "lower_level": "@LLC_DRAM_channel",
                "children": [
                    {"name": "LLC_pref", "module": "prefetcher", "model": "no"},
                    {"name": "LLC_repl", "module": "replacement", "model": "lru"}
                ]
            },
            ...
        ]
    }

----------------------------------
Debugging with --dump
----------------------------------

Use ``--dump`` to see the fully-resolved module hierarchy that ChampSim constructs from
your config. This is useful for verifying that modules are connected correctly::

    bin/champsim --config my_config.json --dump

This prints every module, its parameters, and its connections to stdout.

----------------------------------
Globals and Lexical Scoping
----------------------------------

Every non-reserved top-level scalar in the config is a *global*: any module can read it
through ``get_parameter`` fall-through, exactly like a locally-declared parameter. A
top-level ``"globals"`` object is equivalent for those who prefer it spelled out::

    {
        "l2_prefetch_degree": 4,
        "globals": {"trace_warmup_fraction": 0.2},
        "children": [ ... ]
    }

A ``"globals"`` object on any module opens a *lexical scope*: its keys are visible to
that module and everything beneath it in the tree, shadowing outer scopes, and shadowed
by local parameters. Ordinary module parameters remain module-local by design — a
submodule can never accidentally capture its parent's ``sets`` or ``latency``::

    {
        "name": "cpu0_L2C", "module": "cache", "model": "DEFAULT_CACHE",
        "globals": {"prefetch_degree": 8},
        "children": [
            {"name": "pf_a", "module": "prefetcher", "model": "my_pref"},
            {"name": "pf_b", "module": "prefetcher", "model": "my_pref",
             "prefetch_degree": 2}
        ]
    }

Here ``pf_a`` resolves ``prefetch_degree`` to 8 from the cache's scope; ``pf_b``'s local
value of 2 shadows it. Resolution order is always: local parameter, then enclosing
scopes innermost-first, then the root globals.
