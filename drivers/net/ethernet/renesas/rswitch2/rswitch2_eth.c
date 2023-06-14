// SPDX-License-Identifier: GPL-2.0
/* Renesas RSwitch2 Ethernet device driver
 *
 * Copyright (C) 2019-2021 Renesas Electronics Corporation
 *
 */
//#define DEBUG_SERDES

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/if_vlan.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>
#include <linux/net_tstamp.h>

#include <linux/of_platform.h>

#include "rswitch2.h"
#include "rswitch2_eth.h"
#include "rswitch2_gwca.h"
#include "rswitch2_fwd.h"

#include "rswitch2_rmac.h"
#include "rswitch2_serdes.h"
#include "../rtsn_ptp.h"

static int rswitch2_gwca_set_state(struct rswitch2_drv *rsw2, enum gwmc_op state)
{
	int ret;
	u32 reg_val;

	reg_val = FIELD_PREP(GWMC_OPC, state);
	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWMC);

	ret = readl_poll_timeout(rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWMS, reg_val,
						reg_val == FIELD_PREP(GWMS_OPS, state),
						RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "Setting GWCA state timed out\n");
		return ret;
	}
	return 0;
}

static int rswitch2_emac_set_state(struct net_device *ndev, enum emac_op state)
{
	int ret;
	u32 reg_val;
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	void __iomem *etha_base_addr;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;
	etha_base_addr = (void __iomem *)ndev->base_addr;

	reg_val = FIELD_PREP(EAMC_OPC, state);
	iowrite32(reg_val, etha_base_addr + RSW2_ETHA_EAMC);

	ret = readl_poll_timeout_atomic(etha_base_addr + RSW2_ETHA_EAMS, reg_val,
						(reg_val == FIELD_PREP(EAMS_OPS, state)),
						RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "Setting EMAC state from %d to %d timed out\n", reg_val, state);
		return ret;
	}

	return 0;
}

static int rswitch2_gwca_init(struct rswitch2_drv *rsw2)
{
	u32 reg_val;
	int ret;

	/* TODO: Only GWCA0 supported in FPGA implementation */

	ret = rswitch2_gwca_set_state(rsw2, gwmc_disable);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "Failed to set GWCA disable state\n");
		goto err_out;
	}


	ret = rswitch2_gwca_set_state(rsw2, gwmc_reset);
		if (ret != 0) {
			rsw2_err(MSG_GEN, "Failed to set GWCA reset state\n");
			goto err_out;
		}


	ret = rswitch2_gwca_set_state(rsw2, gwmc_disable);
		if (ret != 0) {
			rsw2_err(MSG_GEN, "Failed to set GWCA disable state\n");
			goto err_out;
		}


	ret = rswitch2_gwca_set_state(rsw2, gwmc_config);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "Failed to set GWCA config state\n");
		goto err_out;
	}

	/* Reset multicast table */
	iowrite32(GWMTIRM_MTIOG, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWMTIRM);

	/* Wait for operation to complete */
	ret = readl_poll_timeout(rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWMTIRM, reg_val,
							(reg_val == GWMTIRM_MTR),
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "Failed to reset multicast table\n");
		goto err_out;
	}

	/* Reset AXI RAM */
	iowrite32(GWARIRM_ARIOG, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWARIRM);

	/* Wait for operation to complete */
	ret = readl_poll_timeout(rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWARIRM, reg_val,
							(reg_val == GWARIRM_ARR),
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "Failed to reset AXI RAM table\n");
		goto err_out;
	}

	/* Set VLAN egress mode for GWCA to SCTAG*/
	iowrite32(FIELD_PREP(GWVCC_VEM, sc_ctag), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWVCC);

	reg_val = FIELD_PREP(GWVTC_CTV, RSWITCH2_DEF_CTAG_VLAN_ID);
	reg_val |= FIELD_PREP(GWVTC_CTP, RSWITCH2_DEF_CTAG_PCP);
	reg_val |= FIELD_PREP(GWVTC_CTD, RSWITCH2_DEF_CTAG_DEI);
	reg_val |= FIELD_PREP(GWVTC_STV, RSWITCH2_DEF_STAG_VLAN_ID);
	reg_val |= FIELD_PREP(GWVTC_STP, RSWITCH2_DEF_STAG_PCP);
	reg_val |= FIELD_PREP(GWVTC_STD, RSWITCH2_DEF_STAG_DEI);
	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWVTC);

	/* Allow split frames with up to 15 descriptors */
	reg_val = ioread32(rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWMDNC);
	reg_val |= FIELD_PREP(GWMDNC_TXDMN, TX_MAX_DESC_PER_FRAME - 1);
	reg_val |= FIELD_PREP(GWMDNC_RXDMN, RX_MAX_DESC_PER_FRAME - 1);
	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWMDNC);

	return 0;

err_out:
	return ret;
}


/* SerDes functionality */
void rswitch2_serdes_write32(void __iomem *addr, u32 offs,  u32 bank, u32 data)
{
	iowrite32(bank, addr + RSWITCH_SERDES_BANK_SELECT);
	iowrite32(data, addr + offs);
#ifdef DEBUG_SERDES
	printk("SERDES WR %d: %03x - %04x  <= %04x\n", (int)((u64)addr&0xc00)/0x400, bank, offs, data);
#endif
}

u32 rswitch2_serdes_read32(void __iomem *addr, u32 offs,  u32 bank)
{
	u32 ret;

	iowrite32(bank, addr + RSWITCH_SERDES_BANK_SELECT);
	ret = ioread32(addr + offs);

#ifdef DEBUG_SERDES
	printk("SERDES RD %d: %03x - %04x  is %04x\n", (int)((u64)addr&0xc00)/0x400, bank, offs, ret);
#endif
	return ret;
}

static int rswitch2_serdes_reg_wait(void __iomem *addr, u32 offs, u32 bank, u32 mask, u32 expected)
{
	u32 ret;
	int i;

	iowrite32(bank, addr + RSWITCH_SERDES_BANK_SELECT);
	//udelay(100);

	for (i = 0; i < 1000 /*RSWITCH2_REG_POLL_TIMEOUT*/; i++) {
		ret = ioread32(addr + offs);
		if ((ret & mask) == expected) {
#ifdef DEBUG_SERDES
			printk("SERDES po %d: %03x - %04x  is %04x after %d iterations\n", (int)((u64)addr&0xc00)/0x400, bank, offs, ret, i);
#endif
			return 0;
		}
		mdelay(1);
	}
#ifdef DEBUG_SERDES
	printk("(SERDES po %d: %03x - %04x  is %04x but %04x==%04x expected\n", (int)((u64)addr&0xc00)/0x400, bank, offs, ret, (ret&mask), expected);
#endif
	return -ETIMEDOUT;
}

static int rswitch2_serdes_set_speed(struct rswitch2_eth_port *eth_port, int phy_speed)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_physical_port *phy_port;
	void __iomem *addr = eth_port->phy_port->serdes_chan_addr;
	int ret;

	phy_port = eth_port->phy_port;
	rsw2 = eth_port->rsw2;

	rsw2_notice(MSG_SERDES, "Set serdes speed: %d\n", phy_speed);

	switch (phy_port->phy_iface) {
	case PHY_INTERFACE_MODE_SGMII:
		if (phy_speed == SPEED_1000)
			rswitch2_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x0140);
		else if (phy_speed == SPEED_100)
			rswitch2_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x2100);
		else if (phy_speed == SPEED_10)
			rswitch2_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x0100);
		else
			return -EOPNOTSUPP;
#if 0
		//Autoneg for 1G
		ret = rswitch2_serdes_reg_wait(addr, 0x008, BANK_1F80, BIT(0), 1);
		if (ret)
			return ret;
		rswitch2_serdes_write32(addr, 0x008, BANK_1F80, 0);
#endif
		break;

	case PHY_INTERFACE_MODE_USXGMII:
		if (phy_speed == SPEED_2500)
			rswitch2_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x120);
		else
			return -EOPNOTSUPP;
		udelay(50);
		rswitch2_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2600);
		ret = rswitch2_serdes_reg_wait(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, BIT(10), 0);
		if (ret) {
			rsw2_err(MSG_SERDES, "Speed update failed  err=%d\n", ret);
			return ret;
		}
		break;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}




static int rswitch2_serdes_init(struct net_device *ndev, bool check_op);
static void rswitch2_phy_state_change(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct phy_device *phydev;
	u32 reg_val;
	int ret;
	unsigned long flags;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;
	phy_port = eth_port->phy_port;
	phydev = ndev->phydev;

	rsw2_info(MSG_GEN, "Link change(%d): %s uses %s at %d Mbps\n", phydev->link, ndev->name, phy_modes(phydev->interface), phydev->speed);
	if(phydev->speed == SPEED_10000)
		return;

	if(!phydev->link) {

		void __iomem *etha_base_addr;
		phy_interface_t phy_iface;
		int speed;

		etha_base_addr = (void __iomem *)ndev->base_addr;
		spin_lock_irqsave(&rsw2->lock, flags);
		rsw2_notice(MSG_GEN, "================= Link Down start (%s) ===================\n", ndev->name);

		/* ETHA state machine will lock up due to SerDes connection in S4 ES 1.0
		 * Lockup can be release by switching SerDes to USXGMII */
		if(phy_port->phy_iface != PHY_INTERFACE_MODE_USXGMII) {
			reg_val = FIELD_PREP(EAMC_OPC, emac_disable);
			iowrite32(reg_val, etha_base_addr + RSW2_ETHA_EAMC);

			phy_iface = phy_port->phy_iface;
			speed = phy_port->phy->speed;

			phy_port->phy_iface = PHY_INTERFACE_MODE_USXGMII;
			phy_port->phy->speed = SPEED_2500;
			rswitch2_serdes_init(ndev, false);

			rswitch2_emac_set_state(ndev, emac_disable);

			phy_port->phy_iface = phy_iface;
			phy_port->phy->speed = speed;
		}

		rsw2_notice(MSG_GEN, "===================== Link Down end =======================\n");
		spin_unlock_irqrestore(&rsw2->lock, flags);

	} else {
		uint phy_port_num;
		void __iomem *etha_base_addr;
		etha_base_addr = (void __iomem *)ndev->base_addr;
		spin_lock_irqsave(&rsw2->lock, flags);

		rsw2_notice(MSG_GEN, "================= Link Up start (%s) ==================\n", ndev->name);

		if(phy_port->phy_iface != PHY_INTERFACE_MODE_USXGMII) {
			phy_port_num = eth_port->port_num - eth_port->rsw2->num_of_cpu_ports;


		ret = rswitch2_emac_set_state(ndev, emac_config);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Port set state 'config' failed\n");
		}
		else {
			rsw2_dbg(MSG_GEN, "Port set state 'config' SUCCEEDED\n");
		}

		reg_val = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MPIC);
		reg_val = reg_val & ~0x1Fu;

		switch (phydev->speed) {
			case SPEED_100:
				reg_val |= FIELD_PREP(MPIC_LSC, rsw2_rmac_100mbps);
				reg_val |= FIELD_PREP(MPIC_PIS, rsw2_rmac_gmii);
				phy_port->phy_iface = PHY_INTERFACE_MODE_SGMII;
				phy_port->phy->speed = SPEED_100;

				break;

			case SPEED_1000:
				reg_val |= FIELD_PREP(MPIC_LSC, rsw2_rmac_1000mbps);
				reg_val |= FIELD_PREP(MPIC_PIS, rsw2_rmac_gmii);
				phy_port->phy_iface = PHY_INTERFACE_MODE_SGMII;
				phy_port->phy->speed = SPEED_1000;

				break;

			case SPEED_2500:
				reg_val |= FIELD_PREP(MPIC_LSC, rsw2_rmac_2500mbps);
				reg_val |= FIELD_PREP(MPIC_PIS, rsw2_rmac_xgmii);
				phy_port->phy_iface = PHY_INTERFACE_MODE_USXGMII;
				phy_port->phy->speed = SPEED_2500;

				break;

			default:
				rsw2_err(MSG_GEN, "Unsupported Speed\n");
				spin_unlock_irqrestore(&rsw2->lock, flags);
				return;
		}

		rsw2_notice(MSG_GEN, "Link change: %s uses %s at %d Mbps\n", ndev->name, phy_modes(phydev->interface), phydev->speed);

		iowrite32(reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MPIC);
		rsw2_dbg(MSG_GEN, "reg_val=0x%.8x (expected): 0x%.8x\n", ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MPIC), reg_val);

		ret = rswitch2_emac_set_state(ndev, emac_disable);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Port set state 'disable' failed\n");
		}
		else {
			rsw2_dbg(MSG_GEN, "Port set state 'disable' SUCCEEDED\n");
		}

		ret = rswitch2_emac_set_state(ndev, emac_operation);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Port set state 'operation' failed\n");
		}
		else {
			rsw2_dbg(MSG_GEN, "Port set state 'operation' SUCCEEDED\n");
		}
		}

		rswitch2_serdes_init(ndev, true);

		rsw2_notice(MSG_GEN, "===================== Link Up end =======================\n");
		spin_unlock_irqrestore(&rsw2->lock, flags);

	}

	if (netif_msg_link(rsw2)) {
		int phy_speed = 0;

		if(phydev->link)
			phy_speed =phydev->speed;

		phy_print_status(phydev);

		rsw2_info(MSG_GEN, "Link status changed. PHY %d UID 0x%08x Link = %d Speed = %d\n",
				  phydev->mdio.addr, phydev->phy_id, phydev->link, phy_speed);
	}
	else {
		rsw2_dbg(MSG_GEN,"PHY state change but no msg link!\n");
	}
}

static struct device_node *rswitch2_get_port_node(struct rswitch2_drv *rsw2, unsigned int port_num)
{
	struct device_node *ports, *port;
	int err = 0;
	u32 index;

	ports = of_get_child_by_name(rsw2->dev->of_node, "ports");
	if (!ports)
		return NULL;

	for_each_child_of_node(ports, port) {
		err = of_property_read_u32(port, "reg", &index);
		if (err < 0)
			return NULL;
		if (index == port_num) {
			of_node_get(port);
			break;
		}

	}
	of_node_put(ports);

	return port;
}





static int rswitch2_serdes_init_sram(struct rswitch2_drv *rsw2)
{
	int ret;
	int lane;
	void __iomem *addr;

	//printk("%s()\n", __FUNCTION__);
	for (lane = 0; lane < rsw2->num_of_tsn_ports; lane++) {
		addr = rsw2->serdes_base_addr + (RSW2_SERDES_CHANNEL_OFFSET * lane);
		ret = rswitch2_serdes_reg_wait(addr,
				VR_XS_PMA_MP_12G_16G_25G_SRAM, BANK_180, BIT(0), 0x01);
		if (ret) {
			rsw2_err(MSG_SERDES, "SERDES SRAM init on lane %d failed (step 1)\n", lane);
			return ret;
		}
	}

	//Step 2 only on lane 0
	rswitch2_serdes_write32(rsw2->serdes_base_addr, VR_XS_PMA_MP_12G_16G_25G_SRAM,
			       BANK_180, 0x3);

	for (lane = 0; lane < rsw2->num_of_tsn_ports; lane++) {
		addr = rsw2->serdes_base_addr + (RSW2_SERDES_CHANNEL_OFFSET * lane);
		ret = rswitch2_serdes_reg_wait(addr,
				SR_XS_PCS_CTRL1, BANK_300, BIT(15), 0);
		if (ret) {
			rsw2_err(MSG_SERDES, "SERDES on lane %d does not finish reset (step 3)\n", lane);
			return ret;
		}
	}
	return 0;
}

