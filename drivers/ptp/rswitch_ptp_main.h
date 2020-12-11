/**
    @brief  PTP 1588 clock for Renesas R Switch header

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

#ifndef __RSWITCH_PTP_MAIN_H__
#define __RSWITCH_PTP_MAIN_H__

#include <linux/ptp_clock_kernel.h>


#define RSWITCHPTP_DRIVER_VERSION          "1.0.1"





#ifndef NOT
#define  NOT !
#endif

#ifndef AND
#define AND &&
#endif

#ifndef OR
#define OR ||
#endif
#define TIMER0 0
#define TIMER1 1
#define TIMER_ACTIVE TIMER0
/**
    @brief  RSWITCH gPTP registers
*/
enum ptp_rswitch_regoffset 
{
    GWCPU_GPTP_BASE   = 0x0000 + (TIMER_ACTIVE * 20),
    GWCPU_GPTP_TME    = 0x0000 ,             ///< @brief gPTP Configuration & Control
    GWCPU_GPTP_TMD    = 0x0004 ,             ///< @brief gPTP Configuration & Control
    GWCPU_GPTP_GPTI   = 0x0010 + (TIMER_ACTIVE * 20),             ///< @brief gPTP Timer Increment
    
    GWCPU_GPTP_GPTO0  = 0X0014 + (TIMER_ACTIVE * 20),             ///< @brief gPTP Timer Offset (0)
    GWCPU_GPTP_GPTO1  = 0X0018 + (TIMER_ACTIVE * 20),              ///< @brief gPTP Timer Offset (1)
    GWCPU_GPTP_GPTO2  = 0X001C + (TIMER_ACTIVE * 20),             ///< @brief gPTP Timer Offset (2)
    GWCPU_GPTP_GTM0    = 0x0030 + (TIMER_ACTIVE * 20),


    GWCPU_GPTP_GPCT0  = 0X0024 + (TIMER_ACTIVE * 20),             ///< @brief gPTP Capture Time (0)
    GWCPU_GPTP_GPCT1  = 0X0028 + (TIMER_ACTIVE * 20),             ///< @brief gPTP Capture Time (1)
    GWCPU_GPTP_GPCT2  = 0X002C + (TIMER_ACTIVE * 20),             ///< @brief gPTP Capture Time (2)
    GWCPU_GPTP_TCD0   = 0X0210 + (TIMER_ACTIVE * 20), 
    GWCPU_GPTP_TCM0   = 0X0214 + (TIMER_ACTIVE * 20),
    GWCPU_GPTP_TCST0   = 0X022C ,
    

};



/* 
    Bit definitions for the GPTI register 
*/
#define TIV_MASK                (0xFFFFFFFF)
#define TIV_MASK_OLD            (0x0FFFFFFF)
#define GPTP_CLK_DEV            1000001
#define RSWITCH_PTP_BITFILE_NEW  0x20021001











#endif /* __RSWITCH_PTP_MAIN_H__ */

/*
* Local variables:
* Mode: C
* tab-width: 4
* indent-tabs-mode: nil
* c-basic-offset: 4
* End:
* vim: ts=4 expandtab sw=4
*/
