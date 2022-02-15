/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas RSwitch2 Forwarding Engine device driver
 *
 * Copyright (C) 2021 Renesas Electronics Corporation
 *
 */

#ifndef _RSWITCH2_FWD_H
#define _RSWITCH2_FWD_H

#include "rswitch2.h"

#include <linux/bits.h>

#define RSWITCH2_FWD_MAX_HASH_ENTRIES	2048

#define RSW2_FWD_TWBF_N (48)	/* Number of two-byte filters */
#define RSW2_FWD_THBF_N (16)	/* Number of three-byte filters number */
#define RSW2_FWD_FOBF_N (48)	/* Number of four-byte filters number */
#define RSW2_FWD_RAGF_N (16)	/* Number of range-byte filters number */
#define RSW2_FWD_CADF_N (64)	/* Number of cascade filters number */
#define RSW2_FWD_CFMF_N (7)		/* Number of cascade mapped filters */


/* FIXME: Enum error list */
#define EFILTER_LIST_FULL (1)

enum perfect_filter_mode {
	pf_mask_mode = 0,
	pf_expand_mode = 1,
	pf_precise_mode = 2,
};


struct three_byte_filter {
	u8 offset;
	enum perfect_filter_mode mode;
	union {
		u8 val[3];
		u8 mask[3];
		uint id;
	} m;

	union {
		u8 val[6];
		uint id;
	} e;

	union {
		u8 val[3];
		u8 val2[3];
		uint id;
		uint id2;
	} p;
};

struct cascade_filter {
	ulong pframe_bitmap;
	ulong eframe_bitmap;
	struct {
		bool enabled;
		uint id;
	} entry[RSW2_FWD_CFMF_N];
	ulong stream_id;
};




#define RSW2_FWD_FWGC		0x0000 /* Forwarding Engine General Configuration */
#define RSW2_FWD_FWTTC0		0x0010 /* Forwarding Engine TAG TPID Configuration 0 */
#define RSW2_FWD_FWTTC1		0x0014 /* Forwarding Engine TAG TPID Configuration 1 */
#define RSW2_FWD_FWCEPTC	0x0020 /* Forwarding Engine CPU Exceptional Path Target Configuration */
#define RSW2_FWD_FWCEPRC0	0x0024 /* Forwarding Engine CPU Exceptional Path Reason Configuration 0 */
#define RSW2_FWD_FWCEPRC1	0x0028 /* Forwarding Engine CPU Exceptional Path Reason Configuration 1 */
#define RSW2_FWD_FWCEPRC2	0x002C /* Forwarding Engine CPU Exceptional Path Reason Configuration 2 */
#define RSW2_FWD_FWCLPTC	0x0030 /* Forwarding Engine CPU Learning Path Target Configuration */
#define RSW2_FWD_FWCLPRC	0x0034 /* Forwarding Engine CPU Learning Path Reason Configuration */
#define RSW2_FWD_FWCMPTC	0x0040 /* Forwarding Engine CPU Mirroring Path Target Configuration */
#define RSW2_FWD_FWEMPTC	0x0044 /* Forwarding Engine Ethernet Mirroring Path Target Configuration */
#define RSW2_FWD_FWSDMPTC	0x0050 /* Forwarding Engine Source-Destination Mirroring Path Target Configuration */
#define RSW2_FWD_FWSDMPVC	0x0054 /* Forwarding Engine Source-Destination Mirroring Path Vector Configuration */

/* Forwarding Engine Level Based WaterMark Configuration i (i=0..PORT_N) */
#define RSW2_FWD_FWLBWMC(i)	(0x0080 + (0x4 * (i)))

/* Forwarding Engine Port Configuration 0 i (i=0..PORT_N) */
#define RSW2_FWD_FWPC0(i)	(0x0100 + (0x10 * (i)))
#define FWPC0_VLANRUS	BIT_MASK(30)
#define FWPC0_VLANRU	BIT_MASK(29)
#define FWPC0_VLANSA	BIT_MASK(28)
#define FWPC0_MACHMA	BIT_MASK(27)
#define FWPC0_MACHLA	BIT_MASK(26)
#define FWPC0_MACRUSSA	BIT_MASK(25)
#define FWPC0_MACRUSA	BIT_MASK(24)
#define FWPC0_MACSSA	BIT_MASK(23)
#define FWPC0_MACRUDSA	BIT_MASK(22)
#define FWPC0_MACRUDA	BIT_MASK(21)
#define FWPC0_MACDSA	BIT_MASK(20)
#define FWPC0_L2SE		BIT_MASK(9)
#define FWPC0_IP6OE		BIT_MASK(8)
#define FWPC0_IP6TE		BIT_MASK(7)
#define FWPC0_IP6UE		BIT_MASK(6)
#define FWPC0_IP4OE		BIT_MASK(5)
#define FWPC0_IP4TE		BIT_MASK(4)
#define FWPC0_IP4UE		BIT_MASK(3)
#define FWPC0_LTHRUSS	BIT_MASK(2)
#define FWPC0_LTHRUS	BIT_MASK(1)
#define FWPC0_LTHTA		BIT_MASK(0)

/* Forwarding Engine Port Configuration 1 i (i=0..PORT_N) */
#define RSW2_FWD_FWPC1(i)	(0x0104 + (0x10 * (i)))
#define FWPC1_LTHFM		GENMASK(22, 16)
#define FWPC1_DDSL		BIT_MASK(1)
#define FWPC1_DDE		BIT_MASK(0)

/* Forwarding Engine Port Configuration 2 i (i=0..PORT_N) */
#define RSW2_FWD_FWPC2(i)		(0x0108 + (0x10 * (i)))

/* ForWarding engine TWo Byte Filter Configuration i (i=0..PFL_TWBF_N-1) */
#define RSW2_FWD_FWTWBFC(i)		(0x1000 + (0x10 * (i)))

/* ForWarding engine TWo Byte Filter Value Configuration i (i=0..PFL_TWBF_N-1) */
#define RSW2_FWD_FWTWBFVC(i)	(0x1004 + (0x10 * (i)))

