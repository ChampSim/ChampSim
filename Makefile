CPPFLAGS += -Iinc
CFLAGS += --std=c++17 -Wall -O3
CXXFLAGS += --std=c++17 -Wall -O3

.phony: all clean configclean test

cppsrc = $(wildcard src/*.cc)
csrc = $(wildcard src/*.c)

# Generated configuration makefile contains:
#  - $(module_dirs)
#  - Each module's compilation flags
#  - Each module's source files, appended to $(cppsrc) and $(csrc)
#  - $(executable_name), if specified
include _configuration.mk

executable_name ?= bin/champsim

all: $(executable_name)

clean:
	find src test $(module_dirs) -name \*.o -delete
	find src test $(module_dirs) -name \*.d -delete
	$(RM) test/000-test-main

configclean: clean
	$(RM) inc/champsim_constants.h src/core_inst.cc inc/cache_modules.inc inc/ooo_cpu_modules.inc _configuration.mk

exec_obj = $(patsubst %.cc,%.o,$(cppsrc)) $(patsubst %.c,%.o,$(csrc))
$(exec_obj) : CPPFLAGS += -MMD -MP

$(executable_name): $(exec_obj)
	mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_obj = $(filter-out src/main.o, $(exec_obj)) $(patsubst %.cc,%.o,$(wildcard test/*.cc))
test: $(test_obj)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o test/000-test-main $^ $(LDLIBS) && test/000-test-main

-include $(wildcard src/*.d) $(foreach dir,$(module_dirs),$(wildcard $(dir)/*.d))

