/*
*  Renesas R-Switch  -  definitions
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


#ifndef __RENESAS_RSWITCH_H__
#define __RENESAS_RSWITCH_H__




// --------------------------------------------- Real and Virtual MAC addresses ---------------------------------------------

#define RENESAS_RSWITCH_BASE_PORT_MAC                  { 0x02, 0x00, 0x00, 0x88, 0x88, 0x00 }


// ------ Source & Destination Port Identifiers
#define RENESAS_RSWITCH_PORT_VECTOR_CPUHOST            0
#define RENESAS_RSWITCH_PORT_VECTOR_TSN0               0
#define RENESAS_RSWITCH_PORT_VECTOR_TSN1               1
#define RENESAS_RSWITCH_PORT_VECTOR_TSN2               2
#define RENESAS_RSWITCH_PORT_VECTOR_TSN3               3
#define RENESAS_RSWITCH_PORT_VECTOR_TSN4               4
#define RENESAS_RSWITCH_PORT_VECTOR_TSN5               5
#define RSWITCH_FWD_CONFIG_TIMEOUT_MS                  100
#define LINK_VERIFICATION_CONFIG_TIMEOUT_MS            1000
#define PKT_BUF_SZ  1538
#define RSWITCH_REQUEST_DELAY_CYCLE                    10
#define PLATFORM_CLOCK_FREQUENCY_MHZ                   70
#define PLATFORM_CLOCK_FREQUENCY_HZ                    70312500
//#define PLATFORM_CLOCK_FREQUENCY_HZ                    100000000
#define RENESAS_RSWITCH_L2_FWD_ENTRIES                 1024
#define RENESAS_RSWITCH_VLAN_FWD_ENTRIES               1024
#define RENESAS_RSWITCH_MAX_VLAN_COMMUNITIES           2    
#define RENESAS_RSWITCH_MAX_PCP_DEI_CONFIG             16  
#define RENESAS_RSWITCH_MAX_VLAN_GROUPS                3
#define RENESAS_RSWITCH_DEST_FWD_ENTRIES               4
#define RENESAS_ETH_INGRESS_FILTER_ID                  6
#define RENESAS_RSWITCH_MSDU_FILTERS                   40
#define  RENESAS_RSWITCH_INGRESS_METERS                 40
#define RENESAS_RSWITCH_INGRESS_FILTERS                128
#define RENESAS_RSWITCH_INGRESS_GATES                  40
#define RENESAS_RSWITCH_INGRESS_GATES_PER_SCHEDULE     8
#define RENESAS_RSWITCH_MAX_INGRESS_GATE_CONTROLS      40
// ------ AVB/TSN Ethernet Ports
#define RENESAS_RSWITCH_MAX_ETHERNET_PORTS             7           /** @brief Limited by number of Port vectors */

#define RENESAS_RSWITCH_MAX_PORTS                      RENESAS_RSWITCH_MAX_ETHERNET_PORTS  + 1
#define RENESAS_RSWITCH_QUEUE_ENABLE                        1

#define RSWITCH_BITFILE_VERSION                        0x19120401
#define RENESAS_RSWITCH_MAC_ID_LENGTH                  6

#define RENESAS_RSWITCH_TX_QUEUES                      8

#define RENESAS_RSWITCH_MAX_PORT_NAME                  32

#define RENESAS_RSWITCH_TX_CBS                         4

#define RENESAS_RSWITCH_MAX_FRAME_SIZE                 2000
#define RENESAS_RSWITCH_MAX_BWF                        1000

#define RENESAS_RSWITCH_VLAN_PCP_VALUES                     7  
#define RENESAS_RSWITCH_VLAN_DEI_VALUES                     7
#define RENESAS_RSWITCH_MAX_VLAN_ID                    4095 

#define RENESAS_RSWITCH_ETH_MAX_GATE_CONTROLS          64
#define RENESAS_RSWITCH_TX_TAS_MAX_SECONDS             0xFFFFFFFFFFFF 
#define RENESAS_RSWITCH_TX_TAS_MAX_NANOSECONDS         0xFFFFFFFF               

#define RENESAS_RSWITCH_TX_TAS_MIN_GATEINTERVAL        700
#define RENESAS_RSWITCH_TX_TAS_MAX_GATEINTERVAL        16700000


#define RENESAS_RSWITCH_MAX_FWD_TBL_ENTRY              4096




typedef  __uint128_t uint128_t;   

// ---------------------------- [R-SWITCH Port Configuration ] ----------------------------



enum rswitch_GateState
{
    rswitch_GateState_Close,
    rswitch_GateState_Open

};

/**
    @brief  TSN Ethernet Port Transmit Time-Aware-Shaper Gate
*/
struct rswitch_Gate
{
    uint32_t                QueueNumber;            ///< @brief (0 .. RACE_ETH_TX_QUEUES-1)
    enum rswitch_GateState   State;                  ///< @brief Gate is open or closed
};

struct rswitch_Queue_Gate
{
    uint32_t                TimeInterval;     ///< @brief Multiplier to Gate Time Tick
    enum rswitch_GateState   State;                  ///< @brief Gate is open or closed
};



/**
    @brief  TSN Ethernet Port Transmit Time-Aware-Shaper Gate Control
*/
struct rswitch_GateControl
{
    
    uint32_t                QueueNumber; 
    uint32_t                GateControls;                      ///< @brief Number of Gate[] entries used
    struct rswitch_Queue_Gate      QueueGateConfig[RENESAS_RSWITCH_ETH_MAX_GATE_CONTROLS];   ///< @brief Gate state per Tx Queue
    uint32_t                skip_first_entry;
    uint32_t                OpenCycleTime;
};


