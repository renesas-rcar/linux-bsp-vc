// SPDX-License-Identifier: GPL-2.0
/* Renesas RSwitch PCIe device driver
 *
 * Copyright (C) 2021 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License version 2,
 * as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/gpio.h>
#include <linux/phy.h>

#include "rswitch2.h"
#include "rswitch2_pci.h"

#define RSWITCH2_PCI_DRIVER "rswitch2_pci_driver"

#define RSWITCH2_PCI_VID (0x1172)
#define RSWITCH2_PCI_DID (0xE002)

/* Module parameter */
static int expected_dev_id = RSWITCH2_PCI_DID;

module_param_named(device_id, expected_dev_id, int, 0644);
MODULE_PARM_DESC(device_id, "Override device ID to match");

/* The RSwitch2 PCI driver supports devices with VID = 0x1172,
 * PID can be overwritten by module parameter for debug purposes
 */
static struct pci_device_id rswitch2_pci_id_table[] = {
	{ PCI_DEVICE(RSWITCH2_PCI_VID, PCI_ANY_ID) },
	{0,}
};


static const u8 rsw2_default_mac[ETH_ALEN] __aligned(2) = {
	0x74, 0x90, 0x50, 0x00, 0x00, 0x00
};

#define RSW2_INTERNAL_PORT_MARKER 0xCC


MODULE_DEVICE_TABLE(pci, rswitch2_pci_id_table);

static int rswicth2_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void rswitch2_pci_remove(struct pci_dev *pdev);

/* Driver registration structure */
static struct pci_driver rswitch2_pci_driver = {
	.name = RSWITCH2_PCI_DRIVER,
	.id_table = rswitch2_pci_id_table,
	.probe = rswicth2_pci_probe,
	.remove = rswitch2_pci_remove
};

/* Device driver's private data structure */
struct rswitch2_pci_driver_priv {
	struct rswitch2_drv *rsw2;
	u8 __iomem *hwmem;
	struct kobject *kobj;
	u32 sysid;
	u32 rtlid;
};


static int __init rswitch2_pci_driver_init(void)
{
	/* Register new PCI driver */
	return pci_register_driver(&rswitch2_pci_driver);
}

static void __exit rswitch2_pci_driver_exit(void)
{
	/* Unregister */
	pci_unregister_driver(&rswitch2_pci_driver);
}

void release_device(struct pci_dev *pdev)
{
	/* Free memory region */
	pci_release_region(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
	/* And disable device */
	pci_disable_device(pdev);
}

static int rswitch2_pci_get_fw_version(struct pci_dev *pdev)
{
	u8 __iomem *cur_reg;

	struct rswitch2_pci_driver_priv *drv_priv;

	drv_priv = (struct rswitch2_pci_driver_priv *)pci_get_drvdata(pdev);

	if (!drv_priv)
		return -ENODEV;

	cur_reg = drv_priv->hwmem + RSWITCH2_PCI_FPGA_OFFSET
			+ RSWITCH2_PCI_FW_SYSID_REG;
	drv_priv->sysid = ioread32(cur_reg);

	cur_reg = drv_priv->hwmem + RSWITCH2_PCI_FPGA_OFFSET
			+ RSWITCH2_PCI_FW_RTLID_REG;
	drv_priv->rtlid = ioread32(cur_reg);

	return 0;
}

static int rswitch2_pci_set_a2p_addr_tbl(struct pci_dev *pdev)
{
	u32 data;
	u8 __iomem *cur_reg;

	struct rswitch2_pci_driver_priv *drv_priv;

	drv_priv = (struct rswitch2_pci_driver_priv *)pci_get_drvdata(pdev);

	if (!drv_priv)
		return -ENODEV;

	cur_reg = drv_priv->hwmem + RSWITCH2_PCI_A2P_ADDR_MAP_LO0_REG;
	data = RSWITCH2_PCI_A2P_ADDR_LO0 + RSWITCH2_PCI_A2P_ADDR_SPACE0_TYPE;
	iowrite32(data, cur_reg);

	cur_reg = drv_priv->hwmem + RSWITCH2_PCI_A2P_ADDR_MAP_HI0_REG;
	data = RSWITCH2_PCI_A2P_ADDR_HI0;
	iowrite32(data, cur_reg);

	cur_reg = drv_priv->hwmem + RSWITCH2_PCI_A2P_ADDR_MAP_LO1_REG;
	data = RSWITCH2_PCI_A2P_ADDR_LO1 + RSWITCH2_PCI_A2P_ADDR_SPACE0_TYPE;
	iowrite32(data, cur_reg);

	cur_reg = drv_priv->hwmem + RSWITCH2_PCI_A2P_ADDR_MAP_HI1_REG;
	data = RSWITCH2_PCI_A2P_ADDR_HI1;
	iowrite32(data, cur_reg);

	return 0;
}

/* Export firmware version to sysfs */
static ssize_t fw_sysid_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct device *dev;
	struct rswitch2_pci_driver_priv *drv_priv;

	dev = kobj_to_dev(kobj->parent);

	drv_priv = (struct rswitch2_pci_driver_priv *)dev_get_drvdata(dev);

	return sprintf(buf, "0x%.8x\n", drv_priv->sysid);
}

