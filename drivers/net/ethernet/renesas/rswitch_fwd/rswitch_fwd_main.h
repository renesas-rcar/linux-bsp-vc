/*
*  Renesas RSWITCH Forwarding Engine (FWDE) Device Driver
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


#ifndef __RSWITCH_FWD_MAIN_H__
#define __RSWITCH_FWD_MAIN_H__

#ifndef NOT
#define  NOT !
#endif

#ifndef AND
#define AND &&
#endif

#ifndef OR
#define OR ||
#endif


#ifndef UINT32_MAX
#define UINT32_MAX                (0xFFFFFFFF)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX                (0xFFFF)
#endif


// ----------------------------------------- [ Build Features ] -----------------------------------------




#ifdef CONFIG_RENESAS_RSWITCH_FWD_STATS 
/**
    @brief  Get extended (debug) I/O counts for streams

    This can only be used when the FPGA supports this feature
*/
#define RSWITCHFWD_FPGA_STATISTICS_COUNTERS   /** @brief FPGA statistic I/O counters */
#endif 


/*
    Platform device             rswitchfwd
    IOCTL device                rswitchfwd_ctrl
*/

#define RSWITCHFWD_CLASS                "rswitchfwd"


#define RSWITCHFWD_DRIVER_VERSION       "1.0.2"


//#define RSWITCH_FWD_CONFIG_TIMEOUT_MS   10


/***************************************************************************
                            D E B U G
****************************************************************************/

/* Enable this to activate the module debug if you disable this then */
/* the debug output will be completely disabled and you will have to */
/* rebuild the driver in order to debug it.                          */
//define DEBUG    1


/* Note you can enable/disable the debug via the software variable debug     */
/* this lets you control if the driver works with the debug enabled          */
/* Of course this is only applicable if you have compiled with debug support */
#ifdef  DEBUG
#define debugk(fmt, args...) if (DebugPrint > 0) printk(fmt, ##args)
#define DEBUG_OPTARG(arg) arg,
#else
#define debugk(fmt,args...)
#define DEBUG_OPTARG(arg)
#endif



/**
    @brief  RSWITCH  Forwarding Engine Registers
*/
enum RSWITCHFWD_SFR
{

