/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas RSwitch2 Gateway Common Agent device driver
 *
 * Copyright (C) 2021 Renesas Electronics Corporation
 *
 */
#ifndef _RSWITCH2_GWCA_H
#define _RSWITCH2_GWCA_H

#include <linux/bits.h>

#define RSW2_GWCA_FRM_PRIO_N	8		/* Priority number handled by GWCA */
#define RSW2_PTP_TN				2		/* gPTP timer number connected to the switch */
#define RSW2_AXI_CHAIN_N		128	/* AXI Descriptor Chain number */
#define RSW2_AXI_RINC_N			8		/* RX incremental descriptor Chain number */
#define RSW2_AXI_TLIM_N			32	/* TX rate limiter number */
#define RSW2_PAS_LVL_N			2		/* Pause level number */


#define RSW2_GCWA_GWMC			0x0000 /* GWCA Mode Configuration */
#define GWMC_OPC				GENMASK(2, 0)

#define RSW2_GCWA_GWMS			0x0004 /* GWCA Mode Status */
#define GWMS_OPS				GENMASK(2, 0)

enum gwmc_op {
	gwmc_reset		= 0,
	gwmc_disable	= 1,
	gwmc_config		= 2,
	gwmc_operation	= 3,
};

#define RSW2_GCWA_GWIRC			0x0010 /* GWCA IPV remapping configuration */
#define GWIRC_IPVR(i)			(7 << (i * 4))

#define RSW2_GCWA_GWRDQSC		0x0014 /* GWCA RX Descriptor Queue Security Configuration */
#define GWRDQSC_RDQSL(i)		BIT_MASK(i)

#define RSW2_GCWA_GWRDQC		0x0018 /* GWCA RX Descriptor Queue Control */
#define GWRDQC_RDQD(i)			BIT_MASK(i)
#define GWRDQC_RDQP(i)			(BIT_MASK(i) << 16)

#define RSW2_GCWA_GWRDQAC		0x001C /* GWCA RX Descriptor Queue Arbitration Control */
#define GWRDQAC_RDQA(i)			((0xF) << (i * 4))

#define RSW2_GCWA_GWRGC			0x0020 /* GWCA RX General Configuration */
#define GWRGC_RCPT				BIT_MASK(0)

/* GWCA Reception Maximum Frame Size Configuration q (q=0.. FRM_PRIO_N-1) */
#define RSW2_GCWA_GWRMFSC(q)	(0x0040 + 4 * (q))
#define GWRMFSC_MFS				GENMASK(15, 0)

/* GWCA Reception Descriptor Queue Depth Configuration q (q=0.. FRM_PRIO_N-1) */
#define RSW2_GCWA_GWRDQDC(q)	(0x0060 + (4 * (q)))
#define GWRDQDC_DQD				GENMASK(10, 0)

/* GWCA RX Descriptor Queue q Monitoring (q=0.. FRM_PRIO_N-1) */
#define RSW2_GCWA_GWRDQM(q)		(0x0080 + (4 * (q)))
#define GWRDQM_DNQ				GENMASK(10, 0)


/* GWCA RX Descriptor Queue q Max Level Monitoring (q=0.. FRM_PRIO_N-1) */
#define RSW2_GCWA_GWRDQMLM(q)	(0x00A0 + (4 * (q)))

/* GWCA RX Descriptor Queue q Max Level Monitoring EMU (q=0.. FRM_PRIO_N-1) */
#define RSW2_GCWA_GWRDQMLME(q)	(0x00C0 + (4 * (q)))
#define GWRDQM_DMLQ				GENMASK(10, 0)

#define RSW2_GCWA_GWMTIRM		0x0100 /* GWCA Multicast Table Initialization Reg. Monitoring */
#define GWMTIRM_MTR				BIT_MASK(1)
#define GWMTIRM_MTIOG			BIT_MASK(0)