/* ForWarding engine THree Byte Filter Configuration i (i=0..PFL_THBF_N-1) */
#define RSW2_FWD_FWTHBFC(i)		(0x1400 + (0x10 * (i)))
#define FWTHBFC_THBFOV		GENMASK(23, 16)
#define FWTHBFC_THBFUM		GENMASK(1, 0)

/* ForWarding engine THree Byte Filter Value Configuration 0 i (i=0..PFL_THBF_N-1) */
#define RSW2_FWD_FWTHBFV0C(i)	(0x1404 + (0x10 * (i)))
#define FWTHBFV0C_THBFV0B2 		GENMASK(23, 16)
#define FWTHBFV0C_THBFV0B1 		GENMASK(15, 8)
#define FWTHBFV0C_THBFV0B0 		GENMASK(7, 0)

/* ForWarding engine THree Byte Filter Value Configuration 1 i (i=0..PFL_THBF_N-1) */
#define RSW2_FWD_FWTHBFV1C(i)	(0x1408 + (0x10 * (i)))
#define FWTHBFV0C_THBFV1B2 		GENMASK(23, 16)
#define FWTHBFV0C_THBFV1B1 		GENMASK(15, 8)
#define FWTHBFV0C_THBFV1B0 		GENMASK(7, 0)

/* ForWarding engine FOur Byte Filter Configuration i (i=0..PFL_FOBF_N-1) */
#define RSW2_FWD_FWFOBFC(i)		(0x1800 + (0x10 * (i)))

/* ForWarding engine FOur Byte Filter Value 0 Configuration i (i=0..PFL_FOBF_N-1) */
#define RSW2_FWD_FWFOBFV0C(i)	(0x1804 + (0x10 * (i)))

/* ForWarding engine FOur Byte Filter Value 1 Configuration i (i=0..PFL_FOBF_N-1) */
#define RSW2_FWD_FWFOBFV1C(i)	(0x1808 + (0x10 * (i)))

/* Range Filter Configuration i (i=0..PFL_RAGF_N-1) */
#define RSW2_FWD_FWRFC(i)		(0x1C00 + (0x10 * (i)))

/* Forwarding Engine Range Filter Value Configuration i (i=0..PFL_RAGF_N-1) */
#define RSW2_FWD_FWRFVC(i)		(0x1C04 + (0x10 * (i)))

/* Forwarding Engine Cascade Filter Configuration i (i=0..PFL_CADF_N-1) */
#define RSW2_FWD_FWCFC(i)		(0x2000 + (0x40 * (i)))
#define FWCFC_CFPFFV			GENMASK(19, 16) /* Cascade Filter P-Frame Filter Valid */
#define FWCFC_CFEFFV			GENMASK( 6,  0) /* Cascade Filter E-Frame Filter Valid */

/* Forwarding Engine Cascade Filter Mapping Config. i j (i=0..PFL_CADF_N-1) (j=0..PFL_CFMF_N)*/
#define RSW2_FWD_FWCFMC(i, j)	(0x2004 + (0x40 * (i)) + (0x4 * (j)))
#define FWCFMC_CFFV				BIT_MASK(15)
#define FWCFMC_CFFN				GENMASK(14, 0)

#define RSW2_FWD_FWIP4SC	0x4008 /* Forwarding Engine IPv4 Stream Configuration */
#define RSW2_FWD_FWIP6SC	0x4018 /* Forwarding Engine IPv6 Stream Configuration */
#define RSW2_FWD_FWIP6OC	0x401C /* Forwarding Engine IPv6 Offset Configuration */
#define RSW2_FWD_FWL2SC		0x4020 /* Forwarding Engine Layer 2 Stream Configuration */
#define RSW2_FWD_FWSFHEC	0x4030 /* Forwarding Engine Stream Filter Hash Equation Configuration */
#define RSW2_FWD_FWSHCR0	0x4040 /* Forwarding Engine Software Hash Calculation Request 0 */
#define RSW2_FWD_FWSHCR1	0x4044 /* Forwarding Engine Software Hash Calculation Request 1 */
#define RSW2_FWD_FWSHCR2	0x4048 /* Forwarding Engine Software Hash Calculation Request 2 */
#define RSW2_FWD_FWSHCR3	0x404C /* Forwarding Engine Software Hash Calculation Request 3 */
#define RSW2_FWD_FWSHCR4	0x4050 /* Forwarding Engine Software Hash Calculation Request 4 */
#define RSW2_FWD_FWSHCR5	0x4054 /* Forwarding Engine Software Hash Calculation Request 5 */
#define RSW2_FWD_FWSHCR6	0x4058 /* Forwarding Engine Software Hash Calculation Request 6 */
#define RSW2_FWD_FWSHCR7	0x405C /* Forwarding Engine Software Hash Calculation Request 7 */
#define RSW2_FWD_FWSHCR8	0x4060 /* Forwarding Engine Software Hash Calculation Request 8 */
#define RSW2_FWD_FWSHCR9	0x4064 /* Forwarding Engine Software Hash Calculation Request 9 */
#define RSW2_FWD_FWSHCR10	0x4068 /* Forwarding Engine Software Hash Calculation Request 10*/
#define RSW2_FWD_FWSHCR11	0x406C /* Forwarding Engine Software Hash Calculation Request 11*/
#define RSW2_FWD_FWSHCR12	0x4070 /* Forwarding Engine Software Hash Calculation Request 12*/
#define RSW2_FWD_FWSHCR13	0x4074 /* Forwarding Engine Software Hash Calculation Request 13*/
#define RSW2_FWD_FWSHCRR	0x4078 /* Forwarding Engine Software Hash Calculation Request Result */
#define RSW2_FWD_FWLTHHEC	0x4090 /* Forwarding Engine L3 Hash Entry Configuration */
#define FWLTHHEC_LTHHMUE	GENMASK(26, 16)
#define FWLTHHEC_LTHHMC		GENMASK( 9, 0)
#define RSW2_FWD_FWLTHHC	0x4094 /* Forwarding Engine L3 Hash Configuration */
#define RSW2_FWD_FWLTHTL0	0x40A0 /* Forwarding Engine L3 Table Learn 0 */
#define RSW2_FWD_FWLTHTL1	0x40A4 /* Forwarding Engine L3 Table Learn 1 */
#define RSW2_FWD_FWLTHTL2	0x40A8 /* Forwarding Engine L3 Table Learn 2 */
#define RSW2_FWD_FWLTHTL3	0x40AC /* Forwarding Engine L3 Table Learn 3 */
#define RSW2_FWD_FWLTHTL4	0x40B0 /* Forwarding Engine L3 Table Learn 4 */
#define RSW2_FWD_FWLTHTL5	0x40B4 /* Forwarding Engine L3 Table Learn 5 */
#define RSW2_FWD_FWLTHTL6	0x40B8 /* Forwarding Engine L3 Table Learn 5 */
#define RSW2_FWD_FWLTHTL7	0x40BC /* Forwarding Engine L3 Table Learn 7 */
#define FWLTHTL7_LTHSLVL	GENMASK(22, 16)	/* L3 Source Lock Vector Learn */
#define FWLTHTL7_LTHRVL		BIT_MASK(15)	/* L3 Routing Number Learn Valid */
#define FWLTHTL7_LTHRNL		GENMASK(7, 0)	/* L3 Routing Number Learn */

