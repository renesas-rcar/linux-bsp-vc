// SPDX-License-Identifier: GPL-2.0+
/*
 * Microchip LAN865X 10Base-T1S Ethernet Driver (MAC + PHY)
 *
 * (c) Copyright 2021 Microchip Technology Inc.
 * Author: Parthiban Veerasooran <Parthiban.Veerasooran@microchip.com>
 * Using Open Alliance TC6 Library written by Thorsten Kummermehr
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/property.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#include "tc6.h"

#define DRV_NAME		"LAN865x"
#define DRV_VERSION		"0.7"

#define REG_STDR_CONFIG0	0x1
#define REG_STDR_STATUS0	0x2

#define REG_MAC_NW_CTRL		0x20000
#define REG_MAC_NW_CONFIG	0x20001
#define REG_MAC_HASHL		0x20020
#define REG_MAC_HASHH		0x20021
#define REG_MAC_ADDR_L		0x20022
#define REG_MAC_ADDR_H		0x20023

#define REG_PHY_PLCA_VER	0x13C00
#define REG_PHY_PLCA_CTRL0	0x13C01
#define REG_PHY_PLCA_CTRL1	0x13C02
#define REG_PHY_PLCA_STS	0x13C03
#define REG_PHY_PLCA_BURST	0x13C05

#define MAC_PROMISCUOUS_MODE	0x00000010
#define MAC_MULTICAST_MODE	0x00000040
#define MAC_UNICAST_MODE	0x00000080
#define TX_CUT_THROUGH_MODE	0x8206
#define TX_STORE_FWD_MODE	0x8006
#define PLCA_ENABLE		0x8000
#define MACPHY_HW_READY		0x80000000
#define ETH_BUF_LEN		1536
#define ETH_MIN_HEADER_LEN  	42
#define TX_TIMEOUT		(4 * HZ)
#define TC6_SECURE		(false)

#define ENABLE_COLLISION_DETECTION  0

#define LAN865X_MSG_DEFAULT	\
	(NETIF_MSG_PROBE | NETIF_MSG_IFUP | NETIF_MSG_IFDOWN | NETIF_MSG_LINK)

/* Driver local data */
struct lan865x_priv {
	struct net_device *netdev;
	struct spi_device *spi;
	struct spi_message msg;
	struct spi_transfer transfer;
	struct TC6_t tc6;
	u8 eth_rx_buf[ETH_BUF_LEN];
	u32 msg_enable;
	bool spiBusy;
	bool Eth_RxInvalid;
	uint16_t Eth_RxLength;
	wait_queue_head_t irq_wq;
	wait_queue_head_t ns_wq;
	uint32_t read_value;
	bool enable_data;
	struct completion tc6_completion;
	u8 plca_enable;
	u8 plca_node_id;
	u8 plca_node_count;
	u8 tx_cut_thr_mode;
	struct mutex lock;
	struct timer_list tc6_timer;
	struct task_struct *ns_task;
	struct task_struct *irq_task;
	bool irq_flag;
	bool ns_flag;
};

#define TIMEOUT 1000    //milliseconds

