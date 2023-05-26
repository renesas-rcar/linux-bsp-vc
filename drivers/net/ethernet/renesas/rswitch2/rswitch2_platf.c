// SPDX-License-Identifier: GPL-2.0
/* Renesas RSwitch2 platform device driver
 *
 * Copyright (C) 2021, 2022 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License version 2,
 * as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/phy.h>

#include "rswitch2.h"
#include "rswitch2_platf.h"


static const u8 rsw2_default_mac[ETH_ALEN] __aligned(2) = {
	0x74, 0x90, 0x50, 0x00, 0x00, 0x00
};

#define RSW2_INTERNAL_PORT_MARKER 0xCC

static int log_level_gen = LOGLEVEL_ERR;
module_param(log_level_gen,int,0660);

static int log_level_desc = LOGLEVEL_ERR;
module_param(log_level_desc,int,0660);

static int log_level_rxtx = LOGLEVEL_ERR;
module_param(log_level_rxtx,int,0660);

static int log_level_fwd = LOGLEVEL_ERR;
module_param(log_level_fwd,int,0660);

static int log_level_serdes = LOGLEVEL_ERR;
module_param(log_level_serdes,int,0660);


/* Device driver's private data structure */
struct rswitch2_platf_driver_priv {
	struct rswitch2_drv *rsw2;
	struct clk *rsw_clk;
	struct clk *phy_clk;
};

#ifdef RSW2_NEW_MII
static int rswitch2_mii_read(struct mii_bus *bus, int addr, int regnum)
{
	struct rswitch_etha *etha = bus->priv;
	int mode, devad, regad;

	mode = regnum & MII_ADDR_C45;
	devad = (regnum >> MII_DEVADDR_C45_SHIFT) & 0x1f;
	regad = regnum & MII_REGADDR_C45_MASK;

	/* Not support Clause 22 access method */
	if (!mode)
		return 0;

	return rswitch_etha_set_access(etha, true, addr, devad, regad, 0);
}

static int rswitch2_mii_write(struct mii_bus *bus, int addr, int regnum, u16 val)
{
	struct rswitch_etha *etha = bus->priv;
	int mode, devad, regad;

	mode = regnum & MII_ADDR_C45;
	devad = (regnum >> MII_DEVADDR_C45_SHIFT) & 0x1f;
	regad = regnum & MII_REGADDR_C45_MASK;

	/* Not support Clause 22 access method */
	if (!mode)
		return 0;

	return rswitch_etha_set_access(etha, false, addr, devad, regad, val);
}

static int rswitch2_mii_reset(struct mii_bus *bus)
{
	/* TODO */
	return 0;
}



/* FIXME: Per Port!!! */
static int rswitch2_register_mii_bus(struct rswitch2_drv *rsw2)
{
	struct mii_bus *mii_bus;
	struct device_node *port;
	int err;

	mii_bus = mdiobus_alloc();
	if (!mii_bus)
		return -ENOMEM;

	mii_bus->name = "rswitch2_mii";
	sprintf(mii_bus->id, "etha%d", rdev->etha->index);
	mii_bus->priv = rdev->etha;
	mii_bus->read = rswitch2_mii_read;
	mii_bus->write = rswitch2_mii_write;
	mii_bus->reset = rswitch2_mii_reset;
	mii_bus->parent = &rdev->ndev->dev;

	port = rswitch_get_port_node(rdev);
	of_node_get(port);
	err = of_mdiobus_register(mii_bus, port);
	if (err < 0) {
		mdiobus_free(mii_bus);
		goto out;
	}

	rdev->etha->mii = mii_bus;

out:
	of_node_put(port);

	return err;
}
#endif



