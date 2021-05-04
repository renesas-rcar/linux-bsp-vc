// SPDX-License-Identifier: GPL-2.0-only
/* Renesas R-SWITCH2 Ethernet Driver
 *
 * Provide APIs to Network Interface Part of Ethernet Driver
 *
 * Copyright (C) 2020 Renesas Electronics Corporation
 * Author: Asad Kamal
 *
 * bugs/issues: Autoneg not working
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
#include <linux/netdevice.h>
#include <linux/net_tstamp.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/pci.h>
#include <linux/phy.h>
#include <linux/phy_led_triggers.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/renesas_rswitch2.h>
#include <linux/renesas_rswitch2_platform.h>
#include <linux/renesas_vc2_platform.h>
#include <linux/semaphore.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "rswitch2_eth.h"
#include "rswitch2_eth_usr.h"
#include "rswitch2_reg.h"

//#define DEBUG 1

//#define PHY_CONFIG_SUPPORT
#define BOARD_VC3
#define TX_TS_FEATURE 1
#define RSWITCH2_PM_OPS NULL
#define RSWITCH2_ETH_INTERRUPT_LINES 1
#define RSWITCH2_INT_LINE_GPIO 511
#define RSWITCH2_ETH_CTRL_MINOR  (127)
#define PHY_SEL_MUX 0x01004CC0
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
#define RSWITCH2_PROC_ROOT_DIR "rswitch2"
#define RSWITCH2_ETH_PROC_FILE_DRIVER "VERSION"
#define RSWITCH2_ETH_PROC_FILE_ERRORS "etherrors"
#define RSWITCH2_ETH_PROC_FILE_COUNTERS "etha"
#define RSWITCH2_ETH_PROC_FILE_RXTX "rxtx-all"
#define RSWITCH2_ETH_PROC_FILE_RXTX_SHORT "rxtx"
#define RSWITCH2_ETH_PROC_FILE_PORT_MONITOR_SHORT "port-monitor"
#define RSWITCH2_ETH_PROC_FILE_PORT_MONITOR "port-monitor-all"
#define RSWITCH2_ETH_PROC_FILE_MCAST_CPU "mcast-cpu"
#define FUTURE_USE
#endif

/* Global Variables Start */
static DEFINE_SPINLOCK(port_lock);
struct net_device **ppndev;
static void __iomem *gwca_addr;
static struct ethports_config board_config;
static u8 tx_queue;
static void __iomem *ethport_addr[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS];
static u8 napi_pending[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1][NUM_RX_QUEUE];
static u32 desc_bat_size;
static u32 ts_desc_bat_size;
static dma_addr_t desc_bat_dma;
static dma_addr_t ts_desc_bat_dma;

static dma_addr_t rx_desc_dma[NUM_RX_QUEUE];
static dma_addr_t ts_desc_dma;
static struct sk_buff **rx_skb[NUM_RX_QUEUE];
static u32 num_rx_ring[NUM_RX_QUEUE + 1];
static u32 cur_rx[NUM_RX_QUEUE];	/* Consumer ring indices */
static u32 dirty_rx[NUM_RX_QUEUE];	/* Producer ring indices */
static u32 num_ts_ring;
static u32 cur_ts;	/* Consumer ring indices */
static u32 dirty_ts;	/* Producer ring indices */
static struct rswitch2_ext_ts_desc *rx_ring[NUM_RX_QUEUE];
static struct rswitch2_desc *desc_bat;
static struct rswitch2_desc *ts_desc_bat;
static struct rswitch2_ts_desc *ts_ring;
static int board_variant;
static unsigned int gpio;
static bool mux_port;
static u64 rxtx_all_clear = 1;
static u64 error_clear = 1;
static unsigned int nonetdev;
static unsigned int netdev_created;
static struct rswitch2_eth_config config; /* Active Operational Configuration */
static struct rswitch2_eth_config config_new; /* New Operational Configuration */
static struct rswitch2_proc_counter counter;
static int rswitchtsn_devmajor;
module_param(rswitchtsn_devmajor, int, 0440);
static struct class *rswitchtsn_devclass;
static dev_t rswitchtsn_dev;
static struct cdev rswitchtsn_cdev;
static struct device rswitchtsn_device;
#define MS_TO_NS(x) (x * 1E6L)
static const struct of_device_id rswitch2_of_tbl[] = {
	{ .compatible = "rswitch2-tsn", .data = NULL },
	{ } /* terminate list */
};

struct proc_dir_entry *root_dir;
#ifdef FUTURE_USE
//static u32 board_type = RENESAS_VC2;
#endif
static const char rswitch2_priv_flags[][ETH_GSTRING_LEN] = {
	"master-phy",
};

static int phydev_disable_bcast_seterrata(struct net_device *ndev, u32 speed, u32 master);
/* Interrupts */
static int rswitch2_eth_irq_data[RSWITCH2_ETH_INTERRUPT_LINES];
static int rswitch2_eth_irq[RSWITCH2_ETH_INTERRUPT_LINES];

#ifdef CONFIG_RENESAS_RSWITCH2_STATS
/* Proc Directories */
static struct {
	struct proc_dir_entry *rootdir;
} rswitch2_eth_procdir;
#endif
/* Global Variables End */

/* Port Base 0..n  0xA000, 0xC000 ...
 * Port Registers  0x0000 - 0x07FF (relative to the start of each port)
 */
static inline void portreg_write(struct net_device *ndev, unsigned long data, int reg)
{
	struct port_private *mdp = netdev_priv(ndev);
#ifdef DEBUG
	pr_info("Reg=%lx Value = %lx\n", (uintptr_t)(mdp->port_base_addr + reg),
		data);
#endif
	iowrite32(data, (mdp->port_base_addr + reg));
}

static inline unsigned long portreg_read(struct net_device *ndev, int reg)
{
	struct port_private *mdp = netdev_priv(ndev);

	return ioread32(mdp->port_base_addr + reg);
}

void rswitch2_modify(struct net_device *ndev, u32 reg, u32 clear, u32 set)
{
#ifdef DEBUG
	pr_info("Reg=%x Value = %x\n", reg,
		(ioread32(gwca_addr + reg) & ~clear) | set);
#endif
	//pr_info("Value of GWCAADDR=%x\n", gwca_addr);
	iowrite32((ioread32(gwca_addr + reg) & ~clear) | set, gwca_addr + reg);
}

static void port_setstate(struct net_device *ndev, enum rswitch2_portstate state)
{
	struct port_private *mdp = netdev_priv(ndev);

	if (mdp->portstate == state)
		return;

	mdp->portstate = state;
}

static int port_waitstatus(struct net_device *ndev, u32 reg, u32 mask, u32 status)
{
	int i = 0;
	int ret = 0;
	int value = 0;

	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		value = portreg_read(ndev, reg);

		if ((value & mask) == status)
			return ret;
		mdelay(1);
	}

	pr_err("++ %s port_Wait_Status TIMEOUT. Reg(%x) Mask(%08X) Wait(%08X) Last(%08X)\n",
	       ndev->name, reg, mask, status, value);

	return -ETIMEDOUT;
}

extern const char * const port_querystatetext(enum rswitch2_portstate state)
{
	switch (state) {
	case rswitch2_portstate_unknown:
		return "Unknown";
	case rswitch2_portstate_startreset:
		return "Start Reset";
	case rswitch2_portstate_reset:
		return "Reset";
	case rswitch2_portstate_startconfig:
		return "Start Config";
	case rswitch2_portstate_config:
		return "Config";
	case rswitch2_portstate_startoperate:
		return "Start Operation";
	case rswitch2_portstate_operate:
		return "Operation";
	case rswitch2_portstate_disable:
		return "Disable";
	case rswitch2_portstate_failed:
		return "Failed";
	default:
		return "INVALID";
	}
}

extern int port_changestate(struct net_device *ndev, enum rswitch2_portstate newstate)
{
	int ret = 0;

	switch (newstate) {
	case rswitch2_portstate_startreset:
		break;
	case rswitch2_portstate_reset:
		portreg_write(ndev, ETH_TSN_EAMC_OPC_RESET, EAMC);

		ret = port_waitstatus(ndev, EAMS, ETH_TSN_EAMS_OPS_MASK, ETH_TSN_EAMS_OPS_RESET);
		break;
	case rswitch2_portstate_startconfig:
		break;
	case rswitch2_portstate_config:
		portreg_write(ndev, (portreg_read(ndev, EAMC) & ~ETH_TSN_EAMC_OPC) | ETH_TSN_EAMC_OPC_CONFIG, EAMC);

		ret = port_waitstatus(ndev, EAMS, ETH_TSN_EAMS_OPS_MASK, ETH_TSN_EAMS_OPS_CONFIG);
		break;
	case rswitch2_portstate_startoperate:
		break;
	case rswitch2_portstate_operate:
		portreg_write(ndev, (portreg_read(ndev, EAMC) & ~ETH_TSN_EAMC_OPC) | ETH_TSN_EAMC_OPC_OPERATION, EAMC);

		ret = port_waitstatus(ndev, EAMS, ETH_TSN_EAMS_OPS_MASK, ETH_TSN_EAMS_OPS_OPERATION);
		break;
	case rswitch2_portstate_disable:
		ret = portreg_read(ndev, EAMC) & ~ETH_TSN_EAMC_OPC;

		portreg_write(ndev,  ETH_TSN_EAMC_OPC_DISABLE, EAMC);

		ret = port_waitstatus(ndev, EAMS, ETH_TSN_EAMS_OPS_MASK, ETH_TSN_EAMS_OPS_DISABLE);
		break;
	default:
		pr_err("[RSWITCH2-ETH] %s Invalid state(%u)\n", ndev->name, (u32)newstate);
		return -EINVAL;
	}
	if (ret < 0) {
		pr_err("[RSWITCH2-ETH] %s Timeout waiting for state(%s)\n", ndev->name, port_querystatetext(newstate));
		port_setstate(ndev, rswitch2_portstate_failed);
	} else {
		port_setstate(ndev, newstate);
	}

	return ret;
}

/* Switch block doesn't have 'ROM' for MAC addresses */
static void port_generate_mac_address(u32 Port, struct net_device *ndev)
{
	char default_mac[] = RENESAS_RSWITCH2_BASE_PORT_MAC;

	default_mac[5] = Port + 1;
	memcpy(ndev->dev_addr, default_mac, 6);
}

static void port_set_mac_address(struct net_device *ndev)
{
	portreg_write(ndev, ((ndev->dev_addr[5]) |
			     (ndev->dev_addr[4] << 8) |
			     (ndev->dev_addr[3] << 16) |
			     (ndev->dev_addr[2] << 24)), MRMAC1);
	portreg_write(ndev, ((ndev->dev_addr[0] << 8) |
			     (ndev->dev_addr[1] << 0)), MRMAC0);
}

static void port_query_mac_address(struct net_device *ndev)
{
	ndev->dev_addr[0] = (portreg_read(ndev, MRMAC0) >> 8) & 0xFF;
	ndev->dev_addr[1] = (portreg_read(ndev, MRMAC0) >> 0) & 0xFF;
	ndev->dev_addr[2] = (portreg_read(ndev, MRMAC1) >> 24) & 0xFF;
	ndev->dev_addr[3] = (portreg_read(ndev, MRMAC1) >> 16) & 0xFF;
	ndev->dev_addr[4] = (portreg_read(ndev, MRMAC1) >> 8) & 0xFF;
	ndev->dev_addr[5] = (portreg_read(ndev, MRMAC1) >> 0) & 0xFF;
}

static u32 port_get_priv_flags(struct net_device *ndev)
{
	struct port_private *mdp = netdev_priv(ndev);

	phy_write(ndev->phydev, 0x1F, 0x0);
	mdp->priv_flags = phy_read(ndev->phydev, 0x9) == 0x800 ? 1 : 0;
	if (mdp->priv_flags)
		pr_info("Master Mode Phy\n");
	else
		pr_info("Slave Mode Phy\n");
	return mdp->priv_flags;
}

static int port_set_priv_flags(struct net_device *ndev, u32 flags)
{
	struct port_private *mdp = netdev_priv(ndev);

	mdp->priv_flags = flags;

	if (mdp->priv_flags) {
		phy_write(ndev->phydev, 0x1F, 0x0);
		phy_write(ndev->phydev, 0x9, 0x800);
	} else {
		phy_write(ndev->phydev, 0x1F, 0x0);
		phy_write(ndev->phydev, 0x9, 0x00);
	}

	return 0;
}

static void port_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	int i = 0;

	switch (stringset) {
	case ETH_SS_PRIV_FLAGS:
		for (i = 0; i < ARRAY_SIZE(rswitch2_priv_flags); i++)
			strcpy(data + i * ETH_GSTRING_LEN,
			       rswitch2_priv_flags[i]);
		break;
	}
}

static int port_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_PRIV_FLAGS:
		return ARRAY_SIZE(rswitch2_priv_flags);
	default:
		return -EOPNOTSUPP;
	}
}

static int port_set_settings(struct net_device *ndev, const struct ethtool_link_ksettings *ecmd)
{
	struct port_private *mdp = netdev_priv(ndev);
	int ret = 0;

	if (NOT mdp->opened)
		return -EAGAIN;

	if (ndev->phydev != NULL) {
		//spin_lock_irqsave(&port_lock, flags);
		mdelay(1);
		if (ecmd->base.speed == 100) {
			port_changestate(ndev, rswitch2_portstate_disable);
			port_changestate(ndev, rswitch2_portstate_config);
			portreg_write(ndev, (0x01130000 + 0x0b), MPIC);
			port_changestate(ndev, rswitch2_portstate_operate);
			if (mdp->portindex != 5)
				port_get_priv_flags(ndev);
			phydev_disable_bcast_seterrata(ndev, 100, mdp->priv_flags);
		} else if (ecmd->base.speed == 1000) {
			port_changestate(ndev, rswitch2_portstate_disable);
			port_changestate(ndev, rswitch2_portstate_config);
			portreg_write(ndev, (0x01130000 + 0x13), MPIC);
			port_changestate(ndev, rswitch2_portstate_operate);
			if (mdp->portindex != 5)
				port_get_priv_flags(ndev);
			phydev_disable_bcast_seterrata(ndev, 1000, mdp->priv_flags);
		}
		ret = phy_ethtool_set_link_ksettings(ndev, ecmd);
		if (ret != 0) {
			mdelay(1);
			//spin_unlock_irqrestore(&port_lock, flags);
			return ret;
		}
		//spin_unlock_irqrestore(&port_lock, flags);
	} else {
		pr_err("[RSWITCH2] ERROR: %s is not open\n", ndev->name);
		return -EOPNOTSUPP;
	}
	return 0;
}

/* Query timestamping capabilities of port
 *  Return to the socket layer the port timestamping capabilities
 *  - External AVB ports. Show we can do hardware timestamping
 *  - Internal AVB port. Show we cannot do any hardware timestamping
 */
