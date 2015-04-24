#include <fstream>

#include <chdl/chdl.h>
#include <chdl/lfsr.h>
#include <chdl/ingress.h>
#include <chdl/egress.h>
#include <chdl/counter.h>

#include "altram.h"

using namespace chdl;
using namespace std;

// Generate random reads/writes to RAM using the CHDL memory interface
template <unsigned B, unsigned N, unsigned A, unsigned I>
void ReqGen(mem_req<B, N, A, I> &req, mem_resp<B, N, I> &resp)
{
  bvec<(1<<I)> next_ids, ids(Reg(next_ids));

  _(req, "valid") = AndN(Lfsr<3, 127, 3, 0x5eed>(_(req, "ready")));
  _(_(req, "contents"), "wr") = AndN(Lfsr<2, 31, 1, 0x5eed>(
    _(req, "ready") && _(req, "valid")
  ));
  _(_(req, "contents"), "id") = Log2(~ids);

  _(_(req, "contents"), "addr") = Zext<A>(
    Lfsr<7, 127, 3, 0x1234>(_(req, "valid"))
  );

  Flatten(_(_(req, "contents"), "data")) = Zext<N * B>(
    Lfsr<32, 63, 1, 0x17>(_(req, "valid") && _(_(req, "contents"), "wr"))
  );

  _(_(req, "contents"), "mask") =
    Lfsr<N, 127, 1, 0x17>(_(req, "valid") && _(_(req, "contents"), "wr"));

  for (unsigned i = 0; i < (1<<I); ++i) {
    node set(!_(_(req, "contents"), "wr") &&
             _(req, "valid") && _(req, "ready") &&
             _(_(req, "contents"), "id") == Lit<I>(i)),
        clear(!_(_(resp, "contents"), "wr") &&
              _(resp, "valid") && _(resp, "ready") &&
              _(_(resp, "contents"), "id") == Lit<I>(i));

    Cassign(next_ids[i]).
      IF(set && !clear, Lit(1)).
      IF(clear && !set, Lit(0)).
      ELSE(ids[i]);
  }

  _(resp, "ready") = !AndN(Lfsr<2, 31, 1, 0x5eed5>());
  
  TAP(ids);

  Counter("generator_reqs",
          _(req,"ready") && _(req,"valid") && !_(_(req,"contents"),"wr"));
  Counter("generator_resps",
          _(resp,"ready") && _(resp,"valid") && !_(_(resp,"contents"),"wr"));
}

template <unsigned B, unsigned N, unsigned A, unsigned I>
  void ReqGen(mem_port<B, N, A, I> &p)
{ ReqGen(_(p, "req"), _(p, "resp")); }

template <unsigned B, unsigned N, unsigned A, unsigned R>
  void AvlDummy(avl<B, N, A, R> &a)
{    
    node burstbegin(_(a, "burstbegin")), upper_mode, ready,
         wr_req(_(a, "wr_req")), rd_req(_(a, "rd_req")),
         wr_upper(Wreg(wr_req, burstbegin)),
         wr_finished,
         valid((ready && rd_req) || wr_finished),
         avl_dummy_error(valid && _(a, "size") != Lit<R>(2));

    TAP(wr_upper);
    TAP(wr_finished);
    TAP(upper_mode);
    TAP(avl_dummy_error);

    mem_port<B, 2*N, A, 0> p;
    vec<2*N, bvec<B> > d;
    ready = _(_(p, "req"), "ready") && !wr_finished;
    _(_(p, "resp"), "ready") = Lit(1);
    _(_(p, "req"), "valid") = valid;
    _(_(_(p, "req"), "contents"), "addr") =
      Mux(burstbegin, Wreg(burstbegin, _(a, "addr")), _(a, "addr"));
    _(_(_(p, "req"), "contents"), "data") = d;
    _(_(_(p, "req"), "contents"), "wr") = wr_finished;
    _(_(_(p, "req"), "contents"), "mask") = ~Lit<2*N>(0);
    
    wr_finished = Wreg(wr_req || _(_(p, "req"), "ready"), wr_upper);
    
    Scratchpad<10>(p);

    TAP(p);

    node v(_(_(p, "resp"), "valid") && !_(_(_(p, "resp"), "contents"), "wr"));
    vec<2*N, bvec<B> > q =
      Mux(v, Wreg(v, _(_(_(p, "resp"), "contents"), "data")),
                     _(_(_(p, "resp"), "contents"), "data"));
    vec<N, bvec<B> > lower_bits(q[range<0,N-1>()]),
                     upper_bits(Wreg(v, q[range<N,2*N-1>()]));

    upper_mode = Reg(v);
    
    _(a, "ready") = ready;
    _(a, "rdata_valid") = v || upper_mode;
    _(a, "rdata") = Mux(upper_mode, lower_bits, upper_bits);

    d[range<0,N-1>()] = Wreg(wr_req && !wr_upper, _(a, "wdata"));
    d[range<N,2*N-1>()] = Wreg(wr_req && wr_upper, _(a, "wdata"));
}