    /* Configuration Registers */
    RSWITCHFWD_FWCEPC          = 0x0000,        ///< @brief Routing Configuration Register 0
    RSWITCHFWD_FWCLPC          = 0x0004,        ///< @brief Routing Configuration Register 1
    RSWITCHFWD_FWMPC0          = 0x0010,        ///< @brief Routing Configuration Register 2
    RSWITCHFWD_FWMPC1          = 0x0014,        ///< @brief Routing Configuration Register 3
    RSWITCHFWD_FWMPC2          = 0x0018,        ///< @brief Routing Configuration Register 4
    RSWITCHFWD_FWMPC3          = 0x001C,        ///< @brief Routing Configuration Register 7
    RSWITCHFWD_FWMPC4          = 0x0020,        ///< @brief Routing Configuration Register 8
    RSWITCHFWD_FWASPLC         = 0x0030,        ///< @brief Routing Configuration Register 12
    RSWITCHFWD_FWSTPDBC        = 0x0034,        ///< @brief Routing Configuration Register 13
    RSWITCHFWD_FWSTPBC         = 0x0038,        ///< @brief Routing Configuration Register 14
    RSWITCHFWD_FWSTPLEC        = 0x003C,        ///< @brief Routing Configuration Register 15
    RSWITCHFWD_FWSTPFC         = 0x0040,        ///< @brief Hash Function Configuration Register 0
    RSWITCHFWD_FWSTPFLC        = 0x0044,        ///< @brief Hash Function Configuration Register 1
    RSWITCHFWD_FWMC            = 0x0048,        ///< @brief Hash Function Configuration Register 2
    RSWITCHFWD_FWAC            = 0x004C,        ///< @brief Hash Function Configuration Register 3
    RSWITCHFWD_FWPVPC          = 0x0050,        ///< @brief Hash Function Configuration Register 4
    RSWITCHFWD_FWPVIC          = 0x0054,        ///< @brief Hash Function Configuration Register 5
    RSWITCHFWD_FWPVCC0         = 0x0060,        ///< @brief Hash Function Configuration Register 8
    RSWITCHFWD_FWSOVPC0        = 0x00A0,        ///< @brief Hash Function Configuration Register 9
    RSWITCHFWD_FWSOVPC1        = 0x00A4,        ///< @brief Hash Function Configuration Register 9
    RSWITCHFWD_FWSOVPC2        = 0x00A8,        ///< @brief Hash Function Configuration Register 9
    RSWITCHFWD_FWSOVPC3        = 0x00AC,        ///< @brief Hash Function Configuration Register 9
    RSWITCHFWD_FWSOVPC4        = 0x00B0,        ///< @brief Hash Function Configuration Register 9
    RSWITCHFWD_FWMDANFV0       = 0x00C0,        ///< @brief Hash Function Configuration Register 10
    RSWITCHFWD_FWMDANFCSD0     = 0x0140,        ///< @brief Hash Function Configuration Register 11
    RSWITCHFWD_FWSPBFE         = 0x01C0,        ///< @brief Hash Function Configuration Register 12
    RSWITCHFWD_FWSPBFC0        = 0x01C4,        ///< @brief Hash Function Configuration Register 13
    RSWITCHFWD_FWSPBFC1        = 0x01C8,        ///< @brief Hash Function Configuration Register 14
    RSWITCHFWD_FWSPBFC20       = 0x0210,        ///< @brief Hash Function Configuration Register 15
    RSWITCHFWD_FWSPBFC30       = 0x0290,        ///< @brief Hash Function Configuration Register 16
    RSWITCHFWD_FWMAFC0         = 0x0310,        ///< @brief Hash Function Configuration Register 17
    RSWITCHFWD_FWMAFC1         = 0x0318,        ///< @brief Hash Function Configuration Register 18
    RSWITCHFWD_FWMDFC00        = 0x0320,        ///< @brief Hash Function Configuration Register 19
    RSWITCHFWD_FWMDFC01        = 0x0324,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWMDFC02        = 0x0328,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWMDFC03        = 0x032C,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWMDFC04        = 0x0330,        ///< @brief Hash Function Configuration Register 19
    RSWITCHFWD_FWMDFC05        = 0x0334,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWMDFC10        = 0x0340,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWMDFC11        = 0x0344,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWMDFC12        = 0x0348,        ///< @brief Hash Function Configuration Register 19
    RSWITCHFWD_FWMDFC13        = 0x034C,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWMDFC14        = 0x0350,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWMDFC15        = 0x0354,        ///< @brief Hash Function Configuration Register 20
    RSWITCHFWD_FWBFC0          = 0x0360,        ///< @brief Hash Function Configuration Register 21
    RSWITCHFWD_FWBFC1          = 0x0364,        ///< @brief Hash Function Configuration Register 21
    RSWITCHFWD_FWBFC2          = 0x0368,        ///< @brief Hash Function Configuration Register 21
    RSWITCHFWD_FWBFC3          = 0x036C,        ///< @brief Hash Function Configuration Register 21
    RSWITCHFWD_FWBFC4          = 0x0370,        ///< @brief Hash Function Configuration Register 21
    RSWITCHFWD_FWBFC5          = 0x0374,        ///< @brief Hash Function Configuration Register 21
    RSWITCHFWD_FWPVC0          = 0x0380,        ///< @brief Exceptional Handling Counter Register 0
    RSWITCHFWD_FWPVC1          = 0x0384,        ///< @brief Exceptional Handling Counter Register 0
    RSWITCHFWD_FWPVC2          = 0x0388,        ///< @brief Exceptional Handling Counter Register 0
    RSWITCHFWD_FWPVC3          = 0x038C,        ///< @brief Exceptional Handling Counter Register 0
    RSWITCHFWD_FWPVC4          = 0x0390,        ///< @brief Exceptional Handling Counter Register 0
    RSWITCHFWD_FWDTTC          = 0x03A0,        ///< @brief Exceptional Handling Counter Register 2
    RSWITCHFWD_FWAFCE          = 0x03A4,        ///< @brief Exceptional Handling Counter Register 3
    RSWITCHFWD_FWQMEC0         = 0x0480,        ///< @brief Exceptional Handling Counter Register 4
    RSWITCHFWD_FWQMMC0         = 0x0500,        ///< @brief Exceptional Handling Counter Register 5
    RSWITCHFWD_FWQRFDC0        = 0x0580,        ///< @brief Exceptional Handling Flagging Register 0
    RSWITCHFWD_FWQYFDC0        = 0x0600,        ///< @brief Exceptional Handling Flagging Register 1
    RSWITCHFWD_FWQVTCMC0       = 0x0680,        ///< @brief Exceptional Handling Flagging Register 2
    RSWITCHFWD_FWQMCBSC0       = 0x0900,        ///< @brief Exceptional Handling Flagging Register 3
    RSWITCHFWD_FWQMCIRC0       = 0x0D00,        ///< @brief Exceptional Handling Flagging Register 4
    RSWITCHFWD_FWQMEBSC0       = 0x1100,        ///< @brief Exceptional Handling Flagging Register 5
    RSWITCHFWD_FWQMEIRC0       = 0x1500,        ///< @brief Exceptional Handling Interrupt Enable Register 0
    RSWITCHFWD_FWQMCFC0        = 0x1900,        ///< @brief Exceptional Handling Interrupt Disable Register 0
    RSWITCHFWD_FWQIUEC0        = 0x1980,        ///< @brief Exceptional Handling Interrupt Enable Register 1
    RSWITCHFWD_FWQGMC0         = 0x1A00,        ///< @brief Exceptional Handling Interrupt Disable Register 1
    RSWITCHFWD_BPWMC10         = 0x1A80,        ///< @brief Exceptional Handling Interrupt Enable Register 2
    RSWITCHFWD_BPWMC2          = 0x1B00,        ///< @brief Exceptional Handling Interrupt Disable Register 2
    RSWITCHFWD_BPWMC3          = 0x1B04,        ///< @brief Exceptional Handling Interrupt Enable Register 3
    RSWITCHFWD_BPWML           = 0x1B08,        ///< @brief Exceptional Handling Interrupt Disable Register 3
    RSWITCHFWD_BPPFL           = 0x1B0C,        ///< @brief Exceptional Handling Interrupt Enable Register 4
    RSWITCHFWD_FWATC           = 0x1B10,        ///< @brief Exceptional Handling Interrupt Disable Register 4
    RSWITCHFWD_FWQTL0          = 0x1B20,        ///< @brief Exceptional Handling Interrupt Enable Register 5
    RSWITCHFWD_FWQTL1          = 0x1B24,        ///< @brief Exceptional Handling Interrupt Enable Register 5
    RSWITCHFWD_FWQTL2          = 0x1B28,        ///< @brief Exceptional Handling Interrupt Enable Register 5
    RSWITCHFWD_FWQTL3          = 0x1B2C,        ///< @brief Exceptional Handling Interrupt Enable Register 5
    RSWITCHFWD_FWQTLR          = 0x1B30,        ///< @brief Exceptional Handling Interrupt Disable Register 5