static int rswitch2_port_get_ts_info(struct net_device *ndev, struct ethtool_ts_info *tsinfo)
{
	struct port_private *mdp = netdev_priv(ndev);

	/* SOF_TIMESTAMPING_TX_HARDWARE:  try to obtain send time stamp in hardware
	 * SOF_TIMESTAMPING_TX_SOFTWARE:  if SOF_TIMESTAMPING_TX_HARDWARE is off or fails, then do it in software
	 * SOF_TIMESTAMPING_RX_HARDWARE:  return the original, unmodified time stamp as generated by the hardware
	 * SOF_TIMESTAMPING_RX_SOFTWARE:  if SOF_TIMESTAMPING_RX_HARDWARE is off or fails, then do it in software
	 * SOF_TIMESTAMPING_RAW_HARDWARE: return original raw hardware time stamp
	 * SOF_TIMESTAMPING_SYS_HARDWARE: return hardware time stamp transformed to the system time base
	 * SOF_TIMESTAMPING_SOFTWARE:     return system time stamp generated in software
	 *
	 * The bits in the 'tx_types' and 'rx_filters' fields correspond to
	 * the 'hwtstamp_tx_types' and 'hwtstamp_rx_filters' enumeration values,
	 */
	if (mdp->portnumber != board_config.eth_ports) {
		if (mdp->portstate != rswitch2_portstate_operate) {
			pr_info("ethtool:Info: Device not Up PTP Clock not set, showing Default\n");
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
	} else {
		if (mdp->portstate != rswitch2_portstate_operate) {
			pr_info("ethtool:Info: Device not Up PTP Clock not set, showing Default\n");
			tsinfo->phc_index = 0;
			return 0;
		}
		tsinfo->phc_index = 0;
		return 0;
	}
	return 0;
}

static const struct ethtool_ops port_ethtool_ops = {
	.get_link_ksettings = phy_ethtool_get_link_ksettings,
	.set_link_ksettings = port_set_settings,
	//.set_settings = port_set_settings,
	.get_strings = port_get_strings,
	.get_sset_count = port_get_sset_count,
	.get_priv_flags = port_get_priv_flags,
	.set_priv_flags = port_set_priv_flags,
	.get_link = ethtool_op_get_link,
	.get_ts_info = rswitch2_port_get_ts_info,
};

static int port_dev_init(struct net_device *ndev)
{
	struct port_private *priv = netdev_priv(ndev);
	int ret = 0;
	u32 priority = 0;
	u32 eavtc = 0;
	u32 port = 0;

	/* Enable Promiscuous mode */
	portreg_write(ndev, 0x00070007, MRAFC);
	/* Set MAC address */
	port_set_mac_address(ndev);
	/* Set Maximum Queue Depth */
	for (priority = 0; priority < RSWITCH2_MAX_CTAG_PCP; priority++)
		portreg_write(ndev, 0x7FF, (EATDQDC0 + (priority * 4)));
	/* Set VLAN Egress Mode to SC-TAG if vlan config not active*/
	/* Check if Eth agent is configured */
	if (config_new.ports) {
		for (port = 0; port < config_new.ports; port++) {
			struct rswitch2_eth_port_config *port_config;

			port_config = &config_new.eth_port_config[port];
			if (port_config->cpu)
				continue;

			if (priv->portnumber != port_config->port_number)
				continue;

			if (!port_config->eth_vlan_tag_config.bEnable) {
				portreg_write(ndev,
					      RSWITCH2_VLAN_EGRESS_SCTAG_MODE,
					      EAVCC);
				eavtc = (RSWITCH2_DEF_CTAG_VLAN_ID << 0) |
					(RSWITCH2_DEF_CTAG_PCP << 12) |
					(RSWITCH2_DEF_CTAG_DEI << 15) |
					(RSWITCH2_DEF_STAG_VLAN_ID << 16) |
					(RSWITCH2_DEF_STAG_PCP << 28) |
					(RSWITCH2_DEF_STAG_DEI << 31);
				portreg_write(ndev, eavtc, EAVTC);
			}
			break;
		}
		/* If the port in question not configured  put default value */
		if (port == config_new.ports) {
			portreg_write(ndev, RSWITCH2_VLAN_EGRESS_SCTAG_MODE,
				      EAVCC);
			eavtc = (RSWITCH2_DEF_CTAG_VLAN_ID << 0) |
				(RSWITCH2_DEF_CTAG_PCP << 12) |
				(RSWITCH2_DEF_CTAG_DEI << 15) |
				(RSWITCH2_DEF_STAG_VLAN_ID << 16) |
				(RSWITCH2_DEF_STAG_PCP << 28) |
				(RSWITCH2_DEF_STAG_DEI << 31);
			portreg_write(ndev, eavtc, EAVTC);
		}
	/* If Ethernet agent not configured put default value */
	} else {
		portreg_write(ndev, RSWITCH2_VLAN_EGRESS_SCTAG_MODE, EAVCC);
		eavtc = (RSWITCH2_DEF_CTAG_VLAN_ID << 0) |
			(RSWITCH2_DEF_CTAG_PCP << 12) |
			(RSWITCH2_DEF_CTAG_DEI << 15) |
			(RSWITCH2_DEF_STAG_VLAN_ID << 16) |
			(RSWITCH2_DEF_STAG_PCP << 28) |
			(RSWITCH2_DEF_STAG_DEI << 31);
		portreg_write(ndev, eavtc, EAVTC);
	}
	return ret;
}

static void port_set_rate(struct net_device *ndev)
{
	struct port_private *mdp = netdev_priv(ndev);

	switch (mdp->speed) {
	case 100:       /* 100BASE */
		port_changestate(ndev, rswitch2_portstate_config);
		portreg_write(ndev, 0x01130000 + 0x0b, MPIC);
		port_changestate(ndev, rswitch2_portstate_operate);
		break;
	case 1000:      /* 1000BASE */
		port_changestate(ndev, rswitch2_portstate_config);
		portreg_write(ndev, 0x01130000 + 0x13, MPIC);
		port_changestate(ndev, rswitch2_portstate_operate);
		break;
	default:
		pr_err("[RSWITCH2] %s Cannot set speed %uMBit - not supported\n",
		       ndev->name, mdp->speed);
	}
}

static void port_phy_eventhandler(struct net_device *ndev)
{
	struct port_private *mdp = netdev_priv(ndev);
	struct phy_device *phydev = mdp->phydev;
	int new_state = false;
	u32 mlvc = 0;

	//pr_info("In phy handler with Phy link = %d\n", phydev->link);
	if (phydev->link > PHY_DOWN) {
		if (phydev->speed != mdp->speed) {
			new_state = true;
			mdp->speed = phydev->speed;
			port_set_rate(ndev);
		}
		if (mdp->link == PHY_DOWN) {
			new_state = true;
			mdp->link = phydev->link;
		}
	} else if (mdp->link > PHY_DOWN) {
		new_state = true;
		mdp->link = PHY_DOWN;
		mdp->speed = 0;
	}

	if (new_state && netif_msg_link(mdp)) {
		if ((phydev->speed == 0xffffffff || phydev->duplex != DUPLEX_FULL) &&
		    mdp->portindex < 5) {
			phydev->link = false;
			mdp->link = false;
		}
		phy_print_status(phydev);
		/* Start Link Verification everytime link comes Up if previous state was down
		Status not to be checked will be populated as ethtool private flag and proc entry*/
		if(phydev->link) {
			/* Preemption Support Start link verification, if fails forced one from message handler */
			mlvc = portreg_read(ndev, MLVC);
			mlvc |= (1 << 8) | (1 << 16) | 0x7F;
			portreg_write(ndev,mlvc, MLVC);
		}
	}
}

static bool is_realtek_t1_phy(struct device_node *phy_node)
{
	u32 phy_addr = 0;
	bool ret;

	of_property_read_u32(phy_node, "reg", &phy_addr);

	/* Realtek T1 phys have addresses 0x0, 0x1, 0x2 and 0x3 */
	switch (phy_addr) {
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
		ret = true;
		break;
	default:
		ret = false;
	}

	return ret;
}

static int rswitch2_phy_init(struct device_node *child, struct net_device *ndev)
{
	struct device_node *pn;
	struct phy_device *phydev;
	struct port_private *priv = netdev_priv(ndev);
	int mdio_val = 0;
	int err;

	priv->link = 0;
	priv->speed = 0;
	priv->duplex = -1;

	pn = of_parse_phandle(child, "phy-handle", 0);
	if (!pn) {
		/* In the case of a fixed PHY, the DT node associated
		 * to the PHY is the Ethernet MAC DT node.
		 */
		if (of_phy_is_fixed_link(child)) {
			err = of_phy_register_fixed_link(child);
			if (err)
				return err;
		}
		pn = of_node_get(child);
	}

	phydev = of_phy_connect(ndev, pn, port_phy_eventhandler, 0,
				priv->phy_interface);
	if (!phydev) {
		netdev_err(ndev, "failed to connect PHY\n");
		err = -ENOENT;
		goto err_deregister_fixed_link;
	}
	phydev->irq = PHY_POLL;

	phy_set_max_speed(phydev, SPEED_1000);

	phydev->supported = PHY_1000BT_FEATURES | PHY_100BT_FEATURES | SUPPORTED_Autoneg;
	/* 10BASE is not supported */
	phydev->supported &= ~PHY_10BT_FEATURES;
	//phydev->supported &= ~SUPPORTED_100baseT_Half;
	phy_attached_info(phydev);
	priv->phydev = phydev;
	if (priv->portindex != 5)
		priv->phydev->autoneg = 0;
	else
		priv->phydev->autoneg = 1;
	/* Workaround: Normally the phy led-triggers are automatically
	 * created when calling of_phy_connect(), one trigger for each
	 * supported speed that is read from the phy BMSR register.
	 * However our Realtek T1 phys return no supported speed from BMSR
	 * so we need to call phy_led_triggers_register() again here
	 * *after* the supported speed have been set in
	 * phydev->supported.
	 * This will be removed once we have a Realtek PHY driver instead
	 * of the genphy.
	 */
	if (is_realtek_t1_phy(pn)) {
		err = phy_led_triggers_register(phydev);
		if (err)
			pr_err("phy_led_triggers_register() for node \"%s\" failed!! (err=%d)\n",
			       pn->name, err);
	}

	mdio_val = mdiobus_read(phydev->mdio.bus, 0x0, 0x18);
	pr_info("Phy Register address for 0 is %x\n", mdio_val);

	of_node_put(pn);

	return 0;

err_deregister_fixed_link:
	if (of_phy_is_fixed_link(child))
		of_phy_deregister_fixed_link(child);
	return err;
}

static int rswitch2_enable_interrupts(void)
{
	iowrite32(0xFFFF, gwca_addr + GWDIE0);
	iowrite32(0x1, gwca_addr + GWTSDIE);

	return 0;
}

static int port_open(struct net_device *ndev)
{
	struct port_private *mdp = netdev_priv(ndev);
	int ret = 0;
	static int port_count;

#ifdef INTERNAL_GW
	if (mdp->portnumber == board_config.eth_ports) {
		napi_enable(&mdp->napi[0]);
		pr_info("Napi Enabled for GW Port\n");
		netif_start_queue(ndev);

		mdp->opened = true;
		if (port_count == 0) {
			rswitch2_enable_interrupts();
			port_count++;
		}

		return ret;
	}
#endif

	napi_enable(&mdp->napi[0]);
	napi_enable(&mdp->napi[1]);
	/* device init */
	ret = port_dev_init(ndev);
	if (ret != 0) {
		port_setstate(ndev, rswitch2_portstate_failed);
		return ret;
	}

	// Change Port from CONFIG to OPERATE mode
	ret = port_changestate(ndev, rswitch2_portstate_operate);
	if (ret < 0) {
		pr_err("Port Change State to Operate Failed\n");
		return ret;
	}
	ptp_rswitch2_port_init(ndev, mdp->pdev);
	netif_start_queue(ndev);
	mdp->opened = true;
	//ndev->phydev->state = PHY_UP;

	if (board_variant == 3) {
		if (mdp->portindex != 0) {
			/* Software reset Phy can be in PDWN mode*/
			phy_write(ndev->phydev, 0, 0x8000);
			ret = phydev_disable_bcast_seterrata(ndev, 100, 1);
			if (ret < 0) {
				pr_err("Phy Disable Bcast and config failed\n");
				return ret;
			}
			phy_start(ndev->phydev);
		}
	} else if (board_variant == 2) {
#ifdef PHY_CONFIG_SUPPORT
		phy_start(ndev->phydev);
#endif
	}

	if (port_count == 0) {
		rswitch2_enable_interrupts();
		port_count++;
	}

	return ret;
}

static struct net_device_stats *port_get_statistics(struct net_device *ndev)
{
	struct port_private *mdp = netdev_priv(ndev);
	struct net_device_stats *nstats;
	struct net_device_stats *stats = &mdp->stats;

	nstats = &ndev->stats;
	nstats->rx_packets = stats->rx_packets;
	nstats->tx_packets = stats->tx_packets;
	nstats->rx_bytes = stats->rx_bytes;
	nstats->tx_bytes = stats->tx_bytes;

	return &ndev->stats;
}

static int port_close(struct net_device *ndev)
{
	struct port_private *mdp = netdev_priv(ndev);
	struct rswitch2_tstamp_skb *ts_skb, *ts_skb2;
	int err = 0;

	if (NOT mdp->opened) {
		pr_err("[RSWITCH2] ERROR: Attempting to close netdev (%s) when already closed\n",
		       ndev->name);
		return -ENODEV;
	}
	if (mdp->portnumber == board_config.eth_ports) {
		napi_disable(&mdp->napi[0]);
		netif_stop_queue(ndev);
	} else {
		list_for_each_entry_safe(ts_skb, ts_skb2, &mdp->ts_skb_list, list) {
			list_del(&ts_skb->list);
			kfree(ts_skb);
		}
		napi_disable(&mdp->napi[0]);
		napi_disable(&mdp->napi[1]);
		netif_stop_queue(ndev);

		//free_irq(board_config.Eth_Port[mdp->portnumber].Virtual_IRQ, ndev);

		// Change Port to CONFIG
		err = port_changestate(ndev, rswitch2_portstate_config);
		if (err < 0) {
			pr_err("Port Change to Config Failed\n");
			return err;
		}

		if (ndev->phydev) {
			//phy_write(ndev->phydev,0, 0x8000);
			phy_stop(ndev->phydev);
			//phy_write(ndev->phydev,0, 0x8000);
		}
	}
	mdp->opened = false;

	return 0;
}

static int rswitch2_tx_free(struct net_device *ndev, int q, bool free_txed_only)
{
	struct port_private *priv = netdev_priv(ndev);
	struct rswitch2_ext_desc *desc;
	int free_num = 0;
	int entry;
	int entry_cpy = 0;
	u32 size;

	for (; priv->cur_tx[q] - priv->dirty_tx[q] > 0; priv->dirty_tx[q]++) {
		bool txed;

		entry = priv->dirty_tx[q] % (priv->num_tx_ring[q] * NUM_TX_DESC);
		entry_cpy = entry;

		desc = &priv->tx_ring[q][entry];

		txed = ((desc->die_dt & 0xF0) | 0x08) == DT_FEMPTY;

		if (free_txed_only && !txed)
			break;

		/* Descriptor type must be checked before all other reads */
		dma_rmb();
		size = le16_to_cpu((desc->info_ds & 0xFFF)) & TX_DS;
		/* Free the original skb. */

		if (priv->tx_skb[q][entry / NUM_TX_DESC]) {
			dma_unmap_single(&pcidev->dev, le32_to_cpu(desc->dptrl),
					 size, DMA_TO_DEVICE);

			/* Last packet descriptor? */
			if (entry % NUM_TX_DESC == NUM_TX_DESC - 1) {
				entry /= NUM_TX_DESC;
				if (q % 2 == 0)
					dev_kfree_skb_any(priv->tx_skb[q][entry]);
				priv->tx_skb[q][entry] = NULL;
			}
			free_num++;
		}

		desc->die_dt = DT_EEMPTY;
	}

	return free_num;
}

static int port_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct port_private *priv;
	struct net_device_stats *stats;
	u16 q = 0;
	struct rswitch2_ext_desc *desc;
	struct rswitch2_ext_desc *desc_first;
	unsigned long flags;
	u32 dma_addr;
	void *buffer;
	u32 entry;
	u32 len;
	struct rswitch2_tstamp_skb *ts_skb;

	priv = netdev_priv(ndev);
	stats = &priv->stats;

#ifdef DEBUG
	pr_info("Xmit called\n");
#endif
	if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)
		q = RSWITCH2_QUEUE_NC;

	spin_lock_irqsave(&priv->lock, flags);
	if (priv->cur_tx[q] - priv->dirty_tx[q] > (priv->num_tx_ring[q] - 1) *
	    NUM_TX_DESC) {
		netif_err(priv, tx_queued, ndev,
			  "still transmitting with the full ring!\n");
		pr_err("Error still transmitting with the full ring\n");
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
	desc->dptrl = cpu_to_le32(dma_addr);
	desc->info_ds = cpu_to_le16(len);

#ifdef INTERNAL_GW
	if (priv->portnumber != board_config.eth_ports)
#endif
	{
		desc->info1l = 0x01 << 2; // Direct Descriptor
	}

#ifdef INTERNAL_GW
	if (priv->portnumber != board_config.eth_ports)
#endif
	{
		desc->info1h = (((u32)1) << priv->portnumber) << 16;
	}
	buffer = skb->data + len;
	len = skb->len - len;
	dma_addr = dma_map_single(&pcidev->dev, buffer, len, DMA_TO_DEVICE);
	if (dma_mapping_error(&pcidev->dev, dma_addr))
		goto unmap;

	desc++;
	desc->info_ds = cpu_to_le16(len);

	desc->dptrl = cpu_to_le32(dma_addr);

#ifdef TX_TS_FEATURE
	/* TX timestamp required */
	if ((q) == RSWITCH2_QUEUE_NC) {
		ts_skb = kmalloc(sizeof(*ts_skb), GFP_ATOMIC);

		if (!ts_skb) {
			desc--;
			dma_unmap_single(&pcidev->dev, dma_addr, len,
					 DMA_TO_DEVICE);
			goto unmap;
		}
		ts_skb->skb = skb;
		ts_skb->tag = priv->ts_skb_tag++;

		priv->ts_skb_tag &= 0xff;
		if (priv->ts_skb_tag == 0xFF)
			priv->ts_skb_tag = 0x0;
		list_add_tail(&ts_skb->list, &priv->ts_skb_list);

		/* TAG and timestamp required flag */
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;

		desc_first->info1l |= (ts_skb->tag & 0xFF) << 8;
		desc_first->info1l  |= (1 << 3);// | TX_TSR;
		/* Check if queue changed , frame type 0x88F7(ptp), message type delay request(0x12)*/
		if (skb->data[skb->len - 1] && tx_queue != skb->data[skb->len - 1] &&
		    skb->data[14] == 0x12 && skb->data[12] == 0x88 &&
		    skb->data[13] == 0xF7) {
			tx_queue = skb->data[skb->len - 1];
			pr_info("PTP tx queue=%d\n", tx_queue);
		}
		desc_first->info1l |= (tx_queue << 28); // IPV to tx queue
	}
#endif
	skb_tx_timestamp(skb);
	/* Descriptor type must be set after all the above writes */
	dma_wmb();
	desc->die_dt = DT_FEND;
	desc--;
	desc->die_dt = DT_FSTART;

	rswitch2_modify(ndev, GWTRC0, GWTRC_TSRQ0 << (q + NUM_RX_QUEUE + (priv->portnumber * 2)),
			GWTRC_TSRQ0 << (q + NUM_RX_QUEUE + (priv->portnumber * 2)));
	priv->cur_tx[q] += NUM_TX_DESC;
	if (priv->cur_tx[q] - priv->dirty_tx[q] >
	    (priv->num_tx_ring[q] - 1) * NUM_TX_DESC &&
	    !rswitch2_tx_free(ndev, q, true)) {
		pr_info("Stopping subqueue\n");
		netif_stop_subqueue(ndev, q);
	}
	stats->tx_packets++;
	stats->tx_bytes += len;

exit:
	mmiowb();
	spin_unlock_irqrestore(&priv->lock, flags);

	return NETDEV_TX_OK;

unmap:
	dma_unmap_single(&pcidev->dev, le32_to_cpu(desc->dptrl),
			 le16_to_cpu((desc->info_ds & 0xFFF)), DMA_TO_DEVICE);
drop:
	dev_kfree_skb_any(skb);
	priv->tx_skb[q][entry / NUM_TX_DESC] = NULL;
	goto exit;

	return NETDEV_TX_OK;
}

static int rswitch2_hwtstamp_get(struct net_device *ndev, struct ifreq *req)
{
	struct port_private *mdp = netdev_priv(ndev);
	struct hwtstamp_config config;

	config.flags = 0;
	config.tx_type = mdp->tstamp_tx_ctrl ? HWTSTAMP_TX_ON :
					       HWTSTAMP_TX_OFF;
	if (mdp->tstamp_rx_ctrl & RSWITCH2_RXTSTAMP_TYPE_V2_L2_EVENT)
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
	else if (mdp->tstamp_rx_ctrl & RSWITCH2_RXTSTAMP_TYPE_ALL)
		config.rx_filter = HWTSTAMP_FILTER_ALL;
	else
		config.rx_filter = HWTSTAMP_FILTER_NONE;

	return copy_to_user(req->ifr_data, &config, sizeof(config)) ?
	       -EFAULT : 0;
}

static int rswitch2_port_hwtstamp_set(struct net_device *ndev, struct ifreq *ifr, int cmd)
{
	struct port_private *mdp = netdev_priv(ndev);
	struct hwtstamp_config config;
	u32 tstamp_tx_ctrl;
	u32 tstamp_rx_ctrl = RSWITCH2_TSN_RXTSTAMP_ENABLED;

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	/* reserved for future extensions */
	if (config.flags)
		return -EINVAL;

	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
		tstamp_tx_ctrl = 0;
		break;
	case HWTSTAMP_TX_ON:
		tstamp_tx_ctrl = RSWITCH2_TSN_TXTSTAMP_ENABLED;
		break;
	/* Below line for One Step Timestamping */
	case 2:
		tstamp_tx_ctrl = RSWITCH2_TSN_TXTSTAMP_ENABLED;
		break;
	default:
		pr_err("[RSWITCH] %s Set HW TimeStamp. Tx_type(%u) Invalid\n",
		       ndev->name, config.tx_type);
		return -ERANGE;
	}

	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		tstamp_rx_ctrl = 0;
		break;

	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
		tstamp_rx_ctrl |= RSWITCH2_RXTSTAMP_TYPE_V2_L2_EVENT;
		break;

	default:
		config.rx_filter = HWTSTAMP_FILTER_ALL;
		tstamp_rx_ctrl |= RSWITCH2_RXTSTAMP_TYPE_ALL;
	}

	mdp->tstamp_tx_ctrl = tstamp_tx_ctrl;
	mdp->tstamp_rx_ctrl = tstamp_rx_ctrl;

	return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ? -EFAULT : 0;
}

static int rswitch2_port_IOCTL(struct net_device *ndev, struct ifreq *rq, int cmd)
{
	struct port_private *mdp = netdev_priv(ndev);
	struct phy_device *phydev = mdp->phydev;

	if (!netif_running(ndev))
		return -EINVAL;

	switch (cmd) {
	case SIOCGHWTSTAMP:
		return rswitch2_hwtstamp_get(ndev, rq);

	case SIOCSHWTSTAMP:
		return rswitch2_port_hwtstamp_set(ndev, rq, cmd);

	default:
		return phy_mii_ioctl(phydev, rq, cmd);
	}
}

static const struct net_device_ops  port_netdev_ops = {
	.ndo_open = port_open,
	.ndo_stop = port_close,
	.ndo_do_ioctl = rswitch2_port_IOCTL,
	.ndo_start_xmit = port_start_xmit,
	.ndo_get_stats = port_get_statistics,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_change_mtu = eth_change_mtu
};

static int port_mii_initialise(struct net_device *ndev)
{
	u32 mir = 0;

	//mir = RSWITCH2_MIR_MII;
	//mir = (0x01130000 + 0x13);
	mir = 0x01130000 + 0xb;
	portreg_write(ndev, mir, MPIC);

	mir = portreg_read(ndev, MPIC);

	return 0;
}

static int rswitch2_ts_desc_init(struct platform_device *pdev)
{
	int error = 0;

	/* Allocate descriptor base address table */
	ts_desc_bat_size = sizeof(struct rswitch2_desc) * NUM_TS_DESC;
	ts_desc_bat = dma_alloc_coherent(&pcidev->dev, ts_desc_bat_size,
					 &ts_desc_bat_dma, GFP_KERNEL);
#ifdef DEBUG
	pr_info("TS Desc bat = %llx\n", ts_desc_bat_dma);
#endif
	if (!ts_desc_bat) {
		dev_err(&pdev->dev,
			"Cannot allocate desc base address table (size %d bytes)\n",
			ts_desc_bat_size);
		error = -ENOMEM;
	}

	ts_desc_bat->die_dt = DT_EOS;
	iowrite32(ts_desc_bat_dma, gwca_addr + GWTDCAC10);
	iowrite32(0x0, gwca_addr + GWTDCAC00);
	return error;
}

