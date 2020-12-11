/**
    @brief  Renesas R-Switch Forwarding Engine (FWDE) Device Driver

    Configure the RSWITCH Forwarding Engine. 
    To do the Following
    - MAC routing table with Hash Table Configuration
    - qcI Configuration
    - VLAN table configuration
    - Learning & Migration
    - Port Lock
    
    @bugs

    @notes

    @author
        Asad Kamal
        
        

    Copyright (C) 2015 Renesas Electronics Corporation

    This program is free software; you can redistribute it and/or modify it
    under the terms and conditions of the GNU General Public License,
    version 2, as published by the Free Software Foundation.

    This program is distributed in the hope it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.
    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

    The full GNU General Public License is included in this distribution in
    the file called "COPYING".
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cache.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/ethtool.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/renesas_rswitch_platform.h>
#include "rswitch_fwd.h"
#include "rswitch_fwd_main.h"
#include <linux/gpio.h>      // GPIO functions/macros
#include "../../../../ptp/rswitch_ptp.h"
#ifdef CONFIG_RENESAS_RSWITCH_FWD_STATS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif /* CONFIG_RENESAS_ACE_GW_STATS */
#define RSWITCHITCH_INT_FWD_LINE_GPIO   436
//#define DEBUG
EXPORT_SYMBOL(rswitchfwd_error);
#ifdef CONFIG_RENESAS_RSWITCH_FWD_STATS

struct proc_dir_entry   *root_debug_dir;
EXPORT_SYMBOL(root_debug_dir);


struct proc_dir_entry   * RootDir;  
struct proc_dir_entry   * ErrorDir;  

//      /proc/slimsw_fwd/
#define RSWITCHFWD_PROC_DIR               "rswitch_fwd"

//      /proc/net/slimsw_fwd/l2
#define RSWITCHFWD_PROC_FILE_L2           "L2"
//      /proc/net/slimsw_fwd/VLAN
#define RSWITCHFWD_PROC_FILE_VLAN         "VLAN"
//      /proc/net/slimsw_fwd/Mirroring
#define RSWITCHFWD_PROC_FILE_MIRROR          "Mirror"

#define RSWITCH_PROC_ROOT_DIR 		   "rswitch"
#define RSWITCHFWD_PROC_FILE_DRIVER        "fwd-driver"
#define RSWITCHFWD_PROC_DIR_ERROR          "error"
#define RSWITCHFWD_PROC_FILE_ERROR_PORT    "port"
#define RSWITCHFWD_PROC_FILE_ERROR_GLOBAL  "global"
#define RSWITCHFWD_PROC_FILE_TBL_IDX       "index"
#define RSWITCHFWD_PROC_FILE_ERROR         "fwd-error"
#define RSWITCHFWD_PROC_FILE_STATUS        "fwd-status"
#define RSWITCHFWD_PROC_FILE_COUNTER       "fwd-counter"
#endif /* CONFIG_RENESAS_RSWITCH_FWD_STATS */

static  struct rswitch_fwd_Error rswitch_fwd_Errors;
static  struct rswitch_fwd_Counter fwd_Counter;
static  struct rswitch_fwd_Status rswitch_fwd_Status_new;
static  struct rswitch_fwd_qci_status fwd_qci_status;

uint32_t pci_device_found = 1;
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

static u32 my_ioread32(  void __iomem  * addr)
{
    
    {
        return ioread32(addr);
    }
    
    
}


/* 
    when debug support is compiled in you can enable it by a
    parameter rather than having to rebuild the driver.
    Provide this as an argument to the driver when installing it e.g. 
    insmod racegw.ko debug=1

    debug={level}       0 - all debug prints off.               1 - some debug prints enabled
*/
int rswitchfwd_debug = 0;
module_param(rswitchfwd_debug, int, 0);
MODULE_PARM_DESC(rswitchfwd_debug, "Control Debug Output: 0=off 1=on, 2=max");
DEFINE_SPINLOCK(rswitchfwd_lock);

static int            rswitchfwd_DevMajor;
module_param(rswitchfwd_DevMajor, int, 0440);
static struct class * rswitchfwd_DevClass = NULL;
static dev_t          rswitchfwd_Dev;
static struct cdev    rswitchfwd_cDev;
static struct device  rswitchfwd_device;
#define RSWITCH_FWD_CTRL_MINOR (127)
#define RSWITCH_DEBUG_RDASK   0x300
#define RSWITCH_DEBUG_RDAU    0x304
#define RSWITCH_DEBUG_RDASKC  0x308


static uint32_t             rswitchfwd_base_physical = 0;
static uint32_t             rswitchfwd_debug_physical = 0;



static void __iomem       *rswitchfwd_base_addr = 0;
static void __iomem       *rswitchfwd_debug_addr = 0;
uint32_t   rswitch_debug_mode  = 0;

// Static Config (Active)
static struct rswitch_fwd_Config             rswitchfwd_Config;

// Static Config (passed from user)
static struct rswitch_fwd_Config             rswitchfwd_ConfigNew;










/**
    @brief  Write a single FPGA Forwarding Engine Configuration register write and verify

    @param      Register

    @param      Value    

    @return     < 0 on error
*/
static bool iowrite_and_verify_config_reg(uint32_t Value, uint32_t Register)
{
    uint32_t    Check = 0;

    my_iowrite32(Value, rswitchfwd_base_addr + Register);

    Check = my_ioread32(rswitchfwd_base_addr + Register);

    return true;
}

/**
    @brief  Write a single FPGA Forwarding Engine Configuration register write and verify

    @param      Register

    @return     u32 Register value
*/
static u32 ioread_config_reg(uint32_t Register)
{

    return my_ioread32(rswitchfwd_base_addr + Register);
    
}



/**
    @brief  Rswitch Enter Debug Mode Function

    @param      void

    @return     < 0 on error
*/
static int rswitch_switch_debug_mode(void)
{
    u32 RDASKC = 0;
    u32 RDASKC_def  = 0;
    /* Read RDASKC.DSK*/
    RDASKC = ioread32(rswitchfwd_debug_addr + RSWITCH_DEBUG_RDASKC);
    RDASKC_def = RDASKC;
    /*Write it to RDASK */
    iowrite32(RDASKC, rswitchfwd_debug_addr + RSWITCH_DEBUG_RDASK);
    /* XOR RDASKC.DSK to 0xFFFFFFFF */
    RDASKC = RDASKC ^ 0xFFFFFFFF;
    //printk("XOR RDASKC = %x \n", RDASKC);
    
    /*Write it to RDASK */
    iowrite32(RDASKC, rswitchfwd_debug_addr + RSWITCH_DEBUG_RDASK);
    iowrite32(RDASKC_def, rswitchfwd_debug_addr + RSWITCH_DEBUG_RDASK);
    /*Set RDAU.DU = 1 */
    iowrite32(0x01, rswitchfwd_debug_addr + RSWITCH_DEBUG_RDAU);
    rswitch_debug_mode = 1;
    return 0;

}


/**
    @brief      Rswitch Forwarding Error Function(to be exported)

    @param      void  

    @return     < 0 on error
*/
extern int rswitchfwd_error(void)
{
    uint32_t FWEIS0 = 0;
    uint32_t FWEIS1 = 0;
    uint32_t FWEIS2 = 0;
    uint32_t FWEIS3 = 0;
    uint32_t FWEIS4 = 0;
    uint32_t FWEIS5 = 0;
    uint32_t FWEIS6 = 0;
    uint32_t FWMIS0 = 0;
    uint32_t FWMIS1 = 0;
    uint32_t FWMIS2 = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    FWEIS0 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWEIS0);
    FWEIS1 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWEIS1);
    FWMIS0 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWMIS0);
    FWMIS1 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWMIS1);
    FWMIS2 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWMIS2);
    for(i = 0; i <= RENESAS_RSWITCH_MAX_ETHERNET_PORTS; i++)
    {
        rswitch_fwd_Errors.SPL_violation_status[i] = (FWEIS0 >> i) & 0x01;

    }
    iowrite32(FWEIS0, rswitchfwd_base_addr + RSWITCHFWD_FWEIS0);
    if((FWEIS1 >> 0) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.watermark_critical_overtook =  1;

    }
    if((FWEIS1 >> 1) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.watermark_flush_overtook =  1;

    }
    if((FWEIS1 >> 2) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.bufferpool_out_of_pointer =  1;

    }
    if((FWEIS1 >> 3) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.hw_learning_fail =  1;

    }
    if((FWEIS1 >> 4) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.MAC_tbl_overflow =  1;

    }
    if((FWEIS1 >> 5) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.list_filtering_error =  1;

    }
    if((FWEIS1 >> 6) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.qci_filtering_error =  1;

    }
    if((FWEIS1 >> 7) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.ECC_two_bit_ecc_error =  1;

    }
    if((FWEIS1 >> 8) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.ECC_two_bit_critical_error =  1;

    }
    if((FWEIS1 >> 9) & 0x01)
    {
        rswitch_fwd_Errors.FWEIS1.ECC_two_bit_fatal_error =  1;

    }
    iowrite32(FWEIS1, rswitchfwd_base_addr + RSWITCHFWD_FWEIS1);
    FWEIS3 = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWEIS3));
    FWEIS4 = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWEIS4));
    for(i =0 ; i < (RENESAS_RSWITCH_MAX_ETHERNET_PORTS); i++)
    {
        FWEIS2 = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWEIS20 + (i*4))); 
        FWEIS5 = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWEIS50 + (i*4))); 
        FWEIS6 = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWEIS60 + (i*4)));   
        for(j = 0; j < 8; j++)
        {
            if(FWEIS2 >> j )
            {
                rswitch_fwd_Errors.frame_red_status[j + (i*8)] = 1;
            }
        }
        iowrite32(FWEIS2, rswitchfwd_base_addr + (RSWITCHFWD_FWEIS20 + (i*4)));
        if(FWEIS3 >> i)
        {
            rswitch_fwd_Errors.gate_filter_gate_change_error[i] = 1;

        }  
        
        if(FWEIS4 >> i)
        {
            rswitch_fwd_Errors.gate_filter_config_change_error[i] = 1;

        }  
        
        for(j = 0; j < 8; j++)
        {
            if(FWEIS5 >> j )
            {
                rswitch_fwd_Errors.frame_gateclosed_status[j + (i*8)] = 1;
            }
            if(FWEIS6 >> j )
            {
                rswitch_fwd_Errors.frame_oversize_status[j + (i*8)] = 1;
            }
        }
        iowrite32(FWEIS5, rswitchfwd_base_addr + (RSWITCHFWD_FWEIS50 + (i*4)));
        iowrite32(FWEIS6, rswitchfwd_base_addr + (RSWITCHFWD_FWEIS60 + (i*4)));
        if(FWMIS0 != 0)
        {
            if((FWMIS0 >> 0) & 0x01)
            {
                rswitch_fwd_Status_new.FWMIS0.qcitbl_full_status = 1;
                
            }
            if((FWMIS0 >> 1) & 0x01)
            {
                rswitch_fwd_Status_new.FWMIS0.srptbl_full_status = 1;
                
            }
            if((FWMIS0 >> 16) & 0x01)
            {
                rswitch_fwd_Status_new.FWMIS0.mactbl_full_status = 1;
                
            }
            if((FWMIS0 >> 17) & 0x01)
            {
                rswitch_fwd_Status_new.FWMIS0.vlantbl_full_status = 1;
                
            }
            if((FWMIS0 >> 18) & 0x01)
            {
                rswitch_fwd_Status_new.FWMIS0.AFDMAS = 1;
                
            }
            if((FWMIS0 >> 19) & 0x01)
            {
                rswitch_fwd_Status_new.FWMIS0.MADAS = 1;
                
            }
        }
        if((FWMIS1 != 0) || (FWMIS2 != 0))
        {
            for(i  = 0; i <RENESAS_RSWITCH_MAX_ETHERNET_PORTS; i++)
            {
                if((FWMIS1 >> i) & 0x01)
                {
                    rswitch_fwd_Status_new.gate_filter_sched_start_status[i] = 1;
                }
                if((FWMIS2 >> i) & 0x01)
                {
                    rswitch_fwd_Status_new.gate_filter_cfgchng_complete_status[i] = 1;
                }

            }
            



        }
    }
    iowrite32(FWEIS3, rswitchfwd_base_addr + RSWITCHFWD_FWEIS3);
    iowrite32(FWEIS4, rswitchfwd_base_addr + RSWITCHFWD_FWEIS4);
    iowrite32(FWEIS0, rswitchfwd_base_addr + RSWITCHFWD_FWMIS0);
    iowrite32(FWEIS1, rswitchfwd_base_addr + RSWITCHFWD_FWMIS1);
    iowrite32(FWEIS2, rswitchfwd_base_addr + RSWITCHFWD_FWMIS1);
    return 0;



}



// ------------------------------------------------------------------------------------ 
//
//      Proc Statistics
//
// ------------------------------------------------------------------------------------ 
#ifdef CONFIG_RENESAS_RSWITCH_FWD_STATS

/**
    @brief  Return formatted Driver Information
*/
static int rswitchfwd_proc_Driver_Show(struct seq_file * m, void * v)
{
    seq_printf(m, "S/W Version: %s\n", RSWITCHFWD_DRIVER_VERSION);
    seq_printf(m, "Build on %s at %s\n", __DATE__, __TIME__);
#ifdef DEBUG
    seq_printf(m, "Debug Level: %d \n", rswitchfwd_debug);
#else
    seq_printf(m, "Debug: no\n");
#endif

    return 0;
}





/**
    @brief  Return formatted forwarding Engine Configuration Status
*/
static int rswitchfwd_proc_status_Show(struct seq_file * m, void * v)
{
    int i = 0;
    int j  = 0;
    for(i = 0; i < RENESAS_RSWITCH_MAX_ETHERNET_PORTS; i++)
    {
        
        seq_printf(m, "Gate Filter Schedule Start Status Port %d =%d  \n",i,rswitch_fwd_Status_new.gate_filter_sched_start_status[i]); 
        seq_printf(m, "Gate Filter Config Change Status Port %d =%d  \n",i,rswitch_fwd_Status_new.gate_filter_cfgchng_complete_status[i]); 
        fwd_qci_status.oper_start_time_upper[i] = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWGOCST00 + (4*i)));
        fwd_qci_status.oper_start_time_middle[i] = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWGOCST10 + (4*i)));
        fwd_qci_status.oper_start_time_lower[i] = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWGOCST20 + (4*i)));
        seq_printf(m, "QCI Operation Start Time Upper Port %d = %d   \n",i,fwd_qci_status.oper_start_time_upper[i]);
        seq_printf(m, "QCI Operation Start Time Middle Port %d = %d  \n",i,fwd_qci_status.oper_start_time_middle[i]);
        seq_printf(m, "QCI Operation Start Time Lower Port %d = %d \n",i,fwd_qci_status.oper_start_time_lower[i]);
        fwd_qci_status.oper_LIT_time_upper[i] = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWGOLIT00 + (4*i)));
        fwd_qci_status.oper_LIT_time_middle[i] = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWGOLIT00 + (4*i)));
        fwd_qci_status.oper_LIT_time_lower[i] = ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWGOLIT00 + (4*i))); 
        seq_printf(m, "QCI Last Interval Time Upper Port %d = %d   \n",i,fwd_qci_status.oper_LIT_time_upper[i]);
        seq_printf(m, "QCI Last Interval Time Middle Port %d = %d  \n",i,fwd_qci_status.oper_LIT_time_middle[i]);
        seq_printf(m, "QCI Last Interval Time Lower Port %d = %d \n",i,fwd_qci_status.oper_LIT_time_lower[i]); 
        for(j  = 0; j < 8; j++)
        {
            
            fwd_qci_status.oper_entry_num[(i*8) + j] = ((ioread32(rswitchfwd_base_addr + (RSWITCHFWD_FWGOEN00 + (i*4) + ((j/4)*0x80))) >> (i*8)) & 0xFF);

            
            
            seq_printf(m, "QCI Operation Entry Num Gate%d Port %d = %d \n",j, i,fwd_qci_status.oper_entry_num[(i*8) + j]); 


        }
        
    }
    return 0;
    
}