const MemoryMap_t TC6_MEMMAP[] = {
    {  .address = 0x00000001,  .value=0x00004000,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x000130D0,  .value=0x000030EA,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x000130D1,  .value=0x00000800,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
#if (ENABLE_COLLISION_DETECTION)
    {  .address = 0x00013084,  .value=0x0000338F,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x00013087,  .value=0x0000901F,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
#else
    {  .address = 0x00013084,  .value=0x0000320F,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x00013087,  .value=0x0000101F,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
#endif
    {  .address = 0x00013085,  .value=0x00000006,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x0001308A,  .value=0x0000C0FF,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x00013088,  .value=0x0000633F,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x0001308B,  .value=0x00000404,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x00013080,  .value=0x00000680,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x000130F1,  .value=0x00002440,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x00013096,  .value=0x00002000,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
    {  .address = 0x00013099,  .value=0x00007F80,  .mask=0x00000000,  .op=MemOp_Write,  .secure=false  },
};

const uint32_t TC6_MEMMAP_LENGTH = (sizeof(TC6_MEMMAP) / sizeof(MemoryMap_t));

/* use ethtool to change the level for any given device */
static struct {
	u32 msg_enable;
} debug = { -1 };

/* PLCA enable - Could be configured by parameter */
static u8 plca_enable = 1;
/* PLCA node-id - Could configured by parameter */
static u8 plca_node_id = 0;
/* PLCA count - Could configured by parameter */
static u8 plca_node_count = 8;

/******************************************************************************
** Function Name      : Tc6_Read_Callback
** Description        : Callback function from register read
******************************************************************************/
void Tc6_Read_Callback(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);

	priv->read_value = value;
	complete(&priv->tc6_completion);
	TC6_EnableData(&priv->tc6, true);
}

/******************************************************************************
** Function Name      : Tc6_Write_Callback
** Description        : Callback function from register write
******************************************************************************/
void Tc6_Write_Callback(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);

	complete(&priv->tc6_completion);
}

/******************************************************************************
** Function Name      : Tc6_ReadRegister
** Description        : Function to read register
******************************************************************************/
bool Tc6_ReadRegister(struct lan865x_priv *priv, uint32_t addr, uint32_t *value)
{
	bool ret;
	long wait_remaining;

	reinit_completion(&priv->tc6_completion);
	ret = TC6_ReadRegister(&priv->tc6, addr, TC6_SECURE, Tc6_Read_Callback, priv);
	if (ret) {
		priv->ns_flag = true;
		wake_up_interruptible(&priv->ns_wq);
		wait_remaining = wait_for_completion_interruptible_timeout(&priv->tc6_completion,
						   			   msecs_to_jiffies(100));
		if ((wait_remaining == 0) || (wait_remaining == -ERESTARTSYS)) {
			ret = false;
		} else {
			*value = priv->read_value;
			ret = true;
		}
	}
	if (!ret) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "SPI read register failed for addr: %x\n", addr);
	}
	return ret;
}

/******************************************************************************
** Function Name      : Tc6_ReadRegister
** Description        : Function to write register
******************************************************************************/
bool Tc6_WriteRegister(struct lan865x_priv *priv, uint32_t addr, uint32_t value)
{
	bool ret;
	long wait_remaining;

	reinit_completion(&priv->tc6_completion);
	ret = TC6_WriteRegister(&priv->tc6, addr, value, TC6_SECURE, Tc6_Write_Callback, priv);
	if (ret) {
		priv->ns_flag = true;
		wake_up_interruptible(&priv->ns_wq);
		wait_remaining = wait_for_completion_interruptible_timeout(&priv->tc6_completion,
						   			   msecs_to_jiffies(100));
		if ((wait_remaining == 0) || (wait_remaining == -ERESTARTSYS))
			ret = false;
		else
			ret = true;
	}
	if (!ret) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "SPI write register failed for addr: %x\n", addr);
	}
	return ret;
}

void TC6Stub_Print(TC6_t *pInst, void *pGlobalTag, const char *statement, ... )
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);

	if (printk_ratelimit())
		dev_info(&priv->spi->dev, "TC6Stub_Print: %s\n", statement);
}

void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pRx, uint16_t offset, uint16_t len, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);

	if (priv->Eth_RxInvalid)
		return;
	if (offset + len > ETH_BUF_LEN)
	{
		priv->Eth_RxInvalid = true;
		return;
	}
	if (offset)
	{
		if (!priv->Eth_RxLength)
		{
			priv->Eth_RxInvalid = true;
			return;
		}
	}
	else
	{
		if (priv->Eth_RxLength)
		{
			priv->Eth_RxInvalid = true;
			return;
		}
	}
	memcpy(priv->eth_rx_buf + offset, pRx, len);
	priv->Eth_RxLength += len;
}

void static send_rx_eth_pkt(struct lan865x_priv *priv, u8 *buf, u16 len)
{
	struct sk_buff *skb = NULL;

	skb = netdev_alloc_skb(priv->netdev, len + NET_IP_ALIGN);
	if (!skb) {
		if (netif_msg_rx_err(priv)) {
			if (printk_ratelimit())
				netdev_err(priv->netdev, "Out of memory for Rx'd frame\n");
		}
		priv->netdev->stats.rx_dropped++;
	} else {
		skb_reserve(skb, NET_IP_ALIGN);
		/* copy the packet from the receive buffer */
		memcpy(skb_put(skb, len), buf, len);
		skb->protocol = eth_type_trans(skb, priv->netdev);
		/* update statistics */
		priv->netdev->stats.rx_packets++;
		priv->netdev->stats.rx_bytes += len;
		netif_rx_ni(skb);
	}
}