static int rswitch2_desc_init(struct platform_device *pdev)
{
	int q = 0;
	int error = 0;

	/* Allocate descriptor base address table */

	desc_bat_size = sizeof(struct rswitch2_desc) * board_config.dbat_entry_num;
	pr_info("desc_bat_size = %d\n", desc_bat_size);
	desc_bat = dma_alloc_coherent(&pcidev->dev, desc_bat_size,
				      &desc_bat_dma, GFP_KERNEL);
#ifdef DEBUG
	pr_info("Desc bat = %llx\n", desc_bat_dma);
#endif
	if (!desc_bat) {
		dev_err(&pdev->dev,
			"Cannot allocate desc base address table (size %d bytes)\n",
			desc_bat_size);
		error = -ENOMEM;
	}
	for (q = RSWITCH2_BE; q < board_config.dbat_entry_num; q++)
		desc_bat[q].die_dt = DT_EOS;
#ifdef DEBUG
	pr_info("Reg=%x Value = %llx\n", GWDCBAC1, desc_bat_dma);
#endif
	iowrite32(desc_bat_dma, gwca_addr + GWDCBAC1);
	return error;
}

static bool rswitch2_rx(struct net_device *ndev, int *quota, int q)
{
	struct port_private *priv = netdev_priv(ndev);
	u32 portnumber  = 0;
	int entry = cur_rx[q] % num_rx_ring[q];
	int boguscnt = (dirty_rx[q] + num_rx_ring[q]) - cur_rx[q];
	/* Need to put queue wise */
	struct net_device_stats *stats = &priv->stats;
	struct rswitch2_ext_ts_desc *desc;
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
		portnumber = ((u64)(desc->info1 >> 36)) & 0x7;
#ifdef INTERNAL_GW
		if (priv->portnumber == board_config.eth_ports) {
			portnumber = board_config.eth_ports;
			//pr_info("Port Number = %d\n", portnumber);
		}
#endif

		ndev = ppndev[portnumber];
		//pr_info("Port Number1 = %d\n", portnumber);

#ifdef DEBUG_RX
		//if (priv->portnumber != portnumber)
			//pr_err("Mismatch\n");
#endif
		priv = netdev_priv(ndev);
		/* Descriptor type must be checked before all other reads */
		dma_rmb();

		pkt_len = le16_to_cpu(desc->info_ds) & RX_DS;

		if (--boguscnt < 0)
			break;

		/* We use 0-byte descriptors to mark the DMA mapping errors */
		if (!pkt_len)
			continue;

		get_ts = priv->tstamp_rx_ctrl & RSWITCH2_TSN_RXTSTAMP_TYPE;
		skb = rx_skb[q][entry];
		rx_skb[q][entry] = NULL;

		dma_unmap_single(&pcidev->dev, le32_to_cpu(desc->dptrl),
				 PKT_BUF_SZ,
				 DMA_FROM_DEVICE);

		get_ts &= (q == RSWITCH2_DESC_NC) ?
			   RSWITCH2_RXTSTAMP_TYPE_V2_L2_EVENT :
			   ~RSWITCH2_RXTSTAMP_TYPE_V2_L2_EVENT;
		if (get_ts) {
			struct skb_shared_hwtstamps *shhwtstamps;

			shhwtstamps = skb_hwtstamps(skb);
			memset(shhwtstamps, 0, sizeof(*shhwtstamps));
			ts.tv_sec = (u64)le32_to_cpu(desc->ts_sec);
			ts.tv_nsec = le32_to_cpu(desc->ts_nsec & 0x3FFFFFFF);
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

	/* Refill the RX ring buffers. */
	for (; cur_rx[q] - dirty_rx[q] > 0; dirty_rx[q]++) {
		entry = dirty_rx[q] % num_rx_ring[q];
		desc = &rx_ring[q][entry];
		desc->info_ds = cpu_to_le16(PKT_BUF_SZ);

		if (!rx_skb[q][entry]) {
			skb = dev_alloc_skb(PKT_BUF_SZ + RSWITCH2_ALIGN - 1);

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
			desc->dptrl = cpu_to_le32(dma_addr);
			rx_skb[q][entry] = skb;
		}
		/* Descriptor type must be set after all the above writes */
		dma_wmb();

		desc->die_dt = DT_FEMPTY;
	}

	*quota -= limit - (++boguscnt);

	//pr_info("Returned from RX\n");
	return boguscnt <= 0;
}

static int rswitch2_poll(struct napi_struct *napi, int budget)
{
	struct net_device *ndev = napi->dev;
	struct port_private *priv = netdev_priv(ndev);
	unsigned long flags;
	int q = napi - priv->napi;
	u32 portnumber = 0;
	u8 np = 0;
	struct rswitch2_ext_ts_desc *desc;
	int quota = budget;
	u32 ris, tis;
	u32 tis_q = 0;
	u32 rxq = q;
	u32 rx_mask = 0;
	u32 tis_port  = 0;
	int entry = 0;
	u32 dis = 0;

#ifdef INTERNAL_GW
	/* Try registering separate poller for Internal GW */
	if (priv->portnumber == board_config.eth_ports)
		rxq = RSWITCH2_RX_GW_QUEUE_NUM;
	else
		rxq = q + 1;
#endif
	np = napi_pending[priv->portnumber][rxq];
	for (;;) {
		rx_mask = BIT(rxq);
		dis = ioread32(gwca_addr + GWDIS0);
		ris = (dis & board_config.ris_bitmask) >> board_config.rx_queue_offset;
		tis = (dis & board_config.tis_bitmask) >> (NUM_RX_QUEUE);

#ifdef DEBUG
		pr_info("Value of ris =%x\n", ris);
		pr_info("Value of tis =%x\n", tis);
		pr_info("rx_mask = %d\n", rx_mask);
#endif
		if (!((ris & rx_mask) || (tis  >> (q + (priv->portnumber * 2))) || np))
			break;

		if ((ris & rx_mask) || (np)) {
			np = 0;
			/* Processing RX Descriptor Ring */
			entry = cur_rx[rxq] % num_rx_ring[rxq];
			desc = &rx_ring[rxq][entry];
			portnumber = ((u64)(desc->info1 >> 36)) & 0x7;

			if (((priv->portnumber == board_config.eth_ports)) || (ris == 0x01))
				portnumber = board_config.eth_ports;


			if (portnumber !=  priv->portnumber) {
#ifdef DEBUG_RX
				pr_info("Port Number Mismatch\n");
#endif
			}
			/* Clear RX interrupt */
			iowrite32(((rx_mask)), gwca_addr + GWDIS0);
			if (rswitch2_rx(ppndev[portnumber], &quota, rxq)) {
				napi_pending[portnumber][rxq] = 1;
				goto out;
			}
		}

		/* Processing TX Descriptor Ring */
		tis_q = tis % 2;
		tis_port = tis / 2;
		if (tis != 0) {
			if (tis >> (q + (priv->portnumber * 2))) {
				spin_lock_irqsave(&priv->lock, flags);

				/* Clear TX interrupt */
				iowrite32((tis  << (NUM_RX_QUEUE)), gwca_addr + GWDIS0);
				rswitch2_tx_free(ndev, q, true);
				netif_wake_subqueue(ndev, q);
				mmiowb();
				spin_unlock_irqrestore(&priv->lock, flags);
			}
		}
	}
	napi_complete(napi);
	napi_pending[portnumber][rxq] = 0;

	/* Re-enable RX/TX interrupts */
	iowrite32(0xFFFFFFFF, gwca_addr + GWDIE0);
	mmiowb();

out:
	return budget - quota;
}

static bool rswitch2_queue_interrupt(struct net_device *ndev, int q)
{
	struct port_private *priv = netdev_priv(ndev);

	/* Registered 7 Napis one per port*/
	if (napi_schedule_prep(&priv->napi[q % 2])) {
		/* Mask RX and TX interrupts */
		iowrite32(0xFFFFFFFF, gwca_addr + GWDID0);
		__napi_schedule(&priv->napi[q % 2]);
	}
	return true;
}

static bool rswitch2_tx_ts_interrupt(struct net_device *ndev)
{
	struct port_private *priv = netdev_priv(ndev);
	struct rswitch2_ts_desc *desc;
	struct rswitch2_tstamp_skb *ts_skb, *ts_skb2;
	struct skb_shared_hwtstamps shhwtstamps;
	struct timespec64 ts;
	struct sk_buff *skb;
	u16 tsun, tfa_tsun;
	int entry = cur_ts % TS_RING_SIZE;

	desc = &ts_ring[entry];
	if (desc->die_dt != DT_FEMPTY_ND) {
		ts.tv_nsec = desc->ts_nsec & 0x3FFFFFFF;
		ts.tv_sec = desc->ts_sec;
		tfa_tsun = ((desc->dptrl) & 0xFF);

		memset(&shhwtstamps, 0, sizeof(shhwtstamps));
		shhwtstamps.hwtstamp = timespec64_to_ktime(ts);

		list_for_each_entry_safe(ts_skb, ts_skb2, &priv->ts_skb_list, list) {
			skb = ts_skb->skb;
			tsun = ts_skb->tag;

			list_del(&ts_skb->list);
			kfree(ts_skb);

			if (tsun == tfa_tsun) {
				/* If timestamp for given tsun find clone skb and pass back to socket */
				skb_tstamp_tx(skb, &shhwtstamps);
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
		//desc->info_ds = cpu_to_le16(PKT_BUF_SZ);
		desc->info_ds = 0;
		desc->die_dt = DT_FEMPTY_ND;
	}
	return 0;
}

static irqreturn_t rswitch2_eth_interrupt(int IRQ, void *dev_id)
{
	u32 ris = 0;
	u32 tis = 0;
	u32 tsdis = 0;
	int q = 0;
	struct rswitch2_ext_ts_desc *desc;
	struct rswitch2_ts_desc *ts_desc;
	u32 portnumber = 0;
	u32 entry = 0;
	irqreturn_t ret = IRQ_HANDLED;
	u32 dis = 0;

#ifdef DEBUG
	pr_info("Interrupt Happened\n");
#endif

	spin_lock(&port_lock);

	/* Check Descriptor Interrupt status*/
	tsdis = ioread32(gwca_addr + GWTSDIS);
	dis = ioread32(gwca_addr + GWDIS0);
	ris = (dis & board_config.ris_bitmask) >> board_config.rx_queue_offset;
	tis = (dis & board_config.tis_bitmask) >> (NUM_RX_QUEUE);
	entry = cur_ts % num_ts_ring;
	ts_desc = &ts_ring[entry];
	do {
		if (tsdis != 0) {
			iowrite32(0x01, gwca_addr + GWTSDID);
			iowrite32(0x01, gwca_addr + GWTSDIS);
			portnumber = (uint64_t)(ts_desc->dptrl >> 16) & 0x7;
			rswitch2_tx_ts_interrupt(ppndev[portnumber]);
			iowrite32(0x01, gwca_addr + GWTSDIE);
			ret = IRQ_HANDLED;
		}
		if (ris != 0) {
			for (q = NUM_RX_QUEUE - 1; q >= 0; q--) {
				if (((ris >> q) & 0x01) == 0x01) {
					int entry = cur_rx[q] % num_rx_ring[q];

					desc = &rx_ring[q][entry];
					portnumber = (u64)(desc->info1 >> 36) & 0x7;
#ifdef INTERNAL_GW
					if (q == RSWITCH2_RX_GW_QUEUE_NUM)
						portnumber = board_config.eth_ports;
					else
						q = q - 1;
#endif
					if (rswitch2_queue_interrupt(ppndev[portnumber], q))
						ret = IRQ_HANDLED;
				}
			}
		}
		if (tis != 0) {
			for (q = (board_config.dbat_entry_num); q >= 0; q--) {
				if ((tis >> q) & 0x01) {
					if (rswitch2_queue_interrupt(ppndev[q / 2], q))
						ret = IRQ_HANDLED;
				}
			}
		}
		tsdis = ioread32(gwca_addr + GWTSDIS);
	} while (tsdis != 0);
	spin_unlock(&port_lock);
	return ret;
}

static int drv_probe_createnetdev(struct platform_device *pdev, u32 portnumber,
				  struct net_device **ppndev,
				  struct device_node *child)
{
	struct net_device *ndev = NULL;
	struct port_private *mdp = NULL;
	char portname[IFNAMSIZ];
	int err = 0;
	u32 index = 0;
	u32 mir = 0;

	//mir = RSWITCH2_MIR_MII;
	//mir = 0x01130000 + 0x13;
	mir = 0x01130000 + 0xb;
	if (board_variant == 0x03) {
		if (portnumber != 5) {
			if (portnumber == 0)
				sprintf(portname, "%s%u", RSWITCH2ETH_BASE_PORTNAME, portnumber);
			if (portnumber == 1)
				sprintf(portname, "%s%u", RSWITCH2ETH_BASE_PORTNAME, portnumber+6);
			else if (portnumber == 2)
				sprintf(portname, "%s%u", RSWITCH2ETH_BASE_PORTNAME, portnumber+3);
			else if (portnumber == 3)
				sprintf(portname, "%s%u", RSWITCH2ETH_BASE_PORTNAME, portnumber+1);
			else if (portnumber == 4)
				sprintf(portname, "%s%u", RSWITCH2ETH_BASE_PORTNAME, portnumber+2);
		} else {
			strcpy(portname, "eth1");
		}
	} else if (board_variant == 0x02) {
		sprintf(portname, "%s%u", RSWITCH2ETH_BASE_PORTNAME, portnumber);
	}

	ndev = alloc_etherdev_mqs(sizeof(struct port_private), NUM_TX_QUEUE, NUM_RX_QUEUE);
	if (ndev == NULL) {
		dev_err(&pdev->dev, "[RSWITCH2] Failed to allocate %s device\n", portname);
		return -ENOMEM;
	}
	index = portnumber;
	if (board_variant == 0x03) {
		if ((portnumber == 4) || (portnumber == 5))
			portnumber = 0;
	}
	ndev->base_addr = board_config.eth_port[portnumber].start;            // net_device: The I/O base address of the network interface

	strncpy(ndev->name, portname, sizeof(ndev->name) - 1);
	strncpy(board_config.eth_port[portnumber].portname, portname,
		sizeof(board_config.eth_port[portnumber].portname)-1);

	SET_NETDEV_DEV(ndev, &pdev->dev);

	/* Fill in the fields of the device structure with ethernet values */
	ether_setup(ndev);

	/* Access private data within net_device and initialise it */
	mdp = netdev_priv(ndev);

	mdp->ndev = ndev;
	if (board_variant == 0x03)
		mdp->portindex = index;
	mdp->node = child;
	mdp->num_tx_ring[RSWITCH2_BE] = TX_RING_SIZE0;
	mdp->num_tx_ring[RSWITCH2_NC] = TX_RING_SIZE1;

	mdp->portnumber = portnumber;
	mdp->port_base_addr = ethport_addr[portnumber];
	mdp->pdev = pdev;

	mdp->ether_link_active_low = false;

	mdp->edmac_endian = EDMAC_LITTLE_ENDIAN;

	spin_lock_init(&mdp->lock);

	err = port_changestate(ndev, rswitch2_portstate_disable);
	if (err < 0) {
		free_netdev(ndev);
		return err;
	}

	/* Initialise HW timestamp list */
	INIT_LIST_HEAD(&mdp->ts_skb_list);
	err = port_changestate(ndev, rswitch2_portstate_config);
	if (err < 0) {
		pr_err("Failed from Change state\n");
		free_netdev(ndev);
		return err;
	}
	/* set function (Open/Close/Xmit/IOCTL etc) */
	ndev->netdev_ops = &port_netdev_ops;
	ndev->ethtool_ops = &port_ethtool_ops;
	ndev->max_mtu = 2048 - (ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN);
	ndev->min_mtu = ETH_MIN_MTU;
	/* debug message level */
	mdp->msg_enable = RSWITCH2_DEF_MSG_ENABLE;

	/* Read MAC address from Port. If invalid (not 00:00:00:00:00:00, is
	 * not a multicast address, and is not FF:FF:FF:FF:FF:FF), generate one
	 */
	port_query_mac_address(ndev);
	if (!is_valid_ether_addr(ndev->dev_addr)) {
		pr_err("[RSWITCH2] %s no valid MAC address supplied, using a generated one\n", ndev->name);
		port_generate_mac_address(portnumber, ndev);
	}
	/* MDIO bus initialise (Connect MAC to PHY) */
	// Port is now CONFIG
	err = port_changestate(ndev, rswitch2_portstate_config);
	if (err < 0) {
		pr_err("Port Change State to Config Failed\n");
		return err;
	}
	err = port_mii_initialise(ndev);
	if (err != 0) {
		dev_err(&pdev->dev, "[RSWITCH2] %s Failed to initialise MDIO. %d\n", ndev->name, err);
		free_netdev(ndev);
		return err;
	}

	/* print device information */
	pr_info("[RSWITCH2] %s Base address at 0x%x MAC(%pM) VRR(%08X)\n", ndev->name, (u32)ndev->base_addr, ndev->dev_addr, mdp->vrr);

	/* Save this network_device in the list of devices */
	*ppndev = ndev;
	portreg_write(ndev, mir, MPIC);
	netif_napi_add(ndev, &mdp->napi[0], rswitch2_poll, 64);
	netif_napi_add(ndev, &mdp->napi[1], rswitch2_poll, 64);
	return err;
}


static int drv_probe_ioremap_eth(u32 a)
{
	/* Get Base addresses and size for each Ethernet AVB/TSN port */
	board_config.eth_port[a].start = RSWITCH2_FPGA_ETH_BASE + (a * RSWITCH2_FPGA_ETH_PORT_SIZE);
	board_config.eth_port[a].size = RSWITCH2_FPGA_ETH_PORT_SIZE;
	board_config.eth_port[a].end = board_config.eth_port[a].start + RSWITCH2_FPGA_ETH_PORT_SIZE;
	ethport_addr[a] = ioaddr + RSWITCH2_FPGA_ETH_OFFSET + RSWITCH2_FPGA_ETH_PORT_SIZE * a;

#ifdef DEBUG
	pr_info("[RSWITCH2] IO Remap Port(%u) Address(0x%08X - 0x%08X) to (0x%08lX - 0x%08lX)\n",
		a, board_config.eth_port[a].start, board_config.eth_port[a].end,
		(uintptr_t)ethport_addr[a], (uintptr_t)ethport_addr[a] +
		board_config.eth_port[a].size - 1);
#endif
	return 0;
}

#ifdef INTERNAL_GW
static int drv_probe_createvirtualnetdev(struct platform_device *pdev,
					 uint32_t portnumber,
					 struct net_device **ppndev)
{
	struct net_device *ndev = NULL;
	struct port_private *mdp = NULL;
	int err = 0;

	/* Allocate the next tsn{n} device. Sets several net_device fields with
	 * appropriate values for Ethernet devices. This is a wrapper for
	 * alloc_netdev
	 */
	ndev = alloc_etherdev(sizeof(struct port_private));
	if (ndev == NULL) {
		dev_err(&pdev->dev, "[RSWITCH2] Failed to allocate %s device\n", "tsngw");
		return -ENOMEM;
	}

	ndev->dma = 1;	// net_device: DMA Not used on this architecture

	/* Software Internal GW port know as tsngw */
	strncpy(ndev->name, "tsngw", sizeof(ndev->name) - 1);

	/* Set the sysfs physical device reference for the network logical
	 * device if set prior to registration will cause a symlink during
	 * initialization.
	 */
	SET_NETDEV_DEV(ndev, &pdev->dev);

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(ndev);

	/* Access private data within net_device and initialise it */
	mdp = netdev_priv(ndev);

	/* Allocate the port number as the one more than total number of
	 * physical ports
	 */
	mdp->portnumber = board_config.eth_ports;

	mdp->pdev = pdev;
	mdp->num_tx_ring[RSWITCH2_BE] = TX_RING_SIZE0;
	num_rx_ring[2] = RX_RING_SIZE1;

	mdp->ether_link_active_low = false;

	mdp->edmac_endian = EDMAC_LITTLE_ENDIAN;

	spin_lock_init(&mdp->lock);

	/* set function (Open/Close/Xmit/IOCTL etc) */
	ndev->netdev_ops = &port_netdev_ops;
	ndev->ethtool_ops = &port_ethtool_ops;

	/* debug message level */
	mdp->msg_enable = RSWITCH2_DEF_MSG_ENABLE;

	/* Initialise HW timestamp list */
	INIT_LIST_HEAD(&mdp->ts_skb_list);

	/* Read MAC address from Port. If invalid (not 00:00:00:00:00:00, is not
	 * a multicast address, and is not FF:FF:FF:FF:FF:FF), generate one
	 */
	/* Use Automatic generated MAC address */
	port_generate_mac_address(portnumber, ndev);
	/* Save this network_device in the list of devices */
	*ppndev = ndev;

	netif_napi_add(ndev, &mdp->napi[0], rswitch2_poll, 64);
	pr_info("Internal SW Gateway Port Registered\n");

	return err;
}
#endif

static int drv_probe_ioremap_common(struct platform_device *pdev)
{
	board_config.gwca0.start = RSWITCH2_FPGA_ETH_BASE + (board_config.eth_ports * RSWITCH2_FPGA_ETH_PORT_SIZE);
	board_config.gwca0.size = RSWITCH2_FPGA_GWCA0_SIZE;
	board_config.gwca0.end = board_config.gwca0.start + RSWITCH2_FPGA_GWCA0_SIZE;
	gwca_addr = ioaddr + board_config.gwca0.start;

#ifdef DEBUG
	pr_info("[RSWITCH] IO Remap GWCA Address(0x%08X - 0x%08X) to (0x%08lX - 0x%08lX)\n",
	       board_config.gwca0.start, board_config.gwca0.start + RSWITCH2_FPGA_GWCA0_SIZE,
	       (uintptr_t)gwca_addr, (uintptr_t)gwca_addr + board_config.gwca0.size - 1);
#endif
	return 0;
}

static int rswitch2_bpool_config(void)
{
	int i = 0;
	int read_value = 0;

	/* Do Further Configuration here as per xml, Default value works fine
	 * so we just do a trigger now
	 */
	read_value = ioread32(ioaddr + CABPIRM);
	if (read_value == RSWITCH2_COM_BPR) {
		pr_err("Bpool already in ready state\n");
		return 0;
	}
	iowrite32(RSWITCH2_COM_BPIOG, ioaddr + CABPIRM);
	read_value = ioread32(ioaddr + CABPIRM);
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		read_value = ioread32(ioaddr + CABPIRM);

		if (read_value == RSWITCH2_COM_BPR)
			return 0;
		mdelay(1);
	}
	pr_err("Buffer Pool Initialisation Failed\n");

	return -1;
}

static int rswitch2_clock_enable(void)
{
	u32 rcec_val = 0;

#ifdef DEBUG
	pr_info("Reg=%x value = %x\n", RCEC, board_config.port_bitmask |
					     RCE_BIT);
#endif
	rcec_val = ioread32(ioaddr + RCEC);
	if (rcec_val == (board_config.port_bitmask | RCE_BIT))
		return 0;

	iowrite32(board_config.port_bitmask | RCE_BIT, ioaddr + RCEC);
	rcec_val = ioread32(ioaddr + RCEC);
	if (rcec_val != (board_config.port_bitmask | RCE_BIT)) {
		pr_err("Clock Enable Failed with RCEC = %x\n", rcec_val);
		return -1;
	}
	else
		pr_debug("Clock Enabled with RCEC = %x\n", rcec_val);

	return 0;
}

static int rswitch2_gwca_change_mode(enum rswitch2_gwca_mode mode)
{
	u32 rcec_val = 0;
	u32 rcdc_val = 0;
	u32 gwms_val = 0;
	u32 clock_enable = 0;
	u32 i = 0;
	int err = 0;

	rcec_val = ioread32(ioaddr + RCEC);
	clock_enable = (rcec_val >> 16) & ((rcec_val & 0xFFFF) == board_config.port_bitmask);
	if (!clock_enable) {
		err = rswitch2_clock_enable();
		if (err < 0) {
			pr_err("Clock Enable Failed\n");
			return err;
		}
	}
#ifdef DEBUG
	pr_info("Reg=%x value = %x\n", GWMC, mode);
#endif
	iowrite32(mode, gwca_addr + GWMC);

	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		gwms_val = ioread32(gwca_addr + GWMS);

		if (gwms_val == mode) {
			if (mode == rswitch2_gwca_mode_disable) {
				rcdc_val = 1 << board_config.eth_ports;
#ifdef DEBUG
				pr_info("Reg=%x value = %x\n", RCDC, rcdc_val);
#endif
				iowrite32(rcdc_val, ioaddr + RCDC);
			}
			return 0;
		}
		mdelay(1);
	}

	pr_err("++  GWCA mode change TIMEOUT. Reg(%x)  Wait(%08X) Last(%08X)\n",
	       GWMS, mode, gwms_val);

	return -1;
}

#ifdef FUTURE_USE
static int rswitch2_gwca_reset(void)
{
	int err = 0;

	err = rswitch2_gwca_change_mode(rswitch2_gwca_mode_disable);
	if (err < 0) {
		pr_err("Mode Change to Disable Failed\n");
		return err;
	}
	err = rswitch2_gwca_change_mode(rswitch2_gwca_mode_reset);
	if (err < 0) {
		pr_err("Mode Change to Reset Failed\n");
		return err;
	}
	err = rswitch2_gwca_change_mode(rswitch2_gwca_mode_disable);
	if (err < 0) {
		pr_err("Mode Change to Disable Failed\n");
		return err;
	}

	return 0;
}

static int rswitch2_reset(void)
{
#ifdef DEBUG
	pr_info("Reg=%x value = %x\n", RRC, 0x01);
#endif
	iowrite32(0x1, ioaddr + RRC);
#ifdef DEBUG
	pr_info("Reg=%x value = %x\n", RRC, 0x0);
#endif
	iowrite32(0x0, ioaddr + RRC);

	return 0;
}
#endif

int rswitch2_gwca_mcast_tbl_reset(void)
{
	u32 gwmtirm_val = 0;
	u32 i = 0;

#ifdef DEBUG
	pr_info("Reg=%x value = %x\n", GWMTIRM, 0x01);
#endif
	iowrite32(0x01, gwca_addr + GWMTIRM);
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		gwmtirm_val = ioread32(gwca_addr + GWMTIRM);

		if (((gwmtirm_val >> 1) & 0x01) == 0x01)
			return 0;
		mdelay(1);
	}

	pr_info("Multicast Table reset failed\n");

	return -1;
}

