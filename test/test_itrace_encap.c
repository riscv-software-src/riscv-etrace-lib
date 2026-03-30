// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Macros, data structures, and helper routines based on the
 * ratified v1.0.0 RISC-V etrace encapsulation specification [1].
 *
 * [1] https://github.com/riscv-non-isa/e-trace-encap
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <rv_etrace_encap.h>
#include <rv_etrace_itrace.h>

struct itrace_test {
	const char *name;
	struct rv_etrace_params *params;
	const unsigned char *ref_pkt;
	unsigned int ref_pkt_size;
	unsigned int ref_flow;
	unsigned int ref_srcid;
	unsigned int ref_extend;
	unsigned long long ref_tstamp;
	struct rv_itrace_data ref_it;
};

static void itrace_do_test(const struct itrace_test *t)
{
	unsigned int in_flow, in_srcid, in_extend, in_pkt_size;
	const struct rv_etrace_packet *in_pkt;
	struct rv_etrace_payload in_pld, out_pld;
	struct rv_etrace_packet out_pkt;
	unsigned long long in_tstamp;
	struct rv_itrace_data in_it;
	int i, rc;

	memset(&in_pld, 0, sizeof(in_pld));
	memset(&in_it, 0, sizeof(in_it));
	memset(&out_pld, 0, sizeof(out_pld));
	memset(&out_pkt, 0, sizeof(out_pkt));

	printf("TEST: %s: starting\n", t->name);

	in_pkt = (const struct rv_etrace_packet *)t->ref_pkt;
	in_pkt_size = rv_etrace_packet_size(t->params, in_pkt);
	if (in_pkt_size != t->ref_pkt_size) {
		printf("TEST: %s: error: mismatch packet size, expected %d, got %d\n",
			t->name, t->ref_pkt_size, in_pkt_size);
		return;
	}

	in_flow = rv_etrace_packet_header_flow_read(in_pkt);
	if (in_flow != t->ref_flow) {
		printf("TEST: %s: error: mismatch flow, expected 0x%x, got 0x%x\n",
			t->name, t->ref_flow, in_flow);
	}

	if (t->params->packet.srcid_bytes_p) {
		rc = rv_etrace_packet_srcid_read(t->params, in_pkt, &in_srcid);
		if (rc < 0) {
			printf("TEST: %s: error: srcid read failed\n", t->name);
			return;
		}
		if (in_srcid != t->ref_srcid) {
			printf("TEST: %s: error: mismatch srcid, expected 0x%x, got 0x%x\n",
				t->name, t->ref_srcid, in_srcid);
			return;
		}
	}

	in_extend = rv_etrace_packet_header_extend_read(in_pkt);
	if (in_extend != t->ref_extend) {
		printf("TEST: %s: error: mismatch extend, expected 0x%x, got 0x%x\n",
			t->name, t->ref_extend, in_extend);
		return;
	}

	if (t->params->packet.tstamp_bytes_p &&
	    rv_etrace_packet_header_extend_read(in_pkt)) {
		rc = rv_etrace_packet_tstamp_read(t->params, in_pkt, &in_tstamp);
		if (rc < 0) {
			printf("TEST: %s: error: time stamp read failed\n", t->name);
			return;
		}
		if (in_tstamp != t->ref_tstamp) {
			printf("TEST: %s: error: mismatch tstamp, expected 0x%llx, got 0x%llx\n",
				t->name, t->ref_tstamp, in_tstamp);
			return;
		}
	}

	rc = rv_etrace_packet_payload_read(t->params, in_pkt, &in_pld);
	if (rc < 0) {
		printf("TEST: %s: error: payload read failed\n", t->name);
		return;
	}

	rc = rv_itrace_payload_read(t->params, &in_pld, &in_it);
	if (rc < 0) {
		printf("TEST: %s: error: itrace data read failed\n", t->name);
		return;
	}
	if (in_it.format != t->ref_it.format) {
		printf("TEST: %s: error: mismatch format, expected 0x%x, got 0x%x\n",
			t->name, t->ref_it.format, in_it.format);
		return;
	}

	if (memcmp(&in_it, &t->ref_it, sizeof(in_it))) {
		printf("TEST: %s: error: mismatch in decoded itrace data\n",
			t->name);
		for (i = 0; i < sizeof(in_it); i++) {
			printf("TEST: %s: reference[%d]=0x%x decoded[%d]=0x%x%s\n",
				t->name,
				i, ((unsigned char *)&t->ref_it)[i],
				i, ((unsigned char *)&in_it)[i],
				(((unsigned char *)&t->ref_it)[i] != ((unsigned char *)&in_it)[i])
				? " (mismatch)" : "");
		}
		return;
	}

	rc = rv_itrace_payload_write(t->params, &out_pld, &in_it);
	if (rc < 0) {
		printf("TEST: %s: error: itrace data write failed\n", t->name);
		return;
	}

	rc = rv_etrace_packet_write(t->params, &out_pkt,
				    in_flow, in_srcid, in_extend, in_tstamp, &out_pld);
	if (rc < 0) {
		printf("TEST: %s: error: packet write failed\n", t->name);
		return;
	}

	if (memcmp(&out_pkt, t->ref_pkt, t->ref_pkt_size)) {
		printf("TEST: %s: error: mismatch in encoded packet\n",
			t->name);
		for (i = 0; i < t->ref_pkt_size; i++) {
			printf("TEST: %s: encoded[%d]=0x%x reference[%d]=0x%x%s\n",
				t->name,
				i, ((unsigned char *)&out_pkt)[i],
				i, t->ref_pkt[i],
				(((unsigned char *)&out_pkt)[i] != t->ref_pkt[i])
				? " (mismatch)" : "");
		}
		return;
	}

	printf("TEST: %s: completed\n", t->name);
}