static ssize_t fw_rtlid_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct device *dev;
	struct rswitch2_pci_driver_priv *drv_priv;

	dev = kobj_to_dev(kobj->parent);
	drv_priv = (struct rswitch2_pci_driver_priv *)dev_get_drvdata(dev);

	return sprintf(buf, "0x%.8x\n", drv_priv->rtlid);
}
static struct kobj_attribute fw_sysid_attr =
	__ATTR(sysid, 0444, fw_sysid_show, NULL /*add_slot_store*/);

static struct kobj_attribute fw_rtlid_attr =
	__ATTR(rtlid, 0444, fw_rtlid_show, NULL/*remove_slot_store */);

static struct attribute *fw_attrs[] = {
	&fw_sysid_attr.attr,
	&fw_rtlid_attr.attr,
	NULL,
};

static const struct attribute_group fw_attr_group = {
	.attrs = fw_attrs,
};

static int rswitch2_pci_extend_sysfs(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;
	struct rswitch2_pci_driver_priv *drv_priv;
	int ret;

	drv_priv = (struct rswitch2_pci_driver_priv *)pci_get_drvdata(pdev);

	drv_priv->kobj = kobject_create_and_add("firmware", &dev->kobj);
	if (!drv_priv->kobj)
		return -ENOMEM;

	ret = sysfs_create_group(drv_priv->kobj, &fw_attr_group);

	return ret;
}

static void rswitch2_pci_remove_sysfs(struct pci_dev *pdev)
{
	struct rswitch2_pci_driver_priv *drv_priv;

	drv_priv = (struct rswitch2_pci_driver_priv *)pci_get_drvdata(pdev);

	sysfs_remove_group(drv_priv->kobj, &fw_attr_group);
	kobject_del(drv_priv->kobj);
}

int __weak rswitch2_pci_fw_register(u8 __iomem *hwmem_coherent)
{
	(void)hwmem_coherent;
	pr_info("Firmware functionality is not supported\n");

	return 0;
}

void __weak rswitch2_pci_fw_unregister(void) {}

