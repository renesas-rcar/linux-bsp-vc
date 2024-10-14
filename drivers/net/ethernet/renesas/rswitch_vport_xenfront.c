// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/xenbus.h>

#include "rswitch_vport_xen.h"

struct rvp_xenfront {
	struct xenbus_device *xdev;

	struct page *shm_page;
	struct rswitch_vport_xen_shm *shm;
	int shm_gref;
	int from_be_irq;

	struct wait_queue_head wait;

	bool rvpf_running;
};

static struct device *rswitch_dev;
static void __iomem *rswitch_regs;
static int rswitch_irq;

static int rswitch_xen_probe(struct platform_device *pdev)
{
	if (rswitch_dev)
		return -EBUSY;

	rswitch_regs = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR_OR_NULL(rswitch_regs)) {
		rswitch_regs = NULL;
		return -EINVAL;
	}

	rswitch_irq = platform_get_irq(pdev, 0);
	if (rswitch_irq < 0)
		return rswitch_irq;

	rswitch_dev = &pdev->dev;
	return 0;
}

static int rswitch_xen_remove(struct platform_device *pdev)
{
	/* FIXME: support this */

	rswitch_dev = NULL;
	rswitch_regs = NULL;
	return 0;
}

static const struct of_device_id rswitch_xen_of_table[] = {
	{ .compatible = "renesas,etherswitch-xen" },
	{}
};

static struct platform_driver rswitch_xen_platform_driver = {
	.probe = rswitch_xen_probe,
	.remove = rswitch_xen_remove,
	.driver = {
		.name = "rswitch-xen",
		.of_match_table = rswitch_xen_of_table,
	}
};

static int rvp_xenfront_call_xenback(struct rvp_xenfront *rvpx, int command)
{
	int ret;

	rvpx->shm->fe_to_be_command = command;
	notify_remote_via_evtchn(rvpx->shm->fe_to_be_chn);

	ret = wait_event_interruptible(rvpx->wait,
		READ_ONCE(rvpx->shm->fe_to_be_command) == BE_COMMAND_DONE);

	if (ret == 0) {
		rmb();
		ret = rvpx->shm->fe_to_be_result;
	}

	return ret;
}

static irqreturn_t rvp_xenfront_comm_interrupt(int irq, void *data)
{
	struct rvp_xenfront *rvpx = data;

	if (rvpx->shm->fe_to_be_command)
		wake_up_interruptible(&rvpx->wait);

	xen_irq_lateeoi(irq, 0);
	return IRQ_HANDLED;
}

static int rvp_xenfront_register(struct rswitch_vport_desc *rvpd)
{
	struct rvp_xenfront *rvpx = rvpd->fe_drv_priv;

	return rvp_xenfront_call_xenback(rvpx, BE_COMMAND_REGISTER);
}

static void rvp_xenfront_unregister(struct rswitch_vport_desc *rvpd)
{
	struct rvp_xenfront *rvpx = rvpd->fe_drv_priv;

	rvp_xenfront_call_xenback(rvpx, BE_COMMAND_UNREGISTER);
}

static int rvp_xenfront_attach_frontend_rings(struct rswitch_vport_desc *rvpd)
{
	struct rvp_xenfront *rvpx = rvpd->fe_drv_priv;

	return rvp_xenfront_call_xenback(rvpx, BE_COMMAND_ATTACH_RINGS);
}

static int rvp_xenfront_detach_frontend_rings(struct rswitch_vport_desc *rvpd)
{
	struct rvp_xenfront *rvpx = rvpd->fe_drv_priv;

	return rvp_xenfront_call_xenback(rvpx, BE_COMMAND_DETACH_RINGS);
}

static const struct rswitch_vport_desc_ops rvpx_ops = {
	.vport_register = rvp_xenfront_register,
	.vport_unregister = rvp_xenfront_unregister,
	.attach_frontend_rings = rvp_xenfront_attach_frontend_rings,
	.detach_frontend_rings = rvp_xenfront_detach_frontend_rings,
};

static int rvp_xenfront_run(struct rvp_xenfront *rvpx)
{
	struct rswitch_vport_desc *rvpd = &rvpx->shm->rvpd;
	int osid;
	int ret;

	ret = xenbus_gather(XBT_NIL, rvpx->xdev->nodename,
			"osid", "%d", &osid, NULL);
	if (ret < 0) {
		xenbus_dev_error(rvpx->xdev, ret, "osid read failed");
		return ret;
	}

	rvpd->ops = &rvpx_ops;
	rvpd->id = RSWITCH_VPORT_ID_DYNAMIC;
	rvpd->osid = osid;
	rvpd->irq_index = osid;		/* FIXME */
	rvpd->fe_reg_base = rswitch_regs;
	rvpd->fe_irq = rswitch_irq;
	rvpd->dma_dev = &rvpx->xdev->dev;
	rvpd->fe_drv_priv = rvpx;

	return rswitch_vport_frontend_run(rvpd);
}

static void rvp_xenfront_stop(struct rvp_xenfront *rvpx)
{
	struct rswitch_vport_desc *rvpd = &rvpx->shm->rvpd;

	rswitch_vport_frontend_stop(rvpd);
}

static void rvp_xenfront_backend_changed(struct xenbus_device *dev,
		enum xenbus_state backend_state)
{
	struct rvp_xenfront *rvpx = dev_get_drvdata(&dev->dev);
	int ret;

	if (backend_state == XenbusStateConnected && !rvpx->rvpf_running) {
		ret = rvp_xenfront_run(rvpx);
		if (ret == 0)
			rvpx->rvpf_running = true;
	}

	if (backend_state != XenbusStateConnected && rvpx->rvpf_running) {
		rvp_xenfront_stop(rvpx);
		rvpx->rvpf_running = false;
	}
}

