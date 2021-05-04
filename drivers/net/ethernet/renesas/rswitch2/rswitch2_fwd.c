// SPDX-License-Identifier: GPL-2.0-only
/* Renesas R-SWITCH2 Switch Driver
 *
 * Provide APIs to Forwarding Engine  of Switch Driver
 *
 * Copyright (C) 2020 Renesas Electronics Corporation
 * Author: Asad Kamal
 */

#include <linux/cache.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/fs.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/net_tstamp.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/pci.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/renesas_rswitch2.h>
#include <linux/renesas_rswitch2_platform.h>
#include <linux/renesas_vc2_platform.h>
#include <linux/semaphore.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "rswitch2_eth.h"
#include "rswitch2_fwd.h"
#include "rswitch2_reg.h"
#define L2_MAC_FWD
#define VLAN_FWD
#define IP_TBL_FWD
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
#define RSWITCH2_FWD_PROC_FILE_ERRORS		"fwderrors-all"
#define RSWITCH2_FWD_PROC_FILE_ERRORS_SHORT	"fwderrors"
#define RSWITCH2_FWD_PROC_FILE_COUNTERS		"fwdcounters"
#define RSWITCH2_FWD_PROC_FILE_FILTERS		"filter"
#define RSWITCH2_FWD_PROC_FILE_L3		"L3"
#define RSWITCH2_FWD_PROC_FILE_L2		"l2"
#define RSWITCH2_FWD_PROC_FILE_VLAN		"vlan"
#define RSWITCH2_FWD_PROC_FILE_L2_L3_UPDATE	"updateL2L3"
#define RSWITCH2_FWD_PROC_FILE_BUFFER		"buffer"
#define	RSWITCH2_FWD_PROC_FILE_IP		"ip"
#endif
/* Global Variables */
static struct rswitch2_fwd_config rswitch2fwd_config;
static struct ethports_config board_config;
/* Static config (passed from user) */
static struct rswitch2_fwd_config rswitch2fwd_confignew;
static struct rswitch2_fwd_filter_config filter_tbl_config;
static int vlan_tbl_reconfig;
static int mac_tbl_reconfig;
static int l3_tbl_reconfig;
static int l23_tbl_reconfig;
static int ip_tbl_reconfig;
struct rswitch2_l2_fwd_config l2_fwd_config;
struct rswitch2_ipv_fwd_config ipv_fwd_config;
struct rswitch2_ipv4_stream_fwd_config ipv4_stream_config;
struct rswitch2_l3_stream_fwd l3_stream_fwd;
static int rswitch2fwd_devmajor;
module_param(rswitch2fwd_devmajor, int, 0440);
static struct class *rswitch2fwd_devclass;
static dev_t rswitch2fwd_dev;
static struct cdev rswitch2fwd_cdev;
static struct device rswitch2fwd_device;
extern struct net_device **ppndev;  /* for the interface names */
static struct rswitch2_fwd_counter counter;
static struct rswitch2_fwd_error errors;
#define RSWITCH2_FWD_CTRL_MINOR (127)
#ifdef L2_MAC_FWD

static int rswitch2_l2_mac_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;

	iowrite32(1 << 0, ioaddr + FWMACTIM);
#ifdef DEBUG
	pr_info("Register address= %x Write value = %x\n", FWMACTIM, 1 << 0);
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWMACTIM);
		if ((value >> 1) == 0x01)
			return 0;
		mdelay(1);
	}

	pr_err("mac Reset failed");

	return -1;
}
#endif

#ifdef IP_TBL_FWD
static int rswitch2_ip_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;

	iowrite32(1 << 0, ioaddr + FWIPTIM);
#ifdef DEBUG
	pr_info("Register address= %x Write value = %x\n", FWIPTIM, 1 << 0);
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWIPTIM);
		if ((value >> 1) == 0x01)
			return 0;
		mdelay(1);
	}

	pr_err("ip Reset failed");

	return -1;
}
#endif

#ifdef VLAN_FWD
static int rswitch2_l2_vlan_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;

	iowrite32(1 << 0, ioaddr + FWVLANTIM);
#ifdef DEBUG
	pr_info("Register address= %x Write value = %x\n", FWVLANTIM, 1 << 0);
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWVLANTIM);
		if ((value >> 1) == 0x01)
			return 0;
		mdelay(1);
	}

	pr_err("vlan Reset failed");

	return -1;
}
#endif

static ssize_t rswitch2_fwd_l2_clear(struct file *filp, const char *buff,
				     size_t len, loff_t *off)
{
	u32 ret = 0;
	u64 l2_tbl_clear = 0;

	ret = kstrtoull_from_user(buff, len, 10, &l2_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}

	rswitch2_l2_mac_tbl_reset();

	return len;
}

static ssize_t rswitch2_fwd_l2_vlan_clear(struct file *filp, const char *buff,
					  size_t len, loff_t *off)
{
	u32 ret = 0;
	u64 l2_tbl_clear = 0;

	ret = kstrtoull_from_user(buff, len, 10, &l2_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}

	rswitch2_l2_vlan_tbl_reset();

	return len;
}

static ssize_t rswitch2_fwd_ip_clear(struct file *filp, const char *buff,
				     size_t len, loff_t *off)
{
	u32 ret = 0;
	u64 ip_tbl_clear = 0;

	ret = kstrtoull_from_user(buff, len, 10, &ip_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}

	rswitch2_ip_tbl_reset();

	return len;
}

static int rswitch2_l23_update_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;

	iowrite32(1 << 0, ioaddr + FWL23UTIM);
#ifdef DEBUG
	pr_info("Register address= %x Write value = %x\n", FWL23UTIM, 1 << 0);
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWL23UTIM);
		if ((value >> 1) == 0x01)
			return 0;
		mdelay(1);
	}

	pr_err("L2-3 Update Table Reset failed");

	return -1;
}

static ssize_t rswitch2_fwd_l2_l3_update_clear(struct file *filp,
					       const char *buff, size_t len,
					       loff_t *off)
{
	u32 ret = 0;
	u64 l2l3_update_tbl_clear = 0;

	ret = kstrtoull_from_user(buff, len, 10, &l2l3_update_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}

	rswitch2_l23_update_tbl_reset();

	return len;
}

static int rswitch2_l3_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;

	iowrite32(1 << 0, ioaddr + FWLTHTIM);
#ifdef DEBUG
	pr_info("Register address= %x Write value = %x\n", FWLTHTIM, 1 << 0);
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWLTHTIM);
		if ((value >> 1) == 0x01)
			return 0;
		mdelay(1);
	}

	pr_err("L3 Table Reset failed");

	return -1;
}

static ssize_t rswitch2_fwd_l3_clear(struct file *filp, const char *buff,
				     size_t len, loff_t *off)
{
	u32 ret = 0;
	u64 l3_tbl_clear = 0;

	ret = kstrtoull_from_user(buff, len, 10, &l3_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}

	rswitch2_l3_tbl_reset();

	return len;
}

#ifdef IP_TBL_FWD
int rswitch2_ipv_learn_status(void)
{
	int i = 0;
	int value = 0;

	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWIPTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 2) & 0x01) || ((value >> 1) & 0x01) ||
			    ((value >> 0) & 0x01)) {
				pr_err("ip learn fail value = %x\n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}

	pr_err("ip entry Learn Timeout\n");

	return -1;
}
#endif

#ifdef L2_MAC_FWD
int rswitch2_mac_learn_status(void)
{
	int i = 0;
	int value = 0;

	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWMACTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 2) & 0x01) || ((value >> 1) & 0x01) ||
			    ((value >> 0) & 0x01)) {
				pr_err("mac learn fail value = %x\n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}

	pr_err("mac entry Learn Timeout\n");

	return -1;
}
#endif

#ifdef VLAN_FWD
int rswitch2_vlan_learn_status(void)
{
	int i = 0;
	int value = 0;

	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWVLANTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 2) & 0x01) || ((value >> 1) & 0x01) ||
			    ((value >> 0) & 0x01)) {
				pr_err("vlan learn fail value = %x\n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}

	pr_err("vlan entry Learn Timeout\n");

	return -1;
}
#endif

int rswitch2_l23_update_learn_status(void)
{
	int i = 0;
	int value = 0;

	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWL23URLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 1) & 0x01) || ((value >> 0) & 0x01)) {
				pr_err("L23  Update learn fail value = %x\n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}

	pr_err("L23 Update entry Learn Timeout\n");

	return -1;
}

int rswitch2_l3_learn_status(void)
{
	int i = 0;
	int value = 0;

	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWLTHTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 2) & 0x01) || ((value >> 1) & 0x01) ||
			    ((value >> 0) & 0x01)) {
				pr_err("L3 learn fail value = %x\n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}

	pr_err("L3 entry Learn Timeout\n");

	return -1;
}

int rswitch2_mcast_tbl_learn_status(void)
{
	int i = 0;
	int value = 0;
	void __iomem *gwca_addr =  NULL;

	gwca_addr = ioaddr + board_config.gwca0.start;
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(gwca_addr + GWMSTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 1) & 0x01) || ((value >> 0) & 0x01)) {
				pr_err("MCAST Table learn fail value = %x\n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}

	pr_err("MCAST Table entry Learn Timeout\n");

	return -1;
}

static struct rswitch2_ipv4_stream_id
rswitch2_gen_ipv4_stream(u32 hash_value,
			 struct rswitch2_ipv4_stream_fwd_entry *entry,
			 struct rswitch2_ipv4_stream_fwd_config *config)
{
	static struct rswitch2_ipv4_stream_id stream_id;

	if ((config->stag_dei) || (config->stag_pcp) || (config->stag_vlan)) {
		stream_id.stag = (entry->stag_pcp << 13) |
				 (entry->stag_dei << 12) | (entry->stag_vlan);
	} else {
		stream_id.stag = 0;
	}

	if ((config->ctag_dei) || (config->ctag_pcp) || (config->ctag_vlan)) {
		stream_id.ctag = (entry->ctag_pcp << 13) |
				 (entry->ctag_dei << 12) | (entry->ctag_vlan);
	} else {
		stream_id.ctag = 0;
	}

	if (config->destination_port)
		stream_id.destination_port = entry->destination_port;
	else
		stream_id.destination_port = 0;

	stream_id.hash_value = hash_value;
	if (config->source_ip) {
		stream_id.source_ip = (entry->source_ip_addr[0] << 24) |
				      (entry->source_ip_addr[1] << 16) |
				      (entry->source_ip_addr[2] << 8) |
				      (entry->source_ip_addr[3] << 0);
	} else {
		stream_id.source_ip = 0;
	}
	if (config->destination_ip) {
		stream_id.destination_ip = (entry->destination_ip_addr[0] << 24) |
					   (entry->destination_ip_addr[1] << 16) |
					   (entry->destination_ip_addr[2] << 8) |
					   (entry->destination_ip_addr[3] << 0);
	} else {
		stream_id.destination_ip = 0;
	}
	stream_id.frame_fmt = entry->ipv4_stream_frame_format_code;

	return stream_id;
}

static int
rswitch2_ipv4_stream_gen_config(
			struct rswitch2_ipv4_stream_fwd_config *streamconfig,
			struct rswitch2_ipv4_hash_configuration *hashconfig)
{
	u32 fwip4sc_val = 0;

	fwip4sc_val = (streamconfig->destination_port << 24) |
		      (streamconfig->destination_ip << 23) |
		      (streamconfig->source_ip << 22) |
		      (streamconfig->ctag_dei << 21) |
		      (streamconfig->ctag_pcp << 20) |
		      (streamconfig->ctag_vlan << 19) |
		      (streamconfig->stag_dei << 18) |
		      (streamconfig->stag_pcp << 17) |
		      (streamconfig->stag_vlan << 16) |
		      (hashconfig->destination_port << 12) |
		      (hashconfig->source_port << 11) |
		      (hashconfig->destination_ip << 9) |
		      (hashconfig->source_ip << 8) |
		      (hashconfig->protocol << 10) |
		      (hashconfig->ctag_dei << 7) |
		      (hashconfig->ctag_pcp << 6) |
		      (hashconfig->ctag_vlan << 5) |
		      (hashconfig->stag_dei << 4) |
		      (hashconfig->stag_pcp << 3) |
		      (hashconfig->stag_vlan << 2) |
		      (hashconfig->dest_mac << 0) |
		      (hashconfig->src_mac << 1);
	iowrite32(fwip4sc_val, ioaddr + FWIP4SC);
#ifdef DEBUG
	pr_err("Register address= %x Write value = %x\n", FWIP4SC, fwip4sc_val);
#endif
	return 0;
}

static int
rswitch2_calc_ipv4_hash(struct rswitch2_ipv4_hash_configuration *config,
			struct rswitch2_ipv4_stream_fwd_entry *entry)
{
	u32 value = 0;
	u32 src_ip = 0;
	u32 dest_ip = 0;
	u32 i = 0;
	u32 fwshcr1_val = 0;
	u32 fwshcr2_val = 0;
	u32 fwshcr3_val = 0;
	u32 fwshcr13_val = 0;

	if (config->dest_mac) {
		iowrite32((entry->dest_mac[3] << 24) |
			  (entry->dest_mac[2] << 16) |
			  (entry->dest_mac[1] << 8) |
			  (entry->dest_mac[0] << 0), ioaddr + FWSHCR0);
#ifdef DEBUG
		pr_err("Register address= %x Write value = %x\n", FWSHCR0,
		       (entry->dest_mac[3] << 24) | (entry->dest_mac[2] << 16) |
		       (entry->dest_mac[1] << 8) | (entry->dest_mac[0] << 0));
#endif
		fwshcr1_val = (entry->dest_mac[5] << 24) |
			      (entry->dest_mac[4] << 16);
		if (!config->src_mac) {
			iowrite32(fwshcr1_val, ioaddr + FWSHCR1);
#ifdef DEBUG
			pr_info("Register address= %x Write value = %x\n", FWSHCR1,
			       fwshcr1_val);
#endif
		}
	}
	if (config->src_mac) {
		fwshcr1_val |= (entry->src_mac[1] << 8) |
			       (entry->src_mac[0] << 0);
		iowrite32(fwshcr1_val, ioaddr + FWSHCR1);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWSHCR1,
			fwshcr1_val);
#endif
		fwshcr2_val = (entry->src_mac[5] << 24) |
			      (entry->src_mac[4] << 16) |
			      (entry->src_mac[3] << 8) |
			      (entry->src_mac[2] << 0);
		iowrite32(fwshcr2_val, ioaddr + FWSHCR2);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWSHCR2,
			fwshcr2_val);
#endif
	}

	if ((config->ctag_dei) || (config->stag_dei) || (config->ctag_pcp) ||
	    (config->stag_pcp) || (config->ctag_vlan) || (config->stag_vlan)) {
		fwshcr3_val = (entry->stag_pcp << 29) |
			      (entry->stag_dei << 28) |
			      (entry->stag_vlan << 16) |
			      (entry->ctag_pcp << 13) |
			      (entry->ctag_dei << 12) |
			      (entry->ctag_vlan);
		iowrite32(fwshcr3_val, ioaddr + FWSHCR3);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWSHCR3,
			fwshcr3_val);
#endif
	}
	iowrite32(0x0, ioaddr + FWSHCR4);

	if (config->source_ip) {
		src_ip = (entry->source_ip_addr[0] << 24) |
			 (entry->source_ip_addr[1] << 16) |
			 (entry->source_ip_addr[2] << 8) |
			 (entry->source_ip_addr[3] << 0);
		iowrite32(src_ip, ioaddr + FWSHCR5);
	}
	if (config->destination_ip) {
		dest_ip = (entry->destination_ip_addr[0] << 24) |
			  (entry->destination_ip_addr[1] << 16) |
			  (entry->destination_ip_addr[2] << 8) |
			  (entry->destination_ip_addr[3] << 0);
		iowrite32(dest_ip, ioaddr + FWSHCR9);
	}
	if ((config->destination_port) || (config->source_port)) {
		fwshcr13_val = entry->destination_port |
			       (entry->source_port << 16);
		iowrite32(fwshcr13_val, ioaddr + FWSHCR13);
	} else {
		iowrite32(fwshcr13_val, ioaddr + FWSHCR13);
	}
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWSHCRR);
		if (((value >> 31) & 0x01) == 0x0)
			return (value & 0xFFFF);
		mdelay(1);
	}

	pr_err("hash calculation Timeout\n");

	return -1;
}