static int rswitch2_serdes_common_setting(struct rswitch2_drv *rsw2)
{
	int ret;
	int lane;
	void __iomem *addr;

	enum {UNDEF, SGMII_ONLY, USXGMII_ONLY, MIXED, ERROR} mode = UNDEF;

	/* Check if common init is already done */
	if (!rsw2->serdes_common_init_done)
		rsw2->serdes_common_init_done = true;
	else
		return 0;

	/* Check the SERDES configuration of all ports to decide which
	 * PLLs to be activated */
	for (lane = 0; lane < rsw2->num_of_tsn_ports; lane++) {
		phy_interface_t phy_iface;
		uint phy_port_num = lane + rsw2->num_of_cpu_ports;
		struct rswitch2_eth_port *cur_port = rsw2->ports[phy_port_num];

		if(!cur_port)
			continue;

		phy_iface = cur_port->phy_port->phy_iface;
		rsw2_dbg(MSG_SERDES, "PHYiface[%d] is %s (%d)\n", lane, phy_modes(phy_iface), phy_iface);

		switch (phy_iface) {
		case PHY_INTERFACE_MODE_SGMII :
			if (mode == UNDEF || mode == SGMII_ONLY) mode = SGMII_ONLY;
			else if (mode == USXGMII_ONLY || mode == MIXED) mode = MIXED;
			else mode = ERROR;
			break;
		case PHY_INTERFACE_MODE_USXGMII :
			if (mode == UNDEF || mode == USXGMII_ONLY) mode = USXGMII_ONLY;
			else if (mode == SGMII_ONLY || mode == MIXED) mode = MIXED;
			else mode = ERROR;
			break;
		default:
			mode = ERROR;
		}
	}

	/* Disable FUSE_OVERRIDE_EN */
	for (lane = 0; lane < rsw2->num_of_tsn_ports; lane++) {
		addr = rsw2->serdes_base_addr + RSWITCH_SERDES_FUSE_OVERRIDE(lane);
		if (ioread32(addr))
			iowrite32(0, addr);
	}

	/* Initialize SRAM */
	ret = rswitch2_serdes_init_sram(rsw2);
	if (ret)
		return ret;

	for (lane = 0; lane < rsw2->num_of_tsn_ports; lane++) {
		addr = rsw2->serdes_base_addr + (RSW2_SERDES_CHANNEL_OFFSET * lane);
		rswitch2_serdes_write32(addr, VR_XS_PCS_SFTY_MR_CTRL, BANK_380, 0x443);
	}

	addr = rsw2->serdes_base_addr;
	switch (mode) {
	case SGMII_ONLY:
		rsw2_notice(MSG_SERDES, "Configure SERDES PLLs for SGMII mode\n");
		//S4.1~S4.5
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_REF_CLK_CTRL, BANK_180, 0x97);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_MPLLB_CTRL0, BANK_180, 0x60);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_MPLLB_CTRL2, BANK_180, 0x2200);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_MPLLB_CTRL1, BANK_180, 0);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_MPLLB_CTRL3, BANK_180, 0x3d);
		break;

	case USXGMII_ONLY:
		rsw2_notice(MSG_SERDES, "Configure SERDES PLLs for USXGMII mode\n");
		//U4.1 ~ U4.5
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_REF_CLK_CTRL, BANK_180, 0x57);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_10G_MPLLA_CTRL2, BANK_180, 0xc200);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_MPLLA_CTRL0, BANK_180, 0x42);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_MPLLA_CTRL1, BANK_180, 0);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_MPLLA_CTRL3, BANK_180, 0x2f);
		break;

	case MIXED:
		rsw2_notice(MSG_SERDES, "Configure SERDES PLLs for mixed SGMII and USXGMII mode\n");
		//to allow mix operation SGMII and USXGMII PLLs configured both
		//C4.1 == U4.1 | S4.1
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_REF_CLK_CTRL, BANK_180, 0x57 | 0x97);

		//C4.2 == U4.2
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_10G_MPLLA_CTRL2, BANK_180, 0xc200);
		//C4.3 == U4.3
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_MPLLA_CTRL0, BANK_180, 0x42);
		//C4.4 == U4.4
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_MPLLA_CTRL1, BANK_180, 0);
		//C4.5 == U4.5
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_MPLLA_CTRL3, BANK_180, 0x2f);

		//C4.6 == S4.2
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_MPLLB_CTRL0, BANK_180, 0x60);
		//C4.7 == S4.2
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_MPLLB_CTRL2, BANK_180, 0x2200);
		//C4.8 == S4.2
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_MPLLB_CTRL1, BANK_180, 0);
		//C4.9 == S4.2
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_MPLLB_CTRL3, BANK_180, 0x3d);
		break;

	default:
		rsw2_err(MSG_SERDES, "Unsupported combination of MAC xMII formats\n");
		return -EOPNOTSUPP;
	}

	/* Assert soft reset for SERDES PHY (step 5) */
	for (lane = 0; lane < rsw2->num_of_tsn_ports; lane++) {
		addr = rsw2->serdes_base_addr + (RSW2_SERDES_CHANNEL_OFFSET * lane);
		rswitch2_serdes_write32(addr, 0x03d0, BANK_380, 1);
	}
	rswitch2_serdes_write32(rsw2->serdes_base_addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x8000);

	/* Re-Initialize SRAM */
	ret = rswitch2_serdes_init_sram(rsw2);
	if (ret)
		return ret;

	/* Check for soft reset done */
	ret = rswitch2_serdes_reg_wait(rsw2->serdes_base_addr,
			VR_XS_PCS_DIG_CTRL1, BANK_380, BIT(15), 0);
	if (ret) {
		rsw2_err(MSG_SERDES, "SERDES does not finished soft reset (step 8)\n");
		return ret;
	}

	return 0;
}

static int rswitch2_serdes_chan_setting(struct rswitch2_eth_port *eth_port)
{
	void __iomem *addr;
	struct rswitch2_drv *rsw2;
	struct rswitch2_physical_port *phy_port;
	int ret;

	phy_port = eth_port->phy_port;
	rsw2 = eth_port->rsw2;
	addr = phy_port->serdes_chan_addr;

	//printk("%s(port=%d, mode=%d)\n", __FUNCTION__, eth_port->port_num, mode);
	switch (phy_port->phy_iface) {
	case PHY_INTERFACE_MODE_SGMII:
		rswitch2_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2000);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_MPLL_CMN_CTRL, BANK_180, 0x11);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_VCO_CAL_LD0, BANK_180, 0x540);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_VCO_CAL_REF0, BANK_180, 0x15);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, 0x100);
		rswitch2_serdes_write32(addr, VR_XS_PMA_CONSUMER_10G_RX_GENCTRL4, BANK_180, 0);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_RATE_CTRL, BANK_180, 0x02);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_RX_RATE_CTRL, BANK_180, 0x03);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, 0x100);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, 0x100);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_AFE_DFE_EN_CTRL, BANK_180, 0);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_RX_EQ_CTRL0, BANK_180, 0x07);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_10G_RX_IQ_CTRL0, BANK_180, 0);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1, BANK_180, 0x310);

		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, 0x0101);
		ret = rswitch2_serdes_reg_wait(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, BIT(0), 0);
		if (ret) {
			rsw2_err(MSG_SERDES, "SerDes req wait failed\n");
			return ret;
		}
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, 0x101);
		ret = rswitch2_serdes_reg_wait(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, BIT(0), 0);
		if (ret) {
			rsw2_err(MSG_SERDES, "SerDes req wait failed\n");
			return ret;
		}

		//Accept AutoNeg
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1, BANK_180, 0x1310);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL0, BANK_180, 0x1800);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL1, BANK_180, 0);

		rswitch2_serdes_write32(addr, SR_XS_PCS_CTRL2, BANK_300, 0x01);

		rswitch2_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2100);
		ret = rswitch2_serdes_reg_wait(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, BIT(8), 0);
		if (ret) {
			rsw2_err(MSG_SERDES, "SerDes req wait failed\n");
			return ret;
		}
#if 0
		//Enable AutoNeg
		rswitch2_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x0140);  //start from 1000
		//rswitch2_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x2100);  //start from 100
		rswitch2_serdes_write32(addr, 0x004, BANK_1F80, 5);
		rswitch2_serdes_write32(addr, 0x028, BANK_1F80, 0x7a1);
		rswitch2_serdes_write32(addr, 0x000, BANK_1F80, 0x208);
#endif
		break;

	case PHY_INTERFACE_MODE_USXGMII:
		rswitch2_serdes_write32(addr, SR_XS_PCS_CTRL2, BANK_300, 0x0);
		rswitch2_serdes_write32(addr, VR_XS_PCS_DEBUG_CTRL, BANK_380, 0x50);
		rswitch2_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2200);

		rswitch2_serdes_write32(addr, VR_XS_PCS_KR_CTRL, BANK_380, 0x400);

		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_MPLL_CMN_CTRL,
				       BANK_180, 0x1);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_VCO_CAL_LD0, BANK_180, 0x56a);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_VCO_CAL_REF0, BANK_180, 0x15);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, 0x1100);
		rswitch2_serdes_write32(addr, VR_XS_PMA_CONSUMER_10G_RX_GENCTRL4, BANK_180, 1);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_RATE_CTRL, BANK_180, 0x01);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_RX_RATE_CTRL, BANK_180, 0x01);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, 0x300);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, 0x300);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_AFE_DFE_EN_CTRL, BANK_180, 0);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_RX_EQ_CTRL0, BANK_180, 0x0);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_10G_RX_IQ_CTRL0, BANK_180, 0);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1, BANK_180, 0x310);

		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, 0x0301);
		ret = rswitch2_serdes_reg_wait(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, BIT(0), 0);
		if (ret)
			return ret;

		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, 0x301);
		ret = rswitch2_serdes_reg_wait(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, BIT(0), 0);
		if (ret)
			return ret;

		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1, BANK_180, 0x1310);
		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL0, BANK_180, 0x1800);

		rswitch2_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL1, BANK_180, 0);

		rswitch2_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2300);
		ret = rswitch2_serdes_reg_wait(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, BIT(8), 0);
		if (ret)
			return ret;

		//TODO: Some phys does not support auto neg on USXGMII (as the one on VC4)
		//This code shall be finally enabled/disabled by devicetree
#if 0
		//Enter AN_ON
		rswitch2_serdes_write32(addr, VR_MII_AN_CTRL, BANK_1F80, 1);
		rswitch2_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x1000);
		ret = rswitch2_serdes_reg_wait(addr, 8, BANK_1F80, BIT(0), 1);
		rswitch2_serdes_write32(addr, 8, BANK_1F80, 0);
		if (ret) {
				printk("Enter AN_ON failed\n");
				return ret;
		}
#endif
		break;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

#if 0
static int dummy_config_aneg(struct phy_device *phydev)
{
	struct rswitch2_drv *rsw2;
	struct net_device *ndev = phydev->attached_dev;
	struct rswitch2_eth_port *eth_port;

	eth_port = netdev_priv(ndev);

	rsw2 = eth_port->rsw2;
	rsw2_err(MSG_GEN, "dummy_config_aneg(): Link change(%d): %s uses %s at %d Mbps\n", phydev->link, ndev->name, phy_modes(phydev->interface), phydev->speed);
	phydev->speed = 2500;
	phydev->link = 1;
	linkmode_empty(phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, phydev->supported);
	//ETHTOOL_LINK_MODE_2500baseT_Full_BIT
	return 0;
}
#endif
static void rswitch2_serdes_check_operation(struct timer_list *t)
{
	struct rswitch2_physical_port *phy_port = from_timer(phy_port, t, serdes_usxgmii_op_timer);
	unsigned long flags;
	u32 reg_val;


	/* FIXME: May there more elegant way to get eth_port /rsw2 data? */
	struct rswitch2_drv *rsw2;
	struct net_device *ndev = phy_port->phy->attached_dev;
	struct rswitch2_eth_port *eth_port;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	spin_lock_irqsave(&rsw2->lock, flags);


	if (phy_port->serdes_usxgmii_op_cnt < RSW2_SERDES_OP_RETRIES) {
		//Link status is latched, for current status read twice
		rswitch2_serdes_read32(phy_port->serdes_chan_addr, SR_XS_PCS_STS1, BANK_300);
		reg_val = rswitch2_serdes_read32(phy_port->serdes_chan_addr, SR_XS_PCS_STS1, BANK_300);
		if (reg_val & 0x04) {
			rsw2_notice(MSG_SERDES, "SerDes USXGMII is operational on port %s %s (retries = %d)\n", phy_port->mii_bus->name, phy_port->mii_bus->id, phy_port->serdes_usxgmii_op_cnt);
		}
		else {
			rsw2_notice(MSG_SERDES, "Resetting SerDes USXGMII on port %s %s (retries = %d)\n", phy_port->mii_bus->name, phy_port->mii_bus->id, phy_port->serdes_usxgmii_op_cnt);


			reg_val = rswitch2_serdes_read32(phy_port->serdes_chan_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180);
			rswitch2_serdes_write32(phy_port->serdes_chan_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, reg_val | 0x10);
			udelay(10);
			rswitch2_serdes_write32(phy_port->serdes_chan_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, reg_val);

			/* Try again */
			phy_port->serdes_usxgmii_op_cnt++;
			mod_timer(&phy_port->serdes_usxgmii_op_timer, jiffies + msecs_to_jiffies(RSW2_SERDES_OP_TIMER_INTERVALL));
		}
	}
	else {
		rsw2_err(MSG_SERDES, "Could not bring SerDes USXGMII on port %s %s into operational state. Giving up!\n", phy_port->mii_bus->name, phy_port->mii_bus->id);
	}

	spin_unlock_irqrestore(&rsw2->lock, flags);
}

static int rswitch2_serdes_init(struct net_device *ndev, bool check_op)
{
	int ret;
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	int phy_speed;
	uint port_num;

	eth_port = netdev_priv(ndev);
	phy_port = eth_port->phy_port;

	/* Only physical ports have PHYs attached */
	BUG_ON(phy_port == NULL);
	rsw2 = eth_port->rsw2;

	ret = rswitch2_serdes_common_setting(rsw2);
	if (ret)
		return ret;

	port_num = eth_port->port_num - eth_port->rsw2->num_of_cpu_ports;

	/* TODO: Support more modes and speed selection */
	//printk("%s: sw0p%d  phy_mode=%d\n", __FUNCTION__, port_num, ndev->phydev->interface );
	switch (phy_port->phy_iface) {
	case PHY_INTERFACE_MODE_SGMII:
		if((phy_port->phy->speed != SPEED_100) && (phy_port->phy->speed != SPEED_1000)) {
			phy_speed = SPEED_100;
			rsw2_notice(MSG_SERDES, "No valid default speed. Setting to %d Mbit/s\n", phy_speed);
		} else
			phy_speed = phy_port->phy->speed;

		rsw2_notice(MSG_SERDES, "port %d uses SGMII at %d Mbit/s\n", port_num, phy_speed);
		break;

	case PHY_INTERFACE_MODE_USXGMII:
		if(phy_port->phy->speed != SPEED_2500) {
			phy_speed = SPEED_2500;
			rsw2_notice(MSG_SERDES, "No valid default speed. Setting to %d Mbit/s\n", phy_speed);
		} else
			phy_speed = phy_port->phy->speed
			;
		rsw2_notice(MSG_SERDES, "port %d uses USXGMII at %d Mbit/s\n", port_num, phy_speed);
		break;

	default:
		rsw2_err(MSG_SERDES, "%s: Don't support this interface %d on port %d", __func__,
				phy_port->phy_iface, port_num);
		return -EOPNOTSUPP;
	}


	/* Set channel settings*/
	ret = rswitch2_serdes_chan_setting(eth_port);
	if (ret) {
		rsw2_err(MSG_SERDES, "channel specific SERDES configuration for port %d failed. ret=%d\n", port_num, ret);
		return ret;
	}

	/* Set speed (bps) */
	ret = rswitch2_serdes_set_speed(eth_port, phy_speed);
	if (ret) {
		rsw2_err(MSG_SERDES, "set initial SERDES speed for port %d failed\n", port_num);
		return ret;
	}

#if 0
	/* The serdes connection to PHYs takes quite long */
	ret = rswitch2_serdes_reg_wait(phy_port->serdes_chan_addr, SR_XS_PCS_STS1, BANK_300, BIT(2), BIT(2));
	printk("%s:%d link-up ret=%d\n", __FUNCTION__, __LINE__, ret);
	if (ret) {
		//reset RX side and retry
		u32 value = rswitch2_serdes_read32(phy_port->serdes_chan_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180);
		rswitch2_serdes_write32(phy_port->serdes_chan_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, value | 0x10);
		udelay(10);
		rswitch2_serdes_write32(phy_port->serdes_chan_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, value);

		ret = rswitch2_serdes_reg_wait(phy_port->serdes_chan_addr, SR_XS_PCS_STS1, BANK_300, BIT(2), BIT(2));
		printk("%s:%d link-up ret=%d\n", __FUNCTION__, __LINE__, ret);
	}
	if (ret) {
		netdev_err(ndev, "SerDes on port %d has not reached link up\n", port_num);
		//return ret;
	}
	else
		dev_info(rsw2->dev, "SerDes on port %d connected.\n", port_num);
#endif

	//printk("Step 11,12\n");
	rswitch2_serdes_write32(phy_port->serdes_chan_addr, 0x03c0, BANK_380, 0);
	rswitch2_serdes_write32(phy_port->serdes_chan_addr, 0x03d0, BANK_380, 0);

	/* USXGMII needs deferred check to proof operation */
	if(check_op && (phy_port->phy_iface == PHY_INTERFACE_MODE_USXGMII)) {
		rsw2_info(MSG_SERDES, "Starting SerDes serdes_usxgmii_op_timer for '%s'\n", ndev->name);
		timer_setup(&phy_port->serdes_usxgmii_op_timer, rswitch2_serdes_check_operation, 0);
		mod_timer(&phy_port->serdes_usxgmii_op_timer, jiffies + msecs_to_jiffies(RSW2_SERDES_OP_TIMER_INTERVALL_FIRST));
	}

	return 0;
}

static void rswitch2_phy_port_update_stats(struct rswitch2_physical_port *phy_port) {

	u64 bytes_upper;
	u64 bytes_lower;

	phy_port->rx_pkt_cnt += ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MRGFCE);
	phy_port->rx_pkt_cnt += ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MRGFCP);

	bytes_upper = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MRXBCEU);
	bytes_lower = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MRXBCEL);

	phy_port->rx_byte_cnt += (bytes_upper << 32) | bytes_lower;


	phy_port->tx_pkt_cnt += ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MTGFCE);
	phy_port->tx_pkt_cnt += ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MTGFCP);

	bytes_upper = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MTXBCEU);
	bytes_lower = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MTXBCEL);

	phy_port->tx_byte_cnt += (bytes_upper << 32) | bytes_lower;

}

