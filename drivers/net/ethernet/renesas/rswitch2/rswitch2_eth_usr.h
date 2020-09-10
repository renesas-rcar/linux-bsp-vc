/*
*  Renesas R-Switch2 Ethernet (RSWITCH) Device Driver
*
*  Copyright (C) 2020 Renesas Electronics Corporation
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


#ifndef __RSWITCH2_ETH_USR_H__
#define __RSWITCH2_ETH_USR_H__



#define RSWITCH2_ETHERNET_DEVICE_NAME     "rswitch_ctrl"


#define RSWITCH2_IOCTL_MAGIC            'E'
#define RSWITCH2_IOCTL_START            24

#define RSWITCH2_IOCTL_PORTSTART        30



//
// ---- IOCTLs
//




/**
    @brief  Set Static Port Configuration
*/
#define RSWITCH2_SET_CONFIG         _IOW(RSWITCH2_IOCTL_MAGIC, RSWITCH2_IOCTL_START + 10,  struct rswitch2_eth_config *) 

/**
    @brief  Query Port Configuration
*/
#define RSWITCH2_GET_CONFIG         _IOW(RSWITCH2_IOCTL_MAGIC, RSWITCH2_IOCTL_START + 11, struct rswitch2_eth_config *)



#endif

