 #include <fstream>

#include <chdl/chdl.h>

#include <chdl/gates.h>
#include <chdl/gateops.h>
#include <chdl/lit.h>
#include <chdl/netlist.h>
#include <chdl/reg.h>

#include <chdl/tap.h>
#include <chdl/cassign.h>

#include <chdl/lfsr.h>
#include <chdl/memreq.h>

using namespace std;
using namespace chdl;

const unsigned POLICY_WRITEMEM_WRITE_THROUGH = 0;
const unsigned POLICY_WRITEMEM_WRITE_BACK = 1;
const unsigned POLICY_WRITEALLOC_NOALLOC = 0;
const unsigned POLICY_WRITEALLOC_ALLOC = 1;

const unsigned policy_writemem = POLICY_WRITEMEM_WRITE_THROUGH;
const unsigned policy_writealloc = POLICY_WRITEALLOC_ALLOC;

// generalized flipflop structure
template <typename t> using SReg =
	ag<STRTYPE("value"), t,
	ag<STRTYPE("next"), t
> >;

// scheduling queue entry
template <unsigned SQE, unsigned B, unsigned Nin, unsigned Nout, unsigned A, unsigned Iin, unsigned Iout, unsigned W> using NB_SQ_Entry =
    ag<STRTYPE("addr"),     bvec<A>,
    ag<STRTYPE("req_id"), bvec<Iin>,
    ag<STRTYPE("out_req_id"), bvec<Iout>,
    ag<STRTYPE("data"), vec<Nin, bvec<B>>,
    ag<STRTYPE("wmask"), bvec<Nin>,
    ag<STRTYPE("rw"), node,
    ag<STRTYPE("valid"), node,
    ag<STRTYPE("issued"), node,
    ag<STRTYPE("miss"), node,
    ag<STRTYPE("need_req_block"), node, // if the entry should request a block from main memory on its next issue 
    ag<STRTYPE("waiting_for_miss_bi"), node, // if the entry is waiting on a block from memory to come in
    ag<STRTYPE("miss_bring_in"), node, // if the entry is carrying returned block data from main memory and is repairing a miss on its next issue
    ag<STRTYPE("miss_bring_in_way"), bvec<W>, // when repairing a miss, which way are we evicting?  the decision is made in the first miss pass when writebacks occur
    ag<STRTYPE("block_data"), vec<Nout, bvec<B>>,
    ag<STRTYPE("wait_for"), bvec<1 << SQE>,
    ag<STRTYPE("done"), node
 > > > > > > > > > > > > > > > >;

// scheduling queue
template <unsigned SQE, unsigned B, unsigned Nin, unsigned Nout, unsigned A, unsigned Iin, unsigned Iout, unsigned W> using NB_SQ = 
	ag<STRTYPE("entries"),     vec<1 << SQE, NB_SQ_Entry<SQE, B, Nin, Nout, A, Iin, Iout, W>>,
	ag<STRTYPE("head"),     bvec<SQE>,
	ag<STRTYPE("tail"),	bvec<SQE>
> > >;

// tag lookup register structure

template <unsigned SQE, unsigned W, unsigned B, unsigned Nin, unsigned Nout, unsigned A, unsigned I> using NB_Tag_Lookup =
	ag<STRTYPE("valid"),	node,
	ag<STRTYPE("addr"),     bvec<A>,
	ag<STRTYPE("data"),     vec<Nin, bvec<B>>,
	ag<STRTYPE("wmask"),	bvec<Nin>,
	ag<STRTYPE("miss_bring_in"), node,
	ag<STRTYPE("miss_bring_in_way"), bvec<W>,
	ag<STRTYPE("need_req_block"), node,
	ag<STRTYPE("block_data"), vec<Nout, bvec<B>>,
	ag<STRTYPE("rw"),	node,
	ag<STRTYPE("sq_ndx"),	bvec<SQE>
> > > > > > > > > >;

// tag check register
template <unsigned SQE, unsigned tag_size, unsigned W, unsigned B, unsigned Nin, unsigned Nout, unsigned A, unsigned I> using NB_Tag_Check =
	ag<STRTYPE("valid"),	node,
	ag<STRTYPE("addr"),     bvec<A>,
	ag<STRTYPE("data"),     vec<Nin, bvec<B>>,
	ag<STRTYPE("wmask"),	bvec<Nin>,
	ag<STRTYPE("rw"),	node,
	ag<STRTYPE("miss_bring_in"), node,
	ag<STRTYPE("miss_bring_in_way"), bvec<W>,
	ag<STRTYPE("need_req_block"), node,
	ag<STRTYPE("block_data"), vec<Nout, bvec<B>>,
	ag<STRTYPE("sq_ndx"),	bvec<SQE>
> > > > > > > > > >;