static struct rv_etrace_params itrace_params = {
	.packet = {
		.srcid_bytes_p = 1,
		.tstamp_bytes_p = 4,
		.type_width_p = 0,
	},
	.itrace = {
		.arch_p = 0,
		.blocks_p = 0,		/* Not provided, assuming 0 */
		.bpred_size_p = 5,
		.cache_size_p = 4,
		.call_counter_size_p = 0,
		.ctype_width_p = 0,
		.context_width_p = 20,
		.time_width_p = 25,
		.ecause_width_p = 6,
		.f0s_width_p = 1,
		.filter_context_p = 0,
		.filter_time_p = 0,
		.filter_excint_p = 0,
		.filter_privilege_p = 0,
		.filter_tval_p = 0,
		.iaddress_lsb_p = 1,
		.iaddress_width_p = 55,
		.iretire_width_p = 6,
		.ilastsize_width_p = 1,
		.itype_width_p = 4,
		.nocontext_p = 0,
		.notime_p = 0,
		.privilege_width_p = 3,
		.retires_p = 6,
		.return_stack_size_p = 4,
		.sijump_p = 0,
		.impdef_width_p = 0
	},
};

static const unsigned char itrace_format2_test1_packet[] = {
	0x08,			/* header byte */
	0xef,			/* srcID */
	0x46,			/* format 2  */
	0x44,0x44,0x44,0x44,0x44,0x44,/**Address field */
	0x00,/* notify ,updiscon,irreport, irdepth */
};

static struct itrace_test itrace_format2_test1 = {
	.name = "itrace_format2_test1",
	.params = &itrace_params,
	.ref_pkt = itrace_format2_test1_packet,
	.ref_pkt_size = sizeof(itrace_format2_test1_packet),
	.ref_srcid = 0xef,
	.ref_it = {
		.format = 2,
		.format2 = {
			.iaddress = {
				.addr = 0x22222222222222,
				.notify = 0,
				.updiscon = 0,
				.irreport = 0,
				.irdepth = 0,
			},
		},
	},
};

static const unsigned char itrace_format2_test2_packet[] = {
	0x88,			/* header byte */
	0xef,			/* srcID */
	0x12, 0x34, 0x56, 0x78,	/* tstamp */
	0x46,			/* format 2  */
	0x44,0x44,0x44,0x44,0x44,0x44,/* Address field */
	0x00,/* notify ,updiscon,irreport, irdepth */
};

static struct itrace_test itrace_format2_test2 = {
	.name = "itrace_format2_test2",
	.params = &itrace_params,
	.ref_pkt = itrace_format2_test2_packet,
	.ref_pkt_size = sizeof(itrace_format2_test2_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
		.format = 2,
		.format2 = {
			.iaddress = {
				.addr = 0x22222222222222,
				.notify = 0,
				.updiscon = 0,
				.irreport = 0,
				.irdepth = 0,
			},
		},
	},
};