#define RSW2_GCWA_GWMSTLS		0x0104 /* GWCA Multicast Table Learning Setting */
#define GWMSTLS_MSENL			GENMASK(22, 16)
#define GWMSTLS_MNL				GENMASK(10, 8)
#define GWMSTLS_MNRCNL			GENMASK(6, 0)

#define RSW2_GCWA_GWMSTLR		0x0108 /* GWCA Multicast Table Learning Result */
#define GWMSTLR_MTL				BIT_MASK(31)
#define GWMSTLR_MTLSF			BIT_MASK(1)
#define GWMSTLR_MTLF			BIT_MASK(0)

#define RSW2_GCWA_GWMSTSS		0x010C /* GWCA Multicast Table Searching Setting */
#define GWMSTSS_MSENS			GENMASK(6, 0)

#define RSW2_GCWA_GWMSTSR		0x0110 /* GWCA Multicast Table Searching Result */
#define GWMSTSR_MTS				BIT_MASK(31)
#define GWMSTSR_MTSEF			BIT_MASK(16)
#define GWMSTSR_MNR				GENMASK(10, 8)
#define GWMSTSR_MNRCNR			BIT_MASK(0)

#define RSW2_GCWA_GWMAC0		0x0120 /* GWCA MAC address configuration 0 */
#define GWMAC0_MAUP				GENMASK(15, 0)

#define RSW2_GCWA_GWMAC1		0x0124 /* GWCA MAC address configuration 1 */
#define GWMAC1_MADP				GENMASK(31, 0)

#define RSW2_GCWA_GWVCC			0x0130 /* GWCA VLAN control configuration */
#define GWVCC_VEM				GENMASK(18, 16)
#define GWVCC_VIM				BIT_MASK(0)

enum gwvcc_vlan_egress_mode {
	no_vlan		= 0,
	ctag		= 1,
	hw_ctag		= 2,
	sc_ctag		= 3,
	hw_sc_ctag	= 4,
};

#define RSW2_GCWA_GWVTC			0x0134 /* GWCA VLAN TAG configuration */
#define GWVTC_STD				BIT_MASK(31)
#define GWVTC_STP				GENMASK(30, 28)
#define GWVTC_STV				GENMASK(27, 16)
#define GWVTC_CTD				BIT_MASK(15)
#define GWVTC_CTP				GENMASK(14, 12)
#define GWVTC_CTV				GENMASK(11, 0)

#define RSW2_GCWA_GWTTFC		0x0138 /* GWCA Transmission TAG Filtering Configuration */
#define GWTTFC_UT				BIT_MASK(8)
#define GWTTFC_SCRT				BIT_MASK(7)
#define GWTTFC_SCT				BIT_MASK(6)
#define GWTTFC_CRT				BIT_MASK(5)
#define GWTTFC_CT				BIT_MASK(4)
#define GWTTFC_CSRT				BIT_MASK(3)
#define GWTTFC_CST				BIT_MASK(2)
#define GWTTFC_RT				BIT_MASK(1)
#define GWTTFC_NT				BIT_MASK(0)

/* GWCA TimeStamp Descriptor Chain Address Configuration 0 s (s=0 to PTP_TN-1) */
#define RSW2_GCWA_GWTDCAC0(s)	(0x0140 + (8 * (s)))
#define GWTDCAC0_TSCCAUP		GENMASK(7, 0)

/* GWCA TimeStamp Descriptor Chain Address Configuration 1 s (s=0 to PTP_TN-1) */
#define RSW2_GCWA_GWTDCAC1(s)	(0x0144 + (8 * (s)))

/* GWCA TimeStamp Descriptor Chain Configuration s (s=0 to PTP_TN-1) */
#define RSW2_GCWA_GWTSDCC(s)	(0x0160 + (4 * (s)))
#define GWTSDCC_OSID			GENMASK(10, 8)
#define GWTSDCC_DCS				BIT_MASK(1)
#define GWTSDCC_TE				BIT_MASK(0)

#define RSW2_GCWA_GWTSNM		0x0180 /* GWCA Agent TimeStamp Number Monitoring */

