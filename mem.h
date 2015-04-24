#ifndef MEM_H
#define MEM_H

#include "config.h"

namespace chdl{
	void myRouteIdBits(bvec<2> &valid, const resp_flit &r, node in_valid, bvec<2>& id)
	{

		bvec<CLOG2(2)> sel = Mux(_(_(r, "contents"),"id")[range<N_I-2,N_I-1>()] == id,Lit(1), Lit(0));
		valid = Decoder(sel, in_valid);
	}

	void myRouteAddrBits(bvec<2> &valid, const req_flit &r, node in_valid, bvec<2>& id)
	{

		bvec<CLOG2(2)> sel = Mux(_(_(r, "contents"),"addr")[range<SZ-2,SZ-1>()] == id,Lit(1), Lit(0));
		valid = Decoder(sel, in_valid);

	}

	void myRouteBits(bvec<4> &valid, const resp_flit &r, node in_valid)
	{

		    bvec<CLOG2(4)> sel = _(_(r, "contents"),"id")[range<5,6>()];
		    valid = Decoder(sel, in_valid);

	}

	template <typename T, typename F>
	void RRouter(vec<2, flit<T> > &out, bvec<2>& id, flit<T> &in, const F &func )
	{

		bvec<2> valid, ready;

		for (unsigned i = 0; i < 2; ++i) {
	 		_(out[i], "contents") = _(in, "contents");
	  		_(out[i], "valid") = valid[i];
	  		ready[i] = _(out[i], "ready");
		}

		func(valid, in, _(in, "valid"), id);

		// Input is ready if all of the selected outputs are also ready
		_(in, "ready") = AndN((~Lit<2>(0) & ~valid) | ready);
	}

	template <typename T>
	  void MemRouter(vec<4, flit<T> > &out, flit<T> &in)
	  {

	    bvec<4> valid, ready;

	    for (unsigned i = 0; i < 4; ++i) {
	      _(out[i], "contents") = _(in, "contents");
	      _(out[i], "valid") = valid[i];
	      ready[i] = _(out[i], "ready");
	    }
	    
	    myRouteBits(valid, in, _(in, "valid"));

	    // Input is ready if all of the selected outputs are also ready
	    _(in, "ready") = AndN((~Lit<4>(0) & ~valid) | ready);
	  }
}

#endif /*MEM_H*/