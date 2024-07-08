// SPDX-License-Identifier: GPL-2.0
/* Renesas Ethernet Switch device driver
 *
 * Copyright (C) 2020 Renesas Electronics Corporation
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/net_tstamp.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/reset.h>
#include <linux/sys_soc.h>
#include <net/switchdev.h>

#include "rtsn_ptp.h"

static void *debug_addr;
static inline u32 rs_read32(void *addr)
{
	return ioread32(addr);
}

static inline void rs_write32(u32 data, void *addr)
{
	iowrite32(data, addr);
}

#define RSWITCH_NUM_HW		5
#define RSWITCH_MAX_NUM_ETHA	3

#define RSWITCH_AXI_CHAIN_W	7
#define RSWITCH_MAX_NUM_CHAINS	(1 << RSWITCH_AXI_CHAIN_W)

#define RSWITCH_MAC_ENTRY_W	10

#define RSWITCH_LTH_STREAM_W	10

#define RSWITCH_LTH_RRULE_W	8
#define RSWITCH_MAX_NUM_RRULE	(1 << RSWITCH_LTH_RRULE_W)

#define RSWITCH_PSFP_MSDU_W	4
#define RSWITCH_PSFP_GATE_W	3
#define RSWITCH_PSFP_MTR_W	5
#define RSWITCH_FRER_RECE_W	7

#define RSWITCH_SERDES_NUM	3

#define RSWITCH_GWCA_IDX_TO_HW_NUM(i)	((i) + RSWITCH_MAX_NUM_ETHA)
#define RSWITCH_HW_NUM_TO_GWCA_IDX(i)	((i) - RSWITCH_MAX_NUM_ETHA)

#define TX_RING_SIZE		1024
#define RX_RING_SIZE		1024
#define TS_RING_SIZE		(TX_RING_SIZE * RSWITCH_MAX_NUM_ETHA)

#define MAX_MTU_SZ		9000
#define MAX_DESC_SZ		2048
#define RSWITCH_ALIGN		128

#define RSWITCH_RX_BUF_SIZE	(NET_SKB_PAD + NET_IP_ALIGN \
				 + MAX_DESC_SZ \
				 + SKB_DATA_ALIGN(sizeof(struct skb_shared_info)) \
				 + RSWITCH_ALIGN)

#define RSWITCH_MAX_CTAG_PCP	7

#define GWCA_TS_IRQ_RESOURCE_NAME	"gwca1_rxts0"
#define GWCA_TS_IRQ_NAME		"rswitch: gwca1_rxts0"
#define GWCA_TS_IRQ_BIT			BIT(0)
#define GWCA_IPV_NUM		0

#define RSWITCH_COMA_OFFSET	0x00009000
#define RSWITCH_ETHA_OFFSET	0x0000a000	/* with RMAC */
#define RSWITCH_ETHA_SIZE	0x00002000	/* with RMAC */
#define RSWITCH_GWCA0_OFFSET	0x00010000
#define RSWITCH_GWCA1_OFFSET	0x00012000
#define RSWITCH_GPTP_OFFSET    0x00018000

#define FWRO	0
#define CARO	RSWITCH_COMA_OFFSET
#define GWRO	RSWITCH_GWCA1_OFFSET
/*#define GWRO	RSWITCH_GWCA0_OFFSET*/
#define TARO	0
#define RMRO	0x1000
enum rswitch_reg {
	FWGC		= FWRO + 0x0000,
	FWTTC0		= FWRO + 0x0010,
	FWTTC1		= FWRO + 0x0014,
	FWLBMC		= FWRO + 0x0018,
	FWCEPTC		= FWRO + 0x020,
	FWCEPRC0	= FWRO + 0x024,
	FWCEPRC1	= FWRO + 0x028,
	FWCEPRC2	= FWRO + 0x02C,
	FWCLPTC		= FWRO + 0x030,
	FWCLPRC		= FWRO + 0x034,
	FWCMPTC		= FWRO + 0x040,
	FWEMPTC		= FWRO + 0x044,
	FWSDMPTC	= FWRO + 0x050,
	FWSDMPVC	= FWRO + 0x054,
	FWLBWMC0	= FWRO + 0x080,
	FWPC00		= FWRO + 0x100,
	FWPC10		= FWRO + 0x104,
	FWPC20		= FWRO + 0x108,
	FWCTGC00	= FWRO + 0x400,
	FWCTGC10	= FWRO + 0x404,
	FWCTTC00	= FWRO + 0x408,
	FWCTTC10	= FWRO + 0x40C,
	FWCTTC200	= FWRO + 0x410,
	FWCTSC00	= FWRO + 0x420,
	FWCTSC10	= FWRO + 0x424,
	FWCTSC20	= FWRO + 0x428,
	FWCTSC30	= FWRO + 0x42C,
	FWCTSC40	= FWRO + 0x430,
	FWTWBFC0	= FWRO + 0x1000,
	FWTWBFVC0	= FWRO + 0x1004,
	FWTHBFC0	= FWRO + 0x1400,
	FWTHBFV0C0	= FWRO + 0x1404,
	FWTHBFV1C0	= FWRO + 0x1408,
	FWFOBFC0	= FWRO + 0x1800,
	FWFOBFV0C0	= FWRO + 0x1804,
	FWFOBFV1C0	= FWRO + 0x1808,
	FWRFC0		= FWRO + 0x1C00,
	FWRFVC0		= FWRO + 0x1C04,
	FWCFC0		= FWRO + 0x2000,
	FWCFMC00	= FWRO + 0x2004,
	FWIP4SC		= FWRO + 0x4008,
	FWIP6SC		= FWRO + 0x4018,
	FWIP6OC		= FWRO + 0x401C,
	FWL2SC		= FWRO + 0x4020,
	FWSFHEC		= FWRO + 0x4030,
	FWSHCR0		= FWRO + 0x4040,
	FWSHCR1		= FWRO + 0x4044,
	FWSHCR2		= FWRO + 0x4048,
	FWSHCR3		= FWRO + 0x404C,
	FWSHCR4		= FWRO + 0x4050,
	FWSHCR5		= FWRO + 0x4054,
	FWSHCR6		= FWRO + 0x4058,
	FWSHCR7		= FWRO + 0x405C,
	FWSHCR8		= FWRO + 0x4060,
	FWSHCR9		= FWRO + 0x4064,
	FWSHCR10	= FWRO + 0x4068,
	FWSHCR11	= FWRO + 0x406C,
	FWSHCR12	= FWRO + 0x4070,
	FWSHCR13	= FWRO + 0x4074,
	FWSHCRR		= FWRO + 0x4078,
	FWLTHHEC	= FWRO + 0x4090,
	FWLTHHC		= FWRO + 0x4094,
	FWLTHTL0	= FWRO + 0x40A0,
	FWLTHTL1	= FWRO + 0x40A4,
	FWLTHTL2	= FWRO + 0x40A8,
	FWLTHTL3	= FWRO + 0x40AC,
	FWLTHTL4	= FWRO + 0x40B0,
	FWLTHTL5	= FWRO + 0x40B4,
	FWLTHTL6	= FWRO + 0x40B8,
	FWLTHTL7	= FWRO + 0x40BC,
	FWLTHTL80	= FWRO + 0x40C0,
	FWLTHTL9	= FWRO + 0x40D0,
	FWLTHTLR	= FWRO + 0x40D4,
	FWLTHTIM	= FWRO + 0x40E0,
	FWLTHTEM	= FWRO + 0x40E4,
	FWLTHTS0	= FWRO + 0x4100,
	FWLTHTS1	= FWRO + 0x4104,
	FWLTHTS2	= FWRO + 0x4108,
	FWLTHTS3	= FWRO + 0x410C,
	FWLTHTS4	= FWRO + 0x4110,
	FWLTHTSR0	= FWRO + 0x4120,
	FWLTHTSR1	= FWRO + 0x4124,
	FWLTHTSR2	= FWRO + 0x4128,
	FWLTHTSR3	= FWRO + 0x412C,
	FWLTHTSR40	= FWRO + 0x4130,
	FWLTHTSR5	= FWRO + 0x4140,
	FWLTHTR		= FWRO + 0x4150,
	FWLTHTRR0	= FWRO + 0x4154,
	FWLTHTRR1	= FWRO + 0x4158,
	FWLTHTRR2	= FWRO + 0x415C,
	FWLTHTRR3	= FWRO + 0x4160,
	FWLTHTRR4	= FWRO + 0x4164,
	FWLTHTRR5	= FWRO + 0x4168,
	FWLTHTRR6	= FWRO + 0x416C,
	FWLTHTRR7	= FWRO + 0x4170,
	FWLTHTRR8	= FWRO + 0x4174,
	FWLTHTRR9	= FWRO + 0x4180,
	FWLTHTRR10	= FWRO + 0x4190,
	FWIPHEC		= FWRO + 0x4214,
	FWIPHC		= FWRO + 0x4218,
	FWIPTL0		= FWRO + 0x4220,
	FWIPTL1		= FWRO + 0x4224,
	FWIPTL2		= FWRO + 0x4228,
	FWIPTL3		= FWRO + 0x422C,
	FWIPTL4		= FWRO + 0x4230,
	FWIPTL5		= FWRO + 0x4234,
	FWIPTL6		= FWRO + 0x4238,
	FWIPTL7		= FWRO + 0x4240,
	FWIPTL8		= FWRO + 0x4250,
	FWIPTLR		= FWRO + 0x4254,
	FWIPTIM		= FWRO + 0x4260,
	FWIPTEM		= FWRO + 0x4264,
	FWIPTS0		= FWRO + 0x4270,
	FWIPTS1		= FWRO + 0x4274,
	FWIPTS2		= FWRO + 0x4278,
	FWIPTS3		= FWRO + 0x427C,
	FWIPTS4		= FWRO + 0x4280,
	FWIPTSR0	= FWRO + 0x4284,
	FWIPTSR1	= FWRO + 0x4288,
	FWIPTSR2	= FWRO + 0x428C,
	FWIPTSR3	= FWRO + 0x4290,
	FWIPTSR4	= FWRO + 0x42A0,
	FWIPTR		= FWRO + 0x42B0,
	FWIPTRR0	= FWRO + 0x42B4,
	FWIPTRR1	= FWRO + 0x42B8,
	FWIPTRR2	= FWRO + 0x42BC,
	FWIPTRR3	= FWRO + 0x42C0,
	FWIPTRR4	= FWRO + 0x42C4,
	FWIPTRR5	= FWRO + 0x42C8,
	FWIPTRR6	= FWRO + 0x42CC,
	FWIPTRR7	= FWRO + 0x42D0,
	FWIPTRR8	= FWRO + 0x42E0,
	FWIPTRR9	= FWRO + 0x42F0,
	FWIPHLEC	= FWRO + 0x4300,
	FWIPAGUSPC	= FWRO + 0x4500,
	FWIPAGC		= FWRO + 0x4504,
	FWIPAGM0	= FWRO + 0x4510,
	FWIPAGM1	= FWRO + 0x4514,
	FWIPAGM2	= FWRO + 0x4518,
	FWIPAGM3	= FWRO + 0x451C,
	FWIPAGM4	= FWRO + 0x4520,
	FWMACHEC	= FWRO + 0x4620,
	FWMACHC		= FWRO + 0x4624,
	FWMACTL0	= FWRO + 0x4630,
	FWMACTL1	= FWRO + 0x4634,
	FWMACTL2	= FWRO + 0x4638,
	FWMACTL3	= FWRO + 0x463C,
	FWMACTL4	= FWRO + 0x4640,
	FWMACTL5	= FWRO + 0x4650,
	FWMACTLR	= FWRO + 0x4654,
	FWMACTIM	= FWRO + 0x4660,
	FWMACTEM	= FWRO + 0x4664,
	FWMACTS0	= FWRO + 0x4670,
	FWMACTS1	= FWRO + 0x4674,
	FWMACTSR0	= FWRO + 0x4678,
	FWMACTSR1	= FWRO + 0x467C,
	FWMACTSR2	= FWRO + 0x4680,
	FWMACTSR3	= FWRO + 0x4690,
	FWMACTR		= FWRO + 0x46A0,
	FWMACTRR0	= FWRO + 0x46A4,
	FWMACTRR1	= FWRO + 0x46A8,
	FWMACTRR2	= FWRO + 0x46AC,
	FWMACTRR3	= FWRO + 0x46B0,
	FWMACTRR4	= FWRO + 0x46B4,
	FWMACTRR5	= FWRO + 0x46C0,
	FWMACTRR6	= FWRO + 0x46D0,
	FWMACHLEC	= FWRO + 0x4700,
	FWMACAGUSPC	= FWRO + 0x4880,
	FWMACAGC	= FWRO + 0x4884,
	FWMACAGM0	= FWRO + 0x4888,
	FWMACAGM1	= FWRO + 0x488C,
	FWVLANTEC	= FWRO + 0x4900,
	FWVLANTL0	= FWRO + 0x4910,
	FWVLANTL1	= FWRO + 0x4914,
	FWVLANTL2	= FWRO + 0x4918,
	FWVLANTL3	= FWRO + 0x4920,
	FWVLANTL4	= FWRO + 0x4930,
	FWVLANTLR	= FWRO + 0x4934,
	FWVLANTIM	= FWRO + 0x4940,
	FWVLANTEM	= FWRO + 0x4944,
	FWVLANTS	= FWRO + 0x4950,
	FWVLANTSR0	= FWRO + 0x4954,
	FWVLANTSR1	= FWRO + 0x4958,
	FWVLANTSR2	= FWRO + 0x4960,
	FWVLANTSR3	= FWRO + 0x4970,
	FWPBFCi		= FWRO + 0x4A00,
	FWPBFCSDC00	= FWRO + 0x4A04,
	FWL23URL0	= FWRO + 0x4E00,
	FWL23URL1	= FWRO + 0x4E04,
	FWL23URL2	= FWRO + 0x4E08,
	FWL23URL3	= FWRO + 0x4E0C,
	FWL23URLR	= FWRO + 0x4E10,
	FWL23UTIM	= FWRO + 0x4E20,
	FWL23URR	= FWRO + 0x4E30,
	FWL23URRR0	= FWRO + 0x4E34,
	FWL23URRR1	= FWRO + 0x4E38,
	FWL23URRR2	= FWRO + 0x4E3C,
	FWL23URRR3	= FWRO + 0x4E40,
	FWL23URMC0	= FWRO + 0x4F00,
	FWPMFGC0	= FWRO + 0x5000,
	FWPGFC0		= FWRO + 0x5100,
	FWPGFIGSC0	= FWRO + 0x5104,
	FWPGFENC0	= FWRO + 0x5108,
	FWPGFENM0	= FWRO + 0x510c,
	FWPGFCSTC00	= FWRO + 0x5110,
	FWPGFCSTC10	= FWRO + 0x5114,
	FWPGFCSTM00	= FWRO + 0x5118,
	FWPGFCSTM10	= FWRO + 0x511C,
	FWPGFCTC0	= FWRO + 0x5120,
	FWPGFCTM0	= FWRO + 0x5124,
	FWPGFHCC0	= FWRO + 0x5128,
	FWPGFSM0	= FWRO + 0x512C,
	FWPGFGC0	= FWRO + 0x5130,
	FWPGFGL0	= FWRO + 0x5500,
	FWPGFGL1	= FWRO + 0x5504,
	FWPGFGLR	= FWRO + 0x5518,
	FWPGFGR		= FWRO + 0x5510,
	FWPGFGRR0	= FWRO + 0x5514,
	FWPGFGRR1	= FWRO + 0x5518,
	FWPGFRIM	= FWRO + 0x5520,
	FWPMTRFC0	= FWRO + 0x5600,
	FWPMTRCBSC0	= FWRO + 0x5604,
	FWPMTRC0RC0	= FWRO + 0x5608,
	FWPMTREBSC0	= FWRO + 0x560C,
	FWPMTREIRC0	= FWRO + 0x5610,
	FWPMTRFM0	= FWRO + 0x5614,
	FWFTL0		= FWRO + 0x6000,
	FWFTL1		= FWRO + 0x6004,
	FWFTLR		= FWRO + 0x6008,
	FWFTOC		= FWRO + 0x6010,
	FWFTOPC		= FWRO + 0x6014,
	FWFTIM		= FWRO + 0x6020,
	FWFTR		= FWRO + 0x6030,
	FWFTRR0		= FWRO + 0x6034,
	FWFTRR1		= FWRO + 0x6038,
	FWFTRR2		= FWRO + 0x603C,
	FWSEQNGC0	= FWRO + 0x6100,
	FWSEQNGM0	= FWRO + 0x6104,
	FWSEQNRC	= FWRO + 0x6200,
	FWCTFDCN0	= FWRO + 0x6300,
	FWLTHFDCN0	= FWRO + 0x6304,
	FWIPFDCN0	= FWRO + 0x6308,
	FWLTWFDCN0	= FWRO + 0x630C,
	FWPBFDCN0	= FWRO + 0x6310,
	FWMHLCN0	= FWRO + 0x6314,
	FWIHLCN0	= FWRO + 0x6318,
	FWICRDCN0	= FWRO + 0x6500,
	FWWMRDCN0	= FWRO + 0x6504,
	FWCTRDCN0	= FWRO + 0x6508,
	FWLTHRDCN0	= FWRO + 0x650C,
	FWIPRDCN0	= FWRO + 0x6510,
	FWLTWRDCN0	= FWRO + 0x6514,
	FWPBRDCN0	= FWRO + 0x6518,
	FWPMFDCN0	= FWRO + 0x6700,
	FWPGFDCN0	= FWRO + 0x6780,
	FWPMGDCN0	= FWRO + 0x6800,
	FWPMYDCN0	= FWRO + 0x6804,
	FWPMRDCN0	= FWRO + 0x6808,
	FWFRPPCN0	= FWRO + 0x6A00,
	FWFRDPCN0	= FWRO + 0x6A04,
	FWEIS00		= FWRO + 0x7900,
	FWEIE00		= FWRO + 0x7904,
	FWEID00		= FWRO + 0x7908,
	FWEIS1		= FWRO + 0x7A00,
	FWEIE1		= FWRO + 0x7A04,
	FWEID1		= FWRO + 0x7A08,
	FWEIS2		= FWRO + 0x7A10,
	FWEIE2		= FWRO + 0x7A14,
	FWEID2		= FWRO + 0x7A18,
	FWEIS3		= FWRO + 0x7A20,
	FWEIE3		= FWRO + 0x7A24,
	FWEID3		= FWRO + 0x7A28,
	FWEIS4		= FWRO + 0x7A30,
	FWEIE4		= FWRO + 0x7A34,
	FWEID4		= FWRO + 0x7A38,
	FWEIS5		= FWRO + 0x7A40,
	FWEIE5		= FWRO + 0x7A44,
	FWEID5		= FWRO + 0x7A48,
	FWEIS60		= FWRO + 0x7A50,
	FWEIE60		= FWRO + 0x7A54,
	FWEID60		= FWRO + 0x7A58,
	FWEIS61		= FWRO + 0x7A60,
	FWEIE61		= FWRO + 0x7A64,
	FWEID61		= FWRO + 0x7A68,
	FWEIS62		= FWRO + 0x7A70,
	FWEIE62		= FWRO + 0x7A74,
	FWEID62		= FWRO + 0x7A78,
	FWEIS63		= FWRO + 0x7A80,
	FWEIE63		= FWRO + 0x7A84,
	FWEID63		= FWRO + 0x7A88,
	FWEIS70		= FWRO + 0x7A90,
	FWEIE70		= FWRO + 0x7A94,
	FWEID70		= FWRO + 0x7A98,
	FWEIS71		= FWRO + 0x7AA0,
	FWEIE71		= FWRO + 0x7AA4,
	FWEID71		= FWRO + 0x7AA8,
	FWEIS72		= FWRO + 0x7AB0,
	FWEIE72		= FWRO + 0x7AB4,
	FWEID72		= FWRO + 0x7AB8,
	FWEIS73		= FWRO + 0x7AC0,
	FWEIE73		= FWRO + 0x7AC4,
	FWEID73		= FWRO + 0x7AC8,
	FWEIS80		= FWRO + 0x7AD0,
	FWEIE80		= FWRO + 0x7AD4,
	FWEID80		= FWRO + 0x7AD8,
	FWEIS81		= FWRO + 0x7AE0,
	FWEIE81		= FWRO + 0x7AE4,
	FWEID81		= FWRO + 0x7AE8,
	FWEIS82		= FWRO + 0x7AF0,
	FWEIE82		= FWRO + 0x7AF4,
	FWEID82		= FWRO + 0x7AF8,
	FWEIS83		= FWRO + 0x7B00,
	FWEIE83		= FWRO + 0x7B04,
	FWEID83		= FWRO + 0x7B08,
	FWMIS0		= FWRO + 0x7C00,
	FWMIE0		= FWRO + 0x7C04,
	FWMID0		= FWRO + 0x7C08,
	FWSCR0		= FWRO + 0x7D00,
	FWSCR1		= FWRO + 0x7D04,
	FWSCR2		= FWRO + 0x7D08,
	FWSCR3		= FWRO + 0x7D0C,
	FWSCR4		= FWRO + 0x7D10,
	FWSCR5		= FWRO + 0x7D14,
	FWSCR6		= FWRO + 0x7D18,
	FWSCR7		= FWRO + 0x7D1C,
	FWSCR8		= FWRO + 0x7D20,
	FWSCR9		= FWRO + 0x7D24,
	FWSCR10		= FWRO + 0x7D28,
	FWSCR11		= FWRO + 0x7D2C,
	FWSCR12		= FWRO + 0x7D30,
	FWSCR13		= FWRO + 0x7D34,
	FWSCR14		= FWRO + 0x7D38,
	FWSCR15		= FWRO + 0x7D3C,
	FWSCR16		= FWRO + 0x7D40,
	FWSCR17		= FWRO + 0x7D44,
	FWSCR18		= FWRO + 0x7D48,
	FWSCR19		= FWRO + 0x7D4C,
	FWSCR20		= FWRO + 0x7D50,
	FWSCR21		= FWRO + 0x7D54,
	FWSCR22		= FWRO + 0x7D58,
	FWSCR23		= FWRO + 0x7D5C,
	FWSCR24		= FWRO + 0x7D60,
	FWSCR25		= FWRO + 0x7D64,
	FWSCR26		= FWRO + 0x7D68,
	FWSCR27		= FWRO + 0x7D6C,
	FWSCR28		= FWRO + 0x7D70,
	FWSCR29		= FWRO + 0x7D74,
	FWSCR30		= FWRO + 0x7D78,
	FWSCR31		= FWRO + 0x7D7C,
	FWSCR32		= FWRO + 0x7D80,
	FWSCR33		= FWRO + 0x7D84,
	FWSCR34		= FWRO + 0x7D88,
	FWSCR35		= FWRO + 0x7D8C,
	FWSCR36		= FWRO + 0x7D90,
	FWSCR37		= FWRO + 0x7D94,
	FWSCR38		= FWRO + 0x7D98,
	FWSCR39		= FWRO + 0x7D9C,
	FWSCR40		= FWRO + 0x7DA0,
	FWSCR41		= FWRO + 0x7DA4,
	FWSCR42		= FWRO + 0x7DA8,
	FWSCR43		= FWRO + 0x7DAC,
	FWSCR44		= FWRO + 0x7DB0,
	FWSCR45		= FWRO + 0x7DB4,
	FWSCR46		= FWRO + 0x7DB8,