static void rswitch2_platf_set_base_addr(struct rswitch2_drv *rsw2)
{
	u8 __iomem *base_addr = rsw2->base_addr;

	rsw2->fwd_base_addr = base_addr + RSWITCH2_MFWD_BASE_ADDR;
	rsw2->fab_base_addr = base_addr + RSWITCH2_FAB_BASE_ADDR;
	rsw2->coma_base_addr = base_addr + RSWITCH2_COMA_BASE_ADDR;
	rsw2->etha_base_addrs[0] = base_addr + RSWITCH2_TSNA0_BASE_ADDR;
	rsw2->etha_base_addrs[1] = base_addr + RSWITCH2_TSNA1_BASE_ADDR;
	rsw2->etha_base_addrs[2] = base_addr + RSWITCH2_TSNA2_BASE_ADDR;
	rsw2->gwca_base_addrs[0] = base_addr + RSWITCH2_GWCA0_BASE_ADDR;
#if RSWITCH2_CPU_PORTS > 1
	rsw2->gwca_base_addrs[1] = base_addr + RSWITCH2_GWCA1_BASE_ADDR;
#endif
	rsw2->ptp_base_addr = base_addr + RSWITCH2_GPTP_BASE_ADDR;
}

static int rswitch2_platf_request_irqs(struct rswitch2_drv *rsw2)
{
	struct platform_device *pdev;
	const struct device_node *dn_irqs;
	int dt_prop_len;
	const char **irq_names;
	int dt_max_irqs, dt_cur_irq;
	int irq;
	int ret;


	pdev = container_of(rsw2->dev, struct platform_device, dev);

	dn_irqs = of_get_property(rsw2->dev->of_node, "interrupt-names", &dt_prop_len);
	if (!dn_irqs) {
		rsw2_err(MSG_GEN, "No irqs specified in device tree\n");
		return -EINVAL;
	}
	else {
		rsw2_dbg(MSG_GEN, "Got irq array of len %d  from device tree nodes\n", dt_prop_len);
	}

	irq_names = devm_kcalloc(&pdev->dev, RSWITCH2_MAX_IRQS,
			       sizeof(*irq_names), GFP_KERNEL);
	if (!irq_names)
		return -ENOMEM;


	ret = of_property_read_string_array(rsw2->dev->of_node, "interrupt-names",
			irq_names, RSWITCH2_MAX_IRQS);
	if (ret < 0)
		return ret;

	dt_max_irqs = (unsigned int)ret;

	for(dt_cur_irq = 0; dt_cur_irq < dt_max_irqs; dt_cur_irq++) {
		size_t prop_str_len;

		prop_str_len = strlen(irq_names[dt_cur_irq]);

		rsw2_info(MSG_GEN, "Got irq #%.02d '%s'\n", dt_cur_irq, irq_names[dt_cur_irq]);

		/* FIXME: Check if DT string is shorter than sizeof(RSW2_GWCA0_NAME) - 1 */
		if(prop_str_len > sizeof(RSW2_GWCA0_NAME)) {

			ret = strncmp(irq_names[dt_cur_irq], RSW2_GWCA0_NAME, sizeof(RSW2_GWCA0_NAME) - 1);
			if(ret == 0) {
				const char *irq_type_str = irq_names[dt_cur_irq] + sizeof(RSW2_GWCA0_NAME);

				irq = platform_get_irq_byname(pdev, irq_names[dt_cur_irq]);
				if (irq < 0) {
					rsw2_err(MSG_GEN, "Failed to get IRQ\n");
					return -EINVAL; /* FIXME: error handling / memory */
				}
				rsw2_dbg(MSG_GEN, "Got irq %d '%s' from DT index %d\n", irq, irq_names[dt_cur_irq], dt_cur_irq);

				ret = strncmp(irq_type_str, "rxtx", 4);
				if(ret == 0) {
					rsw2_info(MSG_GEN, "Registering RXTX irq #%.02d '%s'\n", dt_cur_irq, irq_names[dt_cur_irq]);
					if(rsw2->num_of_rxtx_irqs >=  RSWITCH2_MAX_RXTX_IRQS) {
						rsw2_err(MSG_GEN, "Too many RXTX interrupts\n");
						// FIXME: free memory
						return -EINVAL;

					}
					rsw2->rxtx_irqs[rsw2->num_of_rxtx_irqs] = irq;
					rsw2->num_of_rxtx_irqs++;

				}
				else {
					rsw2_info(MSG_GEN, "Registering status irq #%.02d '%s'\n", dt_cur_irq, irq_names[dt_cur_irq]);
					if(rsw2->num_of_status_irqs >=  RSWITCH2_MAX_STATUS_IRQS) {
						rsw2_err(MSG_GEN, "Too many status interrupts\n");

						// FIXME: free memory
						return -EINVAL;
					}
					rsw2->status_irqs[rsw2->num_of_status_irqs] = irq;
					rsw2->num_of_status_irqs++;

				}
			}
		}
	}

	devm_kfree(&pdev->dev, irq_names);

	return 0;
}