static const unsigned char itrace_format30_test1_packet[] = {
	0x0E,			/* header byte */
	0xef,			/* srcID */
	0x43,			/* format 3 and subformat 0 */
	0xff, 0xff, 0xff,	/* Time field */
	0x55, 0x55, 0x55,	/* Context field */
	0x11,0x22,0x33,0x44,0x55,0x66,0x7/** Address field */
};

static struct itrace_test itrace_format30_test1 = {
	.name = "itrace_format30_test1",
	.params = &itrace_params,
	.ref_pkt = itrace_format30_test1_packet,
	.ref_pkt_size = sizeof(itrace_format30_test1_packet),
	.ref_srcid = 0xef,
	.ref_it = {
		.format = 3,
		.format3.subformat = 0,
		.format3.format30 = {
			.branch = 0,
			.privilege = 2,
			.time = 0x1ffffff,
			.context = 0xaaaaa,
			.address = 0x76655443322114,
		},
	},
};

static const unsigned char itrace_format30_test2_packet[] = {
	0x8E,			/* header byte */
	0xef,			/* srcID */
	0x12, 0x34, 0x56, 0x78,	/* tstamp */
	0x53,			/* format 3 and subformat 0 */
	0xff, 0xff, 0xff,	/* Time field */
	0x55, 0x55, 0x55,	/* Context field */
	0x11,0x22,0x33,0x44,0x55,0x66,0x7/** Address field */
};

static struct itrace_test itrace_format30_test2 = {
	.name = "itrace_format30_test2",
	.params = &itrace_params,
	.ref_pkt = itrace_format30_test2_packet,
	.ref_pkt_size = sizeof(itrace_format30_test2_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
		.format = 3,
		.format3.subformat = 0,
		.format3.format30 = {
			.branch = 1,
			.privilege = 2,
			.time = 0x1ffffff,
			.context = 0xaaaaa,
			.address = 0x76655443322114,
		},
	},
};

static const unsigned char itrace_format31_test1_packet[] = {
	0x16,			/* header byte */
	0xef,			/* srcID */
	0x17,			/* format 3 and subformat 1 */
	0xff,0xff, 0xff,
	0xab, 0xab, 0xb,	/* Context field */
	0x02,0x18,
	0x11, 0x11,0x11,0x11,0x11,0x0,	/** Address field */
	0x0,0x0,0x0,0x0,0x0,0x0,0x0	/** tval field (50-bit address) */
};

static struct itrace_test itrace_format31_test1 = {
	.name = "itrace_format31_test1",
	.params = &itrace_params,
	.ref_pkt = itrace_format31_test1_packet,
	.ref_pkt_size = sizeof(itrace_format31_test1_packet),
	.ref_srcid = 0xef,
	.ref_it = {
		.format = 3,
		.format3.subformat = 1,
		.format3.format31 = {
			.branch  = 1,
			.privilege = 0,
			.time = 0x1ffffff,
			.context = 0x5d5d5,
			.ecause = 0x10,
			.interrupt = 0,
			.thaddr = 0,
			.address = 0x1111111111180,
			.tval = 0,
		},
	},
};

static const unsigned char itrace_format31_test2_packet[] = {
	0x96,			/* header byte */
	0xef,			/* srcID */
	0x12, 0x34, 0x56, 0x78,	/* tstamp */
	0x17,			/* format 3 and subformat 1 */
	0xff,0xff, 0xff,
	0xab, 0xab, 0xb,	/* Context field */
	0x02,0x18,
	0x11,0x11, 0x11,0x11,0x11,0x0,	/** Address field */
	0x0,0x0,0x0,0x0,0x0,0x0,0x0 /** tval field (50-bit address) */
};

static struct itrace_test itrace_format31_test2 = {
	.name = "itrace_format31_test2",
	.params = &itrace_params,
	.ref_pkt = itrace_format31_test2_packet,
	.ref_pkt_size = sizeof(itrace_format31_test2_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
		.format = 3,
		.format3.subformat = 1,
		.format3.format31 = {
			.branch  = 1,
			.privilege = 0,
			.time = 0x1ffffff,
			.context = 0x5d5d5,
			.ecause = 0x10,
			.interrupt = 0,
			.thaddr = 0,
			.address = 0x1111111111180,
			.tval = 0,
		},
	},
};