static int rswitch2_ipv4_l3_tbl_config(struct rswitch2_l3_stream_fwd *config)
{
	u32 fwlthtl0_val = 0;
	u32 fwlthtl1_val = 0;
	u32 fwlthtl2_val = 0;
	u32 fwlthtl3_val = 0;
	u32 fwlthtl4_val = 0;
	u32 fwlthtl7_val = 0;
	u32 fwlthtl9_val = 0;
	u32 port_lock = 0;
	u32 entries = 0;
	u32 ds_dpv = 0;
	u32 dest_ports = 0;
	int ret = 0;
	u32 dpv = 0;
	struct rswitch2_ipv4_stream_fwd_config *stream_config;
	struct rswitch2_ipv4_stream_id *stream_id;
	struct rswitch2_ipv4_stream_fwd_entry *fwd_entry;

	stream_config = &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config;
	stream_id = kmalloc(sizeof(struct rswitch2_ipv4_stream_id), GFP_KERNEL);

	for (entries = 0; entries < stream_config->ipv4_stream_fwd_entries;
	     entries++) {
		dpv = 0;
		ds_dpv = 0;
		fwd_entry = &stream_config->ipv4_stream_fwd_entry[entries];
		if (!fwd_entry->filter_enable) {
			memcpy(stream_id, &fwd_entry->stream_id,
			       sizeof(struct rswitch2_ipv4_stream_id));
			fwlthtl0_val = (fwd_entry->security_learn << 8) |
				       stream_id->frame_fmt;
			fwlthtl1_val = (stream_id->stag << 16) | stream_id->ctag;
			fwlthtl2_val = (stream_id->destination_port << 16) |
				       stream_id->hash_value;
			fwlthtl3_val = stream_id->source_ip;
			fwlthtl4_val = stream_id->destination_ip;
		} else {
			fwlthtl0_val = fwd_entry->security_learn << 8;
			fwlthtl4_val = fwd_entry->filter_number;
		}
		iowrite32(fwlthtl0_val, ioaddr + FWLTHTL0);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWLTHTL0,
			fwlthtl0_val);
#endif
		iowrite32(fwlthtl1_val, ioaddr + FWLTHTL1);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWLTHTL1,
			fwlthtl1_val);
#endif
		iowrite32(fwlthtl2_val, ioaddr + FWLTHTL2);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWLTHTL2,
			fwlthtl2_val);
#endif
		iowrite32(fwlthtl3_val, ioaddr + FWLTHTL3);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWLTHTL3,
			fwlthtl3_val);
#endif
		iowrite32(fwlthtl4_val, ioaddr + FWLTHTL4);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWLTHTL4,
			fwlthtl4_val);
#endif
		for (port_lock = 0;
		     port_lock < fwd_entry->source_lock.source_ports;
		     port_lock++) {
			ds_dpv |= 1 << fwd_entry->source_lock.source_port_number[port_lock];
		}
		ds_dpv |=  fwd_entry->source_lock.cpu << board_config.eth_ports;
		fwlthtl7_val = (ds_dpv << 16) |
			       (fwd_entry->routing_valid << 15) |
			       (fwd_entry->routing_number << 0);
		iowrite32(fwlthtl7_val, ioaddr + FWLTHTL7);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWLTHTL7,
			fwlthtl7_val);
#endif
		iowrite32(fwd_entry->csdn,
			  ioaddr + FWLTHTL80);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWLTHTL80,
			fwd_entry->csdn);
#endif
		for (dest_ports = 0;
		     dest_ports < fwd_entry->destination_vector_config.dest_eth_ports;
		     dest_ports++) {
			dpv |= 1 << fwd_entry->destination_vector_config.port_number[dest_ports];
		}
		dpv |= fwd_entry->destination_vector_config.cpu << board_config.eth_ports;
		fwlthtl9_val = (fwd_entry->mirroring_config.cpu_mirror_enable << 21) |
			       (fwd_entry->mirroring_config.eth_mirror_enable << 20) |
			       (fwd_entry->mirroring_config.ipv_config.ipv_update_enable  << 19) |
			       (fwd_entry->mirroring_config.ipv_config.ipv_value  << 16) |
				dpv;
		iowrite32(fwlthtl9_val, ioaddr + FWLTHTL9);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n", FWLTHTL9,
			fwlthtl9_val);
#endif
		ret = rswitch2_l3_learn_status();
		if (ret < 0)
			pr_err("L3 Learning Failed for entry num= %d\n", entries);
	}
	if (stream_id != NULL)
		kfree(stream_id);

	return 0;
}

static int rswitch2_l3_tbl_two_byte_filter_config(
				struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 fwtwbfc = 0;
	u32 fwtwbfvc = 0;

	/* Below line not mentioned in spec, need to check if necessary */
	for (filter = 0; filter < RSWITCH2_MAX_TWO_BYTE_FILTERS; filter++) {
		iowrite32(0, ioaddr + FWTWBFVC0 + filter * 0x10);
		iowrite32(0, ioaddr + FWTWBFC0 + filter * 0x10);
	}
	for (filter = 0; filter < config->num_two_byte_filters; filter++) {
		fwtwbfc = config->two_byte[filter].offset << 16 |
			  config->two_byte[filter].offset_mode << 8 |
			  config->two_byte[filter].unit_mode;
		fwtwbfvc = config->two_byte[filter].value1 << 16 |
			   config->two_byte[filter].value0;

		iowrite32(fwtwbfvc, ioaddr + FWTWBFVC0 +
				config->two_byte[filter].filter_number * 0x10);
		iowrite32(fwtwbfc, ioaddr + FWTWBFC0 +
				config->two_byte[filter].filter_number * 0x10);
	}

	return 0;
}

static int rswitch2_l3_tbl_three_byte_filter_config(
				struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 fwthbfc = 0;
	u32 fwthbfv0c = 0;
	u32 fwthbfv1c = 0;

	/* Below line not mentioned in spec, need to check if necessary */
	for (filter = 0; filter < RSWITCH2_MAX_THREE_BYTE_FILTERS; filter++) {
		iowrite32(0, ioaddr + FWTHBFV0C0 + filter * 0x10);
		iowrite32(0, ioaddr + FWTHBFV1C0 + filter * 0x10);
		iowrite32(0, ioaddr + FWTHBFC0 + filter * 0x10);
	}
	/* Not mentioned line in spec ends */
	for (filter = 0; filter < config->num_three_byte_filters; filter++) {
		fwthbfc = config->three_byte[filter].offset << 16 |
			  config->three_byte[filter].unit_mode;
		fwthbfv0c = config->three_byte[filter].value0;
		fwthbfv1c = config->three_byte[filter].value1;

		iowrite32(fwthbfv0c, ioaddr + FWTHBFV0C0 +
			  config->three_byte[filter].filter_number * 0x10);
		iowrite32(fwthbfv1c, ioaddr + FWTHBFV1C0 +
			  config->three_byte[filter].filter_number * 0x10);
		iowrite32(fwthbfc, ioaddr + FWTHBFC0 +
			  config->three_byte[filter].filter_number * 0x10);
	}

	return 0;
}

static int rswitch2_l3_tbl_four_byte_filter_config(
				struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 fwfobfc = 0;
	u32 fwfobfv0c = 0;
	u32 fwfobfv1c = 0;

	/* Below line not mentioned in spec, need to check if necessary */
	for (filter = 0; filter < RSWITCH2_MAX_FOUR_BYTE_FILTERS; filter++) {
		iowrite32(0, ioaddr + FWFOBFV0C0 + filter * 0x10);
		iowrite32(0, ioaddr + FWFOBFV1C0 + filter * 0x10);
		iowrite32(0, ioaddr + FWFOBFC0 + filter * 0x10);
	}
	/* Not mentioned line in spec ends */
	for (filter = 0; filter < config->num_four_byte_filters; filter++) {
		fwfobfc = config->four_byte[filter].offset << 16 |
			  config->four_byte[filter].unit_mode;
		fwfobfv0c = config->four_byte[filter].value0;
		fwfobfv1c = config->four_byte[filter].value1;

		iowrite32(fwfobfv0c, ioaddr + FWFOBFV0C0 +
			  config->four_byte[filter].filter_number * 0x10);
		iowrite32(fwfobfv1c, ioaddr + FWFOBFV1C0 +
			  config->four_byte[filter].filter_number * 0x10);
		iowrite32(fwfobfc, ioaddr + FWFOBFC0 +
			  config->four_byte[filter].filter_number * 0x10);
	}

	return 0;
}

static int
rswitch2_l3_tbl_range_filter_config(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 fwrfc = 0;
	u32 fwrfvc = 0;

	/* Below line not mentioned in spec, need to check if necessary */
	for (filter = 0; filter < RSWITCH2_MAX_RANGE_FILTERS; filter++) {
		iowrite32(0, ioaddr + FWRFVC0 + filter * 0x10);
		iowrite32(0, ioaddr + FWRFC0 + filter * 0x10);
	}
	/* Not mentioned line in spec ends */
	for (filter = 0; filter < config->num_range_filters; filter++) {
		fwrfc = config->range[filter].offset << 16 |
			config->range[filter].offset_mode << 8;
		fwrfvc = config->range[filter].range << 16 |
			 config->range[filter].value1 << 8 |
			 config->range[filter].value0;

		iowrite32(fwrfvc, ioaddr + FWRFVC0 +
			  config->range[filter].filter_number * 0x10);
		iowrite32(fwrfc, ioaddr + FWRFC0 +
			  config->range[filter].filter_number * 0x10);
	}

	return 0;
}

static int
rswitch2_l3_tbl_cascade_filter_config(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 ports = 0;
	u32 pdpv = 0;
	u32 edpv = 0;
	u32 usefilterid = 0;
	u32 fwcfmc = 0;
	struct rswitch2_cascade_filter *cascade;

	/* Below line not mentioned in spec, need to check if necessary */
	for (filter = 0; filter < RSWITCH2_MAX_CASCADE_FILTERS; filter++) {
		for (usefilterid = 0;
		     usefilterid < RSWITCH2_MAX_USED_CASCADE_FILTERS;
		     usefilterid++) {
			/* Clear the register first */
			iowrite32(0, ioaddr + FWCFMC00 + filter * 0x40 +
				     usefilterid * 4);
		}
		iowrite32(0, ioaddr + FWCFC0 + filter * 0x40);
	}
	/* Not mentioned line in spec ends */
	for (filter = 0; filter < config->num_cascade_filters; filter++) {
		edpv = 0;
		pdpv = 0;
		cascade = &config->cascade[filter];
		for (ports = 0; ports < cascade->pframe_valid.dest_eth_ports;
		     ports++)
			pdpv |= 1 << cascade->pframe_valid.port_number[ports];
		for (ports = 0; ports < cascade->eframe_valid.dest_eth_ports;
		     ports++)
			edpv |= 1 << cascade->eframe_valid.port_number[ports];
		edpv |= cascade->eframe_valid.cpu << board_config.eth_ports;

		for (usefilterid = 0;
		     usefilterid < RSWITCH2_MAX_USED_CASCADE_FILTERS;
		     usefilterid++) {
			fwcfmc = 1 << 15;
			if (usefilterid < cascade->used_filter_ids) {
				fwcfmc = (1 << 15) |
					 cascade->used_filter_id_num[usefilterid];
			} else {
				fwcfmc = 0;
			}
			iowrite32(fwcfmc, ioaddr + FWCFMC00 +
					  cascade->filter_number * 0x40 +
					  usefilterid * 4);
		}
		iowrite32(edpv | (pdpv << 16), ioaddr + FWCFC0 +
			  cascade->filter_number * 0x40);
	}

	return 0;
}

static int
rswitch2_disable_perfect_filter(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;

	for (filter = 0; filter < config->num_cascade_filters; filter++) {
		iowrite32(0, ioaddr + FWCFC0 +
			  config->cascade[filter].filter_number * 0x40);
	}

	return 0;
}

static int rswitch2_l3_tbl_config(struct rswitch2_l3_stream_fwd *config)
{
	u32 entries = 0;
	int ret = 0;
	struct rswitch2_ipv4_stream_fwd_config *stream_config;
	struct rswitch2_ipv4_stream_fwd_entry *fwd_entry;

	stream_config = &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config;
	if (config->fwd_filter_config.num_cascade_filters)
		rswitch2_disable_perfect_filter(&config->fwd_filter_config);
	if (config->fwd_filter_config.num_two_byte_filters)
		rswitch2_l3_tbl_two_byte_filter_config(&config->fwd_filter_config);
	if (config->fwd_filter_config.num_three_byte_filters)
		rswitch2_l3_tbl_three_byte_filter_config(&config->fwd_filter_config);
	if (config->fwd_filter_config.num_four_byte_filters)
		rswitch2_l3_tbl_four_byte_filter_config(&config->fwd_filter_config);
	if (config->fwd_filter_config.num_range_filters)
		rswitch2_l3_tbl_range_filter_config(&config->fwd_filter_config);
	if (config->fwd_filter_config.num_cascade_filters)
		rswitch2_l3_tbl_cascade_filter_config(&config->fwd_filter_config);

	if (!stream_config->bEnable)
		return 0;

	for (entries = 0;
	     entries < stream_config->ipv4_stream_fwd_entries;
	     entries++) {
		fwd_entry = &stream_config->ipv4_stream_fwd_entry[entries];
		if (fwd_entry->filter_enable)
			continue;
		fwd_entry->hash_value =
			rswitch2_calc_ipv4_hash(&stream_config->ipv4_hash_configuration,
						fwd_entry);

		fwd_entry->stream_id = rswitch2_gen_ipv4_stream(fwd_entry->hash_value,
								fwd_entry,
								stream_config);
	}
	rswitch2_ipv4_stream_gen_config(stream_config,
					&stream_config->ipv4_hash_configuration);
	if (!l3_tbl_reconfig) {
		ret = rswitch2_l3_tbl_reset();
		if (ret < 0) {
			pr_err("l3  tbl Reset Failed\n");
			return -1;
		}
		l3_tbl_reconfig = 1;
	}
	iowrite32((config->max_l3_unsecure_entry << 16) |
		  config->max_l3_collision, ioaddr + FWLTHHEC);
#ifdef DEBUG
	pr_info("Register address= %x Write value = %x\n", FWLTHHEC,
		(config->max_l3_unsecure_entry << 16) |
		config->max_l3_collision);
#endif
	iowrite32(config->l3_hash_eqn, ioaddr + FWLTHHC);
#ifdef DEBUG
	pr_info("Register address= %x Write value = %x\n", FWLTHHC,
		config->l3_hash_eqn);
#endif
	rswitch2_ipv4_l3_tbl_config(config);

	return 0;
}

#ifdef VLAN_FWD
static int rswitch2_l2_vlan_tbl_config(struct rswitch2_l2_fwd_config *config)
{
	u32 vlantmue = 0;
	u32 fwvlantl0_val = 0;
	u32 fwvlantl1_val = 0;
	u32 ds_dpv = 0;
	u32 dest_ports = 0;
	u32 dpv = 0;
	u32 fwvlantl4_val = 0;
	u32 entries = 0;
	u32 port_lock = 0;
	int ret = 0;
	struct rswitch2_l2_fwd_vlan_config_entry *cfg_entry;

	vlantmue = config->max_unsecure_vlan_entry;
	iowrite32((vlantmue << 16), ioaddr + FWVLANTEC);
	for (entries = 0; entries < config->l2_fwd_vlan_config_entries; entries++) {
		cfg_entry = &config->l2_fwd_vlan_config_entry[entries];
		ds_dpv = 0;
		dpv = 0;
		fwvlantl0_val = (cfg_entry->vlan_learn_disable << 10) |
				(cfg_entry->vlan_security_learn << 8);
		iowrite32(fwvlantl0_val, ioaddr + FWVLANTL0);
		fwvlantl1_val = cfg_entry->vlan_id;
		iowrite32(fwvlantl1_val, ioaddr + FWVLANTL1);
		for (port_lock = 0;
		     port_lock < cfg_entry->source_lock.source_ports;
		     port_lock++) {
			ds_dpv |= 1 << cfg_entry->source_lock.source_port_number[port_lock];
		}
		ds_dpv |= cfg_entry->source_lock.cpu << board_config.eth_ports;
#ifdef DEBUG
		pr_info("ds_dpv = %x\n", ds_dpv);
#endif
		iowrite32(ds_dpv, ioaddr + FWVLANTL2);
		iowrite32(cfg_entry->csdn, ioaddr + FWVLANTL3);

		for (dest_ports = 0;
		     dest_ports < cfg_entry->destination_vector_config.dest_eth_ports;
		     dest_ports++) {
			dpv |= 1 << cfg_entry->destination_vector_config.port_number[dest_ports];
		}
		dpv |= cfg_entry->destination_vector_config.cpu << board_config.eth_ports;
		fwvlantl4_val = cfg_entry->mirroring_config.cpu_mirror_enable << 21 |
				cfg_entry->mirroring_config.eth_mirror_enable << 20 |
				cfg_entry->mirroring_config.ipv_config.ipv_update_enable << 19 |
				cfg_entry->mirroring_config.ipv_config.ipv_value << 16 |
				dpv;
		iowrite32(fwvlantl4_val, ioaddr + FWVLANTL4);
		ret = rswitch2_vlan_learn_status();
		if (ret < 0)
			pr_err("vlan Learning Failed for entry num= %d\n", entries);
	}

	return 0;
}
#endif

