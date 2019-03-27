cmake_minimum_required(VERSION 2.8.12)

project(CPUSimulator C CXX)

set(CHAMPSIM_BRANCH_PREDICTOR "hashed_perceptron" CACHE STRING "The branch prediction mechanism used by the simulation.")
set(CHAMPSIM_PREFETCHER_L1D "l1d_no" CACHE STRING "The prefetching mechanism used in the L1D cache.")
set(CHAMPSIM_PREFETCHER_L2C "l2c_no" CACHE STRING "The prefetching mechanism used in the L2C cache.")
set(CHAMPSIM_PREFETCHER_LLC "llc_no" CACHE STRING "The prefetching mechanism used in the LLC cache.")
set(CHAMPSIM_REPLACEMENT_POLICY "lru" CACHE STRING "The replacement policy used in the Last-Level Cache.")
set(CHAMPSIM_NUMBER_CORE "1" CACHE STRING "The number of core used by the simulator.")

################################################################################
# Getting all the internals sources of ChampSim to compile them.               #
# As well, we need the sources from the different aspect of the simulator,     #
# namely the prefetching mechanism for each level of the memory hierarchy,     #
# the replacement policy for the last level cache (LLC) and the branch         #
# prediction machanism.                                                        #
#                                                                              #
# Note : The internal sources are the common ones, used in any variants of     #
# ChampSim.                                                                    #
################################################################################

file(
  GLOB_RECURSE
  CHAMPSIM_INTERNALS_SOURCES
  src/internals/*.cc
)

# Looking for the branch predictor related-files and knobs.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/branch/${CHAMPSIM_BRANCH_PREDICTOR}/${CHAMPSIM_BRANCH_PREDICTOR}.cmake")
    include(${CMAKE_CURRENT_SOURCE_DIR}/src/branch/${CHAMPSIM_BRANCH_PREDICTOR}/${CHAMPSIM_BRANCH_PREDICTOR}.cmake)
else ()
    message(FATAL_ERROR "The branch predictor ${CHAMPSIM_BRANCH_PREDICTOR} could not be found. Aborting!")
endif ()

# Looking for the L1D prefetcher related-files and knobs.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/prefetcher/${CHAMPSIM_PREFETCHER_L1D}/${CHAMPSIM_PREFETCHER_L1D}.cmake")
	include(${CMAKE_CURRENT_SOURCE_DIR}/src/prefetcher/${CHAMPSIM_PREFETCHER_L1D}/${CHAMPSIM_PREFETCHER_L1D}.cmake)
else ()
	message(FATAL_ERROR "The L1D prefetcher ${CHAMPSIM_PREFETCHER_L1D} could not be found. Aborting!")
endif ()

# Looking for the L2C prefetcher related-files and knobs.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/prefetcher/${CHAMPSIM_PREFETCHER_L2C}/${CHAMPSIM_PREFETCHER_L2C}.cmake")
	include(${CMAKE_CURRENT_SOURCE_DIR}/src/prefetcher/${CHAMPSIM_PREFETCHER_L2C}/${CHAMPSIM_PREFETCHER_L2C}.cmake)
else ()
	message(FATAL_ERROR "The L2C prefetcher ${CHAMPSIM_PREFETCHER_L2C} could not be found. Aborting!")
endif ()

# Looking for the LLC prefetcher related-files and knobs.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/prefetcher/${CHAMPSIM_PREFETCHER_LLC}/${CHAMPSIM_PREFETCHER_LLC}.cmake")
	include(${CMAKE_CURRENT_SOURCE_DIR}/src/prefetcher/${CHAMPSIM_PREFETCHER_LLC}/${CHAMPSIM_PREFETCHER_LLC}.cmake)
else ()
	message(FATAL_ERROR "The LLC prefetcher ${CHAMPSIM_PREFETCHER_LLC} could not be found. Aborting!")
endif ()

# Looking for the replacement policy related-files and knobs.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/replacement/${CHAMPSIM_REPLACEMENT_POLICY}/${CHAMPSIM_REPLACEMENT_POLICY}.cmake")
	include(${CMAKE_CURRENT_SOURCE_DIR}/src/replacement/${CHAMPSIM_REPLACEMENT_POLICY}/${CHAMPSIM_REPLACEMENT_POLICY}.cmake)
else ()
	message(FATAL_ERROR "The replacement policy ${CHAMPSIM_REPLACEMENT_POLICY} could not be found. Aborting!")
endif ()

# We also have to specify the internals source directory as an include directory.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/internals)

# Modifying the output directory of the binaries.
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# Then, we need to find a proper name for this binary.
set(CHAMPSIM_BINARY_NAME "${CHAMPSIM_BRANCH_PREDICTOR}-${CHAMPSIM_PREFETCHER_L1D}-${CHAMPSIM_PREFETCHER_L2C}-${CHAMPSIM_PREFETCHER_LLC}-${CHAMPSIM_REPLACEMENT_POLICY}-${CHAMPSIM_NUMBER_CORE}core")

# A sum-up of the current simulator configuration would be welcome. ;)
message(STATUS "The CPU Simulator has successfully been configured! :D")
message(STATUS "Using the branch predictor: ${CHAMPSIM_BRANCH_PREDCITOR}")
message(STATUS "Using the L1D prefetcher: ${CHAMPSIM_PREFETCHER_L1D}")
message(STATUS "Using the L2C prefetcher: ${CHAMPSIM_PREFETCHER_L2C}")
message(STATUS "Using the LLC prefetcher: ${CHAMPSIM_PREFETCHER_LLC}")
message(STATUS "Using the replacement policy: ${CHAMPSIM_REPLACEMENT_POLICY}")
message(STATUS "Using ${CHAMPSIM_NUMBER_CORE} cores")

add_executable(
  ${CHAMPSIM_BINARY_NAME}
  ${CHAMPSIM_INTERNALS_SOURCES}
  ${CHAMPSIM_BRANCH_PREDICTOR_SOURCES}
  ${CHAMPSIM_PREFETCHER_L1D_SOURCES}
  ${CHAMPSIM_PREFETCHER_L2C_SOURCES}
  ${CHAMPSIM_PREFETCHER_LLC_SOURCES}
  ${CHAMPSIM_REPLACEMENT_POLICY_SOURCES}
)
