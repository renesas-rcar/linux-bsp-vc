/*
*  Renesas VC2 PLATFORM  -  definitions
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


#ifndef __RENESAS_VC2_PLATFORM_H__
#define __RENESAS_VC2_PLATFORM_H__


#define VC2_FPGA_PLTFRM_DRIVER_VERSION      "0.0.1"
#define VC2_FPGA_PROC_DIR                         "vcfpga"
#define VC2_FPGA_PROC_FILE_BITFILE_VERSION    "bitfile"

extern void __iomem          *ioaddr;
extern void __iomem          *ioaddr_pci;
extern u32                   pci_id;
extern struct pci_dev        *pcidev;


enum vc2_bfile_op_type {
    VC2_IOCTL_BFILE_OP,
    VC2_FILEWR_BFILE_OP,

};

struct vc2_file_op {
    enum vc2_bfile_op_type bfile_op;
    u32 file_offset;
    u32 file_end;
};

#endif


/*
    Change History
    2020-08-10    AK  Initial Version
    2020-09-03    AK  Changed Platform driver name, export pci id


*/