static int rswitch2_gwca_axi_ram_reset(void)
{
	u32 gwarirm_val = 0;
	u32 i = 0;

#ifdef DEBUG
	pr_info("Reg=%x value = %x\n", GWARIRM, 0x01);
#endif
	iowrite32(0x01, gwca_addr + GWARIRM);
	for (i = 0; i < RSWITCH2_PORT_CONFIG_TIMEOUT_MS; i++) {
		gwarirm_val = ioread32(gwca_addr + GWARIRM);

		if (((gwarirm_val >> 1) & 0x01) == 0x01)
			return 0;
		mdelay(1);
	}

	pr_info("AXI RAM reset failed\n");

	return -1;
}

static int rswitch2_gwca_init(void)
{
	int err = 0;

	err = rswitch2_gwca_change_mode(rswitch2_gwca_mode_disable);
	if (err < 0) {
		pr_err("Mode Change to Disable Failed\n");
		return err;
	}
	err = rswitch2_gwca_change_mode(rswitch2_gwca_mode_config);
	if (err < 0) {
		pr_err("Mode Change to Config Failed\n");
		return err;
	}
	err = rswitch2_gwca_mcast_tbl_reset();
	if (err < 0) {
		pr_err("Multicast Table Reset Failed\n");
		return err;
	}
	err = rswitch2_gwca_axi_ram_reset();
	if (err < 0) {
		pr_err("AXI RAM Reset Failed\n");
		return err;
	}

	/* Use Default setting values at start Configure via ioctl */
	return 0;
}

static int rswitch2_tx_ring_format(struct net_device *ndev, int q)
{
	struct port_private *priv = netdev_priv(ndev);
	struct rswitch2_ext_desc *tx_desc;
	struct rswitch2_desc *desc;
	int tx_ring_size = sizeof(*tx_desc) * priv->num_tx_ring[q] * NUM_TX_DESC;
	u32 gwdcc = 0;
	int i;
	u32 balr = 0;
	u32 dcp = 0;
	u32 dqt = 0;
	u32 ede = 0;

	priv->cur_tx[q] = 0;
	priv->dirty_tx[q] = 0;
	memset(priv->tx_ring[q], 0, tx_ring_size);
#ifdef DEBUG
	pr_info("priv->tx_ring[q]=%llx\n", priv->tx_desc_dma[q]);
#endif
	/* Build TX ring buffer */
	for (i = 0, tx_desc = priv->tx_ring[q]; i < priv->num_tx_ring[q];
	     i++, tx_desc++) {
		tx_desc->die_dt = DT_EEMPTY;
		tx_desc++;
		tx_desc->die_dt = DT_EEMPTY;
	}
	tx_desc->dptrl = cpu_to_le32((u32)priv->tx_desc_dma[q]);
	tx_desc->die_dt = DT_LINKFIX; /* type */

	desc = &desc_bat[q + NUM_RX_QUEUE + (priv->portnumber * 2)];
#ifdef DEBUG
	//pr_info("Value of Desc = %lx\n", desc_bat[q + (priv->portnumber * 2)]);
#endif
	desc->die_dt = DT_LINKFIX; /* type */
#ifdef DEBUG
	//pr_info("Value of Desc after = %x\n", desc_bat[q + (priv->portnumber *  2)]);
	pr_info("priv->tx_desc_dma[q] = %llx\n", priv->tx_desc_dma[q]);
#endif
	desc->dptrl = cpu_to_le32((u32)priv->tx_desc_dma[q]);

	balr = 1 << 24;
	dcp = (q + (priv->portnumber *  2)) << 16;
	dqt = 1 << 11;
	ede = 1 << 8;
	gwdcc = balr | dcp | dqt | ede;
#if 0
	pr_info("value of gwdcc = %x\n", gwdcc);
	if ((q == 0) && (priv->portnumber == 0))
		iowrite32(priv->tx_desc_dma[0], ioaddr + GWDCBAC1);
#endif
#ifdef DEBUG
	pr_info("Reg=%x value = %x\n", GWDCC0 + ((q + NUM_RX_QUEUE + (priv->portnumber * 2)) * 4),
	       balr | dcp | dqt | ede);
#endif
	iowrite32(balr | dcp | dqt | ede, gwca_addr + GWDCC0 + ((q + NUM_RX_QUEUE + (priv->portnumber * 2)) * 4));

	return 0;
}

static int rswitch2_ts_ring_free(void)
{
	struct rswitch2_ts_desc *ts_desc;
	int ts_ring_size = sizeof(*ts_desc) * num_ts_ring;

	if (ts_ring) {
		dma_free_coherent(&pcidev->dev, ts_ring_size, ts_ring, ts_desc_dma);
		ts_ring = NULL;
	}

	return 0;
}

static void rswitch2_ts_ring_format(void)
{
	struct rswitch2_ts_desc *ts_desc;
	struct rswitch2_desc *desc;
	s32 i;
	u32 dcs = 0;
	u32 te = 0;
	u32 ts_ring_size = sizeof(*ts_desc) * num_ts_ring;

	cur_ts = 0;
	dirty_ts = 0;

	/* Check if multiplication required */
	memset(ts_ring, 0, ts_ring_size);

	for (i = 0, ts_desc = ts_ring; i < num_ts_ring; i++, ts_desc++) {
		/* TS descriptor */
		//ts_desc->info_ds = cpu_to_le16(PKT_BUF_SZ); // Needs to be seen if you need zero byte descriptor for timestamp
		ts_desc->info_ds = 0;
		ts_desc->die_dt = DT_FEMPTY_ND;
	}

	ts_desc->dptrl = cpu_to_le32((u32)ts_desc_dma);
	ts_desc->die_dt = DT_LINKFIX; /* type */

	desc = ts_desc_bat;
	desc->die_dt = DT_LINKFIX; /* type */
	desc->dptrl = cpu_to_le32((u32)ts_desc_dma);

	dcs = 0 << 2;
	te = 1 << 0;
	iowrite32(dcs | te, gwca_addr + GWTSDCC0);
}

static int rswitch2_tx_ring_init(struct net_device *ndev, int q)
{
	struct port_private *priv = netdev_priv(ndev);
	int ring_size;

#ifdef DEBUG
	pr_info("priv->num_tx_ring[q]=%d\n", priv->num_tx_ring[q]);
	pr_info("Tx ring init start\n");
	pr_info("priv->num_tx_ring[q]=%d\n", priv->num_tx_ring[q]);
#endif
	priv->tx_skb[q] = kcalloc(priv->num_tx_ring[q],
				  sizeof(*priv->tx_skb[q]), GFP_KERNEL);
#ifdef DEBUG
	pr_info("Tpriv->num_tx_ring[q]=%d\n", priv->num_tx_ring[q]);
#endif
	if (!priv->tx_skb[q])
		goto error;

	/* Allocate rings for the aligned buffers */
	priv->tx_align[q] = kmalloc(DPTR_ALIGN * priv->num_tx_ring[q] +
				    DPTR_ALIGN - 1, GFP_KERNEL);
	if (!priv->tx_align[q])
		goto error;

	/* Allocate all TX descriptors. */
	ring_size = sizeof(struct rswitch2_ext_desc) *
			   (priv->num_tx_ring[q] * NUM_TX_DESC + 1);

	priv->tx_ring[q] = dma_alloc_coherent(&pcidev->dev, ring_size,
					      &priv->tx_desc_dma[q],
					      GFP_KERNEL);
	if (!priv->tx_ring[q])
		goto error;

	return 0;

error:
	return -ENOMEM;
}

static int rswitch2_ts_ring_init(void)
{
	int ts_ring_size = 0;

	ts_ring_size = sizeof(struct rswitch2_ts_desc) * (num_ts_ring + 1);
	ts_ring = dma_alloc_coherent(&pcidev->dev, ts_ring_size,
				     &ts_desc_dma, GFP_KERNEL);
	dirty_ts = 0;

	return 0;
}

static int rswitch2_txdmac_init(struct net_device *ndev)
{
	int error;

	error = rswitch2_tx_ring_init(ndev, RSWITCH2_BE);
	if (error < 0) {
		pr_err("TX Ring Init Failed\n");
		return error;
	}
	error = rswitch2_tx_ring_init(ndev, RSWITCH2_NC);
	if (error < 0) {
		pr_err("TX Ring Init Failed\n");
		return error;
	}
	/* Descriptor format */
	error = rswitch2_tx_ring_format(ndev, RSWITCH2_BE);
	if (error < 0) {
		pr_err("TX Ring Format Failed\n");
		return error;
	}
	error = rswitch2_tx_ring_format(ndev, RSWITCH2_NC);
	if (error < 0) {
		pr_err("TX Ring Format Failed\n");
		return error;
	}

	return 0;
}

static int rswitch2_gw_txdmac_init(struct net_device *ndev)
{
	int error;

	error = rswitch2_tx_ring_init(ndev, RSWITCH2_BE);
	if (error < 0) {
		pr_err("TX Ring Init Failed\n");
		return error;
	}

	/* Descriptor format */
	error = rswitch2_tx_ring_format(ndev, RSWITCH2_BE);
	if (error < 0) {
		pr_err("TX Ring Format Failed\n");
		return error;
	}

	return 0;
}

static int rswitch2_rx_ring_format(int q)
{
	struct rswitch2_ext_ts_desc *rx_desc;
	struct rswitch2_desc *rx_ts_desc;
	int rx_ring_size = sizeof(*rx_desc) * num_rx_ring[q];
	dma_addr_t dma_addr;
	int i;
	u32 balr = 0;
	u32 ets  = 0;
	u32 dqt = 0;
	u32 ede = 0;
	u32 gwdcc = 0;

	dirty_rx[q] = 0;
	cur_rx[q] = 0;

	memset(rx_ring[q], 0, rx_ring_size);
	for (i = 0; i < num_rx_ring[q]; i++) {
		/* RX descriptor */
		rx_desc = &rx_ring[q][i];

		rx_desc->info_ds = cpu_to_le16(PKT_BUF_SZ);

		dma_addr = dma_map_single(&pcidev->dev, (void *)rx_skb[q][i]->data,
					  PKT_BUF_SZ, DMA_FROM_DEVICE);

		/* We just set the data size to 0 for a failed mapping which
		 * should prevent DMA from happening...
		 */
		if (dma_mapping_error(&pcidev->dev, dma_addr)) {
			pr_err("Descriptor Mapping error\n");
			rx_desc->info_ds = cpu_to_le16(0);
		}
		rx_desc->dptrl = cpu_to_le32(dma_addr);
		rx_desc->die_dt = DT_FEMPTY;
	}

	rx_desc = &rx_ring[q][i];
	rx_desc->dptrl = cpu_to_le32((u32)rx_desc_dma[q]);
	rx_desc->die_dt = DT_LINKFIX; /* type */
	rx_ts_desc = &desc_bat[q + board_config.rx_queue_offset];

	rx_ts_desc->die_dt = DT_LINKFIX; /* type */
	rx_ts_desc->dptrl = cpu_to_le32((u32)rx_desc_dma[q]);
	balr = 1 << 24;
	ets = 1 << 9;
	dqt = 0 << 11;
	ede = 1 << 8;
	gwdcc = balr | dqt | ede | ets;
#if 0
	pr_info("value of gwdcc = %x\n", gwdcc);
	if ((q == 0) && (priv->PortNumber == 0))
		iowrite32(priv->tx_desc_dma[0], ioaddr + GWDCBAC1);
#endif
#ifdef DEBUG
	pr_info("Reg=%x value = %x Desc num=%d\n", GWDCC0 + ((q + board_config.rx_queue_offset) * 4),
	       balr | dqt | ede | ets, q + board_config.rx_queue_offset);
#endif
	iowrite32(balr | dqt | ets | ede, gwca_addr + GWDCC0 + (q + board_config.rx_queue_offset) * 4);

	return 0;
}

static int rswitch2_rx_ring_init(int q)
{
	struct sk_buff *skb;
	int ring_size;
	int i;

	rx_skb[q] = kcalloc(num_rx_ring[q], sizeof(*rx_skb[q]), GFP_KERNEL);
	if (!rx_skb[q])
		goto error;

	for (i = 0; i < num_rx_ring[q]; i++) {
		skb = dev_alloc_skb(PKT_BUF_SZ + RSWITCH2_ALIGN - 1);
		if (!skb)
			goto error;
		skb_reserve(skb, NET_IP_ALIGN);
		rx_skb[q][i] = skb;
	}
	ring_size = sizeof(struct rswitch2_ext_ts_desc) * (num_rx_ring[q] + 1);
	rx_ring[q] = dma_alloc_coherent(&pcidev->dev, ring_size,
					&rx_desc_dma[q], GFP_KERNEL);
	if (!rx_ring[q])
		goto error;
	dirty_rx[q] = 0;

	return 0;
error:
	return -ENOMEM;
}