#define RSW2_GCWA_GWTSMNM		0x0184 /* GWCA Agent TimeStamp Maximum Number Monitoring     */
#define RSW2_GCWA_GWTSMNME		0x0188 /* GWCA Agent TimeStamp Maximum Number Monitoring Emu */
#define GWTSMNM_TNTR			GENMASK(7, 0)

#define RSW2_GCWA_GWAC			0x0190 /* GWCA AXI Control */
#define GWAC_AMP				BIT_MASK(1)
#define GWAC_AMPR				BIT_MASK(0)

#define RSW2_GCWA_GWDCBAC0		0x0194 /* GWCA Descriptor Chain Base Address Configuration 0 */
#define GWDCBAC0_DCBAUP			GENMASK(7, 0)

#define RSW2_GCWA_GWDCBAC1		0x0198 /* GWCA Descriptor Chain Base Address Configuration 1 */
#define GWDCBAC1_DCBADP			GENMASK(31, 0)

#define RSW2_GCWA_GWMDNC		0x01A0 /* GWCA Maximum Descriptor Number Configuration */
#define GWMDNC_TSDMN			GENMASK(17, 16)
#define GWMDNC_TXDMN			GENMASK(12, 8)
#define GWMDNC_RXDMN			GENMASK(4, 0)


/* GWCA AXI Transmission Request Configuration i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWTRC(i)		(0x0200 + (4 * (i)))
#define GWTRC_TSR(t)			BIT_MASK(t)

/*  GWCA Transmission Pause Configuration p ( p = 0 to PAS_LVL_N1-1) */
#define RSW2_GCWA_GWTPC(p)		(0x0300 + (4 * (p)))
#define GWTPC_PPPL				GENMASK(7, 0)

#define RSW2_GCWA_GWARIRM		0x0380 /* GWCA AXI RAM Initialization Register Monitoring */
#define GWARIRM_ARR				BIT_MASK(1)
#define GWARIRM_ARIOG			BIT_MASK(0)

/* GWCA AXI Descriptor chain Configuration i ( i = 0 to AXI_CHAIN_N-1) */
#define RSW2_GCWA_GWDCC(i)		(0x0400 + (4 * (i)))
#define GWDCC_OSID				GENMASK(31, 28)
#define GWDCC_BALR				BIT_MASK(24)
#define GWDCC_DCP				GENMASK(18, 16)
#define GWDCC_DQT				BIT_MASK(11)
#define GWDCC_SL				BIT_MASK(10)
#define GWDCC_ETS				BIT_MASK(9)
#define GWDCC_EDE				BIT_MASK(8)
#define GWDCC_SM				GENMASK(1, 0)

#define RSW2_GCWA_GWAARSS		0x0800 /* GWCA AXI Address RAM Searching Setting */
#define GWAARSS_AARA			GENMASK(6, 0)

#define RSW2_GCWA_GWAARSR0		0x0804 /* GWCA AXI Address RAM Searching Result 0 */
#define GWAARSR0_AARS			BIT_MASK(31)
#define GWAARSR0_AARSSF			BIT_MASK(17)
#define GWAARSR0_AARSEF			BIT_MASK(16)
#define GWAARSR0_ACARUP			GENMASK(7, 0)

#define RSW2_GCWA_GWAARSR1		0x0808 /* GWCA AXI Address RAM Searching Result 1 */
#define GWAARSR1_ACARDP			GENMASK(31, 0)

/* GWCA Incremental Data Area Used Area Size i ( i=0 to AXI_RINC_N -1) */
#define RSW2_GCWA_GWIDAUAS(i)	(0x0840 + (4 * (i)))
#define GWIDAUAS_IDAUAS			GENMASK(23, 0)

/* GWCA Incremental Data Area Size Monitoring i ( i=0 to AXI_RINC_N -1) */
#define RSW2_GCWA_GWIDASM(i)	(0x0880 + (4 * (i)))
#define GWIDASM_IDAS			GENMASK(23, 0)