// ex register
template <unsigned SQE, unsigned tag_size, unsigned S, unsigned W, unsigned B, unsigned Nin, unsigned Nout, unsigned A, unsigned I> using NB_EX =
	ag<STRTYPE("valid"),	node,
	ag<STRTYPE("addr"),     bvec<A>,
	ag<STRTYPE("data"),     vec<Nin, bvec<B>>,
	ag<STRTYPE("wmask"),	bvec<Nin>,
	ag<STRTYPE("rw"),	node,
	ag<STRTYPE("miss_bring_in"), node,
	ag<STRTYPE("miss_bring_in_way"), bvec<W>,
	ag<STRTYPE("need_req_block"), node,
	ag<STRTYPE("evicted_tag"), bvec<tag_size>,
	ag<STRTYPE("block_data"), vec<Nout, bvec<B>>,
	ag<STRTYPE("hit"),	node,
	ag<STRTYPE("set"),	bvec<S>,
	ag<STRTYPE("way"),	bvec<W>,
	ag<STRTYPE("sq_ndx"),	bvec<SQE>
> > > > > > > > > > > > > >;

// wb register
template <unsigned SQE, unsigned tag_size, unsigned inwords_in_block, unsigned S, unsigned W, unsigned B, unsigned Nin, unsigned Nout, unsigned A, unsigned I> using NB_WB = 
	ag<STRTYPE("valid"),	node,
	ag<STRTYPE("addr"),     bvec<A>,
	ag<STRTYPE("data"),     vec<Nin, bvec<B>>,
	ag<STRTYPE("rw"),	node,
	ag<STRTYPE("miss_bring_in"), node,
	ag<STRTYPE("miss_bring_in_way"), bvec<W>,
	ag<STRTYPE("need_req_block"), node,
	ag<STRTYPE("evicted_tag"), bvec<tag_size>,
	ag<STRTYPE("block_data"), vec<Nout, bvec<B>>,
	ag<STRTYPE("word_offset"), bvec<CLOG2(inwords_in_block)>,
	ag<STRTYPE("hit"),	node,
	ag<STRTYPE("set"),	bvec<S>,
	ag<STRTYPE("way"),	bvec<W>,
	ag<STRTYPE("sq_ndx"),	bvec<SQE>
> > > > > > > > > > > > > >;


// generalized nonblocking cache
/*
resp is the response from cache -> CPU
in_req is the request from CPU -> cache
out_resp is the response from memory -> cache
out_req is the request from cache -> memory
*/

// 2^S sets of cache data/tag storage (each block is Nout bytes)
// Each set is 2^W way-associative
// This means the size of the cache is:
// 2^(S+W) * Nout (bytes)
// 2^SQE scheduling queue entries.  This represents the maximum simultaneous outstanding misses.
// Input memory request: Nin B-bit bytes across with Ain address bits (word addressed)
// Output memory request (to the next level of memory (e.g., L2 cache, routing network, etc)): Nout B-bit bytes across with Aout address bits (block addressed)
  // All requests are paired with responses by unique I-bit-long identifiers