void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);

	if (len > ETH_BUF_LEN) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "Packet length greater than MTU: %d\n\r", len);
	}

	if (!success || priv->Eth_RxInvalid || !priv->Eth_RxLength)
	{
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "Packet drop\n\r");
		priv->netdev->stats.rx_dropped++;
		goto end;
	}
	if (len < ETH_MIN_HEADER_LEN)
	{
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "Received invalid small packet length: %d\n\r", len);
		priv->netdev->stats.rx_dropped++;
		goto end;
	}
	send_rx_eth_pkt(priv, priv->eth_rx_buf, len);

end:
	priv->Eth_RxLength = 0;
	priv->Eth_RxInvalid = false;
}

void TC6_CB_OnError(TC6_t *pInst, TC6_Error_t err, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);

	if (printk_ratelimit())
		dev_err(&priv->spi->dev, "tc6 err = %x\n", err);
}

static void lan865x_spi_transfer_complete(void *context)
{
	struct lan865x_priv *priv = context;

	priv->spiBusy = false;
	mutex_lock(&priv->lock);
	TC6Stub_CB_OnSpiBufferDone(&priv->tc6, true);
	mutex_unlock(&priv->lock);
}

bool TC6Stub_SpiTransaction(TC6_t *pInst, const uint8_t *pTx, uint8_t *pRx, uint16_t len, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);
	int status = 0;

	if (priv->spiBusy)
		return false;

	priv->spiBusy = true;

	spi_message_init(&priv->msg);

	priv->transfer.tx_nbits = 1; /* 1 MOSI line */
	priv->transfer.rx_nbits = 1; /* 1 MISO line */
	priv->transfer.speed_hz = 0; /* Use device setting */
	priv->transfer.bits_per_word = 0; /* Use device setting */
	priv->transfer.tx_buf = pTx;
	priv->transfer.rx_buf = pRx;
	priv->transfer.delay.value = 0;
	priv->transfer.delay.unit = SPI_DELAY_UNIT_USECS;
	priv->transfer.cs_change = 0;
	priv->transfer.len = len;
	priv->msg.complete = lan865x_spi_transfer_complete;
	priv->msg.context = priv;

	spi_message_add_tail(&priv->transfer, &priv->msg);
	status = spi_async(priv->spi, &priv->msg);
	if (status < 0) {
		if (printk_ratelimit())
			dev_err(&priv->spi->dev, "spy_async failed, status: %d\n", status);
		return false;
	}
	return true;
}

void OnStatus0(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);

	if (printk_ratelimit())
		dev_err(&priv->spi->dev, "Ext sts0: %d\n", value);
		
	TC6_UnlockExtendedStatus(pInst);
}

void TC6_CB_OnExtendedStatus(TC6_t *g, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(g, struct lan865x_priv, tc6);

	TC6_ReadRegister(g, REG_STDR_STATUS0, TC6_SECURE, OnStatus0, priv);
}

void TC6Stub_InitializeSpi(TC6_t *pInst, void *pGlobalTag)
{
}

static bool lan865x_init(struct lan865x_priv *priv)
{
	uint32_t regVal;
	bool success = true;
	uint32_t i = 0;

	TC6_Init(&priv->tc6);
	if (success) {
		while (i < TC6_MEMMAP_LENGTH) {
			success = Tc6_WriteRegister(priv, TC6_MEMMAP[i].address, TC6_MEMMAP[i].value);
			if (!success)
				break;
			i++;
		}
	}
	if (success) {
		if (priv->tx_cut_thr_mode) {
			success = Tc6_WriteRegister(priv, REG_STDR_CONFIG0, TX_CUT_THROUGH_MODE);
		}
		else {
			success = Tc6_WriteRegister(priv, REG_STDR_CONFIG0, TX_STORE_FWD_MODE);
		}
	}
	if (success) {
		if (priv->plca_enable) {
			/* Setting max id and local node id */
			regVal = (priv->plca_node_count << 8) | priv->plca_node_id;
			success = Tc6_WriteRegister(priv, REG_PHY_PLCA_CTRL1, regVal);
			/* Enable PLCA */
			if (success) {
				success = Tc6_WriteRegister(priv, REG_PHY_PLCA_CTRL0, PLCA_ENABLE);
			}
		}
	}
	if (success) {
		success = Tc6_ReadRegister(priv, REG_STDR_STATUS0, &regVal);
		dev_info(&priv->spi->dev, "STS0: %d\n", regVal);
		if (regVal != MACPHY_HW_READY)
			success = false;
	}
	return success;
}

