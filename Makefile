CPPFLAGS += -Iinc
CXXFLAGS += --std=c++17 -Wall -O3
CPPFLAGS += -MMD -MP

.phony: all clean configclean test

cppsrc = $(wildcard src/*.cc)

# Generated configuration makefile contains:
#  - $(module_dirs)
#  - Each module's compilation flags
#  - Each module's source files, appended to $(cppsrc) and $(csrc)
#  - $(executable_name)
#  - $(generated_files)
include _configuration.mk

all: $(executable_name)

clean:
	find src test $(module_dirs) \( -name '*.o' -o -name '*.d' \) -delete
	$(RM) test/000-test-main

configclean: clean
	$(RM) $(generated_files) _configuration.mk

$(executable_name): $(wildcard src/*.cc)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

exec_obj = $(patsubst %.cc,%.o,$(cppsrc))

test_obj = $(filter-out src/core_inst.o src/main.o, $(exec_obj)) $(patsubst %.cc,%.o,$(wildcard test/*.cc))
test: CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
test: $(test_obj)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o test/000-test-main $^ $(LDLIBS) && test/000-test-main

-include $(wildcard src/*.d) $(wildcard test/*.d) $(foreach dir,$(module_dirs),$(wildcard $(dir)/*.d))