	RIPV		= CARO + 0x0000,
	RRC		= CARO + 0x0004,
	RCEC		= CARO + 0x0008,
	RCDC		= CARO + 0x000C,
	RSSIS		= CARO + 0x0010,
	RSSIE		= CARO + 0x0014,
	RSSID		= CARO + 0x0018,
	CABPIBWMC	= CARO + 0x0020,
	CABPWMLC	= CARO + 0x0040,
	CABPPFLC0	= CARO + 0x0050,
	CABPPWMLC0	= CARO + 0x0060,
	CABPPPFLC00	= CARO + 0x00A0,
	CABPULC		= CARO + 0x0100,
	CABPIRM		= CARO + 0x0140,
	CABPPCM		= CARO + 0x0144,
	CABPLCM		= CARO + 0x0148,
	CABPCPM		= CARO + 0x0180,
	CABPMCPM	= CARO + 0x0200,
	CARDNM		= CARO + 0x0280,
	CARDMNM		= CARO + 0x0284,
	CARDCN		= CARO + 0x0290,
	CAEIS0		= CARO + 0x0300,
	CAEIE0		= CARO + 0x0304,
	CAEID0		= CARO + 0x0308,
	CAEIS1		= CARO + 0x0310,
	CAEIE1		= CARO + 0x0314,
	CAEID1		= CARO + 0x0318,
	CAMIS0		= CARO + 0x0340,
	CAMIE0		= CARO + 0x0344,
	CAMID0		= CARO + 0x0348,
	CAMIS1		= CARO + 0x0350,
	CAMIE1		= CARO + 0x0354,
	CAMID1		= CARO + 0x0358,
	CASCR		= CARO + 0x0380,

	/* Ethernet Agent Address space Empty in spec */
	EAMC		= TARO + 0x0000,
	EAMS		= TARO + 0x0004,
	EAIRC		= TARO + 0x0010,
	EATDQSC		= TARO + 0x0014,
	EATDQC		= TARO + 0x0018,
	EATDQAC		= TARO + 0x001C,
	EATPEC		= TARO + 0x0020,
	EATMFSC0	= TARO + 0x0040,
	EATDQDC0	= TARO + 0x0060,
	EATDQM0		= TARO + 0x0080,
	EATDQMLM0	= TARO + 0x00A0,
	EACTQC		= TARO + 0x0100,
	EACTDQDC	= TARO + 0x0104,
	EACTDQM		= TARO + 0x0108,
	EACTDQMLM	= TARO + 0x010C,
	EAVCC		= TARO + 0x0130,
	EAVTC		= TARO + 0x0134,
	EATTFC		= TARO + 0x0138,
	EACAEC		= TARO + 0x0200,
	EACC		= TARO + 0x0204,
	EACAIVC0	= TARO + 0x0220,
	EACAULC0	= TARO + 0x0240,
	EACOEM		= TARO + 0x0260,
	EACOIVM0	= TARO + 0x0280,
	EACOULM0	= TARO + 0x02A0,
	EACGSM		= TARO + 0x02C0,
	EATASC		= TARO + 0x0300,
	EATASENC0	= TARO + 0x0320,
	EATASCTENC	= TARO + 0x0340,
	EATASENM0	= TARO + 0x0360,
	EATASCTENM	= TARO + 0x0380,
	EATASCSTC0	= TARO + 0x03A0,
	EATASCSTC1	= TARO + 0x03A4,
	EATASCSTM0	= TARO + 0x03A8,
	EATASCSTM1	= TARO + 0x03AC,
	EATASCTC	= TARO + 0x03B0,
	EATASCTM	= TARO + 0x03B4,
	EATASGL0	= TARO + 0x03C0,
	EATASGL1	= TARO + 0x03C4,
	EATASGLR	= TARO + 0x03C8,
	EATASGR		= TARO + 0x03D0,
	EATASGRR	= TARO + 0x03D4,
	EATASHCC	= TARO + 0x03E0,
	EATASRIRM	= TARO + 0x03E4,
	EATASSM		= TARO + 0x03E8,
	EAUSMFSECN	= TARO + 0x0400,
	EATFECN		= TARO + 0x0404,
	EAFSECN		= TARO + 0x0408,
	EADQOECN	= TARO + 0x040C,
	EADQSECN	= TARO + 0x0410,
	EACKSECN	= TARO + 0x0414,
	EAEIS0		= TARO + 0x0500,
	EAEIE0		= TARO + 0x0504,
	EAEID0		= TARO + 0x0508,
	EAEIS1		= TARO + 0x0510,
	EAEIE1		= TARO + 0x0514,
	EAEID1		= TARO + 0x0518,
	EAEIS2		= TARO + 0x0520,
	EAEIE2		= TARO + 0x0524,
	EAEID2		= TARO + 0x0528,
	EASCR		= TARO + 0x0580,

	MPSM		= RMRO + 0x0000,
	MPIC		= RMRO + 0x0004,
	MPIM		= RMRO + 0x0008,
	MIOC		= RMRO + 0x0010,
	MIOM		= RMRO + 0x0014,
	MXMS		= RMRO + 0x0018,
	MTFFC		= RMRO + 0x0020,
	MTPFC		= RMRO + 0x0024,
	MTPFC2		= RMRO + 0x0028,
	MTPFC30		= RMRO + 0x0030,
	MTATC0		= RMRO + 0x0050,
	MTIM		= RMRO + 0x0060,
	MRGC		= RMRO + 0x0080,
	MRMAC0		= RMRO + 0x0084,
	MRMAC1		= RMRO + 0x0088,
	MRAFC		= RMRO + 0x008C,
	MRSCE		= RMRO + 0x0090,
	MRSCP		= RMRO + 0x0094,
	MRSCC		= RMRO + 0x0098,
	MRFSCE		= RMRO + 0x009C,
	MRFSCP		= RMRO + 0x00a0,
	MTRC		= RMRO + 0x00a4,
	MRIM		= RMRO + 0x00a8,
	MRPFM		= RMRO + 0x00aC,
	MPFC0		= RMRO + 0x0100,
	MLVC		= RMRO + 0x0180,
	MEEEC		= RMRO + 0x0184,
	MLBC		= RMRO + 0x0188,
	MXGMIIC		= RMRO + 0x0190,
	MPCH		= RMRO + 0x0194,
	MANC		= RMRO + 0x0198,
	MANM		= RMRO + 0x019C,
	MPLCA1		= RMRO + 0x01a0,
	MPLCA2		= RMRO + 0x01a4,
	MPLCA3		= RMRO + 0x01a8,
	MPLCA4		= RMRO + 0x01ac,
	MPLCAM		= RMRO + 0x01b0,
	MHDC1		= RMRO + 0x01c0,
	MHDC2		= RMRO + 0x01c4,
	MEIS		= RMRO + 0x0200,
	MEIE		= RMRO + 0x0204,
	MEID		= RMRO + 0x0208,
	MMIS0		= RMRO + 0x0210,
	MMIE0		= RMRO + 0x0214,
	MMID0		= RMRO + 0x0218,
	MMIS1		= RMRO + 0x0220,
	MMIE1		= RMRO + 0x0224,
	MMID1		= RMRO + 0x0228,
	MMIS2		= RMRO + 0x0230,
	MMIE2		= RMRO + 0x0234,
	MMID2		= RMRO + 0x0238,
	MMPFTCT		= RMRO + 0x0300,
	MAPFTCT		= RMRO + 0x0304,
	MPFRCT		= RMRO + 0x0308,
	MFCICT		= RMRO + 0x030c,
	MEEECT		= RMRO + 0x0310,
	MMPCFTCT0	= RMRO + 0x0320,
	MAPCFTCT0	= RMRO + 0x0330,
	MPCFRCT0	= RMRO + 0x0340,
	MHDCC		= RMRO + 0x0350,
	MROVFC		= RMRO + 0x0354,
	MRHCRCEC	= RMRO + 0x0358,
	MRXBCE		= RMRO + 0x0400,
	MRXBCP		= RMRO + 0x0404,
	MRGFCE		= RMRO + 0x0408,
	MRGFCP		= RMRO + 0x040C,
	MRBFC		= RMRO + 0x0410,
	MRMFC		= RMRO + 0x0414,
	MRUFC		= RMRO + 0x0418,
	MRPEFC		= RMRO + 0x041C,
	MRNEFC		= RMRO + 0x0420,
	MRFMEFC		= RMRO + 0x0424,
	MRFFMEFC	= RMRO + 0x0428,
	MRCFCEFC	= RMRO + 0x042C,
	MRFCEFC		= RMRO + 0x0430,
	MRRCFEFC	= RMRO + 0x0434,
	MRUEFC		= RMRO + 0x043C,
	MROEFC		= RMRO + 0x0440,
	MRBOEC		= RMRO + 0x0444,
	MTXBCE		= RMRO + 0x0500,
	MTXBCP		= RMRO + 0x0504,
	MTGFCE		= RMRO + 0x0508,
	MTGFCP		= RMRO + 0x050C,
	MTBFC		= RMRO + 0x0510,
	MTMFC		= RMRO + 0x0514,
	MTUFC		= RMRO + 0x0518,
	MTEFC		= RMRO + 0x051C,

	GWMC		= GWRO + 0x0000,
	GWMS		= GWRO + 0x0004,
	GWIRC		= GWRO + 0x0010,
	GWRDQSC		= GWRO + 0x0014,
	GWRDQC		= GWRO + 0x0018,
	GWRDQAC		= GWRO + 0x001C,
	GWRGC		= GWRO + 0x0020,
	GWRMFSC0	= GWRO + 0x0040,
	GWRDQDC0	= GWRO + 0x0060,
	GWRDQM0		= GWRO + 0x0080,
	GWRDQMLM0	= GWRO + 0x00A0,
	GWMTIRM		= GWRO + 0x0100,
	GWMSTLS		= GWRO + 0x0104,
	GWMSTLR		= GWRO + 0x0108,
	GWMSTSS		= GWRO + 0x010C,
	GWMSTSR		= GWRO + 0x0110,
	GWMAC0		= GWRO + 0x0120,
	GWMAC1		= GWRO + 0x0124,
	GWVCC		= GWRO + 0x0130,
	GWVTC		= GWRO + 0x0134,
	GWTTFC		= GWRO + 0x0138,
	GWTDCAC00	= GWRO + 0x0140,
	GWTDCAC10	= GWRO + 0x0144,
	GWTSDCC0	= GWRO + 0x0160,
	GWTNM		= GWRO + 0x0180,
	GWTMNM		= GWRO + 0x0184,
	GWAC		= GWRO + 0x0190,
	GWDCBAC0	= GWRO + 0x0194,
	GWDCBAC1	= GWRO + 0x0198,
	GWIICBSC	= GWRO + 0x019C,
	GWMDNC		= GWRO + 0x01A0,
	GWTRC0		= GWRO + 0x0200,
	GWTPC0		= GWRO + 0x0300,
	GWARIRM		= GWRO + 0x0380,
	GWDCC0		= GWRO + 0x0400,
	GWAARSS		= GWRO + 0x0800,
	GWAARSR0	= GWRO + 0x0804,
	GWAARSR1	= GWRO + 0x0808,
	GWIDAUAS0	= GWRO + 0x0840,
	GWIDASM0	= GWRO + 0x0880,
	GWIDASAM00	= GWRO + 0x0900,
	GWIDASAM10	= GWRO + 0x0904,
	GWIDACAM00	= GWRO + 0x0980,
	GWIDACAM10	= GWRO + 0x0984,
	GWGRLC		= GWRO + 0x0A00,
	GWGRLULC	= GWRO + 0x0A04,
	GWRLIVC0	= GWRO + 0x0A80,
	GWRLULC0	= GWRO + 0x0A84,
	GWIDPC		= GWRO + 0x0B00,
	GWIDC0		= GWRO + 0x0C00,
	GWDIS0		= GWRO + 0x1100,
	GWDIE0		= GWRO + 0x1104,
	GWDID0		= GWRO + 0x1108,
	GWTSDIS		= GWRO + 0x1180,
	GWTSDIE		= GWRO + 0x1184,
	GWTSDID		= GWRO + 0x1188,
	GWEIS0		= GWRO + 0x1190,
	GWEIE0		= GWRO + 0x1194,
	GWEID0		= GWRO + 0x1198,
	GWEIS1		= GWRO + 0x11A0,
	GWEIE1		= GWRO + 0x11A4,
	GWEID1		= GWRO + 0x11A8,
	GWEIS20		= GWRO + 0x1200,
	GWEIE20		= GWRO + 0x1204,
	GWEID20		= GWRO + 0x1208,
	GWEIS3		= GWRO + 0x1280,
	GWEIE3		= GWRO + 0x1284,
	GWEID3		= GWRO + 0x1288,
	GWEIS4		= GWRO + 0x1290,
	GWEIE4		= GWRO + 0x1294,
	GWEID4		= GWRO + 0x1298,
	GWEIS5		= GWRO + 0x12A0,
	GWEIE5		= GWRO + 0x12A4,
	GWEID5		= GWRO + 0x12A8,
	GWSCR0		= GWRO + 0x1800,
	GWSCR1		= GWRO + 0x1900,
};

/* ETHA/RMAC */
enum rswitch_etha_mode {
	EAMC_OPC_RESET,
	EAMC_OPC_DISABLE,
	EAMC_OPC_CONFIG,
	EAMC_OPC_OPERATION,
};
#define EAMS_OPS_MASK	EAMC_OPC_OPERATION

#define EAVCC_VEM_SC_TAG	(0x3 << 16)

#define MPIC_PIS_MASK		GENMASK(2, 0)
#define MPIC_PIS_MII		0x00
#define MPIC_PIS_GMII		0x02
#define MPIC_PIS_XGMII		0x04
#define MPIC_LSC_MASK		GENMASK(5, 3)
#define MPIC_LSC_100M		1
#define MPIC_LSC_1G		2
#define MPIC_LSC_2_5G		3
#define MPIC_PSMCS_MASK		GENMASK(22, 16)
#define MPIC_PSMHT_MASK		GENMASK(26, 24)

#define MDIO_READ_C45		0x03
#define MDIO_WRITE_C45		0x01
#define MDIO_READ_C22		0x02
#define MDIO_WRITE_C22		0x01

#define REG_MASK		0xffff
#define DEV_MASK		GENMASK(24, 16)
#define ACCESS_MODE		BIT(30)

#define MPSM_PSME		BIT(0)
#define MPSM_MFF_C45		BIT(2)
#define MPSM_PDA_SHIFT		3
#define MPSM_PDA_MASK		GENMASK(7, MPSM_PDA_SHIFT)
#define MPSM_PDA(val)		((val) << MPSM_PDA_SHIFT)
#define MPSM_PRA_SHIFT		8
#define MPSM_PRA_MASK		GENMASK(12, MPSM_PRA_SHIFT)
#define MPSM_PRA(val)		((val) << MPSM_PRA_SHIFT)
#define MPSM_POP_SHIFT		13
#define MPSM_POP_MASK		GENMASK(14, MPSM_POP_SHIFT)
#define MPSM_POP(val)		((val) << MPSM_POP_SHIFT)
#define MPSM_PRD_SHIFT		16
#define MPSM_PRD_MASK		GENMASK(31, MPSM_PRD_SHIFT)
#define MPSM_PRD_WRITE(val)	((val) << MPSM_PRD_SHIFT)
#define MPSM_PRD_READ(val)	(((val) & MPSM_PRD_MASK) >> MPSM_PRD_SHIFT)

/* Completion flags */
#define MMIS1_PAACS             BIT(2) /* Address */
#define MMIS1_PWACS             BIT(1) /* Write */
#define MMIS1_PRACS             BIT(0) /* Read */
#define MMIS1_CLEAR_FLAGS       0xf

#define MLVC_PLV	BIT(16)

/* GWCA */
enum rswitch_gwca_mode {
	GWMC_OPC_RESET,
	GWMC_OPC_DISABLE,
	GWMC_OPC_CONFIG,
	GWMC_OPC_OPERATION,
};
#define GWMS_OPS_MASK	GWMC_OPC_OPERATION

#define GWMTIRM_MTIOG	BIT(0)
#define GWMTIRM_MTR	BIT(1)

#define GWVCC_VEM_SC_TAG	(0x3 << 16)

#define GWARIRM_ARIOG	BIT(0)
#define GWARIRM_ARR	BIT(1)

#define GWDCC_BALR		BIT(24)
#define GWDCC_DCP_MASK		GENMASK(18, 16)
#define GWDCC_DCP(prio)		FIELD_PREP(GWDCC_DCP_MASK, (prio))
#define GWDCC_DQT		BIT(11)
#define GWDCC_ETS		BIT(9)
#define GWDCC_EDE		BIT(8)

#define GWMDNC_TXDMN(val)	((val & 0x1f) << 8)

#define GWTPC_PPPL(ipv)		BIT(ipv)
#define GWDCC_OFFS(chain)	(GWDCC0 + (chain) * 4)
/* COMA */
#define RRC_RR		BIT(0)
#define RRC_RR_CLR	(0)
#define RCEC_RCE	BIT(16)
#define RCDC_RCD	BIT(16)

#define CABPIRM_BPIOG	BIT(0)
#define CABPIRM_BPR	BIT(1)

#define CABPPFLC_INIT_VALUE	0x00800080

/* MFWD */
#define FWPC0(i)                (FWPC00 + (i) * 0x10)
#define FWPC0_LTHTA	BIT(0)
#define FWPC0_IP4UE	BIT(3)
#define FWPC0_IP4TE	BIT(4)
#define FWPC0_IP4OE	BIT(5)
#define FWPC0_L2SE	BIT(9)
#define FWPC0_IPDSA	BIT(12)
#define FWPC0_IPHLA	BIT(18)
#define FWPC0_MACSDA	BIT(20)
#define FWPC0_MACSSA	BIT(23)
#define FWPC0_MACHLA	BIT(26)
#define FWPC0_MACHMA	BIT(27)
#define FWPC0_VLANSA	BIT(28)

#define FWPC1(i)                (FWPC10 + (i) * 0x10)
#define FWCP1_LTHFW_MASK	GENMASK(16 + (RSWITCH_NUM_HW - 1), 16)
#define FWPC1_DDE		BIT(0)

#define FWPC2(i)                (FWPC20 + (i) * 0x10)
#define FWCP2_LTWFW_MASK	GENMASK(16 + (RSWITCH_NUM_HW - 1), 16)

#define	FWPBFC(i)		(FWPBFCi + (i) * 0x10)
#define	FWPBFC_PBDV_MASK	GENMASK(RSWITCH_NUM_HW - 1, 0)

#define FWPBFCSDC(j, i)         (FWPBFCSDC00 + (i) * 0x10 + (j) * 0x04)

#define FWMACHEC_MACHMUE_MASK	GENMASK(16 + RSWITCH_MAC_ENTRY_W, 16)

#define FWMACTIM_MACTIOG	BIT(0)
#define FWMACTIM_MACTR		BIT(1)

#define FWIP4SC_IDPTS		BIT(24)
#define FWIP4SC_IIDS		BIT(23)
#define FWIP4SC_IISS		BIT(22)
#define FWIP4SC_ICDS		BIT(21)
#define FWIP4SC_ICPS		BIT(20)
#define FWIP4SC_ICVS		BIT(19)
#define FWIP4SC_ISDS		BIT(18)
#define FWIP4SC_ISPS		BIT(17)
#define FWIP4SC_ISVS		BIT(16)
#define FWIP4SC_IDPTH		BIT(8)
#define FWIP4SC_IIDH		BIT(7)
#define FWIP4SC_IISH		BIT(6)
#define FWIP4SC_ICDH		BIT(5)
#define FWIP4SC_ICPH		BIT(4)
#define FWIP4SC_ICVH		BIT(3)
#define FWIP4SC_ISDH		BIT(2)
#define FWIP4SC_ISPH		BIT(1)
#define FWIP4SC_ISVH		BIT(0)

#define FWLTHHEC_HMUE		GENMASK(16 + RSWITCH_LTH_STREAM_W, 16)

#define FWLTHTL0_ED		BIT(16)
#define FWLTHTL0_SL		BIT(8)

#define FWLTHTL5_MSDUV		BIT(31)
#define FWLTHTL5_MSDUN		GENMASK(16 + RSWITCH_PSFP_MSDU_W - 1, 16)
#define FWLTHTL5_GATEV		BIT(15)
#define FWLTHTL5_GATEN		GENMASK(RSWITCH_PSFP_GATE_W - 1, 0)

#define FWLTHTL6_MTRV		BIT(31)
#define FWLTHTL6_MTRN		GENMASK(16 + RSWITCH_PSFP_MTR_W - 1, 16)
#define FWLTHTL6_FRERV		BIT(15)
#define FWLTHTL6_FRERN		GENMASK(RSWITCH_FRER_RECE_W - 1, 0)

#define FWLTHTL7_SLV		GENMASK(16 + RSWITCH_NUM_HW - 1, 16)
#define FWLTHTL7_RV		BIT(15)
#define FWLTHTL7_RN		GENMASK(RSWITCH_LTH_RRULE_W - 1, 0)

#define FWLTHTL8(i)		(FWLTHTL80 + (i) * 4)
#define FWLTHTL8_CSD		GENMASK(RSWITCH_AXI_CHAIN_W - 1, 0)

#define FWLTHTL9_CME		BIT(21)
#define FWLTHTL9_EME		BIT(20)
#define FWLTHTL9_IPU		BIT(19)
#define FWLTHTL9_IPV		GENMASK(18, 16)
#define FWLTHTL9_DV		GENMASK(RSWITCH_NUM_HW - 1, 0)

#define FWLTHTLR_L		BIT(31)
#define FWLTHTLR_LCN		GENMASK(16 + RSWITCH_LTH_STREAM_W - 1, 16)
#define FWLTHTLR_LO		BIT(3)
#define FWLTHTLR_LEF		BIT(2)
#define FWLTHTLR_LSF		BIT(1)
#define FWLTHTLR_LF		BIT(0)

#define FWLTHTIM_TR		BIT(1)
#define FWLTHTIM_TIOG		BIT(0)

#define FWL23URL0_PV		GENMASK(16 + RSWITCH_NUM_HW - 1, 16)
#define FWL23URL0_RN		GENMASK(7, 0)

#define FWL23URL1_RTU		GENMASK(26, 25)
#define FWL23URL1_SDEIU		BIT(24)
#define FWL23URL1_SPCPU		BIT(23)
#define FWL23URL1_SVIDU		BIT(22)
#define FWL23URL1_CDEIU 	BIT(21)
#define FWL23URL1_CPCPU 	BIT(20)
#define FWL23URL1_CVIDU 	BIT(19)
#define FWL23URL1_MSAU		BIT(18)
#define FWL23URL1_MDAU		BIT(17)
#define FWL23URL1_TTLU		BIT(16)

