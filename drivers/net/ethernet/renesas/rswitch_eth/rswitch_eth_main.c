/**
    @brief  R-Switch Ethernet Driver

    Driver for the R-Switch TSN ports.   


    

    @author
        Asad Kamal
        
        
    Copyright (C) 2014 Renesas Electronics Corporation

    This program is free software; you can redistribute it and/or modify it
    under the terms and conditions of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

    This program is distributed in the hope it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.
    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

    The full GNU General Public License is included in this distribution in
    the file called "COPYING".
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>      // GPIO functions/macros
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
#include <linux/skbuff.h>
#include <linux/renesas_rswitch_platform.h>
#include "../../../../ptp/rswitch_ptp.h"
#include "rswitch_eth.h"
#include "rswitch_eth_main.h"
#include <linux/pci.h>
#include <linux/sched/signal.h>
#ifdef CONFIG_RENESAS_RSWITCH_ETH_STATS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif /* CONFIG_RENESAS_RSWITCH_ETH_STATS */

#define RSWITCH_SUPPORT_SPEED_100
#define TX_TS_FEATURE
#define _DEBUG
#define INTERNAL_GATEWAY_PORT
#define BUF_LEN                   20
#define MAX_FRAME_SIZE            2048
#define BOARD_TYPE_SNOW
#define RSWITCH_INT_LINE_GPIO 510
#define MS_TO_NS(x) (x * 1E6L)
#define TAS_DEBUG 1
#define RSWITCHETH_CTRL_MINOR  (127)
#define  RSWITCH_ETH_INTERRUPT_LINES  2
#define RSWITCH_EARO      0x0    
#define RSWITCH_RMAC_OFFSET    0x2000 + RSWITCH_EARO + 0x1800
#define RSWITCH_RMAC_SYSTEM_OFFSET  0x2000 + RSWITCH_EARO
#define RSWITCH_AXIRO   0x1000
#define MODEL_COMPATIBLE_H3VC2 "renesas,h3vc2"
#define MODEL_COMPATIBLE_H3VC3 "renesas,h3vc3"
#define BOARD_VARIANT_H3VC2 (2)
#define BOARD_VARIANT_H3VC3 (3)

static unsigned int error_flag[4] = {0,0,0,0};
static int board_variant = 0;
static char run_mode[BUF_LEN] = "normal";


