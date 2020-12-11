/*
*  Renesas R-Switch Ethernet (RSWITCH) Device Driver
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


#ifndef __RSWITCHTSN_MAIN_H__
#define __RSWITCHTSN_MAIN_H__



#include <linux/netdevice.h>
#include <linux/phy.h>
#include"../../../../../include/linux/renesas_rswitch.h"


#include <../drivers/ptp/rswitch_ptp.h>
#ifndef NOT
#define  NOT !
#endif

#ifndef AND
#define AND &&
#endif

#ifndef OR
#define OR ||
#endif




#define RSWITCHETH_CLASS                 "rswitchtsn"
#define RSWITCHETH_BASE_AVB_PORTNAME     "tsn"


#define RSWITCH_PORT_CONFIG_TIMEOUT_MS     100     /** @brief Port mode change Timeout (ms) */

#define RSWITCHETH_DRIVER_VERSION          "1.1.0"

#define TX_TIMEOUT                      (5 * HZ)


#define RSWITCH_RGMII_MDIO_ADDRESS         0x05

#define DEFAULT_EMAC_VC                 0x03
#define DEFAULT_PMAC_VC                 0x0
#define RSWITCH_TSN_CONFIG_TIMEOUT_MS   10 
#define RSWITCH_MIR_RGMII                0x01130007
#define RSWITCH_MIR_MII                  0x01130004
#define RSWITCH_MIR_RMII                 0x01130005
#define RENESAS_RSWITCH_PMAC_BIT         4
#define RENESAS_RSWITCH_MAX_FRAME_SIZE_OFFSET 0
#define RENESAS_RSWITCH_FRAME_DISCARD_ENABLE  16
#define RENESAS_RSWITCH_CBS_NUM_OFFSET        0
#define RSWITCH_OPERATION_MASK               1
#define DEFINE_8021Q                         0x8100
#define RSWITCH_BE 0
#define RSWITCH_NC 1


#define RSWITCH_QUEUE_NC                  1
#define SLOW_PROT_EGRESS_PRIORITY         3
#define SLOW_PROT_EGRESS_LAT_PRIORITY     2
#define RSWITCH_DESCLEARN_CONFIG_TIMEOUT_MS 100
/* Used 32 RX & TX Descriptor Chains*/
#define RX_DBAT_ENTRY_NUM   32
#define TX_DBAT_ENTRY_NUM   1024
#define TS_DBAT_ENTRY_NUM   2

#define NUM_RX_QUEUE    3
#define NUM_TX_QUEUE    2
#define TS_RING_SIZE    1024
#define TX_RING_SIZE0   1024    /* TX ring size for Best Effort */
#define TX_RING_SIZE1   1024    /* TX ring size for Network Control */
#define RX_RING_SIZE0   1024    /* RX ring size for Best Effort */
#define RX_RING_SIZE1   1024    /* RX ring size for Network Control */
#define RX_RING_SIZE2   1024
#define DPTR_ALIGN  4
#define RENESAS_RSWITCH_RX_GW_QUEUE 2
/* Use 2 Tx Descriptor per frame */
#define NUM_TX_DESC     2

#define RSWITCH_ALIGN   128


