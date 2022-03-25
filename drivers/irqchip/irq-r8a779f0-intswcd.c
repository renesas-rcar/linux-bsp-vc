// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include "irq-renesas-intswcd.h"
#include <dt-bindings/interrupt-controller/renesas-intswcd-r8a779f0.h>

static const struct intswcd_hwirq_info r8a779f0_ihi[] = {
#define ENTRY(_name, _type, _irq_offset, _reg, _bit)	\
	[R8A779F0_INTSWCD_##_name] = {			\
		.type = INTSWCD_##_type,		\
		.parent_irq_offset = _irq_offset,	\
		.reg = _reg,				\
		.mask = BIT(_bit),			\
	},

	ENTRY(INTRIIC0EE,  LEVEL, 33,  9, 0)
	ENTRY(INTRIIC0TEI, LEVEL, 33,  9, 1)
	ENTRY(INTRIIC0RI,   EDGE, 24, 13, 0)
	ENTRY(INTRIIC0TI,   EDGE, 24, 13, 1)
	ENTRY(INTOSTM0TINT, EDGE, 12,  2, 0)

#undef ENTRY
};

const struct intswcd_info r8a779f0_ii = {
	.ihi = r8a779f0_ihi,
	.ihi_nr = ARRAY_SIZE(r8a779f0_ihi),
	.level_parent_irqs_nr = 51,
	.edge_parent_irqs_nr = 29,
	.level_sts_regs_base = 0x000,
	.level_msk_regs_base = 0x040,
	.level_regs_nr = 14,
	.edge_sts_regs_base = 0x200,
	.edge_msk_regs_base = 0x250,
	.edge_clr_regs_base = 0x2a0,
	.edge_regs_nr = 17,
};


