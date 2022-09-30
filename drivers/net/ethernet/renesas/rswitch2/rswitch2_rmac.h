/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas RSwitch2 Gateway Common Agent device driver
 *
 * Copyright (C) 2021 Renesas Electronics Corporation
 *
 */
#ifndef _RSWITCH2_RMACGWCA_H
#define _RSWITCH2_GWCA_H

#include <linux/bits.h>

#define RSW2_RMAC_OFFSET	0x1000

#define RSW2_RMAC_MPSM	0x0000 /* MAC PHY station management */
#define MPSM_PRD	GENMASK(31, 16)
#define MPSM_POP	GENMASK(14, 13)
#define MPSM_PRA	GENMASK(12, 8)
#define MPSM_PDA	GENMASK(7, 3)
#define MPSM_MFF	BIT_MASK(2)
#define MPSM_PSME	BIT_MASK(0)


#define RSW2_RMAC_MPIC	0x0004 /* MAC PHY interfaces configuration */
#define MPIC_PSMCT	GENMASK(30, 28)
#define MPIC_PSMHT	GENMASK(26, 24)
#define MPIC_PSMDP	BIT_MASK(23)
#define MPIC_PSMCS	GENMASK(22, 16)
#define MPIC_RPMT	BIT_MASK(11)
#define MPIC_PLSPP	BIT_MASK(10)
#define MPIC_PIPP	BIT_MASK(9)
#define MPIC_PIP	BIT_MASK(8)
#define MPIC_LSC	GENMASK(5, 3)
#define MPIC_PIS	GENMASK(2, 0)

enum rsw2_rmac_link_speed {
	rsw2_rmac_100mbps	= 1,
	rsw2_rmac_1000mbps	= 2,
	rsw2_rmac_2500mbps	= 3,
};

enum rsw2_rmac_phy_if_type {
	rsw2_rmac_mii	= 0,
	rsw2_rmac_gmii	= 2,
	rsw2_rmac_xgmii	= 4,
};

#define RSW2_RMAC_MPIM	0x0008 /* MAC PHY interfaces monitoring register */
#define MPIM_LPIA	BIT_MASK(1)
#define MPIM_PLS	BIT_MASK(0)

#define RSW2_RMAC_MTFFC	0x0020	/* MAC transmission frame format configuration */
#define MTFFC_FCM	BIT_MASK(1)
#define MTFFC_DPAD	BIT_MASK(0)

#define RSW2_RMAC_MTPFC	0x0024	/* MAC transmission pause or PFC frame configuration */
#define MTPFC_PFRLV	GENMASK(31, 27)
#define MTPFC_PFM	BIT_MASK(26)
#define MTPFC_PFRT	GENMASK(23, 16)
#define MTPFC_PT	GENMASK(15, 0)

#define RSW2_RMAC_MTPFC2	0x0028	/* MAC transmission pause or PFC frame configuration2 */
#define MTPFC2_MPFR		BIT_MASK(17)
#define MTPFC2_PFTTZ	BIT_MASK(16)
#define MTPFC2_MPFCFR	GENMASK(9, 8)
#define MTPFC2_PFCTTZ	GENMASK(1, 0)


#define RSW2_RMAC_MTPFC3(t)	(0x0030 + 4 * (t))	/* MAC transmission pause or PFC frame configuration 3 */
#define MTPFC3_PFCPG(i)	BIT_MASK(i)

#define RSW2_RMAC_MTATC0	(0x0050 + 4 * (t)) /* MAC transmission automatic timestamp configuration */
#define MTATC0_TRTL		GEN_MASK(10, 8)
#define MTATC0_TRTP		GEN_MASK(7, 0)

#define RSW2_RMAC_MRGC	0x0080 /* MAC reception general configuration */
#define MRGC_PFCRC		GENMASK(23, 16)
#define MRGC_RFCFE		BIT_MASK(4)
#define MRGC_MPDE		BIT_MASK(3)
#define MRGC_PFRTZ		BIT_MASK(2)
#define MRGC_PFRC		BIT_MASK(1)
#define MRGC_RCPT		BIT_MASK(0)


#define RSW2_RMAC_MRMAC0	0x0084 /* MAC reception MAC address configuration 0 */
#define MRMAC0_MAUP		GENMASK(15, 0)

