/**
    @brief  PTP 1588 clock for Renesas RSwitch2 on Cetitec




    @author
        
        Asad Kamal
    

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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/wait.h>
#include <linux/netdevice.h>
//#include <linux/renesas_rswitch_platform.h>
#include "../../../include/linux/renesas_rswitch2.h"
#include "../../../include/linux/renesas_vc2_platform.h"
#include "rswitch2_eth.h"
#include "rswitch2_ptp.h"
#include "rswitch2_reg.h"
//#include "rswitch_ptp_main.h"

#include <linux/of.h>
#include <linux/uaccess.h>



#define _DEBUG








/* Global variables & exports */
static struct ptp_rswitch2  * ptp_rswitch2 = NULL;












/*
    Register access functions
*/
static inline void ptp_rswitch2_write(struct ptp_rswitch2 * ptp_rswitch2, u32 data, enum rswitch2_reg reg_offset)
{
    
    
    iowrite32(data, ptp_rswitch2->addr_iomem_gptp + (reg_offset));
    
}


static inline u32 ptp_rswitch2_read(struct ptp_rswitch2 * ptp_rswitch2, enum rswitch2_reg reg_offset)
{
    
    
    
    return ioread32(ptp_rswitch2->addr_iomem_gptp + reg_offset );
    
}














/* Caller must hold lock */
static void ptp_rswitch2_time_read(struct ptp_rswitch2 *ptp_rswitch2, struct timespec *ts)
{

    
    ts->tv_nsec = ptp_rswitch2_read(ptp_rswitch2, PTPGPTPTM00);

    ts->tv_sec = ptp_rswitch2_read(ptp_rswitch2, PTPGPTPTM10) |
    ((s64)ptp_rswitch2_read(ptp_rswitch2, PTPGPTPTM20) << 32);
    
}


/* Caller must hold lock */
static u64 ptp_rswitch2_cnt_read(struct ptp_rswitch2 *ptp_rswitch2)
{
    
    struct timespec ts;
    ktime_t kt;

    ptp_rswitch2_time_read(ptp_rswitch2, &ts);
    kt = timespec_to_ktime(ts);

    return ktime_to_ns(kt);
}





/* Caller must hold lock */
static void ptp_rswitch2_time_write(struct ptp_rswitch2 *ptp_rswitch2, const struct timespec64 *ts)
{


    
    
    /* Reset Timer and load the offset */
    ptp_rswitch2_write(ptp_rswitch2, (0x01), PTPTMDC);
    ptp_rswitch2_write(ptp_rswitch2, 0, PTPTOVC10);
    ptp_rswitch2_write(ptp_rswitch2, 0, PTPTOVC00);
    ptp_rswitch2_write(ptp_rswitch2, (0x01), PTPTMEC);
    ptp_rswitch2_write(ptp_rswitch2, ts->tv_sec, PTPTOVC10);
    ptp_rswitch2_write(ptp_rswitch2, ts->tv_nsec, PTPTOVC00);
#if 0
    /* PPS output reset */
    iowrite32(0x00, ptp_rswitch2->addr_iomem_gptp + GWCPU_GPTP_GTM0);
    iowrite32(0x01, ptp_rswitch2->addr_iomem_gptp + GWCPU_GPTP_TCM0);
    iowrite32(0x00, ptp_rswitch2->addr_iomem_gptp + GWCPU_GPTP_TCST0);
    iowrite32(0x1DCD6500, ptp_rswitch2->addr_iomem_gptp + GWCPU_GPTP_TCD0);
#endif
}


/* Caller must hold lock */
static void ptp_rswitch2_cnt_write(struct ptp_rswitch2 *ptp_rswitch2, u64 ns)
{
    struct timespec ts;
    
    ts = ns_to_timespec(ns);

    ptp_rswitch2_time_write(ptp_rswitch2, &ts);
}




/* Caller must hold lock */
static void ptp_rswitch2_update_addend(struct ptp_rswitch2 *ptp_rswitch2, u32 addend)
{
    
    
        iowrite32(addend & TIV_MASK, ptp_rswitch2->addr_iomem_gptp + PTPTIVC0);
    
    
}


