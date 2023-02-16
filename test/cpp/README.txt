This directory includes the testing infrastructure for ChampSim. The tests rely on CATCH2, a publicly available framework.

The files are organized as follows: "src/xxx-descriptive-name.cc", where "xxx" is a three-digit number whose value indicates the portion of the simulator being tested.

000 - Reserved: main function
001-099 - Top-level and utility functionality
100-199 - Core front-end
200-299 - Execution core (out-of-order)
300-399 - Core retirement
400-499 - Caches
600-699 - Page table walkers
700-799 - DRAM
800-899 - Virtual Memory
900-999 - Peculiar and assorted bugs

The tests can be run using the top-level make, using 'make test'