/* GWCA Incremental Data Area Start Address Monitoring 0 i ( i=0 to AXI_RINC_N -1) */
#define RSW2_GCWA_GWIDASAM0(i)	(0x0900 + (8 * (i)))
#define GWIDASAM0_IDASAUP		GENMASK(7, 0)

/* GWCA Incremental Data Area Start Address Monitoring 1 i ( i=0 to AXI_RINC_N -1) */
#define RSW2_GCWA_GWIDASAM1(i)	(0x0904 + (8 * (i)))
#define GWIDASAM1_IDASADPi		GENMASK(31, 0)

/* GWCA Incremental Data Area Current Address Monitoring 0 i ( i=0 to AXI_RINC_N -1) */
#define RSW2_GCWA_GWIDACAM0(i)	(0x0980 + (8 * (i)))
#define GWIDACAM0_IDACAUP		GENMASK(7, 0)

/*  GWCA Incremental Data Area Current Address Monitoring 1 i ( i=0 to AXI_RINC_N -1) */
#define RSW2_GCWA_GWIDACAM1(i)	(0x0984 + (8 * (i)))
#define GWIDACAM1_IDACADP		GENMASK(31, 0)

#define RSW2_GCWA_GWGRLC		0x0A00 /* GWCA Global Rate Limiter Configuration */
#define GWGRLC_GRLULRS			BIT_MASK(17)
#define GWGRLC_GRLE				BIT_MASK(16)
#define GWGRLC_GRLIV			GENMASK(15, 0)

#define RSW2_GCWA_GWGRLULC		0x0A04 /* GWCA Global Rate Limiter Upper Limit Configuration */
#define GWGRLULC_GRLUL			GENMASK(23, 0)

/*  GWCA Rate limiter incremental value configuration i (i = 0 to AXI_TLIM_N-1) */
#define RSW2_GCWA_GWRLC(i)		(0x0A80 + (8 * (i)))
#define GWRLC_RLE				BIT_MASK(16)
#define GWRLC_RLIV				GENMASK(11, 0)

/*  GWCA Rate limiter upper limit configuration i (i = 0 to AXI_TLIM_N-1) */
#define RSW2_GCWA_GWRLULC(i)	(0x0A84 + (8 * (i)))
#define GWRLULC_RLUL			GENMASK(23, 0)

#define RSW2_GCWA_GWIDPC		0x0B80 /* GWCA Interrupt Delay Prescaler Configuration */
#define GWIDPC_IDPV				GENMASK(9, 0)

/* GWCA Interrupt Delay Configuration i ( i = 0 to AXI_CHAIN_N-1) */
#define RSW2_GCWA_GWIDC(i)		(0x0C00 + (4 * (i)))
#define GWIDC_IDV				GENMASK(11, 0)


#define RSW2_GCWA_GWRDCN		0x1000 /* GWCA Received Data CouNter */
#define GWRDCN_RDN				GENMMASK(31, 0)


#define RSW2_GCWA_GWRDCNE		0x1080 /* GWCA Received Data CouNter */
#define GWRDCNE_TDN				GENMMASK(31, 0)

#define RSW2_GCWA_GWTDCN		0x1004 /* GWCA Transmitted Data CouNter */
#define GWTDCN_TN				GENMMASK(31, 0)

#define RSW2_GCWA_GWTDCNE		0x1084 /* GWCA Transmitted Data CouNter */
#define GWTDCNE_TSOVFEN			GENMASK(15, 0)

#define RSW2_GCWA_GWTSCN		0x1008 /* GWCA TimeStamp CouNter */
#define GWTSCN_USMFSEN			GENMASK(15, 0)

#define RSW2_GCWA_GWTSCNE		0x1088 /* GWCA TimeStamp CouNter */
#define GWTSCNE_TFEN			GENMASK(15, 0)