#ifdef L2_MAC_FWD
static int rswitch2_l2_mac_tbl_config(struct rswitch2_l2_fwd_config *config)
{
	u32 ds_dpv = 0;
	u32 ss_dpv = 0;
	u32 dpv = 0;
	u32 fwmactl0_val = 0;
	u32 fwmactl1_val = 0;
	u32 fwmactl2_val = 0;
	u32 fwmactl3_val = 0;
	u32 fwmactl5_val = 0;
	//u32 dpv = 0;
	u32 entries = 0;
	u32 port_lock = 0;
	u32 dest_ports = 0;
	struct rswitch2_l2_fwd_mac_config_entry *entry;
	int ret = 0;

	for (entries = 0; entries < config->l2_fwd_mac_config_entries;
	     entries++) {
		entry = &config->l2_fwd_mac_config_entry[entries];
		ss_dpv = 0;
		dpv = 0;
		ds_dpv = 0;
		fwmactl0_val = (entry->mac_learn_disable << 10) |
			       (entry->mac_dynamic_learn << 9) |
			       (entry->mac_security_learn << 8);
		iowrite32(fwmactl0_val, ioaddr + FWMACTL0);
		fwmactl1_val = (entry->mac[0] << 8) |
			       (entry->mac[1] << 0);
		iowrite32(fwmactl1_val, ioaddr + FWMACTL1);

		fwmactl2_val = (entry->mac[2] << 24) |
			       (entry->mac[3] << 16) |
			       (entry->mac[4] << 8) |
			       (entry->mac[5] << 0);
		iowrite32(fwmactl2_val, ioaddr + FWMACTL2);
		for (port_lock = 0;
		     port_lock < entry->destination_source.source_ports;
		     port_lock++) {
			ds_dpv |= 1 << entry->destination_source.source_port_number[port_lock];
		}
		ds_dpv |= entry->destination_source.cpu << board_config.eth_ports;

		for (port_lock = 0;
		     port_lock < entry->source_source.source_ports;
		     port_lock++) {
			ss_dpv |= 1 << entry->source_source.source_port_number[port_lock];
		}
		ss_dpv |= entry->source_source.cpu << board_config.eth_ports;

		fwmactl3_val = ss_dpv | (ds_dpv << 16);
		iowrite32(fwmactl3_val, ioaddr + FWMACTL3);
		iowrite32(entry->csdn, ioaddr + FWMACTL4);
		for (dest_ports = 0;
		     dest_ports < entry->destination_vector_config.dest_eth_ports;
		     dest_ports++) {
			dpv |= 1 << entry->destination_vector_config.port_number[dest_ports];
		}
		dpv |= entry->destination_vector_config.cpu << board_config.eth_ports;
		fwmactl5_val = entry->mirroring_config.cpu_mirror_enable << 21 |
			       entry->mirroring_config.eth_mirror_enable << 20 |
			       entry->mirroring_config.ipv_config.ipv_update_enable << 19 |
			       entry->mirroring_config.ipv_config.ipv_value << 16 |
			       dpv;
		iowrite32(fwmactl5_val, ioaddr + FWMACTL5);
		ret = rswitch2_mac_learn_status();
		if (ret < 0)
			pr_err("mac Learning Failed for entry num= %d\n", entries);
	}

	return 0;
}
#endif

static int rswitch2_config_mac_aging(struct rswitch2_mac_aging_config *config)
{
	u32 fwmacagc = 0;

	iowrite32(config->mac_aging_prescalar, ioaddr + FWMACAGUSPC);
	fwmacagc = (config->on_off << 24) |
		   (config->mac_aging_polling_mode << 18) |
		   (config->mac_aging_security << 17) | (config->on_off << 16) |
		   (config->mac_aging_time_sec);
	iowrite32(fwmacagc, ioaddr + FWMACAGC);

	return 0;
}

static int rswitch2_l2_table_update_config(struct rswitch2_l2_fwd_config *config)
{
	int ret = 0;

	//if(config->l2_fwd_mac_config_entries)
	{
		if (!mac_tbl_reconfig) {
			ret = rswitch2_l2_mac_tbl_reset();
			if (ret < 0) {
				pr_err("l2 MAC tbl Reset Failed\n");
				return -1;
			}
			mac_tbl_reconfig = 1;
		}
		iowrite32((config->max_unsecure_hash_entry << 16) |
			  config->max_hash_collision, ioaddr + FWMACHEC);
		iowrite32(config->mac_hash_eqn, ioaddr + FWMACHC);
		ret = rswitch2_l2_mac_tbl_config(config);
		if (ret < 0) {
			pr_err("l2 MAC tbl Config Failed\n");
			return -1;
		}
		if (config->mac_aging_config.bEnable)
			ret = rswitch2_config_mac_aging(&config->mac_aging_config);
	}
	//if(config->l2_fwd_vlan_config_entries)
	{
		if (!vlan_tbl_reconfig) {
			ret = rswitch2_l2_vlan_tbl_reset();
			if (ret < 0) {
				pr_err("l2 VLAN tbl Reset Failed\n");
				return -1;
			}
			vlan_tbl_reconfig = 1;
		}
		iowrite32(config->max_unsecure_vlan_entry << 16,
			  ioaddr + FWVLANTEC);
		ret = rswitch2_l2_vlan_tbl_config(config);
		if (ret < 0) {
			pr_err("l2 VLAN tbl Config Failed\n");
			return -1;
		}
	}

	return 0;
}

static int rswitch2_l23_table_update_config(struct rswitch2_l23_update *config)
{
	u32 dpv = 0;
	u32 entries = 0;
	u32 dest_ports = 0;
	u32 fwl23url0 = 0;
	u32 fwl23url1 = 0;
	int ret = 0;
	u32 fwl23url2 = 0;
	u32 fwl23url3 = 0;
	struct rswitch2_l23_update_config *uc;

	if (!l23_tbl_reconfig) {
		ret = rswitch2_l23_update_tbl_reset();
		if (ret < 0) {
			pr_err("l23 Update  tbl Reset Failed\n");
			return -1;
		}
		l23_tbl_reconfig = 1;
	}
	for (entries = 0; entries < config->entries; entries++) {
		uc = &config->l23_update_config[entries];
		dpv = 0;
		for (dest_ports = 0;
		     dest_ports < uc->routing_port_update.dest_eth_ports;
		     dest_ports++) {
			dpv |= 1 << uc->routing_port_update.port_number[dest_ports];
		}
#ifdef DEBUG
		pr_info("config->l23_update_config[entries].src_mac_update = %x\n",
			uc->src_mac_update);
#endif
		dpv |= uc->routing_port_update.cpu << board_config.eth_ports;
		fwl23url0 = (dpv << 16) | uc->routing_number;
		fwl23url1 = ((uc->rtag & 0x03) << 25) |
			    ((uc->stag_dei_update & 0x01) << 24) |
			    ((uc->stag_pcp_update & 0x01) << 23) |
			    ((uc->stag_vlan_update & 0x01) << 22) |
			    ((uc->ctag_dei_update & 0x01) << 21) |
			    ((uc->ctag_pcp_update & 0x01) << 20) |
			    ((uc->ctag_vlan_update & 0x01) << 19) |
			    ((uc->src_mac_update & 0x01) << 18) |
			    ((uc->dest_mac_update & 0x01) << 17) |
			    ((uc->ttl_update & 0x01) << 16) |
			    ((uc->dest_mac[0] & 0xFF) << 8) |
			    ((uc->dest_mac[1] & 0xFF) << 0);
		fwl23url2 = ((uc->dest_mac[2] & 0xFF) << 24) |
			    ((uc->dest_mac[3] & 0xFF) << 16) |
			    ((uc->dest_mac[4] & 0xFF) << 8) |
			    ((uc->dest_mac[5] & 0xFF) << 0);
		fwl23url3 = ((uc->stag_dei & 0x01) << 31) |
			    ((uc->stag_pcp & 0x07) << 28) |
			    ((uc->stag_vlan & 0xFFF) << 16) |
			    ((uc->ctag_dei & 0x01) << 15) |
			    ((uc->ctag_pcp & 0x07) << 12) |
			    ((uc->ctag_vlan & 0xFFF) << 0);
		iowrite32(fwl23url0, ioaddr + FWL23URL0);
#ifdef DEBUG
		pr_info("fwl23url1=%x\n", fwl23url1);
#endif
		iowrite32(fwl23url1, ioaddr + FWL23URL1);
		iowrite32(fwl23url2, ioaddr + FWL23URL2);
		iowrite32(fwl23url3, ioaddr + FWL23URL3);
		ret = rswitch2_l23_update_learn_status();
		if (ret < 0)
			pr_err("L23 Update Learning  Failed for entry num= %d\n", entries);
	}

	return 0;
}


#ifdef IP_TBL_FWD
static int rswitch2_ip_tbl_config(struct rswitch2_ipv_fwd_config *config)
{
	u32 ds_dpv = 0;
	u32 ss_dpv = 0;
	u32 dpv = 0;
	u32 fwiphec_val = 0;
	u32 fwiphc_val = 0;
	u32 fwiptl0_val = 0;
	u32 fwiptl4_val = 0;
	u32 fwiptl5_val = 0;
	u32 fwiptl8_val = 0;
	//u32 dpv = 0;
	u32 entries = 0;
	u32 port_lock = 0;
	u32 dest_ports = 0;
	int ret = 0;
	struct rswitch2_ipv_fwd_config_entry *entry;

	if (!ip_tbl_reconfig) {
		ret = rswitch2_ip_tbl_reset();
		if (ret < 0) {
			pr_err("IP tbl Reset Failed\n");
			return -1;
		}
		ip_tbl_reconfig = 1;
	}
	fwiphec_val = (config->max_unsecure_hash_entry << 16) |
		      config->max_hash_collision;
	iowrite32(fwiphec_val, ioaddr + FWIPHEC);
	fwiphc_val = config->ipv_hash_eqn;
	for (entries = 0; entries < config->ipv_fwd_config_entries; entries++) {
		ds_dpv = 0;
		ss_dpv = 0;
		dpv = 0;
		entry = &config->entry[entries];
		fwiptl0_val = (entry->ipv_learn_disable << 10) |
			      (entry->ipv_dynamic_learn << 9) |
			      (entry->ipv_security_learn << 8) |
			      entry->ip_addr_type;
		iowrite32(fwiptl0_val, ioaddr + FWIPTL0);
		if (entry->ip_addr_type == ip_addr_ipv4) {
			fwiptl4_val = (entry->ipaddr[0] << 24) |
				      (entry->ipaddr[1] << 16) |
				      (entry->ipaddr[2] << 8) |
				      entry->ipaddr[3] << 0;
			iowrite32(fwiptl4_val, ioaddr + FWIPTL4);
		} else {
			/* ipv6 Suppport to be provided */
		}

		for (port_lock = 0;
		     port_lock < entry->destination_source.source_ports;
		     port_lock++) {
			ds_dpv |= 1 << entry->destination_source.source_port_number[port_lock];
		}
		ds_dpv |= entry->destination_source.cpu << board_config.eth_ports;
		for (port_lock = 0;
		     port_lock < entry->source_source.source_ports;
		     port_lock++) {
			ss_dpv |= 1 << entry->source_source.source_port_number[port_lock];
		}
		ss_dpv |= entry->source_source.cpu << board_config.eth_ports;
		fwiptl5_val = entry->routing_number | (ds_dpv << 16) |
			      (entry->routing_valid << 15);
		iowrite32(fwiptl5_val, ioaddr + FWIPTL5);
		iowrite32(ss_dpv, ioaddr + FWIPTL6);
		for (dest_ports = 0;
		     dest_ports < entry->destination_vector_config.dest_eth_ports;
		     dest_ports++) {
			dpv |= 1 << entry->destination_vector_config.port_number[dest_ports];
		}
		dpv |= entry->destination_vector_config.cpu << board_config.eth_ports;
		if (entry->destination_vector_config.cpu)
			iowrite32(entry->csdn, ioaddr + FWIPTL7);
		fwiptl8_val = entry->mirroring_config.cpu_mirror_enable << 21 |
			      entry->mirroring_config.eth_mirror_enable << 20 |
			      entry->mirroring_config.ipv_config.ipv_update_enable << 19 |
			      entry->mirroring_config.ipv_config.ipv_value << 16 |
			      dpv;
		iowrite32(fwiptl8_val, ioaddr + FWIPTL8);
		ret = rswitch2_ipv_learn_status();
		if (ret < 0)
			pr_err("ip Learning Failed for entry num= %d\n", entries);
	}

	return 0;
}
#endif

static int rswitch2_fwd_default_config(void)
{
	u32 machma = 0;
	u32 machla = 0;
	u32 macdsa = 0;
	u32 l2se = 0;
	u32 lthta = 0;
	u32 i = 0;
	u32 vlansa = 0;
	u32 fwpc0 = 0;
	u32 iphma = 0;
	u32 iphla = 0;
	u32 iprussa = 0;
	u32 iprusa = 0;
	u32 ipssa = 0;
	u32 iprudsa = 0;
	u32 ipruda = 0;
	u32 ipdsa = 0;
	u32 ip6ea = 0;
	u32 ip4ea = 0;
	u32 ip6oe = 0;
	u32 ip6te = 0;
	u32 ip6ue = 0;
	u32 ip4oe = 0;
	u32 ip4te = 0;
	u32 ip4ue = 0;
	u32 lthruss = 0;
	u32 lthrus = 0;

	iowrite32(0xFFFFFFFF, ioaddr + FWSCR27);
	iowrite32(0xFFFFFFFF, ioaddr + FWSCR28);
	iowrite32(0xFFFFFFFF, ioaddr + FWSCR29);
	iowrite32(0xFFFFFFFF, ioaddr + FWSCR30);
	iowrite32(0xFFFFFFFF, ioaddr + FWSCR31);
	iowrite32(0xFFFFFFFF, ioaddr + FWSCR32);
	iowrite32(0xFFFFFFFF, ioaddr + FWSCR33);
	iowrite32(0xFFFFFFFF, ioaddr + FWSCR34);
	/* No vlan Mode, Dont touch register FWGC FWTTC0 FWTTC1*/
	/* No Exceptional path target Dont touch register FWCEPTC, FWCEPRC0, FWCEPRC1, FWCEPRC2*/
	/* No Learn frame to cpu Dont touch register FWCLPTC FWCLPRC */
	/* No mirroring configuration Dont touch register FWCMPTC FWEMPTC FWSDMPTC FWSDMPVC */
	/* No frame Discarded for watermark FWLBWMCi default values */
	for (i = 0; i < board_config.eth_ports; i++) {
		vlansa = 1;

		/* Migration Active */
		machma = 1;
		/*Learning Active */
		machla = 1;
		/* mac DA Search Active */
		macdsa = 1;
		iphma = 0;
		iphla = 1;
		iprussa = 0;
		iprusa = 0;
		ipssa = 0;
		iprudsa = 0;
		ipruda = 0;
		ipdsa = 1;
		ip6ea = 0;
		ip4ea = 1;
		/* l2 stream Enable */
		l2se = 1;
		ip6oe = 0;
		ip6te = 0;
		ip6ue = 0;
		ip4oe = 1;
		ip4te = 1;
		ip4ue = 1;
		lthruss = 0;
		lthrus = 0;
		/*L3 Table Active */
		lthta = 1;

		fwpc0 = (vlansa << 28) | (machma << 27) | (machla << 26) |
			(macdsa << 20) | (l2se << 9) | (lthta << 0) |
			(iphla << 18) | (ipdsa << 12) | (ip4ea << 10) |
			(ip4oe << 5) | (ip4te << 4) | (ip4ue << 3) | lthta;
		iowrite32(fwpc0, ioaddr + FWPC00 + i * 0x10);
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n",
			FWPC00 + (i * 0x10), fwpc0);
#endif
	}

	return 0;
}

static int rswitch2fwd_ioctl_getfconfig(struct file *file, unsigned long arg)
{
	char __user *buf = (char __user *)arg;
	int err = 0;

	rswitch2fwd_config.fwd_gen_config.vlan_mode = ioread32(ioaddr + FWGC);
	err = copy_to_user(buf, &rswitch2fwd_config,
			   sizeof(rswitch2fwd_config));
	if (err < 0)
		return err;

	return 0;
}

static int rswitch2_port_fwding_config(struct rswitch2_port_forwarding *config,
				       u32 agent)
{
	u32 dpv = 0;
	u32 dest_ports = 0;
	u32 fwpbfc = 0;

	for (dest_ports = 0;
	     dest_ports < config->destination_vector_config.dest_eth_ports;
	     dest_ports++) {
		dpv |= 1 << config->destination_vector_config.port_number[dest_ports];
	}
	dpv |= config->destination_vector_config.cpu << board_config.eth_ports;
	fwpbfc = (config->force_frame_priority << 26) |
		 (config->ipv6_priority_decode << 25) |
		 (config->ipv4_priority_decode << 23) |
		 (config->ipv4_priority_decode_mode << 24) |
		 (config->security_learn << 22) |
		 (config->mirroring_config.cpu_mirror_enable << 21) |
		 (config->mirroring_config.eth_mirror_enable << 20) |
		 (config->mirroring_config.ipv_config.ipv_update_enable << 19) |
		 (config->mirroring_config.ipv_config.ipv_value << 16) | dpv;
	iowrite32(fwpbfc, ioaddr + FWPBFC0 + agent * 0x10);
	iowrite32(config->csdn, ioaddr + FWPBFCSDC00 + agent * 0x10);

	return 0;
}

