.. _Creating_Config:

================================================
Creating a Configuration File
================================================

.. note::

   This page describes the **legacy** configuration format, which uses high-level shorthand
   keys (``"branch_predictor"``, ``"L1D"``, ``"num_cores"``, etc.) to auto-generate the full
   module hierarchy at runtime. This format is handled by the ``LEGACY_ENVIRONMENT`` module.

   For full control over every module in the hierarchy, see the
   :ref:`Explicit Configuration Format <Explicit_Config>`.

The configuration file is a central fixture of ChampSim.
An example configuration file (``champsim_config.json``) is given in the root of the repository, but it is large and unwieldy.
Your configuration file will likely be much smaller.
This page will walk you through many of the features of the ChampSim configuration system.

The configuration file is loaded at runtime via the ``--config`` flag::

    bin/champsim --config my_config.json --warmup-instructions 200000000 --simulation-instructions 500000000 trace.xz

If ``--config`` is omitted, ChampSim loads ``champsim_config.json`` from the current directory.
You can also pipe a config via stdin with ``--config -``.

-------------------------------
Your first configuration file
-------------------------------

All ChampSim configuration files are JSON files.
We can start with the most trivial of configuration files.::

    {
    }

This would specify a default configuration.
But, this is not frequently useful.
Let's change the branch predictor that ChampSim uses.
Legal values for the ``branch_predictor`` key are registered model names.
By convention, these match the directory names under the ``branch/`` directory
(e.g. ``"hashed_perceptron"``, ``"bimodal"``, ``"perceptron"``).
The same convention applies for BTBs (under ``btb/``),
prefetchers (under ``prefetcher/``), and replacement policies (under ``replacement/``).::

    {
        "ooo_cpu": [
            { "branch_predictor": "perceptron" }
        ]
    }

This configuration file will configure ChampSim with a default configuration, but with a perceptron branch predictor instead.
Core options must be placed inside an entry of the ``ooo_cpu`` array; keys placed at the config root are ignored for the core.
We can take this further and specify many things about our single-core system.::

    {
        "ooo_cpu": [
            {
                "branch_predictor": "perceptron",

                "rob_size": 226, "lq_size": 90, "sq_size": 85,
                "fetch_width": 6, "decode_width": 6, "lq_width": 2, "sq_width": 1,

                "decode_latency": 3, "execute_latency": 2
            }
        ]
    }

Each of these options will specify something about our core. Next, we'll specify some of our caches.

---------------------
Cache Configuration
---------------------

We can begin by changing some features of our L1 data cache.
We can specify its size based on the block size, the number of sets, and the number of ways.
The following configuration file specifies a 64 KiB L1 data cache.::

    {
        "block_size": 64,
        "L1D": { "sets": 256, "ways": 4 }
    }

Note that 64 is the default block size, specified here for clarity.
The default prefetcher is the do-nothing prefetcher, and the default replacement policy is LRU, but they can be specified, too.::

    {
        "L1D": {
            "sets": 256, "ways": 4,
            "prefetcher": "next_line",
            "replacement": "drrip"
        }
    }

Specifying a cache this way will create an identical L1D for each core in the configuration.
So far, we've only handled the single-core case.

--------------------------
Multi-core configurations
--------------------------

Multi-core simulations can be enabled by specifying the ``num_cores`` key.::

    {
        "num_cores": 2
    }

This will create two identical cores with default configurations.
This can be combined with the other specifications that we have made so far to create identical cores.::

    {
        "num_cores": 2,
        "ooo_cpu": [
            {
                "branch_predictor": "perceptron",
                "rob_size": 190
            }
        ],

        "L1D": {
            "sets": 256, "ways": 4,
            "prefetcher": "next_line",
            "replacement": "drrip"
        }
    }

-----------------------
Heterogeneous systems
-----------------------

ChampSim supports a variety of system configurations, including systems that are not homogeneous.
For example, we could create two cores with different ROB sizes.
Specify all cores in a list under the ``ooo_cpu`` key.::

    {
        "num_cores": 2,
        "ooo_cpu": [
            { "rob_size": 120 },
            { "rob_size": 240 }
        ]
    }

Each object in the ``ooo_cpu`` list specifies one core, and takes all of the options that we have discussed so far.::


    {
        "num_cores": 2,
        "ooo_cpu": [
            {
                "branch_predictor": "bimodal",
                "rob_size": 120, "lq_size": 90,
                "L1D": { "prefetcher": "next_line" }
            },
            {
                "branch_predictor": "gshare",
                "rob_size": 240, "lq_size": 70,
                "L1D": { "prefetcher": "no" }
            }
        ]
    }

The ``LEGACY_ENVIRONMENT`` builds a fixed cache topology: for each core it instantiates
private ``L1I``, ``L1D``, ``ITLB``, ``DTLB``, ``L2C``, and ``STLB`` caches, all sharing a
single last-level cache named ``LLC`` that feeds ``DRAM``. There is no ``caches`` key, and
caches cannot be referred to by a ``name``; the ``L1D``/``L2C``/``LLC`` keys take objects
of settings, not string references to a named cache.

Heterogeneous per-core cache settings are expressed by placing the cache-override object
inside the corresponding ``ooo_cpu`` entry, which is merged over the matching top-level
cache object for that core.
In the following configuration, each core has a distinctly-configured L1 cache.::

    {
        "num_cores": 2,
        "ooo_cpu": [
            { "L1D": { "replacement": "lru" } },
            { "L1D": { "replacement": "srrip" } }
        ]
    }

Arbitrary topologies (multiple or named last-level caches, custom ``lower_level`` wiring,
extra levels such as an L4) are not supported by this legacy format. For full control over
every module and its connections, use the
:ref:`Explicit Configuration Format <Explicit_Config>`.

.. _Legacy_Submodule_Params:

-------------------------------------------
Passing Parameters to Submodules
-------------------------------------------

By default, submodules (prefetchers, replacement policies, branch predictors, BTBs) are
specified by name as a plain string::

    {
        "L2C": { "prefetcher": "ip_stride" }
    }

To pass configuration parameters to a submodule, use the **object form** instead.
Specify the model name under the ``"model"`` key and any additional keys as parameters::

    {
        "L2C": {
            "prefetcher": {
                "model": "ip_stride",
                "degree": 4,
                "tracker_sets": 512
            }
        }
    }

These additional keys (``"degree"``, ``"tracker_sets"``) are forwarded to the module's
constructor via ``ModuleBuilder``.  Inside the module, they are read with
``builder.get_parameter<T>("degree", true, 3)`` (see the :ref:`Tutorial <Tutorial_Prefetcher>`).

**Multiple submodules with parameters** — when specifying multiple modules of the same
type (e.g. chained prefetchers), use the **array form**::

    {
        "L2C": {
            "prefetcher": [
                "no",
                {
                    "model": "ip_stride",
                    "degree": 6,
                    "tracker_sets": 256
                }
            ]
        }
    }

Plain string entries (``"no"``) get no extra parameters.  Object entries must have a
``"model"`` key plus any desired parameters.

This same pattern works for any submodule type::

    {
        "L2C": {
            "replacement": {
                "model": "srrip",
                "max_rrpv": 7
            }
        },
        "branch_predictor": {
            "model": "hashed_perceptron",
            "num_tables": 16
        }
    }
