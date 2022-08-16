CPPFLAGS += -Iinc
CXXFLAGS += --std=c++17 -Wall -O3

.phony: all all_execs clean configclean test makedirs

all: all_execs

cppsrc = $(wildcard src/*.cc)
testsrc = $(wildcard test/*.cc)

$(cppsrc:.cc=.o) $(testsrc:.cc=.o): CPPFLAGS += -MMD -MP

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - $(build_dirs), the list of all directories that hold executables
#  - $(module_dirs), the list of all directories that hold module object files
#  - $(module_objs), the list of all object files corresponding to modules
#  - All dependencies and flags assigned according to the modules
include _configuration.mk

all_execs: $(filter-out test/000-test-main, $(executable_name))

# Remove all intermediate files
clean:
	@-find src test $(module_dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-$(RM) test/000-test-main

# Remove all configuration files
configclean: clean
	@-$(RM) -r $(module_dirs) _configuration.mk

# Make directories that don't exist
# exclude "test" to not conflict with the phony target
$(filter-out test, $(sort $(build_dirs) $(module_dirs))): | $(dir $@)
	-mkdir $@

# All module .o files should be made like .cc files
$(module_objs):
	$(COMPILE.cc) $(OUTPUT_OPTION) $<

# Add main as a dependency for the primary executables
$(filter-out test/000-test-main, $(executable_name)): src/main.o

# Add address sanitizers for tests
#test/000-test-main: CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer

# Test depends on the sources in test/
test/000-test-main: $(testsrc:.cc=.o)

# Tests: build and run
test: test/000-test-main
	test/000-test-main

# Both main and test executables depend on the sources in src/ (except for main)
$(executable_name): $(filter-out src/main.o, $(cppsrc:.cc=.o))
	$(LINK.cc) $(OUTPUT_OPTION) $^

-include $(wildcard src/*.d) $(wildcard test/*.d) $(foreach dir,$(wildcard .csconfig/*/),$(wildcard $(dir)/*.d))