#define FWL23URL3_SDEI		BIT(31)
#define FWL23URL3_SPCP		GENMASK(30, 28)
#define FWL23URL3_SVID		GENMASK(27, 16)
#define FWL23URL3_CDEI		BIT(15)
#define FWL23URL3_CPCP		GENMASK(14, 12)
#define FWL23URL3_CVID		GENMASK(11, 0)

#define FWL23URLR_L		BIT(31)
#define FWL23URLR_LSF		BIT(1)
#define FWL23URLR_LF		BIT(0)

#define FWL23UTIM_TR		BIT(1)
#define FWL23UTIM_TIOG		BIT(0)

#define RSWITCH_SERDES_LOCAL_OFFSET             0x2600
#define RSWITCH_SERDES_OFFSET                   0x0400
#define RSWITCH_SERDES_BANK_SELECT              0x03fc

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
#define VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL0    0x00c0
#define VR_XS_PMA_MP_12G_16G_25G_TX_STS         0x0100
#define VR_XS_PMA_MP_12G_16G_25G_RX_STS         0x0180

#define BANK_300                                0x0300
#define SR_XS_PCS_CTRL1                         0x0000
#define SR_XS_PCS_STS1                          0x0004
#define SR_XS_PCS_CTRL2                         0x001c

#define BANK_380                                0x0380
#define VR_XS_PCS_DIG_CTRL1                     0x0000
#define VR_XS_PCS_DEBUG_CTRL                    0x0014
#define VR_XS_PCS_KR_CTRL                       0x001c
#define VR_XS_PCS_SFTY_UE_INTRO                 0x03c0
#define VR_XS_PCS_SFTY_DISABLE                  0x03d0

#define BANK_1F00                               0x1f00
#define SR_MII_CTRL                             0x0000

#define BANK_1F80                               0x1f80
#define VR_MII_AN_CTRL                          0x0004

/* Descriptors */
enum RX_DS_CC_BIT {
	RX_DS	= 0x0fff, /* Data size */
	RX_TR	= 0x1000, /* Truncation indication */
	RX_EI	= 0x2000, /* Error indication */
	RX_PS	= 0xc000, /* Padding selection */
};

enum TX_DS_TAGL_BIT {
	TX_DS	= 0x0fff, /* Data size */
	TX_TAGL	= 0xf000, /* Frame tag LSBs */
};

enum DIE_DT {
	/* Frame data */
	DT_FSINGLE	= 0x80,
	DT_FSTART	= 0x90,
	DT_FMID		= 0xA0,
	DT_FEND		= 0xB0,

	/* Chain control */
	DT_LEMPTY	= 0xC0,
	DT_EEMPTY	= 0xD0,
	DT_LINKFIX	= 0x00,
	DT_LINK		= 0xE0,
	DT_EOS		= 0xF0,
	/* HW/SW arbitration */
	DT_FEMPTY	= 0x40,
	DT_FEMPTY_IS	= 0x10,
	DT_FEMPTY_IC	= 0x20,
	DT_FEMPTY_ND	= 0x30,
	DT_FEMPTY_START	= 0x50,
	DT_FEMPTY_MID	= 0x60,
	DT_FEMPTY_END	= 0x70,

	DT_MASK		= 0xF0,
	DIE		= 0x08,	/* Descriptor Interrupt Enable */
};

/* For timestamp descriptor in dptrl (Byte 4 to 7) */
#define TS_DESC_TSUN(dptrl)	((dptrl) & GENMASK(7, 0))
#define TS_DESC_SPN(dptrl)	(((dptrl) & GENMASK(10, 8)) >> 8)
#define TS_DESC_DPN(dptrl)	(((dptrl) & GENMASK(17, 16)) >> 16)
#define TS_DESC_TN(dptrl)	((dptrl) & BIT(24))

struct rswitch_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	__u8  dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
} __packed;

struct rswitch_ts_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	__u8  dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
	__le32 ts_nsec;
	__le32 ts_sec;
} __packed;

struct rswitch_ext_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	__u8  dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
	__le64 info1;
} __packed;

struct rswitch_ext_ts_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	__u8  dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
	__le64 info1;
	__le32 ts_nsec;
	__le32 ts_sec;
} __packed;

#define INFO1_FMT               BIT(2)
#define INFO1_TXC		BIT(3)

#define INFO1_TSUN(val)		((u64)(val) << 8ULL)
#define INFO1_IPV(prio)		((u64)(prio) << 28ULL)
#define INFO1_DV(port_vector)	((u64)(port_vector) << 48ULL)

struct rswitch_etha {
	int index;
	void __iomem *addr;
	void __iomem *serdes_addr;
	struct mii_bus *mii;
	phy_interface_t phy_interface;
	bool needs_stop_workaround;
	enum rswitch_etha_mode mode;
	u32 psmcs;
	u8 mac_addr[MAX_ADDR_LEN];
	bool mac_addr_write_pending;
	int link;
	int speed;
	bool needs_usgmii_serdes_check;
	struct delayed_work usgmii_serdes_check_work;
	int usgmii_serdes_check_retries;
};
#define USGMII_SERDES_CHECK_MAX_RETRIES		5
#define USGMII_SERDES_CHECK_FIRST_DELAY_MS	1500
#define USGMII_SERDES_CHECK_NEXT_DELAY_MS	500

struct rswitch_gwca_chain {
	union {
		struct rswitch_ext_desc *tx_ring;
		struct rswitch_ext_ts_desc *rx_ring;
		struct rswitch_ts_desc *ts_ring;
	};

	/* Common */
	dma_addr_t ring_dma;
	u32 num_ring;
	u32 cur;
	u32 dirty;

	/* For [rt]x_ring */
	int index;
	bool dir_tx;
	struct sk_buff **skb;
	struct net_device *ndev;	/* chain to ndev for irq */

	/* For RX multi-descriptor handling */
	void **rx_bufs;
	bool multi_desc;
	u16 total_len;
	struct sk_buff *skb_multi;
};

struct rswitch_gwca_ts_info {
	struct sk_buff *skb;
	struct list_head list;

	int port;
	u8 tag;
};

#define RSWITCH_NUM_IRQ_REGS	(RSWITCH_MAX_NUM_CHAINS / BITS_PER_TYPE(u32))
struct rswitch_gwca {
	int index;
	struct rswitch_gwca_chain *chains;
	int num_chains;
	struct rswitch_gwca_chain ts_queue;
	struct list_head ts_info_list;
	DECLARE_BITMAP(used, RSWITCH_MAX_NUM_CHAINS);
	u32 tx_irq_bits[RSWITCH_NUM_IRQ_REGS];
	u32 rx_irq_bits[RSWITCH_NUM_IRQ_REGS];
};

#define NUM_CHAINS_PER_NDEV	2
struct rswitch_device {
	struct rswitch_private *priv;
	struct list_head list;

	struct device_node *port_dn;
	unsigned int port;

	struct device_node *phy_dn;
	bool phy_dn_is_fixed_link;

	struct net_device *ndev;
	struct napi_struct napi;
	struct rswitch_gwca_chain *tx_chain;
	struct rswitch_gwca_chain *rx_chain;
	spinlock_t lock;
	u8 ts_tag;

	struct net_device *brdev;	/* master bridge device */
	unsigned int learning_requested : 1;
	unsigned int learning_offloaded : 1;
	unsigned int forwarding_requested : 1;
	unsigned int forwarding_offloaded : 1;

	struct list_head ipv4_local_route_list;
	struct list_head ipv4_bcast_route_list;
	struct list_head ipv4_ucast_route_list;

	unsigned int l3_offload_enabled : 1;
};

struct rswitch_mfwd_mac_table_entry {
	int chain_index;
	unsigned char addr[MAX_ADDR_LEN];
};

struct rswitch_mfwd {
	struct rswitch_mac_table_entry *mac_table_entries;
	int num_mac_table_entries;
};

struct rswitch_private {
	struct platform_device *pdev;
	void __iomem *addr;
	void __iomem *serdes_addr;
	struct rtsn_ptp_private *ptp_priv;
	struct rswitch_desc *desc_bat;
	dma_addr_t desc_bat_dma;
	u32 desc_bat_size;

	struct list_head port_list;

	struct rswitch_gwca gwca;
	struct rswitch_etha etha[RSWITCH_MAX_NUM_ETHA];
	struct rswitch_mfwd mfwd;

	struct clk *rsw_clk;
	struct clk *phy_clk;

	struct reset_control *sd_rst;

	u8 chan_running;
	bool serdes_common_init;

	spinlock_t lock;	/* lock interrupt registers' control */
	struct clk *clk;

	struct net_device *offload_brdev;

	spinlock_t l3_lock;
	DECLARE_BITMAP(l23_update_bitmap, RSWITCH_MAX_NUM_RRULE);
	struct list_head l23_update_list;

	struct notifier_block fib_nb;
	bool fib_nb_registered;
};

struct rswitch_l23update_spec {
	u8 dst_mac[ETH_ALEN];
};

struct rswitch_l23update {
	struct list_head list;
	unsigned int use_count;
	unsigned int index;
	struct rswitch_l23update_spec spec;
};

struct rswitch_ipv4_route {
	struct list_head list;
	__be32 addr;
	__be32 mask;
	__be32 gw_addr;
	struct list_head offload_list;
};

struct rswitch_ipv4_route_exception {
	struct list_head list;
	__be32 addr;
};

/* Hardware frame type identifieers (bits 130:128 of stream id) */
#define RSWITCH_FRAME_TYPE_IPV4_TCP	3
#define RSWITCH_FRAME_TYPE_IPV4_UDP	2
#define RSWITCH_FRAME_TYPE_IPV4_OTHER	1

struct rswitch_ipv4_dst_offload {
	struct list_head list;
	struct rswitch_l23update *update;
	__be32 addr;
	u8 types_offloaded;	/* bits for RSWITCH_FRAME_TYPE_* */
};

#define rswitch_for_all_ports(_priv, _rdev) \
	list_for_each_entry(_rdev, &_priv->port_list, list)

#define rswitch_undo_for_prev_ports(_priv, _rdev) \
	list_for_each_entry_continue_reverse(_rdev, &_priv->port_list, list)

static void rswitch_update_l2_offload(struct rswitch_private *priv);
static void rswitch_update_l3_offload(struct rswitch_private *priv);
static void rswitch_handle_l3_learning(struct rswitch_device *rdev,
		struct sk_buff *skb);
static void rdev_cleanup_ipv4_offloads_all(struct rswitch_device *rdev);


static int num_etha_ports = 3;
module_param(num_etha_ports, int, 0644);
MODULE_PARM_DESC(num_etha_ports, "Number of using ETHA ports");

static bool parallel_mode;
module_param(parallel_mode, bool, 0644);
MODULE_PARM_DESC(parallel_mode, "Operate simultaneously with Realtime core");

#define RSWITCH_TIMEOUT_MS	1000
static int rswitch_reg_wait(void __iomem *addr, u32 offs, u32 mask, u32 expected)
{
	int i;

	for (i = 0; i < RSWITCH_TIMEOUT_MS; i++) {
		if ((rs_read32(addr + offs) & mask) == expected)
			return 0;

		mdelay(1);
	}

	return -ETIMEDOUT;
}

static u32 rswitch_etha_offs(int index)
{
	return RSWITCH_ETHA_OFFSET + index * RSWITCH_ETHA_SIZE;
}

static u32 rswitch_etha_read(struct rswitch_etha *etha, enum rswitch_reg reg)
{
	return rs_read32(etha->addr + reg);
}

static void rswitch_etha_write(struct rswitch_etha *etha, u32 data, enum rswitch_reg reg)
{
	rs_write32(data, etha->addr + reg);
}

static void rswitch_etha_modify(struct rswitch_etha *etha, enum rswitch_reg reg,
				u32 clear, u32 set)
{
	rswitch_etha_write(etha, (rswitch_etha_read(etha, reg) & ~clear) | set, reg);
}

static void rswitch_modify(void __iomem *addr, enum rswitch_reg reg, u32 clear, u32 set)
{
	rs_write32((rs_read32(addr + reg) & ~clear) | set, addr + reg);
}

static bool __maybe_unused rswitch_is_any_data_irq(struct rswitch_private *priv, u32 *dis, bool tx)
{
	int i;
	u32 *mask = tx ? priv->gwca.tx_irq_bits : priv->gwca.rx_irq_bits;

	for (i = 0; i < RSWITCH_NUM_IRQ_REGS; i++) {
		if (dis[i] & mask[i])
			return true;
	}

	return false;
}

static void rswitch_get_data_irq_status(struct rswitch_private *priv, u32 *dis)
{
	int i;

	for (i = 0; i < RSWITCH_NUM_IRQ_REGS; i++)
		dis[i] = rs_read32(priv->addr + GWDIS0 + i * 0x10);
}

static void rswitch_enadis_data_irq(struct rswitch_private *priv, int index, bool enable)
{
	u32 offs = (enable ? GWDIE0 : GWDID0) + (index / 32) * 0x10;
	u32 tmp = 0;

	/* For VPF? */
	if (enable)
		tmp = rs_read32(priv->addr + offs);

	rs_write32(BIT(index % 32) | tmp, priv->addr + offs);
}

static void rswitch_ack_data_irq(struct rswitch_private *priv, int index)
{
	u32 offs = GWDIS0 + (index / 32) * 0x10;

	rs_write32(BIT(index % 32), priv->addr + offs);
}

static bool rswitch_is_chain_rxed(struct rswitch_gwca_chain *c, u8 unexpected)
{
	int entry;
	struct rswitch_ext_ts_desc *desc;

	entry = c->dirty % c->num_ring;
	desc = &c->rx_ring[entry];

	if ((desc->die_dt & DT_MASK) != unexpected)
		return true;

	return false;
}


static bool rswitch_rx(struct net_device *ndev, int *quota)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_gwca_chain *c = rdev->rx_chain;
	int boguscnt = c->dirty + c->num_ring - c->cur;
	int entry = c->cur % c->num_ring;
	struct rswitch_ext_ts_desc *desc = &c->rx_ring[entry];
	int limit;
	u16 pkt_len;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	u32 get_ts;

	boguscnt = min(boguscnt, *quota);
	limit = boguscnt;

	while ((desc->die_dt & DT_MASK) != DT_FEMPTY) {
		dma_rmb();
		pkt_len = le16_to_cpu(desc->info_ds) & RX_DS;
		if (--boguscnt < 0)
			break;
		dma_addr = le32_to_cpu(desc->dptrl) | ((__le64)le32_to_cpu(desc->dptrh) << 32);
		dma_unmap_single(ndev->dev.parent, dma_addr,
				 RSWITCH_RX_BUF_SIZE - NET_SKB_PAD - NET_IP_ALIGN,
				 DMA_FROM_DEVICE);

		if ((desc->die_dt & DT_MASK) == DT_FSTART) {
			if (c->multi_desc) {
				/* Got the error so freed the multi-descriptor skb */
				dev_kfree_skb_any(c->skb_multi);
			}
			c->skb_multi = build_skb(c->rx_bufs[entry], RSWITCH_RX_BUF_SIZE);
			if (!c->skb_multi) {
				c->multi_desc = false;
				goto next;
			}
			skb_checksum_none_assert(c->skb_multi);
			skb_reserve(c->skb_multi, NET_SKB_PAD + NET_IP_ALIGN);
			skb_put(c->skb_multi, pkt_len);
			c->multi_desc = true;
			c->total_len = pkt_len;

			goto next;
		} else if ((desc->die_dt & DT_MASK) == DT_FMID) {
			if (!c->multi_desc)
				goto next;

			skb_add_rx_frag(c->skb_multi,
					skb_shinfo(c->skb_multi)->nr_frags,
					virt_to_page(c->rx_bufs[entry]),
					offset_in_page(c->rx_bufs[entry]) + NET_SKB_PAD + NET_IP_ALIGN,
					pkt_len, RSWITCH_RX_BUF_SIZE);
			c->total_len += pkt_len;
			goto next;
		} else if ((desc->die_dt & DT_MASK) == DT_FEND) {
			if (!c->multi_desc)
				goto next;

			skb_add_rx_frag(c->skb_multi,
					skb_shinfo(c->skb_multi)->nr_frags,
					virt_to_page(c->rx_bufs[entry]),
					offset_in_page(c->rx_bufs[entry]) + NET_SKB_PAD + NET_IP_ALIGN,
					pkt_len, RSWITCH_RX_BUF_SIZE);

			skb = c->skb_multi;
			pkt_len += c->total_len;
			c->skb_multi = NULL;
			c->multi_desc = false;
		}  else {
			/* F_SINGLE */
			if (c->multi_desc) {
				/* Got the error so freed the multi-descriptor skb */
				dev_kfree_skb_any(c->skb_multi);
				c->skb_multi = NULL;
				c->multi_desc = false;
			}
			skb = build_skb(c->rx_bufs[entry], RSWITCH_RX_BUF_SIZE);
			if (!skb)
				goto next;
			skb_checksum_none_assert(skb);
			skb_reserve(skb, NET_SKB_PAD + NET_IP_ALIGN);
			skb_put(skb, pkt_len);
		}

		get_ts = rdev->priv->ptp_priv->tstamp_rx_ctrl & RTSN_RXTSTAMP_TYPE_V2_L2_EVENT;
		if (get_ts) {
			struct skb_shared_hwtstamps *shhwtstamps;
			struct timespec64 ts;

			shhwtstamps = skb_hwtstamps(skb);
			memset(shhwtstamps, 0, sizeof(*shhwtstamps));
			ts.tv_sec = (u64)le32_to_cpu(desc->ts_sec);
			ts.tv_nsec = le32_to_cpu(desc->ts_nsec & 0x3FFFFFFF);
			shhwtstamps->hwtstamp = timespec64_to_ktime(ts);
		}
		skb->protocol = eth_type_trans(skb, ndev);
		napi_gro_receive(&rdev->napi, skb);
		rdev->ndev->stats.rx_packets++;
		rdev->ndev->stats.rx_bytes += pkt_len;

next:
		c->rx_bufs[entry] = NULL;
		entry = (++c->cur) % c->num_ring;
		desc = &c->rx_ring[entry];
	}

	/* Refill the RX ring buffers */
	for (; c->cur - c->dirty > 0; c->dirty++) {
		entry = c->dirty % c->num_ring;
		desc = &c->rx_ring[entry];
		desc->info_ds = cpu_to_le16(MAX_DESC_SZ);

		if (!c->rx_bufs[entry]) {
			c->rx_bufs[entry] = netdev_alloc_frag(RSWITCH_RX_BUF_SIZE);
			if (!c->rx_bufs[entry])
				break;	/* Better luch next round */

			dma_addr = dma_map_single(ndev->dev.parent,
					c->rx_bufs[entry] + NET_SKB_PAD + NET_IP_ALIGN,
					RSWITCH_RX_BUF_SIZE - NET_SKB_PAD - NET_IP_ALIGN,
					DMA_FROM_DEVICE);

			if (dma_mapping_error(ndev->dev.parent, dma_addr))
				desc->info_ds = cpu_to_le16(0);
			desc->dptrl = cpu_to_le32(lower_32_bits(dma_addr));
			desc->dptrh = cpu_to_le32(upper_32_bits(dma_addr));
		}
		dma_wmb();
		desc->die_dt = DT_FEMPTY | DIE;
	}

	*quota -= limit - (++boguscnt);

	return boguscnt <= 0;
}

static int rswitch_tx_free(struct net_device *ndev, bool free_txed_only)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_ext_desc *desc;
	int free_num = 0;
	int entry, size;
	dma_addr_t dma_addr;
	struct rswitch_gwca_chain *c = rdev->tx_chain;
	struct sk_buff *skb;

	for (; c->cur - c->dirty > 0; c->dirty++) {
		entry = c->dirty % c->num_ring;
		desc = &c->tx_ring[entry];
		if (free_txed_only && (desc->die_dt & DT_MASK) != DT_FEMPTY)
			break;

		dma_rmb();
		size = le16_to_cpu(desc->info_ds) & TX_DS;
		skb = c->skb[entry];
		if (skb) {
			dma_addr = le32_to_cpu(desc->dptrl) |
				   ((__le64)le32_to_cpu(desc->dptrh) << 32);
			dma_unmap_single(ndev->dev.parent, dma_addr,
					size, DMA_TO_DEVICE);
			dev_kfree_skb_any(c->skb[entry]);
			c->skb[entry] = NULL;
			rdev->ndev->stats.tx_packets++;
			free_num++;
		}
		desc->die_dt = DT_EEMPTY;
		rdev->ndev->stats.tx_bytes += size;
	}

	return free_num;
}

static int rswitch_poll(struct napi_struct *napi, int budget)
{
	struct net_device *ndev = napi->dev;
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_private *priv = rdev->priv;
	unsigned long flags;
	int quota = budget;

retry:
	rswitch_tx_free(ndev, true);

	if (rswitch_rx(ndev, &quota))
		goto out;
	else if (rswitch_is_chain_rxed(rdev->rx_chain, DT_FEMPTY))
		goto retry;

	netif_wake_subqueue(ndev, 0);

	if (napi_complete_done(napi, budget - quota)) {
		spin_lock_irqsave(&priv->lock, flags);
		/* Re-enable RX/TX interrupts */
		rswitch_enadis_data_irq(priv, rdev->tx_chain->index, true);
		rswitch_enadis_data_irq(priv, rdev->rx_chain->index, true);
		spin_unlock_irqrestore(&priv->lock, flags);
	}

	__iowmb();

out:
	return budget - quota;
}

static bool rswitch_agent_clock_is_enabled(void __iomem *base_addr, int port)
{
	u32 val = rs_read32(base_addr + RCEC);

	if (val & RCEC_RCE)
		return (val & BIT(port)) ? true : false;
	else
		return false;
}

static void rswitch_agent_clock_ctrl(void __iomem *base_addr, int port, int enable)
{
	u32 val;

	if (enable) {
		val = rs_read32(base_addr + RCEC);
		rs_write32(val | RCEC_RCE | BIT(port), base_addr + RCEC);
	} else {
		val = rs_read32(base_addr + RCDC);
		rs_write32(val | BIT(port), base_addr + RCDC);
	}
}

static int rswitch_etha_change_mode(struct rswitch_etha *etha,
				    enum rswitch_etha_mode mode)
{
	void __iomem *base_addr;
	int ret;

	base_addr = etha->addr - rswitch_etha_offs(etha->index);

	/* Enable clock */
	if (!rswitch_agent_clock_is_enabled(base_addr, etha->index))
		rswitch_agent_clock_ctrl(base_addr, etha->index, 1);

	rs_write32(mode, etha->addr + EAMC);

	ret = rswitch_reg_wait(etha->addr, EAMS, EAMS_OPS_MASK, mode);
	if (ret == 0)
		etha->mode = mode;

	/* Do not disable clock when entering DISABLED mode, otherwise
	 * MII stops working */

	return ret;
}