static const unsigned char itrace_format31_test3_packet[] = {
	0x0f,			/* header byte */
	0xef,			/* srcID */
	0x17,			/* format 3 and subformat 1 */
	0xff,0xff, 0xff,
	0xab, 0xab, 0xb,	/* Context field */
	0x0a,0x18,
	0x11, 0x11, 0x11,0x11,0x11,0x0	/** Address field */
};

static struct itrace_test itrace_format31_test3 = {
	.name = "itrace_format31_test3",
	.params = &itrace_params,
	.ref_pkt = itrace_format31_test3_packet,
	.ref_pkt_size = sizeof(itrace_format31_test3_packet),
	.ref_srcid = 0xef,
	.ref_it = {
		.format = 3,
		.format3.subformat = 1,
		.format3.format31 = {
			.branch  = 1,
			.privilege = 0,
			.time = 0x1ffffff,
			.context = 0x5d5d5,
			.ecause = 0x10,
			.interrupt = 1,
			.thaddr = 0,
			.address = 0x1111111111180,
		},
	},
};

static const unsigned char itrace_format31_test4_packet[] = {
	0x8f,			/* header byte */
	0xef,			/* srcID */
	0x12, 0x34, 0x56, 0x78,	/* tstamp */
	0x17,			/* format 3 and subformat 1 */
	0xff,0xff, 0xff,
	0xab, 0xab, 0xb,	/* Context field */
	0x0a,0x18,
	0x11,0x11, 0x11,0x11,0x11,0x0,	/** Address field */
};

static struct itrace_test itrace_format31_test4 = {
	.name = "itrace_format31_test4",
	.params = &itrace_params,
	.ref_pkt = itrace_format31_test4_packet,
	.ref_pkt_size = sizeof(itrace_format31_test4_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
		.format = 3,
		.format3.subformat = 1,
		.format3.format31 = {
			.branch  = 1,
			.privilege = 0,
			.time = 0x1ffffff,
			.context = 0x5d5d5,
			.ecause = 0x10,
			.interrupt = 1,
			.thaddr = 0,
			.address = 0x1111111111180,
		},
	},
};

static const unsigned char itrace_format32_test1_packet[] = {
	0x07,			/* header byte */
	0xef,			/* srcID */
	0xAB,			/* format 3 and subformat 2 */
	0xff, 0xff, 0xff,	/* Time field */
	0xab, 0xab, 0xb,	/* Context field */
};

static struct itrace_test itrace_format32_test1 = {
	.name = "itrace_format32_test1",
	.params = &itrace_params,
	.ref_pkt = itrace_format32_test1_packet,
	.ref_pkt_size = sizeof(itrace_format32_test1_packet),
	.ref_srcid = 0xef,
	.ref_it = {
		.format = 3,
		.format3.subformat = 2,
		.format3.format32 = {
			.privilege = 2,
			.time = 0x1ffffff,
			.context = 0xbabab,
		},
	},
};

static const unsigned char itrace_format32_test2_packet[] = {
	0x87,			/* header byte */
	0xef,			/* srcID */
	0x12, 0x34, 0x56, 0x78,	/* tstamp */
	0xAB,			/* format 3 and subformat 2 */
	0xff, 0xff, 0xff,	/* Time field (if available) */
	0xcd, 0xcd, 0xd,	/* Context field (if available) */
};

static struct itrace_test itrace_format32_test2 = {
	.name = "itrace_format32_test2",
	.params = &itrace_params,
	.ref_pkt = itrace_format32_test2_packet,
	.ref_pkt_size = sizeof(itrace_format32_test2_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
		.format = 3,
		.format3.subformat = 2,
		.format3.format32 = {
			.privilege = 2,
			.time = 0x1ffffff,
			.context = 0xdcdcd,
		},
	},
};

static const unsigned char itrace_format33_test1_packet[] = {
	0x0E,			/* header byte */
	0xef,			/* srcID */
	0x5f,			/* format 33  */
	0x1,
	0x34,0x12,0x33,0x44,
	0x3,
	0x78,0xb0,0x11,0x22,0x33,0x44,0x01
};

static struct itrace_test itrace_format33_test1 = {
	.name = "itrace_format33_test1",
	.params = &itrace_params,
	.ref_pkt = itrace_format33_test1_packet,
	.ref_pkt_size = sizeof(itrace_format33_test1_packet),
	.ref_srcid = 0xef,
	.ref_it = {
		.format = 3,
		.format3.subformat = 3,
		.format3.format33 = {
			.ienable = 0x1,
			.encoder_mode = 0x9891a00a,
			.qual_status = 0x1,
			.ioptions =0x60f00688,
			.denable = 1,
			.dloss = 1,
			.doptions =0xa2199108,
		},
	},
};

