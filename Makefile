
CPPFLAGS += -Iinc
CFLAGS += -Wall -O3
CXXFLAGS += -Wall -O3
.phony: all exec clean

all: exec

include _configuration.mk