static void rswitch_etha_read_mac_address(struct rswitch_etha *etha)
{
	u8 *mac = &etha->mac_addr[0];
	u32 mrmac0 = rswitch_etha_read(etha, MRMAC0);
	u32 mrmac1 = rswitch_etha_read(etha, MRMAC1);

	mac[0] = (mrmac0 >>  8) & 0xFF;
	mac[1] = (mrmac0 >>  0) & 0xFF;
	mac[2] = (mrmac1 >> 24) & 0xFF;
	mac[3] = (mrmac1 >> 16) & 0xFF;
	mac[4] = (mrmac1 >>  8) & 0xFF;
	mac[5] = (mrmac1 >>  0) & 0xFF;
}

static void rswitch_etha_write_mac_address(struct rswitch_etha *etha)
{
	u8 *mac = &etha->mac_addr[0];

        iowrite32((mac[0] << 8) | mac[1], etha->addr + MRMAC0);
        iowrite32((mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5],
                  etha->addr + MRMAC1);
}

static bool rswitch_etha_wait_link_verification(struct rswitch_etha *etha)
{
	/* Request Link Verification */
	rswitch_etha_write(etha, MLVC_PLV, MLVC);
	return rswitch_reg_wait(etha->addr, MLVC, MLVC_PLV, 0);
}

static void rswitch_etha_pis_lsc_setting(struct rswitch_etha *etha)
{
	u32 pis, lsc;

	switch (etha->phy_interface) {
	case PHY_INTERFACE_MODE_SGMII:
		pis = MPIC_PIS_GMII;
		break;
	case PHY_INTERFACE_MODE_USXGMII:
	case PHY_INTERFACE_MODE_5GBASER:
		pis = MPIC_PIS_XGMII;
		break;
	default:
		pis = FIELD_GET(MPIC_PIS_MASK, rswitch_etha_read(etha, MPIC));
		break;
	}

	switch (etha->speed) {
	case 100:
		lsc = MPIC_LSC_100M;
		break;
	case 1000:
		lsc = MPIC_LSC_1G;
		break;
	case 2500:
		lsc = MPIC_LSC_2_5G;
		break;
	default:
		lsc = FIELD_GET(MPIC_LSC_MASK, rswitch_etha_read(etha, MPIC));
		break;
	}

	rswitch_etha_modify(etha, MPIC, MPIC_PIS_MASK | MPIC_LSC_MASK,
			    FIELD_PREP(MPIC_PIS_MASK, pis) |
			    FIELD_PREP(MPIC_LSC_MASK, lsc));
}

static void rswitch_etha_enable_mii(struct rswitch_etha *etha)
{
	rswitch_etha_modify(etha, MPIC, MPIC_PSMCS_MASK | MPIC_PSMHT_MASK,
			    FIELD_PREP(MPIC_PSMCS_MASK, etha->psmcs) |
			    FIELD_PREP(MPIC_PSMHT_MASK, 0x06));
}

static int rswitch_etha_hw_init(struct rswitch_device *rdev)
{
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];
	int err;

	/* Change to CONFIG Mode */
	err = rswitch_etha_change_mode(etha, EAMC_OPC_DISABLE);
	if (err < 0)
		return err;
	err = rswitch_etha_change_mode(etha, EAMC_OPC_CONFIG);
	if (err < 0)
		return err;

	rs_write32(EAVCC_VEM_SC_TAG, etha->addr + EAVCC);
	rswitch_etha_enable_mii(etha);

	return rswitch_etha_change_mode(etha, EAMC_OPC_DISABLE);
}

void rswitch_serdes_write32(void __iomem *addr, u32 offs,  u32 bank, u32 data)
{
	iowrite32(bank, addr + RSWITCH_SERDES_BANK_SELECT);
	iowrite32(data, addr + offs);
}

u32 rswitch_serdes_read32(void __iomem *addr, u32 offs,  u32 bank)
{
	iowrite32(bank, addr + RSWITCH_SERDES_BANK_SELECT);
	return ioread32(addr + offs);
}

static int rswitch_serdes_reg_wait(void __iomem *addr, u32 offs, u32 bank, u32 mask, u32 expected)
{
	int i;

	iowrite32(bank, addr + RSWITCH_SERDES_BANK_SELECT);
	mdelay(1);

	for (i = 0; i < RSWITCH_TIMEOUT_MS; i++) {
		if ((ioread32(addr + offs) & mask) == expected)
			return 0;
		mdelay(1);
	}

	return -ETIMEDOUT;
}

static int rswitch_serdes_common_init_ram(void __iomem *common_addr)
{
	int ret, i;

	for (i = 0; i < RSWITCH_SERDES_NUM; i++) {
		ret = rswitch_serdes_reg_wait(common_addr + i * RSWITCH_SERDES_OFFSET,
					      VR_XS_PMA_MP_12G_16G_25G_SRAM, BANK_180,
					      BIT(0), 0x01);
		if (ret)
			return ret;
	}

	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_16G_25G_SRAM, BANK_180, 0x03);

	return 0;
}

static void rswitch_serdes_common_setting(void __iomem *common_addr)
{
	/* Set combination mode */
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_16G_25G_REF_CLK_CTRL, BANK_180, 0xd7);
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_10G_MPLLA_CTRL2, BANK_180, 0xc200);
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_16G_MPLLA_CTRL0, BANK_180, 0x42);
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_MPLLA_CTRL1, BANK_180, 0);
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_MPLLA_CTRL3, BANK_180, 0x2f);
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_16G_MPLLB_CTRL0, BANK_180, 0x60);
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_16G_MPLLB_CTRL2, BANK_180, 0x2200);
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_MPLLB_CTRL1, BANK_180, 0);
	rswitch_serdes_write32(common_addr, VR_XS_PMA_MP_12G_MPLLB_CTRL3, BANK_180, 0x3d);
}

static int rswitch_serdes_chan_setting(void __iomem *addr, phy_interface_t phy_interface)
{
	int ret;

	switch (phy_interface) {
	case PHY_INTERFACE_MODE_SGMII:
		rswitch_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2000);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_MPLL_CMN_CTRL,
				       BANK_180, 0x11);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_VCO_CAL_LD0, BANK_180, 0x540);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_VCO_CAL_REF0, BANK_180, 0x15);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, 0x100);
		rswitch_serdes_write32(addr, VR_XS_PMA_CONSUMER_10G_RX_GENCTRL4, BANK_180, 0);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_RATE_CTRL, BANK_180, 0x02);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_RX_RATE_CTRL, BANK_180, 0x03);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, 0x100);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, 0x100);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_AFE_DFE_EN_CTRL, BANK_180, 0);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_RX_EQ_CTRL0, BANK_180, 0x07);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_10G_RX_IQ_CTRL0, BANK_180, 0);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1, BANK_180, 0x310);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, 0x101);
		ret = rswitch_serdes_reg_wait(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2,
					      BANK_180, BIT(0), 0);
		if (ret)
			return ret;

		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, 0x101);
		ret = rswitch_serdes_reg_wait(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2,
					      BANK_180, BIT(0), 0);
		if (ret)
			return ret;

		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1,
				       BANK_180, 0x1310);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL0,
				       BANK_180, 0x1800);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL1, BANK_180, 0);
		rswitch_serdes_write32(addr, SR_XS_PCS_CTRL2, BANK_300, 0x01);
		rswitch_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2100);
		ret = rswitch_serdes_reg_wait(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, BIT(8), 0);
		if (ret)
			return ret;

		break;
	case PHY_INTERFACE_MODE_USXGMII:
	case PHY_INTERFACE_MODE_5GBASER:
		rswitch_serdes_write32(addr, SR_XS_PCS_CTRL2, BANK_300, 0x0);
		rswitch_serdes_write32(addr, VR_XS_PCS_DEBUG_CTRL, BANK_380, 0x50);
		rswitch_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2200);
		rswitch_serdes_write32(addr, VR_XS_PCS_KR_CTRL, BANK_380, 0x400);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_MPLL_CMN_CTRL, BANK_180, 0x1);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_VCO_CAL_LD0, BANK_180, 0x56a);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_VCO_CAL_REF0, BANK_180, 0x15);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1,
				       BANK_180, 0x1100);
		rswitch_serdes_write32(addr, VR_XS_PMA_CONSUMER_10G_RX_GENCTRL4, BANK_180, 1);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_RATE_CTRL, BANK_180, 0x01);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_RX_RATE_CTRL, BANK_180, 0x01);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, 0x300);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, 0x300);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_AFE_DFE_EN_CTRL, BANK_180, 0);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_RX_EQ_CTRL0, BANK_180, 0x4);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_10G_RX_IQ_CTRL0, BANK_180, 0);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1, BANK_180, 0x310);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2, BANK_180, 0x0301);
		ret = rswitch_serdes_reg_wait(addr, VR_XS_PMA_MP_12G_16G_TX_GENCTRL2,
					      BANK_180, BIT(0), 0);
		if (ret)
			return ret;
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2, BANK_180, 0x301);
		ret = rswitch_serdes_reg_wait(addr, VR_XS_PMA_MP_12G_16G_RX_GENCTRL2,
					      BANK_180, BIT(0), 0);
		if (ret)
			return ret;
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL1,
				       BANK_180, 0x1310);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL0,
				       BANK_180, 0x1800);
		rswitch_serdes_write32(addr, VR_XS_PMA_MP_12G_16G_25G_TX_EQ_CTRL1, BANK_180, 0);
		rswitch_serdes_write32(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x2300);
		ret = rswitch_serdes_reg_wait(addr, VR_XS_PCS_DIG_CTRL1, BANK_380, BIT(8), 0);
		if (ret)
			return ret;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int rswitch_serdes_set_chan_speed(void __iomem *addr, phy_interface_t phy_interface, int speed)
{
	switch (phy_interface) {
	case PHY_INTERFACE_MODE_SGMII:
		if (speed == 1000)
			rswitch_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x140);
		else if (speed == 100)
			rswitch_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x2100);

		break;
	case PHY_INTERFACE_MODE_USXGMII:
		/* USXGMII - 2.5Gbps */
		rswitch_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x120);

		break;
	case PHY_INTERFACE_MODE_5GBASER:
		rswitch_serdes_write32(addr, SR_MII_CTRL, BANK_1F00, 0x2120);

		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int rswitch_serdes_monitor_linkup(struct rswitch_etha *etha)
{
	int ret, retry = 5;
	u32 val;

retry:
	ret = rswitch_serdes_reg_wait(etha->serdes_addr,
				      SR_XS_PCS_STS1, BANK_300, BIT(2), BIT(2));
	if (ret) {
		pr_debug("\n%s: SerDes Link up failed, restart linkup\n", __func__);

		if (retry < 0)
			return -ETIMEDOUT;

		retry--;

		val = rswitch_serdes_read32(etha->serdes_addr,
					    VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180);
		rswitch_serdes_write32(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1,
				       BANK_180, val | BIT(4));
		udelay(20);
		rswitch_serdes_write32(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1,
				       BANK_180, val & (~BIT(4)));

		goto retry;
	}

	return 0;
}

static int rswitch_serdes_common_init(struct rswitch_etha *etha)
{
	void __iomem *common_addr = etha->serdes_addr - etha->index * RSWITCH_SERDES_OFFSET;
	int ret, i;

	/* Initialize SRAM */
	ret = rswitch_serdes_common_init_ram(common_addr);
	if (ret)
		return ret;

	for (i = 0; i < RSWITCH_SERDES_NUM; i++) {
		ret = rswitch_serdes_reg_wait(common_addr + i * RSWITCH_SERDES_OFFSET,
					      SR_XS_PCS_CTRL1, BANK_300, BIT(15), 0);
		if (ret)
			return ret;
	}

	for (i = 0; i < RSWITCH_SERDES_NUM; i++)
		rswitch_serdes_write32(common_addr + i * RSWITCH_SERDES_OFFSET,
				       0x03d4, BANK_380, 0x443);

	/* Set common setting */
	rswitch_serdes_common_setting(common_addr);

	for (i = 0; i < RSWITCH_SERDES_NUM; i++)
		rswitch_serdes_write32(common_addr + i * RSWITCH_SERDES_OFFSET,
				       VR_XS_PCS_SFTY_DISABLE, BANK_380, 0x01);

	/* Assert softreset for PHY */
	rswitch_serdes_write32(common_addr, VR_XS_PCS_DIG_CTRL1, BANK_380, 0x8000);

	/* Initialize SRAM */
	ret = rswitch_serdes_common_init_ram(common_addr);
	if (ret)
		return ret;

	return rswitch_serdes_reg_wait(common_addr, VR_XS_PCS_DIG_CTRL1, BANK_380, BIT(15), 0);
}

static int rswitch_serdes_chan_init(struct rswitch_etha *etha, bool etha_stop_workaround)
{
	phy_interface_t phy_interface;
	int speed;
	int ret;
	u32 val;

	if (etha_stop_workaround) {
		phy_interface = PHY_INTERFACE_MODE_USXGMII;
		speed = 2500;
	} else {
		phy_interface = etha->phy_interface;
		speed = etha->speed;
	}

	/* Set channel settings*/
	ret = rswitch_serdes_chan_setting(etha->serdes_addr, phy_interface);
	if (ret)
		return ret;

	/* Set speed (bps) */
	ret = rswitch_serdes_set_chan_speed(etha->serdes_addr, phy_interface, speed);
	if (ret)
		return ret;

	rswitch_serdes_write32(etha->serdes_addr, VR_XS_PCS_SFTY_UE_INTRO, BANK_380, 0);
	rswitch_serdes_write32(etha->serdes_addr, VR_XS_PCS_SFTY_DISABLE, BANK_380, 0);

	val = rswitch_serdes_read32(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL0,
				    BANK_180);
	rswitch_serdes_write32(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL0,
			       BANK_180, val | BIT(8));

	ret = rswitch_serdes_reg_wait(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_TX_STS,
				      BANK_180, BIT(0), 1);
	if (ret)
		return ret;

	rswitch_serdes_write32(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_TX_GENCTRL0,
			       BANK_180, val &= ~BIT(8));

	ret = rswitch_serdes_reg_wait(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_TX_STS,
				      BANK_180, BIT(0), 0);
	if (ret)
		return ret;

	val = rswitch_serdes_read32(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1,
				    BANK_180);
	rswitch_serdes_write32(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1,
			       BANK_180, val | BIT(4));

	ret = rswitch_serdes_reg_wait(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_RX_STS,
				      BANK_180, BIT(0), 1);
	if (ret)
		return ret;

	rswitch_serdes_write32(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1,
			       BANK_180, val &= ~BIT(4));

	ret = rswitch_serdes_reg_wait(etha->serdes_addr, VR_XS_PMA_MP_12G_16G_25G_RX_STS,
				      BANK_180, BIT(0), 0);
	if (ret)
		return ret;

	/* Check Link up restart */
	return etha_stop_workaround ? 0 : rswitch_serdes_monitor_linkup(etha);
}

static void rswitch_usgmii_serdes_check(struct work_struct *work)
{
	struct rswitch_etha *etha = container_of(to_delayed_work(work),
			typeof(*etha), usgmii_serdes_check_work);
	u32 val;

	/* Note on concurrency: other serdes access can only happen from
	 * within rswitch_adjust_link, that explicitly cancels this work */

	/* Link status is latched, for current status read twice */
	rswitch_serdes_read32(etha->serdes_addr, SR_XS_PCS_STS1, BANK_300);
	val = rswitch_serdes_read32(etha->serdes_addr, SR_XS_PCS_STS1, BANK_300);
	if (val & 0x4) {
		if (etha->usgmii_serdes_check_retries > 0)
			printk(KERN_INFO "TSN%d: USGMII serdes ok\n",
					etha->index);
		return;
	}

	if (etha->usgmii_serdes_check_retries >= USGMII_SERDES_CHECK_MAX_RETRIES) {
		printk(KERN_ERR "TSN%d USGMII serdes still not ok, aborting\n",
				etha->index);
		return;
	}

	printk(KERN_ERR "TSN%d USGMII serdes not ok, retrying\n", etha->index);

	val = rswitch_serdes_read32(etha->serdes_addr,
		VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180);
	rswitch_serdes_write32(etha->serdes_addr,
		VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, val | 0x10);
	udelay(10);
	rswitch_serdes_write32(etha->serdes_addr,
		VR_XS_PMA_MP_12G_16G_25G_RX_GENCTRL1, BANK_180, val);

	etha->usgmii_serdes_check_retries++;
	schedule_delayed_work(&etha->usgmii_serdes_check_work,
			msecs_to_jiffies(USGMII_SERDES_CHECK_NEXT_DELAY_MS));
}

static int rswitch_etha_set_access_c45(struct rswitch_etha *etha, bool read,
				      int phyad, int devad, int regad, int data)
{
	int pop = read ? MDIO_READ_C45 : MDIO_WRITE_C45;
	u32 val;
	int ret;

	/* No match device */
	if (devad == 0xffffffff)
		return 0;

	/* Clear completion flags */
	writel(MMIS1_CLEAR_FLAGS, etha->addr + MMIS1);

	/* Submit address to PHY (MDIO_ADDR_C45 << 13) */
	val = MPSM_PSME | MPSM_MFF_C45;
	rs_write32((regad << 16) | (devad << 8) | (phyad << 3) | val, etha->addr + MPSM);

	ret = rswitch_reg_wait(etha->addr, MMIS1, MMIS1_PAACS, MMIS1_PAACS);
	if (ret)
		return ret;

	/* Clear address completion flag */
	rswitch_etha_modify(etha, MMIS1, MMIS1_PAACS, MMIS1_PAACS);

	/* Read/Write PHY register */
	if (read) {
		writel((pop << 13) | (devad << 8) | (phyad << 3) | val, etha->addr + MPSM);

		ret = rswitch_reg_wait(etha->addr, MMIS1, MMIS1_PRACS, MMIS1_PRACS);
		if (ret)
			return ret;

		/* Read data */
		ret = (rs_read32(etha->addr + MPSM) & MPSM_PRD_MASK) >> 16;

		/* Clear read completion flag */
		rswitch_etha_modify(etha, MMIS1, MMIS1_PRACS, MMIS1_PRACS);
	} else {
		rs_write32((data << 16) | (pop << 13) | (devad << 8) | (phyad << 3) | val,
			   etha->addr + MPSM);

		ret = rswitch_reg_wait(etha->addr, MMIS1, MMIS1_PWACS, MMIS1_PWACS);
	}

	return ret;
}

static int rswitch_etha_set_access_c22(struct rswitch_etha *etha, bool read,
				       int phyad, int regad, int data)
{
	int pop = read ? MDIO_READ_C22 : MDIO_WRITE_C22;
	int ret;
	u32 val;

	val = MPSM_POP(pop) | MPSM_PDA(phyad) | MPSM_PRA(regad) | MPSM_PSME;

	if (!read)
		val |= MPSM_PRD_WRITE(data);

	rs_write32(val, etha->addr + MPSM);

	ret = rswitch_reg_wait(etha->addr, MPSM, MPSM_PSME, 0);
	if (ret)
		return ret;

	return read ? MPSM_PRD_READ(rswitch_etha_read(etha, MPSM)) : 0;
}

static int rswitch_etha_mii_read(struct mii_bus *bus, int addr, int regnum)
{
	struct rswitch_etha *etha = bus->priv;
	int mode, devad, regad;

	mode = regnum & MII_ADDR_C45;

	/* Clause 22 */
	if (!mode)
		return rswitch_etha_set_access_c22(etha, true, addr, regnum, 0);

	devad = (regnum >> MII_DEVADDR_C45_SHIFT) & 0x1f;
	regad = regnum & MII_REGADDR_C45_MASK;

	return rswitch_etha_set_access_c45(etha, true, addr, devad, regad, 0);
}

static int rswitch_etha_mii_write(struct mii_bus *bus, int addr, int regnum, u16 val)
{
	struct rswitch_etha *etha = bus->priv;
	int mode, devad, regad;

	mode = regnum & MII_ADDR_C45;

	/* Clause 22 */
	if (!mode)
		return rswitch_etha_set_access_c22(etha, false, addr, regnum, val);

	devad = (regnum >> MII_DEVADDR_C45_SHIFT) & 0x1f;
	regad = regnum & MII_REGADDR_C45_MASK;

	return rswitch_etha_set_access_c45(etha, false, addr, devad, regad, val);
}

static int rswitch_etha_mii_reset(struct mii_bus *bus)
{
	/* TODO */
	return 0;
}

static int rswitch_mii_register(struct rswitch_device *rdev)
{
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];
	struct mii_bus *mii_bus;
	int err;

	mii_bus = mdiobus_alloc();
	if (!mii_bus)
		return -ENOMEM;

	mii_bus->name = "rswitch_mii";
	sprintf(mii_bus->id, "etha%d", rdev->port);
	mii_bus->priv = etha;
	mii_bus->read = rswitch_etha_mii_read;
	mii_bus->write = rswitch_etha_mii_write;
	mii_bus->reset = rswitch_etha_mii_reset;
	/* Use rswitch dev here, because rdev->ndev is not yet registered */
	mii_bus->parent = &rdev->priv->pdev->dev;

	err = of_mdiobus_register(mii_bus, rdev->port_dn);
	if (err < 0) {
		mdiobus_free(mii_bus);
		return err;
	}

	etha->mii = mii_bus;
	return 0;
}

static void rswitch_mii_unregister(struct rswitch_device *rdev)
{
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];

	if (etha->mii) {
		mdiobus_unregister(etha->mii);
		mdiobus_free(etha->mii);
		etha->mii = NULL;
	}
}

static void rswitch_etha_start(struct net_device *ndev)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];
	int err;

	if (etha->speed != ndev->phydev->speed ||
	    etha->mac_addr_write_pending) {
		if (etha->mode != EAMC_OPC_DISABLE) {
			netdev_warn(ndev, "start: unexpected etha mode\n");
			return;
		}

		err = rswitch_etha_change_mode(etha, EAMC_OPC_CONFIG);
		if (err) {
			netdev_warn(ndev, "failed to change etha mode to CONFIG\n");
			return;
		}

		if (etha->speed != ndev->phydev->speed) {
			etha->speed = ndev->phydev->speed;
			rswitch_etha_pis_lsc_setting(etha);	/* with updated etha->speed */
		}

		if (etha->mac_addr_write_pending) {
			rswitch_etha_write_mac_address(etha);
			etha->mac_addr_write_pending = false;
		}

		err = rswitch_etha_change_mode(etha, EAMC_OPC_DISABLE);
		if (err) {
			netdev_warn(ndev, "failed to change etha mode to DISABLE\n");
			return;
		}
	}

	if (etha->mode == EAMC_OPC_DISABLE) {
		err = rswitch_etha_change_mode(etha, EAMC_OPC_OPERATION);
		if (err) {
			netdev_warn(ndev, "failed to change etha mode to OPERATION\n");
			return;
		}
	}

	if (etha->mode != EAMC_OPC_OPERATION) {
		netdev_warn(ndev, "unexpected etha mode\n");
		return;
	}

	if (!rdev->priv->serdes_common_init) {
		err = rswitch_serdes_common_init(etha);
		if (err) {
			netdev_warn(ndev, "failed to initialize serdes\n");
			return;
		}
		rdev->priv->serdes_common_init = true;
	}

	err = rswitch_serdes_chan_init(etha, false);
	if (err < 0) {
		netdev_warn(ndev, "failed to configure serdes\n");
		return;
	}

	etha->link = 1;

	err = rswitch_etha_wait_link_verification(etha);
	if (err < 0)
		netdev_warn(ndev, "link verification failed\n");

	if (etha->needs_usgmii_serdes_check)
		schedule_delayed_work(&etha->usgmii_serdes_check_work,
			msecs_to_jiffies(USGMII_SERDES_CHECK_FIRST_DELAY_MS));
}

