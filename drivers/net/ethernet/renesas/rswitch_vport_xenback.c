// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/xenbus.h>

#include "rswitch_vport_xen.h"

struct rvp_xenback {
	struct xenbus_device *xdev;

	struct rswitch_vport_xen_shm *shm;
	int shm_gref;
	int from_fe_irq;

	struct work_struct work;
	int work_command;

	bool rvp_registered;
};

static void rvp_xenback_comm_work(struct work_struct *work)
{
	struct rvp_xenback *rvpx = container_of(work, typeof(*rvpx), work);
	struct rswitch_vport_desc *rvpd = &rvpx->shm->rvpd;
	int ret = 0;

	switch (rvpx->work_command) {
	case BE_COMMAND_REGISTER:
		ret = rswitch_vport_register(rvpd);
		if (ret == 0)
			rvpx->rvp_registered = true;
		break;
	case BE_COMMAND_UNREGISTER:
		if (rvpx->rvp_registered) {
			rswitch_vport_unregister(rvpd);
			rvpx->rvp_registered = false;
		}
		break;
	case BE_COMMAND_ATTACH_RINGS:
		if (rvpx->rvp_registered)
			ret = rswitch_vport_attach_frontend_rings(rvpd);
		else
			ret = -EIO;
		break;
	case BE_COMMAND_DETACH_RINGS:
		if (rvpx->rvp_registered)
			ret = rswitch_vport_detach_frontend_rings(rvpd);
		else
			ret = -EIO;
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	rvpx->shm->fe_to_be_result = ret;
	wmb();
	rvpx->shm->fe_to_be_command = BE_COMMAND_DONE;
	notify_remote_via_evtchn(rvpx->shm->be_to_fe_chn);
}

static irqreturn_t rvp_xenback_comm_interrupt(int irq, void *data)
{
	struct rvp_xenback *rvpx = data;

	if (rvpx->shm->fe_to_be_command > BE_COMMAND_IN_PROGRESS) {
		rvpx->work_command = rvpx->shm->fe_to_be_command;
		rvpx->shm->fe_to_be_command = BE_COMMAND_IN_PROGRESS;
		schedule_work(&rvpx->work);
	}

	xen_irq_lateeoi(irq, 0);
	return IRQ_HANDLED;
}

static void rvp_xenback_frontend_changed(struct xenbus_device *dev,
		enum xenbus_state frontend_state)
{
	struct rvp_xenback *rvpx = dev_get_drvdata(&dev->dev);
	int ret;

	if (frontend_state == XenbusStateConnected &&
	    dev->state != XenbusStateConnected) {
		ret = bind_interdomain_evtchn_to_irqhandler_lateeoi(
				dev->otherend_id,
				rvpx->shm->fe_to_be_chn,
				rvp_xenback_comm_interrupt, 0,
				"rvp_xen_comm", rvpx);
		if (ret < 0) {
			xenbus_dev_fatal(dev, ret, "bind comm handler failed");
			xenbus_switch_state(dev, XenbusStateClosed);
		} else {
			rvpx->from_fe_irq = ret;
			xenbus_switch_state(dev, XenbusStateConnected);
		}
	}

	if (frontend_state != XenbusStateConnected &&
	    dev->state == XenbusStateConnected) {
		if (rvpx->rvp_registered) {
			rswitch_vport_unregister(&rvpx->shm->rvpd);
			rvpx->rvp_registered = false;
		}

		unbind_from_irqhandler(rvpx->from_fe_irq, rvpx);
		/* FXIME: reinit? */
		xenbus_switch_state(dev, XenbusStateClosed);
	}
}

static int rvp_xenback_probe(struct xenbus_device *dev,
		const struct xenbus_device_id *id)
{
	struct rvp_xenback *rvpx;
	int ret;

	rvpx = devm_kzalloc(&dev->dev, sizeof(*rvpx), GFP_KERNEL);
	if (!rvpx) {
		ret = -ENOMEM;
		xenbus_dev_fatal(dev, ret, "allocation failed");
		goto rvpx_alloc_failed;
	}

	rvpx->xdev = dev;
	dev_set_drvdata(&dev->dev, rvpx);
	INIT_WORK(&rvpx->work, rvp_xenback_comm_work);

	rvpx->shm = (struct rswitch_vport_xen_shm *)
		__get_free_page(GFP_KERNEL);
	if (!rvpx->shm) {
		ret = -ENOMEM;
		xenbus_dev_fatal(dev, ret, "shm allocation failed");
		goto shm_alloc_failed;
	}
	memset(rvpx->shm, 0, sizeof(*rvpx->shm));

	ret = gnttab_grant_foreign_access(dev->otherend_id,
			virt_to_gfn(rvpx->shm), 0);
	if (ret < 0) {
		xenbus_dev_fatal(dev, ret, "grant shm access failed");
		goto shm_grant_failed;
	}
	rvpx->shm_gref = ret;
	ret = xenbus_printf(XBT_NIL, dev->nodename, "shm-gref",
			"%u", rvpx->shm_gref);
	if (ret < 0) {
		xenbus_dev_fatal(dev, ret, "shm gref publish failed");
		goto shm_gref_publish_failed;
	}

	ret = xenbus_alloc_evtchn(dev, &rvpx->shm->be_to_fe_chn);
	if (ret < 0) {
		xenbus_dev_fatal(dev, ret, "be_to_fe channel alloc failed");
		goto channel_alloc_failed;
	}

	xenbus_switch_state(dev, XenbusStateInitWait);
	return 0;

channel_alloc_failed:
shm_gref_publish_failed:
	gnttab_end_foreign_access(rvpx->shm_gref, 0, (unsigned long)rvpx->shm);
shm_grant_failed:
	put_page(virt_to_page((unsigned long)rvpx->shm));
shm_alloc_failed:
rvpx_alloc_failed:
	xenbus_switch_state(dev, XenbusStateClosed);
return ret;
}

static int rvp_xenback_remove(struct xenbus_device *dev)
{
	struct rvp_xenback *rvpx = dev_get_drvdata(&dev->dev);

	xenbus_switch_state(dev, XenbusStateClosing);

	xenbus_free_evtchn(dev, rvpx->shm->be_to_fe_chn);
	gnttab_end_foreign_access(rvpx->shm_gref, 0, (unsigned long)rvpx->shm);
	put_page(virt_to_page((unsigned long)rvpx->shm));

	xenbus_switch_state(dev, XenbusStateClosed);
	return 0;
}

static const struct xenbus_device_id rvp_xenback_ids[] = {
	{ "rswitch_vport" },
	{ "" }
};

static struct xenbus_driver rvp_xenback_driver = {
	.ids = rvp_xenback_ids,
	.probe = rvp_xenback_probe,
	.remove = rvp_xenback_remove,
	.otherend_changed = rvp_xenback_frontend_changed,
};

static int __init rvp_xenback_init(void)
{
	if (!xen_domain())
		return -ENODEV;

	return xenbus_register_backend(&rvp_xenback_driver);
}
module_init(rvp_xenback_init);

static void __exit rvp_xenback_exit(void)
{
	xenbus_unregister_driver(&rvp_xenback_driver);
}
module_exit(rvp_xenback_exit);

MODULE_LICENSE("GPL");
