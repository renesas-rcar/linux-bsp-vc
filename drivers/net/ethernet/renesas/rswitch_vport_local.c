// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>

#include "rswitch_lib.h"

#define VPORT_NR	2
static struct rswitch_vport_desc *rvpd_array[VPORT_NR];

#define RVPL_IRQ_INDEX 7

static const struct rswitch_vport_desc_ops rvpl_ops = {
	.vport_register = rswitch_vport_register,
	.vport_unregister = rswitch_vport_unregister,
	.attach_frontend_rings = rswitch_vport_attach_frontend_rings,
	.detach_frontend_rings = rswitch_vport_detach_frontend_rings,
};

static struct rswitch_vport_desc *rvpl_create(void)
{
	struct rswitch_vport_desc *rvpd;
	int ret;

	rvpd = kzalloc(sizeof(*rvpd), GFP_KERNEL);
	if (!rvpd)
		return ERR_PTR(-ENOMEM);

	rvpd->ops = &rvpl_ops;
	rvpd->id = RSWITCH_VPORT_ID_DYNAMIC;
	rvpd->irq_index = RVPL_IRQ_INDEX;
	rvpd->fe_irq = -1;

	ret = rswitch_vport_frontend_run(rvpd);
	if (ret) {
		kfree(rvpd);
		return ERR_PTR(ret);
	}

	return rvpd;
}

static void rvpl_remove(struct rswitch_vport_desc *rvpd)
{
	rswitch_vport_frontend_stop(rvpd);
	kfree(rvpd);
}

static int rvpl_init(void)
{
	unsigned int i;
	int ret;

	for (i = 0; i < VPORT_NR; i++) {
		rvpd_array[i] = rvpl_create();
		if (IS_ERR(rvpd_array[i])) {
			ret = PTR_ERR(rvpd_array[i]);
			goto failed;
		}
	}

	return 0;

	do {
		i--;
		rvpl_remove(rvpd_array[i]);
failed:
		;
	} while (i > 0);

	return ret;
}

static void rvpl_exit(void)
{
	unsigned int i;

	for (i = 0; i < VPORT_NR; i++)
		rvpl_remove(rvpd_array[i]);
}

module_init(rvpl_init);
module_exit(rvpl_exit);
MODULE_LICENSE("GPL v2");
