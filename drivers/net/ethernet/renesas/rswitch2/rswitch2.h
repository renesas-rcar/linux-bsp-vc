/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas RSwitch2 common header
 *
 * Copyright (C) 2021, 2022 Renesas Electronics Corporation
 */

#ifndef _RSWITCH2_RSWITCH2_H
#define _RSWITCH2_RSWITCH2_H

#include <linux/if_ether.h>
#include <linux/phy.h>



#define RSWITCH2_NAME 	"rswitch2"
#define RSW2_GWCA0_NAME	"gwca0"


#define RSWITCH2_AXI_CHAIN_N		128
#define RSWITCH2_BITS_PER_REG		32
#define RSWITCH2_CHAIN_REG_NUM		(RSWITCH2_AXI_CHAIN_N / RSWITCH2_BITS_PER_REG)

#define RSWITCH2_PTP_Q_NUM			2
#define RSWITCH2_MAC_AGING_TIME		300

#define RSWITCH2_MAX_RXTX_IRQS		32
#define RSWITCH2_MAX_STATUS_IRQS	8

#define RSWITCH2_MAX_IRQS			(RSWITCH2_MAX_RXTX_IRQS + RSWITCH2_MAX_STATUS_IRQS)


#define RSWITCH2_MAX_VLAN_ENTRIES	8192

/* Wait max. 100ms as default */
#define RSWITCH2_REG_POLL_TIMEOUT	100000

/* Poll every 100us */
#define RSWITCH2_REG_POLL_DELAY		100


static const u8 rsw2_base_mac[ETH_ALEN] __aligned(2) = {
		0x74, 0x90, 0x50, 0x00, 0x00, 0x00 };

#define RSWITCH2_BASE_ADDR_MAC { 0x74, 0x90, 0x50, 0x00, 0x00, 0x00 }

static const u8 rsw2_ptp_multicast_mac[ETH_ALEN] __aligned(2) = {
		0x01, 0x80, 0xc2, 0x00, 0x00, 0x0E };

static const u8 rsw2_own_multicast_mac[ETH_ALEN] __aligned(2) = {
		0x74, 0x90, 0x50, 0x00, 0xcc, 0x00};


enum rsw2_msg_section {
	MSG_GEN = 0,
	MSG_DESC,			/* Descriptors, ring layout */
	MSG_RXTX,			/* Reception/Transmission */
	MSG_FWD,			/* Forwarding engine */
	MSG_SERDES,			/* SerDes */
	MSG_LAST_ENTRY
};





struct rswitch2_port_data {
	char phy_id[MII_BUS_ID_SIZE + 3];
	phy_interface_t phy_iface;
	u32 reg;
	u8 mac_addr[ETH_ALEN] __aligned(2);
};

struct rsw2_q_port_backref {
	uint port_num;
	uint port_q;
};

struct rswitch2_drv {
	void __iomem *base_addr;
	void __iomem *fwd_base_addr;
	void __iomem *fab_base_addr;
	void __iomem *coma_base_addr;
	void __iomem **etha_base_addrs;
	void __iomem **gwca_base_addrs;
	void __iomem *serdes_base_addr;
	void __iomem *ptp_base_addr;
	void __iomem *sram_base_addr;  /* for sram debug and pps output control */
	struct device *dev;
	u32 num_of_cpu_ports;
	u32 num_of_tsn_ports;
	u32 ports_intialized;
	bool serdes_common_init_done;
	struct rswitch2_eth_port **ports;

	/* Move these to eth generic struct */

	struct rswitch2_dma_desc **bat_addr;
	dma_addr_t bat_dma_addr;

