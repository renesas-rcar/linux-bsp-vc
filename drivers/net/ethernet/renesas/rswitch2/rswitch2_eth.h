/*
*  Renesas R-Switch2 Ethernet (RSWITCH) Device Driver
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


#ifndef __RSWITCH2_ETH_H__
#define __RSWITCH2_ETH_H__


#define RSWITCH2ETH_BASE_PORTNAME     "tsn"
#define RSWITCH2_ETH_CLASS                 "rswitch2eth"
#define RSWITCH2_RXTSTAMP_TYPE_V2_L2_EVENT  0x00000002
#define RSWITCH2_TSN_RXTSTAMP_TYPE          0x00000006
#define RENESAS_RSWITCH2_PHYS       4
#define NUM_TX_QUEUE    2
#define INTERNAL_GW     1
#define RSWITCH2_RX_GW_QUEUE_NUM 0
#define NUM_RX_QUEUE    2 + INTERNAL_GW
#define RENESAS_RSWITCH2_BASE_PORT_MAC                  { 0x02, 0x00, 0x00, 0x88, 0x88, 0x00 }
#define PORT_BITMASK (unsigned int)(GENMASK_ULL(RENESAS_RSWITCH2_MAX_ETHERNET_PORTS, 0))
#define PORTGWCA_BITMASK (unsigned int)(GENMASK_ULL(RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1, 0))
#define RCE_BIT 1 << 16
#define RX_DBAT_ENTRY_NUM   64
#define TX_DBAT_ENTRY_NUM   1024
#define DBAT_ENTRY_NUM    (RENESAS_RSWITCH2_MAX_ETHERNET_PORTS * NUM_TX_QUEUE) + NUM_RX_QUEUE + INTERNAL_GW
#define RX_QUEUE_OFFSET   (RENESAS_RSWITCH2_MAX_ETHERNET_PORTS * NUM_TX_QUEUE) + INTERNAL_GW
#define RSWITCH2_BE 0
#define RSWITCH2_NC 1
#define TX_RING_SIZE0   1024    /* TX ring size for Best Effort */
#define TX_RING_SIZE1   1024    /* TX ring size for Network Control */
#define RX_RING_SIZE0   1024    /* RX ring size for Best Effort */
#define RX_RING_SIZE1   1024    /* RX ring size for Network Control */
#define RX_RING_SIZE2   1024
#define DPTR_ALIGN  4
#define RSWITCH2_QUEUE_NC                  1
#define RENESAS_RSWITCH2_RX_GW_QUEUE 2
#define RIS_BITMASK   GENMASK(DBAT_ENTRY_NUM, RX_QUEUE_OFFSET)
#define TIS_BITMASK   GENMASK(RX_QUEUE_OFFSET -1, 0)
#define RSWITCH2_COM_BPIOG 0x01
#define RSWITCH2_COM_BPR   0x02
#define RSWITCH2_ETH_DRIVER_VERSION "0.0.3"
/* Use 2 Tx Descriptor per frame */
#define NUM_TX_DESC     2
#define RSWITCH2_DEF_MSG_ENABLE \
    (NETIF_MSG_LINK    | \
    NETIF_MSG_TIMER    | \
    NETIF_MSG_RX_ERR| \
    NETIF_MSG_TX_ERR)

#define RSWITCH2_ALIGN   128
#define PKT_BUF_SZ	1538
#define GWTRC_TSRQ0              0x01

/* The Ethernet Basic descriptor definitions. */
struct rswitch2_desc {
    __le16 info_ds;     /* Descriptor size */
    u8 die_dt;  /* Descriptor interrupt enable and type */
    __u8  dptrh;    /* Descriptor pointer  MSB*/
    __le32 dptrl;    /* Descriptor pointer  LSW*/
} __packed;;

/* The Ethernet TS descriptor definitions. */
struct rswitch2_ts_desc {
    __le16 info_ds;     /* Descriptor size */
    u8 die_dt;  /* Descriptor interrupt enable and type */
    __u8  dptrh;    /* Descriptor pointer  MSB*/
    __le32 dptrl;    /* Descriptor pointer  LSW*/
    __le32 ts_nsec;
    __le32 ts_sec;
} __packed;;


struct rswitch2_ext_desc {
    __le16 info_ds;     /* Descriptor size */
    u8 die_dt;  /* Descriptor interrupt enable and type */
    __u8  dptrh;    /* Descriptor pointer  MSB*/
    __le32 dptrl;    /* Descriptor pointer  LSW*/
    __le32 info1l;  /* Descriptor pointer */
    __le32 info1h;
} __packed;;
struct rswitch2_ext_ts_desc {
    __le16 info_ds;     /* Descriptor size */
    u8 die_dt;  /* Descriptor interrupt enable and type */
    __u8  dptrh;    /* Descriptor pointer  MSB*/
    __le32 dptrl;    /* Descriptor pointer  LSW*/
    __le64 info1;  /* Descriptor pointer */
    __le32 ts_nsec;
    __le32 ts_sec;
} __packed;;



enum {
    EDMAC_LITTLE_ENDIAN,
    EDMAC_BIG_ENDIAN
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


enum DIE_DT {
    /* Frame data */
    DT_FSINGLE  = 0x80,
    DT_FSTART   = 0x90,
    DT_FMID     = 0xA0,
    DT_FEND     = 0xB8,

    /* Chain control */

