// SPDX-License-Identifier: GPL-2.0
/* Renesas RSwitch2 Ethernet CPU port device driver
 *
 * Copyright (C) 2019-2021 Renesas Electronics Corporation
 *
 */
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/iopoll.h>
#include <linux/etherdevice.h>
#include "rswitch2.h"
#include "rswitch2_fwd.h"

static int rsw2_fwd_find_free_cascade_filter_slot(struct rswitch2_drv *rsw2)
{

	u32 filter_conf;
	uint slot_num;

	for (slot_num = 0; slot_num < RSW2_FWD_THBF_N; slot_num++) {
		filter_conf = ioread32(rsw2->fwd_base_addr + RSW2_FWD_FWCFC(slot_num));

		if (filter_conf == 0) {
			rsw2_info(MSG_FWD, "Cascade filter slot %d is unused\n", slot_num);

			break;
		}
	}

	if (slot_num >= RSW2_FWD_THBF_N) {
		return -EFILTER_LIST_FULL;
	}

	return (int)slot_num;
}


static int rsw2_fwd_find_free_3byte_filter_slot(struct rswitch2_drv *rsw2)
{
	u32 filter_conf;
	u32 filter_val0;
	u32 filter_val1;
	uint slot_num;

	for (slot_num = 0; slot_num < RSW2_FWD_THBF_N; slot_num++) {
		filter_conf = ioread32(rsw2->fwd_base_addr + RSW2_FWD_FWTHBFC(slot_num));
		filter_val0 = ioread32(rsw2->fwd_base_addr + RSW2_FWD_FWTHBFV0C(slot_num));
		filter_val1 = ioread32(rsw2->fwd_base_addr + RSW2_FWD_FWTHBFV1C(slot_num));

		if ((filter_conf == 0) && (filter_val0 == 0) && (filter_val1 == 0)) {
			//printk("3byte filter slot %d is unused\n", slot_num);

			break;
		}
	}

	if (slot_num >= RSW2_FWD_THBF_N)
		return -EFILTER_LIST_FULL;

	return (int)slot_num;
}

static int rsw2_fwd_add_cascade_filter(struct rswitch2_drv *rsw2, struct cascade_filter *filter)
{
	int ret;
	u32 filter_conf;

	uint filter_slot;
	uint filter_map_entry;

	ret = rsw2_fwd_find_free_cascade_filter_slot(rsw2);
	if (ret < 0)
		return ret;

	filter_slot = (uint)ret;
	filter->stream_id = filter_slot;

	filter_conf = FIELD_PREP(FWCFC_CFPFFV, filter->pframe_bitmap);
	filter_conf |= FIELD_PREP(FWCFC_CFEFFV, filter->eframe_bitmap);

	for (filter_map_entry = 0; filter_map_entry < RSW2_FWD_CFMF_N; filter_map_entry++) {
		u32 reg_val;

		reg_val = FIELD_PREP(FWCFMC_CFFV, filter->entry[filter_map_entry].enabled);
		reg_val |= FIELD_PREP(FWCFMC_CFFN, filter->entry[filter_map_entry].id);

		iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWCFMC(filter_slot, filter_map_entry));
	}

	iowrite32(filter_conf, rsw2->fwd_base_addr + RSW2_FWD_FWCFC(filter_slot));

	return ret;
}