static struct rswitch_ext_ts_desc *desc_end[NUM_RX_QUEUE * RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
static int Config_Enable_TAS(struct net_device * ndev, struct rswitch_Config_Port_Tx_TAS * TAS);
static int rswitch_tx_free(struct net_device *ndev, int q, bool free_txed_only);
dma_addr_t gtx_desc_bat_dma[15];

module_param_string(mode, run_mode, BUF_LEN, 0);
MODULE_PARM_DESC(mode, "NOR or TAS");
static uint32_t count_tas_frame = 0;

u32 rx_desc_bat_size;
u32 ts_desc_bat_size;
u32 tx_desc_bat_size;
u32 tx_ts_desc_bat_size;
dma_addr_t rx_desc_bat_dma[14];  
dma_addr_t ts_desc_bat_dma; 
struct rswitch_ext_ts_desc *rx_desc_bat[14];

struct rswitch_ext_ts_desc *ts_desc_bat;
dma_addr_t rx_desc_dma[NUM_RX_QUEUE];
dma_addr_t ts_desc_dma;
struct rswitch_ext_desc *tx_desc_bat[15];
dma_addr_t tx_desc_dma[NUM_TX_QUEUE];
dma_addr_t tx_desc_bat_dma[15];
struct sk_buff **rx_skb[NUM_RX_QUEUE];

u32 num_rx_ring[NUM_RX_QUEUE];
u32 cur_rx[NUM_RX_QUEUE];   /* Consumer ring indices */
u32 dirty_rx[NUM_RX_QUEUE]; /* Producer ring indices */
u32 num_ts_ring;
u32 cur_ts; /* Consumer ring indices */
u32 dirty_ts;   /* Producer ring indices */
struct rswitch_ext_ts_desc *rx_ring[NUM_RX_QUEUE];
struct rswitch_ext_ts_desc *ts_ring;


static void __iomem       * EthPort_Addr[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
static void __iomem       * GWCA_Addr = NULL;

ktime_t         ktime; 
/**
    @brief  Net Devices for AVB/TSN Ethernet ports
*/
struct net_device        ** ppndev = NULL;

DEFINE_SPINLOCK(port_lock);

static int                  rswitchtsn_DevMajor;
module_param(rswitchtsn_DevMajor, int, 0440);
static struct class       * rswitchtsn_DevClass = NULL;
static dev_t                rswitchtsn_Dev;
static struct cdev          rswitchtsn_cDev;
static struct device        rswitchtsn_device;





struct EthPorts_TAS_Config_Gate_List gate_list;


/**
    @brief  Board/Hardware Configuration
*/
struct EthPorts_Config      Board_Config;


/*
    Operational Configuration
*/
struct rswitch_Config       Config;                     ///< @brief Active Operational Configuration
struct rswitch_Config       Config_New;                 ///< @brief New Operational Configuration
struct rswitch_Error        rswitch_errors;





/* 
    Interrupts
*/

int                rswitch_eth_IRQ_Data[RSWITCH_ETH_INTERRUPT_LINES];
int                rswitch_eth_IRQ[RSWITCH_ETH_INTERRUPT_LINES];



/** 
    @brief  R-SWITCH Per-Port Registers 
*/
static const u32    port_RegOffsets[MAX_REGISTER_PORT_OFFSET] = 
{
    
    /* R Switch Agent registers */
    [RSWITCH_EAMC]     =  RSWITCH_EARO + 0x0000,
    [RSWITCH_EATMTC]   =  RSWITCH_EARO + 0x0010,
    [RSWITCH_EATAFS]   =  RSWITCH_EARO + 0x0014,
    [RSWITCH_EAPRC]    =  RSWITCH_EARO + 0x0018,
    [RSWITCH_EATMS0]   =  RSWITCH_EARO + 0x0020,
    [RSWITCH_EATMS1]   =  RSWITCH_EARO + 0x0024,
    [RSWITCH_EATMS2]   =  RSWITCH_EARO + 0x0028,
    [RSWITCH_EATMS3]   =  RSWITCH_EARO + 0x002C,
    [RSWITCH_EATMS4]   =  RSWITCH_EARO + 0x0030,
    [RSWITCH_EATMS5]   =  RSWITCH_EARO + 0x0034,
    [RSWITCH_EATMS6]   =  RSWITCH_EARO + 0x0038,
    [RSWITCH_EATMS7]   =  RSWITCH_EARO + 0x003C,
    [RSWITCH_EATDQD0]  =  RSWITCH_EARO + 0x0040,
    [RSWITCH_EATWMC0]  =  RSWITCH_EARO + 0x0060,
    [RSWITCH_EAVTMC]   =  RSWITCH_EARO + 0x0080,
    [RSWITCH_EAVTC]    =  RSWITCH_EARO + 0x0084,
    [RSWITCH_TPTPC]    =  RSWITCH_EARO + 0x00D0,
    [RSWITCH_TTML]     =  RSWITCH_EARO + 0x00D4,
    [RSWITCH_TTJ]      =  RSWITCH_EARO + 0x00D8,
    [RSWITCH_TCC]      =  RSWITCH_EARO + 0x0100,
    [RSWITCH_TCS]      =  RSWITCH_EARO + 0x0104,
    [RSWITCH_TGS]      =  RSWITCH_EARO + 0x010C,
    [RSWITCH_TACST0]   =  RSWITCH_EARO + 0x0110,
    [RSWITCH_TACST1]   =  RSWITCH_EARO + 0x0114,
    [RSWITCH_TACST2]   =  RSWITCH_EARO + 0x0118,
    [RSWITCH_TALIT0]   =  RSWITCH_EARO + 0x0120,
    [RSWITCH_TALIT1]   =  RSWITCH_EARO + 0x0124,
    [RSWITCH_TALIT2]   =  RSWITCH_EARO + 0x0128,
    [RSWITCH_TAEN0]    =  RSWITCH_EARO + 0x0130,
    [RSWITCH_TAEN1]    =  RSWITCH_EARO + 0x0134,
    [RSWITCH_TASFE]    =  RSWITCH_EARO + 0x0140,
    [RSWITCH_TACLL0]   =  RSWITCH_EARO + 0x0150,
    [RSWITCH_TACLL1]   =  RSWITCH_EARO + 0x0154,
    [RSWITCH_TACLL2]   =  RSWITCH_EARO + 0x0158,
    [RSWITCH_CACC]     =  RSWITCH_EARO + 0x0160,
    [RSWITCH_CCS]      =  RSWITCH_EARO + 0x0164,
    [RSWITCH_CAIV0]    =  RSWITCH_EARO + 0x0170,
    [RSWITCH_CAIV1]    =  RSWITCH_EARO + 0x0174,
    [RSWITCH_CAIV2]    =  RSWITCH_EARO + 0x0178,
    [RSWITCH_CAIV3]    =  RSWITCH_EARO + 0x017c,
    [RSWITCH_CAIV4]    =  RSWITCH_EARO + 0x0180,
    [RSWITCH_CAIV5]    =  RSWITCH_EARO + 0x0184,
    [RSWITCH_CAIV6]    =  RSWITCH_EARO + 0x0188,
    [RSWITCH_CAIV7]    =  RSWITCH_EARO + 0x018c,
    [RSWITCH_CAUL0]    =  RSWITCH_EARO + 0x0190,
    [RSWITCH_CAUL1]    =  RSWITCH_EARO + 0x0194,
    [RSWITCH_CAUL2]    =  RSWITCH_EARO + 0x0198,
    [RSWITCH_CAUL3]    =  RSWITCH_EARO + 0x019C,
    [RSWITCH_CAUL4]    =  RSWITCH_EARO + 0x01A0,
    [RSWITCH_CAUL5]    =  RSWITCH_EARO + 0x01A4,
    [RSWITCH_CAUL6]    =  RSWITCH_EARO + 0x01A8,
    [RSWITCH_CAUL7]    =  RSWITCH_EARO + 0x01AC,
    [RSWITCH_EAMS]     =  RSWITCH_EARO + 0x0200,
    [RSWITCH_EAOTM]    =  RSWITCH_EARO + 0x0204,
    [RSWITCH_EADQM0]   =  RSWITCH_EARO + 0x0210,
    [RSWITCH_EADQM1]   =  RSWITCH_EARO + 0x0214,
    [RSWITCH_EADQM2]   =  RSWITCH_EARO + 0x0218,
    [RSWITCH_EADQM3]   =  RSWITCH_EARO + 0x021C,
    [RSWITCH_EADQM4]   =  RSWITCH_EARO + 0x0220,
    [RSWITCH_EADQM5]   =  RSWITCH_EARO + 0x0224,
    [RSWITCH_EADQM6]   =  RSWITCH_EARO + 0x0228,
    [RSWITCH_EADQM7]   =  RSWITCH_EARO + 0x022C,
    [RSWITCH_TOCST0]   =  RSWITCH_EARO + 0x0240,
    [RSWITCH_TOCST1]   =  RSWITCH_EARO + 0x0244,
    [RSWITCH_TOCST2]   =  RSWITCH_EARO + 0x0248,
    [RSWITCH_TOLIT0]   =  RSWITCH_EARO + 0x0250,
    [RSWITCH_TOLIT1]   =  RSWITCH_EARO + 0x0254,
    [RSWITCH_TOLIT2]   =  RSWITCH_EARO + 0x0258,
    [RSWITCH_TOEN0]    =  RSWITCH_EARO + 0x0260,
    [RSWITCH_TOEN1]    =  RSWITCH_EARO + 0x0264,
    [RSWITCH_TOSFE]    =  RSWITCH_EARO + 0x0270,
    [RSWITCH_TCLR0]    =  RSWITCH_EARO + 0x0280,
    [RSWITCH_TCLR1]    =  RSWITCH_EARO + 0x0284,
    [RSWITCH_TCLR2]    =  RSWITCH_EARO + 0x0288,
    [RSWITCH_TSMS]     =  RSWITCH_EARO + 0x0290,
    [RSWITCH_COCC]     =  RSWITCH_EARO + 0x02A0,
    [RSWITCH_COIV0]    =  RSWITCH_EARO + 0x02B0,
    [RSWITCH_COIV1]    =  RSWITCH_EARO + 0x02B4,
    [RSWITCH_COIV2]    =  RSWITCH_EARO + 0x02B8,
    [RSWITCH_COIV3]    =  RSWITCH_EARO + 0x02BC,
    [RSWITCH_COIV4]    =  RSWITCH_EARO + 0x02C0,
    [RSWITCH_COIV5]    =  RSWITCH_EARO + 0x02C4,
    [RSWITCH_COIV6]    =  RSWITCH_EARO + 0x02C8,
    [RSWITCH_COIV7]    =  RSWITCH_EARO + 0x02CC,
    [RSWITCH_COUL0]    =  RSWITCH_EARO + 0x02D0,
    [RSWITCH_COUL1]    =  RSWITCH_EARO + 0x02D4,
    [RSWITCH_COUL2]    =  RSWITCH_EARO + 0x02D8,
    [RSWITCH_COUL3]    =  RSWITCH_EARO + 0x02DC,
    [RSWITCH_COUL4]    =  RSWITCH_EARO + 0x02E0,
    [RSWITCH_COUL5]    =  RSWITCH_EARO + 0x02E4,
    [RSWITCH_COUL6]    =  RSWITCH_EARO + 0x02E8,
    [RSWITCH_COUL7]    =  RSWITCH_EARO + 0x02EC,
    [RSWITCH_EAEIS0]   =  RSWITCH_EARO + 0x0300,    
    [RSWITCH_EAEIE0]   =  RSWITCH_EARO + 0x0304,
    [RSWITCH_EAEID0]   =  RSWITCH_EARO + 0x0308,
    [RSWITCH_EAEIS1]   =  RSWITCH_EARO + 0x0310,    
    [RSWITCH_EAEIE1]   =  RSWITCH_EARO + 0x0314,
    [RSWITCH_EAEID1]   =  RSWITCH_EARO + 0x0318,
    [RSWITCH_EAEIS2]   =  RSWITCH_EARO + 0x0320,    
    [RSWITCH_EAEIE2]   =  RSWITCH_EARO + 0x0324,
    [RSWITCH_EAEID2]   =  RSWITCH_EARO + 0x0328,
    [RSWITCH_EAMIS]    =  RSWITCH_EARO + 0x0330,
    [RSWITCH_EAMIE]    =  RSWITCH_EARO + 0x0334,
    [RSWITCH_EAMID]    =  RSWITCH_EARO + 0x0338,
    [RSWITCH_TCD]      =  RSWITCH_EARO + 0x0340,
    [RSWITCH_EATQD]    =  RSWITCH_EARO + 0x0344,
    [RSWITCH_EADRR]    =  RSWITCH_EARO + 0x0350,
    [RSWITCH_EADRRR]   =  RSWITCH_EARO + 0x0354,
    



    [RSWITCH_RMAC_MCC]     =  RSWITCH_RMAC_OFFSET + 0x0000,         ///< @brief RMAC Configuration
    [RSWITCH_RMAC_MPSM]    =  RSWITCH_RMAC_OFFSET + 0x0010,         ///< @brief RMAC Configuration
    [RSWITCH_RMAC_MPIC]    =  RSWITCH_RMAC_OFFSET + 0x0014,
    [RSWITCH_RMAC_MTFFC]   =  RSWITCH_RMAC_OFFSET + 0x0020,
    [RSWITCH_RMAC_MTAPC]   =  RSWITCH_RMAC_OFFSET + 0x0024,
    [RSWITCH_RMAC_MTMPC]   =  RSWITCH_RMAC_OFFSET + 0x0028,
    [RSWITCH_RMAC_MTATC0]  =  RSWITCH_RMAC_OFFSET + 0x0040,
    [RSWITCH_RMAC_MRGC]    =  RSWITCH_RMAC_OFFSET + 0x0080,
    [RSWITCH_RMAC_MRMAC0]  =  RSWITCH_RMAC_OFFSET + 0x0084,
    [RSWITCH_RMAC_MRMAC1]  =  RSWITCH_RMAC_OFFSET + 0x0088,
    [RSWITCH_RMAC_MRAFC]   =  RSWITCH_RMAC_OFFSET + 0x008C,
    [RSWITCH_RMAC_MRSCE]   =  RSWITCH_RMAC_OFFSET + 0x0090,
    [RSWITCH_RMAC_MRSCP]   =  RSWITCH_RMAC_OFFSET + 0x0094,
    [RSWITCH_RMAC_MRSCC]   =  RSWITCH_RMAC_OFFSET + 0x0098,
    [RSWITCH_RMAC_MRFSCE]  =  RSWITCH_RMAC_OFFSET + 0x009C,
    [RSWITCH_RMAC_MRFSCP]  =  RSWITCH_RMAC_OFFSET + 0x00A0,
    [RSWITCH_RMAC_MTRC]    =  RSWITCH_RMAC_OFFSET + 0x00A4,
    [RSWITCH_RMAC_MPFC0]   =  RSWITCH_RMAC_OFFSET + 0x0100,
    [RSWITCH_RMAC_MLVC]    =  RSWITCH_RMAC_OFFSET + 0x0340,
    [RSWITCH_RMAC_MEEEC]   =  RSWITCH_RMAC_OFFSET + 0x0350,
    [RSWITCH_RMAC_MLBC]    =  RSWITCH_RMAC_OFFSET + 0x0360,
    [RSWITCH_RMAC_MGMR]    =  RSWITCH_RMAC_OFFSET + 0x0400,
    [RSWITCH_RMAC_MMPFTCT] =  RSWITCH_RMAC_OFFSET + 0x0410,
    [RSWITCH_RMAC_MAPFTCT] =  RSWITCH_RMAC_OFFSET + 0x0414,
    [RSWITCH_RMAC_MPFRCT]  =  RSWITCH_RMAC_OFFSET + 0x0418,
    [RSWITCH_RMAC_MFCICT]  =  RSWITCH_RMAC_OFFSET + 0x041C,
    [RSWITCH_RMAC_MEEECT]  =  RSWITCH_RMAC_OFFSET + 0x0420,
    [RSWITCH_RMAC_MEIS]    =  RSWITCH_RMAC_OFFSET + 0x0500,
    [RSWITCH_RMAC_MEIE]    =  RSWITCH_RMAC_OFFSET + 0x0504,
    [RSWITCH_RMAC_MEID]    =  RSWITCH_RMAC_OFFSET + 0x0508,
    [RSWITCH_RMAC_MMIS0]   =  RSWITCH_RMAC_OFFSET + 0x0510,
    [RSWITCH_RMAC_MMIE0]   =  RSWITCH_RMAC_OFFSET + 0x0514,
    [RSWITCH_RMAC_MMID0]   =  RSWITCH_RMAC_OFFSET + 0x0518,
    [RSWITCH_RMAC_MXMS]    =  RSWITCH_RMAC_OFFSET + 0x0600,
    

    [RSWITCH_RMAC_SYS_RXBCE]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1000,
    [RSWITCH_RMAC_SYS_RXBCP]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1004,
    [RSWITCH_RMAC_SYS_RGFCE]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1008,
    [RSWITCH_RMAC_SYS_RGFCP]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x100C,
    [RSWITCH_RMAC_SYS_RBFC]     =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1010,
    [RSWITCH_RMAC_SYS_RMFC]     =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1014,
    [RSWITCH_RMAC_SYS_RVFC]     =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1018,
    [RSWITCH_RMAC_SYS_RPEFC]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x101C,
    [RSWITCH_RMAC_SYS_RNEFC]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1020,
    [RSWITCH_RMAC_SYS_RFMEFC]   =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1024,
    [RSWITCH_RMAC_SYS_RFFMEFC]  =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1028,
    [RSWITCH_RMAC_SYS_RCFCEFC]  =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x102C,
    [RSWITCH_RMAC_SYS_RFCEFC]   =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1030,
    [RSWITCH_RMAC_SYS_RRCFEFC]  =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1034,
    [RSWITCH_RMAC_SYS_RRXFEFC]  =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1038,
    [RSWITCH_RMAC_SYS_RUEFC]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x103C,
    [RSWITCH_RMAC_SYS_ROEFC]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1040,
    [RSWITCH_RMAC_SYS_RBOEC]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1044,
    [RSWITCH_RMAC_SYS_TXBCE]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1100,
    [RSWITCH_RMAC_SYS_TXBCP]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1104,
    [RSWITCH_RMAC_SYS_TGFCE]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1108,
    [RSWITCH_RMAC_SYS_TGFCP]    =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x110C,
    [RSWITCH_RMAC_SYS_TBFC]     =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1110,
    [RSWITCH_RMAC_SYS_TMFC]     =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1114,
    [RSWITCH_RMAC_SYS_TVFC]     =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x1118,
    [RSWITCH_RMAC_SYS_TEFC]     =  RSWITCH_RMAC_SYSTEM_OFFSET + 0x111C,
    

    /* Gateway CPU Registers */
    [RSWITCH_GWCA_RR0]       =   0x0000,
    [RSWITCH_GWCA_RIPV]      =   0x0100,
    [RSWITCH_GWCA_RMIS0]     =   0x0200,
    [RSWITCH_GWCA_RMIE0]     =   0x0204,
    [RSWITCH_GWCA_RMID0]     =   0x0208,
    [RSWITCH_GWCA_RMIS1]     =   0x0210,
    [RSWITCH_GWCA_RMIE1]     =   0x0214,
    [RSWITCH_GWCA_RMID1]     =   0x0218,
    [RSWITCH_GWCA_REIS0]     =   0x0220,
    [RSWITCH_GWCA_REIE0]     =   0x0224,
    [RSWITCH_GWCA_REID0]     =   0x0228,
    [RSWITCH_GWCA_REIS1]     =   0x0230,
    [RSWITCH_GWCA_REIE1]     =   0x0234,
    [RSWITCH_GWCA_REID1]     =   0x0238,
    [RSWITCH_GWCA_RDIS]      =   0x0240,
    [RSWITCH_GWCA_RDIE]      =   0x0244,
    [RSWITCH_GWCA_RDID]      =   0x0248,
    [RSWITCH_GWCA_RMDIOIS]   =   0x0280,
    [RSWITCH_GWCA_RMDIOIE]   =   0x0284,
    [RSWITCH_GWCA_RMDIOID]   =   0x0288,
    [RSWITCH_GWCA_RDASK]     =   0x0300,
    [RSWITCH_GWCA_RDAU]      =   0x0304,
    [RSWITCH_GWCA_RDASKC]    =   0x0308,
    [RSWITCH_GWCA_GWMC]      =   0x0400,
    [RSWITCH_GWCA_GWRTC0]    =   0x0410,
    [RSWITCH_GWCA_GWERDQD0]  =   0x0430,
    [RSWITCH_GWCA_GWPRDQD0]  =   0x0450,
    [RSWITCH_GWCA_GWEWMC0]   =   0x0470,
    [RSWITCH_GWCA_GWPWMC0]   =   0x0490,
    [RSWITCH_GWCA_GWRMSTLS]  =   0x04B0,
    [RSWITCH_GWCA_GWRMSTLR]  =   0x04B4,
    [RSWITCH_GWCA_GWRGC]     =   0x04B8,
    [RSWITCH_GWCA_GWMS]      =   0x0500,
    [RSWITCH_GWCA_GWRR]      =   0x0504,
    [RSWITCH_GWCA_GWERDQM0]  =   0x0510,
    [RSWITCH_GWCA_GWPRDQM0]  =   0x0530,
    [RSWITCH_GWCA_GWERDQML0] =   0x0550,
    [RSWITCH_GWCA_GWPRDQML0] =   0x0570,
    [RSWITCH_GWCA_GWRDQM]    =   0x0590,
    [RSWITCH_GWCA_GWRDQML]   =   0x0594,
    [RSWITCH_GWCA_GWRMSTSS]  =   0x05A0,
    [RSWITCH_GWCA_GWRMSTSR0] =   0x05A4,
    [RSWITCH_GWCA_GWEIS0]    =   0x0600,
    [RSWITCH_GWCA_GWEIE0]    =   0x0604,
    [RSWITCH_GWCA_GWEID0]    =   0x0608,
    [RSWITCH_GWCA_GWEIS1]    =   0x0610,
    [RSWITCH_GWCA_GWEIE1]    =   0x0614,
    [RSWITCH_GWCA_GWEID1]    =   0x0618,
    [RSWITCH_GWCA_GWTSRR]    =   0x0700,
    [RSWITCH_GWCA_GWTSRRR0]  =   0x0704,
    [RSWITCH_GWCA_GWDRR]     =   0x0710,
    [RSWITCH_GWCA_GWDRRR0]   =   0x0714,
    [RSWITCH_AXI_AXIWC]      =   RSWITCH_AXIRO + 0x0000,
    [RSWITCH_AXI_AXIRC]      =   RSWITCH_AXIRO + 0x0004,
    [RSWITCH_AXI_TDPC0]      =   RSWITCH_AXIRO + 0x0010,
    [RSWITCH_AXI_TFT]        =   RSWITCH_AXIRO + 0x0090,
    [RSWITCH_AXI_TATLS0]     =   RSWITCH_AXIRO + 0x00A0,
    [RSWITCH_AXI_TATLS1]     =   RSWITCH_AXIRO + 0x00A4,
    [RSWITCH_AXI_TATLR]      =   RSWITCH_AXIRO + 0x00A8,
    [RSWITCH_AXI_RATLS0]     =   RSWITCH_AXIRO + 0x00B0,
    [RSWITCH_AXI_RATLS1]     =   RSWITCH_AXIRO + 0x00B4,
    [RSWITCH_AXI_RATLR]      =   RSWITCH_AXIRO + 0x00B8,
    [RSWITCH_AXI_TSA0]       =   RSWITCH_AXIRO + 0x00C0,
    [RSWITCH_AXI_TSS0]       =   RSWITCH_AXIRO + 0x00C4,
    [RSWITCH_AXI_TRCR0]      =   RSWITCH_AXIRO + 0x0140,
    [RSWITCH_AXI_RIDAUAS0]   =   RSWITCH_AXIRO + 0x0180,
    [RSWITCH_AXI_RR]         =   RSWITCH_AXIRO + 0x0200,
    [RSWITCH_AXI_TATS]       =   RSWITCH_AXIRO + 0x0210,
    [RSWITCH_AXI_TATSR0]     =   RSWITCH_AXIRO + 0x0214,
    [RSWITCH_AXI_RATS]       =   RSWITCH_AXIRO + 0x0220,
    [RSWITCH_AXI_RATSR0]     =   RSWITCH_AXIRO + 0x0224,
    [RSWITCH_AXI_RIDASM0]    =   RSWITCH_AXIRO + 0x0240,
    [RSWITCH_AXI_RIDASAM0]   =   RSWITCH_AXIRO + 0x0244,
    [RSWITCH_AXI_RIDACAM0]   =   RSWITCH_AXIRO + 0x0248,
    [RSWITCH_AXI_EIS0]       =   RSWITCH_AXIRO + 0x0300,
    [RSWITCH_AXI_EIE0]       =   RSWITCH_AXIRO + 0x0304,
    [RSWITCH_AXI_EID0]       =   RSWITCH_AXIRO + 0x0308,
    [RSWITCH_AXI_EIS1]       =   RSWITCH_AXIRO + 0x0310,
    [RSWITCH_AXI_EIE1]       =   RSWITCH_AXIRO + 0x0314,
    [RSWITCH_AXI_EID1]       =   RSWITCH_AXIRO + 0x0318,
    [RSWITCH_AXI_TCEIS0]     =   RSWITCH_AXIRO + 0x0340,
    [RSWITCH_AXI_TCEIE0]     =   RSWITCH_AXIRO + 0x0344,
    [RSWITCH_AXI_TCEID0]     =   RSWITCH_AXIRO + 0x0348,
    [RSWITCH_AXI_TCEIS1]     =   RSWITCH_AXIRO + 0x0350,
    [RSWITCH_AXI_TCEIE1]     =   RSWITCH_AXIRO + 0x0354,
    [RSWITCH_AXI_TCEID1]     =   RSWITCH_AXIRO + 0x0358,
    [RSWITCH_AXI_RFSEIS0]    =   RSWITCH_AXIRO + 0x04C0,
    [RSWITCH_AXI_RFSEIE0]    =   RSWITCH_AXIRO + 0x04C4,
    [RSWITCH_AXI_RFSEID0]    =   RSWITCH_AXIRO + 0x04C8,
    [RSWITCH_AXI_RFSEIS1]    =   RSWITCH_AXIRO + 0x04D0,
    [RSWITCH_AXI_RFSEIE1]    =   RSWITCH_AXIRO + 0x04D4,
    [RSWITCH_AXI_RFSEID1]    =   RSWITCH_AXIRO + 0x04D8,
    [RSWITCH_AXI_RFSEIS2]    =   RSWITCH_AXIRO + 0x04E0,
    [RSWITCH_AXI_RFSEIE2]    =   RSWITCH_AXIRO + 0x04E4,
    [RSWITCH_AXI_RFSEID2]    =   RSWITCH_AXIRO + 0x04E8,
    [RSWITCH_AXI_RFSEIS3]    =   RSWITCH_AXIRO + 0x04F0,
    [RSWITCH_AXI_RFSEIE3]    =   RSWITCH_AXIRO + 0x04F4,
    [RSWITCH_AXI_RFSEID3]    =   RSWITCH_AXIRO + 0x04F8,
    [RSWITCH_AXI_RFSEIS4]    =   RSWITCH_AXIRO + 0x0500,
    [RSWITCH_AXI_RFSEIE4]    =   RSWITCH_AXIRO + 0x0504,
    [RSWITCH_AXI_RFSEID4]    =   RSWITCH_AXIRO + 0x0508,
    [RSWITCH_AXI_RFSEIS5]    =   RSWITCH_AXIRO + 0x0510,
    [RSWITCH_AXI_RFSEIE5]    =   RSWITCH_AXIRO + 0x0514,
    [RSWITCH_AXI_RFSEID5]    =   RSWITCH_AXIRO + 0x0518,
    [RSWITCH_AXI_RFSEIS6]    =   RSWITCH_AXIRO + 0x0520,
    [RSWITCH_AXI_RFSEIE6]    =   RSWITCH_AXIRO + 0x0524,
    [RSWITCH_AXI_RFSEID6]    =   RSWITCH_AXIRO + 0x0528,
    [RSWITCH_AXI_RFSEIS7]    =   RSWITCH_AXIRO + 0x0530,
    [RSWITCH_AXI_RFSEIE7]    =   RSWITCH_AXIRO + 0x0534,
    [RSWITCH_AXI_RFSEID7]    =   RSWITCH_AXIRO + 0x0538,
    [RSWITCH_AXI_RFEIS0]     =   RSWITCH_AXIRO + 0x0540,
    [RSWITCH_AXI_RFEIE0]     =   RSWITCH_AXIRO + 0x0544,
    [RSWITCH_AXI_RFEID0]     =   RSWITCH_AXIRO + 0x0548,
    [RSWITCH_AXI_RFEIS1]     =   RSWITCH_AXIRO + 0x0550,
    [RSWITCH_AXI_RFEIE1]     =   RSWITCH_AXIRO + 0x0554,
    [RSWITCH_AXI_RFEID1]     =   RSWITCH_AXIRO + 0x0558,
    [RSWITCH_AXI_RFEIS2]     =   RSWITCH_AXIRO + 0x0560,
    [RSWITCH_AXI_RFEIE2]     =   RSWITCH_AXIRO + 0x0564,
    [RSWITCH_AXI_RFEID2]     =   RSWITCH_AXIRO + 0x0568,
    [RSWITCH_AXI_RFEIS3]     =   RSWITCH_AXIRO + 0x0570,
    [RSWITCH_AXI_RFEIE3]     =   RSWITCH_AXIRO + 0x0574,
    [RSWITCH_AXI_RFEID3]     =   RSWITCH_AXIRO + 0x0578,
    [RSWITCH_AXI_RFEIS4]     =   RSWITCH_AXIRO + 0x0580,
    [RSWITCH_AXI_RFEIE4]     =   RSWITCH_AXIRO + 0x0584,
    [RSWITCH_AXI_RFEID4]     =   RSWITCH_AXIRO + 0x0588,
    [RSWITCH_AXI_RFEIS5]     =   RSWITCH_AXIRO + 0x0590,
    [RSWITCH_AXI_RFEIE5]     =   RSWITCH_AXIRO + 0x0594,
    [RSWITCH_AXI_RFEID5]     =   RSWITCH_AXIRO + 0x0598,
    [RSWITCH_AXI_RFEIS6]     =   RSWITCH_AXIRO + 0x05A0,
    [RSWITCH_AXI_RFEIE6]     =   RSWITCH_AXIRO + 0x05A4,
    [RSWITCH_AXI_RFEID6]     =   RSWITCH_AXIRO + 0x05A8,
    [RSWITCH_AXI_RFEIS7]     =   RSWITCH_AXIRO + 0x05B0,
    [RSWITCH_AXI_RFEIE7]     =   RSWITCH_AXIRO + 0x05B4,
    [RSWITCH_AXI_RFEID7]     =   RSWITCH_AXIRO + 0x05B8,
    [RSWITCH_AXI_RCEIS0]     =   RSWITCH_AXIRO + 0x05C0,
    [RSWITCH_AXI_RCEIE0]     =   RSWITCH_AXIRO + 0x05C4,
    [RSWITCH_AXI_RCEID0]     =   RSWITCH_AXIRO + 0x05C8,
    [RSWITCH_AXI_RCEIS1]     =   RSWITCH_AXIRO + 0x05D0,
    [RSWITCH_AXI_RCEIE1]     =   RSWITCH_AXIRO + 0x05D4,
    [RSWITCH_AXI_RCEID1]     =   RSWITCH_AXIRO + 0x05D8,
    [RSWITCH_AXI_RCEIS2]     =   RSWITCH_AXIRO + 0x05E0,
    [RSWITCH_AXI_RCEIE2]     =   RSWITCH_AXIRO + 0x05E4,
    [RSWITCH_AXI_RCEID2]     =   RSWITCH_AXIRO + 0x05E8,
    [RSWITCH_AXI_RCEIS3]     =   RSWITCH_AXIRO + 0x05F0,
    [RSWITCH_AXI_RCEIE3]     =   RSWITCH_AXIRO + 0x05F4,
    [RSWITCH_AXI_RCEID3]     =   RSWITCH_AXIRO + 0x05F8,
    [RSWITCH_AXI_RCEIS4]     =   RSWITCH_AXIRO + 0x0600,
    [RSWITCH_AXI_RCEIE4]     =   RSWITCH_AXIRO + 0x0604,
    [RSWITCH_AXI_RCEID4]     =   RSWITCH_AXIRO + 0x0608,
    [RSWITCH_AXI_RCEIS5]     =   RSWITCH_AXIRO + 0x0610,
    [RSWITCH_AXI_RCEIE5]     =   RSWITCH_AXIRO + 0x0614,
    [RSWITCH_AXI_RCEID5]     =   RSWITCH_AXIRO + 0x0618,
    [RSWITCH_AXI_RCEIS6]     =   RSWITCH_AXIRO + 0x0620,
    [RSWITCH_AXI_RCEIE6]     =   RSWITCH_AXIRO + 0x0624,
    [RSWITCH_AXI_RCEID6]     =   RSWITCH_AXIRO + 0x0628,
    [RSWITCH_AXI_RCEIS7]     =   RSWITCH_AXIRO + 0x0630,
    [RSWITCH_AXI_RCEIE7]     =   RSWITCH_AXIRO + 0x0634,
    [RSWITCH_AXI_RCEID7]     =   RSWITCH_AXIRO + 0x0638,
    [RSWITCH_AXI_RIDAOIS]    =   RSWITCH_AXIRO + 0x0640,
    [RSWITCH_AXI_RIDAOIE]    =   RSWITCH_AXIRO + 0x0644,
    [RSWITCH_AXI_RIDAOID]    =   RSWITCH_AXIRO + 0x0648, 
    [RSWITCH_AXI_TSFEIS]     =   RSWITCH_AXIRO + 0x06C0,
    [RSWITCH_AXI_TSFEIE]     =   RSWITCH_AXIRO + 0x06C4,
    [RSWITCH_AXI_TSFEID]     =   RSWITCH_AXIRO + 0x06C8,
    [RSWITCH_AXI_TSCEIS]     =   RSWITCH_AXIRO + 0x06D0,
    [RSWITCH_AXI_TSCEIE]     =   RSWITCH_AXIRO + 0x06D4,
    [RSWITCH_AXI_TSCEID]     =   RSWITCH_AXIRO + 0x06D8,
    [RSWITCH_AXI_DIS]        =   RSWITCH_AXIRO + 0x0B00,
    [RSWITCH_AXI_DIE]        =   RSWITCH_AXIRO + 0x0B04,
    [RSWITCH_AXI_DID]        =   RSWITCH_AXIRO + 0x0B08,
    [RSWITCH_AXI_TDIS0]      =   RSWITCH_AXIRO + 0x0B10,
    [RSWITCH_AXI_TDIE0]      =   RSWITCH_AXIRO + 0x0B14,
    [RSWITCH_AXI_TDID0]      =   RSWITCH_AXIRO + 0x0B18,
    [RSWITCH_AXI_RDIS0]      =   RSWITCH_AXIRO + 0x0B90,
    [RSWITCH_AXI_RDIE0]      =   RSWITCH_AXIRO + 0x0B94,
    [RSWITCH_AXI_RDID0]      =   RSWITCH_AXIRO + 0x0B98,
    [RSWITCH_AXI_TSDIS]      =   RSWITCH_AXIRO + 0x0C10,
    [RSWITCH_AXI_TSDIE]      =   RSWITCH_AXIRO + 0x0C14,
    [RSWITCH_AXI_TSDID]      =   RSWITCH_AXIRO + 0x0C18,
};

/**
    @brief Write 64 bit read maska and write function

    @param  addr 

    @param  value

    @return void

*/
static void my_iowrite32( uint64_t value, void __iomem  * addr)
{


    uint64_t readvalue = 0;
    
    /*lower address */
    if (( (addr - ioaddr) & 0x04) == 0 )
    {
        readvalue = ioread64(addr);
        readvalue = readvalue & 0xFFFFFFFF00000000;
        readvalue |= (value & 0xFFFFFFFF);
        iowrite64(readvalue,addr);
        

    }
    /* Upper address */
    else
    {
        readvalue = ioread64(addr -4 );
        readvalue = readvalue & 0x00000000FFFFFFFF;
        readvalue |= ((value << 32) & 0xFFFFFFFF00000000) ;
        iowrite64(readvalue,(addr -4));

    }
    
}


/**
    @brief Write 32 bit GWCA register

    @param  int enum_index Register offset 

    @param  data value to be written

    @return int

*/
static inline void eth_gwca_reg_write(unsigned long data, int enum_index)
{
    

    my_iowrite32(data, GWCA_Addr  + (port_RegOffsets[enum_index]));
}


/**
    @brief Read 32 bit GWCA register

    @param  int enum_index Register offset 

    @return int

*/
static inline unsigned long eth_gwca_reg_read( int enum_index)
{
    
    return ioread32(GWCA_Addr + (port_RegOffsets[enum_index]));
}


/**
    @brief Set GWCA Mode

    @param  mode

    @return int

*/
static int rswitch_gwca_set_mode(uint32_t mode)
{
    eth_gwca_reg_write(mode, RSWITCH_GWCA_GWMC); 
    return 0;

}


/**
    @brief GWCA Init Function

    @param  void  

    @return int

*/
static int rswitch_gwca_init(void)
{

    /* Call this function in config mode */

    
    
    int k  = 0;
    uint32_t RR = 0;
    uint32_t  RATLR = 0;
    uint8_t desc_learn  = 0;
    
    uint32_t GWRR  = 0;
    /* Disable GWMC */
    eth_gwca_reg_write(RSWITCH_GWMC_DISABLE, RSWITCH_GWCA_GWMC);
    /* Config GWMC */
    eth_gwca_reg_write(RSWITCH_GWMC_CONFIG, RSWITCH_GWCA_GWMC);
    
    /* Call this function in config mode */
    

    
    /* Check RAM Ready */

    RR = eth_gwca_reg_read(RSWITCH_AXI_RR);
    
    if((RR & 0x03) == 0x03)
    {
        GWRR = eth_gwca_reg_read(RSWITCH_GWCA_GWRR);
        while(GWRR == 0)
        {
            GWRR = eth_gwca_reg_read(RSWITCH_GWCA_GWRR);

        }
        printk("RAM rest Complete with GWRR = %d\n", GWRR);
        eth_gwca_reg_write(0x33333,RSWITCH_AXI_AXIWC ); 
        eth_gwca_reg_write(0x33333,RSWITCH_AXI_AXIRC ); 
        /*Set TS Descriptor Chain Priority  Check on what basis*/
        eth_gwca_reg_write(0x76543210,RSWITCH_AXI_TDPC0 ); /// On assumption , verify value // 8 Descriptor per register setting assuming total Desc Chain as 64
        eth_gwca_reg_write(0x76543210,RSWITCH_AXI_TDPC0 + (4*1)  );
        eth_gwca_reg_write(0x76543210,RSWITCH_AXI_TDPC0 + (4*2) );
        eth_gwca_reg_write(0x76543210,RSWITCH_AXI_TDPC0 + (4*3) );
        eth_gwca_reg_write(0x76543210,RSWITCH_AXI_TDPC0 + (4*4) );
        eth_gwca_reg_write(0x76543210,RSWITCH_AXI_TDPC0 + (4*5) );
        eth_gwca_reg_write(0x76543210,RSWITCH_AXI_TDPC0 + (4*6) );
        eth_gwca_reg_write(0x76543210,RSWITCH_AXI_TDPC0 + (4*7) );
        /* Configure all as express now later configuration will take care */
        eth_gwca_reg_write(0x0,RSWITCH_AXI_TFT );
        
        
        RATLR = eth_gwca_reg_read(RSWITCH_AXI_RATLR);
        eth_gwca_reg_write((RATLR | 0x80000000),RSWITCH_AXI_RATLR );
        RATLR = eth_gwca_reg_read (RSWITCH_AXI_RATLR);
        for (k = 0; k < RSWITCH_DESCLEARN_CONFIG_TIMEOUT_MS; k++) 
        {
            RATLR = eth_gwca_reg_read( RSWITCH_AXI_RATLR);
            if((RATLR & 0x80000000) == 0x0)
            {
                desc_learn = 1;
                break;

            }
            mdelay(1);

        } 
        if(desc_learn  == 0)
        {
            printk("Descriptor Learning Error  \n");
            return -1;
        }
        
    }
    else
    {
        printk("AXI RAM not initialised \n");
        return -1;

    }
    /* Disable GWMC */
    printk("AXI RAM  initialisation SUCCESS \n");
    return 0;
}


/**
    @brief  Write a 32-bit I/O register

    Port Base 0..n  0x1000, 0x1800, 0x2000, 0x2800, ...
    Port Registers  0x0000 - 0x07FF (relative to the start of each port)
*/
static inline void portreg_write(struct net_device * ndev, unsigned long data, int enum_index)
{
    struct port_private * mdp = netdev_priv(ndev);
    my_iowrite32(data, (mdp->port_base_addr  + (mdp->PortReg_offset[enum_index])));
}


/**
    @brief  Read a 32-bit I/O register
*/
static inline unsigned long portreg_read(struct net_device * ndev, int enum_index)
{
    struct port_private * mdp = netdev_priv(ndev);

    return ioread32(mdp->port_base_addr + (mdp->PortReg_offset[enum_index]));
}








/* PROC Staistics */
/*
    /proc/rswitch_eth/
                
                driver
                
                

            ports/status
            
                {port number}/
                                {tx|rx}          Current Tx/Rx statistics
                                
                                


    

    



    

    
*/


//      /proc/rswitch_eth/
#define RSWITCH_ETH_PROC_DIR                         "rswitch_eth"



//      /proc/rswitch_eth/driver 
#define RSWITCH_ETH_PROC_FILE_DRIVER                 "ethdriver"





//      /proc/rswitch_eth/ports
#define RSWITCH_ETH_PROC_DIR_PORTS                   "ports"
#define RSWITCH_GW_PROC_DIR                          "gw"
#define RSWITCH_GW_PROC_DIR_QUEUES                   "queue"
#define RSWITCH_GW_PROC_ERROR                        "error"
#define RSWITCH_GW_PROC_AXI_DIR_RXDESCS              "RXDESC"
#define RSWITCH_GW_PROC_AXI_DIR_TXDESCS              "TXDESC"
#define RSWITCH_GW_PROC_AXI_DIR_TSDESCS              "TSDESC"
#define RSWITCH_GW_PROC_AXI_DIR_RXINCDESC            "RXINCDESC"

//      /proc/rswitch_eth/ports/{n}/rx
#define RSWITCH_ETH_PROC_FILE_PORTS_PORT_RX          "rx"
#define RSWITCH_ETH_PROC_FILE_PORTS_PORT_Err         "error"
#define RSWITCH_ETH_PROC_FILE_AXI_TSDESC_ERROR       "error"
#define RSWITCH_ETH_PROC_FILE_AXI_TXDESC_ERROR       "error"
#define RSWITCH_ETH_PROC_FILE_AXI_RXDESC_ERROR       "error"
#define RSWITCH_ETH_PROC_FILE_AXI_RXINCDESC_ERROR    "error"
#define RSWITCH_ETH_PROC_FILE_GW_QUEUES_QUEUE_ERROR  "error"
#define RSWITCH_GW_PROC_AXI_ERROR                    "error"
#define RSWITCH_GW_PROC_AXI                          "AXI"
//      /proc/rswitch_eth/ports/{n}/tx
#define RSWITCH_ETH_PROC_FILE_PORTS_PORT_TX          "tx"
#define RSWITCH_ETH_PROC_FILE_PORTS_PORT_Monitor     "monitor"







/* Proc Directories*/
static struct 
{
    struct proc_dir_entry   * RootDir;                      // /proc/rswitch_eth/
    struct proc_dir_entry   * PortsDir;                     //           ports/
    struct proc_dir_entry   * gwDir;                     //           ports/
    struct proc_dir_entry   * QueuesDir;
    struct proc_dir_entry   * PortDir[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1];   //              {n}/
    struct proc_dir_entry   * QueueDir[RENESAS_RSWITCH_TX_QUEUES];
    struct proc_dir_entry   *axiDir;
    struct proc_dir_entry   *TXDescsDir;
    struct proc_dir_entry   *TXDescDir[64];
    struct proc_dir_entry   *RXDescsDir;
    struct proc_dir_entry   *RXDescDir[256];
    struct proc_dir_entry   *TSDescsDir;
    struct proc_dir_entry   *TSDescDir[8];
    struct proc_dir_entry   *RXincDescsDir;
    struct proc_dir_entry   *RXincDescDir[8];
} rswitch_eth_ProcDir;









/**
    @brief GW Error Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_gw_error_Show(struct seq_file * m, void * v)
{
    
    seq_printf(m, 
    "ECC fatal Error   = %d       \n", rswitch_errors.gw_Error.ECC_fatal_error);
    seq_printf(m, 
    "TX ECC Error      = %d       \n", rswitch_errors.gw_Error.trans_ECC_error);
    seq_printf(m, 
    "AXI Error Status    = %d       \n", rswitch_errors.gw_Error.AXIBMIEIS);
    return 0;
}

/**
    @brief AXI Error Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_axi_error_Show(struct seq_file * m, void * v)
{
    
    seq_printf(m, 
    "ECC fatal Error   = %d       \n", rswitch_errors.gw_Error.AXI_error.ecc_fatal_error);
    
    seq_printf(m, 
    "AXI BUS Error    = %d       \n", rswitch_errors.gw_Error.AXI_error.axi_bus_error);
    return 0;
}


/**
    @brief Driver Status Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_eth_DriverStatus_Show(struct seq_file * m, void * v)
{
    seq_printf(m, "S/W Version: %s\n", RSWITCHETH_DRIVER_VERSION);
    seq_printf(m, "Build on %s at %s\n", __DATE__, __TIME__);
#ifdef DEBUG
    seq_printf(m, "Debug Level: %d \n", DebugPrint);
#else
    seq_printf(m, "Debug: no\n");
#endif

    return 0;
}


/**
    @brief Port TX Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_PortTx_Show(struct seq_file * m, void * v)
{
    int   port = (uintptr_t)m->private;
    struct net_device * ndev = ppndev[port];
    static uint32_t eframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t pframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t ebyte[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t pbyte[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t txerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t bframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t mframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t vframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    eframe[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_TGFCE);
    pframe[port]  += portreg_read(ndev, RSWITCH_RMAC_SYS_TGFCP);
    ebyte[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_TXBCE);
    pbyte[port]  += portreg_read(ndev, RSWITCH_RMAC_SYS_TXBCP);
    txerror[port] +=portreg_read(ndev, RSWITCH_RMAC_SYS_TEFC);
    bframe[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_TBFC);
    mframe[port]  += portreg_read(ndev, RSWITCH_RMAC_SYS_TMFC);
    vframe[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_TVFC);
    seq_printf(m, 
    "E frames   = %d       \n", eframe[port]);
    seq_printf(m, 
    "P frames   = %d       \n", pframe[port]);
    seq_printf(m, 
    "E bytes   = %d       \n", ebyte[port]);
    seq_printf(m, 
    "P bytes   = %d       \n", pbyte[port]);
    seq_printf(m, 
    "Broadcast frames   = %d       \n", bframe[port]);
    seq_printf(m, 
    "Multicast frames   = %d       \n", mframe[port]);
    seq_printf(m, 
    "VLAN frames   = %d       \n", vframe[port]);
    seq_printf(m, 
    "Error frames   = %d       \n", txerror[port]);
    
    return 0;
}

/**
    @brief Port Error Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_PortErr_Show(struct seq_file * m, void * v)
{
    int                              port = (uintptr_t)m->private;
    
    int i = 0;
    seq_printf(m, "Error                        TotalCount");

    
    for(i = 0; i < 32; i++)
    {
        seq_printf(m, "%s                        %d \n", rswitch_errors.Port_Error[port].EAEIS1_error[i].ErrorName, rswitch_errors.Port_Error[port].EAEIS1_error[i].active);
        if(i <= 20)
        {
            seq_printf(m, "%s                        %d \n", rswitch_errors.Port_Error[port].EAEIS0_error[i].ErrorName, rswitch_errors.Port_Error[port].EAEIS0_error[i].active);

        }
        if(i < 24)
        {
            seq_printf(m, "%s                        %d \n", rswitch_errors.Port_Error[port].EAEIS2_error[i].ErrorName, rswitch_errors.Port_Error[port].EAEIS2_error[i].active);

        }
    }
    return 0;
}



/**
    @brief Port Monitor Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_PortMonitor_Show(struct seq_file * m, void * v)
{
    int                              port = (uintptr_t)m->private;
    int i  = 0;
    int j  = 0;
    struct net_device * ndev = ppndev[port];
    uint32_t entq[8];
    uint32_t TCLR2 = 0;
    uint32_t EAMS = 0;
    uint32_t skip_first_entry  = 0;
    uint32_t CIV = 0;
    uint32_t CUL = 0;
    uint32_t EAOTM = 0;
    uint32_t EADQM = 0;
    uint32_t EAMIS = 0;
    uint32_t entr_num  = 0;
    uint32_t COCC = 0;
    seq_printf(m, 
    "***********Agent Status********** \n");
    EAMS = portreg_read(ndev, RSWITCH_EAMS);
    EAOTM = portreg_read(ndev, RSWITCH_EAOTM);

    if((EAMS & 0x03) == 0x0)
    {
        seq_printf(m, 
        "Mode   = Reset       \n");
    }
    else if((EAMS & 0x03) == 0x1)
    {
        seq_printf(m, 
        "Mode   = Disable       \n");
    }
    else if((EAMS & 0x03) == 0x2)
    {
        seq_printf(m, 
        "Mode   = Config       \n");
    }
    else if((EAMS & 0x03) == 0x3)
    {
        seq_printf(m, 
        "Mode   = Operation       \n");
    }
    if(((EAMS >> 16) & 0x01) == 0x0)
    {

        seq_printf(m, 
        "TX   = Active       \n");

    }
    else
    {

        seq_printf(m, 
        "TX   = Suspended       \n");

    }
    if(((EAMS >> 17) & 0x01) == 0x0)
    {

        seq_printf(m, 
        "RX   = Active       \n");

    }
    else
    {

        seq_printf(m, 
        "RX   = Suspended       \n");

    }
    if(EAOTM & 0x01)
    {
        seq_printf(m, 
        "PMAC TX   = Ongoing       \n");

    }
    else
    {
        seq_printf(m, 
        "PMAC TX   = Stopped       \n");

    }
    if((EAOTM >> 1) & 0x01)
    {
        seq_printf(m, 
        "EMAC TX   = Ongoing       \n");

    }
    else
    {
        seq_printf(m, 
        "EMAC TX   = Stopped       \n");

    }
    if((EAOTM >> 2) & 0x01)
    {
        seq_printf(m, 
        "PMAC RX   = Ongoing       \n");

    }
    else
    {
        seq_printf(m, 
        "PMAC RX   = Stopped       \n");

    }
    if((EAOTM >> 3) & 0x01)
    {
        seq_printf(m, 
        "EMAC RX   = Ongoing       \n");

    }
    else
    {
        seq_printf(m, 
        "EMAC RX   = Stopped       \n");

    }
    for(i = 0; i < 7; i++)
    {
        if(EAOTM >> (16+i))
        {
            seq_printf(m, 
            "Queue%d   = Ongoing  Tx      \n", i);
        }
        else
        {
            seq_printf(m, 
            "Queue%d   = Not Ongoing  Tx      \n", i);
        }
        EADQM = portreg_read(ndev, RSWITCH_EADQM0 + i);
        seq_printf(m, 
        "Queue%d   = Descriptor No:%d     \n", i, EADQM);

    }
    seq_printf(m, 
    "***********TAS********** \n");
    EAMIS = portreg_read(ndev, RSWITCH_EAMIS);
    if(EAMIS)
    {
        seq_printf(m, 
        "TAS Op Start Time Upper   = %ld, Middle=%ld, Lower=%ld   \n", 
        portreg_read(ndev, RSWITCH_TOCST0),
        portreg_read(ndev, RSWITCH_TOCST1),
        portreg_read(ndev, RSWITCH_TOCST2));
        seq_printf(m, 
        "TAS Op Last Int Time Upper   = %ld, Middle=%ld, Lower=%ld   \n", 
        portreg_read(ndev, RSWITCH_TOLIT0),
        portreg_read(ndev, RSWITCH_TOLIT1),
        portreg_read(ndev, RSWITCH_TOLIT2));
        skip_first_entry = portreg_read(ndev, RSWITCH_TOSFE);
        for(i = 0; i < 8; i++)
        {
            if(i < 4)
            {
                entq[i]=((portreg_read(ndev, RSWITCH_TOEN0))>>(i*8) & 0xFF);

                

            }
            else
            {
                entq[i]=((portreg_read(ndev, RSWITCH_TOEN1))>>((i%4)*8) & 0xFF);



            }
            
            for(j = 0; j < entq[i]; j++)
            {
                
                portreg_write(ndev, ((entr_num & 0xFF) | (1<<31)) , RSWITCH_TCLR0);
                portreg_write(ndev, ( (1<<31)) , RSWITCH_TCLR1);
                TCLR2 =  portreg_read(ndev, RSWITCH_TCLR2);
                seq_printf(m, 
                "Q%d   entry%d SkipFirstEntry=%d GateState=%s TimeInterval=%d       \n", i,j,((skip_first_entry >> i) & 0x01),
                (TCLR2 >> 28)?"Open":"Closed", TCLR2 & 0xFFFFFFF);
                entr_num++;

            }



        }
    }
    seq_printf(m, 
    "***********CBS********** \n");
    COCC = portreg_read(ndev, RSWITCH_COCC);
    for(i = 0; i < 8; i++)
    {
        CIV = portreg_read(ndev, (RSWITCH_COIV0 + i));
        CUL = portreg_read(ndev, (RSWITCH_COUL0 + i));
        seq_printf(m," Queue%d CBSEnable=%s  OperCIV=%x OperCUL=%x \n", i, (((COCC >> i) & 0x01)?"Yes":"No"), CIV, CUL);

    }
    return 0;
}

/**
    @brief GW Queue Error Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_GWQueueError_Show(struct seq_file * m, void * v)
{
    int                              queue = (uintptr_t)m->private;
    seq_printf(m, "E frame WaterMark Critical Level = %d \n", rswitch_errors.gw_Error.E_frame_Wmark_critical_level[queue]);
    seq_printf(m, "P frame WaterMark Critical Level = %d \n", rswitch_errors.gw_Error.P_frame_Wmark_critical_level[queue]);
    seq_printf(m, "E frame Overflow                 = %d \n", rswitch_errors.gw_Error.E_frame_overflow[queue]);
    seq_printf(m, "P frame Overflow                 = %d \n", rswitch_errors.gw_Error.P_frame_overflow[queue]);
    return 0;
}


/**
    @brief TX Desc Error Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_TXDescError_Show(struct seq_file * m, void * v)
{
    int                              desc = (uintptr_t)m->private;
    seq_printf(m, "TX Descriptor Config Error       = %d \n", rswitch_errors.gw_Error.AXI_error.tx_desc_config_error[desc]);
    
    

    return 0;
}


/**
    @brief Port RX Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_RXDescError_Show(struct seq_file * m, void * v)
{
    int                              desc = (uintptr_t)m->private;
    seq_printf(m, "RX Descriptor frame Size Error   = %d \n", rswitch_errors.gw_Error.AXI_error.rx_desc_frame_sz_error[desc]);
    seq_printf(m, "RX Descriptor Full Error         = %d \n", rswitch_errors.gw_Error.AXI_error.rx_desc_full_error[desc]);
    seq_printf(m, "RX Descriptor Config Error       = %d \n", rswitch_errors.gw_Error.AXI_error.rx_desc_frame_cfg_error[desc]);
    return 0;
}


/**
    @brief Port TS Desc Error Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_TSDescError_Show(struct seq_file * m, void * v)
{
    int                              desc = (uintptr_t)m->private;
    seq_printf(m, "TS Descriptor Full Error         = %d \n", rswitch_errors.gw_Error.AXI_error.ts_desc_full_error[desc]);
    seq_printf(m, "TX Descriptor Config Error       = %d \n", rswitch_errors.gw_Error.AXI_error.ts_desc_config_error[desc]);
    return 0;
}


/**
    @brief Port RX Inc Desc Error Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_RXIncDescError_Show(struct seq_file * m, void * v)
{
    int                              desc = (uintptr_t)m->private;
    seq_printf(m, "RX Descriptor INC Area overflow Error       = %d \n", rswitch_errors.gw_Error.AXI_error. rx_desc_incarea_overflow_error[desc]);
    return 0;
}


/**
    @brief Port RX Show Proc Function

    @param  seq_file *

    @param  void * 

    @return int

*/
static int rswitch_PortRx_Show(struct seq_file * m, void * v)
{
    int                              port = (uintptr_t)m->private;
    struct net_device * ndev = ppndev[port];
    static uint32_t eframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t pframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t ebyte[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t pbyte[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t mframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t bframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t vframe[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t phyerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t nibbleerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t crcerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t fragmisserror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t cfragerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t fragerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t rmacfiltererror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t rxfiltererror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t undersizeerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t oversizeerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    static uint32_t bufferoverflowerror[CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1] = {0,0,0,0,0,0,0};
    eframe[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RGFCE);
    pframe[port]  += portreg_read(ndev, RSWITCH_RMAC_SYS_RGFCP);
    ebyte[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RXBCE);
    pbyte[port]  += portreg_read(ndev, RSWITCH_RMAC_SYS_RXBCP);
    bframe[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RBFC);
    mframe[port]  += portreg_read(ndev, RSWITCH_RMAC_SYS_RMFC);
    vframe[port]  += portreg_read(ndev, RSWITCH_RMAC_SYS_RVFC);
    phyerror[port]  += portreg_read(ndev, RSWITCH_RMAC_SYS_RPEFC);
    nibbleerror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RNEFC);
    crcerror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RFMEFC);
    fragmisserror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RFFMEFC);
    cfragerror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RCFCEFC);
    fragerror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RFCEFC);
    rmacfiltererror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RRCFEFC);
    rxfiltererror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RRXFEFC);
    undersizeerror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RUEFC);
    oversizeerror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_ROEFC);
    bufferoverflowerror[port] += portreg_read(ndev, RSWITCH_RMAC_SYS_RBOEC);
    seq_printf(m, 
    "E frames   = %d       \n", eframe[port]);
    seq_printf(m, 
    "P frames   = %d       \n", pframe[port]);
    seq_printf(m, 
    "Broadcast frames   = %d       \n", bframe[port]);
    seq_printf(m, 
    "Multi frames   = %d       \n", mframe[port]);
    seq_printf(m, 
    "VLAN frames    = %d      \n", vframe[port]);
    seq_printf(m, 
    "E Bytes   = %d       \n", ebyte[port]);
    seq_printf(m, 
    "P Bytes   = %d       \n", pbyte[port]);
    seq_printf(m, 
    "Phy Error   = %d       \n", phyerror[port]);
    seq_printf(m, 
    "Nibble Error   = %d       \n", nibbleerror[port]);
    seq_printf(m, 
    "CRC Error   = %d       \n", crcerror[port]);
    seq_printf(m, 
    "Frag Miss Error   = %d       \n", fragmisserror[port]);
    seq_printf(m, 
    "C Frag Error   = %d       \n", cfragerror[port]);
    seq_printf(m, 
    "Frag Error   = %d       \n", fragerror[port]);
    seq_printf(m, 
    "RMAC Filter Error   = %d       \n", rmacfiltererror[port]);
    seq_printf(m, 
    "RX Filter Error   = %d       \n", rxfiltererror[port]);
    seq_printf(m, 
    "Undersize Error   = %d       \n", undersizeerror[port]);
    seq_printf(m, 
    "Oversize Error   = %d       \n", oversizeerror[port]);
    seq_printf(m, 
    "Buffer Overflow Error   = %d       \n", bufferoverflowerror[port]);
    return 0;
}