#define RSW2_GCWA_GWTSOVFECN	0x100C /* GWCA TimeStamp OVerFlow Error CouNter */
#define GWTSOVFECN_SEQEN		GENMASK(15, 0)

#define RSW2_GCWA_GWTSOVFECNE	0x108C /* GWCA TimeStamp OVerFlow Error CouNter */
#define GWTSOVFECNE_SEQEN		GENMASK(15, 0)

#define RSW2_GCWA_GWUSMFSECN	0x1010 /* GWCA Under Switch Minimum Frame Size Error CouNter */
#define GWUSMFSECN_TXDNEN		GENMASK(15, 0)

#define RSW2_GCWA_GWUSMFSECNE	0x1090 /* GWCA Under Switch Minimum Frame Size Error CouNter */
#define GWUSMFSECNE_FSEN		GENMASK(15, 0)

#define RSW2_GCWA_GWTFECN		0x1014 /* GWCA TAG Filtering Error CouNter */
#define RSW2_GCWA_GWTFECNE		0x1094 /* GWCA TAG Filtering Error CouNter Emu */
#define GWTFECN_TDFEN			GENMASK(15, 0)

#define RSW2_GCWA_GWSEQECN		0x1018 /* GWCA SEQuence Error CouNter */
#define RSW2_GCWA_GWSEQECNE		0x1098 /* GWCA SEQuence Error CouNter Emu */
#define GWSEQECN_SEQEN			GENMASK(15, 0)


#define RSW2_GCWA_GWTXDNECN		0x1020 /* GWCA TX Descriptor Number Error CouNter */
#define RSW2_GCWA_GWTXDNECNE	0x10A0 /* GWCA TX Descriptor Number Error CouNter Emu */
#define GWTXDNECN_TXDNEN		GENMASK(15, 0)

#define RSW2_GCWA_GWFSECN		0x1024 /* GWCA Frame Size Error CouNter */
#define RSW2_GCWA_GWFSECNE		0x10A4 /* GWCA Frame Size Error CouNter Emu */
#define GWFSECN_FSEN			GENMASK(15, 0)

#define RSW2_GCWA_GWTDFECN		0x1028 /* GWCA Timestamp Descriptor Full Error CouNter */
#define RSW2_GCWA_GWTDFECNE		0x10A8 /* GWCA Timestamp Descriptor Full Error CouNter Emu */
#define GWTDFECN_TDFEN			GENMASK(15, 0)

#define RSW2_GCWA_GWTSDNECN		0x102C /* GWCA Timestamp Descriptor Number Error CouNter */
#define RSW2_GCWA_GWTSDNECNE	0x10AC /* GWCA Timestamp Descriptor Number Error CouNter Emu */
#define GWTSDNECN_TSDNEN		GENMASK(15, 0)

#define RSW2_GCWA_GWDQOECN		0x1030 /* GWCA Descriptor Queue Overflow Error CouNter */
#define RSW2_GCWA_GWDQOECNE		0x10B0 /* GWCA Descriptor Queue Overflow Error CouNter Emu */
#define GWDQOECN_DQOEN			GENMASK(15, 0)

#define RSW2_GCWA_GWDQSECN		0x1034 /* GWCA Descriptor Queue Security Error CouNter */
#define RSW2_GCWA_GWDQSECNE		0x10B4 /* GWCA Descriptor Queue Security Error CouNter Emu */
#define GWDQSECN_DQSEN			GENMASK(15, 0)

#define RSW2_GCWA_GWDFECN		0x1038 /* GWCA Descriptor Full Error CouNter */
#define RSW2_GCWA_GWDFECNE		0x10B8 /* GWCA Descriptor Full Error CouNter Emu */
#define GWDFECN_DFEN			GENMASK(15, 0)

#define RSW2_GCWA_GWDSECN		0x103C /* GWCA Descriptor Security Error CouNter */
#define RSW2_GCWA_GWDSECNE		0x10BC /* GWCA Descriptor Security Error CouNter Emu */
#define GWDSECN_DSEN			GENMASK(15, 0)