#define RSWITCH_RXTSTAMP_TYPE_V2_L2_EVENT  0x00000002
#define RSWITCH_TSN_TXTSTAMP_ENABLED       1
#define RSWITCH_TSN_RXTSTAMP_ENABLED       0x10
#define RSWITCH_TSN_RXTSTAMP_TYPE          0x00000006      /* RX type mask */
#define RSWITCH_RXTSTAMP_TYPE_ALL          0x06
#define RSWITCH_TIMESTAMP_CAPTURED_MASK    0x10000
#define RSWITCH_TSN_TXTSTAMP_ENABLED       1
#define RSWITCH_TSN_RXTSTAMP_ENABLED       0x10
#define RSWITCH_TSN_RXTSTAMP_TYPE          0x00000006      /* RX type mask */
#define RSWITCH_RXTSTAMP_TYPE_ALL          0x06
#define SLSW_SLOWPROT_NO_TS               0x00
#define SLSW_SLOWPROT_NO_TS_CAPTURE       0x00
#define SLSW_SLOWPROT_TS_CAPTURE          0x01
#define SLSW_SLOWPROT_TS_INSERT           0x02
#define SLSW_SLOWPROT_TS_ONESTEP          0x03
#define SLSW_SLOWPROT_NCFS_BIT_SHIFT      30
#define SLSW_SLOWPROT_TSUN_BIT_SHIFT      8
#define SLSW_SLOWPROT_TPN_BIT_SHIFT       4
#define SLSW_SLOWPROT_TPP_BIT_SHIFT       1
#define SLSW_SLOWPROT_DATA_REQUEST        1
#define SLSW_TS_PORT_ENABLE               1
#define SLSW_SLOWPROT_IDL_BIT_SHIFT       16
#define SLSW_SLOWPROT_NOFCS               0
#define SLSW_SLOWPROT_IDCF_BIT_SHIFT      27
#define SLSW_SLOWPROT_NO_TSC              0
#define SLSW_SLOWPROT_TSC                 1
#define SLSW_SLOWPROT_TSC_BIT_SHIFT       28 
#define SLSW_TS_CAPTURE_SUMMARY_MASK      0xFFFF
#define RSWITCH_SLOW_PROT_INTERRUPT_MASK_RX 1
#define RSWITCH_SLOW_PROT_INTERRUPT_MASK_TX 2
#define RSWITCH_SLOW_PROT_INTERRUPT_MASK_TS 4
#define RSWITCH_SLOW_PROT_RX_INT_ENABLE     1
#define RSWITCH_SLOW_PROT_TX_INT_ENABLE     2
#define RSWITCH_SLOW_PROT_TS_INT_ENABLE     (1<<16)
#define RSWITCH_ERROR_MIE                   (1<<8)
#define RSWITCH_ERROR_TOE                   (1<<9)
#define RSWITCH_ERROR_TAE                   (1<<3)
#define RSWITCH_ERROR_TSFE                  (1<<1)
#define RSWITCH_ERROR_OOBE                  (1)
#define RSWITCH_QUEUE_ERROR_ULRF            (1<<17)
#define RSWITCH_QUEUE_ERROR_FTBF            (1<<16)
#define RSWITCH_QUEUE_ERROR_QOF             (1<<15)
#define RSWITCH_I4S_EGES_BIT_SHIFT          16
#define RSWITCH_I4S_EQES_BIT_SHIFT          0
#define RSWITCH_I4STL_NTQS_SHIFT            30
#define RSWITCH_I4STL_SPQS_SHIFT            29
#define RSWITCH_I4STL_EXQS_SHIFT            28
#define RSWITCH_I4STL_EPES_SHIFT            15
#define RSWITCH_SSR_FTBE                   (1<<16)
#define RSWITCH_SSR_QOE                    (1<<15)
#define RSWITCH_SIE_FTBE0                  (1<<8)
#define RSWITCH_SIE_FTBE1                  (1<<9)
#define RSWITCH_SIE_QOE0                   (1<<0)
#define RSWITCH_SIE_QOE1                   (1<<1)
#define RSWITCH_SIE_QOE2                   (1<<2)
#define RSWITCH_GATE_TIME_MUL_SHIFT        24
#define RSWITCH_TAS_TEPS                   0x600
#define RSWITCH_TAS_GATE_STATE_CONTINUE    0
#define RSWITCH_TAS_GATE_STATE_END         2
#define RSWITCH_TSRA_GIEF_BIT_SHIFT        30
#define RSWITCH_TSRA_CSPF_BIT_SHIFT        31
#define GTFCR_TFEL              0x00020000
#define GTFCR_TFER              0x00010000
#define GTFCR_TFFL_MASK         0x0000FFFF
#define MAX_FRAME_SIZE 2048
#define TRCR_TSRQ0              0x01

#define NC_QUEUE_OFFSET 1
#define RSWITCH_GWMC_DISABLE 0x01
#define RSWITCH_GWMC_CONFIG 0x02
#define RSWITCH_GWMC_OPERATION 0x03

/* The Ethernet  descriptor definitions. */
struct rswitch_desc {
    __le16 info_ds;     /* Descriptor size */
    u8 info0;       /* Content control MSBs (reserved) */
    u8 die_dt;  /* Descriptor interrupt enable and type */
    __le32 dptr;    /* Descriptor pointer */
    __le32 info1l;  /* Descriptor pointer */
    __le32 info1h;
};

struct rswitch_ts_desc {
    __le16 info_ds;     /* Descriptor size */
    u8 info0;       /* Content control MSBs (reserved) */
    u8 die_dt;  /* Descriptor interrupt enable and type */
    __le32 dptr;    /* Descriptor pointer */
    __le32 ts_nsec; 
    __le32 ts_sec; 
};