static void rswitch2_platf_release_irqs(struct rswitch2_drv *rsw2)
{
	// FIXME
}

#ifdef OLD_STUFF
static phy_interface_t rswitch2_get_phy_inferface(const char *phy_mode_str) {

	int i;
	phy_interface_t intf = PHY_INTERFACE_MODE_NA;
	const int elem = ARRAY_SIZE(rsw2_phy_mode_xlate_tbl);

	for(i = 0; i < elem; i++) {
		int ret;
		ret = strcmp(phy_mode_str, rsw2_phy_mode_xlate_tbl[i].dt_str);
		//printk("Comparing '%s' <-> '%s'\n", phy_mode_str, rsw2_phy_mode_xlate_tbl[i].dt_str);
		if (ret == 0) {
			intf = rsw2_phy_mode_xlate_tbl[i].intf;
			break;
		}
	}

	return intf;
}
#endif

static int rswitch2_platf_set_port_data(struct rswitch2_drv *rsw2)
{
//	int dt_port_num;
//	unsigned int cur_port_num;
//	unsigned int total_ports;
	struct platform_device *pdev;
//	struct rswitch2_port_data *cur_port_data;
//	struct device_node *ports, *port;

	int ret = 0;


	pdev = container_of(rsw2->dev, struct platform_device, dev);
	rsw2_dbg(MSG_GEN, "rswitch2_platf_set_port_data(): pdev is at 0x%px\n", pdev);


	//total_ports = rsw2->num_of_cpu_ports + rsw2->num_of_tsn_ports;
#ifdef  RSW2_DEPRECATED
	rsw2->port_data = kcalloc(total_ports, sizeof(*cur_port_data), GFP_KERNEL);
	if (!rsw2->port_data)
		return -ENOMEM;

	memset(rsw2->port_data, 0, sizeof(*cur_port_data));


	for (cur_port_num = 0; cur_port_num < total_ports; cur_port_num++) {

		cur_port_data = &rsw2->port_data[cur_port_num];
		memcpy(cur_port_data->mac_addr, rsw2_default_mac,
				sizeof(cur_port_data->mac_addr));
		cur_port_data->mac_addr[ETH_ALEN - 1] = (u8)(cur_port_num);

		if (cur_port_num == 0)
			cur_port_data->mac_addr[ETH_ALEN - 2] = RSW2_INTERNAL_PORT_MARKER;
//		else
//			cur_port_data->phy_iface = PHY_INTERFACE_MODE_RGMII;

		pr_info("Port-%d MAC %.2X:%.2X:%.2X:%.2X:%.2X:%.2X \n", cur_port_num,
				cur_port_data->mac_addr[0], cur_port_data->mac_addr[1],
				cur_port_data->mac_addr[2], cur_port_data->mac_addr[3],
				cur_port_data->mac_addr[4], cur_port_data->mac_addr[5]
						);
	}
#endif /* RSW2_DEPRECATED */
#ifdef OLD_STUFF

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



	ports = of_get_child_by_name(rsw2->dev->of_node, "ports");
	if (!ports) {
		dev_err(rsw2->dev, "No ports specified in device tree\n");
		return -EINVAL;
	}
	//else {
	//	dev_err(rsw2->dev, "Got ports from device tree nodes\n");
	//}


	dt_port_num = of_get_child_count(ports);
	if(dt_port_num > rsw2->num_of_tsn_ports) {
		dev_err(rsw2->dev, "%d ports specified in device tree, but maximum %d ports supported.\n",
				dt_port_num, rsw2->num_of_tsn_ports);
		return -EINVAL;
	}
	else {
		dev_info(rsw2->dev, "DT specifies %d Ethernet ports\n", dt_port_num);
	}


	cur_port_num = 0;

	for_each_child_of_node(ports, port) {
		const char *prop_phy_mode_str;

		cur_port_data = &rsw2->port_data[cur_port_num];
		//cur_port_data->phy_id;

		ret = of_property_read_u32(port, "reg", &cur_port_data->reg);
		if (ret < 0) {
			dev_err(rsw2->dev, "Failed to get register property for port %d: %d\n",
					cur_port_num, ret);
			break;
		}
		//else {
		//	dev_info(rsw2->dev, "Got reg=%u for port %d\n", cur_port_data->reg, cur_port_num);
		//}

		ret = of_property_read_string(port, "phy-mode", &prop_phy_mode_str);
		if (ret < 0) {
			dev_err(rsw2->dev, "Failed to get phy mode property for port %d: %d\n",
					cur_port_num, ret);
			break;
		}
		dev_err(rsw2->dev, "DT phy mode: etha%u '%s'\n", cur_port_data->reg, prop_phy_mode_str);

		cur_port_data->phy_iface = rswitch2_get_phy_inferface(prop_phy_mode_str);
		if (cur_port_data->phy_iface == PHY_INTERFACE_MODE_NA) {
			dev_err(rsw2->dev, "Invalid phy mode provided for port %d\n", cur_port_num);
			ret = -EINVAL;
		}
		cur_port_num++;
	}

	of_node_put(ports);
#endif

// FIXME: Free port data on error

	return ret;
}