static void rswitch2_pci_set_base_addr(struct rswitch2_drv *rsw2)
{
	u8 __iomem *base_addr = rsw2->base_addr;

	rsw2->fwd_base_addr = base_addr + RSWITCH2_MFWD_BASE_ADDR;
	rsw2->fab_base_addr = base_addr + RSWITCH2_FAB_BASE_ADDR;
	rsw2->coma_base_addr = base_addr + RSWITCH2_COMA_BASE_ADDR;
	rsw2->etha_base_addrs[0] = base_addr + RSWITCH2_TSNA0_BASE_ADDR;
	rsw2->etha_base_addrs[1] = base_addr + RSWITCH2_TSNA1_BASE_ADDR;
	rsw2->etha_base_addrs[2] = base_addr + RSWITCH2_TSNA2_BASE_ADDR;
	rsw2->etha_base_addrs[3] = base_addr + RSWITCH2_TSNA3_BASE_ADDR;
	rsw2->gwca_base_addrs[0] = base_addr + RSWITCH2_GWCA_BASE_ADDR;
}

static int rswitch2_pci_request_irq_line(struct rswitch2_drv *rsw2)
{
	int irq;

	gpio_request(RSWITCH2_INT_LINE_GPIO, "rsw2_irq");
	gpio_direction_input(RSWITCH2_INT_LINE_GPIO);
	gpio_export(RSWITCH2_INT_LINE_GPIO, true);
	irq = gpio_to_irq(RSWITCH2_INT_LINE_GPIO);

	if (irq < 0) {
		dev_err(rsw2->dev, "Failed to get GPIO line IRQ (%d)\n", irq);
		return -EINVAL;
	}
	rsw2->irq = irq;
	dev_info(rsw2->dev, "Got IRQ (%d)\n", irq);

	return 0;
}

static void rswitch2_pci_release_irq_line(struct rswitch2_drv *rsw2)
{
	gpio_unexport(RSWITCH2_INT_LINE_GPIO);
	gpio_free(RSWITCH2_INT_LINE_GPIO);
}

static int rswitch2_pci_set_port_data(struct rswitch2_drv *rsw2)
{
	unsigned int cur_port;
	unsigned int total_ports;
	struct rswitch2_port_data *cur_port_data;

	total_ports = rsw2->num_of_cpu_ports + rsw2->num_of_tsn_ports;
	rsw2->port_data = kcalloc(total_ports, sizeof(*cur_port_data), GFP_KERNEL);
	if (!rsw2->port_data)
		return -ENOMEM;

	memset(rsw2->port_data, 0, sizeof(*cur_port_data));

	for (cur_port = 0; cur_port < total_ports; cur_port++) {

		cur_port_data = &rsw2->port_data[cur_port];
		memcpy(cur_port_data->mac_addr, rsw2_default_mac,
				sizeof(cur_port_data->mac_addr));
		cur_port_data->mac_addr[ETH_ALEN - 1] = (u8)(cur_port);

		if (cur_port == 0)
			cur_port_data->mac_addr[ETH_ALEN - 2] = RSW2_INTERNAL_PORT_MARKER;
		else
			cur_port_data->phy_iface = PHY_INTERFACE_MODE_RGMII;
	}

	/* etha0 --> PHY 0x04 */
	cur_port_data = &rsw2->port_data[1];
	snprintf(cur_port_data->phy_id, MII_BUS_ID_SIZE + 3, PHY_ID_FMT,
						"e6800000.ethernet-ffffffff", 0x04);
	pr_info("Port 0 PHY ID: '%s'\n", cur_port_data->phy_id);

	/* etha1 --> PHY 0x03 */
	cur_port_data = &rsw2->port_data[2];
	snprintf(cur_port_data->phy_id, MII_BUS_ID_SIZE + 3, PHY_ID_FMT,
						"e6800000.ethernet-ffffffff", 0x03);
	pr_info("Port 1 PHY ID: '%s'\n", cur_port_data->phy_id);

	/* etha2 --> PHY 0x01 */
	cur_port_data = &rsw2->port_data[3];
	snprintf(cur_port_data->phy_id, MII_BUS_ID_SIZE + 3, PHY_ID_FMT,
						"e6800000.ethernet-ffffffff", 0x01);
	pr_info("Port 2 PHY ID: '%s'\n", cur_port_data->phy_id);

	/* etha3 --> PHY 0x00 */
	cur_port_data = &rsw2->port_data[4];
	snprintf(cur_port_data->phy_id, MII_BUS_ID_SIZE + 3, PHY_ID_FMT,
						"e6800000.ethernet-ffffffff", 0x00);
	pr_info("Port 3 PHY ID: '%s'\n", cur_port_data->phy_id);

	return 0;
}


