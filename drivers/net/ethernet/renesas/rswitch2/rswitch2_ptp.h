/**
    @brief  PTP 1588 clock for Renesas R Switch2 header

    Copyright (C) 2014 Renesas Electronics Corporation

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __RSWITCH2_PTP_H__
#define __RSWITCH2_PTP_H__

#include <linux/ptp_clock_kernel.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define RSWITCH2PTP_DRIVER_VERSION          "0.0.1"







/**
    @brief  PTP definition

    
*/

struct ptp_rswitch2
{
    void __iomem          * addr_iomem_gptp;
    spinlock_t              lock; /* protects regs */
    struct ptp_clock      * clock;
    struct ptp_clock_info   caps;
    u32                     default_addend;
    int                     irq;
};



#ifndef NOT
#define  NOT !
#endif

#ifndef AND
#define AND &&
#endif

#ifndef OR
#define OR ||
#endif



/* 
    Bit definitions for the GPTI register 
*/
#define TIV_MASK                (0xFFFFFFFF)
#define TIV_MASK_OLD            (0x0FFFFFFF)
#define GPTP_CLK_DEV            1000001












#endif /* __RSWITCH2_PTP_H__ */

/*
* Local variables:
* Mode: C
* tab-width: 4
* indent-tabs-mode: nil
* c-basic-offset: 4
* End:
* vim: ts=4 expandtab sw=4
*/
