Welcome to the ChampSim wiki!
====================================

ChampSim is an open-source trace based simulator maintained at Texas A&M University and through the support of the computer architecture community.
ChampSim was originally developed to provide a platform for microarchitectural competitions (DPC3, DPC2, CRC2, IPC1, etc.) and has since been used for the development of multiple state-of-the-art cache replacement and prefetching policies.

We encourage you to read below to see if ChampSim is right for your research, class, or project!

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   Modules
   Module-support-library
   Creating-a-configuration-file
   Configuration-API
   Address-operations
   Byte-sizes
   Bandwidth
   Core-model
   Cache-model
   Legacy-modules

ChampSim is commonly used as the basis for academic research.
See a list of publications that use ChampSim :ref:`here <Publications>`.

-------------------
ChampSim's Goal
-------------------

The ultimate design philosophy of ChampSim is to provide an environment where an architect can realistically begin performing significant research within a month, but the simulator works right out of the box for exploration and quickly prototyping cache management ideas!
With this in mind, ChampSim development works towards maintaining an accurate Out of Order execution and cache memory model without the overhead of larger multi-layered architectural research platforms.
There are ChampSim-ready traces available online (from CRC2, IPC1, DPC3, and CVP) to let you quickly get started.
The ChampSim design philosophy is summed up in three main design principles:

* Low startup time: The user should be able to perform new and meaningful memory system research in one month.
* Little architecture knowledge required: Users need only an entry-level comprehension of C++ to begin researching prefetching and replacement schemes.
* Design configurability: The core model should be flexible enough to model a variety of commodity devices.

-------------------
What ChampSim is
-------------------

* A fast trace-based simulator that can get you started in computer architecture research quickly without a deeper understanding of microarchitecture.
* A modular simulator, with multiple interchangeable pieces that can be configured at compile time. This leads to diverse simulation environments and enables you to simulate the system you need!

----------------------
What ChampSim is not
----------------------

* ChampSim is not ideal for research related to OS and OS-interactions... without modifications. ChampSim is a trace-based simulator, meaning that a program was executed with an instrumentation tool, like PIN or Dynamorio, to create a file that represents the instructions executed by some processor. This means that ChampSim does not have any OS interfaces or full-system mode.

* The traces ChampSim uses do not currently support specific instructions aside from memory load/store operations. A non-memory instruction is not fully simulated but given a flat latency and consumes resources from the front-end core model.

* **Perfect**- ChampSim is an ongoing project and while we are working on improving the models in general, there are some pieces that are in active development. We encourage you to make us aware of something you think may make this simulator even better and even submit a pull request to help further development!


------------------
ChampSim Features
------------------

* Heterogeneous cores
* Configurable cache hierarchy - Size, queues, associativity
* Different levels and interconnectivity between cache levels
* Aggressive decoupled frontend
* Modular components including:

  * Branch predictor
  * Cache prefetchers
  * Cache replacement policy
  * Branch target buffers
  * Extendable DRAM model

================
FAQ
================

Why use trace-based simulation?
  Trace-based simulation is much faster than full system, allowing for students and researchers to quickly jump into architectural research, configuring the system and quickly reviewing simulation results.
  This allows for an easier collaboration with industry. The ChampSim traces contain no opcode-specific information making it impossible to reverse engineer the original program that the trace was generated from. Any sensitive information in the traces can be removed through randomization, fuzzing, or other changes to the file without disrupting the overall behavior of the application.
  Traces are easy to distribute for competitions, collaboration, or classroom settings.

Why is it called ChampSim?
  This simulator was originally developed for quickly creating and evaluating ideas for Championships, so it's the Championship Simulator. Since its conception as a competition platform, ChampSim has grown into a research and education platform.

I want to contribute! How do I do that?
  We're glad you asked! Take a look at some of the issues and their tags or get started on a new feature for ChampSim. We are currently using the develop branch to add improvements to the overall simulator. When you're done, make a pull request and we'll review it to be merged into the develop branch of the repository.

How do we cite ChampSim?
  For now, please cite our `arXiv paper <https://arxiv.org/abs/2210.14324>`_.

