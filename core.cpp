#include <fstream>
#include <chdl/loader.h>
#include "config.h"

//#define SIMULATE

using namespace chdl;
using namespace std;

void iqyax_core(mem_port<N_B, N_N, N_A, N_I>& iq_port);
void h2_core(mem_port<N_B, N_N, N_A, N_I>& h2_port);
void l2_cache(port_t& l2_front, port_t& l2_back);

int main(int argc, char* argv[])
{

	int id;
	string core;

	id = atoi(argv[1]); //core id
	core = argv[2]; //core type to use


	ag_id<bvec<2>> local_id;
	_(local_id, "cid") = Lit<2>(id);

	string result = "node" + to_string(id) + ".nand";

	mem_port<N_B, N_N, N_A, N_I> l1_port, net_port_in, net_port_out,
									l2_port, left, right, mem_port, l2_port_out;

	if(core == "iq")
		iqyax_core(l1_port);
	else
		h2_core(l1_port);

	l2_cache(l2_port, l2_port_out);
	Load("interconnect.nand")("up", in_port_t(net_port_in)) //the interconnect network is distributed across the node modules
							("down", out_port_t(net_port_out))
							("left", in_port_t(left))
							("right", out_port_t(right)).tap("interconnect_");
	
	//cout << "ttt" << endl;
	connect_port(net_port_in, l1_port, id);
	connect_port(l2_port, net_port_out);
	connect_port(mem_port, l2_port_out, id);

	
	
	TAP(l2_port); TAP(l1_port); TAP(left); TAP(right); TAP(mem_port);
	TAP(net_port_out); TAP(net_port_in); TAP(l2_port_out);

//cout << "ttt" << endl;
#ifdef SIMULATE
	Scratchpad<SZ>(mem_port);
#else
	Expose(string("left_port"), in_port_t(left)); 
	Expose(string("right_port"), out_port_t(right)); 
	Expose(string("mem_port"), out_port_t(mem_port));
#endif

	cycdet();
	optimize();

#ifdef SIMULATE
	ofstream vcd("test_core.vcd");
	run(vcd, 10000);
#else
	ofstream netlist(result);
	print_netlist(netlist);
#endif


	return 0;
}

void iqyax_core(mem_port<N_B, N_N, N_A, N_I>& iq_port)
{
	
	mem_req<IQ_B, IQ_N, IQ_A, IQ_I> core_port_req;
	mem_resp<IQ_B, IQ_N, IQ_I> core_port_resp;
	mem_port<IQ_B, IQ_N, IQ_A, IQ_I> cache_front_port;
	mem_req<IQ_B, IQ_N, IQ_A, IQ_I> cache_front_port_req(_(cache_front_port, "req"));
	mem_resp<IQ_B, IQ_N, IQ_I> cache_front_port_resp(_(cache_front_port, "resp"));
	mem_port<N_B, N_N, N_A, N_I> cache_back_port;

	Load("iqyax.nand")("req_d", out_mem_req<IQ_B, IQ_N, IQ_A, IQ_I>(core_port_req))
					   ("resp_d", in_mem_resp<IQ_B, IQ_N, IQ_I>(core_port_resp)).tap("iqyax_");
	Load("iq_l1_cache.nand")("front", in_mem_port<IQ_B, IQ_N, IQ_A, IQ_I>(cache_front_port))
							("back", out_port_t(cache_back_port));

	Buffer<3>(cache_front_port_req, core_port_req);
	Buffer<1> (core_port_resp, cache_front_port_resp);

	connect_port(iq_port, cache_back_port);

	TAP(cache_front_port); TAP(cache_back_port); TAP(iq_port);
	TAP(core_port_req); TAP(core_port_resp);


}

void h2_core(mem_port<N_B, N_N, N_A, N_I>& h2_port)
{
	mem_port<H_B, H_N, H_A, H_I> cache_front_port;
	mem_port<N_B, N_N, N_A, N_I> cache_back_port;
	mem_port<H_B, H_N, H_A, H_I> core_port;
	

	Load("h2.nand")("dmem", out_mem_port<H_B, H_N, H_A, H_I>(core_port));
	//cout << "ttt" << endl;
	Load("h2_l1_cache.nand")("front", in_mem_port<H_B, H_N, H_A, H_I>(cache_front_port))
							("back", out_port_t(cache_back_port));

	//cout << "ttt" << endl;
	connect_port(cache_front_port, core_port);
	connect_port(h2_port , cache_back_port);

	TAP(cache_front_port); TAP(cache_back_port); TAP(h2_port); TAP(core_port);
}

void l2_cache(port_t& l2_front, port_t& l2_back)
{
	Load("l2_cache.nand")("front", in_port_t(l2_front))("back",out_port_t(l2_back) );
}