    RSWITCHFWD_FWMTL0          = 0x1B40,        /// < @brief VLAN table register 0, 2048 such registers
    RSWITCHFWD_FWMTL1          = 0x1B44,
    RSWITCHFWD_FWMTL2          = 0x1B48,
    RSWITCHFWD_FWMTL3          = 0x1B4C,
    
    RSWITCHFWD_FWMTLR          = 0x1B50,        /// < @brief ARL table entry 0 register 0, 4096 such registers
    RSWITCHFWD_FWVTL0          = 0x1B60,        /// < @brief ARL table entry 0 register 1, 4096 such registers
    RSWITCHFWD_FWVTL1          = 0x1B64,
    RSWITCHFWD_FWVTL2          = 0x1B68,
    RSWITCHFWD_FWVTL3          = 0x1B6C,

    RSWITCHFWD_FWVTLR          = 0x1B70,        /// < @brief ARL table entry 0 register 2, 4096 such registers
    RSWITCHFWD_FWSTL0          = 0x1B80,        /// < @brief ARL table entry 0 register 3, 4096 such registers
    RSWITCHFWD_FWSTL1          = 0x1B84,
    RSWITCHFWD_FWSTL2          = 0x1B88,
    RSWITCHFWD_FWSTL3          = 0x1B8C,
    RSWITCHFWD_FWSTLR          = 0x1B90,
    RSWITCHFWD_HFSR            = 0x1BA0,
    RSWITCHFWD_MHC0            = 0x1BA4,
    RSWITCHFWD_MHC1            = 0x1BA8,
    RSWITCHFWD_VHC             = 0x1BAC,
    RSWITCHFWD_SHC0            = 0x1BB0,
    RSWITCHFWD_SHC1            = 0x1BB4,
    RSWITCHFWD_QHC0            = 0x1BB8,
    RSWITCHFWD_QHC1            = 0x1BBC,
    RSWITCHFWD_QHC2            = 0x1BC0,
    RSWITCHFWD_FWGGTS0         = 0x2000,
    RSWITCHFWD_FWGACST00       = 0x2080,
    RSWITCHFWD_FWGACST10       = 0x2100,
    RSWITCHFWD_FWGACST20       = 0x2180,
    RSWITCHFWD_FWGALIT00       = 0x2200,
    RSWITCHFWD_FWGALIT10       = 0x2280,
    RSWITCHFWD_FWGALIT20       = 0x2300,
    RSWITCHFWD_FWGAEN00        = 0x2380,
    RSWITCHFWD_FWGAEN10        = 0x2400,
    RSWITCHFWD_FWGIGS0         = 0x2480,
    RSWITCHFWD_FWGJC0          = 0x2500,
    RSWITCHFWD_FWGTL0          = 0x2580,
    RSWITCHFWD_FWGSE0          = 0x2600,
    RSWITCHFWD_FWGCC0          = 0x2680,
    RSWITCHFWD_FWGATL0         = 0x2700,
    RSWITCHFWD_FWGATL1         = 0x2704,
    RSWITCHFWD_FWGATL2         = 0x2708,

    
    RSWITCHFWD_FWMFV0          = 0x2800,
    RSWITCHFWD_FWQMFMC0        = 0x2C00,
    RSWITCHFWD_FWARA0          = 0x3000,
    RSWITCHFWD_FWARA1          = 0x3004,
    RSWITCHFWD_FWTEN0          = 0x3010,
    RSWITCHFWD_FWTEN1          = 0x3014,
    RSWITCHFWD_FWQTS0          = 0x3020,
    RSWITCHFWD_FWQTS1          = 0x3024,
    RSWITCHFWD_FWQTS2          = 0x3028,
    RSWITCHFWD_FWQTSR0         = 0x302C,
    RSWITCHFWD_FWQTSR1         = 0x3030,
    RSWITCHFWD_FWMTS0          = 0x3040,
    RSWITCHFWD_FWMTS1          = 0x3044,
    RSWITCHFWD_FWMTSR0         = 0x3048,
    RSWITCHFWD_FWMTSR1         = 0x304C,
    RSWITCHFWD_FWMTSR2         = 0x3050,
    RSWITCHFWD_FWVTS           = 0x3060,
    RSWITCHFWD_FWVTSR0         = 0x3064,
    RSWITCHFWD_FWVTSR1         = 0x3068,
    RSWITCHFWD_FWVTSR2         = 0x306C,
    RSWITCHFWD_FWVTSR3         = 0x3070,
    RSWITCHFWD_FWSTS0          = 0x3080,
    RSWITCHFWD_FWSTS1          = 0x3084,
    RSWITCHFWD_FWSTSR0         = 0x3088,
    RSWITCHFWD_FWSTSR1         = 0x308C,
    RSWITCHFWD_FWSTSR2         = 0x3090,
    RSWITCHFWD_FWMSAR          = 0x30A0,