// 1-beat AVL dummy
template <unsigned B, unsigned N, unsigned A, unsigned R>
  void AvlDummy1(avl<B, N, A, R> &a)
{    
    node burstbegin(_(a, "burstbegin")), upper_mode, ready,
         wr_req(_(a, "wr_req")), rd_req(_(a, "rd_req")),
         valid(ready && rd_req),
         avl_dummy_error(valid && _(a, "size") != Lit<R>(1));

    TAP(avl_dummy_error);

    mem_port<B, N, A, 0> p;
    ready = _(_(p, "req"), "ready");
    _(_(p, "resp"), "ready") = Lit(1);
    _(_(p, "req"), "valid") = valid;
    _(_(_(p, "req"), "contents"), "addr") =
      Mux(burstbegin, Wreg(burstbegin, _(a, "addr")), _(a, "addr"));
    _(_(_(p, "req"), "contents"), "data") = _(a, "wdata");
    _(_(_(p, "req"), "contents"), "wr") = wr_req;
    _(_(_(p, "req"), "contents"), "mask") = ~Lit<2*N>(0);
    
    Scratchpad<10>(p);

    TAP(p);

    node v(_(_(p, "resp"), "valid") && !_(_(_(p, "resp"), "contents"), "wr"));
    vec<N, bvec<B> > q =
      Mux(v, Wreg(v, _(_(_(p, "resp"), "contents"), "data")),
                     _(_(_(p, "resp"), "contents"), "data"));

    _(a, "ready") = ready;
    _(a, "rdata_valid") = v;
    _(a, "rdata") = q;
}