	struct rswitch2_dma_desc *bat_ts_addr;
	dma_addr_t bat_ts_dma_addr;
	struct rswitch2_dma_ts_desc *ts_desc_ring;
	dma_addr_t ts_desc_dma;
	u32 ts_cur_desc;	/* Consumer ring indices */
	u32 ts_dirty_desc;	/* Producer ring indices */

#ifdef RSW2_DEPRECATED
	struct rswitch2_port_data *port_data;
#endif /* RSW2_DEPRECATED */
	int rxtx_irqs[RSWITCH2_MAX_RXTX_IRQS];
	unsigned int num_of_rxtx_irqs;
	int status_irqs[RSWITCH2_MAX_STATUS_IRQS];
	unsigned int num_of_status_irqs;
	struct rtsn_ptp_private *ptp_drv;
	struct rsw2_q_port_backref port_backref[RSWITCH2_AXI_CHAIN_N];
	int msg_enable;
	int sec_log_lvl[MSG_LAST_ENTRY];
	spinlock_t lock;

};

#define __THIS_FILE__   ((strrchr(__FILE__, '/') == NULL) ?  ( __FILE__ ) :  (strrchr(__FILE__, '/') + 1))

static inline const char *_rsw2_get_msg_section(enum rsw2_msg_section msg_sec) {
	const char *section_str;

		switch(msg_sec) {
		case MSG_GEN:
			section_str = "";
			break;

		case MSG_DESC:
			section_str = "[DESC]";
			break;

		case MSG_RXTX:
			section_str = "[RX/TX]";
			break;

		case MSG_FWD:
			section_str = "[FWD]";
			break;

		case MSG_SERDES:
			section_str = "[SerDes]";
			break;


		default:
			pr_warn("RSwitch2[%s:%d]: Illegal message section.\n", __THIS_FILE__,  __LINE__);
			section_str = "";
		}
		return section_str;
}

#define rsw2_dbg(MSG_SEC, ARG, ...)  _rsw2_pr(rsw2, LOGLEVEL_DEBUG, MSG_SEC, "RSwitch2[%s:%d]%s: " ARG , (__THIS_FILE__), (__LINE__), _rsw2_get_msg_section(MSG_SEC),  ##__VA_ARGS__)
#define rsw2_info(MSG_SEC, ARG, ...)  _rsw2_pr(rsw2, LOGLEVEL_INFO, MSG_SEC, "RSwitch2[%s:%d]%s: " ARG , (__THIS_FILE__), (__LINE__), _rsw2_get_msg_section(MSG_SEC),  ##__VA_ARGS__)
#define rsw2_notice(MSG_SEC, ARG, ...)  _rsw2_pr(rsw2, LOGLEVEL_NOTICE, MSG_SEC, "RSwitch2[%s:%d]%s: " ARG , (__THIS_FILE__), (__LINE__), _rsw2_get_msg_section(MSG_SEC),  ##__VA_ARGS__)
#define rsw2_warn(MSG_SEC, ARG, ...)  _rsw2_pr(rsw2, LOGLEVEL_WARNING, MSG_SEC, "RSwitch2[%s:%d]%s: " ARG , (__THIS_FILE__), (__LINE__), _rsw2_get_msg_section(MSG_SEC),  ##__VA_ARGS__)
#define rsw2_err(MSG_SEC, ARG, ...)  _rsw2_pr(rsw2, LOGLEVEL_ERR, MSG_SEC, "RSwitch2[%s:%d]%s: " ARG , (__THIS_FILE__), (__LINE__), _rsw2_get_msg_section(MSG_SEC),  ##__VA_ARGS__)
#define rsw2_crit(MSG_SEC, ARG, ...)  _rsw2_pr(rsw2, LOGLEVEL_CRIT, MSG_SEC, "RSwitch2[%s:%d]%s: " ARG , (__THIS_FILE__), (__LINE__), _rsw2_get_msg_section(MSG_SEC),  ##__VA_ARGS__)

static inline int _rsw2_pr(struct rswitch2_drv *rsw2,  int level, enum rsw2_msg_section msg_sec, const char *fmt, ...) {
	int ret;
	va_list args;

	if(level <= rsw2->sec_log_lvl[msg_sec]) {
		va_start(args, fmt);
		ret = vprintk(fmt, args);
		va_end(args);
	}
	return ret;
}


int rswitch2_init(struct rswitch2_drv *rsw2);
void rswitch2_exit(struct rswitch2_drv *rsw2);

#endif /* _RSWITCH2_RSWITCH2_H */
