/**
    @brief  Renesas R-SWITCH2 Switch Driver

    Provide APIs to Forwarding Engine  of Switch Driver

    @bugs

    @notes

    @author
        Asad Kamal


    Copyright (C) 2020 Renesas Electronics Corporation

    This program is free software; you can redistribute it and/or modify it
    under the terms and conditions of the GNU general Public License,
    version 2, as published by the Free Software Foundation.

    This program is distributed in the hope it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU general Public License for
    more details.
    You should have received a copy of the GNU general Public License along with
    this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

    The full GNU general Public License is included in this distribution in
    the file called "COPYING".
*/

#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/cache.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/net_tstamp.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/gpio.h>
#include "rswitch2_reg.h"
#include "rswitch2_eth.h"
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/renesas_rswitch2.h>
#include <linux/renesas_rswitch2_platform.h>
#include <linux/renesas_vc2_platform.h>
#include "rswitch2_fwd.h"
#define  L2_MAC_FWD
#define  VLAN_FWD
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
#define RSWITCH2_FWD_PROC_FILE_ERRORS       	"fwderrors-all"
#define RSWITCH2_FWD_PROC_FILE_ERRORS_SHORT	"fwderrors"
#define RSWITCH2_FWD_PROC_FILE_COUNTERS     	"fwdcounters"
#define RSWITCH2_FWD_PROC_FILE_FILTERS    	"filter"
#define RSWITCH2_FWD_PROC_FILE_L3           	"L3"
#define RSWITCH2_FWD_PROC_FILE_L2           	"l2"
#define RSWITCH2_FWD_PROC_FILE_VLAN         	"vlan"
#define RSWITCH2_FWD_PROC_FILE_L2_L3_UPDATE 	"updateL2L3"
#define RSWITCH2_FWD_PROC_FILE_BUFFER       	"buffer"
#endif
/********************************************* Global Variables ************************************************/
static struct rswitch2_fwd_config             rswitch2fwd_config;
static struct  ethports_config      board_config;
// Static config (passed from user)
static struct rswitch2_fwd_config             rswitch2fwd_confignew;
static struct rswitch2_fwd_filter_config      filter_tbl_config;
static int vlan_tbl_reconfig = 0;
static int mac_tbl_reconfig = 0;
static int l3_tbl_reconfig = 0;
static int l23_tbl_reconfig = 0;
struct rswitch2_l2_fwd_config l2_fwd_config;
struct rswitch2_ipv_fwd_config ipv_fwd_config;
struct rswitch2_ipv4_stream_fwd_config ipv4_stream_config;
struct rswitch2_l3_stream_fwd l3_stream_fwd;
static int            rswitch2fwd_devmajor;
module_param(rswitch2fwd_devmajor, int, 0440);
static struct class * rswitch2fwd_devclass = NULL;
static dev_t          rswitch2fwd_dev;
static struct cdev    rswitch2fwd_cdev;
static struct device  rswitch2fwd_device;
extern struct net_device        ** ppndev;  //for the interface names
static struct rswitch2_fwd_counter counter;
static struct rswitch2_fwd_error errors;
#define RSWITCH2_FWD_CTRL_MINOR (127)
#ifdef L2_MAC_FWD

/**
    @brief  L2 table Reset Function

    @return int
*/
static int rswitch2_l2_mac_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;
	iowrite32((1 << 0), ioaddr + FWMACTIM);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWMACTIM, (1 << 0));
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWMACTIM);
		if ((value >> 1) == 0x01) {
			return 0;
		}
		mdelay(1);
	}

	printk("mac Reset failed");
	return -1;
}
#endif


#ifdef IP_TBL_FWD
/**
    @brief  IP table Reset Function

    @return int
*/
static int rswitch2_ip_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;
	iowrite32((1 << 0), ioaddr + FWIPTIM);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWIPTIM, (1 << 0));
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWIPTIM);
		if ((value >> 1) == 0x01) {
			return 0;
		}
		mdelay(1);
	}

	printk("ip Reset failed");
	return -1;
}
#endif


#ifdef VLAN_FWD
/**
    @brief  VLAN Table Reset Function

    @return int
*/
static int rswitch2_l2_vlan_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;
	iowrite32((1 << 0), ioaddr + FWVLANTIM);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWVLANTIM, (1 << 0));
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWVLANTIM);
		if ((value >> 1) == 0x01) {
			return 0;
		}
		mdelay(1);
	}

	printk("vlan Reset failed");
	return -1;
}
#endif


/**
    @brief Write L2 Table Clear Function(Using Table Reset)

    @param  struct file *
    @param  const char *
    @param  size_t
    @param  loff_t *

    @return ssize_t
*/
static ssize_t rswitch2_fwd_l2_clear(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	u32 ret = 0;
	u64 l2_tbl_clear = 0;
	ret = kstrtoull_from_user(buff, len, 10, &l2_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}
	else {
		rswitch2_l2_mac_tbl_reset();
		return len;
	}
}



/**
    @brief Write L2  VLAN Table Clear Function(Using Table Reset)

    @param  struct file *
    @param  const char *
    @param  size_t
    @param  loff_t *

    @return ssize_t
*/
static ssize_t rswitch2_fwd_l2_vlan_clear(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	u32 ret = 0;
	u64 l2_tbl_clear = 0;
	ret = kstrtoull_from_user(buff, len, 10, &l2_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}
	else {
		rswitch2_l2_vlan_tbl_reset();
		return len;
	}
}



/**
    @brief  L2-3 Update Table Reset Function

    @return int
*/
static int rswitch2_l23_update_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;
	iowrite32((1 << 0), ioaddr + FWL23UTIM);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWL23UTIM, (1 << 0));
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWL23UTIM);
		if ((value >> 1) == 0x01) {
			return 0;
		}
		mdelay(1);
	}

	printk("L2-3 Update Table Reset failed");
	return -1;
}


/**
    @brief Write L2 L3 Table Update Clear Function(Using Table Reset)

    @param  struct file *
    @param  const char *
    @param  size_t
    @param  loff_t *

    @return ssize_t
*/
static ssize_t rswitch2_fwd_l2_l3_update_clear(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	u32 ret = 0;
	u64 l2l3_update_tbl_clear = 0;
	ret = kstrtoull_from_user(buff, len, 10, &l2l3_update_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}
	else {
		rswitch2_l23_update_tbl_reset();
		return len;
	}
}


/**
    @brief  L3 Table Reset Function

    @return int
*/
static int rswitch2_l3_tbl_reset(void)
{
	int i = 0;
	u32 value = 0;
	iowrite32((1 << 0), ioaddr + FWLTHTIM);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTIM, (1 << 0));
#endif
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWLTHTIM);
		if ((value >> 1) == 0x01) {
			return 0;
		}
		mdelay(1);
	}

	printk("L3 Table Reset failed");
	return -1;
}


/**
    @brief Write L3 Table Clear Function(Using Table Reset)

    @param  struct file *
    @param  const char *
    @param  size_t
    @param  loff_t *

    @return ssize_t
*/
static ssize_t rswitch2_fwd_l3_clear(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	u32 ret = 0;
	u64 l3_tbl_clear = 0;
	ret = kstrtoull_from_user(buff, len, 10, &l3_tbl_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}
	else {
		rswitch2_l3_tbl_reset();
		return len;
	}
}


#ifdef IP_TBL_FWD
/**
    @brief  IP Table Learn Status

    @return int
*/
int rswitch2_ipv_learn_status(void)
{
	int i = 0;
	int value = 0;
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWIPTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 2) & 0x01) || ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
				printk("ip learn fail value = %x \n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}
	printk("ip entry Learn Timeout \n");
	return -1;
}
#endif


#ifdef L2_MAC_FWD
/**
    @brief  MAC Table Learn Status

    @return int
*/
int rswitch2_mac_learn_status(void)
{
	int i = 0;
	int value = 0;
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWMACTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 2) & 0x01) || ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
				printk("mac learn fail value = %x \n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}
	printk("mac entry Learn Timeout \n");
	return -1;
}
#endif


#ifdef VLAN_FWD
/**
    @brief  VLAN Table Learn Status

    @return int
*/
int rswitch2_vlan_learn_status(void)
{
	int i = 0;
	int value = 0;
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWVLANTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 2) & 0x01) || ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
				printk("vlan learn fail value = %x \n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}
	printk("vlan entry Learn Timeout \n");
	return -1;
}
#endif


/**
    @brief  L23 Update Table Learn Status

    @return int
*/
int rswitch2_l23_update_learn_status(void)
{
	int i = 0;
	int value = 0;
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWL23URLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
				printk("L23  Update learn fail value = %x \n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}
	printk("L23 Update entry Learn Timeout \n");
	return -1;
}


/**
    @brief  L3 Table Learn Status

    @return int
*/
int rswitch2_l3_learn_status(void)
{
	int i = 0;
	int value = 0;
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWLTHTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if (((value >> 2) & 0x01) || ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
				printk("L3 learn fail value = %x \n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}
	printk("L3 entry Learn Timeout \n");
	return -1;
}

/**
    @brief  MCAST Table Learn Status

    @return int

*/
int rswitch2_mcast_tbl_learn_status(void)
{
	int i = 0;
	int value = 0;
	void __iomem      *  gwca_addr =  NULL;
	gwca_addr = ioaddr + board_config.gwca0.start;
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(gwca_addr + GWMSTLR);
		if (((value >> 31) & 0x01) == 0x0) {
			if ( ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
				printk("MCAST Table learn fail value = %x \n", value);
				return -1;
			}
			return 0;
		}
		mdelay(1);
	}
	printk("MCAST Table entry Learn Timeout \n");
	return -1;
}


/**
    @brief  IPV4 Stream Id Generation

    @param  hash_value
    @param  entry
    @param  config   Configurationn Structure

    @return stream_id
*/
static struct rswitch2_ipv4_stream_id rswitch2_gen_ipv4_stream(u32 hash_value,
                struct rswitch2_ipv4_stream_fwd_entry *entry, struct rswitch2_ipv4_stream_fwd_config *config)
{
	static struct rswitch2_ipv4_stream_id stream_id;
	if ((config->stag_dei) || (config->stag_pcp) || (config->stag_vlan)) {
		stream_id.stag =  (entry->stag_pcp << 13) |  (entry->stag_dei << 12)
		                  | (entry->stag_vlan );
	}
	else {
		stream_id.stag = 0;
	}

	if ((config->ctag_dei) || (config->ctag_pcp) || (config->ctag_vlan)) {
		stream_id.ctag =  (entry->ctag_pcp << 13) |  (entry->ctag_dei << 12)
		                  | (entry->ctag_vlan );
	}
	else {
		stream_id.ctag = 0;
	}

	if (config->destination_port) {
		stream_id.destination_port = entry->destination_port;
	}
	else {
		stream_id.destination_port = 0;
	}
	stream_id.hash_value =  hash_value;
	if (config->source_ip) {
		stream_id.source_ip = (entry->source_ip_addr[0] << 24) | (entry->source_ip_addr[1] << 16) | (entry->source_ip_addr[2] << 8) | (entry->source_ip_addr[3] << 0);
	}
	else {
		stream_id.source_ip = 0;
	}
	if (config->destination_ip) {
		stream_id.destination_ip = (entry->destination_ip_addr[0] << 24) | (entry->destination_ip_addr[1] << 16) |
		                           (entry->destination_ip_addr[2] << 8) | (entry->destination_ip_addr[3] << 0);
	}
	else {
		stream_id.destination_ip = 0;
	}
	stream_id.frame_fmt = entry->ipv4_stream_frame_format_code;
	return stream_id;
}


/**
    @brief  IPV4 General Configuration

    @param  streamconfig Stream Configuration Structure
    @param  hashconfig  Hash Configuration Structure

    @return int
*/
static int rswitch2_ipv4_stream_gen_config(struct rswitch2_ipv4_stream_fwd_config *streamconfig, struct rswitch2_ipv4_hash_configuration *hashconfig)
{
	u32 fwip4sc_val = 0;
	fwip4sc_val = (streamconfig->destination_port << 24) | (streamconfig->destination_ip << 23) | (streamconfig->source_ip << 22)
	              | (streamconfig->ctag_dei << 21) | (streamconfig->ctag_pcp << 20) | (streamconfig->ctag_vlan << 19)
	              | (streamconfig->stag_dei << 18) | (streamconfig->stag_pcp << 17) | (streamconfig->stag_vlan << 16)
#if 1
	              | (hashconfig->destination_port << 12) | (hashconfig->source_port << 11) | (hashconfig->destination_ip << 9)
	              | (hashconfig->source_ip << 8) | (hashconfig->protocol << 10) | (hashconfig->ctag_dei << 7)
	              | (hashconfig->ctag_pcp << 6) | (hashconfig->ctag_vlan << 5) | (hashconfig->stag_dei << 4)
	              | (hashconfig->stag_pcp << 3) | (hashconfig->stag_vlan << 2) |(hashconfig->dest_mac << 0) | (hashconfig->src_mac << 1);
#endif
	iowrite32(fwip4sc_val, ioaddr + FWIP4SC);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWIP4SC, fwip4sc_val);
#endif
	return 0;
}


/**
    @brief  IPV4 Hash Calculation

    @param  config   Hash Configuration
    @param  entry   IPV4 Entry

    @return int
*/
static int rswitch2_calc_ipv4_hash(struct rswitch2_ipv4_hash_configuration *config,  struct rswitch2_ipv4_stream_fwd_entry *entry)
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
		iowrite32(((entry->dest_mac[3]<<24) | (entry->dest_mac[2]<<16) | (entry->dest_mac[1]<<8) | (entry->dest_mac[0]<<0)),ioaddr +  FWSHCR0);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWSHCR0,
		       ((entry->dest_mac[3]<<24) | (entry->dest_mac[2]<<16) | (entry->dest_mac[1]<<8) | (entry->dest_mac[0]<<0)));
#endif
		fwshcr1_val = (entry->dest_mac[5]<<24) | (entry->dest_mac[4]<<16);
		if (!config->src_mac) {
			iowrite32(fwshcr1_val, ioaddr + FWSHCR1);
#ifdef DEBUG
			printk("Register address= %x Write value = %x \n",FWSHCR1,
			       fwshcr1_val);
#endif
		}
	}
	if (config->src_mac) {
		fwshcr1_val |= (entry->src_mac[1]<<8) | (entry->src_mac[0]<<0);
		iowrite32(fwshcr1_val, ioaddr + FWSHCR1);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWSHCR1,
		       fwshcr1_val);
#endif
		fwshcr2_val = (entry->src_mac[5]<<24) | (entry->src_mac[4]<<16) | (entry->src_mac[3]<<8) | (entry->src_mac[2]<<0);
		iowrite32(fwshcr2_val, ioaddr + FWSHCR2);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWSHCR2,
		       fwshcr2_val);
#endif
	}

	if ((config->ctag_dei) || (config->stag_dei) || (config->ctag_pcp)
	                || (config->stag_pcp)  || (config->ctag_vlan) || (config->stag_vlan)) {
		fwshcr3_val =  (entry->stag_pcp << 29) |  (entry->stag_dei << 28)
		               | (entry->stag_vlan << 16) | (entry->ctag_pcp << 13)
		               | (entry->ctag_dei << 12)  | (entry->ctag_vlan);
		iowrite32(fwshcr3_val, ioaddr + FWSHCR3);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWSHCR3,
		       fwshcr3_val);
#endif
	}
	iowrite32(0x0, ioaddr + FWSHCR4);

	if (config->source_ip) {
		src_ip = (entry->source_ip_addr[0] << 24) | (entry->source_ip_addr[1] << 16) |
		         (entry->source_ip_addr[2] << 8) | (entry->source_ip_addr[3] << 0);
		iowrite32(src_ip, ioaddr + FWSHCR5);
	}
	if (config->destination_ip) {
		dest_ip = (entry->destination_ip_addr[0] << 24) | (entry->destination_ip_addr[1] << 16) |
		          (entry->destination_ip_addr[2] << 8) | (entry->destination_ip_addr[3] << 0);
		iowrite32(dest_ip, ioaddr + FWSHCR9);
	}
	if ((config->destination_port) || (config->source_port)) {
		fwshcr13_val = entry->destination_port | (entry->source_port << 16);
		iowrite32(fwshcr13_val, ioaddr + FWSHCR13);
	}
	else {
		iowrite32(fwshcr13_val, ioaddr + FWSHCR13);
	}
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = ioread32(ioaddr + FWSHCRR);
		if (((value >> 31) & 0x01) == 0x0) {
			return (value & 0xFFFF);
		}
		mdelay(1);
	}
	printk("hash calculation Timeout \n");
	return -1;
}


/**
    @brief  IPV4 L3 table Configuration

    @param  config L3 Stream Forwarding Configuration

    @return int
*/
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
	stream_config = &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config;
	stream_id = kmalloc(sizeof(struct rswitch2_ipv4_stream_id), GFP_KERNEL);

	for (entries = 0; entries < stream_config->ipv4_stream_fwd_entries; entries++) {
		dpv = 0;
		ds_dpv = 0;
		if(!stream_config->ipv4_stream_fwd_entry[entries].filter_enable) {
			memcpy(stream_id, &stream_config->ipv4_stream_fwd_entry[entries].stream_id,
			sizeof(struct rswitch2_ipv4_stream_id));
			fwlthtl0_val = (stream_config->ipv4_stream_fwd_entry[entries].security_learn
			<< 8) | stream_id->frame_fmt;
			fwlthtl1_val = (stream_id->stag << 16) | stream_id->ctag;
			fwlthtl2_val = (stream_id->destination_port << 16) | stream_id->hash_value;
			fwlthtl3_val = stream_id->source_ip;
			fwlthtl4_val = stream_id->destination_ip;
		} else {
			fwlthtl0_val = (stream_config->ipv4_stream_fwd_entry[entries].security_learn
			<< 8);
			fwlthtl4_val = stream_config->ipv4_stream_fwd_entry[entries].filter_number;
		}
		iowrite32(fwlthtl0_val, ioaddr + FWLTHTL0);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHTL0,
		       fwlthtl0_val);