static void rswitch2_stat_timer(struct timer_list *t)
{
	struct rswitch2_eth_port *eth_port  = from_timer(eth_port, t, stat_timer);
	struct rswitch2_physical_port *phy_port = eth_port->phy_port;
	unsigned long flags;

	spin_lock_irqsave(&eth_port->rsw2->lock, flags);
	rswitch2_phy_port_update_stats(phy_port);

	mod_timer(&eth_port->stat_timer, jiffies + msecs_to_jiffies(RSW2_STAT_TIMER_INTERVALL));
	spin_unlock_irqrestore(&eth_port->rsw2->lock, flags);


}

/*static int rswitch2_mac_set_speed(struct rswitch2_eth_port *eth_port)
{
	u32 oldval, newval, mask;
	int ret;
	struct rswitch2_physical_port *phy_port;

	phy_port = eth_port->phy_port;

	//TODO code the correct speeds

	newval = 0;
	switch (phy_port->phy_iface) {
		case PHY_INTERFACE_MODE_SGMII :
			//speed may chage during link up. MII is fixed
			newval |= FIELD_PREP(MPIC_LSC, rsw2_rmac_1000mbps);
			newval |= FIELD_PREP(MPIC_PIS, rsw2_rmac_gmii);
			break;
		case PHY_INTERFACE_MODE_USXGMII :
			//speed may chage during link up. MII is fixed
			newval |= FIELD_PREP(MPIC_LSC, rsw2_rmac_2500mbps);
			newval |= FIELD_PREP(MPIC_PIS, rsw2_rmac_xgmii);
			break;
		default:
			netdev_err(eth_port->ndev, "Unsupported MAC xMII format %s (%d)\n",phy_modes(phy_port->phy_iface), phy_port->phy_iface );
			return -EINVAL;
	}

	oldval = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MPIC);
	mask = MPIC_LSC | MPIC_PIS;
	if ((oldval & mask) != (newval & mask)) {
		printk("old mode is %x\n", ioread32(eth_port->ndev->base_addr + RSW2_ETHA_EAMC));

		ret = rswitch2_emac_set_state(eth_port->ndev, emac_disable);
		if (ret < 0)
			return ret;
		ret = rswitch2_emac_set_state(eth_port->ndev, emac_config);
		if (ret < 0)
			return ret;

		newval |= oldval & ~mask;
		printk("[%x] = %x\n", (u32)(phy_port->rmac_base_addr), newval);
		iowrite32(newval, phy_port->rmac_base_addr + RSW2_RMAC_MPIC);

		oldval = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MPIC);
		netdev_info(eth_port->ndev, "Configure MAC to %x\n", oldval);

		ret = rswitch2_emac_set_state(eth_port->ndev, emac_disable);
		if (ret < 0)
			return ret;
		ret = rswitch2_emac_set_state(eth_port->ndev, emac_operation);
		if (ret < 0)
			return ret;
	}
	return 0;
}*/

static int rswitch2_eth_open(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct rswitch2_internal_port *intern_port;
	int ret;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;
	phy_port = eth_port->phy_port;
	intern_port = eth_port->intern_port;

	/* Gateway port */
	if (intern_port != NULL) {
		int cur_q;
		const uint 	num_of_rx_q = ARRAY_SIZE(intern_port->rx_q);

		ret = rswitch2_gwca_set_state(rsw2, gwmc_operation);
		if (ret != 0) {
			rsw2_err(MSG_GEN, "Failed to set GWCA operation state\n");
		}

		rsw2_notice(MSG_GEN, "internal port open(): '%s'\n", ndev->name);
		for (cur_q = 0; cur_q < num_of_rx_q; cur_q++) {
			u32 reg_queue = (cur_q + intern_port->rx_q[cur_q].offset) / 32;
			u32 bit_queue = (cur_q + intern_port->rx_q[cur_q].offset) % 32;


			/* Enable RX interrupt */
			iowrite32(BIT(bit_queue), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDIE(reg_queue));

			napi_enable(&intern_port->rx_q[cur_q].napi);
		}
		// FIXME:
		netif_tx_start_all_queues(ndev);
		//netif_start_queue(ndev);
	}
	else {
		int cur_q;
		const uint 	num_of_rx_q = ARRAY_SIZE(phy_port->rx_q);


		rsw2_notice(MSG_GEN, "physical port open(): '%s'\n", ndev->name);
		for (cur_q = 0; cur_q < num_of_rx_q; cur_q++) {
			napi_enable(&phy_port->rx_q[cur_q].napi);
		}

		netif_tx_start_all_queues(ndev);


		ret = rswitch2_serdes_init(ndev, false);
		if (ret != 0) {
			rsw2_err(MSG_SERDES, "%s: rswitch2_serdes_init failed: %d\n", ndev->name, ret);
			return ret;
		}
		else {
			rsw2_info(MSG_SERDES, "%s: rswitch2_serdes_init SUCCESS\n", ndev->name);
		}

		timer_setup(&eth_port->stat_timer, rswitch2_stat_timer, 0);
		mod_timer(&eth_port->stat_timer, jiffies + msecs_to_jiffies(RSW2_STAT_TIMER_INTERVALL));
		if (phy_port != NULL) {
			rsw2_info(MSG_SERDES, "physical port open(): '%s'\n", ndev->name);
			phy_start(ndev->phydev);

		}

		phy_attached_info(ndev->phydev);
	}
	return 0;
}

static inline struct rswitch2_eth_port *
rswitch2_netdev_get_rx_q(struct net_device *ndev, uint q, struct rsw2_rx_q_data **rx_q) {

	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	struct rswitch2_physical_port *phy_port;

	eth_port = netdev_priv(ndev);
	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;

	if(intern_port)
		*rx_q = &intern_port->rx_q[q];
	else
		*rx_q = &phy_port->rx_q[q];

	return eth_port;
}

static inline struct rswitch2_eth_port *
rswitch2_netdev_get_tx_q(struct net_device *ndev, uint q, struct rsw2_tx_q_data **tx_q) {

	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	struct rswitch2_physical_port *phy_port;

	eth_port = netdev_priv(ndev);
	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;

	if(intern_port)
		*tx_q = &intern_port->tx_q[q];
	else
		*tx_q = &phy_port->tx_q[q];

	return eth_port;
}


static void rswitch2_disable_rx(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	struct rswitch2_physical_port *phy_port;
	uint num_of_rx_queues;
	uint q;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;

	if(intern_port) {
		num_of_rx_queues = ARRAY_SIZE(intern_port->rx_q);
	} else {
		num_of_rx_queues = ARRAY_SIZE(phy_port->rx_q);
	}

	for (q = 0; q < num_of_rx_queues; q++) {
		u32 reg_queue;
		u32 bit_queue;
		struct rsw2_rx_q_data *rx_q;

		(void)rswitch2_netdev_get_rx_q(ndev, q, &rx_q);

		reg_queue = (q + rx_q->offset) / RSWITCH2_BITS_PER_REG;
		bit_queue = (q + rx_q->offset) % RSWITCH2_BITS_PER_REG;

		iowrite32(BIT(bit_queue), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDID(reg_queue));
		iowrite32(BIT(bit_queue), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDIS(reg_queue));

		napi_disable(&intern_port->rx_q[q].napi);
	}
}



static int rswitch2_tx_free(struct net_device *ndev, int q)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct rswitch2_internal_port *intern_port;
	struct rsw2_tx_q_data *tx_q;
	struct rswitch2_dma_ext_desc *tx_desc;
	int free_num = 0;
	int entry;
	u32 data_ptr = 0;
	unsigned int data_len = 0;


	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;
	if(intern_port) {
		tx_q = &intern_port->tx_q[q];
	} else {
		tx_q = &phy_port->tx_q[q];
	}

	while (tx_q->cur_desc - tx_q->dirty_desc > 0) {
		entry = tx_q->dirty_desc % tx_q->entries;
		tx_desc = &tx_q->desc_ring[entry];

		if (FIELD_GET(RSW2_DESC_DT, tx_desc->die_dt) != DT_FEMPTY)
			break;

		/* Descriptor type must be checked before all other reads */
		dma_rmb();


		/* Free the original skb */
		if (tx_q->skb[entry]) {
			bool txc = false;
			uint desc_data_len;

			/* FIXME: Avoid using last descriptor - Add TS pending to tx_q */
			txc = FIELD_GET(RSW2_DESC_INFO1_TXC, tx_desc->info1);

			desc_data_len = FIELD_GET(RSW2_DESC_DS, le16_to_cpu(tx_desc->info_ds));
			data_ptr = le32_to_cpu(tx_desc->dptrl - (tx_q->skb[entry]->len - desc_data_len));

			//printk("DMA unmap(): Q: %d    Entry %d   TX desc: 0x%px    0x%.8x  skb_len: %d   len: %d\n", (tx_q->offset + q), entry, tx_desc, data_ptr, tx_q->skb[entry]->len, desc_data_len);
			dma_unmap_single(ndev->dev.parent, data_ptr, tx_q->skb[entry]->len, DMA_TO_DEVICE);

			/* Timestamped TX skbs are free once the timestamp is fetched */
			if(!txc)
				dev_kfree_skb_any(tx_q->skb[entry]);

			tx_q->skb[entry] = NULL;
			data_ptr = 0;
			/* TODO: stats */
			//stats->tx_packets++;

			free_num++;
		} else {
			tx_q->skb[entry] = NULL;

			rsw2_dbg(MSG_GEN, "%s: Entry: %d Sum up length of multi frame: len=%u\n",
					 ndev->name, entry, data_len);
			free_num++;
		}

		/* TODO: update stats */
		//	stats->tx_bytes += size;

		tx_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_EEMPTY);
		tx_q->dirty_desc++;
		dma_wmb();
	}

	return free_num;
}


static int rswitch2_eth_close(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct rswitch2_internal_port *intern_port;
	int ret;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;
	phy_port = eth_port->phy_port;
	intern_port = eth_port->intern_port;

	if (intern_port != NULL) {
		uint cur_q;
		const uint num_of_tx_q = ARRAY_SIZE(intern_port->tx_q);

		rsw2_info(MSG_GEN, "internal port close(): '%s'\n", ndev->name);

		netif_tx_stop_all_queues(ndev);

		rswitch2_disable_rx(ndev);

		ret = rswitch2_gwca_set_state(rsw2, gwmc_disable);
		if (ret != 0) {
			rsw2_err(MSG_GEN,  "Failed to set GWCA disable state\n");
		}

		for (cur_q = 0; cur_q < num_of_tx_q; cur_q++) {
			rswitch2_tx_free(ndev, cur_q);
		}

	} else if (phy_port != NULL) {
		uint cur_q;
		const uint num_of_rx_q = ARRAY_SIZE(phy_port->rx_q);
		const uint num_of_tx_q = ARRAY_SIZE(phy_port->tx_q);

		rsw2_info(MSG_GEN,  "physical port close(): '%s'\n", ndev->name);

		del_timer(&eth_port->stat_timer);
		del_timer(&phy_port->serdes_usxgmii_op_timer);

		netif_carrier_off(ndev);
		netif_tx_stop_all_queues(ndev);

		for (cur_q = 0; cur_q < num_of_rx_q; cur_q++) {
			napi_disable(&phy_port->rx_q[cur_q].napi);
		}
		for (cur_q = 0; cur_q < num_of_tx_q; cur_q++) {
			rswitch2_tx_free(ndev, cur_q);
		}
		phy_stop(ndev->phydev);

	}

	return 0;
}


/* Packet receive function for RSwitch2 */
static bool rswitch2_rx(struct net_device *ndev, int budget, int q)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct rswitch2_internal_port *intern_port;
	struct rsw2_rx_q_data *rx_q;
#ifdef RSW2_RX_TS_DESC
	struct rswitch2_dma_ext_ts_desc *rx_desc;
#else
	struct rswitch2_dma_ext_desc *rx_desc;
#endif
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	int entry;
	u16 pkt_len;
	unsigned int count = 0;

	/* TODO: Move to get_rx_q_from_nedv() */
	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;
	if(intern_port) {
		rx_q = &intern_port->rx_q[q];
	} else {
		rx_q = &phy_port->rx_q[q];
	}
	rsw2 = eth_port->rsw2;


	while (count < budget) {
		struct skb_shared_hwtstamps *shhwtstamps;
		struct timespec64 ts;

		entry = rx_q->cur_desc % rx_q->entries;
		rx_desc = &rx_q->desc_ring[entry];
		//printk("Abs. Q: %d: RX desc. entry %d of %ld\n", q + rx_q->offset, entry, rx_q->entries);

		if(FIELD_GET(RSW2_DESC_DT, rx_desc->die_dt) == DT_FEMPTY)
			break;

		/* Descriptor type must be checked before all other reads */
		dma_rmb();

		count++;

		pkt_len = FIELD_GET(RSW2_DESC_DS, le16_to_cpu(rx_desc->info_ds));

		//printk("RX: pkt_len: %d DMA: 0x%.8x\n", pkt_len, rx_desc->dptrl);
		skb = rx_q->skb[entry];
		rx_q->skb[entry] = NULL;
		dma_unmap_single(ndev->dev.parent, le32_to_cpu(rx_desc->dptrl),
						RSW2_PKT_BUF_SZ, DMA_FROM_DEVICE);

		shhwtstamps = skb_hwtstamps(skb);
		memset(shhwtstamps, 0, sizeof(*shhwtstamps));
		ts.tv_sec = (u64)le32_to_cpu(rx_desc->ts_sec);
		ts.tv_nsec = le32_to_cpu(rx_desc->ts_nsec & 0x3FFFFFFF);
		shhwtstamps->hwtstamp = timespec64_to_ktime(ts);

		skb_put(skb, pkt_len);
		skb->protocol = eth_type_trans(skb, ndev);
		napi_gro_receive(&rx_q->napi, skb);

		// TODO
		if(intern_port) {
			intern_port->rx_pkt_cnt++;
			intern_port->rx_byte_cnt += pkt_len;
		}
		rx_q->cur_desc++;
	}

	/* Refill the RX ring buffers. */
	for (; rx_q->cur_desc - rx_q->dirty_desc > 0; rx_q->dirty_desc++) {
		entry = rx_q->dirty_desc % rx_q->entries;

		rsw2_dbg(MSG_RXTX, "RX Refill Q: %d: RX desc. entry %d of %ld\n", q + rx_q->offset, entry, rx_q->entries);

		rx_desc = &rx_q->desc_ring[entry];
		rx_desc->info_ds = cpu_to_le16(RSW2_PKT_BUF_SZ);
		rx_desc->info1 = 0;

		if (!rx_q->skb[entry]) {
			skb = dev_alloc_skb(RSW2_PKT_BUF_SZ + RSW2_BUF_ALIGN - 1);
			if (!skb)
				break;

			skb_reserve(skb, NET_IP_ALIGN);

			dma_addr = dma_map_single(rsw2->dev, skb->data,
						  RSW2_PKT_BUF_SZ, DMA_FROM_DEVICE);

			/* We just set the data size to 0 for a failed mapping which
			 * should prevent DMA from happening...
			 */
			if (dma_mapping_error(rsw2->dev, dma_addr)) {
				rsw2_err(MSG_RXTX, "Descriptor Mapping error\n");
				rx_desc->info_ds = cpu_to_le16(0);
			}
			rx_desc->dptrl = cpu_to_le32(dma_addr);
			skb_checksum_none_assert(skb);
			rx_q->skb[entry] = skb;
		} else {
			rsw2_warn(MSG_RXTX, "%s: SKB already set: rx_desc->dptrl=0x%.8x\n",
					ndev->name, rx_desc->dptrl);
		}
		/* Descriptor type must be set after all the above writes */
		dma_wmb();
		rx_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_FEMPTY) | RSW2_DESC_DIE;
	}

	return count;
}


static int rswitch2_poll(struct napi_struct *napi, int budget)
{
	struct net_device *ndev = napi->dev;

	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct rswitch2_internal_port *intern_port;
	struct rsw2_rx_q_data *rx_q;
	unsigned long flags;
	int q;
	u32 reg_queue;
	u32 bit_queue;
	int work_done = 0;
	bool rearm_irq;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	rx_q = container_of(napi, struct rsw2_rx_q_data, napi);

	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;
	if(intern_port) {
		q = (rx_q - &intern_port->rx_q[0]);
	} else {
		q = (rx_q - &phy_port->rx_q[0]);
	}

	reg_queue = (q + rx_q->offset) / 32;
	bit_queue = (q + rx_q->offset) % 32;

	rsw2_dbg(MSG_RXTX, "RX poll. q: %d abs_q: %d   budget: %d reg_queue: %d bit_queue: %d\n", q, (q + rx_q->offset), budget, reg_queue, bit_queue);

	work_done = rswitch2_rx(ndev, budget, q);
	rearm_irq = napi_complete_done(napi, work_done);
	if(rearm_irq) {
		/* Re-enable RX interrupts*/
		spin_lock_irqsave(&rsw2->lock, flags);
		iowrite32((1 << bit_queue), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDIS(reg_queue));
		iowrite32((1 << bit_queue), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDIE(reg_queue));
		spin_unlock_irqrestore(&rsw2->lock, flags);
	}
	else {
		/* FIXME: Remove this. Just left for debugging purpose */
		rsw2_notice(MSG_RXTX, "NAPI says: don't rearm\n");
	}

	return work_done;
}


