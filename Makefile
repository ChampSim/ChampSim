CPPFLAGS += -Iinc
CXXFLAGS += --std=c++17 -Wall -O3

.phony: all all_execs clean configclean test makedirs

all: all_execs

cppsrc = $(wildcard src/*.cc)

# Generated configuration makefile contains:
#  - Each module's compilation flags
#  - Each module's source files, appended to $(cppsrc) and $(csrc)
#  - $(executable_name)
include _configuration.mk

all_execs: $(executable_name)

clean:
	find src test .csconfig \( -name '*.o' -o -name '*.d' \) -delete
	$(RM) test/000-test-main

configclean: clean
	$(RM) -r .csconfig _configuration.mk

$(sort $(required_dirs)): | $(dir $@)
	-mkdir $@

$(cppsrc:.cc=.o): CPPFLAGS += -MMD -MP

$(executable_name): $(cppsrc:.cc=.o) | $(dirname $@)
	$(LINK.cc) $(OUTPUT_OPTION) $^

exec_obj = $(patsubst %.cc,%.o,$(cppsrc))

test_obj = $(filter-out src/core_inst.o src/main.o, $(exec_obj)) $(patsubst %.cc,%.o,$(wildcard test/*.cc))
test: CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
test: $(test_obj)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o test/000-test-main $^ $(LDLIBS) && test/000-test-main

-include $(wildcard src/*.d) $(wildcard test/*.d) $(foreach dir,$(wildcard .csconfig/*/),$(wildcard $(dir)/*.d))