static int rsw2_fwd_add_3byte_filter(struct rswitch2_drv *rsw2, struct three_byte_filter *filter) {
	int ret = 0;
	u32 filter_conf;
	u32 filter_val0;
	u32 filter_val1;
	uint filter_slot;

	ret = rsw2_fwd_find_free_3byte_filter_slot(rsw2);
	if (ret < 0)
		return ret;

	filter_slot = (uint)ret;

	filter_conf = FIELD_PREP(FWTHBFC_THBFUM, filter->mode);
	filter_conf |= FIELD_PREP(FWTHBFC_THBFOV, filter->offset);

	switch (filter->mode) {
	case pf_mask_mode:
		filter_val0  = FIELD_PREP(FWTHBFV0C_THBFV0B0, filter->m.val[2]);
		filter_val0 |= FIELD_PREP(FWTHBFV0C_THBFV0B1, filter->m.val[1]);
		filter_val0 |= FIELD_PREP(FWTHBFV0C_THBFV0B2, filter->m.val[0]);

		filter_val1  = FIELD_PREP(FWTHBFV0C_THBFV1B0, filter->m.mask[2]);
		filter_val1 |= FIELD_PREP(FWTHBFV0C_THBFV1B1, filter->m.mask[1]);
		filter_val1 |= FIELD_PREP(FWTHBFV0C_THBFV1B2, filter->m.mask[0]);

		filter->m.id = 2 * (filter_slot + RSW2_FWD_TWBF_N);
		break;

	case pf_expand_mode:
		filter_val0  = FIELD_PREP(FWTHBFV0C_THBFV0B0, filter->e.val[2]);
		rsw2_dbg(MSG_FWD, "filter->e.val[0]: 0x%2x filter_val0: 0x%8x\n", filter->e.val[0], filter_val0);
		filter_val0 |= FIELD_PREP(FWTHBFV0C_THBFV0B1, filter->e.val[1]);
		rsw2_dbg(MSG_FWD, "filter->e.val[1]: 0x%2x filter_val0: 0x%8x\n", filter->e.val[1], filter_val0);
		filter_val0 |= FIELD_PREP(FWTHBFV0C_THBFV0B2, filter->e.val[0]);
		rsw2_dbg(MSG_FWD, "filter->e.val[2]: 0x%2x filter_val0: 0x%8x\n", filter->e.val[2], filter_val0);
		filter_val1  = FIELD_PREP(FWTHBFV0C_THBFV1B0, filter->e.val[5]);
		rsw2_dbg(MSG_FWD, "filter->e.val[3]: 0x%2x filter_val1: 0x%8x\n", filter->e.val[3], filter_val1);
		filter_val1 |= FIELD_PREP(FWTHBFV0C_THBFV1B1, filter->e.val[4]);
		rsw2_dbg(MSG_FWD, "filter->e.val[4]: 0x%2x filter_val1: 0x%8x\n", filter->e.val[4], filter_val1);
		filter_val1 |= FIELD_PREP(FWTHBFV0C_THBFV1B2, filter->e.val[3]);
		rsw2_dbg(MSG_FWD, "filter->e.val[5]: 0x%2x filter_val1: 0x%8x\n", filter->e.val[5], filter_val1);

		filter->e.id = 2 * (filter_slot + RSW2_FWD_TWBF_N);
		break;

	case pf_precise_mode:
		filter_val0  = FIELD_PREP(FWTHBFV0C_THBFV0B0, filter->p.val[2]);
		filter_val0 |= FIELD_PREP(FWTHBFV0C_THBFV0B1, filter->p.val[1]);
		filter_val0 |= FIELD_PREP(FWTHBFV0C_THBFV0B2, filter->p.val[0]);

		filter_val1  = FIELD_PREP(FWTHBFV0C_THBFV1B0, filter->p.val2[2]);
		filter_val1 |= FIELD_PREP(FWTHBFV0C_THBFV1B1, filter->p.val2[1]);
		filter_val1 |= FIELD_PREP(FWTHBFV0C_THBFV1B2, filter->p.val2[0]);

		filter->p.id = 2 * (filter_slot + RSW2_FWD_TWBF_N);
		filter->p.id2 = filter->p.id + 1;
		break;
	}

	iowrite32(filter_val0, rsw2->fwd_base_addr + RSW2_FWD_FWTHBFV0C(filter_slot));
	iowrite32(filter_val1, rsw2->fwd_base_addr + RSW2_FWD_FWTHBFV1C(filter_slot));
	iowrite32(filter_conf, rsw2->fwd_base_addr + RSW2_FWD_FWTHBFC(filter_slot));

	return ret;
}

static int rsw2_fwd_add_l3_entry(struct rswitch2_drv *rsw2, u32 stream_id, u32 src_port_vec, u32 dest_port_vec, u32 cpu_q)
{
	int ret;
	u32 reg_val;

	/* FIXME: FIELD_PREP */
	reg_val = 0;

	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTL0);

	/* StreamID upper part - we only use lowest 32bit */
	iowrite32(0, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTL1);
	iowrite32(0, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTL2);
	iowrite32(0, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTL3);
	iowrite32(stream_id, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTL4);

	reg_val = FIELD_PREP(FWLTHTL7_LTHSLVL, src_port_vec);