/**
    @brief  TSN Ethernet Port Transmit Time-Aware-Shaper/QCI trigger Time configuration
*/
struct rswitch_Config_Port_TAS_QCI_TriggerTime
{
    uint32_t     bEnable; 
    uint64_t     Second;
    uint32_t     NanoSecond;

};

struct rswitch_Config_Port_QCI_TriggerTime
{
    uint64_t nseconds;

};
enum  rswitch_gptp_timer_domain
{
    rswitch_gptp_timer_domain_zero,
    rswitch_gptp_timer_domain_one

};


/**
    @brief  Renesas R-Switch  Ethernet Port Transmit Time-Aware-Shaper configuration
*/
struct rswitch_Config_Port_Tx_TAS
{
    /*TBD*/
    enum  rswitch_gptp_timer_domain timer_domain;
    uint32_t jitter_time;
    uint32_t latency_time; 
    uint64_t SWTimeMultiplier;  
    uint32_t                    bEnable;                ///< @brief TRUE to enable TAS
    uint32_t      AdminGateStates;
    struct rswitch_Gate AdminGateState[RENESAS_RSWITCH_TX_QUEUES];
    uint64_t                    AdminCycleTime;
    uint64_t                    ConfigChangeTime;
    struct rswitch_Config_Port_QCI_TriggerTime AdminBaseTime;
    struct rswitch_Config_Port_QCI_TriggerTime             AdminCycleTimeExtension;
    uint32_t      ConfigPending;
    
    uint32_t                    Queues;           ///< @brief Number of GateControl[] entries used
    struct rswitch_GateControl   GateControl[RENESAS_RSWITCH_TX_QUEUES];
    ///< @brief Gate Definitions
};



struct     CBS_Config
{

    uint32_t Enable;
    uint32_t CBS_Number;


};


/**
    @brief  Renesas R-Switch Ethernet Port Transmit Queue configuration
*/
struct rswitch_Config_Port_Tx_TxQueue
{
    uint32_t                    QueueNumber;            ///< @brief (0 .. RENESAS_RSWITCH_TX_QUEUES-1)
    
    uint32_t                    Pre_Empt_MAC;           ///< @brief TRUE for pMAC, FALSE for eMAC
    
    
    int                    MAX_Frame_Sz;
};



struct  rswitch_Config_EThProtocol_Offset
{
    uint32_t   bEnable;
    uint32_t   ProtOff;

};


enum rswitch_Config_Port_Tx_Stream_Class 
{
    rswitch_Config_Port_Tx_Stream_Class_A = 2000,
    rswitch_Config_Port_Tx_Stream_Class_B = 4000,
    rswitch_Config_Port_Tx_Stream_Class_C = 6000
};

enum rswitch_Config_Port_Tx_Stream_Speed 
{
    rswitch_Config_Port_Tx_Stream_Link_100M = 100,
    rswitch_Config_Port_Tx_Stream_Link_1G = 1000
};

/**
    @brief  Renesas R-Switch Ethernet Port Transmit Bandwidth allocation
*/
struct rswitch_Config_Port_Tx_Stream
{
    uint32_t                    QueueNum;            ///< @brief (0 .. RENESAS_RSWITCH_TX_CBS -1)
    uint32_t                    BandwidthFraction;      ///< @brief Maximum fraction of port Tx Rate(bit/s) available to the queue
    uint32_t                    portTransmitRate;
    uint32_t                    CIVman;
    uint32_t                    CIVexp;
    uint32_t                    CUL;
};

/**
    @brief  Add VLAN header to BE frames (non VLAN tagged frames)
*/
struct rswitch_Config_Port_VLANTag
{
    uint32_t                    bEnable;            ///< @brief TRUE to enable 
    uint32_t                    VLAN_ID;            ///< @brief (RSWITCH_ETH_VLAN_MIN_VLAN_ID .. RSWITCH_ETH_VLAN_MAX_VLAN_ID)
    uint32_t                    PCP;                ///< @brief PCP added to VLAN Tag (0 .. 7)
    uint32_t                    DEI;                ///< @brief DEI added to VLAN Tag (0 .. 7)
};


/**
    @brief  Remove VLAN header for matching frames
*/
struct rswitch_Config_Port_VLANUntag
{
    uint32_t                    bEnable;            ///< @brief TRUE to enable 
};



/**
    @brief  Renesas R-Switch Ethernet port transmit configuration
*/
struct rswitch_Config_Port_Tx
{
    struct rswitch_Config_Port_Tx_TAS   TAS;        ///< @brief Time-Aware-Shaper configuration

    uint32_t                            TxQueues;   ///< @brief Number of Transmit Queues
    struct rswitch_Config_Port_Tx_TxQueue   
    TxQueue[RENESAS_RSWITCH_TX_QUEUES];
    ///< @brief Transmit Queues

    uint32_t                            TxStreams;  ///< @brief Number of Transmit Streams (CBS)
    struct rswitch_Config_Port_Tx_Stream    
    TxStream[RENESAS_RSWITCH_TX_CBS];
    ///< @brief Transmit Streams (CBS)
    struct rswitch_Config_Port_VLANTag
    VLAN_Tag;   ///< @brief Add VLAN header to BE frames (non VLAN tagged frames)
    struct rswitch_Config_Port_VLANUntag
    VLAN_Untag; ///< @brief Remove VLAN header for matching frames
};