static netdev_tx_t rswitch2_eth_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rsw2_tx_q_data *tx_q;
	struct rswitch2_dma_ext_desc *tx_desc;
	u32 reg_val;
	u32 reg_queue;
	u32 bit_queue;
	u32 dma_addr;
	u32 entry;
	u16 q;
	unsigned long flags;
	char *data_ptr;
	unsigned int data_len;


	/* Get current q */
	q = skb_get_queue_mapping(skb);

	eth_port = rswitch2_netdev_get_tx_q(ndev, q, &tx_q);
	rsw2 = eth_port->rsw2;
	rsw2_dbg(MSG_RXTX, "start_xmit() on abs Q: %d  Q: %d\n", (q + tx_q->offset), q);

	/* Free unused descriptors */
	rswitch2_tx_free(ndev, q);

	if (skb_put_padto(skb, ETH_ZLEN))
		goto exit;

	// TODO: Handle more data if there are more descriptors
	// while (netdev_xmit_more() && descr free)

	dma_addr = dma_map_single(rsw2->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(rsw2->dev, dma_addr)) {
		rsw2_err(MSG_RXTX, "DMA mapping failed\n");
		goto unmap;
	}

	data_ptr = skb->data;
	data_len = skb->len;

	if (data_len <= RSWITCH2_MAX_DESC_SIZE) {
		entry = tx_q->cur_desc % tx_q->entries;

		/* FIXME: atomic  ?? */
		tx_q->cur_desc++;
		tx_desc = &tx_q->desc_ring[entry];
		rsw2_dbg(MSG_RXTX, "Using TX desc. 0x%px\n", tx_desc);


		//tx_desc->info_ds = FIELD_PREP(RSW2_DESC_DS, cpu_to_le16(data_len));
		tx_desc->info_ds = cpu_to_le16(data_len);
		tx_desc->dptrl = cpu_to_le32(dma_addr);
		tx_desc->info1 = 0;

		if (eth_port->phy_port) {
			uint ts_tag_entry = eth_port->phy_port->ts_tag % MAX_TS_Q_ENTRIES_PER_PORT;
			uint port_num = eth_port->port_num - rsw2->num_of_cpu_ports;

			if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
				rsw2_dbg(MSG_RXTX, "HW TS xmit(): Tag-Entry: %d port_num: %d skb: 0x%px\n", ts_tag_entry, port_num, skb);
				skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;


				tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_TXC, 1);
				tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_TSUN, ts_tag_entry);

				eth_port->phy_port->ts_skb[ts_tag_entry] = skb;
				eth_port->phy_port->ts_tag++;

			}
			tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_FMT, direct_desc);
			tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_DV, 1 << port_num);

			skb_tx_timestamp(skb);
		}


		/* HW won't process descriptor until type is set,
		 * ensure all other items have been written
		 */
		dma_wmb();

		tx_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_FSINGLE);
	} else {
		struct rswitch2_dma_ext_desc *tx_start_desc;
		do {
			u8 dt_type;

			entry = tx_q->cur_desc % tx_q->entries;

			// FIXME: atomic  ??
			tx_q->cur_desc++;
			tx_desc = &tx_q->desc_ring[entry];
			tx_q->skb[entry] = NULL;

			rsw2_dbg(MSG_RXTX, "Using entry: %d    TX desc. 0x%px\n", entry, tx_desc);
			tx_desc->dptrl = cpu_to_le32(dma_addr + (skb->len - data_len));
			tx_desc->info1 = 0;
			rsw2_dbg(MSG_RXTX, "data_len: %d  skb_len: %d\n", data_len, skb->len);

			if (data_len == skb->len) {
				rsw2_dbg(MSG_RXTX, "FSTART\n");
				tx_start_desc = tx_desc;
				dt_type = FIELD_PREP(RSW2_DESC_DT, DT_FSTART);
				tx_desc->info_ds = cpu_to_le16(RSWITCH2_MAX_DESC_SIZE);
				data_len -= RSWITCH2_MAX_DESC_SIZE;
				if (eth_port->phy_port) {
					uint ts_tag_entry = eth_port->phy_port->ts_tag % MAX_TS_Q_ENTRIES_PER_PORT;

					if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
						rsw2_dbg(MSG_RXTX, "HW TS xmit() MULTI: Tag-Entry: %d port_num: %d skb: 0x%px\n", ts_tag_entry, (eth_port->port_num - rsw2->num_of_cpu_ports), skb);

						skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
						tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_TXC, 1);
						tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_TSUN, ts_tag_entry);
						eth_port->phy_port->ts_skb[ts_tag_entry] = skb;
						eth_port->phy_port->ts_tag++;
					}
					skb_tx_timestamp(skb);
				}
			} else if (data_len <= RSWITCH2_MAX_DESC_SIZE) {
				rsw2_dbg(MSG_RXTX, "FEND\n");
				rsw2_dbg(MSG_RXTX, "Multi TX: Q: %d Entry: %d    TX desc. 0x%px  DMA addr: 0x%.8x  skb_len: %d\n", (q + tx_q->offset), entry, tx_desc, dma_addr, skb->len);

				dt_type = FIELD_PREP(RSW2_DESC_DT, DT_FEND);

				/* FIXME: Hack to pass information to tx_free() not to free skb */
				tx_desc->info_ds = cpu_to_le16(data_len);
				tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_TXC, 1);
				tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_TSUN, 0);
				data_len = 0;
			} else {
				rsw2_dbg(MSG_RXTX, "FMID\n");
				dt_type = FIELD_PREP(RSW2_DESC_DT, DT_FMID);
				tx_desc->info_ds = cpu_to_le16(RSWITCH2_MAX_DESC_SIZE);
				data_len -= RSWITCH2_MAX_DESC_SIZE;
				tx_q->skb[entry] = NULL;

			}

			if (eth_port->phy_port) {
				uint port_num = eth_port->port_num - rsw2->num_of_cpu_ports;

				tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_FMT, direct_desc);
				tx_desc->info1 |= FIELD_PREP(RSW2_DESC_INFO1_DV, 1 << port_num);
			}
			if(FIELD_GET(RSW2_DESC_DT, dt_type) != DT_FSTART) {
				//printk("dt_type: %d\n", dt_type);
			tx_desc->die_dt = dt_type;
			}
		} while (data_len > 0);
		dma_wmb();
		tx_start_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_FSTART);

	}

	/* On multi descriptor transmit, the last entry holds the skb data */
	tx_q->skb[entry] = skb;

	/* Get register offset and bit offset for the used queue */
	reg_queue = (q + tx_q->offset) / 32;
	bit_queue = (q + tx_q->offset) % 32;

	rsw2_dbg(MSG_RXTX, "Write kick to RSW2_GCWA_GWTRC(%d) bit %d\n", reg_queue, bit_queue);
	spin_lock_irqsave(&rsw2->lock, flags);
	reg_val = GWTRC_TSR(bit_queue);
	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTRC(reg_queue));
	spin_unlock_irqrestore(&rsw2->lock, flags);

	/* TODO:: Make generic eth_port counter and use RMAC counters on phy_port */
	if(eth_port->intern_port) {
		eth_port->intern_port->tx_pkt_cnt++;
		eth_port->intern_port->tx_byte_cnt += skb->len;
	}
unmap:

exit:
	return NETDEV_TX_OK;
}

static void rswitch2_get_ts(struct rswitch2_drv *rsw2) {
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct rswitch2_dma_ts_desc *ts_desc;
	struct skb_shared_hwtstamps shhwtstamps;
	struct timespec64 ts;
	uint ring_entries;
	u16 tsun;
	uint src_port_num;
	uint dest_port_num;
	uint entry;


	ring_entries = rsw2->num_of_tsn_ports * MAX_TS_Q_ENTRIES_PER_PORT;
	entry = (rsw2->ts_cur_desc) % ring_entries;
	ts_desc = &rsw2->ts_desc_ring[entry];

	while (FIELD_GET(RSW2_DESC_DT, ts_desc->die_dt) != DT_FEMPTY_ND) {
		ts.tv_nsec = ts_desc->ts_nsec & 0x3FFFFFFF;
		ts.tv_sec = ts_desc->ts_sec;
		tsun = ((ts_desc->tsun) & 0xFF);

		src_port_num = ts_desc->src_port_num;
		dest_port_num = ts_desc->dest_port_num;

		memset(&shhwtstamps, 0, sizeof(shhwtstamps));
		shhwtstamps.hwtstamp = timespec64_to_ktime(ts);

		eth_port = rsw2->ports[dest_port_num + rsw2->num_of_cpu_ports];
		phy_port = eth_port->phy_port;
		BUG_ON(!phy_port);

		rsw2_dbg(MSG_RXTX, "Got TS for skb: 0x%px\n", phy_port->ts_skb[tsun]);
		rsw2_dbg(MSG_RXTX, "Entry %d TX timestamp (%.lld:%.ld) for src port %d, dest port %d, tag; %d\n", entry, ts.tv_sec, ts.tv_nsec, src_port_num, dest_port_num, tsun);

		skb_tstamp_tx(phy_port->ts_skb[tsun], &shhwtstamps);

		dev_kfree_skb_any(phy_port->ts_skb[tsun]);
		phy_port->ts_skb[tsun] = NULL;

		ts_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_FEMPTY_ND);
		ts_desc->die_dt |= FIELD_PREP(RSW2_DESC_DIE, 1);
		dma_wmb();

		rsw2->ts_cur_desc++;
		entry = (rsw2->ts_cur_desc) % ring_entries;
		ts_desc = &rsw2->ts_desc_ring[entry];
	}

}

static irqreturn_t rswitch2_status_interrupt(int irq, void *dev_id)
{
	struct rswitch2_drv *rsw2 = dev_id;
	u32 reg_val;
	irqreturn_t ret = IRQ_HANDLED;
	unsigned long flags;

	/* Read Timestamp IRQ status GWTSDIS */
	spin_lock_irqsave(&rsw2->lock, flags);
	reg_val = ioread32(rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTSDIS);

	rsw2_dbg(MSG_GEN, "GWTSDIS: 0x%.8x\n", reg_val);
	if((reg_val & BIT(0)) == BIT(0)) {
		/* Mask IRQ */
		iowrite32(BIT(0), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTSDID);
		spin_unlock_irqrestore(&rsw2->lock, flags);

		rswitch2_get_ts(rsw2);

		spin_lock_irqsave(&rsw2->lock, flags);
		/* Ack IRQ */
		iowrite32(BIT(0), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTSDIS);
		/* Re-enable IRQ */
		iowrite32(BIT(0), rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTSDIE);
	}
	spin_unlock_irqrestore(&rsw2->lock, flags);

	return ret;
}

static irqreturn_t rswitch2_eth_interrupt(int irq, void *dev_id)
{
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_drv *rsw2 = dev_id;
	struct napi_struct *cur_napi;
	irqreturn_t ret = IRQ_HANDLED;
	u32 reg_val;
	u32 reg_num;
	u32 reg_bit;
	u32 cur_queue;
	u32 irq_status[RSWITCH2_CHAIN_REG_NUM];
	u32 irq_active[RSWITCH2_CHAIN_REG_NUM];
	unsigned long flags;

	spin_lock_irqsave(&rsw2->lock, flags);
	for (reg_num = 0; reg_num < RSWITCH2_CHAIN_REG_NUM; reg_num++) {
		irq_status[reg_num] = ioread32(rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDIS(reg_num));
		irq_active[reg_num] = ioread32(rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDIE(reg_num));
		iowrite32(irq_status[reg_num], rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDID(reg_num));
	}
	spin_unlock_irqrestore(&rsw2->lock, flags);


	for (reg_num = 0; reg_num < RSWITCH2_CHAIN_REG_NUM; reg_num++) {
		reg_val = irq_status[reg_num] & irq_active[reg_num];

		for (reg_bit = 0; reg_bit < RSWITCH2_BITS_PER_REG; reg_bit++) {
			if ((BIT(reg_bit) & reg_val) == BIT(reg_bit)) {
				uint port_num;
				uint port_q;
				cur_queue = (reg_num * RSWITCH2_BITS_PER_REG) + reg_bit;

				port_num = rsw2->port_backref[cur_queue].port_num;
				eth_port = rsw2->ports[port_num];

				port_q = rsw2->port_backref[cur_queue].port_q;
				if(eth_port->intern_port)
					cur_napi = &eth_port->intern_port->rx_q[port_q].napi;
				else
					cur_napi = &eth_port->phy_port->rx_q[port_q].napi;

				if (napi_schedule_prep(cur_napi))
					__napi_schedule(cur_napi);
				else
					/* Although this is no real problem, it shouldn't happen.
					 * It wastes CPU time with unnecessary IRQ handling
					 */
					rsw2_warn(MSG_RXTX, "NAPI is already running Q: %d\n", cur_queue);
			}
		}
	}

	return ret;
}






static u16 rswitch2_eth_select_queue(struct net_device *ndev, struct sk_buff *skb,
							struct net_device *sb_dev)
{
	/* TODO: Select queue as needed. E.g. TX timestamp, it is handled by NC queue */
	return 0;
}

static struct net_device_stats *rswitch2_eth_get_stats(struct net_device *ndev)
{
	struct net_device_stats *nstats;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	struct rswitch2_physical_port *phy_port;
	unsigned long flags;

	nstats = &ndev->stats;

	eth_port = netdev_priv(ndev);
	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;

	spin_lock_irqsave(&eth_port->rsw2->lock, flags);

	/* Only internal ports can TX */
	if(intern_port) {
		nstats->rx_packets = intern_port->rx_pkt_cnt;
		nstats->tx_packets = intern_port->tx_pkt_cnt;
		nstats->rx_bytes = intern_port->rx_byte_cnt;
		nstats->tx_bytes = intern_port->tx_byte_cnt;
		nstats->multicast = 0;
		nstats->rx_errors = 0;
		nstats->rx_crc_errors = 0;
		nstats->rx_frame_errors = 0;
		nstats->rx_length_errors = 0;
		nstats->rx_missed_errors = 0;
		nstats->rx_over_errors = 0;

	} else if(phy_port) {
		rswitch2_phy_port_update_stats(phy_port);
		nstats->rx_packets = phy_port->rx_pkt_cnt;
		nstats->tx_packets = phy_port->tx_pkt_cnt;
		nstats->rx_bytes = phy_port->rx_byte_cnt;
		nstats->tx_bytes = phy_port->tx_byte_cnt;
		nstats->multicast = 0;
		nstats->rx_errors = 0;
		nstats->rx_crc_errors = 0;
		nstats->rx_frame_errors = 0;
		nstats->rx_length_errors = 0;
		nstats->rx_missed_errors = 0;
		nstats->rx_over_errors = 0;

	}
	spin_unlock_irqrestore(&eth_port->rsw2->lock, flags);

	return nstats;
}

/* TODO: Update promiscuous bit */
static void rswitch2_eth_set_rx_mode(struct net_device *ndev)
{
}

/* TODO: Reset on TX Timeout */
static void rswitch2_eth_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
}

static int rswitch2_hwstamp_get(struct net_device *ndev, struct ifreq *req)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	struct rswitch2_physical_port *phy_port;
	struct rtsn_ptp_private *ptp_priv;
	struct hwtstamp_config config;

	eth_port = netdev_priv(ndev);
	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;


	if(intern_port) {
		rsw2_err(MSG_GEN, "HW get intern port!?\n");
		return -EINVAL;
	}
	rsw2 = eth_port->rsw2;
	ptp_priv = rsw2->ptp_drv;


	config.flags = 0;
	config.tx_type = ptp_priv->tstamp_tx_ctrl ? HWTSTAMP_TX_ON :
						    HWTSTAMP_TX_OFF;
	switch (ptp_priv->tstamp_rx_ctrl & RTSN_RXTSTAMP_TYPE) {
	case RTSN_RXTSTAMP_TYPE_V2_L2_EVENT:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
		break;
	case RTSN_RXTSTAMP_TYPE_ALL:
		config.rx_filter = HWTSTAMP_FILTER_ALL;
		break;
	default:
		config.rx_filter = HWTSTAMP_FILTER_NONE;
		break;
	}

	return copy_to_user(req->ifr_data, &config, sizeof(config)) ? -EFAULT : 0;
}

static int rswitch2_hwstamp_set(struct net_device *ndev, struct ifreq *req)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	struct rswitch2_physical_port *phy_port;
	struct rtsn_ptp_private *ptp_priv;
	struct hwtstamp_config config;
	u32 tstamp_rx_ctrl = RTSN_RXTSTAMP_ENABLED;
	u32 tstamp_tx_ctrl;


	eth_port = netdev_priv(ndev);
	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;

	if(intern_port) {
		rsw2_err(MSG_GEN, "HW set intern port!?\n");
		return -EINVAL;
	}

	rsw2 = eth_port->rsw2;
	ptp_priv = rsw2->ptp_drv;

	if (copy_from_user(&config, req->ifr_data, sizeof(config)))
		return -EFAULT;

	if (config.flags)
		return -EINVAL;

	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
		tstamp_tx_ctrl = 0;
		break;
	case HWTSTAMP_TX_ON:
		tstamp_tx_ctrl = RTSN_TXTSTAMP_ENABLED;
		break;
	default:
		return -ERANGE;
	}

	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		tstamp_rx_ctrl = 0;
		break;
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
		tstamp_rx_ctrl |= RTSN_RXTSTAMP_TYPE_V2_L2_EVENT;
		break;
	default:
		config.rx_filter = HWTSTAMP_FILTER_ALL;
		tstamp_rx_ctrl |= RTSN_RXTSTAMP_TYPE_ALL;
		break;
	}

	ptp_priv->tstamp_tx_ctrl = tstamp_tx_ctrl;
	ptp_priv->tstamp_rx_ctrl = tstamp_rx_ctrl;

	return copy_to_user(req->ifr_data, &config, sizeof(config)) ? -EFAULT : 0;
}