static const unsigned char itrace_format33_test2_packet[] = {
	0x8e,			/* header byte */
	0xef,			/* srcID */
	0x12, 0x34, 0x56, 0x78,	/* tstamp */
	0x5f,			/* format 33  */
	0x1,
	0x34,0x12,0x33,0x44,
	0x3,
	0x78,0xb0,0x11,0x22,0x33,0x44,0x01
};

static struct itrace_test itrace_format33_test2 = {
	.name = "itrace_format33_test2",
	.params = &itrace_params,
	.ref_pkt = itrace_format33_test2_packet,
	.ref_pkt_size = sizeof(itrace_format33_test2_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
		.format = 3,
		.format3.subformat = 3,
		.format3.format33 = {
			.ienable = 0x1,
			.encoder_mode = 0x9891a00a,
			.qual_status = 0x1,
			.ioptions =0x60f00688,
			.denable = 1,
			.dloss = 1,
			.doptions =0xa2199108,
		},
	},
};

static const unsigned char itrace_format1_test1_packet[] = {
	0x09,			/* header byte */
	0xef,			/* srcID */
	0x01,			/* format 1 */
	0x11,0x24,0x11,0x44,	/* branch_map depends on branchs */
	0x44,0x44,0x44,0x04,/**Address field  notify ,updiscon,irreport, irdepth */
};

static struct itrace_test itrace_format1_test1 = {
	.name = "itrace_format1_test1",
	.params = &itrace_params,
	.ref_pkt = itrace_format1_test1_packet,
	.ref_pkt_size = sizeof(itrace_format1_test1_packet),
	.ref_srcid = 0xef,
	.ref_it = {
		.format = 1,
		.format1 = {
			.branches = 0x0,
			.branch_map = 0,
			.iaddress = {
				.addr = 0x22222222222222,
				.notify = 0,
				.updiscon = 0,
				.irreport = 0,
				.irdepth = 0,
			},
		},
	},
};

static const unsigned char itrace_format1_test2_packet[] = {
	0x89,			/* header byte */
	0xef,			/* srcID */
	0x12, 0x34, 0x56, 0x78,	/* tstamp */
	0x01,			/* format 1 */
	0x11,0x24,0x11,0x44,	/* branch_map depends on branchs */
	0x44,0x44,0x44,0x04,/**Address field  notify ,updiscon,irreport, irdepth */
};

static struct itrace_test itrace_format1_test2 = {
	.name = "itrace_format1_test2",
	.params = &itrace_params,
	.ref_pkt = itrace_format1_test2_packet,
	.ref_pkt_size = sizeof(itrace_format1_test2_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
		.format = 1,
		.format1 = {
			.branches = 0x0,
			.branch_map = 0,
			.iaddress = {
				.addr = 0x22222222222222,
				.notify = 0,
				.updiscon = 0,
				.irreport = 0,
				.irdepth = 0,
			},
		},
	},
};


static const unsigned char itrace_format00_test1_packet[] = {
    0x0d,           /* header byte */
    0xef,           /* srcID */
    0xf0,           /* format 0 and subformat 0 */
    0x11,0x22,0x33,0x77,  /* branch count (32 bits) */
    0x44,0x33,0x55,0x22, /* Address field (4 bytes) */
    0x0,0x0,0x0,0x0 /* Address fields (notify, updiscon, irreport, irdepth) */
};

static struct itrace_test itrace_format00_test1 = {
    .name = "itrace_format00_test1",
    .params = &itrace_params,
    .ref_pkt = itrace_format00_test1_packet,
    .ref_pkt_size = sizeof(itrace_format00_test1_packet), /** Size of the packet */
    .ref_srcid = 0xef,
    .ref_it = {
	.format = 0,
	.format0.subformat = 0,
       .format0.format00 = {
		.branch_count = 0x7332211f, /**Branch count (0x0, 0x0, 0x0, 0x1) */
		.branch_fmt = 0x3,    /** Branch format (indicates address is present) */
		.iaddress = {
			.addr = 0x0112a99a22,  /**Address from raw packet (0x44, 0x33, 0x55, 0x22)*/
			.notify = 0,
			.updiscon = 0,
			.irreport = 0,
			.irdepth = 0,
		},
        },
    },
};

