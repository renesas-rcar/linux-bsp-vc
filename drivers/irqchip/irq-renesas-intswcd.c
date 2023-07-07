// SPDX-License-Identifier: GPL-2.0

#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>

#include "irq-renesas-intswcd.h"
#include <dt-bindings/interrupt-controller/arm-gic.h>

struct intswcd_handler_data;

struct intswcd {
	void __iomem *regs;
	struct device *dev;
	const struct intswcd_info *ii;
	struct irq_domain *domain;
	unsigned int *hwirq_flags;
	raw_spinlock_t lock;
	struct intswcd_handler_data *hd[];
};

#define INTSWCD_HWIRQ_FLAG_FORCE_LEVEL		BIT(0)

struct intswcd_handler_data {
	struct intswcd *swcd;
	int irq;
	unsigned int hwirq_nr;
	unsigned int hwirq[];
};

#define DEFINE_REG_FUNC(type)						\
static inline u32 __iomem *type##_reg(struct intswcd *swcd, int reg)	\
{									\
	return (u32 __iomem *)(swcd->regs + swcd->ii->type##_regs_base) + reg; \
}

DEFINE_REG_FUNC(level_sts)
DEFINE_REG_FUNC(level_msk)
DEFINE_REG_FUNC(edge_sts)
DEFINE_REG_FUNC(edge_msk)
DEFINE_REG_FUNC(edge_clr)

static void intswcd_irq_set_masked(struct irq_data *d, bool masked)
{
	struct intswcd *swcd = d->domain->host_data;
	const struct intswcd_hwirq_info *ihi = &swcd->ii->ihi[d->hwirq];
	u32 __iomem *reg;
	u32 val;
	unsigned long flags;

	if (ihi->type == INTSWCD_EDGE)
		reg = edge_msk_reg(swcd, ihi->reg);
	else
		reg = level_msk_reg(swcd, ihi->reg);

	raw_spin_lock_irqsave(&swcd->lock, flags);
	val = ioread32(reg);
	if (masked)
		val &= ~ihi->mask;	/* mask irq = clear bit */
	else
		val |= ihi->mask;	/* unmask irq = set bit */
	iowrite32(val, reg);
	raw_spin_unlock_irqrestore(&swcd->lock, flags);
}

static void intswcd_irq_mask(struct irq_data *d)
{
	intswcd_irq_set_masked(d, true);
}

static void intswcd_irq_unmask(struct irq_data *d)
{
	intswcd_irq_set_masked(d, false);
}

static void intswcd_irq_ack(struct irq_data *d)
{
	struct intswcd *swcd = d->domain->host_data;
	const struct intswcd_hwirq_info *ihi = &swcd->ii->ihi[d->hwirq];

	/* This is called both for level and for edge irqs */
	if (ihi->type == INTSWCD_EDGE)
		iowrite32(ihi->mask, edge_clr_reg(swcd, ihi->reg));
}

static struct irq_chip intswcd_irq_chip = {
	.name = "intswcd",
	.irq_mask = intswcd_irq_mask,
	.irq_unmask = intswcd_irq_unmask,
	.irq_ack = intswcd_irq_ack,
};

static int intswcd_irq_map(struct irq_domain *domain, unsigned int irq,
		irq_hw_number_t hwirq)
{
	struct intswcd *swcd = domain->host_data;
	const struct intswcd_hwirq_info *ihi;
	irq_flow_handler_t handler;

	if (WARN_ON(hwirq >= swcd->ii->ihi_nr))
		return -EINVAL;
	ihi = &swcd->ii->ihi[hwirq];

	if (ihi->type == INTSWCD_LEVEL ||
	    (swcd->hwirq_flags[hwirq] & INTSWCD_HWIRQ_FLAG_FORCE_LEVEL)) {
		irq_set_status_flags(irq, IRQ_LEVEL);
		handler = handle_level_irq;
	} else {
		irq_clear_status_flags(irq, IRQ_LEVEL);
		handler = handle_edge_irq;
	}

	irq_set_chip_and_handler(irq, &intswcd_irq_chip, handler);

	return 0;
}

static int intswcd_irq_xlate(struct irq_domain *domain, struct device_node *dn,
		const u32 *intspec, unsigned int intspec_size,
		unsigned long *out_hwirq, unsigned int *out_type)
{
	struct intswcd *swcd = domain->host_data;
	unsigned int hwirq, type, dt_type;
	const struct intswcd_hwirq_info *ihi;

	if (intspec_size < 1) {
		dev_err(swcd->dev, "%pOF: empty interrupt definition\n", dn);
		return -EINVAL;
	}

	hwirq = intspec[0];
	if (hwirq >= swcd->ii->ihi_nr) {
		dev_err(swcd->dev, "%pOF: hwirq %d is out of range\n",
			dn, hwirq);
		return -EINVAL;
	}

	ihi = &swcd->ii->ihi[hwirq];
	if (!ihi->type) {
		dev_err(swcd->dev,
			"%pOF: hwirq %d is not defined in the driver\n",
			dn, hwirq);
		return -EINVAL;
	}

	type = ihi->type == INTSWCD_EDGE ?
		IRQ_TYPE_EDGE_RISING : IRQ_TYPE_LEVEL_HIGH;

	if (intspec_size >= 2) {
		dt_type = intspec[1];

		/* As a quirk, can force level hanling for INTSWCD_EDGE */
		if (ihi->type == INTSWCD_EDGE &&
		    (dt_type == IRQ_TYPE_LEVEL_HIGH ||
		     dt_type == IRQ_TYPE_LEVEL_LOW)) {
			dev_info(swcd->dev,
				"%pOF: force handling hwirq %d as level\n",
				dn, hwirq);
			swcd->hwirq_flags[hwirq] |=
				INTSWCD_HWIRQ_FLAG_FORCE_LEVEL;
			type = dt_type;
		} else if (dt_type != type) {
			dev_err(swcd->dev,
				"%pOF: incompatible trigger type %d for hwirq %d\n",
				dn, dt_type, hwirq);
			return -EINVAL;
		}
	}

	if (intspec_size > 2) {
		dev_err(swcd->dev, "%pOF: unparseable interrupt definition\n",
			dn);
		return -EINVAL;
	}

	*out_hwirq = hwirq;
	*out_type = type;

	return 0;
}

static const struct irq_domain_ops intswcd_domain_ops = {
	.map = intswcd_irq_map,
	.xlate = intswcd_irq_xlate,
};

static void intswcd_irq_handler(struct irq_desc *desc)
{
	struct intswcd_handler_data *hd = irq_desc_get_handler_data(desc);
	struct intswcd *swcd = hd->swcd;
	const struct intswcd_hwirq_info *ihi;
	unsigned int i, hwirq;
	int virq;
	u32 val;

	chained_irq_enter(irq_desc_get_chip(desc), desc);

	for (i = 0; i < hd->hwirq_nr; i++) {
		hwirq = hd->hwirq[i];
		virq = irq_find_mapping(swcd->domain, hwirq);
		if (!virq)
			continue;

		ihi = &swcd->ii->ihi[hwirq];
		if (ihi->type == INTSWCD_EDGE)
			val = ioread32(edge_sts_reg(swcd, ihi->reg)) &
			      ioread32(edge_msk_reg(swcd, ihi->reg));
		else
			val = ioread32(level_sts_reg(swcd, ihi->reg)) &
			      ioread32(level_msk_reg(swcd, ihi->reg));
		if (val & ihi->mask)
			generic_handle_irq(virq);
	}

	chained_irq_exit(irq_desc_get_chip(desc), desc);
}

static inline bool ihi_same_parent(const struct intswcd_hwirq_info *ihi1,
		const struct intswcd_hwirq_info *ihi2)
{
	return ihi1->type == ihi2->type &&
	       ihi1->parent_irq_offset == ihi2->parent_irq_offset;
}

static int intswcd_parse_interrupt_base(struct device *dev,
		const char *prop, struct of_phandle_args *args)
{
	int ret;

	ret = of_parse_phandle_with_args(dev->of_node, prop,
			"#interrupt-cells", 0, args);
	if (ret < 0) {
		dev_err(dev, "failed to parse %s\n", prop);
		return ret;
	}
	if (!irq_find_host(args->np))
		return -EPROBE_DEFER;

	/* support only GIC dt layout for now */
	if (args->args_count != 3 || args->args[0] != GIC_SPI) {
		dev_err(dev, "%s: unsupported dt layout\n", prop);
		return -EINVAL;
	}

	return 0;
}

static void intswcd_init_hw(struct intswcd *swcd)
{
	const struct intswcd_info *ii = swcd->ii;
	const struct intswcd_hwirq_info *ihi;
	unsigned int i;

	/* explicitly mask all interrupts */
	for (i = 0; i < ii->level_regs_nr; i++)
		iowrite32(0, level_msk_reg(swcd, i));
	for (i = 0; i < ii->edge_regs_nr; i++)
		iowrite32(0, edge_msk_reg(swcd, i));

	/* explicitly clear all edge interrupts */
	for (i = 0; i < ii->ihi_nr; i++) {
		ihi = &ii->ihi[i];
		if (ihi->type == INTSWCD_EDGE)
			iowrite32(ihi->mask, edge_clr_reg(swcd, ihi->reg));
	}

}

static int intswcd_probe(struct platform_device *pdev)
{
	struct intswcd *swcd;
	const struct intswcd_info *ii;
	const struct intswcd_hwirq_info *ihi, *ihi2;
	struct of_phandle_args level_args, edge_args, args;
	struct intswcd_handler_data *hd;
	unsigned int i, j, k, hd_i, hd_count, hwirq_nr;
	int ret;

	ii = of_device_get_match_data(&pdev->dev);

	hd_count = ii->level_parent_irqs_nr + ii->edge_parent_irqs_nr;
	swcd = devm_kzalloc(&pdev->dev, struct_size(swcd, hd, hd_count),
			GFP_KERNEL);
	if (!swcd)
		return -ENOMEM;
	swcd->dev = &pdev->dev;
	swcd->ii = ii;

	raw_spin_lock_init(&swcd->lock);

	swcd->hwirq_flags = devm_kzalloc(swcd->dev,
			ii->ihi_nr * sizeof(*swcd->hwirq_flags), GFP_KERNEL);
	if (!swcd->hwirq_flags)
		return -ENOMEM;

	for (i = 0; i < ii->ihi_nr; i++) {
		ihi = &ii->ihi[i];

		switch (ihi->type) {
		case INTSWCD_LEVEL:
			hd_i = ihi->parent_irq_offset;
			break;
		case INTSWCD_EDGE:
			hd_i = ihi->parent_irq_offset +
					ii->level_parent_irqs_nr;
			break;
		default:
			continue;
		}

		/* skip already processed parent */
		if (swcd->hd[hd_i])
			continue;

		/* count number of child interrupts sharing this parent */
		hwirq_nr = 1;
		for (j = i + 1; j < ii->ihi_nr; j++) {
			ihi2 = &ii->ihi[j];
			if (ihi_same_parent(ihi, ihi2))
				hwirq_nr++;
		}

		/* create and populate handler data object */
		hd = devm_kzalloc(swcd->dev, struct_size(hd, hwirq, hwirq_nr),
				GFP_KERNEL);
		if (!hd)
			return -ENOMEM;

		hd->swcd = swcd;
		swcd->hd[hd_i] = hd;

		hd->irq = -1;		/* for now */
		hd->hwirq_nr = hwirq_nr;
		for (j = i, k = 0; j < ii->ihi_nr; j++) {
			ihi2 = &ii->ihi[j];
			if (ihi_same_parent(ihi, ihi2))
				hd->hwirq[k++] = j;
		}
	}

	swcd->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(swcd->regs)) {
		dev_err(swcd->dev, "failed to map registers\n");
		return PTR_ERR(swcd->regs);
	}

	ret = intswcd_parse_interrupt_base(swcd->dev, "first-level-interrupt",
			&level_args);
	if (ret)
		return ret;
	ret = intswcd_parse_interrupt_base(swcd->dev, "first-edge-interrupt",
			&edge_args);
	if (ret)
		return ret;

	intswcd_init_hw(swcd);

	swcd->domain = irq_domain_add_linear(swcd->dev->of_node, ii->ihi_nr,
			&intswcd_domain_ops, swcd);
	if (!swcd->domain)
		return -ENOMEM;

	for (i = 0; i < hd_count; i++) {
		hd = swcd->hd[i];
		if (!hd)
			continue;
		ihi = &swcd->ii->ihi[hd->hwirq[0]];

		args = ihi->type == INTSWCD_EDGE ? edge_args : level_args;
		/* NOTE: here is a dependency in GIC dt layout */
		args.args[1] += ihi->parent_irq_offset;

		ret = irq_create_of_mapping(&args);
		if (ret < 0) {
			dev_err(swcd->dev, "could not map parent for %s/%d\n",
				ihi->type == INTSWCD_EDGE ? "edge" : "level",
				ihi->parent_irq_offset);
			goto map_err;
		}

		hd->irq = ret;
		irq_set_chained_handler_and_data(hd->irq,
				intswcd_irq_handler, hd);
	}

	platform_set_drvdata(pdev, swcd);
	return 0;

map_err:
	for (i = 0; i < hd_count; i++) {
		hd = swcd->hd[i];
		if (hd && hd->irq >= 0)
			irq_set_chained_handler_and_data(hd->irq, NULL, NULL);
	}

	irq_domain_remove(swcd->domain);
	return ret;
}

static const struct of_device_id intswcd_of_table[] = {
#if IS_ENABLED(CONFIG_ARCH_R8A779F0)
	{
		.compatible = "renesas,intswcd-r8a779f0",
		.data = &r8a779f0_ii,
	},
#endif
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, intswcd_of_table);

static struct platform_driver intswcd_driver = {
	.driver = {
		.name = "renesas-intswcd",
		.of_match_table = intswcd_of_table,
	},
	.probe = intswcd_probe,
};
builtin_platform_driver(intswcd_driver);

MODULE_AUTHOR("Nikita Yushchenko <nikita.yoush@cogentembedded.com>");
MODULE_DESCRIPTION("Renesas INTSWCD driver");
MODULE_LICENSE("GPL v2");