static int rswitch2_fwd_gen_config(struct rswitch2_fwd_gen_config *config)
{
	u32 fwpc0 = 0;
	u32 i = 0;
	struct rswitch2_fwd_gen_port_config *port;


	iowrite32(config->vlan_mode, ioaddr + FWGC);
	iowrite32((config->stag_tpid << 16) | config->ctag_tpid,
		  ioaddr + FWTTC0);
	if (config->vlan_mode_config_only)
		return 0;
	for (i = 0; i < board_config.agents; i++) {
		port = &config->fwd_gen_port_config[i];
		fwpc0 = (port->vlan_reject_unknown_secure << 30) |
			(port->vlan_reject_unknown << 29) |
			(port->vlan_search_active << 28) |
			(port->mac_hw_migration_active << 27) |
			(port->mac_hw_learn_active << 26) |
			(port->mac_reject_unknown_src_secure_addr << 25) |
			(port->mac_reject_unknown_src_addr << 24) |
			(port->mac_src_search_active << 23) |
			(port->mac_reject_unknown_dest_secure_addr << 22) |
			(port->mac_reject_unknown_dest_addr << 21) |
			(port->mac_dest_search_active << 20) |
			(port->ip_hw_migration_active << 19) |
			(port->ip_hw_learn_active << 18) |
			(port->ip_reject_unknown_src_secure_addr << 17) |
			(port->ip_reject_unknown_src_addr << 16) |
			(port->ip_src_search_active << 15) |
			(port->ip_reject_unknown_dest_secure_addr << 14) |
			(port->ip_reject_unknown_dest_addr << 13) |
			(port->ip_dest_search_active << 12) |
			(port->ipv6_extract_active << 11) |
			(port->ipv4_extract_active << 10) |
			(port->l2_stream_enabled << 9) |
			(port->ipv6_other_enabled << 8) |
			(port->ipv6_tcp_enabled << 7) |
			(port->ipv6_udp_enabled << 6) |
			(port->ipv4_other_enabled << 5) |
			(port->ipv4_tcp_enabled << 4) |
			(port->ipv4_udp_enabled << 3) |
			(port->l3_reject_unknown_secure_stream << 2) |
			(port->l3_reject_unknown_stream << 1) |
			(port->l3_tbl_active << 0);
		if (port->cpu) {
			iowrite32(fwpc0, ioaddr + FWPC00 +
					 board_config.eth_ports * 0x10);
			if (port->port_forwarding.bEnable)
				rswitch2_port_fwding_config(&port->port_forwarding,
							    board_config.eth_ports);
		} else {
			iowrite32(fwpc0, ioaddr + FWPC00 +
					 port->portnumber * 0x10);
			if (port->port_forwarding.bEnable)
				rswitch2_port_fwding_config(&port->port_forwarding,
							    port->portnumber);
		}
#ifdef DEBUG
		pr_info("Register address= %x Write value = %x\n",
			FWPC00 + i * 0x10, fwpc0);
#endif
	}

	return 0;
}

static int
rswitch2_mcast_rx_desc_chain_config(struct rswitch2_cpu_rx_mcast_config *config)
{

	u32 msenl = 0;
	u32 mnl = 0;
	u32 mnrcnl = 0;
	u32 gwmstls_val = 0;
	u32 entry = 0;
	void __iomem *gwca_addr = NULL;
	s32 ret = 0;

	gwca_addr = ioaddr + board_config.gwca0.start;
	rswitch2_gwca_mcast_tbl_reset();
	for (entry = 0; entry < config->entries; entry++) {
		msenl  = config->entry[entry].entry_num;
		mnl = config->entry[entry].next_entries;
		mnrcnl = config->entry[entry].next_entry_num;
		gwmstls_val = (msenl << 16) | (mnl << 8) | (mnrcnl << 0);
		iowrite32(gwmstls_val, gwca_addr + GWMSTLS);
		ret = rswitch2_mcast_tbl_learn_status();
		if (ret < 0)
			pr_err("MCAST Table  Learning Failed\n");
	}

	return 0;
}

static int updateconfiguration(void)
{
#ifdef DEBUG
	pr_info("Update configuration\n");
#endif
	memcpy(&rswitch2fwd_config, &rswitch2fwd_confignew,
	       sizeof(rswitch2fwd_config));
	/* Update general configuration if required */
	if (rswitch2fwd_config.fwd_gen_config.bEnable) {
#ifdef DEBUG
		pr_info("gen config benable\n");
#endif
		rswitch2_fwd_gen_config(&rswitch2fwd_config.fwd_gen_config);
	}
	if (rswitch2fwd_config.rx_mcast_config.entries)
		rswitch2_mcast_rx_desc_chain_config(&rswitch2fwd_config.rx_mcast_config);
	if (rswitch2fwd_config.l3_stream_fwd.bEnable)
		rswitch2_l3_tbl_config(&rswitch2fwd_config.l3_stream_fwd);
	if (rswitch2fwd_config.l23_update.entries)
		rswitch2_l23_table_update_config(&rswitch2fwd_config.l23_update);
	if (rswitch2fwd_config.l2_fwd_config.bEnable)
		rswitch2_l2_table_update_config(&rswitch2fwd_config.l2_fwd_config);
	if (rswitch2fwd_config.ipv_fwd_config.bEnable)
		rswitch2_ip_tbl_config(&rswitch2fwd_config.ipv_fwd_config);

	return 0;
}

static long rswitch2fwd_ioctl_setconfig(struct file *file, unsigned long arg)
{
	char __user *buf = (char __user *)arg;
	int err = 0;

#ifdef DEBUG
	pr_info("Set config Called\n");
#endif
	err = access_ok(VERIFY_WRITE, buf, sizeof(rswitch2fwd_confignew));

	err = copy_from_user(&rswitch2fwd_confignew, buf,
			     sizeof(rswitch2fwd_confignew));
	if (err != 0) {
		pr_err("[RSWITCH2-FWD] rswitch2fwd_IOCTL_Setportsconfig. copy_from_user returned %d for RSWITCH2_FWD_SET_CONFIG\n",
		       err);
		return err;
	}
	err = updateconfiguration();
	if (err < 0)
		pr_err("Returned Err\n");

	return err;
}

static long rswitch2fwd_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case RSWITCH2_FWD_GET_CONFIG:
		return rswitch2fwd_ioctl_getfconfig(file, arg);

	case RSWITCH2_FWD_SET_CONFIG:
		return rswitch2fwd_ioctl_setconfig(file, arg);

	default:
		pr_err("[RSWITCH2-FWD] IOCTL Unknown or unsupported: 0x%08X\n",
		       cmd);
		ret = -EINVAL;
	}

	return ret;
}

/* Character Device operations */
const struct file_operations rswitch2fwd_fileOps = {
	.owner            = THIS_MODULE,
	.unlocked_ioctl   = rswitch2fwd_ioctl,
	.compat_ioctl     = rswitch2fwd_ioctl,
};

static int rswitch2fwd_drv_remove_chrdev(void)
{
	int ret = 0;

	/* Remove character device */
	unregister_chrdev_region(rswitch2fwd_dev, 1);
	device_del(&rswitch2fwd_device);
	cdev_del(&rswitch2fwd_cdev);
	class_destroy(rswitch2fwd_devclass);

	return ret;
}

static int rswitch2fwd_drv_probe_chrdev(void)
{
	struct device *dev = NULL;
	int ret = 0;

	/* Create class */
	rswitch2fwd_devclass = class_create(THIS_MODULE, RSWITCH2FWD_CLASS);
	if (IS_ERR(rswitch2fwd_devclass)) {
		ret = PTR_ERR(rswitch2fwd_devclass);
		pr_err("[RSWITCH2-FWD] failed to create '%s' class. rc=%d\n",
		       RSWITCH2FWD_CLASS, ret);
		return ret;
	}

	if (rswitch2fwd_devmajor != 0) {
		rswitch2fwd_dev = MKDEV(rswitch2fwd_devmajor, 0);
		ret = register_chrdev_region(rswitch2fwd_dev, 1,
					     RSWITCH2FWD_CLASS);
	} else {
		ret = alloc_chrdev_region(&rswitch2fwd_dev, 0, 1,
					  RSWITCH2FWD_CLASS);
	}
	if (ret < 0) {
		pr_err("[RSWITCH2-FWD] failed to register '%s' character device. rc=%d\n",
		       RSWITCH2FWD_CLASS, ret);
		class_destroy(rswitch2fwd_devclass);
		return ret;
	}
	rswitch2fwd_devmajor = MAJOR(rswitch2fwd_dev);

	cdev_init(&rswitch2fwd_cdev, &rswitch2fwd_fileOps);
	rswitch2fwd_cdev.owner = THIS_MODULE;

	ret = cdev_add(&rswitch2fwd_cdev, rswitch2fwd_dev,
		       RSWITCH2_FWD_CTRL_MINOR + 1);
	if (ret < 0) {
		pr_err("[RSWITCH2-FWD] failed to add '%s' character device. rc=%d\n",
		       RSWITCH2FWD_CLASS, ret);
		unregister_chrdev_region(rswitch2fwd_dev, 1);
		class_destroy(rswitch2fwd_devclass);
		return ret;
	}

	/* device initialize */
	dev = &rswitch2fwd_device;
	device_initialize(dev);
	dev->class = rswitch2fwd_devclass;
	dev->devt = MKDEV(rswitch2fwd_devmajor, RSWITCH2_FWD_CTRL_MINOR);
	dev_set_name(dev, RSWITCH2_FWD_DEVICE_NAME);

	ret = device_add(dev);
	if (ret < 0) {
		pr_err("[RSWITCH2-FWD] failed to add '%s' device. rc=%d\n",
		       RSWITCH2_FWD_DEVICE_NAME, ret);
		cdev_del(&rswitch2fwd_cdev);
		unregister_chrdev_region(rswitch2fwd_dev, 1);
		class_destroy(rswitch2fwd_devclass);
	}

	return ret;
}

#ifdef CONFIG_RENESAS_RSWITCH2_STATS
static void getPortNameStringFull(char *buffer, u8 *number)
{
	struct port_private *priv;
	int i = 0;

	for (i = 0; i < board_config.eth_ports; i++) {
		priv = netdev_priv(ppndev[i]);
		if (*number == priv->portnumber) {
			strcpy(buffer, ppndev[i]->name);
			return;
		}
	}
}

static void getPortNameString(char *buffer, int *number)
{
	int i;
	char s[10];

	if (board_config.eth_ports < *number)
		*number = board_config.eth_ports;

	for (i = 0; i < *number; i++) {
		buffer[i] = '?';
		strncpy(s, ppndev[i]->name, 10);
		s[9] = 0;
		if (strcmp(s, "eth1") == 0) {
			buffer[i] = 'E';
		} else if (strcmp(s, "tsn0") == 0) {
			buffer[i] = 'M';
		} else {
			s[3] = 0;
			if (strcmp(s, "tsn") == 0)
				buffer[i] = ppndev[i]->name[3];  //the interface number
		}
	}
}

static void writeTargetFields(struct seq_file *m, const char *portName,
			      const int portNumber, const u32 dpv,
			      const s32 csdn)
{
	int c = 0;

	for (c = 0; c < portNumber; c++) {
		if (((dpv >> c) & 0x01) == 1)
			seq_printf(m, " %c", portName[c]);
		else
			seq_puts(m, "  ");
	}
	if (((dpv >> board_config.eth_ports) & 0x01) == 0x01) {
		seq_puts(m, " CPU");
		if (csdn >= 0)
			seq_printf(m, ":%-3d", csdn);
	} else {
		seq_puts(m, "    ");
		if (csdn >= 0)
			seq_puts(m, "    ");
	}
}

static void writeMirrorEtcFields(struct seq_file *m, const char *portName,
				 const int portNumber, const u8 cpu_mirror,
				 const u8 eth_mirror, const u32 srclockdpv,
				 const s64 destlockdpv, const s64 destcsdn,
				 const u8 ipv_enable, const u32 ipv_value)
{
	int c = 0;

	seq_printf(m, " %-3s", eth_mirror ? "Eth" : "No");
	seq_printf(m, " %-3s", cpu_mirror ? "CPU" : "No");

	seq_puts(m, " ");
	for (c = 0; c < portNumber; c++) {
		if (((srclockdpv >> c) & 0x01) == 1)
			seq_printf(m, " %c", portName[c]);
		else
			seq_puts(m, "  ");
	}
	if (((srclockdpv >> board_config.eth_ports) & 0x01) == 0x01)
		seq_puts(m, " CPU");
	else
		seq_puts(m, "    ");

	if (destlockdpv >= 0) {
		seq_puts(m, "  ");
		for (c = 0; c < portNumber; c++) {
			if (((destlockdpv >> c) & 0x01) == 1)
				seq_printf(m, " %c", portName[c]);
			else
				seq_puts(m, "  ");
		}
		if (((destlockdpv >> board_config.eth_ports) & 0x01) == 0x01) {
			seq_printf(m, " %s", "CPU");
			if (destcsdn >= 0)
				seq_printf(m, ":%-3lld", destcsdn);
		} else {
			seq_puts(m, "    ");
			if (destcsdn >= 0)
				seq_puts(m, "    ");
		}
	}

	if (ipv_enable)
		seq_printf(m, "  %d", ipv_value);
	else
		seq_puts(m, "  -");
}

static int rswitch2_fwd_ip_show(struct seq_file *m, void *v)
{
	u32 entry = 0;
	u32 value = 0;
	u8 i = 0;
	u32 fwiptrr1 = 0;
	u32 fwiptrr2 = 0;
	u32 fwiptrr3 = 0;
	u32 fwiptrr4 = 0;
	u32 fwiptrr5 = 0;
	u32 fwiptrr6 = 0;
	u32 fwiptrr7 = 0;
	u32 fwiptrr9 = 0;
	u32 csdn = 0;
	u32 routingvalid = 0;
	u32 routingnum = 0;
	u8 learn_disable = 0;
	u8 dynamic = 0;
	u8 security = 0;
	u32 destlockdpv = 0;
	u32 srclockdpv = 0;
	u32 dpv = 0;
	u8 ip[4] = {0, 0, 0, 0};
	enum rswitch2_ip_addr_type ip_type;
	u32 ipv_value = 0;
	u8 ipv_enable = 0;
	u8 cpu_mirror = 0;
	u8 eth_mirror = 0;
	char str1[30];
	char portName[10];
	int  portNumber = 10;

	getPortNameString(portName, &portNumber);

	seq_puts(m, "=========== IP-TABLE ==========================================================================\n");
	seq_puts(m, "Line        IP         Mode     Target-Ports    Mirror   Src-Valid     Dst-Valid    IPV   Rule#\n");
	seq_puts(m, "===============================================================================================\n");

	for (entry = 0; entry <= 1023; entry++) {
		iowrite32(entry, ioaddr + FWIPTR);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWIPTRR0);
			if (((value >> 31) & 0x01) != 0x0) {
				mdelay(1);
				continue;
			}
			if ((((value >> 1) & 0x01) == 0x00) &&
			    (((value >> 0) & 0x01) == 0x1)) {
				fwiptrr1 = ioread32(ioaddr + FWIPTRR1);
				fwiptrr2 = ioread32(ioaddr + FWIPTRR2);
				fwiptrr3 = ioread32(ioaddr + FWIPTRR3);
				fwiptrr4 = ioread32(ioaddr + FWIPTRR4);
				fwiptrr5 = ioread32(ioaddr + FWIPTRR5);
				fwiptrr6 = ioread32(ioaddr + FWIPTRR6);
				csdn = ioread32(ioaddr + FWIPTRR8);
				fwiptrr7 = ioread32(ioaddr + FWIPTRR7);
				fwiptrr9 = ioread32(ioaddr + FWIPTRR9);
				learn_disable = (fwiptrr1 >> 10) & 0x01;
				dynamic = (fwiptrr1 >> 9) & 0x01;
				security = (fwiptrr1 >> 8) & 0x01;
				ip_type = (fwiptrr1 >> 0) & 0x01;
				destlockdpv = (fwiptrr6 >> 16) & 0xFF;
				srclockdpv = (fwiptrr7 >> 0) & 0xFF;
				cpu_mirror = (fwiptrr9 >> 21) & 0x01;
				eth_mirror = (fwiptrr9 >> 20) & 0x01;
				ipv_enable = (fwiptrr9 >> 19) & 0x01;
				ipv_value = (fwiptrr9 >> 16) & 0xFF;
				if ((fwiptrr6 >> 15) & 0x01) {
					routingnum = fwiptrr6 & 0xFF;
					routingvalid = 1;
				} else {
					routingvalid = 0;
				}
				dpv = (fwiptrr9 >> 0) & 0xFF;
				ip[0] = (fwiptrr5 >> 24) & 0xFF;
				ip[1] = (fwiptrr5 >> 16) & 0xFF;
				ip[2] = (fwiptrr5 >> 8) & 0xFF;
				ip[3] = (fwiptrr5 >> 0) & 0xFF;
				if (ip_type != ip_addr_ipv4)
					break;

				seq_printf(m, "%-11d", entry);
				sprintf(str1, "%d.%d.%d.%d",
					ip[0], ip[1], ip[2], ip[3]);
				seq_printf(m, "%-10s", str1);
				seq_printf(m, "  %-7s",
					   dynamic ? "Dynamic" : "Static");

				seq_puts(m, " ");
				writeTargetFields(m, portName, portNumber, dpv,
						  csdn);
				writeMirrorEtcFields(m, portName, portNumber,
						     cpu_mirror, eth_mirror,
				srclockdpv, destlockdpv, -1, ipv_enable, ipv_value);
				if (routingvalid)
					seq_printf(m, "    %-2d", routingnum);
				else
					seq_puts(m, "    - ");

				//default values are not printed
				if (security)
					seq_printf(m, "   SecurityLevel=%d", security);
				if (learn_disable)
					seq_printf(m, "   HwLearnDisable=%d", learn_disable);

				seq_puts(m, "\n");
			}
			break;
		}
	}

	return 0;
}

