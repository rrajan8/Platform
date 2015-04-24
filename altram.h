#ifndef CHDL_ALTRAM_H
#define CHDL_ALTRAM_H

#include <chdl/chdl.h>
#include <chdl/dir.h>
#include <chdl/queue.h>
#include <chdl/memreq.h>
#include <chdl/cassign.h>

namespace chdl {
  // A = width of address bus
  // B = width of memory channel
  // N = number of B-bit transfers per request
  // R = number of bits in burst width signal
  template <unsigned B, unsigned N, unsigned A, unsigned R> using avl =
    ag<STP("ready"),  in<node>,
    ag<STP("burstbegin"), out<node>,
    ag<STP("wr_req"), out<node>,
    ag<STP("rd_req"), out<node>,
    ag<STP("addr"),   out<bvec<A> >,
    ag<STP("wdata"),  out<vec<N, bvec<B> > >,
    ag<STP("rdata_valid"), in<node>,
    ag<STP("rdata"),  in<vec<N, bvec<B> > >,
    ag<STP("bmask"),  out<bvec<N> >, // byte mask for writes
    ag<STP("size"),   out<bvec<R> >  // # beats in burst
  > > > > > > > > > >;

  // Attach a CHDL mem_req and mem_resp to an Avalon interface. This generates
  // responses for both read and write operations. Might assert backpressure but
  // provides none on the response side. Will need to be combined with a
  // sufficiently-large buffer if resp is ever not ready.
  template <unsigned B, unsigned N, unsigned A, unsigned I, unsigned R>
    void AvlAdaptor(mem_req<B, N, A, I> &req,
                    mem_resp<B, N, I> &resp,
                    avl<B, N, A, R> &a)
  {
    const unsigned QQ(3);
    node push, pop;
    
    bvec<QQ + 1> qo; // queue occupancy
    _(_(resp, "contents"), "id") =
      Queue<QQ>(_(_(req, "contents"), "id"), push, pop, qo);
    node full(qo == bvec<QQ+1>((1<<QQ)+1));

    push = _(req, "valid") && _(req, "ready") && !_(req, "wr");
    pop = _(a, "rdata_valid");

    _(a, "size") = bvec<R>(1);
    _(a, "burstbegin") = _(req, "ready") && _(req, "valid");
    _(a, "rd_req") = _(a, "burstbegin") && !_(_(req, "contents"), "wr");
    _(a, "wr_req") = _(a, "burstbegin") && _(_(req, "contents"), "wr");
    _(a, "wdata") = _(_(req, "contents"), "data");
    _(a, "addr") = _(_(req, "contents"), "addr");
    _(a, "bmask") = _(_(req, "contents"), "mask");

    _(req, "ready") = !full && _(a, "ready");

    _(resp, "valid") = _(a, "rdata_valid");
    _(_(resp, "contents"), "data") = _(a, "rdata");
    _(_(resp, "contents"), "llsc") = Lit(0);
  }

  template <unsigned B, unsigned N, unsigned A, unsigned I, unsigned R>
    void AvlAdaptor(mem_port<B, N, A, I> &port, avl<B, N, A, R> &a)
  { AvlAdaptor(_(port, "req"), _(port, "resp"), a); }

  // Should probably go in with the latches, regs, and wregs.
  node RsLatch(node r, node s) {
    node state(Wreg(Xor(r, s), s));
    return state || s;
  }

  node Rs(node r, node s) { return Wreg(Xor(r, s), s); }
  
