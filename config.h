#ifndef CONFIG_H
#define CONFIG_H

#include <chdl/memreq.h>
#include <chdl/net.h>
#include <chdl/chdl.h>

#define IQ_B 8
#define IQ_N 4
#define IQ_A 30
#define IQ_I 7

#define H_B 32
#define H_N 8
#define H_A 27
#define H_I 3

#define N_B 32
#define N_N 8
#define N_A 27
#define N_I 9

#define SS 4 // 2^SS sets
#define WA 0 //2^WA associativity
#define CRQ 2 // Outstanding entries 

#define NUM_NODES 4
#define SZ 15


// typedef chdl::in_mem_req<H_B, H_N, H_A, H_I> in_req_flit;
// typedef chdl::out_mem_req<H_B, H_N, H_A, H_I> out_req_flit;
// typedef chdl::in_mem_resp<H_B, H_N,H_I> in_resp_flit;
// typedef chdl::out_mem_resp<H_B, H_N,H_I> out_resp_flit;
typedef chdl::mem_req<N_B, N_N, N_A, N_I> req_flit;
typedef chdl::mem_resp<N_B, N_N, N_I> resp_flit;

typedef chdl::mem_port<N_B, N_N, N_A, N_I> port_t;
typedef chdl::in_mem_port<N_B, N_N, N_A, N_I> in_port_t;
typedef chdl::out_mem_port<N_B, N_N, N_A, N_I> out_port_t;

namespace chdl
{
	 template <typename T> using ag_id =
    	ag<STP("cid"), in<T> >;

	  //  template <typename T>
	  //   void myResRouter(vec<2, flit<T> > &out, int id, flit<T> &in)
	  // {

	  //   bvec<2> valid, ready;

	  //   for (unsigned i = 0; i < 2; ++i) {
	  //     _(out[i], "contents") = _(in, "contents");
	  //     _(out[i], "valid") = valid[i];
	  //     ready[i] = _(out[i], "ready");
	  //   }
	    
	  //   myRouteIdBits(valid, in, _(in, "valid"), id);

	  //   // Input is ready if all of the selected outputs are also ready
	  //   _(in, "ready") = AndN((~Lit<2>(0) & ~valid) | ready);
	  // }

	  

	  template<unsigned B, unsigned N, unsigned A, unsigned I> 
	  void encapsulate( mem_req<B, N, A, I> &req_out, mem_req<B, N, A, I> &req_in, int id_num)
	  {
	  		_(req_out, "valid") = _(req_in, "valid");
	  		_(req_in, "ready") = _(req_out, "ready");
	  		

	  		_(_(req_out, "contents"), "wr") = _(_(req_in, "contents"), "wr" );
	  		
	  		_(_(req_out, "contents"), "data") = _(_(req_in, "contents"), "data");

	  		if(id_num > -1)
	  			_(_(req_out, "contents"), "id") = Cat(Lit<2>(id_num), _(_(req_in, "contents"), "id")[range<0,I-3>()]);
	  		else
	  			_(_(req_out, "contents"),"id") = _(_(req_in, "contents"),"id");

	  			_(_(req_out, "contents"), "mask") = _(_(req_in, "contents"), "mask");
	  			_(_(req_out, "contents"), "addr") = _(_(req_in, "contents"), "addr");
	  			_(_(req_out, "contents"), "llsc") = _(_(req_in, "contents"), "llsc");
	  		
	  }

	  template<unsigned B, unsigned N, unsigned I> 
	  void rencapsulate( mem_resp<B, N, I> &req_out, mem_resp<B, N, I> &req_in, int id_num)
	  {
	  		_(req_out, "valid") = _(req_in, "valid");
	  		_(req_in, "ready") = _(req_out, "ready");
	  		

	  		_(_(req_out, "contents"), "wr") = _(_(req_in, "contents"), "wr" );
	  		
	  		_(_(req_out, "contents"), "data") = _(_(req_in, "contents"), "data");
	  		_(_(req_out, "contents"), "llsc") = _(_(req_in, "contents"), "llsc");
	  		_(_(req_out, "contents"), "llsc_suc") = _(_(req_in, "contents"), "llsc_suc");

	  		if(id_num > -1)
	  			_(_(req_out, "contents"), "id") = Cat(Lit<2>(id_num), _(_(req_in, "contents"), "id")[range<0,I-3>()]);
	  		else
	  			_(_(req_out, "contents"),"id") = _(_(req_in, "contents"),"id");
	  		
	  }

	  template<unsigned B1, unsigned N1, unsigned A1, unsigned I1>
	  void connect_port(mem_port<B1, N1, A1, I1>& port_out, mem_port<B1, N1, A1, I1>& port_in)
	  {
	  	mem_req<B1, N1, A1, I1> out_req(_(port_out,"req")), in_req(_(port_in,"req"));
	  	mem_resp<B1, N1, I1> out_resp(_(port_out,"resp")), in_resp(_(port_in,"resp"));
	  	
	  	_(out_req, "valid") = _(in_req, "valid");
	  	_(in_req, "ready") = _(out_req, "ready");
	  	_(out_req, "contents") = _(in_req, "contents");

	  	_(in_resp, "valid" ) = _(out_resp, "valid");
	  	_(in_resp, "contents") = _(out_resp, "contents");
	  	_(out_resp, "ready") = _(in_resp, "ready");

	  }

	  template<unsigned B1, unsigned N1, unsigned A1, unsigned I1>
	  void connect_port(mem_port<B1, N1, A1, I1>& port_out, mem_port<B1, N1, A1, I1>& port_in, int id_num)
	  {
	  	mem_req<B1, N1, A1, I1> out_req(_(port_out,"req")), in_req(_(port_in,"req"));
	  	mem_resp<B1, N1, I1> out_resp(_(port_out,"resp")), in_resp(_(port_in,"resp"));
	  	
	  	encapsulate(out_req, in_req, id_num);
	  	rencapsulate(in_resp, out_resp, 0);
	  }


}

#endif /*CONFIG_H*/