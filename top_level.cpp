#include <fstream>

#include <chdl/loader.h>
#include "config.h"

using namespace chdl;
using namespace std;

int main(int argc, char* argv[])
{
	vec<4, port_t> left;
	vec<4, port_t> right;
	vec<4, port_t> mem_out;

	vec<4, port_t> mem_net_ports;
	port_t dram_port;

	for(int ii = 0; ii < NUM_NODES; ii++)
	{
		Load("node"+to_string(ii)+".nand")("left_port", in_port_t(left[ii]))
										  ("right_port", out_port_t(right[ii]))
										  ("mem_port", out_port_t(mem_out[ii])).tap("core_"+to_string(ii)+"_");


	}

		Load("mem_interconnect.nand")("mem_in_0",in_port_t(mem_net_ports[0]))
									("mem_in_1",in_port_t(mem_net_ports[1]))
									("mem_in_2",in_port_t(mem_net_ports[2]))
									("mem_in_3",in_port_t(mem_net_ports[3]))
									("out_mem", out_port_t(dram_port)).tap("mem_net_");

	for(int ii = 0; ii < NUM_NODES; ii++)
	{
		connect_port(left[(ii+1)%NUM_NODES], right[ii]);
		connect_port(mem_net_ports[ii], mem_out[ii]);
	}

	Scratchpad<SZ>(dram_port);

	TAP(left); TAP(right); TAP(mem_out); TAP(mem_net_ports); TAP(dram_port);

	cycdet();
	optimize();
	ofstream vcd("top_level.vcd");
	run(vcd, 100);



}