#define RSW2_RMAC_MRMAC1	0x0088 /* MAC reception MAC address configuration 1 */
#define MRMAC1_MADP		GENMASK(31, 0)

#define RSW2_RMAC_MRAFC	0x008C /* MAC reception address filter configuration */
#define MRAFC_MSAREP	BIT_MASK(26)
#define MRAFC_NSAREP	BIT_MASK(25)
#define MRAFC_SDSFREP	BIT_MASK(24)
#define MRAFC_NDAREP	BIT_MASK(23)
#define MRAFC_BCACP		BIT_MASK(22)
#define MRAFC_MCACP		BIT_MASK(21)
#define MRAFC_BSTENP	BIT_MASK(20)
#define MRAFC_MSTENP	BIT_MASK(19)
#define MRAFC_BCENP		BIT_MASK(18)
#define MRAFC_MCENP		BIT_MASK(17)
#define MRAFC_UCENP		BIT_MASK(16)
#define MRAFC_MSAREE	BIT_MASK(10)
#define MRAFC_NSAREE	BIT_MASK(9)
#define MRAFC_SDSFREE	BIT_MASK(8)
#define MRAFC_NDAREE	BIT_MASK(7)
#define MRAFC_BCACE		BIT_MASK(6)
#define MRAFC_MCACE		BIT_MASK(5)
#define MRAFC_BSTENE	BIT_MASK(4)
#define MRAFC_MSTENE	BIT_MASK(3)
#define MRAFC_BCENE		BIT_MASK(2)
#define MRAFC_MCENE		BIT_MASK(1)
#define MRAFC_UCENE		BIT_MASK(0)

#define RSW2_RMAC_MRSCE	0x0090 /* MAC reception storm configuration for e-frames */
#define MRSCE_CBFE		GENMASK(31, 16)
#define MRSCE_CMFE		GENMASK(15, 0)

#define RSW2_RMAC_MRSCP	0x0094 /* MAC reception storm configuration for p-frames */
#define MRSCP_CBFE		GENMASK(31, 16)
#define MRSCP_CMFE		GENMASK(15, 0)

#define RSW2_RMAC_MRSCC	0x0098 /* MAC reception storm counter configuration */
#define MRSCC_BSCCP		BIT_MASK(17)
#define MRSCC_MSCCP		BIT_MASK(16)
#define MRSCC_BSCCE		BIT_MASK(1)
#define MRSCC_MSCCE		BIT_MASK(0)

#define RSW2_RMAC_MRFSCE	0x009C /* MAC reception frame size configuration for p-frames */
#define MRFSCE_EMNS		GENMASK(31, 16)
#define MRFSCE_EMXS		GENMASK(15, 0)

#define RSW2_RMAC_MRFSCP	0x00a0 /* MAC reception frame size configuration for e-frames */
#define MRFSCP_PMNS		GENMASK(31, 16)
#define MRFSCP_PMXS		GENMASK(15, 0)

#define RSW2_RMAC_MTRC	0x00a4 /* MAC Timestamp reception configuration register */
#define MTRC_DTN		BIT_MASK(28)
#define MTRC_TCTSP		BIT_MASK(27)
#define MTRC_TCTSE		BIT_MASK(26)
#define MTRC_TRDDP		BIT_MASK(25)
#define MTRC_TRDDE		BIT_MASK(24)
#define MTRC_TRHFME		GENMASK(1, 0)

#define RSW2_RMAC_MRIM	0x00a8 /* MAC reception interfaces monitoring */

#define RSW2_RMAC_MRPFM	0x00aC /* MAC PTP filtering register configuration */
#define MRPFM_PFCTCA	GENMASK(23, 16)
#define MRPFM_PTCA		BIT_MASK(0)

#define RSW2_RMAC_MPFC0(t)	(0x0100 + 4 * (t)) /* MAC PTP filtering register configuration */
#define MPFC0_TEF		GENMASK(17, 16)
#define MPFC0_PFBV		GENMASK(15, 8)
#define MPFC0_PFBN		GENMASK(7, 0)

#define RSW2_RMAC_MLVC	0x0180 /* MAC link verification configuration */
#define MLVC_PLV		BIT_MASK(16)
#define MLVC_PASE		BIT_MASK(8)
#define MLVC_LVT		GENMASK(6, 0)