static int rswitch2_fwd_l2_show(struct seq_file *m, void *v)
{
	u32 entry = 0;
	u32 value = 0;
	u8 i = 0;
	u32 fwmactrr1 = 0;
	u32 fwmactrr2 = 0;
	u32 fwmactrr3 = 0;
	u32 fwmactrr4 = 0;
	u32 fwmactrr6 = 0;
	u32 csdn = 0;
	u8 ageing = 0;
	u8 learn_disable = 0;
	u8 dynamic = 0;
	u8 security = 0;
	u32 destlockdpv = 0;
	u32 srclockdpv = 0;
	u32 dpv = 0;
	u8 mac[6] = {0, 0, 0, 0, 0, 0};
	u32 ipv_value = 0;
	u8 ipv_enable = 0;
	u8 cpu_mirror = 0;
	u8 eth_mirror = 0;

	char portName[10];
	int  portNumber = 10;

	getPortNameString(portName, &portNumber);

	seq_puts(m, "=========== MAC-TABLE ===================================================================\n");
	seq_puts(m, "Line        MAC         Mode     Target-Ports    Mirror   Src-Valid     Dst-Valid    IPV\n");
	seq_puts(m, "=========================================================================================\n");

	for (entry = 0; entry <= 1023; entry++) {
		iowrite32(entry, ioaddr + FWMACTR);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWMACTRR0);
			if (((value >> 31) & 0x01) != 0x0) {
				mdelay(1);
				continue;
			}
			if ((((value >> 2) & 0x01) == 0) &&
			    (((value >> 1) & 0x01) == 0x00) &&
			    (((value >> 0) & 0x01) == 0x1)) {
				fwmactrr1 = ioread32(ioaddr + FWMACTRR1);
				fwmactrr2 = ioread32(ioaddr + FWMACTRR2);
				fwmactrr3 = ioread32(ioaddr + FWMACTRR3);
				fwmactrr4 = ioread32(ioaddr + FWMACTRR4);
				csdn = ioread32(ioaddr + FWMACTRR5);
				fwmactrr6 = ioread32(ioaddr + FWMACTRR6);

				ageing = (fwmactrr1 >> 11) & 0x01;
				learn_disable = (fwmactrr1 >> 10) & 0x01;
				dynamic = (fwmactrr1 >> 9) & 0x01;
				security = (fwmactrr1 >> 8) & 0x01;
				destlockdpv = (fwmactrr4 >> 16) & 0xFF;
				srclockdpv = (fwmactrr4 >> 0) & 0xFF;
				cpu_mirror = (fwmactrr6 >> 21) & 0x01;
				eth_mirror = (fwmactrr6 >> 20) & 0x01;
				ipv_enable = (fwmactrr6 >> 19) & 0x01;
				ipv_value = (fwmactrr6 >> 16) & 0xFF;
				dpv = (fwmactrr6 >> 0) & 0xFF;
				mac[0] = (fwmactrr2 >> 8) & 0xFF;
				mac[1] = (fwmactrr2 >> 0) & 0xFF;
				mac[2] = (fwmactrr3 >> 24) & 0xFF;
				mac[3] = (fwmactrr3 >> 16) & 0xFF;
				mac[4] = (fwmactrr3 >> 8) & 0xFF;
				mac[5] = (fwmactrr3 >> 0) & 0xFF;

				seq_printf(m, "%-4d", entry);
				seq_printf(m, " %02x:%02x:%02x:%02x:%02x:%02x",
					   mac[0], mac[1], mac[2], mac[3],
					   mac[4], mac[5]);
				seq_printf(m, "  %-7s",
					   dynamic ?
					   ((ageing) ? "Aged" : "Dynamic") :
					   "Static");

				seq_puts(m, " ");
				writeTargetFields(m, portName, portNumber, dpv,
						  csdn);
				writeMirrorEtcFields(m, portName, portNumber,
						     cpu_mirror, eth_mirror,
						     srclockdpv, destlockdpv,
						     -1, ipv_enable, ipv_value);

				//default values are not printed
				if (security)
					seq_printf(m, "   SecurityLevel=%d", security);
				if (learn_disable)
					seq_printf(m, "   HwLearnDisable=%d", learn_disable);

				seq_puts(m, "\n");
			}
			break;
		}
	}
	return 0;
}

static int rswitch2_fwd_vlan_show(struct seq_file *m, void *v)
{
	u32 entry = 0;
	u32 value = 0;
	u8 i = 0;
	u32 csdn = 0;
	u8 learn_disable = 0;
	u8 security = 0;
	u32 srclockdpv = 0;
	u32 dpv = 0;
	u32 ipv_value = 0;
	u8 ipv_enable = 0;
	u8 cpu_mirror = 0;
	u8 eth_mirror = 0;
	u32 fwvlantsr1 = 0;
	u32 fwvlantsr3 = 0;

	char portName[10];
	int  portNumber = 10;

	getPortNameString(portName, &portNumber);

	seq_puts(m, "=========== VLAN-TABLE ===========================\n");
	seq_puts(m, "VID   Mirror   Src-Valid     Dst-Valid        IPV\n");
	seq_puts(m, "==================================================\n");
	for (entry = 0; entry <= 0xFFF; entry++) {
		iowrite32(entry, ioaddr + FWVLANTS);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWVLANTSR0);
			if (((value >> 31) & 0x01) != 0x0) {
				mdelay(1);
				continue;
			}
			if ((((value >> 1) & 0x01) == 0x00) &&
			    (((value >> 0) & 0x01) == 0x0)) {
				fwvlantsr1 = ioread32(ioaddr + FWVLANTSR1);
				csdn = ioread32(ioaddr + FWVLANTSR2);
				fwvlantsr3 = ioread32(ioaddr + FWVLANTSR3);

				learn_disable = (value >> 10) & 0x01;
				security = (value >> 8) & 0x01;
				srclockdpv = (fwvlantsr1 >> 0) & 0xFF;
				cpu_mirror = (fwvlantsr3 >> 21) & 0x01;
				eth_mirror = (fwvlantsr3 >> 20) & 0x01;
				ipv_enable = (fwvlantsr3 >> 19) & 0x01;
				ipv_value = (fwvlantsr3 >> 16) & 0xFF;
				dpv = (fwvlantsr3 >> 0) & 0xFF;

				seq_printf(m, "%4d ", entry);

				writeMirrorEtcFields(m, portName, portNumber,
						     cpu_mirror, eth_mirror,
						     srclockdpv, dpv, csdn,
						     ipv_enable, ipv_value);

				//default values are not printed
				if (security)
					seq_printf(m, "   SecurityLevel=%d", security);
				if (learn_disable)
					seq_printf(m, "   HwLearnDisable=%d", learn_disable);

				seq_puts(m, "\n");
			}
			break;
		}
	}

	return 0;
}

static int rswitch2_fwd_l3_show(struct seq_file *m, void *v)
{
	char str1[30], str2[30];
	u32 srcipaddr[4];
	u32 destipaddr[4];
	int mode;
	u32 entry = 0;
	u32 value = 0;
	u32 fwlthtrr1 = 0;
	u32 routingnum = 0;
	u32 fwlthtrr2 = 0;
	u32 fwlthtrr3 = 0;
	u32 fwlthtrr4 = 0;
	u32 fwlthtrr5 = 0;
	u32 fwlthtrr8 = 0;
	u32 fwlthtrr10 = 0;
	u32 routingvalid = 0;
	u8 security = 0;
	u32 ctag = 0;
	u32 stag = 0;
	u32 destipport = 0;
	u32 hash = 0;
	u32 csdn = 0;
	u32 cpu_mirror = 0;
	u32 eth_mirror = 0;
	u32 ipv_enable = 0;
	u32 ipv_value = 0;
	u32 srclockdpv = 0;
	u32 dpv = 0;
	u8 i = 0;
	u32 filter_number = 0;

	char portName[10];
	int  portNumber = 10;

	getPortNameString(portName, &portNumber);

	seq_puts(m, "=========== L3-TABLE ==================================================\n");
	seq_puts(m, "Line Format    Target-Ports    Mirror   Src-Valid    IPV  Rule#\n");
	seq_puts(m, "=======================================================================\n");

	for (entry = 0; entry <= 1023; entry++) {
		iowrite32(entry, ioaddr + FWLTHTR);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWLTHTRR0);
			if (((value >> 31) & 0x01) != 0x0) {
				mdelay(1);
				continue;
			}
			if ((((value >> 2) & 0x01) == 0) &&
			    (((value >> 1) & 0x01) == 0x01) &&
			    (((value >> 0) & 0x01) == 0x0)) {
				fwlthtrr1 = ioread32(ioaddr + FWLTHTRR1);
				security = (fwlthtrr1 >> 8);
				mode = (fwlthtrr1 >> 0) & 0x03;

				fwlthtrr2 = ioread32(ioaddr + FWLTHTRR2);
				stag = (fwlthtrr2 >> 16) & 0xFFFF;
				ctag = (fwlthtrr2 >> 0) & 0xFFFF;

				fwlthtrr3 = ioread32(ioaddr + FWLTHTRR3);
				destipport = (fwlthtrr3 >> 16) & 0xFFFF;
				hash = (fwlthtrr3 >> 0) & 0xFFFF;

				fwlthtrr4 = ioread32(ioaddr + FWLTHTRR4);
				srcipaddr[0] = ((fwlthtrr4 >> 24) & 0xFF);
				srcipaddr[1] = ((fwlthtrr4 >> 16) & 0xFF);
				srcipaddr[2] = ((fwlthtrr4 >> 8) & 0xFF);
				srcipaddr[3] = ((fwlthtrr4 >> 0) & 0xFF);

				fwlthtrr5 = ioread32(ioaddr + FWLTHTRR5);
				destipaddr[0] = ((fwlthtrr5 >> 24) & 0xFF);
				destipaddr[1] = ((fwlthtrr5 >> 16) & 0xFF);
				destipaddr[2] = ((fwlthtrr5 >> 8) & 0xFF);
				destipaddr[3] = ((fwlthtrr5 >> 0) & 0xFF);
				filter_number = fwlthtrr5;

				fwlthtrr8 = ioread32(ioaddr + FWLTHTRR8);
				srclockdpv = (fwlthtrr8 >> 16) & 0xFF;
				if ((fwlthtrr8 >> 15) & 0x01) {
					routingnum = fwlthtrr8 & 0xFF;
					routingvalid = 1;
				} else {
					routingvalid = 0;
				}

				csdn = ioread32(ioaddr + FWLTHTRR9);

				fwlthtrr10 = ioread32(ioaddr + FWLTHTRR10);
				cpu_mirror = (fwlthtrr10 >> 21) & 0x01;
				eth_mirror = (fwlthtrr10 >> 20) & 0x01;
				ipv_enable = (fwlthtrr10 >> 19) & 0x01;
				ipv_value = (fwlthtrr10 >> 15) & 0xFF;
				dpv = (fwlthtrr10 >> 0) & 0xFF;

				seq_printf(m, "%-4d", entry);

				if (mode == 0x01)
					seq_printf(m, " %-9s", "IPv4");
				else if (mode == 0x02)
					seq_printf(m, " %-9s", "IPv4+UDP");
				else if (mode == 0x03)
					seq_printf(m, " %-9s", "IPv4+TCP");
				else if (mode == 0x00)
					seq_printf(m, " %-9s", "Filter");

				writeTargetFields(m, portName, portNumber, dpv,
						  csdn);
				writeMirrorEtcFields(m, portName, portNumber,
						     cpu_mirror, eth_mirror,
						     srclockdpv, -1, -1,
						     ipv_enable, ipv_value);

				if (routingvalid)
					seq_printf(m, "    %-2d", routingnum);
				else
					seq_puts(m, "    - ");

				//default values are not printed
				if (security)
					seq_printf(m, "   SecurityLevel=%d", security);

				seq_puts(m, "\n");

				//the 2nd line
				if (mode == 0) {
					//seq_printf(m, "FilterNr=%d\n", filter_number);
				} else {
					seq_puts(m, "      \\-> ");
					sprintf(str1, "%d.%d.%d.%d",
						srcipaddr[0], srcipaddr[2],
						srcipaddr[2], srcipaddr[3]);
					sprintf(str2, "%d.%d.%d.%d:%d",
						destipaddr[0], destipaddr[1],
						destipaddr[2], destipaddr[3],
						destipport);
					seq_printf(m, "SA=%s  DA=%s  CTag=%04x  STag=%04x  Hash=%04x\n",
						   str1, str2, ctag, stag,
						   hash);
				}
			}
			break;
		}
	}

	return 0;
}

static int rswitch2_fwd_l2_l3_update_show(struct seq_file *m, void *v)
{
	u32 i;
	u32 entry;
	u32 value;
	u32 fwl23urrr1;
	u32 fwl23urrr2;
	u32 fwl23urrr3;
	u8 mac[6];
	char str1[30];
	int dpv;
	int rtag;
	int sdieul;
	int spcpul;
	int sidul;
	int cdieul;
	int cpcpul;
	int cidul;
	int msaul;
	int mdaul;
	int ttlul;
	char portName[10];
	int portNumber = 10;

	getPortNameString(portName, &portNumber);

	seq_puts(m, "=========== Routing-Update-Table ==========================================\n");
	seq_puts(m, "Rule# Dst-Valid    TTL  DA-MAC            SA-MAC   CTAG       STAG    RTAG\n");
	seq_puts(m, "================================================== Id P D === Id P D ======\n");

	for (entry = 0; entry <= 255; entry++) {
		iowrite32(entry, ioaddr + FWL23URR);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWL23URRR0);
			if (((value >> 31) & 0x01) != 0x0) {
				mdelay(1);
				continue;
			}
			if ((((value >> 16) & 0x01) == 0) &&
			    (value & 0xFFFF)) {
				fwl23urrr1 = ioread32(ioaddr + FWL23URRR1);
				fwl23urrr2 = ioread32(ioaddr + FWL23URRR2);
				fwl23urrr3 = ioread32(ioaddr + FWL23URRR3);

				dpv = value & 0xFF;
				rtag = (fwl23urrr1 >> 25) & 0x03;
				sdieul = (fwl23urrr1 >> 24) & 0x01;
				spcpul = (fwl23urrr1 >> 23) & 0x01;
				sidul = (fwl23urrr1 >> 22) & 0x01;
				cdieul = (fwl23urrr1 >> 21) & 0x01;
				cpcpul = (fwl23urrr1 >> 20) & 0x01;
				cidul = (fwl23urrr1 >> 19) & 0x01;
				msaul = (fwl23urrr1 >> 18) & 0x01;
				mdaul = (fwl23urrr1 >> 17) & 0x01;
				ttlul = (fwl23urrr1 >> 16) & 0x01;

				mac[0] = (fwl23urrr1 >> 8) & 0xFF;
				mac[1] = (fwl23urrr1 >> 0) & 0xFF;
				mac[2] = (fwl23urrr2 >> 24) & 0xFF;
				mac[3] = (fwl23urrr2 >> 16) & 0xFF;
				mac[4] = (fwl23urrr2 >> 8) & 0xFF;
				mac[5] = (fwl23urrr2 >> 0) & 0xFF;

				seq_printf(m, "%-4d ", entry);

				writeTargetFields(m, portName, portNumber,
						  dpv,  -1);

				seq_printf(m, "  %-3s", (ttlul) ? "dec" : "-");

				if (mdaul)
					sprintf(str1, "%02x:%02x:%02x:%02x:%02x:%02x",
						mac[0], mac[1], mac[2], mac[3],
						mac[4], mac[5]);
				else
					strcpy(str1, "-");
				seq_printf(m, "  %-18s", str1);

				seq_printf(m, "  %-3s", (msaul) ? "Yes" : "-");

				if (cidul)
					seq_printf(m, "  %4d", (fwl23urrr3 >> 0) & 0xFFF);
				else
					seq_puts(m, "     -");
				if (cpcpul)
					seq_printf(m, " %d", (fwl23urrr3 >> 12) & 0x07);
				else
					seq_puts(m, " -");
				if (cdieul)
					seq_printf(m, " %d", (fwl23urrr3 >> 15) & 0x01);
				else
					seq_puts(m, " -");

				if (sidul)
					seq_printf(m, "   %4d", (fwl23urrr3 >> 16) & 0xFFF);
				else
					seq_puts(m, "      -");
				if (spcpul)
					seq_printf(m, " %d", (fwl23urrr3 >> 28) & 0x07);
				else
					seq_puts(m, " -");
				if (sdieul)
					seq_printf(m, " %d", (fwl23urrr3 >> 31) & 0x01);
				else
					seq_puts(m, " -");

				seq_printf(m, "   %d", rtag);

				seq_puts(m, "\n");
			}
			break;
		}
	}

	return 0;
}