static int lan865x_set_hw_macaddr(struct net_device *netdev)
{
	uint32_t regVal;
	bool ret;
	struct lan865x_priv *priv = netdev_priv(netdev);
	struct device *dev = &priv->spi->dev;
	u8 *mac = netdev->dev_addr;

	if (!priv->enable_data) {
		if (netif_msg_drv(priv))
			dev_info(dev, "%s: Setting MAC address to %pM\n",
				 netdev->name, netdev->dev_addr);
		/* MAC address setting */
		regVal = (mac[3] << 24) | (mac[2] << 16) | (mac[1] << 8) | mac[0];
		ret = Tc6_WriteRegister(priv, REG_MAC_ADDR_L, regVal);
		if (ret) {
			regVal = (mac[5] << 8) | mac[4];
			ret = Tc6_WriteRegister(priv, REG_MAC_ADDR_H, regVal);
		}
		if (!ret) {
			dev_printk(KERN_DEBUG, dev,
				   "%s() Failed to set Mac address\n",
				   __func__);
			return -EIO;
		}
	} else {
		if (netif_msg_drv(priv))
			dev_printk(KERN_DEBUG, dev,
				   "%s() Hardware must be disabled to set Mac address\n",
				   __func__);
		return -EBUSY;
	}
	return 0;
}

static int ns_handler(void *data)
{
	struct lan865x_priv *priv = data;

	while (likely(!kthread_should_stop())) {
		wait_event_interruptible(priv->ns_wq,  priv->ns_flag ||
							kthread_should_stop());
		priv->ns_flag = false;
		mutex_lock(&priv->lock);
		TC6_Service(&priv->tc6, true);
		mutex_unlock(&priv->lock);
	}
	return 0;
}

static int irq_handler(void *data)
{
	struct lan865x_priv *priv = data;

	while (likely(!kthread_should_stop())) {
		wait_event_interruptible(priv->irq_wq,  priv->irq_flag ||
							kthread_should_stop());
		priv->irq_flag = false;
		mutex_lock(&priv->lock);
		TC6_Service(&priv->tc6, false);
		mutex_unlock(&priv->lock);
	}
	return 0;
}

static irqreturn_t lan865x_irq(int irq, void *dev_id)
{
	struct lan865x_priv *priv = dev_id;

	if (&priv->tc6 && priv->enable_data) {
		priv->irq_flag = true;
		wake_up_interruptible(&priv->irq_wq);
	}

	return IRQ_HANDLED;
}

void TC6_CB_OnNeedService(TC6_t *pInst, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);

	priv->ns_flag = true;
	wake_up_interruptible(&priv->ns_wq);
}

static int lan865x_hw_enable(struct lan865x_priv *priv)
{
	if (!Tc6_WriteRegister(priv, REG_MAC_NW_CTRL, 0xC))
		return -EIO;

	priv->enable_data = true;

	return 0;
}

static int lan865x_hw_disble(struct lan865x_priv *priv)
{
	if (!Tc6_WriteRegister(priv, REG_MAC_NW_CTRL, 0x0))
		return -EIO;

	priv->enable_data = false;

	return 0;
}

static int
lan865x_set_link_ksettings(struct net_device *netdev,
			    const struct ethtool_link_ksettings *cmd)
{
	struct lan865x_priv *priv = netdev_priv(netdev);
	int ret = 0;

	if (!priv->enable_data) {
		if ((cmd->base.autoneg != AUTONEG_DISABLE) || (cmd->base.speed != SPEED_10) ||
				(cmd->base.duplex != DUPLEX_HALF)) {
			if (netif_msg_link(priv))
				netdev_warn(netdev, "Unsupported link setting\n");
			ret = -EOPNOTSUPP;
		}
	} else {
		if (netif_msg_link(priv))
			netdev_warn(netdev, "Warning: hw must be disabled to set link mode\n");
		ret = -EBUSY;
	}
	return ret;
}