template <unsigned B, unsigned N, unsigned A, unsigned I>
  void MemVerify(mem_port<B, N, A, I> &p)
{
  struct vars : public tickable {
    bool req_wr, resp_wr, req_valid, resp_valid;
    unsigned long req_id, req_addr, resp_id;
    bool req_data[N*B], resp_data[N*B], req_mask[N];
    
    map<unsigned long, vector<bool> > contents;
    map<unsigned, unsigned long> rd_addr;
    map<unsigned, vector<bool> > rd_val;

    void post_tock(cdomain_handle_t cd) {
      if (req_valid) {
          cout << sim_time() << ": ";
          if (req_wr) {
            cout << "Write " << req_addr << ": id=" << req_id << endl;
            contents[req_addr].resize(N*B);
            for (unsigned i = 0; i < N*B; ++i) {
              if (req_mask[i/B]) {
                contents[req_addr][i] = req_data[i];
                cout << req_data[i];
              } else {
                cout << contents[req_addr][i];
              }
            }
            cout << endl;
          } else {
            cout << "Read " << req_addr << ": id=" << req_id << endl;
            rd_addr[req_id] = req_addr;

            rd_val[req_id].resize(contents[req_addr].size());
            for (unsigned i = 0; i < contents[req_addr].size(); ++i)
              rd_val[req_id][i] = contents[req_addr][i];
          }
      }
      if (resp_valid && !resp_wr) {
        vector<bool> &x(rd_val[resp_id]);
        unsigned long addr(rd_addr[resp_id]);
        bool e[N*B];
        cout << "Got response, id=" << resp_id << ", addr=" << addr << endl;
        for (unsigned i = 0; i < N*B; ++i) {
          cout << resp_data[i];
          e[i] = (((x.size() > i) && x[i]) != resp_data[i]);
        }
        cout << endl;
        for (unsigned i = 0; i < N*B; ++i)
          cout << (e[i] ? 'E' : '-');
        cout << endl;
      }
    }
  };

  vars &v = *(new vars());

  Egress(v.req_valid, _(_(p, "req"), "ready") && _(_(p, "req"), "valid"));
  Egress(v.resp_valid, _(_(p, "resp"), "ready") && _(_(p, "resp"), "valid"));
  Egress(v.req_wr, _(_(_(p, "req"), "contents"), "wr"));
  Egress(v.resp_wr, _(_(_(p, "resp"), "contents"), "wr"));
  EgressInt(v.req_addr, _(_(_(p, "req"), "contents"), "addr"));
  EgressInt(v.req_id, _(_(_(p, "req"), "contents"), "id"));
  EgressInt(v.resp_id, _(_(_(p, "resp"), "contents"), "id"));

  
  for (unsigned i = 0; i < N; ++i) {
    Egress(v.req_mask[i], _(_(_(p, "req"), "contents"), "mask")[i]);
    for (unsigned j = 0; j < B; ++j) {
      Egress(v.req_data[i*B + j], _(_(_(p,"req"),"contents"),"data")[i][j]);
      Egress(v.resp_data[i*B + j], _(_(_(p,"resp"),"contents"),"data")[i][j]);
    }
  }
}

template <unsigned N> struct num_t {};

template <unsigned T, unsigned B, unsigned N, unsigned A, unsigned I>
  void MemDelay(mem_port<B, N, A, I> &out, mem_port<B, N, A, I> &in, num_t<T>)
{
  mem_port<B, N, A, I> x;
  Buffer<1>(_(x, "req"), _(in, "req"));
  Connect(_(in, "resp"), _(x, "resp"));

  MemDelay(out, x, num_t<T - 1>());
}

template <unsigned B, unsigned N, unsigned A, unsigned I>
  void MemDelay(mem_port<B, N, A, I> &out, mem_port<B, N, A, I> &in, num_t<0>)
{
  Connect(_(out, "req"), _(in, "req"));
  Connect(_(in, "resp"), _(out, "resp"));
}

template <unsigned T, unsigned B, unsigned N, unsigned A, unsigned I>
  void MemDelay(mem_port<B, N, A, I> &out, mem_port<B, N, A, I> &in)
{ return MemDelay(out, in, num_t<T>()); }

int main(int argc, char **argv) {
  const unsigned B(8), N(4), A(30), I(4), BM(32), NM(16), AM(26);
  
  mem_port<B, N, A, I> a, a1;
  mem_port<BM, NM, AM, I> b;
  avl<BM, NM/2, AM, 3> c;
  // avl<BM, NM, AM, 3> c;
  
  ReqGen(a);
  MemDelay<20>(a1, a);
  SizeAdaptor(b, a1);
  // Scratchpad<8>(b);
  AvlAdaptor(b, c);
  AvlDummy/*1*/(c);
  
  TAP(a);
  TAP(b);
  TAP(c);

  MemVerify(a);
  //MemVerify(b);
                 
  optimize();

  ofstream vcd("altram.vcd");
  run(vcd, 10000);
  
  return 0;
}