#endif
		iowrite32(fwlthtl1_val, ioaddr + FWLTHTL1);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHTL1,
		       fwlthtl1_val);
#endif
		iowrite32(fwlthtl2_val, ioaddr + FWLTHTL2);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHTL2,
		       fwlthtl2_val);
#endif
		iowrite32(fwlthtl3_val, ioaddr + FWLTHTL3);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHTL3,
		       fwlthtl3_val);
#endif
		iowrite32(fwlthtl4_val, ioaddr + FWLTHTL4);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHTL4,
		       fwlthtl4_val);
#endif
		for (port_lock = 0; port_lock < stream_config->ipv4_stream_fwd_entry[entries].source_lock.source_ports; port_lock++) {
			ds_dpv |= 1 << stream_config->ipv4_stream_fwd_entry[entries].source_lock.source_port_number[port_lock];

		}
		ds_dpv  |=  stream_config->ipv4_stream_fwd_entry[entries].source_lock.cpu << board_config.eth_ports;
		fwlthtl7_val = (ds_dpv << 16) | (stream_config->ipv4_stream_fwd_entry[entries].routing_valid << 15)
		               | (stream_config->ipv4_stream_fwd_entry[entries].routing_number << 0);
		iowrite32(fwlthtl7_val, ioaddr + FWLTHTL7);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHTL7,
		       fwlthtl7_val);
#endif
		iowrite32(stream_config->ipv4_stream_fwd_entry[entries].csdn, ioaddr + FWLTHTL80);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHTL80,
		       stream_config->ipv4_stream_fwd_entry[entries].csdn);
#endif
		for (dest_ports = 0;
		                dest_ports < stream_config->ipv4_stream_fwd_entry[entries].destination_vector_config.dest_eth_ports;
		                dest_ports++) {
			dpv |= 1 << stream_config->ipv4_stream_fwd_entry[entries].destination_vector_config.port_number[dest_ports];
		}
		dpv |= stream_config->ipv4_stream_fwd_entry[entries].destination_vector_config.cpu << board_config.eth_ports;
		fwlthtl9_val = stream_config->ipv4_stream_fwd_entry[entries].mirroring_config.cpu_mirror_enable << 21
		               | stream_config->ipv4_stream_fwd_entry[entries].mirroring_config.eth_mirror_enable << 20
		               | stream_config->ipv4_stream_fwd_entry[entries].mirroring_config.ipv_config.ipv_update_enable  << 19
		               | stream_config->ipv4_stream_fwd_entry[entries].mirroring_config.ipv_config.ipv_value  << 16
		               | dpv;
		iowrite32(fwlthtl9_val, ioaddr + FWLTHTL9);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHTL9,
		       fwlthtl9_val);
#endif
		ret = rswitch2_l3_learn_status();
		if (ret < 0) {
			printk("L3 Learning Failed for entry num= %d\n", entries);
		}
	}
	if(stream_id != NULL) {
		kfree(stream_id);
	}
	return 0;
}

static int rswitch2_l3_tbl_two_byte_filter_config(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 fwtwbfc = 0;
	u32 fwtwbfvc = 0;
	/* Below line not mentioned in spec, need to check if necessary */
	for(filter = 0; filter < RSWITCH2_MAX_TWO_BYTE_FILTERS; filter++) {
		
		iowrite32(0,ioaddr + (FWTWBFVC0 + (filter * 0x10)));
		iowrite32(0,ioaddr + (FWTWBFC0 + (filter * 0x10)));
	}
	for(filter = 0; filter < config->num_two_byte_filters; filter++) {
		fwtwbfc = config->two_byte[filter].offset << 16 |
				config->two_byte[filter].offset_mode << 8 |
				config->two_byte[filter].unit_mode;
		fwtwbfvc = config->two_byte[filter].value1 << 16 |
				config->two_byte[filter].value0;

		iowrite32(fwtwbfvc,ioaddr + (FWTWBFVC0 + (config->two_byte[filter].filter_number * 0x10)));
		iowrite32(fwtwbfc,ioaddr + (FWTWBFC0 + (config->two_byte[filter].filter_number * 0x10)));
	}
	return 0;

}
static int rswitch2_l3_tbl_three_byte_filter_config(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 fwthbfc = 0;
	u32 fwthbfv0c = 0;
	u32 fwthbfv1c = 0;
	/* Below line not mentioned in spec, need to check if necessary */
	for(filter = 0; filter < RSWITCH2_MAX_THREE_BYTE_FILTERS; filter++) {
		
		iowrite32(0,ioaddr + (FWTHBFV0C0 + (filter * 0x10)));
		iowrite32(0,ioaddr + (FWTHBFV1C0 + (filter * 0x10)));
		iowrite32(0,ioaddr + (FWTHBFC0 + (filter * 0x10)));
	}
	/*Not mentioned line in spec ends */
	for(filter = 0; filter < config->num_three_byte_filters; filter++) {
		fwthbfc = config->three_byte[filter].offset << 16 |
				config->three_byte[filter].unit_mode;
		fwthbfv0c = config->three_byte[filter].value0;
		fwthbfv1c = config->three_byte[filter].value1;

		iowrite32(fwthbfv0c,ioaddr + (FWTHBFV0C0 + (config->three_byte[filter].filter_number * 0x10)));
		iowrite32(fwthbfv1c,ioaddr + (FWTHBFV1C0 + (config->three_byte[filter].filter_number * 0x10)));
		iowrite32(fwthbfc,ioaddr + (FWTHBFC0 + (config->three_byte[filter].filter_number * 0x10)));
	}
	return 0;

}

static int rswitch2_l3_tbl_four_byte_filter_config(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 fwfobfc = 0;
	u32 fwfobfv0c = 0;
	u32 fwfobfv1c = 0;
	/* Below line not mentioned in spec, need to check if necessary */
	for(filter = 0; filter < RSWITCH2_MAX_FOUR_BYTE_FILTERS; filter++) {
		
		iowrite32(0,ioaddr + (FWFOBFV0C0 + (filter * 0x10)));
		iowrite32(0,ioaddr + (FWFOBFV1C0 + (filter * 0x10)));
		iowrite32(0,ioaddr + (FWFOBFC0 + (filter * 0x10)));
	}
	/*Not mentioned line in spec ends */
	for(filter = 0; filter < config->num_four_byte_filters; filter++) {
		fwfobfc = config->four_byte[filter].offset << 16 |
				config->four_byte[filter].unit_mode;
		fwfobfv0c = config->four_byte[filter].value0;
		fwfobfv1c = config->four_byte[filter].value1;

		iowrite32(fwfobfv0c,ioaddr + (FWFOBFV0C0 + (config->four_byte[filter].filter_number * 0x10)));
		iowrite32(fwfobfv1c,ioaddr + (FWFOBFV1C0 + (config->four_byte[filter].filter_number * 0x10)));
		iowrite32(fwfobfc,ioaddr + (FWFOBFC0 + (config->four_byte[filter].filter_number * 0x10)));
	}
	return 0;

}


static int rswitch2_l3_tbl_range_filter_config(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 fwrfc = 0;
	u32 fwrfvc = 0;
	/* Below line not mentioned in spec, need to check if necessary */
	for(filter = 0; filter < RSWITCH2_MAX_RANGE_FILTERS; filter++) {
		iowrite32(0,ioaddr + (FWRFVC0 + (filter * 0x10)));
		iowrite32(0,ioaddr + (FWRFC0 + (filter * 0x10)));
	}
	/*Not mentioned line in spec ends */
	for(filter = 0; filter < config->num_range_filters; filter++) {
		fwrfc = config->range[filter].offset << 16 |
				config->range[filter].offset_mode << 8;
		fwrfvc = config->range[filter].range  << 16 |
				config->range[filter].value1 << 8 |
				config->range[filter].value0;


		iowrite32(fwrfvc,ioaddr + (FWRFVC0 + (config->range[filter].filter_number * 0x10)));
		iowrite32(fwrfc,ioaddr + (FWRFC0 + (config->range[filter].filter_number * 0x10)));
	}
	return 0;

}

static int rswitch2_l3_tbl_cascade_filter_config(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	u32 ports = 0;
	u32 pdpv = 0;
	u32 edpv = 0;
	u32 usefilterid = 0;
	u32 fwcfmc = 0;
	/* Below line not mentioned in spec, need to check if necessary */
	for(filter = 0; filter < RSWITCH2_MAX_CASCADE_FILTERS; filter++) {
		for(usefilterid = 0; usefilterid < RSWITCH2_MAX_USED_CASCADE_FILTERS; usefilterid++) {
			/* Clear the register first */
			iowrite32(0,ioaddr + (FWCFMC00 + (filter * 0x40) + (usefilterid * 4)));
		}
		iowrite32(0,ioaddr + (FWCFC0 + (filter * 0x40)));
	}
	/*Not mentioned line in spec ends */
	for(filter = 0; filter < config->num_cascade_filters; filter++) {
		edpv = 0;
		pdpv = 0;
		for(ports = 0; ports < config->cascade[filter].pframe_valid.dest_eth_ports; ports++) {
			pdpv |= 1 << config->cascade[filter].pframe_valid.port_number[ports];
		}
		for(ports = 0; ports < config->cascade[filter].eframe_valid.dest_eth_ports;ports++) {
			edpv |= 1 << config->cascade[filter].eframe_valid.port_number[ports];
		}
		edpv |= config->cascade[filter].eframe_valid.cpu << board_config.eth_ports;

		for(usefilterid = 0; usefilterid < RSWITCH2_MAX_USED_CASCADE_FILTERS; usefilterid++) {
			fwcfmc = 1 << 15;
			if(usefilterid < config->cascade[filter].used_filter_ids) {
				iowrite32(fwcfmc | config->cascade[filter].used_filter_id_num[usefilterid],
				ioaddr + (FWCFMC00 + (config->cascade[filter].filter_number * 0x40) +
				(usefilterid * 4)));
			} else {
				iowrite32(0x0,
				ioaddr + (FWCFMC00 + (config->cascade[filter].filter_number * 0x40) +
				(usefilterid * 4)));
			}

		}
		iowrite32(edpv | (pdpv << 16),ioaddr + (FWCFC0 + (config->cascade[filter].filter_number * 0x40)));
	}
	return 0;

}

static int rswitch2_disable_perfect_filter(struct rswitch2_fwd_filter_config *config)
{
	u32 filter = 0;
	for(filter = 0; filter < config->num_cascade_filters; filter++) {
		iowrite32(0, ioaddr + (FWCFC0 + (config->cascade[filter].filter_number * 0x40)));
	}

	return 0;
}
/**
    @brief  L3 table Configuration

    @param  config L3 Configuration

    @return int
*/
static int rswitch2_l3_tbl_config(struct rswitch2_l3_stream_fwd *config)
{
	u32 entries = 0;
	int ret = 0 ;
	struct rswitch2_ipv4_stream_fwd_config* stream_config;
	stream_config = &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config;
	if(config->fwd_filter_config.num_cascade_filters) {
		rswitch2_disable_perfect_filter(&config->fwd_filter_config);
	}
	if(config->fwd_filter_config.num_two_byte_filters) {
		rswitch2_l3_tbl_two_byte_filter_config( &config->fwd_filter_config);
	}
	if(config->fwd_filter_config.num_three_byte_filters) {
		rswitch2_l3_tbl_three_byte_filter_config( &config->fwd_filter_config);
	}
	if(config->fwd_filter_config.num_four_byte_filters) {
		rswitch2_l3_tbl_four_byte_filter_config( &config->fwd_filter_config);
	}
	if(config->fwd_filter_config.num_range_filters) {
		rswitch2_l3_tbl_range_filter_config( &config->fwd_filter_config);
	}
	if(config->fwd_filter_config.num_cascade_filters) {
		rswitch2_l3_tbl_cascade_filter_config( &config->fwd_filter_config);
	}

	if (stream_config->bEnable) {
		for (entries = 0; entries < stream_config->ipv4_stream_fwd_entries; entries++) {
			if(!stream_config->ipv4_stream_fwd_entry[entries].filter_enable) {
				stream_config->ipv4_stream_fwd_entry[entries].hash_value =
			        	rswitch2_calc_ipv4_hash(&stream_config->ipv4_hash_configuration,
			        		&stream_config->ipv4_stream_fwd_entry[entries]);

				stream_config->ipv4_stream_fwd_entry[entries].stream_id =
			        	rswitch2_gen_ipv4_stream(config->ipv4_ipv6_stream_config.
							ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].hash_value,
			                                &stream_config->ipv4_stream_fwd_entry[entries],
			                                &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config);
			}
		}
		rswitch2_ipv4_stream_gen_config(&config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config,
		                                &stream_config->ipv4_hash_configuration);
		if (!l3_tbl_reconfig) {
			ret = rswitch2_l3_tbl_reset();
			if (ret < 0) {
				printk("l3  tbl Reset Failed \n");
				return -1;
			}
			l3_tbl_reconfig = 1;
		}
		iowrite32(((config->max_l3_unsecure_entry << 16) | (config->max_l3_collision)), ioaddr +FWLTHHEC);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHHEC,
		       ((config->max_l3_unsecure_entry << 16) | (config->max_l3_collision)));
#endif
		iowrite32(config->l3_hash_eqn, ioaddr + FWLTHHC);
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWLTHHC,
		       config->l3_hash_eqn);
#endif
		rswitch2_ipv4_l3_tbl_config(config);
	}

	return 0;
}


#ifdef VLAN_FWD
/**
    @brief  VLAN table Configuration

    @param  config L2 Forwarding Configuration

    @return int
*/
static int rswitch2_l2_vlan_tbl_config(struct rswitch2_l2_fwd_config* config)
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
	vlantmue = config->max_unsecure_vlan_entry;
	iowrite32((vlantmue << 16), ioaddr + FWVLANTEC);
	for (entries = 0; entries < config->l2_fwd_vlan_config_entries; entries++) {
		ds_dpv = 0;
		dpv = 0;
		fwvlantl0_val = (config->l2_fwd_vlan_config_entry[entries].vlan_learn_disable << 10)
		                | (config->l2_fwd_vlan_config_entry[entries].vlan_security_learn << 8);

		iowrite32(fwvlantl0_val, ioaddr + FWVLANTL0);
		fwvlantl1_val = (config->l2_fwd_vlan_config_entry[entries].vlan_id);
		iowrite32(fwvlantl1_val, ioaddr + FWVLANTL1);
		for (port_lock = 0; port_lock < config->l2_fwd_vlan_config_entry[entries].source_lock.source_ports; port_lock++) {
			ds_dpv |= 1 << config->l2_fwd_vlan_config_entry[entries].source_lock.source_port_number[port_lock];
		}
		ds_dpv  |=  config->l2_fwd_vlan_config_entry[entries].source_lock.cpu << board_config.eth_ports;
#ifdef DEBUG
		printk("ds_dpv = %x \n", ds_dpv);
#endif
		iowrite32(ds_dpv, ioaddr + FWVLANTL2);
		iowrite32(config->l2_fwd_vlan_config_entry[entries].csdn, ioaddr+FWVLANTL3);

		for (dest_ports = 0; dest_ports < config->l2_fwd_vlan_config_entry[entries].destination_vector_config.dest_eth_ports; dest_ports++) {
			dpv |= 1 << config->l2_fwd_vlan_config_entry[entries].destination_vector_config.port_number[dest_ports];
		}
		dpv |= config->l2_fwd_vlan_config_entry[entries].destination_vector_config.cpu << board_config.eth_ports;
		fwvlantl4_val = config->l2_fwd_vlan_config_entry[entries].mirroring_config.cpu_mirror_enable << 21
		                | config->l2_fwd_vlan_config_entry[entries].mirroring_config.eth_mirror_enable << 20
		                | config->l2_fwd_vlan_config_entry[entries].mirroring_config.ipv_config.ipv_update_enable  << 19
		                | config->l2_fwd_vlan_config_entry[entries].mirroring_config.ipv_config.ipv_value  << 16
		                | dpv;
		iowrite32(fwvlantl4_val, ioaddr + FWVLANTL4);
		ret = rswitch2_vlan_learn_status();
		if (ret < 0) {
			printk("vlan Learning Failed for entry num= %d\n", entries);
		}
	}
	return 0;
}
#endif