/**
    @brief  Return formatted Counter Information
*/
static int rswitchfwd_proc_counter_Show(struct seq_file * m, void * v)
{
    int i  = 0;
    fwd_Counter.exceptional_path_pkt_cnt = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWCEP);
    fwd_Counter.mirror_path_pkt_cnt = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWCMP);
    fwd_Counter.rejected_pkt_cnt = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWCRP);
    fwd_Counter.bcast_pkt_cnt = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWBPC);
    fwd_Counter.mcast_pkt_cnt = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWMPC);
    fwd_Counter.rx_desc_cnt = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWRDC);
    fwd_Counter.remain_ptr_cnt = (ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWBPPC) & 0xFFFF);
    fwd_Counter.least_remain_ptr_cnt = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWBPLC);
    fwd_Counter.total_ptr_cnt = (ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWBPPC) >> 16);
    seq_printf(m, "Exceptional Path Cnt =%d  \n",fwd_Counter.exceptional_path_pkt_cnt); 
    seq_printf(m, "Mirror Packet Cnt =%d  \n",fwd_Counter.mirror_path_pkt_cnt);
    seq_printf(m, "Broadcast Packet Cnt =%d  \n",fwd_Counter.bcast_pkt_cnt);
    seq_printf(m, "Multicast Packet Cnt =%d  \n",fwd_Counter.mcast_pkt_cnt); 
    seq_printf(m, "Rejected Packet Cnt =%d  \n",fwd_Counter.rejected_pkt_cnt); 
    seq_printf(m, "RX Desc  Cnt =%d  \n",fwd_Counter.rx_desc_cnt); 
    seq_printf(m, "Remain Buffer Pointer Cnt =%d  \n",fwd_Counter.remain_ptr_cnt); 
    seq_printf(m, "Least Remain Pointer Cnt =%d  \n",fwd_Counter.least_remain_ptr_cnt); 
    seq_printf(m, "Total Buffer Pointer Cnt =%d  \n",fwd_Counter.total_ptr_cnt); 
    for(i = 0; i < (RENESAS_RSWITCH_MAX_ETHERNET_PORTS*8); i++)
    {
        fwd_Counter.msdu_reject_pkt_cnt[i] = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWQMSRPC0 + (i*4));
        fwd_Counter.qci_gate_reject_pkt_cnt[i] = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWQGRPC0 + (i*4));
        fwd_Counter.qci_meter_drop_pkt_cnt[i] = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWQMDPC0 + (i*4));
        fwd_Counter.qci_meter_green_pkt_cnt[i] = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWQMGPC0 + (i*4));
        fwd_Counter.qci_meter_yellow_pkt_cnt[i] = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWQMYPC0 + (i*4));
        fwd_Counter.qci_meter_red_pkt_cnt[i] = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWQMRPC0 + (i*4));
        seq_printf(m, "MSDU Reject Pkt Count Meter%d  =%d  \n",i, fwd_Counter.msdu_reject_pkt_cnt[i]);
        seq_printf(m, "QCI Gate%d Reject Pkt Count   =%d  \n",i, fwd_Counter.qci_gate_reject_pkt_cnt[i]);
        seq_printf(m, "QCI Meter%d Drop Pkt Count   =%d  \n",i, fwd_Counter.qci_meter_drop_pkt_cnt[i]);
        seq_printf(m, "QCI Meter%d  Green Pkt Count   =%d  \n",i, fwd_Counter.qci_meter_green_pkt_cnt[i]); 
        seq_printf(m, "QCI Meter%d  Yellow Pkt Count   =%d  \n",i, fwd_Counter.qci_meter_yellow_pkt_cnt[i]);
        seq_printf(m, "QCI Meter%d Red Pkt Count   =%d  \n",i, fwd_Counter.qci_meter_red_pkt_cnt[i]);
    }
    return 0;
}


/**
    @brief  Return formatted Forwarding Engine Error Information
*/
static int rswitchfwd_proc_error_Show(struct seq_file * m, void * v)
{
    int i = 0;
    int j = 0;
    for(i = 0; i < RENESAS_RSWITCH_MAX_ETHERNET_PORTS; i++)
    {
        seq_printf(m, "SPL Violation Status Port %d =%d  \n",i,rswitch_fwd_Errors.SPL_violation_status[i]); 
        seq_printf(m, "Gate Change Error Port %d =%d  \n",i,rswitch_fwd_Errors.gate_filter_gate_change_error[i]); 
        seq_printf(m, "Gate Config Error Port %d =%d  \n",i,rswitch_fwd_Errors.gate_filter_config_change_error[i]); 
    }
    seq_printf(m, "SPL Violation Status CPU = %d \n",   rswitch_fwd_Errors.SPL_violation_status[i]);
    seq_printf(m, "WaterMarkCritical Overtook Status = %d \n",   rswitch_fwd_Errors.FWEIS1.watermark_critical_overtook);
    seq_printf(m, "WaterMarkFlush Overtook Status = %d \n",   rswitch_fwd_Errors.FWEIS1.watermark_flush_overtook);
    seq_printf(m, "Out of Buffer Pointer Status = %d \n",   rswitch_fwd_Errors.FWEIS1.bufferpool_out_of_pointer);
    seq_printf(m, "HW learn Fail Status = %d \n",   rswitch_fwd_Errors.FWEIS1.hw_learning_fail);
    seq_printf(m, "MAC Tbl Ovderflow = %d \n",   rswitch_fwd_Errors.FWEIS1.MAC_tbl_overflow);
    seq_printf(m, "LIST Filtering Error = %d \n",   rswitch_fwd_Errors.FWEIS1.list_filtering_error);
    seq_printf(m, "QCI Filtering Error = %d \n",   rswitch_fwd_Errors.FWEIS1.qci_filtering_error);
    seq_printf(m, "ECC Two Bit ECC Error = %d \n",   rswitch_fwd_Errors.FWEIS1.ECC_two_bit_ecc_error);
    seq_printf(m, "ECC Two Bit Critical Error = %d \n",   rswitch_fwd_Errors.FWEIS1.ECC_two_bit_critical_error);
    seq_printf(m, "ECC Two Bit Fatal Error = %d \n",   rswitch_fwd_Errors.FWEIS1.ECC_two_bit_fatal_error);
    for(i = 0; i < RENESAS_RSWITCH_MAX_ETHERNET_PORTS ; i++)
    {
        seq_printf(m, "Red Flag Status Meter %d =%d  \n",i,rswitch_fwd_Errors.frame_red_status[i]); 
        for(j = 0; j < 8; j++)
        {

            seq_printf(m, "Frame gate Closed status for Gate %d Port %d =%d  \n",j,i,rswitch_fwd_Errors.frame_gateclosed_status[(i*8) + j]);
            seq_printf(m, "Frame Oversize status for Gate %d Port %d =%d  \n",j,i,rswitch_fwd_Errors.frame_oversize_status[(i*8) + j]); 

        }
    }
    seq_printf(m, "QCI TABLE FULL STATUS = %d \n",   rswitch_fwd_Status_new.FWMIS0.qcitbl_full_status); 
    seq_printf(m, "SRP TABLE FULL STATUS = %d \n",   rswitch_fwd_Status_new.FWMIS0.srptbl_full_status); 
    seq_printf(m, "MAC TABLE FULL STATUS = %d \n",   rswitch_fwd_Status_new.FWMIS0.mactbl_full_status); 
    seq_printf(m, "VLAN TABLE FULL STATUS = %d \n",   rswitch_fwd_Status_new.FWMIS0.vlantbl_full_status);
    seq_printf(m, "Aeging Finished with No Deletion = %d \n",   rswitch_fwd_Status_new.FWMIS0.AFDMAS); 
    seq_printf(m, "Aeging Finished with  Deletion = %d \n",   rswitch_fwd_Status_new.FWMIS0.MADAS);
    return 0; 
}


/**
    @brief  Return formatted VLAN Table Information
*/
static int rswitchfwd_proc_VLAN_Show(struct seq_file * m, void * v)
{
    
    u32 FWVTSR0 = 0;
    u32 FWVTSR1 = 0;
    u32 FWVTSR2 = 0;
    u32 FWVTSR3 = 0;
    u32 eth_mirror = 0;
    u32 cpu_mirror = 0;
    u32 pcp_update = 0;
    u32 new_pcp = 0;
    u32 csdn = 0;
    u32 c = 0;
    u32 VLAN = 0;
    if(rswitch_debug_mode == 0)
    {
        rswitch_switch_debug_mode();
        
    }
    seq_printf(m, "VLAN     Filter-Ports     Rout-Ports       ETH-Mirror     CPU-Mirror    PCP-Update New-PCP CSDN     \n");
    for(VLAN =0; VLAN <= 0xFFF; VLAN++)
    {
        iowrite32(VLAN, rswitchfwd_base_addr + RSWITCHFWD_FWVTS);
        iowrite32((0x80000000 ), rswitchfwd_base_addr + RSWITCHFWD_FWVTSR3);
        FWVTSR3 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWVTSR3);
        if((FWVTSR3 & 0x01) == 0x00)
        {
            FWVTSR0 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWVTSR0);
            FWVTSR1 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWVTSR1);
            FWVTSR2 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWVTSR2);
            eth_mirror = FWVTSR2 >> 17;
            cpu_mirror = FWVTSR2 >> 16;
            pcp_update = FWVTSR2 >> 15;
            new_pcp = FWVTSR2 >> 12;
            csdn = FWVTSR2 & 0xFF;
            seq_printf(m, "%4d     ", VLAN);
            seq_printf(m, "    ");
            for (c = 0; c < RENESAS_RSWITCH_MAX_ETHERNET_PORTS; c++)
            {
                
                if(((FWVTSR0 >> c) & 0x01) == 1)
                {
                    seq_printf(m, "%2u", c);

                }
            }
            if((FWVTSR0 >> RENESAS_RSWITCH_MAX_ETHERNET_PORTS) == 0x01)
            {
                seq_printf(m, "%3s","CPU\n");
            }
            seq_printf(m, "          ");
            for (c = 0; c < RENESAS_RSWITCH_MAX_ETHERNET_PORTS; c++)
            {
                
                if(((FWVTSR1 >> c) & 0x01) == 1)
                {
                    seq_printf(m, "%2u", c);

                }
            }
            if((FWVTSR1 >> RENESAS_RSWITCH_MAX_ETHERNET_PORTS) == 0x01)
            {
                seq_printf(m, "%3s","CPU");
            }
            if(eth_mirror)
            {
                seq_printf(m, "%21s","Yes");
            }
            else
            {
                seq_printf(m, "%21s","No");
            }
            if(cpu_mirror)
            {
                seq_printf(m, "%15s","Yes");
            }
            else
            {
                seq_printf(m, "%15s","No");
            }
            if(pcp_update)
            {
                seq_printf(m, "%15s","Yes");
            }
            else
            {
                seq_printf(m, "%15s","No");
            }
            seq_printf(m, "  %-8u%-4d \n", new_pcp, csdn);

        }


    }
    return 0;
}


/**
    @brief  Return formatted Layer-2 routing Information
*/
static int rswitchfwd_proc_L2_Show(struct seq_file * m, void * v)
{
    u32 FWMTRR0 = 0;
    u32 FWMTRR1 = 0;
    u32 FWMTRR2 = 0;
    u32 entry  = 0;
    u32 c = 0;
    u32 Mach = 0;
    u32 Macl  = 0;
    u32 PCP  = 0;
    u32 PUE  = 0;
    u32 CSDN  = 0;
    u32 DPV  = 0;
    u32 Aging = 0;
    u32 Dynamic = 0;
    if(rswitch_debug_mode == 0)
    {
        rswitch_switch_debug_mode();
        
    }
    seq_printf(m, "Line      MAC address      Aging       Ports       CPU   PCP\n");
    seq_printf(m, "============================================================\n");
    for(entry = 0; entry <= 1023; entry++)
    {

        iowrite32((0x80000000 | entry), rswitchfwd_base_addr + RSWITCHFWD_FWMTR);
        
        FWMTRR0 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWMTRR0);
        
        if(FWMTRR0 != 0)
        {
            FWMTRR1 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWMTRR1);
            
            FWMTRR2 = ioread32(rswitchfwd_base_addr + RSWITCHFWD_FWMTRR2);
            
            Mach = FWMTRR1;
            Macl = FWMTRR2 >> 13;
            DPV = FWMTRR0 & 0xFF;
            CSDN  = (FWMTRR0 >> 8) & 0xFF;
            PCP = (FWMTRR0 >> 17) & 0x7;
            Aging = (FWMTRR1 >> 29) & 0x1;
            Dynamic = (FWMTRR1 >> 30) & 0x1;
            seq_printf(m, "%4d   %02X:%02X:%02X:%02X:%02X:%02X     ",
                entry,
                ((Mach >> 21) & 0xFF) , ((Mach >> 13) & 0xFF), ((Mach >> 5) & 0xFF),
                ((Mach & 0x1F ) << 3) | ((Macl >> 16) & 0x07), ((Macl >> 8) & 0xFF), ((Macl >> 0) & 0xFF)
                );
            if (Dynamic == 1)
            {
                seq_printf(m, "%1d     ", Aging);
            }
            else
            {
                seq_printf(m, "S     ");
            }
            for (c = 0; c < RENESAS_RSWITCH_MAX_ETHERNET_PORTS; c++)
            {
                
                if(((DPV >> c) & 0x01) == 1)
                {
                    seq_printf(m, "%1d ", c);
                }
                else
                {
                    seq_printf(m, "  ");
                }
            }
            if((DPV >> RENESAS_RSWITCH_MAX_ETHERNET_PORTS) == 0x01)
            {
                seq_printf(m, "  %2d    ",CSDN);
            }
            else
            {
                seq_printf(m, "   -    ");
            }
            if (PUE == 1)
            {
                seq_printf(m, "%2d ",PCP);
            }
            else
            {
                seq_printf(m, "No ");
            }
            seq_printf(m, "\n");
        }
        
        
    }

    return 0;
}
























/**
    @brief  L2 Table Proc Open
*/
static int rswitchfwd_proc_L2_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitchfwd_proc_L2_Show, NULL);
}


/**
    @brief  VLAN Table Proc Open
*/
static int rswitchfwd_proc_VLAN_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitchfwd_proc_VLAN_Show, NULL);
}

/**
    @brief  Forwarding Error Proc Open
*/
static int rswitchfwd_proc_error_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitchfwd_proc_error_Show, NULL);
}

/**
    @brief  Forwarding Counter Proc Open
*/
static int rswitchfwd_proc_counter_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitchfwd_proc_counter_Show, NULL);
}


/**
    @brief  Forwarding Status Proc Open
*/
static int rswitchfwd_proc_status_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitchfwd_proc_status_Show, NULL);
}



/**
    @brief  Driver Proc Open
*/
static int rswitchfwd_proc_Driver_Open(struct inode * inode, struct  file * file) 
{
    return single_open(file, rswitchfwd_proc_Driver_Show, NULL);
}




/**
    @brief L2 Table Proc File Operation
*/
static const struct file_operations     rswitchfwd_L2_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitchfwd_proc_L2_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};


/**
    @brief  VLAN Table Proc File Operation
*/
static const struct file_operations     rswitchfwd_VLAN_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitchfwd_proc_VLAN_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};



/**
    @brief  Forwarding Error Proc File Operation
*/
static const struct file_operations     rswitchfwd_error_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitchfwd_proc_error_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};


/**
    @brief  Forwarding Counter Proc File Operation
*/
static const struct file_operations     rswitchfwd_counter_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitchfwd_proc_counter_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

/**
    @brief  Forwarding Status Proc File Operation
*/
static const struct file_operations     rswitchfwd_status_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitchfwd_proc_status_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};



/**
    @brief  Driver Status Proc File Operation
*/
static const struct file_operations     rswitchfwd_Driver_Fops = 
{
    .owner   = THIS_MODULE,
    .open    = rswitchfwd_proc_Driver_Open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};








/**
    @brief  Create /proc entries
*/
static void rswitchfwd_proc_Initialise(void)
{
    /* 
        create root & sub-directories 
    */
    memset(&RootDir, 0, sizeof(RootDir));

    root_debug_dir = proc_mkdir(RSWITCH_PROC_ROOT_DIR, NULL);
    RootDir = root_debug_dir;

    proc_create(RSWITCHFWD_PROC_FILE_DRIVER, 0, RootDir, &rswitchfwd_Driver_Fops);
    
    proc_create(RSWITCHFWD_PROC_FILE_L2, 0, RootDir, &rswitchfwd_L2_Fops);
    proc_create(RSWITCHFWD_PROC_FILE_VLAN, 0, RootDir, &rswitchfwd_VLAN_Fops);
    proc_create(RSWITCHFWD_PROC_FILE_ERROR, 0, RootDir, &rswitchfwd_error_Fops);
    proc_create(RSWITCHFWD_PROC_FILE_STATUS, 0, RootDir, &rswitchfwd_status_Fops);
    proc_create(RSWITCHFWD_PROC_FILE_COUNTER, 0, RootDir, &rswitchfwd_counter_Fops);

}