/* Forwarding Engine L3 Table Learn 8 i (i=0..PORT_GWCA_N-1) */
#define RSW2_FWD_FWLTHTL8(i)	(0x40C0 + (0x4 * (i)))
#define FWLTHTL8_LTHCSDL	GENMASK(6, 0)	/* L3 CPU Sub-Destination Learn */

#define RSW2_FWD_FWLTHTL9	0x40D0 /* Forwarding Engine L3 Table Learn 9 */
#define FWLTHTL9_LTHCMEL	BIT_MASK(21)  /* L3 CPU Mirroring Enable Learn */
#define FWLTHTL9_LTHEMEL 	BIT_MASK(20)  /* L3 Ethernet Mirroring Enable Learn */
#define FWLTHTL9_LTHIPUL 	BIT_MASK(19)  /* L3 Internal Priority Update Learn */
#define FWLTHTL9_LTHIPVL 	GENMASK(18, 16) /* L3 Internal Priority Value Learn */
#define FWLTHTL9_LTHDVL 	GENMASK(6, 0)	/* L3 Destination Vector Learn */
#define RSW2_FWD_FWLTHTLR	0x40D4 /* Forwarding Engine L3 Table Learn Result */
#define FWLTHTLR_LTHTL 		BIT_MASK(31) /* L3 Table Learn */
#define FWLTHTLR_LTHLCN		GENMASK(25, 16)
#define FWLTHTLR_LTHLO BIT_MASK(3) /* L3 Learn Overwrite */
#define FWLTHTLR_LTHLEF BIT_MASK(2) /* L3 Learn ECC Fail */
#define FWLTHTLR_LTHLSF BIT_MASK(1) /* L3 Learn Security Fail */
#define FWLTHTLR_LTHLF BIT_MASK(0) /* L3 Learn Fail */
#define RSW2_FWD_FWLTHTIM	0x40E0 /* Forwarding Engine L3 Table Initialization Monitoring */
#define FWLTHTIM_LTHTR		BIT_MASK(1)
#define FWLTHTIM_LTHTIOG	BIT_MASK(0)
#define RSW2_FWD_FWLTHTEM	0x40E4 /* Forwarding Engine L3 Table Entry Monitoring */
#define RSW2_FWD_FWLTHTS0	0x4100 /* Forwarding Engine L3 Table Search 0 */
#define RSW2_FWD_FWLTHTS1	0x4104 /* Forwarding Engine L3 Table Search 1 */
#define RSW2_FWD_FWLTHTS2	0x4108 /* Forwarding Engine L3 Table Search 2 */
#define RSW2_FWD_FWLTHTS3	0x410C /* Forwarding Engine L3 Table Search 3 */
#define RSW2_FWD_FWLTHTS4	0x4110 /* Forwarding Engine L3 Table Search 4 */
#define RSW2_FWD_FWLTHTSR0	0x4120 /* Forwarding Engine L3 Table Search Result 0 */
#define RSW2_FWD_FWLTHTSR1	0x4124 /* Forwarding Engine L3 Table Search Result 1 */
#define RSW2_FWD_FWLTHTSR2	0x4128 /* Forwarding Engine L3 Table Search Result 2 */
#define RSW2_FWD_FWLTHTSR3	0x412C /* Forwarding Engine L3 Table Search Result 3 */

/* Forwarding Engine L3 Table Search Result 4 i (i=0..PORT_GWCA_N-1) */
#define RSW2_FWD_FWLTHTSR4(i)	(0x4130 + (0x4 * (i)))

#define RSW2_FWD_FWLTHTSR5	0x4140 /* Forwarding Engine L3 Table Search Result 5 */
#define RSW2_FWD_FWLTHTR	0x4150 /* Forwarding Engine L3 Table Read */
#define RSW2_FWD_FWLTHTRR0	0x4154 /* Forwarding Engine L3 Table Read Result 0 */
#define RSW2_FWD_FWLTHTRR1	0x4158 /* Forwarding Engine L3 Table Read Result 1 */
#define RSW2_FWD_FWLTHTRR2	0x415C /* Forwarding Engine L3 Table Read Result 2 */
#define RSW2_FWD_FWLTHTRR3	0x4160 /* Forwarding Engine L3 Table Read Result 3 */
#define RSW2_FWD_FWLTHTRR4	0x4164 /* Forwarding Engine L3 Table Read Result 4 */
#define RSW2_FWD_FWLTHTRR5	0x4168 /* Forwarding Engine L3 Table Read Result 5 */
#define RSW2_FWD_FWLTHTRR6	0x416C /* Forwarding Engine L3 Table Read Result 6 */
#define RSW2_FWD_FWLTHTRR7	0x4170 /* Forwarding Engine L3 Table Read Result 7 */
#define RSW2_FWD_FWLTHTRR8	0x4174 /* Forwarding Engine L3 Table Read Result 8 */