static int rvp_xenfront_probe(struct xenbus_device *dev,
		const struct xenbus_device_id *id)
{
	struct rvp_xenfront *rvpx;
	struct gnttab_map_grant_ref map_op;
	struct gnttab_unmap_grant_ref unmap_op;
	int ret;

	xenbus_switch_state(dev, XenbusStateInitialising);

	dma_coerce_mask_and_coherent(&dev->dev, DMA_BIT_MASK(40));

	rvpx = kzalloc(sizeof(*rvpx), GFP_KERNEL);
	if (!rvpx) {
		ret = -ENOMEM;
		xenbus_dev_fatal(dev, ret, "alloc etherdev failed");
		goto alloc_failed;
	}

	rvpx->xdev = dev;
	init_waitqueue_head(&rvpx->wait);
	dev_set_drvdata(&dev->dev, rvpx);

	ret = xenbus_gather(XBT_NIL, dev->otherend,
			"shm-gref", "%u", &rvpx->shm_gref, NULL);
	if (ret < 0) {
		xenbus_dev_fatal(dev, ret, "shm gref read failed");
		goto shm_gref_read_failed;
	}

	ret = gnttab_alloc_pages(1, &rvpx->shm_page);
	if (ret) {
		xenbus_dev_fatal(dev, ret, "shm gnttab alloc failed");
		goto shm_gnttab_alloc_failed;
	}

	gnttab_set_map_op(&map_op,
		(uintptr_t)pfn_to_kaddr(page_to_xen_pfn(rvpx->shm_page)),
		GNTMAP_host_map, rvpx->shm_gref, dev->otherend_id);
	ret = gnttab_map_refs(&map_op, NULL, &rvpx->shm_page, 1);
	if (ret) {
		xenbus_dev_fatal(dev, ret, "shm map failed");
		goto shm_map_failed;
	}
	rvpx->shm = (struct rswitch_vport_xen_shm *)
		page_address(rvpx->shm_page);

	ret = bind_interdomain_evtchn_to_irqhandler_lateeoi(dev->otherend_id,
			rvpx->shm->be_to_fe_chn,
			rvp_xenfront_comm_interrupt, 0, "rvp_xen_comm", rvpx);
	if (ret < 0) {
		xenbus_dev_fatal(dev, ret, "bind comm handler failed");
		goto bind_comm_handler_failed;
	}
	rvpx->from_be_irq = ret;

	ret = xenbus_alloc_evtchn(dev, &rvpx->shm->fe_to_be_chn);
	if (ret < 0) {
		xenbus_dev_fatal(dev, ret, "fe_to_be channel alloc failed");
		goto channel_alloc_failed;
	}

	xenbus_switch_state(dev, XenbusStateConnected);
	return 0;

channel_alloc_failed:
	unbind_from_irqhandler(rvpx->from_be_irq, rvpx);
bind_comm_handler_failed:
	gnttab_set_unmap_op(&unmap_op,
		(uintptr_t)pfn_to_kaddr(page_to_xen_pfn(rvpx->shm_page)),
		GNTMAP_host_map, rvpx->shm_gref);
	gnttab_unmap_refs(&unmap_op, NULL, &rvpx->shm_page, 1);
shm_map_failed:
	gnttab_free_pages(1, &rvpx->shm_page);
shm_gnttab_alloc_failed:
shm_gref_read_failed:
	kfree(rvpx);
alloc_failed:
	xenbus_switch_state(dev, XenbusStateClosed);
	return ret;
}

static int rvp_xenfront_remove(struct xenbus_device *dev)
{
	struct rvp_xenfront *rvpx = dev_get_drvdata(&dev->dev);
	struct gnttab_unmap_grant_ref unmap_op;

	if (rvpx->rvpf_running) {
		rvp_xenfront_stop(rvpx);
		rvpx->rvpf_running = false;
	}

	xenbus_switch_state(dev, XenbusStateClosing);

	xenbus_free_evtchn(dev, rvpx->shm->fe_to_be_chn);
	unbind_from_irqhandler(rvpx->from_be_irq, rvpx);
	gnttab_set_unmap_op(&unmap_op,
		(uintptr_t)pfn_to_kaddr(page_to_xen_pfn(rvpx->shm_page)),
		GNTMAP_host_map, rvpx->shm_gref);
	gnttab_unmap_refs(&unmap_op, NULL, &rvpx->shm_page, 1);
	gnttab_free_pages(1, &rvpx->shm_page);

	kfree(rvpx);

	xenbus_switch_state(dev, XenbusStateClosed);

	return 0;
}

static const struct xenbus_device_id rvp_xenfront_ids[] = {
	{ "rswitch_vport" },
	{ "" }
};

static struct xenbus_driver rvp_xenfront_driver = {
	.ids = rvp_xenfront_ids,
	.probe = rvp_xenfront_probe,
	.remove = rvp_xenfront_remove,
	.otherend_changed = rvp_xenfront_backend_changed,
};

static int __init rvp_xenfront_init(void)
{
	int ret;

	if (!xen_domain())
		return -ENODEV;

	ret = platform_driver_register(&rswitch_xen_platform_driver);
	if (ret)
		return ret;

	ret = xenbus_register_frontend(&rvp_xenfront_driver);
	if (ret) {
		platform_driver_unregister(&rswitch_xen_platform_driver);
		return ret;
	}

	return 0;
}
module_init(rvp_xenfront_init);

static void __exit rvp_xenfront_exit(void)
{
	xenbus_unregister_driver(&rvp_xenfront_driver);
	platform_driver_unregister(&rswitch_xen_platform_driver);
}
module_exit(rvp_xenfront_exit);

MODULE_LICENSE("GPL");