/**
    @brief  Remove /proc entries
*/
static void rswitchfwd_proc_Terminate(void)
{
    
    remove_proc_entry(RSWITCHFWD_PROC_FILE_L2, RootDir);
    remove_proc_entry(RSWITCHFWD_PROC_FILE_VLAN, RootDir);
    remove_proc_entry(RSWITCHFWD_PROC_FILE_DRIVER, RootDir);
    
    remove_proc_entry(RSWITCHFWD_PROC_FILE_ERROR, RootDir);
    remove_proc_entry(RSWITCHFWD_PROC_FILE_STATUS, RootDir);
    remove_proc_entry(RSWITCHFWD_PROC_FILE_COUNTER, RootDir);
    remove_proc_entry(RSWITCH_PROC_ROOT_DIR, NULL);    
}

#endif



// ------------------------------------------------------------------------------------ 
//
//      IOCTL Configuration Functions
//
// ------------------------------------------------------------------------------------ 


/**
    @brief  Query FPGA configuration from FPGA

    @return 0, or < 0 on error
*/
static int rswitchfwd_IOCTL_GetfConfig(struct file * file, unsigned long arg)
{
    char __user *buf = (char __user *)arg;
    int     err = 0;

    if ((err = copy_to_user(buf, &rswitchfwd_Config, sizeof(rswitchfwd_Config))) < 0)
    {
        return err;
    }

    return 0;
}