#ifdef L2_MAC_FWD
/**
    @brief  MAC table Configuration

    @param  config L2 MAC Forwarding Configuration

    @return int
*/
static int rswitch2_l2_mac_tbl_config(struct rswitch2_l2_fwd_config* config)
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

	int ret = 0;
	for (entries = 0; entries < config->l2_fwd_mac_config_entries; entries++) {
		ss_dpv = 0;
		dpv = 0;
		ds_dpv = 0;
		fwmactl0_val = (config->l2_fwd_mac_config_entry[entries].mac_learn_disable << 10) | (config->l2_fwd_mac_config_entry[entries].mac_dynamic_learn << 9)
		               | (config->l2_fwd_mac_config_entry[entries].mac_security_learn << 8);
		iowrite32(fwmactl0_val, ioaddr + FWMACTL0);
		fwmactl1_val = (config->l2_fwd_mac_config_entry[entries].mac[0]<< 8) | (config->l2_fwd_mac_config_entry[entries].mac[1]<< 0);
		iowrite32(fwmactl1_val, ioaddr + FWMACTL1);

		fwmactl2_val = (config->l2_fwd_mac_config_entry[entries].mac[2]<< 24) | (config->l2_fwd_mac_config_entry[entries].mac[3]<< 16)
		               | (config->l2_fwd_mac_config_entry[entries].mac[4]<< 8) | (config->l2_fwd_mac_config_entry[entries].mac[5]<< 0);

		iowrite32(fwmactl2_val, ioaddr + FWMACTL2);
		for (port_lock = 0; port_lock < config->l2_fwd_mac_config_entry[entries].destination_source.source_ports; port_lock++) {
			ds_dpv |= 1 << config->l2_fwd_mac_config_entry[entries].destination_source.source_port_number[port_lock];

		}
		ds_dpv  |=  config->l2_fwd_mac_config_entry[entries].destination_source.cpu << board_config.eth_ports;

		for (port_lock = 0; port_lock < config->l2_fwd_mac_config_entry[entries].source_source.source_ports; port_lock++) {
			ss_dpv |= 1 << config->l2_fwd_mac_config_entry[entries].source_source.source_port_number[port_lock];

		}
		ss_dpv  |=  config->l2_fwd_mac_config_entry[entries].source_source.cpu << board_config.eth_ports;

		fwmactl3_val = ss_dpv | (ds_dpv << 16);
		iowrite32(fwmactl3_val, ioaddr + FWMACTL3);
		iowrite32(config->l2_fwd_mac_config_entry[entries].csdn, ioaddr + FWMACTL4);
		for (dest_ports = 0; dest_ports < config->l2_fwd_mac_config_entry[entries].destination_vector_config.dest_eth_ports; dest_ports++) {
			dpv |= 1 << config->l2_fwd_mac_config_entry[entries].destination_vector_config.port_number[dest_ports];
		}
		dpv |= config->l2_fwd_mac_config_entry[entries].destination_vector_config.cpu << board_config.eth_ports;
		fwmactl5_val = config->l2_fwd_mac_config_entry[entries].mirroring_config.cpu_mirror_enable << 21
		               | config->l2_fwd_mac_config_entry[entries].mirroring_config.eth_mirror_enable << 20
		               | config->l2_fwd_mac_config_entry[entries].mirroring_config.ipv_config.ipv_update_enable  << 19
		               | config->l2_fwd_mac_config_entry[entries].mirroring_config.ipv_config.ipv_value  << 16
		               | dpv;
		iowrite32(fwmactl5_val, ioaddr + FWMACTL5);
		ret = rswitch2_mac_learn_status();
		if (ret < 0) {
			printk("mac Learning Failed for entry num= %d\n", entries);
		}
	}
	return 0;
}
#endif


static int rswitch2_config_mac_aging(struct   rswitch2_mac_aging_config *config)
{
	u32 fwmacagc = 0;
	iowrite32(config->mac_aging_prescalar, ioaddr + FWMACAGUSPC);
	fwmacagc = (config->on_off << 24) | (config->mac_aging_polling_mode << 18) |
	           (config->mac_aging_security << 17) | (config->on_off << 16) |
	           (config->mac_aging_time_sec);
	iowrite32(fwmacagc, ioaddr + FWMACAGC);
	return 0;
}


/**
    @brief  L2  table  Update Configuration

    @param  config L23 UPdate Configuration

    @return int
*/
static int rswitch2_l2_table_update_config(struct  rswitch2_l2_fwd_config *config)
{
	int ret = 0;
	//if(config->l2_fwd_mac_config_entries)
	{
		if (!mac_tbl_reconfig) {
			ret = rswitch2_l2_mac_tbl_reset();
			if (ret < 0) {
				printk("l2 MAC tbl Reset Failed \n");
				return -1;
			}
			mac_tbl_reconfig = 1;
		}
		iowrite32(((config->max_unsecure_hash_entry << 16) |config->max_hash_collision), ioaddr + FWMACHEC);
		iowrite32(config->mac_hash_eqn, ioaddr + FWMACHC);
		ret = rswitch2_l2_mac_tbl_config(config);
		if (ret < 0) {
			printk("l2 MAC tbl Config Failed \n");
			return -1;
		}
		if (config->mac_aging_config.bEnable) {
			ret = rswitch2_config_mac_aging(&config->mac_aging_config);
		}
	}
	//if(config->l2_fwd_vlan_config_entries)
	{
		if (!vlan_tbl_reconfig) {
			ret = rswitch2_l2_vlan_tbl_reset();
			if (ret < 0) {
				printk("l2 VLAN tbl Reset Failed \n");
				return -1;
			}
			vlan_tbl_reconfig = 1;
		}
		iowrite32(((config->max_unsecure_vlan_entry << 16)), ioaddr + FWVLANTEC);
		ret = rswitch2_l2_vlan_tbl_config(config);
		if (ret < 0) {
			printk("l2 VLAN tbl Config Failed \n");
			return -1;
		}
	}
	return 0;
}


/**
    @brief  L23  table  Update Configuration

    @param  config L23 UPdate Configuration

    @return int
*/
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
	if (!l23_tbl_reconfig) {
		ret = rswitch2_l23_update_tbl_reset();
		if (ret < 0) {
			printk("l23 Update  tbl Reset Failed \n");
			return -1;
		}
			l23_tbl_reconfig = 1;
	}
	for (entries = 0; entries < config->entries; entries++) {
		dpv = 0;
		for (dest_ports = 0; dest_ports < config->l23_update_config[entries].routing_port_update.dest_eth_ports; dest_ports++) {
			dpv |= 1 << config->l23_update_config[entries].routing_port_update.port_number[dest_ports];
		}
#ifdef DEBUG
		printk("config->l23_update_config[entries].src_mac_update = %x \n", config->l23_update_config[entries].src_mac_update);
#endif
		dpv |= config->l23_update_config[entries].routing_port_update.cpu << board_config.eth_ports;
		fwl23url0 = (dpv << 16) | (config->l23_update_config[entries].routing_number);
		fwl23url1 = ((config->l23_update_config[entries].rtag  & 0x03) << 25) |
		            ((config->l23_update_config[entries].stag_dei_update  & 0x01) << 24) |
		            ((config->l23_update_config[entries].stag_pcp_update  & 0x01) << 23) |
		            ((config->l23_update_config[entries].stag_vlan_update  & 0x01) << 22) |
		            ((config->l23_update_config[entries].ctag_dei_update  & 0x01) << 21) |
		            ((config->l23_update_config[entries].ctag_pcp_update  & 0x01) << 20) |
		            ((config->l23_update_config[entries].ctag_vlan_update  & 0x01) << 19) |
		            ((config->l23_update_config[entries].src_mac_update  & 0x01) << 18) |
		            ((config->l23_update_config[entries].dest_mac_update  & 0x01) << 17) |
		            ((config->l23_update_config[entries].ttl_update  & 0x01) << 16) |
		            ((config->l23_update_config[entries].dest_mac[0]  & 0xFF) << 8) |
		            ((config->l23_update_config[entries].dest_mac[1]  & 0xFF) << 0);
		fwl23url2 = ((config->l23_update_config[entries].dest_mac[2]  & 0xFF) << 24) |
		            ((config->l23_update_config[entries].dest_mac[3]  & 0xFF) << 16) |
		            ((config->l23_update_config[entries].dest_mac[4]  & 0xFF) << 8) |
		            ((config->l23_update_config[entries].dest_mac[5]  & 0xFF) << 0);
		fwl23url3 = ((config->l23_update_config[entries].stag_dei  & 0x01 ) << 31) |
		            ((config->l23_update_config[entries].stag_pcp  & 0x07) << 28) |
		            ((config->l23_update_config[entries].stag_vlan  & 0xFFF) << 16) |
		            ((config->l23_update_config[entries].ctag_dei  & 0x01 ) << 15) |
		            ((config->l23_update_config[entries].ctag_pcp  & 0x07) << 12) |
		            ((config->l23_update_config[entries].ctag_vlan  & 0xFFF) << 0);
		iowrite32(fwl23url0, ioaddr + FWL23URL0);
#ifdef DEBUG
		printk("fwl23url1=%x\n",fwl23url1);
#endif
		iowrite32(fwl23url1, ioaddr + FWL23URL1);
		iowrite32(fwl23url2, ioaddr + FWL23URL2);
		iowrite32(fwl23url3, ioaddr + FWL23URL3);
		ret = rswitch2_l23_update_learn_status();
		if (ret < 0) {
			printk("L23 Update Learning  Failed for entry num= %d\n", entries);
		}
	}
	return 0;
}


#ifdef IP_TBL_FWD
/**
    @brief  IP table Configuration

    @param  config IP Forwarding Configuration

    @return int

*/
static int rswitch2_ip_tbl_config(struct rswitch2_ipv_fwd_config* config)
{
	u32 ds_dpv = 0;
	u32 ss_dpv = 0;
	u32 dpv = 0;
	u32 fwiphec_val =0;
	u32 fwiphc_val = 0;
	u32 fwiptl0_val = 0;
	u32 fwiptl1_val = 0;
	u32 fwiptl2_val = 0;
	u32 fwiptl3_val = 0;
	u32 fwiptl4_val = 0;
	u32 fwiptl5_val = 0;
	u32 fwiptl6_val = 0;
	u32 fwiptl7_val = 0;
	u32 fwiptl8_val = 0;
	//u32 dpv = 0;
	u32 entries = 0;
	u32 port_lock = 0;
	u32 dest_ports = 0;

	int ret = 0;
	fwiphec_val = (config->max_unsecure_hash_entry  << 16) | (config->max_hash_collision);
	iowrite32(fwiphec_val, ioaddr + FWIPHEC);
	fwiphc_val = config->ipv_hash_eqn;
	for (entries = 0; entries < config->ipv_fwd_config_entries; entries++) {
		ds_dpv = 0;
		ss_dpv  = 0;
		dpv = 0;
		fwiptl0_val = (config->ipv_fwd_config_entry[entries].ipv_learn_disable << 10) | (config->ipv_fwd_config_entry[entries].ipv_dynamic_learn << 9)
		              | (config->ipv_fwd_config_entry[entries].ipv_security_learn << 8) | (config->ipv_fwd_config_entry[entries].ip_addr_type);
		iowrite32(fwiptl0_val, ioaddr + FWIPTL0);
		if (config->ipv_fwd_config_entry[entries].ip_addr_type == ip_addr_ipv4) {
			fwiptl1_val = (config->ipv_fwd_config_entry[entries].ipaddr[0] << 24) | (config->ipv_fwd_config_entry[entries].ipaddr[1] << 16) |
			              (config->ipv_fwd_config_entry[entries].ipaddr[2] << 8) | (config->ipv_fwd_config_entry[entries].ipaddr[3] << 0);
			iowrite32(fwiptl1_val, ioaddr + FWIPTL1);
		}

		else {

			/* ipv6 Suppport to be provided */

		}


		for (port_lock = 0; port_lock < config->ipv_fwd_config_entry[entries].destination_source.source_ports; port_lock++) {
			ds_dpv |= 1 << config->ipv_fwd_config_entry[entries].destination_source.source_port_number[port_lock];

		}
		ds_dpv  |=  config->ipv_fwd_config_entry[entries].destination_source.cpu << board_config.eth_ports;
		printk("ss_dpv = %x \n", ss_dpv);

		for (port_lock = 0; port_lock < config->ipv_fwd_config_entry[entries].source_source.source_ports; port_lock++) {
			ss_dpv |= 1 << config->ipv_fwd_config_entry[entries].source_source.source_port_number[port_lock];

		}
		ss_dpv  |=  config->ipv_fwd_config_entry[entries].source_source.cpu << board_config.eth_ports;
		printk("ss_dpv = %x \n", ss_dpv);
		fwiptl5_val = config->ipv_fwd_config_entry[entries].routing_number | (ds_dpv << 16) | (1 << 15);
		iowrite32(fwiptl5_val, ioaddr + FWIPTL5);
		iowrite32(ss_dpv, ioaddr + FWIPTL6);
		for (dest_ports = 0; dest_ports < config->ipv_fwd_config_entry[entries].destination_vector_config.dest_eth_ports; dest_ports++) {
			dpv |= 1 << config->ipv_fwd_config_entry[entries].destination_vector_config.port_number[dest_ports];
		}
		dpv |= config->ipv_fwd_config_entry[entries].destination_vector_config.cpu << board_config.eth_ports;
		if (config->ipv_fwd_config_entry[entries].destination_vector_config.cpu) {
			iowrite32(config->ipv_fwd_config_entry[entries].csdn, ioaddr + FWIPTL7);


		}
		fwiptl8_val = config->ipv_fwd_config_entry[entries].mirroring_config.cpu_mirror_enable << 21
		              | config->ipv_fwd_config_entry[entries].mirroring_config.eth_mirror_enable << 20
		              | config->ipv_fwd_config_entry[entries].mirroring_config.ipv_config.ipv_update_enable  << 19
		              | config->ipv_fwd_config_entry[entries].mirroring_config.ipv_config.ipv_value  << 16
		              | dpv;
		iowrite32(fwiptl8_val, ioaddr + FWIPTL8);
		ret = rswitch2_ipv_learn_status();
		if (ret < 0) {
			printk("ip Learning Failed for entry num= %d\n", entries);
		}

	}
	return 0;
}
#endif


/**
    @brief  General Configuration

    @return int
*/
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
	u32 iprusa  = 0;
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
	u32 lthrus =0;
	/* No vlan Mode, Dont touch register FWGC FWTTC0 FWTTC1*/
	/* No Exceptional path target Dont touch register FWCEPTC, FWCEPRC0, FWCEPRC1, FWCEPRC2*/
	/* No Learn frame to cpu Dont touch register FWCLPTC FWCLPRC*/
	/* No mirroring configuration Dont touch register FWCMPTC FWEMPTC FWSDMPTC FWSDMPVC*/
	/* No frame Discarded for watermark FWLBWMCi default values*/
	for (i = 0; i < board_config.eth_ports; i++) {
#if 0
		vlanRUS = 0;
		vlanRU  = 0;
#endif
		vlansa = 1;

		/* Migration Active */
		machma = 1;
		/*Learning Active */
		machla =  1;
#if 0
		macRUSSA = 0;
		macRUSA = 0;
		macSSA = 0;
		macRUDSA = 0;
		macRUDA = 0;
#endif
		/* mac DA Search Active */
		macdsa = 1;
#if 1
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
#endif
		/* l2 stream Enable */
		l2se = 1;
#if 1
		ip6oe = 0;
		ip6te = 0;
		ip6ue = 0;
		ip4oe = 1;
		ip4te = 1;
		ip4ue = 1;
		lthruss = 0;
		lthrus =0;
#endif
		/*L3 Table Active */
		lthta = 1;

		fwpc0 = (vlansa << 28) | (machma<<27) | (machla << 26) | (macdsa << 20 ) | (l2se << 9 ) | (lthta << 0 ) | (iphla << 18) | (ipdsa << 12) | (ip4ea << 10)
		        | (ip4oe << 5) | (ip4te << 4) | (ip4ue << 3) | (lthta);
		iowrite32(fwpc0, (ioaddr + FWPC00 + (i * 0x10)));
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWPC00 + (i * 0x10),
		       fwpc0);
#endif
	}
#if 0
	/* Direct Descriptor already enabled in ethernet driver, no ned to touch FWPC1*/
	ret = rswitch2_l2_mac_tbl_reset();
	if (ret < 0) {
		return -1;

	}

	l2_fwd_config.l2_fwd_mac_config_entries = 1;
	l2_fwd_config.l2_fwd_mac_config_entry[0].mac_dynamic_learn = 1;
	l2_fwd_config.l2_fwd_mac_config_entry[0].mac[0] = 0x11;
	l2_fwd_config.l2_fwd_mac_config_entry[0].mac[1] = 0x11;
	l2_fwd_config.l2_fwd_mac_config_entry[0].mac[2] = 0x11;
	l2_fwd_config.l2_fwd_mac_config_entry[0].mac[3] = 0x11;
	l2_fwd_config.l2_fwd_mac_config_entry[0].mac[4] = 0x11;
	l2_fwd_config.l2_fwd_mac_config_entry[0].mac[5] = 0x11;
	l2_fwd_config.l2_fwd_mac_config_entry[0].destination_source.source_ports = 2;
	l2_fwd_config.l2_fwd_mac_config_entry[0].destination_source.source_port_number[0] = 0x0;
	l2_fwd_config.l2_fwd_mac_config_entry[0].destination_source.source_port_number[1] = 0x01;
	l2_fwd_config.l2_fwd_mac_config_entry[0].destination_source.cpu = 1;
	l2_fwd_config.l2_fwd_mac_config_entry[0].source_source.source_ports = 2;
	l2_fwd_config.l2_fwd_mac_config_entry[0].source_source.source_port_number[0] = 0x0;
	l2_fwd_config.l2_fwd_mac_config_entry[0].source_source.source_port_number[1] = 0x01;
	l2_fwd_config.l2_fwd_mac_config_entry[0].source_source.cpu = 1;
	l2_fwd_config.l2_fwd_mac_config_entry[0].destination_vector_config.dest_eth_ports = 1;
	l2_fwd_config.l2_fwd_mac_config_entry[0].destination_vector_config.port_number[0] = 0x01;
	l2_fwd_config.l2_fwd_mac_config_entry[0].mac_security_learn = 1;

	rswitch2_l2_mac_tbl_config(&l2_fwd_config);
	ret = rswitch2_l2_vlan_tbl_reset();
	if (ret < 0) {
		return -1;

	}

	l2_fwd_config.l2_fwd_vlan_config_entries = 1;
	l2_fwd_config.max_unsecure_vlan_entry = 1;

	l2_fwd_config.l2_fwd_vlan_config_entry[0].vlan_id = 0xFFF;

	l2_fwd_config.l2_fwd_vlan_config_entry[0].source_lock.source_ports = 2;
	l2_fwd_config.l2_fwd_vlan_config_entry[0].source_lock.source_port_number[0] = 0x0;
	l2_fwd_config.l2_fwd_vlan_config_entry[0].source_lock.source_port_number[1] = 0x01;
	l2_fwd_config.l2_fwd_vlan_config_entry[0].source_lock.cpu = 1;

	l2_fwd_config.l2_fwd_vlan_config_entry[0].destination_vector_config.dest_eth_ports = 1;
	l2_fwd_config.l2_fwd_vlan_config_entry[0].destination_vector_config.port_number[0] = 0x01;
	l2_fwd_config.l2_fwd_vlan_config_entry[0].vlan_security_learn = 1;

	rswitch2_l2_vlan_tbl_config(&l2_fwd_config);
	rswitch2_ip_tbl_reset();
	ipv_fwd_config.max_unsecure_hash_entry = 0;
	ipv_fwd_config.max_hash_collision = 1023;
	ipv_fwd_config.ipv_hash_eqn = 1;
	ipv_fwd_config.ipv_fwd_config_entries = 1;
	ipv_fwd_config.ipv_fwd_config_entry[0].ipv_dynamic_learn = 1;
	ipv_fwd_config.ipv_fwd_config_entry[0].ipaddr[0] = 192;
	ipv_fwd_config.ipv_fwd_config_entry[0].ipaddr[1] = 168;
	ipv_fwd_config.ipv_fwd_config_entry[0].ipaddr[2] = 1;
	ipv_fwd_config.ipv_fwd_config_entry[0].ipaddr[3] = 99;
	ipv_fwd_config.ipv_fwd_config_entry[0].destination_source.source_ports = 2;
	ipv_fwd_config.ipv_fwd_config_entry[0].destination_source.source_port_number[0] = 0x0;
	ipv_fwd_config.ipv_fwd_config_entry[0].destination_source.source_port_number[1] = 0x01;
	ipv_fwd_config.ipv_fwd_config_entry[0].destination_source.cpu = 1;
	ipv_fwd_config.ipv_fwd_config_entry[0].source_source.source_ports = 2;
	ipv_fwd_config.ipv_fwd_config_entry[0].source_source.source_port_number[0] = 0x0;
	ipv_fwd_config.ipv_fwd_config_entry[0].source_source.source_port_number[1] = 0x01;
	ipv_fwd_config.ipv_fwd_config_entry[0].source_source.cpu = 1;
	ipv_fwd_config.ipv_fwd_config_entry[0].destination_vector_config.dest_eth_ports = 1;
	ipv_fwd_config.ipv_fwd_config_entry[0].destination_vector_config.port_number[0] = 0x01;
	ipv_fwd_config.ipv_fwd_config_entry[0].ipv_security_learn = 1;

	rswitch2_ip_tbl_config(&ipv_fwd_config);