static int rswitch2_platf_probe(struct platform_device *pdev)
{
	int ret;
	struct rswitch2_platf_driver_priv *drv_priv;
	struct rswitch2_drv *rsw2;
	size_t array_size;

	struct resource *res_rsw2, *res_serdes, *res_sram;

	res_rsw2 = platform_get_resource_byname(pdev, IORESOURCE_MEM, "iobase");
	res_serdes = platform_get_resource_byname(pdev, IORESOURCE_MEM, "serdes");
	res_sram = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sram");
	if (!res_rsw2 || !res_serdes || !res_sram) {
		dev_err(&pdev->dev, "Invalid resources, check device tree\n");
		return -EINVAL;
	}


	drv_priv = devm_kzalloc(&pdev->dev, sizeof(*drv_priv), GFP_KERNEL);
	if (!drv_priv) {
		return -ENOMEM;
		// FIXME: Clean up
	}

	drv_priv->rsw_clk = devm_clk_get(&pdev->dev, "rsw2");
	if (IS_ERR(drv_priv->rsw_clk)) {
		dev_err(&pdev->dev, "Failed to get rsw2 clock: %ld\n", PTR_ERR(drv_priv->rsw_clk));
		return PTR_ERR(drv_priv->rsw_clk);
	}

	drv_priv->phy_clk = devm_clk_get(&pdev->dev, "eth-phy");
	if (IS_ERR(drv_priv->phy_clk)) {
		dev_err(&pdev->dev, "Failed to get eth-phy clock: %ld\n", PTR_ERR(drv_priv->phy_clk));
		return PTR_ERR(drv_priv->phy_clk);
	}

	/* Allocate memory for the common rswitch2 data */
	rsw2 = kzalloc(sizeof(*rsw2), GFP_KERNEL);
	if (!rsw2) {
		ret = -ENOMEM;
		goto out_drv_priv_mem;
	}
	drv_priv->rsw2 = rsw2;

	/* Set default logging levels */
	rsw2->sec_log_lvl[MSG_GEN] = log_level_gen;
	rsw2->sec_log_lvl[MSG_DESC] = log_level_desc;
	rsw2->sec_log_lvl[MSG_RXTX] = log_level_rxtx;
	rsw2->sec_log_lvl[MSG_FWD] = log_level_fwd;
	rsw2->sec_log_lvl[MSG_SERDES] = log_level_serdes;

	/* Set driver private data */
	platform_set_drvdata(pdev, drv_priv);

	rsw2->sd_rst = devm_reset_control_get(&pdev->dev, "eth-phy");
	if (IS_ERR(rsw2->sd_rst)) {
		dev_err(&pdev->dev, "Failed to get reset control: %ld\n", PTR_ERR(rsw2->sd_rst));
		return PTR_ERR(rsw2->sd_rst);
	}
	//drv_priv->pdev = pdev;
	rsw2->base_addr = devm_ioremap_resource(&pdev->dev, res_rsw2);
	if (IS_ERR(rsw2->base_addr))
		return PTR_ERR(rsw2->base_addr);

	rsw2->serdes_base_addr = devm_ioremap_resource(&pdev->dev, res_serdes);
	if (IS_ERR(rsw2->serdes_base_addr))
		return PTR_ERR(rsw2->serdes_base_addr);

	rsw2->sram_base_addr = devm_ioremap_resource(&pdev->dev, res_sram);
	if (IS_ERR(rsw2->sram_base_addr))
		return PTR_ERR(rsw2->sram_base_addr);

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

	rswitch2_platf_set_base_addr(rsw2);

	/* Set number of ports */
	rsw2->num_of_tsn_ports = RSWITCH2_TSN_PORTS;
	rsw2->num_of_cpu_ports = RSWITCH2_CPU_PORTS;

	/* TODO: Get data from DT 	 */
	ret = rswitch2_platf_set_port_data(rsw2);
	if (ret < 0) {
		ret = -ENOMEM;
		goto out_gwca_mem;
	}

	ret = rswitch2_platf_request_irqs(rsw2);
	if (ret < 0) {
		ret = -ENOMEM;
		goto out_port_mem;
	}

	rsw2_dbg(MSG_GEN, "pdev->dev.power.disable_depth:: %d\n", pdev->dev.power.disable_depth);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);
	clk_prepare(drv_priv->phy_clk);
	clk_enable(drv_priv->phy_clk);

	rsw2_dbg(MSG_GEN,"pdev->dev.power.disable_depth:: %d\n", pdev->dev.power.disable_depth);


	device_set_wakeup_capable(&pdev->dev, 1);

	/* Init RSwitch2 core */
	ret = rswitch2_init(rsw2);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Failed to initialized RSwitch2 driver: %d\n", ret);
		goto out_req_irq;
	}

	return 0;

