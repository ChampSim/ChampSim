CPPFLAGS += -Iinc
CXXFLAGS += --std=c++17 -Wall -Wextra -Wno-unused-parameter -O3

.phony: all all_execs clean configclean test makedirs

test_main_name=test/bin/000-test-main

all: all_execs

cppsrc = $(wildcard src/*.cc)
testsrc = $(wildcard test/*.cc)

#$(cppsrc:.cc=.o) $(testsrc:.cc=.o): CPPFLAGS += -MMD -MP

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - $(build_dirs), the list of all directories that hold executables
#  - $(module_dirs), the list of all directories that hold module object files
#  - $(module_objs), the list of all object files corresponding to modules
#  - All dependencies and flags assigned according to the modules
include _configuration.mk

all_execs: $(filter-out $(test_main_name), $(executable_name))

# Remove all intermediate files
clean:
	@-find src test $(module_dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
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

# Link test executable
$(test_main_name): $(testsrc:.cc=.o)
	$(LINK.cc) $(OUTPUT_OPTION) $(filter-out %/main.o, $^)

# Link main executables
$(filter-out $(test_main_name), $(executable_name)):
	$(LINK.cc) $(OUTPUT_OPTION) $^

# Tests: build and run
test: $(test_main_name)
	$(test_main_name)

-include $(wildcard src/*.d) $(wildcard test/*.d) $(foreach dir,$(wildcard .csconfig/*/),$(wildcard $(dir)/*.d))