#define RSW2_GCWA_GWDSZECN		0x1040 /* GWCA Data SiZe Error CouNter */
#define RSW2_GCWA_GWDSZECNE		0x10C0 /* GWCA Data SiZe Error CouNter Emu */
#define GWDSZECN_DSZEN			GENMASK(15, 0)

#define RSW2_GCWA_GWDCTECN		0x1044 /* GWCA Descriptor Chain Type Error CouNter */
#define RSW2_GCWA_GWDCTECNE		0x10C4 /* GWCA Descriptor Chain Type Error CouNter Emu */
#define GWDCTECN_DCTEN			GENMASK(15, 0)

#define RSW2_GCWA_GWRXDNECN		0x1048 /* GWCA RX Descriptor Number Error CouNter */
#define RSW2_GCWA_GWRXDNECNE	0x10C8 /* GWCA RX Descriptor Number Error CouNter Emu */
#define GWRXDNECN_RXDNEN		GENMASK(15, 0)

/* GWCA Data Interrupt Status i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWDIS(i)		(0x1100 + (0x10 * (i)))
#define GWDIS_DIS(t)			BIT_MASK(t)

/* GWCA Data Interrupt Enable i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWDIE(i)		(0x1104 + (0x10 * (i)))
#define GWDIE_DIE(t)			BIT_MASK(t)

/*  GWCA Data Interrupt Disable i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWDID(i)		(0x1108 + (0x10 * (i)))
#define GWDID_DID(t)			BIT_MASK(t)

/* GWCA Data Interrupt Disable i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWDIDS(i)		(0x110C + (0x10 * (i)))
#define GWDIDS_DIDS(t)			BIT_MASK(t)

#define RSW2_GCWA_GWTSDIS		0x1180 /* GWCA TimeStamp Data Interrupt Status */
#define GWTSDIS_TSDIS			GENMASK(1, 0)

#define RSW2_GCWA_GWTSDIE		0x1184 /* GWCA TimeStamp Data Interrupt Enable */
#define GWTSDIE_TSDIE			GENMASK(1, 0)

#define RSW2_GCWA_GWTSDID		0x1188 /* GWCA TimeStamp Data Interrupt Disable */
#define GWTSDID_TSDID			GENMASK(1, 0)

#define RSW2_GCWA_GWEIS0		0x1190 /* GWCA Error Interrupt Status 0 */
#define GWEIS0_TSDNES			GENMASK(29, 28)
#define GWEIS0_TDFES			GENMASK(25, 24)
#define GWEIS0_FSES				GENMASK(23, 16)
#define GWEIS0_TSHES			BIT_MASK(15)
#define GWEIS0_TXDNES			BIT_MASK(14)
#define GWEIS0_SEQES			BIT_MASK(12)
#define GWEIS0_TFES				BIT_MASK(11)
#define GWEIS0_USMFSES			BIT_MASK(10)
#define GWEIS0_TSOVFES			BIT_MASK(9)
#define GWEIS0_L23UECCES		BIT_MASK(8)
#define GWEIS0_TSECCES			BIT_MASK(7)
#define GWEIS0_AECCES			BIT_MASK(6)
#define GWEIS0_MECCES			BIT_MASK(5)
#define GWEIS0_DSECCES			BIT_MASK(4)
#define GWEIS0_PECCES			BIT_MASK(3)
#define GWEIS0_TECCES			BIT_MASK(2)
#define GWEIS0_DECCES			BIT_MASK(1)
#define GWEIS0_AES				BIT_MASK(0)