struct rswitch_ext_desc {
    __le16 info_ds;     /* Descriptor size */
    u8 info0;       /* Content control MSBs (reserved) */
    u8 die_dt;  /* Descriptor interrupt enable and type */
    __le32 dptr;    /* Descriptor pointer */
    __le32 info1l;  /* Descriptor pointer */
    __le32 info1h;
};
struct rswitch_ext_ts_desc {
    __le16 info_ds;     /* Descriptor size */
    u8 info0;       /* Content control MSBs (reserved) */
    u8 die_dt;  /* Descriptor interrupt enable and type */
    __le32 dptr;    /* Descriptor pointer */
    __le64 info1;  /* Descriptor pointer */
    __le32 ts_nsec; 
    __le32 ts_sec; 
};



enum RX_DS_CC_BIT {
    RX_DS       = 0x0fff, /* Data size */
    RX_TR       = 0x1000, /* Truncation indication */
    RX_EI       = 0x2000, /* Error indication */
    RX_PS       = 0xc000, /* Padding selection */
};
enum TX_DS_TAGL_BIT {
    TX_DS       = 0x0fff, /* Data size */
    TX_TAGL     = 0xf000, /* Frame tag LSBs */
};

struct rswitch_tstamp_skb {
    struct list_head list;
    struct sk_buff *skb;
    u8 tag;
};


enum DIE_DT {
    /* Frame data */
    DT_FSINGLE  = 0x80,
    DT_FSTART   = 0x90,
    DT_FMID     = 0xA0,
    DT_FEND     = 0xB8,
    
    /* Chain control */
    
    DT_LEMPTY   = 0xC8, // May be same as linkfix
    DT_EEMPTY       = 0xD0,
    DT_LINK     = 0xE0,
    DT_EOS      = 0xF0,
    /* HW/SW arbitration */
    DT_FEMPTY   = 0x48,
    DT_FEMPTY_IS    = 0x10,
    DT_FEMPTY_IC    = 0x20,
    DT_FEMPTY_ND    = 0x38,
    DT_FEMPTY_START = 0x50,
    DT_FEMPTY_MID   = 0x60,
    DT_FEMPTY_END   = 0x70,
};


struct              FWd_Config
{
    u32                 Start;                                  ///< @brief Port registers Base address
    u32                 Size;                                   ///< @brief Port registers Size
    u32                 End;                                    ///< @brief Port registers End

};


struct              Queue_Config
{
    u32                 Start;                                  ///< @brief Port registers Base address
    u32                 Size;                                   ///< @brief Port registers Size
    u32                 End;                                    ///< @brief Port registers End

};




struct EthPort_Config
{
    char                PortName[IFNAMSIZ];
    u32                 Virtual_IRQ;

    // Pre-defined
    u8                  PHY_ID;
    
    // platform resources
    u32                 Start;                                  ///< @brief Port registers Base address
    u32                 Size;                                   ///< @brief Port registers Size
    u32                 End;                                    ///< @brief Port registers End
    u32                 IRQ;
};


struct GWCA_Config
{
    
    u32                 Virtual_IRQ;

    
    
    // platform resources
    u32                 Start;                                  ///< @brief Port registers Base address
    u32                 Size;                                   ///< @brief Port registers Size
    u32                 End;                                    ///< @brief Port registers End
    u32                 IRQ;
};


/**
    @brief  RSWITCH FPGA Common Configuration structure
*/
struct EthPorts_Config
{

    u8                      Eth_Ports;                          ///< @brief Number of Ethernet AVB ports
    struct EthPort_Config   Eth_Port[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    struct GWCA_Config      GWCA;
};

struct        TAS_Queue_Gate_Controls
{
    u32 TimeVal;
    u32 State;


};

struct TAS_Queue_Gate_List
{
    u32            Tx_Queue; 
    u32            Gate_Control;                         
    struct        TAS_Queue_Gate_Controls Gate_Controls[RENESAS_RSWITCH_ETH_MAX_GATE_CONTROLS];       


}; 

struct EthPorts_TAS_Config_Gate_List
{
    u32            TxQueues; 
    struct TAS_Queue_Gate_List Queue_Gate_List[RENESAS_RSWITCH_TX_QUEUES];

};

/**
    @brief  R-Switch Registers
*/
enum 
{