  // 2-beat adaptor
  template <unsigned B, unsigned N, unsigned A, unsigned I, unsigned R>
    void AvlAdaptor(mem_req<B, 2*N, A, I> &req,
                    mem_resp<B, 2*N, I> &resp,
                    avl<B, N, A, R> &a)
  {
    node ready(_(a, "ready")), rvalid(_(a, "rdata_valid"));

    // In the second beat of 2-beat sequence
    node wr_start(ready && _(req, "valid") && _(_(req, "contents"), "wr")),
         rd_start(ready && _(req, "valid") && !_(_(req, "contents"), "wr")),
         wr, rd, wr2, rd2, beat2(wr2 || rd2), full;

    TAP(wr_start); TAP(rd_start); TAP(wr); TAP(rd); TAP(wr2); TAP(rd2);
    TAP(full);
         
    _(req, "ready") = ready && !beat2 && !full;

    wr = RsLatch(wr2 && ready, wr_start);
    rd = RsLatch(rd2 && ready && rvalid, rd_start);

    wr2 = Rs(wr2 && ready, wr && !wr2 && ready);
    rd2 = Rs(rd2 && rvalid, rd && !rd2 && ready && rvalid);

    full = Rs(_(resp, "valid") && _(resp, "ready"), rd2 && rvalid);

    bvec<A> addr(Latch(Reg(rd_start || wr_start), _(_(req, "contents"), "addr")));
    vec<2*N, bvec<B> > d(Latch(wr, _(_(req, "contents"), "data")));
    vec<N, bvec<B> > dh(d[range<N,2*N-1>()]), dl(d[range<0,N-1>()]),
                     q(_(a, "rdata"));

    _(a, "wdata") = Mux(wr2, dl, dh);
    _(a, "addr") = addr;
    _(a, "wr_req") = wr;
    _(a, "rd_req") = rd_start;
    _(a, "burstbegin") = _(req, "ready") && _(req, "valid");

    _(resp, "valid") = full || rd2 && rvalid;
    _(_(resp, "contents"), "data") =  Cat(q, Latch(rd2, q));
    _(_(resp, "contents"), "id") = Wreg(_(req, "ready") && _(req, "valid"),
                                        _(_(req, "contents"), "id"));
    
    _(a, "size") = Lit<R>(2);

    bvec<N> mask_h(_(_(req, "contents"), "mask")[range<0,N-1>()]),
            mask_l(_(_(req, "contents"), "mask")[range<N,2*N-1>()]);
    _(a, "bmask") = Mux(wr2, mask_l, mask_h);
  }

  template <unsigned B, unsigned N, unsigned A, unsigned I, unsigned R>
    void AvlAdaptor(mem_port<B, 2*N, A, I> &port, avl<B, N, A, R> &a)
  { AvlAdaptor(_(port, "req"), _(port, "resp"), a); }
  