static void rswitch_etha_stop(struct net_device *ndev)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];
	int err;

	if (etha->mode != EAMC_OPC_OPERATION) {
		netdev_warn(ndev, "unexpected etha mode\n");
		return;
	}

	if (etha->needs_stop_workaround) {
		/* To unblock switching etha from OPERATION to DISABLE on S4 rev 1.0
		 * in SGMII mode, need to:
		 *   (1) write EAMC_OPC_DISABLE to EAMC,
		 *   (2) run serdes configuration for USGMII mode
		 */
		rs_write32(EAMC_OPC_DISABLE, etha->addr + EAMC);
		err = rswitch_serdes_chan_init(etha, true);
		if (err)
			netdev_warn(ndev, "failed to apply etha stop workaround\n");
	}

	err = rswitch_etha_change_mode(etha, EAMC_OPC_DISABLE);
	if (err != 0) {
		netdev_warn(ndev, "failed to change etha mode to DISABLE\n");
		return;
	}

	etha->link = 0;
}

static void rswitch_adjust_link(struct net_device *ndev)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];
	struct phy_device *phydev = ndev->phydev;

	if (etha->needs_usgmii_serdes_check)
		cancel_delayed_work_sync(&etha->usgmii_serdes_check_work);

	if (phydev->link && etha->link && phydev->speed != etha->speed)
		rswitch_etha_stop(ndev);

	if (phydev->link != etha->link) {
		phy_print_status(phydev);
		if (phydev->link)
			rswitch_etha_start(ndev);
		else
			rswitch_etha_stop(ndev);
	}
}

static int rswitch_phy_init(struct rswitch_device *rdev)
{
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];
	struct phy_device *phydev;

	phydev = of_phy_connect(rdev->ndev, rdev->phy_dn,
				rswitch_adjust_link, 0,
				etha->phy_interface);
	if (!phydev)
		return -ENOENT;

	if (etha->phy_interface == PHY_INTERFACE_MODE_SGMII)
		phy_set_max_speed(phydev, 1000);
	else
		phy_set_max_speed(phydev, 2500);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_10baseT_Half_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_10baseT_Full_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_100baseT_Half_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_1000baseT_Half_BIT);

	phy_attached_info(phydev);
	return 0;
}

static void rswitch_phy_deinit(struct rswitch_device *rdev)
{
	phy_disconnect(rdev->ndev->phydev);
}

static int rswitch_open(struct net_device *ndev)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	unsigned long flags;

	napi_enable(&rdev->napi);

	phy_start(ndev->phydev);

	netif_start_queue(ndev);

	/* Enable RX */
	rswitch_modify(rdev->priv->addr, GWTRC0, 0, BIT(rdev->rx_chain->index));

	/* Enable interrupt */
	spin_lock_irqsave(&rdev->priv->lock, flags);
	rswitch_enadis_data_irq(rdev->priv, rdev->tx_chain->index, true);
	rswitch_enadis_data_irq(rdev->priv, rdev->rx_chain->index, true);
	spin_unlock_irqrestore(&rdev->priv->lock, flags);

	iowrite32(GWCA_TS_IRQ_BIT, rdev->priv->addr + GWTSDIE);

	rdev->priv->chan_running |= BIT(rdev->port);

	if (rdev->brdev)
		rswitch_update_l2_offload(rdev->priv);
	if (!netdev_has_any_upper_dev(ndev))
		rswitch_update_l3_offload(rdev->priv);

	return 0;
};

static int rswitch_stop(struct net_device *ndev)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_gwca_ts_info *ts_info, *ts_info2;

	napi_disable(&rdev->napi);

	phy_stop(ndev->phydev);

	netif_stop_queue(ndev);

	rdev->priv->chan_running &= ~BIT(rdev->port);
	if (rdev->brdev)
		rswitch_update_l2_offload(rdev->priv);
	if (!netdev_has_any_upper_dev(ndev))
		rswitch_update_l3_offload(rdev->priv);
	if (!rdev->priv->chan_running)
		iowrite32(GWCA_TS_IRQ_BIT, rdev->priv->addr + GWTSDID);

	list_for_each_entry_safe(ts_info, ts_info2, &rdev->priv->gwca.ts_info_list, list) {
		if (ts_info->port != rdev->port)
			continue;
		dev_kfree_skb_irq(ts_info->skb);
		list_del(&ts_info->list);
		kfree(ts_info);
	}

	return 0;
};

static int rswitch_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	int ret = NETDEV_TX_OK;
	int entry;
	dma_addr_t dma_addr;
	struct rswitch_ext_desc *desc;
	unsigned long flags;
	struct rswitch_gwca_chain *c = rdev->tx_chain;
	int i, num_desc, pkt_len, size;

	spin_lock_irqsave(&rdev->lock, flags);

	if (rdev->l3_offload_enabled)
		rswitch_handle_l3_learning(rdev, skb);

	num_desc = skb->len % MAX_DESC_SZ ? skb->len / MAX_DESC_SZ + 1 : skb->len / MAX_DESC_SZ;

	if (c->cur - c->dirty > c->num_ring - num_desc) {
		netif_stop_subqueue(ndev, 0);
		ret = NETDEV_TX_BUSY;
		goto out;
	}

	if (skb_put_padto(skb, ETH_ZLEN))
		goto out;

	dma_addr = dma_map_single(ndev->dev.parent, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(ndev->dev.parent, dma_addr))
		goto drop;

	entry = c->cur % c->num_ring;

	/* Stored the skb at the last descriptor so that can free the previous descriptors ASAP */
	c->skb[(entry + num_desc - 1) % c->num_ring] = skb;
	desc = &c->tx_ring[entry];
	desc->dptrl = cpu_to_le32(lower_32_bits(dma_addr));
	desc->dptrh = cpu_to_le32(upper_32_bits(dma_addr));

	if (num_desc > 1) {
		size = skb->len / num_desc;

		pkt_len = skb->len - (num_desc - 1) * size;
		desc->info_ds = cpu_to_le16(pkt_len);
		for (i = 1; i < num_desc; i++) {
			desc = &c->tx_ring[(entry + i) % c->num_ring];
			desc->dptrl = cpu_to_le32(lower_32_bits(dma_addr + pkt_len));
			desc->dptrh = cpu_to_le32(upper_32_bits(dma_addr + pkt_len));
			desc->info_ds = cpu_to_le16(size);

			pkt_len += size;
		}
	} else {
		desc->info_ds = cpu_to_le16(skb->len);
	}

	if (!parallel_mode)
		desc->info1 = cpu_to_le64(INFO1_DV(BIT(rdev->port)) |
					  INFO1_IPV(GWCA_IPV_NUM) | INFO1_FMT);
	else
		desc->info1 = cpu_to_le64(INFO1_IPV(GWCA_IPV_NUM));

	if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
		struct rswitch_gwca_ts_info *ts_info;

		ts_info = kzalloc(sizeof(*ts_info), GFP_ATOMIC);
		if (!ts_info) {
			dma_unmap_single(ndev->dev.parent, dma_addr, skb->len, DMA_TO_DEVICE);
			return -ENOMEM;
		}

		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		rdev->ts_tag++;
		desc->info1 |= cpu_to_le64(INFO1_TSUN(rdev->ts_tag) | INFO1_TXC);

		ts_info->skb = skb_get(skb);
		ts_info->port = rdev->port;
		ts_info->tag = rdev->ts_tag;
		list_add_tail(&ts_info->list, &rdev->priv->gwca.ts_info_list);

		skb_tx_timestamp(skb);
	}

	dma_wmb();

	if (num_desc > 1) {
		for (i = num_desc - 1; i >= 0; i--) {
			desc = &c->tx_ring[(entry + i) % c->num_ring];
			if (!i)
				desc->die_dt = DT_FSTART;
			else if (i == num_desc - 1)
				desc->die_dt = DT_FEND | DIE;
			else
				desc->die_dt = DT_FMID;
		}
	} else {
		desc = &c->tx_ring[entry];
		desc->die_dt = DT_FSINGLE | DIE;
	}

	c->cur += num_desc;
	rswitch_modify(rdev->priv->addr, GWTRC0, 0, BIT(c->index));

out:
	spin_unlock_irqrestore(&rdev->lock, flags);

	return ret;

drop:
	dev_kfree_skb_any(skb);
	goto out;

}

static struct net_device_stats *rswitch_get_stats(struct net_device *ndev)
{
	return &ndev->stats;
}

static int rswitch_hwstamp_get(struct net_device *ndev, struct ifreq *req)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_private *priv = rdev->priv;
	struct rtsn_ptp_private *ptp_priv = priv->ptp_priv;
	struct hwtstamp_config config;

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

static int rswitch_hwstamp_set(struct net_device *ndev, struct ifreq *req)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_private *priv = rdev->priv;
	struct rtsn_ptp_private *ptp_priv = priv->ptp_priv;
	struct hwtstamp_config config;
	u32 tstamp_rx_ctrl = RTSN_RXTSTAMP_ENABLED;
	u32 tstamp_tx_ctrl;

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

static int rswitch_do_ioctl(struct net_device *ndev, struct ifreq *req, int cmd)
{
	if (!netif_running(ndev))
		return -EINVAL;

	switch (cmd) {
	case SIOCGHWTSTAMP:
		return rswitch_hwstamp_get(ndev, req);
	case SIOCSHWTSTAMP:
		return rswitch_hwstamp_set(ndev, req);
	default:
		if (ndev->phydev)
			return phy_mii_ioctl(ndev->phydev, req, cmd);
	}

	return -EOPNOTSUPP;
}

static int rswitch_get_port_parent_id(struct net_device *ndev,
				      struct netdev_phys_item_id *ppid)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	const char *name;

	name = dev_name(&rdev->priv->pdev->dev);
	ppid->id_len = min_t(size_t, strlen(name), sizeof(ppid->id_len));
	memcpy(ppid->id, name, ppid->id_len);
	return 0;
}

static int rswitch_get_phys_port_name(struct net_device *ndev,
				      char *name, size_t len)
{
	struct rswitch_device *rdev = netdev_priv(ndev);

	snprintf(name, len, "tsn%d", rdev->port);
	return 0;
}

int rswitch_set_mac_address(struct net_device *ndev, void *p)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];
	int ret;

	ret = eth_mac_addr(ndev, p);

	if (ret == 0) {
		/* eth_mac_addr() will reject if netdev is up, thus deferring
		 * write to hardware till next link up is ok */
		ether_addr_copy(etha->mac_addr, ndev->dev_addr);
		etha->mac_addr_write_pending = true;
	}

	return ret;
}

static const struct net_device_ops rswitch_netdev_ops = {
	.ndo_open = rswitch_open,
	.ndo_stop = rswitch_stop,
	.ndo_start_xmit = rswitch_start_xmit,
	.ndo_get_stats = rswitch_get_stats,
	.ndo_do_ioctl = rswitch_do_ioctl,
	.ndo_get_port_parent_id = rswitch_get_port_parent_id,
	.ndo_get_phys_port_name = rswitch_get_phys_port_name,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_set_mac_address = rswitch_set_mac_address,
};

static bool rswitch_port_check(const struct net_device *ndev)
{
	return (ndev->netdev_ops == &rswitch_netdev_ops);
}

static int rswitch_get_ts_info(struct net_device *ndev, struct ethtool_ts_info *info)
{
	struct rswitch_device *rdev = netdev_priv(ndev);

	info->phc_index = ptp_clock_index(rdev->priv->ptp_priv->clock);
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

static const struct ethtool_ops rswitch_ethtool_ops = {
	.get_ts_info = rswitch_get_ts_info,
	.get_link_ksettings = phy_ethtool_get_link_ksettings,
	.set_link_ksettings = phy_ethtool_set_link_ksettings,
};

static const struct of_device_id renesas_eth_sw_of_table[] = {
	{ .compatible = "renesas,etherswitch", },
	{ }
};
MODULE_DEVICE_TABLE(of, renesas_eth_sw_of_table);

static void rswitch_clock_enable(struct rswitch_private *priv)
{
	rs_write32(GENMASK(RSWITCH_NUM_HW - 1, 0) | RCEC_RCE, priv->addr + RCEC);
}

static void rswitch_reset(struct rswitch_private *priv)
{
	if (!parallel_mode) {
		rs_write32(RRC_RR, priv->addr + RRC);
		rs_write32(RRC_RR_CLR, priv->addr + RRC);

		reset_control_assert(priv->sd_rst);
		mdelay(1);
		reset_control_deassert(priv->sd_rst);

		/* There is a slight difference in SerDes hardware behavior between
		 * each version after resetting. This step is to ensure the stable
		 * condition of initialization, especially for R-Car S4 v1.1.
		 */
		mdelay(1);
		rs_write32(0, priv->serdes_addr + RSWITCH_SERDES_LOCAL_OFFSET);
	} else {
		int gwca_idx;
		u32 gwro_offset;
		int mode;
		int count;

		if (priv->gwca.index == RSWITCH_GWCA_IDX_TO_HW_NUM(0)) {
			gwca_idx = 1;
			gwro_offset = RSWITCH_GWCA1_OFFSET;
		} else {
			gwca_idx = 0;
			gwro_offset = RSWITCH_GWCA0_OFFSET;
		}

		count = 0;
		do {
			mode = rs_read32(priv->addr + gwro_offset + 0x0004) & GWMS_OPS_MASK;
			if (mode == GWMC_OPC_OPERATION)
				break;

			count++;
			if (!(count % 100))
				pr_info(" rswitch wait for GWMS%d %d==%d\n", gwca_idx, mode,
					GWMC_OPC_OPERATION);

			mdelay(10);
		} while (1);
	}
}

static const struct soc_device_attribute rswitch_soc_needs_etha_stop_wa[]  = {
	{ .soc_id = "r8a779f0", .revision = "ES1.0" },
	{ /* Sentinel */ }
};

static const struct soc_device_attribute rswitch_soc_needs_usgmii_serdes_check[]  = {
	{ .soc_id = "r8a779f0", .revision = "ES1.0" },
	{ /* Sentinel */ }
};

static void rswitch_etha_init(struct rswitch_device *rdev)
{
	struct rswitch_private *priv = rdev->priv;
	struct rswitch_etha *etha = &priv->etha[rdev->port];
	int err;

	memset(etha, 0, sizeof(*etha));

	etha->index = rdev->port;

	etha->addr = priv->addr + rswitch_etha_offs(etha->index);
	etha->serdes_addr = priv->serdes_addr + etha->index * RSWITCH_SERDES_OFFSET;

	err = of_get_phy_mode(rdev->port_dn, &etha->phy_interface);
	if (err < 0) {
		/* The default is SGMII interface */
		etha->phy_interface = PHY_INTERFACE_MODE_SGMII;
	}

	if (etha->phy_interface == PHY_INTERFACE_MODE_SGMII &&
	    soc_device_match(rswitch_soc_needs_etha_stop_wa))
		etha->needs_stop_workaround = true;

	if (etha->phy_interface == PHY_INTERFACE_MODE_USXGMII &&
	    soc_device_match(rswitch_soc_needs_usgmii_serdes_check)) {
		etha->needs_usgmii_serdes_check = true;
		INIT_DELAYED_WORK(&etha->usgmii_serdes_check_work,
				rswitch_usgmii_serdes_check);
	}

	/* MPIC.PSMCS = (clk [MHz] / (MDC frequency [MHz] * 2) - 1.
	 * Calculating PSMCS value as MDC frequency = 2.5MHz. So, multiply
	 * both the numerator and the denominator by 10.
	 */
	etha->psmcs = clk_get_rate(priv->clk) / 100000 / (25 * 2) - 1;
}

static int rswitch_gwca_change_mode(struct rswitch_private *priv,
				    enum rswitch_gwca_mode mode)
{
	int ret;

	/* Enable clock */
	if (!rswitch_agent_clock_is_enabled(priv->addr, priv->gwca.index))
		rswitch_agent_clock_ctrl(priv->addr, priv->gwca.index, 1);

	rs_write32(mode, priv->addr + GWMC);

	ret = rswitch_reg_wait(priv->addr, GWMS, GWMS_OPS_MASK, mode);

	/* Disable clock */
	if (mode == GWMC_OPC_DISABLE)
		rswitch_agent_clock_ctrl(priv->addr, priv->gwca.index, 0);

	return ret;
}

static int rswitch_gwca_mcast_table_reset(struct rswitch_private *priv)
{
	rs_write32(GWMTIRM_MTIOG, priv->addr + GWMTIRM);
	return rswitch_reg_wait(priv->addr, GWMTIRM, GWMTIRM_MTR, GWMTIRM_MTR);
}

static int rswitch_gwca_axi_ram_reset(struct rswitch_private *priv)
{
	rs_write32(GWARIRM_ARIOG, priv->addr + GWARIRM);
	return rswitch_reg_wait(priv->addr, GWARIRM, GWARIRM_ARR, GWARIRM_ARR);
}

static int rswitch_gwca_hw_init(struct rswitch_private *priv)
{
	int err;

	err = rswitch_gwca_change_mode(priv, GWMC_OPC_DISABLE);
	if (err < 0)
		return err;
	err = rswitch_gwca_change_mode(priv, GWMC_OPC_CONFIG);
	if (err < 0)
		return err;
	err = rswitch_gwca_mcast_table_reset(priv);
	if (err < 0)
		return err;
	err = rswitch_gwca_axi_ram_reset(priv);
	if (err < 0)
		return err;

	/* Full setting flow */
	rs_write32(GWVCC_VEM_SC_TAG, priv->addr + GWVCC);
	rs_write32(0, priv->addr + GWTTFC);
	rs_write32(lower_32_bits(priv->desc_bat_dma), priv->addr + GWDCBAC1);
	rs_write32(upper_32_bits(priv->desc_bat_dma), priv->addr + GWDCBAC0);
	iowrite32(lower_32_bits(priv->gwca.ts_queue.ring_dma), priv->addr + GWTDCAC10);
	iowrite32(upper_32_bits(priv->gwca.ts_queue.ring_dma), priv->addr + GWTDCAC00);
	iowrite32(GWCA_TS_IRQ_BIT, priv->addr + GWTSDCC0);

	iowrite32((0xff << 8) | 0xff, priv->addr + GWMDNC);
	iowrite32(GWTPC_PPPL(GWCA_IPV_NUM), priv->addr + GWTPC0);

	err = rswitch_gwca_change_mode(priv, GWMC_OPC_DISABLE);
	if (err < 0)
		return err;
	err = rswitch_gwca_change_mode(priv, GWMC_OPC_OPERATION);
	if (err < 0)
		return err;

	return 0;
}

static void rswitch_gwca_chain_free(struct net_device *ndev,
				    struct rswitch_private *priv,
				    struct rswitch_gwca_chain *c)
{
	int i;

	if (!c->dir_tx) {
		dma_free_coherent(ndev->dev.parent,
				  sizeof(struct rswitch_ext_ts_desc) *
				  (c->num_ring + 1), c->rx_ring, c->ring_dma);
		c->rx_ring = NULL;

		for (i = 0; i < c->num_ring; i++)
			skb_free_frag(c->rx_bufs[i]);

		kfree(c->rx_bufs);
		c->rx_bufs = NULL;
	} else {
		dma_free_coherent(ndev->dev.parent,
				  sizeof(struct rswitch_desc) *
				  (c->num_ring + 1), c->tx_ring, c->ring_dma);
		c->tx_ring = NULL;
		kfree(c->skb);
		c->skb = NULL;
	}
}

static void rswitch_gwca_ts_queue_free(struct rswitch_private *priv)
{
	struct rswitch_gwca_chain *gq = &priv->gwca.ts_queue;

	dma_free_coherent(&priv->pdev->dev,
			  sizeof(struct rswitch_ts_desc) * (gq->num_ring + 1),
			  gq->ts_ring, gq->ring_dma);
	gq->ts_ring = NULL;
}

static int rswitch_gwca_chain_init(struct net_device *ndev,
				   struct rswitch_private *priv,
				   struct rswitch_gwca_chain *c,
				   bool dir_tx, int num_ring)
{
	int i, bit;
	int index = c->index;	/* Keep the index before memset() */
	void *rx_buf;

	memset(c, 0, sizeof(*c));
	c->index = index;
	c->dir_tx = dir_tx;
	c->num_ring = num_ring;
	c->ndev = ndev;

	if (!dir_tx) {
		c->rx_bufs = kcalloc(c->num_ring, sizeof(*c->rx_bufs), GFP_KERNEL);
		if (!c->rx_bufs)
			goto out;

		for (i = 0; i < c->num_ring; i++) {
			rx_buf = netdev_alloc_frag(RSWITCH_RX_BUF_SIZE);
			if (!rx_buf)
				goto out;
			c->rx_bufs[i] = rx_buf;
		}
		c->rx_ring = dma_alloc_coherent(ndev->dev.parent,
				sizeof(struct rswitch_ext_ts_desc) *
				(c->num_ring + 1), &c->ring_dma, GFP_KERNEL);
	} else {
		c->skb = kcalloc(c->num_ring, sizeof(*c->skb), GFP_KERNEL);
		if (!c->skb)
			return -ENOMEM;

		c->tx_ring = dma_alloc_coherent(ndev->dev.parent,
				sizeof(struct rswitch_ext_desc) *
				(c->num_ring + 1), &c->ring_dma, GFP_KERNEL);
	}
	if (!c->rx_ring && !c->tx_ring)
		goto out;

	index = c->index / 32;
	bit = BIT(c->index % 32);
	if (dir_tx)
		priv->gwca.tx_irq_bits[index] |= bit;
	else
		priv->gwca.rx_irq_bits[index] |= bit;

	return 0;

out:
	rswitch_gwca_chain_free(ndev, priv, c);

	return -ENOMEM;
}

static int rswitch_gwca_ts_queue_alloc(struct rswitch_private *priv)
{
	struct rswitch_gwca_chain *gq = &priv->gwca.ts_queue;

	memset(gq, 0, sizeof(*gq));
	gq->num_ring = TS_RING_SIZE;
	gq->ts_ring = dma_alloc_coherent(&priv->pdev->dev,
					 sizeof(struct rswitch_ts_desc) *
					 (gq->num_ring + 1), &gq->ring_dma, GFP_KERNEL);
	return !gq->ts_ring ? -ENOMEM : 0;
}

static int rswitch_gwca_chain_format(struct net_device *ndev,
				struct rswitch_private *priv,
				struct rswitch_gwca_chain *c)
{
	struct rswitch_ext_desc *ring;
	struct rswitch_desc *desc;
	int tx_ring_size = sizeof(*ring) * c->num_ring;
	int i;
	dma_addr_t dma_addr;

	memset(c->tx_ring, 0, tx_ring_size);
	for (i = 0, ring = c->tx_ring; i < c->num_ring; i++, ring++) {
		if (!c->dir_tx) {
			dma_addr = dma_map_single(ndev->dev.parent,
					c->rx_bufs[i] + NET_SKB_PAD + NET_IP_ALIGN,
					RSWITCH_RX_BUF_SIZE - NET_SKB_PAD - NET_IP_ALIGN,
					DMA_FROM_DEVICE);
			if (!dma_mapping_error(ndev->dev.parent, dma_addr))
				ring->info_ds = cpu_to_le16(MAX_DESC_SZ);
			ring->dptrl = cpu_to_le32(lower_32_bits(dma_addr));
			ring->dptrh = cpu_to_le32(upper_32_bits(dma_addr));
			ring->die_dt = DT_FEMPTY | DIE;
		} else {
			ring->die_dt = DT_EEMPTY | DIE;
		}
	}
	ring->dptrl = cpu_to_le32(lower_32_bits(c->ring_dma));
	ring->dptrh = cpu_to_le32(upper_32_bits(c->ring_dma));
	ring->die_dt = DT_LINKFIX;

	desc = &priv->desc_bat[c->index];
	desc->die_dt = DT_LINKFIX;
	desc->dptrl = cpu_to_le32(lower_32_bits(c->ring_dma));
	desc->dptrh = cpu_to_le32(upper_32_bits(c->ring_dma));

	iowrite32(GWDCC_BALR | (c->dir_tx ? GWDCC_DCP(GWCA_IPV_NUM) | GWDCC_DQT : 0) | GWDCC_EDE,
		  priv->addr + GWDCC_OFFS(c->index));

	return 0;
}

static void rswitch_gwca_ts_queue_fill(struct rswitch_private *priv,
				       int start_index, int num)
{
	struct rswitch_gwca_chain *gq = &priv->gwca.ts_queue;
	struct rswitch_ts_desc *desc;
	int i, index;

	for (i = 0; i < num; i++) {
		index = (i + start_index) % gq->num_ring;
		desc = &gq->ts_ring[index];
		desc->die_dt = DT_FEMPTY_ND | DIE;
	}

	desc = &gq->ts_ring[gq->num_ring];
	desc->die_dt = DT_LINKFIX;
	desc->dptrl = cpu_to_le32(lower_32_bits(gq->ring_dma));
	desc->dptrh = cpu_to_le32(upper_32_bits(gq->ring_dma));
}

static int rswitch_gwca_chain_ext_ts_format(struct net_device *ndev,
					    struct rswitch_private *priv,
					    struct rswitch_gwca_chain *c)
{
	struct rswitch_ext_ts_desc *ring;
	struct rswitch_desc *desc;
	int ring_size = sizeof(*ring) * c->num_ring;
	int i;
	dma_addr_t dma_addr;

	memset(c->rx_ring, 0, ring_size);
	for (i = 0, ring = c->rx_ring; i < c->num_ring; i++, ring++) {
		if (!c->dir_tx) {
			dma_addr = dma_map_single(ndev->dev.parent,
					c->rx_bufs[i] + NET_SKB_PAD + NET_IP_ALIGN,
					RSWITCH_RX_BUF_SIZE - NET_SKB_PAD - NET_IP_ALIGN,
					DMA_FROM_DEVICE);
			if (!dma_mapping_error(ndev->dev.parent, dma_addr))
				ring->info_ds = cpu_to_le16(MAX_DESC_SZ);
			ring->dptrl = cpu_to_le32(lower_32_bits(dma_addr));
			ring->dptrh = cpu_to_le32(upper_32_bits(dma_addr));
			ring->die_dt = DT_FEMPTY | DIE;
		} else {
			ring->die_dt = DT_EEMPTY | DIE;
		}
	}
	ring->dptrl = cpu_to_le32(lower_32_bits(c->ring_dma));
	ring->dptrh = cpu_to_le32(upper_32_bits(c->ring_dma));
	ring->die_dt = DT_LINKFIX;

	desc = &priv->desc_bat[c->index];
	desc->die_dt = DT_LINKFIX;
	desc->dptrl = cpu_to_le32(lower_32_bits(c->ring_dma));
	desc->dptrh = cpu_to_le32(upper_32_bits(c->ring_dma));

	iowrite32(GWDCC_BALR | (c->dir_tx ? GWDCC_DCP(GWCA_IPV_NUM) | GWDCC_DQT : 0) |
		  GWDCC_ETS | GWDCC_EDE,
		  priv->addr + GWDCC_OFFS(c->index));

	return 0;
}

static int rswitch_desc_alloc(struct rswitch_private *priv)
{
	struct device *dev = &priv->pdev->dev;
	int i, num_chains = priv->gwca.num_chains;

	priv->desc_bat_size = sizeof(struct rswitch_desc) * num_chains;
	priv->desc_bat = dma_alloc_coherent(dev, priv->desc_bat_size,
					    &priv->desc_bat_dma, GFP_KERNEL);
	if (!priv->desc_bat)
		return -ENOMEM;
	for (i = 0; i < num_chains; i++)
		priv->desc_bat[i].die_dt = DT_EOS;

	return 0;
}

static void rswitch_desc_free(struct rswitch_private *priv)
{
	if (priv->desc_bat)
		dma_free_coherent(&priv->pdev->dev, priv->desc_bat_size,
				  priv->desc_bat, priv->desc_bat_dma);
	priv->desc_bat = NULL;
}

static struct rswitch_gwca_chain *rswitch_gwca_get(struct rswitch_private *priv)
{
	int index;

	index = find_first_zero_bit(priv->gwca.used, priv->gwca.num_chains);
	if (index >= priv->gwca.num_chains)
		return NULL;
	set_bit(index, priv->gwca.used);
	priv->gwca.chains[index].index = index;

	return &priv->gwca.chains[index];
}

static void rswitch_gwca_put(struct rswitch_private *priv,
			     struct rswitch_gwca_chain *c)
{
	clear_bit(c->index, priv->gwca.used);
}

static int rswitch_txdmac_init(struct net_device *ndev,
			       struct rswitch_private *priv)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	int err;

	rdev->tx_chain = rswitch_gwca_get(priv);
	if (!rdev->tx_chain)
		return -EBUSY;

	err = rswitch_gwca_chain_init(ndev, priv, rdev->tx_chain, true, TX_RING_SIZE);
	if (err < 0)
		goto out_init;

	err = rswitch_gwca_chain_format(ndev, priv, rdev->tx_chain);
	if (err < 0)
		goto out_format;

	return 0;

out_format:
	rswitch_gwca_chain_free(ndev, priv, rdev->tx_chain);

out_init:
	rswitch_gwca_put(priv, rdev->tx_chain);

	return err;
}

