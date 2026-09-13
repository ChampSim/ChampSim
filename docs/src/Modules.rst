.. _Modules:

====================================
The ChampSim Module System
====================================

ChampSim provides a runtime module system that allows you to swap implementations of key
simulator components without recompilation. Each component type has an **interface** (a C++
abstract base class with pure virtual methods) and one or more **implementations** (concrete
classes that override every method). ChampSim ships default implementations for all
interfaces; you can replace any of them with your own.

Modules are implemented as C++ classes that inherit from an interface base class in the
``champsim::modules`` namespace. Every module:

1. **Inherits** from one of the interface base classes listed below.
2. **Implements** every pure virtual method declared by that interface (use empty stubs
   ``override {}`` for methods where no behavior is needed).
3. **Accepts a** ``champsim::modules::ModuleBuilder`` **in its constructor** to receive
   configuration parameters and access its parent module.
4. **Registers itself** with a static registration line so the simulator can instantiate it
   by name at runtime.

------------------------------------------
Interfaces and Implementations
------------------------------------------

ChampSim distinguishes between **interfaces** and **implementations**:

* An **interface** is an abstract base class that defines the contract a module must fulfill.
  Interfaces live in ``champsim::modules`` and have names like ``prefetcher``,
  ``replacement``, ``branch_predictor``, ``btb``, ``cache_module``, ``core_module``, etc.

* An **implementation** is a concrete class that inherits from an interface and provides the
  actual behavior. ChampSim ships default implementations (e.g. ``DEFAULT_CACHE``,
  ``DEFAULT_CORE``, ``lru``, ``hashed_perceptron``, ``ip_stride``, etc.).

When writing a new module, you implement an existing interface. The four user-facing
module interfaces are:

* ``champsim::modules::branch_predictor`` — attached to a core
* ``champsim::modules::btb`` — attached to a core
* ``champsim::modules::prefetcher`` — attached to a cache
* ``champsim::modules::replacement`` — attached to a cache

Higher-level interfaces (``cache_module``, ``core_module``, ``memory_controller_module``,
etc.) define the contracts that caches, cores, and DRAM controllers fulfill. The default
implementations ``DEFAULT_CACHE`` (``CACHE`` class) and ``DEFAULT_CORE`` (``O3_CPU`` class)
implement these interfaces. See :ref:`Cache_model` and :ref:`Core_model` for details.

The simulation loop itself is driven by orchestration interfaces —
``packet_producer`` (packet providers), ``phase_controller`` (run structure, completion,
and health), and ``listener`` (run-wide reporting) — plus the ``packet_consumer``
mixin for anything that executes packets. See :ref:`Orchestration` for the full
contracts.

------------------------------------------
Module Construction and ModuleBuilder
------------------------------------------

All modules receive a ``champsim::modules::ModuleBuilder`` in their constructor. The
``ModuleBuilder`` provides:

* **Configuration parameters** from the JSON config file via ``get_parameter<T>()``.
* **Access to the parent module** via ``get_parent<T>()``. For example, a prefetcher can
  obtain a pointer to its parent cache.
* **The module's name and model** via ``get_name()`` and ``get_model()``.

Example constructor::

    struct my_pref : champsim::modules::prefetcher {
        champsim::modules::cache_module* cache_ = nullptr;
        int degree = 3;

        my_pref(champsim::modules::ModuleBuilder builder)
            : cache_(builder.get_parent<champsim::modules::cache_module>()),
              degree(builder.get_parameter<int>("degree", true, 3))
        {}
    };

.. doxygenstruct:: champsim::modules::ModuleBuilder
   :members: get_parameter,get_parent,get_name,get_model

------------------------------------------
Module Registration
------------------------------------------

After defining your module class, register it with a static registration line in the
``.cc`` file::

    #include "my_pref.h"
    #include "cache.h"

    champsim::modules::prefetcher::register_module<my_pref> my_pref_register("my_pref");

The string ``"my_pref"`` is the **model name** — this is the value you use in the JSON
configuration file to select your module::

    { "L2C": { "prefetcher": "my_pref" } }

Or in the explicit config format::

    { "name": "L2C_prefetcher", "module": "prefetcher", "model": "my_pref", "degree": 4 }

By convention, the model name matches the directory name under ``prefetcher/``,
``replacement/``, ``branch/``, or ``btb/``.

----------------------------
Branch Predictors
----------------------------

A branch predictor module inherits from ``champsim::modules::branch_predictor`` and must
implement three functions.

.. doxygenstruct:: champsim::modules::branch_predictor
   :members:

-----------------------------------
Branch Target Buffers
-----------------------------------

A BTB module inherits from ``champsim::modules::btb`` and must implement three functions.

.. doxygenstruct:: champsim::modules::btb
   :members:

-----------------------------------
Memory Prefetchers
-----------------------------------

A prefetcher module inherits from ``champsim::modules::prefetcher`` and must implement six
functions. The ``prefetch_line()`` helper is provided by the base class for issuing
prefetch requests into the parent cache.

.. doxygenstruct:: champsim::modules::prefetcher
   :members:

-----------------------------------
Replacement Policies
-----------------------------------

A replacement policy module inherits from ``champsim::modules::replacement`` and must
implement five functions.

.. doxygenstruct:: champsim::modules::replacement
   :members:

-----------------------------------
Cache Module Interface
-----------------------------------

The ``cache_module`` interface defines the contract that caches fulfill. Prefetchers
and replacement policies can query their parent cache through this interface, for
example to read queue occupancy or cache geometry.

.. doxygenstruct:: champsim::modules::cache_module
   :members:

-----------------------------------
Core Module Interface
-----------------------------------

The ``core_module`` interface defines the contract that CPU cores fulfill. Branch
predictors and BTBs are attached to a core_module.

.. doxygenstruct:: champsim::modules::core_module
   :members:

-----------------------------------
Memory Controller Interface
-----------------------------------

The ``memory_controller_module`` interface defines the contract that DRAM controllers
fulfill. Stats can be retrieved per channel.

.. doxygenstruct:: champsim::modules::memory_controller_module
   :members:

