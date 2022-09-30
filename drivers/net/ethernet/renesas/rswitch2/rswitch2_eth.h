/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas RSwitch2 Gateway Common Agent device driver
 *
 * Copyright (C) 2021 Renesas Electronics Corporation
 *
 */

#ifndef _RSWITCH2_ETH_H
#define _RSWITCH2_ETH_H

#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/timer.h>


#define RSW2_NETDEV_BASENAME	"sw%d"
#define RSW2_PORT_BASENAME	    "etha%d"

/* Maximum different Q types per port */
#define MAX_Q_TYPES_PER_PORT			4

/* Maximum timestamp descriptor Q entries */
#define MAX_TS_Q_ENTRIES_PER_PORT	256


/* Queues use for link local data start at 8 */
#define RSW2_LL_RX_Q_OFFSET		8

/* Reserve 8 queues use for link local data */
#define RSW2_LINK_LOCAL_RX_Q_NUM	8
#define RSW2_LINK_LOCAL_TX_Q_NUM	8

/* Pre-defined link local RX queues */
enum rsw2_link_local_rx_queues {
	rsw2_ll_rx_q_port0	= RSW2_LL_RX_Q_OFFSET,
	rsw2_ll_rx_q_port1,
	rsw2_ll_rx_q_port2,
	rsw2_ll_rx_q_port3,
	rsw2_ll_rx_q_port4,
	rsw2_ll_rx_q_port5,
	rsw2_ll_rx_q_port6,
	rsw2_ll_rx_q_port7,
	rsw2_ll_rx_q_max_entry,
};
#define RSW2_LL_RX_RING_SIZE 128

#define RSW2_LL_TX_Q_OFFSET (rsw2_ll_rx_q_max_entry)

/* Pre-defined link local TX queues */
enum rsw2_link_local_tx_queues {
	rsw2_ll_tx_q_port0	= RSW2_LL_TX_Q_OFFSET,
	rsw2_ll_tx_q_port1,
	rsw2_ll_tx_q_port2,
	rsw2_ll_tx_q_port3,
	rsw2_ll_tx_q_port4,
	rsw2_ll_tx_q_port5,
	rsw2_ll_tx_q_port6,
	rsw2_ll_tx_q_port7,
	rsw2_ll_tx_q_max_entry,
};
#define RSW2_LL_TX_RING_SIZE 64

#define RSW2_LL_RX_PER_PORT_QUEUES 1
#define RSW2_LL_TX_PER_PORT_QUEUES 1

#define RSW2_BE_RX_Q_OFFSET (rsw2_ll_tx_q_max_entry)

/* Pre-defined best effort RX queues */
enum rsw2_rx_queues {
	rsw2_be_rx_q_0		= RSW2_BE_RX_Q_OFFSET,
	rsw2_be_rx_q_1,
	rsw2_be_rx_q_2,
	rsw2_be_rx_q_3,
	rsw2_be_rx_q_4,
	rsw2_be_rx_q_5,
	rsw2_be_rx_q_6,
	rsw2_be_rx_q_7,
	rsw2_be_rx_q_8,
	rsw2_be_rx_q_9,
	rsw2_be_rx_q_10,
	rsw2_be_rx_q_11,
	rsw2_be_rx_q_12,
	rsw2_be_rx_q_13,
	rsw2_be_rx_q_14,
	rsw2_be_rx_q_15,
	rsw2_be_rx_q_max_entry,
};

#define RSW2_BE_RX_RING_SIZE 1024



#define RSW2_BE_TX_Q_OFFSET 96

/* Pre-defined best effort TX queues */
enum rsw2_tx_queues {
	rsw2_be_tx_q_0		= RSW2_BE_TX_Q_OFFSET,
	rsw2_be_tx_q_1,
	rsw2_be_tx_q_2,
	rsw2_be_tx_q_3,
	rsw2_be_tx_q_4,
	rsw2_be_tx_q_5,
	rsw2_be_tx_q_6,
	rsw2_be_tx_q_7,
	rsw2_be_tx_q_max_entry,
};
#define RSW2_BE_TX_RING_SIZE 512

#define RX_BAT_START		RSW2_BE_RX_Q_OFFSET
#define TX_BAT_START		RSW2_BE_TX_Q_OFFSET

#define BAT_ENTRIES			128

#define NUM_BE_RX_QUEUES	(rsw2_be_rx_q_max_entry - RSW2_BE_RX_Q_OFFSET)
#define NUM_BE_TX_QUEUES	(rsw2_be_tx_q_max_entry - RSW2_BE_TX_Q_OFFSET)

#define NUM_ALL_QUEUES (NUM_BE_TX_QUEUES + NUM_BE_RX_QUEUES)

/* Queues after the pre-defined once, can be used for dynamic allocation */
#define DYNAMIC_QUEUE_OFFSET	NUM_ALL_QUEUES

/* Max. descriptor per frame */
#define RX_MAX_DESC_PER_FRAME 16
#define TX_MAX_DESC_PER_FRAME 16

//#define RX_MAX_DESC_PER_FRAME 32
//#define TX_MAX_DESC_PER_FRAME 32

/* If set too small packets from the the stack lead to too many descriptor per frame
 * This will let the driver stall as there is currently no recovery. Other values than
 * 4095 should be only used for debugging split descriptor behavior
 */
#define RSWITCH2_MAX_DESC_SIZE 4095


#define RSW2_BUF_ALIGN	128
#define RSW2_PKT_BUF_SZ 1538


/* Default VLAN settings */
#define RSWITCH2_DEF_CTAG_VLAN_ID	1
#define RSWITCH2_DEF_CTAG_PCP		0
#define RSWITCH2_DEF_CTAG_DEI		0
#define RSWITCH2_DEF_STAG_VLAN_ID	1
#define RSWITCH2_DEF_STAG_PCP		0
#define RSWITCH2_DEF_STAG_DEI		0



enum rsw2_desc_dt {
	/* Empty data descriptors */
	DT_FEMPTY_IS	= 1,
	DT_FEMPTY_IC	= 2,
	DT_FEMPTY_ND	= 3,
	DT_FEMPTY		= 4,
	DT_FEMPTY_START	= 5,
	DT_FEMPTY_MID	= 6,
	DT_FEMPTY_END	= 7,
	/* Data descriptors */
	DT_FSINGLE		= 8,
	DT_FSTART		= 9,
	DT_FMID			= 10,
	DT_FEND			= 11,
	/* Chain control */
	DT_LINKFIX		= 0,
	DT_LEMPTY		= 12,
	DT_EEMPTY		= 13,
	DT_LINK			= 14,
	DT_EOS			= 15
};


struct rswitch2_dma_desc {
	__le16 info_ds;		/* Descriptor size */
	u8 die_dt;			/* Descriptor interrupt enable and type */
	__u8  dptrh;		/* Descriptor pointer MSB */
	__le32 dptrl;		/* Descriptor pointer LSW */
} __packed;

#if 0
/* The Ethernet TS descriptor definitions. */
struct rswitch2_dma_ts_desc {
	__le16 info_ds;		/* Descriptor size */
	u8 die_dt;			/* Descriptor interrupt enable and type */
	__u8  dptrh;		/* Descriptor pointer MSB */
	__le32 dptrl;		/* Descriptor pointer LSW */
	__le32 ts_nsec;
	__le32 ts_sec;
} __packed;
#endif
struct rswitch2_dma_ts_desc {
	__le16 info_ds;		/* Descriptor size */
	u8 die_dt;			/* Descriptor interrupt enable and type */
	__u8  : 8;			/* Unused */
	__u8  tsun;			/* Timestamp unified number */
	__u8  src_port_num;
	__u8  dest_port_num;
	__u8  tn;			/* Timer number */
	__le32 ts_nsec;
	__le32 ts_sec;
} __packed;

struct rswitch2_dma_ext_desc {
	__le16 info_ds;		/* Descriptor size & INFO0*/
	u8 die_dt;			/* Descriptor interrupt enable and type */
	u8  dptrh;			/* Descriptor pointer MSB */
	__le32 dptrl;		/* Descriptor pointer LSW */
	__le64 info1;		/* Descriptor INFO1  */
} __packed;

struct rswitch2_dma_ext_ts_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	__u8  dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
	__le64 info1;
	__le32 ts_nsec;
	__le32 ts_sec;
} __packed;



#define RSW2_DESC_DS	GENMASK(11, 0)
#define RSW2_DESC_INFO0	GENMASK(15, 12)
#define RSW2_DESC_INFO0_SEC	BIT_MASK(13)
#define RSW2_DESC_INFO0_FI	BIT_MASK(12)

#define RSW2_DESC_DT	GENMASK(7, 4)
#define RSW2_DESC_DIE	BIT_MASK(3)
#define RSW2_DESC_AXIE	BIT_MASK(2)
#define RSW2_DESC_DSE	BIT_MASK(1)
#define RSW2_DESC_ERR	BIT_MASK(0)

#define RSW2_DESC_INFO1_SEC	BIT_MASK(0)
#define RSW2_DESC_INFO1_FI	BIT_MASK(1)
#define RSW2_DESC_INFO1_FMT	BIT_MASK(2) /* Descriptor format */
enum rsw2_info1_format {
	direct_desc = 1
};
#define RSW2_DESC_INFO1_TXC		BIT_MASK(3)		/* TX Timestamp capture */
#define RSW2_DESC_INFO1_IET		BIT_MASK(4)		/* Timestamp insertion request */
#define RSW2_DESC_INFO1_CRT		BIT_MASK(5)		/* Residence time calculation request */
#define RSW2_DESC_INFO1_TN		BIT_MASK(6)		/* Timer utilized for capture/insertion */
#define RSW2_DESC_INFO1_TSUN	GENMASK(15, 8)	/* Timestamp unique number*/
#define RSW2_DESC_INFO1_RN		GENMASK(23, 16) /* Routing valid */
#define RSW2_DESC_INFO1_RV		BIT_MASK(27)	/* Routing valid */
#define RSW2_DESC_INFO1_IPV		GENMASK(30, 28)	/* Internal priority value / Target priority of the frame */
#define RSW2_DESC_INFO1_FW		BIT_MASK(31)	/* The FCS contained in the frame is wrong */
#define RSW2_DESC_INFO1_CSD0	GENMASK(38, 32)/* CPU sub destination for GWCA0 */
#define RSW2_DESC_INFO1_CSD1	GENMASK(46, 40)/* CPU sub destination for GWCA1 */
#define RSW2_DESC_INFO1_DV		GENMASK(54, 48)/* Destination vector */


#define RSW2_STAT_TIMER_INTERVALL (3000)
#define RSW2_SERDES_OP_TIMER_INTERVALL (500)
#define RSW2_SERDES_OP_TIMER_INTERVALL_FIRST (1500)
#define RSW2_SERDES_OP_RETRIES (5)

#define RSW2_RX_TS_DESC 1


struct rswitch2_dma_extts_desc {
	__le16 info_ds;		/* Descriptor size */
	u8 die_dt;			/* Descriptor interrupt enable and type */
	__u8  dptrh;		/* Descriptor pointer MSB */
	__le32 dptrl;		/* Descriptor pointer LSW */
	__le64 info1;		/* Descriptor pointer */
	__le32 ts_nsec;
	__le32 ts_sec;
} __packed;

struct rsw2_rx_q_data {
#ifdef RSW2_RX_TS_DESC
	struct rswitch2_dma_ext_ts_desc *desc_ring;
#else
	struct rswitch2_dma_ext_desc *desc_ring;
#endif

	dma_addr_t desc_dma;
	//u32 bat_entry;
	size_t entries;
	u32 cur_desc;	/* Consumer ring indices */
	u32 dirty_desc;	/* Producer ring indices */
	struct sk_buff **skb;
	struct napi_struct napi;
	uint offset;
};

struct rsw2_tx_q_data {
	/* FIXME: check if there better type then rswitch2_dma_ext_desc */
	struct rswitch2_dma_ext_desc *desc_ring;
	dma_addr_t desc_dma;
	//u32 bat_entry;
	size_t entries;
	u32 cur_desc;	/* Consumer ring indices */
	u32 dirty_desc;	/* Producer ring indices */
	struct sk_buff **skb;
	uint offset;
};


struct rswitch2_physical_port {
	void __iomem *rmac_base_addr;
	void __iomem *serdes_chan_addr;
	struct mii_bus *mii_bus;
	phy_interface_t phy_iface;
	struct rsw2_rx_q_data rx_q[RSW2_LL_RX_PER_PORT_QUEUES];
	struct rsw2_tx_q_data tx_q[RSW2_LL_TX_PER_PORT_QUEUES];
	u8 ts_tag;
	struct sk_buff *ts_skb[MAX_TS_Q_ENTRIES_PER_PORT];
	u64 rx_pkt_cnt;
	u64 rx_byte_cnt;
	u64 tx_pkt_cnt;
	u64 tx_byte_cnt;
	struct timer_list serdes_usxgmii_op_timer;
	u32 serdes_usxgmii_op_cnt;

};

struct rswitch2_internal_port {
	/*	u32 num_rx_ring;
	u32 num_tx_ring;
	struct napi_struct napi_tx[NUM_TX_QUEUES];
	struct napi_struct napi_rx[NUM_RX_QUEUES];
*/
	struct rsw2_rx_q_data rx_q[NUM_BE_RX_QUEUES];
	struct rsw2_tx_q_data tx_q[NUM_BE_TX_QUEUES];
	u64 rx_pkt_cnt;
	u64 rx_byte_cnt;
	u64 tx_pkt_cnt;
	u64 tx_byte_cnt;
	u32 rx_over_errors;
	u32 rx_fifo_errors;
	struct work_struct work;
};

struct rswitch2_eth_port {
	struct net_device *ndev;
	struct rswitch2_drv *rsw2;

	/* FIXME */
	struct net_device_stats stats[NUM_BE_RX_QUEUES];
	struct timer_list stat_timer;

	uint lock_count;



	unsigned int port_num;
	struct rswitch2_physical_port *phy_port;
	struct rswitch2_internal_port *intern_port;

};

int rswitch2_eth_init(struct rswitch2_drv *rsw2);
void rswitch2_eth_exit(struct rswitch2_drv *rsw2);

/* Register definitions */

#define RSW2_ETHA_EAMC		0x0000 /* Ethernet Agent Mode Configuration */
#define EAMC_OPC		GENMASK(1, 0)

#define RSW2_ETHA_EAMS		0x0004 /* Ethernet Agent Mode Status */
#define EAMS_OPS		GENMASK(1, 0)

enum emac_op {
	emac_reset		= 0,
	emac_disable	= 1,
	emac_config		= 2,
	emac_operation	= 3,
};

#define RSW2_ETHA_EAMC		0x0000 /* Ethernet Agent Mode Configuration */

#define RSW2_ETHA_EATDQSC	0x0014 /* Ethernet Agent TX Descriptor Queue Security Configuration */

#define RSW2_ETHA_EATDQC	0x0018 /* Ethernet Agent TX Descriptor Queue Configuration */

#define RSW2_ETHA_EATDQAC	0x001C /* Ethernet Agent TX Descriptor Queue Arbitration Configuration */

#define RSW2_ETHA_EATPEC	0x0020 /* Ethernet Agent TX Pre-Emption Configuration */

#define RSW2_ETHA_EATMFSC(q)	(0x0040 + 0x4 * (q)) /* Ethernet Agent Transmission Maximum Frame Size Configuration q */

#define RSW2_ETHA_EATDQDC(q)	(0x0060 + 0x4 * (q)) /* Ethernet Agent Transmission Descriptor Queue Depth Configuration q */

#define RSW2_ETHA_EATDQM(q)		(0x0080 + 0x4 * (q)) /* Ethernet Agent Transmission Descriptor Queue q Monitoring */

#define RSW2_ETHA_EATDQMLM(q)	(0x00A0 + 0x4 * (q)) /* Ethernet Agent Transmission Descriptor Queue q Max Level Monitoring */
#define RSW2_ETHA_EATDQMLME(q)	(0x00C0 + 0x4 * (q)) /* Ethernet Agent Transmission Descriptor Queue q Max Level Monitoring Emu */

#define RSW2_ETHA_EAVCC		0x0130 /* Ethernet Agent VLAN control configuration */

#define RSW2_ETHA_EAVTC		0x0134 /* Ethernet Agent VLAN TAG configuration */

#define RSW2_ETHA_EARTFC	0x0138 /* Ethernet Agent Reception TAG Filtering Configuration */

#define RSW2_ETHA_EACAEC	0x0200 /* Ethernet Agent CBS Admin Enable Configuration */

#define RSW2_ETHA_EACC		0x0204 /* Ethernet Agent CBS Configuration */

#define RSW2_ETHA_EACAIVC(q)	(0x0220 + 0x4 * (q)) /* Ethernet Agent CBS Admin Increment Value Configuration q */

#define RSW2_ETHA_EACAULC(q)	(0x0240 + 0x4 * (q)) /* Ethernet Agent CBS Admin Upper Limit Configuration q */

#define RSW2_ETHA_EACOEM		0x0260 /* Ethernet Agent CBS Oper Enable Monitoring */

#define RSW2_ETHA_EACOIVM(q)	(0x0280 + 0x4 * (q)) /* Ethernet Agent CBS Oper Increment Value Monitoring q */

#define RSW2_ETHA_EACOULM(q)	(0x02A0 + 0x4 * (q)) /* Ethernet Agent CBS Oper Upper Limit Monitoring q */

#define RSW2_ETHA_EACGSM		0x02C0 /* Ethernet Agent CBS Gate State Monitoring */

#define RSW2_ETHA_EATASC		0x0300 /* Ethernet Agent TAS Configuration */

#define RSW2_ETHA_EATASIGSC		0x0304 /* Ethernet Agent TAS Initial Gate State Configuration */

#define RSW2_ETHA_EATASENC(i)	(0x0320 + 0x4 * (i)) /* Ethernet Agent TAS Entry Number Configuration i */