/**
    @brief Open Driver Status Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_eth_DriverStatus_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitch_eth_DriverStatus_Show, NULL);
}

/**
    @brief Open GW Error Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_gw_error_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitch_gw_error_Show, NULL);
}

/**
    @brief Open AXI Error Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_axi_error_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitch_axi_error_Show, NULL);
}


/**
    @brief Open Port RX Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_PortRx_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_PortRx_Show, (void *)p);
}


/**
    @brief Open GW Queue Error Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_GWQueueError_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_GWQueueError_Show, (void *)p);
}

/**
    @brief Open TX Desc Error Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_TXDescError_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_TXDescError_Show, (void *)p);
}


/**
    @brief Open RX Desc Error Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_RXDescError_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_RXDescError_Show, (void *)p);
}


/**
    @brief Open TS Desc Error Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_TSDescError_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_TSDescError_Show, (void *)p);
}


/**
    @brief Open RX Desc Error Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_RXIncDescError_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_RXIncDescError_Show, (void *)p);
}

/**
    @brief Open Port Monitor Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_PortMonitor_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_PortMonitor_Show, (void *)p);
}

/**
    @brief Open Port Tx Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_PortTx_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_PortTx_Show, (void *)p);
}


/**
    @brief Open Port Error Proc Directory

    @param  inode *

    @param  file *  

    @return int

*/
static int rswitch_PortErr_Open(struct inode * inode, struct file * file) 
{
    uintptr_t p = (uintptr_t)PDE_DATA(file_inode(file));

    return single_open(file, rswitch_PortErr_Show, (void *)p);
}







/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_Driver_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitch_eth_DriverStatus_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};


/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_axi_error_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitch_axi_error_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};


/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_gw_error_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitch_gw_error_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_PortRx_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_PortRx_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};


/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_txdesc_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_TXDescError_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_rxdesc_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_RXDescError_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_rxincdesc_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_RXIncDescError_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};


/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_tsdesc_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_TSDescError_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};




/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_gwqueue_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_GWQueueError_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};
/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_PortMonitor_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_PortMonitor_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};


/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_PortTx_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_PortTx_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};



/**
    @brief  Proc File Operation

*/
static const struct file_operations     rswitch_eth_PortErr_Fops =
{
    .owner   = THIS_MODULE,
    .open    = rswitch_PortErr_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};


/**
    @brief  Create Proc Directories

    @param  void   

    @return void

*/
static void rswitch_eth_Create_Proc_Entry(void)
{
    uintptr_t     p = 0;
    char    PortNumber[256];
    char    DescNum[8];
    char    QueueNumber[8];
    /* 
        create root & sub-directories 
    */
    memset(&rswitch_eth_ProcDir, 0, sizeof(rswitch_eth_ProcDir));

    rswitch_eth_ProcDir.RootDir = root_debug_dir;
    
    
    proc_create(RSWITCH_ETH_PROC_FILE_DRIVER, 0, rswitch_eth_ProcDir.RootDir, &rswitch_eth_Driver_Fops);
    

    
    
    
    
    rswitch_eth_ProcDir.PortsDir = proc_mkdir(RSWITCH_ETH_PROC_DIR_PORTS, rswitch_eth_ProcDir.RootDir);
    rswitch_eth_ProcDir.gwDir = proc_mkdir(RSWITCH_GW_PROC_DIR, rswitch_eth_ProcDir.RootDir);
    proc_create(RSWITCH_GW_PROC_ERROR, 0, rswitch_eth_ProcDir.gwDir, &rswitch_gw_error_Fops);
    rswitch_eth_ProcDir.axiDir = proc_mkdir(RSWITCH_GW_PROC_AXI, rswitch_eth_ProcDir.gwDir);
    proc_create(RSWITCH_GW_PROC_AXI_ERROR, 0, rswitch_eth_ProcDir.axiDir, &rswitch_axi_error_Fops);
    rswitch_eth_ProcDir.TXDescsDir = proc_mkdir(RSWITCH_GW_PROC_AXI_DIR_TXDESCS, rswitch_eth_ProcDir.axiDir);
    rswitch_eth_ProcDir.RXDescsDir = proc_mkdir(RSWITCH_GW_PROC_AXI_DIR_RXDESCS, rswitch_eth_ProcDir.axiDir);
    rswitch_eth_ProcDir.TSDescsDir = proc_mkdir(RSWITCH_GW_PROC_AXI_DIR_TSDESCS, rswitch_eth_ProcDir.axiDir);
    rswitch_eth_ProcDir.RXincDescsDir = proc_mkdir(RSWITCH_GW_PROC_AXI_DIR_RXINCDESC, rswitch_eth_ProcDir.axiDir);
    rswitch_eth_ProcDir.QueuesDir = proc_mkdir(RSWITCH_GW_PROC_DIR_QUEUES, rswitch_eth_ProcDir.gwDir);
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.PortDir)/sizeof(rswitch_eth_ProcDir.PortDir[0]); p++)
    {
        
        sprintf(PortNumber, "%lu", p);
        if(p < ((sizeof(rswitch_eth_ProcDir.PortDir)/sizeof(rswitch_eth_ProcDir.PortDir[0])) -1 ))
        {
            rswitch_eth_ProcDir.PortDir[p] = proc_mkdir(PortNumber, rswitch_eth_ProcDir.PortsDir);
            
            proc_create_data(RSWITCH_ETH_PROC_FILE_PORTS_PORT_RX,
            0, rswitch_eth_ProcDir.PortDir[p], &rswitch_eth_PortRx_Fops, (void *)p);
            proc_create_data(RSWITCH_ETH_PROC_FILE_PORTS_PORT_TX,
            0, rswitch_eth_ProcDir.PortDir[p], &rswitch_eth_PortTx_Fops, (void *)p);
            proc_create_data(RSWITCH_ETH_PROC_FILE_PORTS_PORT_Err,
            0, rswitch_eth_ProcDir.PortDir[p], &rswitch_eth_PortErr_Fops, (void *)p);
            proc_create_data(RSWITCH_ETH_PROC_FILE_PORTS_PORT_Monitor,
            0, rswitch_eth_ProcDir.PortDir[p], &rswitch_eth_PortMonitor_Fops, (void *)p);
            
        }

        
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.QueueDir)/sizeof(rswitch_eth_ProcDir.QueueDir[0]); p++)
    {
        
        sprintf(QueueNumber, "%lu", p);
        if(p < ((sizeof(rswitch_eth_ProcDir.QueueDir)/sizeof(rswitch_eth_ProcDir.QueueDir[0])) -1 ))
        {
            rswitch_eth_ProcDir.QueueDir[p] = proc_mkdir(QueueNumber, rswitch_eth_ProcDir.QueuesDir);
            
            proc_create_data(RSWITCH_ETH_PROC_FILE_GW_QUEUES_QUEUE_ERROR,
            0, rswitch_eth_ProcDir.QueueDir[p], &rswitch_eth_gwqueue_Fops, (void *)p);
            
            
        }

        
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.TXDescDir)/sizeof(rswitch_eth_ProcDir.TXDescDir[0]); p++)
    {
        
        sprintf(DescNum, "%lu", p);
        if(p < ((sizeof(rswitch_eth_ProcDir.TXDescDir)/sizeof(rswitch_eth_ProcDir.TXDescDir[0])) -1 ))
        {
            rswitch_eth_ProcDir.TXDescDir[p] = proc_mkdir(DescNum, rswitch_eth_ProcDir.TXDescsDir);
            
            proc_create_data(RSWITCH_ETH_PROC_FILE_AXI_TXDESC_ERROR,
            0, rswitch_eth_ProcDir.TXDescDir[p], &rswitch_eth_txdesc_Fops, (void *)p);
            
            
        }

        
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.RXDescDir)/sizeof(rswitch_eth_ProcDir.RXDescDir[0]); p++)
    {
        
        sprintf(DescNum, "%lu", p);
        if(p < ((sizeof(rswitch_eth_ProcDir.RXDescDir)/sizeof(rswitch_eth_ProcDir.RXDescDir[0])) -1 ))
        {
            rswitch_eth_ProcDir.RXDescDir[p] = proc_mkdir(DescNum, rswitch_eth_ProcDir.RXDescsDir);
            
            proc_create_data(RSWITCH_ETH_PROC_FILE_AXI_RXDESC_ERROR,
            0, rswitch_eth_ProcDir.RXDescDir[p], &rswitch_eth_rxdesc_Fops, (void *)p);
            
            
        }

        
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.TSDescDir)/sizeof(rswitch_eth_ProcDir.TSDescDir[0]); p++)
    {
        
        sprintf(DescNum, "%lu", p);
        if(p < ((sizeof(rswitch_eth_ProcDir.TSDescDir)/sizeof(rswitch_eth_ProcDir.TSDescDir[0])) -1 ))
        {
            rswitch_eth_ProcDir.TSDescDir[p] = proc_mkdir(DescNum, rswitch_eth_ProcDir.TSDescsDir);
            
            proc_create_data(RSWITCH_ETH_PROC_FILE_AXI_TSDESC_ERROR,
            0, rswitch_eth_ProcDir.TSDescDir[p], &rswitch_eth_tsdesc_Fops, (void *)p);
            
            
        }

        
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.RXincDescDir)/sizeof(rswitch_eth_ProcDir.RXincDescDir[0]); p++)
    {
        
        sprintf(DescNum, "%lu", p);
        if(p < ((sizeof(rswitch_eth_ProcDir.RXincDescDir)/sizeof(rswitch_eth_ProcDir.RXincDescDir[0])) -1 ))
        {
            rswitch_eth_ProcDir.RXincDescDir[p] = proc_mkdir(DescNum, rswitch_eth_ProcDir.RXincDescsDir);
            
            proc_create_data(RSWITCH_ETH_PROC_FILE_AXI_RXINCDESC_ERROR,
            0, rswitch_eth_ProcDir.RXincDescDir[p], &rswitch_eth_rxincdesc_Fops, (void *)p);
            
            
        }

        
        
        
    }





    
    
    
}



/**
    @brief  Remove Proc Directories

    @param  void   

    @return void

*/
static void rswitch_eth_Remove_Proc_Entry(void)
{
    int     p = 0;
    char    PortNumber[256];

    char QueueNumber[64];
    
    char DescNum[64];
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.PortDir)/sizeof(rswitch_eth_ProcDir.PortDir[0]); p++)
    {
        if(p < ((sizeof(rswitch_eth_ProcDir.PortDir)/sizeof(rswitch_eth_ProcDir.PortDir[0])) -1 ))
        {
            
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_PORTS_PORT_RX, rswitch_eth_ProcDir.PortDir[p]);
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_PORTS_PORT_TX, rswitch_eth_ProcDir.PortDir[p]);
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_PORTS_PORT_Err, rswitch_eth_ProcDir.PortDir[p]);
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_PORTS_PORT_Monitor, rswitch_eth_ProcDir.PortDir[p]);

            
            sprintf(PortNumber, "%u", p);
            remove_proc_entry(PortNumber, rswitch_eth_ProcDir.PortsDir);
        }
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.QueueDir)/sizeof(rswitch_eth_ProcDir.QueueDir[0]); p++)
    {
        if(p < ((sizeof(rswitch_eth_ProcDir.QueueDir)/sizeof(rswitch_eth_ProcDir.QueueDir[0])) -1 ))
        {
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_GW_QUEUES_QUEUE_ERROR, rswitch_eth_ProcDir.QueueDir[p]);
            
            

            sprintf(QueueNumber, "%u", p);
            remove_proc_entry(QueueNumber, rswitch_eth_ProcDir.QueuesDir);
        }
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.TXDescDir)/sizeof(rswitch_eth_ProcDir.TXDescDir[0]); p++)
    {
        
        
        if(p < ((sizeof(rswitch_eth_ProcDir.TXDescDir)/sizeof(rswitch_eth_ProcDir.TXDescDir[0])) -1 ))
        {
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_AXI_TXDESC_ERROR, rswitch_eth_ProcDir.TXDescDir[p]);
            sprintf(DescNum, "%u", p);
            remove_proc_entry(DescNum, rswitch_eth_ProcDir.TXDescsDir);
            
            
            
            
        }

        
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.RXDescDir)/sizeof(rswitch_eth_ProcDir.RXDescDir[0]); p++)
    {
        
        
        if(p < ((sizeof(rswitch_eth_ProcDir.RXDescDir)/sizeof(rswitch_eth_ProcDir.RXDescDir[0])) -1 ))
        {
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_AXI_RXDESC_ERROR, rswitch_eth_ProcDir.RXDescDir[p]);
            sprintf(DescNum, "%u", p);
            remove_proc_entry(DescNum, rswitch_eth_ProcDir.RXDescsDir);
            
            
            
        }

        
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.TSDescDir)/sizeof(rswitch_eth_ProcDir.TSDescDir[0]); p++)
    {
        
        
        if(p < ((sizeof(rswitch_eth_ProcDir.TSDescDir)/sizeof(rswitch_eth_ProcDir.TSDescDir[0])) -1 ))
        {
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_AXI_TSDESC_ERROR, rswitch_eth_ProcDir.TSDescDir[p]);
            sprintf(DescNum, "%u", p);
            remove_proc_entry(DescNum, rswitch_eth_ProcDir.TSDescsDir);
            
            
            
        }

        
        
        
    }
    for (p = 0; p  < sizeof(rswitch_eth_ProcDir.RXincDescDir)/sizeof(rswitch_eth_ProcDir.RXincDescDir[0]); p++)
    {
        
        sprintf(DescNum, "%u", p);
        if(p < ((sizeof(rswitch_eth_ProcDir.RXincDescDir)/sizeof(rswitch_eth_ProcDir.RXincDescDir[0])) -1 ))
        {
            remove_proc_entry(RSWITCH_ETH_PROC_FILE_AXI_RXINCDESC_ERROR, rswitch_eth_ProcDir.RXincDescDir[p]);
            sprintf(DescNum, "%u", p);
            remove_proc_entry(DescNum, rswitch_eth_ProcDir.RXincDescsDir);
            
            
            
        }

        
        
        
    }
    remove_proc_entry(RSWITCH_GW_PROC_DIR_QUEUES, rswitch_eth_ProcDir.gwDir);
    remove_proc_entry(RSWITCH_GW_PROC_AXI_DIR_TXDESCS, rswitch_eth_ProcDir.axiDir);
    remove_proc_entry(RSWITCH_GW_PROC_AXI_DIR_RXDESCS, rswitch_eth_ProcDir.axiDir);
    remove_proc_entry(RSWITCH_GW_PROC_AXI_DIR_TSDESCS, rswitch_eth_ProcDir.axiDir);
    remove_proc_entry(RSWITCH_GW_PROC_AXI_DIR_RXINCDESC, rswitch_eth_ProcDir.axiDir);
    remove_proc_entry(RSWITCH_GW_PROC_ERROR, rswitch_eth_ProcDir.gwDir);
    remove_proc_entry(RSWITCH_GW_PROC_AXI_ERROR, rswitch_eth_ProcDir.axiDir);
    remove_proc_entry(RSWITCH_GW_PROC_AXI, rswitch_eth_ProcDir.gwDir);
    remove_proc_entry(RSWITCH_GW_PROC_DIR, rswitch_eth_ProcDir.RootDir);
    

    remove_proc_entry(RSWITCH_ETH_PROC_DIR_PORTS, rswitch_eth_ProcDir.RootDir);

    remove_proc_entry(RSWITCH_ETH_PROC_FILE_DRIVER, rswitch_eth_ProcDir.RootDir);

}




/**
    @brief  Remove Proc Entries

    @param  void   

    @return void

*/
void rswitch_eth_proc_Terminate(void)
{
    rswitch_eth_Remove_Proc_Entry();  
}



/**
    @brief  Create Proc Entries

    @param  void   

    @return void

*/
void rswitch_eth_proc_Start(void)
{
    rswitch_eth_Create_Proc_Entry();

}



/**
    @brief  Write a 32-bit I/O register

    Port Base 0..n  0x1000, 0x1800, 0x2000, 0x2800, ...
    Port Registers  0x0000 - 0x07FF (relative to the start of each port)
*/

static void port_SetState(struct net_device * ndev, enum RSWITCH_PortState State)
{
    struct port_private * mdp = netdev_priv(ndev);

    if (mdp->PortState == State)
    {
        return;
    }

    mdp->PortState = State;
}

/**
    @brief  Wait for selected Port register to confirm status

    @param  ndev    net_device for the individual switch port device 
    @param  reg     register to check
    @param  mask    
    @param  status  register status within mask to wait for

    @return -ETIMEDOUT on timeout, or 0 on success
*/
static int port_WaitStatus(struct net_device * ndev, u32 reg, u32 mask, u32 status)
{
    int     i = 0;
    int     ret = 0;
    int     value = 0;

    for (i = 0; i < RSWITCH_PORT_CONFIG_TIMEOUT_MS; i++) 
    {
        value = portreg_read(ndev, reg);
        
        if ((value & mask) == status)
        {
            return ret;
        }
        mdelay(1);
    }

    printk("++ %s port_Wait_Status TIMEOUT. Reg(%x) Mask(%08X) Wait(%08X) Last(%08X) \n",
    ndev->name, port_RegOffsets[reg], mask, status, value);

    return -ETIMEDOUT;
}


/**
    @brief  RSWITCH Change Port State Function

    @param  ndev    net_device for the port device     

    @param  NewState

    @return < 0, or 0 on success

*/
extern int port_ChangeState(struct net_device * ndev, enum RSWITCH_PortState NewState)
{
    int    ret = 0;

    switch (NewState)
    {
    case RSWITCH_PortState_StartReset:
        break;
    case RSWITCH_PortState_Reset:
        portreg_write(ndev, ETH_TSN_EAMC_OPC_RESET, RSWITCH_EAMC);
        
        ret = port_WaitStatus(ndev, RSWITCH_EAMS, ETH_TSN_EAMS_OPS_MASK, ETH_TSN_EAMS_OPS_RESET);
        break;
    case RSWITCH_PortState_StartConfig:
        break;
    case RSWITCH_PortState_Config:
        
        portreg_write(ndev, (portreg_read(ndev, RSWITCH_EAMC) & ~ETH_TSN_EAMC_OPC) |ETH_TSN_EAMC_OPC_CONFIG, RSWITCH_EAMC);
        
        ret = port_WaitStatus(ndev, RSWITCH_EAMS, ETH_TSN_EAMS_OPS_MASK, ETH_TSN_EAMS_OPS_CONFIG);
        break;
    case RSWITCH_PortState_StartOperate:
        break;
    case RSWITCH_PortState_Operate:
        portreg_write(ndev, (portreg_read(ndev, RSWITCH_EAMC) & ~ETH_TSN_EAMC_OPC) | ETH_TSN_EAMC_OPC_OPERATION, RSWITCH_EAMC);
        
        ret = port_WaitStatus(ndev, RSWITCH_EAMS, ETH_TSN_EAMS_OPS_MASK, ETH_TSN_EAMS_OPS_OPERATION);
        break;
    case RSWITCH_PortState_Disable:
        ret=portreg_read(ndev, RSWITCH_EAMC) & ~ETH_TSN_EAMC_OPC;
        printk("Value of ret is %x \n", ret); 
        portreg_write(ndev,  ETH_TSN_EAMC_OPC_DISABLE, RSWITCH_EAMC);
        
        ret = port_WaitStatus(ndev, RSWITCH_EAMS, ETH_TSN_EAMS_OPS_MASK, ETH_TSN_EAMS_OPS_DISABLE);
        break;
    default:
        pr_err("[RSWITCH-ETH] %s Invalid state(%u)\n", ndev->name, (u32)NewState);
        return -EINVAL;
    }
    if (ret < 0)
    {
        pr_err("[RSWITCH-ETH] %s Timeout waiting for state(%s)\n", ndev->name, port_QueryStateText(NewState));
        port_SetState(ndev, RSWITCH_PortState_Failed);
    }
    else
    {
        port_SetState(ndev, NewState);
    }

    return ret;
}