static int
lan865x_get_link_ksettings(struct net_device *netdev,
			    struct ethtool_link_ksettings *cmd)
{
	ethtool_link_ksettings_zero_link_mode(cmd, supported);
	ethtool_link_ksettings_add_link_mode(cmd, supported, 10baseT_Half);
	ethtool_link_ksettings_add_link_mode(cmd, supported, TP);

	cmd->base.speed = SPEED_10;
	cmd->base.duplex = DUPLEX_HALF;
	cmd->base.port	= PORT_TP;
	cmd->base.autoneg = AUTONEG_DISABLE;

	return 0;
}

static void lan865x_set_msglevel(struct net_device *netdev, u32 val)
{
	struct lan865x_priv *priv = netdev_priv(netdev);

	priv->msg_enable = val;
}

static u32 lan865x_get_msglevel(struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);

	return priv->msg_enable;
}

static void
lan865x_get_drvinfo(struct net_device *netdev, struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info,
		dev_name(netdev->dev.parent), sizeof(info->bus_info));
}

static const struct ethtool_ops lan865x_ethtool_ops = {
	.get_drvinfo	= lan865x_get_drvinfo,
	.get_msglevel	= lan865x_get_msglevel,
	.set_msglevel	= lan865x_set_msglevel,
	.get_link_ksettings = lan865x_get_link_ksettings,
	.set_link_ksettings = lan865x_set_link_ksettings,
};

static int lan865x_set_mac_address(struct net_device *netdev, void *addr)
{
	struct sockaddr *address = addr;

	if (netif_running(netdev))
		return -EBUSY;
	if (!is_valid_ether_addr(address->sa_data))
		return -EADDRNOTAVAIL;

	ether_addr_copy(netdev->dev_addr, address->sa_data);
	return lan865x_set_hw_macaddr(netdev);
}

/* Returns hash bit number for given MAC address
 * Example:
 * 01 00 5E 00 00 01 -> returns bit number 31 */
static u32 lan865x_hash(u8 addr[ETH_ALEN])
{
	return (ether_crc(ETH_ALEN, addr) >> 26) & 0x3f;
}

static void lan865x_set_multicast_list(struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);
	u32 regVal = 0;

	if (netdev->flags & IFF_PROMISC) {
		/* Enabling promiscuous mode */
		regVal |= MAC_PROMISCUOUS_MODE;
		regVal &= (~MAC_MULTICAST_MODE);
		regVal &= (~MAC_UNICAST_MODE);
	} else if (netdev->flags & IFF_ALLMULTI) {
		/* Enabling all multicast mode */
		regVal &= (~MAC_PROMISCUOUS_MODE);
		regVal |= MAC_MULTICAST_MODE;
		regVal &= (~MAC_UNICAST_MODE);
	} else if (!netdev_mc_empty(netdev)) {
		/* Enabling specific multicast addresses */
		struct netdev_hw_addr *ha;
		u32 hash_lo = 0;
		u32 hash_hi = 0;

		netdev_for_each_mc_addr(ha, netdev) {
			u32 bit_num = lan865x_hash(ha->addr);
			u32 mask = 1 << (bit_num & 0x1F);

			if (bit_num & 0x20)
				hash_hi |= mask;
			else
				hash_lo |= mask;
		}
		if (!TC6_WriteRegister(&priv->tc6, REG_MAC_HASHH, hash_hi, TC6_SECURE, NULL, NULL)) {
			if (netif_msg_timer(priv)) {
				netdev_err(netdev, "Failed to write REG_HASHH\n");
			}
		}
		if (!TC6_WriteRegister(&priv->tc6, REG_MAC_HASHL, hash_lo, TC6_SECURE, NULL, NULL)) {
			if (netif_msg_timer(priv)) {
				netdev_err(netdev, "Failed to write REG_HASHL\n");
			}
		}
		regVal &= (~MAC_PROMISCUOUS_MODE);
		regVal &= (~MAC_MULTICAST_MODE);
		regVal |= MAC_UNICAST_MODE;
	} else {
		/* Enabling local MAC address only */
		if (!TC6_WriteRegister(&priv->tc6, REG_MAC_HASHH, 0, TC6_SECURE, NULL, NULL)) {
			if (netif_msg_timer(priv)) {
				netdev_err(netdev, "Failed to write REG_HASHH\n");
			}
		}
		if (!TC6_WriteRegister(&priv->tc6, REG_MAC_HASHL, 0, TC6_SECURE, NULL, NULL)) {
			if (netif_msg_timer(priv)) {
				netdev_err(netdev, "Failed to write REG_HASHL\n");
			}
		}
		regVal &= (~MAC_PROMISCUOUS_MODE);
		regVal &= (~MAC_MULTICAST_MODE);
		regVal &= (~MAC_UNICAST_MODE);
	}
	if (!TC6_WriteRegister(&priv->tc6, REG_MAC_NW_CONFIG, regVal, TC6_SECURE, NULL, NULL)) {
		if (netif_msg_timer(priv)) {
			netdev_err(netdev, "Failed to enable promiscuous mode\n");
		}
	}
}