#endif

#if 0
	ipv4_stream_config.bEnable = 1;
	ipv4_stream_config.ipv4_hash_configuration.destination_port = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.source_port = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.destination_ip = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.source_ip = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.protocol = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.ctag_dei = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.ctag_pcp = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.ctag_vlan = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.stag_dei = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.stag_pcp = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.stag_vlan = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.dest_mac = Feature_Exclude;
	ipv4_stream_config.ipv4_hash_configuration.src_mac = Feature_Exclude;
	ipv4_stream_config.destination_port = Feature_Exclude;
	ipv4_stream_config.destination_ip = Feature_Include;
	ipv4_stream_config.source_ip = Feature_Exclude;
	ipv4_stream_config.ctag_dei = Feature_Exclude;
	ipv4_stream_config.ctag_pcp = Feature_Exclude;
	ipv4_stream_config.ctag_vlan = Feature_Exclude;
	ipv4_stream_config.stag_dei = Feature_Exclude;
	ipv4_stream_config.stag_pcp = Feature_Exclude;
	ipv4_stream_config.stag_vlan = Feature_Exclude;
	ipv4_stream_config.ipv4_stream_fwd_entries = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].destination_ip_addr[0] = 192;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].destination_ip_addr[1] = 168;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].destination_ip_addr[2] = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].destination_ip_addr[3] = 99;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].security_learn = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].ipv4_stream_frame_format_Code = ipv4_NoTCP_NoUDP;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].source_lock.source_ports = 2;

	ipv4_stream_config.ipv4_stream_fwd_entry[0].source_lock.source_port_number[0] = 0x0;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].source_lock.source_port_number[1] = 0x01;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].source_lock.cpu = 1;

	ipv4_stream_config.ipv4_stream_fwd_entry[0].destination_vector_config.dest_eth_ports = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].destination_vector_config.port_number[0] = 0x01;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].routing_number = 0;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].routing_valid = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[0].hash_value =
	        rswitch2_calc_ipv4_hash(&ipv4_stream_config.ipv4_hash_configuration,  &ipv4_stream_config.ipv4_stream_fwd_entry[0]);
	ipv4_stream_config.ipv4_stream_fwd_entry[0].stream_id = rswitch2_gen_ipv4_stream(ipv4_stream_config.ipv4_stream_fwd_entry[0].hash_value,
	                &ipv4_stream_config.ipv4_stream_fwd_entry[0], &ipv4_stream_config);
	ipv4_stream_config.ipv4_stream_fwd_entry[1].destination_ip_addr[0] = 192;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].destination_ip_addr[1] = 168;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].destination_ip_addr[2] = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].destination_ip_addr[3] = 99;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].security_learn = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].ipv4_stream_frame_format_Code = ipv4_UDP;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].source_lock.source_ports = 2;

	ipv4_stream_config.ipv4_stream_fwd_entry[1].source_lock.source_port_number[0] = 0x0;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].source_lock.source_port_number[1] = 0x01;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].source_lock.cpu = 1;

	ipv4_stream_config.ipv4_stream_fwd_entry[1].destination_vector_config.dest_eth_ports = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].destination_vector_config.port_number[0] = 0x01;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].routing_number = 0;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].routing_valid = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[1].hash_value =
	        rswitch2_calc_ipv4_hash(&ipv4_stream_config.ipv4_hash_configuration,  &ipv4_stream_config.ipv4_stream_fwd_entry[1]);
	ipv4_stream_config.ipv4_stream_fwd_entry[1].stream_id = rswitch2_gen_ipv4_stream(ipv4_stream_config.ipv4_stream_fwd_entry[1].hash_value,
	                &ipv4_stream_config.ipv4_stream_fwd_entry[1], &ipv4_stream_config);
	ipv4_stream_config.ipv4_stream_fwd_entry[2].destination_ip_addr[0] = 192;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].destination_ip_addr[1] = 168;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].destination_ip_addr[2] = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].destination_ip_addr[3] = 99;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].security_learn = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].ipv4_stream_frame_format_Code = ipv4_TCP;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].source_lock.source_ports = 2;

	ipv4_stream_config.ipv4_stream_fwd_entry[2].source_lock.source_port_number[0] = 0x0;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].source_lock.source_port_number[1] = 0x01;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].source_lock.cpu = 1;

	ipv4_stream_config.ipv4_stream_fwd_entry[2].destination_vector_config.dest_eth_ports = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].destination_vector_config.port_number[0] = 0x01;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].routing_number = 0;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].routing_valid = 1;
	ipv4_stream_config.ipv4_stream_fwd_entry[2].hash_value =
	        rswitch2_calc_ipv4_hash(&ipv4_stream_config.ipv4_hash_configuration,  &ipv4_stream_config.ipv4_stream_fwd_entry[2]);

	ipv4_stream_config.ipv4_stream_fwd_entry[2].stream_id = rswitch2_gen_ipv4_stream(ipv4_stream_config.ipv4_stream_fwd_entry[1].hash_value,
	                &ipv4_stream_config.ipv4_stream_fwd_entry[2], &ipv4_stream_config);
	rswitch2_ipv4_stream_gen_config(&ipv4_stream_config, &ipv4_stream_config.ipv4_hash_configuration);

	l3_stream_fwd.max_l3_unsecure_entry = 0xFFFF;
	l3_stream_fwd.max_l3_collision = 0xFFFF;
	l3_stream_fwd.l3_hash_eqn = 0x1;
	memcpy(&l3_stream_fwd.ipv4_ipv6_stream_config.ipv4_stream_fwd_config, &ipv4_stream_config, sizeof(ipv4_stream_config));
	ret = rswitch2_l3_tbl_reset();
	if (ret < 0) {
		printk("l3 tbl Reset Failed \n");
		return -1;
	}
	rswitch2_ipv4_l3_tbl_config(&l3_stream_fwd);
#endif
	return 0;
}


/**
    @brief  IOCTL Get Configuration

    @param  file
    @param  arg

    @return int
*/
static int rswitch2fwd_ioctl_getfconfig(struct file * file, unsigned long arg)
{
	char __user *buf = (char __user *)arg;
	int     err = 0;
	rswitch2fwd_config.fwd_gen_config.vlan_mode = ioread32(ioaddr + FWGC);
	if ((err = copy_to_user(buf, &rswitch2fwd_config, sizeof(rswitch2fwd_config))) < 0) {
		return err;
	}

	return 0;
}


/**
    @brief  FWD Port Forwarding Configuration

    @param  config Forwarding Port Forwarding Configuration structure
    @param  agent Agnet Number

    @return int
*/
static int rswitch2_port_fwding_config(struct rswitch2_port_forwarding * config, u32 agent)
{
	u32 dpv = 0;
	u32 dest_ports = 0;
	u32 fwpbfc = 0;
	for (dest_ports = 0; dest_ports < config->destination_vector_config.dest_eth_ports; dest_ports++) {
		dpv |= 1 << config->destination_vector_config.port_number[dest_ports];
	}
	dpv |= config->destination_vector_config.cpu << board_config.eth_ports;
	fwpbfc =  (config->force_frame_priority << 26) | (config->ipv6_priority_decode << 25) |
	          (config->ipv4_priority_decode << 23) | (config->ipv4_priority_decode_mode << 24) |
	          (config->security_learn << 22) | (config->mirroring_config.cpu_mirror_enable << 21) |
	          (config->mirroring_config.eth_mirror_enable << 20) |  (config->mirroring_config.ipv_config.ipv_update_enable << 19) |
	          (config->mirroring_config.ipv_config.ipv_value << 16) | dpv;
	iowrite32(fwpbfc, (ioaddr + FWPBFC0 + (agent * 0x10)));
	iowrite32(config->csdn, (ioaddr + FWPBFCSDC00 + (agent * 0x10)));

	return 0;
}


/**
    @brief  FWD General Configuration

    @param  config Forwarding General Configuration structure

    @return int
*/
static int rswitch2_fwd_gen_config(struct rswitch2_fwd_gen_config *config)
{
	u32 fwpc0 = 0;
	u32 i = 0;

	iowrite32(config->vlan_mode, (ioaddr + FWGC));
	iowrite32(((config->stag_tpid << 16) | config->ctag_tpid), (ioaddr + FWTTC0));
	if (config->vlan_mode_config_only) {
		return 0;
	}
	for (i = 0; i < board_config.agents; i++) {
		fwpc0 = 0;
		fwpc0 = (config->fwd_gen_port_config[i].vlan_reject_unknown_secure << 30) | (config->fwd_gen_port_config[i].vlan_reject_unknown << 29) |
		        (config->fwd_gen_port_config[i].vlan_search_active << 28) | (config->fwd_gen_port_config[i].mac_hw_migration_active<<27) |
		        (config->fwd_gen_port_config[i].mac_hw_learn_active << 26) | (config->fwd_gen_port_config[i].mac_reject_unknown_src_secure_addr << 25) |
		        (config->fwd_gen_port_config[i].mac_reject_unknown_src_addr << 24) | (config->fwd_gen_port_config[i].mac_src_search_active << 23) |
		        (config->fwd_gen_port_config[i].mac_reject_unknown_dest_secure_addr << 22) | (config->fwd_gen_port_config[i].mac_reject_unknown_dest_addr << 21) |
		        (config->fwd_gen_port_config[i].mac_dest_search_active << 20 ) | (config->fwd_gen_port_config[i].ip_hw_migration_active << 19) |
		        (config->fwd_gen_port_config[i].ip_hw_learn_active << 18) | (config->fwd_gen_port_config[i].ip_reject_unknown_src_secure_addr << 17) |
		        (config->fwd_gen_port_config[i].ip_reject_unknown_src_addr << 16) | (config->fwd_gen_port_config[i].ip_src_search_active << 15) |
		        (config->fwd_gen_port_config[i].ip_reject_unknown_dest_secure_addr << 14) | (config->fwd_gen_port_config[i].ip_reject_unknown_dest_addr << 13) |
		        (config->fwd_gen_port_config[i].ip_dest_search_active << 12) | (config->fwd_gen_port_config[i].ipv6_extract_active << 11) |
		        (config->fwd_gen_port_config[i].ipv4_extract_active << 10) | (config->fwd_gen_port_config[i].l2_stream_enabled << 9 ) |
		        (config->fwd_gen_port_config[i].ipv6_other_enabled << 8) | (config->fwd_gen_port_config[i].ipv6_tcp_enabled << 7) |
		        (config->fwd_gen_port_config[i].ipv6_udp_enabled << 6) | (config->fwd_gen_port_config[i].ipv4_other_enabled << 5) |
		        (config->fwd_gen_port_config[i].ipv4_tcp_enabled << 4) | (config->fwd_gen_port_config[i].ipv4_udp_enabled << 3) |
		        (config->fwd_gen_port_config[i].l3_reject_unknown_secure_stream << 2) | (config->fwd_gen_port_config[i].l3_reject_unknown_stream << 1) |
		        (config->fwd_gen_port_config[i].l3_tbl_active << 0 );
		if (config->fwd_gen_port_config[i].cpu) {
			iowrite32(fwpc0, (ioaddr + FWPC00 + (board_config.eth_ports * 0x10)));
			if (config->fwd_gen_port_config[i].port_forwarding.bEnable) {
				rswitch2_port_fwding_config(&config->fwd_gen_port_config[i].port_forwarding, board_config.eth_ports);
			}
		}
		else {
			iowrite32(fwpc0, (ioaddr + FWPC00 + (config->fwd_gen_port_config[i].portnumber * 0x10)));
			if (config->fwd_gen_port_config[i].port_forwarding.bEnable) {
				rswitch2_port_fwding_config(&config->fwd_gen_port_config[i].port_forwarding, config->fwd_gen_port_config[i].portnumber);
			}
		}
#ifdef DEBUG
		printk("Register address= %x Write value = %x \n",FWPC00 + (i * 0x10),
		       fwpc0);
#endif
	}

	return 0;
}


static int rswitch2_mcast_rx_desc_chain_config(struct rswitch2_cpu_rx_mcast_config *config)
{

	u32 msenl = 0;
	u32 mnl = 0;
	u32 mnrcnl = 0;
	u32 gwmstls_val = 0;
	u32 entry = 0;
	void __iomem      *  gwca_addr =  NULL;
	s32 ret = 0;
	gwca_addr = ioaddr + board_config.gwca0.start;
	rswitch2_gwca_mcast_tbl_reset();
	for(entry = 0; entry < config->entries; entry++) {
		msenl  = config->entry[entry].entry_num;
		mnl = config->entry[entry].next_entries;
		mnrcnl = config->entry[entry].next_entry_num;
		gwmstls_val = (msenl << 16) | (mnl << 8) | (mnrcnl << 0);
		iowrite32(gwmstls_val, gwca_addr + GWMSTLS);
		ret = rswitch2_mcast_tbl_learn_status();
		if (ret < 0) {
			printk("MCAST Table  Learning Failed \n");
		}
	}
	
	return 0;
}



/**
    @brief  update configuration

    @return int
*/
static int updateconfiguration(void)
{
#ifdef DEBUG
	printk("Update configuration \n");
#endif
	memcpy(&rswitch2fwd_config,&rswitch2fwd_confignew, sizeof(rswitch2fwd_config));
	/* Update general configuration if required */
	if (rswitch2fwd_config.fwd_gen_config.bEnable) {
#ifdef DEBUG
		printk("gen config benable \n");
#endif
		/*Call L3 configuration Function */
		rswitch2_fwd_gen_config(&rswitch2fwd_config.fwd_gen_config);
	}
	if(rswitch2fwd_config.rx_mcast_config.entries) {
		rswitch2_mcast_rx_desc_chain_config(&rswitch2fwd_config.rx_mcast_config);
	}
	if (rswitch2fwd_config.l3_stream_fwd.bEnable) {
		/*Call L3 configuration Function */
		rswitch2_l3_tbl_config(&rswitch2fwd_config.l3_stream_fwd);
	}
	if (rswitch2fwd_config.l23_update.entries) {
		/*Call L3 configuration Function */
		rswitch2_l23_table_update_config(&rswitch2fwd_config.l23_update);
	}
	if (rswitch2fwd_config.l2_fwd_config.bEnable) {
		/*Call L3 configuration Function */
		rswitch2_l2_table_update_config(&rswitch2fwd_config.l2_fwd_config);
	}

	return 0;
}


/**
    @brief  IOCTL Set Configuration

    @param  file
    @param  arg

    @return long
*/
static long rswitch2fwd_ioctl_setconfig(struct file * file, unsigned long arg)
{
	char __user * buf = (char __user *)arg;

	int           err = 0;
#ifdef DEBUG
	printk("Set config Called \n");
#endif
	err = access_ok(VERIFY_WRITE, buf, sizeof(rswitch2fwd_confignew));

	err = copy_from_user(&rswitch2fwd_confignew, buf, sizeof(rswitch2fwd_confignew));
	if (err != 0) {

		pr_err("[RSWITCH2-FWD] rswitch2fwd_IOCTL_Setportsconfig. copy_from_user returned %d for RSWITCH2_FWD_SET_CONFIG \n", err);
		return err;
	}
	if ((err = updateconfiguration()) < 0) {
		printk("Returned Err \n");

		return err;
	}
	return err;
}