static int rswitch2_eth_do_ioctl(struct net_device *ndev, struct ifreq *req, int cmd)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct phy_device *phydev = ndev->phydev;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	if (!netif_running(ndev)) {
		rsw2_warn(MSG_GEN, "netif not running\n");

		return -EINVAL;
	}

	if (!phydev) {
		rsw2_warn(MSG_GEN, "no phydev\n");
		return -ENODEV;
	}
	switch (cmd) {
	case SIOCGHWTSTAMP:
		/* HW get timestamp */
		return rswitch2_hwstamp_get(ndev, req);
		break;
	case SIOCSHWTSTAMP:
		/* HW set timestamp */
		return rswitch2_hwstamp_set(ndev, req);
		break;
	}

	return phy_mii_ioctl(phydev, req, cmd);
}

static int rswitch2_eth_change_mtu(struct net_device *ndev, int new_mtu)
{
	if (netif_running(ndev))
		return -EBUSY;

	ndev->mtu = new_mtu;
	netdev_update_features(ndev);

	return 0;
}


static void rswitch2_port_set_mac_addr(struct net_device *port_ndev)
{
	u32 reg_val;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;

	eth_port = netdev_priv(port_ndev);
	phy_port = eth_port->phy_port;

	reg_val = FIELD_PREP(MRMAC0_MAUP,
			((port_ndev->dev_addr[0] & 0xff) << 8) | port_ndev->dev_addr[1]);
	iowrite32(reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MRMAC0);

	reg_val = ((port_ndev->dev_addr[2] & 0xff) << 24);
	reg_val |= ((port_ndev->dev_addr[3] & 0xff) << 16);
	reg_val |= ((port_ndev->dev_addr[4] & 0xff) << 8) | port_ndev->dev_addr[5];
	iowrite32(reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MRMAC1);
}

/* TODO: Currently unused. Maybe used if MAC is set by bootloader */
#if 0
static void rswitch2_port_get_mac_addr(struct net_device *port_ndev)
{
	u32 reg_val;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;

	eth_port = netdev_priv(port_ndev);
	phy_port = eth_port->phy_port;

	reg_val = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MRMAC0);
	port_ndev->dev_addr[1] = FIELD_GET(MRMAC0_MAUP, reg_val) & 0xff;
	port_ndev->dev_addr[0] = (FIELD_GET(MRMAC0_MAUP, reg_val) << 8) & 0xff;

	reg_val = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MRMAC1);
	port_ndev->dev_addr[5] = (reg_val & 0xff);
	port_ndev->dev_addr[4] = ((reg_val << 8) & 0xff);
	port_ndev->dev_addr[3] = ((reg_val << 26) & 0xff);
	port_ndev->dev_addr[2] = 8(reg_val << 24) & 0xff);

}
#endif


static void rswitch2_intern_set_mac_addr(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	int num_of_ports;
	u32 reg_val;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	num_of_ports = (rsw2->num_of_cpu_ports + rsw2->num_of_tsn_ports);
	rsw2_fwd_add_l2_entry(rsw2, ndev->dev_addr,  ((1 << num_of_ports) - 1),  ((1 << num_of_ports) - 1), rsw2_be_rx_q_0);

	reg_val = FIELD_PREP(GWMAC0_MAUP,
			((ndev->dev_addr[0] & 0xff) << 8) | ndev->dev_addr[1]);
	iowrite32(reg_val, ((volatile void __iomem *)ndev->base_addr) + RSW2_GCWA_GWMAC0);

	reg_val = ((ndev->dev_addr[2] & 0xff) << 24);
	reg_val |= ((ndev->dev_addr[3] & 0xff) << 16);
	reg_val |= ((ndev->dev_addr[4] & 0xff) << 8) | ndev->dev_addr[5];
	iowrite32(reg_val, ((volatile void __iomem *)ndev->base_addr) + RSW2_GCWA_GWMAC1);
}


static int rswitch2_eth_mac_addr(struct net_device *ndev, void *p)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct sockaddr *addr = p;
	u8 old_macaddr[ETH_ALEN];
	u8 *new_macaddr;


	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	if (netif_running(ndev))
		return -EBUSY;

	new_macaddr = (u8 *)addr->sa_data;
	memcpy(old_macaddr, ndev->dev_addr, ETH_ALEN);

	rsw2_dbg(MSG_GEN, "Update MAC for '%s' from %.2X:%.2X:%.2X:%.2X:%.2X:%.2X "
							                 "to %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n",
			ndev->name,
			old_macaddr[0], old_macaddr[1],
			old_macaddr[2], old_macaddr[3],
			old_macaddr[4], old_macaddr[5],
			new_macaddr[0], new_macaddr[1],
			new_macaddr[2], new_macaddr[3],
			new_macaddr[4], new_macaddr[5]
	);

	memcpy(ndev->dev_addr, addr->sa_data, ETH_ALEN);


	if(eth_port->intern_port) {
		rswitch2_intern_set_mac_addr(ndev);
	} else {
		/* Update fwd engine */
		rswitch2_port_set_mac_addr(ndev);
	}
	rsw2_fwd_del_l2_entry(rsw2, old_macaddr);


	return 0;
}

static const struct net_device_ops rswitch2_netdev_ops = {
	.ndo_open				= rswitch2_eth_open,
	.ndo_stop				= rswitch2_eth_close,
	.ndo_start_xmit			= rswitch2_eth_start_xmit,
	.ndo_select_queue		= rswitch2_eth_select_queue,
	.ndo_get_stats			= rswitch2_eth_get_stats,
	.ndo_set_rx_mode		= rswitch2_eth_set_rx_mode,
	.ndo_tx_timeout			= rswitch2_eth_tx_timeout,
	.ndo_do_ioctl			= rswitch2_eth_do_ioctl,
	.ndo_change_mtu			= rswitch2_eth_change_mtu,
	.ndo_validate_addr		= eth_validate_addr,
	.ndo_set_mac_address	= rswitch2_eth_mac_addr,
};


/* Allocate TS descriptor base address table */
static int rswitch2_ts_ring_init(struct rswitch2_drv *rsw2)
{
	struct rswitch2_dma_desc *bat_entry;
	struct rswitch2_dma_ts_desc *ts_desc;
	struct rswitch2_dma_ext_desc *dma_desc;
	size_t ts_ring_size;
	uint ring_entries;
	int i;
	u32 reg_val;
	int ret;


	rsw2->ts_cur_desc = 0;
	rsw2->ts_dirty_desc = 0;


	/* Create BAT entry for TS descriptors */
	bat_entry = dma_alloc_coherent(rsw2->dev, sizeof(*bat_entry), &rsw2->bat_ts_dma_addr, GFP_KERNEL);
	if (!bat_entry) {
		ret = -ENOMEM;
		goto bat_alloc_err;
	}
	rsw2->bat_ts_addr = bat_entry;

	rsw2_info(MSG_RXTX, "BAT TS: BAT entry is at 0x%px DMA: (DMA: 0x%llx)\n", bat_entry, rsw2->bat_ts_dma_addr);


	ring_entries = rsw2->num_of_tsn_ports * MAX_TS_Q_ENTRIES_PER_PORT;
	ts_ring_size = sizeof(*ts_desc) * (ring_entries + 1);

	rsw2_dbg(MSG_RXTX, "rswitch2_ts_ring_init(): ring_size: %ld\n", ts_ring_size);

	rsw2->ts_desc_ring = dma_alloc_coherent(rsw2->dev, ts_ring_size,
											&rsw2->ts_desc_dma, GFP_KERNEL);

	if (!rsw2->ts_desc_ring) {
		rsw2_err(MSG_RXTX, "Failed to alloc memory for TS descriptor ring\n");
		ret = -ENOMEM;
		goto dma_alloc_err;
	}

	memset(rsw2->ts_desc_ring, 0, ts_ring_size);

	/* Init descriptors  */
	for (i = 0, ts_desc = rsw2->ts_desc_ring; i < ring_entries; i++, ts_desc++) {
		ts_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_FEMPTY_ND);
		ts_desc->die_dt |= FIELD_PREP(RSW2_DESC_DIE, 1);
	}

	rsw2_dbg(MSG_RXTX, "TS Ring is at 0x%px (DMA: 0x%llx)\n", rsw2->ts_desc_ring, rsw2->ts_desc_dma);

	/* Close the loop */
	dma_desc = (struct rswitch2_dma_ext_desc *)ts_desc;
	dma_desc->dptrl = cpu_to_le32((u32)rsw2->ts_desc_dma);
	dma_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_LINKFIX); /* type */

	/* Enlist ring to BAT */
	bat_entry->die_dt = DT_LINKFIX; /* type */
	bat_entry->dptrl = cpu_to_le32((u32)rsw2->ts_desc_dma);

	dma_wmb();

	iowrite32(rsw2->bat_ts_dma_addr, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTDCAC1(0));
	iowrite32(0, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTDCAC0(0));

	reg_val = FIELD_PREP(GWTSDCC_TE, 1);
	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTSDCC(0));

	/* Enable timestamp interrupt */
	reg_val = FIELD_PREP(GWTSDIE_TSDIE, 1);
	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWTSDIE);

	return 0;

dma_alloc_err:
	dma_free_coherent(rsw2->dev, sizeof(*bat_entry), rsw2->bat_ts_addr, rsw2->bat_ts_dma_addr);

bat_alloc_err:

	return ret;
}



/* Allocate descriptor base address table */
static int rswitch2_bat_init(struct rswitch2_drv *rsw2)
{
	int ret;
	struct rswitch2_dma_desc *bat_entry;
	size_t bat_size;
	int i;

	bat_size = sizeof(*bat_entry) * BAT_ENTRIES;

	rsw2->bat_addr = kzalloc(sizeof(*rsw2->bat_addr) * BAT_ENTRIES, GFP_KERNEL);
	if (!rsw2->bat_addr) {
		ret = -ENOMEM;
		goto bat_ptr_alloc_err;
	}
	bat_entry = dma_alloc_coherent(rsw2->dev, bat_size, &rsw2->bat_dma_addr, GFP_KERNEL);
	if (!bat_entry) {
		ret = -ENOMEM;
		goto dma_alloc_err;
	}
	rsw2_info(MSG_RXTX, "BAT is at 0x%px (DMA: 0x%llx)\n", rsw2->bat_addr, rsw2->bat_dma_addr);

	/* Init BE RX BAT */
	for (i = rsw2_be_rx_q_0; i < rsw2_be_rx_q_max_entry; i++) {
		//pr_info("Creating BAT entry for queue %d (RX queue %d)\n", i, (i - RSW2_BE_RX_Q_OFFSET));
		bat_entry[i].die_dt = FIELD_PREP(RSW2_DESC_DT, DT_EOS);
		rsw2->bat_addr[i] = &bat_entry[i];
	}

	/* Init LL RX BAT */
	for (i = rsw2_ll_rx_q_port0; i < rsw2_ll_rx_q_max_entry; i++) {
		//pr_info("Creating BAT entry for queue %d (RX queue %d)\n", i, (i - RSW2_LL_RX_Q_OFFSET));
		bat_entry[i].die_dt = FIELD_PREP(RSW2_DESC_DT, DT_EOS);
		rsw2->bat_addr[i] = &bat_entry[i];
	}


	for (i = rsw2_be_tx_q_0; i < rsw2_be_tx_q_max_entry; i++) {
		//pr_info("Creating BAT entry for queue %d (TX queue %d)\n", i, (i - RSW2_BE_TX_Q_OFFSET));
		bat_entry[i].die_dt = FIELD_PREP(RSW2_DESC_DT, DT_EOS);
		rsw2->bat_addr[i] = &bat_entry[i];
	}

	/* Init LL TX BAT */
	for (i = rsw2_ll_tx_q_port0; i < rsw2_ll_tx_q_max_entry; i++) {
		//pr_info("Creating BAT entry for queue %d (TX queue %d)\n", i, (i - RSW2_LL_TX_Q_OFFSET));
		bat_entry[i].die_dt = FIELD_PREP(RSW2_DESC_DT, DT_EOS);
		rsw2->bat_addr[i] = &bat_entry[i];
	}

	iowrite32(rsw2->bat_dma_addr, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDCBAC1);
	iowrite32(0, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDCBAC0);

	return 0;

dma_alloc_err:
	kfree(rsw2->bat_addr);

bat_ptr_alloc_err:

	return ret;
}

static int rswitch2_rx_ring_format(struct net_device *ndev, int q)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rsw2_rx_q_data *rx_q;
	struct rswitch2_dma_desc *bat_entry;
#ifdef RSW2_RX_TS_DESC
	struct rswitch2_dma_ext_ts_desc *rx_desc;
#else
	struct rswitch2_dma_ext_desc *rx_desc;
#endif
	int ring_size;
	int i;
	dma_addr_t dma_addr;
	u32 reg_queue;
	u32 bit_queue;
	u32 reg_val;

	eth_port = rswitch2_netdev_get_rx_q(ndev, q, &rx_q);
	BUG_ON(!eth_port);

	rsw2 = eth_port->rsw2;

	ring_size = sizeof(*rx_q->desc_ring) * (rx_q->entries + 1);
	memset(rx_q->desc_ring, 0, ring_size);

	for (i = 0, rx_desc = rx_q->desc_ring; i < rx_q->entries; i++, rx_desc++) {
		rx_desc->info_ds = cpu_to_le16(RSW2_PKT_BUF_SZ);

		dma_addr = dma_map_single(rsw2->dev, (void *)rx_q->skb[i]->data,
					  	  	  	  RSW2_PKT_BUF_SZ, DMA_FROM_DEVICE);

		/* We just set the data size to 0 for a failed mapping which
		 * should prevent DMA from happening...
		 */
		if (dma_mapping_error(rsw2->dev, dma_addr)) {
			dev_err(rsw2->dev, "Descriptor mapping error\n");
			rx_desc->info_ds = cpu_to_le16(0);

			return -ENOMEM;
		}
		rx_desc->dptrl = cpu_to_le32(dma_addr);
		rx_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_FEMPTY) | RSW2_DESC_DIE;
	}
	/* Close the loop */
	rx_desc->dptrl = cpu_to_le32((u32)rx_q->desc_dma);
	rx_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_LINKFIX); /* type */

	bat_entry = rsw2->bat_addr[q + rx_q->offset];
	bat_entry->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_LINKFIX); /* type */
	bat_entry->dptrl = cpu_to_le32((u32)rx_q->desc_dma);

	reg_val = FIELD_PREP(GWDCC_BALR, 1); /* Base address load request */
	reg_val |= FIELD_PREP(GWDCC_DCP, 1); /* Chain priority */
	reg_val |= FIELD_PREP(GWDCC_EDE, 1); /* Extended Descriptor Enable */

#ifdef RSW2_RX_TS_DESC
	reg_val |= FIELD_PREP(GWDCC_ETS, 1); /* Extended Timestamp Enable */
#endif
	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDCC(q + rx_q->offset));

	/* Activate RX interrupt for this q */
	reg_queue = (q + rx_q->offset) / RSWITCH2_BITS_PER_REG;
	bit_queue = (q + rx_q->offset) % RSWITCH2_BITS_PER_REG;

	reg_val = GWDIE_DIE(bit_queue);
	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDIE(reg_queue));

	return 0;
}

static int rswitch2_rx_ring_init(struct net_device *ndev, uint q, uint q_offset)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rsw2_rx_q_data *rx_q;
	struct sk_buff *skb;
	int ring_size;
	int i;

	eth_port = rswitch2_netdev_get_rx_q(ndev, q, &rx_q);
	BUG_ON(!eth_port);

	rsw2 = eth_port->rsw2;

	rx_q->offset = q_offset;
	rsw2->port_backref[q + q_offset].port_num = eth_port->port_num;
	rsw2->port_backref[q + q_offset].port_q = q;

	ring_size = sizeof(*rx_q->desc_ring) * (rx_q->entries + 1);

	rx_q->skb = kcalloc(rx_q->entries, sizeof(*rx_q->skb), GFP_KERNEL);
	if (!rx_q->skb)
		goto skb_ptr_alloc_err;

	for (i = 0; i < rx_q->entries; i++) {
		skb = dev_alloc_skb(RSW2_PKT_BUF_SZ + RSW2_BUF_ALIGN - 1);
		if (!skb) {
			pr_err("Failed to allocate skb for Q %d\n", q + q_offset);
			goto skb_alloc_err;
		}
		skb_reserve(skb, NET_IP_ALIGN);
		rx_q->skb[i] = skb;
	}

	rx_q->desc_ring = dma_alloc_coherent(rsw2->dev, ring_size,
					     	 	 	 	 &rx_q->desc_dma, GFP_KERNEL);

	if (!rx_q->desc_ring)
		goto dma_alloc_err;

	rx_q->dirty_desc = 0;

	return 0;