void OnRawTx(TC6_t *pInst, const uint8_t *pTx, uint16_t len, void *pTag, void *pGlobalTag)
{
	struct lan865x_priv *priv = container_of(pInst, struct lan865x_priv, tc6);
	struct sk_buff *tx_skb = pTag;

	priv->netdev->stats.tx_packets++;
	priv->netdev->stats.tx_bytes += len;
	dev_kfree_skb(tx_skb);
	if (netif_queue_stopped(priv->netdev))
		netif_wake_queue(priv->netdev);
}

static netdev_tx_t lan865x_send_packet(struct sk_buff *skb,
					struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);
	bool ret;

	ret = TC6_SendRawEthernetPacket(&priv->tc6, skb->data, skb->len, OnRawTx, skb);
	if (!ret) {
		netif_stop_queue(priv->netdev);
		return NETDEV_TX_BUSY;
	}

	return NETDEV_TX_OK;
}

void timer_callback(struct timer_list * data)
{
	struct lan865x_priv *priv = from_timer(priv, data, tc6_timer);

	priv->irq_flag = true;
	wake_up_interruptible(&priv->irq_wq);

	mod_timer(&priv->tc6_timer, jiffies + msecs_to_jiffies(TIMEOUT));
}

static int lan865x_net_close(struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);

	if (lan865x_hw_disble(priv) != 0) {
		if (netif_msg_ifup(priv))
			netdev_err(netdev, "lan865x_hw_disable() failed\n");
		return -EIO;
	}
	netif_stop_queue(netdev);

	return 0;
}

static int lan865x_net_open(struct net_device *netdev)
{
	struct lan865x_priv *priv = netdev_priv(netdev);
	int ret;

	if (!is_valid_ether_addr(netdev->dev_addr)) {
		if (netif_msg_ifup(priv))
			netdev_err(netdev, "Invalid MAC address %pM\n", netdev->dev_addr);
		return -EADDRNOTAVAIL;
	}
	ret = lan865x_set_hw_macaddr(netdev);
	if (ret != 0)
		return ret;
	if (lan865x_hw_enable(priv) != 0) {
		if (netif_msg_ifup(priv))
			netdev_err(netdev, "lan865x_hw_enable() failed\n");
		return -EIO;
	}
	netif_start_queue(netdev);

	return 0;
}

static void lan865x_tx_timeout(struct net_device *netdev, unsigned int txqueue)
{
	struct lan865x_priv *priv = netdev_priv(netdev);

	if (netif_msg_timer(priv)) {
		if (printk_ratelimit())
			netdev_err(netdev, "tx timeout\n");
	}

	netdev->stats.tx_errors++;
}

static const struct net_device_ops lan865x_netdev_ops = {
	.ndo_open		= lan865x_net_open,
	.ndo_stop		= lan865x_net_close,
	.ndo_start_xmit		= lan865x_send_packet,
	.ndo_set_rx_mode	= lan865x_set_multicast_list,
	.ndo_set_mac_address	= lan865x_set_mac_address,
	.ndo_tx_timeout		= lan865x_tx_timeout,
	.ndo_validate_addr	= eth_validate_addr,
};

