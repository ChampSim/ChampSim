CPPFLAGS += -Iinc
CFLAGS += --std=c++17 -Wall -O3
CXXFLAGS += --std=c++17 -Wall -O3
.phony: all exec clean test

cppsrc = $(wildcard src/*.cc)
csrc = $(wildcard src/*.c)

all: exec

include _configuration.mk

exec_obj = $(patsubst %.cc,%.o,$(cppsrc)) $(patsubst %.c,%.o,$(csrc))
$(exec_obj) : CPPFLAGS += -MMD -MP

$(executable_name): $(exec_obj)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_obj = $(filter-out src/main.o, $(exec_obj)) $(patsubst %.cc,%.o,$(wildcard test/*.cc))
test: $(test_obj)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o test/000-test-main $^ $(LDLIBS) && test/000-test-main