// 	reg_val |= FIELD_PREP(FWLTHTL7_LTHRNL, 47 /*- FIXME: Rule number */);
//	reg_val |= FIELD_PREP(FWLTHTL7_LTHRVL, 1);

	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTL7);

	reg_val = FIELD_PREP(FWLTHTL8_LTHCSDL, cpu_q);
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTL8(0));

	reg_val = FIELD_PREP(FWLTHTL9_LTHDVL, dest_port_vec);
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTL9);


	/* Wait for learning result */
	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWLTHTIM, reg_val,
						FIELD_GET(FWLTHTIM_LTHTR, reg_val),
						RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		pr_err("L3 table learning timed out\n");
		return ret;
	}

	reg_val = ioread32(rsw2->fwd_base_addr + RSW2_FWD_FWLTHTLR);
	if (FIELD_GET(FWLTHTLR_LTHLF, reg_val)) {
		/* FIXME: Check others error bits */
		rsw2_err(MSG_FWD, "Learning failed\n");
	} else {
		rsw2_info(MSG_FWD, "Learning succeeded\n");
	}
	return ret;
}

int rsw2_fwd_add_l2_entry(struct rswitch2_drv *rsw2, const u8 *macaddr, u32 src_port_vec, u32 dest_port_vec, u32 cpu_q)
{
	int ret;
	u32 reg_val;

	iowrite32(0, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL0);

	reg_val = ((macaddr[0] & 0xff) << 8) | macaddr[1];
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL1);

	reg_val = ((macaddr[2] & 0xff) << 24);
	reg_val |= ((macaddr[3] & 0xff) << 16);
	reg_val |= ((macaddr[4] & 0xff) << 8) | macaddr[5];
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL2);

	/* Lock vector learn on all ports */
	reg_val = FIELD_PREP(FWMACTL3_MACDSLVL, dest_port_vec);
	reg_val |= FIELD_PREP(FWMACTL3_MACSSLVL, src_port_vec);
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL3);

	reg_val = FIELD_PREP(FWMACTL4_MACCSDL, cpu_q);

	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL4(0));

	/* CPU port */
	reg_val = FIELD_PREP(FWMACTL5_MACDVL, (1 << rsw2->num_of_tsn_ports));

	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL5);

	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWMACTLR, reg_val,
							(FIELD_GET(FWMACTLR_MACTL, reg_val) == 0),
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_FWD, "MAC Table Learn timed out\n");
		return ret;
	} else {
		rsw2_info(MSG_FWD, "MAC Table entry learned: 0x%.8x\n", reg_val);
	}

	return 0;
}

int rsw2_fwd_del_l2_entry(struct rswitch2_drv *rsw2, const u8 *macaddr)
{
	int ret;
	u32 reg_val;

	reg_val = FIELD_PREP(FWMACTL0_MACED, 1);
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL0);

	reg_val = ((macaddr[0] & 0xff) << 8) | macaddr[1];
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL1);

	reg_val = ((macaddr[2] & 0xff) << 24);
	reg_val |= ((macaddr[3] & 0xff) << 16);
	reg_val |= ((macaddr[4] & 0xff) << 8) | macaddr[5];
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL2);

	iowrite32(0, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL5);

	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWMACTLR, reg_val,
							(FIELD_GET(FWMACTLR_MACTL, reg_val) == 0),
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_FWD, "MAC Table delete timed out\n");
		return ret;
	} else {
		rsw2_info(MSG_FWD, "MAC Table entry deleted: 0x%.8x\n", reg_val);
	}

	return 0;
}

int rswitch2_fwd_init(struct rswitch2_drv *rsw2)
{
	int cur_port;
	int num_of_ports = (rsw2->num_of_cpu_ports + rsw2->num_of_tsn_ports);
	u32 reg_val;
	int ret;

	/* Simple static forward configuration to allow access to internal port */

	/* Enable access for both APBs */
	/* TODO: Needs to be checked on S4 */
	iowrite32(0xFFFFFFFF, rsw2->fwd_base_addr + RSW2_FWD_FWSCR27);
	iowrite32(0xFFFFFFFF, rsw2->fwd_base_addr + RSW2_FWD_FWSCR28);
	iowrite32(0xFFFFFFFF, rsw2->fwd_base_addr + RSW2_FWD_FWSCR29);
	iowrite32(0xFFFFFFFF, rsw2->fwd_base_addr + RSW2_FWD_FWSCR30);
	iowrite32(0xFFFFFFFF, rsw2->fwd_base_addr + RSW2_FWD_FWSCR31);
	iowrite32(0xFFFFFFFF, rsw2->fwd_base_addr + RSW2_FWD_FWSCR32);
	iowrite32(0xFFFFFFFF, rsw2->fwd_base_addr + RSW2_FWD_FWSCR33);
	iowrite32(0xFFFFFFFF, rsw2->fwd_base_addr + RSW2_FWD_FWSCR34);

	/* Enable IPv4 traffic. Configure MAC handling */
	reg_val = FWPC0_LTHTA | FWPC0_IP4UE | FWPC0_IP4TE /*| FWPC0_IP4OE*/;
	reg_val |= FWPC0_MACDSA | FWPC0_MACSSA | FWPC0_MACHLA | FWPC0_MACHMA;

	// FIXME: check if needed
	reg_val |= FWPC0_L2SE;

	for (cur_port = 0; cur_port < num_of_ports; cur_port++) {
		iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWPC0(cur_port));
	}

	/* Specification seems to be wrong */