/**
    @brief  Free TS Ring Buffers

    @param  void

    @return < 0, or 0 on success

*/ 
static int rswitch_ts_ring_free(void)
{
    
    struct rswitch_ext_ts_desc *ts_desc;
    int ts_ring_size = sizeof(*ts_desc) * num_ts_ring;
    if (ts_ring) {
        
        
        dma_free_coherent(&pcidev->dev, ts_ring_size, ts_ring,
        ts_desc_dma);
        ts_ring = NULL;
    }    


    return 0;

}


/**
    @brief  Format TS Ring Buffers

    @param  void

    @return void

*/
static void rswitch_ts_ring_format( void)
{
    struct rswitch_ext_ts_desc *ts_desc;
    
    
    
    int ts_ring_size = sizeof(*ts_desc) * num_ts_ring;
    uint32_t addr;
    
    int i;
    cur_ts = 0;
    
    dirty_ts = 0;
    
    

    /* Check if multiplication required */
    memset(ts_ring, 0, ts_ring_size);
    
    for (i = 0; i < num_ts_ring; i++) {
        
        /* TS descriptor */
        ts_desc = &ts_ring[i];
        
        ts_desc->info_ds = cpu_to_le16(PKT_BUF_SZ); // Needs to be seen if you need zero byte descriptor for timestamp
        
        ts_desc->die_dt = DT_FEMPTY_ND;
        
    }
    
    ts_desc = &ts_ring[i];
    ts_desc->dptr = cpu_to_le32((u32)ts_desc_dma);
    ts_desc->die_dt = DT_LINK; /* type */



    
    
    ts_desc_bat->die_dt = DT_LINK; /* type */
    ts_desc_bat->dptr = cpu_to_le32((u32)ts_desc_dma);
    
    eth_gwca_reg_write(0x5 ,RSWITCH_AXI_TSS0 );
    addr = ts_desc_bat_dma;
    
    eth_gwca_reg_write( addr, RSWITCH_AXI_TSA0);
    
    
    
}


/**
    @brief   RX Ring Buffers Free

    @param  q

    @return < 0, or 0 on success

*/
static int rswitch_rx_ring_free( int q)
{
    
    int i = 0;
    int ring_size = 0;
    if (rx_ring[q]) {

        for (i = 0; i < num_rx_ring[q] -1; i++) {
            struct rswitch_ext_ts_desc *desc = &rx_ring[q][i];

            if (!dma_mapping_error(&pcidev->dev,
                        le32_to_cpu(desc->dptr)))
            dma_unmap_single(&pcidev->dev,
            le32_to_cpu(desc->dptr),
            PKT_BUF_SZ,
            DMA_FROM_DEVICE);

        }
        
        ring_size = sizeof(struct rswitch_ext_ts_desc) *
        (num_rx_ring[q]);
        dma_free_coherent(&pcidev->dev, ring_size, rx_ring[q],
        rx_desc_dma[q]);
        
        rx_ring[q] = NULL;
    }

    /* Free RX skb ringbuffer */
    if (rx_skb[q]) {
        for (i = 0; i < num_rx_ring[q] -1; i++)
        
        dev_kfree_skb(rx_skb[q][i]);
    }
    
    kfree(rx_skb[q]);
   
    rx_skb[q] = NULL;


    return 0;
}

/**
    @brief  Format RX Ring Buffers

    @param  q

    @return < 0, or 0 on success

*/
static int rswitch_rx_ring_format( int q)
{
    struct rswitch_ext_ts_desc *rx_desc;
    
    uint32_t  RATLR  = 0;
    struct rswitch_ext_ts_desc *rx_ts_desc;
    int rx_ring_size = sizeof(*rx_desc) * num_rx_ring[q];
    uint32_t addr;
    dma_addr_t dma_addr;
    int i,k;
    
    uint32_t desc_learn = 0;
    dirty_rx[q] = 0;
    cur_rx[q] = 0;
    
    
    
    memset(rx_ring[q], 0, rx_ring_size);
    for (i = 0; i < num_rx_ring[q]; i++) {
        
        /* RX descriptor */
        rx_desc = &rx_ring[q][i];
        
        rx_desc->info_ds = cpu_to_le16(PKT_BUF_SZ);
    

        dma_addr = dma_map_single(&pcidev->dev,(void*)rx_skb[q][i]->data ,
        PKT_BUF_SZ,
        DMA_FROM_DEVICE);
        

        
        /* We just set the data size to 0 for a failed mapping which
        * should prevent DMA from happening...
        */
        if (dma_mapping_error(&pcidev->dev, dma_addr))
        {
            printk("Descriptor Mapping error \n");
            rx_desc->info_ds = cpu_to_le16(0);
        }
        rx_desc->dptr = cpu_to_le32(dma_addr);
        rx_desc->die_dt = DT_FEMPTY;
        
    }
    
    rx_desc = &rx_ring[q][i];
    rx_desc->dptr = cpu_to_le32((u32)rx_desc_dma[q]);
    rx_desc->die_dt = DT_LINK; /* type */
    rx_ts_desc = rx_desc_bat[q];
    
    rx_ts_desc->die_dt = DT_LINK; /* type */
    rx_ts_desc->dptr = cpu_to_le32((u32)rx_desc_dma[q]);
    desc_learn = 0;
    eth_gwca_reg_write((0x0C | ( q << 24)) ,RSWITCH_AXI_RATLS0 );
    addr = rx_desc_bat_dma[q];
    
    eth_gwca_reg_write( addr, RSWITCH_AXI_RATLS1);
    /* Kick Descriptor learning */
    RATLR = eth_gwca_reg_read( RSWITCH_AXI_RATLR);
    eth_gwca_reg_write((RATLR | 0x80000000),RSWITCH_AXI_RATLR );
    RATLR = eth_gwca_reg_read( RSWITCH_AXI_RATLR);
    for (k = 0; k < RSWITCH_DESCLEARN_CONFIG_TIMEOUT_MS; k++) 
    {
        RATLR = eth_gwca_reg_read( RSWITCH_AXI_RATLR);
        
        if((RATLR & 0x80000000) == 0x0)
        {
            desc_learn = 1;
            break;

        }
        mdelay(1);

    } 
    if(desc_learn  == 0)
    {
        printk("Descriptor Learning Error  \n");
        return -1;
    }
    desc_learn = 0;
    return 0;
}



/**
    @brief  Free Tx Ring Buffers

    @param  ndev    net_device for the port device     

    @param  q

    @return < 0, or 0 on success

*/
static int rswitch_tx_ring_free(struct net_device *ndev, int q)
{
    struct port_private *priv = netdev_priv(ndev);
    int ring_size = 0;
    if (priv->tx_ring[q]) {
        rswitch_tx_free(ndev, q, false);

        ring_size = sizeof(struct rswitch_ext_desc) *
        (priv->num_tx_ring[q] * NUM_TX_DESC);
        priv->tx_ring[q] = NULL;
    }

    /* Free aligned TX buffers */
    kfree(priv->tx_align[q]);
    priv->tx_align[q] = NULL;

    /* Free TX skb ringbuffer.
    * SKBs are freed by rswitch_tx_free() call above.
    */
    kfree(priv->tx_skb[q]);
    priv->tx_skb[q] = NULL;
    return 0;

}



/**
    @brief  Format Ring Buffers

    @param  ndev    net_device for the port device     

    @param  q

    @return < 0, or 0 on success

*/
static int rswitch_ring_format(struct net_device *ndev, int q)
{
    
    uint32_t desc_learn = 0;
    int k  = 0;
    struct port_private *priv = netdev_priv(ndev);
    
    struct rswitch_ext_desc *tx_desc;
    
    uint32_t TATLR  = 0;
    
    
    int tx_ring_size = sizeof(*tx_desc) * priv->num_tx_ring[q] *
    NUM_TX_DESC;
    
    uint32_t addr;
    
    int i;

    
    priv->cur_tx[q] = 0;
    
    priv->dirty_tx[q] = 0;
    
    

    memset(priv->tx_ring[q], 0, tx_ring_size);
    /* Build TX ring buffer */
    for (i = 0, tx_desc = priv->tx_ring[q]; i < priv->num_tx_ring[q];
    i++, tx_desc++) {
        
        tx_desc->die_dt = DT_EEMPTY;
        tx_desc++;
        tx_desc->die_dt = DT_EEMPTY;
        
    }
    tx_desc->dptr = cpu_to_le32(tx_desc_bat_dma[(priv->PortNumber * 2) + q]);
    tx_desc->die_dt = DT_LINK; /* type */
    
    addr = tx_desc_bat_dma[(priv->PortNumber * 2) + q];
    
    
    eth_gwca_reg_write((0x02 | ( ((priv->PortNumber * 2) + q) << 24)) ,RSWITCH_AXI_TATLS0 );
    
    
    eth_gwca_reg_write( addr, RSWITCH_AXI_TATLS1);
    
    /* Kick Descriptor learning */
    TATLR = eth_gwca_reg_read( RSWITCH_AXI_TATLR);
    eth_gwca_reg_write((TATLR | 0x80000000),RSWITCH_AXI_TATLR );
    TATLR = eth_gwca_reg_read( RSWITCH_AXI_TATLR);
    for (k = 0; k < RSWITCH_DESCLEARN_CONFIG_TIMEOUT_MS; k++) 
    {
        TATLR = eth_gwca_reg_read( RSWITCH_AXI_TATLR);
        
        if((TATLR & 0x80000000) == 0x0)
        {
            desc_learn = 1;
            break;

        }
        mdelay(3000);

    } 
    if(desc_learn  == 0)
    {
        printk("Descriptor Learning Error  \n");
        return -1;
    }

    return 0;
    
}







/**
    @brief  Initialises TS Ring Buffers

    @param  void

    @return < 0, or 0 on success

*/
static int rswitch_ts_ring_init(void)
{
    int ts_ring_size = 0;
    ts_ring_size = sizeof(struct rswitch_ext_ts_desc) * (num_ts_ring + 1);
    ts_ring = dma_alloc_coherent(&pcidev->dev, ts_ring_size,
    &ts_desc_dma,
    GFP_KERNEL);
    

    
    dirty_ts = 0;
    
    return 0;
    
}


/**
    @brief  Initialises RX Ring Buffers 

    @param  q

    @return < 0, or 0 on success

*/
static int rswitch_rx_ring_init(int q)
{
    struct sk_buff *skb;
    int ring_size;
    int i;
    rx_skb[q] = kcalloc(num_rx_ring[q],
    sizeof(*rx_skb[q]), GFP_KERNEL);    
    if (!rx_skb[q])
    {
        printk(" SKB Alloc error\n");
        goto error;

    }
    for (i = 0; i < num_rx_ring[q]; i++) {
        skb = dev_alloc_skb(PKT_BUF_SZ + RSWITCH_ALIGN - 1);
        if (!skb)
        goto error;
        
        skb_reserve(skb, NET_IP_ALIGN);
        rx_skb[q][i] = skb;
    }
    ring_size = sizeof(struct rswitch_ext_ts_desc) * (num_rx_ring[q] + 1);
    rx_ring[q] = dma_alloc_coherent(&pcidev->dev, ring_size,
    &rx_desc_dma[q],
    GFP_KERNEL);
    

    if (!rx_ring[q])
    {
        printk("Error \n");
        goto error;

    }
    dirty_rx[q] = 0;
    
    return 0;
error:
    return -ENOMEM;

}


/**
    @brief  Initialises Ring Buffers

    @param  ndev    net_device for the port device     

    @param  q

    @return < 0, or 0 on success

*/
static int rswitch_ring_init(struct net_device *ndev, int q)
{
    struct port_private *priv = netdev_priv(ndev);
    
    int ring_size;
    
    
    
    priv->tx_skb[q] = kcalloc(priv->num_tx_ring[q],
    sizeof(*priv->tx_skb[q]), GFP_KERNEL);
    
    if (!priv->tx_skb[q])
    {
        printk(" SKB Allocation error\n");
        goto error;

    }
    
    

    /* Allocate rings for the aligned buffers */
    priv->tx_align[q] = kmalloc(DPTR_ALIGN * priv->num_tx_ring[q] +
    DPTR_ALIGN - 1, GFP_KERNEL);
    if (!priv->tx_align[q])
    goto error;

    
    
    /* Allocate all TX descriptors. */
    ring_size = sizeof(struct rswitch_ext_desc) *
    (priv->num_tx_ring[q] * NUM_TX_DESC + 1);
    
    
    priv->tx_ring[q]    = tx_desc_bat[(priv->PortNumber * 2) + q];
    if (!priv->tx_ring[q])
    goto error;

    return 0;

error:
    

    return -ENOMEM;
}



/**
    @brief  DMAC initialise

    @param  ndev    net_device for the port device 

    @return < 0, or 0 on success
*/
static int rswitch_dmac_init(struct net_device *ndev)
{
    
    /* Call this function in config mode */
    
    int error;
    
    
    

    error = rswitch_ring_init(ndev, 0);
    error = rswitch_ring_init(ndev, 1);
    /* Descriptor format */
    rswitch_ring_format(ndev, RSWITCH_BE);
    rswitch_ring_format(ndev, RSWITCH_NC);
    


    
    return 0;
}



/**
    @brief  MII bus initialise

    @param  ndev    net_device for the port device 

    @return < 0, or 0 on success

    Connect MAC to PHY for the port.
*/
static int port_MII_Initialise(struct net_device * ndev)
{
    
    u32         MIR = 0;
    u32         EAMS = 0;
    

    if(board_variant == BOARD_VARIANT_H3VC3) {
        MIR = RSWITCH_MIR_RGMII;
    } else {  /* BOARD_VARIANT_H3VC2 */
        MIR = RSWITCH_MIR_MII;
    }
    portreg_write(ndev, MIR, RSWITCH_RMAC_MPIC);
    

    MIR = portreg_read(ndev, RSWITCH_RMAC_MPIC);
    EAMS = portreg_read(ndev, RSWITCH_EAMS);
    

    return 0;
}













// ------------------------------------------------------------------------------------ 
//
//      Port Device Operations
//
// ------------------------------------------------------------------------------------ 


// -------------------------------- [ Port State ] --------------------------------


/**
    @brief  Return the English string for the  Port State

    @param  state         State

    @return Pointer to a static string
*/
extern const char * const port_QueryStateText(enum RSWITCH_PortState state)
{
    switch (state) 
    {
    case RSWITCH_PortState_Unknown:        return "Unknown";
    case RSWITCH_PortState_StartReset:     return "Start Reset";
    case RSWITCH_PortState_Reset:          return "Reset";
    case RSWITCH_PortState_StartConfig:    return "Start Config";
    case RSWITCH_PortState_Config:         return "Config";
    case RSWITCH_PortState_StartOperate:   return "Start Operation";
    case RSWITCH_PortState_Operate:        return "Operation";
    case RSWITCH_PortState_Disable:        return "Disable";
    case RSWITCH_PortState_Failed:         return "Failed";
    default:                              return "INVALID";
    }
}

/**
    @brief  Return the  Port State

    @param  state        Port State

    @return enum
*/
extern enum RSWITCH_PortState port_QueryState(struct net_device * ndev)
{
    struct port_private * mdp = netdev_priv(ndev);

    return mdp->PortState;
}







/**
    @brief  Disable Tx & Rx

    (sh_eth_rcv_snd_disable)

    @param  ndev    net_device for the individual switch port device
*/
static void port_TxRx_Disable(struct net_device * ndev)
{
    portreg_write(ndev, (portreg_read(ndev, RSWITCH_RMAC_MCC) & ~RMAC_RC_RE), RSWITCH_RMAC_MCC);     // RMAC
    portreg_write(ndev, (portreg_read(ndev, RSWITCH_RMAC_MCC) & ~RMAC_TC_TE), RSWITCH_RMAC_MCC);     // RMAC
}



/**
    @brief  Enable Tx & Rx
    
    (sh_eth_rcv_snd_enable)

    @param  ndev    net_device for the individual switch port device
*/
static void port_TxRx_Enable(struct net_device * ndev)
{
    portreg_write(ndev, (portreg_read(ndev, RSWITCH_RMAC_MCC) | RMAC_RC_RE), RSWITCH_RMAC_MCC);     // RMAC
    portreg_write(ndev, (portreg_read(ndev, RSWITCH_RMAC_MCC) | RMAC_TC_TE), RSWITCH_RMAC_MCC);     // RMAC
}

// ------------------------------------[ Transmit ]------------------------------------









/** 
    @brief RSWITCH TX Decriptor Free Function

    @param      ndev

    @param      q
    
    @param      free_txed_only

    @return < 0, or 0 on success
*/
static int rswitch_tx_free(struct net_device *ndev, int q, bool free_txed_only)
{
    
    struct port_private *priv = netdev_priv(ndev);
    
    struct rswitch_ext_desc *desc;
    struct rswitch_ext_desc *linkdesc;
    int free_num = 0;
    int entry;
    int entry_cpy = 0;
    u32 size;
    
    
    for (; priv->cur_tx[q] - priv->dirty_tx[q] > 0; priv->dirty_tx[q]++) {
        bool txed;
        entry = priv->dirty_tx[q] % (priv->num_tx_ring[q] *
        NUM_TX_DESC);
        entry_cpy = entry;
        
        desc = &priv->tx_ring[q][entry];
        
        txed = ((desc->die_dt & 0xF0) | 0x08) == DT_FEMPTY;
        
        if (free_txed_only && !txed)
        {
            
            
            break;


        }
        /* Descriptor type must be checked before all other reads */
        dma_rmb();
        size = le16_to_cpu((desc->info_ds & 0xFFF)) & TX_DS;
        /* Free the original skb. */
        
        if (priv->tx_skb[q][entry / NUM_TX_DESC]) {
            
            dma_unmap_single(&pcidev->dev, le32_to_cpu(desc->dptr),
            size, DMA_TO_DEVICE);
            /* Last packet descriptor? */
            
            if (entry % NUM_TX_DESC == NUM_TX_DESC - 1) {
                entry /= NUM_TX_DESC;
                if(q % 2 == 0)
                dev_kfree_skb_any(priv->tx_skb[q][entry]);
                priv->tx_skb[q][entry] = NULL;
                
            }
            free_num++;
        }
        
        desc->die_dt = DT_EEMPTY;
        
    }
    linkdesc = &priv->tx_ring[q][((priv->num_tx_ring[q] * NUM_TX_DESC))];
    /* Below line because of Descriptor write back of DT_LINK once Link Fix comes can be avoided */
    if(linkdesc->die_dt == 0xC0)
    {
        
        linkdesc->die_dt = DT_LINK;

    }
    
    return free_num;
}


/** 
    @brief RSWITCH Register Modify Function

    @param      ndev

    @param      reg
    
    @param      clear

    @param      set

    @return     void
*/
void rswitch_modify(struct net_device *ndev, uint32_t reg, u32 clear,
u32 set)
{
    eth_gwca_reg_write( (eth_gwca_reg_read( reg) & ~clear) | set, reg);
}

/** 
    @brief  Packet transmit (BE, NC, TCP/UDP)

    @param[in]  skb
    @param[in]  ndev

    @return < 0, or 0 on success
*/
static int port_Start_Xmit(struct sk_buff * skb, struct net_device * ndev)
{


    struct port_private *priv = netdev_priv(ndev);
    
    
    struct net_device_stats * Stats = &priv->Stats;
    u16 q = 0;
    u32 info0 =  0;
    struct rswitch_ext_desc *desc;
    struct rswitch_ext_desc *desc_first;
    unsigned long flags;
    u32 dma_addr;
    void *buffer;
    u32 entry;
    u32 len;
    
    struct rswitch_tstamp_skb *ts_skb;
#ifdef TAS_DEBUG
    if (strcasecmp(run_mode, "TAS") == 0)
    {
        q = RSWITCH_QUEUE_NC ;
    }
#endif
    

    if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) 
    {
        q = RSWITCH_QUEUE_NC ;
    }
    
    spin_lock_irqsave(&priv->lock, flags);
    if (priv->cur_tx[q] - priv->dirty_tx[q] > (priv->num_tx_ring[q] - 1) *
            NUM_TX_DESC) {
        netif_err(priv, tx_queued, ndev,
        "still transmitting with the full ring!\n");
        printk("Error still transmitting with the full ring\n");
        netif_stop_subqueue(ndev, (q % 2));
        spin_unlock_irqrestore(&priv->lock, flags);
        return NETDEV_TX_BUSY;
    }

    if (skb_put_padto(skb, ETH_ZLEN))
    goto exit;
    
    entry = priv->cur_tx[q] % (priv->num_tx_ring[q] * NUM_TX_DESC);
    
    priv->tx_skb[q][entry / NUM_TX_DESC] = skb;
    buffer = PTR_ALIGN(priv->tx_align[q], DPTR_ALIGN) +
    entry / NUM_TX_DESC * DPTR_ALIGN;
    
    len = PTR_ALIGN(skb->data, DPTR_ALIGN) - skb->data;
    /* Zero length DMA descriptors are problematic as they seem to
    * terminate DMA transfers. Avoid them by simply using a length of
    * DPTR_ALIGN (4) when skb data is aligned to DPTR_ALIGN.
    *
    * As skb is guaranteed to have at least ETH_ZLEN (60) bytes of
    * data by the call to skb_put_padto() above this is safe with
    * respect to both the length of the first DMA descriptor (len)
    * overflowing the available data and the length of the second DMA
    * descriptor (skb->len - len) being negative.
    */
    if (len == 0)
    len = DPTR_ALIGN;

    memcpy(buffer, skb->data, len);
    
    dma_addr = dma_map_single(&pcidev->dev, buffer, len, DMA_TO_DEVICE);
    
    if (dma_mapping_error(&pcidev->dev, dma_addr))
    goto drop;

    desc = &priv->tx_ring[q][entry];
    
    desc_first = desc;
    desc->dptr = cpu_to_le32(dma_addr);
    desc->info_ds = cpu_to_le16(len);
#ifdef INTERNAL_GATEWAY_PORT
    if(priv->PortNumber == Board_Config.Eth_Ports)
    {
        info0 = 0x00; // Normal Descriptor
    }
    else
#endif
    {
        info0 = 0x01; // Direct Descriptor
    }
    desc->info_ds =  desc->info_ds | (info0 << 12);
    /* GW Port does not need target to be specified*/
    if(priv->PortNumber == Board_Config.Eth_Ports)
    {
        desc->info1h = 0;
        
    }
    else
    {
        desc->info1h = ((uint32_t)1) << priv->PortNumber; 
    }
    buffer = skb->data + len;
    len = skb->len - len;
    dma_addr = dma_map_single(&pcidev->dev, buffer, len, DMA_TO_DEVICE);
    if (dma_mapping_error(&pcidev->dev, dma_addr))
    goto unmap;

    desc++;
    desc->info_ds = cpu_to_le16(len);
    
    desc->dptr = cpu_to_le32(dma_addr);

#ifdef TX_TS_FEATURE
    /* TX timestamp required */
    if ((q) == RSWITCH_QUEUE_NC) {
        ts_skb = kmalloc(sizeof(*ts_skb), GFP_ATOMIC);
        
        if (!ts_skb) {

            printk("Cannot allocate TS SKB \n");
            desc--;
            dma_unmap_single(&pcidev->dev, dma_addr, len,
            DMA_TO_DEVICE);
            goto unmap;
        }
        ts_skb->skb = skb;
        ts_skb->tag = priv->ts_skb_tag++;
        
        priv->ts_skb_tag &= 0xff;
        if(priv->ts_skb_tag == 0xFF)
        {
            priv->ts_skb_tag = 0x0;
        }
        list_add_tail(&ts_skb->list, &priv->ts_skb_list);
        
        /* TAG and timestamp required flag */
        skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
        
        desc_first->info1l = (ts_skb->tag & 0xFF);// | TX_TSR;
        desc_first->info1l  = desc_first->info1l | (1 << 8);
        
    }
#endif
    skb_tx_timestamp(skb);
    /* Descriptor type must be set after all the above writes */
    dma_wmb();
    if(q == RSWITCH_QUEUE_NC)
    {  
        desc->die_dt = DT_FEND;
        
    }
    else
    {
        desc->die_dt = DT_FEND;

    }
    
    desc--;
    desc->die_dt = DT_FSTART;
    
    rswitch_modify(ndev, RSWITCH_AXI_TRCR0, TRCR_TSRQ0 << (q + (priv->PortNumber*2)) , TRCR_TSRQ0 << (q + (priv->PortNumber*2)));
    priv->cur_tx[q] += NUM_TX_DESC;
    if (priv->cur_tx[q] - priv->dirty_tx[q] >
            (priv->num_tx_ring[q] - 1) * NUM_TX_DESC &&
            !rswitch_tx_free(ndev, q, true))
    { 
        printk("Stopping subqueue \n");
        netif_stop_subqueue(ndev, q);
    }
    Stats->tx_packets++;
    Stats->tx_bytes += len;
exit:
    mmiowb();
    spin_unlock_irqrestore(&priv->lock, flags);
    
    return NETDEV_TX_OK;

unmap:
    dma_unmap_single(&pcidev->dev, le32_to_cpu(desc->dptr),
    le16_to_cpu((desc->info_ds & 0xFFF)), DMA_TO_DEVICE);
drop:
    dev_kfree_skb_any(skb);
    priv->tx_skb[q][entry / NUM_TX_DESC] = NULL;
    goto exit;


    return NETDEV_TX_OK;
}









/**
    @brief  Rswitch TAS Error function

    @param  ndev
    
    @param  EAEIS0

    @return < 0, or 0 on success

*/
static int rswitch_port_error_tas_interrupt(struct net_device   * ndev, u32 EAEIS0)
{
    struct port_private  * mdp = netdev_priv(ndev);
    int i  = 0;
    
    
    
    if(EAEIS0 >> 18)
    {
        /* Increment Counter */
        rswitch_errors.Port_Error[mdp->PortNumber].EAEIS0_error[18].active =1;
    }
    if(EAEIS0 >> 19)
    {
        /* Increment Counter */
        rswitch_errors.Port_Error[mdp->PortNumber].EAEIS0_error[19].active =1;
    }
    if(EAEIS0 >> 20)
    {
        /* Increment Counter */
        rswitch_errors.Port_Error[mdp->PortNumber].EAEIS0_error[20].active =1;
    }
    portreg_write(ndev, 0x1C0000, RSWITCH_EAEIS0);
    portreg_write(ndev, 0x00, RSWITCH_TCC);
    portreg_write(ndev, 0x03, RSWITCH_EAMIS);
    for (i = 0; i < Config_New.Ports; i++)
    {
        if(Config_New.Port[i].PortNumber == mdp->PortNumber) 
        {

            break;
        }
    }                
    Config_New.Port[i].TxParam.TAS.ConfigPending = 0;

    
    return 0;

}


