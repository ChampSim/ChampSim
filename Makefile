override ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
override LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link

#list of external build dependencies
EXT_BUILD =

LDLIBS   += -llzma -lz -lbz2 -lfmt

.PHONY: all clean configclean test

test_main_name=$(ROOT_DIR)/test/bin/000-test-main
executable_name:=
dirs:=
deps:=

# Migrate names from a source directory (and suffix) to a target directory (and suffix)
# $1 - source directory
# $2 - target directory
# $3 - source suffix
# $4 - target suffix
# $5 - unique build id
migrate = $(patsubst $1/%$3,$2/$(5)_%$4,$(filter %main.cc,$(wildcard $1/*$3))) $(patsubst $1/%$3,$2/%$4,$(filter-out %main.cc,$(wildcard $1/*$3)))

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - $(dirs), the list of all directories that hold object files
#  - $(objs), the list of all object files corresponding to sources
#  - All dependencies and flags assigned according to the modules
#
# Customization points:
#  - OBJ_ROOT: at make-time, override the object file directory
include _configuration.mk


override CPPFLAGS += -I$(ROOT_DIR)/ramulator2/src
override LDFLAGS  += -L$(ROOT_DIR)/ramulator2 -L$(ROOT_DIR)/ramulator2/build/_deps/spdlog-build -L$(ROOT_DIR)/ramulator2/build/_deps/yaml-cpp-build

#if ramulator has been installed
ifneq ($(wildcard $(ROOT_DIR)/ramulator2/.*),)
	#this feels bad. How to only build ramulator when needed?
	override EXT_BUILD += $(ROOT_DIR)/ramulator2/libramulator.so
	override LDLIBS    += -lspdlog -lyaml-cpp -lramulator
	override CPPFLAGS  += -DRAMULATOR
endif


all: $(filter-out $(test_main_name), $(executable_name))


.DEFAULT_GOAL := all

#if we need to build ramulator
$(ROOT_DIR)/ramulator2/libramulator.so:
	cp -r ramulator_plugins/* ramulator2/. && \
	cd ramulator2 && \
	mkdir -p build && \
	cd build && \
	cmake .. && \
	$(MAKE);

# Remove all intermediate files
clean:
	@-find src test .csconfig branch btb prefetcher replacement $(dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-$(RM) inc/champsim_constants.h
	@-$(RM) inc/cache_modules.h
	@-$(RM) inc/ooo_cpu_modules.h
	@-$(RM) src/core_inst.cc
	@-$(RM) $(test_main_name)

# Remove all configuration files
configclean: clean
	@-$(RM) -r _configuration.mk

reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))

$(ROOT_DIR)/absolute.options:
	@echo "-I$(ROOT_DIR)/inc -isystem $(TRIPLET_DIR)/include" > $@

$(sort $(dirs)):
	-mkdir -p $@

# All .o files should be made like .cc files
$(sort $(objs)):
	$(CXX) $(call reverse, $(addprefix @,$(filter %.options, $^))) -MMD -MT $@ -MT $(@:.o=.d) $(sort $(CPPFLAGS)) $(CXXFLAGS) -c -o $@ $(filter %.cc, $^)


# Link test executable
$(test_main_name): override CPPFLAGS += -DCHAMPSIM_TEST_BUILD
$(test_main_name): override CXXFLAGS += -g3 -Og
$(test_main_name): override LDLIBS += -lCatch2Main -lCatch2

ifdef POSTBUILD_CLEAN
.INTERMEDIATE: $(objs) $($(OBJS):.o=.d)
endif

# Link main executables
$(executable_name): $(EXT_BUILD)

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

ifeq (,$(filter clean configclean pytest, $(MAKECMDGOALS)))
-include $(objs:.o=.d)
endif