#define RSW2_GCWA_GWEIE0		0x1194 /* GWCA Error Interrupt Enable 0 */
#define GWEIE0_TSDNEE			GENMASK(29, 28)
#define GWEIE0_TDFEE			GENMASK(25, 24)
#define GWEIE0_FSEE				GENMASK(23, 16)
#define GWEIE0_TSHEE			BIT_MASK(15)
#define GWEIE0_TXDNEE			BIT_MASK(14)
#define GWEIE0_SEQEE			BIT_MASK(12)
#define GWEIE0_TFEE				BIT_MASK(11)
#define GWEIE0_USMFSEE			BIT_MASK(10)
#define GWEIE0_TSOVFEE			BIT_MASK(9)
#define GWEIE0_L23UECCEE		BIT_MASK(8)
#define GWEIE0_TSECCEE			BIT_MASK(7)
#define GWEIE0_AECCEE			BIT_MASK(6)
#define GWEIE0_MECCEE			BIT_MASK(5)
#define GWEIE0_DSECCEE			BIT_MASK(4)
#define GWEIE0_PECCEE			BIT_MASK(3)
#define GWEIE0_TECCEE			BIT_MASK(2)
#define GWEIE0_DECCEE			BIT_MASK(1)
#define GWEIE0_AEE				BIT_MASK(0)


#define RSW2_GCWA_GWEID0		0x1198 /* GWCA Error Interrupt Disable 0 */
#define GWEID0_TSDNED			GENMASK(29, 28)
#define GWEID0_TDFED			GENMASK(25, 24)
#define GWEID0_FSED				GENMASK(23, 16)
#define GWEID0_TSHED			BIT_MASK(15)
#define GWEID0_TXDNED			BIT_MASK(14)
#define GWEID0_SEQED			BIT_MASK(12)
#define GWEID0_TFED				BIT_MASK(11)
#define GWEID0_USMFSED			BIT_MASK(10)
#define GWEID0_TSOVFED			BIT_MASK(9)
#define GWEID0_L23UECCED		BIT_MASK(8)
#define GWEID0_TSECCED			BIT_MASK(7)
#define GWEID0_AECCED			BIT_MASK(6)
#define GWEID0_MECCED			BIT_MASK(5)
#define GWEID0_DSECCED			BIT_MASK(4)
#define GWEID0_PECCED			BIT_MASK(3)
#define GWEID0_TECCED			BIT_MASK(2)
#define GWEID0_DECCED			BIT_MASK(1)
#define GWEID0_AED				BIT_MASK(0)

#define RSW2_GCWA_GWEIS1		0x11A0 /* GWCA Error Interrupt Status 1 */
#define GWEIS1_DQSES			GENMASK(23, 16)
#define GWEIS1_DQOES			GENMASK(7, 0)

#define RSW2_GCWA_GWEIE1		0x11A4 /* GWCA Error Interrupt Enable 1 */
#define GWEIE1_DQSEE			GENMASK(23, 16)
#define GWEIE1_DQOEE			GENMASK(7, 0)

#define RSW2_GCWA_GWEID1		0x11A8 /* GWCA Error Interrupt Disable 1 */
#define GWEID1_DQSED			GENMASK(23, 16)
#define GWEID1_DQOED			GENMASK(7, 0)

/* GWCA Error Interrupt Status 2 i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWEIS2(i)		(0x1200 + (10 * (i)))
#define GWEIS2_DFES(t)			BIT_MASK(t)

/* GWCA Error Interrupt Enable 2 i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWEIE2(i)		(0x1204 + (10 * (i)))
#define GWEIE2_DFEE(t)			BIT_MASK(t)

/* GWCA Error Interrupt Disable 2 i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWEID2(i)		(0x1208 + (10 * (i)))
#define GWEID2_DFED(t)			BIT_MASK(t)

#define RSW2_GCWA_GWEIS3		0x1280 /* GWCA Error Interrupt Status 3 */
#define GWEIS3_IAOES			GENMASK(7, 0)

#define RSW2_GCWA_GWEIE3		0x1284 /* GWCA Error Interrupt Enable 3 */
#define GWEIE3_IAOEE			GENMASK(7, 0)

#define RSW2_GCWA_GWEID3		0x1288 /* GWCA Error Interrupt Disable 3 */
#define GWEID3_IAOED			GENMASK(7, 0)