    RSWITCHFWD_FWDRFDP         = 0x30A4,
    RSWITCHFWD_FWEFS           = 0x30A8,
    RSWITCHFWD_FWRR            = 0x30AC,
    RSWITCHFWD_FWCEP           = 0x3100,
    RSWITCHFWD_FWCMP           = 0x3104,
    RSWITCHFWD_FWCRP           = 0x3108,
    RSWITCHFWD_FWBPC           = 0x310C,

    RSWITCHFWD_FWMPC           = 0x3110,
    RSWITCHFWD_FWRDC            = 0x3114,
    RSWITCHFWD_FWBPPC          = 0x3118,
    RSWITCHFWD_FWBPLC          = 0x311C,
    RSWITCHFWD_FWQMSRPC0       = 0x3200,
    RSWITCHFWD_FWQGRPC0        = 0x3600,
    RSWITCHFWD_FWQMDPC0        = 0x3A00,
    RSWITCHFWD_FWQMGPC0        = 0x3E00,
    RSWITCHFWD_FWQMYPC0        = 0x4200,
    RSWITCHFWD_FWQMRPC0        = 0x4600,
    RSWITCHFWD_FWGOCST00       = 0x4A00,
    RSWITCHFWD_FWGOCST10       = 0x4A80,

    RSWITCHFWD_FWGOCST20       = 0x4B00,
    RSWITCHFWD_FWGOLIT00       = 0x4B80,
    RSWITCHFWD_FWGOLIT10       = 0x4C00,
    RSWITCHFWD_FWGOLIT20       = 0x4C80,
    RSWITCHFWD_FWGOEN00        = 0x4D00,
    RSWITCHFWD_FWGOEN10        = 0x4D80,
    RSWITCHFWD_FWGTR0          = 0x4E00,
    RSWITCHFWD_FWGTR1          = 0x4E04,
    RSWITCHFWD_FWGTR2          = 0x4E08,


