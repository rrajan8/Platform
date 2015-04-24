#include <fstream>

#include <chdl/chdl.h>
#include <chdl/memreq.h>
#include <chdl/loader.h>
#include "config.h"
#include "cache.cpp"

using namespace chdl;
using namespace std;

int main(int argc, char* argv[])
{
  bvec<2> id = Lit<2>(0);
#ifdef IQ_CACHE
  string name = "iq_l1_cache.nand";
  in_mem_port<IQ_B, IQ_N, IQ_A, IQ_I> front;
  mem_req<IQ_B, IQ_N, IQ_A, IQ_I> req_in(_(front, "req"));
  mem_resp<IQ_B, IQ_N, IQ_I> resp_out(_(front, "resp"));
#elif defined(L1_CACHE) 
	string name = "h2_l1_cache.nand";
  in_mem_port<H_B, H_N, H_A, H_I> front;
  mem_req<H_B, H_N, H_A, H_I> req_in(_(front, "req"));
  mem_resp<H_B, H_N, H_I> resp_out(_(front, "resp"));
#else
  string name = "l2_cache.nand";
  in_port_t front;
  req_flit req_in(_(front, "req"));
  resp_flit resp_out(_(front, "resp"));
#endif
  
#ifdef IQ_CACHE
  mem_port<IQ_B, 32, N_A, IQ_I> out_port; //using the same number of ID bits as the Iqyax mem_req 
  out_port_t back;
  mem_req<IQ_B, 32, N_A, IQ_I> req_out(_(out_port, "req"));
  mem_resp<IQ_B, 32, IQ_I> resp_in(_(out_port, "resp"));
  mem_req<N_B, N_N, N_A, N_I> cache_req_out(_(back, "req"));
  mem_resp<N_B, N_N, N_I> cache_resp_in(_(back, "resp"));

  //shim layer to support 32 bytes
  _(cache_req_out, "valid") = _(req_out, "valid");
  _(req_out, "ready") = _(cache_req_out, "ready");
  _(_(cache_req_out, "contents"), "mask") = bvec<H_N>(_(_(req_out,"contents"),"mask")[0]);
  Flatten(_(_(cache_req_out, "contents"), "data")) = Flatten(_(_(req_out, "contents"), "data"));
  _(_(cache_req_out, "contents"), "addr") = _(_(req_out,"contents"),"addr");  
  _(_(cache_req_out, "contents"), "id") = Cat(id, _(_(req_out, "contents"), "id"));
  _(_(cache_req_out, "contents"), "wr") = _(_(req_out, "contents"), "wr");

  _(resp_in, "valid") = _(cache_resp_in, "valid");
  _(cache_resp_in, "ready") = _(resp_in, "ready");
  Flatten(_(_(resp_in, "contents"),"data")) = Flatten(_(_(cache_resp_in, "contents"),"data"));
  _(_(resp_in, "contents"),"id") = _(_(cache_resp_in, "contents"),"id")[range<0, N_I-3>()];
  _(_(resp_in, "contents"),"wr") = _(_(cache_resp_in, "contents"),"wr");

#else
  mem_port<N_B, N_N, N_A, IQ_I> sback;
  mem_req<N_B, N_N, N_A, IQ_I> req_out(_(sback, "req"));
  mem_resp<N_B, N_N, IQ_I> resp_in(_(sback, "resp"));

  out_port_t back;
  req_flit freq_out(_(back, "req"));
  resp_flit fresp_in(_(back, "resp"));

  _(freq_out, "valid") = _(req_out, "valid");
  _(req_out, "ready") = _(freq_out, "ready");
  _(_(freq_out, "contents"), "mask") = _(_(req_out,"contents"),"mask");
  _(_(freq_out, "contents"), "data") = _(_(req_out, "contents"), "data");
  _(_(freq_out, "contents"), "addr") = _(_(req_out,"contents"),"addr");  
  _(_(freq_out, "contents"), "wr") = _(_(req_out, "contents"), "wr");
  _(_(freq_out, "contents"), "id") = Cat(id, _(_(req_out, "contents"), "id"));

  _(resp_in, "valid") = _(fresp_in, "valid");
  _(fresp_in, "ready") = _(resp_in, "ready");
  _(_(resp_in, "contents"), "data") = _(_(fresp_in, "contents"), "data");  
  _(_(resp_in, "contents"), "id") = _(_(fresp_in, "contents"), "id")[range<0,N_I-3>()];
  _(_(resp_in, "contents"), "wr") =  _(_(fresp_in, "contents"), "wr");

#endif

  #ifdef IQ_CACHE
    NBCache<SS,WA,CRQ,IQ_B,IQ_N,IQ_A,IQ_I,32,N_A,IQ_I>(resp_out,
               req_in, resp_in, req_out);
	#elif defined(L1_CACHE)
    NBCache<SS,WA,CRQ,H_B,H_N,H_A,H_I,N_N,N_A,IQ_I>(resp_out,
							 req_in, resp_in, req_out);
  #else
    NBCache<SS,WA,CRQ,N_B,N_N,N_A,N_I,N_N,N_A,IQ_I>(resp_out,
               req_in, resp_in, req_out);
  #endif
	

  EXPOSE(front); EXPOSE(back);

	
  TAP(front); TAP(back);

	optimize();
	ofstream netlist(name);
	print_netlist(netlist);

	return 0;


}

// Adapt request
/*        const unsigned WIDE_B(32), WIDE_N(tbNout/(WIDE_B/B));
        mem_req<WIDE_B, WIDE_N, A, I> out_req_wide;
        _(out_req_wide, "valid") = _(out_req, "valid");
        _(out_req, "ready") = _(out_req_wide, "ready");
        _(_(out_req_wide, "contents"), "mask")
          = bvec<WIDE_N>(_(_(out_req, "contents"), "mask")[0]);
        _(_(out_req_wide, "contents"), "addr") =
           _(_(out_req, "contents"), "addr");
        Flatten(_(_(out_req_wide, "contents"), "data"))
          = Flatten(_(_(out_req, "contents"), "data"));
        _(_(out_req_wide, "contents"), "id")
          = _(_(out_req, "contents"), "id");
        
        node req_width_adaptor_error(
          _(out_req_wide, "ready") &&
          _(out_req_wide, "valid") &&
          !(_(_(out_req, "contents"), "mask") == ~Lit<tbNout>(0) ||
            _(_(out_req, "contents"), "mask") == Lit<tbNout>(0))
        );
        TAP(out_req_wide);
        TAP(req_width_adaptor_error);
        // Adapt response
        mem_resp<B, tbNout, I> out_resp_wide;
        _(out_resp, "valid") = _(out_resp_wide, "valid");
        _(out_resp_wide, "ready") = _(out_resp, "ready");
        Flatten(_(_(out_resp, "contents"), "data"))
          = Flatten(_(_(out_resp_wide, "contents"), "data"));
        _(_(out_resp, "contents"), "id")
          = _(_(out_resp_wide, "contents"), "id");
        
        TAP(out_resp_wide);
        */