dma_alloc_err:
	while (--i < 0)
		dev_kfree_skb(rx_q->skb[i]);
skb_alloc_err:
	kfree(rx_q->skb);
skb_ptr_alloc_err:

	return -ENOMEM;
}


static int rswitch2_tx_ring_init(struct net_device *ndev, uint q, uint q_offset)
{
	struct rswitch2_eth_port *eth_port;
	struct rsw2_tx_q_data *tx_q;
	struct rswitch2_drv *rsw2;
	int ring_size;

	eth_port = rswitch2_netdev_get_tx_q(ndev, q, &tx_q);
	BUG_ON(!eth_port);

	rsw2 = eth_port->rsw2;
	tx_q->offset = q_offset;
	rsw2->port_backref[q + q_offset].port_num = eth_port->port_num;
	rsw2->port_backref[q + q_offset].port_q = q;

	/* Allocate ptr to sk_bufs */
	tx_q->skb = kcalloc(tx_q->entries, sizeof(*tx_q->skb), GFP_KERNEL);
	if (!tx_q->skb)
		goto tx_skb_err;

	/* Allocate all TX descriptors plus 1 to point back */
	ring_size = sizeof(*tx_q->desc_ring) * (tx_q->entries + 1);

	tx_q->desc_ring = dma_alloc_coherent(rsw2->dev,
					      ring_size, &tx_q->desc_dma, GFP_KERNEL);
	if (!tx_q->desc_ring)
		goto tx_ring_err;

	tx_q->cur_desc = 0;

	return 0;

tx_ring_err:
	kfree(tx_q->skb);
tx_skb_err:
	return -ENOMEM;
}



static int rswitch2_tx_ring_format(struct net_device *ndev, int q)
{
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_drv *rsw2;
	struct rswitch2_dma_desc *bat_entry;
	struct rsw2_tx_q_data *tx_q;
	struct rswitch2_dma_ext_desc *tx_desc;
	size_t ring_size;
	u32 reg_val;
	int i;

	eth_port = rswitch2_netdev_get_tx_q(ndev, q, &tx_q);
	BUG_ON(!eth_port);
	rsw2 = eth_port->rsw2;

	tx_q->cur_desc = 0;
	tx_q->dirty_desc = 0;

	ring_size = sizeof(*tx_q->desc_ring) * (tx_q->entries + 1);
	memset(tx_q->desc_ring, 0, ring_size);

	/* Build TX ring buffer */
	for (i = 0, tx_desc = tx_q->desc_ring; i < tx_q->entries; i++, tx_desc++) {
		tx_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_EEMPTY);
	}
	/* Close the loop */
	tx_desc->dptrl = cpu_to_le32((u32)tx_q->desc_dma);
	tx_desc->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_LINKFIX); /* type */

	bat_entry = rsw2->bat_addr[q + tx_q->offset];
	bat_entry->die_dt = FIELD_PREP(RSW2_DESC_DT, DT_LINKFIX); /* type */
	bat_entry->dptrl = cpu_to_le32((u32)tx_q->desc_dma);

	reg_val = FIELD_PREP(GWDCC_BALR, 1); /* Base address load request */
	reg_val |= FIELD_PREP(GWDCC_DCP, 1); /* Chain priority */
	reg_val |= FIELD_PREP(GWDCC_DQT, 1); /* Transmission queue */
	reg_val |= FIELD_PREP(GWDCC_EDE, 1); /* Extended Descriptor Enable */

	iowrite32(reg_val, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWDCC(q + tx_q->offset));

	return 0;
}



static u32 rswitch2_get_msglevel(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	return rsw2->msg_enable;
}

static void rswitch2_set_msglevel(struct net_device *ndev, u32 value)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	rsw2->msg_enable = value;
}

static const char rswitch2_gstrings_stats[][ETH_GSTRING_LEN] = {
	"rx_queue_0_current",
	"tx_queue_0_current",
	"rx_queue_0_dirty",
	"tx_queue_0_dirty",
	"rx_queue_0_packets",
	"tx_queue_0_packets",
	"rx_queue_0_bytes",
	"tx_queue_0_bytes",
	"rx_queue_0_mcast_packets",
	"rx_queue_0_errors",
	"rx_queue_0_crc_errors",
	"rx_queue_0_frame_errors",
	"rx_queue_0_length_errors",
	"rx_queue_0_missed_errors",
	"rx_queue_0_over_errors",

	"rx_queue_1_current",
	"tx_queue_1_current",
	"rx_queue_1_dirty",
	"tx_queue_1_dirty",
	"rx_queue_1_packets",
	"tx_queue_1_packets",
	"rx_queue_1_bytes",
	"tx_queue_1_bytes",
	"rx_queue_1_mcast_packets",
	"rx_queue_1_errors",
	"rx_queue_1_crc_errors",
	"rx_queue_1_frame_errors",
	"rx_queue_1_length_errors",
	"rx_queue_1_missed_errors",
	"rx_queue_1_over_errors",
};

static void rswitch2_get_ethtool_stats(struct net_device *ndev,
				   struct ethtool_stats *estats, u64 *data)
{
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	int i = 0;
	int q;

	eth_port =  netdev_priv(ndev);
	intern_port = eth_port->intern_port;

	/* Port specific stats */
	for (q = 0; q < NUM_BE_RX_QUEUES; q++) {
		struct net_device_stats *stats = &eth_port->stats[q];

		if (intern_port) {
			data[i++] = intern_port->rx_q[q].cur_desc;
			data[i++] = intern_port->tx_q[q].cur_desc;
			data[i++] = intern_port->rx_q[q].dirty_desc;
			data[i++] = intern_port->tx_q[q].dirty_desc;
		}
		data[i++] = stats->rx_packets;
		data[i++] = stats->tx_packets;
		data[i++] = stats->rx_bytes;
		data[i++] = stats->tx_bytes;
		data[i++] = stats->multicast;
		data[i++] = stats->rx_errors;
		data[i++] = stats->rx_crc_errors;
		data[i++] = stats->rx_frame_errors;
		data[i++] = stats->rx_length_errors;
		data[i++] = stats->rx_missed_errors;
		data[i++] = stats->rx_over_errors;
	}
}

#define RSWITCH2_STATS_LEN	ARRAY_SIZE(rswitch2_gstrings_stats)

static int rswicth2_get_sset_count(struct net_device *netdev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return RSWITCH2_STATS_LEN;
	default:
		return -EOPNOTSUPP;
	}
}

static void rswitch2_get_strings(struct net_device *ndev, u32 stringset, u8 *data)
{
	switch (stringset) {
	case ETH_SS_STATS:
		memcpy(data, rswitch2_gstrings_stats, sizeof(rswitch2_gstrings_stats));
		break;
	}
}

/* FIXḾE: Distinguish queues here? */
static void rswitch2_get_ringparam(struct net_device *ndev,
			       struct ethtool_ringparam *ring)
{
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;

	eth_port = netdev_priv(ndev);
	intern_port = eth_port->intern_port;
	BUG_ON(intern_port == NULL);

	ring->rx_max_pending = RSW2_BE_RX_RING_SIZE;
	ring->tx_max_pending = RSW2_BE_TX_RING_SIZE;
	ring->rx_pending = intern_port->rx_q[0].entries;
	ring->tx_pending = intern_port->tx_q[0].entries;
}

static int rswitch2_set_ringparam(struct net_device *ndev,
			      struct ethtool_ringparam *ring)
{
	/* TODO */
	return 0;
}

static int rswitch2_get_ts_info(struct net_device *ndev, struct ethtool_ts_info *info)
{
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_drv *rsw2;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	if(eth_port->intern_port) {
		info->so_timestamping = SOF_TIMESTAMPING_RX_SOFTWARE |
					SOF_TIMESTAMPING_SOFTWARE;
		info->phc_index = -1;

		return 0;
	}

	info->phc_index = ptp_clock_index(rsw2->ptp_drv->clock);
	info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
				SOF_TIMESTAMPING_RX_SOFTWARE |
				SOF_TIMESTAMPING_SOFTWARE |
				SOF_TIMESTAMPING_TX_HARDWARE |
				SOF_TIMESTAMPING_RX_HARDWARE |
				SOF_TIMESTAMPING_RAW_HARDWARE;
	info->tx_types = BIT(HWTSTAMP_TX_OFF) | BIT(HWTSTAMP_TX_ON);
	info->rx_filters = BIT(HWTSTAMP_FILTER_NONE) | BIT(HWTSTAMP_FILTER_ALL);

	return 0;
}

static int rswitch2_phy_ethtool_set_link_ksettings(struct net_device *ndev,
				   const struct ethtool_link_ksettings *cmd)
{
	struct phy_device *phydev = ndev->phydev;

	__ETHTOOL_DECLARE_LINK_MODE_MASK(advertising);
	u8 autoneg = cmd->base.autoneg;
	u8 duplex = cmd->base.duplex;
	u32 speed = cmd->base.speed;

	if (!phydev)
		return -ENODEV;


	if (cmd->base.phy_address != phydev->mdio.addr)
		return -EINVAL;

	linkmode_copy(advertising, cmd->link_modes.advertising);

	/* We make sure that we don't pass unsupported values in to the PHY */
	linkmode_and(advertising, advertising, phydev->supported);

	/* Verify the settings we care about. */
	if (autoneg != AUTONEG_ENABLE && autoneg != AUTONEG_DISABLE)
		return -EINVAL;

	if (autoneg == AUTONEG_ENABLE && linkmode_empty(advertising))
		return -EINVAL;

	if (autoneg == AUTONEG_DISABLE &&
	    (
	     (duplex != DUPLEX_HALF &&
	      duplex != DUPLEX_FULL)))
		return -EINVAL;

	phydev->autoneg = autoneg;

	if (autoneg == AUTONEG_DISABLE) {
		phydev->speed = speed;
		phydev->duplex = duplex;
	}

	linkmode_copy(phydev->advertising, advertising);

	linkmode_mod_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
			 phydev->advertising, autoneg == AUTONEG_ENABLE);

	phydev->master_slave_set = cmd->base.master_slave_cfg;
	phydev->mdix_ctrl = cmd->base.eth_tp_mdix_ctrl;

	/* Restart the PHY */
	phy_start_aneg(phydev);

	return 0;
}


static const struct ethtool_ops rswitch2_ethtool_ops = {
	.nway_reset			= phy_ethtool_nway_reset,
	.get_msglevel		= rswitch2_get_msglevel,
	.set_msglevel		= rswitch2_set_msglevel,
	.get_link			= ethtool_op_get_link,
	.get_strings		= rswitch2_get_strings,
	.get_ethtool_stats	= rswitch2_get_ethtool_stats,
	.get_sset_count		= rswicth2_get_sset_count,
	.get_ringparam		= rswitch2_get_ringparam,
	.set_ringparam		= rswitch2_set_ringparam,
	.get_ts_info		= rswitch2_get_ts_info,
	.get_link_ksettings	= phy_ethtool_get_link_ksettings,
	.set_link_ksettings	= rswitch2_phy_ethtool_set_link_ksettings,
	.get_wol			= NULL,
	.set_wol			= NULL,
};


static void rswitch2_init_port_mac(struct net_device *port_ndev)
{
	u32 reg_val;
	int speed;
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	phy_interface_t phy_iface;

	eth_port = netdev_priv(port_ndev);
	phy_port = eth_port->phy_port;
	rsw2 = eth_port->rsw2;
	BUG_ON(phy_port == NULL);

	/* 1 Cycle extra hold time */
	reg_val = FIELD_PREP(MPIC_PSMHT, 0x6);

	/* Set clock divider */
	reg_val |= FIELD_PREP(MPIC_PSMCS, 0x40);

	/* speed for MAC will later updated when link comes up */
	//phy_iface = phy_port->phy_iface;  //not yet initialised here
	//phy_iface = eth_port->rsw2->port_data[eth_port->port_num-1].phy_iface;
	phy_iface = phy_port->phy_iface;
	switch (phy_iface) {
		case PHY_INTERFACE_MODE_SGMII :
			//speed may chage during link up. MII is fixed
			speed = 1000;
			reg_val |= FIELD_PREP(MPIC_LSC, rsw2_rmac_1000mbps);
			reg_val |= FIELD_PREP(MPIC_PIS, rsw2_rmac_gmii);
			break;
		case PHY_INTERFACE_MODE_USXGMII :
			//speed may chage during link up. MII is fixed
			speed = 2500;
			reg_val |= FIELD_PREP(MPIC_LSC, rsw2_rmac_2500mbps);
			reg_val |= FIELD_PREP(MPIC_PIS, rsw2_rmac_xgmii);
			break;
		default:
			rsw2_err(MSG_GEN, "Unsupported MAC xMII format %s (%d) on port %d\n",phy_modes(phy_iface), phy_iface, eth_port->port_num-1);
			//return -EINVAL;
	}

	iowrite32(reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MPIC);

	//TODO: This is part of the MDIO access function, pre-conf has no effect
	reg_val = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MPSM);
	reg_val |= MPSM_MFF;
	iowrite32(reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MPSM);


	/* Enable broad-/multi-/uni-cast reception of eMAC and pMAC frames*/
	reg_val = MRAFC_BCENE;
	reg_val |= MRAFC_MCENE;
	reg_val |= MRAFC_UCENE;
	reg_val |= MRAFC_BCENP;
	reg_val |= MRAFC_MCENP;
	reg_val |= MRAFC_UCENP;
	iowrite32(reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MRAFC);
}




static void rswitch2_init_mac_addr(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct device_node *port_node;
//	struct device_node *dev_node;
	u8 *macaddr;
	int ret;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;


	port_node = rswitch2_get_port_node(rsw2, (eth_port->port_num - rsw2->num_of_cpu_ports));
//	dev_node = ndev->dev.of_node;
	ndev->dev.of_node = port_node;

	if(eth_port->intern_port) {
		ret = eth_platform_get_mac_address(rsw2->dev, ndev->dev_addr);
		if ((ret == 0) && is_valid_ether_addr(ndev->dev_addr)) {
			/* device tree or NVMEM values are valid so use them */
			rsw2_info(MSG_GEN, "MAC: '%s' Got valid MAC from eth_platform_get_mac_address()\n", ndev->name);

		}
		else {
			rsw2_info(MSG_GEN, "MAC: '%s' INVALID from eth_platform_get_mac_address()\n", ndev->name);
			eth_hw_addr_random(ndev);
		}
	}
	else {
		/* Although this calls of_get_mac_addr_nvmem() internally
		 * it can't succeed because it expects dev.of_node to belong to
		 * a platform device, which is not possible for the 'port' sub-node
		 */
		ret = eth_platform_get_mac_address(&ndev->dev, ndev->dev_addr);
		if((ret == 0) && is_valid_ether_addr(ndev->dev_addr)) {
			rsw2_info(MSG_GEN, "MAC: '%s' VALID MAC from eth_platform_get_mac_address()\n", ndev->name);
		}
		else if((nvmem_get_mac_address(&ndev->dev, ndev->dev_addr) == 0) &&
				is_valid_ether_addr(ndev->dev_addr)) {
			rsw2_info(MSG_GEN, "MAC: '%s' VALID from nvmem_get_mac_address()\n", ndev->name);
		}
		else {
			rsw2_info(MSG_GEN,"MAC: '%s' using random MAC\n", ndev->name);
			eth_hw_addr_random(ndev);
		}
	}
	macaddr = (u8 *)ndev->dev_addr;
	rsw2_notice(MSG_GEN,"Assigned MAC %.2X:%.2X:%.2X:%.2X:%.2X:%.2X to '%s'\n",
			macaddr[0], macaddr[1],
			macaddr[2], macaddr[3],
			macaddr[4], macaddr[5],
			ndev->name);

//	ndev->dev.of_node = dev_node;

}




// FIXME: don't re-invent base functions
static void rswitch_modify(void __iomem *addr, u32 reg, u32 clear, u32 set)
{
	iowrite32((ioread32(addr + reg) & ~clear) | set, addr + reg);
}
// FIXME
#define MMIS1_CLEAR_FLAGS       0xf

#define MPSM_PRD_SHIFT		16
#define MPSM_PRD_MASK		GENMASK(31, MPSM_PRD_SHIFT)

#define MDIO_READ_C45		0x03
#define MDIO_WRITE_C45		0x01

