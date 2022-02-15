/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas RSwitch PCIe device driver
 *
 * Copyright (C) 2021, 2022 Renesas Electronics Corporation
 */


#ifndef _RSWITCH2_PLATF_H
#define _RSWITCH2_PLATF_H

/* Rswitch2 IP register offsets */
#define RSWITCH2_MFWD_BASE_ADDR		0x00000000	/* Fabric Register Offset */
#define RSWITCH2_FAB_BASE_ADDR		0x00008000	/* Forwarding Engine Register Offset */
#define RSWITCH2_COMA_BASE_ADDR		0x00009000	/* Common Agent Register Offset */
#define RSWITCH2_TSNA0_BASE_ADDR	0x0000A000	/* TSN Agent Register Offset */
#define RSWITCH2_TSNA1_BASE_ADDR	0x0000C000	/* TSN Agent Register Offset */
#define RSWITCH2_TSNA2_BASE_ADDR	0x0000E000	/* TSN Agent Register Offset */
#define RSWITCH2_GWCA0_BASE_ADDR	0x00010000	/* GWCA Register Offset	*/
#define RSWITCH2_GWCA1_BASE_ADDR	0x00012000	/* GWCA Register Offset	*/
#define RSWITCH2_GPTP_BASE_ADDR     0x00018000  /* GPTP Register Offset */

#define RSWITCH2_TSN_PORTS			3
#define RSWITCH2_CPU_PORTS			1  // FIXME: USe only one internal port for now

/* SerDes definitions */
/* SerDes */
enum rswitch_serdes_mode {
	USXGMII,
	SGMII,
	COMBINATION,
};

#define RSWITCH_SERDES_OFFSET                   0x0400
#define RSWITCH_SERDES_BANK_SELECT              0x03fc
#define RSWITCH_SERDES_FUSE_OVERRIDE(n)         (0x2600 - (n) * 0x400)

#define BANK_180                                0x0180
#define VR_XS_PMA_MP_12G_16G_25G_SRAM           0x026c
#define VR_XS_PMA_MP_12G_16G_25G_REF_CLK_CTRL   0x0244
#define VR_XS_PMA_MP_10G_MPLLA_CTRL2            0x01cc
#define VR_XS_PMA_MP_12G_16G_25G_MPLL_CMN_CTRL  0x01c0
#define VR_XS_PMA_MP_12G_16G_MPLLA_CTRL0        0x01c4
#define VR_XS_PMA_MP_12G_MPLLA_CTRL1            0x01c8
#define VR_XS_PMA_MP_12G_MPLLA_CTRL3            0x01dc
#define VR_XS_PMA_MP_12G_16G_25G_VCO_CAL_LD0    0x0248
#define VR_XS_PMA_MP_12G_VCO_CAL_REF0           0x0258
#define VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1    0x0144
#define VR_XS_PMA_CONSUMER_10G_RX_GENCTRL4      0x01a0
#define VR_XS_PMA_MP_12G_16G_25G_TX_RATE_CTRL   0x00d0
#define VR_XS_PMA_MP_12G_16G_25G_RX_RATE_CTRL   0x0150
#define VR_XS_PMA_MP_12G_16G_TX_GENCTRL2        0x00c8
#define VR_XS_PMA_MP_12G_16G_RX_GENCTRL2        0x0148
#define VR_XS_PMA_MP_12G_AFE_DFE_EN_CTRL        0x0174
#define VR_XS_PMA_MP_12G_RX_EQ_CTRL0            0x0160
#define VR_XS_PMA_MP_10G_RX_IQ_CTRL0            0x01ac
#define VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1    0x00c4
#define VR_XS_PMA_MP_12G_16G_TX_GENCTRL2        0x00c8
#define VR_XS_PMA_MP_12G_16G_RX_GENCTRL2        0x0148
#define VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1    0x00c4
#define VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL0    0x00d8
#define VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL1    0x00dc
#define VR_XS_PMA_MP_12G_16G_MPLLB_CTRL0        0x01d0
#define VR_XS_PMA_MP_12G_MPLLB_CTRL1            0x01d4
#define VR_XS_PMA_MP_12G_16G_MPLLB_CTRL2        0x01d8
#define VR_XS_PMA_MP_12G_MPLLB_CTRL3            0x01e0

#define BANK_300                                0x0300
#define SR_XS_PCS_CTRL1                         0x0000
#define SR_XS_PCS_STS1                          0x0004
#define SR_XS_PCS_CTRL2                         0x001c

#define BANK_380                                0x0380
#define VR_XS_PCS_DIG_CTRL1                     0x0000
#define VR_XS_PCS_DEBUG_CTRL                    0x0014
#define VR_XS_PCS_KR_CTRL                       0x001c

#define BANK_1F00                               0x1f00
#define SR_MII_CTRL                             0x0000

#define BANK_1F80                               0x1f80
#define VR_MII_AN_CTRL                          0x0004


struct rsw2_phy_mode_xlate {
	const char *dt_str;
	phy_interface_t intf;
};

static const struct rsw2_phy_mode_xlate rsw2_phy_mode_xlate_tbl[] = {
	{ "mii", PHY_INTERFACE_MODE_MII },
	{ "gmii", PHY_INTERFACE_MODE_GMII },
	{ "rgmii", PHY_INTERFACE_MODE_GMII },
	{ "sgmii", PHY_INTERFACE_MODE_SGMII },
	{ "usxgmii", PHY_INTERFACE_MODE_USXGMII },
};


#endif /* _RSWITCH2_PLATF_H */

