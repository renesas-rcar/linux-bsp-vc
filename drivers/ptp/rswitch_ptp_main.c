/**
    @brief  PTP 1588 clock for Renesas RSwitch on Cetitec

    Includes Timestamp FIFO and external access


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
#include <linux/renesas_rswitch_platform.h>
#include "../../../include/linux/renesas_rswitch.h"
#include "../net/ethernet/renesas/rswitch_eth/rswitch_eth_main.h"
#include "rswitch_ptp.h"
#include "rswitch_ptp_main.h"

#include <linux/of.h>
#include <linux/uaccess.h>



#define _DEBUG








/* Global variables & exports */
static struct ptp_rswitch  * ptp_rswitch = NULL;








EXPORT_SYMBOL(rswitchptp_GetTime);
EXPORT_SYMBOL(ptp_rswitch_init);

/* Global variables & Exports End */



static void my_iowrite32( uint64_t value, void __iomem  * addr)
{



    uint64_t readvalue = 0;
    
    /*lower address */
    if (( (addr - ioaddr) & 0x04) == 0 )
    {
        readvalue = ioread64(addr);
        readvalue = readvalue & 0xFFFFFFFF00000000;
        readvalue |= (value & 0xFFFFFFFF);
        iowrite64(readvalue,addr);
        

    }
    /* Upper address */
    else
    {
        readvalue = ioread64(addr -4 );
        readvalue = readvalue & 0x00000000FFFFFFFF;
        readvalue |= ((value << 32) & 0xFFFFFFFF00000000) ;
        iowrite64(readvalue,(addr -4));

    }
    



}


/*
    Register access functions
*/
static inline void ptp_rswitch_write(struct ptp_rswitch * ptp_rswitch, u32 data, enum ptp_rswitch_regoffset reg_offset)
{
    
    
    my_iowrite32(data, ptp_rswitch->addr_iomem_gPTP + (reg_offset));
    
}


static inline u32 ptp_rswitch_read(struct ptp_rswitch * ptp_rswitch, enum ptp_rswitch_regoffset reg_offset)
{
    
    
    
    return ioread32(ptp_rswitch->addr_iomem_gPTP + reg_offset );
    
}














/* Caller must hold lock */
static void ptp_rswitch_time_read(struct ptp_rswitch *ptp_rswitch, struct timespec *ts)
{

    
    ts->tv_nsec = ptp_rswitch_read(ptp_rswitch, GWCPU_GPTP_GPCT0);

    ts->tv_sec = ptp_rswitch_read(ptp_rswitch, GWCPU_GPTP_GPCT1) |
    ((s64)ptp_rswitch_read(ptp_rswitch, GWCPU_GPTP_GPCT2) << 32);
    
}


/* Caller must hold lock */
static u64 ptp_rswitch_cnt_read(struct ptp_rswitch *ptp_rswitch)
{
    
    struct timespec ts;
    ktime_t kt;

    ptp_rswitch_time_read(ptp_rswitch, &ts);
    kt = timespec_to_ktime(ts);

    return ktime_to_ns(kt);
}





/* Caller must hold lock */
static void ptp_rswitch_time_write(struct ptp_rswitch *ptp_rswitch, const struct timespec64 *ts)
{


    
    
    /* Reset Timer and load the offset */
    ptp_rswitch_write(ptp_rswitch, (0x01), GWCPU_GPTP_TMD);
    ptp_rswitch_write(ptp_rswitch, (0x01), GWCPU_GPTP_TME);

    ptp_rswitch_write(ptp_rswitch, ts->tv_sec, GWCPU_GPTP_GPTO1);
    
    ptp_rswitch_write(ptp_rswitch, ts->tv_nsec, GWCPU_GPTP_GPTO0);
    /* PPS output reset */
    iowrite32(0x00, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_GTM0);
    iowrite32(0x01, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_TCM0);
    iowrite32(0x00, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_TCST0);
    iowrite32(0x1DCD6500, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_TCD0);
}


/* Caller must hold lock */
static void ptp_rswitch_cnt_write(struct ptp_rswitch *ptp_rswitch, u64 ns)
{
    struct timespec ts;
    
    ts = ns_to_timespec(ns);

    ptp_rswitch_time_write(ptp_rswitch, &ts);
}




/* Caller must hold lock */
static void ptp_rswitch_update_addend(struct ptp_rswitch *ptp_rswitch, u32 addend)
{
    
    if(bitfile_version < RSWITCH_PTP_BITFILE_NEW)
    {
        iowrite32(addend & TIV_MASK_OLD, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_GPTI);
    }
    else if (bitfile_version >= RSWITCH_PTP_BITFILE_NEW)
    {
        iowrite32(addend & TIV_MASK, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_GPTI);
    }
    
}


