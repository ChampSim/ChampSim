CPPFLAGS += -Iinc
CFLAGS += --std=c++17 -Wall -Wextra -Wno-unused-parameter -O3
CXXFLAGS += --std=c++17 -Wall -Wextra -Wno-unused-parameter -O3
CPPFLAGS += -MMD -MP

.phony: all clean configclean test

cppsrc = $(wildcard src/*.cc)
csrc = $(wildcard src/*.c)

# Generated configuration makefile contains:
#  - $(module_dirs)
#  - Each module's compilation flags
#  - Each module's source files, appended to $(cppsrc) and $(csrc)
#  - $(executable_name), if specified
#  - $(generated_files)
include _configuration.mk

executable_name ?= bin/champsim

all: $(executable_name)

clean:
	find src test $(module_dirs) \( -name '*.o' -o -name '*.d' \) -delete
	$(RM) test/000-test-main

configclean: clean
	$(RM) $(generated_files) _configuration.mk

exec_obj = $(patsubst %.cc,%.o,$(cppsrc)) $(patsubst %.c,%.o,$(csrc))

$(executable_name): $(exec_obj)
	mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_obj = $(filter-out src/core_inst.o src/main.o, $(exec_obj)) $(patsubst %.cc,%.o,$(wildcard test/*.cc))
test: CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
test: $(test_obj)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o test/000-test-main $^ $(LDLIBS) && test/000-test-main

-include $(wildcard src/*.d) $(wildcard test/*.d) $(foreach dir,$(module_dirs),$(wildcard $(dir)/*.d))