#ifdef CONFIG_OF
static int lan865x_get_of_data(struct lan865x_priv *priv)
{
	struct spi_device *spi = priv->spi;
	int ret;

	if (!spi->dev.of_node)
		return -EINVAL;

	ret = of_property_read_u8(spi->dev.of_node, "plca-enable", &priv->plca_enable);
	if (ret < 0) {
		dev_err(&spi->dev, "plca-enable property is not found in device tree\n");
		return ret;
	}
	if (priv->plca_enable > 1) {
		dev_err(&spi->dev, "Bad value in plca-enable property\n");
		return -EINVAL;
	}

	if (priv->plca_enable) {
		ret = of_property_read_u8(spi->dev.of_node, "plca-node-id", &priv->plca_node_id);
		if (ret < 0) {
			dev_err(&spi->dev, "plca-node-id property is not found in device tree\n");
			return ret;
		}
		if (priv->plca_node_id > 254) {
			dev_err(&spi->dev, "Bad value in plca-node-id property\n");
			return -EINVAL;
		}

		if (priv->plca_node_id == 0) {
			ret = of_property_read_u8(spi->dev.of_node, "plca-node-count", &priv->plca_node_count);
			if (ret < 0) {
				dev_err(&spi->dev, "plca-node-count property is not found in device tree\n");
				return ret;
			}
			if (priv->plca_node_count < 1) {
				dev_err(&spi->dev, "Bad value in plca-node-count property\n");
				return -EINVAL;
			}
		}
	}

	ret = of_property_read_u8(spi->dev.of_node, "tx-cut-through-mode", &priv->tx_cut_thr_mode);
	if (ret < 0) {
		dev_err(&spi->dev, "tx-cut-through-mode property is not found in device tree\n");
		return ret;
	}
	if (priv->tx_cut_thr_mode > 1) {
		dev_err(&spi->dev, "Bad value in tx-cut-through-mode property\n");
		return -EINVAL;
	}

	return 0;
}
#endif

#ifdef CONFIG_ACPI
static int lan865x_get_acpi_data(struct lan865x_priv *priv)
{
	priv->tx_cut_thr_mode = 0;

	return 0;
}
#endif

static int lan865x_probe(struct spi_device *spi)
{
	struct lan865x_priv *priv;
	struct net_device *netdev;
	unsigned char macaddr[ETH_ALEN];
	int ret;

	netdev = alloc_etherdev(sizeof(struct lan865x_priv));
	if (!netdev) {
		ret = -ENOMEM;
		goto error_alloc;
	}
	priv = netdev_priv(netdev);
	priv->netdev = netdev;	/* priv to netdev reference */
	priv->spi = spi;	/* priv to spi reference */
	priv->msg_enable = netif_msg_init(debug.msg_enable, LAN865X_MSG_DEFAULT);
	mutex_init(&priv->lock);
	spi_set_drvdata(spi, priv);	/* spi to priv reference */
	SET_NETDEV_DEV(netdev, &spi->dev);

	/* Initialize PLCA configuration (optional from module parameter) */
	priv->plca_enable = plca_enable;
	priv->plca_node_id = plca_node_id;
	priv->plca_node_count = plca_node_count;

	init_completion(&priv->tc6_completion);
	init_waitqueue_head(&priv->irq_wq);
	init_waitqueue_head(&priv->ns_wq);

#ifdef CONFIG_OF
	ret = lan865x_get_of_data(priv);
#endif
#ifdef CONFIG_ACPI
	ret = lan865x_get_acpi_data(priv);
#endif
	if (ret) {
		if (netif_msg_probe(priv))
			dev_err(&spi->dev, "No platform data for lan865x\n");
		ret = -ENODEV;
		goto error_platform_data;
	}

	timer_setup(&priv->tc6_timer, timer_callback, 0);
	mod_timer(&priv->tc6_timer, jiffies + msecs_to_jiffies(TIMEOUT));

	priv->irq_task = kthread_run(irq_handler, priv, "%s irq_task",
				     dev_name(&spi->dev));
	if (IS_ERR(priv->irq_task)) {
		ret = PTR_ERR(priv->irq_task);
		goto error_irq_task;
	}
	sched_set_fifo(priv->irq_task);

	priv->ns_task = kthread_run(ns_handler, priv, "%s ns_task",
				    dev_name(&spi->dev));
	if (IS_ERR(priv->ns_task)) {
		ret = PTR_ERR(priv->ns_task);
		goto error_ns_task;
	}
	sched_set_fifo(priv->ns_task);

	ret = lan865x_init(priv);
	if (!ret) {
		if (netif_msg_probe(priv))
			dev_err(&spi->dev, "lan865x init failed, Hardware not found\n");
		ret = -ENODEV;
		goto error_init;
	}

	if (device_get_mac_address(&spi->dev, macaddr, sizeof(macaddr)))
		ether_addr_copy(netdev->dev_addr, macaddr);
	else
		eth_hw_addr_random(netdev);

	spi->rt = true;
	spi->bits_per_word = 32;
	spi_setup(spi);
	pr_debug("spi->irq: %d\n", spi->irq);

	ret = devm_request_threaded_irq(&spi->dev, spi->irq, NULL,
					lan865x_irq, IRQF_ONESHOT,
					dev_name(&spi->dev), priv);
	if ((ret != -ENOTCONN) && (ret < 0)) {
		dev_err(&spi->dev, "error attaching irq %d\n", ret);
		goto error_request_irq;
	}

	netdev->if_port = IF_PORT_10BASET;
	netdev->irq = spi->irq;
	netdev->netdev_ops = &lan865x_netdev_ops;
	netdev->watchdog_timeo = TX_TIMEOUT;
	netdev->ethtool_ops = &lan865x_ethtool_ops;
	ret = register_netdev(netdev);
	if (ret) {
		if (netif_msg_probe(priv))
			dev_err(&spi->dev, "register netdev failed (ret = %d)\n",
				ret);
		goto error_netdev_register;
	}

	if (priv->plca_enable)
		netdev_info(netdev, "PLCA mode is active (node-count=%d  node-id=%d)\n",
				priv->plca_node_count, priv->plca_node_id);
	else
		netdev_info(netdev, "CSMA/CD mode is active\n");
	netdev_info(netdev, "Tx mode is %s\n",
			(priv->tx_cut_thr_mode) ? "cut-through" : "store-forward");

	return 0;

error_netdev_register:
	devm_free_irq(&spi->dev, spi->irq, priv);
error_init:
error_request_irq:
	kthread_stop(priv->ns_task);
error_ns_task:
	kthread_stop(priv->irq_task);
error_irq_task:
	del_timer(&priv->tc6_timer);
error_platform_data:
	free_netdev(netdev);
error_alloc:
	return ret;
}