/* Forwarding Engine L3 Table Read Result 9 i (i=0..PORT_GWCA_N-1) */
#define RSW2_FWD_FWLTHTRR9(i)	(0x4180 + (0x4 * (i)))

#define RSW2_FWD_FWLTHTRR10	0x4190 /* Forwarding Engine L3 Table Read Result 10 */

#define RSW2_FWD_FWMACHEC	0x4620 /* Forwarding Engine MAC Hash Entry Configuration */
#define FWMACHEC_MACHMUE	GENMASK(26, 16)
#define FWMACHEC_MACHMC		GENMASK(9, 0)


#define RSW2_FWD_FWMACHC	0x4624 /* Forwarding Engine MAC Hash Configuration */
#define RSW2_FWD_FWMACTL0	0x4630 /* Forwarding Engine MAC Table Learn 0 */
#define FWMACTL0_MACED		BIT_MASK(16)

#define RSW2_FWD_FWMACTL1	0x4634 /* Forwarding Engine MAC Table Learn 1 */
#define FWMACTL1_MACMALP0	GENMASK(15, 0)

#define RSW2_FWD_FWMACTL2	0x4638 /* Forwarding Engine MAC Table Learn 2 */
#define FWMACTL2_MACMALP1	GENMASK(31, 0)

#define RSW2_FWD_FWMACTL3	0x463C /* Forwarding Engine MAC Table Learn 3 */
#define FWMACTL3_MACDSLVL	GENMASK(22, 16)
#define FWMACTL3_MACSSLVL	GENMASK(6, 0)

/* Forwarding Engine MAC Table Learn 4 (i=0..PORT_GWCA_N-1) */
#define RSW2_FWD_FWMACTL4(i)	(0x4640 + (0x4 * (i)))
#define FWMACTL4_MACCSDL		GENMASK(6, 0)

#define RSW2_FWD_FWMACTL5	0x4650 /* Forwarding Engine MAC Table Learn 5 */
#define FWMACTL5_MACCMEL	BIT_MASK(21)
#define FWMACTL5_MACEMEL	BIT_MASK(20)
#define FWMACTL5_MACIPUL	BIT_MASK(19)
#define FWMACTL5_MACIPVL	GENMASK(18, 16)
#define FWMACTL5_MACDVL		GENMASK(6, 0)

#define RSW2_FWD_FWMACTLR	0x4654 /* Forwarding Engine MAC Table Learn Result */
#define FWMACTLR_MACTL		BIT_MASK(31)
#define FWMACTLR_MACLCN		GENMASK(25, 16)
#define FWMACTLR_MACLO		BIT_MASK(3)
#define FWMACTLR_MACLEF		BIT_MASK(2)
#define FWMACTLR_MACLSF		BIT_MASK(1)
#define FWMACTLR_MACLF		BIT_MASK(0)


#define RSW2_FWD_FWMACTIM	0x4660 /* Forwarding Engine MAC Table Initialization Monitoring */
#define FWMACTIM_MACTR		BIT_MASK(1)
#define FWMACTIM_MACTIOG	BIT_MASK(0)

#define RSW2_FWD_FWMACTEM	0x4664 /* Forwarding Engine MAC Table Entry Monitoring */
#define FWMACTEM_MACTUEN	GENMASK(28, 16)
#define FWMACTEM_MACTEN		GENMASK(10, 0)

#define RSW2_FWD_FWMACTS0	0x4670 /* Forwarding Engine MAC Table Search 0 */
#define RSW2_FWD_FWMACTS1	0x4674 /* Forwarding Engine MAC Table Search 1 */
#define RSW2_FWD_FWMACTSR0	0x4678 /* Forwarding Engine MAC Table Search Result 0 */
#define RSW2_FWD_FWMACTSR1	0x467C /* Forwarding Engine MAC Table Search Result 1 */

/* Forwarding Engine MAC Table Search Result 2 (i=0..PORT_GWCA_N-1) */
#define RSW2_FWD_FWMACTSR2(i)	(0x4680 + (0x4 * (i)))

#define RSW2_FWD_FWMACTSR3	0x4690 /* Forwarding Engine MAC Table Search Result 3 */
#define RSW2_FWD_FWMACTR	0x46A0 /* Forwarding Engine MAC Table Read */
#define RSW2_FWD_FWMACTRR0	0x46A4 /* Forwarding Engine MAC Table Read Result 0 */
#define RSW2_FWD_FWMACTRR1	0x46A8 /* Forwarding Engine MAC Table Read Result 1 */
#define RSW2_FWD_FWMACTRR2	0x46AC /* Forwarding Engine MAC Table Read Result 2 */
#define RSW2_FWD_FWMACTRR3	0x46B0 /* Forwarding Engine MAC Table Read Result 3 */
#define RSW2_FWD_FWMACTRR4	0x46B4 /* Forwarding Engine MAC Table Read Result 4 */

/* Forwarding Engine MAC Table Read Result 5 (i=0..PORT_GWCA_N-1) */
#define RSW2_FWD_FWMACTRR5(i)	(0x46C0 + (0x4 * (i)))

#define RSW2_FWD_FWMACTRR6	0x46D0 /* Forwarding Engine MAC Table Read Result 6 */

#define RSW2_FWD_FWMACAGUSP	0x4880 /* Forwarding Engine MAC AGing US Prescaler Configuration*/
#define FWMACAGUSP_MACAGUSP	GENMASK(9, 0)