    /* R-Switch Ethernet Agent registers */
    RSWITCH_EAMC,
    RSWITCH_EATMTC,
    RSWITCH_EATAFS,
    RSWITCH_EAPRC,
    RSWITCH_EATMS0,
    RSWITCH_EATMS1,
    RSWITCH_EATMS2,
    RSWITCH_EATMS3,
    RSWITCH_EATMS4,
    RSWITCH_EATMS5,
    RSWITCH_EATMS6,
    RSWITCH_EATMS7,
    RSWITCH_EATDQD0,
    RSWITCH_EATWMC0,
    RSWITCH_EAVTMC,
    RSWITCH_EAVTC,
    RSWITCH_TPTPC,
    RSWITCH_TTML,
    RSWITCH_TTJ,
    RSWITCH_TCC,
    RSWITCH_TCS,
    RSWITCH_TGS,
    RSWITCH_TACST0,
    RSWITCH_TACST1,
    RSWITCH_TACST2,
    RSWITCH_TALIT0,
    RSWITCH_TALIT1,
    RSWITCH_TALIT2,
    RSWITCH_TAEN0,
    RSWITCH_TAEN1,
    RSWITCH_TASFE,
    RSWITCH_TACLL0,
    RSWITCH_TACLL1,
    RSWITCH_TACLL2,
    RSWITCH_CACC,
    RSWITCH_CCS,
    RSWITCH_CAIV0,
    RSWITCH_CAIV1,
    RSWITCH_CAIV2,
    RSWITCH_CAIV3,
    RSWITCH_CAIV4,
    RSWITCH_CAIV5,
    RSWITCH_CAIV6,
    RSWITCH_CAIV7,
    RSWITCH_CAUL0,
    RSWITCH_CAUL1,
    RSWITCH_CAUL2,
    RSWITCH_CAUL3,
    RSWITCH_CAUL4,
    RSWITCH_CAUL5,
    RSWITCH_CAUL6,
    RSWITCH_CAUL7,
    RSWITCH_EAMS,
    RSWITCH_EAOTM,
    RSWITCH_EADQM0,
    RSWITCH_EADQM1,
    RSWITCH_EADQM2,
    RSWITCH_EADQM3,
    RSWITCH_EADQM4,
    RSWITCH_EADQM5,
    RSWITCH_EADQM6,
    RSWITCH_EADQM7,
    RSWITCH_TOCST0,
    RSWITCH_TOCST1,
    RSWITCH_TOCST2,
    RSWITCH_TOLIT0,
    RSWITCH_TOLIT1,
    RSWITCH_TOLIT2,
    RSWITCH_TOEN0,
    RSWITCH_TOEN1,
    RSWITCH_TOSFE,
    RSWITCH_TCLR0,
    RSWITCH_TCLR1,
    RSWITCH_TCLR2,
    RSWITCH_TSMS,
    RSWITCH_COCC,
    RSWITCH_COIV0,
    RSWITCH_COIV1,
    RSWITCH_COIV2,
    RSWITCH_COIV3,
    RSWITCH_COIV4,
    RSWITCH_COIV5,
    RSWITCH_COIV6,
    RSWITCH_COIV7,
    RSWITCH_COUL0,
    RSWITCH_COUL1,
    RSWITCH_COUL2,
    RSWITCH_COUL3,
    RSWITCH_COUL4,
    RSWITCH_COUL5,
    RSWITCH_COUL6,
    RSWITCH_COUL7,
    RSWITCH_EAEIS0,
    RSWITCH_EAEIE0,
    RSWITCH_EAEID0,
    RSWITCH_EAEIS1,
    RSWITCH_EAEIE1,
    RSWITCH_EAEID1,
    RSWITCH_EAEIS2,
    RSWITCH_EAEIE2,
    RSWITCH_EAEID2,
    RSWITCH_EAMIS,
    RSWITCH_EAMIE,
    RSWITCH_EAMID,
    RSWITCH_TCD,
    RSWITCH_EATQD,
    RSWITCH_EADRR,
    RSWITCH_EADRRR,
    


    /* R-Switch RMAC  Registers */
    RSWITCH_RMAC_MCC,
    RSWITCH_RMAC_MPSM,
    RSWITCH_RMAC_MPIC,
    RSWITCH_RMAC_MTFFC,
    RSWITCH_RMAC_MTAPC,
    RSWITCH_RMAC_MTMPC,
    RSWITCH_RMAC_MTATC0,
    RSWITCH_RMAC_MRGC,
    RSWITCH_RMAC_MRMAC0,
    RSWITCH_RMAC_MRMAC1,
    RSWITCH_RMAC_MRAFC,
    RSWITCH_RMAC_MRSCE,
    RSWITCH_RMAC_MRSCP,
    RSWITCH_RMAC_MRSCC,
    RSWITCH_RMAC_MRFSCE,
    RSWITCH_RMAC_MRFSCP,
    RSWITCH_RMAC_MTRC,
    RSWITCH_RMAC_MPFC0,
    RSWITCH_RMAC_MLVC,
    RSWITCH_RMAC_MEEEC,
    RSWITCH_RMAC_MLBC,
    RSWITCH_RMAC_MGMR,
    RSWITCH_RMAC_MMPFTCT,
    RSWITCH_RMAC_MAPFTCT,
    RSWITCH_RMAC_MPFRCT,
    RSWITCH_RMAC_MFCICT,
    RSWITCH_RMAC_MEEECT,
    RSWITCH_RMAC_MEIS,
    RSWITCH_RMAC_MEIE,
    RSWITCH_RMAC_MEID,
    RSWITCH_RMAC_MMIS0,
    RSWITCH_RMAC_MMIE0,
    RSWITCH_RMAC_MMID0,
    RSWITCH_RMAC_MXMS,

