 #include <fstream>
 #include "mem.h"
 #include <chdl/loader.h>

using namespace chdl;
using namespace std;




int main(int argc, char* argv[])
{
	in_port_t up, left;
	out_port_t down, right;
	
	bvec<2> mask(_(_(_(up, "req"),"contents"),"id")[range<N_I-2, N_I-1>()]);
	
	//EXPOSE(in<bvec<2>>(mask));

	vec<2, req_flit> arbit_reqs;
	vec<2, req_flit> route_reqs;
	req_flit arbit_req, route_req, up_req(_(up, "req"));

	Buffer<1>(arbit_reqs[0], up_req);
	arbit_reqs[1] = _(left, "req");
	_(down, "req") = route_reqs[0];
	_(right, "req") = route_reqs[1];

	Arbiter(arbit_req, ArbRR<2>, arbit_reqs);
	Buffer<1>(route_req, arbit_req);
	RRouter(route_reqs, mask, route_req, myRouteAddrBits);

	vec<2, resp_flit> arbit_resps, route_resps;
	resp_flit route_resp, arbit_resp;
	arbit_resps[0] = _(down, "resp");
	arbit_resps[1] = _(right, "resp");
	_(up, "resp") = route_resps[0];
	_(left, "resp") = route_resps[1];

	Arbiter(arbit_resp, ArbRR<2>, arbit_resps);
	Buffer<1>(route_resp, arbit_resp);
	RRouter(route_resps, mask, route_resp, myRouteIdBits);

	TAP(arbit_reqs); TAP(arbit_req); TAP(route_reqs); TAP(route_req); TAP(mask);

	EXPOSE(up); EXPOSE(left); EXPOSE(right); EXPOSE(down);



	cycdet();
	optimize();
	ofstream netlist("interconnect.nand");
	print_netlist(netlist);

	return 0;




}