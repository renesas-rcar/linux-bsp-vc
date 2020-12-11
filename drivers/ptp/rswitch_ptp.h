/**
    @brief  PTP 1588 clock for Renesas ACE R SWITCH - external header

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

#ifndef __RSWITCH_PTP_H__
#define __RSWITCH_PTP_H__


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ptp_clock_kernel.h>




/**
    @brief  PTP definition

    
*/

struct ptp_rswitch 
{
    void __iomem          * addr_iomem_gPTP;
    void __iomem          * addr_iomem_PTP_FIFO;
    spinlock_t              lock; /* protects regs */
    struct ptp_clock      * clock;
    struct ptp_clock_info   caps;
    struct resource       * rsrc_iomem_gPTP;
    struct resource       * rsrc_iomem_PTP_FIFO;
    u32                     default_addend;
    int                     irq;
};




// ----- Exported Functions
extern int rswitchptp_GetTime(struct timespec * TimeSpec);
extern void ptp_rswitch_init(struct net_device *ndev, struct platform_device *pdev); 



#endif /* __RSWITCH_PTP_H__ */

/*
* Local variables:
* Mode: C
* tab-width: 4
* indent-tabs-mode: nil
* c-basic-offset: 4
* End:
* vim: ts=4 expandtab sw=4
*/
