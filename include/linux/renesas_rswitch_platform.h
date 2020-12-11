/*
*  Renesas R-SWITCH Core Engine (RSWITCH)
*
*  Copyright (C) 2015 Renesas Electronics Corporation
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


#ifndef __RENESAS_RSWITCH_PLATFORM_H__
#define __RENESAS_RSWITCH_PLATFORM_H__
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>

//    ---- RSWITCH- Platform Devices ----

#define RSWITCH_FPGA_ETH_PLATFORM           "rswitch-tsn"       /* TSN Ethernet Ports */

#define RSWITCH_FPGA_DEBUG_PLATFORM         "rswitch-debug"     /* FPGA debug interface */

#define RSWITCH_FPGA_GPTP_PLATFORM          "rswitch-ptp"
#define RSWITCH_FPGA_FWD_PLATFORM           "rswitch-fwd"





//
// rFabric & Communications agents, I/O buffers
//

#define RSWITCH_FPGA_GATEWAY_BASE          0x00000000
#define RSWITCH_FPGA_GATEWAY_SIZE          0x10000000




// Communication Agent: TSN Ethernet Ports (IRQ 17 to 26 reserved)
#define RSWITCH_FPGA_ETH_OFFSET            0x18000
#define RSWITCH_FPGA_ETH_BASE              (RSWITCH_FPGA_GATEWAY_BASE + RSWITCH_FPGA_ETH_OFFSET)
#define RSWITCH_FPGA_ETH_PORT_SIZE         0x4000
#define RSWITCH_FPGA_GWCA_OFFSET           0x14000
#define RSWITCH_FPGA_GWCA_BASE             (RSWITCH_FPGA_GATEWAY_BASE + RSWITCH_FPGA_GWCA_OFFSET) 
#define RSWITCH_FPGA_GWCA_SIZE             0x4000
#define RSWITCH_FPGA_FWD_OFFSET            0x0
#define RSWITCH_FPGA_FWD_BASE              (RSWITCH_FPGA_GATEWAY_BASE + RSWITCH_FPGA_FWD_OFFSET)
#define RSWITCH_FPGA_FWD_SIZE              0x10000




// gPTP
#define RSWITCH_FPGA_GPTP_OFFSET           RSWITCH_FPGA_GWCA_BASE + 0x0800 
#define RSWITCH_FPGA_GPTP_BASE             (RSWITCH_FPGA_GATEWAY_BASE + RSWITCH_FPGA_GPTP_OFFSET)
#define RSWITCH_FPGA_GPTP_SIZE             0x0500




// Base: Frame Buffers & descriptors
#define RSWITCH_FPGA_RAM_AREA_START        0x40000000
#define RSWITCH_FPGA_RAM_AREA_SIZE         0x01000000





extern void __iomem  *ioaddr ;
extern uint32_t bitfile_version;
extern struct pci_dev *pcidev;
extern struct proc_dir_entry   *root_debug_dir;
extern int rswitchfwd_error(void);

#endif /* __RENESAS_RSWITCH_PLATFORM_H__ */


/*
    Change History
    2018-05-22      AK  Initial Version
    2019-10-11      AK  Updated more exports
    2020-01-06      AK  Release Version

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