    RSWITCH_RMAC_SYS_RXBCE,
    RSWITCH_RMAC_SYS_RXBCP,
    RSWITCH_RMAC_SYS_RGFCE,
    RSWITCH_RMAC_SYS_RGFCP,
    RSWITCH_RMAC_SYS_RBFC,
    RSWITCH_RMAC_SYS_RMFC,
    RSWITCH_RMAC_SYS_RVFC,
    RSWITCH_RMAC_SYS_RPEFC,
    RSWITCH_RMAC_SYS_RNEFC,
    RSWITCH_RMAC_SYS_RFMEFC,
    RSWITCH_RMAC_SYS_RFFMEFC,
    RSWITCH_RMAC_SYS_RCFCEFC,
    RSWITCH_RMAC_SYS_RFCEFC,
    RSWITCH_RMAC_SYS_RRCFEFC,
    RSWITCH_RMAC_SYS_RRXFEFC,
    RSWITCH_RMAC_SYS_RUEFC,
    RSWITCH_RMAC_SYS_ROEFC,
    RSWITCH_RMAC_SYS_RBOEC,
    RSWITCH_RMAC_SYS_TXBCE,
    RSWITCH_RMAC_SYS_TXBCP,
    RSWITCH_RMAC_SYS_TGFCE,
    RSWITCH_RMAC_SYS_TGFCP,
    RSWITCH_RMAC_SYS_TBFC,
    RSWITCH_RMAC_SYS_TMFC,
    RSWITCH_RMAC_SYS_TVFC,
    RSWITCH_RMAC_SYS_TEFC,