#define RSW2_FWD_FWMACAGC	0x4884 /* Forwarding Engine MAC AGing Configuration */
#define FWMACAGC_MACDESOG	BIT_MASK(29)
#define FWMACAGC_MACAGOG	BIT_MASK(28)
#define FWMACAGC_MACDES		BIT_MASK(24)
#define FWMACAGC_MACAGPM	BIT_MASK(18)
#define FWMACAGC_MACAGSL	BIT_MASK(17)
#define FWMACAGC_MACAGE		BIT_MASK(16)
#define FWMACAGC_MACAGT		GENMASK(15, 0)

#define RSW2_FWD_FWMACAGM0	0x4888 /* Forwarding Engine MAC AGing Monitoring 0 */
#define RSW2_FWD_FWMACAGM1	0x488C /* Forwarding Engine MAC AGing Monitoring 1 */

#define RSW2_FWD_FWVLANTEC	0x4900 /* Forwarding Engine VLAN Table Entry Configuration */
#define FWVLANTEC_VLANTMUE	GENMASK(28, 16)

#define RSW2_FWD_FWVLANTL0		0x4910 /* Forwarding Engine VLAN Table Learn 0 */
#define RSW2_FWD_FWVLANTL1		0x4914 /* Forwarding Engine VLAN Table Learn 1 */
#define RSW2_FWD_FWVLANTL2		0x4918 /* Forwarding Engine VLAN Table Learn 2 */

/* Forwarding Engine VLAN Table Learn 3 (i=0..PORT_GWCA_N-1) */
#define RSW2_FWD_FWVLANTL3(i)	(0x4920 + (0x4 * (i)))

#define RSW2_FWD_FWVLANTL4		0x4930 /* Forwarding Engine VLAN Table Learn 4 */
#define RSW2_FWD_FWVLANTLR		0x4934 /* Forwarding Engine VLAN Table Learn Result */
#define RSW2_FWD_FWVLANTIM		0x4940 /* Forwarding Engine VLAN Table Initialization Monitoring */
#define FWVLANTIM_VLANTR		BIT_MASK(1)
#define FWVLANTIM_VLANTIOG		BIT_MASK(0)

#define RSW2_FWD_FWVLANTEM		0x4944 /* Forwarding Engine VLAN Table Entry Monitoring */
#define RSW2_FWD_FWVLANTS		0x4950 /* Forwarding Engine VLAN Table Search */
#define RSW2_FWD_FWVLANTSR0		0x4954 /* Forwarding Engine VLAN Table Search Result 0 */
#define RSW2_FWD_FWVLANTSR1		0x4958 /* Forwarding Engine VLAN Table Search Result 1 */

/* Forwarding Engine VLAN Table Search Result 2 (i=0..PORT_GWCA_N-1) */
#define RSW2_FWD_FWVLANTSR2(i)	(0x4960 + (0x4 * (i)))

#define RSW2_FWD_FWVLANTSR3		0x4970 /* Forwarding Engine VLAN Table Search Result 3 */

/* Forwarding Engine Port Based Forwarding Configuration i (i=0..PORT_N) */
#define RSW2_FWD_FWPBFC(i)		(0x4A00 + (0x10 * (i)))
#define FWPBFC_FAIFP	BIT_MASK(1)
#define FWPBFC_IP6PDE	BIT_MASK(1)
#define FWPBFC_IP4PDM	BIT_MASK(1)
#define FWPBFC_IP4PDE	BIT_MASK(1)
#define FWPBFC_PBSL		BIT_MASK(1)
#define FWPBFC_PBCME	BIT_MASK(1)
#define FWPBFC_PBEME	BIT_MASK(1)
#define FWPBFC_PBIPU	BIT_MASK(1)
#define FWPBFC_PBIPV	GENMASK(18, 16)
#define FWPBFC_PBDV		GENMASK(6, 0)

/* Forwarding Engine Port Based Forwarding CSD Configuration j i (j= 0..PORT_GWCA_N-1) (i= 0..PORT_N-1)*/
#define RSW2_FWD_FWPBFCSDC(j, i)	(0x4A04 + (0x10 * (i)) + (0x4 * (j)))

#define RSW2_FWD_FWL23URL0		0x4E00 /* Forwarding Engine Layer2/Layer3 Update Rule Learn 0 */
#define RSW2_FWD_FWL23URL1		0x4E04 /* Forwarding Engine Layer2/Layer3 Update Rule Learn 1 */
#define RSW2_FWD_FWL23URL2		0x4E08 /* Forwarding Engine Layer2/Layer3 Update Rule Learn 2 */
#define RSW2_FWD_FWL23URL3		0x4E0C /* Forwarding Engine Layer2/Layer3 Update Rule Learn 3 */
#define RSW2_FWD_FWL23URLR		0x4E10 /* Forwarding Engine Layer2/Layer3 Update Rule Learn Result */
#define RSW2_FWD_FWL23UTIM		0x4E20 /* Forwarding Engine Layer2/Layer3 Update Table Initialization Monitoring */
#define RSW2_FWD_FWL23URR		0x4E30 /* Forwarding Engine Layer2/Layer3 Update Rule Read */
#define RSW2_FWD_FWL23URRR0		0x4E34 /* Forwarding Engine Layer2/Layer3 Update Rule Read Result 0 */
#define RSW2_FWD_FWL23URRR1		0x4E38 /* Forwarding Engine Layer2/Layer3 Update Rule Read Result 1 */
#define RSW2_FWD_FWL23URRR2		0x4E3C /* Forwarding Engine Layer2/Layer3 Update Rule Read Result 2 */
#define RSW2_FWD_FWL23URRR3		0x4E40 /* Forwarding Engine Layer2/Layer3 Update Rule Read Result 3 */

/* Forwarding Engine Layer2/Layer3 Update ReMapping Configuration i (i=0..LTH_REMAP_N) */
#define RSW2_FWD_FWL23URMC(i)	(0x4F00 + (0x4 * (i)))

/* Forwarding Engine PSFP MSDU Filter Global Configuration i (i=0..PSFP_MSDU_N-1) */
#define RSW2_FWD_FWPMFGC(i)		(0x5000 + (0x4 * (i)))

