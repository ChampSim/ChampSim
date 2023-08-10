ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link
LDLIBS   += -llzma -lz -lbz2 -lfmt

.PHONY: all all_execs clean configclean test makedirs list_execs

all: all_execs

test_main_name=$(ROOT_DIR)/test/bin/000-test-main

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - $(dirs), the list of all directories that hold object files
#  - $(objs), the list of all object files corresponding to sources
#  - All dependencies and flags assigned according to the modules
include _configuration.mk

all_execs: $(filter-out $(test_main_name), $(executable_name))

# Remove all intermediate files
clean:
	@-find src test .csconfig $(module_dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-$(RM) inc/champsim_constants.h
	@-$(RM) inc/cache_modules.h
	@-$(RM) inc/ooo_cpu_modules.h
	@-$(RM) src/core_inst.cc
	@-$(RM) $(test_main_name)

# Remove all configuration files
configclean: clean
	@-$(RM) -r $(module_dirs) _configuration.mk

# Make directories that don't exist
# exclude "test" to not conflict with the phony target
$(filter-out test, $(sort $(dirs))): | $(dir $@)
	-mkdir $@

# All .o files should be made like .cc files
$(objs):
	$(CXX) $(addprefix @,$(filter %.options, $^)) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $(filter %.cc, $^)

# Link test executable
$(test_main_name): CXXFLAGS += -g3 -Og -Wconversion
$(test_main_name): LDLIBS += -lCatch2Main -lCatch2
$(test_main_name):
	$(LINK.cc) $(LDFLAGS) -o $@ $(filter-out %/main.o, $^) $(LOADLIBES) $(LDLIBS)

# Link main executables
$(filter-out $(test_main_name), $(executable_name)):
	$(LINK.cc) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

# Tests: build and run
test: $(test_main_name)
	$(test_main_name)

pytest:
	PYTHONPATH=$(PYTHONPATH):$(shell pwd) python3 -m unittest discover -v --start-directory='test/python'

-include $(foreach dir,$(wildcard .csconfig/*/) $(wildcard .csconfig/test/*/),$(wildcard $(dir)/obj/*.d))

