CPPFLAGS += -Iinc
CXXFLAGS += --std=c++17 -Wall -O3

.phony: all clean configclean test makedirs

cppsrc = $(wildcard src/*.cc)

# Generated configuration makefile contains:
#  - $(module_dirs)
#  - Each module's compilation flags
#  - Each module's source files, appended to $(cppsrc) and $(csrc)
#  - $(executable_name)
#  - $(generated_files)
include _configuration.mk

all: makedirs $(executable_name)

clean:
	find src test .csconfig \( -name '*.o' -o -name '*.d' \) -delete
	$(RM) test/000-test-main

configclean: clean
	$(RM) -r .csconfig _configuration.mk

makedirs:
	mkdir -p $(module_dirs)

$(executable_name): $(wildcard src/*.cc)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(patsubst %,-I%,$(module_dirs)) -MMD -MP $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

exec_obj = $(patsubst %.cc,%.o,$(cppsrc))

test_obj = $(filter-out src/core_inst.o src/main.o, $(exec_obj)) $(patsubst %.cc,%.o,$(wildcard test/*.cc))
test: CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
test: $(test_obj)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o test/000-test-main $^ $(LDLIBS) && test/000-test-main

-include $(wildcard src/*.d) $(wildcard test/*.d) $(foreach dir,$(wildcard .csconfig/*/),$(wildcard $(dir)/*.d))