#define RSW2_RMAC_MEEEC	0x0184 /* MAC energy efficient ethernet configuration */
#define MEEEC_LPITR		BIT_MASK(0)

#define RSW2_RMAC_MLBC	0x0188 /* MAC loopback configuration */

#define RSW2_RMAC_MXGMIIC	0x0190 /* XGMII configuration */
#define MXGMIIC_LFS_TXIDLE	BIT_MASK(1)
#define MXGMIIC_LFS_TXRFS	BIT_MASK(0)


#define RSW2_RMAC_MPCH	0x0194 /* XGMII PCH configuration */
#define MPCH_RPHCRCD		BIT_MASK(17)
#define MPCH_RXPCH_TSM		BIT_MASK(16)
#define MPCH_CTRIOD			BIT_MASK(11)
#define MPCH_IETRIOD		BIT_MASK(10)
#define MPCH_CTPTE			BIT_MASK(9)
#define MPCH_IETPTE			BIT_MASK(8)
#define MPCH_TXPCH_PID		GENMASK(7, 4)
#define MPCH_TXPCH_ETYPE	GENMASK(3, 1)
#define MPCH_TXPCH_M		BIT_MASK(0)

#define RSW2_RMAC_MANC	0x0198 /* Auto-negotiation configuration */
#define MANC_AN_REQ		BIT_MASK(0)

#define RSW2_RMAC_MANM	0x019C /* Auto-negotiation message */
#define MANM_TX_AN_MES	GENMASK(31, 15)
#define MANM_RX_AN_MES	GENMASK(15, 0)

#define RSW2_RMAC_MEIS	0x0200 /* MAC error interrupt status */
#define MEIS_FOES		BIT_MASK(29)
#define MEIS_FUES		BIT_MASK(28)
#define MEIS_FFS		BIT_MASK(27)
#define MEIS_RPOOMS		BIT_MASK(26)
#define MEIS_FRCES		BIT_MASK(25)
#define MEIS_CFCES		BIT_MASK(24)
#define MEIS_FFMES		BIT_MASK(23)
#define MEIS_FCMCES		BIT_MASK(22)
#define MEIS_PNAES		BIT_MASK(21)
#define MEIS_PDES		BIT_MASK(20)
#define MEIS_CTLES		GENMASK(13, 12)
#define MEIS_RPCRES		BIT_MASK(11)
#define MEIS_RPOES		BIT_MASK(10)
#define MEIS_REOES		BIT_MASK(9)
#define MEIS_FCES		BIT_MASK(8)
#define MEIS_BFES		BIT_MASK(7)
#define MEIS_TBCIS		BIT_MASK(6)
#define MEIS_TCES		BIT_MASK(5)
#define MEIS_FCDS		BIT_MASK(4)
#define MEIS_PFRROS		BIT_MASK(3)
#define MEIS_PRES		BIT_MASK(2)
#define MEIS_TIES		BIT_MASK(1)
#define MEIS_TSLS		BIT_MASK(0)

#define RSW2_RMAC_MEIE	0x0204 /* MAC error interrupt enable */
#define MEIE_FOEE		BIT_MASK(29)
#define MEIE_FUEE		BIT_MASK(28)
#define MEIE_FFE		BIT_MASK(27)
#define MEIE_RPOOME		BIT_MASK(26)
#define MEIE_FRCEE		BIT_MASK(25)
#define MEIE_CFCEE		BIT_MASK(24)
#define MEIE_FFMEE		BIT_MASK(23)
#define MEIE_FCMCEE		BIT_MASK(22)
#define MEIE_PNAEE		BIT_MASK(21)
#define MEIE_PDEE		BIT_MASK(20)
#define MEIE_CTLEE		GENMASK(13, 12)
#define MEIE_RPCREE		BIT_MASK(11)
#define MEIE_RPOEE		BIT_MASK(10)
#define MEIE_REOEE		BIT_MASK(9)
#define MEIE_FCEE		BIT_MASK(8)
#define MEIE_BFEE		BIT_MASK(7)
#define MEIE_TBCIE		BIT_MASK(6)
#define MEIE_TCEE		BIT_MASK(5)
#define MEIE_FCDE		BIT_MASK(4)
#define MEIE_PFRROE		BIT_MASK(3)
#define MEIE_PREE		BIT_MASK(2)
#define MEIE_TIEE		BIT_MASK(1)
#define MEIE_TSLE		BIT_MASK(0)