/* Tas and CBS error not covered tbd later when functionality implemented */
static int rswitch2_fwd_errors_show(struct seq_file *m, int longreport)
{
	u8 i = 0;
	char buffer[4];

	seq_puts(m, "           ");
	for (i = 0; i < board_config.eth_ports; i++) {
		getPortNameStringFull(buffer, &i);
		seq_printf(m, "%8s", buffer);
	}
	seq_printf(m, "%7s\n", "CPU");
	//seq_puts(m, "\n");
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ddntfs.error[i] = (ioread32(ioaddr + FWEIS00 +
						   i * 0x10) >> 29) & 0x01;
		if (!errors.ddntfs.set_any && errors.ddntfs.error[i])
			errors.ddntfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ddses.error[i] = (ioread32(ioaddr + FWEIS00 +
						  i * 0x10) >> 28) & 0x01;
		if (!errors.ddses.set_any && errors.ddses.error[i])
			errors.ddses.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ddfes.error[i] = (ioread32(ioaddr + FWEIS00 +
					 i * 0x10) >> 27) & 0x01;
		if (!errors.ddfes.set_any && errors.ddfes.error[i])
			errors.ddfes.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ddes.error[i] = (ioread32(ioaddr + FWEIS00 +
					i * 0x10) >> 26) & 0x01;
		if (!errors.ddes.set_any && errors.ddes.error[i])
			errors.ddes.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.wmiufs.error[i] = (ioread32(ioaddr + FWEIS00 +
					  i * 0x10) >> 25) & 0x01;
		if (!errors.wmiufs.set_any && errors.wmiufs.error[i])
			errors.wmiufs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.wmisfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					  i * 0x10) >> 24) & 0x01;
		if (!errors.wmisfs.set_any && errors.wmisfs.error[i])
			errors.wmisfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.wmffs.error[i] = (ioread32(ioaddr + FWEIS00 +
					 i * 0x10) >> 23) & 0x01;
		if (!errors.wmffs.set_any && errors.wmffs.error[i])
			errors.wmffs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.wmcfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					 i * 0x10) >> 22) & 0x01;
		if (!errors.wmcfs.set_any && errors.wmcfs.error[i])
			errors.wmcfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.smhmfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					  i * 0x10) >> 19) & 0x01;
		if (!errors.smhmfs.set_any && errors.smhmfs.error[i])
			errors.smhmfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.smhlfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					  i * 0x10) >> 18) & 0x01;
		if (!errors.smhlfs.set_any && errors.smhlfs.error[i])
			errors.smhlfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.pbntfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					  i * 0x10) >> 17) & 0x01;
		if (!errors.pbntfs.set_any && errors.pbntfs.error[i])
			errors.pbntfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwvufs.error[i] = (ioread32(ioaddr + FWEIS00 +
					   i * 0x10) >> 16) & 0x01;
		if (!errors.ltwvufs.set_any && errors.ltwvufs.error[i])
			errors.ltwvufs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwdufs.error[i] = (ioread32(ioaddr + FWEIS00 +
					   i * 0x10) >> 15) & 0x01;
		if (!errors.ltwdufs.set_any && errors.ltwdufs.error[i])
			errors.ltwdufs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwsufs.error[i] = (ioread32(ioaddr + FWEIS00 +
					   i * 0x10) >> 14) & 0x01;
		if (!errors.ltwsufs.set_any && errors.ltwsufs.error[i])
			errors.ltwsufs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwntfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					   i * 0x10) >> 13) & 0x01;
		if (!errors.ltwntfs.set_any && errors.ltwntfs.error[i])
			errors.ltwntfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwvspfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					    i * 0x10) >> 12) & 0x01;
		if (!errors.ltwvspfs.set_any && errors.ltwvspfs.error[i])
			errors.ltwvspfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwsspfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					    i * 0x10) >> 11) & 0x01;
		if (!errors.ltwsspfs.set_any && errors.ltwsspfs.error[i])
			errors.ltwsspfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwdspfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					    i * 0x10) >> 10) & 0x01;
		if (!errors.ltwdspfs.set_any && errors.ltwdspfs.error[i])
			errors.ltwdspfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.lthufs.error[i] = (ioread32(ioaddr + FWEIS00 +
					  i * 0x10) >> 3) & 0x01;
		if (!errors.lthufs.set_any && errors.lthufs.error[i])
			errors.lthufs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.lthntfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					   i * 0x10) >> 2) & 0x01;
		if (!errors.lthntfs.set_any && errors.lthntfs.error[i])
			errors.lthntfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.lthfsfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					   i * 0x10) >> 1) & 0x01;
		if (!errors.lthfsfs.set_any && errors.lthfsfs.error[i])
			errors.lthfsfs.set_any = 1;
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.lthspfs.error[i] = (ioread32(ioaddr + FWEIS00 +
					   i * 0x10) >> 0) & 0x01;
		if (!errors.lthspfs.set_any && errors.lthspfs.error[i])
			errors.lthspfs.set_any = 1;
	}
	if (errors.ddntfs.set_any || longreport) {
		seq_printf(m, "%-8s", "DDNTFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ddntfs.error[i]);
		seq_puts(m, "  Direct Descr, No Target\n");
	}
	if (errors.ddses.set_any || longreport) {
		seq_printf(m, "%-8s", "DDSES");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ddses.error[i]);
		seq_puts(m, "  Direct Descr, Security\n");
	}
	if (errors.ddfes.set_any || longreport) {
		seq_printf(m, "%-8s", "DDFES");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ddfes.error[i]);
		seq_puts(m, "  Direct Descr, Format\n");
	}
	if (errors.ddes.set_any || longreport) {
		seq_printf(m, "%-8s", "DDES");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ddes.error[i]);
		seq_puts(m, "  Direct Descr, Error\n");
	}
	if (errors.wmiufs.set_any || longreport) {
		seq_printf(m, "%-8s", "WMIUFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.wmiufs.error[i]);
		seq_puts(m, "  WaterMark IPV Unsecure\n");
	}
	if (errors.wmisfs.set_any || longreport) {
		seq_printf(m, "%-8s", "WMISFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.wmisfs.error[i]);
		seq_puts(m, "  WaterMark IPV Secure\n");
	}
	if (errors.wmffs.set_any || longreport) {
		seq_printf(m, "%-8s", "WMFFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.wmffs.error[i]);
		seq_puts(m, "  WaterMark Flush\n");
	}
	if (errors.wmcfs.set_any || longreport) {
		seq_printf(m, "%-8s", "WMCFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.wmcfs.error[i]);
		seq_puts(m, "  WaterMark Critical\n");
	}
	if (errors.smhmfs.set_any || longreport) {
		seq_printf(m, "%-8s", "SMHLFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.smhmfs.error[i]);
		seq_puts(m, "  Source MAC Migration\n");
	}
	if (errors.smhlfs.set_any || longreport) {
		seq_printf(m, "%-8s", "SMHLFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.smhlfs.error[i]);
		seq_puts(m, "  Source MAC Learning\n");
	}
	if (errors.pbntfs.set_any || longreport) {
		seq_printf(m, "%-8s", "PBNTFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.pbntfs.error[i]);
		seq_puts(m, "  Port Based No Target\n");
	}
	if (errors.ltwvufs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTWVUFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ltwvufs.error[i]);
		seq_puts(m, "  L2 VLAN Unknown\n");
	}
	if (errors.ltwdufs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTWDUFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ltwdufs.error[i]);
		seq_puts(m, "  L2 Dest Unknown\n");
	}
	if (errors.ltwsufs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTWSUFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ltwsufs.error[i]);
		seq_puts(m, "  L2 Source Unknown\n");
	}
	if (errors.ltwntfs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTWNTFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ltwntfs.error[i]);
		seq_puts(m, "  L2 No Target\n");
	}
	if (errors.ltwvspfs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTWVSPFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ltwvspfs.error[i]);
		seq_puts(m, "  L2 VLAN Source Port\n");
	}
	if (errors.ltwsspfs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTWSSPFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ltwsspfs.error[i]);
		seq_puts(m, "  L2 Source Source Port\n");
	}
	if (errors.ltwdspfs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTWDSPFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.ltwdspfs.error[i]);
		seq_puts(m, "  L2 Destination Source Port\n");
	}
	if (errors.lthufs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTHUFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.lthufs.error[i]);
		seq_puts(m, "  L3 Unknown\n");
	}
	if (errors.lthntfs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTHNTFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.lthntfs.error[i]);
		seq_puts(m, "  L3 No Target\n");
	}
	if (errors.lthfsfs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTHFSFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.lthfsfs.error[i]);
		seq_puts(m, "  L3 Format Security\n");
	}
	if (errors.lthspfs.set_any || longreport) {
		seq_printf(m, "%-8s", "LTHSPFS");
		for (i = 0; i <= board_config.eth_ports; i++)
			seq_printf(m, "%8d", errors.lthspfs.error[i]);
		seq_puts(m, "  L3 Source Port\n");
	}
	errors.global_error.arees = (ioread32(ioaddr + FWEIS1) >> 16) & 0x01;
	errors.global_error.l23uses = (ioread32(ioaddr + FWEIS1) >> 9) & 0x01;
	errors.global_error.l23uees = (ioread32(ioaddr + FWEIS1) >> 8) & 0x01;
	errors.global_error.vlantses = (ioread32(ioaddr + FWEIS1) >> 7) & 0x01;
	errors.global_error.vlantees = (ioread32(ioaddr + FWEIS1) >> 6) & 0x01;
	errors.global_error.mactses = (ioread32(ioaddr + FWEIS1) >> 5) & 0x01;
	errors.global_error.mactees = (ioread32(ioaddr + FWEIS1) >> 4) & 0x01;
	errors.global_error.lthtses = (ioread32(ioaddr + FWEIS1) >> 1) & 0x01;
	errors.global_error.lthtees = (ioread32(ioaddr + FWEIS1) >> 0) & 0x01;
	errors.global_error.macadas = (ioread32(ioaddr + FWMIS0) >> 17) & 0x01;
	errors.global_error.vlantfs = (ioread32(ioaddr + FWMIS0) >> 3) & 0x01;
	errors.global_error.mactfs = (ioread32(ioaddr + FWMIS0) >> 2) & 0x01;
	errors.global_error.lthtfs = (ioread32(ioaddr + FWMIS0) >> 0) & 0x01;
	if (errors.global_error.arees || errors.global_error.l23uses ||
	    errors.global_error.l23uees || errors.global_error.vlantses ||
	    errors.global_error.vlantees || errors.global_error.mactses ||
	    errors.global_error.mactees || errors.global_error.lthtses ||
	    errors.global_error.lthtees || errors.global_error.macadas ||
	    errors.global_error.vlantfs || errors.global_error.mactfs ||
	    errors.global_error.lthtfs)
		errors.global_error.set_any = 1;
	if (errors.global_error.set_any || longreport) {
		seq_puts(m, "\n");
		seq_puts(m, "=======================Port-Independent-Errors===========================\n");
		if (errors.global_error.arees || longreport)
			seq_printf(m, "AREES    %8d  ATS RAM ECC\n",
				   errors.global_error.arees);
		if (errors.global_error.l23uses || longreport)
			seq_printf(m, "L23USES  %8d  L2/L3 Update Security\n",
				   errors.global_error.l23uses);
		if (errors.global_error.l23uees || longreport)
			seq_printf(m, "L23UEES  %8d  Layer2/Layer3 Update ECC\n",
				   errors.global_error.l23uees);
		if (errors.global_error.vlantses || longreport)
			seq_printf(m, "VLANTSES %8d  VLAN Table Security\n",
				   errors.global_error.vlantses);
		if (errors.global_error.vlantees || longreport)
			seq_printf(m, "VLANTEES %8d  VLAN Table ECC\n",
				   errors.global_error.vlantees);
		if (errors.global_error.mactses || longreport)
			seq_printf(m, "MACTSES  %8d  MAC Table Security\n",
				   errors.global_error.mactses);
		if (errors.global_error.mactees || longreport)
			seq_printf(m, "MACTEES  %8d  MAC Table ECC\n",
				   errors.global_error.mactees);
		if (errors.global_error.lthtses || longreport)
			seq_printf(m, "LTHTSES  %8d  L3 Table Security\n",
				   errors.global_error.lthtses);
		if (errors.global_error.lthtees || longreport)
			seq_printf(m, "LTHTEES  %8d  L3 Table ECC\n",
				   errors.global_error.lthtees);
		if (errors.global_error.macadas || longreport)
			seq_printf(m, "MACADAS  %8d  MAC Address Deleted Aging\n",
				   errors.global_error.macadas);
		if (errors.global_error.vlantfs || longreport)
			seq_printf(m, "VLANTFS  %8d  VLAN Table Full\n",
				   errors.global_error.vlantfs);
		if (errors.global_error.mactfs || longreport)
			seq_printf(m, "MACTFS   %8d  MAC Table Full\n",
				   errors.global_error.mactfs);
		if (errors.global_error.lthtfs || longreport)
			seq_printf(m, "LTHTFS   %8d  L3 Table Full\n",
				   errors.global_error.lthtfs);
	}

	return 0;
}

static int rswitch2_fwd_errors_all_show(struct seq_file *m, void *v)
{
	return rswitch2_fwd_errors_show(m, 1);
}

static int rswitch2_fwd_errors_short_show(struct seq_file *m, void *v)
{
	return rswitch2_fwd_errors_show(m, 0);
}

static int rswitch2_fwd_counter_clear_func(void)
{
	memset(&counter, 0, sizeof(struct rswitch2_fwd_counter));

	return 0;
}

static int rswitch2_fwd_error_clear_func(void)
{
	int i = 0;

	for (i = 0; i <= board_config.eth_ports; i++)
		iowrite32(0xFFFFFFFF, ioaddr + FWEIS00 + i * 0x10);
	iowrite32(0xFFFFFFFF, ioaddr + FWEIS1);
	iowrite32(0xFFFFFFFF, ioaddr + FWMIS0);
	memset(&errors, 0, sizeof(struct rswitch2_fwd_error));

	return 0;
}

static ssize_t rswitch2_fwd_counters_clear(struct file *filp, const char *buff,
					   size_t len, loff_t *off)
{
	u32 ret = 0;
	u64 fwd_counter_clear = 0;

	ret = kstrtoull_from_user(buff, len, 10, &fwd_counter_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}

	rswitch2_fwd_counter_clear_func();

	return len;
}

static ssize_t rswitch2_fwd_errors_clear(struct file *filp, const char *buff,
					 size_t len, loff_t *off)
{
	u32 ret = 0;
	u64 fwd_error_clear = 0;

	ret = kstrtoull_from_user(buff, len, 10, &fwd_error_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}

	rswitch2_fwd_error_clear_func();

	return len;
}