/* Forwarding Engine PSFP Gate Filter Configuration i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFC(i)		(0x5100 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Initial Gate State Configuration i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFIGSC(i)	(0x5104 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Entry Number Configuration i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFENC(i)	(0x5108 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Entry Number Monitoring i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFENM(i)	(0x510C + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Cycle Start Time Configuration 0 i (i=0..PSFP_GATE_N-1)*/
#define RSW2_FWD_FWPGFCSTC0(i)	(0x5110 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Cycle Start Time Configuration 1 i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFCSTC1(i)	(0x5114 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Cycle Start Time Monitoring 0 i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFCSTM0(i)	(0x5118 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Cycle Start Time Monitoring 1 i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFCSTM1(i)	(0x511C + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Cycle Time Configuration i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFCTC(i)	(0x5120 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Cycle Time Monitoring i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFCTM(i)	(0x5124 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Hardware Calibration Configuration i (i=0..PSFP_GATE_N-1) */
#define RSW2_FWD_FWPGFHCC(i)	(0x5128 + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Status Monitoring i */
#define RSW2_FWD_FWPGFSM(i)		(0x512C + (0x40 * (i)))

/* Forwarding Engine PSFP Gate Filter Global Configuration i */
#define RSW2_FWD_FWPGFGC(i)		(0x5130 + (0x40 * (i)))

#define RSW2_FWD_FWPGFGL0		0x5500 /* Forwarding Engine PSFP Gate Filter Gate Learn 0 */
#define RSW2_FWD_FWPGFGL1		0x5504 /* Forwarding Engine PSFP Gate Filter Gate Learn 1 */
#define RSW2_FWD_FWPGFGLR		0x5508 /* Forwarding Engine PSFP Gate Filter Gate Learn Result */
#define RSW2_FWD_FWPGFGR		0x5510 /* Forwarding Engine PSFP Gate Filter Gate Read */
#define RSW2_FWD_FWPGFGRR0		0x5514 /* Forwarding Engine PSFP Gate Filter Read Result 0 */
#define RSW2_FWD_FWPGFGRR1		0x5518 /* Forwarding Engine PSFP Gate Filter Read Result 1 */
#define RSW2_FWD_FWPGFRIM		0x5520 /* Forwarding Engine PSFP Gate Filter RAM Initialization Monitoring */

/* Forwarding Engine PSFP MeTeR Filter Configuration i (i=0..PSFP_MTR_N-1) */
#define RSW2_FWD_FWPMTRFC(i)	(0x5600 + (0x20 * (i)))

/* Forwarding Engine PSFP MeTeR CBS Configuration i (i=0..PSFP_MTR_N-1) */
#define RSW2_FWD_FWPMTRCBSC(i)	(0x5604 + (0x20 * (i)))

/* Forwarding Engine PSFP MeTeR CIR Configuration i (i=0..PSFP_MTR_N-1) */
#define RSW2_FWD_FWPMTRCIRC(i)	(0x5608 + (0x20 * (i)))

/* Forwarding Engine PSFP MeTeR EBS Configuration i (i=0..PSFP_DMTR_N-1) */
#define RSW2_FWD_FWPMTREBSC(i)	(0x560C + (0x20 * (i)))

/* Forwarding Engine PSFP MeTeR EIR Configuration i (i=0..PSFP_DMTR_N-1) */
#define RSW2_FWD_FWPMTREIRC(i)	(0x5610 + (0x20 * (i)))

/* Forwarding Engine PSFP MeTeR Filter Monitoring i (i=0..PSFP_MTR_N-1) */
#define RSW2_FWD_FWPMTRFM(i)	(0x5614 + (0x20 * (i)))

/* Forwarding Engine Direct Descriptor Forwarded Descriptor CouNter i */
#define RSW2_FWD_FWDDFDCN(i)	(0x6300 + (0x20 * (i)))

/* Forwarding Engine Direct Descriptor Forwarded Descriptor CouNter Emu i (i=0..PORT_SLOW_N-1) */
#define RSW2_FWD_FWDDFDCNE(i)	(0x6E00 + (0x20 * (i)))

/* Forwarding Engine Layer 3 Forwarded Descriptor CouNter i */
#define RSW2_FWD_FWLTHFDCN(i)	(0x6304 + (0x20 * (i)))

/* Forwarding Engine Layer 3 Forwarded Descriptor CouNter Emu i */
#define RSW2_FWD_FWLTHFDCNE(i)	(0x6E04 + (0x20 * (i)))

/* Forwarding Engine Layer 2 Forwarded Descriptor CouNter i */
#define RSW2_FWD_FWLTWFDCN(i)	(0x630C + (0x20 * (i)))

/* Forwarding Engine Layer 2 Forwarded Descriptor CouNter Emu i */
#define RSW2_FWD_FWLTWFDCNE(i)	(0x6E0C + (0x20 * (i)))

/* Forwarding Engine Port Based Forwarded Descriptor CouNter i */
#define RSW2_FWD_FWPBFDCN(i)	(0x6310 + (0x20 * (i)))

/* Forwarding Engine Port Based Forwarded Descriptor CouNter Emu i */
#define RSW2_FWD_FWPBFDCNE(i)	(0x6E10 + (0x20 * (i)))

/* Forwarding Engine MAC Hardware Learn CouNter i */
#define RSW2_FWD_FWMHLCN(i)		(0x6314 + (0x20 * (i)))

/* Forwarding Engine MAC Hardware Learn CouNter Emu i */
#define RSW2_FWD_FWMHLCNE(i)	(0x6E14 + (0x20 * (i)))