static void rswitch_txdmac_free(struct net_device *ndev,
				struct rswitch_private *priv)
{
	struct rswitch_device *rdev = netdev_priv(ndev);

	rswitch_gwca_chain_free(ndev, priv, rdev->tx_chain);
	rswitch_gwca_put(priv, rdev->tx_chain);
}

static int rswitch_rxdmac_init(struct net_device *ndev,
			       struct rswitch_private *priv)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	int err;

	rdev->rx_chain = rswitch_gwca_get(priv);
	if (!rdev->rx_chain)
		return -EBUSY;

	err = rswitch_gwca_chain_init(ndev, priv, rdev->rx_chain, false, RX_RING_SIZE);
	if (err < 0)
		goto out_init;
	err = rswitch_gwca_chain_ext_ts_format(ndev, priv, rdev->rx_chain);
	if (err < 0)
		goto out_format;

	return 0;

out_format:
	rswitch_gwca_chain_free(ndev, priv, rdev->rx_chain);

out_init:
	rswitch_gwca_put(priv, rdev->rx_chain);

	return err;
}

static void rswitch_rxdmac_free(struct net_device *ndev,
				struct rswitch_private *priv)
{
	struct rswitch_device *rdev = netdev_priv(ndev);

	rswitch_gwca_chain_free(ndev, priv, rdev->rx_chain);
	rswitch_gwca_put(priv, rdev->rx_chain);
}

static void rswitch_init_mac_address(struct rswitch_device *rdev)
{
	struct rswitch_etha *etha = &rdev->priv->etha[rdev->port];
	struct net_device *ndev = rdev->ndev;
	const u8 *mac;

	mac = of_get_mac_address(rdev->port_dn);
	if (!IS_ERR(mac))
		ether_addr_copy(ndev->dev_addr, mac);

	/*
	 * Kernels before commit f10843e04a07 ("of: net: fix
	 * of_get_mac_addr_nvmem() for non-platform devices") of 5.13 cycle
	 * fail to get nvmem-stored mac address from port node because this
	 * node does not correspond to a platform_device.
	 *
	 * Thus have to explicitly use nvmem API here.
	 */
	if (!is_valid_ether_addr(ndev->dev_addr)) {
		struct nvmem_cell *cell;
		size_t len;

		cell = of_nvmem_cell_get(rdev->port_dn, "mac-address");
		if (!IS_ERR(cell)) {
			mac = nvmem_cell_read(cell, &len);
			if (!IS_ERR(mac)) {
				if (len == ETH_ALEN)
					ether_addr_copy(ndev->dev_addr, mac);
				kfree(mac);
			}
			nvmem_cell_put(cell);
		}
	}

	if (!is_valid_ether_addr(ndev->dev_addr))
		ether_addr_copy(ndev->dev_addr, etha->mac_addr);

	if (!is_valid_ether_addr(ndev->dev_addr))
		eth_hw_addr_random(ndev);

	ether_addr_copy(etha->mac_addr, ndev->dev_addr);
	etha->mac_addr_write_pending = true;
}

static int rswitch_ndev_init(struct rswitch_device *rdev)
{
	struct rswitch_private *priv = rdev->priv;
	struct net_device *ndev = rdev->ndev;
	int err;

	netif_napi_add(ndev, &rdev->napi, rswitch_poll, 64);

	rswitch_init_mac_address(rdev);

	/* FIXME: it seems S4 VPF has FWPBFCSDC0/1 only so that we cannot set
	 * CSD = 1 (rx_chain->index = 1) for FWPBFCS03. So, use index = 0
	 * for the RX.
	 */
	err = rswitch_rxdmac_init(ndev, priv);
	if (err < 0)
		goto out_rxdmac;

	err = rswitch_txdmac_init(ndev, priv);
	if (err < 0)
		goto out_txdmac;

	/* Print device information */
	netdev_info(ndev, "MAC address %pMn", ndev->dev_addr);

	return 0;

out_txdmac:
	rswitch_rxdmac_free(ndev, priv);

out_rxdmac:
	netif_napi_del(&rdev->napi);

	return err;
}

static void rswitch_ndev_deinit(struct rswitch_device *rdev)
{
	struct rswitch_private *priv = rdev->priv;
	struct net_device *ndev = rdev->ndev;

	rswitch_txdmac_free(ndev, priv);
	rswitch_rxdmac_free(ndev, priv);
	netif_napi_del(&rdev->napi);
}

static int rswitch_bpool_config(struct rswitch_private *priv)
{
	u32 val;

	val = rs_read32(priv->addr + CABPIRM);
	if (val & CABPIRM_BPR)
		return 0;

	rs_write32(CABPIRM_BPIOG, priv->addr + CABPIRM);
	return rswitch_reg_wait(priv->addr, CABPIRM, CABPIRM_BPR, CABPIRM_BPR);
}

static void rswitch_coma_init(struct rswitch_private *priv)
{
	iowrite32(CABPPFLC_INIT_VALUE, priv->addr + CABPPFLC0);
}

static void rswitch_queue_interrupt(struct net_device *ndev)
{
	struct rswitch_device *rdev = netdev_priv(ndev);

	if (napi_schedule_prep(&rdev->napi)) {
		spin_lock(&rdev->priv->lock);
		rswitch_enadis_data_irq(rdev->priv, rdev->tx_chain->index, false);
		rswitch_enadis_data_irq(rdev->priv, rdev->rx_chain->index, false);
		spin_unlock(&rdev->priv->lock);
		__napi_schedule(&rdev->napi);
	}
}

static irqreturn_t __maybe_unused rswitch_data_irq(struct rswitch_private *priv, u32 *dis)
{
	struct rswitch_gwca_chain *c;
	int i;
	int index, bit;

	for (i = 0; i < priv->gwca.num_chains; i++) {
		if (!test_bit(i, priv->gwca.used))
			continue;
		c = &priv->gwca.chains[i];
		index = c->index / 32;
		bit = BIT(c->index % 32);
		if (!(dis[index] & bit))
			continue;

		rswitch_ack_data_irq(priv, c->index);
		rswitch_queue_interrupt(c->ndev);
	}

	return IRQ_HANDLED;
}

static irqreturn_t rswitch_irq(int irq, void *dev_id)
{
	struct rswitch_private *priv = dev_id;
	irqreturn_t ret = IRQ_NONE;
	u32 dis[RSWITCH_NUM_IRQ_REGS];

	rswitch_get_data_irq_status(priv, dis);

	if (rswitch_is_any_data_irq(priv, dis, true) ||
	    rswitch_is_any_data_irq(priv, dis, false))
		ret = rswitch_data_irq(priv, dis);

	return ret;
}

static int rswitch_request_irqs(struct rswitch_private *priv)
{
	int irq, err;

	/* FIXME: other queues */
	irq = platform_get_irq_byname(priv->pdev, "gwca1_rxtx0");
	if (irq < 0)
		goto out;

	err = request_irq(irq, rswitch_irq, 0, "rswitch: gwca1_rxtx0", priv);
	if (err < 0)
		goto out;

out:
	return err;
}

static int rswitch_free_irqs(struct rswitch_private *priv)
{
	int irq;

	irq = platform_get_irq_byname(priv->pdev, "gwca1_rxtx0");
	if (irq < 0)
		return irq;

	free_irq(irq, priv);

	return 0;
}

static void rswitch_ts(struct rswitch_private *priv)
{
	struct rswitch_gwca_chain *gq = &priv->gwca.ts_queue;
	struct rswitch_gwca_ts_info *ts_info, *ts_info2;
	struct skb_shared_hwtstamps shhwtstamps;
	int entry = gq->cur % gq->num_ring;
	struct rswitch_ts_desc *desc;
	struct timespec64 ts;
	u32 tag, port;

	desc = &gq->ts_ring[entry];
	while ((desc->die_dt & DT_MASK) != DT_FEMPTY_ND) {
		dma_rmb();

		port = TS_DESC_DPN(__le32_to_cpu(desc->dptrl));
		tag = TS_DESC_TSUN(__le32_to_cpu(desc->dptrl));

		list_for_each_entry_safe(ts_info, ts_info2, &priv->gwca.ts_info_list, list) {
			if (!(ts_info->port == port && ts_info->tag == tag))
				continue;

			memset(&shhwtstamps, 0, sizeof(shhwtstamps));
			ts.tv_sec = __le32_to_cpu(desc->ts_sec);
			ts.tv_nsec = __le32_to_cpu(desc->ts_nsec & cpu_to_le32(0x3fffffff));
			shhwtstamps.hwtstamp = timespec64_to_ktime(ts);
			skb_tstamp_tx(ts_info->skb, &shhwtstamps);
			dev_consume_skb_irq(ts_info->skb);
			list_del(&ts_info->list);
			kfree(ts_info);
			break;
		}

		gq->cur++;
		entry = gq->cur % gq->num_ring;
		desc = &gq->ts_ring[entry];
	}

	/* Refill the TS ring buffers */
	for (; gq->cur - gq->dirty > 0; gq->dirty++) {
		entry = gq->dirty % gq->num_ring;
		desc = &gq->ts_ring[entry];
		desc->die_dt = DT_FEMPTY_ND | DIE;
	}
}