static int rswitch2_fwd_counters_show(struct seq_file *m, void *v)
{
	u8 i = 0;
	char buffer[4];

	seq_puts(m, "           ");
	for (i = 0; i < board_config.eth_ports; i++) {
		getPortNameStringFull(buffer, &i);
		seq_printf(m, "%8s", buffer);
	}
	seq_printf(m, "%7s\n", "CPU");
	seq_printf(m, "%-8s", "DDFDN");
	for (i = 0; i < board_config.eth_ports; i++)
		seq_printf(m, "%8s", "-");
	counter.ddfdn[i] += ioread32(ioaddr + FWDDFDCN0 +
				     board_config.eth_ports * 20);
	seq_printf(m, "%8d", counter.ddfdn[i]);

	seq_puts(m, "  Direct Descr\n");
	seq_printf(m, "%-8s", "LTHFDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.lthfdn[i] += ioread32(ioaddr + FWLTHFDCN0 + i * 20);
		seq_printf(m, "%8d", counter.lthfdn[i]);
	}
	seq_puts(m, "  L3 Descr\n");
	seq_printf(m, "%-8s", "LTWFDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.ltwfdn[i] += ioread32(ioaddr + FWLTWFDCN0 + i * 20);
		seq_printf(m, "%8d", counter.ltwfdn[i]);
	}
	seq_puts(m, "  L2 Descr\n");
	seq_printf(m, "%-8s", "PBFDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.pbfdn[i] += ioread32(ioaddr + FWPBFDCN0 + i * 20);
		seq_printf(m, "%8d", counter.pbfdn[i]);
	}
	seq_puts(m, "  Port-Based Descr\n");
	seq_printf(m, "%-8s", "MHLN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.mhln[i] += ioread32(ioaddr + FWMHLCN0 + i * 20);
		seq_printf(m, "%8d", counter.mhln[i]);
	}
	seq_puts(m, "  MAC Learned\n");
	seq_printf(m, "%-8s", "ICRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.icrdn[i] += ioread32(ioaddr + FWICRDCN0 + i * 20);
		seq_printf(m, "%8d", counter.icrdn[i]);
	}
	seq_puts(m, "  Reject Integrity Check\n");
	seq_printf(m, "%-8s", "WMRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.wmrdn[i] += ioread32(ioaddr + FWWMRDCN0 + i * 20);
		seq_printf(m, "%8d", counter.wmrdn[i]);
	}
	seq_puts(m, "  Reject WaterMark\n");
	seq_printf(m, "%-8s", "DDRDN");
	for (i = 0; i < board_config.eth_ports; i++)
		seq_printf(m, "%8s", "-");
	counter.ddrdn[i] += ioread32(ioaddr + FWDDRDCN0 +
				     board_config.eth_ports * 20);
	seq_printf(m, "%8d", counter.ddrdn[i]);
	seq_puts(m, "  Reject Direct Descr\n");
	seq_printf(m, "%-8s", "LTHRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.lthrdn[i] += ioread32(ioaddr + FWLTHRDCN0 + i * 20);
		seq_printf(m, "%8d", counter.lthrdn[i]);
	}
	seq_puts(m, "  Reject L3\n");
	seq_printf(m, "%-8s", "LTWRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.ltwrdn[i] += ioread32(ioaddr + FWLTWRDCN0 + i * 20);
		seq_printf(m, "%8d", counter.ltwrdn[i]);
	}
	seq_puts(m, "  Reject L2\n");
	seq_printf(m, "%-8s", "PBRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.pbrdn[i] += ioread32(ioaddr +  FWPBRDCN0 + i * 20);
		seq_printf(m, "%8d", counter.pbrdn[i]);
	}
	seq_puts(m, "  Reject Port-Based\n");

	return 0;
}

static int rswitch2_fwd_buffer_show(struct seq_file *m, void *v)
{
	int i, j;
	u32 r, rl, rh;
	u32 total;
	const u32 ports = 5;
	int sum = 0;

	r = ioread32(ioaddr + CABPPCM);
	rh = r >> 16;
	rl = r & 0xffff;
	total = rh;
	sum = total - rl;
	seq_printf(m, "Total existing      %04x\n", total);
	seq_printf(m, "Remaining current   %04x  %d%%\n", rl, rl * 100 / total);
	r = ioread32(ioaddr + CABPLCM);
	seq_printf(m, "Remaining minimum   %04x  %d%%\n\n", r, r * 100 / total);

	seq_puts(m, "               tsn6   tsn7   tsn5   tsn4    CPU  --  reject\n");
	seq_puts(m, "Used current");
	for (i = 0; i < ports; i++) {
		r = ioread32(ioaddr + CABPCPM + i * 4);
		sum -= r;
		seq_printf(m, "   %04x", r);
	}
	r = ioread32(ioaddr + CARDNM);
	seq_printf(m, "  --  %04x\n", r);
	seq_puts(m, "Used maximum");
	for (i = 0; i < ports; i++) {
		r = ioread32(ioaddr + CABPMCPM + i * 4);
		seq_printf(m, "   %04x", r);
	}
	r = ioread32(ioaddr + CARDMNM);
	seq_printf(m, "  --  %04x\n", r);

	r = ioread32(ioaddr + CARDCN);
	seq_printf(m, "Reject total   %08x  %d\n", r, r);

	if (sum != 0)
		seq_printf(m, "Lost buffers %d (small differences normal in case of ongoing traffic)\n",
			   sum);

	seq_puts(m, "\n                        tsn6   tsn7   tsn5   tsn4    CPU\n");
	for (j = 0; j < 8; j++) {
		seq_printf(m, "Pending frames in Q#%d", j);
		for (i = 0; i < ports; i++) {
			r = ioread32(ioaddr + EATDQM0 + (j * 4) +
				     0xA000 + i * 0x2000);
			seq_printf(m, " %6d", r);
		}
		seq_puts(m, "\n");
	}

	return 0;
}