#define RSW2_RMAC_MEID	0x0208 /* MAC error interrupt disable */
#define MEID_FOED		BIT_MASK(29)
#define MEID_FUED		BIT_MASK(28)
#define MEID_FFD		BIT_MASK(27)
#define MEID_RPOOMD		BIT_MASK(26)
#define MEID_FRCED		BIT_MASK(25)
#define MEID_CFCED		BIT_MASK(24)
#define MEID_FFMED		BIT_MASK(23)
#define MEID_FCMCED		BIT_MASK(22)
#define MEID_PNAED		BIT_MASK(21)
#define MEID_PDED		BIT_MASK(20)
#define MEID_CTLED		GENMASK(13, 12)
#define MEID_RPCRED		BIT_MASK(11)
#define MEID_RPOED		BIT_MASK(10)
#define MEID_REOED		BIT_MASK(9)
#define MEID_FCED		BIT_MASK(8)
#define MEID_BFED		BIT_MASK(7)
#define MEID_TBCID		BIT_MASK(6)
#define MEID_TCED		BIT_MASK(5)
#define MEID_FCDD		BIT_MASK(4)
#define MEID_PFRROD		BIT_MASK(3)
#define MEID_PRED		BIT_MASK(2)
#define MEID_TIED		BIT_MASK(1)
#define MEID_TSLD		BIT_MASK(0)

#define RSW2_RMAC_MMIS0	0x0210 /* MAC monitoring interrupt status 0 */
#define MMIS0_XLISDS	BIT_MASK(12)
#define MMIS0_XRFSDS	BIT_MASK(11)
#define MMIS0_XLFSDS	BIT_MASK(10)
#define MMIS0_XLFES		BIT_MASK(9)
#define MMIS0_XLFDS		BIT_MASK(8)
#define MMIS0_ANDETS	BIT_MASK(6)
#define MMIS0_VFRS		BIT_MASK(4)
#define MMIS0_LVFS		BIT_MASK(3)
#define MMIS0_LVSS		BIT_MASK(2)
#define MMIS0_PIDS		BIT_MASK(1)
#define MMIS0_PLSCS		BIT_MASK(0)

#define RSW2_RMAC_MMIE0	0x0214 /* MAC monitoring interrupt enable 0 */
#define MMIE0_XLISDE	BIT_MASK(12)
#define MMIE0_XRFSDE	BIT_MASK(11)
#define MMIE0_XLFSDE	BIT_MASK(10)
#define MMIE0_XLFEE		BIT_MASK(9)
#define MMIE0_XLFDE		BIT_MASK(8)
#define MMIE0_ANDETE	BIT_MASK(6)
#define MMIE0_VFRE		BIT_MASK(4)
#define MMIE0_LVFE		BIT_MASK(3)
#define MMIE0_LVSE		BIT_MASK(2)
#define MMIE0_PIDE		BIT_MASK(1)
#define MMIE0_PLSCE		BIT_MASK(0)

#define RSW2_RMAC_MMID0	0x0218 /* MAC monitoring interrupt disable 0 */
#define MMID0_XLISDD	BIT_MASK(12)
#define MMID0_XRFSDD	BIT_MASK(11)
#define MMID0_XLFSDD	BIT_MASK(10)
#define MMID0_XLFED		BIT_MASK(9)
#define MMID0_XLFDD		BIT_MASK(8)
#define MMID0_ANDETD	BIT_MASK(6)
#define MMID0_VFRD		BIT_MASK(4)
#define MMID0_LVFD		BIT_MASK(3)
#define MMID0_LVSD		BIT_MASK(2)
#define MMID0_PIDD		BIT_MASK(1)
#define MMID0_PLSCD		BIT_MASK(0)

#define RSW2_RMAC_MMIS1	0x0220 /* MAC monitoring interrupt status 1 */
#define MMIS1_PPRACS	BIT_MASK(3)
#define MMIS1_PAACS		BIT_MASK(2)
#define MMIS1_PWACS		BIT_MASK(1)
#define MMIS1_PRACS		BIT_MASK(0)