  // Single-entry writeback cache for adapting between different word/byte
  // sizes.
  //   req1's word size (B*N) has to be a multiple of req0's word size in order
  //   for this to work correctly, and all sizes must be a power of 2.
  //   req0, resp0 -- small req/resp pair, toward processor
  //   req1, resp1 -- larger req/resp pair, toward memory
  template <unsigned B0, unsigned N0, unsigned A0, unsigned I,
            unsigned B1, unsigned N1, unsigned A1>
    void SizeAdaptor(mem_req<B1, N1, A1, I> &req1, mem_resp<B1, N1, I> &resp1,
                     mem_req<B0, N0, A0, I> &req0, mem_resp<B0, N0, I> &resp0)
  {
    const unsigned M(N1*B1 / (N0*B0)), // Number of req0 words in the line
                   MM(CLOG2(M)); // Number of bits to address a line in M

    node load_line;

    TAP(load_line);

    // Valid bit: have we loaded anything yet?
    node valid(Wreg(load_line, Lit(1)));

    TAP(valid);

    // Dirty bit: has this line been written to?
    node next_dirty, dirty(Reg(next_dirty));

    TAP(dirty);

    // The tag: A0 - MM bits from the upper part of req0's address.
    bvec<A0> req_addr;
    bvec<A0 - MM> tag(Wreg(load_line, req_addr[range<MM, A0-1>()]));

    TAP(tag);

    // The storage itself. M words of N0 bytes of size B0 bits.
    vec<M, vec<N0, bvec<B0> > > line, line_in;
    vec<N0, bvec<B0> > wrdata;
    Flatten(line_in) = Flatten(_(_(resp1, "contents"), "data"));

    node write_line;
    bvec<M> load_word(Decoder(req_addr[range<0,MM-1>()], write_line));
    vec<M, bvec<N0> > load_byte;

    for (unsigned i = 0; i < M; ++i) {
      for (unsigned j = 0; j < N0; ++j) {
        bvec<B0> wrval(Mux(load_line, wrdata[j], line_in[i][j]));
        line[i][j] = Wreg(load_line || load_byte[i][j], wrval);
      }
    }

    TAP(line);
    TAP(load_byte);
    TAP(wrdata);

    node valid_req(_(req0, "valid") && _(req0, "ready"));  

    // Latch in the request when its valid. All of its fields will be important.
    mem_req<B0, N0, A0, I> req0l(Latch(!valid_req, req0));

    node miss(valid_req && (!valid || req_addr[range<MM, A0-1>()] != tag));

    TAP(valid_req);
    TAP(miss);

    TAP(req0l);
    req_addr = _(_(req0l, "contents"), "addr");
     
    req_addr = Latch(!valid_req, _(_(req0, "contents"), "addr"));
    wrdata = Latch(!valid_req, _(_(req0, "contents"), "data"));
    TAP(req_addr);

    for (unsigned i = 0; i < M; ++i)
      load_byte[i] = bvec<N0>(load_word[i]) & _(_(req0l, "contents"), "mask");

    // The controller. Basic state machine.
    enum state_t {
      ST_READY,      // Ready to receive commands.
      ST_WAITING,    // Waiting for resp to be ready
      ST_EVICTING,   // Evicting the currently-held line.
      ST_EV_WAITING,
      ST_REQUESTING, // Requesting the new line.
      ST_REQ_WAITING,
      ST_WRITING     // If we had a miss, take a cycle to complete the write.
    };

    bvec<3> next_state, state(Reg(next_state));
    bvec<8> state1h(Decoder(state));

    TAP(state);

    _(req0, "ready") = state1h[ST_READY];

    _(_(resp0, "contents"), "data") =
      Mux(req_addr[range<0,MM-1>()], Mux(load_line, line, line_in));
    _(_(resp0, "contents"), "id") = _(_(req0l, "contents"), "id");
    _(_(resp0, "contents"), "wr") = _(_(req0l, "contents"), "wr");
    
    _(resp1, "ready") = Lit(1);

    Cassign(next_dirty).
      IF(load_line && !_(_(req0l, "contents"), "wr"), Lit(0)).
      IF(load_line && _(_(req0l, "contents"), "wr"), Lit(1)).
      IF(valid_req && !miss && _(_(req0, "contents"), "wr"), Lit(1)).
      ELSE(dirty);

    node hit(state1h[ST_READY] && valid_req && !miss);
    write_line = (hit || state1h[ST_WRITING]) && _(_(req0l, "contents"), "wr"); 

    node do_request(state1h[ST_READY] && miss && !dirty),
         request_sent(state1h[ST_REQUESTING] && _(req1, "ready")),
         finish_request(state1h[ST_REQ_WAITING] &&
                        _(resp1, "valid") &&
                        !_(_(resp1, "contents"), "wr")),
         do_writestate(finish_request && _(_(req0l, "contents"), "wr")),
         do_evict(state1h[ST_READY] && miss && dirty),
         evict_sent(state1h[ST_EVICTING] && _(req1, "ready")),
         finish_evict(state1h[ST_EV_WAITING] /*&& _(resp1, "valid")*/),
         do_wait((hit||(finish_request&&!do_writestate)||state1h[ST_WRITING])
                  && !_(resp0, "ready")),
         finish_wait(state1h[ST_WAITING] && _(resp0, "ready"));

    _(resp0, "valid") = hit || state1h[ST_WAITING] || state1h[ST_WRITING] ||
                        (finish_request && !do_writestate);

    TAP(do_request);
    TAP(request_sent);
    TAP(do_evict);
    TAP(evict_sent);
    TAP(finish_evict);
    TAP(do_wait);
    TAP(finish_wait);
    TAP(do_writestate);

    load_line = finish_request;

    Cassign(next_state).
      IF(do_wait, Lit<3>(ST_WAITING)).
      IF(finish_wait, Lit<3>(ST_READY)).
      IF(do_request || finish_evict, Lit<3>(ST_REQUESTING)).
      IF(request_sent, Lit<3>(ST_REQ_WAITING)).
      IF(do_evict, Lit<3>(ST_EVICTING)).
      IF(evict_sent, Lit<3>(ST_EV_WAITING)).
      IF(finish_request && !do_writestate, Lit<3>(ST_READY)).
      IF(do_writestate, Lit<3>(ST_WRITING)).
      IF(state1h[ST_WRITING], Lit<3>(ST_READY)).
      ELSE(state);

    _(req1, "valid") = state1h[ST_REQUESTING] || state1h[ST_EVICTING];
    _(_(req1, "contents"), "addr") = Zext<A1>(
      Mux(state1h[ST_EVICTING], req_addr[range<MM, A0-1>()], tag)
    );
    _(_(req1, "contents"), "id") = _(_(req0l, "contents"), "id");
    _(_(req1, "contents"), "wr") = state1h[ST_EVICTING];
    _(_(req1, "contents"), "mask") = ~Lit<N1>(0);
    Flatten(_(_(req1, "contents"), "data")) = Flatten(line);
  }


  template <unsigned B0, unsigned N0, unsigned A0, unsigned I,
            unsigned B1, unsigned N1, unsigned A1>
    void SizeAdaptor(mem_port<B1, N1, A1, I> &out, mem_port<B0, N0, A0, I> &in)
  {  SizeAdaptor(_(out, "req"), _(out, "resp"), _(in, "req"), _(in, "resp")); }
};

#endif