/**
    @brief  Rswitch Ethernet Error function

    @param  ndev

    @return < 0, or 0 on success

*/
static int rswitch_eth_error(struct net_device   * ndev)
{
    
    u32 EAEIS0 = 0;
    u32 EAEIS1 = 0;
    u32 EAEIS2 = 0;
    struct port_private  * mdp = netdev_priv(ndev);
    u32 i  =0;
    EAEIS0 = portreg_read(ndev, RSWITCH_EAEIS0);
    EAEIS1 = portreg_read(ndev, RSWITCH_EAEIS1);
    EAEIS2 = portreg_read(ndev, RSWITCH_EAEIS2);
    
    
    if((EAEIS0 >> 18) || (EAEIS0 >> 19) || (EAEIS0 >> 20)) 
    {

        rswitch_port_error_tas_interrupt(ndev, EAEIS0);

    }
    EAEIS0 =  EAEIS0 & 0x3FFFF;
    
    for(i =0; i < 32; i++)
    {
        if((EAEIS1 >> i) & 0x01)
        {
            rswitch_errors.Port_Error[mdp->PortNumber].EAEIS1_error[i].active=1;
            /* Raise particular error flag Should be indexed as error bit*/ 

        }
        if(i < 18)
        {
            if((EAEIS0 >> i) & 0x01)
            {

                /* Raise particular error flag Should be indexed as error bit*/ 
                rswitch_errors.Port_Error[mdp->PortNumber].EAEIS0_error[i].active=1;
            } 

        }
        if(i < 24)
        {

            if((EAEIS2 >> i) & 0x01)
            {

                /* Raise particular error flag Should be indexed as error bit*/ 
                rswitch_errors.Port_Error[mdp->PortNumber].EAEIS2_error[i].active=1;
            }

        }
    }
    portreg_write(ndev, 0x3FFFF, RSWITCH_EAEIS0);
    portreg_write(ndev, 0xFFFFFFFF, RSWITCH_EAEIS1);
    portreg_write(ndev, 0xFFFFFFFF, RSWITCH_EAEIS2);
    return 0;
}


/**
    @brief  Rswitch AXI BMI Error function

    @param  void

    @return < 0, or 0 on success

*/
static int rswitch_axibmi_error(void)
{
    uint32_t EIS0 = 0;
    uint32_t EIS1 = 0;
    
    uint32_t TCEIS = 0;
    uint32_t TSCEIS = 0;
    uint32_t RFSEIS = 0;
    uint32_t RFEIS = 0;
    uint32_t RCEIS = 0;
    uint32_t RIDAOIS = 0;
    uint32_t TSFEIS = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t q = 0;
    EIS0 = eth_gwca_reg_read((RSWITCH_AXI_EIS0));
    EIS1 = eth_gwca_reg_read((RSWITCH_AXI_EIS1));
    if((EIS0 >> 0) & 0x01)
    {
        rswitch_errors.gw_Error.AXI_error.axi_bus_error =1;

    }
    if((EIS0 >> 1) & 0x01)
    {
        rswitch_errors.gw_Error.AXI_error.ecc_fatal_error =1;


    }
    for(i = 0; i < 2; i++)
    {
        if((EIS0 >> (8 +i)) & 0x01)
        {
            TCEIS = eth_gwca_reg_read((RSWITCH_AXI_TCEIS0 + (3*i)));
            for(j = 0; j < 32; j++)
            {
                if((TCEIS >> j) & 0x01)
                {

                    rswitch_errors.gw_Error.AXI_error.tx_desc_config_error[(i*32) + j] = 1;
                }


            }
            
        }
    }
    for(i = 0; i < 8; i++)
    {
        if((EIS1 >> i) & 0x01)
        {
            RFSEIS = eth_gwca_reg_read((RSWITCH_AXI_RFSEIS0 + (3*i)));
            for(j = 0; j < 32; j++)
            {
                if((RFSEIS >> j) & 0x01)
                {

                    rswitch_errors.gw_Error.AXI_error.rx_desc_frame_sz_error[(i*32) + j] = 1;
                }

            }
            eth_gwca_reg_write(RFSEIS,(RSWITCH_AXI_RFSEIS0 + (3*i)));
        }
        if((EIS1 >> (i+8)) & 0x01)
        {
            

            RFEIS = eth_gwca_reg_read((RSWITCH_AXI_RFEIS0 + (3*i)));
            
            for(j = 0; j < 32; j++)
            {
                if((RFEIS >> j) & 0x01)
                {
                    q = (i*32) + j;
                    printk("RX Desc %d Full Error \n", (i*32) + j);
                    error_flag[q] = 1;
                    rswitch_errors.gw_Error.AXI_error.rx_desc_full_error[(i*32) + j] = 1;
                }
            }
            eth_gwca_reg_write(RFEIS,(RSWITCH_AXI_RFEIS0 + (3*i)));
            for(j = 0; j < 32; j++)
            {
                if(((RFEIS >> j) & 0x01) == 0)
                {

                    rswitch_errors.gw_Error.AXI_error.rx_desc_full_error[(i*32) + j] = 0;
                 
                }

            }

            



        }
        if((EIS1 >> (i+16)) & 0x01)
        {

            RCEIS = eth_gwca_reg_read((RSWITCH_AXI_RCEIS0 + (3*i)));

            for(j = 0; j < 32; j++)
            {
                if((RCEIS >> j) & 0x01)
                {

                    rswitch_errors.gw_Error.AXI_error.rx_desc_frame_cfg_error[(i*32) + j] = 1;
                }

            }
            eth_gwca_reg_write(RCEIS,(RSWITCH_AXI_RCEIS0 + (3*i)));

        }
        
    }
    if((EIS1 >> 24) & 0x01)
    {
        RIDAOIS = eth_gwca_reg_read((RSWITCH_AXI_RIDAOIS));

        for(j = 0; j < 8; j++)
        {
            if((RIDAOIS >> j) & 0x01)
            {

                rswitch_errors.gw_Error.AXI_error.rx_desc_incarea_overflow_error[j] = 1;
            }

        }
        eth_gwca_reg_write(RIDAOIS,(RSWITCH_AXI_RIDAOIS));
        

    }
    if((EIS1 >> 25) & 0x01)
    {
        TSFEIS = eth_gwca_reg_read((RSWITCH_AXI_TSFEIS));

        for(j = 0; j < 8; j++)
        {
            if((TSFEIS >> j) & 0x01)
            {

                rswitch_errors.gw_Error.AXI_error.ts_desc_full_error[j] = 1;
            }

        }
        eth_gwca_reg_write(TSFEIS,(RSWITCH_AXI_TSFEIS));

    }
    if((EIS1 >> 26) & 0x01)
    {
        TSCEIS = eth_gwca_reg_read((RSWITCH_AXI_TSCEIS));

        for(j = 0; j < 8; j++)
        {
            if((TSCEIS >> j) & 0x01)
            {

                rswitch_errors.gw_Error.AXI_error.ts_desc_config_error[j] = 1;
            }

        }
        eth_gwca_reg_write(TSCEIS,(RSWITCH_AXI_TSCEIS));

    }

    eth_gwca_reg_write(EIS0,(RSWITCH_AXI_EIS0));
    eth_gwca_reg_write(EIS1, (RSWITCH_AXI_EIS1));
    return 0;
    
}


/**
    @brief  Rswitch GW error function

    @param  void

    @return < 0, or 0 on success

*/
static int rswitch_gw_error(void)
{
    uint32_t GWEIS0 = 0;
    uint32_t i = 0;
    
    uint32_t GWEIS1 = 0;
    GWEIS0 = eth_gwca_reg_read((RSWITCH_GWCA_GWEIS0));
    
    GWEIS1 = eth_gwca_reg_read((RSWITCH_GWCA_GWEIS1));
    
    if((GWEIS0 >> 0) & 0x01)
    {
        /* Search for particular AXIBMI error */
        rswitch_errors.gw_Error.AXIBMIEIS =1;
        rswitch_axibmi_error();
    }
    if((GWEIS0 >> 1) & 0x01)
    {
        rswitch_errors.gw_Error.trans_ECC_error =1;

    }
    if((GWEIS0 >> 2) & 0x01)
    {
        rswitch_errors.gw_Error.ECC_fatal_error =1;

    }
    for(i = 0; i < 8; i++)
    {
        if((GWEIS1 >> i) & 0x01)
        {
            
            rswitch_errors.gw_Error.E_frame_Wmark_critical_level[i] =1;
            
            
            
        }
        if((GWEIS1 >> (i*1)) & 0x01)
        {
            rswitch_errors.gw_Error.P_frame_Wmark_critical_level[i] =1; 
        }
        if((GWEIS1 >> (i*2)) & 0x01)
        {
            rswitch_errors.gw_Error.E_frame_overflow[i] =1;   
        }
        if((GWEIS1 >> (i*3)) & 0x01)
        {
            rswitch_errors.gw_Error.P_frame_overflow[i] =1;
        }
        
    }
    eth_gwca_reg_write(GWEIS0,(RSWITCH_GWCA_GWEIS0));
    eth_gwca_reg_write(GWEIS1,(RSWITCH_GWCA_GWEIS1));
    return 0;

}


/**
    @brief  Rswitch Error Interrupt Handling ISR

    @param  IRQ

    @param  dev_id

    @return IRQ_HANDLED

*/
static irqreturn_t rswitch_error_interrupt(int IRQ, void * dev_id)
{
    uint32_t REIS0 = 0;
    uint32_t REIS1 = 0;
    uint32_t RMIS0 = 0;
    
    uint32_t PortIndex = 0;
    spin_lock(&port_lock);
    
    REIS0 = eth_gwca_reg_read((RSWITCH_GWCA_REIS0));
    REIS1 = eth_gwca_reg_read((RSWITCH_GWCA_REIS1));
    RMIS0 = eth_gwca_reg_read((RSWITCH_GWCA_RMIS0));
    
    if((REIS0 != 0) || (REIS1 != 0) || (RMIS0 != 0))
    {
        if((REIS0 != 0) || (RMIS0 != 0))
        {

            /* Export call from forwarding engine error function */
            rswitchfwd_error();
            eth_gwca_reg_write(REIS0,RSWITCH_GWCA_REIS0 );
            eth_gwca_reg_write(REIS0,RSWITCH_GWCA_RMIS0 );
        }   
        if(REIS1 != 0)
        {
            
            for(PortIndex = 0; PortIndex<Board_Config.Eth_Ports;PortIndex++)
            {
                
                if(((REIS1 >> PortIndex) & 0x01))
                {
                    /* Call netdev error handler */
                    rswitch_eth_error(ppndev[PortIndex]);
                }
                
            }
            if(((REIS1 >> PortIndex) & 0x01))
            {
                /* Call GW Error handler */
                rswitch_gw_error();
            }
        }
    }
    eth_gwca_reg_write(REIS0,(RSWITCH_GWCA_REIS0));
    eth_gwca_reg_write(REIS1,(RSWITCH_GWCA_REIS1));
    spin_unlock(&port_lock);
    
    return IRQ_HANDLED;


}

/**
    @brief  Rswitch Packet receive function

    @param  ndev

    @param  quota

    @param  q

    @return < 0, or 0 on success

*/
static bool rswitch_rx(struct net_device *ndev, int *quota, int q)
{

    struct port_private *priv = netdev_priv(ndev);
    
    u32 PortNumber  = 0;
    int entry = cur_rx[q] % num_rx_ring[q];
    
    int boguscnt = (dirty_rx[q] + num_rx_ring[q]) -
    cur_rx[q];
    
    /* Need to put queue wise */
    struct net_device_stats *stats = &priv->Stats;
    struct rswitch_ext_ts_desc *desc;
    struct rswitch_ext_ts_desc *linkdesc;
    struct sk_buff *skb;
    dma_addr_t dma_addr;
    struct timespec64 ts;
    u32 get_ts = 0;
    u16 pkt_len;
    int limit;
    
    boguscnt = min(boguscnt, *quota);
    limit = boguscnt;
    
    desc = &rx_ring[q][entry];
    

    while (desc->die_dt != DT_FEMPTY) {
        
        PortNumber = ((uint64_t)(desc->info1 >> 32)) & 0x1F;
        
#ifdef INTERNAL_GATEWAY_PORT
        if((priv->PortNumber == Board_Config.Eth_Ports))
        {
            
            PortNumber = Board_Config.Eth_Ports;
            
        }
#endif
        ndev = ppndev[PortNumber];
        
        
        if(priv->PortNumber != PortNumber)
        {
#ifdef DEBUG_RX
            //printk("Mismatch \n");
#endif
        }
        priv = netdev_priv(ndev);
        /* Descriptor type must be checked before all other reads */
        dma_rmb();
        
        pkt_len = le16_to_cpu(desc->info_ds) & RX_DS;

        
        if (--boguscnt < 0)
        break;

        /* We use 0-byte descriptors to mark the DMA mapping errors */
        if (!pkt_len)
        continue;

        
        
        get_ts = priv->tstamp_rx_ctrl & RSWITCH_TSN_RXTSTAMP_TYPE;
        
        skb = rx_skb[q][entry];
        
        memcpy(skb->data, phys_to_virt(desc->dptr), pkt_len);
        
        rx_skb[q][entry] = NULL;
        
        dma_unmap_single(&pcidev->dev, le32_to_cpu(desc->dptr),
        PKT_BUF_SZ,
        DMA_FROM_DEVICE);
        
        get_ts &= (q == RSWITCH_NC) ?RSWITCH_RXTSTAMP_TYPE_V2_L2_EVENT :~RSWITCH_RXTSTAMP_TYPE_V2_L2_EVENT;
        
        if (get_ts) {
            struct skb_shared_hwtstamps *shhwtstamps;

            shhwtstamps = skb_hwtstamps(skb);
            memset(shhwtstamps, 0, sizeof(*shhwtstamps));
            ts.tv_sec = (u64) le32_to_cpu(desc->ts_sec);
            
            ts.tv_nsec = le32_to_cpu(desc->ts_nsec);
            shhwtstamps->hwtstamp = timespec64_to_ktime(ts);
        }
        
        
        skb_put(skb, pkt_len);
        
        skb->protocol = eth_type_trans(skb, ndev);
        
        napi_gro_receive(&priv->napi[q], skb);
        
        
        
        stats->rx_packets++;
        stats->rx_bytes += pkt_len;
        
        entry = (++cur_rx[q]) % num_rx_ring[q];
        desc = &rx_ring[q][entry]; 
       
        

        
    }
    desc_end[q] = desc;
    /* Refill the RX ring buffers. */


    for (; cur_rx[q] - dirty_rx[q] > 0; dirty_rx[q]++) {
        entry = dirty_rx[q] % num_rx_ring[q];
        desc = &rx_ring[q][entry];
        desc->info_ds = cpu_to_le16(PKT_BUF_SZ);

        if (!rx_skb[q][entry]) {
            skb = dev_alloc_skb(
            PKT_BUF_SZ + RSWITCH_ALIGN - 1);
            
            if (!skb)
            break;  /* Better luck next round. */
            
            skb_reserve(skb, NET_IP_ALIGN);
            dma_addr = dma_map_single(&pcidev->dev, skb->data,
            le16_to_cpu(desc->info_ds),
            DMA_FROM_DEVICE);
            skb_checksum_none_assert(skb);
            /* We just set the data size to 0 for a failed mapping
        * which should prevent DMA  from happening...
        */
            if (dma_mapping_error(&pcidev->dev, dma_addr))
            desc->info_ds = cpu_to_le16(0);
            desc->dptr = cpu_to_le32(dma_addr);
            rx_skb[q][entry] = skb;
        }
        /* Descriptor type must be set after all the above writes */
        dma_wmb();
        
        desc->die_dt = DT_FEMPTY;
        
    }
    linkdesc = &rx_ring[q][num_rx_ring[q]];
    if(linkdesc->die_dt == 0xC0)
    {
        
        linkdesc->die_dt = DT_LINK;
        

    }
    *quota -= limit - (++boguscnt);
    

    return boguscnt <= 0;
}


/**
    @brief  Rswitch TX Timestamp Interrupt handling

    @param  ndev

    @return < 0, or 0 on success

*/
static bool rswitch_tx_ts_interrupt(struct net_device *ndev)
{
    struct port_private *priv = netdev_priv(ndev);
    struct rswitch_ext_ts_desc *desc;   
    struct rswitch_tstamp_skb *ts_skb, *ts_skb2;
    struct skb_shared_hwtstamps shhwtstamps;
    struct timespec64 ts;
    struct sk_buff *skb;
    struct rswitch_ext_ts_desc *linkdesc;
    u16 tsun, tfa_tsun;
    int entry = cur_ts % TS_RING_SIZE;
    desc = &ts_ring[entry];    
    if(desc->die_dt != DT_FEMPTY_ND)
    {
        
        ts.tv_nsec  = desc->ts_nsec;  
        
        ts.tv_sec   = desc->ts_sec;    
        
        tfa_tsun = ((desc->info1) & 0xFF) ;
        
        memset(&shhwtstamps, 0, sizeof(shhwtstamps));
        shhwtstamps.hwtstamp = timespec64_to_ktime(ts);

        list_for_each_entry_safe(ts_skb, ts_skb2, &priv->ts_skb_list,list)
        {
            skb = ts_skb->skb;
            tsun = ts_skb->tag;
            
            list_del(&ts_skb->list);
            kfree(ts_skb);
            
            if (tsun == tfa_tsun) 
            {
                /* If timestamp for given tsun find clone skb and pass back to socket */
                
                skb_tstamp_tx(skb, &shhwtstamps);   
                
#ifdef TAS_DEBUG
                if (strcasecmp(run_mode, "TAS") == 0)
                {
                    count_tas_frame++;
                    if((count_tas_frame % 25) == 0)
                    {
                        printk("TX TS Sec = %lx NSEC = %lx \n", ts.tv_sec, ts.tv_nsec);

                    }
                }
#endif
                dev_kfree_skb_any(skb);
                break;             
            }
        }
        entry = (++cur_ts) % TS_RING_SIZE;
        desc = &ts_ring[entry];


    }

    for (; cur_ts - dirty_ts > 0; dirty_ts++) {
        entry = dirty_ts % TS_RING_SIZE;
        
        
        desc = &ts_ring[entry];
        desc->info_ds = cpu_to_le16(PKT_BUF_SZ);
        desc->die_dt = DT_FEMPTY_ND;
        
    }
    linkdesc = &ts_ring[num_ts_ring];
    if(linkdesc->die_dt == 0xc0)
    {
        
        linkdesc->die_dt = DT_LINK;

    }


    return 0;              

}


/**
    @brief  Rswitch RX Poller Function

    @param  napi

    @param  budget

    @return int

*/
static int rswitch_poll(struct napi_struct *napi, int budget)
{
    
    struct net_device *ndev = napi->dev;
    struct port_private *priv = netdev_priv(ndev);
    unsigned long flags;
   
    int q = napi - priv->napi;
    
    
    uint32_t PortNumber = 0;
    
    struct rswitch_ext_ts_desc *desc;
    
    int quota = budget;
    u32 ris, tis;
    static void __iomem *desc_end_base;
    struct rswitch_ext_ts_desc *rx_desc_ptr;
    
    struct rswitch_ext_ts_desc *rx_ts_desc;
    u32 *rx_desc;
    u32 frame_check = 0;
    u32 tis_q = 0;
    u32 rxq = q;
    u32 queue = 0;
    u32 tis_Port  = 0;
    int entry = 0;
    u32 desc_addr = 0;
    u32 RATLR = 0;
    u32 k = 0;
    uint8_t desc_learn  = 0;
#ifdef INTERNAL_GATEWAY_PORT
    /* Try registering separate poller for Internal GW */
    if((priv->PortNumber == Board_Config.Eth_Ports))
    {
        rxq = RENESAS_RSWITCH_RX_GW_QUEUE;
    }  
    else
    {

        rxq = q;
    }
#endif

    for (;;) 
    {

        if(desc_end[rxq] != NULL)
        {
            
            desc_end_base =  phys_to_virt((long long unsigned int)desc_end[rxq]);
            desc_end_base =  (desc_end[rxq]);
            
            
            rx_desc = desc_end_base;
            
            frame_check = *rx_desc;
            
            
        }
        
        tis = eth_gwca_reg_read((RSWITCH_AXI_TDIS0));
        ris = eth_gwca_reg_read( (RSWITCH_AXI_RDIS0));
        
        if ((!((ris & 0xFFFFFFFF) || ((tis  >> (q + (priv->PortNumber * 2))))))  && (((frame_check>>28) & 0x0F)!= 0x8) )
        {
            
            break;
        }
        
        /* Processing RX Descriptor Ring */
       
        entry = cur_rx[rxq] % num_rx_ring[rxq];
        desc = &rx_ring[rxq][entry];
        PortNumber = ((uint64_t)(desc->info1 >> 32)) & 0x1F;
         
        if((priv->PortNumber == Board_Config.Eth_Ports))
        {
            
            PortNumber = Board_Config.Eth_Ports;
            
        }
        
        {
            if ((ris & 0x7FFFFFFF) || (((frame_check>>28) & 0x0F)== 0x8)){
                
                if(PortNumber !=  priv->PortNumber)
                {
#ifdef DEBUG_RX
                    printk("Port Number Mismatch\n");
#endif
                }
                /* Clear RX interrupt */
                eth_gwca_reg_write((ris & 0x7FFFFFFF), RSWITCH_AXI_RDIS0);
                if (rswitch_rx(ppndev[PortNumber], &quota, rxq))
                {
                    
                    goto out;

                }

                
            }
        }
        
        /* Processing TX Descriptor Ring */
        tis_q = tis % 2;
        tis_Port  = tis / 2;
        if(tis != 0) 
        if ((tis  >> (q + (priv->PortNumber * 2))))   
        {

            spin_lock_irqsave(&priv->lock, flags);
            /* Clear TX interrupt */
            
            eth_gwca_reg_write((tis ), RSWITCH_AXI_TDIS0);
            
            {
                rswitch_tx_free(ndev, q, true);
            }
            netif_wake_subqueue(ndev, q);
            mmiowb();
            spin_unlock_irqrestore(&priv->lock, flags);

        }
    }
    napi_complete(napi);
    
    spin_lock_irqsave(&priv->lock, flags);
  
    for(queue = 0; queue <= 2; queue++)
    {
        if(error_flag[queue] == 1)
        {
            printk("Resetting learning base addr \n");
            
            entry  = 0;
         
            while (entry < num_rx_ring[queue])
            {
                        
               desc = &rx_ring[queue][entry];
               desc->die_dt = DT_FEMPTY;
               desc->info_ds = cpu_to_le16(PKT_BUF_SZ);
               entry++;
                       
            }
            rx_desc_ptr = &rx_ring[queue][entry];
            rx_desc_ptr->dptr = cpu_to_le32((u32)rx_desc_dma[queue]);
            rx_desc_ptr->die_dt = DT_LINK; /* type */
            rx_ts_desc = rx_desc_bat[queue];
    
            rx_ts_desc->die_dt = DT_LINK; /* type */
            desc_addr = rx_desc_bat_dma[queue];
            desc_end[queue] = NULL;
            cur_rx[queue] = 0;
            dirty_rx[queue] = 0;
            eth_gwca_reg_write( desc_addr, RSWITCH_AXI_RATLS1);
            /* Kick Descriptor learning */
            RATLR = eth_gwca_reg_read( RSWITCH_AXI_RATLR);
            eth_gwca_reg_write((RATLR | 0x80000000),RSWITCH_AXI_RATLR );
            RATLR = eth_gwca_reg_read( RSWITCH_AXI_RATLR);
            for (k = 0; k < RSWITCH_DESCLEARN_CONFIG_TIMEOUT_MS; k++) 
            {
                RATLR = eth_gwca_reg_read( RSWITCH_AXI_RATLR);
        
                if((RATLR & 0x80000000) == 0x0)
                {
                    desc_learn = 1;
                    break;

                }
                udelay(100);

            } 
            if(desc_learn  == 0)
            {
                printk("Descriptor Learning Error  \n");
                return -1;
            }
            desc_learn = 0;
            
            error_flag[queue] = 0;
            
        }
        
    }

    
    /* Re-enable RX/TX interrupts */
    
    eth_gwca_reg_write( 0xFFFFFFFF, RSWITCH_AXI_RDIE0);
    eth_gwca_reg_write( 0xFFFFFFFF, RSWITCH_AXI_TDIE0);
    eth_gwca_reg_write(0x01,RSWITCH_AXI_TSDIE);

    mmiowb();
    spin_unlock_irqrestore(&priv->lock, flags);
    
    
out:
    return budget - quota;
    
}


/**
    @brief  Queue Interrupt handling function

    @param  ndev

    @param  q

    @return < 0, or 0 on success

*/
static bool rswitch_queue_interrupt(struct net_device *ndev, int q)
{
    struct port_private *priv = netdev_priv(ndev);
    
    {
        
       
        /* Registered 7 Napis one per port*/
        if (napi_schedule_prep(&priv->napi[q%2])) 
        {
            
            /* Mask RX and TX interrupts */
            eth_gwca_reg_write(0xFFFFFFFF, RSWITCH_AXI_RDID0);
            eth_gwca_reg_write(0xFFFFFFFF, RSWITCH_AXI_TDID0);
           
            __napi_schedule(&priv->napi[q%2]);
            
            
            
        }
        return true;
    }
    
    



}






/**
    @brief  Rswitch Interrupt Service routine

    @param  IRQ

    @param  dev_id

    @return < 0, or 0 on success

*/
static irqreturn_t rswitch_eth_interrupt(int IRQ, void * dev_id)
{
    
    u32                 RIS = 0;
    u32                 TIS = 0;
    u32                 TSDIS = 0;
    u32                 REIS = 0;
    int entry =  0;
    int q  = 0;
    struct rswitch_ext_ts_desc *desc;
    struct rswitch_ext_ts_desc *ts_desc;
    uint32_t PortNumber = 0;
    
    irqreturn_t         ret = IRQ_HANDLED;
    spin_lock(&port_lock);
    REIS = eth_gwca_reg_read((RSWITCH_GWCA_REIS1));
    /* Check Descriptor Interrupt status*/
    
    RIS = eth_gwca_reg_read((RSWITCH_AXI_RDIS0));
    TIS = eth_gwca_reg_read((RSWITCH_AXI_TDIS0));
    TSDIS = eth_gwca_reg_read((RSWITCH_AXI_TSDIS));
    
    
    entry = cur_ts % num_ts_ring;
    ts_desc = &ts_ring[entry];
    do
    {
        
        if((TSDIS !=0 ) || (ts_desc->die_dt != DT_FEMPTY_ND))
        {
            
            eth_gwca_reg_write(0x01, (RSWITCH_AXI_TSDID));
            eth_gwca_reg_write(0x01, (RSWITCH_AXI_TSDIS));
            
            PortNumber = ((uint64_t)(ts_desc->info1 >> 32)) & 0x1F;
            rswitch_tx_ts_interrupt(ppndev[PortNumber]);
            eth_gwca_reg_write(0x01, (RSWITCH_AXI_TSDIE));
            
            
            
            ret = IRQ_HANDLED;

        }
        
        


        

        if((RIS != 0))
        {
            for(q = 2; q >= 0; q--)
            {
                
                if(((RIS >> q) & 0x01) == 0x01)
                {
                    
                    int entry = cur_rx[q] % num_rx_ring[q];

                    desc = &rx_ring[q][entry];
                    PortNumber = ((uint64_t)(desc->info1 >> 32)) & 0x1F;
                    
#ifdef INTERNAL_GATEWAY_PORT
                    if(q == RENESAS_RSWITCH_RX_GW_QUEUE)
                    {
                        PortNumber = Board_Config.Eth_Ports;
                        
                    }
#endif
                    if(rswitch_queue_interrupt(ppndev[PortNumber], q))
                    {
                        
                        ret = IRQ_HANDLED;
                    }
                }


            }


        }
        if((TIS != 0))
        {
            for(q = (((CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1)*2)  ); q >= 0; q--)
            {
                if((TIS >> q) & 0x01)
                {
                    if(rswitch_queue_interrupt(ppndev[q/2], q))
                    {
                        
                        ret = IRQ_HANDLED;
                    }
                }


            }


        }

        TSDIS = eth_gwca_reg_read((RSWITCH_AXI_TSDIS));
    }while(TSDIS != 0);

    spin_unlock(&port_lock);
    return ret;
}


