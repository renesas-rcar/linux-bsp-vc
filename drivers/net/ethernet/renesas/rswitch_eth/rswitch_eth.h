/*
*  RSWITCH TSN Header File - API Interface
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

#ifndef __RSWITCH_ETH_H__
#define __RSWITCH_ETH_H__

#include "../../../../../include/linux/renesas_rswitch.h"




#define RSWITCH_ETHERNET_DEVICE_NAME     "rswitch_ctrl"


#define RSWITCH_IOCTL_MAGIC            'E'
#define RSWITCH_IOCTL_START            24

#define RSWITCH_IOCTL_PORTSTART        30



//
// ---- IOCTLs
//




/**
    @brief  Set Static Port Configuration
*/
#define RSWITCH_SET_CONFIG         _IOW(RSWITCH_IOCTL_MAGIC, RSWITCH_IOCTL_START + 10, struct rswitch_Config *) 

/**
    @brief  Query Port Configuration
*/
#define RSWITCH_GET_CONFIG         _IOW(RSWITCH_IOCTL_MAGIC, RSWITCH_IOCTL_START + 11, struct rswitch_Config *)

/**
    @brief  Add Bandwidth reservation to a port
*/



#endif /* __RSWITCH_ETH_H__ */

/*
* Local variables:
* Mode: C
* tab-width: 4
* indent-tabs-mode: nil
* c-basic-offset: 4
* End:
* vim: ts=4 expandtab sw=4
*/