    RSWITCHFWD_FWGFSMS         = 0x4E10,
    RSWITCHFWD_FWEIS0          = 0x5000,
    RSWITCHFWD_FWEIE0          = 0x5004,
    RSWITCHFWD_FWEID0          = 0x5008,
    RSWITCHFWD_FWEIS1          = 0x5010,
    RSWITCHFWD_FWEIE1          = 0x5014,
    RSWITCHFWD_FWEID1          = 0x5018,
    RSWITCHFWD_FWEIS20         = 0x5080,
    RSWITCHFWD_FWEIE20         = 0x5100,
    RSWITCHFWD_FWEID20         = 0x5180,
    RSWITCHFWD_FWEIS3          = 0x5200,
    RSWITCHFWD_FWEIE3          = 0x5204,
    RSWITCHFWD_FWEID3          = 0x5208,
    RSWITCHFWD_FWEIS4          = 0x5210,
    RSWITCHFWD_FWEIE4          = 0x5214,
    RSWITCHFWD_FWEID4          = 0x5218,
    RSWITCHFWD_FWEIS50         = 0x5280,
    RSWITCHFWD_FWEIE50         = 0x5300,

    RSWITCHFWD_FWEID50         = 0x5380,
    RSWITCHFWD_FWEIS60         = 0x5400,
    RSWITCHFWD_FWEIE60         = 0x5480,
    RSWITCHFWD_FWEID60         = 0x5500,
    RSWITCHFWD_FWMIS0          = 0x5580,
    RSWITCHFWD_FWMIE0          = 0x5584,
    RSWITCHFWD_FWMID0          = 0x5588,
    RSWITCHFWD_FWMIS1          = 0x5590,
    RSWITCHFWD_FWMIE1          = 0x5594,
    RSWITCHFWD_FWMID1          = 0x5598,
    RSWITCHFWD_FWMIS2          = 0x55A0,
    RSWITCHFWD_FWMIE2          = 0x55A4,
    RSWITCHFWD_FWMID2          = 0x55A8,
    RSWITCHFWD_FWQTR           = 0x5600,
    RSWITCHFWD_FWQTRR0         = 0x5604,
    RSWITCHFWD_FWQTRR1         = 0x5608,
    RSWITCHFWD_FWQTRR2         = 0x560C,
    RSWITCHFWD_FWQTRR3         = 0x5610,

    RSWITCHFWD_FWMTR           = 0x5620,
    RSWITCHFWD_FWMTRR0         = 0x5624,
    RSWITCHFWD_FWMTRR1         = 0x5628,
    RSWITCHFWD_FWMTRR2         = 0x562C,
    RSWITCHFWD_FWVTR           = 0x5630,
    RSWITCHFWD_FWVTRR          = 0x5634,
    RSWITCHFWD_FWSTR           = 0x5640,
    
    RSWITCHFWD_FWSTRR0         = 0x5644,
    RSWITCHFWD_FWSTRR1         = 0x5648,
    RSWITCHFWD_FWSTRR2         = 0x564C,
    RSWITCHFWD_FWFBTR          = 0x5650,
    RSWITCHFWD_FWFBTRR         = 0x5654,


};



#endif /* __RSWITCH_FWD_MAIN_H__ */


/*
    Change History
    2018-05-22      AK  Initial test version
    2020-01-06      AK  Release version
    
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


