include configure.mk

executable_name ?= champsim

srcDir = src
objDir = obj
binDir = bin

# Set defaults if not specified in configure.mk
CC ?= gcc
CXX ?= g++
CFLAGS ?= -Wall -O3
CXXFLAGS ?= -Wall -O3

# Add some flags which must always be used
CFLAGS += -std=gnu99
CXXFLAGS += -std=c++11
CPPFLAGS += -Iinc -MMD -MP
LDFLAGS +=
LDLIBS +=

core_sources = $(srcDir)/main.cc $(srcDir)/block.cc $(srcDir)/cache.cc $(srcDir)/dram_controller.cc $(srcDir)/ooo_cpu.cc $(srcDir)/uncore.cc $(srcDir)/base_replacement.cc
user_objects = $(objDir)/l1prefetcher.o $(objDir)/l2prefetcher.o $(objDir)/llprefetcher.o $(objDir)/llreplacement.o $(objDir)/branch_predictor.o
core_objects := $(patsubst $(srcDir)/%.cc,$(objDir)/%.o,$(core_sources))

.phony: all clean distclean

all: $(binDir)/$(executable_name)

$(binDir)/$(executable_name): $(core_objects) $(user_objects) configure.mk
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $(core_objects) $(user_objects) $(LDLIBS)

$(objDir)/%.o: $(srcDir)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -O3 $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(objDir)/%.o: $(srcDir)/%.cc
	@mkdir -p $(dir $@)
	$(CXX) -c -O3 $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

$(objDir)/l1prefetcher.o: $(L1PREFETCHER)
$(objDir)/l2prefetcher.o: $(L2PREFETCHER)
$(objDir)/llprefetcher.o: $(LLPREFETCHER)
$(objDir)/llreplacement.o: $(LLREPLACEMENT)
$(objDir)/branch_predictor.o: $(BRANCH_PREDICTOR)
$(user_objects):
	@mkdir -p $(dir $@)
	$(CXX) -c -O3 $(CPPFLAGS) $(CXXFLAGS) -o $@ -x c++ $^

clean:
	$(RM) -r $(objDir)

distclean: clean
	$(RM) -r $(binDir)/$(app)
	$(RM) configure.mk

-include $(wildcard $(objDir)/*.d)