/**
    @brief  Platform Driver Probe GPIO Interrupt

    @param  pdev Platform Device to be instantiated

    @return < 0, or 0 on success

*/
static int  rswitch_drv_Probe_GetInterrupts(struct platform_device * pdev)
{
    int     ret = 0;
    
    int     r;
    int irqNumber = 0;
    

    for (r = 0; r < sizeof(rswitch_eth_IRQ_Data)/sizeof(rswitch_eth_IRQ_Data[0]); r++)
    {
        rswitch_eth_IRQ_Data[r] = r;
        rswitch_eth_IRQ[r] = 0;
    }

    for (r = 0; r < sizeof(rswitch_eth_IRQ)/sizeof(rswitch_eth_IRQ[0]); r++)
    {

        gpio_request(RSWITCH_INT_LINE_GPIO + r, "tsn_irq_lines");
        gpio_direction_input(RSWITCH_INT_LINE_GPIO + r);
        gpio_set_debounce(RSWITCH_INT_LINE_GPIO + r, 50);
        gpio_export(RSWITCH_INT_LINE_GPIO + r, true);
        irqNumber = gpio_to_irq(RSWITCH_INT_LINE_GPIO + r);

        
        if (irqNumber < 0) 
        {
            dev_err(&pdev->dev, "[RSWITCH] Failed to get IRQ (%u)\n", r);
            return -EINVAL;
        }
        
        rswitch_eth_IRQ[r] = irqNumber;
    }


#ifdef DEBUG_PRINT
    printk("Interrupt probed \n");
#endif

    return ret;
}

/**
    @brief  Register GPIO interrupts

    @param  void

    @return < 0, or 0 on success

*/
static int rswitch_Probe_RegisterInterrupts(void)
{
    int     r;
    int     ret = 0;



    /*
        Register one Interrupt Handlers for  all SLOW PROTOCOL Interrupt line
        Interrupt line 0  (TS FIFO interrupt)
        Interrupt line 1  (Rx Interrupt)
        Interrupt line 2  (Tx Interrupt)
        Interrupt line 3  (Agent Error Interrupt)
        
    */
    for (r = 0; r < ARRAY_SIZE(rswitch_eth_IRQ); r++)
    {

        if(r == 1)
        {

            ret = request_irq(rswitch_eth_IRQ[r],
            rswitch_error_interrupt, 
            (IRQF_TRIGGER_RISING  ) , RSWITCH_FPGA_ETH_PLATFORM, 
            rswitch_eth_IRQ_Data);
            
            if (ret != 0) 
            {
                pr_err("[RSWITCH]  Probe FAILED. Cannot assign IRQ(%d) for Line%u. ret=%d\n", 
                rswitch_eth_IRQ[r], r, ret);
                return ret;
            }


        }
        else

        {
            ret = request_irq(rswitch_eth_IRQ[r], 
            rswitch_eth_interrupt, 
            (IRQF_TRIGGER_RISING ) , RSWITCH_FPGA_ETH_PLATFORM, 
            rswitch_eth_IRQ_Data);
            
            if (ret != 0) 
            {
                pr_err("[RSWITCH]  Probe FAILED. Cannot assign IRQ(%d) for Line%u. ret=%d\n", 
                rswitch_eth_IRQ[r], r, ret);
                return ret;
            }
        }
    }

    return ret;
}



/**
    @brief  Remove GPIO interrupts

    @param  void

    @return void

*/
static void rswitch_drv_Remove_Interrupts(void)
{
    int     r;

    for (r = 0; r < sizeof(rswitch_eth_IRQ)/sizeof(rswitch_eth_IRQ[0]); r++)
    {
        if (rswitch_eth_IRQ[r] != 0)
        {
            
            gpio_set_value(RSWITCH_INT_LINE_GPIO + r, 0);              // Set GPIO to 0
            gpio_unexport(RSWITCH_INT_LINE_GPIO + r);                  // Unexport the GPIO
            free_irq(rswitch_eth_IRQ[r],rswitch_eth_IRQ_Data);
            rswitch_eth_IRQ[r] = 0;
            rswitch_eth_IRQ_Data[r] = 0;

        }
    }
}




// ------------------------------------[ Port Setup ]------------------------------------


/**
    @brief  Set the hardware MAC address from dev->dev_addr.

    (update_mac_address)

    @param  ndev    net_device for the individual switch port device 
*/
static void port_Set_MAC_Address(struct net_device * ndev)
{

    portreg_write(ndev, ( (ndev->dev_addr[0]      ) | 
    (ndev->dev_addr[1] << 8 ) |
    (ndev->dev_addr[2] << 16) | 
    (ndev->dev_addr[3] << 24) ),  RSWITCH_RMAC_MRMAC1 );
    portreg_write(ndev, ( (ndev->dev_addr[4]      ) | 
    (ndev->dev_addr[5] << 8 ) ),  RSWITCH_RMAC_MRMAC0);

}


/**
    @brief  Query tsn port MAC address

    @param  ndev    net_device for the individual tsn port device 
portreg
    Return the MAC address
    currently set in the MAHR & MALR port registers
*/
static void port_Query_MAC_Address(struct net_device * ndev)
{

    ndev->dev_addr[0] = (portreg_read(ndev, RSWITCH_RMAC_MRMAC1)        & 0xFF);
    ndev->dev_addr[1] = (portreg_read(ndev, RSWITCH_RMAC_MRMAC1) >> 8)  & 0xFF;
    ndev->dev_addr[2] = (portreg_read(ndev, RSWITCH_RMAC_MRMAC1) >> 16) & 0xFF;
    ndev->dev_addr[3] = (portreg_read(ndev, RSWITCH_RMAC_MRMAC1) >> 24) & 0xFF;
    ndev->dev_addr[4] = (portreg_read(ndev, RSWITCH_RMAC_MRMAC0))       & 0xFF;
    ndev->dev_addr[5] = (portreg_read(ndev, RSWITCH_RMAC_MRMAC0) >> 8)  & 0xFF;

}


/**
    @brief  Generate switch port MAC address

    @param  ndev    net_device for the individual switch port device 

    Switch block doesn't have 'ROM' for MAC addresses.
*/
static void port_Generate_MAC_Address(u32 Port, struct net_device * ndev )
{
    char    Default_MAC[] =RENESAS_RSWITCH_BASE_PORT_MAC;
    
    Default_MAC[5] = Port + 1;
    memcpy(ndev->dev_addr, Default_MAC, 6);
    
}






/**
    @brief  Configure CBS

    @param  ndev    net_device for the port device
    
    @param  TxParam

    @return < 0, or 0 on success

*/
static int Config_Tx_Streams(struct net_device * ndev, struct  rswitch_Config_Port_Tx* TxParam)
{
    u32 CACC  = 0;
    u32 CIV = 0;
    u32 Streams = 0;
    u32 CUL  = 0;
    for(Streams = 0; Streams < TxParam->TxStreams; Streams++)
    {
        /* Set CACC */
        CACC =  portreg_read(ndev, RSWITCH_CACC);
        CACC |= (1 << TxParam->TxStream[Streams].QueueNum);
        portreg_write(ndev, CACC, RSWITCH_CACC);
        /* Set CIV */ 
        CIV = (TxParam->TxStream[Streams].CIVman << 16) | (TxParam->TxStream[Streams].CIVexp);
        portreg_write(ndev, CIV , (RSWITCH_CAIV0 + (TxParam->TxStream[Streams].QueueNum)));
        CUL = TxParam->TxStream[Streams].CUL;
        portreg_write(ndev, CUL , (RSWITCH_CAUL0 + (TxParam->TxStream[Streams].QueueNum)));
        
        
    }
    portreg_write(ndev, 0xFF , RSWITCH_CCS);
    return 0;
}








/**
    @brief  Set Config Change Time

    @param  ndev    net_device for the port device
    
    @param  TAS

    @param  GateEnabled

    @return < 0, or 0 on success

*/
static int SetConfigChangeTime(struct net_device * ndev, struct rswitch_Config_Port_Tx_TAS * TAS, uint32_t GateEnabled)
{

    struct timespec     TimeSpec;
    
    u64 CurrentTime = 0;
    u64 AdminBaseTime = 0;
    
    u64 numCycles = 0;
    rswitchptp_GetTime(&TimeSpec);
    
    CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
    AdminBaseTime = TAS->AdminBaseTime.nseconds;
    if(AdminBaseTime > (CurrentTime + (TAS->SWTimeMultiplier)))
    {
        TAS->ConfigChangeTime = AdminBaseTime;
        if(GateEnabled)
        {
            TAS->ConfigPending = 1;
        }

    }
    else if (!GateEnabled)
    {
        
        numCycles = div64_u64((CurrentTime - AdminBaseTime), TAS->AdminCycleTime);
        
        TAS->ConfigChangeTime = (numCycles*TAS->AdminCycleTime) + AdminBaseTime + TAS->SWTimeMultiplier;


    }
    else
    {

        
        numCycles = div64_u64((CurrentTime - AdminBaseTime), TAS->AdminCycleTime);
        
        TAS->ConfigChangeTime = (numCycles*TAS->AdminCycleTime) + AdminBaseTime + TAS->SWTimeMultiplier;
        TAS->ConfigPending = 1;
    }


    return 0;




}




/**
    @brief  Set Cycle Start Time

    @param  ndev    net_device for the port device
    
    @param  OperBaseTime

    @param  OperCycleTime

    @param  OperCycleTimeExtension

    @param  ConfigChangeTime

    @param  ConfigPending

    @param  SWTimeMultiplier

    @param  last_cmn_interval

    @return < 0, or 0 on success
*/
static int SetCycleStartTime(struct net_device * ndev,u64 OperBaseTime, u64 OperCycleTime, u64 OperCycleTimeExtension, u64 ConfigChangeTime, u32 ConfigPending, u64 SWTimeMultiplier, u32 last_cmn_interval)
{

    struct timespec     TimeSpec;
    u64 time_ns = 0;
    u64 time_ns_lower = 0;
    u64 time_ns_middle = 0;
    u64 time_ns_upper = 0;
    u64 CurrentTime = 0;
    u64       CycleStartTime = 0;
    u64 numCycles = 0;
    
    
    
    
    u32 LIT  = 0;
    
    if(ConfigPending == 0)
    {
        rswitchptp_GetTime(&TimeSpec);
        
        CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
        
        if(OperBaseTime > (CurrentTime +(SWTimeMultiplier))) 
        {
            
            CycleStartTime = OperBaseTime;

        }
        else 
        {
            
            numCycles = div64_u64((CurrentTime - OperBaseTime), OperCycleTime);
            
            CycleStartTime = (numCycles*OperCycleTime) + OperBaseTime + SWTimeMultiplier;
            


            
            
        }
        
        
        time_ns  =  CycleStartTime ;
        
        time_ns_lower  = time_ns & 0xFFFFFFFF;
        
        
        time_ns_middle  = (uint64_t)(time_ns >> 32) & 0xFFFFFFFF;
        
        portreg_write(ndev, time_ns_lower, RSWITCH_TACST2);
        portreg_write(ndev, time_ns_middle, RSWITCH_TACST1);
        portreg_write(ndev, time_ns_upper, RSWITCH_TACST0);
        
    }
    else
    {
        rswitchptp_GetTime(&TimeSpec);
        CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
        /* Just to reconfigure with always good start time */
        ConfigChangeTime = CurrentTime + (OperCycleTime) + OperCycleTimeExtension + 10000;
        if(ConfigChangeTime > (CurrentTime + (OperCycleTime) + OperCycleTimeExtension) )
        {
            numCycles = div64_u64((CurrentTime - OperBaseTime), OperCycleTime);
            
            CycleStartTime = (numCycles*OperCycleTime) + OperBaseTime + SWTimeMultiplier;

            time_ns  =  CycleStartTime ;
            time_ns_lower  = time_ns & 0xFFFFFFFF;
            time_ns_middle  = (time_ns >> 32) & 0xFFFFFFFF;
            
            portreg_write(ndev, time_ns_lower, RSWITCH_TACST2);
            portreg_write(ndev, time_ns_middle, RSWITCH_TACST1);
            portreg_write(ndev, time_ns_upper, RSWITCH_TACST0);
            LIT = CycleStartTime - last_cmn_interval - OperCycleTimeExtension;
            time_ns  =  LIT ;
            time_ns_lower  = time_ns & 0xFFFFFFFF;
            time_ns_middle  = (time_ns >> 32) & 0xFFFFFFFF;
            
            portreg_write(ndev, time_ns_lower, RSWITCH_TALIT2);
            portreg_write(ndev, time_ns_middle, RSWITCH_TALIT1);
            portreg_write(ndev, time_ns_upper, RSWITCH_TALIT0);
            
            

        }
        else
        {
            
            CycleStartTime = ConfigChangeTime;
            time_ns  =  CycleStartTime ;
            time_ns_lower  = time_ns & 0xFFFFFFFF;
            time_ns_middle  = (time_ns >> 32) & 0xFFFFFFFF;
            
            portreg_write(ndev, time_ns_lower, RSWITCH_TACST2);
            portreg_write(ndev, time_ns_middle, RSWITCH_TACST1);
            portreg_write(ndev, time_ns_upper, RSWITCH_TACST0);
            
            
        }
        
    }
    






    return 0;
}




/**
    @brief  Configure and Enable TAS

    @param  ndev    net_device for the port device
    
    @param  TAS

    @return < 0, or 0 on success
*/
static int Config_Enable_TAS(struct net_device * ndev, struct rswitch_Config_Port_Tx_TAS * TAS)
{
    uint32_t    err = 0;
    
    uint64_t TAEN = 0;
    uint32_t entry = 0;
    uint32_t Queues  = 0;
    uint32_t IGS  = 0;
    uint32_t GateControls  = 0;
    uint32_t TACLL0  = 0;
    uint32_t TACLL1  = 0;
    uint32_t TASFE  = 0; 
    uint32_t EAMIS = 0;
    uint64_t OperBaseTime  = 0;
    uint64_t OperCycleTime = 0;
    uint64_t OperCycleTimeExtension  = 0;
    u32 TAS_Enable  = 0;
    u32 last_cmn_interval  = 0;
    u64 CurrentTime  = 0;
    u32 TAEN0 = 0;
    u32 TAEN1 = 0;
    struct timespec     TimeSpec;

    /*Configure GPTP Timer Domain */
    portreg_write(ndev, TAS->timer_domain, RSWITCH_TPTPC);
    /*Configure Jitter & Latency */
    portreg_write(ndev, TAS->jitter_time, RSWITCH_TTJ);
    portreg_write(ndev, TAS->latency_time, RSWITCH_TTML);

    
    last_cmn_interval = TAS->GateControl[0].QueueGateConfig[TAS->GateControl[0].GateControls - 1].TimeInterval;
    EAMIS  = portreg_read(ndev,  RSWITCH_EAMIS);
    TAS_Enable = ((EAMIS ) & 0x01);
    
    
    
    
    
    SetConfigChangeTime(ndev, TAS, TAS_Enable);
    rswitchptp_GetTime(&TimeSpec);
    CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;

    OperBaseTime = TAS->AdminBaseTime.nseconds;
    OperCycleTime = TAS->AdminCycleTime;
    OperCycleTimeExtension =  TAS->AdminCycleTimeExtension.nseconds;
    
    SetCycleStartTime(ndev,OperBaseTime,OperCycleTime,OperCycleTimeExtension,TAS->ConfigChangeTime, TAS->ConfigPending, TAS->SWTimeMultiplier , last_cmn_interval);
    
    {
        /*Configure Initial Gate State*/
        IGS = 0x0F;
        
        for(Queues = 0; Queues < TAS->AdminGateStates; Queues++)
        {
            /*802.1 QBV by default all gates open */
            IGS = TAS->AdminGateState[Queues].State? IGS: IGS & (~((!TAS->AdminGateState[Queues].State) << TAS->AdminGateState[Queues].QueueNumber));

        }
        
        portreg_write(ndev, IGS, RSWITCH_TGS);
        IGS = portreg_read(ndev,  RSWITCH_TGS);

        
        for(Queues = 0; Queues < TAS->Queues; Queues++)
        {
            TAEN |= (uint64_t)((uint64_t)TAS->GateControl[Queues].GateControls << ((TAS->GateControl[Queues].QueueNumber)*8 ));
        }
        
        TAEN0 = (TAEN & 0xFFFFFFFF);
        TAEN1 =((uint64_t)(TAEN & 0xFFFFFFFF00000000) >> 32);
        portreg_write(ndev, TAEN0, RSWITCH_TAEN0);
        portreg_write(ndev, TAEN1, RSWITCH_TAEN1);
        
        /* Queue should be sorted by rswitchtool in array in ascending order*/
        for(Queues = 0; Queues < TAS->Queues; Queues++)
        {
            
            if(TAS->GateControl[Queues].QueueGateConfig[0].State == TAS->GateControl[Queues].QueueGateConfig[TAS->GateControl[Queues].GateControls-1].State)
            {
                
                TAS->GateControl[Queues].skip_first_entry = 1;
                TAS->GateControl[Queues].QueueGateConfig[TAS->GateControl[Queues].GateControls-1].TimeInterval += TAS->GateControl[Queues].QueueGateConfig[0].TimeInterval;
            }
            for(GateControls = 0; GateControls < TAS->GateControl[Queues].GateControls; GateControls++ )
            {
                
                TACLL0 = (TAS->GateControl[Queues].QueueGateConfig[GateControls].State << 28) |  TAS->GateControl[Queues].QueueGateConfig[GateControls].TimeInterval;          
                TACLL1 =  entry++;
                portreg_write(ndev, TACLL0, RSWITCH_TACLL0);
                portreg_write(ndev, TACLL1, RSWITCH_TACLL1);
                portreg_write(ndev, 0x80000000, RSWITCH_TACLL2);
            }
            
            
            TASFE |= TAS->GateControl[Queues].skip_first_entry << TAS->GateControl[Queues].QueueNumber;
            portreg_write(ndev, TASFE, RSWITCH_TASFE);
            

            
        }
        if((TAS_Enable)  ==  1)
        {
            
            portreg_write(ndev, 0x80000000, RSWITCH_TCS);

        }

        else

        {
            
            rswitchptp_GetTime(&TimeSpec);
            mdelay(500);
            portreg_write(ndev, 0x01, RSWITCH_TCC);
            rswitchptp_GetTime(&TimeSpec);
        }
        

    }

    return err;
}























/**
    @brief  Configure TX Queues

    @param  ndev    net_device for the port device
    
    @param  TxQueue

    @param  PminSize

    @return < 0, or 0 on success
*/
static int Config_Tx_Queues(struct net_device * ndev, struct rswitch_Config_Port_Tx_TxQueue* TxQueue, u32 PminSize)
{
    u32 EATMTC = 0;
    u32 TxQueueConfig = 0;
    
    if(TxQueue->MAX_Frame_Sz >= 0)
    {
        portreg_write(ndev,TxQueue->MAX_Frame_Sz, (RSWITCH_EATMS0 + TxQueue->QueueNumber));

    }
    
    if(TxQueue->Pre_Empt_MAC)
    {
        if(!TxQueueConfig)
        {
            portreg_write(ndev,PminSize, RSWITCH_EATAFS);
            TxQueueConfig = 1;
            
        }
        EATMTC = portreg_read(ndev, RSWITCH_EATMTC);
        EATMTC = EATMTC | (1 << TxQueue->QueueNumber);
        portreg_write(ndev,EATMTC, RSWITCH_EATMTC);
        
        
    } 
    else
    {
        EATMTC = portreg_read(ndev, RSWITCH_EATMTC);
        EATMTC &= ~(1UL << TxQueue->QueueNumber);
        portreg_write(ndev,EATMTC, RSWITCH_EATMTC);


    }    
    return 0;
}



/**
    @brief  Set port Configuration

    @param[in]  file
    @param      arg

    @return     < 0 on error
*/
static long IOCTL_SetPortConfig(struct file * file, unsigned long arg)
{
    char __user * buf = (char __user *)arg;
    int     a = 0;
    
    int     err = 0;
    
    u32 queues = 0;
    u32 EAMIS = 0;
    enum RSWITCH_PortState eState;

#ifdef _DEBUG
    printk("[RSWITCH] %s \n", __FUNCTION__);
#endif

    err = copy_from_user(&Config_New, buf, sizeof(Config_New));

    if (err != 0)            
    {
        pr_err("[RSWITCH] IOCTL_SetPortConfig. copy_from_user returned %d for RSWITCH_SW_SET_CONFIG \n", err);    
        return err;
    }

    /*
        Validate new configuration first
    */
    if (Config_New.Ports > CONFIG_RENESAS_RSWITCH_ETH_PORTS)
    {
        printk("[RSWITCH] RSWITCH_SET_CONFIG ERROR 1: Too many Ports (%u) Max(%u)\n", Config_New.Ports, CONFIG_RENESAS_RSWITCH_ETH_PORTS);
        return -EINVAL;
    }
    memcpy(&Config, &Config_New, sizeof(Config));
    for (a = 0; a < Config_New.Ports; a++)
    {
        struct net_device   * ndev = ppndev[Config_New.Port[a].PortNumber];
        if (Config_New.Port[a].PortNumber > CONFIG_RENESAS_RSWITCH_ETH_PORTS)
        {
            printk("[RSWITCH] RSWITCH_SET_CONFIG ERROR 2: Port %u. Port Number invalid (%u)\n", a, Config_New.Port[a].PortNumber);
            return -EINVAL;
        }
        eState = port_QueryState(ndev);
        if(eState  !=  RSWITCH_PortState_Config)
        {
            if (Config_New.Port[a].TxParam.TAS.bEnable)
            {
                
                err = Config_Enable_TAS(ndev, &Config_New.Port[a].TxParam.TAS);

                if(err < 0)
                {
                    printk("TAS did not configured \n");

                }
                
            }
            else
            {
                
                EAMIS  = portreg_read(ndev,  RSWITCH_EAMIS);
                portreg_write(ndev, 0x00, RSWITCH_TCC);

                


            }
        }
        else
        {
            for(queues = 0; queues < Config_New.Port[a].TxParam.TxQueues;queues++)
            {
                Config_Tx_Queues(ndev, &Config_New.Port[a].TxParam.TxQueue[queues],Config_New.Port[a].PminSize );

            }



        }
        
        {

            Config_Tx_Streams(ndev,&Config_New.Port[a].TxParam);


        } 
    }
    

    

    return err;
}










/**
    @brief  Get all ports CBS & Routing

    @param[in]  file
    @param      arg

    @return     < 0 on error
*/
static long IOCTL_GetfPortConfig(struct file * file, unsigned long arg)
{
    char __user *buf = (char __user *)arg;
    int     err = 0;

    

    if ((err = copy_to_user(buf, &Config, sizeof(Config))) < 0)
    {
        return -EFAULT;
    }

    return 0;
}


/**
    @brief  Driver File IOCTLs

    @return < 0, or 0 on success
*/
static long drv_IOCTL(struct file * file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    switch (cmd)
    {
    case RSWITCH_SET_CONFIG:
        return IOCTL_SetPortConfig(file, arg);
    case RSWITCH_GET_CONFIG:
        return IOCTL_GetfPortConfig(file, arg);
    default:
        pr_err("[RSWITCH] File IOCTL Unknown: 0x%08X\n", cmd);
        ret = -EINVAL;
    }

    return ret;
}



/**
    @brief  Get HW TS  IOCTL

    @param  ndev    net_device for the port device
    
    @param  req

    @return < 0, or 0 on success
*/
static int rswitch_hwtstamp_get(struct net_device *ndev, struct ifreq *req)
{
    struct port_private  * mdp = netdev_priv(ndev);
    struct hwtstamp_config config; 
    
    config.flags = 0;
    config.tx_type = mdp->tstamp_tx_ctrl ? HWTSTAMP_TX_ON :
    HWTSTAMP_TX_OFF;
    if (mdp->tstamp_rx_ctrl & RSWITCH_RXTSTAMP_TYPE_V2_L2_EVENT)
    config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
    else if (mdp->tstamp_rx_ctrl & RSWITCH_RXTSTAMP_TYPE_ALL)
    config.rx_filter = HWTSTAMP_FILTER_ALL;
    else
    config.rx_filter = HWTSTAMP_FILTER_NONE;

    return copy_to_user(req->ifr_data, &config, sizeof(config)) ?
    -EFAULT : 0;

}


/**
    @brief  Control hardware time stamping 

    @param  ndev    net_device for the individual switch port device 'tsn{n}'
    @param  ifr
    @param  cmd

    @return < 0, or 0 on success
*/
static int rswitch_port_hwtstamp_set(struct net_device *ndev, struct ifreq *ifr, int cmd)
{
    struct port_private  * mdp = netdev_priv(ndev);
    struct hwtstamp_config   config;
    u32 tstamp_tx_ctrl;
    u32 tstamp_rx_ctrl = RSWITCH_TSN_RXTSTAMP_ENABLED;
    printk("Set TX TS called \n");
    

    if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
    {
        return -EFAULT;
    }

    /* reserved for future extensions */
    if (config.flags)
    {
        return -EINVAL;
    }

    switch (config.tx_type) 
    {
        
    case HWTSTAMP_TX_OFF:
        tstamp_tx_ctrl = 0;
    case HWTSTAMP_TX_ON:
        tstamp_tx_ctrl = RSWITCH_TSN_TXTSTAMP_ENABLED;
        break;
        /* Below line for One Step Timestamping */
    case 2: 
        tstamp_tx_ctrl = RSWITCH_TSN_TXTSTAMP_ENABLED;
        break;
    default:
        printk("[RSWITCH] %s Set HW TimeStamp. Tx_type(%u) Invalid\n", ndev->name, config.tx_type);
        return -ERANGE;
    }

    switch (config.rx_filter) 
    {
    case HWTSTAMP_FILTER_NONE:
        tstamp_rx_ctrl = 0;
        break;
    case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
        tstamp_rx_ctrl |= RSWITCH_RXTSTAMP_TYPE_V2_L2_EVENT;
        break;
    default:
        config.rx_filter = HWTSTAMP_FILTER_ALL;
        tstamp_rx_ctrl |= RSWITCH_RXTSTAMP_TYPE_ALL;
    }

    mdp->tstamp_tx_ctrl = tstamp_tx_ctrl;
    mdp->tstamp_rx_ctrl = tstamp_rx_ctrl;
    

    return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ? -EFAULT : 0;
}