/**
    @brief  Renesas R-Switch Ethernet port receive configuration
*/
struct rswitch_Config_Port_Rx
{
    uint32_t                    Pre_Empt_MAC;           ///< @brief TRUE for pMAC, eMAC always enabled
    struct rswitch_Config_Port_VLANTag
    VLAN_Tag;               ///< @brief Add VLAN header to BE frames (non VLAN tagged frames)
    struct rswitch_Config_Port_VLANUntag
    VLAN_Untag;             ///< @brief Remove VLAN header for matching frames
    
};







/**
    @brief  Renesas R-Switch  Ethernet Port Parameters
*/
struct rswitch_Config_Port
{
    uint32_t                    PortNumber;         ///< @brief Ethernet Port number (0 .. RENESAS_RSWITCH_MAX_ETHERNET_PORTS-1)
    uint32_t                    PminSize;
    uint32_t                    bEnableMAC;         ///< @brief TRUE if MAC provided
    uint8_t                     MAC[RENESAS_RSWITCH_MAC_ID_LENGTH];  
    ///< @brief 6 bytes MAC ID
    
    struct rswitch_Config_Port_Rx     
    RxParam;            ///< @brief Receive definitions
    struct rswitch_Config_Port_Tx    
    TxParam;            ///< @brief Transmit definitions
    char                        PortName[RENESAS_RSWITCH_MAX_PORT_NAME];
    ///< @brief Optional Port Name
};





/**
    @brief  Renesas R-Switch  Ethernet Port definition (Static Configuration)

    Definition for all Renesas R-Switch Ethernet Ports
*/
struct rswitch_Config
{
    uint32_t                    Ports;                              ///< @brief How many entries in Port[]
    struct rswitch_Config_Port   Port[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];      ///< @brief Ethernet port definitions
    
    

};

struct rswitch_EAEIS_error
{
    char                        ErrorName[64];
    uint32_t                    active;

};

struct rswitch_fwd_FWEIS1
{
    uint32_t watermark_critical_overtook;
    uint32_t watermark_flush_overtook;
    uint32_t bufferpool_out_of_pointer;
    uint32_t hw_learning_fail;
    uint32_t MAC_tbl_overflow;
    uint32_t list_filtering_error;
    uint32_t qci_filtering_error;
    uint32_t ECC_two_bit_ecc_error;
    uint32_t ECC_two_bit_critical_error;
    uint32_t ECC_two_bit_fatal_error;
};