/**
    @brief  Forwarding Engine Broadcast Configuration

    @param  struct rswitch_Config_Broadcast *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Broadcast(struct rswitch_Config_Broadcast *Config)
{
    uint32_t Ports = 0;
    uint32_t DPV = 0, CSDN  = 0;
    printk("DPV ######= %d \n", DPV);
    for(Ports = 0; Ports < Config->Dest_Eths; Ports++)
    {
        DPV = DPV | (1 << Config->Dest_Eth[Ports].PortNumber);  

    }
    
    DPV = DPV | (Config->CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    CSDN = Config->CSDN;
    iowrite_and_verify_config_reg(DPV, RSWITCHFWD_FWMDANFV0 + (Config->Broadcast_Config_Port.CPU_Port? (RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 4) :(Config->Broadcast_Config_Port.PortNumber*4)));
    iowrite_and_verify_config_reg(CSDN, RSWITCHFWD_FWMDANFCSD0 + (Config->Broadcast_Config_Port.CPU_Port? (RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 4) :(Config->Broadcast_Config_Port.PortNumber*4)));
    return 0;

}

/**
    @brief  Forwarding Engine Source Port Forwarding Configuration

    @param  struct rswitch_Config_Source_Fwd *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Src_Port_Fwd(struct rswitch_Config_Source_Fwd *Config)
{
    uint32_t Configs, Dests = 0;
    uint32_t DPV = 0,DestDPV = 0,CSDN = 0, CPUMirror = 0, PortMirror = 0;
    for(Configs = 0; Configs < Config->source_fwd_port_configs; Configs++)
    {
        if(Config->Source_Port_Config[Configs].CPU_Host)
        {

            DPV = DPV | (Config->Source_Port_Config[Configs].CPU_Host << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
            CPUMirror = CPUMirror | (Config->Source_Port_Config[Configs].CPU_Mirror << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
            PortMirror = PortMirror | (Config->Source_Port_Config[Configs].Eth_Mirror << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
            
        }
        else
        {
            DPV = DPV | (1 << Config->Source_Port_Config[Configs].PortNumber); 
            CPUMirror = CPUMirror | (Config->Source_Port_Config[Configs].CPU_Mirror << Config->Source_Port_Config[Configs].PortNumber);
            PortMirror = CPUMirror | (Config->Source_Port_Config[Configs].Eth_Mirror << Config->Source_Port_Config[Configs].PortNumber);
        }
        for(Dests = 0; Dests < Config->Source_Port_Config[Configs].Dest_Config.Dest_Eths; Dests++)
        {
            
            DestDPV = DestDPV | (1 << Config->Source_Port_Config[Configs].Dest_Config.Dest_Eth[Dests].PortNumber); 

        } 
        DestDPV = DestDPV | (Config->Source_Port_Config[Configs].Dest_Config.CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS); 
        CSDN = Config->Source_Port_Config[Configs].Dest_Config.CSDN;
        iowrite_and_verify_config_reg(DestDPV, RSWITCHFWD_FWSPBFC20 + (Config->Source_Port_Config[Configs].CPU_Host? 
        (RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 4) :(Config->Source_Port_Config[Configs].PortNumber*4)));
        iowrite_and_verify_config_reg(CSDN, RSWITCHFWD_FWSPBFC30 + (Config->Source_Port_Config[Configs].CPU_Host? 
        (RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 4) :(Config->Source_Port_Config[Configs].PortNumber*4)));
    }
    iowrite_and_verify_config_reg(DPV,RSWITCHFWD_FWSPBFE);
    iowrite_and_verify_config_reg(CPUMirror,RSWITCHFWD_FWSPBFC0);
    iowrite_and_verify_config_reg(PortMirror,RSWITCHFWD_FWSPBFC1);

    
    return 0;


}


/**
    @brief  Forwarding Engine Dynamic Authentication Configuration

    @param  struct rswitch_Config_Dynamic_Auth *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Dyamic_Auth(struct rswitch_Config_Dynamic_Auth *Config)
{
    uint32_t FWMAFC0 = 0;
    uint32_t Ports  = 0; 
    uint32_t DPV = 0;
    FWMAFC0 = ((Config->mac_e_enable << 18) | (Config->mac_3_enable << 17) | (Config->mac_0_enable << 16) | (Config->Dest.Priority_Enable << 15)|
    (Config->Dest.Priority << 12) | (Config->Dest.CSDN));
    for(Ports = 0; Ports < Config->Dest.Dest_Eths; Ports++)
    {
        DPV = DPV | (1 << Config->Dest.Dest_Eth[Ports].PortNumber);

    }
    DPV = DPV | (Config->Dest.CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    iowrite_and_verify_config_reg(FWMAFC0,RSWITCHFWD_FWMAFC0);
    iowrite_and_verify_config_reg(DPV,RSWITCHFWD_FWMAFC1);
    

    return 0;


}



/**
    @brief  Forwarding Engine Destination Forwarding Configuration

    @param  struct rswitch_Config_Dest_Fwding *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Destination_Fwd(struct rswitch_Config_Dest_Fwding *Config)
{
    uint32_t Mach = 0,Macl = 0,Maskh = 0,Maskl = 0,entries = 0;
    uint32_t PriorityEnable = 0, Priority = 0, CSDN = 0, DPV = 0, dest = 0;
    uint32_t FWMDFC0 = 0,FWMDFC1 = 0,FWMDFC2 = 0,FWMDFC3 = 0,FWMDFC4 = 0;
    for(entries = 0; entries < Config->DestFwdEntries; entries++)
    {
        Mach = (uint32_t)(Config->DestFwd_Entry[entries].MAC[1]) | (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].MAC[0] << 8);
        Macl = (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].MAC[5]) | (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].MAC[4] << 8) 
        | (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].MAC[3] << 16) | (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].MAC[2] << 24);
        Maskh = (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].Mask[1]) | (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].Mask[0] << 8);
        Maskl = (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].Mask[5]) | (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].Mask[4] << 8) 
        | (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].Mask[3] << 16) | (uint32_t)((uint32_t)Config->DestFwd_Entry[entries].Mask[2] << 24);
        PriorityEnable = Config->DestFwd_Entry[entries].Destination.Priority_Enable;
        Priority =  Config->DestFwd_Entry[entries].Destination.Priority;
        CSDN = Config->DestFwd_Entry[entries].Destination.CSDN;
        FWMDFC0 =  (1 << 16) | (PriorityEnable << 15) | (Priority << 12) | (CSDN);
        FWMDFC1 = Mach;
        FWMDFC2 = Macl;
        FWMDFC3 = Maskh;
        FWMDFC4 = Maskl;
        for(dest =  0; dest < Config->DestFwd_Entry[entries].Destination.Dest_Eths; dest++)
        {
            DPV = DPV | (1 << Config->DestFwd_Entry[entries].Destination.Dest_Eth[dest].PortNumber);


        }
        DPV = DPV | (Config->DestFwd_Entry[entries].Destination.CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
        iowrite_and_verify_config_reg(FWMDFC0,(RSWITCHFWD_FWMDFC00+ (entries * 0x20)));
        iowrite_and_verify_config_reg(FWMDFC1,(RSWITCHFWD_FWMDFC01+ (entries * 0x20)));
        iowrite_and_verify_config_reg(FWMDFC2,(RSWITCHFWD_FWMDFC02+ (entries * 0x20)));
        iowrite_and_verify_config_reg(FWMDFC3,(RSWITCHFWD_FWMDFC03+ (entries * 0x20)));  
        iowrite_and_verify_config_reg(FWMDFC4,(RSWITCHFWD_FWMDFC04+ (entries * 0x20))); 
        iowrite_and_verify_config_reg(DPV,(RSWITCHFWD_FWMDFC05+ (entries * 0x20)));      

    }

    return 0;






}

/**
    @brief  Forwarding Engine Private VLAN Configuration

    @param  struct rswitch_Config_Private_VLAN *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Private_VLAN(struct rswitch_Config_Private_VLAN *Config)
{
    uint32_t source_independent = 0;
    uint32_t FWPVC0 = 0,groupid = 0,groups = 0,DPV=0,Dest = 0;
    source_independent =  Config->source_independent;
    FWPVC0 = (source_independent << 1) | 1;
    iowrite_and_verify_config_reg(FWPVC0, (RSWITCHFWD_FWPVC0)); 
    if(source_independent == 0)
    {
        for(groups = 0; groups < Config->VLAN_Groups; groups++)
        {
            groupid = Config->vlan_group[groups].GrpID;
            for(Dest = 0; Dest < Config->vlan_group[groups].Dest_Eths; Dest++)
            {
                DPV = DPV | (1 << Config->vlan_group[groups].Dest_Eth[Dest].PortNumber);



            }
            DPV = DPV | (Config->vlan_group[groups].CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
            iowrite_and_verify_config_reg(DPV, (RSWITCHFWD_FWPVC1+ (groupid * 0x4))); 

        }




    }

    return 0;





}

/**
    @brief  Forwarding Engine Double Tagging Configuration

    @param  rswitch_Config_Double_Tag *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Double_Tag(struct rswitch_Config_Double_Tag *Config)
{
    

    uint32_t tag_value =  0;
    tag_value =  Config->tag_value;
    iowrite_and_verify_config_reg(tag_value, (RSWITCHFWD_FWDTTC)); 

    return 0;


}


/**
    @brief  Forwarding Engine Filter CSD Configuration

    @param  rswitch_Config_fwd_Agent_FilterCSD *

    @return 0, or < 0 on error
*/
static int rswitch_Config_AgentFilterCSD (struct rswitch_Config_fwd_Agent_FilterCSD *Config)
{
    uint32_t Eth = 0,DPV = 0;
    for(Eth = 0; Eth < Config->Eths; Eth++)
    {
        DPV = DPV | (1 << Config->Eth[0].PortNumber);


    }
    DPV =  DPV | (Config->CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    iowrite_and_verify_config_reg(DPV, (RSWITCHFWD_FWAFCE)); 

    return 0;

}


/**
    @brief  Forwarding Engine Water Mark Control Configuration

    @param  rswitch_Config_fwd_WaterMarkControl *

    @return 0, or < 0 on error
*/
static int rswitch_Config_WaterMarkControl( struct rswitch_Config_fwd_WaterMarkControl *Config)
{
    uint32_t flush_level = 0,critical_level = 0,assert_pframe = 0, dassert_pframe = 0;
    uint32_t configs = 0, pcpconfig = 0,pcpdei_flush = 0,pcpdei_clevel = 0;
    uint32_t DPV_clevel = 0, DPV_flush  = 0;
    uint32_t CNFPCPV = 0;
    uint32_t CNFPCPU = 0;
    uint32_t CNFWMD = 0;
    uint32_t DDB = 0;
    uint32_t FWSOVPC4  = 0;
   
    flush_level  =  Config->flush_level;
    critical_level = Config->critical_level;
    assert_pframe = Config->rmn_pause_frm_assrt;
    dassert_pframe = Config->rmn_pause_frm_dassrt;
    if(Config->NC_Config.bEnable)
    {
        if(Config->NC_Config.PCPUpdate)
        {
            CNFPCPV = Config->NC_Config.PCP;
            CNFPCPU = Config->NC_Config.PCPUpdate;

        }
    
        DDB = Config->NC_Config.DirectDescriptorBypass;
        CNFWMD = Config->NC_Config.ForceWatermark;
        FWSOVPC4  = (CNFPCPV << 21) | (CNFPCPU << 20) | (CNFWMD << 16) | (DDB << 0);
        
        iowrite_and_verify_config_reg(FWSOVPC4, RSWITCHFWD_FWSOVPC4);
    }
    else
    {

        iowrite_and_verify_config_reg(0x01, RSWITCHFWD_FWSOVPC4);

    }
    for(configs = 0; configs < Config->WaterMarkControlPortConfigs; configs++)
    {
        pcpdei_flush = 0;
        pcpdei_clevel = 0;
        

        for(pcpconfig = 0; pcpconfig < Config->WaterMarkControlPortConfig[configs].PCP_DEI_Configs; pcpconfig++)
        {
            
            pcpdei_flush = pcpdei_flush | (!Config->WaterMarkControlPortConfig[configs].pcp_dei_config[pcpconfig].flush << 
            ((Config->WaterMarkControlPortConfig[configs].pcp_dei_config[pcpconfig].PCP_ID*2) + 
            (Config->WaterMarkControlPortConfig[configs].pcp_dei_config[pcpconfig].DEI))) ;
            
            pcpdei_clevel = pcpdei_clevel | (!Config->WaterMarkControlPortConfig[configs].pcp_dei_config[pcpconfig].clevel << 
            ((Config->WaterMarkControlPortConfig[configs].pcp_dei_config[pcpconfig].PCP_ID*2) + 
            (Config->WaterMarkControlPortConfig[configs].pcp_dei_config[pcpconfig].DEI))) ;
            

        }
        if(Config->WaterMarkControlPortConfig[configs].CPU_Enable)
        {

            DPV_flush = DPV_flush | (!Config->WaterMarkControlPortConfig[configs].novlanflush << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
            DPV_clevel = DPV_clevel | (!Config->WaterMarkControlPortConfig[configs].novlanclevel << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
            iowrite_and_verify_config_reg(((pcpdei_flush << 16) | (pcpdei_clevel)), (RSWITCHFWD_BPWMC10 + (RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 4))); 

        }
        else
        {
            DPV_flush = DPV_flush |(!Config->WaterMarkControlPortConfig[configs].novlanflush <<  Config->WaterMarkControlPortConfig[configs].PortNumber);
            DPV_clevel = DPV_clevel |(!Config->WaterMarkControlPortConfig[configs].novlanclevel <<  Config->WaterMarkControlPortConfig[configs].PortNumber);
            iowrite_and_verify_config_reg(((pcpdei_flush << 16) | (pcpdei_clevel)), (RSWITCHFWD_BPWMC10 + (Config->WaterMarkControlPortConfig[configs].PortNumber * 4)));
        }
        


    }
    iowrite_and_verify_config_reg(DPV_clevel, (RSWITCHFWD_BPWMC2)); 
    iowrite_and_verify_config_reg(DPV_flush, (RSWITCHFWD_BPWMC3)); 
    iowrite_and_verify_config_reg(((critical_level << 16) |flush_level), (RSWITCHFWD_BPWML)); 
    iowrite_and_verify_config_reg(((assert_pframe << 16) |dassert_pframe), (RSWITCHFWD_BPPFL)); 

    return 0;
}


/**
    @brief  Forwarding Engine Exceptional Path Configuration

    @param  rswitch_Config_fwd_ExceptionalPath *

    @return 0, or < 0 on error
*/
static int  rswitch_Config_Exceptional_Path(struct rswitch_Config_fwd_ExceptionalPath *Config)
{
    uint32_t EPCSD = 0, EPCPR = 0, SPLVF = 0, LFF = 0, RDEF = 0, DVNF = 0, MDANFF = 0, VNFF = 0, SNFF = 0, DSELF = 0, QCIRF = 0, WMFF = 0, BOF = 0, AOF = 0, FWCEPC = 0;
    EPCSD = Config->CSDN;
    EPCPR = Config->Priority;
    SPLVF = Config->SPLViolationtoCPU;
    LFF = Config->FilterRejecttoCPU;
    RDEF = Config->ErrorDesctoCPU;
    DVNF = Config->NullDPVtoCPU;
    MDANFF = Config->UnmatchMACtoCPU;  
    VNFF = Config->UnmatchVLANtoCPU;
    SNFF = Config->UnmatchSIDtoCPU;
    DSELF =  Config->LearnstatictoCPU;
    QCIRF = Config->QCIrejecttoCPU;
    WMFF = Config->WMarktoCPU;
    BOF = Config->BPDUFailtoCPU;
    AOF = Config->AuthFailtoCPU;
    FWCEPC = (AOF << 27) | (BOF << 26) | (WMFF << 25) | (QCIRF << 24) | (DSELF << 23) | (SNFF << 22) 
    | (VNFF << 21) | (MDANFF << 20) | (DVNF << 19) | (RDEF << 18) | (LFF << 17)
    | (SPLVF << 16) | (EPCPR << 12) | (EPCSD);
    iowrite_and_verify_config_reg(FWCEPC, (RSWITCHFWD_FWCEPC)); 
    return 0;
}



/**
    @brief  Forwarding Engine Learning Configuration

    @param  rswitch_Config_fwd_Learning *

    @return 0, or < 0 on error
*/
static int  rswitch_Config_Learning(struct rswitch_Config_fwd_Learning *Config)
{
    uint32_t LPCSD = 0, LPCPR = 0, MLPA = 0,VLPA = 0, HLA = 0, USPVLA = 0, FWCLPC = 0;
    LPCSD = Config->CSDN;
    LPCPR = Config->Priority;
    MLPA = Config->MACLearning;
    VLPA = Config->VLANLearning;
    HLA = Config->HWLearning;
    USPVLA = Config->UnknownSourcePortLearn;
    FWCLPC = (USPVLA << 20) | (HLA << 19) | (VLPA << 18) | (MLPA << 16) | (LPCPR << 12) | (LPCSD);
    iowrite_and_verify_config_reg(FWCLPC, (RSWITCHFWD_FWCLPC)); 
    return 0;



}


/**
    @brief  Forwarding Engine Mirroring Configuration

    @param  rswitch_Config_fwd_Mirroring *

    @return 0, or < 0 on error
*/
static int  rswitch_Config_Mirroring(struct rswitch_Config_fwd_Mirroring  *Config)
{
    uint32_t SourcePort = 0, DPVsrc = 0, DestPort = 0, DPVtrgt = 0, EthMirror = 0, CPUMirror = 0, DPVeth = 0, DPVcpu = 0;
    uint32_t MCSD = 0, CMP = 0, CMPC = 0, EMP = 0, EMPC = 0, SDMP = 0, SDMPC = 0, EFM = 0, MVC = 0, FWMPC2 = 0;
    for(SourcePort = 0; SourcePort < Config->Source_PortConfig.Ports; SourcePort++)
    {
        DPVsrc = DPVsrc | (1 << Config->Source_PortConfig.Port[SourcePort].PortNumber);
    }
    DPVsrc =  DPVsrc | (Config->Source_PortConfig.CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    for(DestPort = 0; DestPort < Config->Dest_PortConfig.Ports; DestPort++)
    {
        DPVtrgt = DPVtrgt | (1 << Config->Dest_PortConfig.Port[DestPort].PortNumber);
    }
    DPVtrgt = DPVtrgt | (Config->Dest_PortConfig.CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    MCSD = Config->CSDN;
    CMP = Config->CPU_Mirror_Priority_Value;
    CMPC = Config->CPU_Mirror_Priority_Type;
    EMP = Config->Eth_Mirror_Priority_Value;
    EMPC = Config->Eth_Mirror_Priority_Type;
    SDMP = Config->SD_Mirror_Priority_Value;
    SDMPC = Config->SD_Mirror_Priority_Type;
    EFM = Config->Error_Mirror;
    MVC = Config->ForwardSourceDest;
    FWMPC2 = (MVC << 25) | (EFM << 24) | (SDMPC << 23) | (SDMP << 20) | (EMPC << 19) | (EMP << 16) 
    | (CMPC << 15) | (CMP << 12) | (MCSD);
    
    for(EthMirror = 0; EthMirror < Config->DestEThMirrorPort.Ports; EthMirror++)
    {
        DPVeth = DPVeth | (1 << Config->DestEThMirrorPort.Port[EthMirror].PortNumber);
    }
    DPVeth =  DPVeth | (Config->DestEThMirrorPort.CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    for(CPUMirror = 0; CPUMirror < Config->DestCPUMirrorPort.Ports; CPUMirror++)
    {
        DPVcpu = DPVcpu | (1 << Config->DestCPUMirrorPort.Port[CPUMirror].PortNumber);
    }
    DPVcpu =  DPVcpu | (Config->DestCPUMirrorPort.CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    iowrite_and_verify_config_reg(DPVsrc, (RSWITCHFWD_FWMPC0));
    iowrite_and_verify_config_reg(DPVtrgt,(RSWITCHFWD_FWMPC1));
    iowrite_and_verify_config_reg(FWMPC2, (RSWITCHFWD_FWMPC2));
    iowrite_and_verify_config_reg(DPVeth, (RSWITCHFWD_FWMPC3));
    iowrite_and_verify_config_reg(DPVcpu, (RSWITCHFWD_FWMPC4));
    
    return 0;
}


/**
    @brief  Forwarding Engine Port Lock Configuration

    @param  rswitch_Config_fwd_Port_Lock *

    @return 0, or < 0 on error
*/
static int rswitch_Config_PortLock (struct rswitch_Config_fwd_Port_Lock *Config)
{
    uint32_t LockPorts = 0, DPVlock = 0;
    for(LockPorts = 0; LockPorts << Config->LockPort.Ports; LockPorts++)
    {
        DPVlock = DPVlock | ( 1 << Config->LockPort.Port[LockPorts].PortNumber);
    }
    DPVlock = DPVlock | (Config->LockPort.CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    iowrite_and_verify_config_reg(DPVlock, (RSWITCHFWD_FWASPLC));
    return 0;
}


/**
    @brief  Forwarding Engine Spanning Tree Protocol Configuration

    @param  rswitch_Config_fwd_Spanning_Tree *

    @return 0, or < 0 on error
*/
static int rswitch_Config_SpanningTreeProtocol (struct rswitch_Config_fwd_Spanning_Tree *Config)
{
    uint32_t Ports = 0, DPVdis = 0, DPVblk = 0, DPVlrn = 0, DPVfwd = 0, DPVlrnfwd = 0; 
    for(Ports = 0; Ports < Config->Ports; Ports++)
    {
        if(Config->SPT_Port_Config[Ports].STP_State == rswitch_Config_SPT_State_Disabled)
        {
            if(Config->SPT_Port_Config[Ports].CPU)
            {
                DPVdis = DPVdis | (Config->SPT_Port_Config[Ports].CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);

            }
            else
            {
                DPVdis = DPVdis | (1 << Config->SPT_Port_Config[Ports].PortNumber);
            }
            printk( "Disable  \n");
        }
        else if(Config->SPT_Port_Config[Ports].STP_State == rswitch_Config_SPT_State_Blocked)
        {
            if(Config->SPT_Port_Config[Ports].CPU)
            {
                DPVblk = DPVblk | (Config->SPT_Port_Config[Ports].CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);

            }
            else
            {
                DPVblk = DPVblk | (1 << Config->SPT_Port_Config[Ports].PortNumber);            
            }
            printk( "blk  \n");
        }
        else if(Config->SPT_Port_Config[Ports].STP_State == rswitch_Config_SPT_State_Learning)
        {
            if(Config->SPT_Port_Config[Ports].CPU)
            {
                DPVlrn = DPVlrn | (Config->SPT_Port_Config[Ports].CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);

            }
            else
            {
                DPVlrn = DPVlrn | (1 << Config->SPT_Port_Config[Ports].PortNumber);
            }
            printk( "learn  \n");
        }
        else if(Config->SPT_Port_Config[Ports].STP_State == rswitch_Config_SPT_State_Forwarding)
        {
            if(Config->SPT_Port_Config[Ports].CPU)
            {
                DPVfwd = DPVfwd | (Config->SPT_Port_Config[Ports].CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);

            }
            else
            {
                DPVfwd = DPVfwd | (1 << Config->SPT_Port_Config[Ports].PortNumber);
            }
            printk( "frwrd  \n");

        }
        else if(Config->SPT_Port_Config[Ports].STP_State == rswitch_Config_SPT_State_LearnandFrwrd)
        {
            
            if(Config->SPT_Port_Config[Ports].CPU)
            {
                DPVlrnfwd = DPVlrnfwd | (Config->SPT_Port_Config[Ports].CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
                printk("Learn frwrd CPU \n");
            }
            else
            {
                DPVlrnfwd = DPVlrnfwd | (1 << Config->SPT_Port_Config[Ports].PortNumber);
            }
        }
        printk("Value of DPVlrnfwd = %x \n", DPVlrnfwd);
    }
    iowrite_and_verify_config_reg(DPVdis, (RSWITCHFWD_FWSTPDBC));
    iowrite_and_verify_config_reg(DPVblk, (RSWITCHFWD_FWSTPBC));
    iowrite_and_verify_config_reg(DPVlrn, (RSWITCHFWD_FWSTPLEC));
    iowrite_and_verify_config_reg(DPVfwd, (RSWITCHFWD_FWSTPFC));
    iowrite_and_verify_config_reg(DPVlrnfwd, (RSWITCHFWD_FWSTPFLC));
    return 0;
}


/**
    @brief  Forwarding Engine Migration Configuration

    @param  rswitch_Config_fwd_Migration *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Migration(struct rswitch_Config_fwd_Migration *Config)
{
    uint32_t Ports = 0,DPV = 0;
    for(Ports = 0; Ports < Config->Migration_Port.Ports; Ports++)
    {
        DPV = DPV | (1 << Config->Migration_Port.Port[Ports].PortNumber);
    }
    DPV = DPV | (Config->Migration_Port.CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    iowrite_and_verify_config_reg(DPV, (RSWITCHFWD_FWMC));
    return 0;
}


/**
    @brief  Forwarding Engine Static Authentication Configuration

    @param  rswitch_Config_fwd_Static_Auth *

    @return 0, or < 0 on error
*/
static int rswitch_Config_StaticAuth(struct rswitch_Config_fwd_Static_Auth *Config)
{
    uint32_t Ports = 0,DPV = 0;
    printk("In Static auth \n");
    for(Ports = 0; Ports < Config->Static_Auth_Port.Ports; Ports++)
    {
        DPV = DPV | (1 << Config->Static_Auth_Port.Port[Ports].PortNumber);
    }
    DPV = DPV | (Config->Static_Auth_Port.CPU << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    iowrite_and_verify_config_reg(DPV, (RSWITCHFWD_FWAC));
    return 0;
}


/**
    @brief  Forwarding Engine Search Configuration

    @param  rswitch_Config_fwd_Search_Config *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Search(struct rswitch_Config_fwd_Search_Config *Config)
{
    uint32_t QA = 0, QMAS = 0, QSI = 0, QDI = 0, QPI = 0, QVI = 0, QMAI = 0,SQP = 0,FWSOVPC0 = 0;
    uint32_t SIDTA = 0, SIDLFA = 0, SIDLFS = 0, SMAS = 0, SUNS = 0, FWSOVPC1 = 0;
    uint32_t MTA = 0, MLFA = 0, MLFS = 0, SPFA = 0, FWSOVPC2 = 0;
    
    uint32_t VTA = 0, VLFA = 0, VLFS = 0, VMAHCSD = 0, VMAHF = 0, VMAHP = 0, FWSOVPC3 = 0;  
    printk("In Search Config \n");
    QA = Config->QCI_Filtering;
    QMAS = Config->QCI_MAC_Select;
    QSI = Config->QCISPN;
    QDI = Config->QCIDEI;
    QPI = Config->QCIPCP;
    QVI = Config->QCIVLAN;
    QMAI = Config->QCIMAC;
    SQP = Config->Queue_Priority;
    FWSOVPC0 = (SQP << 15 ) | (QMAI << 14 ) | (QVI << 13 ) | (QPI << 12 ) | (QDI << 11 ) | (QSI << 10 )
    | (QMAS << 9 ) | (QA );
    iowrite_and_verify_config_reg(FWSOVPC0, (RSWITCHFWD_FWSOVPC0));

    SIDTA = Config->SID_Tbl;
    SIDLFA = Config->SID_Filter;
    SIDLFS = Config->SID_Filter_Select;
    SMAS = Config->SID_MAC_Select;
    SUNS = Config->SID_UniqueNum_Select;
    FWSOVPC1 = (SUNS << 15) | (SMAS << 14) | (SIDLFS << 13) | (SIDLFA << 12) | (SIDTA);
    iowrite_and_verify_config_reg(FWSOVPC1, (RSWITCHFWD_FWSOVPC1));



    MTA = Config->MAC_Tbl;
    MLFA = Config->MAC_Filter;
    MLFS = Config->MAC_Filter_Select;
    SPFA = Config->Src_Port_Filter;
    printk("MTA = %d \n", MTA);
    FWSOVPC2 = (SPFA << 15) | (MLFS << 14) | (MLFA << 13) | (MTA);
    printk("FWSOVPC2 = %d \n", FWSOVPC2);
    iowrite_and_verify_config_reg(FWSOVPC2, (RSWITCHFWD_FWSOVPC2));
    
    VTA = Config->VLAN_Tbl;
    VLFA = Config->VLAN_Filter;
    VLFS = Config->VLAN_Filter_Select;
    VMAHCSD = Config->VLAN_MAC_CSDN;
    VMAHF = Config->VLAN_MAC_Fwd;
    VMAHP = Config->MAC_VLAN_Priority;
    FWSOVPC3 = (VMAHP << 14) | (VMAHF << 12) | (VMAHCSD << 11) | (VLFS << 10) | (VLFA << 9)  | (VTA);
    iowrite_and_verify_config_reg(FWSOVPC3, (RSWITCHFWD_FWSOVPC3));
    
    
    printk("Search Config Done \n");

    




    return 0;
}



/**
    @brief  Forwarding Engine Private VLAN Setting

    @param  rswitch_Config_fwd_Pvt_VLAN_Settings *

    @return 0, or < 0 on error
*/
static int rswitch_Config_Private_VLAN_Setting(struct rswitch_Config_fwd_Pvt_VLAN_Settings *Config)
{
    uint32_t DPVcom[RENESAS_RSWITCH_MAX_VLAN_COMMUNITIES]= {0,0};
    uint32_t DPViso = 0, Ports = 0, community = 0, DPVpro =0; 
    for(Ports = 0; Ports < Config->Ports; Ports ++)
    {
        if(Config->Port[Ports].CPU)
        {
            switch(Config->Port[Ports].Type)
            {
            case rswitch_Config_fwd_Pvt_VLAN_Isolated:
                DPViso = DPViso | ( 1 << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
                break;
            case rswitch_Config_fwd_Pvt_VLAN_Promiscous:
                DPVpro = DPVpro | ( 1 << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
                break;
            case rswitch_Config_fwd_Pvt_VLAN_Community:
                for(community = 0; community < Config->Port[Ports].Pvt_VLAN_Communities; community++)
                {
                    DPVcom[Config->Port[Ports].Pvt_VLAN_Community[community].Community_ID] = DPVcom[Config->Port[Ports].Pvt_VLAN_Community[community].Community_ID] | 
                    ( 1 << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
                }
                break;
            default:
                printk("Invalid VLAN Setting Type \n");
                return -1;
            }


        }
        else
        {
            switch(Config->Port[Ports].Type)
            {
            case rswitch_Config_fwd_Pvt_VLAN_Isolated:
                DPViso = DPViso | ( 1 << Config->Port[Ports].PortNumber );
                break;
            case rswitch_Config_fwd_Pvt_VLAN_Promiscous:
                DPVpro = DPVpro | ( 1 << Config->Port[Ports].PortNumber);
                break;
            case rswitch_Config_fwd_Pvt_VLAN_Community:
                for(community = 0; community < Config->Port[Ports].Pvt_VLAN_Communities; community++)
                {
                    DPVcom[Config->Port[Ports].Pvt_VLAN_Community[community].Community_ID] = DPVcom[Config->Port[Ports].Pvt_VLAN_Community[community].Community_ID] | 
                    ( 1 << Config->Port[Ports].PortNumber);
                }
                break;
            default:
                printk("Invalid VLAN Setting Type \n");
                return -1;
            }



        }


    }
    iowrite_and_verify_config_reg(DPViso, (RSWITCHFWD_FWPVIC));
    iowrite_and_verify_config_reg(DPVpro, (RSWITCHFWD_FWPVPC));
    for(community = 0; community < RENESAS_RSWITCH_MAX_VLAN_COMMUNITIES; community++)
    {
        iowrite_and_verify_config_reg(DPVcom[community], (RSWITCHFWD_FWPVCC0 + (community*4)));


    }

    return 0;

}



/**
    @brief  Forwarding Engine BPDU Configuration

    @param  rswitch_Config_BPDU_Fwd *

    @return 0, or < 0 on error
*/
static int rswitch_Config_BPDU(struct rswitch_Config_BPDU_Fwd *Config)
{
    uint32_t BFCSD = 0, BFPNV = 0, BFPUE = 0, BFE = 0, FWBFC0 = 0, Mach = 0, Macl =0 , Maskh = 0, Maskl = 0, Ports = 0, DPV =0; 
    BFCSD = Config->BPDU_Dest.CSDN;
    BFPNV = Config->BPDU_Dest.Priority;
    BFPUE = Config->BPDU_Dest.Priority_Enable;
    BFE = Config->bEnable;
    FWBFC0 = (BFE << 16) | (BFPUE << 15) | (BFPNV << 12) | (BFCSD); 
    Mach = ((uint32_t)Config->MAC[0] << 8) | ((uint32_t)Config->MAC[1]);
    Macl = ((uint32_t)Config->MAC[2] << 24) | ((uint32_t)Config->MAC[3] << 16) | ((uint32_t)Config->MAC[4] << 8) |((uint32_t)Config->MAC[5]);
    Maskh = ((uint32_t)Config->Mask[0] << 8) | ((uint32_t)Config->Mask[1]);
    Maskl = ((uint32_t)Config->Mask[2] << 24) | ((uint32_t)Config->Mask[3] << 16) | ((uint32_t)Config->Mask[4] << 8) |((uint32_t)Config->Mask[5]);
    for(Ports = 0; Ports < Config->BPDU_Dest.Dest_Eths; Ports++)
    {
        DPV =  DPV | (1 << Config->BPDU_Dest.Dest_Eth[Ports].PortNumber);

    }
    DPV = DPV | (Config->BPDU_Dest.CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
    iowrite_and_verify_config_reg(FWBFC0, (RSWITCHFWD_FWBFC0));
    iowrite_and_verify_config_reg(Mach, (RSWITCHFWD_FWBFC1));
    iowrite_and_verify_config_reg(Macl, (RSWITCHFWD_FWBFC2));
    iowrite_and_verify_config_reg(Maskh, (RSWITCHFWD_FWBFC3));
    iowrite_and_verify_config_reg(Maskl, (RSWITCHFWD_FWBFC4));
    iowrite_and_verify_config_reg(DPV, (RSWITCHFWD_FWBFC5));
    return 0;
}

/**
    @brief  Forwarding Engine VLAN Table Configuration

    @param  rswitch_Config_fwd_Insert_VLAN_Config *

    @return 0, or < 0 on error
*/
static int rswitch_Config_VLAN_Tbl(struct rswitch_Config_fwd_Insert_VLAN_Config *Config)
{
    uint32_t entries =0 , VLAN = 0, DPVfil = 0, DPVrou = 0, Ports = 0, VCSDL = 0, VPNVL = 0, VPUEL = 0, VCMEL = 0, VEMEL = 0, FWVTL3 = 0, FWVTLR = 0, i = 0, learn = 0;
    for(entries = 0; entries < Config->VLANFwdEntries; entries++)
    {
        VLAN = Config->VLANFwd[entries].VLAN_ID;
        DPVfil = 0; 
        DPVrou = 0; 
        for(Ports = 0; Ports < Config->VLANFwd[entries].VLAN_Filter.Ports; Ports++)
        {
            DPVfil = DPVfil | (1 << Config->VLANFwd[entries].VLAN_Filter.Port[Ports].PortNumber);


        }
        DPVfil = DPVfil | (Config->VLANFwd[entries].VLAN_Filter.CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
        for(Ports = 0; Ports < Config->VLANFwd[entries].VLAN_Routing.Ports; Ports++)
        {
            DPVrou = DPVrou | (1 << Config->VLANFwd[entries].VLAN_Routing.Port[Ports].PortNumber);


        }
        DPVrou = DPVrou | (Config->VLANFwd[entries].VLAN_Routing.CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
        VCSDL = Config->VLANFwd[entries].CSDN;
        VPNVL = Config->VLANFwd[entries].PCP;
        VPUEL = Config->VLANFwd[entries].PCPUpdate;
        VCMEL = Config->VLANFwd[entries].CPUMirror;
        VEMEL = Config->VLANFwd[entries].EthMirror;
        FWVTL3 = (VEMEL << 17) | (VCMEL << 16) | (VPUEL << 15) | (VPNVL << 12) | (VCSDL);
        iowrite_and_verify_config_reg(VLAN, (RSWITCHFWD_FWVTL0));
        iowrite_and_verify_config_reg(DPVfil, (RSWITCHFWD_FWVTL1));
        iowrite_and_verify_config_reg(DPVrou, (RSWITCHFWD_FWVTL2));
        iowrite_and_verify_config_reg(FWVTL3, (RSWITCHFWD_FWVTL3));
        iowrite_and_verify_config_reg(0x80000000, (RSWITCHFWD_FWVTLR));
        for (i = 0; i < RSWITCH_FWD_CONFIG_TIMEOUT_MS; i++) 
        {
            FWVTLR  = ioread_config_reg(RSWITCHFWD_FWVTLR);

            if ((FWVTLR & 0x80000000) == 0x0000000)
            {
                learn  = 1;
                break;
            }
            
            mdelay(1);
        }
        if(learn == 0)
        {
            printk("VLAN Table Learning Failed \n");
            return -1;
        }
        else
        {
            learn  = 0;

        }            
        


    }
    return 0;
}



/**
    @brief  Forwarding Engine L2 Table Configuration

    @param  rswitch_Config_fwd_Insert_L2_Config *

    @return 0, or < 0 on error
*/
static int rswitch_Config_L2_Tbl(struct rswitch_Config_fwd_Insert_L2_Config *Config)
{
    
    
    uint32_t entries = 0, Mach = 0, Macl = 0, DPV = 0, Ports = 0, MCSDL = 0, MPNVL =0,  MPUEL = 0, MCMEL = 0, MEMEL = 0, MLFEL = 0, MDEL = 0;
    uint32_t  FWMTL3  = 0, FWMTLR = 0, i = 0, learn = 0; 
    for(entries = 0; entries < Config->L2FwdEntries; entries++)
    {
        
        
        Mach = ((uint32_t)Config->L2Fwd[entries].MAC[0] << 8) | ((uint32_t)Config->L2Fwd[entries].MAC[1]);
        Macl = ((uint32_t)Config->L2Fwd[entries].MAC[2] << 24) | ((uint32_t)Config->L2Fwd[entries].MAC[3] << 16) | ((uint32_t)Config->L2Fwd[entries].MAC[4] << 8) | ((uint32_t)Config->L2Fwd[entries].MAC[5]);
        DPV = 0;
        
        for(Ports = 0; Ports < Config->L2Fwd[entries].Destination.DestEths; Ports++)
        {
            DPV = DPV | (1 << Config->L2Fwd[entries].Destination.DestEth[Ports].PortNumber);


        }
        DPV = DPV | (Config->L2Fwd[entries].Destination.CPU_Enable << RENESAS_RSWITCH_MAX_ETHERNET_PORTS);
        MCSDL = Config->L2Fwd[entries].Destination.CSDN;
        MPNVL = Config->L2Fwd[entries].Destination.PCP;
        MPUEL = Config->L2Fwd[entries].Destination.PCPUpdate;
        MCMEL = Config->L2Fwd[entries].Destination.CPUMirror;
        MEMEL = Config->L2Fwd[entries].Destination.EthMirror;
        MLFEL = Config->L2Fwd[entries].Destination.ListFilter;
        MDEL =  Config->L2Fwd[entries].Destination.Dynamic;
        FWMTL3 = (MDEL << 19) | (MLFEL << 18) | (MEMEL << 17)  | (MCMEL << 16)  | (MPUEL << 15) | (MPNVL << 12) | (MCSDL); 

        iowrite_and_verify_config_reg(Mach, (RSWITCHFWD_FWMTL0));
        
        
        iowrite_and_verify_config_reg(Macl, (RSWITCHFWD_FWMTL1));
        
        
        iowrite_and_verify_config_reg(DPV, (RSWITCHFWD_FWMTL2));
        
        
        iowrite_and_verify_config_reg(FWMTL3, (RSWITCHFWD_FWMTL3));
        mdelay(1);
        iowrite_and_verify_config_reg(0x80000000, (RSWITCHFWD_FWMTLR));

        for (i = 0; i < RSWITCH_FWD_CONFIG_TIMEOUT_MS; i++) 
        {
            FWMTLR  = ioread_config_reg(RSWITCHFWD_FWMTLR);

            if ((FWMTLR ) == 0x00000000)
            {
                learn  = 1;
                break;
            }
            else if((FWMTLR ) == 0x00000002)
            {
                learn = 2;
            }
            mdelay(1);
        }
        if(learn == 0)
        {
            printk("MAC Table Learning Failed with FWMTLR = %x \n",FWMTLR);
            return -1;
        }
        else if (learn == 1)
        {
            learn  = 0;

        }         
        else if (learn == 2)
        {
            learn  = 0;
            printk("MAC Table Learning Overwrite \n");
        }            
    }
    return 0;
}

/**
    @brief  Forwarding Engine QCI Set Config Change Time

    @param  rswitch_Config_PSFP *

    @param  uint32_t GateEnabled

    @return 0, or < 0 on error
*/
static int SetConfigChangeTime( struct rswitch_Config_PSFP * PSFP, uint32_t GateEnabled)
{

    struct timespec     TimeSpec;

    u64 CurrentTime = 0;
    u64 AdminBaseTime = 0;
    
    u64 numCycles = 0;
    rswitchptp_GetTime(&TimeSpec);
    
    CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
    AdminBaseTime = PSFP->AdminBaseTime.nseconds;
    if(AdminBaseTime > (CurrentTime + (PSFP->SWTimeMultiplier * PSFP->AdminCycleTime)))
    {
        PSFP->ConfigChangeTime = AdminBaseTime;
        if(GateEnabled)
        {
            PSFP->ConfigPending = 1;
        }

    }
    else if (!GateEnabled)
    {
        
        numCycles = div64_u64((CurrentTime - AdminBaseTime), PSFP->AdminCycleTime);
        numCycles +=PSFP->SWTimeMultiplier;
        PSFP->ConfigChangeTime = (numCycles*PSFP->AdminCycleTime) + AdminBaseTime;


    }
    else
    {

        
        numCycles = div64_u64((CurrentTime - AdminBaseTime), PSFP->AdminCycleTime);
        numCycles +=PSFP->SWTimeMultiplier;
        PSFP->ConfigChangeTime = (numCycles*PSFP->AdminCycleTime) + AdminBaseTime;
        PSFP->ConfigPending = 1;
    }



    return 0;



}


/**
    @brief  Forwarding Engine Set Cycle Start Time

    @param  PortNumber, OperBaseTime, OperCycleTime, OperCycleTimeExtension, ConfigChangeTime
            ConfigPending, SWTimeMultiplier, last_cmn_interval

    @return 0, or < 0 on error
*/
static int SetCycleStartTime(u32 PortNumber, u64 OperBaseTime, u64 OperCycleTime, 
u64 OperCycleTimeExtension, u64 ConfigChangeTime, 
u32 ConfigPending, u32 SWTimeMultiplier, u32 last_cmn_interval)
{

    struct timespec     TimeSpec;
    u64 time_ns = 0;
    u64 time_ns_lower = 0;
    u64 time_ns_middle = 0;
    u64 time_ns_upper = 0;
    u64 CurrentTime = 0;
    u64       CycleStartTime = 0;
    u64 numCycles = 0;
    
    

    u32 LIT  = 0;
    if(ConfigPending == 0)
    {
        rswitchptp_GetTime(&TimeSpec);
        
        CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
        
        if(OperBaseTime > (CurrentTime +(SWTimeMultiplier * OperCycleTime))) 
        {
            
            CycleStartTime = OperBaseTime;
            while((CycleStartTime - CurrentTime) > 2000000000)
            {
                rswitchptp_GetTime(&TimeSpec);
                CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
            }
            

        }
        else 
        {
            
            numCycles = div64_u64((CurrentTime - OperBaseTime), OperCycleTime);
            numCycles +=SWTimeMultiplier;
            
            
            CycleStartTime = (numCycles*OperCycleTime) + OperBaseTime;
            
            while((CycleStartTime - CurrentTime) > 2000000000)
            {
                rswitchptp_GetTime(&TimeSpec);
                CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
                numCycles = div64_u64((CurrentTime - OperBaseTime), OperCycleTime);
                SWTimeMultiplier = SWTimeMultiplier - 10;
                numCycles +=SWTimeMultiplier;
                CycleStartTime = (numCycles*OperCycleTime) + OperBaseTime;
                
            }
            
#ifdef TAS_DEBUG
            printk("Current Time is %lx \n", CurrentTime);
            printk("Cycke start time is %lx \n", CycleStartTime);
#endif
            
            
        }
        
        
        time_ns  =  CycleStartTime ;
        
        time_ns_lower  = time_ns & 0xFFFFFFFF;
        
        
        time_ns_middle  = (uint64_t)(time_ns >> 32) & 0xFFFFFFFF;
        
        iowrite_and_verify_config_reg(time_ns_lower , (RSWITCHFWD_FWGACST20  + ((PortNumber)*4))); 
        iowrite_and_verify_config_reg(time_ns_middle , (RSWITCHFWD_FWGACST10  + ((PortNumber)*4))); 
        iowrite_and_verify_config_reg(time_ns_upper , (RSWITCHFWD_FWGACST00  + ((PortNumber)*4))); 
        
    }
    else
    {
        rswitchptp_GetTime(&TimeSpec);
        CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
        /* Just to reconfigure with always good start time */
        ConfigChangeTime = CurrentTime + (OperCycleTime) + OperCycleTimeExtension + 10000;
        if(ConfigChangeTime > (CurrentTime + (OperCycleTime) + OperCycleTimeExtension))
        {
            numCycles = div64_u64((CurrentTime - OperBaseTime), OperCycleTime);
            numCycles +=SWTimeMultiplier;
            CycleStartTime = (numCycles*OperCycleTime) + OperBaseTime;
            while((CycleStartTime - CurrentTime) > 2000000000)
            {
                rswitchptp_GetTime(&TimeSpec);
                CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;
                numCycles = div64_u64((CurrentTime - OperBaseTime), OperCycleTime);
                SWTimeMultiplier = SWTimeMultiplier - 10;
                numCycles +=SWTimeMultiplier;
                CycleStartTime = (numCycles*OperCycleTime) + OperBaseTime;
            }
            
            
            time_ns  =  CycleStartTime ;
            time_ns_lower  = time_ns & 0xFFFFFFFF;
            time_ns_middle  = (time_ns >> 32) & 0xFFFFFFFF;
            
            iowrite_and_verify_config_reg(time_ns_lower , (RSWITCHFWD_FWGACST20  + ((PortNumber)*4))); 
            iowrite_and_verify_config_reg(time_ns_middle , (RSWITCHFWD_FWGACST10  + ((PortNumber)*4))); 
            iowrite_and_verify_config_reg(time_ns_upper , (RSWITCHFWD_FWGACST00  + ((PortNumber)*4))); 
            LIT = CycleStartTime - last_cmn_interval - OperCycleTimeExtension;
            time_ns  =  LIT ;
            time_ns_lower  = time_ns & 0xFFFFFFFF;
            time_ns_middle  = (time_ns >> 32) & 0xFFFFFFFF;
            
            iowrite_and_verify_config_reg(time_ns_lower , (RSWITCHFWD_FWGALIT20  + ((PortNumber)*4))); 
            iowrite_and_verify_config_reg(time_ns_middle , (RSWITCHFWD_FWGALIT10  + ((PortNumber)*4))); 
            iowrite_and_verify_config_reg(time_ns_upper , (RSWITCHFWD_FWGALIT00  + ((PortNumber)*4))); 
            
            

        }
        else
        {
            
            CycleStartTime = ConfigChangeTime;
            time_ns  =  CycleStartTime ;
            time_ns_lower  = time_ns & 0xFFFFFFFF;
            time_ns_middle  = (time_ns >> 32) & 0xFFFFFFFF;
            
            iowrite_and_verify_config_reg(time_ns_lower , (RSWITCHFWD_FWGALIT20  + ((PortNumber)*4))); 
            iowrite_and_verify_config_reg(time_ns_middle , (RSWITCHFWD_FWGALIT10  + ((PortNumber)*4))); 
            iowrite_and_verify_config_reg(time_ns_upper , (RSWITCHFWD_FWGALIT00  + ((PortNumber)*4))); 
            
            
        }
        
    }
    
    return 0;






}




/**
    @brief  Forwarding Engine Configure QCI Gate Filters

    @param  void

    @return 0, or < 0 on error
*/
static int rswitch_Config_Gate_Filters(void)
{
    uint32_t Gates  = 0;    
    uint32_t GateID = 0;
    uint32_t PSFP_Enable  = 0;
    
    
    
    uint32_t GateControls = 0;
    uint32_t GateControl  = 0;
    uint32_t State = 0;
    uint32_t IPV  = 0;
    uint32_t GIS  = 0;
    uint32_t TimeInterval = 0;
    uint32_t schedule_num =0;
    uint32_t GATAL  = 0;
    uint32_t FWGATL1  = 0;
    uint32_t gptp_timer_domain = 0;
    uint32_t jitter_clock  = 0;
    uint32_t transmission_latency  = 0; 
    uint32_t FWMIS1 = 0;
    uint32_t FWGATL2 = 0;
    uint32_t FWGATL0 = 0;
    uint32_t i, learn  = 0;
    uint32_t Schedule = 0;
    uint32_t FWQGMC, FWGIGS = 0;
    uint32_t FWGAEN = 0;
    uint32_t FWQIUEC  = 0;
    uint32_t PortNumber = 0, InvalidRx = 0, IPVEnable  = 0;
    uint32_t InitialGates  = 0;
    uint32_t InitialGateID  = 0;
    uint32_t last_cmn_interval[RENESAS_RSWITCH_MAX_PORTS ];
    uint64_t OperBaseTime  = 0;
    uint64_t OperCycleTime = 0;
    uint64_t OperCycleTimeExtension  = 0;
    u64 CurrentTime  = 0;
    struct timespec     TimeSpec;
    
    
    for(Schedule = 0; Schedule < rswitchfwd_Config.qci_config.Schedules; Schedule++)
    {
        gptp_timer_domain = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].timer_domain;
        PortNumber = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].PortNumber;
        iowrite_and_verify_config_reg(gptp_timer_domain, (RSWITCHFWD_FWGGTS0 + ((PortNumber)*4)));
        jitter_clock = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].jitter_time;
        iowrite_and_verify_config_reg(jitter_clock, (RSWITCHFWD_FWGJC0 + ((PortNumber)*4)));
        transmission_latency = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].latency_time;
        iowrite_and_verify_config_reg(transmission_latency, (RSWITCHFWD_FWGTL0 + ((PortNumber)*4)));
        for(InitialGates = 0; InitialGates < rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].AdminGateStates; InitialGates++)
        {
            /* Initial Gate state will be moved separately not be taken from gate control list*/
            State = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].AdminGateState[InitialGates].State;
            IPV   = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].AdminGateState[InitialGates].IPV;
            GIS  = (IPV << 1) | State;
            InitialGateID = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].AdminGateState[InitialGates].GateID;
            FWGIGS = ioread_config_reg(RSWITCHFWD_FWGIGS0 + ((InitialGateID/8) * 4));
            FWGIGS = FWGIGS | (GIS << ((InitialGateID%8)*4));
            iowrite_and_verify_config_reg(FWGIGS, (RSWITCHFWD_FWGIGS0  + ((InitialGateID/8)*4)));


        } 
        for(Gates = 0; Gates <  rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].Gates; Gates++)
        {
            
            GateID = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].Gate[Gates].GateID;
            /*GateId/8 = PortNumber*/
            FWQGMC = ioread_config_reg(RSWITCHFWD_FWQGMC0 + ((GateID/8) * 4));
            InvalidRx = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].Gate[Gates].Invalid_Rx;
            FWQGMC = FWQGMC | (InvalidRx << (GateID%8));
            iowrite_and_verify_config_reg(FWQGMC, (RSWITCHFWD_FWQGMC0 + ((GateID/8)*4)));
            GateControls =  rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].Gate[Gates].GateControls;
            FWGAEN = ((GateID%8) < 4)?  ioread_config_reg(RSWITCHFWD_FWGAEN00 + ((GateID/8) * 4)):
            ioread_config_reg(RSWITCHFWD_FWGAEN10 + ((GateID/8) * 4));
            FWGAEN = FWGAEN | (GateControls << ((GateID%4) * 8));
            iowrite_and_verify_config_reg(FWGAEN, ((((GateID%8) < 4)? RSWITCHFWD_FWGAEN00:RSWITCHFWD_FWGAEN10)  + ((GateID/8)*4)));
            
            FWQIUEC = ioread_config_reg(RSWITCHFWD_FWQIUEC0 + ((GateID/8) * 4));
            IPVEnable = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].Gate[Gates].IPV_enable;
            FWQIUEC = FWQIUEC | (IPVEnable << (GateID%8));
            iowrite_and_verify_config_reg(FWQIUEC, (RSWITCHFWD_FWQIUEC0  + ((GateID/8)*4)));
            
            /* Does the table will start from Gate State 0 or 1 since we have programmed Gate state 0 as initial gate state */
            for(GateControl = 0; GateControl < GateControls; GateControl++)
            {
                State = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].Gate[Gates].GateControl[GateControl].State;
                IPV   = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].Gate[Gates].GateControl[GateControl].IPV;
                GIS  = (IPV << 1) | State;
                TimeInterval = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].Gate[Gates].GateControl[GateControl].GateTimeTickMultiplier;
                FWGATL0 = (GIS << 28) | TimeInterval;
                iowrite_and_verify_config_reg(FWGATL0, RSWITCHFWD_FWGATL0);
                schedule_num = GateID/8;
                GATAL  =  GateID%8;
                FWGATL1 = (schedule_num << 16) | GATAL;
                /* Assuming Gate Filter Admin Table address learn as GateControl List Num in each schedule, Needs to be confired by Japan*/
                iowrite_and_verify_config_reg(FWGATL1, RSWITCHFWD_FWGATL1);
                iowrite_and_verify_config_reg(0x80000000, RSWITCHFWD_FWGATL2);
                for (i = 0; i < RSWITCH_FWD_CONFIG_TIMEOUT_MS; i++) 
                {
                    FWGATL2  = ioread_config_reg(RSWITCHFWD_FWGATL2);

                    if ((FWGATL2 & 0x80000000) == 0x80000000)
                    {
                        learn  = 1;
                        break;
                    }
                    mdelay(1);
                }
                if(learn == 0)
                {
                    printk("Gate Table Learning Failed \n");
                    return -1;
                }
                else
                {
                    learn  = 0;

                }                           
            }
        }
        FWMIS1  = ioread_config_reg(RSWITCHFWD_FWMIS1);
        PSFP_Enable  = (FWMIS1 >> (PortNumber)) & 0x01;
        SetConfigChangeTime(&rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule], PSFP_Enable);
        rswitchptp_GetTime(&TimeSpec);
        CurrentTime = ((u64)(TimeSpec.tv_sec * (u64)1000000000)) + TimeSpec.tv_nsec;

        OperBaseTime = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].AdminBaseTime.nseconds;
        OperCycleTime = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].AdminCycleTime;
        OperCycleTimeExtension =  rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].CycleTimeExtension.nseconds;
        last_cmn_interval[PortNumber] = rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].
        Gate[0].GateControl[RENESAS_RSWITCH_MAX_INGRESS_GATE_CONTROLS - 1].GateTimeTickMultiplier;
        SetCycleStartTime(PortNumber,OperBaseTime,OperCycleTime,OperCycleTimeExtension,
        rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].ConfigChangeTime, 
        rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].ConfigPending, 
        rswitchfwd_Config.qci_config.QCI_Gate_Config[Schedule].SWTimeMultiplier , last_cmn_interval[PortNumber]);


        
        
        if(PSFP_Enable)
        {

            
            iowrite_and_verify_config_reg(1 , (RSWITCHFWD_FWGCC0 + ((PortNumber)*4))); 
            

        }
        else
        {
            
            iowrite_and_verify_config_reg(1 , (RSWITCHFWD_FWGSE0 + ((PortNumber)*4))); 


        }
    }

    return 0;
}