static const unsigned char itrace_format00_test2_packet[] = {
	0x8d,			/* header byte */
	0xef,			/* srcID */
	0x12,0x34,0x56,0x78,	/* tstamp */
	0xf0,           /* format 0 and subformat 0 */
	 0x11,0x22,0x33,0x77,  /* branch count (32 bits) */
        0x44,0x33,0x55,0x22, /* Address field (4 bytes) */
        0x0,0x0,0x0,0x0 /* Address fields (notify, updiscon, irreport, irdepth) */
};

static struct itrace_test itrace_format00_test2 = {
	.name = "itrace_format00_test2",
	.params = &itrace_params,
	.ref_pkt = itrace_format00_test2_packet,
	.ref_pkt_size = sizeof(itrace_format00_test2_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
        .format = 0,
        .format0.subformat = 0,
	.format0.format00 = {
		.branch_count = 0x7332211f, /**Branch count (0x0, 0x0, 0x0, 0x1) */
		.branch_fmt = 0x3,    /** Branch format (indicates address is present) */
		.iaddress = {
			.addr = 0x0112a99a22,  /**Address from raw packet (0x44, 0x33, 0x55, 0x22)*/
			.notify = 0,
			.updiscon = 0,
			.irreport = 0,
			.irdepth = 0,
		},
        },
    },
};

static const unsigned char itrace_format01_test1_packet[] = {
	0x07,			/* header byte */
	0xef,			/* srcID */
	0xA4,			/* format 0 and subformat 1 */
	0x11,0x00,0x00,
	0x12,0x04,0x0,
};

static struct itrace_test itrace_format01_test1 = {
	.name = "itrace_format01_test1",
	.params = &itrace_params,
	.ref_pkt = itrace_format01_test1_packet,
	.ref_pkt_size = sizeof(itrace_format01_test1_packet),
	.ref_srcid = 0xef,
	.ref_it = {
		.format = 0,			/* Format 0 */
		.format0.subformat = 1,		/* Subformat 1 */
		.format0.format01 = {
			.index = 0xa,
			.branches = 0x11,
			.branch_map =0x20900000,
			.irreport = 0,
			.irdepth = 0x0,
		},
	},
};

static const unsigned char itrace_format01_test2_packet[] = {
	0x87,			/* header byte */
	0xef,			/* srcID */
	0x12,0x34,0x56,0x78,	/* tstamp */
	0xA4,			/* format 0 and subformat 1 */
	0x11,0x00,0x00,
	0x12,0x04,0x0,
};

static struct itrace_test itrace_format01_test2 = {
	.name = "itrace_format01_test2",
	.params = &itrace_params,
	.ref_pkt = itrace_format01_test2_packet,
	.ref_pkt_size = sizeof(itrace_format01_test2_packet),
	.ref_srcid = 0xef,
	.ref_extend = 1,
	.ref_tstamp = 0x78563412,
	.ref_it = {
		.format = 0,			/* Format 0 */
		.format0.subformat = 1,		/* Subformat 1 */
		.format0.format01 = {
			.index = 0xa,
			.branches = 0x11,
			.branch_map =0x20900000,
			.irreport = 0,
			.irdepth = 0x0,
		},
	},
};

int main(int argc, char *argv[])
{
	itrace_do_test(&itrace_format1_test1);
	itrace_do_test(&itrace_format1_test2);
	itrace_do_test(&itrace_format2_test1);
	itrace_do_test(&itrace_format2_test2);
	itrace_do_test(&itrace_format30_test1);
	itrace_do_test(&itrace_format30_test2);
	itrace_do_test(&itrace_format31_test1);
	itrace_do_test(&itrace_format31_test2);
	itrace_do_test(&itrace_format31_test3);
	itrace_do_test(&itrace_format31_test4);
	itrace_do_test(&itrace_format32_test1);
	itrace_do_test(&itrace_format32_test2);
	itrace_do_test(&itrace_format33_test1);
	itrace_do_test(&itrace_format33_test2);
	itrace_do_test(&itrace_format00_test1);
	itrace_do_test(&itrace_format00_test2);
	itrace_do_test(&itrace_format01_test1);
	itrace_do_test(&itrace_format01_test2);
	return 0;
}
