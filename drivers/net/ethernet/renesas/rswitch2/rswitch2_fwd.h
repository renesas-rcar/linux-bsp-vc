// SPDX-License-Identifier: GPL-2.0-only
/*
 * Renesas R-SWitch2 Forwarding Engine (FWDE) Device Driver - API Interface
 *
 * Copyright (C) 2014 Renesas Electronics Corporation
 */

#ifndef __RSWITCH2_FWD_H__
#define __RSWITCH2_FWD_H__
#define RSWITCH2FWD_CLASS "rswitch2fwd"
#define RSWITCH2_FWD_DEVICE_NAME "rswitch2fwd_ctrl"

#define RSWITCH2FWD_IOCTL_MAGIC 'G'
#define RSWITCH2FWD_IOCTL_START 10

#include"../../../../../include/linux/renesas_rswitch2.h"

struct rswitch2_fwd_filter_src_valid {
	char agent_name[5];
	uint32_t set_any;
	char stream_str[5];
};
/* Get the complete static forwarding engine table */
#define RSWITCH2_FWD_GET_CONFIG		_IOR(RSWITCH2FWD_IOCTL_MAGIC, \
					     RSWITCH2FWD_IOCTL_START + 1, \
					     struct rswitch2_fwd_Config *)

/* Set the complete static forwarding engine table */
#define RSWITCH2_FWD_SET_CONFIG		_IOW(RSWITCH2FWD_IOCTL_MAGIC, \
					     RSWITCH2FWD_IOCTL_START + 2, \
					     struct rswitch2_fwd_Config *)
#endif