/**
    @brief  Forwarding Engine Configure QCI Table

    @param  void

    @return 0, or < 0 on error
*/
static int rswitch_Config_QCI_Tbl(void)
{

    uint32_t FWRR = 0;
    uint32_t FWSOVPC0 = 0;
    uint32_t DEI  = 0;
    uint32_t PCP =0;
    uint32_t VLAN = 0;
    uint32_t Macl,Mach  = 0;
    uint32_t Port_Number = 0;
    uint32_t FWQTL3  = 0;
    uint32_t i, learn  = 0;
    uint32_t FWQTLR,FWQTL0  = 0;
    uint32_t Filters = 0;
    FWRR  = ioread_config_reg(RSWITCHFWD_FWRR);
    if((FWRR & 0x01) == 0x01)
    {
        FWSOVPC0 = ioread_config_reg(RSWITCHFWD_FWSOVPC0);
        FWSOVPC0 = FWSOVPC0 & 0xFFFFFFFE;
        iowrite_and_verify_config_reg(FWSOVPC0, RSWITCHFWD_FWSOVPC0);
        for(Filters = 0; Filters < rswitchfwd_Config.qci_config.Filters; Filters++)
        {
            
            
            DEI = rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].DEI;
            PCP = rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Ingress_PCP_Config.PCP_ID;
            VLAN  = rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Ingress_VLAN_Config.VLAN_ID;
            Mach = (uint32_t)rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Filter_ID[1] |
            (uint32_t)((uint32_t)rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Filter_ID[0] << 8);

            FWQTL0 = Mach | (VLAN << 16) | (PCP << 28) | (DEI << 31);
            iowrite_and_verify_config_reg(FWQTL0 , (RSWITCHFWD_FWQTL0)); 
            Macl = (uint32_t)rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Filter_ID[5] |
            (uint32_t)((uint32_t)rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Filter_ID[4] << 8) |
            (uint32_t)((uint32_t)rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Filter_ID[3] << 16)|
            (uint32_t)((uint32_t)rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Filter_ID[2] << 24);
            iowrite_and_verify_config_reg(Macl , (RSWITCHFWD_FWQTL1)); 
            Port_Number = rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Port_Number;
            iowrite_and_verify_config_reg(Port_Number , (RSWITCHFWD_FWQTL2)); 
            FWQTL3 = (rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].FilterGate.bEnable << 30) |
            (rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].FilterGate.Gate_ID << 16) |
            (rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].FilterMSDU.bEnable << 29) |
            (rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].FilterMSDU.MSDU_ID << 8) |
            (rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Ingress_Filter_Meter.bEnable << 28) |
            (rswitchfwd_Config.qci_config.Ingress_Filters_Config[Filters].Ingress_Filter_Meter.Meter_ID << 0);
            iowrite_and_verify_config_reg(Port_Number , (RSWITCHFWD_FWQTL3)); 
            iowrite_and_verify_config_reg(0x80000000, RSWITCHFWD_FWQTLR);
            for (i = 0; i < RSWITCH_FWD_CONFIG_TIMEOUT_MS; i++) 
            {
                FWQTLR  = ioread_config_reg(RSWITCHFWD_FWQTLR);
                
                if ((FWQTLR & 0x80000000) == 0x80000000)
                {
                    learn  = 1;
                    break;
                }
                mdelay(1);
            }
            if(learn == 0)
            {
                printk("Filter Table Learning Failed \n");
                return -1;
            }
            else
            {
                learn  = 0;

            }
            
            
        }
        FWSOVPC0 = ioread_config_reg(RSWITCHFWD_FWSOVPC0);
        FWSOVPC0 = FWSOVPC0 | 0x1;
        iowrite_and_verify_config_reg(FWSOVPC0, RSWITCHFWD_FWSOVPC0);

    }

    return 0;

}


