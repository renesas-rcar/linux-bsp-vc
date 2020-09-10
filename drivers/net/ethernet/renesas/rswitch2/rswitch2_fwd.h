/*
*  Renesas R-SWitch2 Forwarding Engine (FWDE) Device Driver - API Interface
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


#ifndef __RSWITCH2_FWD_H__
#define __RSWITCH2_FWD_H__
#define RSWITCH2FWD_CLASS                "rswitch2fwd"
#define RSWITCH2_FWD_DEVICE_NAME          "rswitch2fwd_ctrl"


#define RSWITCH2FWD_IOCTL_MAGIC              'G'
#define RSWITCH2FWD_IOCTL_START              10

#include"../../../../../include/linux/renesas_rswitch2.h"
/**
    @brief  Get the complete static forwarding engine table
*/
#define RSWITCH2_FWD_GET_CONFIG         _IOR(RSWITCH2FWD_IOCTL_MAGIC, RSWITCH2FWD_IOCTL_START + 1, struct rswitch2_fwd_Config *)

/**
    @brief  Set the complete static forwarding engine table
*/
#define RSWITCH2_FWD_SET_CONFIG         _IOW(RSWITCH2FWD_IOCTL_MAGIC, RSWITCH2FWD_IOCTL_START + 2, struct rswitch2_fwd_Config *)


#endif


/*
    Change History
    2020-08-10    AK  Initial Version


*/
