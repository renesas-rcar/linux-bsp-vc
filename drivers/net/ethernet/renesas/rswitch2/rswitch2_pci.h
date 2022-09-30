/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas RSwitch PCIe device driver
 *
 * Copyright (C) 2021 Renesas Electronics Corporation
 */


#ifndef _RSWITCH2_PCI_H
#define _RSWITCH2_PCI_H

/* FPGA registers */
/* Avalon-MM-to-PCI Express Address Translation Table registers used by this driver */
#define RSWITCH2_PCI_A2P_ADDR_MAP_LO0_REG 0x1000
#define RSWITCH2_PCI_A2P_ADDR_MAP_HI0_REG 0x1004
#define RSWITCH2_PCI_A2P_ADDR_MAP_LO1_REG 0x1008
#define RSWITCH2_PCI_A2P_ADDR_MAP_HI1_REG 0x100C

#define A2P_ADDR_TYPE_32BIT 0
#define A2P_ADDR_TYPE_64BIT 1

/* The device is able to access 2G of memory in 2 pages of 1G via Avalon-MM-PCIe bridge
 * Thus first page is from 0x00000000 - 0x3FFFFFFF
 * and 2nd page is from    0x40000000 - 0x7FFFFFFF
 */
#define RSWITCH2_PCI_A2P_ADDR_SPACE0_TYPE  (A2P_ADDR_TYPE_32BIT)
#define RSWITCH2_PCI_A2P_ADDR_LO0 0x00000000
#define RSWITCH2_PCI_A2P_ADDR_HI0 0x00000000

#define RSWITCH2_PCI_A2P_ADDR_SPACE1_TYPE  (A2P_ADDR_TYPE_32BIT)
#define RSWITCH2_PCI_A2P_ADDR_LO1 0x40000000
#define RSWITCH2_PCI_A2P_ADDR_HI1 0x00000000

#define RSWITCH2_PCI_FPGA_OFFSET 0x01000000
#define RSWITCH2_PCI_FW_SYSID_REG 0x4D40
#define RSWITCH2_PCI_FW_RTLID_REG 0x4D48

/* Offset to RSwitch2 IP within PCI window */
#define RSWITCH2_PCI_IP_OFFSET		0x01040000

/* Rswitch2 IP register offsets */
#define RSWITCH2_MFWD_BASE_ADDR		0x00000000	/* Fabric Register Offset */
#define RSWITCH2_FAB_BASE_ADDR		0x00008000	/* Forwarding Engine Register Offset */
#define RSWITCH2_COMA_BASE_ADDR		0x00009000	/* Common Agent Register Offset */
#define RSWITCH2_TSNA0_BASE_ADDR	0x0000A000	/* TSN Agent Register Offset */
#define RSWITCH2_TSNA1_BASE_ADDR	0x0000C000	/* TSN Agent Register Offset */
#define RSWITCH2_TSNA2_BASE_ADDR	0x0000E000	/* TSN Agent Register Offset */
#define RSWITCH2_TSNA3_BASE_ADDR	0x00010000	/* TSN Agent Register Offset */
#define RSWITCH2_GWCA_BASE_ADDR		0x00012000	/* GWCA Register Offset	*/

#define RSWITCH2_TSN_PORTS			4
#define RSWITCH2_CPU_PORTS			1

#define RSWITCH2_INT_LINE_GPIO		511

#endif /* _RSWITCH2_PCI_H */