/* This function is called by the kernel */
static int rswicth2_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int bar;
	int ret;
	u16 vendor_id, device_id;
	phys_addr_t mmio_start, mmio_len;
	const int bar_zero = 0;
	struct rswitch2_pci_driver_priv *drv_priv;
	struct rswitch2_drv *rsw2;
	size_t array_size;

	/* Let's read data from the PCI device configuration registers */
	pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor_id);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &device_id);

	pci_info(pdev, "vendor ID: 0x%X device ID: 0x%X\n",
		 vendor_id, device_id);
	if (device_id != expected_dev_id) {
		pci_info(pdev, "Device did not match expected vendor ID: 0x%X device ID: 0x%X\n",
			 RSWITCH2_PCI_VID, expected_dev_id);
		pci_info(pdev, "You may want to override device ID\n");

		ret = -ENODEV;
		goto out_pci_ident;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		pci_err(pdev, "Failed to enable PCI device: vendor ID: 0x%X device ID: 0x%X\n",
			vendor_id, device_id);
		goto out_pci_ident;
	}

	ret = pci_set_mwi(pdev);
	if (ret) {
		pci_err(pdev, "Failed to enable PCI Memory-Write-Ivalidate transaction");
		goto out_pci_dev;
	}

	pci_set_master(pdev);

	/* Request IO BAR */
	bar = pci_select_bars(pdev, IORESOURCE_MEM);

	/* Enable device_id memory */
	ret = pci_enable_device_mem(pdev);
	if (ret)
		goto out_pci_dev;

	/* Request memory region for the BAR */
	ret = pci_request_region(pdev, bar, RSWITCH2_PCI_DRIVER);
	if (ret)
		goto out_pci_dev;

	/* Get start and stop memory offsets */
	mmio_start = pci_resource_start(pdev, bar_zero);
	mmio_len = pci_resource_len(pdev, bar_zero);

	pci_dbg(pdev, "mmio_start: 0x%llX mmio_len 0x%llX\n", mmio_start, mmio_len);

	if (!(pci_resource_flags(pdev, bar_zero) & IORESOURCE_MEM)) {
		pci_info(pdev, "BAR %d is not an MMIO resource, aborting\n", bar_zero);
		ret = -ENODEV;
		goto out_pci_dev;
	}

	/* Allocate memory for the driver private data */
	drv_priv = kzalloc(sizeof(*drv_priv), GFP_KERNEL);
	if (!drv_priv) {
		ret = -ENOMEM;
		goto out_pci_dev;
	}

	/* Remap BAR to the local pointer */
	drv_priv->hwmem = ioremap(mmio_start, mmio_len);
	pci_dbg(pdev, "Mapped memory is at 0x%px\n", drv_priv->hwmem);

	if (!drv_priv->hwmem) {
		pci_err(pdev, "Failed to map I/O memory\n");
		ret = -EIO;
		goto out_drv_priv_mem;
	}

	pci_set_dma_mask(pdev, DMA_BIT_MASK(64));

	/* Set driver private data */
	/* Now we can access mapped "hwmem" from the any driver's function */
	pci_set_drvdata(pdev, drv_priv);

	/* Set Avalon-MM-PCI address translation */
	ret = rswitch2_pci_set_a2p_addr_tbl(pdev);
	if (ret != 0)
		goto out_map_iomem;

	ret = rswitch2_pci_get_fw_version(pdev);
	if (ret != 0)
		goto out_map_iomem;

	rswitch2_pci_extend_sysfs(pdev);

	rswitch2_pci_fw_register(drv_priv->hwmem);

	/* Allocate memory for the common rswitch2 data */
	rsw2 = kzalloc(sizeof(*rsw2), GFP_KERNEL);
	if (!rsw2) {
		ret = -ENOMEM;
		goto out_reg_sysfs;
	}
	drv_priv->rsw2 = rsw2;

	/* Allocate base address array for multiple device instances */
	array_size = sizeof(*rsw2->etha_base_addrs) * RSWITCH2_TSN_PORTS;
	rsw2->etha_base_addrs = kzalloc(array_size, GFP_KERNEL);
	if (!rsw2->etha_base_addrs) {
		ret = -ENOMEM;
		goto out_rsw2_mem;
	}

	array_size = sizeof(*rsw2->gwca_base_addrs) * RSWITCH2_CPU_PORTS;
	rsw2->gwca_base_addrs = kzalloc(array_size, GFP_KERNEL);
	if (!rsw2->gwca_base_addrs) {
		ret = -ENOMEM;
		goto out_etha_mem;
	}

	rsw2->dev = &pdev->dev;
	rsw2->base_addr = drv_priv->hwmem + RSWITCH2_PCI_IP_OFFSET;

	rswitch2_pci_set_base_addr(rsw2);

	/* Set number of ports */
	rsw2->num_of_tsn_ports = RSWITCH2_TSN_PORTS;
	rsw2->num_of_cpu_ports = RSWITCH2_CPU_PORTS;

	ret = rswitch2_pci_set_port_data(rsw2);
	if (ret < 0) {
		ret = -ENOMEM;
		goto out_gwca_mem;
	}

	ret = rswitch2_pci_request_irq_line(rsw2);
	if (ret < 0) {
		ret = -ENOMEM;
		goto out_port_mem;
	}

	/* Init RSwitch2 core */
	ret = rswitch2_init(rsw2);
	if (ret < 0) {
		pci_err(pdev, "Failed to initialized RSwitch2 driver: %d\n", ret);
		goto out_req_irq;
	}

	return 0;