out_req_irq:
	rswitch2_platf_release_irqs(drv_priv->rsw2);

out_port_mem:
//	kfree(rsw2->port_data);

out_gwca_mem:
	kfree(rsw2->gwca_base_addrs);

out_etha_mem:
	kfree(rsw2->etha_base_addrs);

out_rsw2_mem:
	kfree(rsw2);

out_drv_priv_mem:
	devm_kfree(&pdev->dev, drv_priv);

	return ret;
}

/* Clean up */
//static void rswitch2_pci_remove(struct pci_dev *pdev)
static int rswitch2_platf_remove(struct platform_device *pdev)
{
	struct rswitch2_platf_driver_priv *drv_priv = platform_get_drvdata(pdev);


	if (drv_priv) {
		if (drv_priv->rsw2) {
			struct rswitch2_drv *rsw2 = drv_priv->rsw2;


			rswitch2_exit(drv_priv->rsw2);

			rswitch2_platf_release_irqs(drv_priv->rsw2);

			rsw2_dbg(MSG_GEN, "pdev->dev.power.disable_depth:: %d\n", pdev->dev.power.disable_depth);

			pm_runtime_put(&pdev->dev);
			pm_runtime_disable(&pdev->dev);
			clk_disable(drv_priv->phy_clk);

			rsw2_dbg(MSG_GEN, "pdev->dev.power.disable_depth:: %d\n", pdev->dev.power.disable_depth);
		}
	}

	if (drv_priv) {
		if (drv_priv->rsw2) {
//			iounmap(drv_priv->rsw2->base_addr);
//			iounmap(drv_priv->rsw2->serdes_base_addr);

			kfree(drv_priv->rsw2->gwca_base_addrs);
			kfree(drv_priv->rsw2->etha_base_addrs);
			kfree(drv_priv->rsw2);
		}


		devm_kfree(&pdev->dev, drv_priv);
		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

#ifdef OLD_STUFF
static int renesas_eth_sw_probe(struct platform_device *pdev)
{
	struct rswitch_private *priv;
	struct resource *res, *res_serdes;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "iobase");
	res_serdes = platform_get_resource_byname(pdev, IORESOURCE_MEM, "serdes");
	if (!res || !res_serdes) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->rsw_clk = devm_clk_get(&pdev->dev, "rsw2");
	if (IS_ERR(priv->rsw_clk)) {
		dev_err(&pdev->dev, "Failed to get rsw2 clock: %ld\n", PTR_ERR(priv->rsw_clk));
		return PTR_ERR(priv->rsw_clk);
	}

	priv->phy_clk = devm_clk_get(&pdev->dev, "eth-phy");
	if (IS_ERR(priv->phy_clk)) {
		dev_err(&pdev->dev, "Failed to get eth-phy clock: %ld\n", PTR_ERR(priv->phy_clk));
		return PTR_ERR(priv->phy_clk);
	}

	platform_set_drvdata(pdev, priv);
	priv->pdev = pdev;
	priv->addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->addr))
		return PTR_ERR(priv->addr);

	priv->serdes_addr = devm_ioremap_resource(&pdev->dev, res_serdes);
	if (IS_ERR(priv->serdes_addr))
		return PTR_ERR(priv->serdes_addr);

	debug_addr = priv->addr;

	/* Fixed to use GWCA0 */
	priv->gwca.index = 3;
	priv->gwca.num_chains = num_ndev * NUM_CHAINS_PER_NDEV;
	priv->gwca.chains = devm_kcalloc(&pdev->dev, priv->gwca.num_chains,
					 sizeof(*priv->gwca.chains), GFP_KERNEL);
	if (!priv->gwca.chains)
		return -ENOMEM;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);
	clk_prepare(priv->phy_clk);
	clk_enable(priv->phy_clk);

	rswitch_init(priv);

	device_set_wakeup_capable(&pdev->dev, 1);

	return 0;
}