    /* R-Switch Gateway CPU Registers */
    RSWITCH_GWCA_RR0,
    RSWITCH_GWCA_RIPV,
    RSWITCH_GWCA_RMIS0,
    RSWITCH_GWCA_RMIE0,
    RSWITCH_GWCA_RMID0,
    RSWITCH_GWCA_RMIS1,
    RSWITCH_GWCA_RMIE1,
    RSWITCH_GWCA_RMID1,
    RSWITCH_GWCA_REIS0,
    RSWITCH_GWCA_REIE0,
    RSWITCH_GWCA_REID0,
    RSWITCH_GWCA_REIS1,
    RSWITCH_GWCA_REIE1,
    RSWITCH_GWCA_REID1,
    RSWITCH_GWCA_RDIS,
    RSWITCH_GWCA_RDIE,
    RSWITCH_GWCA_RDID,
    RSWITCH_GWCA_RMDIOIS,
    RSWITCH_GWCA_RMDIOIE,
    RSWITCH_GWCA_RMDIOID,
    RSWITCH_GWCA_RDASK,
    RSWITCH_GWCA_RDAU,
    RSWITCH_GWCA_RDASKC,
    RSWITCH_GWCA_GWMC,
    RSWITCH_GWCA_GWRTC0,
    RSWITCH_GWCA_GWERDQD0,
    RSWITCH_GWCA_GWPRDQD0,
    RSWITCH_GWCA_GWEWMC0,
    RSWITCH_GWCA_GWPWMC0,
    RSWITCH_GWCA_GWRMSTLS,
    RSWITCH_GWCA_GWRMSTLR,
    RSWITCH_GWCA_GWRGC,
    RSWITCH_GWCA_GWMS,
    RSWITCH_GWCA_GWRR,
    RSWITCH_GWCA_GWERDQM0,
    RSWITCH_GWCA_GWPRDQM0,
    RSWITCH_GWCA_GWERDQML0,
    RSWITCH_GWCA_GWPRDQML0,
    RSWITCH_GWCA_GWRDQM,
    RSWITCH_GWCA_GWRDQML,
    RSWITCH_GWCA_GWRMSTSS,
    RSWITCH_GWCA_GWRMSTSR0,
    RSWITCH_GWCA_GWEIS0,
    RSWITCH_GWCA_GWEIE0,
    RSWITCH_GWCA_GWEID0,
    RSWITCH_GWCA_GWEIS1,
    RSWITCH_GWCA_GWEIE1,
    RSWITCH_GWCA_GWEID1,
    RSWITCH_GWCA_GWTSRR,
    RSWITCH_GWCA_GWTSRRR0,
    RSWITCH_GWCA_GWDRR,
    RSWITCH_GWCA_GWDRRR0,
    RSWITCH_AXI_AXIWC,
    RSWITCH_AXI_AXIRC,
    RSWITCH_AXI_TDPC0,
    RSWITCH_AXI_TFT,
    RSWITCH_AXI_TATLS0,
    RSWITCH_AXI_TATLS1,
    RSWITCH_AXI_TATLR,
    RSWITCH_AXI_RATLS0,
    RSWITCH_AXI_RATLS1,
    RSWITCH_AXI_RATLR,
    RSWITCH_AXI_TSA0,
    RSWITCH_AXI_TSS0,
    RSWITCH_AXI_TRCR0,
    RSWITCH_AXI_RIDAUAS0,
    RSWITCH_AXI_RR,
    RSWITCH_AXI_TATS,
    RSWITCH_AXI_TATSR0,
    RSWITCH_AXI_RATS,
    RSWITCH_AXI_RATSR0,
    RSWITCH_AXI_RIDASM0,
    RSWITCH_AXI_RIDASAM0,
    RSWITCH_AXI_RIDACAM0,
    RSWITCH_AXI_EIS0,
    RSWITCH_AXI_EIE0,
    RSWITCH_AXI_EID0,
    RSWITCH_AXI_EIS1,
    RSWITCH_AXI_EIE1,
    RSWITCH_AXI_EID1,
    RSWITCH_AXI_TCEIS0,
    RSWITCH_AXI_TCEIE0,
    RSWITCH_AXI_TCEID0,
    RSWITCH_AXI_TCEIS1,
    RSWITCH_AXI_TCEIE1,
    RSWITCH_AXI_TCEID1,
    RSWITCH_AXI_RFSEIS0,
    RSWITCH_AXI_RFSEIE0,
    RSWITCH_AXI_RFSEID0,
    RSWITCH_AXI_RFSEIS1,
    RSWITCH_AXI_RFSEIE1,
    RSWITCH_AXI_RFSEID1,
    RSWITCH_AXI_RFSEIS2,
    RSWITCH_AXI_RFSEIE2,
    RSWITCH_AXI_RFSEID2,
    RSWITCH_AXI_RFSEIS3,
    RSWITCH_AXI_RFSEIE3,
    RSWITCH_AXI_RFSEID3,
    RSWITCH_AXI_RFSEIS4,
    RSWITCH_AXI_RFSEIE4,
    RSWITCH_AXI_RFSEID4,
    RSWITCH_AXI_RFSEIS5,
    RSWITCH_AXI_RFSEIE5,
    RSWITCH_AXI_RFSEID5,
    RSWITCH_AXI_RFSEIS6,
    RSWITCH_AXI_RFSEIE6,
    RSWITCH_AXI_RFSEID6,
    RSWITCH_AXI_RFSEIS7,
    RSWITCH_AXI_RFSEIE7,
    RSWITCH_AXI_RFSEID7,
    RSWITCH_AXI_RFEIS0,
    RSWITCH_AXI_RFEIE0,
    RSWITCH_AXI_RFEID0,
    RSWITCH_AXI_RFEIS1,
    RSWITCH_AXI_RFEIE1,
    RSWITCH_AXI_RFEID1,
    RSWITCH_AXI_RFEIS2,
    RSWITCH_AXI_RFEIE2,
    RSWITCH_AXI_RFEID2,
    RSWITCH_AXI_RFEIS3,
    RSWITCH_AXI_RFEIE3,
    RSWITCH_AXI_RFEID3,
    RSWITCH_AXI_RFEIS4,
    RSWITCH_AXI_RFEIE4,
    RSWITCH_AXI_RFEID4,
    RSWITCH_AXI_RFEIS5,
    RSWITCH_AXI_RFEIE5,
    RSWITCH_AXI_RFEID5,
    RSWITCH_AXI_RFEIS6,
    RSWITCH_AXI_RFEIE6,
    RSWITCH_AXI_RFEID6,
    RSWITCH_AXI_RFEIS7,
    RSWITCH_AXI_RFEIE7,
    RSWITCH_AXI_RFEID7,

