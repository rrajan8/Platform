CXXFLAGS ?= -std=c++11
LDLIBS ?= -lchdl -lchdl-module

all: core top_level interconnect mem cache_syn

core: core.cpp

top_level: top_level.cpp

cache_syn:	cache_syn.cpp cache.cpp

interconnect: interconnect.cpp

mem: mem.cpp

harmonica_fpga: harmonica_fpga.cpp

clean:
	rm -f h2_cache iq_cache core top_level *cache_syn interconnect *interconnect.nand mem node*.nand *_cache.nand *.vcd *~