/**
    @brief  Forwarding Engine Configure QCI Meters

    @param  void

    @return 0, or < 0 on error
*/
static int rswitch_Config_Meters(void)
{

    uint32_t Meters  = 0;
    uint32_t MeterID  = 0;
    uint32_t MarkallFrameRed = 0;
    uint32_t FWQMMC = 0;
    uint32_t MarkallFrameRedEnable = 0;
    uint32_t FWQRFDC  = 0;
    uint32_t DropOnYellow  = 0;
    uint32_t FWQYFDC = 0;
    uint32_t i  = 0;
    uint32_t colourcode  = 0;
    uint32_t pcp_colour_map, dei_colour_map = 0;
    uint32_t FWQVTCMC  = 0;
    uint32_t CBS,CIR,EBS,EIR = 0;
    uint32_t Coupling  = 0;
    uint32_t FWQMEC  = 0;
    uint32_t FWQMCFC = 0;
    for(Meters = 0; Meters <  rswitchfwd_Config.qci_config.Meters; Meters++)
    {
        MeterID = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].Meter_Id;

        /*Meter Mode */
        MarkallFrameRed = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].Mark_All_Frames_Red;
        FWQMMC = ioread_config_reg(RSWITCHFWD_FWQMMC0 + ((MeterID/8) * 4));
        FWQMMC = FWQMMC | ( MarkallFrameRed < (MeterID%8));
        iowrite_and_verify_config_reg(FWQMMC, RSWITCHFWD_FWQMMC0 + ((MeterID/8) * 4));
        
        /*Red Frame Setting*/
        MarkallFrameRedEnable = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].Mark_All_Red_Enable;
        FWQRFDC = ioread_config_reg(RSWITCHFWD_FWQRFDC0 + ((MeterID/8) * 4));
        FWQRFDC = FWQRFDC | ( MarkallFrameRedEnable < (MeterID%8));
        iowrite_and_verify_config_reg(FWQRFDC, RSWITCHFWD_FWQRFDC0 + ((MeterID/8) * 4));
        
        /*Yellow Frame Setting */
        DropOnYellow = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].Drop_On_Yellow;
        FWQYFDC = ioread_config_reg(RSWITCHFWD_FWQYFDC0 + ((MeterID/8) * 4));
        FWQYFDC = FWQYFDC | ( DropOnYellow < (MeterID%8));
        iowrite_and_verify_config_reg(FWQYFDC, RSWITCHFWD_FWQYFDC0 + ((MeterID/8) * 4));

        /*VLAN Tag Colour Map Setting */
        for(i = 0; i <RENESAS_RSWITCH_MAX_PCP_DEI_CONFIG; i++)
        {
            colourcode = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].vlan_colour_map[i].colour_code;
            pcp_colour_map = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].vlan_colour_map[i].PCP_value;
            dei_colour_map = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].vlan_colour_map[i].DEI_value;
            FWQVTCMC = ioread_config_reg(RSWITCHFWD_FWQVTCMC0 + ((MeterID/2) * 4));
            FWQVTCMC = FWQVTCMC | ( colourcode < (((MeterID%2) * 16) + (pcp_colour_map * 2) + dei_colour_map));
            iowrite_and_verify_config_reg(FWQVTCMC, RSWITCHFWD_FWQVTCMC0 + ((MeterID/2) * 4));

        }
        /*CBS */
        CBS = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].CBS;
        
        iowrite_and_verify_config_reg(CBS, RSWITCHFWD_FWQMCBSC0 + ((MeterID) * 4));

        /*CIR */
        CIR = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].CIR;
        
        iowrite_and_verify_config_reg(CIR, RSWITCHFWD_FWQMCIRC0 + ((MeterID) * 4));


        /*EBS */
        EBS = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].EBS;
        
        iowrite_and_verify_config_reg(EBS, RSWITCHFWD_FWQMEBSC0 + ((MeterID) * 4));
        
        /*EIR */
        EIR = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].EIR;
        
        iowrite_and_verify_config_reg(EIR, RSWITCHFWD_FWQMEIRC0 + ((MeterID) * 4));

        /*Coupling Setting */
        Coupling = rswitchfwd_Config.qci_config.Ingress_Meter_Config[Meters].Coupling;
        FWQMCFC = ioread_config_reg(RSWITCHFWD_FWQMCFC0 + ((MeterID/8) * 4));
        FWQMCFC = FWQMCFC | ( Coupling < (MeterID%8));
        iowrite_and_verify_config_reg(FWQMCFC, RSWITCHFWD_FWQMCFC0 + ((MeterID/8) * 4));


        FWQMEC = ioread_config_reg(RSWITCHFWD_FWQMEC0 + ((MeterID/8) * 4));
        FWQMEC = FWQMEC | ( 1 < (MeterID%8));
        iowrite_and_verify_config_reg(FWQMEC, RSWITCHFWD_FWQMEC0 + ((MeterID/8) * 4));

    }
    return 0 ;
}


