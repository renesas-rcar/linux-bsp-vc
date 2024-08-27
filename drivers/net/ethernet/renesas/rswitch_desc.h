// SPDX-License-Identifier: GPL-2.0

#ifndef RSWITCH_DESC_H
#define RSWITCH_DESC_H

struct rswitch_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	u8 dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
} __packed;

struct rswitch_ts_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	u8 dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
	__le32 ts_nsec;
	__le32 ts_sec;
} __packed;

struct rswitch_ext_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	u8 dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
	__le64 info1;
} __packed;

struct rswitch_ext_ts_desc {
	__le16 info_ds;	/* Descriptor size */
	u8 die_dt;	/* Descriptor interrupt enable and type */
	u8 dptrh;	/* Descriptor pointer MSB */
	__le32 dptrl;	/* Descriptor pointer LSW */
	__le64 info1;
	__le32 ts_nsec;
	__le32 ts_sec;
} __packed;

#define RSWITCH_MAX_DESC_SZ	2048

/* FIXME */
#define RSWITCH_RX_BUF_ALIGN	128

#define RSWITCH_RX_BUF_SIZE	(NET_SKB_PAD + NET_IP_ALIGN \
				 + RSWITCH_MAX_DESC_SZ \
				 + SKB_DATA_ALIGN(sizeof(struct skb_shared_info)) \
				 + RSWITCH_RX_BUF_ALIGN)

enum {
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

enum {
	RX_DS	= 0x0fff, /* Data size */
	RX_TR	= 0x1000, /* Truncation indication */
	RX_EI	= 0x2000, /* Error indication */
	RX_PS	= 0xc000, /* Padding selection */
};

enum {
	TX_DS	= 0x0fff, /* Data size */
	TX_TAGL	= 0xf000, /* Frame tag LSBs */
};

#define INFO1_FMT               BIT(2)
#define INFO1_TXC		BIT(3)
#define INFO1_TSUN(val)		((u64)(val) << 8ULL)
#define INFO1_IPV(prio)		((u64)(prio) << 28ULL)
#define INFO1_DV(port_vector)	((u64)(port_vector) << 48ULL)

/* For timestamp descriptor in dptrl (Byte 4 to 7) */
#define TS_DESC_TSUN(dptrl)	((dptrl) & GENMASK(7, 0))
#define TS_DESC_SPN(dptrl)	(((dptrl) & GENMASK(10, 8)) >> 8)
#define TS_DESC_DPN(dptrl)	(((dptrl) & GENMASK(17, 16)) >> 16)
#define TS_DESC_TN(dptrl)	((dptrl) & BIT(24))

#endif