/**
    @brief  Adjust gPTP increment frequency

    @return < 0 on error
*/
static int ptp_rswitch_adjfreq(struct ptp_clock_info *ptp, s32 ppb)
{
    u64             adj;
    u32             diff;
    u32             addend;
    int             neg_adj = 0;
    unsigned long   flags;
    
    struct ptp_rswitch *ptp_rswitch = container_of(ptp, struct ptp_rswitch, caps);

    if (ppb < 0) 
    {
        neg_adj = 1;
        ppb = -ppb;
    }
    addend = ptp_rswitch->default_addend;
    adj = addend;
    adj *= ppb;
    diff = div_u64(adj, 1000000000ULL);

    addend = neg_adj ? addend - diff : addend + diff;

    spin_lock_irqsave(&ptp_rswitch->lock, flags);
    ptp_rswitch_update_addend(ptp_rswitch, addend);
    spin_unlock_irqrestore(&ptp_rswitch->lock, flags);

    return 0;
}


/**
    @brief  Adjust gPTP timer

    @return < 0 on error
*/
static int ptp_rswitch_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
    
    s64 now;
    unsigned long flags;
    
    struct ptp_rswitch *ptp_rswitch = container_of(ptp, struct ptp_rswitch, caps);
    
    spin_lock_irqsave(&ptp_rswitch->lock, flags);
    
    now = ptp_rswitch_cnt_read(ptp_rswitch);
    
    now += delta;
    
    ptp_rswitch_cnt_write(ptp_rswitch, now);

    spin_unlock_irqrestore(&ptp_rswitch->lock, flags);

    return 0;
}






static int ptp_rswitch_gettime(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
    unsigned long flags;
    
    struct ptp_rswitch *ptp_rswitch = container_of(ptp, struct ptp_rswitch, caps);
    
    spin_lock_irqsave(&ptp_rswitch->lock, flags);

    ptp_rswitch_time_read(ptp_rswitch, ts);

    spin_unlock_irqrestore(&ptp_rswitch->lock, flags);

    return 0;
}




static int ptp_rswitch_settime(struct ptp_clock_info *ptp, const struct timespec64 *ts)
{
    unsigned long flags;
    struct ptp_rswitch *ptp_rswitch = container_of(ptp, struct ptp_rswitch, caps);
    
    spin_lock_irqsave(&ptp_rswitch->lock, flags);

    ptp_rswitch_time_write(ptp_rswitch, ts);

    spin_unlock_irqrestore(&ptp_rswitch->lock, flags);

    return 0;
}


static int ptp_rswitch_enable(struct ptp_clock_info *ptp, struct ptp_clock_request *rq, int on)
{
    return -EOPNOTSUPP;
}


static struct ptp_clock_info    ptp_rswitch_caps = 
{
    .owner      = THIS_MODULE,
    .name       = "rswitch-ptp",             
    .max_adj    = 50000000,
    .n_ext_ts   = 0,
    .adjfreq    = ptp_rswitch_adjfreq,
    .adjtime    = ptp_rswitch_adjtime,
    .gettime64    = ptp_rswitch_gettime,
    .settime64    = ptp_rswitch_settime,
    .enable     = ptp_rswitch_enable,
};


extern int rswitchptp_GetTime(struct timespec * TimeSpec)
{
    
    ptp_rswitch_time_read(ptp_rswitch, TimeSpec);

    return 0;
}







/**
    @brief  PTP Init Function, Called from Eth driver
*/

extern void ptp_rswitch_init(struct net_device *ndev, struct platform_device *pdev)
{
    struct port_private  *priv = netdev_priv(ndev); 
    priv->ptp.default_addend = ptp_rswitch->default_addend;
    priv->ptp.clock = ptp_rswitch->clock;
    priv->ptp.caps = ptp_rswitch->caps;
}