    RSWITCH_AXI_RCEIS0,
    RSWITCH_AXI_RCEIE0,
    RSWITCH_AXI_RCEID0,
    RSWITCH_AXI_RCEIS1,
    RSWITCH_AXI_RCEIE1,
    RSWITCH_AXI_RCEID1,
    RSWITCH_AXI_RCEIS2,
    RSWITCH_AXI_RCEIE2,
    RSWITCH_AXI_RCEID2,
    RSWITCH_AXI_RCEIS3,
    RSWITCH_AXI_RCEIE3,
    RSWITCH_AXI_RCEID3,
    RSWITCH_AXI_RCEIS4,
    RSWITCH_AXI_RCEIE4,
    RSWITCH_AXI_RCEID4,
    RSWITCH_AXI_RCEIS5,
    RSWITCH_AXI_RCEIE5,
    RSWITCH_AXI_RCEID5,
    RSWITCH_AXI_RCEIS6,
    RSWITCH_AXI_RCEIE6,
    RSWITCH_AXI_RCEID6,
    RSWITCH_AXI_RCEIS7,
    RSWITCH_AXI_RCEIE7,
    RSWITCH_AXI_RCEID7,
    RSWITCH_AXI_RIDAOIS,
    RSWITCH_AXI_RIDAOIE,
    RSWITCH_AXI_RIDAOID,
    RSWITCH_AXI_TSFEIS,
    RSWITCH_AXI_TSFEIE,
    RSWITCH_AXI_TSFEID,
    RSWITCH_AXI_TSCEIS,
    RSWITCH_AXI_TSCEIE,
    RSWITCH_AXI_TSCEID,
    RSWITCH_AXI_DIS,
    RSWITCH_AXI_DIE,
    RSWITCH_AXI_DID,
    RSWITCH_AXI_TDIS0,
    RSWITCH_AXI_TDIE0,
    RSWITCH_AXI_TDID0,
    RSWITCH_AXI_RDIS0,
    RSWITCH_AXI_RDIE0,
    RSWITCH_AXI_RDID0,
    RSWITCH_AXI_TSDIS,
    RSWITCH_AXI_TSDIE,
    RSWITCH_AXI_TSDID,



    MAX_REGISTER_PORT_OFFSET,
};






/*
    Port Register's bits
*/

enum ETH_TSN_CCC_BIT 
{
    ETH_TSN_EAMC_OPC              = 0x00000003,
    ETH_TSN_EAMC_OPC_RESET        = 0x00000000,
    ETH_TSN_EAMC_OPC_MASK         = 0x00000003,
    ETH_TSN_EAMC_OPC_DISABLE      = 0x00000001,
    ETH_TSN_EAMC_OPC_CONFIG       = 0x00000002,
    ETH_TSN_EAMC_OPC_OPERATION    = 0x00000003,
};

enum ETH_TSN_EAMS_BIT 
{
    ETH_TSN_EAMS_OPS             = 0x0000000F,
    ETH_TSN_EAMS_OPS_MASK        = 0x0000000F,
    ETH_TSN_EAMS_OPS_RESET       = 0x00000000,
    ETH_TSN_EAMS_OPS_DISABLE     = 0x00000001,
    ETH_TSN_EAMS_OPS_CONFIG      = 0x00000002,
    ETH_TSN_EAMS_OPS_OPERATION   = 0x00000003,
    
};


/* ---------------------------------------------------

    Port RMAC Register's bits

--------------------------------------------------- */


/**
    @brief  RMAC Receive Control Register
*/
enum RMAC_RC_BIT
{
    RMAC_RC_RE   = 0x00000001,          ///< @brief Rx Enable
    RMAC_RC_RXF  = 0x00000002,          ///< @brief PAUSE frame detection
    RMAC_RC_RZPF = 0x00000004,          ///< @brief PAUSE frame reception time
    RMAC_RC_RCPT = 0x00000008           ///< @brief CRC passed
};





/**
    @brief  RMAC Transmit Control Register
*/
enum RMAC_TC_BIT
{
    RMAC_TC_TE  = 0x00000002,           ///< @brief Tx Enable
    RMAC_TC_TXF = 0x00000002,           ///< @brief PAUSE frames
};




/**
    @brief  Request/Current state of AVB switching Block & Port
*/
enum RSWITCH_PortState
{
    RSWITCH_PortState_Unknown = 0,           ///< @brief Undetected/Unknown
    RSWITCH_PortState_StartReset,            ///< @brief RESET mode has been requested, not yet completed
    RSWITCH_PortState_Reset,                 ///< @brief Port is RESET
    RSWITCH_PortState_StartDisable,            ///< @brief RESET mode has been requested, not yet completed
    RSWITCH_PortState_Disable, 
    RSWITCH_PortState_StartConfig,           ///< @brief CONFIG mode has been requested, not yet completed
    RSWITCH_PortState_Config,                ///< @brief Port is in CONFIG mode
    RSWITCH_PortState_StartOperate,          ///< @brief OPERATE mode has been requested, not yet completed
    RSWITCH_PortState_Operate,               ///< @brief Port is in OPERATE mode
    RSWITCH_PortState_Failed                 ///< @brief Port is not operational
};


enum 
{
    EDMAC_LITTLE_ENDIAN, 
    EDMAC_BIG_ENDIAN
};


#define RSWITCH_DEF_MSG_ENABLE \
    (NETIF_MSG_LINK    | \
    NETIF_MSG_TIMER    | \
    NETIF_MSG_RX_ERR| \
    NETIF_MSG_TX_ERR)


#define RSWITCH_QUEUE_PER_PORT 2


/**
    @brief  Private data held in network-device (One per AVB/TSN port)
*/
struct port_private 
{
    struct sk_buff **rx_skb[NUM_RX_QUEUE];
    struct sk_buff **tx_skb[NUM_TX_QUEUE];
    struct net_device *ndev;
    u32                          PortNumber;        ///< @brief Port number (0 .. n)
    enum RSWITCH_PortState        PortState;
    struct platform_device     * pdev;
    u32 num_rx_ring[NUM_RX_QUEUE];
    u32 num_tx_ring[NUM_TX_QUEUE];
    u32 rx_desc_bat_size;
    u32 tx_desc_bat_size;
    u32 tx_ts_desc_bat_size;
    dma_addr_t rx_desc_bat_dma; 
    struct rswitch_ext_desc *rx_desc_bat;
    struct rswitch_ext_desc         *tx_ts_desc_bat;
    dma_addr_t rx_desc_dma[NUM_RX_QUEUE];