static int rswitch2_rxdmac_init(void)
{
	int error;

	error = rswitch2_rx_ring_init(RSWITCH2_BE + INTERNAL_GW);
	if (error < 0) {
		pr_err("RX Ring Init Failed\n");
		return error;
	}
	error = rswitch2_rx_ring_init(RSWITCH2_NC + INTERNAL_GW);
	if (error < 0) {
		pr_err("RX Ring Init Failed\n");
		return error;
	}
	/* Descriptor format */
	error = rswitch2_rx_ring_format(RSWITCH2_BE + INTERNAL_GW);
	if (error < 0) {
		pr_err("RX Ring Format Failed\n");
		return error;
	}
	error = rswitch2_rx_ring_format(RSWITCH2_NC + INTERNAL_GW);
	if (error < 0) {
		pr_err("RX Ring Format Failed\n");
		return error;
	}

	return 0;
}

static int rswitch2_gw_rxdmac_init(void)
{
	int error;

	error = rswitch2_rx_ring_init(RSWITCH2_RX_GW_QUEUE_NUM);
	if (error < 0) {
		pr_err("RX Ring Init Failed\n");
		return error;
	}

	/* Descriptor format */
	error = rswitch2_rx_ring_format(RSWITCH2_RX_GW_QUEUE_NUM);
	if (error < 0) {
		pr_err("RX Ring Format Failed\n");
		return error;
	}

	return 0;
}

static int rswitch2_probe_registerinterrupts(void)
{
	int r;
	int ret = 0;

	for (r = 0; r < ARRAY_SIZE(rswitch2_eth_irq); r++) {
		ret = request_irq(rswitch2_eth_irq[r], rswitch2_eth_interrupt,
				  IRQF_TRIGGER_RISING, RSWITCH2_FPGA_ETH_PLATFORM,
				  rswitch2_eth_irq_data);

		if (ret != 0) {
			pr_err("[RSWITCH2]  Probe FAILED. Cannot assign IRQ(%d) for Line%u. ret=%d\n",
			rswitch2_eth_irq[r], r, ret);
			return ret;
		}
	}

	return ret;
}

static int rswitch2_drv_probe_getinterrupts(struct platform_device *pdev)
{
	int ret = 0;
	int r;
	int irqNumber = 0;

	for (r = 0; r < ARRAY_SIZE(rswitch2_eth_irq_data); r++) {
		rswitch2_eth_irq_data[r] = r;
		rswitch2_eth_irq[r] = 0;
	}

	for (r = 0; r < ARRAY_SIZE(rswitch2_eth_irq); r++) {
		gpio_request(RSWITCH2_INT_LINE_GPIO + r, "tsn_irq_lines");
		gpio_direction_input(RSWITCH2_INT_LINE_GPIO + r);
		gpio_set_debounce(RSWITCH2_INT_LINE_GPIO + r, 50);
		gpio_export(RSWITCH2_INT_LINE_GPIO + r, true);
		irqNumber = gpio_to_irq(RSWITCH2_INT_LINE_GPIO + r);

		if (irqNumber < 0) {
			dev_err(&pdev->dev, "[RSWITCH2] Failed to get IRQ (%u)\n", r);
			return -EINVAL;
		}

		rswitch2_eth_irq[r] = irqNumber;
	}

#ifdef DEBUG
	pr_info("Interrupt probed\n");
#endif

	return ret;
}

extern enum rswitch2_portstate port_querystate(struct net_device *ndev)
{
	struct port_private *mdp = netdev_priv(ndev);

	return mdp->portstate;
}

extern enum rswitch2_gwca_mode gwca_querystate(void)
{
	return ioread32(gwca_addr + GWMS);
}

/* Get all ports CBS & Routing */
static long ioctl_getfportconfig(struct file *file, unsigned long arg)
{
	char __user *buf = (char __user *)arg;
	int err = 0;

	err = copy_to_user(buf, &config, sizeof(config));
	if (err < 0)
		return -EFAULT;

	return 0;
}

int config_vlan_tagging_gwca(struct rswitch2_eth_vlan_config *config)
{
	u32 gwvcc = 0;
	u32 gwvtc = 0;
	u32 gwttfc = 0;

	gwvcc = (config->egress_vlan_mode << 16) | (config->ingress_vlan_mode << 0);
	gwvtc = (config->vlan_tag_config.ctag_vlan << 0) |
		(config->vlan_tag_config.ctag_pcp << 12) |
		(config->vlan_tag_config.ctag_dei << 15) |
		(config->vlan_tag_config.stag_vlan << 16) |
		(config->vlan_tag_config.stag_pcp << 28) |
		(config->vlan_tag_config.stag_dei << 31);
	gwttfc = (config->vlan_filtering.unknown_tag << 8) |
		 (config->vlan_filtering.scr_tag << 7) |
		 (config->vlan_filtering.sc_tag << 6) |
		 (config->vlan_filtering.cr_tag << 5) |
		 (config->vlan_filtering.c_tag << 4) |
		 (config->vlan_filtering.cosr_tag << 3) |
		 (config->vlan_filtering.cos_tag << 2) |
		 (config->vlan_filtering.r_tag << 1) |
		 (config->vlan_filtering.no_tag << 0);
	iowrite32(gwvcc, gwca_addr + GWVCC);
	iowrite32(gwvtc, gwca_addr + GWVTC);
	iowrite32(gwttfc, gwca_addr + GWTTFC);

	return 0;
}

int config_vlan_tagging(struct net_device *ndev, struct rswitch2_eth_vlan_config *config)
{
	u32 eavcc = 0;
	u32 eavtc = 0;
	u32 eattfc = 0;

	eavcc = (config->egress_vlan_mode << 16) | (config->ingress_vlan_mode << 0);
	eavtc = (config->vlan_tag_config.ctag_vlan << 0) |
		(config->vlan_tag_config.ctag_pcp << 12) |
		(config->vlan_tag_config.ctag_dei << 15) |
		(config->vlan_tag_config.stag_vlan << 16) |
		(config->vlan_tag_config.stag_pcp << 28) |
		(config->vlan_tag_config.stag_dei << 31);
	eattfc = (config->vlan_filtering.unknown_tag << 8) |
		 (config->vlan_filtering.scr_tag << 7) |
		 (config->vlan_filtering.sc_tag << 6) |
		 (config->vlan_filtering.cr_tag << 5) |
		 (config->vlan_filtering.c_tag << 4) |
		 (config->vlan_filtering.cosr_tag << 3) |
		 (config->vlan_filtering.cos_tag << 2) |
		 (config->vlan_filtering.r_tag << 1) |
		 (config->vlan_filtering.no_tag << 0);
	portreg_write(ndev, eavcc, EAVCC);
	portreg_write(ndev, eavtc, EAVTC);
	portreg_write(ndev, eattfc, EATTFC);

	return 0;
}

/**
    @brief  Configure TX Queues Max frame size

    @param  ndev    net_device for the port device

    @param  txparam

    @return < 0, or 0 on success
*/
static int config_tx_queue_max_frm_sz(struct net_device * ndev, struct rswitch2_config_port_tx* txparam)
{
	u32 queue = 0;
	for (queue = 0; queue < txparam->txqueues; queue++) {
		if (txparam->txqueue[queue].max_frame_sz >= 0) {
			portreg_write(ndev,txparam->txqueue[queue].max_frame_sz, (EATMFSC0 + (4 * txparam->txqueue[queue].queuenumber)));
		}
	}
	return 0;
}


/**
    @brief  Configure TX Queues

    @param  ndev    net_device for the port device

    @param  txparam

    @param  PminSize

    @return < 0, or 0 on success
*/
static int config_tx_queues(struct net_device *ndev, struct rswitch2_config_port_tx *txparam,
							u32 pminsize)
{
	u32 queue = 0;
	u32 eatpec = 0;
	eatpec = portreg_read(ndev, EATPEC);
	for (queue = 0; queue < txparam->txqueues; queue++) {
		if (txparam->txqueue[queue].preempt_mac) {
			eatpec = eatpec | (1 << txparam->txqueue[queue].queuenumber);
		}  else {
			eatpec &= ~(1UL << txparam->txqueue[queue].queuenumber);
		}
	}
	eatpec = eatpec | (pminsize << 16);
	portreg_write(ndev,eatpec, EATPEC);
	return 0;
}


/**
    @brief  Configure CBS

    @param  ndev    net_device for the port device

    @param  TxParam

    @return < 0, or 0 on success

*/
static int config_tx_streams(struct net_device *ndev, struct  rswitch2_config_port_tx *txparam)
{
	u32 eacaec  = 0;
	u32 civ = 0;
	u32 streams = 0;
	u32 cul  = 0;
	portreg_write(ndev, eacaec, EACAEC);
	for(streams = 0; streams < txparam->txstreams; streams++) {
		/* Set CIV */
		civ = (txparam->txstream[streams].civman << 16) | (txparam->txstream[streams].civexp);
		portreg_write(ndev, civ , (EACAIVC0 + (4 * txparam->txstream[streams].queuenum)));
		cul = txparam->txstream[streams].cul;
		portreg_write(ndev, cul , (EACAULC0 + ( 4 * txparam->txstream[streams].queuenum)));
	 	/* Set EACAEC */
		eacaec =  portreg_read(ndev, EACAEC);
		eacaec |= (1 << txparam->txstream[streams].queuenum);
		portreg_write(ndev, eacaec, EACAEC);
	}
	portreg_write(ndev, 0xFF , EACC);
	return 0;
}


/**
    @brief  Set port Configuration

    @param[in]  file
    @param      arg

    @return     < 0 on error
*/
static long ioctl_setportconfig(struct file *file, unsigned long arg)
{
	char __user *buf = (char __user *)arg;
	int a = 0;
	int err = 0;
	enum rswitch2_portstate estate;

#ifdef DEBUG
	pr_info("[RSWITCH2] %s\n", __func__);
#endif

	err = copy_from_user(&config_new, buf, sizeof(config_new));

	if (err != 0) {
		pr_err("[RSWITCH2] %s. copy_from_user returned %d for RSWITCH2_SW_SET_CONFIG\n",
		       __func__, err);
		return err;
	}

	/* Validate new configuration first */
	if (config_new.ports > board_config.agents) {
		pr_err("[RSWITCH2] RSWITCH2_SET_CONFIG ERROR 1: Too many Ports (%u) Max(%u)\n",
		       config_new.ports, board_config.agents);
		return -EINVAL;
	}
	memcpy(&config, &config_new, sizeof(config));
	for (a = 0; a < config_new.ports; a++) {
		struct net_device *ndev;

		if (!config_new.eth_port_config[a].cpu)
			ndev = ppndev[config_new.eth_port_config[a].port_number];
		if (config_new.eth_port_config[a].port_number > board_config.agents) {
			pr_err("[RSWITCH2] RSWITCH2_SET_CONFIG ERROR 2: Port %u. Port Number invalid (%u)\n",
			       a, config_new.eth_port_config[a].port_number);
			return -EINVAL;
		}

		if (config_new.eth_port_config[a].cpu) {
			if (config_new.eth_port_config[a].eth_vlan_tag_config.bEnable) {
				rswitch2_gwca_change_mode(rswitch2_gwca_mode_disable);
				rswitch2_gwca_change_mode(rswitch2_gwca_mode_config);
				if (gwca_querystate() == rswitch2_gwca_mode_config) {
					err = config_vlan_tagging_gwca(&config_new.eth_port_config[a].eth_vlan_tag_config);
					if (err < 0)
						pr_err("VLAN Tagging Configuration Failed\n");
				}
				rswitch2_gwca_change_mode(rswitch2_gwca_mode_operation);
			}
		} else {
			estate = port_querystate(ndev);
			if (estate == rswitch2_portstate_config) {
				if (config_new.eth_port_config[a].eth_vlan_tag_config.bEnable) {
					err = config_vlan_tagging(ndev, &config_new.eth_port_config[a].eth_vlan_tag_config);
					if (err < 0)
						pr_err("VLAN Tagging Configuration Failed\n");
				}
				config_tx_queues(ndev, &config_new.eth_port_config[a].txparam,config_new.eth_port_config[a].pminsize);
			} else {
				if (config_new.eth_port_config[a].eth_vlan_tag_config.bEnable) {
					pr_warn("Port %s in operation mode, Cannot configure VLAN \n", ndev->name);
				}
				if (config_new.eth_port_config[a].txparam.txqueues) {
					port_changestate(ndev, rswitch2_portstate_config);
					config_tx_queues(ndev, &config_new.eth_port_config[a].txparam,config_new.eth_port_config[a].pminsize);
					port_changestate(ndev, rswitch2_portstate_operate);
				}
			}
			config_tx_streams(ndev,&config_new.eth_port_config[a].txparam);
			config_tx_queue_max_frm_sz(ndev,&config_new.eth_port_config[a].txparam);
		}
	}

	return err;
}