out_req_irq:
	rswitch2_pci_release_irq_line(drv_priv->rsw2);

out_port_mem:
	kfree(rsw2->port_data);

out_gwca_mem:
	kfree(rsw2->gwca_base_addrs);

out_etha_mem:
	kfree(rsw2->etha_base_addrs);

out_rsw2_mem:
	kfree(rsw2);

out_reg_sysfs:
	rswitch2_pci_fw_unregister();
	rswitch2_pci_remove_sysfs(pdev);

out_map_iomem:
	iounmap(drv_priv->hwmem);

out_drv_priv_mem:
	kfree(drv_priv);

out_pci_dev:
	release_device(pdev);

out_pci_ident:
	return ret;
}

/* Clean up */
static void rswitch2_pci_remove(struct pci_dev *pdev)
{
	struct rswitch2_pci_driver_priv *drv_priv = pci_get_drvdata(pdev);

	if (drv_priv)
		if (drv_priv->rsw2)
			rswitch2_exit(drv_priv->rsw2);


	rswitch2_pci_fw_unregister();

	rswitch2_pci_remove_sysfs(pdev);

	pci_disable_device(pdev);
	release_device(pdev);
	if (drv_priv) {
		if (drv_priv->rsw2) {
			kfree(drv_priv->rsw2->gwca_base_addrs);
			kfree(drv_priv->rsw2->etha_base_addrs);
			kfree(drv_priv->rsw2);
		}

		if (drv_priv->hwmem)
			iounmap(drv_priv->hwmem);

		rswitch2_pci_release_irq_line(drv_priv->rsw2);

		kfree(drv_priv);
	}
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dennis Ostermann <dennis.ostermann@renesas.com>");
MODULE_DESCRIPTION("RSwitch2 PCI driver");
MODULE_VERSION("0.1");

module_init(rswitch2_pci_driver_init);
module_exit(rswitch2_pci_driver_exit);