/** 
    @brief IOCTL to device

    (IOCTLs for the Switch are handled elsewhere)

    @param  ndev    net_device for the individual switch port device 'avb{n}'
    @param  rq
    @param  cmd

    @return < 0, or 0 on success
*/
static int rswitch_port_IOCTL(struct net_device * ndev, struct ifreq * rq, int cmd)
{

    
    struct port_private * mdp = netdev_priv(ndev);
    struct phy_device     * phydev = mdp->phydev;
    printk("Port IOCTL called with cmd=%x \n", cmd);
    if (NOT netif_running(ndev))
    {
        return -EINVAL;
    }

    switch (cmd)
    {
        
    case SIOCGHWTSTAMP:
        return rswitch_hwtstamp_get(ndev, rq);
    case SIOCSHWTSTAMP:
        return rswitch_port_hwtstamp_set(ndev, rq, cmd);

    default:
        return phy_mii_ioctl(phydev, rq, cmd);
        return -1;
    }
}



const struct file_operations    drv_FileOps = 
{
    .owner            = THIS_MODULE,
    .open             = NULL,
    .release          = NULL,
    .unlocked_ioctl   = drv_IOCTL,
    .compat_ioctl     = drv_IOCTL,
};

/**
    @brief  Enable Interrupt

    @param  void

    @return < 0, or 0 on success
*/
static int rswitch_eth_enable_interrupt(void)
{
    struct net_device * ndev;
    int i  = 0;

    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_EIE0);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_EIE1);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_TCEIE0);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_RFSEIE0);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_RFEIE0);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_RCEIE0);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_RIDAOIE);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_TSFEIE);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_TSCEIE);

    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_RDIE0);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_TDIE0);
    eth_gwca_reg_write(0x01,RSWITCH_AXI_TSDIE);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_AXI_DIE);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_GWCA_RDIE);
    eth_gwca_reg_write(0x1,RSWITCH_GWCA_REIE0);
    eth_gwca_reg_write(0xFFFFFFFF,RSWITCH_GWCA_REIE1);
    eth_gwca_reg_write(0x1,RSWITCH_GWCA_RMIE0);

    eth_gwca_reg_write(0x01,RSWITCH_GWCA_GWEIE0);
    eth_gwca_reg_write(0x01,RSWITCH_GWCA_GWEIE1);
    for(i = 0; i < CONFIG_RENESAS_RSWITCH_ETH_PORTS; i++)
    {
        ndev = ppndev[i];
        portreg_write(ndev, 0x1FFFFF, RSWITCH_EAEIE0);
        portreg_write(ndev, 0xFFFFFFFF, RSWITCH_EAEIE1);
        portreg_write(ndev, 0xFFFFFF, RSWITCH_EAEIE2);
    }

    return 0;
}



/**
    @brief  Device Initialise

    @param  ndev    net_device for the port device

    @return < 0, or 0 on success
*/
static int port_dev_Init(struct net_device * ndev)
{
    
    
    

    
    
    int                     ret = 0;
    

    /* Enable Promiscous mode */
    portreg_write(ndev, 0x00070007, RSWITCH_RMAC_MRAFC);
    
    
    
    

    
    
    
    
    
    /* Set MAC address */
    port_Set_MAC_Address(ndev);




    return ret;
}


/** 
    @brief Network device open 

    @param  ndev    net_device for the individual switch port device

    @return < 0, or 0 on success
*/
static int port_Open(struct net_device * ndev)
{
    struct port_private * mdp = netdev_priv(ndev);
    int     ret = 0;
    
    u32     TAS_Enable = 0;
    
    u32     PortIndex  = 0;
    u32     link_verification  = 0;
    u32     k  = 0;
    u32     MLVC = 0;
    
#ifdef INTERNAL_GATEWAY_PORT
    if(mdp->PortNumber == Board_Config.Eth_Ports)
    {
        napi_enable(&mdp->napi[0]);
        printk("Napi Enabled for GW Port \n");
        netif_start_queue(ndev);

        mdp->Opened = true;
        return ret;
    }
#endif
    for(PortIndex = 0; PortIndex<Config.Ports;PortIndex++)
    {
        if(mdp->PortNumber == Config.Port[PortIndex].PortNumber)
        {
            break;

        }
    }


    { 
        
        napi_enable(&mdp->napi[0]);
        napi_enable(&mdp->napi[1]);
        /* device init */
        ret = port_dev_Init(ndev);
        if (ret != 0)
        {
            port_SetState(ndev, RSWITCH_PortState_Failed);
            return ret;
        }

        
        
        
        // Change Port from CONFIG to OPERATE mode
        port_ChangeState(ndev, RSWITCH_PortState_Operate);

        

        ptp_rswitch_init(ndev, mdp->pdev);
        port_TxRx_Enable(ndev);
        
        
        portreg_write(ndev,  0x03, RSWITCH_RMAC_MCC);
        
        
        TAS_Enable = portreg_read(ndev,  RSWITCH_EAMIS);;
        
        if(TAS_Enable == 0)
        {
            
            if (Config_New.Port[PortIndex].TxParam.TAS.bEnable)
            {
                
                
                ret = Config_Enable_TAS(ndev, &Config_New.Port[PortIndex].TxParam.TAS);
                
                if(ret < 0)
                {

                    printk("TAS did not configured \n");
                }

            }
        }
        
        
        netif_start_queue(ndev);

        mdp->Opened = true;
        /* Preemption Support Start link verification with 128ms response time, if fails SW does it */
        /* Enable both EMAC and PMAC */
        MLVC = portreg_read(ndev, RSWITCH_RMAC_MLVC);
        MLVC |= (1 << 8) | (1 << 17) | 0x7F;
        portreg_write(ndev,MLVC, RSWITCH_RMAC_MLVC);
        
        for (k = 0; k < LINK_VERIFICATION_CONFIG_TIMEOUT_MS; k++) 
        {
            MLVC = portreg_read(ndev, RSWITCH_RMAC_MLVC);
            if((MLVC & 0x10000) != 0x0)
            {
                link_verification = 1;
                break;

            }
            mdelay(1);

        } 

        if((link_verification) == 0x0)
        {
            printk("Link Verification failed for PMAC support for %s, Falling back on SW Method \n", ndev->name);
            MLVC |= (1 << 8) | (1 << 16) | 0x7F;
            portreg_write(ndev,MLVC, RSWITCH_RMAC_MLVC);
        }
        else
        {
            printk("Link Verification Passed for PMAC support for %s\n",ndev->name);

        }

        
        
    }
    
    return ret;
}


/** 
    @brief  Device close

    @param  ndev    net_device for the individual switch port device 

    @return < 0, or 0 on success
*/
static int port_Close(struct net_device * ndev)
{
    struct port_private    * mdp = netdev_priv(ndev);
    struct rswitch_tstamp_skb *ts_skb, *ts_skb2;

    

    if (NOT mdp->Opened)
    {
        printk("[RSWITCH] ERROR: Attempting to close netdev (%s) when already closed\n", ndev->name);
        return -ENODEV;
    }
    
    if(mdp->PortNumber == Board_Config.Eth_Ports)
    {
        napi_disable(&mdp->napi[0]);
        netif_stop_queue(ndev);
        
    }
    else
    {
        list_for_each_entry_safe(ts_skb, ts_skb2, &mdp->ts_skb_list, list) {
            list_del(&ts_skb->list);
            kfree(ts_skb);
        }
        napi_disable(&mdp->napi[0]);
        napi_disable(&mdp->napi[1]);
        
        




        
        

        netif_stop_queue(ndev);

        
        
        

        // FPGA DEBUG ONLY: EthAgent_Close(ndev);

        /* Stop the E-MAC's Rx & Tx processes. */
        port_TxRx_Disable(ndev);

        /* Wait stopping the Rx eMAC & pMAC process */
        


        
        

        /* 
        PHY Disconnect (Internal port has no PHY)
        */
        if (mdp->phydev != NULL) 
        {
            phy_stop(mdp->phydev);
            phy_disconnect(mdp->phydev);
        }

        free_irq(Board_Config.Eth_Port[mdp->PortNumber].Virtual_IRQ, ndev);
        

        // Change Port to CONFIG
        port_ChangeState(ndev, RSWITCH_PortState_Config);
    }
    mdp->Opened = false;

    return 0;
}


/** 
    @brief Query device statistics

    @param  ndev    net_device for the individual switch port device 

    @return pointer to stats in net_device
*/
static struct net_device_stats  * port_Get_Statistics(struct net_device * ndev)
{
    struct port_private     * mdp = netdev_priv(ndev);
    struct net_device_stats * nstats;
    struct net_device_stats * Stats = &mdp->Stats;



    nstats = &ndev->stats;
    nstats->rx_packets = Stats->rx_packets;
    nstats->tx_packets = Stats->tx_packets;
    nstats->rx_bytes = Stats->rx_bytes;
    nstats->tx_bytes = Stats->tx_bytes;


    return &ndev->stats;
}


/**
    @brief  Device operations
*/
static const struct net_device_ops  port_netdev_ops = 
{
    .ndo_open               = port_Open,         // Called when the network interface is up'ed
    .ndo_stop               = port_Close,        // Called when the network interface is down'ed
    .ndo_do_ioctl           = rswitch_port_IOCTL,
    .ndo_start_xmit         = port_Start_Xmit,   // Start the transmission of a frame
    
    .ndo_get_stats          = port_Get_Statistics,
    .ndo_validate_addr      = eth_validate_addr,
    .ndo_set_mac_address    = eth_mac_addr,             // Use default function to set dev_addr in ndev
    .ndo_change_mtu         = eth_change_mtu  // Set new MTU size
};


// ------------------------------------------------------------------------------------ 
//
//      Port ethtool Operations
//
// ------------------------------------------------------------------------------------ 



/**
    @brief  ethtool operation - Get Port parameters

    @param  ndev    net_device for the individual switch port device 
*/
static int port_get_settings(struct net_device * ndev, struct ethtool_cmd * ecmd)
{
    struct port_private * mdp = netdev_priv(ndev);
    
    int           ret;

    if (mdp->Opened != true)
    {
        printk("[RSWITCH] ERROR: %s is not open \n", ndev->name);
        return -EOPNOTSUPP;
    }

    return ret;
}


/**
    @brief  ethtool operation - Set Port parameters

    @note
    - Duplex cannot be set as only FULL duplex is supported by RMAC

    @param  ndev    net_device for the individual switch port device
*/
static int port_set_settings(struct net_device * ndev, struct ethtool_cmd * ecmd)
{
    struct port_private * mdp = netdev_priv(ndev);
    unsigned long           flags = 0;
    int                     ret = 0;

    if (NOT mdp->Opened)
    {
        return -EAGAIN;
    }

    if (mdp->phydev != NULL)
    {
        spin_lock_irqsave(&port_lock, flags);

        ret = phy_ethtool_sset(mdp->phydev, ecmd);
        if (ret != 0)
        {
            mdelay(1);
            spin_unlock_irqrestore(&port_lock, flags);
            return ret;
        }

        mdelay(1);

        spin_unlock_irqrestore(&port_lock, flags);
    }
    else
    {
        printk("[RSWITCH] ERROR: %s is not open \n", ndev->name);
        return -EOPNOTSUPP;
    }

    return ret;
}

/**
    @brief  Query Phy State (Not Supprted)
*/
extern const char * port_QueryPHYState(enum phy_state State)
{
    switch (State)
    {
    case PHY_DOWN:          return "Down";
    case PHY_STARTING:      return "Starting";
    case PHY_READY:         return "Ready";
    case PHY_PENDING:       return "Pending";
    case PHY_UP:            return "Up";
    case PHY_AN:            return "An";
    case PHY_RUNNING:       return "Running";
    case PHY_NOLINK:        return "No LInk";
    case PHY_FORCING:       return "Forcing";
    case PHY_CHANGELINK:    return "Change Link";
    case PHY_HALTED:        return "Halted";
    case PHY_RESUMING:      return "Resuming";
    default:                return "INVALID";
    }
}





/**
    @brief  ethtool operation - 

    @param  ndev    net_device for the individual switch port device
*/
static int port_nway_reset(struct net_device *ndev)
{
    struct port_private * mdp = netdev_priv(ndev);
    unsigned long           flags;
    int                     ret = 0;

    if (mdp->phydev != NULL)
    {
        spin_lock_irqsave(&port_lock, flags);
        ret = phy_start_aneg(mdp->phydev);
        spin_unlock_irqrestore(&port_lock, flags);
    }

    return ret;
}


/**
    @brief  ethtool operation - get message level

    @param  ndev    net_device for the individual switch port device 
*/
static u32 port_get_msglevel(struct net_device * ndev)
{
    struct port_private *mdp = netdev_priv(ndev);

    return mdp->msg_enable;
}


/**
    @brief  ethtool operation - set message level

    @param  ndev    net_device for the individual switch port device 
    @param  value
*/
static void port_set_msglevel(struct net_device * ndev, u32 value)
{
    struct port_private *mdp = netdev_priv(ndev);

    mdp->msg_enable = value;
}


static const char port_gstrings_stats[][ETH_GSTRING_LEN] = 
{
    "Rx ----\n     "
    "Packets",
    "Bytes",
    "RMAC\n     "
    "  CRC Error Frames",   
    "  Frame Errors",
    "  False Carrier Indication",
    "  Undersize Frames",
    "  Residual-Bit Frames",
    "  Good Frames",
    "  Broadcast Address Frames",
    "  Multicast Address Frames",
    "  VLAN TAG Frames",
    "  Energy Efficient Events",
    "  Frame Assembly Erros",
    "  Frame SMD Errors",
    "  Frame Assembly OK",
    "  Fragments",
    "  Bytes",
    "  MAC Merge Hold",
    "Tx ----\n     "
    "Packets",
    "Bytes",
    "Agent Outstanding\n     "
    "  Queue 0",
    "  Queue 1",
    "  Queue 2",
    "  Queue 3",
    "  Queue 4",
    "  Queue 5",
    "  Queue 6",
    "  Queue 7",
    "RMAC\n     "
    "  Undersize Frames",
    "  Oversize Frames",
    "  Successful Frames",
    "  Fragments",
    "  Bytes",
};


#define RSWITCH_PORT_STATS_LEN_AVB  ARRAY_SIZE(port_gstrings_stats)



/**
    @brief  ethtool operation Get String(Not Supported)

    @param  ndev

    @param  stringset
    
    @param  data

    @return void
*/
static void port_get_strings(struct net_device *ndev, u32 stringset, u8 *data)
{
    switch (stringset) 
    {
    case ETH_SS_STATS:
        memcpy(data, *port_gstrings_stats, sizeof(port_gstrings_stats));
        break;
    }
}



/**
    @brief  ethtool operation Get Self test COUNT(Not Supported)

    @param  netdev

    @param  sset


    @return 0
*/
static int port_get_sset_count(struct net_device *netdev, int sset)
{
    switch (sset) 
    {
    case ETH_SS_STATS:
        return RSWITCH_PORT_STATS_LEN_AVB;
    default:
        return -EOPNOTSUPP;
    }
}




/**
    @brief  ethtool operation Get Network Stats

    @param  ndev

    @param  stats

    @param  data

    @return void
*/
static void port_get_ethtool_stats(struct net_device *ndev, struct ethtool_stats * stats, u64 * data)
{
    struct port_private     * mdp = netdev_priv(ndev);
    struct net_device_stats * Stats = &mdp->Stats;

    int    i = 0;

    data[i++] = Stats->rx_packets;
    data[i++] = Stats->rx_bytes;

    
}



/**
    @brief  Query timestamping capabilities of port

    Return to the socket layer the port timestamping capabilities
    - External AVB ports. Show we can do hardware timestamping
    - Internal AVB port. Show we cannot do any hardware timestamping

    @param  ndev    net_device for the individual switch port device 'avb{n}'
    @param  tsinfo

    @return < 0, or 0 on success
*/
static int rswitch_port_get_ts_info(struct net_device * ndev, struct ethtool_ts_info * tsinfo)
{
    struct port_private * mdp = netdev_priv(ndev);

    /*
        SOF_TIMESTAMPING_TX_HARDWARE:  try to obtain send time stamp in hardware
        SOF_TIMESTAMPING_TX_SOFTWARE:  if SOF_TIMESTAMPING_TX_HARDWARE is off or fails, then do it in software
        SOF_TIMESTAMPING_RX_HARDWARE:  return the original, unmodified time stamp as generated by the hardware
        SOF_TIMESTAMPING_RX_SOFTWARE:  if SOF_TIMESTAMPING_RX_HARDWARE is off or fails, then do it in software
        SOF_TIMESTAMPING_RAW_HARDWARE: return original raw hardware time stamp
        SOF_TIMESTAMPING_SYS_HARDWARE: return hardware time stamp transformed to the system time base
        SOF_TIMESTAMPING_SOFTWARE:     return system time stamp generated in software

        The bits in the 'tx_types' and 'rx_filters' fields correspond to the 'hwtstamp_tx_types' and 
        'hwtstamp_rx_filters' enumeration values,
    */
    if(mdp->PortNumber != Board_Config.Eth_Ports)
    {
        if(mdp->PortState != RSWITCH_PortState_Operate)
        {
            printk("ethtool:Info: Device not Up PTP Clock not set, showing Default\n");
            tsinfo->phc_index = 0;
            return 0;
        }
        tsinfo->phc_index = ptp_clock_index(mdp->ptp.clock);
        
        tsinfo->so_timestamping = SOF_TIMESTAMPING_TX_HARDWARE | 
        SOF_TIMESTAMPING_RX_HARDWARE |
        SOF_TIMESTAMPING_SOFTWARE |
        SOF_TIMESTAMPING_TX_SOFTWARE |
        SOF_TIMESTAMPING_RX_SOFTWARE |
        SOF_TIMESTAMPING_RAW_HARDWARE;
        tsinfo->tx_types = (1 << HWTSTAMP_TX_OFF) |
        (1 << HWTSTAMP_TX_ON);
        tsinfo->rx_filters = (1 << HWTSTAMP_FILTER_NONE) |
        (1 << HWTSTAMP_FILTER_ALL);
        

    }
    else
    {
        if(mdp->PortState != RSWITCH_PortState_Operate)
        {
            printk("ethtool:Info: Device not Up PTP Clock not set, showing Default\n");
            tsinfo->phc_index = 0;
            return 0;
        }
        tsinfo->phc_index = 0;
        return 0;

    }



    

    return 0;
}







/**
    @brief  ethtool operations
*/
static const struct ethtool_ops     port_ethtool_ops = 
{
    .get_settings       = port_get_settings,
    .set_settings       = port_set_settings,
    .nway_reset         = port_nway_reset,
    .get_msglevel       = port_get_msglevel,
    .set_msglevel       = port_set_msglevel,
    .get_link           = ethtool_op_get_link,
    .get_strings        = port_get_strings,
    .get_ethtool_stats  = port_get_ethtool_stats,
    .get_sset_count     = port_get_sset_count,
    .get_ts_info        = rswitch_port_get_ts_info,
    
};





// ------------------------------------------------------------------------------------ 
//
//      Driver Load & Unload
//
// ------------------------------------------------------------------------------------ 



/**
    @brief  Platform Device ID Table
*/
static struct platform_device_id    rswitchtsn_ID_Table[] = 
{
    { RSWITCH_FPGA_ETH_PLATFORM,        (kernel_ulong_t)NULL },
    { }
};
MODULE_DEVICE_TABLE(platform, rswitchtsn_ID_Table);


/**
    @brief  Gain access to physical memory areas - EThernet Ports, GGIS and for ALTERA board (LED controller)

    @param  pdev    The device to be instantiated
    @return 0, or < 0 on error
*/
static int drv_Probe_IORemap(struct platform_device * pdev)
{
    
    int     err = 0;
    int     a = 0;
    
    /* AK:Porting Will get memory as PCI driver rather than Platform driver will be checked once debug driver passess */

    
    /*
        Get Base addresses and size for each Ethernet AVB/TSN port
    */
    for (a = 0; a < Board_Config.Eth_Ports; a++)
    {
        
        {
            Board_Config.Eth_Port[a].Start = (RSWITCH_FPGA_ETH_BASE + ( a * RSWITCH_FPGA_ETH_PORT_SIZE));
            Board_Config.Eth_Port[a].Size = RSWITCH_FPGA_ETH_PORT_SIZE ;
            Board_Config.Eth_Port[a].End =  Board_Config.Eth_Port[a].Start + RSWITCH_FPGA_ETH_PORT_SIZE;
            EthPort_Addr[a] = ioaddr + RSWITCH_FPGA_ETH_OFFSET + (RSWITCH_FPGA_ETH_PORT_SIZE * a);
            
#ifdef _DEBUG
            printk("[RSWITCH] IO Remap Port(%u) Address(0x%08X - 0x%08X) to (0x%08lX - 0x%08lX) \n", 
            a, Board_Config.Eth_Port[a].Start, Board_Config.Eth_Port[a].End, (uintptr_t)EthPort_Addr[a], (uintptr_t)EthPort_Addr[a] + Board_Config.Eth_Port[a].Size - 1);
#endif
        }
    }

    /*
        Get Base addresses and size for GW CPU Area
    */
    
    Board_Config.GWCA.Start = RSWITCH_FPGA_GWCA_BASE;
    Board_Config.GWCA.Size = RSWITCH_FPGA_GWCA_SIZE;
    Board_Config.GWCA.End = Board_Config.GWCA.Start + RSWITCH_FPGA_GWCA_SIZE;
    GWCA_Addr = ioaddr + RSWITCH_FPGA_GWCA_OFFSET ;
    
#ifdef _DEBUG
    printk("[RSWITCH] IO Remap GWCA Address(0x%08X - 0x%08X) to (0x%08lX - 0x%08lX) \n", 
    Board_Config.GWCA.Start, Board_Config.GWCA.Start + RSWITCH_FPGA_GWCA_SIZE, (uintptr_t)GWCA_Addr, (uintptr_t)GWCA_Addr + Board_Config.GWCA.Size - 1);
#endif
    
    rswitch_drv_Probe_GetInterrupts(pdev);
    


    

    return err;
}




/**
    @brief  Platform Driver Create Network Devices for gw port

    @param  ppndev

    @param  PortNumber

    @param  pdev The device to be instantiated

    @return 0, or < 0 on error
*/
static int drv_Probe_CreateVirtualNetdev(struct platform_device * pdev, uint32_t PortNumber, struct net_device ** ppndev)
{
    struct  net_device       * ndev = NULL;
    struct  port_private     * mdp = NULL;
    char                       PortName[IFNAMSIZ];
    int     err = 0;


    
    

    /*
        Allocate the next tsn{n} device. Sets several net_device fields with appropriate 
        values for Ethernet devices. This is a wrapper for alloc_netdev
    */

    
    ndev = alloc_etherdev(sizeof(struct port_private));
    
    if (ndev == NULL)
    {
        dev_err(&pdev->dev, "[RSWITCH-ETH] Failed to allocate %s device\n", PortName);
        return -ENOMEM;
    }


    ndev->dma = 1;                                                 // net_device: DMA Not used on this architecture

    /* Software Internal GW port know as tsngw */
    strncpy(ndev->name, "tsngw", sizeof(ndev->name)-1);
    
    
    /* 
        Set the sysfs physical device reference for the network logical device
        if set prior to registration will cause a symlink during initialization.
    */    
    SET_NETDEV_DEV(ndev, &pdev->dev);

    /* 
        Fill in the fields of the device structure with ethernet values. 
    */
    ether_setup(ndev);

    /*
        Access private data within net_device and initialise it
    */
    mdp = netdev_priv(ndev);
    
    /* Allocate the port number as the one more than total number of physical ports */
    mdp->PortNumber = Board_Config.Eth_Ports;

    
    mdp->pdev = pdev;
    mdp->num_tx_ring[RSWITCH_BE] = TX_RING_SIZE0;

    mdp->num_rx_ring[RSWITCH_BE] = RX_RING_SIZE0;
    
    
    
    
    num_rx_ring[2] = RX_RING_SIZE1;
    
    
    
    mdp->ether_link_active_low  = false;

    mdp->edmac_endian           = EDMAC_LITTLE_ENDIAN;
    mdp->PortReg_offset         = port_RegOffsets;    // Registers relative to this port
    
    /* 
        set function (Open/Close/Xmit/IOCTL etc)
    */
    ndev->netdev_ops = &port_netdev_ops;
    ndev->ethtool_ops = &port_ethtool_ops;

    /* debug message level */
    mdp->msg_enable = RSWITCH_DEF_MSG_ENABLE;

    /* Initialise HW timestamp list */
    INIT_LIST_HEAD(&mdp->ts_skb_list);
    
    /* 
        Read MAC address from Port. If invalid (not 00:00:00:00:00:00, is not a multicast address, 
        and is not FF:FF:FF:FF:FF:FF), generate one
    */
    /* Use Automatic generated MAC address */
    port_Generate_MAC_Address(PortNumber, ndev);
    

    /* 
        Network device register
    */
    err = register_netdev(ndev);
    
    if (err != 0)
    {
        dev_err(&pdev->dev, "[RSWITCH-ETH] %s Failed to register net device. err(%d)\n", ndev->name, err);
        free_netdev(ndev);
        return err;
    }

    

    /* Save this network_device in the list of devices */
    *ppndev = ndev;
    
    netif_napi_add(ndev, &mdp->napi[0], rswitch_poll, 64);
    printk("Internal SW Gateway Port Registered \n");
    return err;
}




