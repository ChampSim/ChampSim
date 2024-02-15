override ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
DEP_ROOT = $(ROOT_DIR)/.csconfig/dep

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
override LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link

RAMULATOR_DIR=$(ROOT_DIR)/ramulator2
RAMULATOR_LIB=$(RAMULATOR_DIR)
LDLIBS   += -llzma -lz -lbz2 -lfmt
INC=

.PHONY: all clean configclean test

test_main_name=$(ROOT_DIR)/test/bin/000-test-main
executable_name:=
dirs:=

# Migrate names from a source directory (and suffix) to a target directory (and suffix)
# $1 - source directory
# $2 - target directory
# $3 - source suffix
# $4 - target suffix
migrate = $(patsubst $1/%$3,$2/%$4,$(wildcard $1/*$3))

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - $(dirs), the list of all directories that hold object files
#  - $(objs), the list of all object files corresponding to sources
#  - All dependencies and flags assigned according to the modules
#
# Customization points:
#  - OBJ_ROOT: at make-time, override the object file directory
include _configuration.mk


#if ramulator exists, include the library as well as the compile flags. We also add an extra dependency to the build, ensuring ramulator has been compiled.
ifeq ($(RAMULATOR_MODEL),1)
LDLIBS := -L$(RAMULATOR_LIB) -L$(RAMULATOR_DIR)deps/spdlog-build -L$(RAMULATOR_DIR)/deps/yaml-cpp-build -lspdlog -lyaml-cpp $(LDLIBS)
CPPFLAGS += -DRAMULATOR -I$(RAMULATOR_DIR)/src
$(filter-out $(test_main_name), $(executable_name)) : -lramulator
endif

#this target invokes ramulator's build system and copies our local plugins into their system.
-lramulator:
	cp -r ramulator_plugins/* ramulator2/. && \
	cd ramulator2 && \
	mkdir -p build && \
	cd build && \
	cmake .. && \
	make -j;


all: $(filter-out $(test_main_name), $(executable_name))

.DEFAULT_GOAL := all


	


# Remove all intermediate files
clean:
	@-find src test .csconfig branch btb prefetcher replacement $(clean_dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-$(RM) inc/champsim_constants.h
	@-$(RM) inc/cache_modules.h
	@-$(RM) inc/ooo_cpu_modules.h
	@-$(RM) src/core_inst.cc
	@-$(RM) $(test_main_name)

# Remove all configuration files
configclean: clean
	@-$(RM) -r $(dirs) _configuration.mk

reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))

%/absolute.options: | %
	echo '-I$(ROOT_DIR)/inc -isystem $(TRIPLET_DIR)/include' > $@

# All .o files should be made like .cc files
$(objs):
	mkdir -p $(@D)
	$(CXX) $(call reverse, $(addprefix @,$(filter %.options, $^))) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $(filter %.cc, $^)


%.d:
	mkdir -p $(@D)
	$(CXX) -MM -MT $@ -MT $</$(*F).o -MF $@ $(CPPFLAGS) $(call reverse, $(addprefix @,$(filter %.options, $^))) $(filter %.cc, $^)


# Link test executable
$(test_main_name): override CPPFLAGS += -DCHAMPSIM_TEST_BUILD
$(test_main_name): override CXXFLAGS += -g3 -Og
$(test_main_name): override LDLIBS += -lCatch2Main -lCatch2

ifdef POSTBUILD_CLEAN
.INTERMEDIATE: $(objs) $($(OBJS):.o=.d)
endif

# Link main executables
$(executable_name):

	mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)


# Tests: build and run
ifdef TEST_NUM
selected_test = -\# "[$(addprefix #,$(filter $(addsuffix %,$(TEST_NUM)), $(patsubst %.cc,%,$(notdir $(wildcard $(ROOT_DIR)/test/cpp/src/*.cc)))))]"
endif
test: $(test_main_name)
	$(test_main_name) $(selected_test)

pytest:
	PYTHONPATH=$(PYTHONPATH):$(ROOT_DIR) python3 -m unittest discover -v --start-directory='test/python'