static int ptp_rswitch_probe(struct platform_device *pdev)
{
    int           err = -ENOMEM;


    printk("\n[RSWITCH_PTP] (%s %s) version %s Probing %s .....\n", __DATE__, __TIME__, RSWITCHPTP_DRIVER_VERSION, pdev->name);
    
    ptp_rswitch = kzalloc(sizeof(*ptp_rswitch), GFP_KERNEL);
    if (ptp_rswitch == 0)
    {
        kfree(ptp_rswitch);
        return err;
    }

    err = -ENODEV;

    ptp_rswitch->caps = ptp_rswitch_caps;

    ptp_rswitch->addr_iomem_gPTP = ioaddr + RSWITCH_FPGA_GPTP_OFFSET;
    if(bitfile_version < RSWITCH_PTP_BITFILE_NEW)
    {
       ptp_rswitch_write(ptp_rswitch, ((1000 * (1 << 20)) / PLATFORM_CLOCK_FREQUENCY_MHZ) & TIV_MASK_OLD, GWCPU_GPTP_GPTI);
    }
    else if(bitfile_version >= RSWITCH_PTP_BITFILE_NEW)
    {
        ptp_rswitch_write(ptp_rswitch, (((((uint64_t)1000000000 * (1 << 27)) / PLATFORM_CLOCK_FREQUENCY_HZ)*GPTP_CLK_DEV )/1000000) & TIV_MASK, GWCPU_GPTP_GPTI);
        //printk("[RSWITCH_PTP] Bitfile version %08x TIV= %llx\n", bitfile_version, (((((uint64_t)1000000000 * (1 << 27)) / PLATFORM_CLOCK_FREQUENCY_HZ)*GPTP_CLK_DEV )/1000000) & TIV_MASK );
    }
    

    ptp_rswitch->default_addend = ptp_rswitch_read(ptp_rswitch, GWCPU_GPTP_GPTI);

    ptp_rswitch_write(ptp_rswitch, (0x01), GWCPU_GPTP_TME);
    /* Register PTP Clock*/
    ptp_rswitch->clock = ptp_clock_register(&ptp_rswitch->caps, NULL);
    if (IS_ERR(ptp_rswitch->clock)) 
    {
        err = PTR_ERR(ptp_rswitch->clock);
        kfree(ptp_rswitch);
        return err;
    }
    /* Initialise PPS output */
    iowrite32(0x00, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_GTM0);
    iowrite32(0x01, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_TCM0);
    iowrite32(0x00, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_TCST0);
    iowrite32(0x1DCD6500, ptp_rswitch->addr_iomem_gPTP + GWCPU_GPTP_TCD0);
    printk("[RSWITCH_PTP] ptp_clock_registered for PLATFORM_CLOCK_FREQUENCY_HZ=%d TIV= %x\n", PLATFORM_CLOCK_FREQUENCY_HZ, ptp_rswitch->default_addend);
    
    dev_set_drvdata(&pdev->dev, ptp_rswitch);

    return 0;
}







static int ptp_rswitch_remove(struct platform_device *pdev)
{
    struct ptp_rswitch *ptp_rswitch = dev_get_drvdata(&pdev->dev);

    ptp_clock_unregister(ptp_rswitch->clock);
    if (ptp_rswitch->addr_iomem_gPTP != NULL)
    {
        
        ptp_rswitch->addr_iomem_gPTP = NULL;
    }

    if (ptp_rswitch != NULL)
    {
        kfree(ptp_rswitch);
        ptp_rswitch = NULL;
    }
    
    printk("Module Unloaded \n");
    return 0;
}


static struct platform_driver ptp_rswitch_driver = 
{
    
    .remove      = ptp_rswitch_remove,
    .driver = 
    {
        .name       = RSWITCH_FPGA_GPTP_PLATFORM,
        .owner      = THIS_MODULE,
    },
};

struct platform_device *pdev;

/**
    @brief PTP Module Exit

*/
static void __exit remove_module(void)
{
    

    platform_driver_unregister(&ptp_rswitch_driver);
    platform_device_unregister(pdev);
}


int __init init_module(void)
{
    int retval;
    
    pdev = platform_device_register_simple(
    RSWITCH_FPGA_GPTP_PLATFORM, -1, NULL, 0);
    
    retval = platform_driver_probe(&ptp_rswitch_driver, ptp_rswitch_probe);
    
    
    return retval;
}


module_exit(remove_module);
MODULE_AUTHOR("Asad Kamal");
MODULE_DESCRIPTION("PTP clock for Renesas Slim Switch");
MODULE_LICENSE("GPL");


/*
    Change History
    2019-03-18  0.0.1      Initial test Version Tested with PTP4l Code Cleanup needed, Module Unload need to be tested
    2019-03-25  0.0.2      Register access change to 32 bit for TIV
    2019-03-27  0.0.3      Added PPS
    2019-04-02  0.0.4      Updated for Comparator start time
    2019-10-03  0.0.5      PTP clock made dynamic
    2020-01-06  1.0.0      Release Version Code Clean Up
    2020-02-13  1.0.1      Resolution Changed for TIV

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

