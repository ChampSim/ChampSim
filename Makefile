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

lastwords = $(wordlist 2,$(words $(1)),$(1))
obj_of = $(addsuffix .o, $(basename $(addprefix $(objDir)/,$(notdir $(1)))))

core_sources = $(srcDir)/main.cc $(srcDir)/block.cc $(srcDir)/cache.cc $(srcDir)/dram_controller.cc $(srcDir)/ooo_cpu.cc $(srcDir)/uncore.cc $(srcDir)/base_replacement.cc
user_sources = $(call lastwords,$(L1PREFETCHER)) $(call lastwords,$(L2PREFETCHER)) $(call lastwords,$(LLPREFETCHER)) $(call lastwords,$(LLREPLACEMENT)) $(call lastwords,$(BRANCH_PREDICTOR))
module_objects = $(objDir)/l1prefetcher.o $(objDir)/l2prefetcher.o $(objDir)/llprefetcher.o $(objDir)/llreplacement.o $(objDir)/branch_predictor.o
core_objects := $(call obj_of,$(core_sources))
user_objects := $(call obj_of,$(user_sources))

.phony: all clean distclean

all: $(binDir)/$(executable_name)

$(binDir)/$(executable_name): configure.mk $(core_objects) $(module_objects) $(user_objects)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $(filter-out $<,$^) $(LDLIBS)

$(objDir)/%.o: */%.c configure.mk
	@mkdir -p $(dir $@)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(objDir)/%.o: */%.cc configure.mk
	@mkdir -p $(dir $@)
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

$(objDir)/l1prefetcher.o: $(firstword $(L1PREFETCHER)) configure.mk
$(objDir)/l2prefetcher.o: $(firstword $(L2PREFETCHER)) configure.mk
$(objDir)/llprefetcher.o: $(firstword $(LLPREFETCHER)) configure.mk
$(objDir)/llreplacement.o: $(firstword $(LLREPLACEMENT)) configure.mk
$(objDir)/branch_predictor.o: $(firstword $(BRANCH_PREDICTOR)) configure.mk
$(module_objects):
	@mkdir -p $(dir $@)
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ -x c++ $<

clean:
	$(RM) -r $(objDir)

distclean: clean
	$(RM) -r $(binDir)/$(app)
	$(RM) configure.mk

-include $(wildcard $(objDir)/*.d)