static irqreturn_t rswitch_gwca_ts_irq(int irq, void *dev_id)
{
	struct rswitch_private *priv = dev_id;

	if (ioread32(priv->addr + GWTSDIS) & GWCA_TS_IRQ_BIT) {
		iowrite32(GWCA_TS_IRQ_BIT, priv->addr + GWTSDIS);
		rswitch_ts(priv);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int rswitch_gwca_ts_request_irqs(struct rswitch_private *priv)
{
	int irq;

	irq = platform_get_irq_byname(priv->pdev, GWCA_TS_IRQ_RESOURCE_NAME);
	if (irq < 0)
		return irq;

	return devm_request_irq(&priv->pdev->dev, irq, rswitch_gwca_ts_irq,
				0, GWCA_TS_IRQ_NAME, priv);
}

static int rswitch_fwd_init(struct rswitch_private *priv)
{
	int port, ret;
	int gwca_hw_port = RSWITCH_HW_NUM_TO_GWCA_IDX(priv->gwca.index);
	u32 all_ports = GENMASK(RSWITCH_NUM_HW - 1, 0);
	struct rswitch_device *rdev;

	/* Start with empty configuration, with all features disabled and
	 * all forwarding disallowed */
	for (port = 0; port < RSWITCH_NUM_HW; port++) {
		/* No features */
		rs_write32(0, priv->addr + FWPC0(port));
		/* All L3 forwarding disallowed, direct descriptor disabled */
		rs_write32(FIELD_PREP(FWCP1_LTHFW_MASK, all_ports),
				priv->addr + FWPC1(port));
		/* All L2 forwarding disallowed */
		rs_write32(FIELD_PREP(FWCP2_LTWFW_MASK, all_ports),
				priv->addr + FWPC2(port));
		/* Port based forwarding disallowed */
		rs_write32(0, priv->addr + FWPBFC(port));
	}

	/* For enabled ETHA ports, set port based forwarding to per-port
	 * rx chains */
	rswitch_for_all_ports(priv, rdev) {
		port = rdev->port;
		/* Port-based forwarding to GWCA port */
		rs_write32(FIELD_PREP(FWPBFC_PBDV_MASK, BIT(priv->gwca.index)),
				priv->addr + FWPBFC(port));
		/* Subdestination within GWCA port */
		rs_write32(rdev->rx_chain->index,
				priv->addr + FWPBFCSDC(gwca_hw_port, port));
	}

	/* For GWCA port, allow direct descriptors */
	rswitch_modify(priv->addr, FWPC1(priv->gwca.index), 0, FWPC1_DDE);

	/* Initialize hardware L2 forwarding table */

	/* Allow entire table to be used for "unsecure" entries */
	rswitch_modify(priv->addr, FWMACHEC, 0, FWMACHEC_MACHMUE_MASK);

	/* Initialize MAC hash table */
	rs_write32(FWMACTIM_MACTIOG, priv->addr + FWMACTIM);
	ret = rswitch_reg_wait(priv->addr, FWMACTIM, FWMACTIM_MACTIOG, 0);
	if (ret)
		return ret;

	/* Initialize hardware L3 forwarding */

	/* Allow entire stream table to be used for "unsecure" entries */
	rswitch_modify(priv->addr, FWLTHHEC,
			FWLTHHEC_HMUE,
			FIELD_PREP(FWLTHHEC_HMUE, 1 << RSWITCH_LTH_STREAM_W));

	/* Include only dst_ip (and frame type) in ipv4 stream id */
	rs_write32(FWIP4SC_IIDS, priv->addr + FWIP4SC);

	/* Initialize stream hash table */
	rs_write32(FWLTHTIM_TIOG, priv->addr + FWLTHTIM);
	ret = rswitch_reg_wait(priv->addr, FWLTHTIM, FWLTHTIM_TR, FWLTHTIM_TR);
	if (ret)
		return ret;

	/* Allow access to frame update rules from "unsecure" APB */
	rs_write32(0xffffffff, priv->addr + FWSCR34);
#if RSWITCH_LTH_RRULE_W >= 6
	rs_write32(0xffffffff, priv->addr + FWSCR33);
#endif
#if RSWITCH_LTH_RRULE_W >= 7
	rs_write32(0xffffffff, priv->addr + FWSCR32);
	rs_write32(0xffffffff, priv->addr + FWSCR31);
#endif
#if RSWITCH_LTH_RRULE_W >= 8
	rs_write32(0xffffffff, priv->addr + FWSCR30);
	rs_write32(0xffffffff, priv->addr + FWSCR29);
	rs_write32(0xffffffff, priv->addr + FWSCR28);
	rs_write32(0xffffffff, priv->addr + FWSCR27);
#endif

	/* Initialize frame update rules table */
	rs_write32(FWL23UTIM_TIOG, priv->addr + FWL23UTIM);
	ret = rswitch_reg_wait(priv->addr, FWL23UTIM, FWL23UTIM_TR, FWL23UTIM_TR);
	if (ret)
		return ret;

	/* TODO: add chrdev for fwd */
	/* TODO: add proc for fwd */

	return 0;
}

static int rswitch_init(struct rswitch_private *priv)
{
	struct rswitch_device *rdev;
	int err;

	err = rswitch_desc_alloc(priv);
	if (err < 0)
		return -ENOMEM;

	err = rswitch_gwca_ts_queue_alloc(priv);
	if (err < 0)
		goto err_ts_queue_alloc;

	rswitch_gwca_ts_queue_fill(priv, 0, TS_RING_SIZE);
	INIT_LIST_HEAD(&priv->gwca.ts_info_list);

	if (!parallel_mode)
		rswitch_clock_enable(priv);

	rswitch_for_all_ports(priv, rdev) {
		rswitch_etha_read_mac_address(&priv->etha[rdev->port]);
	}

	rswitch_reset(priv);

	err = rswitch_gwca_hw_init(priv);
	if (err < 0)
		goto err_gwca_hw_init;

	if (!parallel_mode) {
		rswitch_for_all_ports(priv, rdev) {
			err = rswitch_etha_hw_init(rdev);
			if (err < 0)
				goto err_etha_hw_init;
		}
	}

	rswitch_for_all_ports(priv, rdev) {
		err = rswitch_ndev_init(rdev);
		if (err < 0) {
			rswitch_undo_for_prev_ports(priv, rdev) {
				rswitch_ndev_deinit(rdev);
			}
			goto err_ndev_init;
		}
	}

	if (!parallel_mode) {
		err = rswitch_bpool_config(priv);
		if (err < 0)
			goto err_bpool_config;

		rswitch_coma_init(priv);

		err = rswitch_fwd_init(priv);
		if (err < 0)
			goto err_fwd_init;
	}

	err = rtsn_ptp_init(priv->ptp_priv, RTSN_PTP_REG_LAYOUT_S4, RTSN_PTP_CLOCK_S4);
	if (err < 0)
		goto err_ptp_init;

	err = rswitch_request_irqs(priv);
	if (err < 0)
		goto err_request_irqs;
	err = rswitch_gwca_ts_request_irqs(priv);
	if (err < 0)
		goto err_gwca_ts_request_irqs;

	rswitch_for_all_ports(priv, rdev) {
		/* Register mii and attach to phy before registering netdev,
		 * otherwise can enter rswitch_open when phy attachment is
		 * not yes complete, which leads to crash inside phy subsystem.
		 */
		if (!parallel_mode) {
			err = rswitch_mii_register(rdev);
			if (err)
				goto err_mii_register;
		}

		err = rswitch_phy_init(rdev);
		if (err)
			goto err_phy_init;

		err = register_netdev(rdev->ndev);
		if (err) {
		err_mii_register:
			rswitch_undo_for_prev_ports(priv, rdev) {
				unregister_netdev(rdev->ndev);
				rswitch_phy_deinit(rdev);
		err_phy_init:
				rswitch_mii_unregister(rdev);
			}
			goto err_register_loop;
		}
	}

	return 0;

err_register_loop:
err_gwca_ts_request_irqs:
err_request_irqs:
	rtsn_ptp_unregister(priv->ptp_priv);
err_ptp_init:
err_fwd_init:
err_bpool_config:
	rswitch_for_all_ports(priv, rdev) {
		rswitch_ndev_deinit(rdev);
	}
err_ndev_init:
err_etha_hw_init:
err_gwca_hw_init:
err_ts_queue_alloc:
	rswitch_desc_free(priv);

	return err;
}

static void rswitch_deinit(struct rswitch_private *priv)
{
	struct rswitch_device *rdev;

	rswitch_for_all_ports(priv, rdev) {
		unregister_netdev(rdev->ndev);
		rswitch_phy_deinit(rdev);
		rswitch_mii_unregister(rdev);
		rswitch_ndev_deinit(rdev);
	}

	rswitch_free_irqs(priv);
	rswitch_gwca_ts_queue_free(priv);
	rswitch_desc_free(priv);

	rtsn_ptp_unregister(priv->ptp_priv);

	/* Disable R-Switch clock */
	rs_write32(RCDC_RCD, priv->addr + RCDC);
}

static bool rdev_for_l2_offload(struct rswitch_device *rdev)
{
	return rdev->priv->offload_brdev &&
	       rdev->brdev == rdev->priv->offload_brdev &&
	       (rdev->priv->chan_running & BIT(rdev->port));
}

static bool rdev_for_l3_offload(struct rswitch_device *rdev)
{
	return (rdev->priv->chan_running & BIT(rdev->port)) &&
	       !netdev_has_any_upper_dev(rdev->ndev);
}

static void rswitch_update_l2_offload(struct rswitch_private *priv)
{
	struct rswitch_device *rdev;
	unsigned int all_ports_mask, fwd_mask;
	bool learning_needed, forwarding_needed;

	/* calculate fwd_mask with zeroes in bits corresponding to ports that
	 * shall participate in hardware forwarding */
	all_ports_mask = GENMASK(RSWITCH_NUM_HW - 1, 0);
	fwd_mask = all_ports_mask;
	rswitch_for_all_ports(priv, rdev) {
		if (rdev_for_l2_offload(rdev) && rdev->forwarding_requested)
			fwd_mask &= ~BIT(rdev->port);
	}

	rswitch_for_all_ports(priv, rdev) {
		if (rdev_for_l2_offload(rdev)) {
			learning_needed = rdev->learning_requested;
			forwarding_needed = rdev->forwarding_requested;
		} else {
			learning_needed = false;
			forwarding_needed = false;
		}

		if (!rdev->learning_offloaded && learning_needed) {
			rswitch_modify(priv->addr, FWPC0(rdev->port),
				0,
				FWPC0_MACSSA | FWPC0_MACHLA | FWPC0_MACHMA);

			rdev->learning_offloaded = true;
			netdev_info(rdev->ndev, "starting hw learning\n");
		}
		if (rdev->learning_offloaded && !learning_needed) {
			rswitch_modify(priv->addr, FWPC0(rdev->port),
				FWPC0_MACSSA | FWPC0_MACHLA | FWPC0_MACHMA,
				0);

			rdev->learning_offloaded = false;
			netdev_info(rdev->ndev, "stopping hw learning\n");
		}

		if (forwarding_needed) {
			/* Update allowed offload destinations even for ports
			 * with l2 offload enabled earlier.
			 *
			 * Do not allow L2 forwarding to self for hw port. */
			rs_write32(FIELD_PREP(FWCP2_LTWFW_MASK,
					      fwd_mask | BIT(rdev->port)),
					priv->addr + FWPC2(rdev->port));

			if (!rdev->forwarding_offloaded) {
				rswitch_modify(priv->addr, FWPC0(rdev->port),
					0,
					FWPC0_MACSDA);

				rdev->forwarding_offloaded = true;
				netdev_info(rdev->ndev,
						"starting hw forwarding\n");
			}
		} else if (rdev->forwarding_offloaded) {
			rs_write32(FIELD_PREP(FWCP2_LTWFW_MASK, all_ports_mask),
					priv->addr + FWPC2(rdev->port));

			rswitch_modify(priv->addr, FWPC0(rdev->port),
				FWPC0_MACSDA,
				0);

			rdev->forwarding_offloaded = false;
			netdev_info(rdev->ndev, "stopping hw forwarding\n");
		}
	}
}

static void rswitch_update_offload_brdev(struct rswitch_private *priv)
{
	struct rswitch_device *rdev, *rdev2;
	struct net_device *offload_brdev = NULL;

	rswitch_for_all_ports(priv, rdev) {
		if (!rdev->brdev)
			continue;
		rswitch_for_all_ports(priv, rdev2) {
			if (rdev2 == rdev)
				break;
			if (rdev2->brdev == rdev->brdev) {
				offload_brdev = rdev->brdev;
				break;
			}
		}
		if (offload_brdev)
			break;
	}

	if (offload_brdev == priv->offload_brdev)
		return;

	if (offload_brdev && !priv->offload_brdev)
		dev_info(&priv->pdev->dev, "starting l2 offload for %s\n",
				netdev_name(offload_brdev));
	else if (!offload_brdev && priv->offload_brdev)
		dev_info(&priv->pdev->dev, "stopping l2 offload for %s\n",
				netdev_name(priv->offload_brdev));
	else
		dev_info(&priv->pdev->dev,
				"changing l2 offload from %s to %s\n",
				netdev_name(priv->offload_brdev),
				netdev_name(offload_brdev));

	priv->offload_brdev = offload_brdev;

	/* FIXME: drop hw l2 table on bridge change? */

	rswitch_update_l2_offload(priv);
}

static void rswitch_update_l3_offload(struct rswitch_private *priv)
{
	struct rswitch_device *rdev;
	unsigned int all_ports_mask, l3_ports_count, l3_ports_mask;

	all_ports_mask = GENMASK(RSWITCH_NUM_HW - 1, 0);

	l3_ports_count = 0;
	l3_ports_mask = all_ports_mask;
	rswitch_for_all_ports(priv, rdev) {
		if (rdev_for_l3_offload(rdev)) {
			l3_ports_count++;
			l3_ports_mask &= ~BIT(rdev->port);
		}
	}

	rswitch_for_all_ports(priv, rdev) {
#define FWPC0_L3_MASK (FWPC0_LTHTA | \
		       FWPC0_IP4UE | FWPC0_IP4TE | FWPC0_IP4OE)
		if (rdev_for_l3_offload(rdev) && l3_ports_count >= 2) {
			/* Update allowed offload destinations even for ports
			 * with l3 offload enabled earlier.
			 *
			 * Allow offload routing to self for hw port. */
			rswitch_modify(priv->addr, FWPC1(rdev->port),
				FWCP1_LTHFW_MASK,
				FIELD_PREP(FWCP1_LTHFW_MASK, l3_ports_mask));
			if (!rdev->l3_offload_enabled) {
				rswitch_modify(priv->addr, FWPC0(rdev->port),
					0,
					FWPC0_L3_MASK);
				rdev->l3_offload_enabled = 1;
				netdev_info(rdev->ndev, "starting l3 offload\n");
			}
		} else if (rdev->l3_offload_enabled) {
			rswitch_modify(priv->addr, FWPC0(rdev->port),
				FWPC0_L3_MASK,
				0);
			rswitch_modify(priv->addr, FWPC1(rdev->port),
				FWCP1_LTHFW_MASK,
				FIELD_PREP(FWCP1_LTHFW_MASK, all_ports_mask));

			rdev->l3_offload_enabled = 0;

			/* cleanup any offloads at disable */
			rdev_cleanup_ipv4_offloads_all(rdev);

			netdev_info(rdev->ndev, "stopping l3 offload\n");
		}
#undef FWPC0_L3_MASK
	}
}

static void rdev_join_bridge(struct rswitch_device *rdev,
			    struct net_device *brdev)
{
	rdev->brdev = brdev;

	rswitch_update_offload_brdev(rdev->priv);
}

static void rdev_leave_bridge(struct rswitch_device *rdev)
{
	rdev->brdev = NULL;

	rswitch_update_offload_brdev(rdev->priv);
}

static void rdev_update_stp_state(struct rswitch_device *rdev, u8 stp_state)
{
	rdev->learning_requested = (stp_state == BR_STATE_LEARNING ||
				    stp_state == BR_STATE_FORWARDING);
	rdev->forwarding_requested = (stp_state == BR_STATE_FORWARDING);

	rswitch_update_l2_offload(rdev->priv);
}

static int rswitch_netdevice_event(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	struct net_device *ndev = netdev_notifier_info_to_dev(ptr);
	struct rswitch_device *rdev;
	struct netdev_notifier_changeupper_info *info;

	if (!rswitch_port_check(ndev))
		return NOTIFY_DONE;
	rdev = netdev_priv(ndev);

	if (event != NETDEV_CHANGEUPPER)
		return NOTIFY_DONE;
	info = ptr;

	if (!netif_is_bridge_master(info->upper_dev))
		return NOTIFY_DONE;

	if (info->linking)
		rdev_join_bridge(rdev, info->upper_dev);
	else
		rdev_leave_bridge(rdev);

	rswitch_update_l3_offload(rdev->priv);

	return NOTIFY_OK;
}

static int rswitch_port_attr_set(struct net_device *ndev,
				 const struct switchdev_attr *attr,
				 struct switchdev_trans *trans)
{
	struct rswitch_device *rdev = netdev_priv(ndev);

	switch (attr->id) {
	case SWITCHDEV_ATTR_ID_PORT_STP_STATE:
		if (switchdev_trans_ph_commit(trans))
			rdev_update_stp_state(rdev, attr->u.stp_state);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static int rswitch_port_obj_add(struct net_device *ndev,
				const struct switchdev_obj *obj,
				struct switchdev_trans *trans,
				struct netlink_ext_ack *extack)
{
	return -EOPNOTSUPP;
}

static int rswitch_port_obj_del(struct net_device *ndev,
				const struct switchdev_obj *obj)
{
	return -EOPNOTSUPP;
}

static int rswitch_switchdev_event(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	struct net_device *ndev = switchdev_notifier_info_to_dev(ptr);
	int ret;

	if (event == SWITCHDEV_PORT_ATTR_SET) {
		ret = switchdev_handle_port_attr_set(ndev, ptr,
						     rswitch_port_check,
						     rswitch_port_attr_set);
		return notifier_from_errno(ret);
	}

	if (!rswitch_port_check(ndev))
		return NOTIFY_DONE;
	return notifier_from_errno(-EOPNOTSUPP);
}

static int rswitch_switchdev_blocking_event(struct notifier_block *nb,
					    unsigned long event, void *ptr)
{
	struct net_device *ndev = switchdev_notifier_info_to_dev(ptr);
	int ret;

	switch (event) {
	case SWITCHDEV_PORT_OBJ_ADD:
		ret = switchdev_handle_port_obj_add(ndev, ptr,
						    rswitch_port_check,
						    rswitch_port_obj_add);
		break;
	case SWITCHDEV_PORT_OBJ_DEL:
		ret = switchdev_handle_port_obj_del(ndev, ptr,
						    rswitch_port_check,
						    rswitch_port_obj_del);
		break;
	case SWITCHDEV_PORT_ATTR_SET:
		ret = switchdev_handle_port_attr_set(ndev, ptr,
						     rswitch_port_check,
						     rswitch_port_attr_set);
		break;
	default:
		if (!rswitch_port_check(ndev))
			return NOTIFY_DONE;
		ret = -EOPNOTSUPP;
	}

	return notifier_from_errno(ret);
}

static struct notifier_block rswitch_netdevice_nb = {
	.notifier_call = rswitch_netdevice_event,
};
static struct notifier_block rswitch_switchdev_nb = {
	.notifier_call = rswitch_switchdev_event,
};
static struct notifier_block rswitch_switchdev_blocking_nb = {
	.notifier_call = rswitch_switchdev_blocking_event,
};

static atomic_t rswitch_notifier_register_count;

static int rswitch_register_notifiers(void)
{
	int ret;

	if (atomic_inc_return(&rswitch_notifier_register_count) != 1)
		return 0;

	ret = register_netdevice_notifier(&rswitch_netdevice_nb);
	if (ret)
		goto register_netdevice_notifier_failed;

	ret = register_switchdev_notifier(&rswitch_switchdev_nb);
	if (ret)
		goto register_switchdev_notifier_failed;

	ret = register_switchdev_blocking_notifier(&rswitch_switchdev_blocking_nb);
	if (ret)
		goto register_switchdev_blocking_notifier_failed;

	return 0;

register_switchdev_blocking_notifier_failed:
	unregister_switchdev_notifier(&rswitch_switchdev_nb);
register_switchdev_notifier_failed:
	unregister_netdevice_notifier(&rswitch_netdevice_nb);
register_netdevice_notifier_failed:
	atomic_dec(&rswitch_notifier_register_count);

	return ret;
}

static void rswitch_unregister_notifiers(void)
{
	if (atomic_dec_return(&rswitch_notifier_register_count) == 0) {
		unregister_switchdev_blocking_notifier(&rswitch_switchdev_blocking_nb);
		unregister_switchdev_notifier(&rswitch_switchdev_nb);
		unregister_netdevice_notifier(&rswitch_netdevice_nb);
	}
}


static bool rswitch_l23update_matches_spec(struct rswitch_l23update *update,
		struct rswitch_l23update_spec *spec)
{
	return ether_addr_equal(update->spec.dst_mac, spec->dst_mac);
}

static bool rswitch_l23update_hw_op(struct rswitch_private *priv,
		struct rswitch_l23update *update, bool install)
{
	u8 *dst_mac = update->spec.dst_mac;
	u32 val;
	int ret;

	val = FIELD_PREP(FWL23URL0_RN, update->index) |
	      FIELD_PREP(FWL23URL0_PV,
			      install ? GENMASK(RSWITCH_NUM_HW - 1, 0) : 0);
	rs_write32(val, priv->addr + FWL23URL0);

	val = FWL23URL1_TTLU |
	      FWL23URL1_MSAU |
	      FWL23URL1_MDAU |
	      (dst_mac[0] << 8) | (dst_mac[1] << 0);
	rs_write32(val, priv->addr + FWL23URL1);

	val = (dst_mac[2] << 24) | (dst_mac[3] << 16) |
	      (dst_mac[4] << 8)  | (dst_mac[5] << 0);
	rs_write32(val, priv->addr + FWL23URL2);

	val = 0;
	rs_write32(val, priv->addr + FWL23URL3);

	/* ... rule write starts after writing to FWL23URL3 */

	ret = rswitch_reg_wait(priv->addr, FWL23URLR, FWL23URLR_L, 0);
	if (ret) {
		dev_err(&priv->pdev->dev, "timeout writing l23_update\n");
		return false;
	}
	val = rs_read32(priv->addr + FWL23URLR) &
			(FWL23URLR_LSF | FWL23URLR_LF);
	if (val) {
		dev_err(&priv->pdev->dev,
			"writing l23_update failed (err %d)\n", val);
		return false;
	}

	return true;
}

static bool rswitch_ipv4_dst_offload_hw_op(struct rswitch_device *rdev,
		struct rswitch_ipv4_dst_offload *offload,
		u8 frame_type, bool install)
{
	struct rswitch_private *priv = rdev->priv;
	u32 val, err_flags, collisions;
	int ret;
	const char *frame_type_name[] = {
		[RSWITCH_FRAME_TYPE_IPV4_TCP] = "TCP",
		[RSWITCH_FRAME_TYPE_IPV4_UDP] = "UDP",
		[RSWITCH_FRAME_TYPE_IPV4_OTHER] = "OTHER",
	};

	spin_lock(&priv->l3_lock);

	val = (install ? 0 : FWLTHTL0_ED) | frame_type;
	rs_write32(val, priv->addr + FWLTHTL0);

	rs_write32(0, priv->addr + FWLTHTL1);
	rs_write32(0, priv->addr + FWLTHTL2);
	rs_write32(0, priv->addr + FWLTHTL3);
	rs_write32(be32_to_cpu(offload->addr), priv->addr + FWLTHTL4);

	/* accept from all ports in the table entry
	 * (non-particilating ports have l3 table disabled) */
	val = FIELD_PREP(FWLTHTL7_SLV, GENMASK(RSWITCH_NUM_HW - 1, 0)) |
	      FWLTHTL7_RV |
	      FIELD_PREP(FWLTHTL7_RN, offload->update->index);
	rs_write32(val, priv->addr + FWLTHTL7);

	rs_write32(0, priv->addr + FWLTHTL8(0));
	rs_write32(0, priv->addr + FWLTHTL8(1));

	val = FIELD_PREP(FWLTHTL9_DV, BIT(rdev->port));
	rs_write32(val, priv->addr + FWLTHTL9);

	/* ... table entry write starts after writing to FWLTHTL9 */

	ret = rswitch_reg_wait(priv->addr, FWLTHTLR, FWLTHTLR_L, 0);
	val = rs_read32(priv->addr + FWLTHTLR);
	err_flags = val & (FWLTHTLR_LEF | FWLTHTLR_LSF | FWLTHTLR_LF);
	collisions = FIELD_GET(FWLTHTLR_LCN, val);

	spin_unlock(&priv->l3_lock);

	if (ret) {
		dev_err(&priv->pdev->dev,
			"timeout writing l3 table entry\n");
		return false;
	}

	if (err_flags) {
		dev_err(&priv->pdev->dev,
			"writing l3 table entry failed (flags %d)\n",
			err_flags);
		return false;
	}

	if (install)
		offload->types_offloaded |= BIT(frame_type);
	else
		offload->types_offloaded &= ~BIT(frame_type);

	dev_info(&priv->pdev->dev,
		"%s IPv4/%s forwarding %pI4b -> tsn%u, mac %pM; table collisions: %u\n",
		install ? "added" : "removed",
		frame_type_name[frame_type],
		&offload->addr,
		rdev->port,
		offload->update->spec.dst_mac,
		collisions);
	return true;
}

static struct rswitch_l23update *rswitch_get_l23update(
		struct rswitch_private *priv,
		struct rswitch_l23update_spec *spec)
{
	struct rswitch_l23update *update;

	spin_lock(&priv->l3_lock);

	list_for_each_entry(update, &priv->l23_update_list, list) {
		if (rswitch_l23update_matches_spec(update, spec)) {
			update->use_count++;
			goto out;
		}
	}

	update = kzalloc(sizeof(*update), GFP_ATOMIC);
	if (!update)
		goto out;

	update->use_count = 1;
	update->spec = *spec;
	update->index = find_first_zero_bit(priv->l23_update_bitmap,
			RSWITCH_MAX_NUM_RRULE);
	if (update->index == RSWITCH_MAX_NUM_RRULE) {
		dev_err_ratelimited(&priv->pdev->dev,
				"out of l23_update entries\n");
		/* FIXME: trigger expire? */
		goto no_free_bit;
	}
	set_bit(update->index, priv->l23_update_bitmap);

	if (!rswitch_l23update_hw_op(priv, update, true))
		goto hw_op_failed;

	list_add(&update->list, &priv->l23_update_list);
out:
	spin_unlock(&priv->l3_lock);

	return update;

hw_op_failed:
	clear_bit(update->index, priv->l23_update_bitmap);
no_free_bit:
	kfree(update);
	update = NULL;
	goto out;
}

static void rswitch_put_l23update(struct rswitch_private *priv,
		struct rswitch_l23update *update)
{
	spin_lock(&priv->l3_lock);

	update->use_count--;
	if (update->use_count == 0) {
		list_del(&update->list);
		rswitch_l23update_hw_op(priv, update, false);
		clear_bit(update->index, priv->l23_update_bitmap);
		kfree(update);
	}

	spin_unlock(&priv->l3_lock);
}

static inline bool addr4_in_range(__be32 addr,
		__be32 range_addr, __be32 range_mask)
{
	return (addr & range_mask) == (range_addr & range_mask);
}

static inline bool addr4_ranges_intersect(__be32 addr1, __be32 mask1,
		__be32 addr2, __be32 mask2)
{
	/* Two addr/mask ranges intersect when their addrs ANDed
	 * with wider mask (=one with more zeroes) are the same */
	__be32 wider_mask = mask1 & mask2;
	return (addr1 & wider_mask) == (addr2 & wider_mask);
}

/* expected call under rswitch_device lock */
static inline bool addr4_in_exception_list(__be32 addr, struct list_head *list)
{
	struct rswitch_ipv4_route_exception *entry;

	list_for_each_entry(entry, list, list) {
		if (entry->addr == addr)
			return true;
	}

	return false;
}

/* called under rswitch_device lock */
static struct rswitch_ipv4_route *rdev_lookup_ipv4_ucast_route(
		struct rswitch_device *rdev, __be32 addr)
{
	struct rswitch_ipv4_route *route;

	list_for_each_entry(route, &rdev->ipv4_ucast_route_list, list) {
		if (addr4_in_range(addr, route->addr, route->mask))
			return route;
	}

	return NULL;
}

/* called under rswitch_device lock */
static struct rswitch_ipv4_dst_offload *rdev_lookup_ipv4_dst_offload(
		struct rswitch_ipv4_route *route, __be32 addr)
{
	struct rswitch_ipv4_dst_offload *offload;

	list_for_each_entry(offload, &route->offload_list, list) {
		if (offload->addr == addr)
			return offload;
	}

	return NULL;
}

/* called under rswitch_device lock */
static struct rswitch_ipv4_dst_offload *rdev_add_ipv4_dst_offload(
		struct rswitch_device *rdev,
		struct rswitch_ipv4_route *route,
		__be32 addr,
		struct rswitch_l23update_spec *spec)
{
	struct rswitch_ipv4_dst_offload *offload;

	offload = kzalloc(sizeof(*offload), GFP_ATOMIC);
	if (!offload)
		return NULL;

	offload->update = rswitch_get_l23update(rdev->priv, spec);
	if (!offload->update) {
		kfree(offload);
		return NULL;
	}

	offload->addr = addr;
	list_add_tail(&offload->list, &route->offload_list);
	return offload;
}

/* called under rswitch_device lock */
static void rdev_remove_ipv4_dst_offload(struct rswitch_device *rdev,
		struct rswitch_ipv4_route *route,
		struct rswitch_ipv4_dst_offload *offload)
{
	unsigned long types_offloaded = offload->types_offloaded;
	u8 frame_type;

	for_each_set_bit(frame_type, &types_offloaded, BITS_PER_LONG) {
		rswitch_ipv4_dst_offload_hw_op(rdev,
				offload, frame_type, false);
	}
	if (offload->types_offloaded) {
		/* FIXME: what to do if failed? rebuild l3 table? */
		dev_err(&rdev->priv->pdev->dev,
			"further operation unreliable\n");
	}

	list_del(&offload->list);
	rswitch_put_l23update(rdev->priv, offload->update);
	kfree(offload);
}

/* called under rswitch_device lock */
static void rswitch_handle_l3_learning(struct rswitch_device *rdev,
		struct sk_buff *skb)
{
	struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
	struct iphdr *iphdr;
	__be32 src_addr, dst_addr;
	struct rswitch_ipv4_route *route;
	struct rswitch_ipv4_dst_offload *offload;
	u8 frame_type;

	if (ethhdr->h_proto != cpu_to_be16(ETH_P_IP))
		return;
	iphdr = (struct iphdr *)(ethhdr + 1);
	src_addr = iphdr->saddr;
	dst_addr = iphdr->daddr;

	/* Packets from local address are not subject for learning */
	if (addr4_in_exception_list(src_addr, &rdev->ipv4_local_route_list))
		return;

	/* Packet to local address? Why? */
	if (addr4_in_exception_list(dst_addr, &rdev->ipv4_local_route_list))
		return;

	/* Packets to broadcast destination are not subject for learning */
	if (addr4_in_exception_list(dst_addr, &rdev->ipv4_bcast_route_list))
		return;

	/* Lookup route to learn for */
	route = rdev_lookup_ipv4_ucast_route(rdev, dst_addr);
	if (!route)
		return;

	/* Packet is candidate for offload (and not offloaded yet) */

	offload = rdev_lookup_ipv4_dst_offload(route, dst_addr);
	if (offload) {
		/* TODO: verify that ethhdr->h_dest matches offload's */
	} else {
		/* TODO: verify that ethhdr->h_dest matches neighbour table
		 *       for route->gw_addr (if defined) or dst_addr */
		struct rswitch_l23update_spec spec;

		ether_addr_copy(spec.dst_mac, ethhdr->h_dest);
		offload = rdev_add_ipv4_dst_offload(rdev,
				route, dst_addr, &spec);
		if (!offload)
			return;
	}

	if (iphdr->protocol == IPPROTO_TCP)
		frame_type = RSWITCH_FRAME_TYPE_IPV4_TCP;
	else if (iphdr->protocol == IPPROTO_UDP)
		frame_type = RSWITCH_FRAME_TYPE_IPV4_UDP;
	else
		frame_type = RSWITCH_FRAME_TYPE_IPV4_OTHER;

	if (offload->types_offloaded & BIT(frame_type))
		return;

	if (!rswitch_ipv4_dst_offload_hw_op(rdev, offload, frame_type, true)) {
		/* FIXME: what to do if failed? rebuild l3 table? */
		dev_err(&rdev->priv->pdev->dev,
			"further operation unreliable\n");
	}
}


/* called under rswitch_device lock */
static void rdev_cleanup_ipv4_route_offloads(struct rswitch_device *rdev,
		struct rswitch_ipv4_route *route,
		__be32 addr, __be32 mask)
{
	struct rswitch_ipv4_dst_offload *offload, *tmp;

	list_for_each_entry_safe(offload, tmp, &route->offload_list, list) {
		if (addr4_in_range(offload->addr, addr, mask))
			rdev_remove_ipv4_dst_offload(rdev, route, offload);
	}
}

/* called under rswitch_device lock */
static void rdev_cleanup_ipv4_offloads_from(struct rswitch_device *rdev,
		struct list_head *pos,
		__be32 addr, __be32 mask)
{
	struct rswitch_ipv4_route *route;

	while (pos != &rdev->ipv4_ucast_route_list) {
		route = list_entry(pos, typeof(*route), list);
		if (addr4_ranges_intersect(addr, mask,
					route->addr, route->mask))
			rdev_cleanup_ipv4_route_offloads(rdev, route,
					addr, mask);
		pos = pos->next;
	}
}

/* called under rswitch_device lock */
static void rdev_cleanup_ipv4_offloads_addr(struct rswitch_device *rdev,
		__be32 addr)
{
	rdev_cleanup_ipv4_offloads_from(rdev,
			rdev->ipv4_ucast_route_list.next, addr, 0xffffffff);
}

static void rdev_cleanup_ipv4_offloads_all(struct rswitch_device *rdev)
{
	unsigned long flags;

	spin_lock_irqsave(&rdev->lock, flags);
	rdev_cleanup_ipv4_offloads_from(rdev,
			rdev->ipv4_ucast_route_list.next, 0, 0);
	spin_unlock_irqrestore(&rdev->lock, flags);
}

/* called under rswitch_device lock */
static void rdev_ipv4_update_route(struct rswitch_device *rdev,
		__be32 addr, __be32 mask, __be32 gw_addr, bool keep)
{
	struct rswitch_ipv4_route *route;
	struct list_head *pos;
	bool found = false;

	/* Find the route in the list, and/or location for the route */
	list_for_each(pos, &rdev->ipv4_ucast_route_list) {
		route = list_entry(pos, typeof(*route), list);
		if (route->addr == addr && route->mask == mask) {
			found = true;
			break;
		}

		/* stop if got more generic masks */
		if (route->mask != mask && (route->mask & mask) == route->mask)
			break;
	}

	if (!found && !keep)
		return;

	rdev_cleanup_ipv4_offloads_from(rdev, pos, addr, mask);

	if (found && !keep) {
		BUG_ON(!list_empty(&route->offload_list));
		list_del(&route->list);
		kfree(route);
		return;
	}

	if (!found) {
		route = kzalloc(sizeof(*route), GFP_ATOMIC);
		if (!route) {
			/* FIXME: what to do? disable l3 offload? */
			dev_err(&rdev->priv->pdev->dev,
				"allocation failure, further operation unreliable\n");
			return;
		}
		INIT_LIST_HEAD(&route->offload_list);
		list_add(&route->list, pos);
	}

	route->addr = addr;
	route->mask = mask;
	route->gw_addr = gw_addr;
}

/* called under rswitch_device lock */
static void rdev_ipv4_update_route_exception(struct rswitch_device *rdev,
		struct list_head *list, __be32 addr, bool keep)
{
	struct rswitch_ipv4_route_exception *entry;

	list_for_each_entry(entry, list, list) {
		if (entry->addr == addr) {
			if (!keep) {
				list_del(&entry->list);
				kfree(entry);
			}
			/* there is/was entry => addr is not in any offloads */
			return;
		}
	}

	if (keep) {
		entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
		if (!entry) {
			/* FIXME: what to do? disable l3 offload? */
			dev_err(&rdev->priv->pdev->dev,
				"allocation failure, further operation unreliable\n");
			return;
		}

		entry->addr = addr;
		list_add_tail(&entry->list, list);

		/* addr could be in existing offloads */
		rdev_cleanup_ipv4_offloads_addr(rdev, addr);
	}
}

static void rswitch_ipv4_fib_update(struct rswitch_private *priv,
		struct fib_notifier_info *info, bool keep)
{
	struct fib_entry_notifier_info *feni =
			container_of(info, typeof(*feni), info);
	struct fib_info *fib = feni->fi;
	struct fib_nh *fib_nh;
	struct net_device *ndev;
	struct rswitch_device *rdev = NULL;
	unsigned long flags;

	/* FIXME: what if multiple tables? */

	if (fib->fib_nhs != 1 || fib->nh != NULL) {
		/* FIXME: what to do? */
		return;
	}
	fib_nh = &fib->fib_nh[0];
	ndev = fib_nh->fib_nh_dev;

	if (!rswitch_port_check(ndev)) {
		/* not an rswitch port */
		return;
	}
	rdev = netdev_priv(ndev);
	if (rdev->priv != priv) {
		/* not this rswitch instance */
		return;
	}

	/* FIXME: the above is far incomplete against what is possible */

	spin_lock_irqsave(&rdev->lock, flags);

	if (fib->fib_type == RTN_UNICAST) {
		__be32 addr = cpu_to_be32(feni->dst);
		__be32 mask = cpu_to_be32(0xffffffff << (32 - feni->dst_len));
		__be32 gw_addr = fib_nh->fib_nh_gw4;

		netdev_info(ndev,
			"%s ucast route info: addr %pI4b mask %pI4b gw %pI4b\n",
			keep ? "set" : "unset", &addr, &mask, &gw_addr);
		rdev_ipv4_update_route(rdev, addr, mask, gw_addr, keep);
	} else if (fib->fib_type == RTN_LOCAL) {
		__be32 addr = cpu_to_be32(feni->dst);

		netdev_info(ndev, "%s local route info: addr %pI4b\n",
				keep ? "set" : "unset", &addr);
		rdev_ipv4_update_route_exception(rdev,
				&rdev->ipv4_local_route_list, addr, keep);
	} else if (fib->fib_type == RTN_BROADCAST) {
		__be32 addr = cpu_to_be32(feni->dst);

		netdev_info(ndev, "%s bcast route info: addr %pI4b\n",
				keep ? "set" : "unset", &addr);
		rdev_ipv4_update_route_exception(rdev,
				&rdev->ipv4_bcast_route_list, addr, keep);
	}

	spin_unlock_irqrestore(&rdev->lock, flags);
}

static int rswitch_fib_event(struct notifier_block *nb,
		unsigned long event, void *ptr)
{
	struct rswitch_private *priv = container_of(nb, typeof(*priv), fib_nb);
	struct fib_notifier_info *info = ptr;

	/* Handle only IPv4 for now */
	if (info->family == AF_INET) {
		if (event == FIB_EVENT_ENTRY_REPLACE)
			rswitch_ipv4_fib_update(priv, info, true);
		else if (event == FIB_EVENT_ENTRY_DEL)
			rswitch_ipv4_fib_update(priv, info, false);
	}

	return NOTIFY_DONE;
}

static void rswitch_device_create(struct rswitch_private *priv,
		struct device_node *port_dn)
{
	struct device *dev = &priv->pdev->dev;
	struct net_device *ndev;
	struct rswitch_device *rdev;
	u32 port;
	int ret;

	if (of_property_read_u32(port_dn, "reg", &port)) {
		dev_warn(dev, "%pOF: no reg property\n", port_dn);
		return;
	}

	if (port >= num_etha_ports) {
		dev_warn(dev, "%pOF: port %u is out of range (max is %d)\n",
				port_dn, port, num_etha_ports);
		return;
	}

	rswitch_for_all_ports(priv, rdev) {
		if (rdev->port == port) {
			dev_warn(dev, "%pOF: duplicate port\n", port_dn);
			return;
		}
	}

	ndev = alloc_etherdev_mqs(sizeof(*rdev), 1, 1);
	if (!ndev) {
		dev_warn(&priv->pdev->dev, "netdev allocation failed\n");
		return;
	}

	SET_NETDEV_DEV(ndev, &priv->pdev->dev);
	ndev->base_addr = (unsigned long)priv->addr;
	snprintf(ndev->name, IFNAMSIZ, "tsn%d", port);
	ndev->netdev_ops = &rswitch_netdev_ops;
	ndev->ethtool_ops = &rswitch_ethtool_ops;
	ndev->max_mtu = MAX_MTU_SZ;

	rdev = netdev_priv(ndev);
	rdev->port_dn = port_dn;
	rdev->port = port;
	rdev->priv = priv;
	rdev->ndev = ndev;

	rdev->port_dn = of_node_get(port_dn);
	rdev->port = port;

	spin_lock_init(&rdev->lock);

	INIT_LIST_HEAD(&rdev->ipv4_local_route_list);
	INIT_LIST_HEAD(&rdev->ipv4_bcast_route_list);
	INIT_LIST_HEAD(&rdev->ipv4_ucast_route_list);

	rdev->phy_dn = of_parse_phandle(port_dn, "phy-handle", 0);
	if (!rdev->phy_dn && of_phy_is_fixed_link(port_dn)) {
		ret = of_phy_register_fixed_link(port_dn);
		if (ret) {
			dev_warn(dev,
				"could not register fixed_link for port %d\n",
				port);
			goto register_fixed_link_failed;
		}
		rdev->phy_dn = of_node_get(port_dn);
		rdev->phy_dn_is_fixed_link = true;
	}
	if (!rdev->phy_dn) {
		dev_err(dev, "no phy defined for port %d\n", port);
		goto no_phy_dn;
	}

	/* FIXME: to remove */
	rswitch_etha_init(rdev);

	list_add_tail(&rdev->list, &priv->port_list);
	return;

no_phy_dn:
register_fixed_link_failed:
	free_netdev(ndev);
	return;
}

static void rdev_destroy_ipv4_route_lists(struct rswitch_device *rdev)
{
	struct rswitch_ipv4_route *route, *tmp_route;
	struct rswitch_ipv4_route_exception *exception, *tmp_expection;

	/* Per experiment, on module unload path the route lists are not
	 * cleaned up via fib events. Thus have to clean explicitly.
	 *
	 * This runs when fib_notifier is already unregistered, which includes
	 * synchronization. No parallel route lists manupulation shuld be
	 * possible. Thus no locing needed.
	 */

	list_for_each_entry_safe(exception, tmp_expection,
			&rdev->ipv4_local_route_list, list) {
		list_del(&exception->list);
		kfree(exception);
	}

	list_for_each_entry_safe(exception, tmp_expection,
			&rdev->ipv4_bcast_route_list, list) {
		list_del(&exception->list);
		kfree(exception);
	}

	list_for_each_entry_safe(route, tmp_route,
			&rdev->ipv4_ucast_route_list, list) {
		/* offloads must have been cleared at netdev close time */
		WARN_ON_ONCE(!list_empty(&route->offload_list));

		list_del(&route->list);
		kfree(exception);
	}
}

static void rswitch_device_destroy(struct rswitch_device *rdev)
{
	list_del(&rdev->list);

	rdev_destroy_ipv4_route_lists(rdev);

	if (rdev->phy_dn_is_fixed_link)
		of_phy_deregister_fixed_link(rdev->phy_dn);

	of_node_put(rdev->phy_dn);
	of_node_put(rdev->port_dn);

	free_netdev(rdev->ndev);
}

static int renesas_eth_sw_probe(struct platform_device *pdev)
{
	struct rswitch_private *priv;
	struct resource *res, *res_serdes;
	struct device_node *ports_dn, *port_dn;
	struct rswitch_device *rdev;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res_serdes = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res || !res_serdes) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	spin_lock_init(&priv->lock);

	spin_lock_init(&priv->l3_lock);
	INIT_LIST_HEAD(&priv->l23_update_list);

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

	priv->ptp_priv = rtsn_ptp_alloc(pdev);
	if (!priv->ptp_priv)
		return -ENOMEM;

	if (!parallel_mode)
		parallel_mode = of_property_read_bool(pdev->dev.of_node, "parallel_mode");

	if (parallel_mode)
		num_etha_ports = 1;

	priv->ptp_priv->parallel_mode = parallel_mode;

	if (!parallel_mode) {
		priv->rsw_clk = devm_clk_get(&pdev->dev, "rsw2");
		if (IS_ERR(priv->rsw_clk)) {
			dev_err(&pdev->dev, "Failed to get rsw2 clock: %ld\n",
				PTR_ERR(priv->rsw_clk));
			return -PTR_ERR(priv->rsw_clk);
		}

		priv->phy_clk = devm_clk_get(&pdev->dev, "eth-phy");
		if (IS_ERR(priv->phy_clk)) {
			dev_err(&pdev->dev, "Failed to get eth-phy clock: %ld\n",
				PTR_ERR(priv->phy_clk));
			return -PTR_ERR(priv->phy_clk);
		}
	}

	priv->sd_rst = devm_reset_control_get(&pdev->dev, "eth-phy");
	if (IS_ERR(priv->sd_rst))
		return PTR_ERR(priv->sd_rst);

	platform_set_drvdata(pdev, priv);
	priv->pdev = pdev;
	priv->addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->addr))
		return PTR_ERR(priv->addr);

	priv->ptp_priv->addr = priv->addr + RSWITCH_GPTP_OFFSET;
	priv->serdes_addr = devm_ioremap_resource(&pdev->dev, res_serdes);
	if (IS_ERR(priv->serdes_addr))
		return PTR_ERR(priv->serdes_addr);

	debug_addr = priv->addr;
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(40));
	if (ret < 0) {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (ret < 0)
			return ret;
	}

	/* Fixed to use GWCA1 */
	priv->gwca.index = 4;
	priv->gwca.num_chains = num_etha_ports * NUM_CHAINS_PER_NDEV;
	priv->gwca.chains = devm_kcalloc(&pdev->dev, priv->gwca.num_chains,
					 sizeof(*priv->gwca.chains), GFP_KERNEL);
	if (!priv->gwca.chains)
		return -ENOMEM;

	INIT_LIST_HEAD(&priv->port_list);
	ports_dn = of_get_child_by_name(priv->pdev->dev.of_node, "ports");
	if (!ports_dn) {
		dev_err(&pdev->dev, "no ports subnode\n");
		return -EINVAL;
	}
	for_each_available_child_of_node(ports_dn, port_dn) {
		rswitch_device_create(priv, port_dn);
	}
	of_node_put(ports_dn);

	if (list_empty(&priv->port_list))
		dev_warn(&pdev->dev, "could not initialize any ports\n");

	if (!parallel_mode) {
		pm_runtime_enable(&pdev->dev);
		pm_runtime_get_sync(&pdev->dev);
		clk_prepare(priv->phy_clk);
		clk_enable(priv->phy_clk);
	}

	ret = rswitch_init(priv);
	if (ret)
		goto rswitch_init_failed;

	if (!parallel_mode) {
		ret = rswitch_register_notifiers();
		if (ret)
			goto rswitch_register_notifiers_failed;

		priv->fib_nb.notifier_call = rswitch_fib_event;
		ret = register_fib_notifier(&init_net, &priv->fib_nb, NULL, NULL);
		priv->fib_nb_registered = (ret == 0);
	}

	device_set_wakeup_capable(&pdev->dev, 1);

	return 0;

