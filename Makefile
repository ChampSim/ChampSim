override ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
DEP_ROOT = $(ROOT_DIR)/.csconfig/dep

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link
LDLIBS   += -llzma -lz -lbz2 -lfmt

.PHONY: all clean configclean test

test_main_name=$(ROOT_DIR)/test/bin/000-test-main

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - $(dirs), the list of all directories that hold object files
#  - $(objs), the list of all object files corresponding to sources
#  - All dependencies and flags assigned according to the modules
#
# Customization points:
#  - OBJ_ROOT: at make-time, override the object file directory
include _configuration.mk

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

# Make directories that don't exist
$(sort $(dirs)):
	-mkdir $@

reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))

%/absolute.options: | %
	echo '-I$(ROOT_DIR)/inc -isystem $(TRIPLET_DIR)/include' > $@

# All .o files should be made like .cc files
$(objs):
	$(CXX) $(call reverse, $(addprefix @,$(filter %.options, $^))) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $(filter %.cc, $^)

%.d:
	@set -e; \
	rm -f $@; \
	$(CXX) -MM -MG -MF $@.$$$$ $(CPPFLAGS) $(call reverse, $(addprefix @,$(filter %.options, $^))) $(filter %.cc, $^) &> /dev/null; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

# Link test executable
$(test_main_name): CXXFLAGS += -g3 -Og -Wconversion
$(test_main_name): LDLIBS += -lCatch2Main -lCatch2

ifdef POSTBUILD_CLEAN
.INTERMEDIATE: $(objs) $($(OBJS):.o=.d)
endif

# Link main executables
$(executable_name):
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

# Tests: build and run
test: $(test_main_name)
	$(test_main_name)

pytest:
	PYTHONPATH=$(PYTHONPATH):$(ROOT_DIR) python3 -m unittest discover -v --start-directory='test/python'