static int rswitch2_mdio_access(struct net_device *ndev, bool read,
				   int phyad, int devad, int regad, int data)
{
	int pop = read ? MDIO_READ_C45 : MDIO_WRITE_C45;
	u32 reg_val;
	int ret;
	struct rswitch2_drv *rsw2;
	struct rswitch2_physical_port *phy_port;
	struct rswitch2_eth_port *eth_port;

	eth_port = netdev_priv(ndev);
	phy_port = eth_port->phy_port;
	rsw2 = eth_port->rsw2;

	/* No match device */
	if (devad == 0xffffffff)
		return 0;

	/* Clear completion flags */
	writel(MMIS1_CLEAR_FLAGS, phy_port->rmac_base_addr + RSW2_RMAC_MMIS1);

	/* Submit address to PHY (MDIO_ADDR_C45 << 13) */
	reg_val = MPSM_PSME | MPSM_MFF;
	iowrite32((regad << 16) | (devad << 8) | (phyad << 3) | reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MPSM);

	ret = readl_poll_timeout(phy_port->rmac_base_addr + RSW2_RMAC_MMIS1, reg_val,
						reg_val & MMIS1_PAACS,
						RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "RMAC setting MDIO address timed out\n");
		return ret;
	}

	/* Clear address completion flag */
	rswitch_modify(phy_port->rmac_base_addr, RSW2_RMAC_MMIS1, MMIS1_PAACS, MMIS1_PAACS);

	/* Read/Write PHY register */
	if (read) {
		// FIXME: Why writel and iowrite32
		reg_val = MPSM_PSME | MPSM_MFF;
		writel((pop << 13) | (devad << 8) | (phyad << 3) | reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MPSM);

		ret = readl_poll_timeout(phy_port->rmac_base_addr + RSW2_RMAC_MMIS1, reg_val,
							reg_val & MMIS1_PRACS,
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
		if (ret != 0) {
			rsw2_err(MSG_GEN, "RMAC read MDIO timed out\n");
			return ret;
		}
		/* Read data */
		ret = (ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MPSM) & MPSM_PRD_MASK) >> 16;
		//printk("RSW2 MDIO read: phyad: %d, devad: %d, regad: %04x, ret:%04x \n", phyad, devad, regad, ret);

		/* Clear read completion flag */
		rswitch_modify(phy_port->rmac_base_addr, RSW2_RMAC_MMIS1, MMIS1_PRACS, MMIS1_PRACS);



	} else {
		reg_val = MPSM_PSME | MPSM_MFF;
		iowrite32((data << 16) | (pop << 13) | (devad << 8) | (phyad << 3) | reg_val,
				phy_port->rmac_base_addr + RSW2_RMAC_MPSM);

		rsw2_dbg(MSG_GEN, "RSW2 MDIO write: phyad: %d, devad: %d, regad: %04x, data:%04x \n", phyad, devad, regad, data);
		ret = readl_poll_timeout(phy_port->rmac_base_addr + RSW2_RMAC_MMIS1, reg_val,
							reg_val & MMIS1_PWACS,
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
		if (ret != 0) {
			netdev_err(ndev, "RMAC write MDIO timed out\n");
			return ret;
		}
		/* Clear write completion flag */
		rswitch_modify(phy_port->rmac_base_addr, RSW2_RMAC_MMIS1, MMIS1_PWACS, MMIS1_PWACS);
	}

	return ret;
}

static int rswitch2_mdio_read(struct mii_bus *bus, int addr, int regnum)
{
	struct net_device *ndev = bus->priv;
	int mode, devad, regad;

	mode = regnum & MII_ADDR_C45;
	devad = (regnum >> MII_DEVADDR_C45_SHIFT) & 0x1f;
	regad = regnum & MII_REGADDR_C45_MASK;

	/* Not support Clause 22 access method */
	if (!mode) {
		// FIXME
		//printk("NOT C45 regnum=%04x  return 0\n", regnum);

		return 0;
	}
	//printk("rswitch2_mdio_read(): '%s' addr: 0x%.4x  devad: 0x%.4x  regad: 0x%.4x", ndev->name, addr, devad, regad);
	return rswitch2_mdio_access(ndev, true, addr, devad, regad, 0);
}

static int rswitch2_mdio_write(struct mii_bus *bus, int addr, int regnum, u16 val)
{
	struct net_device *ndev = bus->priv;
	int mode, devad, regad;

	mode = regnum & MII_ADDR_C45;
	devad = (regnum >> MII_DEVADDR_C45_SHIFT) & 0x1f;
	regad = regnum & MII_REGADDR_C45_MASK;

	/* Not support Clause 22 access method */
	if (!mode) {
		// FIXME
		//printk("NOT C45 regnum=%04x  return 0\n", regnum);

		return 0;
	}
	//printk("rswitch2_mdio_write(): '%s' addr: 0x%.4x  devad: 0x%.4x  regad: 0x%.4x  val: 0x%.4x", ndev->name, addr, devad, regad, val);
	return rswitch2_mdio_access(ndev, false, addr, devad, regad, val);
}

static int rswitch2_mdio_reset(struct mii_bus *bus)
{
	/* TODO */
	return 0;
}

static int rswitch2_mdio_init(struct net_device *ndev, unsigned int port_num)
{
	struct mii_bus *mii_bus;
	struct device_node *dn_port;
	struct device_node *dn_phy;
	int phy_addr;
	int ret;

	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;
	phy_port = eth_port->phy_port;

	/* Only physical ports have MDIO interface */
	BUG_ON(phy_port == NULL);

	mii_bus = mdiobus_alloc();
	if (!mii_bus)
		return -ENOMEM;

	mii_bus->name = "rswitch_mii";
	sprintf(mii_bus->id, ndev->name, port_num);
	mii_bus->priv = ndev;
	mii_bus->read = rswitch2_mdio_read;
	mii_bus->write = rswitch2_mdio_write;
	mii_bus->reset = rswitch2_mdio_reset;
	mii_bus->parent = rsw2->dev;

	dn_port = rswitch2_get_port_node(rsw2, port_num);
	if (!dn_port)
		return -ENODEV;

	dn_phy = of_parse_phandle(dn_port, "phy-handle", 0);
	if (dn_phy) {
		ret = of_mdiobus_register(mii_bus, dn_port);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Failed to register MDIO bus for Port %d\n", port_num);
			of_node_put(dn_phy);
			goto mdio_bus_free;
		}

		ret = of_mdio_parse_addr(&mii_bus->dev, dn_phy);
		of_node_put(dn_phy);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Failed to get PHY address for Port %d\n", port_num);
			goto mdio_unreg;
		}
		phy_addr = ret;
		dev_info(rsw2->dev, "MDIO: Phy is at addr %d\n", phy_addr);

		phy_port->phy = to_phy_device(&mii_bus->mdio_map[phy_addr]->dev);
		if (!phy_port->phy) {
			rsw2_err(MSG_GEN, "PHY not found for Port %d\n", port_num);
			ret = -ENODEV;
			goto mdio_unreg;
		}
	} else if (of_phy_is_fixed_link(dn_port)) {
		ret = mdiobus_register(mii_bus);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Failed to register MDIO bus for fixed PHY for Port %d\n", port_num);
			goto mdio_bus_free;
		}

		ret = of_phy_register_fixed_link(dn_port);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Failed to register fixed link PHY for Port %d\n", port_num);
			goto mdio_unreg;
		}

		phy_port->phy = of_phy_find_device(dn_port);
		if (!phy_port->phy) {
			rsw2_err(MSG_GEN, "Failed to get fixed link PHY for Port %d\n", port_num);
			ret = -ENODEV;
			goto mdio_unreg;
		}
	} else {
		rsw2_err(MSG_GEN, "No PHY defined for Port %d\n", port_num);
		ret = -EINVAL;
		goto dn_port_put;
	}

	phy_port->phy->interface = phy_port->phy_iface;
	phy_port->mii_bus = mii_bus;

	of_node_put(dn_port);

	return 0;


mdio_unreg:
	mdiobus_unregister(phy_port->mii_bus);
mdio_bus_free:
	mdiobus_free(phy_port->mii_bus);
dn_port_put:
	of_node_put(dn_port);

	return ret;
}

static int rswitch2_get_phy_config(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct device_node *port_node;
	int ret;

	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;
	phy_port = eth_port->phy_port;

	port_node = rswitch2_get_port_node(rsw2, (eth_port->port_num - rsw2->num_of_cpu_ports));
	if(port_node) {
		ret = of_get_phy_mode(port_node, &phy_port->phy_iface);
		if (ret != 0) {
			rsw2_err(MSG_GEN, "of_get_phy_mode failed\n");
			return -ENODEV;
		}
		else {
			rsw2_info(MSG_GEN, "Got phy_iface mode: %s (%d)\n", phy_modes(phy_port->phy_iface), phy_port->phy_iface);
		}
	}
	of_node_put(port_node);

	return 0;
}

static int rswitch2_init_physical_port(struct rswitch2_drv *rsw2, unsigned int port_num)
{
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_physical_port *phy_port;
	struct net_device *port_ndev;
	int ret;
	char port_name[3];
	int port_name_len;
	size_t num_of_rx_queues;
	size_t num_of_tx_queues;
	uint q;

	port_ndev = alloc_etherdev_mqs(sizeof(struct rswitch2_eth_port), 1, 1);
	if (!port_ndev)
		return -ENOMEM;

	phy_port = kzalloc(sizeof(struct rswitch2_physical_port), GFP_KERNEL);
	if (phy_port < 0) {
		rsw2_err(MSG_GEN,  "Failed to allocate physical port\n");
		ret = -ENOMEM;
		goto phy_port_err;
	}
	SET_NETDEV_DEV(port_ndev, rsw2->dev);

	ether_setup(port_ndev);

	eth_port = netdev_priv(port_ndev);
	eth_port->ndev = port_ndev;
	eth_port->rsw2 = rsw2;
	eth_port->port_num = (port_num + rsw2->num_of_cpu_ports);
	eth_port->phy_port = phy_port;

	port_ndev->base_addr = (unsigned long)rsw2->etha_base_addrs[port_num];
	rsw2_dbg(MSG_GEN, "Phy port %d: rsw2->etha_base_addrs[%d] = 0x%px\n", port_num, port_num, rsw2->etha_base_addrs[port_num]);


	/* FIXME */
	port_ndev->irq = rsw2->rxtx_irqs[port_num];

	phy_port->rmac_base_addr = rsw2->etha_base_addrs[port_num] + RSW2_RMAC_OFFSET;
	phy_port->serdes_chan_addr = rsw2->serdes_base_addr
			+ (RSW2_SERDES_CHANNEL_OFFSET * port_num);


	strncpy(port_ndev->name, RSW2_NETDEV_BASENAME, sizeof(port_ndev->name) - sizeof(port_name) - 1);
	port_name_len = snprintf(port_name, sizeof(port_name), "p%1u", eth_port->port_num-1);
	strncat(port_ndev->name, port_name, port_name_len);

	ret = dev_alloc_name(port_ndev, port_ndev->name);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Failed to alloc dev_name()\n");
		goto phy_port_err;
	}
	dev_set_name(&port_ndev->dev, port_ndev->name);

	ret = rswitch2_emac_set_state(port_ndev, emac_disable);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Port set state 'disable' failed\n");
		goto cleanup_phy_port;
	}

	ret = rswitch2_emac_set_state(port_ndev, emac_reset);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Port set state 'reset' failed\n");
		goto cleanup_phy_port;
	}

	ret = rswitch2_emac_set_state(port_ndev, emac_disable);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Port set state 'disable' failed\n");
		goto cleanup_phy_port;
	}

	ret = rswitch2_emac_set_state(port_ndev, emac_config);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Port set state 'config' failed\n");
		goto cleanup_phy_port;
	}

	/* FIXME: Error handling */
	rswitch2_get_phy_config(port_ndev);

	/* MAC related setup */
	rswitch2_init_mac_addr(port_ndev);
	rswitch2_port_set_mac_addr(port_ndev);
	rswitch2_init_port_mac(port_ndev);

	/* Change to OPERATION Mode */
	ret = rswitch2_emac_set_state(port_ndev, emac_operation);
	if (ret < 0) {
		netdev_err(port_ndev, "Port set state 'operation' failed\n");
		goto cleanup_phy_port;
	}


	/* Link Verification */
	/* TODO CMARD: I'm not sure if this is the correct position. The link verification
	 * is a feature to ask the far end MAC if it is pre-emption capable. But this can
	 * only happen when the physical link is established.
	 */
	/*{
		u32 reg_val;
		/ * Request Link Verification * /
		reg_val = ioread32(phy_port->rmac_base_addr + RSW2_RMAC_MPSM);
			reg_val |= MPSM_MFF;
			iowrite32(reg_val, phy_port->rmac_base_addr + RSW2_RMAC_MPSM);


		iowrite32(MLVC_PLV, phy_port->rmac_base_addr + RSW2_RMAC_MLVC);
		ret = readl_poll_timeout(phy_port->rmac_base_addr + RSW2_RMAC_MLVC, reg_val,
							reg_val & MLVC_PLV,
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
		if (ret != 0) {
			dev_err(rsw2->dev, "RMAC Preemption link verification timed out\n");
			return ret;
		} else {
			dev_info(rsw2->dev, "RMAC Preemption link verified!\n");
		}
	}*/

	port_ndev->min_mtu = 2048 - (ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN);
	port_ndev->max_mtu = ETH_MIN_MTU;
	port_ndev->netdev_ops = &rswitch2_netdev_ops;
	port_ndev->ethtool_ops = &rswitch2_ethtool_ops;

	ret = rswitch2_mdio_init(port_ndev, port_num);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "rswitch2_mdio_init failed: %d\n", ret);
		goto cleanup_phy_port;
	}

	rswitch2_port_set_mac_addr(port_ndev);


#if 0
	ret = rswitch2_phy_init(port_ndev, port_num);
	if (ret != 0) {
		dev_err(rsw2->dev, "rswitch2_phy_init failed: %d\n", ret);
		goto cleanup_phy_port;
	}

	ret = rswitch2_serdes_init(port_ndev, port_num);
	if (ret != 0) {
		dev_err(rsw2->dev, "rswitch2_serdes_init failed: %d\n", ret);
		//FIXME: clean up error handling
		goto cleanup_phy_port;
	}
	else {
		dev_err(rsw2->dev, "rswitch2_serdes_init SUCCESS\n");

	}
#endif

	num_of_rx_queues = ARRAY_SIZE(phy_port->rx_q);
	num_of_tx_queues = ARRAY_SIZE(phy_port->tx_q);

	/* Set ring layout */
	for (q = 0; q < num_of_tx_queues; q++) {
		phy_port->tx_q[q].entries = RSW2_LL_TX_RING_SIZE;
	}
	for (q = 0; q < num_of_rx_queues; q++) {
		phy_port->rx_q[q].entries = RSW2_LL_RX_RING_SIZE;
	}

	netif_napi_add(port_ndev, &phy_port->rx_q[0].napi, rswitch2_poll, 64);

	ret = phy_connect_direct(port_ndev, phy_port->phy, &rswitch2_phy_state_change, phy_port->phy_iface);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "%s: phy_connect_direct() failed: %d\n", port_ndev->name, ret);
		return ret;
	} else {
		rsw2_notice(MSG_GEN, "phy_connect_direct(): attached '%s' PHY driver: %d\n", port_ndev->phydev->drv->name , ret);
	}

	rsw2->ports[port_num + rsw2->num_of_cpu_ports] = eth_port;

	return 0;

cleanup_phy_port:
	kfree(phy_port);
phy_port_err:
	free_netdev(port_ndev);

	return ret;
}

static int rswitch2_init_internal_port(struct rswitch2_drv *rsw2, unsigned int port_num)
{
	int ret;
	struct net_device *ndev = NULL;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	int q;

	if (port_num > rsw2->num_of_cpu_ports)
		return -EINVAL;

	ndev = alloc_etherdev_mqs(sizeof(struct rswitch2_eth_port),
			NUM_BE_TX_QUEUES, NUM_BE_RX_QUEUES);
	if (!ndev)
		return -ENOMEM;

	eth_port = netdev_priv(ndev);
	memset(eth_port, 0, sizeof(*eth_port));

	ndev->base_addr = (unsigned long)rsw2->gwca_base_addrs[port_num];

	rsw2_info(MSG_GEN, "Assigning irq %d to internal port %d\n", rsw2->rxtx_irqs[port_num], port_num);

	ndev->irq = rsw2->rxtx_irqs[port_num];
	strncpy(ndev->name, RSW2_NETDEV_BASENAME, sizeof(ndev->name) - 1);

	ret = dev_alloc_name(ndev, ndev->name);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Failed to alloc dev_name()\n");
		goto intern_port_err;
	}

	dev_set_name(&ndev->dev, ndev->name);

	SET_NETDEV_DEV(ndev, rsw2->dev);
	ether_setup(ndev);

	intern_port = kzalloc(sizeof(struct rswitch2_internal_port), GFP_KERNEL);
	if (intern_port < 0) {
		rsw2_err(MSG_GEN, "Failed to allocate internal port\n");
		ret = -ENOMEM;
		goto intern_port_err;
	}
	memset(intern_port, 0, sizeof(*intern_port));

	/* Initialize private data */
	eth_port->ndev = ndev;
	eth_port->rsw2 = rsw2;
	eth_port->port_num = port_num;
	eth_port->intern_port = intern_port;
	eth_port->phy_port = NULL;

	/* Get MAC address and set it in HW */
	rsw2_dbg(MSG_GEN, "Start init the MAC\n");

	rswitch2_init_mac_addr(ndev);
	rswitch2_intern_set_mac_addr(ndev);

	/* Set ring layout */
	for (q = 0; q < NUM_BE_TX_QUEUES; q++) {
		intern_port->tx_q[q].entries = RSW2_BE_TX_RING_SIZE;
	}
	for (q = 0; q < NUM_BE_RX_QUEUES; q++) {
		intern_port->rx_q[q].entries = RSW2_BE_RX_RING_SIZE;
	}

	//spin_lock_init(&eth_port->lock);

	ndev->netdev_ops = &rswitch2_netdev_ops;

	ndev->ethtool_ops = &rswitch2_ethtool_ops;
	ndev->max_mtu = 2048 - (ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN);
	ndev->min_mtu = ETH_MIN_MTU;

	for (q = 0; q < NUM_BE_RX_QUEUES; q++)
		netif_napi_add(ndev, &intern_port->rx_q[q].napi, rswitch2_poll, 64);

	rsw2->ports[port_num] = eth_port;

	return 0;