/**
    @brief  Adjust gPTP increment frequency

    @return < 0 on error
*/
static int ptp_rswitch2_adjfreq(struct ptp_clock_info *ptp, s32 ppb)
{
    u64             adj;
    u32             diff;
    u32             addend;
    int             neg_adj = 0;
    unsigned long   flags;
    
    struct ptp_rswitch2 *ptp_rswitch2 = container_of(ptp, struct ptp_rswitch2, caps);

    if (ppb < 0) 
    {
        neg_adj = 1;
        ppb = -ppb;
    }
    addend = ptp_rswitch2->default_addend;
    adj = addend;
    adj *= ppb;
    diff = div_u64(adj, 1000000000ULL);

    addend = neg_adj ? addend - diff : addend + diff;

    spin_lock_irqsave(&ptp_rswitch2->lock, flags);
    ptp_rswitch2_update_addend(ptp_rswitch2, addend);
    spin_unlock_irqrestore(&ptp_rswitch2->lock, flags);

    return 0;
}


/**
    @brief  Adjust gPTP timer

    @return < 0 on error
*/
static int ptp_rswitch2_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
    
    s64 now;
    unsigned long flags;
    
    struct ptp_rswitch2 *ptp_rswitch2 = container_of(ptp, struct ptp_rswitch2, caps);
    
    spin_lock_irqsave(&ptp_rswitch2->lock, flags);
    
    now = ptp_rswitch2_cnt_read(ptp_rswitch2);
    
    now += delta;
    
    ptp_rswitch2_cnt_write(ptp_rswitch2, now);

    spin_unlock_irqrestore(&ptp_rswitch2->lock, flags);

    return 0;
}






static int ptp_rswitch2_gettime(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
    unsigned long flags;
    
    struct ptp_rswitch2 *ptp_rswitch2 = container_of(ptp, struct ptp_rswitch2, caps);
    
    spin_lock_irqsave(&ptp_rswitch2->lock, flags);

    ptp_rswitch2_time_read(ptp_rswitch2, ts);

    spin_unlock_irqrestore(&ptp_rswitch2->lock, flags);

    return 0;
}




static int ptp_rswitch2_settime(struct ptp_clock_info *ptp, const struct timespec64 *ts)
{
    unsigned long flags;
    struct ptp_rswitch2 *ptp_rswitch2 = container_of(ptp, struct ptp_rswitch2, caps);
    
    spin_lock_irqsave(&ptp_rswitch2->lock, flags);

    ptp_rswitch2_time_write(ptp_rswitch2, ts);

    spin_unlock_irqrestore(&ptp_rswitch2->lock, flags);

    return 0;
}


static int ptp_rswitch2_enable(struct ptp_clock_info *ptp, struct ptp_clock_request *rq, int on)
{
    return -EOPNOTSUPP;
}


static struct ptp_clock_info    ptp_rswitch2_caps = 
{
    .owner      = THIS_MODULE,
    .name       = "rswitch2-ptp",             
    .max_adj    = 50000000,
    .n_ext_ts   = 0,
    .adjfreq    = ptp_rswitch2_adjfreq,
    .adjtime    = ptp_rswitch2_adjtime,
    .gettime64    = ptp_rswitch2_gettime,
    .settime64    = ptp_rswitch2_settime,
    .enable     = ptp_rswitch2_enable,
};


int rswitch2ptp_gettime(struct timespec * timespec)
{
    
    ptp_rswitch2_time_read(ptp_rswitch2, timespec);

    return 0;
}







/**
    @brief  PTP Init Function, Called from Eth driver
*/

void ptp_rswitch2_port_init(struct net_device *ndev, struct platform_device *pdev)
{
    struct port_private  *priv = netdev_priv(ndev); 
    priv->ptp.default_addend = ptp_rswitch2->default_addend;
    priv->ptp.clock = ptp_rswitch2->clock;
    priv->ptp.caps = ptp_rswitch2->caps;
}