#define RSW2_FWD_FWWMRDCN(i)	(0x6504 + (0x20 * (i))) /* Forwarding Engine WaterMark Rejected Descriptor CouNter i */
#define RSW2_FWD_FWWMRDCNE(i)	(0x7004 + (0x20 * (i))) /* Forwarding Engine WaterMark Rejected Descriptor CouNter Emu i */
#define RSW2_FWD_FWDDRDCN(i)	(0x6508 + (0x20 * (i))) /* Forwarding Engine Direct Descriptor Rejected Descriptor CouNter i */
#define RSW2_FWD_FWDDRDCNE(i)	(0x7008 + (0x20 * (i))) /* Forwarding Engine Direct Descriptor Rejected Descriptor CouNter Emu i */
#define RSW2_FWD_FWLTHRDCN(i)	(0x650C + (0x20 * (i))) /* Forwarding Engine Layer 3 Rejected Descriptor CouNter i */
#define RSW2_FWD_FWLTHRDCNE(i)	(0x700C + (0x20 * (i))) /* Forwarding Engine Layer 3 Rejected Descriptor CouNter Emu i*/
#define RSW2_FWD_FWLTWRDCN(i)	(0x6514 + (0x20 * (i))) /* Forwarding Engine Layer 2 Rejected Descriptor CouNter i */
#define RSW2_FWD_FWLTWRDCNE(i)	(0x7014 + (0x20 * (i))) /* Forwarding Engine Layer 2 Rejected Descriptor CouNter Emu i */
#define RSW2_FWD_FWPBRDCN(i)	(0x6518 + (0x20 * (i))) /* Forwarding Engine Port Based Rejected Descriptor CouNter i */
#define RSW2_FWD_FWPBRDCNE(i)	(0x7018 + (0x20 * (i))) /* Forwarding Engine Port Based Rejected Descriptor CouNter Emu i */

#define RSW2_FWD_FWPMFDCN(i)	(0x6700 + (0x4 * (i))) /* Forwarding Engine PSFP MSDU Filtered Descriptor CouNter i*/
#define RSW2_FWD_FWPMFDCNE(i)	(0x7200 + (0x4 * (i))) /* Forwarding Engine PSFP MSDU Filtered Descriptor CouNter Emu i*/
#define RSW2_FWD_FWPGFDCN(i)	(0x6780 + (0x4 * (i))) /* Forwarding Engine PSFP Gate Filtered Descriptor CouNter i */
#define RSW2_FWD_FWPGFDCNE(i)	(0x7280 + (0x4 * (i))) /* Forwarding Engine PSFP Gate Filtered Descriptor CouNter Emu i */
#define RSW2_FWD_FWPMGDCN(i)	(0x6800 + (0x10 * (i))) /* Forwarding Engine PSFP Meter Green Descriptor CouNter i */
#define RSW2_FWD_FWPMGDCNE(i)	(0x7300 + (0x10 * (i))) /* Forwarding Engine PSFP Meter Green Descriptor CouNter Emu i*/
#define RSW2_FWD_FWPMYDCN(i)	(0x6804 + (0x10 * (i))) /* Forwarding Engine PSFP Meter Yellow Descriptor CouNter i */
#define RSW2_FWD_FWPMYDCNE(i)	(0x7304 + (0x10 * (i))) /* Forwarding Engine PSFP Meter Yellow Descriptor CouNter Emu i */
#define RSW2_FWD_FWPMRDCN(i)	(0x6808 + (0x10 * (i))) /* Forwarding Engine PSFP Meter Red Descriptor CouNter i */
#define RSW2_FWD_FWPMRDCNE(i)	(0x7308 + (0x10 * (i))) /* Forwarding Engine PSFP Meter Red Descriptor CouNter Emu i */


/* Forwarding Engine FRER Passed Packet CouNter i (i=0..FRER_RECE_N-1) */
#define RSW2_FWD_FWFRPPCN(i)	(0x6A00 + (0x8 * (i)))
/* Forwarding Engine FRER Passed Packet CouNter Emu i (i=0..FRER_RECE_N-1) */
#define RSW2_FWD_FWFRPPCNE(i)	(0x7500 + (0x8 * (i)))

/* Forwarding Engine FRER Discarded Packet CouNter i (i=0..FRER_RECE_N-1) */
#define RSW2_FWD_FWFRDPCN(i)	(0x6A04 + (0x8 * (i)))
/* Forwarding Engine FRER Discarded Packet CouNter Emu i (i=0..FRER_RECE_N-1) */
#define RSW2_FWD_FWFRDPCNE(i)	(0x7504 + (0x8 * (i)))

/* Forwarding Engine Error Interrupt Status 0 i */
#define RSW2_FWD_FWEIS0(i)	(0x7900 + (0x10 * (i)))

/* Forwarding Engine Error Interrupt Enable 0 i */
#define RSW2_FWD_FWEIE0(i)	(0x7904 + (0x10 * (i)))

/* Forwarding Engine Error Interrupt Disable 0 i (i=0..PORT_N) */
#define RSW2_FWD_FWEID0(i)	(0x7908 + (0x10 * (i)))

