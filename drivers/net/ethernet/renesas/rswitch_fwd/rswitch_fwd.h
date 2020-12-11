/*
*  Renesas R-SWitch Forwarding Engine (FWDE) Device Driver - API Interface
*
*  Copyright (C) 2014 Renesas Electronics Corporation
*
*  This program is free software; you can redistribute it and/or modify it
*  under the terms and conditions of the GNU General Public License,
*  version 2, as published by the Free Software Foundation.
*
*  This program is distributed in the hope it will be useful, but WITHOUT
*  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
*  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
*  more details.
*  You should have received a copy of the GNU General Public License along with
*  this program; if not, write to the Free Software Foundation, Inc.,
*  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
*
*  The full GNU General Public License is included in this distribution in
*  the file called "COPYING".
*/


#ifndef __RSWITCH_FWD_H__
#define __RSWITCH_FWD_H__


#include"../../../../../include/linux/renesas_rswitch.h"

/*
    IOCTLs specifically for FPGA Gateway Forwarding Block
*/
#define RSWITCH_FWD_DEVICE_NAME      "rswitchfwd_ctrl"


#define RSWITCHFWD_IOCTL_MAGIC              'G'
#define RSWITCHFWD_IOCTL_START              10


/**
    @brief  Get the complete static forwarding engine table
*/
#define RSWITCH_FWD_GET_CONFIG         _IOR(RSWITCHFWD_IOCTL_MAGIC, RSWITCHFWD_IOCTL_START + 1, struct rswitch_fwd_Config *)

/**
    @brief  Set the complete static forwarding engine table
*/
#define RSWITCH_FWD_SET_CONFIG         _IOW(RSWITCHFWD_IOCTL_MAGIC, RSWITCHFWD_IOCTL_START + 2, struct rswitch_fwd_Config *)




#endif /* __RSWITCH_FWD_H__*/

/*
* Local variables:
* Mode: C
* tab-width: 4
* indent-tabs-mode: nil
* c-basic-offset: 4
* End:
* vim: ts=4 expandtab sw=4
*/
