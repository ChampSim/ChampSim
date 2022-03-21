
CPPFLAGS += -Iinc -MMD -MP
CFLAGS += -Wall -O3
CXXFLAGS += -Wall -O3
.phony: all exec clean

cppsrc = $(wildcard src/*.cc)
csrc = $(wildcard src/*.c)

all: exec

include _configuration.mk

$(executable_name): $(patsubst %.cc,%.o,$(cppsrc)) $(patsubst %.cc,%.o,$(csrc))
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