#define RSW2_RMAC_MMIE1	0x0224 /* MAC monitoring interrupt enable 1 */
#define MMIE1_PPRACE	BIT_MASK(3)
#define MMIE1_PAACE		BIT_MASK(2)
#define MMIE1_PWACE		BIT_MASK(1)
#define MMIE1_PRACE		BIT_MASK(0)

#define RSW2_RMAC_MMID1	0x0228 /* MAC monitoring interrupt disable 1 */
#define MMID1_PPRACD	BIT_MASK(3)
#define MMID1_PAACD		BIT_MASK(2)
#define MMID1_PWACD		BIT_MASK(1)
#define MMID1_PRACD		BIT_MASK(0)

#define RSW2_RMAC_MMIS2	0x0230 /* MAC monitoring interrupt status 2 */
#define MMIS2_LPIDIS	BIT_MASK(2)
#define MMIS2_LPIAIS	BIT_MASK(1)
#define MMIS2_MPDIS		BIT_MASK(0)

#define RSW2_RMAC_MMIE2	0x0234 /* MAC monitoring interrupt enable 2 */
#define MMIE2_LPIDIE	BIT_MASK(2)
#define MMIE2_LPIAIE	BIT_MASK(1)
#define MMIE2_MPDIE		BIT_MASK(0)

#define RSW2_RMAC_MMID2	0x0238 /* MAC monitoring interrupt disable 2 */
#define MMID2_LPIDID	BIT_MASK(2)
#define MMID2_LPIAID	BIT_MASK(1)
#define MMID2_MPDID		BIT_MASK(0)

#define RSW2_RMAC_MMPFTCT	0x0300 /* MAC manual pause frame transmit counter */
#define MMPFTCT_MPFTC		GENMASK(15, 0)

#define RSW2_RMAC_MAPFTCT	0x0304 /* MAC automatic pause frame transmit counter */
#define MAPFTCT_APFTC		GENMASK(15, 0)

#define RSW2_RMAC_MPFRCT	0x0308 /* MAC pause frame receive counter */
#define MPFRCT_PFRC			GENMASK(15, 0)

#define RSW2_RMAC_MFCICT	0x030c /* MAC false carrier indication counter */
#define MFCICT_FCIC			GENMASK(15, 0)

#define RSW2_RMAC_MEEECT	0x0310 /* MAC energy efficient ethernet counter */
#define MEEECT_EEERC		GENMASK(15, 0)

#define RSW2_RMAC_MMPCFTCT(t)	(0x0320 + 4 * (t)) /* MAC manual PFC frame transmit counter */
#define MMPCFTCT_MPCFCTC		GENMASK(15, 0)

#define RSW2_RMAC_MAPCFTCT(t)	(0x0330 + 4 * (t)) /* MAC automatic PFC frame transmit counter */
#define MAPCFTCT_APCFCTC		GENMASK(15, 0)

#define RSW2_RMAC_MPCFRCT(t)	(0x0340 + 4 * (t)) /* MAC PFC frame receive counter */
#define MPCFRCT_PCFCRC			GENMASK(15, 0)

#define RSW2_RMAC_MROVFC	0x0360 /* Receive overflow Counter */
#define MROVFC_ROVFC		GENMASK(31, 0)

#define RSW2_RMAC_MRHCRCEC	0x0364 /* Reception Header-CRC(PCH CRC) error Counter */
#define MRHCRCEC_RHCRCEC	GENMASK(15, 0)


#define RSW2_RMAC_MRGFCE	0x0408 /* RMAC Received good frame counter E-frames */
#define MRGFCE_RGFNE		GENMASK(31, 0)

#define RSW2_RMAC_MRGFCP	0x040C /* RMAC Received good frame counter P-frames */
#define MRGFCP_RGFNP		GENMASK(31, 0)

#define RSW2_RMAC_MRBFC	0x0410 /* RMAC Received good broadcast frame counter */
#define MRBFC_RBFN		GENMASK(31, 0)

#define RSW2_RMAC_MRMFC	0x0414 /* RMAC Received good multicast frame counter */
#define MRMFC_RMFN		GENMASK(31, 0)