struct rswitch_fwd_Error
{
    uint32_t SPL_violation_status[RENESAS_RSWITCH_MAX_ETHERNET_PORTS + 1];
    struct rswitch_fwd_FWEIS1 FWEIS1;
    uint32_t frame_red_status[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    uint32_t gate_filter_gate_change_error[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t gate_filter_config_change_error[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t frame_gateclosed_status[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    uint32_t frame_oversize_status[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    

};



struct rswitch_fwd_Status_FWMIS0
{
    uint32_t qcitbl_full_status;
    uint32_t srptbl_full_status;
    uint32_t mactbl_full_status;
    uint32_t vlantbl_full_status;
    uint32_t AFDMAS;
    uint32_t MADAS;
};


struct rswitch_fwd_Status
{
    struct rswitch_fwd_Status_FWMIS0 FWMIS0;
    uint32_t gate_filter_sched_start_status[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t gate_filter_cfgchng_complete_status[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    

};

struct rswitch_AXI_error
{
    uint32_t tx_desc_config_error[64];
    uint32_t rx_desc_frame_sz_error[256];
    uint32_t rx_desc_full_error[256];
    uint32_t rx_desc_frame_cfg_error[256];
    uint32_t rx_desc_incarea_overflow_error[8];
    uint32_t ts_desc_full_error[8];
    uint32_t ts_desc_config_error[8];
    uint32_t axi_bus_error;
    uint32_t ecc_fatal_error;

};



struct rswitch_gw_Error
{
    uint32_t ECC_fatal_error;
    uint32_t trans_ECC_error;
    struct rswitch_AXI_error AXI_error;
    uint32_t E_frame_Wmark_critical_level[8];
    uint32_t P_frame_Wmark_critical_level[8];
    uint32_t E_frame_overflow[8];
    uint32_t P_frame_overflow[8];
    uint32_t AXIBMIEIS;

};


struct rswitch_fwd_Counter
{
    uint32_t exceptional_path_pkt_cnt;
    uint32_t mirror_path_pkt_cnt;
    uint32_t rejected_pkt_cnt;
    uint32_t bcast_pkt_cnt;
    uint32_t mcast_pkt_cnt;
    uint32_t rx_desc_cnt;
    uint32_t remain_ptr_cnt;
    uint32_t total_ptr_cnt;
    uint32_t least_remain_ptr_cnt;
    uint32_t msdu_reject_pkt_cnt[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    uint32_t qci_gate_reject_pkt_cnt[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    uint32_t qci_meter_drop_pkt_cnt[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    uint32_t qci_meter_green_pkt_cnt[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    uint32_t qci_meter_yellow_pkt_cnt[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    uint32_t qci_meter_red_pkt_cnt[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
};


struct rswitch_Port_Error
{
    struct rswitch_EAEIS_error EAEIS0_error[32];
    struct rswitch_EAEIS_error EAEIS1_error[32];
    struct rswitch_EAEIS_error EAEIS2_error[32];
    

};
struct rswitch_Error
{
    
    struct rswitch_Port_Error   Port_Error[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    struct rswitch_fwd_Error    fwd_Error;
    struct rswitch_gw_Error     gw_Error;

};


struct rswitch_Ingress_GateControl
{
    uint32_t                IPV; 
    uint32_t                GateTimeTickMultiplier;     ///< @brief Multiplier to Gate Time Tick
    enum rswitch_GateState  State;                  ///< @brief Gate is open or closed
};


/*Put Software Restriction for Gate ID 0-7 Port0, Id 8-15 Port 1 and so on */
struct rswitch_Ingress_Gate
{
    uint32_t                Invalid_Rx;                      ///< @brief InvalidRx configuration
    uint32_t                GateID;                    ///< @brief (0 .. RACE_ETH_TX_QUEUES-1)
    
    
    uint32_t IPV_enable;
    
    uint32_t                GateControls;                      ///< @brief Number of Gate[] entries used
    struct rswitch_Ingress_GateControl     GateControl[RENESAS_RSWITCH_MAX_INGRESS_GATE_CONTROLS];   ///< @brief Gate state per Tx Queue
    
};
struct rswitch_fwd_qci_status
{
    uint32_t oper_start_time_upper[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t oper_start_time_middle[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t oper_start_time_lower[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t oper_LIT_time_upper[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t oper_LIT_time_middle[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t oper_LIT_time_lower[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    uint32_t oper_entry_num[RENESAS_RSWITCH_MAX_ETHERNET_PORTS * 8];
    struct rswitch_Ingress_Gate  OperGate[RENESAS_RSWITCH_INGRESS_GATES_PER_SCHEDULE * RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
};


struct rswitch_Staus
{
    
    
    struct rswitch_fwd_Status     fwd_Status;
    struct rswitch_fwd_Counter    fwd_Counter;
    struct rswitch_fwd_qci_status qci_status;
    

};



enum rswitch_Enable_Disable
{
    rswitch_Disable = 0,
    rswitch_Enable

};





enum rswitch_Config_fwd_enable_disable
{
    rswitch_Config_fwd_disable,
    rswitch_Config_fwd_enable

};

enum rswitch_Config_fwd_Pvt_VLAN_Type
{
    rswitch_Config_fwd_Pvt_VLAN_Isolated,
    rswitch_Config_fwd_Pvt_VLAN_Promiscous,
    rswitch_Config_fwd_Pvt_VLAN_Community
};

enum rswitch_Config_Fwd_Queue_Priority
{
    rswitch_Queue_Priority_IPV,
    rswitch_Queue_Priority_PCP

};


enum rswitch_Config_Fwd_UN_Select
{
    rswitch_Fwd_UN_VLAN,
    rswitch_Fwd_UN_MAC

};

enum rswitch_Config_Fwd_MAC_Select
{
    rswitch_MAC_Select_Src,
    rswitch_MAC_Select_DEst

};


enum rswitch_Config_Fwd_VLAN_MAC_Fwd
{
    rswitch_Fwd_VLANorMAC,
    rswitch_Fwd_VLANandMAC,
    rswitch_Fwd_VLAN,
    rswitch_Fwd_MAC

};

enum rswitch_Config_Fwd_VLAN_MAC_CSDN
{
    rswitch_Fwd_CSDN_VLAN,
    rswitch_Fwd_CSDN_MAC
};

enum rswitch_Config_Fwd_Active_Inactive
{
    rswitch_Fwd_Config_InActive,
    rswitch_Fwd_Config_Active

};

enum rswitch_Config_Fwd_FilterSelect
{
    rswitch_Select_FILTER_White,
    rswitch_Select_FILTER_Black

};
enum rswitch_Config_fwd_mirroring_priority_type
{
    rswitch_Mirror_Priority_Descriptor,
    rswitch_Mirror_Priority_Mirror
};

enum rswitch_Config_fwd_ForwardSourceDest
{
    rswitch_Mirror_Forward_SrcorDest,
    rswitch_Mirror_Forward_SrcandDest
};



enum   rswitch_Config_fwd_mac_learning 
{
    rswitch_fwd_MAC_learn_Inactive,
    rswitch_fwd_MAC_learn_ActiveSucceed,
    rswitch_fwd_MAC_learn_ActiveFailed,
    rswitch_fwd_MAC_learn_ActiveSucceedFailed
};
enum   rswitch_Config_fwd_Include_Exclude
{
    rswitch_fwd_Config_Include,
    rswitch_fwd_Config_Exclude


};

enum rswitch_fwd_config_accept_discard
{
    rswitch_fwd_Config_Discard,
    rswitch_fwd_Config_Accept


};
enum rswitch_Config_Fwd_VLAN_Grp_ID
{
    rswitch_Fwd_VLAN_Group0,
    rswitch_Fwd_VLAN_Group1,
    rswitch_Fwd_VLAN_Group2,
    rswitch_Fwd_VLAN_Group3

};
enum rswitch_fwd_yes_no
{
    rswitch_fwd_Config_No,
    rswitch_fwd_Config_Yes

};

enum rswitch_fwd_msdu_filter_mode
{
    rswitch_fwd_msdu_filter_mode_pas,
    rswitch_fwd_msdu_filter_mode_dropped


};

enum rswitch_fwd_vlan_colour_map
{
    rswitch_fwd_vlan_colour_yellow,
    rswitch_fwd_vlan_colour_red

};

enum rswitch_Config_fwd_MAC_VLAN_Priority
{
    rswitch_Config_Priority_MAC,
    rswitch_Config_Priority_VLAN,
    rswitch_Config_Priority_Highest,
    rswitch_Config_Priority_Lowest

};

struct rswitch_fwd_meter_vlan_colour_map
{
    uint32_t PCP_value;
    uint32_t DEI_value;
    enum rswitch_fwd_vlan_colour_map colour_code;



};

enum rswitch_Config_fwd_STP_State
{
    
    rswitch_Config_SPT_State_Disabled,
    rswitch_Config_SPT_State_Blocked,
    rswitch_Config_SPT_State_Learning,
    rswitch_Config_SPT_State_Forwarding,
    rswitch_Config_SPT_State_LearnandFrwrd

};



struct rswitch_Config_Ingress_Meter
{
    uint32_t                    Meter_Id;  
    uint32_t                    Drop_On_Yellow;
    uint32_t                    Mark_All_Frames_Red;
    uint32_t                    Mark_All_Red_Enable;
    uint32_t                    CBS;
    uint32_t                    CIR;
    uint32_t                    EBS;
    uint32_t                    EIR;
    uint32_t                    Coupling;
    struct rswitch_fwd_meter_vlan_colour_map vlan_colour_map[RENESAS_RSWITCH_MAX_PCP_DEI_CONFIG];
};
struct rswitch_Config_MSDU_Config
{
    uint32_t msdu_enable;
    uint32_t msdu_id;
    uint32_t msdu_size;
    enum rswitch_fwd_msdu_filter_mode mode;

};
struct rswitch_Config_Ingress_PCP
{
    uint32_t                    bEnable;
    uint32_t                    PCP_ID;

};
struct rswitch_Config_Ingress_VLAN
{
    uint32_t                    bEnable;
    uint32_t                    VLAN_ID;

};

struct rswitch_Filter_Meter
{
    uint32_t                    Meter_ID;
    uint32_t bEnable;
};

struct rswitch_Filter_Gate
{
    uint32_t                    Gate_ID;
    uint32_t bEnable;
};

struct rswitch_Filter_MSDU
{
    uint32_t                    MSDU_ID;
    uint32_t bEnable;
};


struct rswitch_Config_Ingress_Filter
{
    
    uint8_t                     Filter_ID[RENESAS_ETH_INGRESS_FILTER_ID];
    uint32_t                    Port_Number;
    struct rswitch_Filter_Gate  FilterGate;
    struct rswitch_Filter_MSDU  FilterMSDU;
    uint32_t                    DEI;
    struct rswitch_Filter_Meter  Ingress_Filter_Meter;
    struct rswitch_Config_Ingress_PCP Ingress_PCP_Config;
    struct rswitch_Config_Ingress_VLAN Ingress_VLAN_Config;
};


struct rswitch_Config_Ingress_Gate
{
    
    uint32_t                    Gate_ID;
    uint32_t                    bEnable;                   

};






struct rswitch_Initial_GateState
{
    uint32_t                GateID;    
    uint32_t                IPV;
    enum rswitch_GateState  State;   

};
struct rswitch_Config_PSFP
{
    
    uint32_t                    Gates;
    uint32_t                    PortNumber;
    enum  rswitch_gptp_timer_domain timer_domain;
    uint32_t jitter_time;
    uint32_t latency_time;
    uint64_t                    AdminCycleTime;
    uint64_t                    ConfigChangeTime;
    uint32_t                    SWTimeMultiplier;
    uint32_t      ConfigPending;
    struct rswitch_Config_Port_QCI_TriggerTime AdminBaseTime;
    struct rswitch_Config_Port_QCI_TriggerTime CycleTimeExtension;
    uint32_t      AdminGateStates;
    struct rswitch_Initial_GateState AdminGateState[RENESAS_RSWITCH_INGRESS_GATES_PER_SCHEDULE];
    struct rswitch_Ingress_Gate  Gate[RENESAS_RSWITCH_INGRESS_GATES_PER_SCHEDULE];

};


struct rswitch_Config_Ingress_Filtering
{

    uint32_t                    bEnable;
    uint32_t                    Schedules;
    struct rswitch_Config_PSFP  QCI_Gate_Config[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    /*Below two for future use */
    uint32_t                    Gates;
    struct rswitch_Config_Ingress_Gate Ingress_Gate_Config[RENESAS_RSWITCH_INGRESS_GATES];
    uint32_t                    Filters;
    struct rswitch_Config_Ingress_Filter Ingress_Filters_Config[RENESAS_RSWITCH_INGRESS_FILTERS];
    uint32_t                    Meters;
    struct rswitch_Config_Ingress_Meter  Ingress_Meter_Config[RENESAS_RSWITCH_INGRESS_METERS];
    uint32_t                    MSDU_Filters;
    struct rswitch_Config_MSDU_Config Msdu_Config[RENESAS_RSWITCH_MSDU_FILTERS];

};

struct rswitch_fwd_Dest_Eth
{
    uint32_t                PortNumber;             ///< @brief Ethernet Port number (0 .. RACE_MAX_ETHERNET_PORTS-1)
};

struct rswitch_Config_Dest
{
    uint32_t                       Dest_Eths;                          ///< @brief Number of entries in Dest_Eth
    struct rswitch_fwd_Dest_Eth    Dest_Eth[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU_Enable;
    uint32_t CSDN;
    uint32_t Priority;
    uint32_t Priority_Enable;



};
struct    rswitch_Config_Src_Port_Vector
{
    uint32_t CPU_Port;
    uint32_t PortNumber;


};
struct rswitch_Config_Broadcast
{
    struct    rswitch_Config_Src_Port_Vector    Broadcast_Config_Port;
    uint32_t                       Dest_Eths;                          ///< @brief Number of entries in Dest_Eth
    struct rswitch_fwd_Dest_Eth    Dest_Eth[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU_Enable;
    uint32_t CSDN;

};
struct     rswitch_fwd_Source_Port_Dest_Config
{
    uint32_t                       Dest_Eths;
    struct rswitch_fwd_Dest_Eth    Dest_Eth[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU_Enable;
    uint32_t CSDN;
};
struct rswitch_fwd_Source_Port_Config
{
    uint32_t                PortNumber;
    uint32_t                CPU_Mirror;
    uint32_t                Eth_Mirror;
    uint32_t                CPU_Host;
    struct     rswitch_fwd_Source_Port_Dest_Config Dest_Config;

};



struct rswitch_Config_Source_Fwd
{
    uint32_t bEnable;
    uint32_t source_fwd_port_configs;
    struct rswitch_fwd_Source_Port_Config  Source_Port_Config[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    
};

struct rswitch_Config_BPDU_Fwd
{
    uint32_t bEnable;
    
    uint8_t                     MAC[RENESAS_RSWITCH_MAC_ID_LENGTH];        ///< @brief Destination MAC ID
    uint8_t                     Mask[RENESAS_RSWITCH_MAC_ID_LENGTH];
    struct rswitch_Config_Dest  BPDU_Dest;


};
struct rswitch_Config_Dynamic_Auth 
{
    uint32_t bEnable;
    uint32_t mac_e_enable;
    uint32_t mac_0_enable;
    uint32_t mac_3_enable;
    struct rswitch_Config_Dest Dest;
};

struct rswitch_DestFwd
{
    uint8_t                     MAC[RENESAS_RSWITCH_MAC_ID_LENGTH];        ///< @brief Destination MAC ID
    uint8_t                     Mask[RENESAS_RSWITCH_MAC_ID_LENGTH];

    struct rswitch_Config_Dest        Destination;                        ///< @brief Destinations for this MAC address

    
};

struct rswitch_Config_Dest_Fwding
{
    uint32_t bEnable;
    uint32_t DestFwdEntries;
    struct rswitch_DestFwd       DestFwd_Entry[RENESAS_RSWITCH_DEST_FWD_ENTRIES];



};
struct rswitch_Config_VLAN_Group
{
    enum rswitch_Config_Fwd_VLAN_Grp_ID   GrpID;  
    uint32_t Dest_Eths;
    struct rswitch_fwd_Dest_Eth    Dest_Eth[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU_Enable;

};
struct rswitch_Config_Private_VLAN
{
    uint32_t bEnable;
    enum rswitch_fwd_yes_no source_independent;
    uint32_t VLAN_Groups;
    struct rswitch_Config_VLAN_Group vlan_group[RENESAS_RSWITCH_MAX_VLAN_GROUPS];
    
};

struct rswitch_Config_fwd_Agent_FilterCSD
{
    uint32_t Eths;
    struct rswitch_fwd_Dest_Eth    Eth[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU_Enable;


};
struct rswitch_fwd_config_watermark_pcp_dei_config
{
    uint32_t PCP_ID;
    uint32_t DEI;
    enum rswitch_fwd_config_accept_discard flush;
    enum rswitch_fwd_config_accept_discard clevel;    


};

struct rswitch_Config_fwd_WaterMarkControlPortConfig
{
    uint32_t PortNumber;
    enum rswitch_fwd_config_accept_discard novlanflush;
    enum rswitch_fwd_config_accept_discard novlanclevel;
    uint32_t PCP_DEI_Configs;
    struct rswitch_fwd_config_watermark_pcp_dei_config pcp_dei_config[RENESAS_RSWITCH_MAX_PCP_DEI_CONFIG];
    uint32_t CPU_Enable;


};



struct rswitch_Config_fwd_WaterMarkControl_NCConfig
{
    uint32_t bEnable;
    uint32_t ForceWatermark;
    enum   rswitch_Config_fwd_enable_disable PCPUpdate;
    uint32_t PCP;
    enum   rswitch_Config_fwd_enable_disable DirectDescriptorBypass;   


};

struct rswitch_Config_fwd_WaterMarkControl
{
    
    uint32_t bEnable;
    uint32_t flush_level;
    uint32_t critical_level;
    uint32_t rmn_pause_frm_assrt;
    uint32_t rmn_pause_frm_dassrt;
    uint32_t WaterMarkControlPortConfigs;
    struct rswitch_Config_fwd_WaterMarkControl_NCConfig NC_Config;
    struct rswitch_Config_fwd_WaterMarkControlPortConfig WaterMarkControlPortConfig[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    

};

struct rswitch_Config_fwd_ExceptionalPath
{
    uint32_t bEnable;
    enum   rswitch_Config_fwd_enable_disable AuthFailtoCPU;
    enum   rswitch_Config_fwd_enable_disable BPDUFailtoCPU;
    enum   rswitch_Config_fwd_enable_disable WMarktoCPU;
    enum   rswitch_Config_fwd_enable_disable QCIrejecttoCPU;
    enum   rswitch_Config_fwd_enable_disable LearnstatictoCPU;
    enum   rswitch_Config_fwd_enable_disable UnmatchSIDtoCPU;
    enum   rswitch_Config_fwd_enable_disable UnmatchVLANtoCPU;
    enum   rswitch_Config_fwd_enable_disable UnmatchMACtoCPU;
    enum   rswitch_Config_fwd_enable_disable NullDPVtoCPU;
    enum   rswitch_Config_fwd_enable_disable ErrorDesctoCPU;
    enum   rswitch_Config_fwd_enable_disable FilterRejecttoCPU;
    enum   rswitch_Config_fwd_enable_disable SPLViolationtoCPU;
    uint32_t Priority;
    uint32_t CSDN;

};

struct rswitch_Config_fwd_Learning
{
    uint32_t bEnable;
    enum   rswitch_Config_fwd_enable_disable  UnknownSourcePortLearn;
    enum   rswitch_Config_fwd_enable_disable  HWLearning;
    enum   rswitch_Config_fwd_enable_disable  VLANLearning;
    enum   rswitch_Config_fwd_mac_learning    MACLearning;
    uint32_t Priority;
    uint32_t CSDN;

};

struct rswitch_Config_fwd_PortConfig
{
    uint32_t Ports;
    struct rswitch_fwd_Dest_Eth    Port[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU;
};


struct rswitch_Config_Pvt_VLAN_Community
{
    uint32_t Community_ID;
    


};
struct rswitch_Config_fwd_Pvt_VLAN_Port
{
    uint32_t PortNumber;
    enum rswitch_Config_fwd_Pvt_VLAN_Type Type;
    uint32_t CPU;
    uint32_t Pvt_VLAN_Communities;
    struct rswitch_Config_Pvt_VLAN_Community  Pvt_VLAN_Community[RENESAS_RSWITCH_MAX_VLAN_COMMUNITIES];

};


struct rswitch_Config_fwd_Pvt_VLAN_Settings
{

    uint32_t Ports;
    struct rswitch_Config_fwd_Pvt_VLAN_Port Port[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    
    

};

struct rswitch_Config_fwd_Mirroring
{
    uint32_t bEnable;
    struct rswitch_Config_fwd_PortConfig  Source_PortConfig;
    struct rswitch_Config_fwd_PortConfig  Dest_PortConfig;
    enum rswitch_Config_fwd_ForwardSourceDest ForwardSourceDest;
    enum rswitch_Config_fwd_enable_disable  Error_Mirror;
    enum rswitch_Config_fwd_mirroring_priority_type SD_Mirror_Priority_Type;
    uint32_t SD_Mirror_Priority_Value;
    enum rswitch_Config_fwd_mirroring_priority_type Eth_Mirror_Priority_Type;
    uint32_t Eth_Mirror_Priority_Value;
    enum rswitch_Config_fwd_mirroring_priority_type CPU_Mirror_Priority_Type;
    uint32_t CPU_Mirror_Priority_Value;
    uint32_t CSDN;
    struct rswitch_Config_fwd_PortConfig    DestEThMirrorPort;
    struct rswitch_Config_fwd_PortConfig    DestCPUMirrorPort;
    
};
struct rswitch_Config_fwd_Port_Lock
{
    uint32_t bEnable;
    struct rswitch_Config_fwd_PortConfig LockPort;
};


struct rswitch_Config_fwd_SPT_Port_Config
{
    uint32_t PortNumber;
    
    enum rswitch_Config_fwd_STP_State STP_State;
    uint32_t CPU;
};
struct rswitch_Config_fwd_Spanning_Tree
{
    uint32_t Ports;
    struct rswitch_Config_fwd_SPT_Port_Config SPT_Port_Config[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    
};

struct rswitch_Config_fwd_Migration
{
    uint32_t bEnable;
    struct rswitch_Config_fwd_PortConfig Migration_Port;


};

struct rswitch_Config_fwd_Static_Auth
{
    uint32_t bEnable;
    struct rswitch_Config_fwd_PortConfig Static_Auth_Port;

};


struct rswitch_Config_fwd_Search_Config
{
    uint32_t bEnable;
    enum   rswitch_Config_Fwd_Queue_Priority    Queue_Priority; 
    enum   rswitch_Config_fwd_Include_Exclude QCIMAC;
    enum   rswitch_Config_fwd_Include_Exclude QCIVLAN;
    enum   rswitch_Config_fwd_Include_Exclude QCIPCP;
    enum   rswitch_Config_fwd_Include_Exclude QCIDEI;
    enum   rswitch_Config_fwd_Include_Exclude QCISPN;
    enum   rswitch_Config_Fwd_MAC_Select QCI_MAC_Select;
    enum   rswitch_Config_fwd_enable_disable QCI_Filtering;
    enum   rswitch_Config_Fwd_UN_Select SID_UniqueNum_Select;
    enum   rswitch_Config_Fwd_MAC_Select SID_MAC_Select;
    enum   rswitch_Config_Fwd_FilterSelect SID_Filter_Select;
    enum   rswitch_Config_Fwd_Active_Inactive SID_Filter;
    enum   rswitch_Config_Fwd_Active_Inactive SID_Tbl;
    enum   rswitch_Config_Fwd_Active_Inactive Src_Port_Filter;
    enum   rswitch_Config_Fwd_FilterSelect MAC_Filter_Select;
    enum   rswitch_Config_Fwd_Active_Inactive MAC_Filter;
    enum   rswitch_Config_Fwd_Active_Inactive MAC_Tbl;
    enum   rswitch_Config_fwd_MAC_VLAN_Priority   MAC_VLAN_Priority;
    enum   rswitch_Config_Fwd_VLAN_MAC_Fwd VLAN_MAC_Fwd;
    enum   rswitch_Config_Fwd_VLAN_MAC_CSDN VLAN_MAC_CSDN;
    enum   rswitch_Config_Fwd_FilterSelect VLAN_Filter_Select;
    enum   rswitch_Config_Fwd_Active_Inactive VLAN_Filter;
    enum   rswitch_Config_Fwd_Active_Inactive VLAN_Tbl;
};


struct rswitch_Config_L2Dest
{
    uint32_t                       DestEths;                          ///< @brief Number of entries in Dest_Eth
    struct rswitch_fwd_Dest_Eth    DestEth[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU_Enable;
    uint32_t CSDN;
    
    enum   rswitch_Config_fwd_enable_disable Dynamic;
    enum   rswitch_Config_fwd_enable_disable EthMirror;
    enum   rswitch_Config_fwd_enable_disable CPUMirror;
    enum   rswitch_Config_fwd_enable_disable PCPUpdate;
    enum   rswitch_Config_fwd_enable_disable ListFilter;
    uint32_t PCP;
};

struct rswitch_L2Fwd
{
    uint8_t                     MAC[RENESAS_RSWITCH_MAC_ID_LENGTH];        ///< @brief Destination MAC ID

    struct rswitch_Config_L2Dest        Destination;                        ///< @brief Destinations for this MAC address

    
};



struct rswitch_Config_fwd_Insert_L2_Config
{
    uint32_t L2FwdEntries;
    struct rswitch_L2Fwd       L2Fwd[RENESAS_RSWITCH_L2_FWD_ENTRIES];
    
    
    
};

struct rswitch_DelL2Fwd 
{
    uint32_t                       Dest_Eths;                          ///< @brief Number of entries in Dest_Eth
    struct rswitch_fwd_Dest_Eth    Dest_Eth[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU_Enable;
    
};

struct rswitch_Config_fwd_Delete_L2_Config
{
    uint32_t L2FwdEntries;
    struct rswitch_DelL2Fwd       DelL2Fwd[RENESAS_RSWITCH_L2_FWD_ENTRIES];
    
};

struct rswitch_VLANFwd_Port_Config
{
    uint32_t                       Ports;                          ///< @brief Number of entries in Dest_Eth
    struct rswitch_fwd_Dest_Eth    Port[RENESAS_RSWITCH_MAX_ETHERNET_PORTS]; 
    uint32_t CPU_Enable;
};

struct rswitch_VLANFwd
{
    uint32_t VLAN_ID;
    struct rswitch_VLANFwd_Port_Config VLAN_Filter;
    struct rswitch_VLANFwd_Port_Config VLAN_Routing;
    enum   rswitch_Config_fwd_enable_disable EthMirror;
    enum   rswitch_Config_fwd_enable_disable CPUMirror;
    enum   rswitch_Config_fwd_enable_disable PCPUpdate;
    uint32_t PCP;
    uint32_t CSDN;
    
};
struct rswitch_Config_fwd_Insert_VLAN_Config
{
    uint32_t VLANFwdEntries;
    struct rswitch_VLANFwd       VLANFwd[RENESAS_RSWITCH_VLAN_FWD_ENTRIES]; 
    
    
};
struct rswitch_Config_Double_Tag
{
    uint32_t bEnable;
    uint32_t tag_value;

};

struct rswitch_fwd_Config
{
    uint32_t ConfigDone;
    struct rswitch_Config_Ingress_Filtering   qci_config;
    uint32_t Broadcast_Config_Ports;
    struct rswitch_Config_Broadcast Broadcast_Config[RENESAS_RSWITCH_MAX_ETHERNET_PORTS];
    struct rswitch_Config_Source_Fwd    Source_Fwd;
    struct rswitch_Config_BPDU_Fwd    BPDU_Fwd;
    struct rswitch_Config_Dynamic_Auth    Dynamic_Auth;
    struct rswitch_Config_Dest_Fwding    Dest_Fwd;
    struct rswitch_Config_Private_VLAN    Priv_VLAN;
    struct rswitch_Config_Double_Tag  Double_Tag;
    struct rswitch_Config_fwd_Agent_FilterCSD AgentFilterCSDPorts;
    
    struct rswitch_Config_fwd_WaterMarkControl  WaterMarkControl;
    struct rswitch_Config_fwd_ExceptionalPath  ExceptionalPath;
    struct rswitch_Config_fwd_Learning Learning;
    struct rswitch_Config_fwd_Mirroring Mirroring;
    struct rswitch_Config_fwd_Port_Lock Port_Lock;
    
    struct rswitch_Config_fwd_Spanning_Tree Spanning_Tree;
    struct rswitch_Config_fwd_Migration Migration;
    struct rswitch_Config_fwd_Static_Auth Static_Auth;
    struct rswitch_Config_fwd_Search_Config Search_Config;
    
    struct rswitch_Config_fwd_Pvt_VLAN_Settings Pvt_VLAN_Settings;
    struct rswitch_Config_fwd_Insert_L2_Config Insert_L2_Config;
    /*Below one is a future feature if requested */
    struct rswitch_Config_fwd_Delete_L2_Config Delete_L2_Config;
    struct rswitch_Config_fwd_Insert_VLAN_Config Insert_VLAN_Config;


};




#endif /* __RENESAS_RSWITCH_H__ */

/*
    Change History
    2019-03-22      AK  Initial Version
    2019-04-01      AK  Updated TAS
    2019-10-11      AK  Updated Error and Monitoring
    2019-12-03      AK  Updated Only for VLAN and L2 Update
    2020-01-06      AK  Code Clean Up Release Version
    2020-02-13      AK  Platform Freq in Hz for gptp
    2020-05-05      AK  Added Network Control Configuration for Watermark
    
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