static long drv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case RSWITCH2_SET_CONFIG:
		return ioctl_setportconfig(file, arg);

	case RSWITCH2_GET_CONFIG:
		return ioctl_getfportconfig(file, arg);

	default:
		pr_err("[RSWITCH2] File IOCTL Unknown: 0x%08X\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

/* Character Driver File Operation */
const struct file_operations drv_fileops = {
	.owner            = THIS_MODULE,
	.unlocked_ioctl   = drv_ioctl,
	.compat_ioctl     = drv_ioctl,
};

static int drv_probe_create_chrdev(void)
{
	struct device *dev;
	int ret = 0;

	/* Create class */
	rswitchtsn_devclass = class_create(THIS_MODULE, RSWITCH2_ETH_CLASS);
	if (IS_ERR(rswitchtsn_devclass)) {
		ret = PTR_ERR(rswitchtsn_devclass);
		pr_err("[RSWITCH2] failed to create '%s' class. rc=%d\n",
		       RSWITCH2_ETH_CLASS, ret);
		return ret;
	}

	if (rswitchtsn_devmajor != 0) {
		rswitchtsn_dev = MKDEV(rswitchtsn_devmajor, 0);
		ret = register_chrdev_region(rswitchtsn_dev, 1, RSWITCH2_ETH_CLASS);
	} else {
		ret = alloc_chrdev_region(&rswitchtsn_dev, 0, 1, RSWITCH2_ETH_CLASS);
	}
	if (ret < 0) {
		pr_err("[RSWITCH2] failed to register '%s' character device. rc=%d\n",
		       RSWITCH2_ETH_CLASS, ret);
		class_destroy(rswitchtsn_devclass);
		return ret;
	}
	rswitchtsn_devmajor = MAJOR(rswitchtsn_dev);

	cdev_init(&rswitchtsn_cdev, &drv_fileops);
	rswitchtsn_cdev.owner = THIS_MODULE;

	ret = cdev_add(&rswitchtsn_cdev, rswitchtsn_dev, RSWITCH2_ETH_CTRL_MINOR + 1);
	if (ret < 0) {
		pr_err("[RSWITCH2] failed to add '%s' character device. rc=%d\n",
		       RSWITCH2_ETH_CLASS, ret);
		unregister_chrdev_region(rswitchtsn_dev, 1);
		class_destroy(rswitchtsn_devclass);
		return ret;
	}

	/* device initialize */
	dev = &rswitchtsn_device;
	device_initialize(dev);
	dev->class = rswitchtsn_devclass;
	dev->devt = MKDEV(rswitchtsn_devmajor, RSWITCH2_ETH_CTRL_MINOR);
	dev_set_name(dev, RSWITCH2_ETHERNET_DEVICE_NAME);

	ret = device_add(dev);
	if (ret < 0) {
		pr_err("[RSWITCH2] failed to add '%s' device. rc=%d\n",
		       RSWITCH2_ETHERNET_DEVICE_NAME, ret);
		cdev_del(&rswitchtsn_cdev);
		unregister_chrdev_region(rswitchtsn_dev, 1);
		class_destroy(rswitchtsn_devclass);
		return ret;
	}

	return ret;
}

static int phydev_disable_bcast_seterrata(struct net_device *ndev, u32 speed, u32 master)
{
	static u32 count;
	u32 read_val = 0;
	u32 index = 0;
	struct port_private *mdp = netdev_priv(ndev);

	index = mdp->portindex;
	if (board_variant == 0x03) {
		if ((index > 0) && (index < 5)) {
			if (count == 0x0) {
				count++;
				mdiobus_write(ndev->phydev->mdio.bus, 0, 0x18, 0x90);
			}
			phy_write(ndev->phydev, 0x1b, 0xDD00);
			phy_write(ndev->phydev, 0x1c, 0x0020);
			msleep(20);
			mdiobus_write(ndev->phydev->mdio.bus, 0, 0x18, 0x90);
			/* Read Register 0x02 0x03 to check phy ids and then
			 * for checking phy is operational.
			 */
			read_val = phy_read(ndev->phydev, 0x02);
			if (read_val != 0x001C) {
				pr_err("Incorrect Phy ID 1= %x expected = 0x001C\n", read_val);
				return -1;
			}
			read_val = phy_read(ndev->phydev, 0x03);
			if (read_val != 0xC800) {
				pr_err("Incorrect Phy ID 2= %x expected = 0xC800\n", read_val);
				return -1;
			}
			phy_write(ndev->phydev, 0x1F, 0x0A42);
			read_val = phy_read(ndev->phydev, 0x10);
			if ((read_val & 0x03) != 0x03) {
				pr_err("Phy is not active register 0x10 Page 0xA42 =  %x\n", read_val);
				return -1;
			}
			if (speed == 100) {
				/* Write Init Patch */
				phy_write(ndev->phydev, 0x1b, 0xBC40);
				phy_write(ndev->phydev, 0x1c, 0x0FDA);
				phy_write(ndev->phydev, 0x1b, 0xBCC4);
				phy_write(ndev->phydev, 0x1c, 0x8309);
				phy_write(ndev->phydev, 0x1b, 0xBCC6);
				phy_write(ndev->phydev, 0x1c, 0x120B);
				phy_write(ndev->phydev, 0x1b, 0xBCC8);
				phy_write(ndev->phydev, 0x1c, 0x0005);
				phy_write(ndev->phydev, 0x1b, 0xBC40);
				phy_write(ndev->phydev, 0x1c, 0x0FDA);
				phy_write(ndev->phydev, 0x1b, 0xAC16);
				phy_write(ndev->phydev, 0x1c, 0x0000);
				phy_write(ndev->phydev, 0x1b, 0xAC1A);
				phy_write(ndev->phydev, 0x1c, 0x0284);
				phy_write(ndev->phydev, 0x1b, 0x823C);
				phy_write(ndev->phydev, 0x1c, 0x4022);
				phy_write(ndev->phydev, 0x1b, 0x823E);
				phy_write(ndev->phydev, 0x1c, 0x007C);
				phy_write(ndev->phydev, 0x1b, 0x8266);
				phy_write(ndev->phydev, 0x1c, 0x4022);
				phy_write(ndev->phydev, 0x1b, 0x8268);
				phy_write(ndev->phydev, 0x1c, 0x007C);
				phy_write(ndev->phydev, 0x1b, 0x8295);
				phy_write(ndev->phydev, 0x1c, 0x3406);
				phy_write(ndev->phydev, 0x1b, 0x82BF);
				phy_write(ndev->phydev, 0x1c, 0x3406);
				phy_write(ndev->phydev, 0x1b, 0x8205);
				phy_write(ndev->phydev, 0x1c, 0x0110);
				phy_write(ndev->phydev, 0x1b, 0x8207);
				phy_write(ndev->phydev, 0x1c, 0x0125);
				phy_write(ndev->phydev, 0x1b, 0x8209);
				phy_write(ndev->phydev, 0x1c, 0x0125);
				phy_write(ndev->phydev, 0x1b, 0x820B);
				phy_write(ndev->phydev, 0x1c, 0x0125);
				phy_write(ndev->phydev, 0x1b, 0x820D);
				phy_write(ndev->phydev, 0x1c, 0x0110);
				phy_write(ndev->phydev, 0x1b, 0x820F);
				phy_write(ndev->phydev, 0x1c, 0x0120);
				phy_write(ndev->phydev, 0x1b, 0x8211);
				phy_write(ndev->phydev, 0x1c, 0x0125);
				phy_write(ndev->phydev, 0x1b, 0x8213);
				phy_write(ndev->phydev, 0x1c, 0x0130);
				phy_write(ndev->phydev, 0x1b, 0x8296);
				phy_write(ndev->phydev, 0x1c, 0x02A5);
				phy_write(ndev->phydev, 0x1b, 0x82C0);
				phy_write(ndev->phydev, 0x1c, 0x0200);
				phy_write(ndev->phydev, 0x1b, 0xC018);
				phy_write(ndev->phydev, 0x1c, 0x0108);
				phy_write(ndev->phydev, 0x1b, 0xC014);
				phy_write(ndev->phydev, 0x1c, 0x0109);
				phy_write(ndev->phydev, 0x1b, 0xC026);
				phy_write(ndev->phydev, 0x1c, 0x075E);
				phy_write(ndev->phydev, 0x1b, 0xC028);
				phy_write(ndev->phydev, 0x1c, 0x0534);
			} else if (speed == 1000) {
				phy_write(ndev->phydev, 0x1b, 0xBC40);
				phy_write(ndev->phydev, 0x1c, 0x0FDA);
				phy_write(ndev->phydev, 0x1b, 0xBCC4);
				phy_write(ndev->phydev, 0x1c, 0x8307);
				phy_write(ndev->phydev, 0x1b, 0xBCC6);
				phy_write(ndev->phydev, 0x1c, 0x120B);
				phy_write(ndev->phydev, 0x1b, 0xBCC8);
				phy_write(ndev->phydev, 0x1c, 0x0005);
				phy_write(ndev->phydev, 0x1b, 0xBC40);
				phy_write(ndev->phydev, 0x1c, 0x0FDA);
				phy_write(ndev->phydev, 0x1b, 0xAC16);
				phy_write(ndev->phydev, 0x1c, 0x0000);
				phy_write(ndev->phydev, 0x1b, 0xAC1A);
				phy_write(ndev->phydev, 0x1c, 0x0284);
				phy_write(ndev->phydev, 0x1b, 0x823C);
				phy_write(ndev->phydev, 0x1c, 0x4022);
				phy_write(ndev->phydev, 0x1b, 0x823E);
				phy_write(ndev->phydev, 0x1c, 0x007C);
				phy_write(ndev->phydev, 0x1b, 0x8266);
				phy_write(ndev->phydev, 0x1c, 0x4022);
				phy_write(ndev->phydev, 0x1b, 0x8268);
				phy_write(ndev->phydev, 0x1c, 0x007C);
				phy_write(ndev->phydev, 0x1b, 0x824B);
				phy_write(ndev->phydev, 0x1c, 0xDA1C);
				phy_write(ndev->phydev, 0x1b, 0x8253);
				phy_write(ndev->phydev, 0x1c, 0x1C00);
				phy_write(ndev->phydev, 0x1b, 0x81E9);
				phy_write(ndev->phydev, 0x1c, 0x00E0);
				phy_write(ndev->phydev, 0x1b, 0x81EB);
				phy_write(ndev->phydev, 0x1c, 0x00FF);
				phy_write(ndev->phydev, 0x1b, 0x81F1);
				phy_write(ndev->phydev, 0x1c, 0x00DA);
				phy_write(ndev->phydev, 0x1b, 0x81F5);
				phy_write(ndev->phydev, 0x1c, 0x0105);
				phy_write(ndev->phydev, 0x1b, 0x824D);
				phy_write(ndev->phydev, 0x1c, 0x2F19);
				phy_write(ndev->phydev, 0x1b, 0x8255);
				phy_write(ndev->phydev, 0x1c, 0x2000);
				phy_write(ndev->phydev, 0x1b, 0x8249);
				phy_write(ndev->phydev, 0x1c, 0xEE09);
				phy_write(ndev->phydev, 0x1b, 0x8251);
				phy_write(ndev->phydev, 0x1c, 0x2214);
				phy_write(ndev->phydev, 0x1b, 0x8245);
				phy_write(ndev->phydev, 0x1c, 0x000D);
				phy_write(ndev->phydev, 0x1b, 0x8247);
				phy_write(ndev->phydev, 0x1c, 0x1606);
				phy_write(ndev->phydev, 0x1b, 0x821B);
				phy_write(ndev->phydev, 0x1c, 0x000D);
				phy_write(ndev->phydev, 0x1b, 0x821D);
				phy_write(ndev->phydev, 0x1c, 0x1606);
				phy_write(ndev->phydev, 0x1b, 0x8242);
				phy_write(ndev->phydev, 0x1c, 0x0A00);
				phy_write(ndev->phydev, 0x1b, 0x826C);
				phy_write(ndev->phydev, 0x1c, 0x0AA5);
				phy_write(ndev->phydev, 0x1b, 0xAC1E);
				phy_write(ndev->phydev, 0x1c, 0x0003);
				phy_write(ndev->phydev, 0x1b, 0xC018);
				phy_write(ndev->phydev, 0x1c, 0x0108);
				phy_write(ndev->phydev, 0x1b, 0xC014);
				phy_write(ndev->phydev, 0x1c, 0x0109);
				phy_write(ndev->phydev, 0x1b, 0xC026);
				phy_write(ndev->phydev, 0x1c, 0x075E);
				phy_write(ndev->phydev, 0x1b, 0xC028);
				phy_write(ndev->phydev, 0x1c, 0x0534);
			}

			/* Step3 Optional application specific configurations */

			/* RGMII timing correction for Rx */
			phy_write(ndev->phydev, 0x1b, 0xd04a);
			phy_write(ndev->phydev, 0x1c, 0x0007);

			/* RGMII timing correction for Tx */
			phy_write(ndev->phydev, 0x1b, 0xd084);
			phy_write(ndev->phydev, 0x1c, 0x0000);

			phy_write(ndev->phydev, 0x1b, 0xd084);
			if (ndev->phydev->phy_id == 0x02) //Id of the muxed interface
				phy_write(ndev->phydev, 0x1c, 0xC000);
			else
				phy_write(ndev->phydev, 0x1c, 0x4000);

			phy_write(ndev->phydev, 0x1b, 0xd084);
			if (ndev->phydev->phy_id == 0x02)
				phy_write(ndev->phydev, 0x1c, 0xC007);
			else
				phy_write(ndev->phydev, 0x1c, 0x4007);

			/* RGMII Typical 1.8V 5-25cm line */
			phy_write(ndev->phydev, 0x1b, 0xd414);
			phy_write(ndev->phydev, 0x1c, 0x1211);
			phy_write(ndev->phydev, 0x1b, 0xd416);
			phy_write(ndev->phydev, 0x1c, 0x1111);
			phy_write(ndev->phydev, 0x1b, 0xd418);
			phy_write(ndev->phydev, 0x1c, 0x1200);
			phy_write(ndev->phydev, 0x1b, 0xd41a);
			phy_write(ndev->phydev, 0x1c, 0x1100);
			phy_write(ndev->phydev, 0x1b, 0xd42e);
			phy_write(ndev->phydev, 0x1c, 0x8082);

			/* SW reset */
			phy_write(ndev->phydev, 0, 0x8000);
			msleep(20);
			read_val = phy_read(ndev->phydev, 0);
			if (read_val != 0x0140) {
				pr_err("Incorrect BMCR %x expected 0x0140\n", read_val);
				return -1;
			}
			/* bring PHY to 100 Mbps mode */
			if (speed == 1000) {
				//pr_info("Writing phy 1000mbps\n");
				phy_write(ndev->phydev, 0, 0x0140);
			} else if (speed == 100) {
				/* bring PHY to 100 Mbps mode with aneg */
				//pr_info("Writing phy 100mbps\n");
				phy_write(ndev->phydev, 0, 0x2100);
			}
			if (master)
				phy_write(ndev->phydev, 9, 0x800);
			else
				phy_write(ndev->phydev, 9, 0x000);
		}
	} else {
		phy_write(ndev->phydev, 0x31, 0xa43);
		phy_read(ndev->phydev, 0x18);
		phy_write(ndev->phydev, 0x18, 0x98);
		phy_read(ndev->phydev, 0x18);
		phy_write(ndev->phydev, 0x31, 0xa42);
	}

	return 0;
}

/* Unmap, mapped physical addresses */
static void drv_unload_unmap(void)
{
	int a;

	for (a = 0; a < ARRAY_SIZE(ethport_addr); a++)
		ethport_addr[a] = NULL;
}

void rswitch2_clock_disable(void)
{
	iowrite32(0xFFFFFFFF, ioaddr + RCDC);
}

static void drv_unload_cleanup(void)
{
	rswitch2_gwca_change_mode(rswitch2_gwca_mode_disable);
	kfree(ppndev);
	ppndev = NULL;
	rswitch2_clock_disable();
	rswitch2_reset();
	drv_unload_unmap();
}

static int rswitch2_tx_ring_free(struct net_device *ndev, int q)
{
	struct port_private *priv = netdev_priv(ndev);

	if (priv->tx_ring[q]) {
		rswitch2_tx_free(ndev, q, false);
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

static int rswitch2_rx_ring_free(int q)
{
	int i = 0;
	int ring_size = 0;

	if (rx_ring[q]) {
		for (i = 0; i < num_rx_ring[q] - 1; i++) {
			struct rswitch2_ext_ts_desc *desc = &rx_ring[q][i];

			if (!dma_mapping_error(&pcidev->dev,
					       le32_to_cpu(desc->dptrl)))
				dma_unmap_single(&pcidev->dev,
						 le32_to_cpu(desc->dptrl),
						 PKT_BUF_SZ,
						 DMA_FROM_DEVICE);
		}

		ring_size = sizeof(struct rswitch2_ext_ts_desc) *
			    num_rx_ring[q];
		dma_free_coherent(&pcidev->dev, ring_size, rx_ring[q],
				  rx_desc_dma[q]);
		rx_ring[q] = NULL;
	}

	/* Free RX skb ringbuffer */
	if (rx_skb[q]) {
		for (i = 0; i < num_rx_ring[q] - 1; i++)
			dev_kfree_skb(rx_skb[q][i]);
	}

	kfree(rx_skb[q]);

	rx_skb[q] = NULL;

	return 0;
}

static int drv_unload_remove_netdevs(void)
{
	struct net_device *ndev = NULL;
	struct port_private *mdp = NULL;
	int a;
	int err = 0;

	for (a = 0; a <= board_config.eth_ports; a++) {
		if (board_variant == 3) {
			if (((a < 4) && (a > 0)) ||
			    (a == board_config.eth_ports) ||
			    ((a == 0) && (nonetdev != 1))) {
				ndev = ppndev[a];
				if (ndev == NULL)
					continue;
				if (ndev->phydev) {
					phydev_disable_bcast_seterrata(ndev, 100, 1);
					phy_disconnect(ndev->phydev);
				}
				if (a != board_config.eth_ports)
					rswitch2_tx_ring_free(ppndev[a], RSWITCH2_NC);
				rswitch2_tx_ring_free(ppndev[a], RSWITCH2_BE);
				mdp = netdev_priv(ndev);
				if (mdp == NULL)
					continue;

				unregister_netdev(ndev);
				if (a != board_config.eth_ports)
					netif_napi_del(&mdp->napi[1]);

				netif_napi_del(&mdp->napi[0]);
				if (a != board_config.eth_ports) {
					err = port_changestate(ndev, rswitch2_portstate_disable);
					if (err < 0) {
						pr_err("Port Change State to Disable Failed\n");
						return err;
					}
				}
				free_netdev(ndev);
				ppndev[a] = NULL;
			}
		} else {
			ndev = ppndev[a];
			if (ndev == NULL)
				continue;

			if (ndev->phydev) {
				if (ndev->phydev->state == PHY_UP)
					phy_stop(ndev->phydev);
				phy_disconnect(ndev->phydev);
			}
			if (a != board_config.eth_ports)
				rswitch2_tx_ring_free(ppndev[a], RSWITCH2_NC);

			rswitch2_tx_ring_free(ppndev[a], RSWITCH2_BE);
			mdp = netdev_priv(ndev);
			if (mdp == NULL)
				continue;

			unregister_netdev(ndev);
			if (a != board_config.eth_ports)
				netif_napi_del(&mdp->napi[1]);

			netif_napi_del(&mdp->napi[0]);
			err = port_changestate(ndev, rswitch2_portstate_disable);
			if (err < 0) {
				pr_err("Port Change State to Disable Failed\n");
				return err;
			}
			free_netdev(ndev);
			ppndev[a] = NULL;
		}
	}
	rswitch2_rx_ring_free(0);
	rswitch2_rx_ring_free(1);
#ifdef INTERNAL_GW
	/* Add RX Ring Format for GW Port */
	rswitch2_rx_ring_free(2);
#endif
	rswitch2_ts_ring_free();

	dma_free_coherent(&pcidev->dev, desc_bat_size, desc_bat, desc_bat_dma);
	dma_free_coherent(&pcidev->dev, ts_desc_bat_size, ts_desc_bat,
			  ts_desc_bat_dma);

	return err;
}

#ifdef CONFIG_RENESAS_RSWITCH2_STATS
static int rswitch2_eth_driver_show(struct seq_file *m, void *v)
{
	seq_printf(m, "S/W Version: %s\n", RSWITCH2_DRIVER_VERSION);
#ifdef DEBUG
	seq_printf(m, "Debug Level: %d\n", DEBUG);
#else
	seq_puts(m, "Debug: no\n");
#endif

	return 0;
}

static void proc_print_u64(struct seq_file *m, const char *title,
			   const char *valueFmt, u64 values[], int cnt)
{
	u32 port;

	seq_printf(m, "%s", title);
	for (port = 0; port < cnt; port++)
		seq_printf(m, valueFmt, values[port]);

	seq_puts(m, "\n");
}


static void proc_print_u32(struct seq_file *m, const char *title,
			   const char *valueFmt, u32 values[], int cnt,
			   const char *comment)
{
	u32 port;

	seq_printf(m, "%s", title);
	for (port = 0; port < cnt; port++)
		seq_printf(m, valueFmt, values[port]);

	seq_printf(m, "%s\n", comment);
}

/**
    @brief Helper function to print a table of values

    @param  seq_file *
    @param  const char *
    @param  const char *
    @param  const char * []
    @param  int

    @return void
*/
static void proc_print_str(struct seq_file *m, const char *title,
			   const char *valueFmt, const char *values[], int cnt)
{
	u32 port;

	seq_printf(m, "%s", title);
	for (port = 0; port < cnt; port++)
		seq_printf(m, valueFmt, values[port]);

	seq_puts(m, "\n");
}

static void rswitch2_rx_tx_counter_clear_func(void)
{
	memset(&counter.rx_tx_counter, 0,
	       sizeof(struct rswitch2_proc_rx_tx_counter));
}

static void rswitch2_error_counter_clear_func(void)
{
	memset(&counter.err_counter, 0,
	       sizeof(struct rswitch2_proc_eth_err_counter));
}

static void rswitch2_counter_clear_func(void)
{
	rswitch2_rx_tx_counter_clear_func();
	rswitch2_error_counter_clear_func();
}

static int rswitch2_eth_rxtx_show(struct seq_file *m, int longReport)
{
	struct net_device *ndev;
	u32 port;
	const char *portName[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	struct rswitch2_proc_rx_tx_counter *rxtx = &counter.rx_tx_counter;

	for (port = 0; port < board_config.eth_ports; port++) {
		ndev = ppndev[port];

		portName[port] = ndev->name;

		rxtx->eframe[port] += portreg_read(ndev, MRGFCE);
		rxtx->pframe[port] += portreg_read(ndev, MRGFCP);
		rxtx->ebyte[port] += (portreg_read(ndev, MRXBCEU) << 32) |
				     portreg_read(ndev, MRXBCEL);
		rxtx->pbyte[port] += (portreg_read(ndev, MRXBCPU) << 32) |
				     portreg_read(ndev, MRXBCPL);
		rxtx->bframe[port] += portreg_read(ndev, MRBFC);
		rxtx->mframe[port] += portreg_read(ndev, MRMFC);
		rxtx->uframe[port] += portreg_read(ndev, MRUFC);
		rxtx->phyerror[port] += portreg_read(ndev, MRPEFC);
		rxtx->nibbleerror[port] += portreg_read(ndev, MRNEFC);
		rxtx->crcerror[port] += portreg_read(ndev, MRFMEFC);
		rxtx->fragmisserror[port] += portreg_read(ndev, MRFFMEFC);
		rxtx->cfragerror[port] += portreg_read(ndev, MRCFCEFC);
		rxtx->fragerror[port] += portreg_read(ndev, MRFCEFC);
		rxtx->rmacfiltererror[port] += portreg_read(ndev, MRRCFEFC);
		rxtx->undersizeerrorgood[port] += portreg_read(ndev, MRGUEFC);
		rxtx->undersizeerrorbad[port] += portreg_read(ndev, MRBUEFC);
		rxtx->oversizeerrorgood[port] += portreg_read(ndev, MRGOEFC);
		rxtx->oversizeerrorbad[port] += portreg_read(ndev, MRBOEFC);
		rxtx->txeframe[port] += portreg_read(ndev, MTGFCE);
		rxtx->txpframe[port] += portreg_read(ndev, MTGFCP);
		rxtx->txebyte[port] += (portreg_read(ndev, MTXBCEU) << 32) |
				       portreg_read(ndev, MTXBCEL);
		rxtx->txpbyte[port] += (portreg_read(ndev, MTXBCPU) << 32) |
				       portreg_read(ndev, MTXBCPL);
		rxtx->txerror[port] += portreg_read(ndev, MTEFC);
		rxtx->txbframe[port] += portreg_read(ndev, MTBFC);
		rxtx->txmframe[port] += portreg_read(ndev, MTMFC);
		rxtx->txuframe[port] += portreg_read(ndev, MTUFC);
	}
	//read the tsngw related ports (always at ethport +1)
	port = board_config.eth_ports;
	portName[port] = "CPU";
	rxtx->eframe[port] += ioread32(gwca_addr + GWTDCN);
	rxtx->txeframe[port] += ioread32(gwca_addr + GWRDCN);

	proc_print_str(m, "Interface      ", " %10s", portName,
		       board_config.eth_ports + 1);
	seq_puts(m,     "================");
	for (port = 0; port < board_config.eth_ports + 1; port++)
		seq_printf(m, "=%.10s", "====================================");
	seq_puts(m, "\n");
	proc_print_u32(m, "Rx (eMAC)      ", " %10d", rxtx->eframe,
		       board_config.eth_ports + 1, " from CPU");
	proc_print_u32(m, "Rx (pMAC)      ", " %10d", rxtx->pframe,
		       board_config.eth_ports, "");
	if (longReport) {
		proc_print_u32(m, "  Broadcast    ", " %10d", rxtx->bframe,
			       board_config.eth_ports, "");
		proc_print_u32(m, "  Multicast    ", " %10d", rxtx->mframe,
			       board_config.eth_ports, "");
		proc_print_u32(m, "  Unicast      ", " %10d", rxtx->uframe,
			       board_config.eth_ports, "");
	}
	proc_print_u32(m, "Rx CRC Error   ", " %10d", rxtx->crcerror,
		       board_config.eth_ports, "");
	proc_print_u32(m, "Rx Phy Error   ", " %10d", rxtx->phyerror,
		       board_config.eth_ports, "");
	proc_print_u32(m, "Rx RMAC Filter ", " %10d", rxtx->rmacfiltererror,
		       board_config.eth_ports, "");
	if (longReport) {
		proc_print_u32(m, "Nibble Error   ", " %10d",
			       rxtx->nibbleerror, board_config.eth_ports, "");
		proc_print_u32(m, "Undersize Good ", " %10d",
			       rxtx->undersizeerrorgood, board_config.eth_ports, "");
		proc_print_u32(m, "Undersize Bad  ", " %10d",
			       rxtx->undersizeerrorbad, board_config.eth_ports, "");
		proc_print_u32(m, "Oversize Good  ", " %10d",
			       rxtx->oversizeerrorgood, board_config.eth_ports, "");
		proc_print_u32(m, "Oversize Bad   ", " %10d",
			       rxtx->oversizeerrorbad, board_config.eth_ports, "");
		proc_print_u32(m, "Frag Miss      ", " %10d",
			       rxtx->fragmisserror, board_config.eth_ports, "");
		proc_print_u32(m, "C Frag Error   ", " %10d", rxtx->cfragerror,
			       board_config.eth_ports, "");
		proc_print_u32(m, "Frag Error     ", " %10d", rxtx->fragerror,
			       board_config.eth_ports, "");
		proc_print_u64(m, "Rx bytes (eMAC)", " %10d", rxtx->ebyte,
			       board_config.eth_ports);
		proc_print_u64(m, "Rx bytes (pMAC)", " %10d", rxtx->pbyte,
			       board_config.eth_ports);
		seq_puts(m, "\n");
	}
	proc_print_u32(m, "Tx (eMAC)      ", " %10d", rxtx->txeframe,
		       board_config.eth_ports + 1, " to CPU");
	proc_print_u32(m, "Tx (pMAC)      ", " %10d", rxtx->txpframe,
		       board_config.eth_ports, "");
	if (longReport) {
		proc_print_u32(m, "  Broadcast    ", " %10d", rxtx->txbframe,
			       board_config.eth_ports, "");
		proc_print_u32(m, "  Multicast    ", " %10d", rxtx->txmframe,
			       board_config.eth_ports, "");
		proc_print_u32(m, "  Unicast      ", " %10d", rxtx->txuframe,
			       board_config.eth_ports, "");
	}
	proc_print_u32(m, "Tx Errors      ", " %10d", rxtx->txerror,
		       board_config.eth_ports, "");
	if (longReport) {
		proc_print_u64(m, "Tx bytes (eMAC)", " %10d", rxtx->txebyte,
			       board_config.eth_ports);
		proc_print_u64(m, "Tx bytes (pMAC)", " %10d", rxtx->txpbyte,
			       board_config.eth_ports);
	}

	return 0;
}

static int rswitch2_eth_rxtx_all_show(struct seq_file *m, void *v)
{
	return rswitch2_eth_rxtx_show(m, 1);
}

static int rswitch2_eth_rxtx_short_show(struct seq_file *m, void *v)
{
	return rswitch2_eth_rxtx_show(m, 0);
}

/**
    @brief Function to show port monitor stats(CBS, preempt) in /proc either in short format

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_eth_port_monitor_show(struct seq_file *m, int longreport)
{
	u32 port = 0;
	u32 eatpec = 0;
	u32 lvs = 0;
	u32 cbs = 0;
	u32 queue = 0;
	u64 bw = 0;
	u32 bwbps = 0;
	u32 bwqm = 0;
	u32 bwrm = 0;
	u32 bwqg = 0;
	u32 bwqk = 0;
	u32 bwrk = 0;
	u64 civexp = 0;
	u64 civman = 0;
	char bwstr[30];
	char qstr[30];
	char hexstr[30];
	u32 civ = 0;
	u32 cul = 0;
	u32 mfs = 0;
	struct port_private   * mdp;
	struct net_device * ndev;
	strcpy(bwstr,"");
	strcpy(hexstr,"");
	strcpy(qstr,"");
	seq_printf(m, "%-16s","");
	for (port = 0; port < board_config.eth_ports; port++) {
		ndev = ppndev[port];
		seq_printf(m, "%-13s",ndev->name);
	}
	seq_printf(m,"\n");
	seq_printf(m, "%-18s","LocAddFragSize");
	for (port = 0; port < board_config.eth_ports; port++) {
		ndev = ppndev[port];
		eatpec = portreg_read(ndev,EATPEC);
		seq_printf(m, "%-13d",(eatpec >> 16) & 0x03);
	}
	seq_printf(m,"\n");
	seq_printf(m, "%-18s","RemPreemptSup");
	for (port = 0; port < board_config.eth_ports; port++) {
		ndev = ppndev[port];
		lvs = portreg_read(ndev,MMIS0);
		lvs =  (lvs >> 2) & 0x01;
		seq_printf(m, "%-13s",lvs?"P":"E");
	}
	seq_printf(m,"\n");
	seq_printf(m,"\n");
	for( queue = 0; queue < RENESAS_RSWITCH2_TX_QUEUES; queue++) {
		seq_printf(m, "%-2s","");
		strcpy(qstr,"");
		sprintf(qstr, "Q%d",queue);
		seq_printf(m, "%-8s",qstr);
		seq_printf(m, "%-8s","MAC");
		for (port = 0; port < board_config.eth_ports; port++) {
			ndev = ppndev[port];
			eatpec = portreg_read(ndev,EATPEC);
			seq_printf(m, "%-13s",((eatpec >> queue) & 0x01)?"P":"E");
		}
		seq_printf(m,"\n");
		seq_printf(m, "%-10s","");
		seq_printf(m, "%-6s","MFS");
		for (port = 0; port < board_config.eth_ports; port++) {
			ndev = ppndev[port];
			mfs = portreg_read(ndev,EATMFSC0 + (queue * 4));
			seq_printf(m, "%-13u",mfs);
		}
		seq_printf(m,"\n");
		seq_printf(m, "%-10s","");
		seq_printf(m, "%-6s","BW");
		for (port = 0; port < board_config.eth_ports; port++) {
			ndev = ppndev[port];
			mdp = netdev_priv(ndev);
			cbs = portreg_read(ndev,EACOEM);
			if ((ndev->phydev != NULL) && (mdp->link)) {
				strcpy(bwstr,"");
				if((cbs >> queue) & 0x01) {
					civ= portreg_read(ndev,EACOIVM0 + (queue *4));
					civexp = civ & 0xFFFF;
					civexp = civexp * 1000000;
					civexp = civexp / 65535;
					civman = civ >> 16;
					civman =  civman * 1000000;
					bw = civman + civexp;
					bw =  bw * 8;
					bw = bw * config.sysfreqkhz*1000;
					bw =  bw /1000000;
					bwbps = bw;
					bwqg = bwbps / 1000000000;
					bwqm = bwbps / 1000000;
					bwqk = bwbps / 1000;
					if(bwqg > 0) {
						sprintf(bwstr, "%dGbps",bwqg);
					} else if (bwqm > 0){
						bwrm = bwbps % 1000000;
						bwrm = bwrm /1000;
						if(bwrm != 0) {
							sprintf(bwstr, "%d.%03dMbps" ,bwqm, bwrm);
						} else {
							sprintf(bwstr, "%dMbps" ,bwqm);
						}
					} else if (bwqk > 0){
						bwrk = bwbps % 1000;
						if(bwrk != 0) {
							sprintf(bwstr, "%d.%03dKbps" ,bwqk, bwrk);
						} else {
							sprintf(bwstr, "%dKbps" ,bwqk);
						}
					}
					seq_printf(m, "%-13s", bwstr);

				} else {

					seq_printf(m, "%-13s", phy_speed_to_str(mdp->speed));
				}
			} else {
				seq_printf(m, "%-13s", "");
			}
		}
		seq_printf(m,"\n");
		if(longreport) {
			seq_printf(m, "%-10s","");
			seq_printf(m, "%-6s","CIV");
			for (port = 0; port < board_config.eth_ports; port++) {
				ndev = ppndev[port];
				cbs = portreg_read(ndev,EACOEM);
				strcpy(hexstr,"");
				if((cbs >> queue) & 0x01) {
					civ = portreg_read(ndev,EACOIVM0 + (queue *4));
					sprintf(hexstr,"%x_%04x",(civ>> 16) & 0xFFFF, civ & 0xFFFF);
					seq_printf(m, "%-13s",hexstr);
				} else {
					seq_printf(m, "%-13s", "");
				}

			}
			seq_printf(m,"\n");
			seq_printf(m, "%-10s","");
			seq_printf(m, "%-6s","CUL");
			for (port = 0; port < board_config.eth_ports; port++) {
				ndev = ppndev[port];
				cbs = portreg_read(ndev,EACOEM);
				strcpy(hexstr,"");
				if((cbs >> queue) & 0x01) {
					cul = portreg_read(ndev,EACOULM0 + (queue*4));
					sprintf(hexstr,"%04x_%04x", (cul>> 16) & 0xFFFF, cul & 0xFFFF);
					seq_printf(m, "%-13s",hexstr);
				} else {
					seq_printf(m, "%-13s", "");
				}

			}
			seq_printf(m,"\n");
		}
		seq_printf(m,"\n");
	}

	return 0;
}

/**
    @brief Wrapper function to show Port monitor status in /proc either in long format

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_eth_port_monitor_all_show(struct seq_file *m, void *v)
{
	return rswitch2_eth_port_monitor_show(m, 1);
}


/**
    @brief Wrapper function to show Port monitor status in /proc either in short format

    @param  seq_file *
    @param  void *

    @return int
*/
static int rswitch2_eth_port_monitor_short_show(struct seq_file *m, void *v)
{
	return rswitch2_eth_port_monitor_show(m, 0);
}

/**
    @brief Errors Show Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
    Tas and CBS error not covered tbd later when functionality implemented
*/
static int rswitch2_eth_errors_show(struct seq_file *m, void *v)
{
	u32 port;
	u32 priority = 0;
	struct net_device *ndev;
	struct port_private *mdp;

	for (port = 0; port < board_config.eth_ports; port++) {
		ndev = ppndev[port];
		mdp = netdev_priv(ndev);
		seq_printf(m, "Port =%s\n", ndev->name);
		seq_printf(m, "Data ECC Error Status   : %ld\n",
			   portreg_read(ndev, EAEIS0) & 0x01);
		seq_printf(m, "TAG ECC Error Status   : %ld\n",
			   (portreg_read(ndev, EAEIS0) >> 0x01) & 0x01);
		seq_printf(m, "PTR ECC Error Status   : %ld\n",
			   (portreg_read(ndev, EAEIS0) >> 0x02) & 0x01);
		seq_printf(m, "DESC ECC Error Status   : %ld\n",
			   (portreg_read(ndev, EAEIS0) >> 0x03) & 0x01);
		seq_printf(m, "Layer 2/3 Update ECC Error Status   : %ld\n",
			   (portreg_read(ndev, EAEIS0) >> 0x04) & 0x01);
		seq_printf(m, "Under Switch Min Frame Sz Error Status   : %ld\n",
			   (portreg_read(ndev, EAEIS0) >> 0x05) & 0x01);
		seq_printf(m, "TAG Filtering Error Status   : %ld\n",
			   (portreg_read(ndev, EAEIS0) >> 0x06) & 0x01);
		seq_printf(m, "Checksum Error Status   : %ld\n",
			   (portreg_read(ndev, EAEIS0) >> 0x07) & 0x01);
		seq_puts(m, "Frame Size Error Status:\n");
		for (priority = 0; priority < 8; priority++)
			seq_printf(m, "Priority%d   ", priority);

		seq_puts(m, "\n");
		for (priority = 0; priority < 8; priority++) {
			seq_printf(m, "%8ld   ",
				   (portreg_read(ndev, EAEIS0) >>
				    (0x08 + priority)) & 0x01);
		}
		seq_puts(m, "\n");
		seq_puts(m, "Descriptor Queue Overflow Error Status:\n");
		for (priority = 0; priority < 8; priority++)
			seq_printf(m, "Priority%d   ", priority);
		seq_puts(m, "\n");
		for (priority = 0; priority < 8; priority++) {
			seq_printf(m, "%8ld   ",
				   (portreg_read(ndev, EAEIS2) >>
				    (0x00 + priority)) & 0x01);
		}
		seq_puts(m, "\n");
		seq_printf(m,
			   "Cut Through Descriptor Queue Overflow Error Status   : %ld\n",
			   (portreg_read(ndev, EAEIS2) >> 0x08) & 0x01);
		seq_puts(m, "Descriptor Queue Security Error Status:\n");
		for (priority = 0; priority < 8; priority++)
			seq_printf(m, "Priority%d   ", priority);
		seq_puts(m, "\n");
		for (priority = 0; priority < 8; priority++) {
			seq_printf(m, "%8ld   ",
				   (portreg_read(ndev, EAEIS2) >> (16 + priority)) & 0x01);
		}
		seq_puts(m, "\n");
		seq_puts(m, "==================================================================================================================\n");
	}

	return 0;
}

static int rswitch2_eth_counters_show(struct seq_file *m, void *v)
{
	int i, j;
	const char *portName[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	u32 port;
	u32 r; //, rl, rh;
	//u32 total;
	//const u32 ports = 5;
	//int sum = 0;
	struct rswitch2_proc_eth_err_counter *err = &counter.err_counter;

	for (port = 0; port < board_config.eth_ports; port++)
		portName[port] = ppndev[port]->name;

	proc_print_str(m, "Interface               ", " %6s", portName,
		       board_config.eth_ports);
	seq_puts(m,     "========================");
	for (port = 0; port < board_config.eth_ports; port++)
		seq_printf(m, "=%.6s", "====================================");
	seq_puts(m, "\n");

	for (j = 0; j < 8; j++) {
		seq_printf(m, "Pending frames in Q#%d   ", j);
		for (i = 0; i < board_config.eth_ports; i++) {
			r = ioread32(ioaddr + EATDQM0 + j * 4 + 0xA000 +
				     i * 0x2000);
			seq_printf(m, " %6d", r);
		}
		seq_puts(m, "\n");
	}

	seq_printf(m, "\n%-24s", "Minimum Frame Size Error");
	for (port = 0; port < board_config.eth_ports; port++) {
		err->min_frame_sz_err[port] += portreg_read(ppndev[port],
							    EAUSMFSECN);
		seq_printf(m, " %6d", err->min_frame_sz_err[port]);
	}

	seq_printf(m, "\n%-24s", "Tag Filtering Error");
	for (port = 0; port < board_config.eth_ports; port++) {
		err->tag_filter_err[port] += portreg_read(ppndev[port],
							  EATFECN);
		seq_printf(m, " %6d", err->tag_filter_err[port]);
	}

	seq_printf(m, "\n%-24s", "Frame Size Error");
	for (port = 0; port < board_config.eth_ports; port++) {
		err->frame_size_err[port] += portreg_read(ppndev[port],
							  EAFSECN);
		seq_printf(m, " %6d", err->frame_size_err[port]);
	}

	seq_printf(m, "\n%-24s", "Descr Queue Overflow Err");
	for (port = 0; port < board_config.eth_ports; port++) {
		err->desc_q_oflow_err[port] += portreg_read(ppndev[port],
							    EADQOECN);
		seq_printf(m, " %6d", err->desc_q_oflow_err[port]);
	}

	seq_printf(m, "\n%-24s", "Descr Queue Security Err");
	for (port = 0; port < board_config.eth_ports; port++) {
		err->desc_q_sec_err[port] += portreg_read(ppndev[port],
							  EADQSECN);
		seq_printf(m, " %6d", err->desc_q_sec_err[port]);
	}

	seq_printf(m, "\n%-24s", "ChecKSum Error");
	for (port = 0; port < board_config.eth_ports; port++) {
		err->csum_err[port] += portreg_read(ppndev[port], EACKSECN);
		seq_printf(m, " %6d", err->csum_err[port]);
	}
	//~ seq_puts(m, "\n");
	//~ for(port = 0; port < board_config.eth_ports; port++){
	//~ ndev = ppndev[port];
	//~ mdp = netdev_priv(ndev);


	//~ seq_printf(m, "%s    %15ld     %15ld    %15ld     %15ld      %15ld      %15ld\n", ndev->name,
	//portreg_read(ndev, ),
	//~ portreg_read(ndev, ),
	//~ portreg_read(ndev, ),
	//~ portreg_read(ndev, ),
	//~ portreg_read(ndev, ),
	//~ portreg_read(ndev, ));

	//~ }
	seq_puts(m, "\n");

	return 0;
}

/**
    @brief Counters Show Show Proc Function

    @param  seq_file *
    @param  void *

    @return int
*/

static int read_mcast_entry_value(int index)
{
	int timeout, value;

	iowrite32(index, gwca_addr + GWMSTSS);
	timeout = 0;
	do {
		value = ioread32(gwca_addr + GWMSTSR);
	} while (value & 0x80000000 && ++timeout < 10);
	return value;
}

static int rswitch2_gwca_mcast_show(struct seq_file *m, void *v)
{
	int c, i, next;
	int value;

	seq_puts(m, "Chain     Additional targets\n");
	seq_puts(m, "=================================================\n");

    //we have 128 entries and show only the used ones
	for (c = 0; c < RSWITCH2_MAX_MCAST_CHAINS; c++) {
		value = read_mcast_entry_value(c);
		i = (value >> 8) & 0x7;  //how many links to follow
		if (i) {
			next = value & (RSWITCH2_MAX_MCAST_CHAINS - 1);
			seq_printf(m, "%3d   #%d ", c, i);
			while (i) {
				seq_printf(m, " -> %d", next);
				value = read_mcast_entry_value(next);
				next = value & (RSWITCH2_MAX_MCAST_CHAINS - 1);
				i--;
			}
			seq_puts(m, "\n");
		}
	}
	return 0;
}


/**
    @brief Write Rx TX ALL Clear Function for Proc

    @param  struct file *
    @param  const char *
    @param  size_t
    @param  loff_t *

    @return ssize_t
*/

static ssize_t rx_tx_all_clear(struct file *filp, const char *buff, size_t len,
			       loff_t *off)
{
	u32 ret;

	ret = kstrtoull_from_user(buff, len, 10, &rxtx_all_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}
	rswitch2_rx_tx_counter_clear_func();

	return len;
}

static ssize_t error_counter_clear(struct file *filp, const char *buff,
				   size_t len, loff_t *off)
{
	u32 ret = 0;

	ret = kstrtoull_from_user(buff, len, 10, &error_clear);
	if (ret) {
		/* Negative error code. */
		pr_info("ko = %d\n", ret);
		return ret;
	}

	rswitch2_error_counter_clear_func();
	return len;
}

static int rswitch2_eth_driver_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_eth_driver_show, NULL);
}

static int rswitch2_eth_rxtx_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_eth_rxtx_all_show, NULL);
}

static int rswitch2_eth_rxtx_short_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_eth_rxtx_short_show, NULL);
}

