// SPDX-License-Identifier: GPL-2.0-or-later
/* PTP 1588 clock for Renesas R Switch2 header
 *
 * Copyright (C) 2014 Renesas Electronics Corporation
 */

#ifndef __RSWITCH2_PTP_H__
#define __RSWITCH2_PTP_H__

#include <linux/ptp_clock_kernel.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define RSWITCH2PTP_DRIVER_VERSION "0.0.1"

struct ptp_rswitch2 {
	void __iomem *addr_iomem_gptp;
	spinlock_t lock; /* protects regs */
	struct ptp_clock *clock;
	struct ptp_clock_info caps;
	u32 default_addend;
	int irq;
};

/* Bit definitions for the GPTI register */
#define TIV_MASK                0xFFFFFFFF
#define TIV_MASK_OLD            0x0FFFFFFF
#define GPTP_CLK_DEV            1000001

#endif /* __RSWITCH2_PTP_H__ */