static int lan865x_remove(struct spi_device *spi)
{
	struct lan865x_priv *priv = spi_get_drvdata(spi);

	del_timer(&priv->tc6_timer);
	unregister_netdev(priv->netdev);
	if (spi->irq > 0)
		devm_free_irq(&spi->dev, spi->irq, priv);
	kthread_stop(priv->irq_task);
	kthread_stop(priv->ns_task);
	free_netdev(priv->netdev);

	return 0;
}

static const struct spi_device_id spidev_spi_ids[] = {
	{ .name = "probe_lan865x" },
	{},
};
MODULE_DEVICE_TABLE(spi, spidev_spi_ids);
#ifdef CONFIG_OF
static const struct of_device_id lan865x_dt_ids[] = {
	{ .compatible = "microchip,lan865x" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lan865x_dt_ids);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id lan865x_acpi_ids[] = {
	{ .id = "LAN865X",
	},
	{},
};
MODULE_DEVICE_TABLE(acpi, lan865x_acpi_ids);
#endif

static struct spi_driver lan865x_driver = {
	.driver = {
		.name = DRV_NAME,
#ifdef CONFIG_OF
		.of_match_table = lan865x_dt_ids,
#endif
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(lan865x_acpi_ids),
#endif
	 },
	.probe = lan865x_probe,
	.remove = lan865x_remove,
	.id_table = spidev_spi_ids,
};
module_spi_driver(lan865x_driver);

MODULE_DESCRIPTION(DRV_NAME " 10Base-T1S MACPHY Ethernet Driver");
MODULE_AUTHOR("Parthiban Veerasooran <Parthiban.Veerasooran@microchip.com>");
MODULE_AUTHOR("Thorsten Kummermehr <Thorsten.Kummermehr@microchip.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level in amount of bits set (0=none, ..., 31=all)");
module_param_named(enable, plca_enable, byte, 0660);
MODULE_PARM_DESC(enable, " PLCA Enable");
module_param_named(node_id, plca_node_id, byte, 0660);
MODULE_PARM_DESC(node_id, " PLCA Node-ID");
module_param_named(node_count, plca_node_count, byte, 0660);
MODULE_PARM_DESC(node_count, " PLCA Node-Count");
MODULE_ALIAS("spi:" DRV_NAME);