static int rswitch2_eth_port_monitor_open(struct inode *inode, struct  file *file)
{
	return single_open(file, rswitch2_eth_port_monitor_all_show, NULL);
}

static int rswitch2_eth_port_monitor_short_open(struct inode *inode, struct  file *file)
{
	return single_open(file, rswitch2_eth_port_monitor_short_show, NULL);
}

static int rswitch2_gwca_mcast_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_gwca_mcast_show, NULL);
}

/**
    @brief Open Error Proc Directory

    @param  inode *
    @param  file *

    @return int
*/
static int rswitch2_eth_errors_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_eth_errors_show, NULL);
}

static int rswitch2_eth_counters_open(struct inode *inode, struct file *file)
{
	return single_open(file, rswitch2_eth_counters_show, NULL);
}

static const struct file_operations rswitch2_eth_driver_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_eth_driver_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_eth_rxtx_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_eth_rxtx_open,
	.read = seq_read,
	.write = rx_tx_all_clear,
	.llseek = seq_lseek,
	.release = single_release,
};


static const struct file_operations rswitch2_eth_rxtx_short_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_eth_rxtx_short_open,
	.read = seq_read,
	.write = rx_tx_all_clear,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_eth_port_monitor_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_eth_port_monitor_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_eth_port_monitor_short_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_eth_port_monitor_short_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/**
    @brief  Proc File Operation
*/
static const struct file_operations rswitch2_eth_errors_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_eth_errors_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations rswitch2_eth_counters_fops = {
	.owner = THIS_MODULE,
	.open = rswitch2_eth_counters_open,
	.read = seq_read,
	.write = error_counter_clear,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
    @brief  Proc File Operation
*/
static const struct file_operations rswitch2_gwca_mcast_fops = {
	.owner   = THIS_MODULE,
	.open    = rswitch2_gwca_mcast_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/**
    @brief  Created Proc Directories

    @param  void

    @return void
*/
static void rswitch2_eth_create_proc_entry(void)
{
	/* create root & sub-directories */
	memset(&rswitch2_eth_procdir, 0, sizeof(rswitch2_eth_procdir));

	rswitch2_eth_procdir.rootdir = proc_mkdir(RSWITCH2_PROC_ROOT_DIR, NULL);

	root_dir = rswitch2_eth_procdir.rootdir;
	proc_create(RSWITCH2_ETH_PROC_FILE_DRIVER, 0,
		    rswitch2_eth_procdir.rootdir, &rswitch2_eth_driver_fops);
	proc_create(RSWITCH2_ETH_PROC_FILE_ERRORS, 0,
		    rswitch2_eth_procdir.rootdir, &rswitch2_eth_errors_fops);
	proc_create(RSWITCH2_ETH_PROC_FILE_COUNTERS, 0,
		    rswitch2_eth_procdir.rootdir, &rswitch2_eth_counters_fops);

	proc_create(RSWITCH2_ETH_PROC_FILE_RXTX, 0,
		    rswitch2_eth_procdir.rootdir, &rswitch2_eth_rxtx_fops);
	proc_create(RSWITCH2_ETH_PROC_FILE_RXTX_SHORT, 0,
		    rswitch2_eth_procdir.rootdir,
		    &rswitch2_eth_rxtx_short_fops);
	proc_create(RSWITCH2_ETH_PROC_FILE_PORT_MONITOR, 0,
		    rswitch2_eth_procdir.rootdir,
		    &rswitch2_eth_port_monitor_fops);
	proc_create(RSWITCH2_ETH_PROC_FILE_PORT_MONITOR_SHORT, 0,
		    rswitch2_eth_procdir.rootdir,
		    &rswitch2_eth_port_monitor_short_fops);

	proc_create(RSWITCH2_ETH_PROC_FILE_MCAST_CPU, 0,
		    rswitch2_eth_procdir.rootdir, &rswitch2_gwca_mcast_fops);
}

static void rswitch2_eth_remove_proc_entry(void)
{
	remove_proc_entry(RSWITCH2_ETH_PROC_FILE_DRIVER,
			  rswitch2_eth_procdir.rootdir);
	remove_proc_entry(RSWITCH2_ETH_PROC_FILE_ERRORS,
			  rswitch2_eth_procdir.rootdir);
	remove_proc_entry(RSWITCH2_ETH_PROC_FILE_COUNTERS,
			  rswitch2_eth_procdir.rootdir);
	remove_proc_entry(RSWITCH2_ETH_PROC_FILE_RXTX,
			  rswitch2_eth_procdir.rootdir);
	remove_proc_entry(RSWITCH2_ETH_PROC_FILE_RXTX_SHORT,
			  rswitch2_eth_procdir.rootdir);
	remove_proc_entry(RSWITCH2_ETH_PROC_FILE_PORT_MONITOR,
			  rswitch2_eth_procdir.rootdir);
	remove_proc_entry(RSWITCH2_ETH_PROC_FILE_PORT_MONITOR_SHORT,
			  rswitch2_eth_procdir.rootdir);

	remove_proc_entry(RSWITCH2_ETH_PROC_FILE_MCAST_CPU,
			  rswitch2_eth_procdir.rootdir);

	remove_proc_entry(RSWITCH2_PROC_ROOT_DIR, NULL);
}
#endif

static int drv_unload_remove_chrdev(void)
{
	int ret = 0;

	if (rswitchtsn_devclass == NULL)
		return ret;

	/* Remove character device */
	unregister_chrdev_region(rswitchtsn_dev, 1);
	device_del(&rswitchtsn_device);
	cdev_del(&rswitchtsn_cdev);
	class_destroy(rswitchtsn_devclass);

	return ret;
}

int rswitch2_eth_init(struct platform_device *pdev, struct pci_dev *pcidev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ports_node;
	struct device_node *child;
	int err;
	u32 gwvtc = 0;
	u32 eth_ports = 0;
	u32 a = 0;
	u32 value = 0;

	err = drv_probe_ioremap_common(pdev);
	err = rswitch2_clock_enable();

	rswitch2_gwca_reset();
	if (err < 0) {
		pr_err("Clock Enable Failed\n");
		return err;
	}
	err = rswitch2_gwca_init();
	if (err < 0) {
		pr_err("GWCA Init Failed\n");
		return err;
	}
	/* Set VLAN egress mode for GWCA to SCTAG*/
	iowrite32((RSWITCH2_VLAN_EGRESS_SCTAG_MODE), gwca_addr + GWVCC);
	gwvtc = (RSWITCH2_DEF_CTAG_VLAN_ID << 0) |
		(RSWITCH2_DEF_CTAG_PCP << 12) |
		(RSWITCH2_DEF_CTAG_DEI << 15) |
		(RSWITCH2_DEF_STAG_VLAN_ID << 16) |
		(RSWITCH2_DEF_STAG_PCP << 28) | (RSWITCH2_DEF_STAG_DEI << 31);
	iowrite32(gwvtc, gwca_addr + GWVTC);
	ports_node = of_get_child_by_name(np, "ports");
	if (!ports_node) {
		pr_err("Incorrect bindings: absent \"ports\" node\n");
		return -ENODEV;
	}
	if (of_property_read_u32(np, "board-variant", &board_variant) < 0) {
		pr_err("Board Variant Not present in device Tree (property \"board-variant\")\n");
		of_node_put(np);
		return -ENODEV;
	}
	pr_info("Board Variant Detected VC%d\n", board_variant);
	ppndev = kmalloc(sizeof(struct net_device *) *
			 (board_config.eth_ports + 1), GFP_KERNEL);
	if (IS_ERR(ppndev)) {
		dev_err(&pdev->dev, "[RSWITCH]2 kmalloc for %lu bytes FAILED\n",
			sizeof(struct net_device *) *
			(board_config.eth_ports + 1));
		err = PTR_ERR(ppndev);
		return err;
	}
	memset(ppndev, 0,
	       sizeof(struct net_device *) * (board_config.eth_ports + 1));

	if (board_variant == 0x03) {
		gpio = of_get_named_gpio(np, "mdio-gpio", 0);
		if (gpio_is_valid(gpio)) {
			pr_err("GPIO %d set to 1\n", gpio);
			gpio_request(gpio, "mdio-gpio");
			gpio_direction_output(gpio, 1);
			gpio_set_value(gpio, 1);
		} else {
			pr_err("RSWITCH VC3Warning: MDIO GPIO Not valid in Device tree\n");
		}
	}
	for_each_available_child_of_node(ports_node, child) {
		u32 index;

		eth_ports++;
		if (eth_ports > board_config.eth_ports) {
			pr_err("Device Tree Error More port nodes than supported by HW\n");
			return -1;
		}
		if (of_property_read_u32(child, "reg", &index) < 0) {
			pr_err("Port number not defined in device tree (property \"reg\")\n");
			of_node_put(child);
			return -ENODEV;
		}
		if ((index == 0) || (index == 4) || (index == 5))
			err = drv_probe_ioremap_eth(0);
		else if (index < 6)
			err = drv_probe_ioremap_eth(index);

		if (board_variant == 0x03) {
			if ((index == 0) || (index == 4) || (index == 5) ||
			    (index == 6) || (index == 7)) {
				if (mux_port == true) {
					pr_err("Can have either tsn0, eth1 or tsn7\n");
					continue;
				} else {
					mux_port = true;
				}
			}

			if ((index == 0) || (index == 4) || (index == 5)) {
				err = drv_probe_createnetdev(pdev, index,
							     &ppndev[0], child);
				netdev_created++;
			} else if (index < 6) {
				err = drv_probe_createnetdev(pdev, index,
							     &ppndev[index],
							     child);
				netdev_created++;
			}
		} else {
			err = drv_probe_createnetdev(pdev, index,
						     &ppndev[index], child);
			netdev_created++;
		}
		if (err < 0) {
			pr_err("Create Netdev for index %d failed\n", index);
			return err;
		}
		if (board_variant == 0x03) {
			if ((index != 0) && (index != 6) && (index != 7)) {
				if ((index == 4) || (index == 5)) {
					if (index == 4) {
						iowrite32(0x2, ioaddr - 0x01040000 + PHY_SEL_MUX);
					} else if (index == 5) {
						iowrite32(0x6, ioaddr - 0x01040000 + PHY_SEL_MUX);
						pr_info("Read after write 5=%x\n",
							ioread32(ioaddr - 0x01040000 + PHY_SEL_MUX));
					}
					err = rswitch2_phy_init(child, ppndev[0]);
					if (err < 0) {
						pr_err("Phy Init Failed\n");
						return err;
					}
				} else {
					err = rswitch2_phy_init(child, ppndev[index]);
					if (err < 0) {
						pr_err("Phy Init Failed\n");
						return err;
					}
				}
			} else if (index == 0) {
				iowrite32(0x0, ioaddr - 0x01040000 + PHY_SEL_MUX);
			} else if (index == 6) {
				nonetdev = 1;
				iowrite32(0x5, ioaddr - 0x01040000 + PHY_SEL_MUX);
			} else if (index == 7) {
				nonetdev = 1;
				iowrite32(0x1, ioaddr - 0x01040000 + PHY_SEL_MUX);
			}
		} else {
#ifdef PHY_CONFIG_SUPPORT
			err = rswitch2_phy_init(child, ppndev[index]);
			if (err < 0) {
				pr_err("Phy Init Failed\n");
				return err;
			}
#endif
		}
		/* Register Interrupts here Function in rswitch2_interrupt.c
		 * Need to share interrupt of forwarding engine and Ethernet,
		 * logical to put in separate file
		 */
		/* Proc Init in rswitch2_proc.c */
	}

#ifdef INTERNAL_GW
	/* Create Software Internal Gw Port */
	drv_probe_createvirtualnetdev(pdev, board_config.eth_ports,
				      &ppndev[board_config.eth_ports]);
#endif
	platform_set_drvdata(pdev, ppndev);

	/* Add character device for IOCTL interface */
	err = drv_probe_create_chrdev();
	if (err < 0) {
		drv_unload_remove_netdevs();
		drv_unload_cleanup();
		return err;
	}

#ifdef CONFIG_RENESAS_RSWITCH2_STATS
	rswitch2_eth_create_proc_entry();
#endif
	num_rx_ring[RSWITCH2_BE] = RX_RING_SIZE0;

	num_rx_ring[RSWITCH2_NC] = RX_RING_SIZE1;
	num_ts_ring = TS_RING_SIZE;

	rswitch2_desc_init(pdev);
	rswitch2_ts_desc_init(pdev);
	pr_info("board_config.eth_ports = %d\n", board_config.eth_ports);

	for (a = 0; a < eth_ports; a++) {
		if (board_variant == 0x03) {
			if ((nonetdev == 1) && (a == 0))
				continue;
			if (a < 4)
				err = rswitch2_txdmac_init(ppndev[a]);
		} else {
			err = rswitch2_txdmac_init(ppndev[a]);
		}
		if (err < 0) {
			pr_err("TX DMAC Init  Failed\n");
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
			rswitch2_eth_remove_proc_entry();
#endif
			drv_unload_remove_chrdev();
			drv_unload_remove_netdevs();
			drv_unload_cleanup();
			return err;
		}
	}
#ifdef INTERNAL_GW
	rswitch2_gw_txdmac_init(ppndev[board_config.eth_ports]);
#endif
	err = rswitch2_rxdmac_init();
	if (err < 0) {
		pr_err("RX DMAC Init Failed\n");
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
		rswitch2_eth_remove_proc_entry();
#endif
		drv_unload_remove_chrdev();
		drv_unload_remove_netdevs();
		drv_unload_cleanup();
		return err;
	}
#ifdef INTERNAL_GW
	rswitch2_gw_rxdmac_init();
#endif

#ifdef DEBUG
	pr_info("Reg=%x Value = %x\n", GWIICBSC, 2048);
#endif
	iowrite32(2048, gwca_addr + GWIICBSC);
	value = ioread32(gwca_addr+GWMDNC);
	iowrite32(value | (0xF << 8), gwca_addr + GWMDNC);
	rswitch2_ts_ring_init();
	rswitch2_ts_ring_format();
	err = rswitch2_gwca_change_mode(rswitch2_gwca_mode_disable);
	if (err < 0) {
		pr_err("gwCA Mode Change Failed\n");
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
		rswitch2_eth_remove_proc_entry();
#endif
		drv_unload_remove_chrdev();
		drv_unload_remove_netdevs();
		drv_unload_cleanup();
		return err;
	}
	err = rswitch2_gwca_change_mode(rswitch2_gwca_mode_operation);
	if (err < 0) {
		pr_err("gwCA Mode Change Failed\n");
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
		rswitch2_eth_remove_proc_entry();
#endif
		drv_unload_remove_chrdev();
		drv_unload_remove_netdevs();
		drv_unload_cleanup();
		return err;
	}
	err = rswitch2_bpool_config();
	if (err < 0) {
		pr_err("Bpool Config Failed\n");
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
		rswitch2_eth_remove_proc_entry();
#endif
		drv_unload_remove_chrdev();
		drv_unload_remove_netdevs();
		drv_unload_cleanup();
		return err;
	}
	/* Enable Direct Descriptor forwading */
	value = ioread32(ioaddr + FWPC10 + 0x10 * board_config.eth_ports);
	iowrite32(value | 0x01, ioaddr + FWPC10 + 0x10 * board_config.eth_ports);
	pr_info("Before Registering Interrupts\n");
	rswitch2_drv_probe_getinterrupts(pdev);
	if (err < 0) {
		pr_err("Interrupt Probe Failed\n");
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
		rswitch2_eth_remove_proc_entry();
#endif
		drv_unload_remove_chrdev();
		drv_unload_remove_netdevs();
		drv_unload_cleanup();
		return err;
	}
	rswitch2_probe_registerinterrupts();
	pr_info("After Registering Interrupts\n");
	if (err < 0) {
		pr_err("Register Interrupt Failed\n");
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
		rswitch2_eth_remove_proc_entry();
#endif
		drv_unload_remove_chrdev();
		drv_unload_remove_netdevs();
		drv_unload_cleanup();
		return err;
	}
	rswitch2_counter_clear_func();
	rswitch2_fwd_init(&board_config);
	ptp_rswitch2_init(pdev);
	for (a = 0; a < eth_ports; a++) {
		if (board_variant == 0x03) {
			if ((nonetdev == 1) && (a == 0))
				continue;

			if (a < 4) {
				err = register_netdev(ppndev[a]);
				if (err != 0) {
					dev_err(&pdev->dev,
						"[RSWITCH2] %s Failed to register net device. err(%d)\n",
						ppndev[a]->name, err);
					free_netdev(ppndev[a]);
					return err;
				}
			}
		} else {
			err = register_netdev(ppndev[a]);
			if (err != 0) {
				dev_err(&pdev->dev,
					"[RSWITCH2] %s Failed to register net device. err(%d)\n",
					ppndev[a]->name, err);
				free_netdev(ppndev[a]);
				return err;
			}
		}
	}
#ifdef INTERNAL_GW
	err = register_netdev(ppndev[board_config.eth_ports]);
	if (err != 0) {
		dev_err(&pdev->dev,
			"[RSWITCH2] %s Failed to register net device. err(%d)\n",
			ppndev[a]->name, err);
		free_netdev(ppndev[a]);
		return err;
	}
#endif
	pr_info("After Forwarding Init\n");

	return 0;
}

static void rswitch2_drv_remove_interrupts(void)
{
	int r;

	for (r = 0; r < ARRAY_SIZE(rswitch2_eth_irq); r++) {
		if (!rswitch2_eth_irq[r])
			continue;
		gpio_set_value(RSWITCH2_INT_LINE_GPIO + r, 0);
		gpio_unexport(RSWITCH2_INT_LINE_GPIO + r);
		free_irq(rswitch2_eth_irq[r], rswitch2_eth_irq_data);
		gpio_free(RSWITCH2_INT_LINE_GPIO + r);
		rswitch2_eth_irq[r] = 0;
		rswitch2_eth_irq_data[r] = 0;
		pr_info("IRQ Free\n");
	}
}

static int rswitch2_remove_one_platform(struct platform_device *pdev)
{
	rswitch2_fwd_exit();
	ptp_rswitch2_remove(pdev);
#ifdef CONFIG_RENESAS_RSWITCH2_STATS
	rswitch2_eth_remove_proc_entry();
#endif
	drv_unload_remove_chrdev();
	drv_unload_remove_netdevs();
	rswitch2_drv_remove_interrupts();
	drv_unload_cleanup();
	platform_set_drvdata(pdev, NULL);
	pr_info("[RSWITCH2] Module Unloaded\n");
	return 0;
}

int rswitch2_port_probe(void)
{
	u32 rcec_val;
	u32 ports = 0;

	iowrite32(0xFFFFFFFF, ioaddr + RCEC);
	rcec_val = ioread32(ioaddr + RCEC) & 0xFF;
	for (ports = 0; ports < 8; ports++) {
		if (!((rcec_val >> ports) & 0x01))
			break;
	}

	board_config.eth_ports = ports - 2;
	board_config.agents = board_config.eth_ports + 1;
	pr_info("Port Present = %d Agent Present = %d\n",
		board_config.eth_ports, board_config.agents);
	board_config.port_bitmask = GENMASK(board_config.eth_ports, 0);
	board_config.portgwca_bitmask = GENMASK(board_config.eth_ports + 1, 0);
	board_config.dbat_entry_num = board_config.eth_ports * NUM_TX_QUEUE +
				      INTERNAL_GW + NUM_RX_QUEUE;
	board_config.rx_queue_offset = 0;
	board_config.ris_bitmask = GENMASK(NUM_RX_QUEUE - 1,
					   board_config.rx_queue_offset);
	board_config.tis_bitmask = GENMASK(board_config.dbat_entry_num - 1,
					   NUM_RX_QUEUE);
	iowrite32(0xFFFFFFFF, ioaddr + RCDC);

	return 0;
};

static int rswitch2_init_one_platform(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int err = 0;

#ifdef DEBUG
	pr_info("ioaddr = %lx\n", (uintptr_t)ioaddr);
	pr_info("In match platform\n");
#endif
	if (pci_id != RENESAS_RSWITCH2_VC_PCI_ID) {
		pr_err("Wrong PCI ID in FPGA, RSW2 expects PCI ID=%x Present ID= %x, Driver Loading Aborted\n",
		       RENESAS_RSWITCH2_VC_PCI_ID, pci_id);
		return -EINVAL;
	}
	match = of_match_device(rswitch2_of_tbl, &pdev->dev);
	if (!match)
		return -EINVAL;
	rswitch2_reset();
	rswitch2_port_probe();
	err = rswitch2_eth_init(pdev, pcidev);
	if (err < 0) {
		pr_err("Driver Probe failed\n");
		platform_set_drvdata(pdev, NULL);
		return err;
	}

	return 0;
}

static struct platform_driver rswitch2_driver_platform = {
	.probe = rswitch2_init_one_platform,
	.remove = rswitch2_remove_one_platform,
	.driver = {
		.name = RSWITCH2_FPGA_ETH_PLATFORM,
		.of_match_table = rswitch2_of_tbl,
		.pm = RSWITCH2_PM_OPS,
	}
};

module_platform_driver(rswitch2_driver_platform);
MODULE_AUTHOR("Asad Kamal");
MODULE_DESCRIPTION("Renesas  R-Switch2 Ethernet AVB/TSN driver");
MODULE_LICENSE("GPL v2");
