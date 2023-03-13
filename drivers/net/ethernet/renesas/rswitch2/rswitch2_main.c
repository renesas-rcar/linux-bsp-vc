// SPDX-License-Identifier: GPL-2.0
/* Renesas RSwitch2 Ethernet CPU port device driver
 *
 * Copyright (C) 2019-2021 Renesas Electronics Corporation
 *
 */
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/io.h>

#include "../rtsn_ptp.h"
#include "rswitch2_fwd.h"
#include "rswitch2_eth.h"
#include "rswitch2_coma.h"

static void rswitch2_reset(struct rswitch2_drv *rsw2)
{
	iowrite32(RRC_RR, rsw2->coma_base_addr + RSW2_COMA_RRC);
	iowrite32(0x0, rsw2->coma_base_addr + RSW2_COMA_RRC);
}

static void rswitch2_clock_enable(struct rswitch2_drv *rsw2)
{
	u32 bitmask;
	u32 reg_val;
	unsigned int num_of_ports;

	/* Enable clock on all ports */
	num_of_ports = rsw2->num_of_tsn_ports + rsw2->num_of_cpu_ports;
	bitmask = (1 << num_of_ports) - 1;

	reg_val = FIELD_PREP(RCEC_ACE, bitmask);
	reg_val |= RCEC_RCE;
	iowrite32(reg_val, rsw2->coma_base_addr + RSW2_COMA_RCEC);
}

static void rswitch2_clock_disable(struct rswitch2_drv *rsw2)
{
	u32 bitmask;
	u32 reg_val;
	unsigned int num_of_ports;

	/* Disable clock on all ports */
	num_of_ports = rsw2->num_of_tsn_ports + rsw2->num_of_cpu_ports;
	bitmask = (1 << num_of_ports) - 1;

	reg_val = FIELD_PREP(RCDC_ACD, bitmask);
	reg_val |= RCDC_RCD;
	iowrite32(0, rsw2->coma_base_addr + RSW2_COMA_RCDC);
}

static int rswitch2_init_buffer_pool(struct rswitch2_drv *rsw2)
{
	int ret;
	u32 reg_val;

	reg_val = ioread32(rsw2->coma_base_addr + RSW2_COMA_CABPIRM);
	if ((reg_val & CABPIRM_BPR) == CABPIRM_BPR) {
		rsw2_err(MSG_GEN, "Buffer pool: Invalid state\n");
		return -EINVAL;
	}

	iowrite32(CABPIRM_BPIOG, rsw2->coma_base_addr + RSW2_COMA_CABPIRM);

	ret = readl_poll_timeout(rsw2->coma_base_addr + RSW2_COMA_CABPIRM, reg_val,
							reg_val & CABPIRM_BPR,
							RSWITCH2_REG_POLL_DELAY, RSWITCH2_REG_POLL_TIMEOUT);
	if (ret != 0) {
		rsw2_err(MSG_GEN, "Initialization of buffer pools timed out\n");
		return ret;
	}

	return 0;
}

int rswitch2_init(struct rswitch2_drv *rsw2)
{
	int ret;

	rswitch2_reset(rsw2);

	ret = rswitch2_init_buffer_pool(rsw2);
	if (ret < 0)
		return ret;

	rswitch2_clock_enable(rsw2);

	rsw2->ptp_drv = rtsn_ptp_alloc_dev(rsw2->dev);
	if (!rsw2->ptp_drv) {
		rsw2_err(MSG_GEN, "Failed to allocate PTP driver struct\n");
		goto err_ptp_init;
	}
	rsw2->ptp_drv->addr = rsw2->ptp_base_addr;

	ret = rtsn_ptp_init(rsw2->ptp_drv, RTSN_PTP_REG_LAYOUT_S4, RTSN_PTP_CLOCK_S4);
	if(ret != 0) {
		rsw2_err(MSG_GEN, "Failed to initialize PTP clock: %d\n", ret);
		goto err_ptp_init;
	}

	/* static configuration of the PPS output */
	iowrite32(0, rsw2->sram_base_addr + RSW2_RSW0PPS0R0);  //1st output relates to timer 0
	iowrite32(1, rsw2->sram_base_addr + RSW2_RSW0PPS1R0);  //2nd output relates to timer 1

	rsw2_notice(MSG_GEN, "PTP clock initialized\n");


	ret = rswitch2_fwd_init(rsw2);
	if (ret < 0)
		goto err_fwd_init;

	ret = rswitch2_eth_init(rsw2);
	if (ret < 0) {
		rsw2_err(MSG_GEN, "Failed to init ethernet driver: %d\n", ret);
		goto err_eth_init;
	}

	return 0;

err_eth_init:
	rswitch2_fwd_exit(rsw2);
err_fwd_init:
	kfree(rsw2->ptp_drv);
err_ptp_init:
	rswitch2_clock_disable(rsw2);
	rswitch2_reset(rsw2);

	return ret;
}

void rswitch2_exit(struct rswitch2_drv *rsw2)
{
	rswitch2_fwd_exit(rsw2);

	rswitch2_eth_exit(rsw2);
}