intern_port_err:
	free_netdev(ndev);

	return ret;
}

static void rswitch2_free_rings(struct net_device *ndev)
{
	struct rswitch2_drv *rsw2;
	struct rswitch2_eth_port *eth_port;
	struct rswitch2_internal_port *intern_port;
	struct rswitch2_physical_port *phy_port;
	uint num_of_rx_queues;
	uint num_of_tx_queues;
	uint q;
#ifdef RSW2_RX_TS_DESC
	struct rswitch2_dma_ext_ts_desc *rx_desc;
#else
	struct rswitch2_dma_ext_desc *rx_desc;
#endif


	eth_port = netdev_priv(ndev);
	rsw2 = eth_port->rsw2;

	intern_port = eth_port->intern_port;
	phy_port = eth_port->phy_port;

	if(intern_port) {
		num_of_rx_queues = ARRAY_SIZE(intern_port->rx_q);
		num_of_tx_queues = ARRAY_SIZE(intern_port->tx_q);
	} else {
		num_of_rx_queues = ARRAY_SIZE(phy_port->rx_q);
		num_of_tx_queues = ARRAY_SIZE(phy_port->tx_q);
	}


	for (q = 0; q < num_of_rx_queues; q++) {
		uint q_entry;
		size_t ring_size;
		struct rsw2_rx_q_data *rx_q;

		(void)rswitch2_netdev_get_rx_q(ndev, q, &rx_q);

		ring_size = sizeof(*rx_q->desc_ring) * (rx_q->entries + 1);

		for(q_entry = 0; q_entry < rx_q->entries; q_entry++) {
			if(rx_q->skb[q_entry] != NULL) {
				rx_desc = &rx_q->desc_ring[q_entry];
				dma_unmap_single(ndev->dev.parent, le32_to_cpu(rx_desc->dptrl),
									RSW2_PKT_BUF_SZ, DMA_FROM_DEVICE);
				dev_kfree_skb_any(rx_q->skb[q_entry]);
			}
		}


		if(rx_q->desc_ring) {
			dma_free_coherent(rsw2->dev, ring_size, rx_q->desc_ring, rx_q->desc_dma);
			rx_q->desc_ring = NULL;
			rx_q->desc_dma = 0;
		}
	}


	for (q = 0; q < num_of_tx_queues; q++) {
		size_t ring_size;
		struct rsw2_tx_q_data *tx_q;

		(void)rswitch2_netdev_get_tx_q(ndev, q, &tx_q);

		ring_size = sizeof(*tx_q->desc_ring) * (tx_q->entries + 1);

		if(tx_q->desc_ring) {
			dma_free_coherent(rsw2->dev, ring_size, tx_q->desc_ring, tx_q->desc_dma);
			tx_q->desc_ring = NULL;
			tx_q->desc_dma = 0;
		}
	}
}
static void rswitch2_free_bat(struct rswitch2_drv *rsw2)
{
	size_t bat_size;

	bat_size = sizeof(struct rswitch2_dma_desc *) * NUM_ALL_QUEUES;

	if (rsw2->bat_dma_addr)
		dma_free_coherent(rsw2->dev, bat_size, rsw2->bat_addr[0],
						rsw2->bat_dma_addr);

	kfree(rsw2->bat_addr);
}


static void rswitch2_disable_ports(struct rswitch2_drv *rsw2)
{

	uint cur_port;
	uint total_ports = rsw2->num_of_cpu_ports + rsw2->num_of_tsn_ports;

	for (cur_port = 0; cur_port < total_ports; cur_port++) {
		struct net_device *ndev;
		struct rswitch2_eth_port *eth_port;
		struct rswitch2_internal_port *intern_port;

		eth_port = rsw2->ports[cur_port];
		if(!eth_port)
			continue;


		ndev = eth_port->ndev;
		intern_port = eth_port->intern_port;

		if (intern_port != NULL) {
			int q;
			if (ndev)
				netif_tx_stop_all_queues(ndev);

			rswitch2_disable_rx(ndev);

			for (q = 0; q < NUM_BE_RX_QUEUES; q++)
				netif_napi_del(&intern_port->rx_q[q].napi);

			rswitch2_free_rings(ndev);
			kfree(eth_port->intern_port);
			eth_port->intern_port = NULL;
		} else if (eth_port->phy_port != NULL) {
			kfree(eth_port->phy_port);
			eth_port->phy_port = NULL;
		}

		if (ndev->phydev != NULL) {
			if (phy_is_started(ndev->phydev))
				phy_stop(ndev->phydev);
		}

	}
}

int rswitch2_eth_init(struct rswitch2_drv *rsw2)
{
	int ret;
	int i;
	unsigned int cur_port;
	unsigned int cur_irq;
	unsigned int num_of_cpus;
	uint cur_rx_q;
	uint cur_tx_q;
	unsigned int total_ports = rsw2->num_of_cpu_ports + rsw2->num_of_tsn_ports;

	spin_lock_init(&rsw2->lock);

	ret = rswitch2_gwca_init(rsw2);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Failed to initialize GWCA: %d\n", ret);
		return ret;

	}

	rsw2->ports = kcalloc(total_ports, sizeof(*rsw2->ports), GFP_KERNEL);
	if (!rsw2->ports)
		return -ENOMEM;

	ret = rswitch2_bat_init(rsw2);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Base table initialization failed: %d\n", ret);
		goto bat_init_err;
	}

	/* Debug message level */
	rsw2->msg_enable = RSW2_DEF_MSG_ENABLE;

	num_of_cpus = num_online_cpus();

	//dev_info(rsw2->dev, "Detected %d CPUs cores - creating 1 BE queue per core", rsw2->num_of_cpu_ports);

	for (cur_port = 0; cur_port < rsw2->num_of_cpu_ports; cur_port++) {
		struct net_device *ndev;

		ret = rswitch2_init_internal_port(rsw2, cur_port);
		if (ret < 0)
			goto port_init_err;

		ndev = rsw2->ports[cur_port]->ndev;

		// FIXME: ARRAY_SIZE()
		for(cur_tx_q = 0; cur_tx_q < NUM_BE_TX_QUEUES; cur_tx_q++) {

			/* Init TX best effort queues */
			ret = rswitch2_tx_ring_init(ndev, cur_tx_q, RSW2_BE_TX_Q_OFFSET);
			if (ret < 0) {
				rsw2_err(MSG_GEN, "TX ring init for '%s' failed\n", ndev->name);
				goto port_init_err;
			}
			rsw2_info(MSG_GEN, "BE rswitch2_tx_ring_init() done\n");

			rswitch2_tx_ring_format(ndev, cur_tx_q);
			if (ret < 0) {
				rsw2_err(MSG_GEN, "TX ring format for '%s' failed\n", ndev->name);
				goto port_init_err;
			}
			rsw2_info(MSG_GEN,"BE rswitch2_tx_ring_format() done\n");
		}
		for(cur_rx_q = 0; cur_rx_q < NUM_BE_RX_QUEUES; cur_rx_q++) {

			/* Init RX best effort queues */
			rsw2_info(MSG_GEN, "BE Initializing RX ring %d\n", cur_rx_q);

			ret = rswitch2_rx_ring_init(ndev, cur_rx_q, RSW2_BE_RX_Q_OFFSET);
			if (ret < 0) {
				rsw2_err(MSG_GEN, "RX ring %d init for '%s' failed\n", cur_rx_q, ndev->name);
				goto port_init_err;
			}
			rsw2_info(MSG_GEN, "BE Formating ring RX ring %d\n", cur_rx_q);
			rswitch2_rx_ring_format(ndev, cur_rx_q);
			if (ret < 0) {
				rsw2_err(MSG_GEN, "RX ring %d format for '%s' failed\n", cur_rx_q, ndev->name);
				goto port_init_err;
			}


		}
	}

	for (cur_port = 0; cur_port < rsw2->num_of_tsn_ports; cur_port++) {
		struct net_device *ndev;
		struct device_node *dn_port;

		dn_port = rswitch2_get_port_node(rsw2, cur_port);
		if(!dn_port) {
			rsw2_err(MSG_GEN, "Port %d has invalid device tree setting. Skipping.\n", cur_port);
			of_node_put(dn_port);
			continue;
		} else if (!of_device_is_available(dn_port)) {
			rsw2_notice(MSG_GEN, "Port %d is disabled in device tree.\n", cur_port);
			of_node_put(dn_port);
			continue;
		}
		of_node_put(dn_port);

		ret = rswitch2_init_physical_port(rsw2, cur_port);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Failed to initialize port %d: %d\n", cur_port, ret);
			continue;
		}
		rsw2_info(MSG_GEN, "rswitch2_init_physical_port(%d) done\n", cur_port);

		ndev = rsw2->ports[cur_port + rsw2->num_of_cpu_ports]->ndev;
		for(cur_tx_q = 0; cur_tx_q < RSW2_LL_TX_PER_PORT_QUEUES; cur_tx_q++) {

			/* Init TX link local queues */
			ret = rswitch2_tx_ring_init(ndev, cur_tx_q, RSW2_LL_TX_Q_OFFSET + cur_port);
			if (ret < 0) {
				rsw2_err(MSG_GEN, "TX ring init for '%s' failed\n", ndev->name);
				goto port_init_err;
				return ret;
			}
			rsw2_info(MSG_GEN, "LL rswitch2_tx_ring_init() done\n");

			rswitch2_tx_ring_format(ndev, cur_tx_q);
			if (ret < 0) {
				rsw2_err(MSG_GEN, "TX ring format for '%s' failed\n", ndev->name);
				goto port_init_err;
			}
			rsw2_info(MSG_GEN, "LL rswitch2_tx_ring_format() done\n");
		}
		for(cur_rx_q = 0; cur_rx_q < RSW2_LL_RX_PER_PORT_QUEUES; cur_rx_q++) {

			/* Init RX link local queues */
			rsw2_info(MSG_GEN, "LL Initializing RX ring %d\n", cur_rx_q);

			ret = rswitch2_rx_ring_init(ndev, cur_rx_q, RSW2_LL_RX_Q_OFFSET + cur_port);
			if (ret < 0) {
				rsw2_err(MSG_GEN, "RX ring %d init for '%s' failed\n", cur_rx_q, ndev->name);
				goto port_init_err;
			}

			rsw2_info(MSG_GEN, "LL Formating ring RX ring %d\n", cur_rx_q);
			rswitch2_rx_ring_format(ndev, cur_rx_q);
			if (ret < 0) {
				rsw2_err(MSG_GEN, "RX ring %d format for '%s' failed\n", cur_rx_q, ndev->name);
				goto port_init_err;
			}

		}
	}

	/* FIXME: return value */
	rswitch2_ts_ring_init(rsw2);


	for (cur_port = 0; cur_port < rsw2->num_of_tsn_ports + rsw2->num_of_cpu_ports; cur_port++) {
		struct net_device *ndev;

		if(!rsw2->ports[cur_port])
			continue;

		ndev = rsw2->ports[cur_port ]->ndev;
		ret = register_netdev(ndev);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Failed to register netdev: %d\n", ret);
			goto port_init_err;
		}
		else
			rsw2_info(MSG_GEN, "Register netdev '%s'sucessfully\n", ndev->name);
	}

	ret = rswitch2_gwca_set_state(rsw2, gwmc_operation);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Failed to set GWCA operation state!\n");
		goto port_init_err;
	}

	/* Request data IRQs */
	for(cur_irq = 0; cur_irq < rsw2->num_of_rxtx_irqs; cur_irq++) {
		ret = request_irq(rsw2->rxtx_irqs[cur_irq], rswitch2_eth_interrupt, IRQ_TYPE_LEVEL_HIGH,
						RSWITCH2_NAME, rsw2);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Failed to request data IRQ(%d): %d\n", rsw2->rxtx_irqs[cur_irq], ret);
			goto bat_init_err;
		}
	}

	/* Request status IRQs */
	for(cur_irq = 0; cur_irq < rsw2->num_of_status_irqs; cur_irq++) {
		ret = request_irq(rsw2->status_irqs[cur_irq], rswitch2_status_interrupt, IRQ_TYPE_LEVEL_HIGH,
						RSWITCH2_NAME, rsw2);
		if (ret < 0) {
			rsw2_err(MSG_GEN, "Failed to request status IRQ(%d): %d\n", rsw2->status_irqs[cur_irq], ret);
			goto bat_init_err;
		}
	}

	/* Set max frame size to enable data storage in internal RAM */
	for( i = 0; i < 8; i++) {
		iowrite32(0xFFFF, rsw2->gwca_base_addrs[0] +RSW2_GCWA_GWRMFSC(i));
	}

	iowrite32(0x0, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWRDQC);

	return 0;

port_init_err:
	rswitch2_disable_ports(rsw2);

bat_init_err:

	return ret;
}

void rswitch2_eth_exit(struct rswitch2_drv *rsw2)
{
	struct rswitch2_eth_port *eth_port;
	uint cur_irq;
	int i;
	size_t ts_ring_size;
	uint ring_entries;
	uint cur_port;
	uint total_ports = rsw2->num_of_cpu_ports + rsw2->num_of_tsn_ports;

	/* Set max frame size to zero to avoid data is stored in internal RAM */
	for( i = 0; i < 8; i++) {
		iowrite32(0x0, rsw2->gwca_base_addrs[0] +RSW2_GCWA_GWRMFSC(i));
	}
	iowrite32(0xFF, rsw2->gwca_base_addrs[0] + RSW2_GCWA_GWRDQC);


	for(cur_irq = 0; cur_irq < rsw2->num_of_rxtx_irqs; cur_irq++) {
		free_irq(rsw2->rxtx_irqs[cur_irq], rsw2);
	}
	for(cur_irq = 0; cur_irq < rsw2->num_of_status_irqs; cur_irq++) {
		free_irq(rsw2->status_irqs[cur_irq], rsw2);
	}

	for (cur_port = 0; cur_port < total_ports; cur_port++) {
		struct net_device *ndev;
		struct rswitch2_internal_port *intern_port;
		struct rswitch2_physical_port *phy_port;


		eth_port = rsw2->ports[cur_port];
		if(!eth_port)
			continue;

		ndev = eth_port->ndev;
		intern_port = eth_port->intern_port;
		phy_port = eth_port->phy_port;

		rtnl_lock();
		if(netif_running(ndev))
			dev_close(ndev);
		rtnl_unlock();

		if (phy_port) {
            uint cur_q;
            uint num_of_rx_queues;
            uint num_of_tx_queues;

            phy_disconnect(phy_port->phy);
            phy_device_remove(phy_port->phy);
			phy_device_free(phy_port->phy);

			mdiobus_unregister(phy_port->mii_bus);
			mdiobus_free(phy_port->mii_bus);

			num_of_rx_queues = ARRAY_SIZE(phy_port->rx_q);
			num_of_tx_queues = ARRAY_SIZE(phy_port->tx_q);

			rswitch2_free_rings(ndev);

			for (cur_q = 0; cur_q < num_of_rx_queues; cur_q++) {
				netif_napi_del(&phy_port->rx_q[cur_q].napi);
				kfree(phy_port->rx_q[cur_q].skb);
			}

			for (cur_q = 0; cur_q < num_of_tx_queues; cur_q++)
				kfree(phy_port->tx_q[cur_q].skb);

			kfree(eth_port->phy_port);
			eth_port->phy_port = NULL;

		} else if (intern_port) {
			uint cur_q;
			uint num_of_rx_queues;
			uint num_of_tx_queues;

			num_of_rx_queues = ARRAY_SIZE(intern_port->rx_q);
			num_of_tx_queues = ARRAY_SIZE(intern_port->tx_q);

			rswitch2_free_rings(ndev);

			for (cur_q = 0; cur_q < num_of_rx_queues; cur_q++) {
				netif_napi_del(&intern_port->rx_q[cur_q].napi);
				kfree(intern_port->rx_q[cur_q].skb);
			}

			for (cur_q = 0; cur_q < num_of_tx_queues; cur_q++)
				kfree(intern_port->tx_q[cur_q].skb);

			kfree(eth_port->intern_port);
			eth_port->intern_port = NULL;
		}
		unregister_netdev(ndev);
		free_netdev(ndev);
	}

	ring_entries = rsw2->num_of_tsn_ports * MAX_TS_Q_ENTRIES_PER_PORT;
	ts_ring_size = sizeof(*rsw2->ts_desc_ring) * (ring_entries + 1);
	dma_free_coherent(rsw2->dev, ts_ring_size, rsw2->ts_desc_ring, rsw2->ts_desc_dma);
	dma_free_coherent(rsw2->dev, sizeof(*rsw2->bat_ts_addr), rsw2->bat_ts_addr, rsw2->bat_ts_dma_addr);

	rswitch2_free_bat(rsw2);
	kfree(rsw2->ports);
}
