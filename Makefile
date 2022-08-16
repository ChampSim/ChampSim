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
	find src test $(objdir) \( -name '*.o' -o -name '*.d' \) -delete
	$(RM) test/000-test-main

configclean: clean
	$(RM) -r $(objdir) _configuration.mk

$(sort $(required_dirs)): | $(dir $@)
	-mkdir $@

$(cppsrc:.cc=.o): CPPFLAGS += -MMD -MP

$(module_objs):
	$(COMPILE.cc) $(OUTPUT_OPTION) $<

$(executable_name): $(cppsrc:.cc=.o) | $(bindir)
	$(LINK.cc) $(OUTPUT_OPTION) $^

test/000-test-main: CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
test/000-test-main: $(filter-out src/main.o, $(cppsrc:.cc=.o)) $(patsubst %.cc,%.o,$(wildcard test/*.cc))
	$(LINK.cc) $(OUTPUT_OPTION) $^

test: test/000-test-main
	test/000-test-main

-include $(wildcard src/*.d) $(wildcard test/*.d) $(foreach dir,$(wildcard .csconfig/*/),$(wildcard $(dir)/*.d))

