/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas RSwitch2 Common Agent device driver
 *
 * Copyright (C) 2021 Renesas Electronics Corporation
 *
 */
#ifndef _RSWITCH2_COMA_H
#define _RSWITCH2_COMA_H

#include <linux/bits.h>

#define RSW2_COMA_RIPV				0x0000 /* R-Switch IP Version */
#define RIPV_CAIPV					GENMASK(23, 21)
#define RIPV_FBIPV					GENMASK(19, 16)
#define RIPV_EAIPV					GENMASK(15, 12)
#define RIPV_FWIPV					GENMASK(11, 8)
#define RIPV_GWIPV					GENMASK(7, 4)
#define RIPV_TIPV					GENMASK(3, 0)

#define RSW2_COMA_RRC				0x0004 /* R-Switch Reset Configuration */
#define RRC_RR						BIT_MASK(0)

#define RSW2_COMA_RCEC				0x0008 /* R-Switch Clock Enable Configuration */
#define RCEC_RCE					BIT_MASK(16)
#define RCEC_ACE					GENMASK(6, 0)

#define RSW2_COMA_RCDC				0x000C /* R-Switch Clock Disable Configuration */
#define RCDC_RCD					BIT_MASK(16)
#define RCDC_ACD					GENMASK(6, 0)

#define RSW2_COMA_RSSIS				0x0010 /* R-Switch Software Synchronization Interrupt Status */
#define RSSIS_NSSSIS(i)				BIT_MASK(((i) & 0xFF) << 8)
#define RSSIS_SNSSIS(i)				BIT_MASK((i) & 0xFF)

#define RSW2_COMA_RSSIE				0x0014 /* R-Switch Software Synchronization Interrupt Enable */
#define RSSIE_NSSSIE(i)				BIT_MASK(((i) & 0xFF) << 8)
#define RSSIE_SNSSIE(i)				BIT_MASK((i) & 0xFF)

#define RSW2_COMA_RSSID				0x0018 /* R-Switch Software Synchronization Interrupt Disable */
#define RSSID_NSSSID(i)				BIT_MASK(((i) & 0xFF) << 8)
#define RSSID_SNSSID(i)				BIT_MASK((i) & 0xFF)

/* Common Agent Buffer Pool IPV Based Watermark Config (i = 0..7) */
#define RSW2_COMA_CABPIBWMC(i)		(0x0020 + (0x4 * (i)))
#define CABPIBWMC_IBSWMPN			GENMASK(28, 16)


#define RSW2_COMA_CABPWMLC			0x0040 /* Common Agent Buffer Pool Watermark Level Configuration */
#define CABPWMLC_WMCL				GENMASK(28, 16)
#define CABPWMLC_WMFL				GENMASK(12, 0)

/* Common Agent Buffer Pointer Pause Frame Level Configuration i (i = 0..PAS_LVL_N -1) */
#define RSW2_COMA_CABPPFLC(i)		(0x0050 + (0x4 * (i)))
#define CABPPFLC_PAL				GENMASK(28, 16)
#define CABPPFLC_PDL				GENMASK(12, 0)

/* Common Agent Buffer Pool Watermark Level Configuration i (i = 0..PORT_N-1) */
#define RSW2_COMA_CABPPWMLC(i)		(0x0060 + (0x4 * (i)))
#define CABPPWMLC_PWMCL				GENMASK(28, 16)
#define CABPPWMLC_PWMFK				GENMASK(12, 0)

/* Common Agent Buffer Pointer per Port Pause Frame Level Configuration */
#define RSW2_COMA_CABPPPFLC(i, j)	(0x00A0 + (0x4 * ((2 * i) + j)))
#define CABPPPFLC_PPAL				GENMASK(28, 16)
#define CABPPPFLC_PPDL				GENMASK(12, 0)

/* Common Agent Buffer Pointer Utilization Level Configuration i (i = 0..PORT_N-1) */
#define RSW2_COMA_CABPULC(i)		(0x0100 + (0x4 * (i)))
#define CABPULC_MNNPN				GENMASK(28, 16)
#define CABPULC_MXNPN				GENMASK(12, 0)

#define RSW2_COMA_CABPIRM			0x0140 /* Common Agent Buffer Pool Initialization Register Monitoring */
#define CABPIRM_BPR					BIT_MASK(1)
#define CABPIRM_BPIOG				BIT_MASK(0)

#define RSW2_COMA_CABPPCM			0x0144 /* Common Agent Buffer Pool Pointer Count Monitoring */
#define CABPPCM_TPC					GENMASK(28, 16)
#define CABPPCM_RPC					GENMASK(12, 0)

#define RSW2_COMA_CABPLCM			0x0148 /* Common Agent Buffer Pool Pointer Least Count Monitoring */
#define CABPLCM_LRC					GENMASK(12, 0)

#define RSW2_COMA_CABPLCME			0x014C /* Common Agent Buffer Pool Pointer Least Count Monitoring Emu */
#define CABPLCME_RPCP				GENMASK(12, 0)

 /* Common Agent Buffer Pointer Count per Port Monitoring i (i = 0..PORT_N-1) */
#define RSW2_COMA_CABPCPM(i)		(0x0180 + (0x4 * (i)))
#define CABPCPM_RPMP				GENMASK(12, 0)

