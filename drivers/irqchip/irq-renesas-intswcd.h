// SPDX-License-Identifier: GPL-2.0

#ifndef _IRQ_RENESAS_INTSWCD_H
#define _IRQ_RENESAS_INTSWCD_H

#include <linux/types.h>

enum {
	INTSWCD_LEVEL = 1,
	INTSWCD_EDGE
};

enum {
	INTSWCD_QUIRK_NONE = 0,
	INTSWCD_QUIRK_EXT_LVL
};

struct intswcd_hwirq_info {
	unsigned int type;
	unsigned int parent_irq_offset;
	unsigned int reg;
	u32 mask;
	unsigned int quirk;
};

struct intswcd_info {
	const struct intswcd_hwirq_info *ihi;
	unsigned int ihi_nr;
	unsigned int level_parent_irqs_nr;
	unsigned int edge_parent_irqs_nr;
	unsigned int level_sts_regs_base;
	unsigned int level_msk_regs_base;
	unsigned int level_regs_nr;
	unsigned int edge_sts_regs_base;
	unsigned int edge_msk_regs_base;
	unsigned int edge_clr_regs_base;
	unsigned int edge_regs_nr;
};

extern const struct intswcd_info r8a779f0_ii;

#endif /* _IRQ_RENESAS_INTSWCD_H */
