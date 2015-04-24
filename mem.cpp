#include <fstream>
#include <chdl/loader.h>
#include "mem.h"

using namespace chdl;
using namespace std;

int main(int argc, char* argv[])
{
	bvec<2> dummy_id;
	vec<NUM_NODES, in_port_t> mem_ports;
	vec<NUM_NODES, req_flit> mem_reqs;
	vec<NUM_NODES, resp_flit> mem_resps;
	vec<NUM_NODES, req_flit> temp_reqs;

	out_port_t out_mem;
	req_flit out_mem_req(_(out_mem, "req"));
	resp_flit out_mem_resp(_(out_mem, "resp"));

	for(int ii =0; ii < NUM_NODES; ii++)
	{
		temp_reqs[ii] = _(mem_ports[ii], "req");
		Buffer<1>(mem_reqs[ii], temp_reqs[ii]);

		_(_(mem_ports[ii], "resp"), "valid") = _(mem_resps[ii], "valid");
		_(_(mem_ports[ii], "resp"), "contents") = _(mem_resps[ii], "contents");
		_(mem_resps[ii], "ready") = _(_(mem_ports[ii], "resp"), "ready");

		Expose(string("mem_in_"+to_string(ii)),mem_ports[ii]);
	}

	TAP(mem_reqs); TAP(temp_reqs); TAP(mem_resps); TAP(out_mem); TAP(out_mem_req);
	TAP(out_mem_resp);

	MemRouter(mem_resps, out_mem_resp);
	Arbiter(out_mem_req,ArbRR<4>,mem_reqs);

	EXPOSE(out_mem);

	cycdet();
	optimize();
	ofstream netlist("mem_interconnect.nand");
	print_netlist(netlist);

	return 0; 
}