#define RSW2_GCWA_GWEIS4		0x1290 /* GWCA Error Interrupt Status 4 */
#define GWEIS4_DSECN			GENMASK(30, 24)
#define GWEIS4_DSEIOS			BIT_MASK(17)
#define GWEIS4_DSES				BIT_MASK(16)
#define GWEIS4_DSSECN			GENMASK(14, 8)
#define GWEIS4_DSSEIOS			BIT_MASK(1)
#define GWEIS4_DSSES			BIT_MASK(0)

#define RSW2_GCWA_GWEIE4		0x1294 /* GWCA Error Interrupt Enable 4 */
#define GWEIE4_DSEIOE			BIT_MASK(17)
#define GWEIE4_DSEE				BIT_MASK(16)
#define GWEIE4_DSSEIOE			BIT_MASK(1)
#define GWEIE4_DSSEE			BIT_MASK(0)

#define RSW2_GCWA_GWEID4		0x1298 /* GWCA Error Interrupt Disable 4 */
#define GWEID4_DSDIOD			BIT_MASK(17)
#define GWEID4_DSDD				BIT_MASK(16)
#define GWEID4_DSDEIOD			BIT_MASK(1)
#define GWEID4_DSDED			BIT_MASK(0)

#define RSW2_GCWA_GWEIS5		0x12A0 /* GWCA Error Interrupt Status 5 */
#define GWEIS5_RXDNECN			GENMASK(30, 24)
#define GWEIS5_RXDNEIOS			BIT_MASK(17)
#define GWEIS5_RXDNES			BIT_MASK(16)
#define GWEIS5_DCTECN			GENMASK(14, 8)
#define GWEIS5_DCTEIOS			BIT_MASK(1)
#define GWEIS5_DCTES			BIT_MASK(0)

#define RSW2_GCWA_GWEIE5		0x12A4 /* GWCA Error Interrupt Enable 5 */
#define GWEIE5_RXDNEIOE			BIT_MASK(17)
#define GWEIE5_RXDNEE			BIT_MASK(16)
#define GWEIE5_DCTEIOE			BIT_MASK(1)
#define GWEIE5_DCTEE			BIT_MASK(0)

#define RSW2_GCWA_GWEID5		0x12A8 /* GWCA Error Interrupt Disable 5 */
#define GWEID5_RXDNEIOD			BIT_MASK(17)
#define GWEID5_RXDNED			BIT_MASK(16)
#define GWEID5_DCTEIOD			BIT_MASK(1)
#define GWEID5_DCTED			BIT_MASK(0)

#define RSW2_GCWA_GWSCR0		0x1800 /* GWCA Security Configuration Register 0 */
#define GWSCR0_TRSL				GENMASK(29, 28)
#define GWSCR0_TSQRSL			GENMASK(25, 24)
#define GWSCR0_DQRSL			GENMASK(23, 16)
#define GWSCR0_APRSL			GENMASK(15, 8)
#define GWSCR0_EIRSL			BIT_MASK(7)
#define GWSCR0_AXRSL			BIT_MASK(6)
#define GWSCR0_TSRSL			BIT_MASK(5)
#define GWSCR0_TGRSL			BIT_MASK(4)
#define GWSCR0_MCRSL			BIT_MASK(3)
#define GWSCR0_MTRSL			BIT_MASK(2)
#define GWSCR0_RRSL				BIT_MASK(1)
#define GWSCR0_MRSL				BIT_MASK(0)


#define RSW2_GCWA_GWSCR1		0x1804 /* GWCA Security Configuration Register 1 */
#define GWSCR1_CRSL				BIT_MASK(0)

/* GWCA Security Configuration Register 2 i ( i = 0 to AXI_CHAIN_N/32-1) */
#define RSW2_GCWA_GWSCR2(i)		(0x1900 + (4 * (i)))
#define GWSCR2_ACRSL(t)			BIT_MASK(t)

#endif /* _RSWITCH2_GWCA_H */