/**
    @brief  IOCTL

    @param  cmd
    @param  arg

    @return long
*/
static long rswitch2fwd_ioctl(struct file * file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case RSWITCH2_FWD_GET_CONFIG:
		return rswitch2fwd_ioctl_getfconfig(file, arg);

	case RSWITCH2_FWD_SET_CONFIG:
		return rswitch2fwd_ioctl_setconfig(file, arg);

	default:
		pr_err("[RSWITCH2-FWD] IOCTL Unknown or unsupported: 0x%08X\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}


/**
    @brief  Character Device operations
*/
const struct file_operations    rswitch2fwd_fileOps = {
	.owner            = THIS_MODULE,
	.open             = NULL,
	.release          = NULL,
	.unlocked_ioctl   = rswitch2fwd_ioctl,
	.compat_ioctl     = rswitch2fwd_ioctl,
};


/**
    @brief  Remove character device

    @return < 0, or 0 on success
*/
static int rswitch2fwd_drv_remove_chrdev(void)
{
	int     ret = 0;
	/*
	    Remove character device
	*/
	unregister_chrdev_region(rswitch2fwd_dev, 1);
	device_del(&rswitch2fwd_device);
	cdev_del(&rswitch2fwd_cdev);
	class_destroy(rswitch2fwd_devclass);

	return ret;
}


/**
    @brief  Add character device

    @return < 0, or 0 on success
*/
static int rswitch2fwd_drv_probe_chrdev(void)
{
	struct device * dev = NULL;
	int             ret = 0;

	/*
	    Create class
	*/
	rswitch2fwd_devclass = class_create(THIS_MODULE, RSWITCH2FWD_CLASS);
	if (IS_ERR(rswitch2fwd_devclass)) {
		ret = PTR_ERR(rswitch2fwd_devclass);
		pr_err("[RSWITCH2-FWD] failed to create '%s' class. rc=%d\n", RSWITCH2FWD_CLASS, ret);
		return ret;
	}

	if (rswitch2fwd_devmajor != 0) {
		rswitch2fwd_dev = MKDEV(rswitch2fwd_devmajor, 0);
		ret = register_chrdev_region(rswitch2fwd_dev, 1, RSWITCH2FWD_CLASS);
	}
	else {
		ret = alloc_chrdev_region(&rswitch2fwd_dev, 0, 1, RSWITCH2FWD_CLASS);
	}
	if (ret < 0) {
		pr_err("[RSWITCH2-FWD] failed to register '%s' character device. rc=%d\n", RSWITCH2FWD_CLASS, ret);
		class_destroy(rswitch2fwd_devclass);
		return ret;
	}
	rswitch2fwd_devmajor = MAJOR(rswitch2fwd_dev);

	cdev_init(&rswitch2fwd_cdev, &rswitch2fwd_fileOps);
	rswitch2fwd_cdev.owner = THIS_MODULE;

	ret = cdev_add(&rswitch2fwd_cdev, rswitch2fwd_dev, RSWITCH2_FWD_CTRL_MINOR + 1);
	if (ret < 0) {
		pr_err("[RSWITCH2-FWD] failed to add '%s' character device. rc=%d\n", RSWITCH2FWD_CLASS, ret);
		unregister_chrdev_region(rswitch2fwd_dev, 1);
		class_destroy(rswitch2fwd_devclass);
		return ret;
	}

	/* device initialize */
	dev = &rswitch2fwd_device;
	device_initialize(dev);
	dev->class  = rswitch2fwd_devclass;
	dev->devt   = MKDEV(rswitch2fwd_devmajor, RSWITCH2_FWD_CTRL_MINOR);
	dev_set_name(dev, RSWITCH2_FWD_DEVICE_NAME);

	ret = device_add(dev);
	if (ret < 0) {
		pr_err("[RSWITCH2-FWD] failed to add '%s' device. rc=%d\n", RSWITCH2_FWD_DEVICE_NAME, ret);
		cdev_del(&rswitch2fwd_cdev);
		unregister_chrdev_region(rswitch2fwd_dev, 1);
		class_destroy(rswitch2fwd_devclass);
		return ret;
	}

	return ret;
}


#if 0
for (entries = 0; entries < config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entries; entries++)
{
	memcpy(stream_id, &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].stream_id, sizeof(struct rswitch2_ipv4_stream_id));
	fwlthtl0_val = (config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].security_learn << 8) | stream_id->frame_fmt;
	fwlthtl1_val = (stream_id->stag << 16) | stream_id->ctag;
	fwlthtl2_val = (stream_id->destination_port << 16) | stream_id->hash_value;
	fwlthtl3_val = stream_id->source_ip;
	fwlthtl4_val = stream_id->destination_ip;
	iowrite32(fwlthtl0_val, ioaddr + FWLTHTL0);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTL0,
	       fwlthtl0_val);
#endif
	iowrite32(fwlthtl1_val, ioaddr + FWLTHTL1);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTL1,
	       fwlthtl1_val);
#endif
	iowrite32(fwlthtl2_val, ioaddr + FWLTHTL2);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTL2,
	       fwlthtl2_val);
#endif
	iowrite32(fwlthtl3_val, ioaddr + FWLTHTL3);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTL3,
	       fwlthtl3_val);
#endif
	iowrite32(fwlthtl4_val, ioaddr + FWLTHTL4);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTL4,
	       fwlthtl4_val);
#endif
	for (port_lock = 0; port_lock < config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].source_lock.source_ports; port_lock++) {
		ds_dpv |= 1 << config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].source_lock.source_port_number[port_lock];

	}
	ds_dpv  |=  config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].source_lock.cpu << board_config.eth_ports;
	fwlthtl7_val = (ds_dpv << 16) | (config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].routing_valid << 15)
	               | (config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].routing_number << 0);
	iowrite32(fwlthtl7_val, ioaddr + FWLTHTL7);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTL7,
	       fwlthtl7_val);
#endif
	iowrite32(config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].csdn, ioaddr + FWLTHTL80);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTL80,
	       config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].csdn);
#endif
	for (dest_ports = 0;
	                dest_ports < config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].destination_vector_config.dest_eth_ports;
	                dest_ports++) {
		dpv |= 1 << config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].destination_vector_config.port_number[dest_ports];
	}
	dpv |= config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].destination_vector_config.cpu << board_config.eth_ports;
	fwlthtl9_val = config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].mirroring_config.cpu_mirror_enable << 21
	               | config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].mirroring_config.eth_mirror_enable << 20
	               | config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].mirroring_config.ipv_config.ipv_update_enable  << 19
	               | config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].mirroring_config.ipv_config.ipv_value  << 16
	               | dpv;
	iowrite32(fwlthtl9_val, ioaddr + FWLTHTL9);
#ifdef DEBUG
	printk("Register address= %x Write value = %x \n",FWLTHTL9,
	       fwlthtl9_val);
#endif
	ret = rswitch2_l3_learn_status();
	if (ret < 0) {
		printk("L3 Learning Failed for entry num= %d\n", entries);
	}
}
#endif


#ifdef CONFIG_RENESAS_RSWITCH2_STATS
/**
    @brief Helper function to get full port name string

    @param  char *

    @param  int *

    @return void

*/
static void getPortNameStringFull(char *buffer, u8 *number)
{
	struct port_private *priv ;
	int i = 0;
	for (i=0; i < board_config.eth_ports; i++) {
		priv = netdev_priv(ppndev[i]);
		if(*number == priv->portnumber) {
			strcpy(buffer, ppndev[i]->name);
			return;	
		}
	}
}


/**
    @brief Helper function to get a port name string

    @param  char *

    @param  int *

    @return void

*/
static void getPortNameString(char *buffer, int *number)
{
	int i;
	char s[10];

	if (board_config.eth_ports < *number)
		*number = board_config.eth_ports;

	for (i=0; i < *number; i++) {
		buffer[i] = '?';
		strncpy(s, ppndev[i]->name, 10);
		s[9] = 0;
		if (strcmp(s, "eth1") == 0)
			buffer[i] = 'E';
		else if (strcmp(s, "tsn0") == 0)
			buffer[i] = 'M';
		else {
			s[3] = 0;
			if (strcmp(s, "tsn") == 0)
				buffer[i] = ppndev[i]->name[3];  //the interface number
		}
	}
}


static void writeTargetFields(struct seq_file * m, 	const char *portName,
	const int portNumber, const u32 dpv, const s32 csdn)
{
	int c = 0;

	for (c = 0; c < portNumber; c++) {
		if (((dpv >> c) & 0x01) == 1)
			seq_printf(m, " %c", portName[c]);
		else
			seq_printf(m, "  ");
	}
	if (((dpv >> board_config.eth_ports) & 0x01) == 0x01) {
		seq_printf(m, " CPU");
		if (csdn >= 0)
			seq_printf(m, ":%-3d", csdn);
	}
	else {
		seq_printf(m, "    ");
		if (csdn >= 0)
			seq_printf(m, "    ");
	}
}


static void writeMirrorEtcFields(struct seq_file * m, 	const char *portName,
	const int portNumber, const u8 cpu_mirror, const u8 eth_mirror,
	const u32 srclockdpv, const s64 destlockdpv, const s64 destcsdn, const u8 ipv_enable, const u32 ipv_value)
{
	int c = 0;

	seq_printf(m," %-3s", eth_mirror ? "Eth" : "No");
	seq_printf(m," %-3s", cpu_mirror ? "CPU" : "No");

	seq_printf(m, " ");
	for (c = 0; c < portNumber; c++) {
		if (((srclockdpv >> c) & 0x01) == 1)
			seq_printf(m, " %c", portName[c]);
		else
			seq_printf(m, "  ");
	}
	if (((srclockdpv >> board_config.eth_ports) & 0x01) == 0x01)
		seq_printf(m, " CPU");
	else
		seq_printf(m, "    ");

	if (destlockdpv >= 0) {
		seq_printf(m, "  ");
		for (c = 0; c < portNumber; c++) {
			if (((destlockdpv >> c) & 0x01) == 1)
				seq_printf(m, " %c", portName[c]);
			else
				seq_printf(m, "  ");
		}
		if (((destlockdpv >> board_config.eth_ports) & 0x01) == 0x01) {
			seq_printf(m, " %s", "CPU");
			if (destcsdn >= 0)
				seq_printf(m, ":%-3lld", destcsdn);
		}
		else {
			seq_printf(m, "    ");
			if (destcsdn >= 0)
				seq_printf(m, "    ");
		}
	}

	if (ipv_enable)
		seq_printf(m, "  %d", ipv_value);
	else
		seq_printf(m, "  -");
}


/**
    @brief L2 Show Show Proc Function

    @param  seq_file *

    @param  void *

    @return int

*/
static int rswitch2_fwd_l2_show(struct seq_file * m, void * v)
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
	u8 mac[6] = {0,0,0,0,0,0};
	u32 ipv_value = 0;
	u8 ipv_enable = 0;
	u8 cpu_mirror = 0;
	u8 eth_mirror = 0;

	char portName[10];
	int  portNumber = 10;
	getPortNameString(portName, &portNumber);

	seq_printf(m, "=========== MAC-TABLE ===================================================================\n");
	seq_printf(m, "Line        MAC         Mode     Target-Ports    Mirror   Src-Valid     Dst-Valid    IPV\n");
	seq_printf(m, "=========================================================================================\n");

	for (entry = 0; entry <= 1023; entry++) {
		iowrite32(entry, ioaddr + FWMACTR);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWMACTRR0);
			if (((value >> 31) & 0x01) == 0x0) {
				if ((((value >> 2) & 0x01) == 0) && (((value >> 1) & 0x01) == 0x00) && (((value >> 0) & 0x01) == 0x1) ) {
					fwmactrr1 = ioread32(ioaddr + FWMACTRR1);
					fwmactrr2 = ioread32(ioaddr + FWMACTRR2);
					fwmactrr3 = ioread32(ioaddr + FWMACTRR3);
					fwmactrr4 = ioread32(ioaddr + FWMACTRR4);
					csdn      = ioread32(ioaddr + FWMACTRR5);
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

					seq_printf(m,"%-4d", entry);
					seq_printf(m," %02x:%02x:%02x:%02x:%02x:%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
					seq_printf(m,"  %-7s", dynamic ? ((ageing) ? "Aged" : "Dynamic") : "Static");

					seq_printf(m, " ");
					writeTargetFields(m, portName, portNumber, dpv, csdn);
					writeMirrorEtcFields(m, portName, portNumber, cpu_mirror, eth_mirror,
						srclockdpv, destlockdpv, -1, ipv_enable, ipv_value);

					//default values are not printed
					if (security)
						seq_printf(m, "   SecurityLevel=%d", security);
					if (learn_disable)
						seq_printf(m, "   HwLearnDisable=%d", learn_disable);

					seq_printf(m, "\n");
				}
				break;
			}
			mdelay(1);
		}
	}
	return 0;
}