#if 0
	for (cur_port = 0; cur_port < num_of_ports; cur_port++) {

		reg_val = ioread32(rsw2->fwd_base_addr + RSW2_FWD_FWPC1(i));
		printk("REG: 0x%.8x\n", rsw2->fwd_base_addr + RSW2_FWD_FWPC1(i));
		printk("READ RSW2_FWD_FWPC1(%d): 0x%.8x\n", i, reg_val);
		reg_val |= FIELD_PREP(FWPC1_DDE, 1);
		reg_val |= FIELD_PREP(FWPC1_DDSL, 1);
		printk("WRITE: RSW2_FWD_FWPC1(%d): 0x%.8x\n", i, reg_val);
		iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWPC1(i));
	}
#endif

	reg_val = ioread32(rsw2->fwd_base_addr + RSW2_FWD_FWPC1(rsw2->num_of_tsn_ports));
	reg_val |= FIELD_PREP(FWPC1_DDE, 1);
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWPC1(rsw2->num_of_tsn_ports));

	reg_val = ioread32(rsw2->fwd_base_addr + RSW2_FWD_FWMACHEC);
	reg_val |= FIELD_PREP(FWMACHEC_MACHMUE, (RSWITCH2_FWD_MAX_HASH_ENTRIES - 1));
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACHEC);

	/* Initialize MAC table learning */

	/* Wait for MAC learning to complete */
	iowrite32(FWMACTIM_MACTIOG, rsw2->fwd_base_addr + RSW2_FWD_FWMACTIM);

	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWMACTIM, reg_val,
						FIELD_GET(FWMACTIM_MACTR, reg_val),
						RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_FWD, "Initialization of MAC table timed out\n");
		return ret;
	}

#if 0
	/* Static entry for GWCA port */
	reg_val = ((rsw2_own_multicast_mac[0] & 0xff) << 8) | rsw2_own_multicast_mac[1];
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL1);

	reg_val = ((rsw2_own_multicast_mac[2] & 0xff) << 24);
	reg_val |= ((rsw2_own_multicast_mac[3] & 0xff) << 16);
	reg_val |= ((rsw2_own_multicast_mac[4] & 0xff) << 8) | rsw2_own_multicast_mac[5];
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL2);

	/* Lock vector learn on all ports */
	reg_val = FIELD_PREP(FWMACTL3_MACDSLVL, ((1 << num_of_ports) - 1));
	reg_val |= FIELD_PREP(FWMACTL3_MACSSLVL, ((1 << num_of_ports) - 1));
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL3);

	/* TODO: PTP queue number might need to be adjusted, depending on GWCA implementation */
	reg_val = FIELD_PREP(FWMACTL4_MACCSDL, 24);

	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL4(0));

	reg_val = FIELD_PREP(FWMACTL5_MACDVL, (1 << rsw2->num_of_tsn_ports));

	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACTL5);

	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWMACTLR, reg_val,
							(FIELD_GET(FWMACTLR_MACTL, reg_val) == 0),
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		pr_err("MAC Table Learn timed out\n");
		return ret;
	} else {
		pr_err("MAC Table entry learned: 0x%.8x\n", reg_val);
	}

	ret = rsw2_fwd_add_l2_entry(rsw2, rsw2_own_multicast_mac,  ((1 << num_of_ports) - 1),  ((1 << num_of_ports) - 1), 24);
	if (ret != 0) {
		pr_err("MAC Table Learn timed out\n");
		return ret;
	} else {
		pr_err("MAC Table entry learned: 0x%.8x\n", reg_val);
	}