#define RSW2_RMAC_MRUFC	0x0418 /* RMAC Received good unicast frame counter */
#define MRUFC_RUFN		GENMASK(31, 0)

#define RSW2_RMAC_MREFC	0x041C /* RMAC Received PHY error frame count */
#define MREFC_RPEFN		GENMASK(15, 0)

#define RSW2_RMAC_MRNEFC	0x0420 /* RMAC Received nibble error frame count */
#define MRNEFC_RNEFN		GENMASK(15, 0)

#define RSW2_RMAC_MRFMEFC	0x0424 /* RMAC Received FCS/mCRC error frame count */
#define MRFMEFC_RFMEFN		GENMASK(31, 0)

#define RSW2_RMAC_MRFFMEFC	0x0428 /* RMAC final fragment missing error frame count */
#define MRFFMEFC_RFFMEFN	GENMASK(15, 0)

#define RSW2_RMAC_MRCFCEFC	0x042C /* RMAC Received C-fragment count error frame count */
#define MRCFCEFC_RCFCEFN	GENMASK(15, 0)

#define RSW2_RMAC_MRFCEFC	0x0430 /* RMAC Received fragment count error frame count */
#define MRFCEFC_RFCEFN		GENMASK(15, 0)

#define RSW2_RMAC_MRRCFEFC	0x0434  /* RMAC Received RMAC filter error frame count */
#define MRRCFEFC_RRCFEFN	GENMASK(15, 0)

#define RSW2_RMAC_MRFC		0x0438 /* RMAC Received frame count */
#define MRFC_RFN			GENMASK(31, 0)

#define RSW2_RMAC_MRGUEFC	0x043C /* RMAC Received good undersize error frame count */
#define MRGUEFC_RGUEFN		GENMASK(31, 0)

#define RSW2_RMAC_MRBUEFC	0x0440 /* RMAC Received bad undersize error frame count */
#define MRBUEFC_RBUEFN		GENMASK(31, 0)

#define RSW2_RMAC_MRGOEFC	0x0444 /* RMAC Received good oversize error frame count */
#define MRGOEFC_RGOEFN		GENMASK(31, 0)

#define RSW2_RMAC_MRBOEFC	0x0448 /* RMAC Received bad oversize error frame count */
#define MRBOEFC_RBOEFN		GENMASK(31, 0)

#define RSW2_RMAC_MRXBCEU	0x044C  /* RMAC Received byte counter E-frames upper side */
#define MRXBCEU_TBNEU		GENMASK(31, 0)

#define RSW2_RMAC_MRXBCEL	0x0450 /* RMAC Received byte counter E-frames lower side */
#define MRXBCEL_TBNEL		GENMASK(31, 0)

#define RSW2_RMAC_MRXBCPU	0x0454 /* RMAC Received byte counter P-frames upper side */
#define MRXBCPU_TBNPU		GENMASK(31, 0)

#define RSW2_RMAC_MRXBCPL	0x0458 /* RMAC Received byte counter P-frames lower side */
#define MRXBCPL_TBNPL		GENMASK(31, 0)

#define RSW2_RMAC_MTGFCE	0x0508 /* Transmitted good frame counter E-frames */
#define RSW2_RMAC_MTGFCP	0x050C /* Transmitted good frame counter P-frames */
#define RSW2_RMAC_MTBFC		0x0510 /* Transmitted broadcast frame counter */
#define RSW2_RMAC_MTMFC		0x0514 /* Transmitted multicast frame counter */
#define RSW2_RMAC_MTUFC		0x0518 /* Transmitted unicast frame counter */
#define RSW2_RMAC_MTEFC		0x051C /* Transmitted error frame counter */
#define RSW2_RMAC_MTXBCEU	0x0520 /* Transmitted byte counter E-frames Upper */
#define RSW2_RMAC_MTXBCEL	0x0524 /* Transmitted byte counter E-frames Lower */
#define RSW2_RMAC_MTXBCPU	0x0528 /* Transmitted byte counter P-frames Upper */
#define RSW2_RMAC_MTXBCPL	0x052C /* Transmitted byte counter P-frames Lower */

#endif /* _RSWITCH2_GWCA_H */