/**
    @brief  Forwarding Engine Configure MSDU Filters

    @param  void

    @return 0, or < 0 on error
*/
static int rswitch_Config_MSDU_Filters (void)
{
    uint32_t MSDUFilters = 0;
    uint32_t MSDUFilterID  = 0;
    uint32_t FWMFV = 0;
    uint32_t mode = 0;
    uint32_t FWQMFMC = 0;
    for(MSDUFilters = 0; MSDUFilters < rswitchfwd_Config.qci_config.MSDU_Filters; MSDUFilters++)
    {
        MSDUFilterID = rswitchfwd_Config.qci_config.Msdu_Config[MSDUFilters].msdu_id;
        FWMFV = rswitchfwd_Config.qci_config.Msdu_Config[MSDUFilters].msdu_size;
        iowrite_and_verify_config_reg(FWMFV, RSWITCHFWD_FWMFV0 + ((MSDUFilterID) * 4));
        mode = rswitchfwd_Config.qci_config.Msdu_Config[MSDUFilters].mode;
        FWQMFMC = ioread_config_reg(RSWITCHFWD_FWQMFMC0 + ((MSDUFilterID/8) * 4));
        FWQMFMC = FWQMFMC | ( mode < (MSDUFilterID%8));
        iowrite_and_verify_config_reg(FWQMFMC, RSWITCHFWD_FWQMFMC0 + ((MSDUFilterID/8) * 4));
    }
    return 0;
}
static int rswitch_Config_Enable_QCI(void)
{

    rswitch_Config_Gate_Filters();
    rswitch_Config_QCI_Tbl();
    
    rswitch_Config_Meters();
    rswitch_Config_MSDU_Filters();
    
    return 0;
}


#ifdef STATIC_TEST
static int rswitch_fill_static_structure(void)
{
    
    /*Basic Config No QCI, will be done, once rswitchtool ready*/
    rswitchfwd_Config.qci_config.bEnable = 0;
    /*Configure One Broadcast */
    rswitchfwd_Config.Broadcast_Config_Ports = 1;
    rswitchfwd_Config.Broadcast_Config[0].Broadcast_Config_Port.PortNumber = 0;
    rswitchfwd_Config.Broadcast_Config[0].Dest_Eths = 1;
    rswitchfwd_Config.Broadcast_Config[0].Dest_Eth[0].PortNumber  = 1;
    rswitchfwd_Config.Broadcast_Config[0].CPU_Enable = 0;
    /*One Source Forwarding */
    rswitchfwd_Config.Source_Fwd.bEnable =  1;
    rswitchfwd_Config.Source_Fwd.source_fwd_port_configs = 1;
    rswitchfwd_Config.Source_Fwd.Source_Port_Config[0].PortNumber = 0;
    rswitchfwd_Config.Source_Fwd.Source_Port_Config[0].CPU_Mirror = 0;
    rswitchfwd_Config.Source_Fwd.Source_Port_Config[0].Eth_Mirror = 1;
    
    rswitchfwd_Config.Source_Fwd.Source_Port_Config[0].Dest_Config.Dest_Eths = 1;
    rswitchfwd_Config.Source_Fwd.Source_Port_Config[0].Dest_Config.Dest_Eth[0].PortNumber = 1;
    rswitchfwd_Config.Source_Fwd.Source_Port_Config[0].Dest_Config.CPU_Enable = 0;
    /*No Dynamic Authentication */
    rswitchfwd_Config.Dynamic_Auth.bEnable = 0;
    /* One Dest Forwarding Entry */
    rswitchfwd_Config.Dest_Fwd.bEnable = 1;
    rswitchfwd_Config.Dest_Fwd.DestFwdEntries = 1;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].MAC[0] = 0x11;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].MAC[1] = 0x22;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].MAC[2] = 0x33;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].MAC[3] = 0x44;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].MAC[4] = 0x55;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].MAC[5] = 0x66;
    /* Full Comparison */
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Mask[0] = 0x00;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Mask[1] = 0x00;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Mask[2] = 0x00;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Mask[3] = 0x00;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Mask[4] = 0x00;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Mask[5] = 0x00;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Destination.Dest_Eths = 1;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Destination.Dest_Eth[0].PortNumber = 0;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Destination.CPU_Enable = 0;
    rswitchfwd_Config.Dest_Fwd.DestFwd_Entry[0].Destination.Priority = 1;
    /*No Private VLAN Configuration */
    rswitchfwd_Config.Priv_VLAN.bEnable = 0;
    /*Double Tag Disable */
    rswitchfwd_Config.Double_Tag.bEnable = 0;
    /*No Agent Filter Ports*/
    rswitchfwd_Config.AgentFilterCSDPorts.Eths =  0;
    /*Water Mark Control Disable*/
    rswitchfwd_Config.WaterMarkControl.bEnable = 0;
    /*Exceptional Path Routing Disable */
    rswitchfwd_Config.ExceptionalPath.bEnable = 0;
    /*Hardware Learning Enable */
    rswitchfwd_Config.Learning.UnknownSourcePortLearn = 1;
    rswitchfwd_Config.Learning.HWLearning = 1;
    rswitchfwd_Config.Learning.VLANLearning = 1;
    rswitchfwd_Config.Learning.MACLearning = 1;
    rswitchfwd_Config.Learning.Priority = 1;
    /*Mirroring Enabled*/
    rswitchfwd_Config.Mirroring.bEnable = 1;
    rswitchfwd_Config.Mirroring.Source_PortConfig.Ports = 1;
    rswitchfwd_Config.Mirroring.Source_PortConfig.Port[0].PortNumber = 0;
    rswitchfwd_Config.Mirroring.Source_PortConfig.CPU = 0;
    rswitchfwd_Config.Mirroring.Dest_PortConfig.Ports = 1;
    rswitchfwd_Config.Mirroring.Dest_PortConfig.Port[0].PortNumber = 2;
    rswitchfwd_Config.Mirroring.Dest_PortConfig.CPU = 0;
    rswitchfwd_Config.Mirroring.ForwardSourceDest = rswitch_Mirror_Forward_SrcorDest;
    rswitchfwd_Config.Mirroring.Error_Mirror = 0;
    rswitchfwd_Config.Mirroring.SD_Mirror_Priority_Type = rswitch_Mirror_Priority_Descriptor;
    rswitchfwd_Config.Mirroring.Eth_Mirror_Priority_Type = rswitch_Mirror_Priority_Descriptor;
    rswitchfwd_Config.Mirroring.Eth_Mirror_Priority_Type = rswitch_Mirror_Priority_Mirror;
    rswitchfwd_Config.Mirroring.DestEThMirrorPort.Ports = 1;
    rswitchfwd_Config.Mirroring.DestEThMirrorPort.Port[0].PortNumber = 1;
    rswitchfwd_Config.Mirroring.DestEThMirrorPort.CPU = 0;
    rswitchfwd_Config.Mirroring.DestCPUMirrorPort.Ports = 0;
    rswitchfwd_Config.Pvt_VLAN_Settings.Ports = 3;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[0].Type = rswitch_Config_fwd_Pvt_VLAN_Promiscous;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[0].PortNumber = 0;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[0].CPU = 0;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[1].Type = rswitch_Config_fwd_Pvt_VLAN_Promiscous;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[1].PortNumber = 1;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[1].CPU = 0;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[2].Type = rswitch_Config_fwd_Pvt_VLAN_Promiscous;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[2].PortNumber = 2;
    rswitchfwd_Config.Pvt_VLAN_Settings.Port[2].CPU = 0;
    /*No Lock Ports */
    rswitchfwd_Config.Port_Lock.bEnable = 0;
    
    /*Spanning Tree Protocol Enabled for Learning */
    rswitchfwd_Config.Spanning_Tree.Ports = 3;
    rswitchfwd_Config.Spanning_Tree.SPT_Port_Config[0].PortNumber = 0;
    rswitchfwd_Config.Spanning_Tree.SPT_Port_Config[1].PortNumber = 1;
    rswitchfwd_Config.Spanning_Tree.SPT_Port_Config[2].CPU = 1;
    rswitchfwd_Config.Spanning_Tree.SPT_Port_Config[0].STP_State = rswitch_Config_SPT_State_LearnandFrwrd;
    rswitchfwd_Config.Spanning_Tree.SPT_Port_Config[1].STP_State = rswitch_Config_SPT_State_LearnandFrwrd;
    rswitchfwd_Config.Spanning_Tree.SPT_Port_Config[2].STP_State = rswitch_Config_SPT_State_LearnandFrwrd;
    /*Migration enabled */
    rswitchfwd_Config.Migration.bEnable = 1;
    rswitchfwd_Config.Migration.Migration_Port.Ports = 3;
    rswitchfwd_Config.Migration.Migration_Port.Port[0].PortNumber = 0;
    rswitchfwd_Config.Migration.Migration_Port.Port[1].PortNumber = 1;
    rswitchfwd_Config.Migration.Migration_Port.Port[2].PortNumber = 2;
    rswitchfwd_Config.Migration.Migration_Port.CPU  = 0;
    /*Static Authentication Enabled */
    rswitchfwd_Config.Static_Auth.bEnable = 1;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.Ports = 7;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.Port[0].PortNumber = 0;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.Port[1].PortNumber = 1;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.Port[2].PortNumber = 2;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.Port[3].PortNumber = 3;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.Port[4].PortNumber = 4;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.Port[5].PortNumber = 5;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.Port[6].PortNumber = 6;
    rswitchfwd_Config.Static_Auth.Static_Auth_Port.CPU = 1;
    /*Search Config Must */
    rswitchfwd_Config.Search_Config.Queue_Priority = rswitch_Queue_Priority_PCP;
    rswitchfwd_Config.Search_Config.QCIMAC = rswitch_fwd_Config_Include;
    rswitchfwd_Config.Search_Config.QCIVLAN = rswitch_fwd_Config_Include;
    rswitchfwd_Config.Search_Config.QCIPCP = rswitch_fwd_Config_Include;
    rswitchfwd_Config.Search_Config.QCIDEI = rswitch_fwd_Config_Include;
    rswitchfwd_Config.Search_Config.QCISPN = rswitch_fwd_Config_Include;
    rswitchfwd_Config.Search_Config.QCI_MAC_Select = rswitch_MAC_Select_DEst;
    /*QCI Filtering Disabled */
    rswitchfwd_Config.Search_Config.QCI_Filtering = 0;
    rswitchfwd_Config.Search_Config.SID_MAC_Select = rswitch_MAC_Select_DEst;
    rswitchfwd_Config.Search_Config.SID_Filter_Select = rswitch_Select_FILTER_White;
    /*All filter Inactive, VLAN and MAC Table Active active */
    rswitchfwd_Config.Search_Config.SID_Filter = rswitch_Fwd_Config_InActive;
    rswitchfwd_Config.Search_Config.SID_Tbl = rswitch_Fwd_Config_InActive;
    rswitchfwd_Config.Search_Config.Src_Port_Filter = rswitch_Fwd_Config_InActive;
    rswitchfwd_Config.Search_Config.MAC_Filter_Select = rswitch_Select_FILTER_White;
    rswitchfwd_Config.Search_Config.MAC_Filter = rswitch_Fwd_Config_InActive;
    rswitchfwd_Config.Search_Config.MAC_Tbl = rswitch_Fwd_Config_Active;
    rswitchfwd_Config.Search_Config.MAC_VLAN_Priority = rswitch_Config_Priority_MAC;
    rswitchfwd_Config.Search_Config.VLAN_MAC_Fwd = rswitch_Fwd_VLANorMAC;
    rswitchfwd_Config.Search_Config.VLAN_MAC_CSDN = rswitch_Fwd_CSDN_MAC;
    rswitchfwd_Config.Search_Config.VLAN_Filter_Select = rswitch_Select_FILTER_White;
    rswitchfwd_Config.Search_Config.VLAN_Filter = rswitch_Fwd_Config_InActive;
    rswitchfwd_Config.Search_Config.VLAN_Tbl = rswitch_Fwd_Config_Active;
    /*Configure 1 L2 MAC for Test*/
    rswitchfwd_Config.Insert_L2_Config.L2FwdEntries =  1;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].MAC[0] =0x01;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].MAC[1] =0x80;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].MAC[2] =0xC2;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].MAC[3] =0x00;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].MAC[4] =0x00;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].MAC[5] =0x0E;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.DestEths = 1;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.DestEth[0].PortNumber = 1;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.CPU_Enable = 1;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.CSDN = 0x01;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.Dynamic = 0x0;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.EthMirror = 0x1;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.CPUMirror = 0x0;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.PCPUpdate = 0x0;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.ListFilter = 0x0;
    rswitchfwd_Config.Insert_L2_Config.L2Fwd[0].Destination.PCP = 0x2;
    
    
    /*Configure 1 VLAN Entry */
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwdEntries =  1;
    
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_ID = 0x7;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_Filter.Ports =0x3;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_Filter.Port[0].PortNumber =0x0;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_Filter.Port[1].PortNumber =0x1;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_Filter.Port[2].PortNumber =0x2;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_Routing.Ports = 0x03;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_Routing.Port[0].PortNumber = 0x0;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_Routing.Port[1].PortNumber = 0x01;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].VLAN_Routing.Port[2].PortNumber = 0x02;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].EthMirror = 1;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].CPUMirror = 0x0;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].PCPUpdate = 0x0;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].PCP  = 0x03;
    rswitchfwd_Config.Insert_VLAN_Config.VLANFwd[0].CSDN = 0x10;
    
    return 0;
}

#endif

