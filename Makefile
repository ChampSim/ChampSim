CPPFLAGS += -Iinc
CFLAGS += -Wall -O3
CXXFLAGS += -Wall -O3
.phony: all exec clean test

cppsrc = $(wildcard src/*.cc)
csrc = $(wildcard src/*.c)

all: exec

include _configuration.mk

$(patsubst %.cc,%.o,$(cppsrc)) $(patsubst %.c,%.o,$(csrc)) : CPPFLAGS += -MMD -MP

$(executable_name): $(patsubst %.cc,%.o,$(cppsrc)) $(patsubst %.cc,%.o,$(csrc))
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test:
	$(MAKE) -C test