int ptp_rswitch2_init(struct platform_device *pdev)
{
    int           err = -ENOMEM;


    printk("\n[RSWITCH2_PTP] (%s %s) version %s Probing %s .....\n", __DATE__, __TIME__, RSWITCH2PTP_DRIVER_VERSION, pdev->name);
    
    ptp_rswitch2 = kzalloc(sizeof(*ptp_rswitch2), GFP_KERNEL);
    if (ptp_rswitch2 == 0) {
        kfree(ptp_rswitch2);
        return err;
    }

    err = -ENODEV;

    ptp_rswitch2->caps = ptp_rswitch2_caps;
    
    ptp_rswitch2->addr_iomem_gptp = (void __iomem *)(0x14000 + ioaddr);
#if 0
    if(bitfile_version < RSWITCH_PTP_BITFILE_NEW)
    {
       ptp_rswitch_write(ptp_rswitch, ((1000 * (1 << 20)) / PLATFORM_CLOCK_FREQUENCY_MHZ) & TIV_MASK_OLD, GWCPU_GPTP_GPTI);
    }
    else if(bitfile_version >= RSWITCH_PTP_BITFILE_NEW)
#endif
    {
        ptp_rswitch2_write(ptp_rswitch2, (((((uint64_t)1000000000 * (1 << 27)) / PLATFORM_CLOCK_FREQUENCY_HZ))) & TIV_MASK, PTPTIVC0);
        //printk("[RSWITCH_PTP] Bitfile version %08x TIV= %llx\n", bitfile_version, (((((uint64_t)1000000000 * (1 << 27)) / PLATFORM_CLOCK_FREQUENCY_HZ)*GPTP_CLK_DEV )/1000000) & TIV_MASK );
    }
    

    ptp_rswitch2->default_addend = ptp_rswitch2_read(ptp_rswitch2, PTPTIVC0);

    ptp_rswitch2_write(ptp_rswitch2, (0x01), PTPTMEC);
    /* Register PTP Clock*/
    ptp_rswitch2->clock = ptp_clock_register(&ptp_rswitch2->caps, NULL);
    if (IS_ERR(ptp_rswitch2->clock)) {
        err = PTR_ERR(ptp_rswitch2->clock);
        kfree(ptp_rswitch2);
        return err;
    }
#if 0
    /* Initialise PPS output */
    iowrite32(0x00, ptp_rswitch->addr_iomem_gptp + GWCPU_GPTP_GTM0);
    iowrite32(0x01, ptp_rswitch->addr_iomem_gptp + GWCPU_GPTP_TCM0);
    iowrite32(0x00, ptp_rswitch->addr_iomem_gptp + GWCPU_GPTP_TCST0);
    iowrite32(0x1DCD6500, ptp_rswitch->addr_iomem_gptp + GWCPU_GPTP_TCD0);
    printk("[RSWITCH_PTP] ptp_clock_registered for PLATFORM_CLOCK_FREQUENCY_HZ=%d TIV= %x\n", PLATFORM_CLOCK_FREQUENCY_HZ, ptp_rswitch->default_addend);
#endif
    

    return 0;
}







int ptp_rswitch2_remove(struct platform_device *pdev)
{
    

    ptp_clock_unregister(ptp_rswitch2->clock);
    if (ptp_rswitch2->addr_iomem_gptp != NULL)
    {
        
        ptp_rswitch2->addr_iomem_gptp = NULL;
    }

    if (ptp_rswitch2 != NULL)
    {
        kfree(ptp_rswitch2);
        ptp_rswitch2 = NULL;
    }
    
    
    return 0;
}




struct platform_device *pdev;





/*
    Change History
    2020-11-19  0.0.1      Initial test Version Untested No pps support
    2020-11-25  0.0.1      Bug fixes while testing

*/



/*
* Local variables:
* Mode: C
* tab-width: 4
* indent-tabs-mode: nil
* c-basic-offset: 4
* End:
* vim: ts=4 expandtab sw=4
*/