/**
    @brief VLAN Show Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_fwd_vlan_show(struct seq_file * m, void * v)
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

	seq_printf(m, "=========== VLAN-TABLE ===========================\n");
	seq_printf(m, "VID   Mirror   Src-Valid     Dst-Valid        IPV\n");
	seq_printf(m, "==================================================\n");
	for (entry = 0; entry <= 0xFFF; entry++) {
		iowrite32(entry, ioaddr + FWVLANTS);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWVLANTSR0);
			if (((value >> 31) & 0x01) == 0x0) {
				if ((((value >> 1) & 0x01) == 0x00) && (((value >> 0) & 0x01) == 0x0) ) {
					fwvlantsr1 = ioread32(ioaddr + FWVLANTSR1);
					csdn       = ioread32(ioaddr + FWVLANTSR2);
					fwvlantsr3 = ioread32(ioaddr + FWVLANTSR3);

					learn_disable = (value >> 10) & 0x01;
					security = (value >> 8) & 0x01;
					srclockdpv = (fwvlantsr1 >> 0) & 0xFF;
					cpu_mirror = (fwvlantsr3 >> 21) & 0x01;
					eth_mirror = (fwvlantsr3 >> 20) & 0x01;
					ipv_enable = (fwvlantsr3 >> 19) & 0x01;
					ipv_value = (fwvlantsr3 >> 16) & 0xFF;
					dpv = (fwvlantsr3 >> 0) & 0xFF;

					seq_printf(m,"%4d ", entry);

					writeMirrorEtcFields(m, portName, portNumber, cpu_mirror, eth_mirror,
						srclockdpv, dpv, csdn, ipv_enable, ipv_value);

					//default values are not printed
					if (security)
						seq_printf(m, "   SecurityLevel=%d", security);
					if (learn_disable)
						seq_printf(m, "   HwLearnDisable=%d", learn_disable);

					seq_printf(m, "\n");
				}
				break;
			}
			mdelay(1);
		}
	}
	return 0;
}


/**
    @brief L3 Show Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_fwd_l3_show(struct seq_file * m, void * v)
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

	seq_printf(m, "=========== L3-TABLE ==================================================\n");
	seq_printf(m, "Line Format    Target-Ports    Mirror   Src-Valid    IPV  Rule#\n");
	seq_printf(m, "=======================================================================\n");

	for (entry = 0; entry <= 1023; entry++) {
		iowrite32(entry, ioaddr + FWLTHTR);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWLTHTRR0);
			if (((value >> 31) & 0x01) == 0x0) {
				if ((((value >> 2) & 0x01) == 0) && (((value >> 1) & 0x01) == 0x01) && (((value >> 0) & 0x01) == 0x0) ) {
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
					}
					else
						routingvalid = 0;

					csdn = ioread32(ioaddr + FWLTHTRR9);

					fwlthtrr10 = ioread32(ioaddr + FWLTHTRR10);
					cpu_mirror = (fwlthtrr10 >> 21) & 0x01;
					eth_mirror = (fwlthtrr10 >> 20) & 0x01;
					ipv_enable = (fwlthtrr10 >> 19) & 0x01;
					ipv_value = (fwlthtrr10 >> 15) & 0xFF;
					dpv = (fwlthtrr10 >> 0) & 0xFF;


					seq_printf(m,"%-4d", entry);

					if (mode == 0x01)
						seq_printf(m, " %-9s", "IPv4");
					else if (mode == 0x02)
						seq_printf(m, " %-9s", "IPv4+UDP");
					else if (mode == 0x03)
						seq_printf(m, " %-9s", "IPv4+TCP");
					else if (mode == 0x00)
						seq_printf(m, " %-9s", "Filter");

					writeTargetFields(m, portName, portNumber, dpv, csdn);
					writeMirrorEtcFields(m, portName, portNumber, cpu_mirror, eth_mirror,
						srclockdpv, -1, -1, ipv_enable, ipv_value);

					if (routingvalid)
						seq_printf(m, "    %-2d", routingnum);
					else
						seq_printf(m, "    - ");

					//default values are not printed
					if (security)
						seq_printf(m, "   SecurityLevel=%d", security);

					seq_printf(m, "\n");


				//the 2nd line
					if (mode == 0) {
						//seq_printf(m, "FilterNr=%d\n", filter_number);
					}
					else {
						seq_printf(m, "      \\-> ");
						sprintf(str1, "%d.%d.%d.%d", srcipaddr[0], srcipaddr[2], srcipaddr[2], srcipaddr[3]);
						sprintf(str2, "%d.%d.%d.%d:%d", destipaddr[0], destipaddr[1], destipaddr[2], destipaddr[3], destipport);
						seq_printf(m, "SA=%s  DA=%s  CTag=%04x  STag=%04x  Hash=%04x\n", str1, str2, ctag, stag, hash);
					}
				}
				break;
			}
			mdelay(1);
		}
	}
	return 0;
}


/**
    @brief L2 L3 Update Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_fwd_l2_l3_update_show(struct seq_file * m, void * v)
{
	u32 i;
	u32 entry;
	u32 value;
	u32 fwl23urrr1;
	u32 fwl23urrr2;
	u32 fwl23urrr3;

	u8 mac[6] ;
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
	int  portNumber = 10;
	getPortNameString(portName, &portNumber);

	seq_printf(m, "=========== Routing-Update-Table ==========================================\n");
	seq_printf(m, "Rule# Dst-Valid    TTL  DA-MAC            SA-MAC   CTAG       STAG    RTAG\n");
	seq_printf(m, "================================================== Id P D === Id P D ======\n");

	for (entry = 0; entry <= 255; entry++) {
		iowrite32(entry, ioaddr + FWL23URR);
		for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
			value = ioread32(ioaddr + FWL23URRR0);
			if (((value >> 31) & 0x01) == 0x0) {
				if ((((value >> 16) & 0x01) == 0) && (value & 0xFFFF)) {
					fwl23urrr1 = ioread32(ioaddr + FWL23URRR1);
					fwl23urrr2 = ioread32(ioaddr + FWL23URRR2);
					fwl23urrr3 = ioread32(ioaddr + FWL23URRR3);

					dpv    = value & 0xFF;
					rtag   = (fwl23urrr1 >> 25) & 0x03;
					sdieul = (fwl23urrr1 >> 24) & 0x01;
					spcpul = (fwl23urrr1 >> 23) & 0x01;
					sidul  = (fwl23urrr1 >> 22) & 0x01;
					cdieul = (fwl23urrr1 >> 21) & 0x01;
					cpcpul = (fwl23urrr1 >> 20) & 0x01;
					cidul  = (fwl23urrr1 >> 19) & 0x01;
					msaul  = (fwl23urrr1 >> 18) & 0x01;
					mdaul  = (fwl23urrr1 >> 17) & 0x01;
					ttlul  = (fwl23urrr1 >> 16) & 0x01;

					mac[0] = (fwl23urrr1 >> 8) & 0xFF;
					mac[1] = (fwl23urrr1 >> 0) & 0xFF;
					mac[2] = (fwl23urrr2 >> 24) & 0xFF;
					mac[3] = (fwl23urrr2 >> 16) & 0xFF;
					mac[4] = (fwl23urrr2 >> 8) & 0xFF;
					mac[5] = (fwl23urrr2 >> 0) & 0xFF;

					seq_printf(m, "%-4d ", entry);

					writeTargetFields(m, portName, portNumber, dpv, -1);

					seq_printf(m, "  %-3s", (ttlul)?"dec":"-");

					if (mdaul)
						sprintf(str1, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
					else
						strcpy(str1, "-");
					seq_printf(m, "  %-18s", str1);

					seq_printf(m, "  %-3s", (msaul)?"Yes":"-");

					if (cidul)
						seq_printf(m, "  %4d", (fwl23urrr3>>0) & 0xFFF);
					else
						seq_printf(m, "     -");
					if (cpcpul)
						seq_printf(m, " %d", (fwl23urrr3>>12) & 0x07);
					else
						seq_printf(m, " -");
					if (cdieul)
						seq_printf(m, " %d", (fwl23urrr3>>15) & 0x01);
					else
						seq_printf(m, " -");

					if (sidul)
						seq_printf(m, "   %4d", (fwl23urrr3>>16) & 0xFFF);
					else
						seq_printf(m, "      -");
					if (spcpul)
						seq_printf(m, " %d", (fwl23urrr3>>28) & 0x07);
					else
						seq_printf(m, " -");
					if (sdieul)
						seq_printf(m, " %d", (fwl23urrr3>>31) & 0x01);
					else
						seq_printf(m, " -");

					seq_printf(m, "   %d", rtag);

					seq_printf(m, "\n");
				}
				break;
			}
			mdelay(1);
		}
	}
	return 0;
}


/**
    @brief Errors Show Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
    Tas and CBS error not covered tbd later when functionality implemented
*/
static int rswitch2_fwd_errors_show(struct seq_file * m, int longreport)
{
	u8 i = 0;
	char buffer[4];
	seq_printf(m, "           ");
	for (i = 0; i < board_config.eth_ports; i++) {
		getPortNameStringFull(buffer, &i);
		seq_printf(m, "%8s", buffer);
	}
	seq_printf(m, "%7s\n","CPU");
#if 1
	//seq_printf(m, "\n");
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ddntfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 29) & 0x01;
		if((!errors.ddntfs.set_any) && (errors.ddntfs.error[i])) {
			errors.ddntfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ddses.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 28) & 0x01;
		if((!errors.ddses.set_any) && (errors.ddses.error[i])) {
			errors.ddses.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ddfes.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 27) & 0x01;
		if((!errors.ddfes.set_any) && (errors.ddfes.error[i])) {
			errors.ddfes.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ddes.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 26) & 0x01;
		if((!errors.ddes.set_any) && (errors.ddes.error[i])){
			errors.ddes.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.wmiufs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 25) & 0x01;
		if((!errors.wmiufs.set_any) && (errors.wmiufs.error[i])) {
			errors.wmiufs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.wmisfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 24) & 0x01;
		if((!errors.wmisfs.set_any) && (errors.wmisfs.error[i])) {
			errors.wmisfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.wmffs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 23) & 0x01;
		if((!errors.wmffs.set_any) && (errors.wmffs.error[i])){
			errors.wmffs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.wmcfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 22) & 0x01;
		if((!errors.wmcfs.set_any) && (errors.wmcfs.error[i])){
			errors.wmcfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.smhmfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 19) & 0x01;
		if((!errors.smhmfs.set_any) && (errors.smhmfs.error[i])) {
			errors.smhmfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.smhlfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 18) & 0x01;
		if((!errors.smhlfs.set_any) && (errors.smhlfs.error[i])) {
			errors.smhlfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.pbntfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 17) & 0x01;
		if((!errors.pbntfs.set_any) && (errors.pbntfs.error[i])) {
			errors.pbntfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwvufs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 16) & 0x01;
		if((!errors.ltwvufs.set_any) && (errors.ltwvufs.error[i])) {
			errors.ltwvufs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwdufs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 15) & 0x01;
		if((!errors.ltwdufs.set_any) && (errors.ltwdufs.error[i])) {
			errors.ltwdufs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwsufs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 14) & 0x01;
		if((!errors.ltwsufs.set_any) && (errors.ltwsufs.error[i])) {
			errors.ltwsufs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwntfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 13) & 0x01;
		if((!errors.ltwntfs.set_any) && (errors.ltwntfs.error[i])) {
			errors.ltwntfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwvspfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 12) & 0x01;
		if((!errors.ltwvspfs.set_any) && (errors.ltwvspfs.error[i])) {
			errors.ltwvspfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwsspfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 11) & 0x01;
		if((!errors.ltwsspfs.set_any) && (errors.ltwsspfs.error[i])) {
			errors.ltwsspfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.ltwdspfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 10) & 0x01;
		if((!errors.ltwdspfs.set_any) && (errors.ltwdspfs.error[i])) {
			errors.ltwdspfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.lthufs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 3) & 0x01;
		if((!errors.lthufs.set_any) && (errors.lthufs.error[i])) {
			errors.lthufs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.lthntfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 2) & 0x01;
		if((!errors.lthntfs.set_any) && (errors.lthntfs.error[i])) {
			errors.lthntfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.lthfsfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 1) & 0x01;
		if((!errors.lthfsfs.set_any) && (errors.lthfsfs.error[i])) {
			errors.lthfsfs.set_any = 1;
		}
	}
	for (i = 0; i <= board_config.eth_ports; i++) {
		errors.lthspfs.error[i] = (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 0) & 0x01;
		if((!errors.lthspfs.set_any) && (errors.lthspfs.error[i])) {
			errors.lthspfs.set_any = 1;
		}
	}
	if(errors.ddntfs.set_any || longreport) {
		seq_printf(m, "%-8s","DDNTFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ddntfs.error[i]);
		}
		seq_printf(m, "  Direct Descr, No Target\n");
	}
	if(errors.ddses.set_any || longreport) {
		seq_printf(m, "%-8s","DDSES");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ddses.error[i]);
		}
		seq_printf(m, "  Direct Descr, Security\n");
	}
	if(errors.ddfes.set_any || longreport) {
		seq_printf(m, "%-8s","DDFES");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ddfes.error[i]);
		}
		seq_printf(m, "  Direct Descr, Format\n");
	}
	if(errors.ddes.set_any || longreport) {
		seq_printf(m, "%-8s","DDES");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ddes.error[i]);
		}
		seq_printf(m, "  Direct Descr, Error\n");
	}
	if(errors.wmiufs.set_any || longreport) {
		seq_printf(m, "%-8s","WMIUFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.wmiufs.error[i]);
		}
		seq_printf(m, "  WaterMark IPV Unsecure\n");
	}
	if(errors.wmisfs.set_any || longreport) {
		seq_printf(m, "%-8s","WMISFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.wmisfs.error[i]);
		}
		seq_printf(m, "  WaterMark IPV Secure\n");
	}
	if(errors.wmffs.set_any || longreport) {
		seq_printf(m, "%-8s","WMFFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.wmffs.error[i]);
		}
		seq_printf(m, "  WaterMark Flush\n");
	}
	if(errors.wmcfs.set_any || longreport) {
		seq_printf(m, "%-8s","WMCFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.wmcfs.error[i]);
		}
		seq_printf(m, "  WaterMark Critical\n");
	}
	if(errors.smhmfs.set_any || longreport) {
		seq_printf(m, "%-8s","SMHLFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.smhmfs.error[i]);
		}
		seq_printf(m, "  Source MAC Migration\n");
	}
	if(errors.smhlfs.set_any || longreport) {
		seq_printf(m, "%-8s","SMHLFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.smhlfs.error[i]);
		}
		seq_printf(m, "  Source MAC Learning\n");
	}
	if(errors.pbntfs.set_any || longreport) {
		seq_printf(m, "%-8s","PBNTFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.pbntfs.error[i]);
		}
		seq_printf(m, "  Port Based No Target\n");
	}
	if(errors.ltwvufs.set_any || longreport) {
		seq_printf(m, "%-8s","LTWVUFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ltwvufs.error[i]);
		}
		seq_printf(m, "  L2 VLAN Unknown\n");
	}
	if(errors.ltwdufs.set_any || longreport) {
		seq_printf(m, "%-8s","LTWDUFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ltwdufs.error[i]);
		}
		seq_printf(m, "  L2 Dest Unknown\n");
	}
	if(errors.ltwsufs.set_any || longreport) {
		seq_printf(m, "%-8s","LTWSUFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ltwsufs.error[i]);
		}
		seq_printf(m, "  L2 Source Unknown\n");
	}
	if(errors.ltwntfs.set_any || longreport) {
		seq_printf(m, "%-8s","LTWNTFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ltwntfs.error[i]);
		}
		seq_printf(m, "  L2 No Target\n");
	}
	if(errors.ltwvspfs.set_any || longreport) {
		seq_printf(m, "%-8s","LTWVSPFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ltwvspfs.error[i]);
		}
		seq_printf(m, "  L2 VLAN Source Port\n");
	}
	if(errors.ltwsspfs.set_any || longreport) {
		seq_printf(m, "%-8s","LTWSSPFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ltwsspfs.error[i]);
		}
		seq_printf(m, "  L2 Source Source Port\n");
	}
	if(errors.ltwdspfs.set_any || longreport) {
		seq_printf(m, "%-8s","LTWDSPFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.ltwdspfs.error[i]);
		}
		seq_printf(m, "  L2 Destination Source Port\n");
	}
	if(errors.lthufs.set_any || longreport) {
		seq_printf(m, "%-8s","LTHUFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.lthufs.error[i]);
		}
	
		seq_printf(m, "  L3 Unknown\n");
	}
	if(errors.lthntfs.set_any || longreport) {
		seq_printf(m, "%-8s","LTHNTFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.lthntfs.error[i]);
		}
		seq_printf(m, "  L3 No Target\n");
	}
	if(errors.lthfsfs.set_any || longreport) {
		seq_printf(m, "%-8s","LTHFSFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.lthfsfs.error[i]);
		}
		seq_printf(m, "  L3 Format Security\n");
	}
	if(errors.lthspfs.set_any || longreport) {
		seq_printf(m, "%-8s","LTHSPFS");
		for (i = 0; i <= board_config.eth_ports; i++) {
			seq_printf(m, "%8d", errors.lthspfs.error[i]);
		}
		seq_printf(m, "  L3 Source Port\n");
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
	if(errors.global_error.arees || errors.global_error.l23uses || 
	errors.global_error.l23uees || errors.global_error.vlantses || 
	errors.global_error.vlantees || errors.global_error.mactses || 
	errors.global_error.mactees ||	errors.global_error.lthtses || 
	errors.global_error.lthtees ||	errors.global_error.macadas || 
	errors.global_error.vlantfs ||	errors.global_error.mactfs || errors.global_error.lthtfs) {
		errors.global_error.set_any = 1;
	}
	if(errors.global_error.set_any  || longreport) {
		seq_printf(m, "\n");
		seq_printf(m, "=======================Port-Independent-Errors===========================\n");
		if(errors.global_error.arees || longreport) {
			seq_printf(m, "AREES    %8d  ATS RAM ECC\n",errors.global_error.arees );
		}
		if(errors.global_error.l23uses || longreport) {
			seq_printf(m, "L23USES  %8d  L2/L3 Update Security\n",errors.global_error.l23uses );
		}
		if(errors.global_error.l23uees || longreport) {
			seq_printf(m, "L23UEES  %8d  Layer2/Layer3 Update ECC\n",errors.global_error.l23uees);
		}
		if(errors.global_error.vlantses || longreport) {
			seq_printf(m, "VLANTSES %8d  VLAN Table Security\n",errors.global_error.vlantses);
		}
		if(errors.global_error.vlantees || longreport) {
			seq_printf(m, "VLANTEES %8d  VLAN Table ECC\n",errors.global_error.vlantees );
		}
		if(errors.global_error.mactses || longreport) {	
			seq_printf(m, "MACTSES  %8d  MAC Table Security\n",errors.global_error.mactses);
		}
		if(errors.global_error.mactees || longreport) {
			seq_printf(m, "MACTEES  %8d  MAC Table ECC\n",errors.global_error.mactees);
		}
		if(errors.global_error.lthtses || longreport) {
			seq_printf(m, "LTHTSES  %8d  L3 Table Security\n",errors.global_error.lthtses);
		}
		if(errors.global_error.lthtees || longreport) {
			seq_printf(m, "LTHTEES  %8d  L3 Table ECC\n",errors.global_error.lthtees);
		}
		if(errors.global_error.macadas || longreport) {
			seq_printf(m, "MACADAS  %8d  MAC Address Deleted Aging\n",errors.global_error.macadas);
		}
		if(errors.global_error.vlantfs || longreport) {
			seq_printf(m, "VLANTFS  %8d  VLAN Table Full\n",errors.global_error.vlantfs );
		}
		if(errors.global_error.mactfs || longreport) {
			seq_printf(m, "MACTFS   %8d  MAC Table Full\n",errors.global_error.mactfs);
		}
		if(errors.global_error.lthtfs || longreport) {
			seq_printf(m, "LTHTFS   %8d  L3 Table Full\n",errors.global_error.lthtfs);
		}
	}
#endif
	return 0;
}


/**
    @brief Wrapper function to show Error in /proc either in long format

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_fwd_errors_all_show(struct seq_file * m, void * v)
{
	return rswitch2_fwd_errors_show(m, 1);
}


/**
    @brief Wrapper function to show error in /proc either in short format

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_fwd_errors_short_show(struct seq_file * m, void * v)
{
	return rswitch2_fwd_errors_show(m, 0);
}

/**
    @brief rswitch2 Fwd counters clear function

    @param  void

    @return int
*/
static int rswitch2_fwd_counter_clear_func(void)
{
	memset(&counter,0, sizeof(struct rswitch2_fwd_counter) );
	return 0;
}



/**
    @brief rswitch2 Fwd Errors clear function

    @param  void

    @return int
*/
static int rswitch2_fwd_error_clear_func(void)
{
	int i = 0;
	for (i = 0; i <= board_config.eth_ports; i++) {
		iowrite32(0xFFFFFFFF,ioaddr + FWEIS00 + (i *0x10));
	}
	iowrite32(0xFFFFFFFF,ioaddr + FWEIS1);
	iowrite32(0xFFFFFFFF,ioaddr + FWMIS0);
	memset(&errors,0, sizeof(struct rswitch2_fwd_error) );
	return 0;
}




/**
    @brief Write FWD Counter Clear Function for Proc

    @param  struct file *
    @param  const char *
    @param  size_t
    @param  loff_t *

    @return ssize_t
*/
static ssize_t rswitch2_fwd_counters_clear(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	u32 ret = 0;
	u64 fwd_counter_clear = 0;
	ret = kstrtoull_from_user(buff, len, 10, &fwd_counter_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}
	else {
		rswitch2_fwd_counter_clear_func();
		return len;
	}
}

/**
    @brief Write FWD Errors Clear Function for Proc

    @param  struct file *
    @param  const char *
    @param  size_t
    @param  loff_t *

    @return ssize_t
*/
static ssize_t rswitch2_fwd_errors_clear(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	u32 ret = 0;
	u64 fwd_error_clear = 0;
	ret = kstrtoull_from_user(buff, len, 10, &fwd_error_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}
	else {
		rswitch2_fwd_error_clear_func();
		return len;
	}
}