/**
    @brief  Update Forwarding Tables from New Configuration (static)

    @return 0, or < 0 on error
*/
static int UpdateConfiguration(void)
{

    uint32_t Ports = 0;
    uint8_t     MAC[RENESAS_RSWITCH_MAC_ID_LENGTH];
    memset(MAC, 0, sizeof(MAC));
    
    /*Sanity Checking onus on rswitchtool*/
    memcpy(&rswitchfwd_Config,&rswitchfwd_ConfigNew, sizeof(rswitchfwd_Config));
    if(rswitchfwd_Config.qci_config.bEnable)
    {
        /*Call QCI configuration */
        rswitch_Config_Enable_QCI();

    }
    for(Ports = 0; Ports < rswitchfwd_Config.Broadcast_Config_Ports; Ports++)
    {
        /*Call Broadcast Config */
        rswitch_Config_Broadcast(&rswitchfwd_Config.Broadcast_Config[Ports]);
    }
    if(rswitchfwd_Config.Source_Fwd.bEnable)
    {
        rswitch_Config_Src_Port_Fwd(&rswitchfwd_Config.Source_Fwd);

    }
    if(rswitchfwd_Config.BPDU_Fwd.bEnable)
    {
        rswitch_Config_BPDU(&rswitchfwd_Config.BPDU_Fwd);

    }
    if(rswitchfwd_Config.Dynamic_Auth.bEnable)
    {
        rswitch_Config_Dyamic_Auth(&rswitchfwd_Config.Dynamic_Auth);

    }
    if(rswitchfwd_Config.Dest_Fwd.bEnable)
    {
        rswitch_Config_Destination_Fwd(&rswitchfwd_Config.Dest_Fwd);

    }
    if(rswitchfwd_Config.Priv_VLAN.bEnable)
    {
        rswitch_Config_Private_VLAN(&rswitchfwd_Config.Priv_VLAN);

    }
    if(rswitchfwd_Config.Double_Tag.bEnable)
    {
        rswitch_Config_Double_Tag(&rswitchfwd_Config.Double_Tag);

    }
    if(rswitchfwd_Config.AgentFilterCSDPorts.Eths)
    {
        rswitch_Config_AgentFilterCSD(&rswitchfwd_Config.AgentFilterCSDPorts);

    }
    if(rswitchfwd_Config.WaterMarkControl.bEnable)
    {
        rswitch_Config_WaterMarkControl(&rswitchfwd_Config.WaterMarkControl);

    }
    else
    {

        iowrite_and_verify_config_reg(0x01, RSWITCHFWD_FWSOVPC4);
    }
    if(rswitchfwd_Config.ExceptionalPath.bEnable)
    {
        rswitch_Config_Exceptional_Path(&rswitchfwd_Config.ExceptionalPath);

    }
    if(rswitchfwd_Config.Learning.bEnable)
    {
        rswitch_Config_Learning(&rswitchfwd_Config.Learning);

    }
    if(rswitchfwd_Config.Mirroring.bEnable)
    {
        rswitch_Config_Mirroring(&rswitchfwd_Config.Mirroring);

    }
    if(rswitchfwd_Config.Port_Lock.bEnable)
    {
        rswitch_Config_PortLock (&rswitchfwd_Config.Port_Lock);

    }
    if(rswitchfwd_Config.Spanning_Tree.Ports)
    {
        rswitch_Config_SpanningTreeProtocol(&rswitchfwd_Config.Spanning_Tree);

    }
    if(rswitchfwd_Config.Migration.bEnable)
    {
        rswitch_Config_Migration(&rswitchfwd_Config.Migration);

    }
    if(rswitchfwd_Config.Static_Auth.bEnable)
    {
        rswitch_Config_StaticAuth(&rswitchfwd_Config.Static_Auth);

    }
    if(rswitchfwd_Config.Search_Config.bEnable)
    {
        rswitch_Config_Search(&rswitchfwd_Config.Search_Config);

    }
    if(rswitchfwd_Config.Pvt_VLAN_Settings.Ports)
    {
        rswitch_Config_Private_VLAN_Setting(&rswitchfwd_Config.Pvt_VLAN_Settings);

    }
    if(rswitchfwd_Config.Insert_L2_Config.L2FwdEntries)
    {
        rswitch_Config_L2_Tbl(&rswitchfwd_Config.Insert_L2_Config);

    }
    if(rswitchfwd_Config.Insert_VLAN_Config.VLANFwdEntries)
    {
        rswitch_Config_VLAN_Tbl(&rswitchfwd_Config.Insert_VLAN_Config);

    }
    rswitchfwd_Config.ConfigDone = 1;
    


    return 0;
}


/**
    @brief  Forwarding Engine Initialisation

    @return < 0, or 0 on success
*/
static int Initialise_ForwardingEngine(void)
{
    /* Initialise Forwarding Engine with static Values if enabled*/
    
#ifdef STATIC_TEST
    rswitch_fill_static_structure();
    rswitch_Config_Broadcast(&rswitchfwd_Config.Broadcast_Config[0]);
    rswitch_Config_SpanningTreeProtocol(&rswitchfwd_Config.Spanning_Tree);
    rswitch_Config_StaticAuth(&rswitchfwd_Config.Static_Auth);
    rswitch_Config_Search(&rswitchfwd_Config.Search_Config);
    rswitch_Config_L2_Tbl(&rswitchfwd_Config.Insert_L2_Config);
    
    
#endif
    /*Bypass by default for Direct Descriptor*/
    iowrite_and_verify_config_reg(1, (RSWITCHFWD_FWSOVPC4));
    

    return 0;



}



/**
    @brief  Set new Forwarding Table Configuration

    Set the complete Layer-2, Layer-3 and Debug Tap configuration

    @param[in]  file
    @param      arg

    @return     < 0 on error
*/
static long rswitchfwd_IOCTL_SetConfig(struct file * file, unsigned long arg)
{
    char __user * buf = (char __user *)arg;
    
    int           err = 0;
    

    err = access_ok(VERIFY_WRITE, buf , sizeof(rswitchfwd_ConfigNew));
    
    err = copy_from_user(&rswitchfwd_ConfigNew, buf, sizeof(rswitchfwd_ConfigNew));
    if (err != 0)            
    {
        
        pr_err("[RSWITCH-FWD] rswitchfwd_IOCTL_SetPortsConfig. copy_from_user returned %d for RSWITCH_FWD_SET_CONFIG \n", err);    
        return err;
    }
    if ((err = UpdateConfiguration()) < 0)
    {
        printk("Returned Err \n");
        
        return err;
    }
    return err;

    

}


/**
    @brief  IOCTLs

    @return < 0, or 0 on success
*/
static long rswitchfwd_IOCTL(struct file * file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    switch (cmd)
    {
        
    case RSWITCH_FWD_GET_CONFIG:
        return rswitchfwd_IOCTL_GetfConfig(file, arg);
    case RSWITCH_FWD_SET_CONFIG:
        return rswitchfwd_IOCTL_SetConfig(file, arg);

        
    default:
        pr_err("[RSWITCH-FWD] IOCTL Unknown or unsupported: 0x%08X\n", cmd);
        ret = -EINVAL;
    }

    return ret;
}




// ------------------------------------------------------------------------------------ 
//
//      Driver Load & Unload
//
// ------------------------------------------------------------------------------------ 


/**
    @brief  Platform Device ID Table
*/
static struct platform_device_id    rswitchfwd_ID_Table[] = 
{
    { RSWITCH_FPGA_FWD_PLATFORM,        (kernel_ulong_t)NULL },
    { }
};
MODULE_DEVICE_TABLE(platform, rswitchfwd_ID_Table);


const struct file_operations    rswitchfwd_FileOps = 
{
    .owner            = THIS_MODULE,
    .open             = NULL,
    .release          = NULL,
    .unlocked_ioctl   = rswitchfwd_IOCTL,
    .compat_ioctl     = rswitchfwd_IOCTL,
};






/**
    @brief  Add character device

    @return < 0, or 0 on success
*/
static int rswitchfwd_drv_Probe_chrdev(void)
{
    struct device * dev = NULL;
    int             ret = 0;

    /* 
        Create class 
    */
    rswitchfwd_DevClass = class_create(THIS_MODULE, RSWITCHFWD_CLASS);
    if (IS_ERR(rswitchfwd_DevClass)) 
    {
        ret = PTR_ERR(rswitchfwd_DevClass);
        pr_err("[RSWITCH-FWD] failed to create '%s' class. rc=%d\n", RSWITCHFWD_CLASS, ret);
        return ret;
    }

    if (rswitchfwd_DevMajor != 0) 
    {
        rswitchfwd_Dev = MKDEV(rswitchfwd_DevMajor, 0);
        ret = register_chrdev_region(rswitchfwd_Dev, 1, RSWITCHFWD_CLASS);
    } 
    else
    {
        ret = alloc_chrdev_region(&rswitchfwd_Dev, 0, 1, RSWITCHFWD_CLASS);
    }
    if (ret < 0) 
    {
        pr_err("[RSWITCH-FWD] failed to register '%s' character device. rc=%d\n", RSWITCHFWD_CLASS, ret);
        class_destroy(rswitchfwd_DevClass);
        return ret;
    }
    rswitchfwd_DevMajor = MAJOR(rswitchfwd_Dev);

    cdev_init(&rswitchfwd_cDev, &rswitchfwd_FileOps);
    rswitchfwd_cDev.owner = THIS_MODULE;

    ret = cdev_add(&rswitchfwd_cDev, rswitchfwd_Dev, RSWITCH_FWD_CTRL_MINOR + 1);
    if (ret < 0) 
    {
        pr_err("[RSWITCH-FWD] failed to add '%s' character device. rc=%d\n", RSWITCHFWD_CLASS, ret);
        unregister_chrdev_region(rswitchfwd_Dev, 1);
        class_destroy(rswitchfwd_DevClass);
        return ret;
    }

    /* device initialize */
    dev = &rswitchfwd_device;
    device_initialize(dev);
    dev->class  = rswitchfwd_DevClass;
    dev->devt   = MKDEV(rswitchfwd_DevMajor, RSWITCH_FWD_CTRL_MINOR);
    dev_set_name(dev, RSWITCH_FWD_DEVICE_NAME);

    ret = device_add(dev);
    if (ret < 0)
    {
        pr_err("[RSWITCH-FWD] failed to add '%s' device. rc=%d\n", RSWITCH_FWD_DEVICE_NAME, ret);
        cdev_del(&rswitchfwd_cDev);
        unregister_chrdev_region(rswitchfwd_Dev, 1);
        class_destroy(rswitchfwd_DevClass);
        return ret;
    }

    return ret;
}




/**
    @brief  Remove character device

    @return < 0, or 0 on success
*/
static int rswitchfwd_drv_Remove_chrdev(void)
{
    int     ret = 0;

    /*
        Remove character device
    */
    unregister_chrdev_region(rswitchfwd_Dev, 1);
    device_del(&rswitchfwd_device);
    cdev_del(&rswitchfwd_cDev);
    class_destroy(rswitchfwd_DevClass);
    
    return ret;
}



/**
    @brief  Platform Driver Load

    @param  pdev    The device to be instantiated
    
    @return Return code (< 0 on error)
*/
static int rswitchfwd_drv_probe(struct platform_device * pdev)
{
    int     ret = 0;
    
    
    int err  = 0;
    printk("Bitfile Version = %x \n", bitfile_version);
    if(bitfile_version < RSWITCH_BITFILE_VERSION)
    {
        printk("\n[RSWITCH-FWD] Cetitec (%s %s) version %s, Probing '%s' failed due to Old Bitfile version %x ,Please reprogram with newer FPGA Greater than or equal to %x\n",
        __DATE__, __TIME__, RSWITCHFWD_DRIVER_VERSION, pdev->name,bitfile_version, RSWITCH_BITFILE_VERSION );
        return err;
    }
    printk("\n[RSWITCH-FWD] (%s %s) version %s Probing %s .....\n",  __DATE__, __TIME__, RSWITCHFWD_DRIVER_VERSION, pdev->name);

    



    /* 
        Platform Resource Memory #0: Forwarding Engine base address from Platform Resource
    */
    
    

    /*
        Arrange for the physical address to be visible from driver.  
        Assign virtual address to I/O memory region.  
    */
    {
        unsigned int    Start = RSWITCH_FPGA_FWD_BASE;
        unsigned int    End = RSWITCH_FPGA_FWD_BASE + RSWITCH_FPGA_FWD_SIZE;
        unsigned int    Size = End - Start + 1;
        rswitchfwd_base_physical = Start;
        rswitchfwd_base_addr = ioaddr + RSWITCH_FPGA_FWD_OFFSET;
        
        printk("[RSWITCH-FWD] IO Remap Fwd.Engine Config Address(0x%08X - 0x%08X) to (0x%08lX - 0x%08lX) \n", 
        Start, End, (uintptr_t)rswitchfwd_base_addr, (uintptr_t)rswitchfwd_base_addr + Size);
        
        Start = RSWITCH_FPGA_GWCA_BASE;
        End = RSWITCH_FPGA_GWCA_BASE + RSWITCH_FPGA_GWCA_SIZE;
        Size = End - Start + 1;
        rswitchfwd_debug_physical = Start;
        rswitchfwd_debug_addr = ioaddr + RSWITCH_FPGA_GWCA_OFFSET;
        
        printk("[RSWITCH-FWD] IO Remap Fwd.Engine Debug Address(0x%08X - 0x%08X) to (0x%08lX - 0x%08lX) \n", 
        Start, End, (uintptr_t)rswitchfwd_debug_addr, (uintptr_t)rswitchfwd_debug_addr + Size);
    }


    

#ifdef CONFIG_RENESAS_RSWITCH_FWD_STATS
    rswitchfwd_proc_Initialise();
#endif /* CONFIG_RENESAS_ACE_GW_STATS */
    /*
        Add character device
    */
    if ((ret = rswitchfwd_drv_Probe_chrdev()) < 0)
    {
#ifdef CONFIG_RENESAS_RSWITCH_FWD_STATS
        rswitchfwd_proc_Terminate();
#endif /* CONFIG_RENESAS_RSWITCH_FWD_STATS */
        iounmap(rswitchfwd_base_addr);
        
        return ret;
    }

    ret = Initialise_ForwardingEngine();

    pr_info("[RSWITCH-FWD] Module loaded\n");
    printk("Value of ret is %x \n", ret);

    return ret;
}


/**
    @brief  Platform Driver Unload

    @param  pdev    The device to be removed

    @return Return code (< 0 on error)
*/
static int rswitchfwd_drv_remove(struct platform_device * pdev)
{


    rswitchfwd_drv_Remove_chrdev();

#ifdef CONFIG_RENESAS_RSWITCH_FWD_STATS
    rswitchfwd_proc_Terminate();
#endif /* CONFIG_RENESAS_RSWITCH_FWD_STATS */

    platform_set_drvdata(pdev, NULL);
    
    pr_info("[RSWITCH-FWD] Module Unloaded\n");

    return 0;
}






/**
    @brief  Platform Device
*/
static struct platform_driver   rswitchfwd_driver = 
{
    
    .remove     = rswitchfwd_drv_remove,
    .id_table   = rswitchfwd_ID_Table,
    .driver = 
    {
        .name = RSWITCH_FPGA_FWD_PLATFORM,
    },
};

struct platform_device *pdev;
/**
    @brief  Forwarding Engine Module Exit

*/
static void __exit remove_module(void)
{
    

    platform_driver_unregister(&rswitchfwd_driver);
    platform_device_unregister(pdev);
}



/**
    @brief  Forwarding Engine Init Function

    @return 0, or < 0 on error
*/
int __init init_module(void)
{
    int retval;
    
    pdev = platform_device_register_simple(
    RSWITCH_FPGA_FWD_PLATFORM, -1, NULL, 0);
    printk("Inside init module \n");
    retval = platform_driver_probe(&rswitchfwd_driver, rswitchfwd_drv_probe);
    
    return retval;
    

    
}


module_exit(remove_module);
MODULE_AUTHOR("Asad Kamal");
MODULE_DESCRIPTION("Renesas RSWITCH Forwarding Engine driver");
MODULE_LICENSE("GPL v2");

/*
    Change History
    2019-03-22 0.0.1 AK Initial version(WIP)
    2019-03-27 0.0.2 AK Added Proc stats for L2
    2019-05-08 0.0.3 AK Updated QCI for Start Time
    2019-07-11 0.0.3 AK L2 Proc Show Bug fixing on Macl boundary
    2019-07-12 0.0.4 AK Added Proc Entry for VLAN
    2019-10-11 0.1.0 AK Error Monitoring Added, New Inerrupt Map, Proc Stats
    2019-11-07 0.1.0 AK Modified bitfile version check
    2019-12-02 0.1.1 AK Single Interrupt Line
    2019-12-03 0.1.2 AK Updated Only for VLAN and L2 Update
    2019-12-09 0.1.3 AK Overwrite not counted as error
    2020-01-06 1.0.0 AK Code Cleaned, Removed warning, Release version   
    2020-01-06 1.0.1 AK Bitfile version error more  verbose  , minor bug in Watermark config 
    2020-05-05 1.0.2 AK Added Network Control Configuration for Watermark 
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