#define RSW2_FWD_FWEIS1		0x7A00 /* Forwarding Engine Error Interrupt Status 1 */
#define RSW2_FWD_FWEIE1		0x7A04 /* Forwarding Engine Error Interrupt Enable 1 */
#define RSW2_FWD_FWEID1		0x7A08 /* Forwarding Engine Error Interrupt Disable 1 */
#define RSW2_FWD_FWEIS2		0x7A10 /* Forwarding Engine Error Interrupt Status 2 */
#define RSW2_FWD_FWEIE2		0x7A14 /* Forwarding Engine Error Interrupt Enable 2 */
#define RSW2_FWD_FWEID2		0x7A18 /* Forwarding Engine Error Interrupt Disable 2 */
#define RSW2_FWD_FWEIS3		0x7A20 /* Forwarding Engine Error Interrupt Status 3 */
#define RSW2_FWD_FWEIE3		0x7A24 /* Forwarding Engine Error Interrupt Enable 3 */
#define RSW2_FWD_FWEID3		0x7A28 /* Forwarding Engine Error Interrupt Disable 3 */
#define RSW2_FWD_FWEIS4		0x7A30 /* Forwarding Engine Error Interrupt Status 4 */
#define RSW2_FWD_FWEIE4		0x7A34 /* Forwarding Engine Error Interrupt Enable 4 */
#define RSW2_FWD_FWEID4		0x7A38 /* Forwarding Engine Error Interrupt Disable 4 */
#define RSW2_FWD_FWEIS5		0x7A40 /* Forwarding Engine Error Interrupt Status 5 */
#define RSW2_FWD_FWEIE5		0x7A44 /* Forwarding Engine Error Interrupt Enable 5 */
#define RSW2_FWD_FWEID5		0x7A48 /* Forwarding Engine Error Interrupt Disable 5 */
#define RSW2_FWD_FWMIS0		0x7C00 /* Forwarding Engine Monitoring Interrupt Status 0 */
#define RSW2_FWD_FWMIE0		0x7C04 /* Forwarding Engine Monitoring Interrupt Enable 0 */
#define RSW2_FWD_FWMID0		0x7C08 /* Forwarding Engine Monitoring Interrupt Disable 0 */
#define RSW2_FWD_FWSCR0		0x7D00 /* Forwarding Engine Security Configuration Register 0 */
#define RSW2_FWD_FWSCR1		0x7D04 /* Forwarding Engine Security Configuration Register 1 */
#define RSW2_FWD_FWSCR2		0x7D08 /* Forwarding Engine Security Configuration Register 2 */
#define RSW2_FWD_FWSCR3		0x7D0C /* Forwarding Engine Security Configuration Register 3 */
#define RSW2_FWD_FWSCR4		0x7D10 /* Forwarding Engine Security Configuration Register 4 */
#define RSW2_FWD_FWSCR5		0x7D14 /* Forwarding Engine Security Configuration Register 5 */
#define RSW2_FWD_FWSCR6		0x7D18 /* Forwarding Engine Security Configuration Register 6 */
#define RSW2_FWD_FWSCR7		0x7D1C /* Forwarding Engine Security Configuration Register 7 */
#define RSW2_FWD_FWSCR8		0x7D20 /* Forwarding Engine Security Configuration Register 8 */
#define RSW2_FWD_FWSCR9		0x7D24 /* Forwarding Engine Security Configuration Register 9 */
#define RSW2_FWD_FWSCR10	0x7D28 /* Forwarding Engine Security Configuration Register 10 */
#define RSW2_FWD_FWSCR11	0x7D2C /* Forwarding Engine Security Configuration Register 11 */
#define RSW2_FWD_FWSCR12	0x7D30 /* Forwarding Engine Security Configuration Register 12 */
#define RSW2_FWD_FWSCR13	0x7D34 /* Forwarding Engine Security Configuration Register 13 */
#define RSW2_FWD_FWSCR14	0x7D38 /* Forwarding Engine Security Configuration Register 14 */
#define RSW2_FWD_FWSCR15	0x7D3C /* Forwarding Engine Security Configuration Register 15 */
#define RSW2_FWD_FWSCR16	0x7D40 /* Forwarding Engine Security Configuration Register 16 */
#define RSW2_FWD_FWSCR17	0x7D44 /* Forwarding Engine Security Configuration Register 17 */
#define RSW2_FWD_FWSCR18	0x7D48 /* Forwarding Engine Security Configuration Register 18 */
#define RSW2_FWD_FWSCR19	0x7D4C /* Forwarding Engine Security Configuration Register 19 */
#define RSW2_FWD_FWSCR20	0x7D50 /* Forwarding Engine Security Configuration Register 20 */
#define RSW2_FWD_FWSCR21	0x7D54 /* Forwarding Engine Security Configuration Register 21 */
#define RSW2_FWD_FWSCR22	0x7D58 /* Forwarding Engine Security Configuration Register 22 */
#define RSW2_FWD_FWSCR23	0x7D5C /* Forwarding Engine Security Configuration Register 23 */
#define RSW2_FWD_FWSCR25	0x7D64 /* Forwarding Engine Security Configuration Register 25 */
#define RSW2_FWD_FWSCR26	0x7D68 /* Forwarding Engine Security Configuration Register 26 */
#define RSW2_FWD_FWSCR27	0x7D6C /* Forwarding Engine Security Configuration Register 27 */
#define RSW2_FWD_FWSCR28	0x7D70 /* Forwarding Engine Security Configuration Register 28 */
#define RSW2_FWD_FWSCR29	0x7D74 /* Forwarding Engine Security Configuration Register 29 */
#define RSW2_FWD_FWSCR30	0x7D78 /* Forwarding Engine Security Configuration Register 30 */
#define RSW2_FWD_FWSCR31	0x7D7C /* Forwarding Engine Security Configuration Register 31 */
#define RSW2_FWD_FWSCR32	0x7D80 /* Forwarding Engine Security Configuration Register 32 */
#define RSW2_FWD_FWSCR33	0x7D84 /* Forwarding Engine Security Configuration Register 33 */
#define RSW2_FWD_FWSCR34	0x7D88 /* Forwarding Engine Security Configuration Register 34 */
#define RSW2_FWD_FWSCR35	0x7D8C /* Forwarding Engine Security Configuration Register 35 */
#define RSW2_FWD_FWSCR36	0x7D90 /* Forwarding Engine Security Configuration Register 36 */
#define RSW2_FWD_FWSCR37	0x7D94 /* Forwarding Engine Security Configuration Register 37 */
#define RSW2_FWD_FWSCR38	0x7D98 /* Forwarding Engine Security Configuration Register 38 */
#define RSW2_FWD_FWSCR39	0x7D9C /* Forwarding Engine Security Configuration Register 39 */
#define RSW2_FWD_FWSCR40	0x7DA0 /* Forwarding Engine Security Configuration Register 40 */


int rswitch2_fwd_init(struct rswitch2_drv *rsw2);
void rswitch2_fwd_exit(struct rswitch2_drv *rsw2);

int rsw2_fwd_del_l2_entry(struct rswitch2_drv *rsw2, const u8 *macaddr);
int rsw2_fwd_add_l2_entry(struct rswitch2_drv *rsw2, const u8 *macaddr, u32 src_port_vec, u32 dest_port_vec, u32 cpu_q);

#endif /* _RSWITCH2_FWD_H */