    DT_LEMPTY   = 0xC8, // May be same as linkfix
    DT_EEMPTY       = 0xD0,
    DT_LINKFIX     = 0x00,
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

/*
    Port Register's bits
*/

enum ETH_TSN_CCC_BIT {
    ETH_TSN_EAMC_OPC              = 0x00000003,
    ETH_TSN_EAMC_OPC_RESET        = 0x00000000,
    ETH_TSN_EAMC_OPC_MASK         = 0x00000003,
    ETH_TSN_EAMC_OPC_DISABLE      = 0x00000001,
    ETH_TSN_EAMC_OPC_CONFIG       = 0x00000002,
    ETH_TSN_EAMC_OPC_OPERATION    = 0x00000003,
};

enum ETH_TSN_EAMS_BIT {
    ETH_TSN_EAMS_OPS             = 0x0000000F,
    ETH_TSN_EAMS_OPS_MASK        = 0x0000000F,
    ETH_TSN_EAMS_OPS_RESET       = 0x00000000,
    ETH_TSN_EAMS_OPS_DISABLE     = 0x00000001,
    ETH_TSN_EAMS_OPS_CONFIG      = 0x00000002,
    ETH_TSN_EAMS_OPS_OPERATION   = 0x00000003,

};

/**
    @brief  Request/Current state of AVB switching Block & Port
*/
enum rswitch2_portstate {
    rswitch2_portstate_unknown = 0,           ///< @brief Undetected/Unknown
    rswitch2_portstate_startreset,            ///< @brief RESET mode has been requested, not yet completed
    rswitch2_portstate_reset,                 ///< @brief Port is RESET
    rswitch2_portstate_startdisable,            ///< @brief RESET mode has been requested, not yet completed
    rswitch2_portstate_disable,
    rswitch2_portstate_startconfig,           ///< @brief CONFIG mode has been requested, not yet completed
    rswitch2_portstate_config,                ///< @brief Port is in CONFIG mode
    rswitch2_portstate_startoperate,          ///< @brief OPERATE mode has been requested, not yet completed
    rswitch2_portstate_operate,               ///< @brief Port is in OPERATE mode
    rswitch2_portstate_failed                 ///< @brief Port is not operational
};

struct rswitch2_phy_mux {
    u32 mux;
    u32 gpio;

};

enum renesas_Board_Type {
    RENESAS_VC1,
    RENESAS_VC2,
    RENESAS_VC3,
};
/**
    @brief  Private data held in network-device (One per AVB/TSN port)
*/
struct port_private {
    struct sk_buff **rx_skb[NUM_RX_QUEUE];
    struct sk_buff **tx_skb[NUM_TX_QUEUE];
    struct net_device *ndev;
    struct device_node *node;
    u32                          portnumber;        ///< @brief Port number (0 .. n)
    enum rswitch2_portstate        portstate;
    struct platform_device     * pdev;
    u32                          phy_addr;
    struct sk_buff *skb;
    const u32                  * portReg_offset;    ///< @brief table of register offsets - per port
    void __iomem               * port_base_addr;    ///< @brief Remapped memory base for port
    void                       * streaming_private;
    int                          edmac_endian;      ///< @brief EDMAC Little, or Big-Endian
    bool                         opened;
    struct rswitch2_phy_mux    phy_mux;
    struct net_device_stats     stats;
    dma_addr_t tx_desc_dma[NUM_TX_QUEUE];
    int                          msg_enable;        ///< @brief ethtool debug level

    u32                          vrr;               ///< @brief Version and Release register
    u32                          tstamp_tx_ctrl;
    u32                          tstamp_rx_ctrl;
    struct                       list_head ts_skb_list;

    u32                          ts_skb_tsun;
    u32                          num_rx_ring[NUM_RX_QUEUE];
    u32                          num_tx_ring[NUM_TX_QUEUE];
    struct rswitch2_ext_desc *tx_ring[NUM_TX_QUEUE];
    void                         *tx_align[NUM_TX_QUEUE];
    u32                          desc_bat_size;
    u32                          portindex;
    dma_addr_t                   desc_bat_dma;
    /* MII transceiver section. */
    struct mii_bus             * mii_bus;           ///< @brief MDIO bus control

    // PHY
    u32                          phy_id;            ///< @brief ID on MDIO bus
    struct phy_device          * phydev;            ///< @brief PHY device control
    enum phy_state               link;              ///< @brief PHY state (Up, down etc)
    phy_interface_t              phy_interface;     ///< @brief PHY Interface (MII etc)
    int                          phy_id1;           ///< @brief PHY register 2
    int                          phy_id2;           ///< @brief PHY register 3
    int                          speed;             ///< @brief Port speed 10, 100, 1000
    int                          duplex;            ///< @brief 1 (TRUE) is FULL and always set, HALF not supported.
    unsigned                     ether_link_active_low:1;
    struct napi_struct napi[NUM_RX_QUEUE];
    u32 cur_rx[NUM_RX_QUEUE];   /* Consumer ring indices */
    u32 dirty_rx[NUM_RX_QUEUE]; /* Producer ring indices */
    u32 cur_tx[NUM_TX_QUEUE];
    u32 dirty_tx[NUM_TX_QUEUE];
    u32 ts_skb_tag;
    // RGMII Controller
    u32                          rgmii_present;     ///< @brief 1 (TRUE) is RGMII Controller on MDIO bus is present
    u32                          rgmii_id;          ///< @brief ID on MDIO bus
    spinlock_t lock;
    u32                          priv_flags;
};

#endif


/*
    Change History
    2020-08-10    AK  Initial Version
    2020-08-19    AK  Structure for port and Masks
    2020-09-18    AK  Changes for Internal GW port
    2020-10-07    AK  Changes for Internal GW port Descriptor chain movement

*/