static int rswitch2_platf_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct rswitch_device *rdev = netdev_priv(ndev);
	struct rswitch_private *priv = rdev->priv;

	/* Disable R-Switch clock */
	rs_write32(RCDC_RCD, rdev->priv->addr + RCDC);

	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	clk_disable(priv->phy_clk);

	dma_free_coherent(ndev->dev.parent, priv->desc_bat_size, priv->desc_bat,
			  priv->desc_bat_dma);

	unregister_netdev(ndev);
	netif_napi_del(&rdev->napi);
	free_netdev(ndev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}
#endif /* OLD_STUFF */

static const struct of_device_id rswitch2_platf_of_table[] = {
	{ .compatible = "renesas,rswitch2", },
	{ }
};
MODULE_DEVICE_TABLE(of, rswitch2_platf_of_table);


static struct platform_driver rswitch2_platf_drv = {
	.probe = rswitch2_platf_probe,
	.remove = rswitch2_platf_remove,
	.driver = {
		.name = "RSwitch2 platform driver",
		.of_match_table = rswitch2_platf_of_table,
	}
};
module_platform_driver(rswitch2_platf_drv);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dennis Ostermann <dennis.ostermann@renesas.com>");
MODULE_DESCRIPTION("RSwitch2 platform driver");
MODULE_VERSION("0.20");
