ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))

CPPFLAGS += -MMD -I$(ROOT_DIR)/inc
CXXFLAGS += --std=c++17 -O3 -Wall -Wextra -Wshadow -Wpedantic

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
CPPFLAGS += -isystem $(TRIPLET_DIR)/include
LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link

.phony: all all_execs clean configclean test makedirs

test_main_name=$(ROOT_DIR)/test/bin/000-test-main

all: all_execs

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - $(build_dirs), the list of all directories that hold executables
#  - $(build_objs), the list of all object files corresponding to core sources
#  - $(module_dirs), the list of all directories that hold module object files
#  - $(module_objs), the list of all object files corresponding to modules
#  - All dependencies and flags assigned according to the modules
include _configuration.mk

all_execs: $(filter-out $(test_main_name), $(executable_name))

# Remove all intermediate files
clean:
	@-find src test .csconfig $(module_dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-$(RM) $(test_main_name)

# Remove all configuration files
configclean: clean
	@-$(RM) -r $(module_dirs) _configuration.mk

# Make directories that don't exist
# exclude "test" to not conflict with the phony target
$(filter-out test, $(sort $(build_dirs) $(module_dirs))): | $(dir $@)
	-mkdir $@

# All .o files should be made like .cc files
$(build_objs) $(module_objs):
	$(COMPILE.cc) $(OUTPUT_OPTION) $<

# Add address sanitizers for tests
#$(test_main_name): CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
$(test_main_name): CXXFLAGS += -g3 -Og -Wconversion
$(test_main_name): LDLIBS   += -lCatch2Main -lCatch2

# Link test executable
$(test_main_name):
	$(LINK.cc) $(LDFLAGS) -o $@ $(filter-out %/main.o, $^) $(LOADLIBES) $(LDLIBS)

# Link main executables
$(filter-out $(test_main_name), $(executable_name)):
	$(LINK.cc) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

# Tests: build and run
test: $(test_main_name)
	$(test_main_name)

-include $(foreach dir,$(wildcard .csconfig/*/) $(wildcard .csconfig/test/*/),$(wildcard $(dir)/obj/*.d))