rswitch_register_notifiers_failed:
	rswitch_deinit(priv);

rswitch_init_failed:

	if (!parallel_mode) {
		clk_disable(priv->phy_clk);
		clk_unprepare(priv->phy_clk);
		pm_runtime_put(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
	}

	while (!list_empty(&priv->port_list)) {
		rdev = list_first_entry(&priv->port_list, typeof(*rdev), list);
		rswitch_device_destroy(rdev);
	}

	return ret;
}

static int renesas_eth_sw_remove(struct platform_device *pdev)
{
	struct rswitch_private *priv = platform_get_drvdata(pdev);
	struct rswitch_device *rdev;

	if (!parallel_mode) {
		if (priv->fib_nb_registered)
			unregister_fib_notifier(&init_net, &priv->fib_nb);

		rswitch_unregister_notifiers();
	}

	rswitch_deinit(priv);

	if (!parallel_mode) {
		clk_disable(priv->phy_clk);
		clk_unprepare(priv->phy_clk);
		pm_runtime_put(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
	}

	while (!list_empty(&priv->port_list)) {
		rdev = list_first_entry(&priv->port_list, typeof(*rdev), list);
		rswitch_device_destroy(rdev);
	}

	return 0;
}

static int __maybe_unused rswitch_suspend(struct device *dev)
{
	struct rswitch_private *priv = dev_get_drvdata(dev);
	struct rswitch_device *rdev;

	rswitch_for_all_ports(priv, rdev) {
		struct net_device *ndev = rdev->ndev;

		if (netif_running(ndev)) {
			netif_stop_subqueue(ndev, 0);
			rswitch_stop(ndev);
		}

		rswitch_txdmac_free(ndev, priv);
		rswitch_rxdmac_free(ndev, priv);
	}

	priv->serdes_common_init = false;
	rtsn_ptp_unregister(priv->ptp_priv);
	rswitch_gwca_ts_queue_free(priv);
	rswitch_desc_free(priv);

	return 0;
}

static int rswitch_resume_chan(struct net_device *ndev)
{
	struct rswitch_device *rdev = netdev_priv(ndev);
	int ret;

	ret = rswitch_etha_hw_init(rdev);
	if (ret)
		return ret;

	ret = rswitch_rxdmac_init(ndev, rdev->priv);
	if (ret)
		goto out_dmac;

	ret = rswitch_txdmac_init(ndev, rdev->priv);
	if (ret) {
		rswitch_rxdmac_free(ndev, rdev->priv);
		goto out_dmac;
	}

	if (netif_running(ndev)) {
		ret = rswitch_open(ndev);
		if (ret)
			goto error;
	}

	return 0;

error:
	rswitch_txdmac_free(ndev, rdev->priv);
	rswitch_rxdmac_free(ndev, rdev->priv);
out_dmac:
	/* Workround that still gets two chains (rx, tx)
	 * to allow the next channel, if any, to restore
	 * the correct index of chains.
	 */
	rswitch_gwca_get(rdev->priv);
	rswitch_gwca_get(rdev->priv);
	rdev->tx_chain->index = -1;

	return ret;
}

static int __maybe_unused rswitch_resume(struct device *dev)
{
	struct rswitch_private *priv = dev_get_drvdata(dev);
	struct rswitch_device *rdev;
	int ret, resumed = 0;

	ret = rswitch_desc_alloc(priv);
	if (ret)
		return ret;

	ret = rswitch_gwca_ts_queue_alloc(priv);
	if (ret) {
		rswitch_desc_free(priv);
		return ret;
	}

	rswitch_gwca_ts_queue_fill(priv, 0, TS_RING_SIZE);
	INIT_LIST_HEAD(&priv->gwca.ts_info_list);

	if (!parallel_mode)
		rswitch_clock_enable(priv);

	ret = rswitch_gwca_hw_init(priv);
	if (ret)
		return ret;

	if (!parallel_mode) {
		rswitch_for_all_ports(priv, rdev) {
			ret = rswitch_etha_hw_init(rdev);
			if (ret < 0)
				return ret;
		}

		ret = rswitch_bpool_config(priv);
		if (ret)
			return ret;

		ret = rswitch_fwd_init(priv);
		if (ret)
			return ret;
	}

	ret = rtsn_ptp_init(priv->ptp_priv, RTSN_PTP_REG_LAYOUT_S4, RTSN_PTP_CLOCK_S4);
	if (ret)
		return ret;

	rswitch_for_all_ports(priv, rdev) {
		struct net_device *ndev = rdev->ndev;

		ret = rswitch_resume_chan(ndev);
		if (ret) {
			pr_info("Failed to resume %s", ndev->name);
		} else {
			resumed++;
		}
	}

	if (resumed == 0) {
		rswitch_gwca_ts_queue_free(priv);
		rswitch_desc_free(priv);

		return -ENXIO;
	}

	return 0;
}

static int __maybe_unused rswitch_runtime_nop(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops rswitch_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rswitch_suspend, rswitch_resume)
	SET_RUNTIME_PM_OPS(rswitch_runtime_nop, rswitch_runtime_nop, NULL)
};

static struct platform_driver renesas_eth_sw_driver_platform = {
	.probe = renesas_eth_sw_probe,
	.remove = renesas_eth_sw_remove,
	.driver = {
		.name	= "renesas_eth_sw",
		.pm	= &rswitch_dev_pm_ops,
		.of_match_table = renesas_eth_sw_of_table,
	}
};
module_platform_driver(renesas_eth_sw_driver_platform);
MODULE_AUTHOR("Yoshihiro Shimoda");
MODULE_DESCRIPTION("Renesas Ethernet Switch device driver");
MODULE_LICENSE("GPL v2");