    dma_addr_t tx_desc_bat_dma;
    struct rswitch_ext_desc *tx_desc_bat;
    dma_addr_t tx_desc_dma[NUM_TX_QUEUE];
    struct rswitch_ext_ts_desc *rx_ring[NUM_RX_QUEUE];
    struct rswitch_ext_desc *tx_ring[NUM_TX_QUEUE];
    void *tx_align[NUM_TX_QUEUE];
    struct sk_buff *skb;
    const u32                  * PortReg_offset;    ///< @brief table of register offsets - per port
    void __iomem               * port_base_addr;    ///< @brief Remapped memory base for port
    void                       * streaming_private;
    int                          edmac_endian;      ///< @brief EDMAC Little, or Big-Endian
    bool                         Opened;

    struct net_device_stats     Stats;

    int                          msg_enable;        ///< @brief ethtool debug level

    u32                          VRR;               ///< @brief Version and Release register
    u32                          tstamp_tx_ctrl;
    u32                          tstamp_rx_ctrl; 
    struct                       list_head ts_skb_list;

    u32                          ts_skb_tsun;
    struct                       ptp_rswitch ptp;

    /* MII transceiver section. */
    struct mii_bus             * mii_bus;           ///< @brief MDIO bus control 

    // PHY
    u32                          phy_id;            ///< @brief ID on MDIO bus
    struct phy_device          * phydev;            ///< @brief PHY device control 
    enum phy_state               link;              ///< @brief PHY state (Up, down etc)
    phy_interface_t              phy_interface;     ///< @brief PHY Interface (MII etc)
    int                          PHY_ID1;           ///< @brief PHY register 2
    int                          PHY_ID2;           ///< @brief PHY register 3
    int                          speed;             ///< @brief Port speed 10, 100, 1000
    int                          duplex;            ///< @brief 1 (TRUE) is FULL and always set, HALF not supported. 
    unsigned                     ether_link_active_low:1;
    u32 cur_rx[NUM_RX_QUEUE];   /* Consumer ring indices */
    u32 dirty_rx[NUM_RX_QUEUE]; /* Producer ring indices */
    u32 cur_tx[NUM_TX_QUEUE];
    u32 dirty_tx[NUM_TX_QUEUE];
    struct napi_struct napi[NUM_RX_QUEUE];
    u32 ts_skb_tag;
    // RGMII Controller
    u32                          RGMII_Present;     ///< @brief 1 (TRUE) is RGMII Controller on MDIO bus is present
    u32                          RGMII_ID;          ///< @brief ID on MDIO bus
    spinlock_t lock;
};


extern struct EthPorts_Config      Board_Config;
extern struct net_device        ** ppndev;
extern struct rswitch_Config       Config;
extern const char * port_QueryPHYState(enum phy_state State);
extern const char * const port_QueryStateText(enum RSWITCH_PortState state);








#endif /* __RSWITCHTSN_MAIN_H__ */


/*
    Change History
    2018-05-22    AK  Initial Version
    2020-01-06    AK  Release Version
    
    
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