static void rswitch2_fwd_show_three_byte_filter(struct seq_file *m)
{
	u32 print0 = 0;
	u32 print1 = 0;
	char streamid0[5] = {'|', '|', '|', '|'};
	char streamid1[5] = {'|', '|', '|', '|'};
	char comp[6] = {'-', '-', '-', '-', '-'};
	u32 value0 = 0;
	u32 value1 = 0;
	u32 value2 = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_three_byte_filter *three;
	struct rswitch2_cascade_filter *cascade;

	for (j = 0; j < filter->num_three_byte_filters; j++) {
		print0 = 0;
		print1 = 0;
		strcpy(streamid0, "||||");
		strcpy(streamid1, "||||");
		strcpy(comp, "-----");
		three = &filter->three_byte[j];
		for (i = 0; i < filter->num_cascade_filters; i++) {
			cascade = &filter->cascade[i];
			for (k = 0; k < cascade->used_filter_ids; k++) {
				if (three->filterid0 ==
				    cascade->used_filter_id_num[k]) {
					streamid0[i] = '&';
					print0 = 1;
				}
				if (three->unit_mode == precise) {
					if (three->filterid1 ==
					    cascade->used_filter_id_num[k]) {
						print1 = 1;
						streamid1[i] = '&';
					}
				}
			}
		}
		if (print0) {
			seq_printf(m, "%-4d", three->filterid0);
			comp[0] = '3';
			comp[2] = three->filter_number + '0';
			if (three->unit_mode == mask)
				comp[4] = 'M';
			else if (three->unit_mode == expand)
				comp[4] = 'E';
			else if (three->unit_mode == precise)
				comp[4] = 'P';
			seq_printf(m, "%-8s", comp);
			seq_printf(m, "%-6d", three->offset);

			if (three->unit_mode != expand) {
				value0 = (three->value0 >> 16) & 0xFF;
				value1 = three->value0 & 0xFFFF;
				seq_printf(m, "0x%02x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%13s", " ");
			} else {
				value0 = (three->value0 >> 8) & 0xFFFF;
				value1 = ((three->value0 << 8) & 0xFF00) |
					 ((three->value1 >> 16) & 0xFF);
				value2 = three->value1 & 0xFFFF;
				seq_printf(m, "0x%04x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value2);
				seq_printf(m, "%-6s", " ");
			}
			if (three->unit_mode == mask) {
				value0 = (three->value1 >> 16) & 0xFF;
				value1 = three->value1 & 0xFFFF;
				seq_printf(m, "0x%02x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%-4s", " ");
			} else {
				seq_printf(m, "%-13s", "-");
			}
			seq_printf(m, "%-c", streamid0[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid0[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid0[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", streamid0[3]);
		}
		if (print1) {
			seq_printf(m, "%-4d", three->filterid1);
			comp[0] = '3';
			comp[2] = three->filter_number + '0';
			comp[4] = 'P';
			seq_printf(m, "%-8s", comp);
			seq_printf(m, "%-6d", three->offset);
			value0 = (three->value1 >> 16) & 0xFF;
			value1 = three->value1 & 0xFFFF;
			seq_printf(m, "0x%02x", value0);
			seq_printf(m, "%c", '_');
			seq_printf(m, "%04x", value1);
			seq_printf(m, "%-13s", " ");
			seq_printf(m, "%-13s", "-");
			seq_printf(m, "%-c", streamid1[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid1[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid1[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", streamid1[3]);
		}
	}
}

static void rswitch2_fwd_show_four_byte_filter(struct seq_file *m)
{
	char streamid0[5] = {'|', '|', '|', '|'};
	char streamid1[5] = {'|', '|', '|', '|'};
	char comp[6] = {'-', '-', '-', '-', '-'};
	u32 value0 = 0;
	u32 value1 = 0;
	u32 value2 = 0;
	u32 value3 = 0;
	u32 print0 = 0;
	u32 print1 = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_four_byte_filter *four;
	struct rswitch2_cascade_filter *cascade;

	for (j = 0; j < filter->num_four_byte_filters; j++) {
		print0 = 0;
		print1 = 0;
		strcpy(streamid0, "||||");
		strcpy(streamid1, "||||");
		strcpy(comp, "-----");
		four = &filter->four_byte[j];
		for (i = 0; i < filter->num_cascade_filters; i++) {
			cascade = &filter->cascade[i];
			for (k = 0; k < cascade->used_filter_ids; k++) {
				if (four->filterid0 ==
				    cascade->used_filter_id_num[k]) {
					streamid0[i] = '&';
					print0 = 1;
				}
				if (four->unit_mode == precise) {
					if (four->filterid1 ==
					    cascade->used_filter_id_num[k]) {
						print1 = 1;
						streamid1[i] = '&';
					}
				}
			}
		}
		if (print0) {
			seq_printf(m, "%-4d", four->filterid0);
			comp[0] = '4';
			comp[2] = four->filter_number + '0';
			if (four->unit_mode == mask)
				comp[4] = 'M';
			else if (four->unit_mode == expand)
				comp[4] = 'E';
			else if (four->unit_mode == precise)
				comp[4] = 'P';
			seq_printf(m, "%-8s", comp);
			seq_printf(m, "%-6d", four->offset);

			if (four->unit_mode != expand) {
				value0 = (four->value0 >> 16) & 0xFFFF;
				value1 = four->value0 & 0xFFFF;
				seq_printf(m, "0x%04x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%-11s", " ");
			} else {
				value0 = (four->value0 >> 16) & 0xFFFF;
				value1 = four->value0 & 0xFFFF;
				value2 = (four->value1 >> 16) & 0xFFFF;
				value3 = four->value1 & 0xFFFF;
				seq_printf(m, "0x%04x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value2);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value3);
				seq_printf(m, "%-1s", " ");
			}
			if (four->unit_mode == mask) {
				value0 = (four->value1 >> 16) & 0xFFFF;
				value1 = four->value1 & 0xFFFF;
				seq_printf(m, "0x%04x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%-2s", " ");
			} else {
				seq_printf(m, "%-13s", "-");
			}
			seq_printf(m, "%-c", streamid0[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid0[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid0[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", streamid0[3]);
		}
		if (print1) {
			seq_printf(m, "%-4d", four->filterid1);
			comp[0] = '4';
			comp[2] = four->filter_number + '0';
			comp[4] = 'P';
			seq_printf(m, "%-8s", comp);
			seq_printf(m, "%-6d", four->offset);
			value0 = (four->value1 >> 16) & 0xFFFF;
			value1 = four->value1 & 0xFFFF;
			seq_printf(m, "0x%04x", value0);
			seq_printf(m, "%c", '_');
			seq_printf(m, "%04x", value1);
			seq_printf(m, "%-11s", " ");
			seq_printf(m, "%-13s", "-");
			seq_printf(m, "%-c", streamid1[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid1[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid1[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", streamid1[3]);
		}
	}
}

static void rswitch2_fwd_show_range_filter(struct seq_file *m)
{
	char streamid0[5] = {'|', '|', '|', '|'};
	char streamid1[5] = {'|', '|', '|', '|'};
	char comp[4] = {'-', '-', '-'};
	u32 print0 = 0;
	u32 print1 = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_range_filter *range;
	struct rswitch2_cascade_filter *cascade;

	for (j = 0; j < filter->num_range_filters; j++) {
		print0  = 0;
		print1 = 0;
		strcpy(streamid0, "||||");
		strcpy(streamid1, "||||");
		strcpy(comp, "--");
		range = &filter->range[j];
		for (i = 0; i < filter->num_cascade_filters; i++) {
			cascade = &filter->cascade[i];
			for (k = 0; k < cascade->used_filter_ids; k++) {
				if (range->filterid0 ==
				   cascade->used_filter_id_num[k]) {
					streamid0[i] = '&';
					print0 = 1;
				}
				if (range->filterid1 ==
				    cascade->used_filter_id_num[k]) {
					print1 = 1;
					streamid1[i] = '&';
				}
			}
		}
		if (print0) {
			seq_printf(m, "%-4d", range->filterid0);
			comp[0] = 'R';
			comp[2] = range->filter_number + '0';
			seq_printf(m, "%-8s", comp);
			if (range->offset_mode == offset) {
				seq_printf(m, "%-6d", range->offset);
			} else if (range->offset_mode == vlan_tag) {
				if ((range->offset == 0x0) ||
				    (range->offset == 0x1))
					seq_printf(m, "%-6s", "STAG");
				else if ((range->offset == 0x2) ||
					(range->offset == 0x03))
					seq_printf(m, "%-6s", "CTAG");
			}
			seq_printf(m, "%03d", range->value0);
			seq_printf(m, "%c", '~');
			seq_printf(m, "%03d", range->value0 + range->range);
			if (range->offset_mode == vlan_tag) {
				if ((range->offset == 0x0) ||
				    (range->offset == 0x2)) {
					seq_printf(m, "%-15s", "(byte 0)");
					seq_printf(m, "%-13s", "-");
				} else if ((range->offset == 0x1) ||
					  (range->offset == 0x03)) {
					seq_printf(m, "%-15s", "(byte 1)");
					seq_printf(m, "%-13s", "-");
				}
			} else {
				seq_printf(m, "%-15s", "");
				seq_printf(m, "%-13s", "-");
			}
			seq_printf(m, "%-c", streamid0[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid0[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid0[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", streamid0[3]);
		}
		if (print1) {
			seq_printf(m, "%-4d", range->filterid1);
			comp[0] = 'R';
			comp[2] = range->filter_number + '0';
			seq_printf(m, "%-8s", comp);
			if (range->offset_mode == offset)
				seq_printf(m, "%-6d", range->offset);
			else if (range->offset_mode == vlan_tag) {
				if ((range->offset == 0x0) ||
				    (range->offset == 0x1))
					seq_printf(m, "%-6s", "STAG");
				else if ((range->offset == 0x2) ||
					 (range->offset == 0x03))
					seq_printf(m, "%-6s", "CTAG");
			}
			seq_printf(m, "%03d", range->value1);
			seq_printf(m, "%c", '~');
			seq_printf(m, "%03d", range->value1 + range->range);
			if (range->offset_mode == vlan_tag) {
				if ((range->offset == 0x0) ||
				    (range->offset == 0x2))
					seq_printf(m, "%-15s", "(byte 0)");
				else if ((range->offset == 0x1) ||
					 (range->offset == 0x03))
					seq_printf(m, "%-15s", "(byte 1)");
			}
			seq_printf(m, "%13s", "-");
			seq_printf(m, "%-c", streamid1[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid1[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid1[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", streamid1[3]);
		}
	}
}

static void rswitch2_fwd_show_src_valid(struct seq_file *m)
{
	u32 fwcfc = 0;
	u8 i = 0;
	u8 j = 0;
	u32 pdpv = 0;
	u32 edpv = 0;
	struct rswitch2_fwd_filter_src_valid psrc[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS];
	struct rswitch2_fwd_filter_src_valid esrc[RENESAS_RSWITCH2_MAX_PORT_AGENT];

	for (i = 0; i < board_config.eth_ports; i++) {
		memset(&psrc[i], 0,
		       sizeof(struct rswitch2_fwd_filter_src_valid));
		memset(&esrc[i], 0,
		       sizeof(struct rswitch2_fwd_filter_src_valid));
	}
	memset(&esrc[i], 0, sizeof(struct rswitch2_fwd_filter_src_valid));
	for (j = 0; j < board_config.eth_ports; j++) {
		strcpy(esrc[j].stream_str, "||||");
		strcpy(psrc[j].stream_str, "||||");
	}
	strcpy(esrc[j].stream_str, "||||");
	for (i = 0; i < RSWITCH2_MAX_CASCADE_FILTERS; i++) {
		fwcfc = ioread32(ioaddr + FWCFC0 + i * 0x40);
		if (fwcfc != 0) {
			edpv = fwcfc & 0xFFFF;
			pdpv = (fwcfc >> 16) & 0xFFFF;
			for (j = 0; j < board_config.eth_ports; j++) {
				if ((edpv >> j) & 0x01) {
					getPortNameStringFull(esrc[j].agent_name, &j);
					esrc[j].set_any = 1;
					esrc[j].stream_str[i] = 'S';
				}
				if ((pdpv >> j) & 0x01) {
					getPortNameStringFull(psrc[j].agent_name, &j);
					psrc[j].set_any = 1;
					psrc[j].stream_str[i] = 'S';
				}
			}
			if ((edpv >> j) & 0x01) {
				strcpy(esrc[j].agent_name, "CPU");
				esrc[j].set_any = 1;
				esrc[j].stream_str[i] = 'S';
			}
		}
	}
	for (j = 0; j < board_config.eth_ports; j++) {
		if (esrc[j].set_any) {
			seq_printf(m, "%-5s", esrc[j].agent_name);
			if (strcmp(esrc[j].agent_name, "CPU") == 0) {
				seq_printf(m, "%s",
					   ".  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
			} else {
				seq_printf(m, "%s",
					   "eFrames  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
			}
			seq_printf(m, "%-c", esrc[j].stream_str[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", esrc[j].stream_str[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", esrc[j].stream_str[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", esrc[j].stream_str[3]);
		}
		if (psrc[j].set_any) {
			seq_printf(m, "%-5s", psrc[j].agent_name);
			seq_printf(m, "%s",
				   "pFrames  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
			seq_printf(m, "%-c", psrc[j].stream_str[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", psrc[j].stream_str[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", psrc[j].stream_str[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", psrc[j].stream_str[3]);
		}
	}
	if (esrc[j].set_any) {
		seq_printf(m, "%-5s", esrc[j].agent_name);
		if (strcmp(esrc[j].agent_name, "CPU") == 0) {
			seq_printf(m, "%s",
				   ".  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
		} else {
			seq_printf(m, "%s",
				   "eFrames  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
		}
		seq_printf(m, "%-c", esrc[j].stream_str[0]);
		seq_printf(m, "%-s", " ");
		seq_printf(m, "%-c", esrc[j].stream_str[1]);
		seq_printf(m, "%-s", " ");
		seq_printf(m, "%-c", esrc[j].stream_str[2]);
		seq_printf(m, "%-s", " ");
		seq_printf(m, "%-c\n", esrc[j].stream_str[3]);
	}
}

static void rswitch2_fwd_show_two_byte_filter(struct seq_file *m)
{
	char streamid0[5] = {'|', '|', '|', '|'};
	char streamid1[5] = {'|', '|', '|', '|'};
	char comp[6] = {'-', '-', '-', '-', '-'};
	u32 print0 = 0;
	u32 print1 = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_two_byte_filter *two;
	struct rswitch2_cascade_filter *cascade;

	for (j = 0; j < filter->num_two_byte_filters; j++) {
		print0 = 0;
		print1 = 0;
		strcpy(streamid0, "||||");
		strcpy(streamid1, "||||");
		strcpy(comp, "-----");
		two = &filter->two_byte[j];
		for (i = 0; i < filter->num_cascade_filters; i++) {
			cascade = &filter->cascade[i];
			for (k = 0; k < cascade->used_filter_ids; k++) {
				if (two->filterid0 ==
				    cascade->used_filter_id_num[k]) {
					streamid0[i] = '&';
					print0 = 1;
				}
				if (two->unit_mode == precise) {
					if (two->filterid1 ==
					    cascade->used_filter_id_num[k]) {
						print1 = 1;
						streamid1[i] = '&';
					}
				}
			}
		}
		if (print0) {
			seq_printf(m, "%-4d", two->filterid0);
			comp[0] = '2';
			comp[2] = two->filter_number + '0';
			if (two->unit_mode == mask)
				comp[4] = 'M';
			else if (two->unit_mode == expand)
				comp[4] = 'E';
			else if (two->unit_mode == precise)
				comp[4] = 'P';
			seq_printf(m, "%-8s", comp);
			if (two->offset_mode == offset) {
				seq_printf(m, "%-6d", two->offset);
			} else if (two->offset_mode == vlan_tag) {
				if (two->offset == 0x0)
					seq_printf(m, "%-6s", "STAG");
				else if (two->offset == 0x2)
					seq_printf(m, "%-6s", "CTAG");
			}
			if (two->unit_mode != expand) {
				seq_printf(m, "0x%04x", two->value0);
				seq_printf(m, "%-16s", " ");
			} else {
				seq_printf(m, "0x%04x", two->value0);
				seq_printf(m, "%-c", '_');
				seq_printf(m, "0x%04x", two->value1);
				seq_printf(m, "%-9s", " ");
			}
			if (two->unit_mode == mask) {
				seq_printf(m, "0x%04x", two->value1);
				seq_printf(m, "%-7s", " ");
			} else {
				seq_printf(m, "%-13s", "-");
			}
			seq_printf(m, "%-c", streamid0[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid0[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid0[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", streamid0[3]);
		}
		if (print1) {
			seq_printf(m, "%-4d", two->filterid1);
			comp[0] = '2';
			comp[2] = two->filter_number + '0';
			comp[4] = 'P';
			seq_printf(m, "%-8s", comp);
			if (two->offset_mode == offset) {
				seq_printf(m, "%-6d", two->offset);
			} else if (two->offset_mode == vlan_tag) {
				if (two->offset == 0x0)
					seq_printf(m, "%-6s", "STAG");
				else if (two->offset == 0x2)
					seq_printf(m, "%-6s", "CTAG");
			}
			seq_printf(m, "0x%04x", two->value1);
			seq_printf(m, "%-16s", "");
			seq_printf(m, "%-13s", "-");
			seq_printf(m, "%-c", streamid1[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid1[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", streamid1[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", streamid1[3]);
		}
	}
}

static void rswitch2_fwd_fill_three_byte_filter(void)
{
	u32 fwthbfc;
	u32 fwthbfv0c;
	u32 fwthbfv1c;
	u32 i;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_three_byte_filter *three;

	for (i = 0; i < RSWITCH2_MAX_THREE_BYTE_FILTERS; i++) {
		fwthbfc = ioread32(ioaddr + FWTHBFC0 + i * 0x10);
		fwthbfv0c = ioread32(ioaddr + FWTHBFV0C0 + i * 0x10);
		fwthbfv1c = ioread32(ioaddr + FWTHBFV1C0 + i * 0x10);
		if (!((fwthbfc != 0) || (fwthbfv0c != 0) || (fwthbfv1c != 0)))
			continue;

		three = &filter->three_byte[filter->num_three_byte_filters];
		three->filter_number = i;
		three->unit_mode = fwthbfc & 0x3;
		three->offset = (fwthbfc >> 16) & 0xFF;
		three->value0 = fwthbfv0c;
		three->value1 = fwthbfv1c;
		three->filterid0 = 2 * (three->filter_number + RSWITCH2_MAX_TWO_BYTE_FILTERS);
		if (three->unit_mode == precise)
			three->filterid1 = three->filterid0 + 1;
		filter->num_three_byte_filters++;
	}
}

static void rswitch2_fwd_fill_four_byte_filter(void)
{
	u32 fwfobfc;
	u32 fwfobfv0c;
	u32 fwfobfv1c;
	u32 i;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_four_byte_filter *four;

	for (i = 0; i < RSWITCH2_MAX_FOUR_BYTE_FILTERS; i++) {
		fwfobfc = ioread32(ioaddr + FWFOBFC0 + i * 0x10);
		fwfobfv0c = ioread32(ioaddr + FWFOBFV0C0 + i * 0x10);
		fwfobfv1c = ioread32(ioaddr + FWFOBFV1C0 + i * 0x10);
		if (!((fwfobfc != 0) || (fwfobfv0c != 0) || (fwfobfv1c != 0)))
			continue;

		four = &filter->four_byte[filter->num_four_byte_filters];

		four->filter_number = i;
		four->unit_mode = fwfobfc & 0x3;
		four->offset = (fwfobfc >> 16) & 0xFF;
		four->value0 = fwfobfv0c;
		four->value1 = fwfobfv1c;
		four->filterid0 = 2 * (four->filter_number +
				       RSWITCH2_MAX_TWO_BYTE_FILTERS +
				       RSWITCH2_MAX_THREE_BYTE_FILTERS);
		if (four->unit_mode == precise)
			four->filterid1 = four->filterid0 + 1;
		filter->num_four_byte_filters++;
	}
}

static void rswitch2_fwd_fill_range_filter(void)
{
	u32 fwrfc;
	u32 fwrfvc;
	u32 i;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_range_filter *range;

	for (i = 0; i < RSWITCH2_MAX_RANGE_FILTERS; i++) {
		fwrfc = ioread32(ioaddr + FWRFC0 + i * 0x10);
		fwrfvc = ioread32(ioaddr + FWRFVC0 + i * 0x10);
		if (!((fwrfc != 0) || (fwrfvc != 0)))
			continue;

		range = &filter->range[filter->num_range_filters];

		range->filter_number = i;
		range->offset_mode = (fwrfc >> 8) & 0x01;
		range->offset = (fwrfc >> 16) & 0xFF;
		range->value0 = fwrfvc & 0xFF;
		range->value1 = (fwrfvc >> 8) & 0xFF;
		range->range = (fwrfvc >> 16) & 0xF;
		range->filterid0 = 2 * (range->filter_number +
					RSWITCH2_MAX_TWO_BYTE_FILTERS +
					RSWITCH2_MAX_THREE_BYTE_FILTERS +
					RSWITCH2_MAX_FOUR_BYTE_FILTERS);
		range->filterid1 = range->filterid0 + 1;
		filter->num_range_filters++;
	}
}

static void rswitch2_fwd_fill_two_byte_filter(void)
{
	u32 fwtwbfc;
	u32 fwtwbfvc;
	u32 i;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_two_byte_filter *two;

	for (i = 0; i < RSWITCH2_MAX_TWO_BYTE_FILTERS; i++) {
		fwtwbfc = ioread32(ioaddr + FWTWBFC0 + i * 0x10);
		fwtwbfvc = ioread32(ioaddr + FWTWBFVC0 + i * 0x10);
		if (!((fwtwbfc != 0) || (fwtwbfvc != 0)))
			continue;

		two = &filter->two_byte[filter->num_two_byte_filters];

		two->filter_number = i;
		two->unit_mode = fwtwbfc & 0x3;
		two->offset_mode = (fwtwbfc >> 8) & 0x01;
		two->offset = (fwtwbfc >> 16) & 0xFF;
		two->value0 = fwtwbfvc & 0xFFFF;
		two->value1 = (fwtwbfvc >> 16) & 0xFFFF;
		two->filterid0 = 2 * two->filter_number;
		if (two->unit_mode == precise)
			two->filterid1 = two->filterid0 + 1;
		filter->num_two_byte_filters++;
	}
}

static void rswitch2_fwd_fill_cascade_filter(void)
{
	u32 fwcfc;
	u32 fwcfmc;
	u32 i;
	u32 j;
	struct rswitch2_fwd_filter_config *filter = &filter_tbl_config;
	struct rswitch2_cascade_filter *cascade;

	for (i = 0; i < RSWITCH2_MAX_CASCADE_FILTERS; i++) {
		fwcfc = ioread32(ioaddr + FWCFC0 + i * 0x40);
		if (fwcfc == 0)
			continue;

		cascade = &filter->cascade[filter->num_cascade_filters];
		cascade->filter_number = i;
		for (j = 0; j < RSWITCH2_MAX_USED_CASCADE_FILTERS; j++) {
			fwcfmc = ioread32(ioaddr + FWCFMC00 + i * 0x40 + j * 4);
			if (fwcfmc == 0)
				continue;

			cascade->used_filter_id_num[j] = fwcfmc & 0xFF;
			cascade->used_filter_ids++;
		}
		filter->num_cascade_filters++;
	}
}

static int rswitch2_fwd_filters_show(struct seq_file *m, void *v)
{
	seq_puts(m, "=========== Filter-Table =======================================\n");
	seq_puts(m, "ID  Comp    Offs  Value                 Mask         Stream ID\n");
	seq_puts(m, "==================================================== 0 1 2 3 ===\n");
	memset(&filter_tbl_config, 0, sizeof(struct rswitch2_fwd_filter_config));
	rswitch2_fwd_fill_two_byte_filter();
	rswitch2_fwd_fill_three_byte_filter();
	rswitch2_fwd_fill_four_byte_filter();
	rswitch2_fwd_fill_range_filter();
	rswitch2_fwd_fill_cascade_filter();
	rswitch2_fwd_show_two_byte_filter(m);
	rswitch2_fwd_show_three_byte_filter(m);
	rswitch2_fwd_show_four_byte_filter(m);
	rswitch2_fwd_show_range_filter(m);
	rswitch2_fwd_show_src_valid(m);

	return 0;
}

static int rswitch2_fwd_l3_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_fwd_l3_show, NULL);
}

static int rswitch2_fwd_l2_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_fwd_l2_show, NULL);
}

static int rswitch2_fwd_vlan_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_fwd_vlan_show, NULL);
}

static int rswitch2_fwd_errors_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_fwd_errors_all_show, NULL);
}

static int rswitch2_fwd_errors_short_open(struct inode *inode,
					  struct file *file)
{
	return single_open(file, rswitch2_fwd_errors_short_show, NULL);
}

static int rswitch2_fwd_counters_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_fwd_counters_show, NULL);
}

static int rswitch2_fwd_l2_l3_update_open(struct inode *inode,
					  struct file *file)
{
	return single_open(file, rswitch2_fwd_l2_l3_update_show, NULL);
}

static int rswitch2_fwd_buffer_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_fwd_buffer_show, NULL);
}

static int rswitch2_fwd_filters_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_fwd_filters_show, NULL);
}

static int rswitch2_fwd_ip_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_fwd_ip_show, NULL);
}

static const struct file_operations rswitch2_fwd_l3_fops = {
	.owner = THIS_MODULE,
	.write = rswitch2_fwd_l3_clear,
	.open = rswitch2_fwd_l3_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_errors_short_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_errors_short_open,
	.write = rswitch2_fwd_errors_clear,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_errors_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_errors_open,
	.write = rswitch2_fwd_errors_clear,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_counters_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_counters_open,
	.write = rswitch2_fwd_counters_clear,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_l2_l3_update_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_l2_l3_update_open,
	.write = rswitch2_fwd_l2_l3_update_clear,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_l2_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_l2_open,
	.write = rswitch2_fwd_l2_clear,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_vlan_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_vlan_open,
	.write = rswitch2_fwd_l2_vlan_clear,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_buffer_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_buffer_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_filters_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_filters_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_fwd_ip_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_fwd_ip_open,
	.read = seq_read,
	.write = rswitch2_fwd_ip_clear,
	.llseek = seq_lseek,
	.release = single_release,
};

static void rswitch2_fwd_create_proc_entry(void)
{
	/* create root & sub-directories */
	proc_create(RSWITCH2_FWD_PROC_FILE_ERRORS, 0000, root_dir,
		    &rswitch2_fwd_errors_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_ERRORS_SHORT, 0000, root_dir,
		    &rswitch2_fwd_errors_short_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_COUNTERS, 0000, root_dir,
		    &rswitch2_fwd_counters_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_L3, 0000, root_dir,
		    &rswitch2_fwd_l3_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_L2_L3_UPDATE, 0000, root_dir,
		    &rswitch2_fwd_l2_l3_update_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_L2, 0000, root_dir,
		    &rswitch2_fwd_l2_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_VLAN, 0000, root_dir,
		    &rswitch2_fwd_vlan_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_BUFFER, 0000, root_dir,
		    &rswitch2_fwd_buffer_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_FILTERS, 0000, root_dir,
		    &rswitch2_fwd_filters_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_IP, 0000, root_dir,
		    &rswitch2_fwd_ip_fops);
}

static void rswitch2_fwd_remove_proc_entry(void)
{
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_ERRORS, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_ERRORS_SHORT, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_COUNTERS, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_L3, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_L2_L3_UPDATE, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_L2, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_VLAN, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_BUFFER, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_FILTERS, root_dir);
	remove_proc_entry(RSWITCH2_FWD_PROC_FILE_IP, root_dir);
}

int rswitch2_fwd_init(struct ethports_config *board_info)
{
	memcpy(&board_config, board_info, sizeof(board_config));
	rswitch2_fwd_default_config();
	rswitch2fwd_drv_probe_chrdev();
	rswitch2_fwd_counter_clear_func();
	rswitch2_fwd_error_clear_func();
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
	rswitch2_fwd_create_proc_entry();
#endif
	return 0;
}
#endif

int rswitch2_fwd_exit(void)
{
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
	rswitch2_fwd_remove_proc_entry();
#endif
	rswitch2fwd_drv_remove_chrdev();

	return 0;
}