/**
    @brief  Platform Driver Descriptor Initiaisation

    @param  pdev The device to be instantiated

    @return 0, or < 0 on error
*/
static int desc_init(struct platform_device * pdev)
{
    int q,i  = 0;
    int err = 0;
    
    /* Allocate RX descriptor base address table  -1 as 1 will be used as special descriptor for timestamp*/
    /* Add one more descriptor for Normal frames Not PTP & SRP */
    for(i  = 0; i < 4; i++)
    {
        rx_desc_bat_size = sizeof(struct rswitch_ext_ts_desc) * (RX_DBAT_ENTRY_NUM );
        rx_desc_bat[i] = dma_alloc_coherent(&pcidev->dev, rx_desc_bat_size,
        &rx_desc_bat_dma[i], GFP_KERNEL);
        
        
        if (!rx_desc_bat[i]) {
            dev_err(&pdev->dev,
            "Cannot allocate desc base address table (size %d bytes)\n",
            rx_desc_bat_size);
            err = -ENOMEM;
            return err;
            
        }
        for (q = RSWITCH_BE; q < RX_DBAT_ENTRY_NUM; q++)
        rx_desc_bat[i][q].die_dt = DT_EOS;
        /* map the rdbt pointer */
        
    }

    ts_desc_bat_size = sizeof(struct rswitch_ext_ts_desc) * (TS_DBAT_ENTRY_NUM );
    ts_desc_bat = dma_alloc_coherent(&pcidev->dev, ts_desc_bat_size,
    &ts_desc_bat_dma, GFP_KERNEL);
    
    
    if (!ts_desc_bat) {
        dev_err(&pdev->dev,
        "Cannot allocate desc base address table (size %d bytes)\n",
        ts_desc_bat_size);
        err = -ENOMEM;
        return err;
        
    }
    
    ts_desc_bat->die_dt = DT_EOS;
    
    

    /* Allocate TX descriptor base address table */

    for(i  = 0; i < 15; i++)
    {
        tx_desc_bat_size = (sizeof(struct rswitch_desc)) * (TX_DBAT_ENTRY_NUM * 2 + 1) ;
        
        
        tx_desc_bat[i] = dma_alloc_coherent(&pcidev->dev, tx_desc_bat_size,
        &tx_desc_bat_dma[i], GFP_KERNEL);

        if (!tx_desc_bat[i]) {
            dev_err(&pdev->dev,
            "Cannot allocate desc base address table (size %d bytes)\n",
            tx_desc_bat_size);
            err = -ENOMEM;
            return err;

        }
        for (q = RSWITCH_BE; q < TX_DBAT_ENTRY_NUM; q++)
        tx_desc_bat[i][q].die_dt = DT_EOS;
        gtx_desc_bat_dma[i] = tx_desc_bat_dma[i];
    }
    
    return err; 



}



/**
    @brief  Platform Driver Proc Statistics Assign Error Name

    

    @param  PortNumber

    

    @return 0, or < 0 on error
*/
static int rswitch_eth_name_error(u32 PortNumber)
{
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[0].ErrorName , "RPCES_Err");   
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[1].ErrorName , "RPNAES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[2].ErrorName , "RPPES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[3].ErrorName , "RPFFES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[4].ErrorName , "RPOES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[5].ErrorName , "RPUES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[6].ErrorName , "RPAES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[7].ErrorName , "RPPOES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[8].ErrorName , "RECES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[9].ErrorName , "RENAES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[10].ErrorName , "REPES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[11].ErrorName , "REFFES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[12].ErrorName , "REUES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[13].ErrorName , "REUES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[14].ErrorName , "reserve"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[15].ErrorName , "REPOES_Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[16].ErrorName , "TxECCErr"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[17].ErrorName , "ECCfatalErr"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[18].ErrorName , "TASGateErr"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[19].ErrorName , "TASConfigErr"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS0_error[20].ErrorName , "ECCTASErr"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[0].ErrorName , "reserved");   
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[1].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[2].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[3].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[4].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[5].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[6].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[7].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[8].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[9].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[10].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[11].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[12].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[13].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[14].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[15].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[16].ErrorName , "TQSMES0Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[17].ErrorName , "TQSMES1Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[18].ErrorName , "TQSMES2Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[19].ErrorName , "TQSMES3Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[20].ErrorName , "TQSMES4Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[21].ErrorName , "TQSMES5Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[22].ErrorName , "TQSMES6Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[23].ErrorName , "TQSMES7Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[24].ErrorName , "CULES0Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[25].ErrorName , "CULES1Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[26].ErrorName , "CULES2Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[27].ErrorName , "CULES3Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[28].ErrorName , "CULES4Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[29].ErrorName , "CULES5Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[30].ErrorName , "CULES6Err");
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS1_error[31].ErrorName , "CULES7Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[0].ErrorName , "WMCLOS0Err");   
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[1].ErrorName , "WMCLOS1Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[2].ErrorName , "WMCLOS2Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[3].ErrorName , "WMCLOS3Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[4].ErrorName , "WMCLOS4Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[5].ErrorName , "WMCLOS5Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[6].ErrorName , "WMCLOS6Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[7].ErrorName , "WMCLOS7Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[8].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[9].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[10].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[11].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[12].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[13].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[14].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[15].ErrorName , "reserved"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[16].ErrorName , "TDOVS0Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[17].ErrorName , "TDOVS1Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[18].ErrorName , "TDOVS2Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[19].ErrorName , "TDOVS3Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[20].ErrorName , "TDOVS4Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[21].ErrorName , "TDOVS5Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[22].ErrorName , "TDOVS6Err"); 
    strcpy(rswitch_errors.Port_Error[PortNumber].EAEIS2_error[23].ErrorName , "TDOVS7Err"); 
    return 0;
}


/**
    @brief  Platform Driver Create Network Devices for physical ports

    @param  ppndev

    @param  PortNumber

    @param  pdev The device to be instantiated

    @return 0, or < 0 on error
*/
static int drv_Probe_CreateNetdev(struct platform_device * pdev, uint32_t PortNumber, struct net_device ** ppndev)
{
    struct  net_device       * ndev = NULL;
    struct  port_private     * mdp = NULL;
    char                       PortName[IFNAMSIZ];
    int     err = 0;

    u32 MIR;
    u32 EAMS = 0;
    sprintf(PortName, "%s%u", RSWITCHETH_BASE_AVB_PORTNAME, PortNumber);
    

    /*
        Allocate the next tsn{n} device. Sets several net_device fields with appropriate 
        values for Ethernet devices. This is a wrapper for alloc_netdev
    */
    ndev = alloc_etherdev(sizeof(struct port_private));
    if (ndev == NULL)
    {
        dev_err(&pdev->dev, "[RSWITCH] Failed to allocate %s device\n", PortName);
        return -ENOMEM;
    }
    ndev = alloc_etherdev_mqs(sizeof(struct port_private),
    NUM_TX_QUEUE, NUM_RX_QUEUE);
    ndev->base_addr = Board_Config.Eth_Port[PortNumber].Start;            // net_device: The I/O base address of the network interface

    strncpy(ndev->name, PortName, sizeof(ndev->name)-1);
    strncpy(Board_Config.Eth_Port[PortNumber].PortName, PortName, sizeof(Board_Config.Eth_Port[PortNumber].PortName)-1);

    /* 
        Set the sysfs physical device reference for the network logical device
        if set prior to registration will cause a symlink during initialization.
    */    
    SET_NETDEV_DEV(ndev, &pdev->dev);

    /* 
        Fill in the fields of the device structure with ethernet values. 
    */
    ether_setup(ndev);

    /*
        Access private data within net_device and initialise it
    */
    mdp = netdev_priv(ndev);
    
    mdp->ndev = ndev;
    
    mdp->num_tx_ring[RSWITCH_BE] = TX_RING_SIZE0;
    mdp->num_tx_ring[RSWITCH_NC] = TX_RING_SIZE1;
    mdp->num_rx_ring[RSWITCH_BE] = RX_RING_SIZE0;
    
    mdp->num_rx_ring[RSWITCH_NC] = RX_RING_SIZE1;
    
    num_rx_ring[RSWITCH_BE] = RX_RING_SIZE0;
    
    num_rx_ring[RSWITCH_NC] = RX_RING_SIZE1;
    num_rx_ring[2] = RX_RING_SIZE2;
    num_ts_ring = TS_RING_SIZE;
    mdp->PortNumber = PortNumber;

    mdp->port_base_addr = EthPort_Addr[PortNumber];
    mdp->pdev = pdev;

    mdp->phy_id = Board_Config.Eth_Port[PortNumber].PHY_ID;
    mdp->phy_interface          = PHY_INTERFACE_MODE_RGMII_ID /*_ID */ ;
    
    mdp->ether_link_active_low  = false;

    mdp->edmac_endian           = EDMAC_LITTLE_ENDIAN;
    mdp->PortReg_offset         = port_RegOffsets;    // Registers relative to this port
    

    if ((err = port_ChangeState(ndev, RSWITCH_PortState_Disable)) < 0)
    {
        printk("Failed from Change State \n");
        free_netdev(ndev);
        return err;
    }
    


    /* Initialise HW timestamp list */
    INIT_LIST_HEAD(&mdp->ts_skb_list);
    if ((err = port_ChangeState(ndev, RSWITCH_PortState_Config)) < 0)
    {
        printk("Failed from Change State \n");
        free_netdev(ndev);
        return err;
    }
    /* 
        set function (Open/Close/Xmit/IOCTL etc)
    */
    ndev->netdev_ops = &port_netdev_ops;
    ndev->ethtool_ops = &port_ethtool_ops;

    /* debug message level */
    mdp->msg_enable = RSWITCH_DEF_MSG_ENABLE;

    
    /* 
        Read MAC address from Port. If invalid (not 00:00:00:00:00:00, is not a multicast address, 
        and is not FF:FF:FF:FF:FF:FF), generate one
    */
    port_Query_MAC_Address(ndev);
    if (NOT is_valid_ether_addr(ndev->dev_addr)) 
    {
        printk("[RSWITCH] %s no valid MAC address supplied, using a generated one\n", ndev->name);
        port_Generate_MAC_Address(PortNumber, ndev);
    }


    

    /* 
        Network device register
    */
    err = register_netdev(ndev);
    if (err != 0)
    {
        dev_err(&pdev->dev, "[RSWITCH] %s Failed to register net device. err(%d)\n", ndev->name, err);
        free_netdev(ndev);
        return err;
    }

    /* 
        MDIO bus initialise (Connect MAC to PHY)
    */
    

    // Port is now CONFIG
    port_ChangeState(ndev, RSWITCH_PortState_Config);
    err = port_MII_Initialise(ndev);
    if (err != 0)
    {
        dev_err(&pdev->dev, "[RSWITCH] %s Failed to initialise MDIO. %d\n", ndev->name, err);
        unregister_netdev(ndev);
        free_netdev(ndev);
        return err;
    }


    /* print device information */
    pr_info("[RSWITCH] %s Base address at 0x%x MAC(%pM) VRR(%08X)\n", ndev->name, (u32)ndev->base_addr, ndev->dev_addr, mdp->VRR);

    /* Save this network_device in the list of devices */
    *ppndev = ndev;

    if(board_variant == BOARD_VARIANT_H3VC3) {
        MIR = RSWITCH_MIR_RGMII;
    } else {  /* BOARD_VARIANT_H3VC2 */
        MIR = RSWITCH_MIR_MII;
    }
    portreg_write(ndev, MIR, RSWITCH_RMAC_MPIC);

    //MIR = portreg_read(ndev, RSWITCH_RMAC_MPIC);
    EAMS = portreg_read(ndev, RSWITCH_EAMS);
    
    netif_napi_add(ndev, &mdp->napi[0], rswitch_poll, 64);
    netif_napi_add(ndev, &mdp->napi[1], rswitch_poll, 64);
    rswitch_eth_name_error(PortNumber);
    
    return err;
}









/**
    @brief  Remove Network Devices
*/
static int drv_Unload_Remove_Netdevs(void)
{
    struct net_device     * ndev = NULL;
    struct port_private   * mdp = NULL;
    int i = 0;
    int q  = 0;
    int     a = 0;
    int     err = 0;
    
    for (a = 0; a <= Board_Config.Eth_Ports; a++)
    {
        ndev = ppndev[a];
        
        if (ndev == NULL)
        {
            continue;
        }
        if(a != Board_Config.Eth_Ports)
        {
            rswitch_tx_ring_free(ppndev[a],RSWITCH_NC);
        }
        rswitch_tx_ring_free(ppndev[a],RSWITCH_BE);
        
        mdp = netdev_priv(ndev);
        if (mdp == NULL)
        {
            continue;
        }
        
        unregister_netdev(ndev);
        if(a != Board_Config.Eth_Ports)
        {
            netif_napi_del( &mdp->napi[1]);
        }
        netif_napi_del( &mdp->napi[0]);
        free_netdev(ndev);
        
        
        
        ppndev[a] = NULL;

        
    }
    rswitch_rx_ring_free(0);
    rswitch_rx_ring_free(1);
#ifdef INTERNAL_GATEWAY_PORT
    /* Add RX Ring Format for GW Port */
    rswitch_rx_ring_free(2);
#endif
    rswitch_ts_ring_free();
    for(i  = 0; i < 4; i++)
    {
        rx_desc_bat_size = sizeof(struct rswitch_ext_ts_desc) * (RX_DBAT_ENTRY_NUM );
        dma_free_coherent(&pcidev->dev, rx_desc_bat_size,rx_desc_bat[i],
        rx_desc_bat_dma[i]);
        rx_desc_bat[i] = NULL;

    }   
    ts_desc_bat_size = sizeof(struct rswitch_ext_ts_desc) * (TS_DBAT_ENTRY_NUM );
    dma_free_coherent(&pcidev->dev, ts_desc_bat_size, ts_desc_bat,
    ts_desc_bat_dma);
    
    
    
    
    ts_desc_bat = NULL;
    for(i  = 0; i < 15; i++)
    {
        tx_desc_bat_size = (sizeof(struct rswitch_desc)) * (TX_DBAT_ENTRY_NUM * 2 + 1) ;
        
        
        dma_free_coherent(&pcidev->dev, tx_desc_bat_size,
        tx_desc_bat[i], tx_desc_bat_dma[i]);

        
        for (q = RSWITCH_BE; q < TX_DBAT_ENTRY_NUM; q++)
        tx_desc_bat[i] = NULL;
        
    }
    return err;
}


/**
    @brief  Add character device for IOCTL interface

    @return < 0, or 0 on success
*/
static int drv_Probe_Create_chrdev(void)
{
    struct device *dev;
    int            ret = 0;

    /* 
        Create class 
    */
    rswitchtsn_DevClass = class_create(THIS_MODULE, RSWITCHETH_CLASS);
    if (IS_ERR(rswitchtsn_DevClass)) 
    {
        ret = PTR_ERR(rswitchtsn_DevClass);
        pr_err("[RSWITCH] failed to create '%s' class. rc=%d\n", RSWITCHETH_CLASS, ret);
        return ret;
    }

    if (rswitchtsn_DevMajor != 0) 
    {
        rswitchtsn_Dev = MKDEV(rswitchtsn_DevMajor, 0);
        ret = register_chrdev_region(rswitchtsn_Dev, 1, RSWITCHETH_CLASS);
    } 
    else
    {
        ret = alloc_chrdev_region(&rswitchtsn_Dev, 0, 1, RSWITCHETH_CLASS);
    }
    if (ret < 0) 
    {
        pr_err("[RSWITCH] failed to register '%s' character device. rc=%d\n", RSWITCHETH_CLASS, ret);
        class_destroy(rswitchtsn_DevClass);
        return ret;
    }
    rswitchtsn_DevMajor = MAJOR(rswitchtsn_Dev);

    cdev_init(&rswitchtsn_cDev, &drv_FileOps);
    rswitchtsn_cDev.owner = THIS_MODULE;

    ret = cdev_add(&rswitchtsn_cDev, rswitchtsn_Dev, RSWITCHETH_CTRL_MINOR + 1);
    if (ret < 0) 
    {
        pr_err("[RSWITCH] failed to add '%s' character device. rc=%d\n", RSWITCHETH_CLASS, ret);
        unregister_chrdev_region(rswitchtsn_Dev, 1);
        class_destroy(rswitchtsn_DevClass);
        return ret;
    }

    /* device initialize */
    dev = &rswitchtsn_device;
    device_initialize(dev);
    dev->class  = rswitchtsn_DevClass;
    dev->devt   = MKDEV(rswitchtsn_DevMajor, RSWITCHETH_CTRL_MINOR);
    dev_set_name(dev, RSWITCH_ETHERNET_DEVICE_NAME);

    ret = device_add(dev);
    if (ret < 0)
    {
        pr_err("[RSWITCH] failed to add '%s' device. rc=%d\n", RSWITCH_ETHERNET_DEVICE_NAME, ret);
        cdev_del(&rswitchtsn_cDev);
        unregister_chrdev_region(rswitchtsn_Dev, 1);
        class_destroy(rswitchtsn_DevClass);
        return ret;
    }

    return ret;
}


/**
    @brief  Remove character device

    @return < 0, or 0 on success
*/
static int drv_Unload_Remove_chrdev(void)
{
    int     ret = 0;

    if (rswitchtsn_DevClass == NULL)
    {
        return ret;
    }

    /*
        Remove character device
    */
    unregister_chrdev_region(rswitchtsn_Dev, 1);
    device_del(&rswitchtsn_device);
    cdev_del(&rswitchtsn_cDev);
    class_destroy(rswitchtsn_DevClass);
    
    return ret;
}


/**
    @brief  Unmap, mapped physical addresses
*/
static void drv_Unload_Unmap(void)
{
    int     a;


    for (a = 0; a < ARRAY_SIZE(EthPort_Addr); a++)
    {
        if (EthPort_Addr[a] != NULL)
        {
            
            EthPort_Addr[a] = NULL;
        }
    }


}



/**
    @brief  Driver Unload Cleanup
*/
static void drv_Unload_Cleanup(void)
{
    if (ppndev != NULL)
    {
        kfree(ppndev);
        ppndev = NULL;
    }

    drv_Unload_Unmap();
}




/**
    @brief  Platform Driver Query Configuration

    @param  void
    
    @return 0, or < 0 on error
*/
static int rswitchtsn_FPGA_QueryConfig(void)
{
    int     a = 0;

    /*
        Number of Ethernet ports
        PHY IDs
    */

    Board_Config.Eth_Ports = CONFIG_RENESAS_RSWITCH_ETH_PORTS;

    for (a = 0; a < Board_Config.Eth_Ports; a++)
    {
#ifdef BOARD_TYPE_SNOW
        Board_Config.Eth_Port[a].PHY_ID = 0x00;
#else
        Board_Config.Eth_Port[a].PHY_ID = a;
#endif /* BOARD_TYPE_SNOW */
    }

    return 0;
}




/**
    @brief  Platform Driver Load

    @param  pdev    The device to be instantiated
    
    @return 0, or < 0 on error
*/
static int rswitchtsn_drv_Probe(struct platform_device * pdev)
{
    int     err = 0;
    int     a = 0;
    const char *prop_compatibel_str = NULL;

    if(bitfile_version < RSWITCH_BITFILE_VERSION)
    {
        printk("\n[RSWITCH] Cetitec (%s %s) version %s, Probing '%s' failed due to Old Bitfile version %x , Please reprogram with newer FPGA Greater than or equal to %x\n",
        __DATE__, __TIME__, RSWITCHETH_DRIVER_VERSION, pdev->name, bitfile_version, RSWITCH_BITFILE_VERSION);
        return err;
    }
    printk("\n[RSWITCH] Cetitec (%s %s) version %s, Probing '%s' .....\n", __DATE__, __TIME__, RSWITCHETH_DRIVER_VERSION, pdev->name);
    
    err = of_property_read_string(of_root, "compatible", &prop_compatibel_str);
    if((err != 0) || (prop_compatibel_str == NULL)) {
        printk("[RSwitch]: Fatal couldn't get root property 'compatible'.\n");
        return err;
    }
    if(strcmp(prop_compatibel_str, MODEL_COMPATIBLE_H3VC3) == 0) {
        board_variant = BOARD_VARIANT_H3VC3;
    } else if (strcmp(prop_compatibel_str, MODEL_COMPATIBLE_H3VC2) == 0) {
        board_variant = BOARD_VARIANT_H3VC2;
    } else {
        printk("[RSwitch]: Fatal couldn't detect board variant. Neither H3VC2 nor H3VC3.\n");
        err = -ENODEV;
        return err;
    }
    printk("[RSwitch]: Detected board variant '%s'\n", prop_compatibel_str);


    memset(&Board_Config, 0, sizeof(Board_Config));
    memset(&Config, 0, sizeof(Config));
    memset(&Config_New, 0, sizeof(Config_New));
    memset(EthPort_Addr, 0, sizeof(EthPort_Addr));
    /* Allocate one more ppndev for Internal SW gateway port */
    ppndev = kmalloc(sizeof(struct net_device *) * (CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1), GFP_KERNEL);
    if ((IS_ERR(ppndev)) != 0) 
    {
        dev_err(&pdev->dev, "[RSWITCH] kmalloc for %u bytes FAILED \n", (unsigned int)sizeof(struct net_device *) * ( CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1));
        err = PTR_ERR(ppndev);
        return err;
    }
    memset(ppndev, 0, sizeof(struct net_device *) * (CONFIG_RENESAS_RSWITCH_ETH_PORTS + 1));

    
    if ((err = rswitchtsn_FPGA_QueryConfig()) < 0)
    {
        return err;
    }

    /*
        Gain access to physical memory areas - Ethernet Ports, GGIS and for ALTERA board (LED controller)
    */
    err = drv_Probe_IORemap(pdev);
    if (err < 0)
    {
        drv_Unload_Cleanup();
#ifdef CONFIG_RENESAS_RSWITCH_ETH_STATS
        rswitch_eth_proc_Terminate();
#endif
        return err;
    }

    /*
        Create netdevs - one per physical port
    */
    for (a = 0; a < Board_Config.Eth_Ports; a++)
    {
        
        err = drv_Probe_CreateNetdev(pdev, a, &ppndev[a]);
        if (err < 0)
        {
            drv_Unload_Cleanup();
#ifdef CONFIG_RENESAS_RSWITCH_ETH_STATS
            rswitch_eth_proc_Terminate();
#endif
            return err;
        }

    }
    
    desc_init(pdev);
    
#ifdef INTERNAL_GATEWAY_PORT

    /* Create Software Internal Gw Port */
    drv_Probe_CreateVirtualNetdev(pdev, Board_Config.Eth_Ports, &ppndev[Board_Config.Eth_Ports]);

#endif
    /*
        Add character device for IOCTL interface
    */
    if ((err = drv_Probe_Create_chrdev()) < 0)
    {
        drv_Unload_Remove_Netdevs();
        drv_Unload_Cleanup();
#ifdef CONFIG_RENESAS_RSWITCH_ETH_STATS
        rswitch_eth_proc_Terminate();
#endif
        return err;
    }

    platform_set_drvdata(pdev, ppndev);
    rswitch_Probe_RegisterInterrupts();

    
#ifdef CONFIG_RENESAS_RSWITCH_ETH_STATS
    rswitch_eth_proc_Start();
#endif 
    rswitch_gwca_init();
    for (a = 0; a < Board_Config.Eth_Ports; a++)
    {
        rswitch_dmac_init(ppndev[a]);

    }
#ifdef INTERNAL_GATEWAY_PORT
    /* Tx Descriptor Initialisation for GW Port */
    rswitch_ring_init(ppndev[a], 0);
    /* Descriptor format */
    rswitch_ring_format(ppndev[a], RSWITCH_BE);
#endif
    rswitch_rx_ring_init(0);
    rswitch_rx_ring_init(1);
#ifdef INTERNAL_GATEWAY_PORT
    /* Add RX Ring Initialisation for GW Port */
    rswitch_rx_ring_init(2);
#endif
    rswitch_rx_ring_format(0);
    rswitch_rx_ring_format(1);
#ifdef INTERNAL_GATEWAY_PORT
    /* Add RX Ring Format for GW Port */
    rswitch_rx_ring_format(2);
#endif
    rswitch_ts_ring_init();
    rswitch_ts_ring_format();
    rswitch_gwca_set_mode(RSWITCH_GWMC_DISABLE);
    rswitch_gwca_set_mode(RSWITCH_GWMC_OPERATION);
    rswitch_eth_enable_interrupt();
    
    pr_info("[RSWITCH] Module loaded\n");

    return err;
}









/**
    @brief  Platform Driver Unload

    @param  pdev    The device to be removed

    @return Return code (< 0 on error)
*/
static int rswitchtsn_drv_Unload(struct platform_device * pdev)
{
    printk("Inside Driver Unload \n");
#ifdef CONFIG_RENESAS_RSWITCH_ETH_STATS
    rswitch_eth_proc_Terminate();
#endif  
    drv_Unload_Remove_chrdev();
    
    drv_Unload_Remove_Netdevs();
    rswitch_drv_Remove_Interrupts();
    drv_Unload_Cleanup();

    platform_set_drvdata(pdev, NULL);

    pr_info("[RSWITCH] Module Unloaded\n");

    return 0;
}


/**
    @brief  Platform Device
*/
static struct platform_driver   rswitchtsn_driver = 
{
    
    .remove     = rswitchtsn_drv_Unload,
    .id_table   = rswitchtsn_ID_Table,
    .driver = 
    {
        .name = RSWITCH_FPGA_ETH_PLATFORM,
    },
};

struct platform_device *pdev;

/**
    @brief  init function
*/
int __init init_module(void)
{
    int retval;
    
    pdev = platform_device_register_simple(
    RSWITCH_FPGA_ETH_PLATFORM, -1, NULL, 0);
    
    retval = platform_driver_probe(&rswitchtsn_driver, rswitchtsn_drv_Probe);
    
    return retval;
    
}

/**
    @brief  Exit function
*/
static void __exit remove_module(void)
{
    

    platform_driver_unregister(&rswitchtsn_driver);
    platform_device_unregister(pdev);

}


module_exit(remove_module);
MODULE_AUTHOR("Asad Kamal");
MODULE_DESCRIPTION("Renesas  R-Switch Ethernet AVB/TSN driver");
MODULE_LICENSE("GPL v2");


/*
    Change History
    2019-03-01  0.0.1  AK  Initial version with TAS and gptp functionality 
    2019-03-22  0.0.2  AK  TAS operation mode start
    2019-03-27  0.0.3  AK  Added Proc stats for MAC Rx & TX
    2019-03-29  0.0.4  AK  Fixed multi port problem, kind of patch, Asad needs to revisit a good way   
    2019-04-01  0.0.5  AK  State Machines for TAS in place 
    2019-04-11  0.0.6  AK  GPTP with TAS fix 
    2019-04-25  0.0.7  AK  Preemption & CBS Support 
    2019-05-15  0.0.8  AK  Temporary fix for Gate TAS error(Polling Method) 
    2019-05-15  0.0.9  AK  TAS Error Software Bug Fixed, Loop issue while writing Gate Control List
    2019-07-23  0.1.0  AK  Added Internal Gateway Port. Test Under Progress
    2019-09-13  0.1.1  AK  Added Max Frame Size Configuration, Internal Port already tested
    2019-09-24  0.1.2  AK  Updated for TAS cycle time configuration
    2019-09-27  0.1.3  AK  Updated for TAS Software Delay, max framesize can be 0 and CBS not mandatory
    2019-10-03  0.1.4  AK  PTP clock made dynamic, Interface up down fixed
    2019-10-11  0.2.0  AK  Error Monitoring Added, New Inerrupt Map, Proc Stats
    2019-11-07  0.2.0  AK  Modified Bitfile version check
    2019-11-12  0.2.1  AK  Internal Gw Up down crash fixed.
    2019-11-14  0.2.2  AK  tcpdump crash fixed.
    2019-12-02  0.2.3  AK  Single Interrupt Line, Crash for no interface tcpdump fixed, Use tasklet for error interrupt
    2019-12-06  0.2.4  AK  Two Interrupt Line, logical separation for data and interrupt
    2020-01-06  1.0.0  AK  Code Clean UP Release Version
    2020-01-06  1.0.1  AK  Bitfile version error more  verbose
    2020-03-30  1.1.0  AK  Handling Rx Descriptor Full Error
*/


/*
* Local variables:
* Mode: C
* tab-width: 4
* indent-tabs-mode: nil
* c-basic-offset: 4
* End:
* vim: ts=4 expandtab sw=4
*/