// The write policy is write-back
template<unsigned S, unsigned W, unsigned SQE, unsigned B, unsigned Nin, unsigned Ain, unsigned Iin, unsigned Nout, unsigned Aout, unsigned Iout>
 void NBCache(


	mem_resp<B, Nin, Iin> &resp, mem_req<B, Nin, Ain, Iin> &in_req, mem_resp<B, Nout, Iout> &out_resp, mem_req<B, Nout, Aout, Iout> &out_req) {


	// data storage
	bvec<S> data_store_addr;
	const unsigned block_size = Nout;
	const unsigned inwords_in_block = block_size / Nin;
	const unsigned tag_size = Ain - S - CLOG2(inwords_in_block);
	vec<1 << W, vec<block_size, bvec<B>>> data_store_write_data;
	vec<1 << W, vec<block_size, bvec<B>>> data_store_read_data;
	vec<1 << W, bvec<block_size>> data_store_we;
	bvec<S> data_valid_store_addr;
	vec<1 << W, bvec<1>> data_valid_store_write_data;
	vec<1 << W, bvec<1>> data_valid_store_read_data;
	bvec<1 << W> data_valid_store_we;
	
	vec<block_size, bvec<B>> requested_block_data;
	vec<Nin, bvec<B>> requested_word_data;
	bvec<CLOG2(inwords_in_block)> ex_requested_word_offset;
	
	
	for (int i = 0; i < 1 << W; i ++) {
		// each entry in data_valid_store is a memory addressed by the set number
		data_valid_store_read_data[i] = Syncmem(data_valid_store_addr, data_valid_store_write_data[i], data_valid_store_we[i]);
		for (int j = 0; j < block_size; j++) {
			// similarly, each entry in data_store is a byte-wide memory addressed by the set number
			data_store_read_data[i][j] = Syncmem(data_store_addr, data_store_write_data[i][j], data_store_we[i][j]); 
			

		}
	}

	// dirty bit storage
	// addressed by set number
	vec<1 << W, bvec<1>> dirty_store_read_data;
	vec<1 << W, bvec<1>> dirty_store_write_data;
	bvec<S> dirty_store_addr;
	bvec<1 << W> dirty_store_we;
	for (int i = 0; i < 1 << W; i ++) {
		dirty_store_read_data[i] = Syncmem(dirty_store_addr, dirty_store_write_data[i], dirty_store_we[i]);
	}


	// eviction block
	vec<1 << S, bvec<W>> replacement_policy_result;

	// first pipeline register: latched input
	mem_req<B, Nin, Ain, Iin> in_req_NOP;
	mem_req<B, Nin, Ain, Iin> in_req_latch = Reg(Mux(_(in_req, "valid") && _(in_req, "ready"), in_req_NOP, in_req));

	// scheduling queue
	SReg<NB_SQ<SQE, B, Nin, Nout, Ain, Iin, Iout, W>> sched_q;
	_(sched_q, "value") = Reg(_(sched_q, "next"));

	// writeback register
	SReg<NB_WB<SQE, tag_size, inwords_in_block, S, W, B, Nin, Nout, Ain, Iin>> wb_reg;
	node wb_reg_block_dirty;
	node abort_block_bring_in;

	// counter for output req ids
	bvec<Iout> out_req_id_counter;
	
	// sched queue logic

	vec<1 << SQE, NB_SQ_Entry<SQE, B, Nin, Nout, Ain, Iin, Iout, W>> sq_all; 
	bvec<1 << SQE> sq_ready_all;
	node sq_ready_any;
	NB_SQ_Entry<SQE, B, Nin, Nout, Ain, Iin, Iout, W> sq_sel;
	bvec<SQE> sq_ready_sel;
	bvec<1 << SQE> sq_ready_sel_decoded;


	node sq_compl;
	bvec<SQE> sq_compl_ndx;
	bvec<1 << SQE> sq_compl_ndx_dec = Decoder(sq_compl_ndx);
	bvec<2> sq_compl_type;
	vec<Nin, bvec<B>> sq_compl_data;

	bvec <1 << SQE> sq_valid_all;	
	bvec <1 << SQE> sq_valid_all_next;
	bvec <1 << SQE> sq_next_free_slot;
	sq_next_free_slot = Decoder(Log2(Not(sq_valid_all)));

	bvec <1 << SQE> sq_done_all;	
	bvec <SQE> sq_done_first;
	bvec <1 << SQE> sq_done_first_dec;
	sq_done_first = Log2(sq_done_all);
	sq_done_first_dec = Decoder(sq_done_first);
	node sq_done_any = !(sq_done_all == Lit<1 << SQE>(0));
	NB_SQ_Entry<SQE, B, Nin, Nout, Ain, Iin, Iout, W> sq_done_first_entry;
	sq_done_first_entry = Mux(sq_done_first, sq_all);

	bvec<1 << SQE> sqwe_all;
	TAP(sqwe_all);
	//const unsigned block_addr_len = Ain - CLOG2(inwords_in_block);

	bvec<1 << SQE> sq_word_addr_matches_in_latch_word_addr;
	bvec<Ain> in_latch_word_addr = _(_(in_req_latch, "contents"), "addr");

	vec<1 << SQE, bvec<S>> sq_set_wide;
	bvec<1 << SQE> sq_set_matches_in_latch_set;
	bvec<S> in_latch_set;
	in_latch_set = _(_(in_req_latch, "contents"), "addr")[range<CLOG2(inwords_in_block), CLOG2(inwords_in_block)+S-1>()];
	vec<1 << SQE, bvec<1 << SQE>> sq_incoming_dependency_check;

	// tells the dependency bits of all entries to clear
	bvec<1 << SQE> sq_clear_dependency;
	// Used for making other entries of the same set wait on a miss to finish.  
	node sq_miss_notification;
	bvec<1 << SQE> sq_set_matches_compl_entry;
	vec<1 << SQE, bvec<1 << SQE>> sq_enable_dependency;

	node in_latch_set_matches_compl_set = (S==0) ? Lit(1) : (_(_(wb_reg, "value"), "set") == in_latch_set);

	for (int i = 0; i < (1 << SQE); i ++) {
		
		sqwe_all[i] = _(in_req_latch, "valid") && sq_next_free_slot[i];
		

		node sqwe = sqwe_all[i];

		NB_SQ_Entry<SQE, B, Nin, Nout, Ain, Iin, Iout, W> curr_entry_next = _(_(sched_q, "next"), "entries")[i];
		NB_SQ_Entry<SQE, B, Nin, Nout, Ain, Iin, Iout, W> curr_entry = _(_(sched_q, "value"), "entries")[i];
		sq_set_wide[i] = _(curr_entry, "addr")[range<CLOG2(inwords_in_block), CLOG2(inwords_in_block)+S-1>()];
		sq_word_addr_matches_in_latch_word_addr[i] = in_latch_word_addr == _(curr_entry, "addr");
		sq_incoming_dependency_check[i] = sq_word_addr_matches_in_latch_word_addr & Lit<1 << SQE>(~(1 << i)) & sq_valid_all;

		node miss_bi_reqids_match = _(_(out_resp, "contents"), "id") == _(curr_entry, "out_req_id");

		sq_valid_all_next[i] = _(curr_entry_next, "valid");
		sq_valid_all[i] = _(curr_entry, "valid");
		sq_done_all[i] = _(curr_entry, "done");

		// whether this entry just completed bringing in a missed block
		node sq_compl_match = sq_compl_ndx_dec[i];
		node sq_miss_bring_in_compl_this = sq_compl && _(_(wb_reg, "value"), "miss_bring_in") && sq_compl_match;
		sq_set_matches_compl_entry[i] = S==0 ? (Lit(1)) : (sq_set_wide[i] == _(_(wb_reg, "value"), "set"));
		sq_set_matches_in_latch_set[i] = S==0 ? (Lit(1)) : (sq_set_wide[i] == in_latch_set);
		for (int j = 0; j < 1 << SQE; j ++) {
			node miss_finished = sq_compl && !_(_(wb_reg, "value"), "hit") && sq_compl_ndx_dec[j];
			node j_waits_on_i = sq_valid_all[j] && sq_valid_all[i] && _(sq_all[j], "wait_for")[i];
			node sq_enable_dependency_update = sq_valid_all[i] && sq_set_matches_compl_entry[i] && miss_finished && !j_waits_on_i;

			node sq_enable_dependency_new = sqwe && ( (in_latch_set_matches_compl_set && miss_finished) || (sq_valid_all[j] && sq_set_matches_in_latch_set[j] && _(sq_all[j], "miss")) );

			sq_enable_dependency[i][j] = (i==j) ? Lit(0) : (sq_enable_dependency_update || sq_enable_dependency_new);
		}

		node normal_access_complete_hit_match = sq_compl && _(_(wb_reg, "value"), "hit") && sq_compl_match;

		node sq_compl_this = sq_compl && sq_compl_match;


		node miss_bi_we = (miss_bi_reqids_match && _(out_resp, "valid") && _(curr_entry, "waiting_for_miss_bi")) || sq_miss_bring_in_compl_this;

		node succ_written_upstream = sq_done_first_dec[i] && sq_done_any && _(resp, "ready");
		TAP(succ_written_upstream);

		Cassign(_(curr_entry_next, "addr")).
			IF(sqwe, _(_(in_req_latch, "contents"), "addr")).
			ELSE(_(curr_entry, "addr"));

		Cassign(_(curr_entry_next, "req_id")).
			IF(sqwe, _(_(in_req_latch, "contents"), "id")).
			ELSE(_(curr_entry, "req_id"));

		Cassign(_(curr_entry_next, "out_req_id")).
			IF(sqwe, Lit<Iout>(0)).
			IF(_(_(wb_reg, "value"), "need_req_block") && sq_compl_this, out_req_id_counter).
			ELSE(_(curr_entry, "out_req_id"));

		Cassign(_(curr_entry_next, "rw")).
			IF(sqwe, _(_(in_req_latch, "contents"), "wr")).
			ELSE(_(curr_entry, "rw"));

		Cassign(_(curr_entry_next, "valid")).
			IF(sqwe, Lit(1)).
			IF(succ_written_upstream, Lit(0)).
			ELSE(_(curr_entry, "valid"));

		Cassign(_(curr_entry_next, "issued")).
			IF(sqwe, Lit(0)).
			IF(sq_compl_this, Lit(0)).
			IF(miss_bi_we, Lit(0)).
			IF(sq_ready_sel_decoded[i] && sq_ready_any, Lit(1)).			
			ELSE(_(curr_entry, "issued"));

		Cassign(_(curr_entry_next, "miss")).
			IF(sqwe, Lit(0)).
			IF(sq_compl_this && !normal_access_complete_hit_match, Lit(1)).
			
			ELSE(_(curr_entry, "miss"));

		node sq_any_dependencies_enabling = sq_enable_dependency[i]!=Lit<1 << SQE>(0);

		Cassign(_(curr_entry_next, "miss_bring_in")).
			IF(sqwe, Lit(0)).
			IF(abort_block_bring_in && sq_compl_this, Lit(0)).
			IF(miss_bi_we, !sq_miss_bring_in_compl_this).	
			IF(sq_any_dependencies_enabling, Lit(0)).		
			ELSE(_(curr_entry, "miss_bring_in"));

		Cassign(_(curr_entry_next, "block_data")).
			IF(miss_bi_we, _(_(out_resp, "contents"), "data")).			
			ELSE(_(curr_entry, "block_data"));

		sq_clear_dependency[i] = Reg(succ_written_upstream);

		Cassign(_(curr_entry_next, "wait_for")).
			IF(sqwe, Or(sq_incoming_dependency_check[i], sq_enable_dependency[i])).
			ELSE(
				Or(
					And(
						_(curr_entry, "wait_for"), 
						Not(sq_clear_dependency)
					),
					sq_enable_dependency[i]
				)
			);

		Cassign(_(curr_entry_next, "done")).
			IF(sqwe, Lit(0)).
			IF(normal_access_complete_hit_match, 
				Lit(1)
			).
			IF(succ_written_upstream, Lit(0)).
			ELSE(_(curr_entry, "done"));



		Cassign(_(curr_entry_next, "data")).
			IF(sqwe, _(_(in_req_latch, "contents"), "data")).
			IF(sq_compl_this && !_(curr_entry, "rw"), sq_compl_data).
			ELSE(_(curr_entry, "data"));

		Cassign(_(curr_entry_next, "wmask")).
			IF(sqwe, _(_(in_req_latch, "contents"), "mask")).
			ELSE(_(curr_entry, "wmask"));

		Cassign(_(curr_entry_next, "waiting_for_miss_bi")).
			IF(sqwe, Lit(0)).
			IF(miss_bi_we, Lit(0)).
			IF(sq_any_dependencies_enabling, Lit(0)).
			IF(sq_compl_this).
				IF(abort_block_bring_in, Lit(0)).
				IF(_(curr_entry, "need_req_block"), Lit(1)).
				ELSE(Lit(0)).
			END().
			ELSE(_(curr_entry, "waiting_for_miss_bi"));

		Cassign(_(curr_entry_next, "miss_bring_in_way")).
			IF(sqwe, Lit<W>(0)).
			IF(sq_compl_this && !_(curr_entry, "miss_bring_in") && !_(curr_entry, "need_req_block"), _(_(wb_reg, "value"), "miss_bring_in_way")).
			ELSE(_(curr_entry, "miss_bring_in_way"));

		Cassign(_(curr_entry_next, "need_req_block")).
			IF(sqwe, Lit(0)).
			IF(miss_bi_we, Lit(0)).
			IF(sq_any_dependencies_enabling, Lit(0)).
			IF(sq_compl_this).
				IF(abort_block_bring_in, Lit(0)).
				IF(_(_(wb_reg, "value"), "need_req_block"), !_(out_req, "valid")).
				IF(!_(curr_entry, "miss_bring_in"), Lit(1)).
				ELSE(Lit(0)).			
			END().
			ELSE(_(curr_entry, "need_req_block"));

	}

	/* update scheduling queue from writeback reg */
	sq_compl = _(_(wb_reg, "value"), "valid");
	sq_compl_ndx = _(_(wb_reg, "value"), "sq_ndx");
	sq_compl_data = requested_word_data;

	// output to "upstream" of memory hierarchy (towards CPU)
	mem_resp<B, Nin, Iin> resp_formed;
	_(resp_formed, "valid") = Lit(1);
	_(_(resp_formed, "contents"), "data") = _(sq_done_first_entry, "data");
	_(_(resp_formed, "contents"), "id") = _(sq_done_first_entry, "req_id");
	mem_resp<B, Nin, Iin> resp_NOP;

	// return reponses when they are reads and they are complete
	node whether_to_output_formed_resp = sq_done_any && !_(sq_done_first_entry, "rw");
	resp = Mux(whether_to_output_formed_resp, resp_NOP, resp_formed);


	node sq_free_slot_avail = !(sq_valid_all_next == Lit<1 << SQE>(-1)); 
	// cache is ready to accept new requests only if there are additional slots in the scheduling queue
	// this removes the risk of misses being "stranded" in the pipeline
	_(in_req, "ready") = sq_free_slot_avail;
	
	// issue/tag lookup register
	SReg<NB_Tag_Lookup<SQE, W, B, Nin, Nout, Ain, Iin>> tag_access_reg;
	_(tag_access_reg, "value") = Reg(_(tag_access_reg, "next"));
	

	// issue logic
	sq_all = _(_(sched_q, "value"), "entries");
	for (int i = 0; i < 1 << SQE; i ++) {
		sq_ready_all[i] = _(sq_all[i], "valid") && (!_(sq_all[i], "issued")) && !_(sq_all[i], "waiting_for_miss_bi") && !_(sq_all[i], "done") && (_(sq_all[i], "wait_for") == Lit<1 << SQE>(0));
	}
	sq_ready_any = !(sq_ready_all == Lit<1 << SQE>(0));
	sq_ready_sel = Log2(sq_ready_all);
	sq_ready_sel_decoded = Decoder(sq_ready_sel);
	// the priority encoder feeds into the control of a mux distinguishing amongst scheduling queue entries
	sq_sel = Mux(sq_ready_sel, sq_all);

	node tag_access_reg_we = sq_ready_any;

	// hook up this output to the tag lookup register
	Cassign(_(_(tag_access_reg, "next"), "addr")).
		IF(tag_access_reg_we, _(sq_sel, "addr")).
		ELSE(_(_(tag_access_reg, "value"), "addr"));

	Cassign(_(_(tag_access_reg, "next"), "data")).
		IF(tag_access_reg_we, _(sq_sel, "data")).
		ELSE(_(_(tag_access_reg, "value"), "data"));

	Cassign(_(_(tag_access_reg, "next"), "rw")).
		IF(tag_access_reg_we, _(sq_sel, "rw")).
		ELSE(_(_(tag_access_reg, "value"), "rw"));

	_(_(tag_access_reg, "next"), "wmask") = _(sq_sel, "wmask");
	_(_(tag_access_reg, "next"), "valid") = tag_access_reg_we;
	_(_(tag_access_reg, "next"), "miss_bring_in") = _(sq_sel, "miss_bring_in");
	_(_(tag_access_reg, "next"), "miss_bring_in_way") = _(sq_sel, "miss_bring_in_way");
	_(_(tag_access_reg, "next"), "need_req_block") = _(sq_sel, "need_req_block");
	_(_(tag_access_reg, "next"), "block_data") = _(sq_sel, "block_data");
	_(_(tag_access_reg, "next"), "sq_ndx") = sq_ready_sel;

	// tag storage


	// tag storage RAM
	// all tags for the same set are accessed simultaneously
	bvec<S> tag_store_addr;
	vec<1 << W, bvec<tag_size>> tag_store_write_data;
	vec<1 << W, bvec<tag_size>> tag_store_read_data;
	bvec<1 << W> tag_store_we;
	for (int i = 0; i < 1 << W; i ++) {
		tag_store_read_data[i] = Syncmem(tag_store_addr, tag_store_write_data[i], tag_store_we[i]); 
	}
	
	
	// tag lookup
	tag_store_addr = _(_(tag_access_reg, "value"), "addr") [range <CLOG2(inwords_in_block), S+CLOG2(inwords_in_block)-1>()];
	data_valid_store_addr = tag_store_addr;

	// replacement logic: which way to evict?
	// for now do a pseudorandom way based on LFSR
	bvec<W> way_to_evict;
	node LFSR_advance = Lit(1);
	bvec<W> LFSR_way_select = Lfsr<W, 65, 18, 1>(LFSR_advance);
	way_to_evict = LFSR_way_select;

	bvec<1 << W> precomputed_way_to_evict_decoded = Decoder(_(_(tag_access_reg, "value"), "miss_bring_in_way"));

	// tag write logic
	// tags are only changed when we have a bring-in
	bvec<tag_size> tag_from_req_ta;
	tag_from_req_ta = _(_(tag_access_reg, "value"), "addr") [range<S+CLOG2(inwords_in_block), Ain-1>()]; // extract tag from request address
	for (int i = 0; i < 1 << W; i ++) {
		tag_store_write_data[i] = tag_from_req_ta;
		tag_store_we[i] = _(_(tag_access_reg, "value"), "valid") && _(_(tag_access_reg, "value"), "miss_bring_in") && precomputed_way_to_evict_decoded[i];

		// data valid store is accessed in parallel with tag store
		data_valid_store_write_data[i][0] = Lit(1);
		data_valid_store_we[i] = tag_store_we[i];
	}

	// we decide the way to evict during the first miss pass
	node tag_lookup_is_normal_access = !_(_(tag_access_reg, "value"), "miss_bring_in") && !_(_(tag_access_reg, "value"), "need_req_block");


	// tag lookup/tag check register
	SReg<NB_Tag_Check<SQE, tag_size, W, B, Nin, Nout, Ain, Iin>> tag_check_reg;
	_(tag_check_reg, "value") = Reg(_(tag_check_reg, "next"));

	// tag check register input
	_(_(tag_check_reg, "next"), "addr") = _(_(tag_access_reg, "value"), "addr");
	_(_(tag_check_reg, "next"), "data") = _(_(tag_access_reg, "value"), "data");
	_(_(tag_check_reg, "next"), "wmask") = _(_(tag_access_reg, "value"), "wmask");
	_(_(tag_check_reg, "next"), "rw") = _(_(tag_access_reg, "value"), "rw");
	_(_(tag_check_reg, "next"), "miss_bring_in") = _(_(tag_access_reg, "value"), "miss_bring_in");

	// if not first miss pass, use precomputed way to evict
	_(_(tag_check_reg, "next"), "miss_bring_in_way") = Mux(tag_lookup_is_normal_access, _(_(tag_access_reg, "value"), "miss_bring_in_way"), way_to_evict);
	_(_(tag_check_reg, "next"), "need_req_block") = _(_(tag_access_reg, "value"), "need_req_block");
	_(_(tag_check_reg, "next"), "block_data") = _(_(tag_access_reg, "value"), "block_data");
	_(_(tag_check_reg, "next"), "valid") = _(_(tag_access_reg, "value"), "valid");
	_(_(tag_check_reg, "next"), "sq_ndx") = _(_(tag_access_reg, "value"), "sq_ndx");

	// tag check
	bvec<tag_size> tag_from_req;
	tag_from_req = _(_(tag_check_reg, "value"), "addr") [range<S+CLOG2(inwords_in_block), Ain-1>()];
	bvec<1 << W> tags_match_wide;
	node any_tag_matched;
	
	// hit/miss check:
	// compare against each tag read from the store
	for (int i = 0; i < 1 << W; i ++) {
		bvec<tag_size> way_tag =  tag_store_read_data[i];
		tags_match_wide[i] = (tag_from_req == way_tag) && data_valid_store_read_data[i][0];
	}

	any_tag_matched = !(tags_match_wide == Lit<1 << W>(0));


	bvec<W> matched_way = Log2(tags_match_wide);

	



	// tag check/ex register
	SReg<NB_EX<SQE, tag_size, S, W, B, Nin, Nout, Ain, Iin>> ex_reg;
	_(ex_reg, "value") = Reg(_(ex_reg, "next"));

	// inputs to ex register
	_(_(ex_reg, "next"), "valid") = _(_(tag_check_reg, "value"), "valid");
	_(_(ex_reg, "next"), "addr") = _(_(tag_check_reg, "value"), "addr");
	_(_(ex_reg, "next"), "rw") = _(_(tag_check_reg, "value"), "rw");
	_(_(ex_reg, "next"), "miss_bring_in") = _(_(tag_check_reg, "value"), "miss_bring_in");
	_(_(ex_reg, "next"), "miss_bring_in_way") = _(_(tag_check_reg, "value"), "miss_bring_in_way");
	_(_(ex_reg, "next"), "need_req_block") = _(_(tag_check_reg, "value"), "need_req_block");
	_(_(ex_reg, "next"), "evicted_tag") = Mux(_(_(tag_check_reg, "value"), "miss_bring_in_way"), tag_store_read_data);
	_(_(ex_reg, "next"), "block_data") = _(_(tag_check_reg, "value"), "block_data");
	_(_(ex_reg, "next"), "data") = _(_(tag_check_reg, "value"), "data");
	_(_(ex_reg, "next"), "wmask") = _(_(tag_check_reg, "value"), "wmask");
	_(_(ex_reg, "next"), "set") = _(_(tag_check_reg, "value"), "addr")[range<CLOG2(inwords_in_block), CLOG2(inwords_in_block)+S-1>()];

	Cassign(_(_(ex_reg, "next"), "way")).
		IF(_(_(tag_check_reg, "value"), "miss_bring_in"), _(_(tag_check_reg, "value"), "miss_bring_in_way")).
		IF(any_tag_matched, matched_way).
		ELSE(_(_(tag_check_reg, "value"), "miss_bring_in_way"));

	_(_(ex_reg, "next"), "hit") = any_tag_matched;
	_(_(ex_reg, "next"), "sq_ndx") = _(_(tag_check_reg, "value"), "sq_ndx");

	// ex stage
	


	// data store read logic
	data_store_addr = _(_(ex_reg, "value"), "set");
	ex_requested_word_offset = _(_(ex_reg, "value"), "addr")[range<0, CLOG2(inwords_in_block)-1>()];
	requested_block_data = Mux(_(_(wb_reg, "value"), "way"), data_store_read_data);

	for (int i = 0; i < Nin; i ++) {
		bvec<CLOG2(block_size)> wb_requested_byte_offset;
		wb_requested_byte_offset = Cat(_(_(wb_reg, "value"), "word_offset"), Lit<CLOG2(Nin)>(i));
		requested_word_data[i] = Mux(wb_requested_byte_offset, requested_block_data);
	}

	// data store write logic
	bvec<1 << W> data_way_write_decoder = Decoder(_(_(ex_reg, "value"), "way"));
	bvec<inwords_in_block> data_word_write_decoder = Decoder(ex_requested_word_offset);
	for (int i = 0; i < 1 << W; i ++) {
		node m_bi_data_we = _(_(ex_reg, "value"), "valid") && _(_(ex_reg, "value"), "miss_bring_in");
		node write_hit_data_we = _(_(ex_reg, "value"), "valid") && _(_(ex_reg, "value"), "rw") && _(_(ex_reg, "value"), "hit") && !_(_(ex_reg, "value"), "miss_bring_in");
		
		// j is the byte within a block
		for (int j = 0; j < block_size; j++) {
			// inword_offs is the byte within a word
			int inword_offs = j % Nin;
			// word_ndx is the word within a block
			int word_ndx = j / Nin;
			
			data_store_we[i][j] = data_way_write_decoder[i] && (m_bi_data_we || (write_hit_data_we && data_word_write_decoder[word_ndx])) && _(_(ex_reg, "value"), "wmask")[inword_offs];

			data_store_write_data[i][j] = Mux(_(_(ex_reg, "value"), "miss_bring_in"), _(_(ex_reg, "value"), "data")[inword_offs],  _(_(ex_reg, "value"), "block_data")[j]);
			

		}
	}
	// dirty bit write logic
	// write dirty bit on write hits
	dirty_store_addr = _(_(ex_reg, "value"), "set");
	node ex_wb_pass = !_(_(ex_reg, "value"), "miss_bring_in") && !_(_(ex_reg, "value"), "need_req_block") && !_(_(ex_reg, "value"), "hit");
	for (int i = 0; i < 1 << W; i ++) {
		dirty_store_we[i] = _(_(ex_reg, "value"), "valid") && data_way_write_decoder[i] && (ex_wb_pass || _(_(ex_reg, "value"), "miss_bring_in") || (_(_(ex_reg, "value"), "hit") && _(_(ex_reg, "value"), "rw")));
		Cassign(dirty_store_write_data[i]).
			IF(ex_wb_pass, Lit(0)).
			IF(_(_(ex_reg, "value"), "miss_bring_in"), Lit(0)).
			ELSE(Lit(1));

	}
	
	
	
	node return_valid_resp = _(_(ex_reg, "value"), "valid") && _(_(ex_reg, "value"), "hit") && !_(_(ex_reg, "value"), "rw");

	// writeback register

	_(wb_reg, "value") = Reg(_(wb_reg, "next"));

	
	// inputs to wb register
	_(_(wb_reg, "next"), "valid") = _(_(ex_reg, "value"), "valid");
	_(_(wb_reg, "next"), "addr") = _(_(ex_reg, "value"), "addr");
	_(_(wb_reg, "next"), "rw") = _(_(ex_reg, "value"), "rw");
	_(_(wb_reg, "next"), "miss_bring_in") = _(_(ex_reg, "value"), "miss_bring_in");
	_(_(wb_reg, "next"), "miss_bring_in_way") = _(_(ex_reg, "value"), "miss_bring_in_way");
	_(_(wb_reg, "next"), "need_req_block") = _(_(ex_reg, "value"), "need_req_block");
	_(_(wb_reg, "next"), "evicted_tag") = _(_(ex_reg, "value"), "evicted_tag");
	_(_(wb_reg, "next"), "block_data") = _(_(ex_reg, "value"), "block_data");
	_(_(wb_reg, "next"), "word_offset") = ex_requested_word_offset;
	_(_(wb_reg, "next"), "data") = _(_(ex_reg, "value"), "data");
	_(_(wb_reg, "next"), "set") = _(_(ex_reg, "value"), "set");
	_(_(wb_reg, "next"), "way") = _(_(ex_reg, "value"), "way");
	_(_(wb_reg, "next"), "hit") = _(_(ex_reg, "value"), "hit");
	_(_(wb_reg, "next"), "sq_ndx") = _(_(ex_reg, "value"), "sq_ndx");


	// output requests to downstream memory hierarchy (away from CPU)

	
	_(_(out_req, "contents"), "id") = out_req_id_counter;

	

	// on a write-back, write back.
	
	wb_reg_block_dirty = Mux(_(_(wb_reg, "value"), "way"), dirty_store_read_data)[0];
	
	node issue_write_back = _(_(wb_reg, "value"), "valid") && !_(_(wb_reg, "value"), "hit") && wb_reg_block_dirty && !_(_(wb_reg, "value"), "need_req_block") && !_(_(wb_reg, "value"), "miss_bring_in");

	// on a miss, request the missed block
	// make to sure to abort block requests if there were intervening writes.  Dirty bit will be zero if there were no intervening writes.
	
	abort_block_bring_in = wb_reg_block_dirty && _(_(wb_reg, "value"), "need_req_block") && _(_(wb_reg, "value"), "valid");
	node issue_block_bring_in = _(_(wb_reg, "value"), "valid") && _(_(wb_reg, "value"), "need_req_block") && !abort_block_bring_in;
	

	bvec<Aout> wb_evicted_block_addr;
	wb_evicted_block_addr = Zext<Aout>(Cat(_(_(wb_reg, "value"), "evicted_tag"), _(_(wb_reg, "value"), "set")));

	bvec<Aout> wb_bring_in_block_addr;
	wb_bring_in_block_addr = Zext<Aout>(_(_(wb_reg, "value"), "addr")[range<CLOG2(inwords_in_block), Ain-1>()]);

	Cassign(_(out_req, "valid")).
		IF(!_(out_req, "ready"), Lit(0)).
		IF(issue_write_back, Lit(1)).
		IF(issue_block_bring_in, Lit(1)).
		ELSE(Lit(0));

	Cassign(_(_(out_req, "contents"), "wr")).
		IF(issue_write_back, Lit(1)).
		IF(issue_block_bring_in, Lit(0)).
		ELSE(Lit(0));

	Cassign(_(_(out_req, "contents"), "addr")).
		IF(issue_write_back, wb_evicted_block_addr).
		IF(issue_block_bring_in, wb_bring_in_block_addr).
		ELSE(Lit<Aout>(0));

	// output request mask is always -1
	_(_(out_req, "contents"), "mask") = Lit<Nout>(-1);


	_(_(out_req, "contents"), "data") = requested_block_data;

	out_req_id_counter = Wreg(issue_block_bring_in, out_req_id_counter + Lit<Iout>(1));
	
	_(out_resp, "ready") = Lit(1);
	TAP(in_req_latch);
	TAP(sched_q);
	TAP(tag_access_reg);
	TAP(tag_check_reg);
	TAP(ex_reg);
	TAP(wb_reg);

	TAP(tag_store_write_data);
	TAP(tag_store_read_data);
	TAP(tag_store_addr);
	TAP(tag_store_we);
	TAP(data_store_write_data);
	TAP(data_store_read_data);
	TAP(data_store_we);
	TAP(data_valid_store_write_data);
	TAP(data_valid_store_read_data);
	TAP(data_valid_store_we);
	TAP(data_word_write_decoder);
	TAP(data_store_addr);
	TAP(ex_wb_pass);
	TAP(dirty_store_write_data);
	TAP(dirty_store_read_data);
	TAP(dirty_store_we);
	TAP(dirty_store_addr);
	TAP(tag_from_req);
	TAP(any_tag_matched);
	TAP(tags_match_wide);
	TAP(matched_way);
	TAP(sq_ready_sel);
	TAP(sq_sel);
	TAP(sq_ready_all);
	TAP(sq_ready_any);
	TAP(out_req_id_counter);
	TAP(sq_compl);
	TAP(sq_compl_ndx);
	TAP(sq_done_any);
	TAP(sq_done_all);
	TAP(sq_done_first);
	TAP(sq_compl_data);
	TAP(sq_clear_dependency);
	TAP(sq_enable_dependency);
	TAP(sq_incoming_dependency_check);
	TAP(sq_next_free_slot);
	TAP(sq_valid_all);
	TAP(requested_word_data);
	TAP(requested_block_data);
	TAP(ex_requested_word_offset);
	TAP(way_to_evict);
	

}