/**
    @brief Counters Show Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_fwd_counters_show(struct seq_file * m, void * v)
{
	u8 i = 0;
	char buffer[4];
	seq_printf(m, "           ");
	for (i = 0; i < board_config.eth_ports; i++) {
		getPortNameStringFull(buffer, &i);
		seq_printf(m, "%8s", buffer);
	}
	seq_printf(m, "%7s\n","CPU");
	/*seq_printf(m, "%-8s","CTFDN");
	for (i = 0; i < board_config.eth_ports; i++) {
		counter.ctfdn[i] += ioread32(ioaddr + FWCTFDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.ctfdn[i]));
	}
	seq_printf(m, "  Cut-Through Descr\n");*/
	seq_printf(m,"%-8s", "DDFDN");
	for (i = 0; i < board_config.eth_ports; i++) {
		seq_printf(m, "%8s","-");
	}
	counter.ddfdn[i] += ioread32(ioaddr + FWDDFDCN0 + (board_config.eth_ports *20));
	seq_printf(m, "%8d", counter.ddfdn[i]);
	
	seq_printf(m, "  Direct Descr\n");
	seq_printf(m, "%-8s","LTHFDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.lthfdn[i] += ioread32(ioaddr + FWLTHFDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.lthfdn[i])) ;

	}
	seq_printf(m, "  L3 Descr\n");
	/*seq_printf(m, "%-8s","IPFDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.ipfdn[i] += ioread32(ioaddr + FWIPFDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.ipfdn[i])) ;
	}
	seq_printf(m, "  IP Descr\n");*/
	seq_printf(m, "%-8s","LTWFDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.ltwfdn[i] += ioread32(ioaddr + FWLTWFDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.ltwfdn[i])) ;

	}
	seq_printf(m, "  L2 Descr\n");
	seq_printf(m, "%-8s","PBFDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.pbfdn[i] += ioread32(ioaddr + FWPBFDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.pbfdn[i])) ;

	}
	seq_printf(m, "  Port-Based Descr\n");
	seq_printf(m, "%-8s","MHLN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.mhln[i] += ioread32(ioaddr + FWMHLCN0 + (i *20));
		seq_printf(m, "%8d", (counter.mhln[i])) ;
	}
	seq_printf(m, "  MAC Learned\n");
	/*seq_printf(m, "%-8s","IHLN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.ihln[i] += ioread32(ioaddr + FWIHLCN0 + (i *20));
		seq_printf(m, "%8d", (counter.ihln[i])) ;
	}
	seq_printf(m, "\n");*/
	seq_printf(m, "%-8s","ICRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.icrdn[i] += ioread32(ioaddr + FWICRDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.icrdn[i])) ;
	}
	seq_printf(m, "  Reject Integrity Check\n");
	seq_printf(m, "%-8s","WMRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.wmrdn[i] += ioread32(ioaddr + FWWMRDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.wmrdn[i])) ;
	}
	seq_printf(m, "  Reject WaterMark\n");
	/*seq_printf(m, "%-8s","CTRDN");
	for (i = 0; i < board_config.eth_ports; i++) {
		counter.ctrdn[i] += ioread32(ioaddr + FWCTRDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.ctrdn[i])) ;
	}
	seq_printf(m, "  Reject Cut-through\n");*/
	seq_printf(m, "%-8s","DDRDN");
	for (i = 0; i < board_config.eth_ports; i++) {
		seq_printf(m, "%8s","-");
	}
	counter.ddrdn[i] += ioread32(ioaddr + FWDDRDCN0 + (board_config.eth_ports *20));
	seq_printf(m, "%8d", counter.ddrdn[i]);
	seq_printf(m, "  Reject Direct Descr\n");
	seq_printf(m, "%-8s","LTHRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.lthrdn[i] += ioread32(ioaddr + FWLTHRDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.lthrdn[i])) ;
	}
	seq_printf(m, "  Reject L3\n");
	/*seq_printf(m, "%-8s","IPRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.iprdn[i] += ioread32(ioaddr + FWIPRDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.iprdn[i]));
	}
	seq_printf(m, "  Reject IP\n");*/
	seq_printf(m, "%-8s","LTWRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.ltwrdn[i] += ioread32(ioaddr + FWLTWRDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.ltwrdn[i])) ;
	}
	seq_printf(m, "  Reject L2\n");
	seq_printf(m, "%-8s","PBRDN");
	for (i = 0; i <= board_config.eth_ports; i++) {
		counter.pbrdn[i] += ioread32(ioaddr +  FWPBRDCN0 + (i *20));
		seq_printf(m, "%8d", (counter.pbrdn[i])) ;
	}
	seq_printf(m, "  Reject Port-Based\n");
	return 0;
}


