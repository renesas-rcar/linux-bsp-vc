/*
*  Renesas VC2 USERSPACE  -  definitions
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


#ifndef __RENESAS_VC2_USRSOACE_H__
#define __RENESAS_VC2_USSPACE_H__
/**
    @brief  Set a single 32-bit word
*/
struct vc2_fpga_pltfrm_memory {
    uint32_t        address;            ///< @brief address relative to 0x0000
    uint32_t        value;              ///< @brief Ne value
    uint32_t        raw;
    char * bitfile;
    uint32_t file_write_enable;
    uint32_t file_size;
};

/* RSWITCH2 SPECIFIC DEFINES START*/
#define RSWITCH2_FPGA_IP_BASE               0x00000000
#define RSWITCH2_FPGA_IP_SIZE               0x00030000
#define RSWITCH2_FPGA_PCI_OFFSET            0x01040000
/* RSWITCH2 SPECIFIC DEFINES END*/




#define VC2_FPGA_PLTFRM_IOCTL_MAGIC         'F'
#define VC2_FPGA_PLTFRM_IOCTL_START         0

#define DRV_NAME	                     "vcfpga"


#define VC2_FPGA_PLTFRM_CLASS                "fpgaupdate"


#define VC2_FPGA_PLTFRM_DEVICE_NAME          "fpgaupdate"


#define VC2_FPGA_PLTFRM_IOCTL_READMEMORY    _IOWR(VC2_FPGA_PLTFRM_IOCTL_MAGIC, VC2_FPGA_PLTFRM_IOCTL_START + 51, struct vc2_fpga_pltfrm_memory *)
#define VC2_FPGA_PLTFRM_IOCTL_WRITEMEMORY   _IOW(VC2_FPGA_PLTFRM_IOCTL_MAGIC, VC2_FPGA_PLTFRM_IOCTL_START + 52, struct vc2_fpga_pltfrm_memory *)

#define VC2_FPGA_PLTFRM_IOCTL_WRITEBITFILE  _IOW(VC2_FPGA_PLTFRM_IOCTL_MAGIC, VC2_FPGA_PLTFRM_IOCTL_START + 53, struct vc2_fpga_pltfrm_memory *)

#endif


/*
    Change History
    2020-08-10    AK  Initial Version
    2020-08-10    AK  Raw Read support

*/
