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
#define RSWITCH2_FWD_PROC_FILE_ERRORS   "fwderrors"
#define RSWITCH2_FWD_PROC_FILE_COUNTERS "fwdcounters"
#define RSWITCH2_FWD_PROC_FILE_L3       "L3"
#define RSWITCH2_FWD_PROC_FILE_L2       "L2"
#define RSWITCH2_FWD_PROC_FILE_L2_L3_UPDATE "Routtblupdate"
#endif
/********************************************* Global Variables ************************************************/
static struct rswitch2_fwd_config             rswitch2fwd_config;
static struct  ethports_config      board_config;
// Static config (passed from user)
static struct rswitch2_fwd_config             rswitch2fwd_confignew;

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
    printk("Register address= %x Write value = %x \n",FWMACTIM, (1 << 0));
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
    printk("Register address= %x Write value = %x \n",FWIPTIM, (1 << 0));
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
    printk("Register address= %x Write value = %x \n",FWVLANTIM, (1 << 0));
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
    @brief  L2-3 Update Table Reset Function

    @return int

*/
static int rswitch2_l23_update_tbl_reset(void)
{
    int i = 0;
    u32 value = 0;
    iowrite32((1 << 0), ioaddr + FWL23UTIM);
    printk("Register address= %x Write value = %x \n",FWL23UTIM, (1 << 0));
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
    @brief  L3 Table Reset Function

    @return int

*/
static int rswitch2_l3_tbl_reset(void)
{
    int i = 0;
    u32 value = 0;
    iowrite32((1 << 0), ioaddr + FWLTHTIM);
    printk("Register address= %x Write value = %x \n",FWLTHTIM, (1 << 0));
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
            if(((value >> 2) & 0x01) || ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
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
            if(((value >> 2) & 0x01) || ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
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
            if(((value >> 2) & 0x01) || ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
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
            if(((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
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
            if(((value >> 2) & 0x01) || ((value >> 1) & 0x01) || ((value >> 0) & 0x01) ) {
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
    if((config->stag_dei) || (config->stag_pcp) || (config->stag_vlan)) {
        stream_id.stag =  (entry->stag_pcp << 13) |  (entry->stag_dei << 12)
                          | (entry->stag_vlan );
        ;


    } else {
        stream_id.stag = 0;

    }
    if((config->ctag_dei) || (config->ctag_pcp) || (config->ctag_vlan)) {
        stream_id.ctag =  (entry->ctag_pcp << 13) |  (entry->ctag_dei << 12)
                          | (entry->ctag_vlan );

    } else {
        stream_id.ctag = 0;

    }

    if(config->destination_port) {
        stream_id.destination_port =  entry->destination_port;



    } else {
        stream_id.destination_port = 0;

    }
    stream_id.hash_value =  hash_value;
    if(config->source_ip) {
        stream_id.source_ip =  (entry->source_ip_addr[0] << 24) | (entry->source_ip_addr[1] << 16) | (entry->source_ip_addr[2] << 8) | (entry->source_ip_addr[3] << 0);



    } else {
        stream_id.source_ip = 0;


    }
    if(config->destination_ip) {
        stream_id.destination_ip =  (entry->destination_ip_addr[0] << 24) | (entry->destination_ip_addr[1] << 16) |
                                    (entry->destination_ip_addr[2] << 8) | (entry->destination_ip_addr[3] << 0);


    } else {
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
    printk("Register address= %x Write value = %x \n",FWIP4SC, fwip4sc_val);

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




    if(config->dest_mac) {
        iowrite32(((entry->dest_mac[3]<<24) | (entry->dest_mac[2]<<16) | (entry->dest_mac[1]<<8) | (entry->dest_mac[0]<<0)),ioaddr +  FWSHCR0);
        printk("Register address= %x Write value = %x \n",FWSHCR0,
               ((entry->dest_mac[3]<<24) | (entry->dest_mac[2]<<16) | (entry->dest_mac[1]<<8) | (entry->dest_mac[0]<<0)));
        fwshcr1_val = (entry->dest_mac[5]<<24) | (entry->dest_mac[4]<<16);
        if(!config->src_mac) {
            iowrite32(fwshcr1_val, ioaddr + FWSHCR1);
            printk("Register address= %x Write value = %x \n",FWSHCR1,
                   fwshcr1_val);
        }
    }
    if(config->src_mac) {
        fwshcr1_val |= (entry->src_mac[1]<<8) | (entry->src_mac[0]<<0);
        iowrite32(fwshcr1_val, ioaddr + FWSHCR1);
        printk("Register address= %x Write value = %x \n",FWSHCR1,
               fwshcr1_val);
        fwshcr2_val = (entry->src_mac[5]<<24) | (entry->src_mac[4]<<16) | (entry->src_mac[3]<<8) | (entry->src_mac[2]<<0);
        iowrite32(fwshcr2_val, ioaddr + FWSHCR2);
        printk("Register address= %x Write value = %x \n",FWSHCR2,
               fwshcr2_val);


    }

    if((config->ctag_dei) || (config->stag_dei) || (config->ctag_pcp)
       || (config->stag_pcp)  || (config->ctag_vlan) || (config->stag_vlan)) {
        fwshcr3_val =  (entry->stag_pcp << 29) |  (entry->stag_dei << 28)
                       | (entry->stag_vlan << 16) | (entry->ctag_pcp << 13)
                       | (entry->ctag_dei << 12)  | (entry->ctag_vlan);
        iowrite32(fwshcr3_val, ioaddr + FWSHCR3);
        printk("Register address= %x Write value = %x \n",FWSHCR3,
               fwshcr3_val);

    }
    iowrite32(0x0, ioaddr + FWSHCR4);

    if(config->source_ip) {
        src_ip = (entry->source_ip_addr[0] << 24) | (entry->source_ip_addr[1] << 16) |
                 (entry->source_ip_addr[2] << 8) | (entry->source_ip_addr[3] << 0);
        iowrite32(src_ip, ioaddr + FWSHCR5);

    }
    if(config->destination_ip) {
        dest_ip = (entry->destination_ip_addr[0] << 24) | (entry->destination_ip_addr[1] << 16) |
                  (entry->destination_ip_addr[2] << 8) | (entry->destination_ip_addr[3] << 0);
        iowrite32(dest_ip, ioaddr + FWSHCR9);


    }
    if((config->destination_port) || (config->source_port)) {
        fwshcr13_val = entry->destination_port | (entry->source_port << 16);
        iowrite32(fwshcr13_val, ioaddr + FWSHCR13);
    } else {
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
    struct rswitch2_ipv4_stream_id *stream_id;
    stream_id = kmalloc(sizeof(struct rswitch2_ipv4_stream_id), GFP_KERNEL);


    for(entries = 0; entries < config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entries; entries++) {
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
        for(port_lock = 0; port_lock < config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].source_lock.source_ports; port_lock++) {
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
        for(dest_ports = 0;
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
        if(ret < 0) {
            printk("L3 Learning Failed for entry num= %d\n", entries);
        }


    }
    kfree(stream_id);
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

    if(config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.bEnable) {
        for(entries = 0; entries < config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entries; entries++) {
            config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].hash_value =
                rswitch2_calc_ipv4_hash(&config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_hash_configuration,
                                        &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries]);

            config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].stream_id =
                rswitch2_gen_ipv4_stream(config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].hash_value,
                                         &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries],
                                         &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config);
        }
        rswitch2_ipv4_stream_gen_config(&config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config,
                                        &config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_hash_configuration);
        ret = rswitch2_l3_tbl_reset();
        if(ret < 0) {
            printk("l3 tbl Reset Failed \n");
            return -1;
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
    for(entries = 0; entries < config->l2_fwd_vlan_config_entries; entries++) {
        fwvlantl0_val = (config->l2_fwd_vlan_config_entry[entries].vlan_learn_disable << 10)
                        | (config->l2_fwd_vlan_config_entry[entries].vlan_security_learn << 8);

        iowrite32(fwvlantl0_val, ioaddr + FWVLANTL0);
        fwvlantl1_val = (config->l2_fwd_vlan_config_entry[entries].vlan_id);
        iowrite32(fwvlantl1_val, ioaddr + FWVLANTL1);
        for(port_lock = 0; port_lock < config->l2_fwd_vlan_config_entry[entries].source_lock.source_ports; port_lock++) {
            ds_dpv |= 1 << config->l2_fwd_vlan_config_entry[entries].source_lock.source_port_number[port_lock];

        }
        ds_dpv  |=  config->l2_fwd_vlan_config_entry[entries].source_lock.cpu << board_config.eth_ports;
#ifdef DEBUG
        printk("ds_dpv = %x \n", ds_dpv);
#endif
        iowrite32(ds_dpv, ioaddr + FWVLANTL2);
        iowrite32(config->l2_fwd_vlan_config_entry[entries].csdn, ioaddr+FWVLANTL3);

        for(dest_ports = 0; dest_ports < config->l2_fwd_vlan_config_entry[entries].destination_vector_config.dest_eth_ports; dest_ports++) {
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
        if(ret < 0) {
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
    for(entries = 0; entries < config->l2_fwd_mac_config_entries; entries++) {

        ss_dpv = 0;
        dpv = 0;
        fwmactl0_val = (config->l2_fwd_mac_config_entry[entries].mac_learn_disable << 10) | (config->l2_fwd_mac_config_entry[entries].mac_dynamic_learn << 9)
                       | (config->l2_fwd_mac_config_entry[entries].mac_security_learn << 8);
        iowrite32(fwmactl0_val, ioaddr + FWMACTL0);
        fwmactl1_val = (config->l2_fwd_mac_config_entry[entries].mac[0]<< 8) | (config->l2_fwd_mac_config_entry[entries].mac[1]<< 0);
        iowrite32(fwmactl1_val, ioaddr + FWMACTL1);
        printk("FWMACTL1_val = %x \n", fwmactl1_val);
        fwmactl2_val = (config->l2_fwd_mac_config_entry[entries].mac[2]<< 24) | (config->l2_fwd_mac_config_entry[entries].mac[3]<< 16)
                       | (config->l2_fwd_mac_config_entry[entries].mac[4]<< 8) | (config->l2_fwd_mac_config_entry[entries].mac[5]<< 0);
        printk("fwmactl2_val = %x \n", fwmactl2_val);
        iowrite32(fwmactl2_val, ioaddr + FWMACTL2);
        for(port_lock = 0; port_lock < config->l2_fwd_mac_config_entry[entries].destination_source.source_ports; port_lock++) {
            ds_dpv |= 1 << config->l2_fwd_mac_config_entry[entries].destination_source.source_port_number[port_lock];

        }
        ds_dpv  |=  config->l2_fwd_mac_config_entry[entries].destination_source.cpu << board_config.eth_ports;
        printk("ss_dpv = %x \n", ss_dpv);
        for(port_lock = 0; port_lock < config->l2_fwd_mac_config_entry[entries].source_source.source_ports; port_lock++) {
            ss_dpv |= 1 << config->l2_fwd_mac_config_entry[entries].source_source.source_port_number[port_lock];

        }
        ss_dpv  |=  config->l2_fwd_mac_config_entry[entries].source_source.cpu << board_config.eth_ports;
        printk("ss_dpv = %x \n", ss_dpv);
        fwmactl3_val = ss_dpv | (ds_dpv << 16);
        iowrite32(fwmactl3_val, ioaddr + FWMACTL3);
        iowrite32(config->l2_fwd_mac_config_entry[entries].csdn, ioaddr + FWMACTL4);
        for(dest_ports = 0; dest_ports < config->l2_fwd_mac_config_entry[entries].destination_vector_config.dest_eth_ports; dest_ports++) {
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
        if(ret < 0) {
            printk("mac Learning Failed for entry num= %d\n", entries);
        }

    }
    return 0;

}
#endif

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
        ret = rswitch2_l2_mac_tbl_reset();
        if(ret < 0) {
            printk("l2 MAC tbl Reset Failed \n");
            return -1;
        }
        iowrite32(((config->max_unsecure_hash_entry << 16) |config->max_hash_collision), ioaddr + FWMACHEC);
        iowrite32(config->mac_hash_eqn, ioaddr + FWMACHC);
        ret = rswitch2_l2_mac_tbl_config(config);
        if(ret < 0) {
            printk("l2 MAC tbl Config Failed \n");
            return -1;
        }
    }
    if(config->l2_fwd_vlan_config_entries)
    {
        ret = rswitch2_l2_vlan_tbl_reset();
        if(ret < 0) {
            printk("l2 VLAN tbl Reset Failed \n");
            return -1;
        }
        iowrite32(((config->max_unsecure_vlan_entry << 16)), ioaddr + FWVLANTEC);
        ret = rswitch2_l2_vlan_tbl_config(config);
        if(ret < 0) {
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
    ret = rswitch2_l23_update_tbl_reset();
    if(ret < 0) {
        printk("l23 Update tbl Reset Failed \n");
        return -1;
    }
    for(entries = 0; entries < config->entries; entries++) {
        for(dest_ports = 0; dest_ports < config->l23_update_config[entries].routing_port_update.dest_eth_ports; dest_ports++) {
            dpv |= 1 << config->l23_update_config[entries].routing_port_update.port_number[dest_ports];
        }
        //printk("config->l23_update_config[entries].src_mac_update = %x \n", config->l23_update_config[entries].src_mac_update);
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
        //printk("fwl23url1=%x\n",fwl23url1);
        iowrite32(fwl23url1, ioaddr + FWL23URL1);
        iowrite32(fwl23url2, ioaddr + FWL23URL2);
        iowrite32(fwl23url3, ioaddr + FWL23URL3);
        ret = rswitch2_l23_update_learn_status();
        if(ret < 0) {
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
    for(entries = 0; entries < config->ipv_fwd_config_entries; entries++) {
        ds_dpv = 0;
        ss_dpv  = 0;
        dpv = 0;
        fwiptl0_val = (config->ipv_fwd_config_entry[entries].ipv_learn_disable << 10) | (config->ipv_fwd_config_entry[entries].ipv_dynamic_learn << 9)
                      | (config->ipv_fwd_config_entry[entries].ipv_security_learn << 8) | (config->ipv_fwd_config_entry[entries].ip_addr_type);
        iowrite32(fwiptl0_val, ioaddr + FWIPTL0);
        if(config->ipv_fwd_config_entry[entries].ip_addr_type == ip_addr_ipv4) {
            fwiptl1_val = (config->ipv_fwd_config_entry[entries].ipaddr[0] << 24) | (config->ipv_fwd_config_entry[entries].ipaddr[1] << 16) |
                          (config->ipv_fwd_config_entry[entries].ipaddr[2] << 8) | (config->ipv_fwd_config_entry[entries].ipaddr[3] << 0);
            iowrite32(fwiptl1_val, ioaddr + FWIPTL1);
        }

        else {

            /* ipv6 Suppport to be provided */

        }


        for(port_lock = 0; port_lock < config->ipv_fwd_config_entry[entries].destination_source.source_ports; port_lock++) {
            ds_dpv |= 1 << config->ipv_fwd_config_entry[entries].destination_source.source_port_number[port_lock];

        }
        ds_dpv  |=  config->ipv_fwd_config_entry[entries].destination_source.cpu << board_config.eth_ports;
        printk("ss_dpv = %x \n", ss_dpv);

        for(port_lock = 0; port_lock < config->ipv_fwd_config_entry[entries].source_source.source_ports; port_lock++) {
            ss_dpv |= 1 << config->ipv_fwd_config_entry[entries].source_source.source_port_number[port_lock];

        }
        ss_dpv  |=  config->ipv_fwd_config_entry[entries].source_source.cpu << board_config.eth_ports;
        printk("ss_dpv = %x \n", ss_dpv);
        fwiptl5_val = config->ipv_fwd_config_entry[entries].routing_number | (ds_dpv << 16) | (1 << 15);
        iowrite32(fwiptl5_val, ioaddr + FWIPTL5);
        iowrite32(ss_dpv, ioaddr + FWIPTL6);
        for(dest_ports = 0; dest_ports < config->ipv_fwd_config_entry[entries].destination_vector_config.dest_eth_ports; dest_ports++) {
            dpv |= 1 << config->ipv_fwd_config_entry[entries].destination_vector_config.port_number[dest_ports];
        }
        dpv |= config->ipv_fwd_config_entry[entries].destination_vector_config.cpu << board_config.eth_ports;
        if(config->ipv_fwd_config_entry[entries].destination_vector_config.cpu) {
            iowrite32(config->ipv_fwd_config_entry[entries].csdn, ioaddr + FWIPTL7);


        }
        fwiptl8_val = config->ipv_fwd_config_entry[entries].mirroring_config.cpu_mirror_enable << 21
                      | config->ipv_fwd_config_entry[entries].mirroring_config.eth_mirror_enable << 20
                      | config->ipv_fwd_config_entry[entries].mirroring_config.ipv_config.ipv_update_enable  << 19
                      | config->ipv_fwd_config_entry[entries].mirroring_config.ipv_config.ipv_value  << 16
                      | dpv;
        iowrite32(fwiptl8_val, ioaddr + FWIPTL8);
        ret = rswitch2_ipv_learn_status();
        if(ret < 0) {
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
    for(i = 0; i < board_config.eth_ports; i++) {
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
    if(ret < 0) {
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
    for(dest_ports = 0; dest_ports < config->destination_vector_config.dest_eth_ports; dest_ports++) {
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
    if(config->vlan_mode == 0x1) {
        iowrite32(config->vlan_mode, (ioaddr + FWGC));
        iowrite32(config->tpid, (ioaddr + FWTTC0));
    } else if(config->vlan_mode == 0x2) {
        iowrite32(config->vlan_mode, (ioaddr + FWGC));
        iowrite32((config->tpid << 16), (ioaddr + FWTTC0));
    }
    for(i = 0; i < board_config.agents; i++) {
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
        if(config->fwd_gen_port_config[i].cpu) {
            iowrite32(fwpc0, (ioaddr + FWPC00 + (board_config.eth_ports * 0x10)));
                if(config->fwd_gen_port_config[i].port_forwarding.bEnable) {
                    rswitch2_port_fwding_config(&config->fwd_gen_port_config[i].port_forwarding, board_config.eth_ports);
                }
        } else {
            iowrite32(fwpc0, (ioaddr + FWPC00 + (config->fwd_gen_port_config[i].portnumber * 0x10)));
            if(config->fwd_gen_port_config[i].port_forwarding.bEnable) {
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
    if(rswitch2fwd_config.fwd_gen_config.bEnable) {
#ifdef DEBUG
        printk("gen config benable \n");
#endif
        /*Call L3 configuration Function */
        rswitch2_fwd_gen_config(&rswitch2fwd_config.fwd_gen_config);

    }
    if(rswitch2fwd_config.l3_stream_fwd.bEnable) {
        /*Call L3 configuration Function */
        rswitch2_l3_tbl_config(&rswitch2fwd_config.l3_stream_fwd);

    }
    if(rswitch2fwd_config.l23_update.entries) {
        /*Call L3 configuration Function */
        rswitch2_l23_table_update_config(&rswitch2fwd_config.l23_update);

    }
    if(rswitch2fwd_config.l2_fwd_config.bEnable) {
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
    err = access_ok(VERIFY_WRITE, buf , sizeof(rswitch2fwd_confignew));

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
    } else {
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


    for(entries = 0; entries < config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entries; entries++) {
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
        for(port_lock = 0; port_lock < config->ipv4_ipv6_stream_config.ipv4_stream_fwd_config.ipv4_stream_fwd_entry[entries].source_lock.source_ports; port_lock++) {
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
        for(dest_ports = 0;
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
        if(ret < 0) {
            printk("L3 Learning Failed for entry num= %d\n", entries);
        }

#endif


#ifdef CONFIG_RENESAS_RSWITCH2_STATS
/**
    @brief L2 L3 Update Show Proc Function

    @param  seq_file *

    @param  void *

    @return int

*/
static int rswitch2_fwd_l2_l3_update_show(struct seq_file * m, void * v)
{
    u32 c = 0;
    u32 i = 0;
    u32 entry = 0;
    u32 value = 0;
    u32 fwl23urrr1 = 0;
    u32 fwl23urrr2 = 0;
    u32 fwl23urrr3 = 0;
    u8  rtag = 0;
    u8  first_print = 0;
    u8 mac[6] = {0,0,0,0,0,0,};
    u32 dpv = 0;
    seq_printf(m, "Line     RTAG    STAG-DEI     STAG-PCP      STAG-ID       CTAG-DEI     CTAG-PCP      CTAG-ID  \n");
    
    for(entry = 0; entry <= 255; entry++){
        iowrite32(entry, ioaddr + FWL23URR);
        for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
            value = ioread32(ioaddr + FWL23URRR0);
            if (((value >> 31) & 0x01) == 0x0) {
                if((((value >> 16) & 0x01) == 0) && (value & 0xFFFF)) {
                    dpv = value & 0xFF;
                    fwl23urrr1 = ioread32(ioaddr + FWL23URRR1);
                    //printk("fwl23urrr1 = %x \n",fwl23urrr1);
                    fwl23urrr2 = ioread32(ioaddr + FWL23URRR2);
                    fwl23urrr3 = ioread32(ioaddr + FWL23URRR3); 
                    rtag = (fwl23urrr1 >> 25) & 0x03;
                    seq_printf(m, "%4d   %6d", entry, rtag); 
                    if((fwl23urrr1 >> 24) & 0x01) {
                        seq_printf(m, "%12d", (fwl23urrr3>>31) & 0x01);
                    } else {
                        seq_printf(m, "%12s", "-");
                    }
                    if((fwl23urrr1 >> 23) & 0x01) {
                        seq_printf(m, "%13d", (fwl23urrr3>>28) & 0x07);
                    } else {
                        seq_printf(m, "%13s", "-");
                    }
                    if((fwl23urrr1 >> 22) & 0x01) {
                        seq_printf(m, "%13d", (fwl23urrr3>>16) & 0xFFF);
                    } else {
                        seq_printf(m, "%13s", "-");
                    }
                    if((fwl23urrr1 >> 21) & 0x01) {
                        seq_printf(m, "%15d", (fwl23urrr3>>15) & 0x01);
                    } else {
                        seq_printf(m, "%15s", "-");
                    }
                    if((fwl23urrr1 >> 20) & 0x01) {
                        seq_printf(m, "%13d", (fwl23urrr3>>12) & 0x07);
                    } else {
                        seq_printf(m, "%13s", "-");
                    }
                    if((fwl23urrr1 >> 19) & 0x01) {
                        seq_printf(m, "%12d\n", (fwl23urrr3>>0) & 0xFFF);
                    } else {
                        seq_printf(m, "%12s", "-\n");
                    }
                    
                    if(!first_print) {
                        seq_printf(m, "Src-Addr-Update      TTL-Update    Dest-MAC           Routing-Ports       Routing-CPU\n");
                        first_print = 1;
                    }
                    if((fwl23urrr1 >> 18) & 0x01) {
                        seq_printf(m, "%-3s", "Yes");
                    } else {
                        seq_printf(m, "%-3s", "NO");
                    }
                    if((fwl23urrr1 >> 16) & 0x01) {
                        seq_printf(m, "%20s", "Yes");
                    } else {
                        seq_printf(m, "%20s", "NO");
                    }
                    if((fwl23urrr1 >> 17) & 0x01) {
                        mac[0] = (fwl23urrr1 >> 8) & 0xFF;
                        mac[1] = (fwl23urrr1 >> 0) & 0xFF;
                        mac[2] = (fwl23urrr2 >> 24) & 0xFF;
                        mac[3] = (fwl23urrr2 >> 16) & 0xFF;
                        mac[4] = (fwl23urrr2 >> 8) & 0xFF;
                        mac[5] = (fwl23urrr2 >> 0) & 0xFF;
                        seq_printf(m, "%14x:%2x:%2x:%2x:%2x:%2x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
                    } else {
                        seq_printf(m, "%14s:%2s:%2s:%2s:%2s:%2s", "--","--", "--","--", "--","--");
                    }
                    for (c = 0; c < board_config.eth_ports; c++) {
                        if(((dpv >> c) & 0x01) == 1) {
                            seq_printf(m, "%3d ", c);
                        } else {
                            seq_printf(m, "  ");
                        }
                    }
                    if((dpv >> board_config.eth_ports) == 0x01) {
                        seq_printf(m, "  %15s    ","Yes");
                    } else {
                        seq_printf(m, "  %15s    ","No");
                    }
                    seq_printf(m, "\n");
                    seq_printf(m, "-----------------------------------------------------------------------------------------\n");
                }
                break;
            }
            mdelay(1);
        }
           
    }
    return 0;
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
    u8 first_print = 0;
    u8 aeging = 0;
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
    u8 c = 0;
    u32 fwvlantsr1 = 0;
    u32 fwvlantsr3 = 0;
    seq_printf(m, "======================================MAC-TABLE==========================================\n");
    seq_printf(m, "Line        MAC         Mode     Targets         Mirror   Src-Lock      Dst-Lock     IPV\n");
    seq_printf(m, "=========================================================================================\n");
    for(entry = 0; entry <= 1023; entry++){
        iowrite32(entry, ioaddr + FWMACTR);
        for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
            value = ioread32(ioaddr + FWMACTRR0);
            if (((value >> 31) & 0x01) == 0x0) {
                if((((value >> 2) & 0x01) == 0) && (((value >> 1) & 0x01) == 0x00) && (((value >> 0) & 0x01) == 0x1) ) {
                    fwmactrr1 = ioread32(ioaddr + FWMACTRR1);
                    fwmactrr2 = ioread32(ioaddr + FWMACTRR2);
                    fwmactrr3 = ioread32(ioaddr + FWMACTRR3);
                    fwmactrr4 = ioread32(ioaddr + FWMACTRR4);
                    csdn = ioread32(ioaddr + FWMACTRR5);
                    fwmactrr6 = ioread32(ioaddr + FWMACTRR6);
                    aeging = (fwmactrr1 >> 11) & 0x01;
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
                    seq_printf(m,"  %-7s", dynamic ? ((aeging) ? "Aged" : "Dynamic") : "Static");
                    
                    seq_printf(m, " ");
                    for (c = 0; c < board_config.eth_ports; c++) {
                        if(((dpv >> c) & 0x01) == 1) {
                            seq_printf(m, " %d", c);
                        } else {
                            seq_printf(m, "  ");
                        }
                    }
                    if (((dpv >> board_config.eth_ports) & 0x01) == 0x01) {
                        seq_printf(m, " CPU:%-3d", csdn);
                    } else {
                        seq_printf(m, "        ");
                    }

                    seq_printf(m," %-3s", eth_mirror ? "Eth" : "No");
                    seq_printf(m," %-3s", cpu_mirror ? "CPU" : "No");
                    
                    seq_printf(m, " ");
                    for (c = 0; c < board_config.eth_ports; c++) {
                        if(((srclockdpv >> c) & 0x01) == 1) {
                            seq_printf(m, " %d", c);
                        } else {
                            seq_printf(m, "  ");
                        }
                    }
                    if(((srclockdpv >> board_config.eth_ports) & 0x01) == 0x01) {
                        seq_printf(m, " CPU");
                    }else {
                        seq_printf(m, "    ");
                    }

                    seq_printf(m, "  ");
                    for (c = 0; c < board_config.eth_ports; c++) {
                        if(((destlockdpv >> c) & 0x01) == 1) {
                            seq_printf(m, " %d", c);
                        } else {
                            seq_printf(m, "  ");
                        }
                    }
                    if(((destlockdpv >> board_config.eth_ports) & 0x01) == 0x01) {
                        seq_printf(m, " %s", "CPU");
                    }else {
                        seq_printf(m, "    ");
                    }

                    if(ipv_enable) {
                        seq_printf(m, "  %d ", ipv_value);
                    } else {
                        seq_printf(m, "  -");
                    }

                    //seq_printf(m,"%12s", security ? "Yes" : "No");
                    //seq_printf(m,"%17s", learn_disable ? "Yes" : "No");

                    seq_printf(m, "\n");
                }
                break;
            }
            mdelay(1);
        }
        
    }
    first_print = 0;
    seq_printf(m, "\n======================================VLAN-TABLE====================================================================\n");
    seq_printf(m, "VID    Learn-Disable    Security    Src-Lock        Src-Lock-CPU    Eth-Mirror    CPU-Mirror    IPV-Update \n");
    seq_printf(m, "===================================================================================================================\n");
    for(entry = 0; entry <= 0xFFF; entry++){
        iowrite32(entry, ioaddr + FWVLANTS);
        for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
            value = ioread32(ioaddr + FWVLANTSR0);
            if (((value >> 31) & 0x01) == 0x0) {
                if((((value >> 1) & 0x01) == 0x00) && (((value >> 0) & 0x01) == 0x0) ) {
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
                    seq_printf(m,"%-4d", entry);
                    seq_printf(m,"%16s", learn_disable ? "Yes" : "No");
                    seq_printf(m,"%12s", security ? "Yes" : "No");
                    seq_printf(m, "%s", " ");
                    for (c = 0; c < board_config.eth_ports; c++) {
                        if(((srclockdpv >> c) & 0x01) == 1) {
                            seq_printf(m, "%4d", c);
                        } else {
                            seq_printf(m, "  ");
                        }
                    }
                    if(((srclockdpv >> board_config.eth_ports) & 0x01) == 0x01) {
                        seq_printf(m, " %22s  ","Yes");
                    }else {
                        seq_printf(m, " %22s  ","No");
                    }
                
                    seq_printf(m,"%12s", eth_mirror ? "Yes" : "No");
                    seq_printf(m,"%14s", cpu_mirror ? "Yes" : "No");
                    if(ipv_enable) {
                        seq_printf(m, "%15d\n", ipv_value);
                    } else {
                        seq_printf(m, "%15s", "No\n");
                    }
                    if(!first_print) {
                        seq_printf(m, "Dest-Ports       Dest-CPU \n");
                    }
                    //printk("dpv=%x \n", dpv);
                    for (c = 0; c < board_config.eth_ports; c++) {
                        if(((dpv >> c) & 0x01) == 1) {
                            seq_printf(m, "%-4d ", c);
                        } else {
                            seq_printf(m, "  ");
                        }
                    }
                    if((dpv >> board_config.eth_ports) == 0x01) {
                        seq_printf(m, "  %15d    ",csdn);
                    }
                    else {
                        seq_printf(m, " %15s  ","No");
                    }
                    
                    seq_printf(m, "\n");
                    seq_printf(m, "===================================================================================================================\n");
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
    char frame_fmt_code[30]; 
    u8 first_print = 0;
    char security[5];
    u32 srcipaddr[4];
    u32 destipaddr[4];
    u32 c = 0;
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
    u32 ctag = 0;
    u32 stag = 0;
    u32 destipport = 0; 
    u32 hash = 0;
    u32 csdn = 0;
    u32 cpu_mirror = 0;
    u32 eth_mirror = 0;
    u32 ipv_update = 0;
    u32 ipv_value = 0;
    char cpu_mirror_string[5];
    char eth_mirror_string[5];
    u32 srclockdpv = 0;
    u32 dpv = 0;
    u8 i = 0;
    seq_printf(m, "Line     Secure            Frame-Fmt          CTAG       STAG       Dest-IPPort   Hash \n");
    
    seq_printf(m, "====================================================================================================\n");
    for(entry = 0; entry <= 1023; entry++){
        iowrite32(entry, ioaddr + FWLTHTR);
        for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
            value = ioread32(ioaddr + FWLTHTRR0);
            if (((value >> 31) & 0x01) == 0x0) {
                if((((value >> 2) & 0x01) == 0) && (((value >> 1) & 0x01) == 0x01) && (((value >> 0) & 0x01) == 0x0) ) {
                    fwlthtrr1 = ioread32(ioaddr + FWLTHTRR1);
                    if(fwlthtrr1 >> 8){
                        strcpy(security,"Yes");
                    } else {
                        strcpy(security,"No");
                    }
                    if(((fwlthtrr1 >> 0) & 0x03) == 0x01){
                        strcpy(frame_fmt_code,"IPV4-NO-TCP-NO-UDP");
                    } else if(((fwlthtrr1 >> 0) & 0x03) == 0x02){
                        strcpy(frame_fmt_code,"IPV4-UDP");
                    } else if(((fwlthtrr1 >> 0) & 0x03) == 0x03){
                        strcpy(frame_fmt_code,"IPV4-TCP");
                    }
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
                    fwlthtrr8 = ioread32(ioaddr + FWLTHTRR8);
                    if((fwlthtrr8 >> 15) & 0x01) {
                        routingnum = fwlthtrr8 & 0xFF;
                        routingvalid = 1;
                    }
                    srclockdpv = (fwlthtrr8 >> 16) & 0xFF;
                    csdn = ioread32(ioaddr + FWLTHTRR9);
                    fwlthtrr10 = ioread32(ioaddr + FWLTHTRR10);
                    cpu_mirror = (fwlthtrr10 >> 21) & 0x01;
                    if(cpu_mirror) {
                        strcpy(cpu_mirror_string,"Yes");
                    } else {
                        strcpy(cpu_mirror_string,"No");
                    }
              
                    eth_mirror = (fwlthtrr10 >> 20) & 0x01;
                    if(eth_mirror) {
                        strcpy(eth_mirror_string,"Yes");
                    } else {
                        strcpy(eth_mirror_string,"No");
                    }
                    ipv_update = (fwlthtrr10 >> 19) & 0x01;
                    
                    ipv_value = (fwlthtrr10 >> 15) & 0xFF;
                    dpv = (fwlthtrr10 >> 0) & 0xFF;
                    seq_printf(m, "%4d   %8s     %12s     %7d      %6d     %12d       %d\n",
                    entry,security, frame_fmt_code, ctag, stag,  destipport, hash); 
                    if(!first_print) {
                        seq_printf(m, "Dest-IP           Src-IP     Source-Lock-Ports       Source-Lock-CPU  Routing-Number\n");
                    }
                    seq_printf(m, "%d.%d.%d.%d   %2d.%d.%d.%d ",destipaddr[0],destipaddr[1],destipaddr[2],destipaddr[3],srcipaddr[0],srcipaddr[2],srcipaddr[2], srcipaddr[3]);
                    for (c = 0; c < board_config.eth_ports; c++) {
                        if(((srclockdpv >> c) & 0x01) == 1) {
                            seq_printf(m, "%5d ", c);
                        } else {
                            seq_printf(m, "  ");
                        }
                    }
                    if((srclockdpv >> board_config.eth_ports) == 0x01) {
                        seq_printf(m, " %18s  ","Yes");
                    }
                    if(routingvalid) {
                        seq_printf(m, "%10d \n", routingnum);
                    } else {
                        seq_printf(m, "%13s \n", "No");
                    }
                    if(!first_print) {
                        seq_printf(m, "CPU-Mirror     ETh-Mirror   IPV-Update     Dest-Ports        CSDN    \n");
                        first_print = 1;
                    }
                    seq_printf(m, " %s  %12s ",cpu_mirror_string, eth_mirror_string);
                    if(ipv_update) {
                        seq_printf(m, "%12d ", ipv_value);
                    } else {
                        seq_printf(m, "%12s", "No");
                    }
                    //printk("dpv=%x \n", dpv);
                    for (c = 0; c < board_config.eth_ports; c++) {
                        if(((dpv >> c) & 0x01) == 1) {
                            seq_printf(m, "%12d ", c);
                        } else {
                            seq_printf(m, "  ");
                        }
                    }
                    if((dpv >> board_config.eth_ports) == 0x01) {
                        seq_printf(m, "  %15d    ",csdn);
                    }
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
static int rswitch2_fwd_errors_show(struct seq_file * m, void * v)
{ 
    u8 i = 0;
    seq_printf(m, "          ");
    for(i = 0; i < board_config.eth_ports; i++) {
        seq_printf(m, "Port%d    ", i);
    }
    seq_printf(m, "CPU\n");
    seq_printf(m, "%-8s","ICFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 30) & 0x01);
    }
#if 1
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","DDNTFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 29) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","DDSES");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 28) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","DDFES");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 27) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","DDES");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 26) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","WMIUFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 25) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","WMISFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 24) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","WMFFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 23) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","WMCFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 22) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","SIPHMFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 21) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","SIPHLFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 20) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","SMHMFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 19) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","SMHLFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 18) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","PBNTFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 17) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWVUFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 16) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWDUFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 15) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWSUFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 14) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWNTFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 13) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWVSPFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 12) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWSSPFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 11) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWDSPFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 10) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IPSUFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 9) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IPDUFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 8) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IPNTFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 7) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IPFSFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 6) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IPSSPFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 5) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IPDSPFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 4) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTHUFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 3) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTHNTFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 2) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTHFSFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 1) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTHSPFS");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWEIS00 + (i *0x10)) >> 0) & 0x01);
    }
    seq_printf(m, "\n");
    seq_printf(m, "\n");
    seq_printf(m, "===============================Port-Independent-Errors==========================================\n");
    seq_printf(m, "FTEES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 17) & 0x01 );
    seq_printf(m, "AREES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 16) & 0x01 );
    seq_printf(m, "L23USES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 9) & 0x01 );
    seq_printf(m, "L23UEES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 8) & 0x01 );
    seq_printf(m, "VLANTSES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 7) & 0x01 );
    seq_printf(m, "VLANTEES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 6) & 0x01 );
    seq_printf(m, "MACTSES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 5) & 0x01 );
    seq_printf(m, "MACTEES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 4) & 0x01 );
    seq_printf(m, "IPTSES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 3) & 0x01 );
    seq_printf(m, "IPTEES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 2) & 0x01 );
    seq_printf(m, "LTHTSES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 1) & 0x01 );
    seq_printf(m, "LTHTEES:%d  \n",(ioread32(ioaddr + FWEIS1) >> 0) & 0x01 );

    seq_printf(m, "MACADAS:%d  \n",(ioread32(ioaddr + FWMIS0) >> 17) & 0x01 );
    seq_printf(m, "IPADAS:%d  \n",(ioread32(ioaddr + FWMIS0) >> 16) & 0x01 );
    seq_printf(m, "VLANTFS:%d  \n",(ioread32(ioaddr + FWMIS0) >> 3) & 0x01 );
    seq_printf(m, "MACTFS:%d  \n",(ioread32(ioaddr + FWMIS0) >> 2) & 0x01 );
    seq_printf(m, "IPTFS:%d  \n",(ioread32(ioaddr + FWMIS0) >> 1) & 0x01 );
    seq_printf(m, "LTHTFS:%d  \n",(ioread32(ioaddr + FWMIS0) >> 0) & 0x01 );
#endif    
return 0;
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
    seq_printf(m, "          ");
    for(i = 0; i < board_config.eth_ports; i++) {
        seq_printf(m, "Port%d    ", i);
    }
    seq_printf(m, "CPU\n");
    seq_printf(m, "%-8s","CTFDN");
    for(i = 0; i < board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWCTFDCN0 + (i *20))));
    }
    seq_printf(m, "\n");
    seq_printf(m,"%-8s", "DDFDN");
    for(i = 0; i < board_config.eth_ports; i++) {
        seq_printf(m, "%8s","-");
    }    
    seq_printf(m, "%8d", ioread32(ioaddr + FWCTFDCN0 + (board_config.eth_ports *20)));
    
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTHFDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWLTHFDCN0 + (i *20)))) ;
        
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IPFDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWIPFDCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWFDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWLTWFDCN0 + (i *20)))) ;
        
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","PBFDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWPBFDCN0 + (i *20)))) ;
        
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","MHLN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWMHLCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IHLN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWIHLCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","ICRDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWICRDCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","WMRDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWWMRDCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","CTRDN");
    for(i = 0; i < board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWCTRDCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","DDRDN");
    for(i = 0; i < board_config.eth_ports; i++) {
        seq_printf(m, "%8s","-");
    }   
    seq_printf(m, "%8d", ioread32(ioaddr + FWCTRDCN0 + (board_config.eth_ports *20)));
    
    
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTHRDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWLTHRDCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","IPRDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWIPRDCN0 + (i *20))));
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","LTWRDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr + FWLTWRDCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
    seq_printf(m, "%-8s","PBRDN");
    for(i = 0; i <= board_config.eth_ports; i++) {
        seq_printf(m, "%8d", (ioread32(ioaddr +  FWPBRDCN0 + (i *20)))) ;
    }
    seq_printf(m, "\n");
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
    @brief Open Error Proc Directory

    @param  inode *

    @param  file *

    @return int

*/
static int rswitch2_fwd_errors_open(struct inode * inode, struct  file * file)
{
    return single_open(file, rswitch2_fwd_errors_show, NULL);
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
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch2_fwd_l3_fops = {
    .owner   = THIS_MODULE,
    .open    = rswitch2_fwd_l3_open,
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
    proc_create(RSWITCH2_FWD_PROC_FILE_ERRORS,0,root_dir, &rswitch2_fwd_errors_fops);
    proc_create(RSWITCH2_FWD_PROC_FILE_COUNTERS,0,root_dir, &rswitch2_fwd_counters_fops);
    proc_create(RSWITCH2_FWD_PROC_FILE_L3,0,root_dir, &rswitch2_fwd_l3_fops);
    proc_create(RSWITCH2_FWD_PROC_FILE_L2_L3_UPDATE,0,root_dir, &rswitch2_fwd_l2_l3_update_fops);
    proc_create(RSWITCH2_FWD_PROC_FILE_L2,0,root_dir, &rswitch2_fwd_l2_fops);

}


/**
    @brief  Remove Proc Directories

    @param  void

    @return void

*/
static void rswitch2_fwd_remove_proc_entry(void)
{

    remove_proc_entry(RSWITCH2_FWD_PROC_FILE_ERRORS, root_dir);
    remove_proc_entry(RSWITCH2_FWD_PROC_FILE_COUNTERS, root_dir);
    remove_proc_entry(RSWITCH2_FWD_PROC_FILE_L3, root_dir);
    remove_proc_entry(RSWITCH2_FWD_PROC_FILE_L2_L3_UPDATE, root_dir);
    remove_proc_entry(RSWITCH2_FWD_PROC_FILE_L2, root_dir);
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


/*
    Change History
    2020-08-10    AK  Initial Version(Application support only for L3 IPV4)
    2020-08-10    AK  Automatic probing for port, DIS bug solved, Frame corruption issue
    2020-09-03    AK  Added Proc Statistics
    2020-09-07    AK  Updated for L2/L3 Update, Proc statistics L2/L3 Update
    2020-09-09    AK  Layer 2 Forwarding Support, Proc statistics
    2020-10-07    AK  Updated for Port Forwarding, Proc root directory change

*/