/**
    @brief Buffer Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_fwd_buffer_show(struct seq_file * m, void * v)
{
	int i, j;
	u32 r, rl, rh;
	u32 total;
	const u32 ports = 5;
	int sum=0;

	r = ioread32(ioaddr + CABPPCM);
	rh = (r >> 16);
	rl = (r & 0xffff);
	total = rh;
	sum = total - rl;
	seq_printf(m, "Total existing      %04x\n", total);
	seq_printf(m, "Remaining current   %04x  %d%%\n", rl, rl*100/total);
	r = ioread32(ioaddr + CABPLCM);
	seq_printf(m, "Remaining minimum   %04x  %d%%\n\n", r, r*100/total);

	seq_printf(m, "               tsn6   tsn7   tsn5   tsn4  tsngw  --  reject\n");
	seq_printf(m, "Used current");
	for (i = 0; i < ports; i++) {
		r = ioread32(ioaddr + CABPCPM+(i*4));
		sum -= r;
		seq_printf(m, "   %04x", r);
	}
	r = ioread32(ioaddr + CARDNM);
	seq_printf(m, "  --  %04x\n", r);
	seq_printf(m, "Used maximum");
	for (i = 0; i < ports; i++) {
		r = ioread32(ioaddr + CABPMCPM+(i*4));
		seq_printf(m, "   %04x", r);
	}
	r = ioread32(ioaddr + CARDMNM);
	seq_printf(m, "  --  %04x\n", r);

	r = ioread32(ioaddr + CARDCN);
	seq_printf(m, "Reject total   %08x  %d\n", r, r);

	if (sum != 0)
		seq_printf(m, "Lost buffers %d (small differences normal in case of ongoing traffic)\n", sum);

	seq_printf(m, "\n                        tsn6   tsn7   tsn5   tsn4  tsngw\n");
	for (j = 0; j < 8; j++) {
		seq_printf(m, "Pending frames in Q#%d", j);
		for (i = 0; i < ports; i++) {
			r = ioread32(ioaddr + EATDQM0+(i*4) + 0xA000 + j*0x2000);
			seq_printf(m, " %6d", r);
		}
		seq_printf(m, "\n");
	}

	return 0;
}

/**
    @brief Show Three Byte Filter 

    @param  struct seq_file * m 

    @return void
*/
static void rswitch2_fwd_show_three_byte_filter(struct seq_file * m)
{
	u32 print0 = 0;
	u32 print1 = 0;
	char streamid0[5] = {'|','|','|','|'};
	char streamid1[5] = {'|','|','|','|'};
	char comp[6] = {'-','-','-','-','-'};
	u32 value0 = 0;
	u32 value1 = 0;
	u32 value2 = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct rswitch2_fwd_filter_config  *filter;
	filter = &filter_tbl_config;
	for(j = 0; j < filter->num_three_byte_filters; j++) {
		print0  = 0;
		print1 = 0;
		strcpy(streamid0, "||||");
		strcpy(streamid1, "||||");
		strcpy(comp, "-----");
		for(i = 0; i < filter->num_cascade_filters; i++) {
			for(k = 0; k < filter->cascade[i].used_filter_ids; k++) {
				if(filter->three_byte[j].filterid0 == filter->cascade[i].used_filter_id_num[k]) {
					streamid0[i] = '&';
					print0 = 1;
				}
				if(filter->three_byte[j].unit_mode == precise) {
					if(filter->three_byte[j].filterid1 == filter->cascade[i].used_filter_id_num[k]) {
						print1 = 1;
						streamid1[i] = '&';
					}
				}
			}
		}
		if(print0) {
			seq_printf(m, "%-4d", filter->three_byte[j].filterid0);
			comp[0] = '3';
			comp[2] = filter->three_byte[j].filter_number + '0';
			if(filter->three_byte[j].unit_mode == mask) {
				comp[4] = 'M';
			} else if(filter->three_byte[j].unit_mode == expand) {
				comp[4] = 'E';
			} else if(filter->three_byte[j].unit_mode == precise) {
				comp[4] = 'P';
			} 
			seq_printf(m, "%-8s", comp);
			seq_printf(m, "%-6d", filter->three_byte[j].offset);
			
			if(filter->three_byte[j].unit_mode != expand) {
				value0 = (filter->three_byte[j].value0 >> 16) & 0xFF;
				value1 =  filter->three_byte[j].value0 & 0xFFFF;
				seq_printf(m, "0x%02x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%13s", " ");
			} else {
				value0 = (filter->three_byte[j].value0 >> 8) & 0xFFFF;
				value1 =  (((filter->three_byte[j].value0 << 8) & 0xFF00) | ((filter->three_byte[j].value1 >> 16) & 0xFF));
				value2 = filter->three_byte[j].value1 & 0xFFFF;
				seq_printf(m, "0x%04x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value2);
				seq_printf(m, "%-6s", " ");
			}
			if(filter->three_byte[j].unit_mode == mask) {
				value0 = (filter->three_byte[j].value1 >> 16) & 0xFF;
				value1 =  filter->three_byte[j].value1 & 0xFFFF;
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
		if(print1) {
			seq_printf(m, "%-4d", filter->three_byte[j].filterid1);
			comp[0] = '3';
			comp[2] = filter->three_byte[j].filter_number + '0';
			
			comp[4] = 'P';
			
			seq_printf(m, "%-8s", comp);
			
			seq_printf(m, "%-6d", filter->three_byte[j].offset);
			value0 = (filter->three_byte[j].value1 >> 16) & 0xFF;
			value1 =  filter->three_byte[j].value1 & 0xFFFF;
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

/**
    @brief Show Four Byte Filter 

    @param  struct seq_file * m 

    @return void
*/
static void rswitch2_fwd_show_four_byte_filter(struct seq_file * m)
{
	char streamid0[5] = {'|','|','|','|'};
	char streamid1[5] = {'|','|','|','|'};
	char comp[6] = {'-','-','-','-','-'};
	u32 value0 = 0;
	u32 value1 = 0;
	u32 value2 = 0;
	u32 value3 = 0;
	u32 print0 = 0;
	u32 print1 = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct rswitch2_fwd_filter_config  *filter;
	filter = &filter_tbl_config;
	for(j = 0; j < filter->num_four_byte_filters; j++) {
		print0  = 0;
		print1 = 0;
		strcpy(streamid0, "||||");
		strcpy(streamid1, "||||");
		strcpy(comp, "-----");
		for(i = 0; i < filter->num_cascade_filters; i++) {
			for(k = 0; k < filter->cascade[i].used_filter_ids; k++) {
				if(filter->four_byte[j].filterid0 == filter->cascade[i].used_filter_id_num[k]) {
					streamid0[i] = '&';
					print0 = 1;
				}
				if(filter->four_byte[j].unit_mode == precise) {
					if(filter->four_byte[j].filterid1 == filter->cascade[i].used_filter_id_num[k]) {
						print1 = 1;
						streamid1[i] = '&';
					}
				}
			}
		}
		if(print0) {
			seq_printf(m, "%-4d", filter->four_byte[j].filterid0);
			comp[0] = '4';
			comp[2] = filter->four_byte[j].filter_number + '0';
			if(filter->four_byte[j].unit_mode == mask) {
				comp[4] = 'M';
			} else if(filter->four_byte[j].unit_mode == expand) {
				comp[4] = 'E';
			} else if(filter->four_byte[j].unit_mode == precise) {
				comp[4] = 'P';
			} 
			seq_printf(m, "%-8s", comp);
			seq_printf(m, "%-6d", filter->four_byte[j].offset);
			
			if(filter->four_byte[j].unit_mode != expand) {
				value0 = (filter->four_byte[j].value0 >> 16) & 0xFFFF;
				value1 =  filter->four_byte[j].value0 & 0xFFFF;
				seq_printf(m, "0x%04x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%-11s", " ");
			} else {
				value0 = (filter->four_byte[j].value0 >> 16) & 0xFFFF;
				value1 =  (filter->four_byte[j].value0 ) & 0xFFFF;
				value2 = (filter->four_byte[j].value1 >> 16) & 0xFFFF;
				value3 =  (filter->four_byte[j].value1 ) & 0xFFFF;
				seq_printf(m, "0x%04x", value0);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value1);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value2);
				seq_printf(m, "%c", '_');
				seq_printf(m, "%04x", value3);
				seq_printf(m, "%-1s", " ");
			}
			if(filter->four_byte[j].unit_mode == mask) {
				value0 = (filter->four_byte[j].value1 >> 16) & 0xFFFF;
				value1 =  filter->four_byte[j].value1 & 0xFFFF;
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
		if(print1) {
			seq_printf(m, "%-4d", filter->four_byte[j].filterid1);
			comp[0] = '4';
			comp[2] = filter->four_byte[j].filter_number + '0';
			
			comp[4] = 'P';
			
			seq_printf(m, "%-8s", comp);
			
			seq_printf(m, "%-6d", filter->four_byte[j].offset);
			value0 = (filter->four_byte[j].value1 >> 16) & 0xFFFF;
			value1 =  filter->four_byte[j].value1 & 0xFFFF;
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

/**
    @brief Show Range Filter 

    @param  struct seq_file * m 

    @return void
*/
static void rswitch2_fwd_show_range_filter(struct seq_file * m)
{
	char streamid0[5] = {'|','|','|','|'};
	char streamid1[5] = {'|','|','|','|'};
	char comp[4] = {'-','-','-'};
	u32 print0 = 0;
	u32 print1 = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct rswitch2_fwd_filter_config  *filter;
	filter = &filter_tbl_config;
	for(j = 0; j < filter->num_range_filters; j++) {
		print0  = 0;
		print1 = 0;
		strcpy(streamid0, "||||");
		strcpy(streamid1, "||||");
		strcpy(comp, "--");
		for(i = 0; i < filter->num_cascade_filters; i++) {
			for(k = 0; k < filter->cascade[i].used_filter_ids; k++) {
				if(filter->range[j].filterid0 == filter->cascade[i].used_filter_id_num[k]) {
					streamid0[i] = '&';
					print0 = 1;
				}

				if(filter->range[j].filterid1 == filter->cascade[i].used_filter_id_num[k]) {
					print1 = 1;
					streamid1[i] = '&';
				}
			}
		}
		if(print0) {
			seq_printf(m, "%-4d", filter->range[j].filterid0);
			comp[0] = 'R';
			comp[2] = filter->range[j].filter_number + '0';
			
			seq_printf(m, "%-8s", comp);
			if(filter->range[j].offset_mode == offset) {
				seq_printf(m, "%-6d", filter->range[j].offset);
			} else if(filter->range[j].offset_mode == vlan_tag) {
				if((filter->range[j].offset == 0x0) || (filter->range[j].offset == 0x1)) {
					seq_printf(m, "%-6s", "STAG");
				} else if((filter->range[j].offset == 0x2) || (filter->range[j].offset == 0x03)) {
					seq_printf(m, "%-6s", "CTAG");
				}
				
			}
			
			seq_printf(m, "%03d", filter->range[j].value0);
			seq_printf(m, "%c", '~');
			seq_printf(m, "%03d", filter->range[j].value0 + filter->range[j].range);
			if(filter->range[j].offset_mode == vlan_tag) {
				if((filter->range[j].offset == 0x0) || (filter->range[j].offset == 0x2)) {
					seq_printf(m, "%-15s", "(byte 0)");
					seq_printf(m, "%-13s", "-");
				} else if((filter->range[j].offset == 0x1) || (filter->range[j].offset == 0x03)) {
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
		if(print1) {
			seq_printf(m, "%-4d", filter->range[j].filterid1);
			comp[0] = 'R';
			comp[2] = filter->range[j].filter_number + '0';
			seq_printf(m, "%-8s", comp);
			if(filter->range[j].offset_mode == offset) {
				seq_printf(m, "%-6d", filter->range[j].offset);
			} else if(filter->range[j].offset_mode == vlan_tag) {
				if((filter->range[j].offset == 0x0) || (filter->range[j].offset == 0x1)) {
					seq_printf(m, "%-6s", "STAG");
				} else if((filter->range[j].offset == 0x2) || (filter->range[j].offset == 0x03)) {
					seq_printf(m, "%-6s", "CTAG");
				}
				
			}
			
			seq_printf(m, "%03d", filter->range[j].value1);
			seq_printf(m, "%c", '~');
			seq_printf(m, "%03d", filter->range[j].value1 + filter->range[j].range);
			if(filter->range[j].offset_mode == vlan_tag) {
				if((filter->range[j].offset == 0x0) || (filter->range[j].offset == 0x2)) {
					seq_printf(m, "%-15s", "(byte 0)");
				} else if((filter->range[j].offset == 0x1) || (filter->range[j].offset == 0x03)) {
					seq_printf(m, "%-15s", "(byte 1)");
				}
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

/**
    @brief Show Source Valids for cascade filter 

    @param  struct seq_file * m 

    @return void
*/
static void rswitch2_fwd_show_src_valid(struct seq_file * m)
{
	u32 fwcfc = 0;
	u8 i  = 0;
	u8 j  = 0;
	u32 pdpv = 0;
	u32 edpv = 0;
	struct rswitch2_fwd_filter_src_valid psrc[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS];
	struct rswitch2_fwd_filter_src_valid esrc[RENESAS_RSWITCH2_MAX_PORT_AGENT];
	for(i  = 0; i < board_config.eth_ports; i++) {
		memset (&psrc[i], 0, sizeof(struct rswitch2_fwd_filter_src_valid));
		memset (&esrc[i], 0, sizeof(struct rswitch2_fwd_filter_src_valid));
	}
	memset (&esrc[i], 0, sizeof(struct rswitch2_fwd_filter_src_valid));
	for(j = 0; j < board_config.eth_ports; j++) {
		strcpy(esrc[j].stream_str, "||||");
		strcpy(psrc[j].stream_str, "||||");
	}
	strcpy(esrc[j].stream_str, "||||");
	for(i = 0; i < RSWITCH2_MAX_CASCADE_FILTERS; i++) {
		fwcfc = ioread32(ioaddr + FWCFC0 + (i*0x40));
		if(fwcfc != 0) {
			edpv  = fwcfc & 0xFFFF;
			pdpv = (fwcfc >> 16) & 0xFFFF;
			for(j = 0; j < board_config.eth_ports; j++) {
				if((edpv >> j) & 0x01) {
					getPortNameStringFull(esrc[j].agent_name, &j);
					esrc[j].set_any = 1;
					esrc[j].stream_str[i] = 'S';
				}
				if((pdpv >> j) & 0x01) {
					getPortNameStringFull(psrc[j].agent_name, &j);
					psrc[j].set_any = 1;
					psrc[j].stream_str[i] = 'S';
	
				}
			}
			if((edpv >> j) & 0x01) {
				strcpy(esrc[j].agent_name, "CPU");
				esrc[j].set_any = 1;
				esrc[j].stream_str[i] = 'S';
			}
		}
	}
	for(j = 0; j < board_config.eth_ports; j++) {
		if(esrc[j].set_any) {
			seq_printf(m, "%-5s", esrc[j].agent_name);
			if(strcmp(esrc[j].agent_name, "CPU")  == 0){
				seq_printf(m, "%s", ".  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
			} else {
				seq_printf(m, "%s", "eFrames  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
			}
			seq_printf(m, "%-c", esrc[j].stream_str[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", esrc[j].stream_str[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", esrc[j].stream_str[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", esrc[j].stream_str[3]);
		}
		if(psrc[j].set_any) {
			seq_printf(m, "%-5s", psrc[j].agent_name);
			seq_printf(m, "%s", "pFrames  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
			
			seq_printf(m, "%-c", psrc[j].stream_str[0]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", psrc[j].stream_str[1]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c", psrc[j].stream_str[2]);
			seq_printf(m, "%-s", " ");
			seq_printf(m, "%-c\n", psrc[j].stream_str[3]);
		}
	}
	if(esrc[j].set_any) {
		seq_printf(m, "%-5s", esrc[j].agent_name);
		if(strcmp(esrc[j].agent_name, "CPU")  == 0){
			seq_printf(m, "%s", ".  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
		} else {
			seq_printf(m, "%s", "eFrames  .  .  .  .  .  .  .  .  .  .  .  .  .  ");
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



/**
    @brief Show Two Byte Filter 

    @param  struct seq_file * m 

    @return void
*/
static void rswitch2_fwd_show_two_byte_filter(struct seq_file * m)
{
	char streamid0[5] = {'|','|','|','|'};
	char streamid1[5] = {'|','|','|','|'};
	char comp[6] = {'-','-','-','-','-'};
	u32 print0 = 0;
	u32 print1 = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct rswitch2_fwd_filter_config  *filter;
	filter = &filter_tbl_config;
	for(j = 0; j < filter->num_two_byte_filters; j++) {
		print0  = 0;
		print1 = 0;
		strcpy(streamid0, "||||");
		strcpy(streamid1, "||||");
		strcpy(comp, "-----");
		for(i = 0; i < filter->num_cascade_filters; i++) {
			for(k = 0; k < filter->cascade[i].used_filter_ids; k++) {
				if(filter->two_byte[j].filterid0 == filter->cascade[i].used_filter_id_num[k]) {
					streamid0[i] = '&';
					print0 = 1;
				}
				if(filter->two_byte[j].unit_mode == precise) {
					if(filter->two_byte[j].filterid1 == filter->cascade[i].used_filter_id_num[k]) {
						print1 = 1;
						streamid1[i] = '&';
					}
				}
			}
		}
		if(print0) {
			seq_printf(m, "%-4d", filter->two_byte[j].filterid0);
			comp[0] = '2';
			comp[2] = filter->two_byte[j].filter_number + '0';
			if(filter->two_byte[j].unit_mode == mask) {
				comp[4] = 'M';
			} else if(filter->two_byte[j].unit_mode == expand) {
				comp[4] = 'E';
			} else if(filter->two_byte[j].unit_mode == precise) {
				comp[4] = 'P';
			} 
			seq_printf(m, "%-8s", comp);
			if(filter->two_byte[j].offset_mode == offset) {
				seq_printf(m, "%-6d", filter->two_byte[j].offset);
			} else if(filter->two_byte[j].offset_mode == vlan_tag) {
				if(filter->two_byte[j].offset == 0x0) {
					seq_printf(m, "%-6s", "STAG");
				} else if(filter->two_byte[j].offset == 0x2) {
					seq_printf(m, "%-6s", "CTAG");
				}
				
			}
			if(filter->two_byte[j].unit_mode != expand) {
				seq_printf(m, "0x%04x", filter->two_byte[j].value0);
				seq_printf(m, "%-16s", " ");
			} else {
				seq_printf(m, "0x%04x", filter->two_byte[j].value0);
				seq_printf(m, "%-c", '_');
				seq_printf(m, "0x%04x", filter->two_byte[j].value1);
				seq_printf(m, "%-9s", " ");
			}
			if(filter->two_byte[j].unit_mode == mask) {
				seq_printf(m, "0x%04x", filter->two_byte[j].value1);
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
		if(print1) {
			seq_printf(m, "%-4d", filter->two_byte[j].filterid1);
			comp[0] = '2';
			comp[2] = filter->two_byte[j].filter_number + '0';
			
			comp[4] = 'P';
			
			seq_printf(m, "%-8s", comp);
			if(filter->two_byte[j].offset_mode == offset) {
				seq_printf(m, "%-6d", filter->two_byte[j].offset);
			} else if(filter->two_byte[j].offset_mode == vlan_tag) {
				if(filter->two_byte[j].offset == 0x0) {
					seq_printf(m, "%-6s", "STAG");
				} else if(filter->two_byte[j].offset == 0x2) {
					seq_printf(m, "%-6s", "CTAG");
				}
				
			}
			seq_printf(m, "0x%04x", filter->two_byte[j].value1);
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


/**
    @brief Fill Three Byte Filter structure to be used by Proc show

    @param  void 

    @return void
*/
static void rswitch2_fwd_fill_three_byte_filter(void)
{
	u32 fwthbfc = 0;
	u32 fwthbfv0c = 0;
	u32 fwthbfv1c = 0;
	u32 i  = 0;
	for(i = 0; i < RSWITCH2_MAX_THREE_BYTE_FILTERS; i++) {
		fwthbfc = ioread32(ioaddr + FWTHBFC0 + (i * 0x10));
		fwthbfv0c = ioread32(ioaddr + FWTHBFV0C0 + (i * 0x10));
		fwthbfv1c = ioread32(ioaddr + FWTHBFV1C0 + (i * 0x10));
		if((fwthbfc != 0) || (fwthbfv0c != 0) || (fwthbfv1c != 0)) {
			filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].filter_number = i;
			filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].unit_mode = fwthbfc & 0x3;
			filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].offset = (fwthbfc >> 16) & 0xFF;
			filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].value0 = fwthbfv0c;
			filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].value1 = fwthbfv1c;
			filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].filterid0 =
				2*(filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].filter_number 
					+ RSWITCH2_MAX_TWO_BYTE_FILTERS);
			if(filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].unit_mode == precise)  {
				filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].filterid1 = 
				(2*(filter_tbl_config.three_byte[filter_tbl_config.num_three_byte_filters].filter_number 
					+ RSWITCH2_MAX_TWO_BYTE_FILTERS)) + 1;
			}
			filter_tbl_config.num_three_byte_filters++;
			
			
		}

	}


}


/**
    @brief Fill Four Byte Filter structure to be used by Proc show

    @param  void 

    @return void
*/
static void rswitch2_fwd_fill_four_byte_filter(void)
{
	u32 fwfobfc = 0;
	u32 fwfobfv0c = 0;
	u32 fwfobfv1c = 0;
	u32 i  = 0;
	for(i = 0; i < RSWITCH2_MAX_FOUR_BYTE_FILTERS; i++) {
		fwfobfc = ioread32(ioaddr + FWFOBFC0 + (i * 0x10));
		fwfobfv0c = ioread32(ioaddr + FWFOBFV0C0 + (i * 0x10));
		fwfobfv1c = ioread32(ioaddr + FWFOBFV1C0 + (i * 0x10));
		if((fwfobfc != 0) || (fwfobfv0c != 0) || (fwfobfv1c != 0)) {
			filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].filter_number = i;
			filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].unit_mode = fwfobfc & 0x3;
			filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].offset = (fwfobfc >> 16) & 0xFF;
			filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].value0 = fwfobfv0c;
			filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].value1 = fwfobfv1c;
			filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].filterid0 =
				2*(filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].filter_number 
					+ RSWITCH2_MAX_TWO_BYTE_FILTERS + RSWITCH2_MAX_THREE_BYTE_FILTERS);
			if(filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].unit_mode == precise)  {
				filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].filterid1 = 
				(2*(filter_tbl_config.four_byte[filter_tbl_config.num_four_byte_filters].filter_number 
					+ RSWITCH2_MAX_TWO_BYTE_FILTERS + RSWITCH2_MAX_THREE_BYTE_FILTERS)) + 1;
			}
			filter_tbl_config.num_four_byte_filters++;
			
			
		}

	}


}


/**
    @brief Fill Range Filter structure to be used by Proc show

    @param  void 

    @return void
*/
static void rswitch2_fwd_fill_range_filter(void)
{
	u32 fwrfc = 0;
	u32 fwrfvc = 0;
	u32 i  = 0;
	for(i = 0; i < RSWITCH2_MAX_RANGE_FILTERS; i++) {
		fwrfc = ioread32(ioaddr + FWRFC0 + (i * 0x10));
		fwrfvc = ioread32(ioaddr + FWRFVC0 + (i * 0x10));
		if((fwrfc != 0) || (fwrfvc != 0)) {
			filter_tbl_config.range[filter_tbl_config.num_range_filters].filter_number = i;
			filter_tbl_config.range[filter_tbl_config.num_range_filters].offset_mode = (fwrfc >> 8) & 0x01;
			filter_tbl_config.range[filter_tbl_config.num_range_filters].offset = (fwrfc >> 16) & 0xFF;
			filter_tbl_config.range[filter_tbl_config.num_range_filters].value0 = fwrfvc & 0xFF;
			filter_tbl_config.range[filter_tbl_config.num_range_filters].value1 = (fwrfvc >> 8) & 0xFF;
			filter_tbl_config.range[filter_tbl_config.num_range_filters].range = (fwrfvc >> 16) & 0xF;
			filter_tbl_config.range[filter_tbl_config.num_range_filters].filterid0 = 
				2*(filter_tbl_config.range[filter_tbl_config.num_range_filters].filter_number 
					+ RSWITCH2_MAX_TWO_BYTE_FILTERS + RSWITCH2_MAX_THREE_BYTE_FILTERS + RSWITCH2_MAX_FOUR_BYTE_FILTERS);
			
			filter_tbl_config.range[filter_tbl_config.num_range_filters].filterid1 = 
				(2*(filter_tbl_config.range[filter_tbl_config.num_range_filters].filter_number + 
				RSWITCH2_MAX_TWO_BYTE_FILTERS + RSWITCH2_MAX_THREE_BYTE_FILTERS + RSWITCH2_MAX_FOUR_BYTE_FILTERS)) + 1;
			filter_tbl_config.num_range_filters++;
			
			
		}

	}


}


/**
    @brief Fill Two Byte Filter structure to be used by Proc show

    @param  void 

    @return void
*/
static void rswitch2_fwd_fill_two_byte_filter(void)
{
	u32 fwtwbfc = 0;
	u32 fwtwbfvc = 0;
	u32 i  = 0;
	for(i = 0; i < RSWITCH2_MAX_TWO_BYTE_FILTERS; i++) {
		fwtwbfc = ioread32(ioaddr + FWTWBFC0 + (i * 0x10));
		fwtwbfvc = ioread32(ioaddr + FWTWBFVC0 + (i * 0x10));
		if((fwtwbfc != 0) || (fwtwbfvc != 0)) {
			filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].filter_number = i;
			filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].unit_mode = fwtwbfc & 0x3;
			filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].offset_mode = (fwtwbfc >> 8) & 0x01;
			filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].offset = (fwtwbfc >> 16) & 0xFF;
			filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].value0 = fwtwbfvc & 0xFFFF;
			filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].value1 = (fwtwbfvc >> 16) & 0xFFFF;
			filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].filterid0 = 
						2*filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].filter_number;
			if(filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].unit_mode == precise)  {
				filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].filterid1 = 
						(2*filter_tbl_config.two_byte[filter_tbl_config.num_two_byte_filters].filter_number) + 1;
			}
			filter_tbl_config.num_two_byte_filters++;
			
			
		}

	}


}


/**
    @brief Fill Cascade Filter structure to be used by Proc show

    @param  void 

    @return void
*/
static void rswitch2_fwd_fill_cascade_filter(void)
{
	u32 fwcfc = 0;
	u32 fwcfmc = 0;
	u32 i  = 0;
	u32 j  = 0;
	for(i = 0; i < RSWITCH2_MAX_CASCADE_FILTERS; i++) {
		fwcfc = ioread32(ioaddr + FWCFC0 + (i * 0x40));
		if((fwcfc != 0) ) {
			filter_tbl_config.cascade[filter_tbl_config.num_cascade_filters].filter_number = i;
			for(j = 0; j < RSWITCH2_MAX_USED_CASCADE_FILTERS; j++) {
				fwcfmc = ioread32(ioaddr + (FWCFMC00 + (i * 0x40) + (j * 4)));
				if(fwcfmc != 0) {
					filter_tbl_config.cascade[filter_tbl_config.
					num_cascade_filters].used_filter_id_num[j] = fwcfmc & 0xFF;
					filter_tbl_config.cascade[filter_tbl_config.
					num_cascade_filters].used_filter_ids++;
				}
				
			}
			filter_tbl_config.num_cascade_filters++;
			
			
		}

	}


}

/**
    @brief Filters Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_fwd_filters_show(struct seq_file * m, void * v)
{

	seq_printf(m, "=========== Filter-Table =======================================\n");
	seq_printf(m, "ID  Comp    Offs  Value                 Mask         Stream ID\n");
	seq_printf(m, "==================================================== 0 1 2 3 ===\n");
	memset(&filter_tbl_config, 0 , sizeof(struct rswitch2_fwd_filter_config));
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



/**
    @brief Open L3 Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_l3_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_l3_show, NULL);
}


/**
    @brief Open L2 Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_l2_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_l2_show, NULL);
}


/**
    @brief Open L2 Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_vlan_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_vlan_show, NULL);
}


/**
    @brief Open All Error Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_errors_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_errors_all_show, NULL);
}

/**
    @brief Open Error Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_errors_short_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_errors_short_show, NULL);
}



/**
    @brief Open Counters Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_counters_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_counters_show, NULL);
}


/**
    @brief Open L2 L3 Update Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_l2_l3_update_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_l2_l3_update_show, NULL);
}


/**
    @brief Open buffer status Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_buffer_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_buffer_show, NULL);
}


/**
    @brief Open Filters status Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_fwd_filters_open(struct inode * inode, struct  file * file)
{
	return single_open(file, rswitch2_fwd_filters_show, NULL);
}


/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_l3_fops = {
	.owner   = THIS_MODULE,
	.write   = rswitch2_fwd_l3_clear,
	.open    = rswitch2_fwd_l3_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_errors_short_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_fwd_errors_short_open,
	.write   = rswitch2_fwd_errors_clear,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};



/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_errors_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_fwd_errors_open,
	.write   = rswitch2_fwd_errors_clear,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_counters_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_fwd_counters_open,
	.write   = rswitch2_fwd_counters_clear,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_l2_l3_update_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_fwd_l2_l3_update_open,
	.write   = rswitch2_fwd_l2_l3_update_clear,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_l2_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_fwd_l2_open,
	.write   = rswitch2_fwd_l2_clear,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_vlan_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_fwd_vlan_open,
	.write   = rswitch2_fwd_l2_vlan_clear,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_buffer_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_fwd_buffer_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


/**
    @brief  Proc File Operation
*/
static const struct file_operations     rswitch2_fwd_filters_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_fwd_filters_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


/**
    @brief  Create Proc Directories

    @param  void

    @return void
*/
static void rswitch2_fwd_create_proc_entry(void)
{
	/*
	    create root & sub-directories
	*/
	proc_create(RSWITCH2_FWD_PROC_FILE_ERRORS, 0, root_dir, &rswitch2_fwd_errors_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_ERRORS_SHORT, 0, root_dir, &rswitch2_fwd_errors_short_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_COUNTERS, 0, root_dir, &rswitch2_fwd_counters_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_L3, 0, root_dir, &rswitch2_fwd_l3_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_L2_L3_UPDATE, 0, root_dir, &rswitch2_fwd_l2_l3_update_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_L2, 0, root_dir, &rswitch2_fwd_l2_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_VLAN, 0, root_dir, &rswitch2_fwd_vlan_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_BUFFER, 0, root_dir, &rswitch2_fwd_buffer_fops);
	proc_create(RSWITCH2_FWD_PROC_FILE_FILTERS, 0, root_dir, &rswitch2_fwd_filters_fops);
}


/**
    @brief  Remove Proc Directories

    @param  void

    @return void
*/
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
}


/**
    @brief  Forwarding engine init

    @return int
*/
int rswitch2_fwd_init(struct ethports_config      *board_info)
{
	memcpy(&board_config, board_info,  sizeof(board_config));
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

/**
    @brief  Forwarding engine exit

    @return int
*/
int rswitch2_fwd_exit(void)
{
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
	rswitch2_fwd_remove_proc_entry();
#endif
	rswitch2fwd_drv_remove_chrdev();


	return 0;
}