#endif

	/* Configure aging */
	reg_val = FIELD_PREP(FWMACAGUSP_MACAGUSP, 0x4E);
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACAGUSP);

	reg_val = FIELD_PREP(FWMACAGC_MACAGT, RSWITCH2_MAC_AGING_TIME);
	reg_val |= FWMACAGC_MACAGE | FWMACAGC_MACAGSL;
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWMACAGC);

	reg_val = FIELD_PREP(FWVLANTEC_VLANTMUE, (RSWITCH2_MAX_VLAN_ENTRIES - 1));
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWVLANTEC);

	iowrite32(FWVLANTIM_VLANTIOG, rsw2->fwd_base_addr + RSW2_FWD_FWVLANTIM);

	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWVLANTIM, reg_val,
							FIELD_GET(FWVLANTIM_VLANTR, reg_val),
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_FWD, "Initialization of VLAN table timed out\n");
		return ret;
	}

	/* Configure port forwarding */
	for (cur_port = 0; cur_port < num_of_ports; cur_port++) {
		reg_val = FIELD_PREP(FWPBFC_PBDV, ((~(1 << cur_port)) & ((1 << num_of_ports) - 1)));
		iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWPBFC(cur_port));

		//reg_val = cur_port + 3;
		reg_val = 24;
		iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWPBFCSDC(0, cur_port));
	}

	iowrite32(0, rsw2->fwd_base_addr + RSW2_FWD_FWIP4SC);

	/* Set max. hash entries and collisions */
	reg_val = FIELD_PREP(FWLTHHEC_LTHHMUE, 0x7FF);
	reg_val |= FIELD_PREP(FWLTHHEC_LTHHMC, 0x3FF);
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWLTHHEC);

	/* Initialize L3 table */
	iowrite32(FWLTHTIM_LTHTIOG, rsw2->fwd_base_addr + RSW2_FWD_FWLTHTIM);

	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWLTHTIM, reg_val,
						FIELD_GET(FWLTHTIM_LTHTR, reg_val),
						RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_FWD, "Initialization of L3 table timed out\n");
		return ret;
	}

	/* FIXME */
	{
		int i;
		int lret;
		struct three_byte_filter filter;
		struct cascade_filter cc_filter[3 /* num of phy. ports */];

		filter.mode = pf_expand_mode;
		filter.offset = 0;

		/* Set PTP multicast address */
		if (!mac_pton("01:80:C2:00:00:0E", filter.e.val))
			printk("mac_pton() failed\n");

		lret = rsw2_fwd_add_3byte_filter(rsw2, &filter);
		//printk("lret = %d filter.e.id = %d\n", lret, filter.e.id);

		memset(cc_filter, 0 , sizeof(cc_filter));

		for (i = 0; i < 3; i++) {
			set_bit(i, &cc_filter[i].eframe_bitmap);
			set_bit(i, &cc_filter[i].pframe_bitmap);


			cc_filter[i].entry[0].id = filter.e.id;
			cc_filter[i].entry[0].enabled = true;

			rsw2_fwd_add_cascade_filter(rsw2, &cc_filter[i]);

			rsw2_fwd_add_l3_entry(rsw2, cc_filter[i].stream_id, cc_filter[i].eframe_bitmap, 0x8 /* CPU only */, /*25*/ 8 + i);
		}
	}

	return 0;
}

void rswitch2_fwd_exit(struct rswitch2_drv *rsw2)
{
	int ret;
	u32 reg_val;

	/* Wait for MAC learning to complete */
	iowrite32(FWMACTIM_MACTIOG, rsw2->fwd_base_addr + RSW2_FWD_FWMACTIM);

	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWMACTIM, reg_val,
			reg_val & FWMACTIM_MACTR,
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0)
		rsw2_err(MSG_FWD, "Initialization of MAC table timed out\n");

	reg_val = FWVLANTIM_VLANTIOG;
	iowrite32(reg_val, rsw2->fwd_base_addr + RSW2_FWD_FWVLANTIM);

	ret = readl_poll_timeout(rsw2->fwd_base_addr + RSW2_FWD_FWVLANTIM, reg_val,
							reg_val & FWVLANTIM_VLANTR,
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0)
		rsw2_err(MSG_FWD, "Initialization of VLAN table timed out\n");

}