#define RSW2_ETHA_EATASENM(i)	(0x0360 + 0x4 * (i)) /* Ethernet Agent TAS Entry Number Monitoring i */

#define RSW2_ETHA_EATASCSTC0	0x03A0 /* Ethernet Agent TAS Cycle Start Time Configuration 0 */

#define RSW2_ETHA_EATASCSTC1	0x03A4 /* Ethernet Agent TAS Cycle Start Time Configuration 1 */

#define RSW2_ETHA_EATASCSTM0	0x03A8 /* Ethernet Agent TAS Cycle Start Time Monitoring 0 */

#define RSW2_ETHA_EATASCSTM1	0x03AC /* Ethernet Agent TAS Cycle Start Time Monitoring 1 */

#define RSW2_ETHA_EATASCTC	0x03B0 /* Ethernet Agent TAS Cycle Time Configuration */

#define RSW2_ETHA_EATASCTM	0x03B4 /* Ethernet Agent TAS Cycle Time Monitoring */

#define RSW2_ETHA_EATASGL0	0x03C0 /* Ethernet Agent TAS Gate Learn 0 */

#define RSW2_ETHA_EATASGL1	0x03C4 /* Ethernet Agent TAS Gate Learn 1 */

#define RSW2_ETHA_EATASGLR	0x03C8 /* Ethernet Agent TAS Gate Learn Result */

#define RSW2_ETHA_EATASGR	0x03D0 /* Ethernet Agent TAS Gate Read */

#define RSW2_ETHA_EATASGRR	0x03D4 /* Ethernet Agent TAS Gate Read Result */

#define RSW2_ETHA_EATASHCC	0x03E0 /* Ethernet Agent TAS Hardware Calibration Configuration */

#define RSW2_ETHA_EATASRIRM	0x03E4 /* Ethernet Agent TAS RAM Initialization Register Monitoring */

#define RSW2_ETHA_EATASSM	0x03E8 /* Ethernet Agent TAS Status Monitoring */

#define RSW2_ETHA_EAUSMFSECN	0x0400 /* Ethernet Agent Switch Minimum Frame Size Error CouNter */
#define RSW2_ETHA_EAUSMFSECNE	0x0480 /* Ethernet Agent Switch Minimum Frame Size Error CouNter Emu */

#define RSW2_ETHA_EATFECN		0x0404 /* Ethernet Agent TAG Filtering Error CouNter */
#define RSW2_ETHA_EATFECNE		0x0484 /* Ethernet Agent TAG Filtering Error CouNter Emu */

#define RSW2_ETHA_EAFSECN		0x0408 /* Ethernet Agent Frame Size Error CouNter */
#define RSW2_ETHA_EAFSECNE		0x0488 /* Ethernet Agent Frame Size Error CouNter Emu */

#define RSW2_ETHA_EADQOECN		0x040C /* Ethernet Agent Descriptor Queue Overflow Error CouNter */
#define RSW2_ETHA_EADQOECNE		0x048C /* Ethernet Agent Descriptor Queue Overflow Error CouNter Emu */

#define RSW2_ETHA_EADQSECN		0x0410 /* Ethernet Agent Descriptor Queue Security Error CouNter */
#define RSW2_ETHA_EADQSECNE		0x0490 /* Ethernet Agent Descriptor Queue Security Error CouNter Emu */

#define RSW2_ETHA_EAEIS0	0x0500 /* Ethernet Agent Error Interrupt Status 0 */

#define RSW2_ETHA_EAEIE0	0x0504 /* Ethernet Agent Error Interrupt Enable 0 */

#define RSW2_ETHA_EAEID0	0x0508 /* Ethernet Agent Error Interrupt Disable 0 */

#define RSW2_ETHA_EAEIS1	0x0510 /* Ethernet Agent Error Interrupt Status 1 */

#define RSW2_ETHA_EAEIE1	0x0514 /* Ethernet Agent Error Interrupt Enable 1 */

#define RSW2_ETHA_EAEID1	0x0518 /* Ethernet Agent Error Interrupt Disable 1 */

#define RSW2_ETHA_EAEIS2	0x0520 /* Ethernet Agent Error Interrupt Status 2 */

#define RSW2_ETHA_EAEIE2	0x0524 /* Ethernet Agent Error Interrupt Enable 2 */

#define RSW2_ETHA_EAEID2	0x0528 /* Ethernet Agent Error Interrupt Disable 2 */

#define RSW2_ETHA_EASCR		0x0580 /* Ethernet Agent Security Configuration Register */






#endif /* _RSWITCH2_ETH_H */