/* Common Agent Buffer Pointer Maximum Count per Port Monitoring I (i = 0..PORT_N-1) */
#define RSW2_COMA_CABPMCPM(i)		(0x0200 + (0x4 * (i)))
/* Common Agent Buffer Pointer Maximum Count per Port Monitoring I Emu (i = 0..PORT_N-1) */
#define RSW2_COMA_CABPMCPME(i)		(0x0280 + (0x4 * (i)))
#define CABPMCPM_RPMCP				GENMASK(12, 0)

#define RSW2_COMA_CARDNM			0x0300 /* Common Agent Rejected Descriptor Number Monitoring */
#define CARDNM_RDNRR				GENMASK(12, 0)

/* Common Agent Rejected Descriptor Maximum Number Monitoring */
#define RSW2_COMA_CARDMNM			0x0304
/* Common Agent Rejected Descriptor Maximum Number Monitoring Emu */
#define RSW2_COMA_CARDMNME			0x0308
#define CARDMNM_RDNRR				GENMASK(12, 0)

#define RSW2_COMA_CARDCN			0x0310 /* Common Agent Rejected Descriptor Counter */
#define RSW2_COMA_CARDCNE			0x0314 /* Common Agent Rejected Descriptor Counter Emu */
#define CARDCN_RDN					GENMASK(31, 0)

#define RSW2_COMA_CAEIS0			0x0400 /* Common Agent Error Interrupt Status 0 */
#define CAEIS0_EEIPLN				GENMASK(19, 16)
#define CAEIS0_WMFLOS				BIT_MASK(10)
#define CAEIS0_WMCLOS				BIT_MASK(9)
#define CAEIS0_BPOPS				BIT_MASK(8)
#define CAEIS0_BPECCES				BIT_MASK(2)
#define CAEIS0_DSECCES				BIT_MASK(1)
#define CAEIS0_PECCES				BIT_MASK(0)

#define RSW2_COMA_CAEIE0			0x0404 /* Common Agent Error Interrupt Enable 0 */
#define CAEIE0_WMFLOE				BIT_MASK(10)
#define CAEIE0_WMCLOE				BIT_MASK(9)
#define CAEIE0_BPOPE				BIT_MASK(8)
#define CAEIE0_BPECCEE				BIT_MASK(2)
#define CAEIE0_DSECCEE				BIT_MASK(1)
#define CAEIE0_PECCEE				BIT_MASK(0)

#define RSW2_COMA_CAEID0			0x0408 /* Common Agent Error Interrupt Disable 0 */
#define CAEID0_WMFLOD				BIT_MASK(10)
#define CAEID0_WMCLOD				BIT_MASK(9)
#define CAEID0_BPOPD				BIT_MASK(8)
#define CAEID0_BPECCED				BIT_MASK(2)
#define CAEID0_DSECCED				BIT_MASK(1)
#define CAEID0_PECCED				BIT_MASK(0)

#define RSW2_COMA_CAEIS1			0x0410 /* Common Agent Error Interrupt Status 1 */
#define CAEIS1_PWMFLOS				GENMASK(22, 16)
#define CAEIS1_PWMCLOS				GENMASK(6, 0)

#define RSW2_COMA_CAEIE1			0x0414 /* Common Agent Error Interrupt Enable 1 */
#define CAEIE1_PWMFLOE				GENMASK(22, 16)
#define CAEIE1_PWMCLOE				GENMASK(6, 0)

#define RSW2_COMA_CAEID1			0x0418 /* Common Agent Error Interrupt Disable 1 */
#define CAEID1_PWMFLOD				GENMASK(22, 16)
#define CAEID1_PWMCLOD				GENMASK(6, 0)

#define RSW2_COMA_CAMIS0			0x0440 /* Common Agent Monitoring Interrupt Status 0 */
#define CAMIS0_PFS					GENMASK(1, 0)

#define RSW2_COMA_CAMIE0			0x0444 /* Common Agent Monitoring Interrupt Enable 0 */
#define CAMIE0_PFE					GENMASK(1, 0)

#define RSW2_COMA_CAMID0			0x0448 /* Common Agent Monitoring Interrupt Disable 0 */
#define CAMID0_PFD					GENMASK(1, 0)

#define RSW2_COMA_CAMIS1			0x0450 /* Common Agent Monitoring Interrupt Status 1 */
#define CAMIS1_PPFS					GENMASK(13, 0)

#define RSW2_COMA_CAMIE1			0x0454 /* Common Agent Monitoring Interrupt Enable 1 */
#define CAMIE1_PPFE					GENMASK(13, 0)

#define RSW2_COMA_CAMID1			0x0458 /* Common Agent Monitoring Interrupt Disable 1 */
#define CAMID1_PPFD					GENMASK(13, 0)

#define RSW2_COMA_CASCR				0x0480 /* Common Agent Security configuration register. */
#define CASCR_MIRSL					BIT_MASK(5)
#define CASCR_EIRSL					BIT_MASK(4)
#define CASCR_CRSL					BIT_MASK(3)
#define CASCR_RRRSL					BIT_MASK(2)
#define CASCR_BPRSL					BIT_MASK(1)
#define CASCR_VRSL					BIT_MASK(0)

#define RSW2_RSW0PPS0R0			0x0604 /* Common PPS output select */
#define RSW2_RSW0PPS0R1			0x0608 /* Common PPS output select */
#define RSW2_RSW0PPS1R0			0x060c /* Common PPS output select */
#define RSW2_RSW0PPS1R1			0x0610 /* Common PPS output select */

#endif /* _RSWITCH2_COMA_H */
