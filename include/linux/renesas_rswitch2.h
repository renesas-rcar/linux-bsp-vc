/*
*  Renesas R-Switch2  -  definitions
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


#ifndef __RENESAS_RSWITCH_H__
#define __RENESAS_RSWITCH_H__

#define RENESAS_RSWITCH2_MAX_FWD_TBL_ENTRY 1024
#define RSWITCH2_MAX_RTAG     3
#define RSWITCH2_MAX_L2_3_UPDATE_ENTRIES   255
#define RSWITCH2_MAX_PLATFORM_CLOCK   125000
#define RENESAS_RSWITCH2_MAX_IPV4_ENTRY 1024
#define RENESAS_RSWITCH2_MAX_ETHERNET_PORTS 4
#define RSWITCH2_MAX_UNSECURE_ENTRY 0x7FF
#define RSWITCH2_MAX_L3_COLLISION  1023
#define RSWITCH2_MAX_CSDN  255
#define RSWITCH2_MAX_CTAG_DEI 1
#define RSWITCH2_MAX_CTAG_VLAN 0xFFF
#define RSWITCH2_MAX_CTAG_PCP 7
#define RSWITCH2_MAX_STAG_DEI 1
#define RSWITCH2_MAX_IPV 7
#define RSWITCH2_MAX_STAG_VLAN 0xFFF
#define RSWITCH2_MAX_STAG_PCP 7
#define RSWITCH2_MAX_ROUTING_NUMBER 255
#define RSWITCH2_MAX_TCPUDP_PORTNUMBER 0xFFFF
#define RENESAS_RSWITCH2_MAX_PORT_AGENT RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1
#define RSWITCH2_FPGA_BASE              0x00000000
#define RSWITCH2_FPGA_SIZE              0x18000
#define RSWITCH2_FPGA_FWD_OFFSET        0x0000
#define RSWITCH2_FPGA_FWD_BASE          RSWITCH2_FPGA_BASE +  RSWITCH2_FPGA_FWD_OFFSET
#define RSWITCH2_FPGA_FWD_SIZE          0x8000
#define RSWITCH2_FPGA_MFAB_OFFSET       0x8000
#define RSWITCH2_FPGA_MFAB_BASE         RSWITCH2_FPGA_BASE + RSWITCH2_FPGA_MFAB_OFFSET
#define RSWITCH2_FPGA_MFAB_SIZE         0x1000
#define RSWITCH2_FPGA_COMA_OFFSET       0x9000
#define RSWITCH2_FPGA_COMA_BASE         RSWITCH2_FPGA_BASE + RSWITCH2_FPGA_COMA_OFFSET
#define RSWITCH2_FPGA_COMA_SIZE         0x1000
#define RSWITCH2_FPGA_ETH_OFFSET        0x0000A000
#define RSWITCH2_FPGA_ETH_BASE          RSWITCH2_FPGA_BASE + RSWITCH2_FPGA_ETH_OFFSET
#define RSWITCH2_FPGA_ETH_PORT_SIZE     0x2000
#define RSWITCH2_FPGA_GWCA0_OFFSET      0x00012000
#define RSWITCH2_FPGA_GWCA0_BASE        RSWITCH2_FPGA_BASE + RSWITCH2_FPGA_GWCA0_OFFSET
#define RSWITCH2_FPGA_GWCA0_SIZE        0x2000
#define RSWITCH2_FPGA_GWCA1_OFFSET      0x00014000
#define RSWITCH2_FPGA_GWCA1_BASE        RSWITCH2_FPGA_BASE + RSWITCH2_FPGA_GWCA1_OFFSET
#define RSWITCH2_FPGA_GWCA1_SIZE        0x2000
#define RSWITCH2_FPGA_ACPA_OFFSET       0x00016000
#define RSWITCH2_FPGA_ACPA_BASE         RSWITCH2_FPGA_BASE + RSWITCH2_FPGA_ACPA_OFFSET
#define RSWITCH2_FPGA_ACPA_SIZE         0x2000
#define RSWITCH2_MIR_MII                  0x01130008
#define RSWITCH2_PORT_CONFIG_TIMEOUT_MS 100
#define RSWITCH2_MAX_L2_FWD_ENTRIES     1024
#define RSWITCH2_MAX_IP_FWD_ENTRIES     1024
#define RSWITCH2_IP_ADDR_LENGTH         4
#define RSWITCH2_MAX_L2_FWD_VLAN_ENTRIES 0xFFF
#define RSWITCH2_MAC_ID_LENGTH          6
#define RSWITCH2_MAX_IPV4_STREAM_ENTRIES 1024
#define RSWITCH2_MAX_TPID 0xFFFF
#define RENESAS_RSWITCH2_MAX_HASH_EQUATION  0xFFFF
#define RENESAS_RSWITCH2_MAX_UNSECURE_VLAN  0x1FFF
#define RENESAS_RSWITCH2_VC_PCI_ID          0xE002
#define PLATFORM_CLOCK_FREQUENCY_HZ         78125000
#define RSWITCH2_MCAST_DESC_CHAIN_NUM_START	1
#define RSWITCH2_MCAST_DESC_NUM			1
#define RSWITCH2_MCAST_NEXT_DESC_NUM        	2
#define RSWITCH2_MAX_FILTER_NUM	            	3
#define RSWITCH2_MAX_TWO_BYTE_FILTERS       	4
#define RSWITCH2_MAX_THREE_BYTE_FILTERS     	4
#define RSWITCH2_MAX_FOUR_BYTE_FILTERS      	4
#define RSWITCH2_MAX_CASCADE_FILTERS        	4
#define RSWITCH2_MAX_USED_CASCADE_FILTERS   	7
#define RSWITCH2_MAX_USED_CASCADE_FILTERS_NUM	32
#define RSWITCH2_MAX_RANGE_FILTERS		4
#define RSWITCH2_MAX_FILTER_OFFSET		255
#define RSWITCH2_MAX_TWO_BYTE_VALUE	0xFFFF
#define RSWITCH2_MAX_THREE_BYTE_VALUE	0xFFFFFF
#define RSWITCH2_MAX_FOUR_BYTE_VALUE	0xFFFFFFFF
#define RSWITCH2_MAX_RANGE_VALUE	255
#define RSWITCH2_MAX_RANGE		15
#define RSWITCH2_MAX_NEXT_ENTRIES	8
#define RSWITCH2_MAX_MCAST_CHAINS	128
typedef  __uint128_t uint128_t;
extern struct proc_dir_entry *root_dir;
enum rswitch2_gwca_mode {
	rswitch2_gwca_mode_reset,
	rswitch2_gwca_mode_disable,
	rswitch2_gwca_mode_config,
	rswitch2_gwca_mode_operation,
};

enum rswitch2_eth_frame_pass_reject {
	rswitch2_eth_frame_pass,
	rswitch2_eth_frame_reject,
};

enum rswitch2_eth_egress_vlan_mode {
	rswitch2_eth_egress_vlan_none,
	rswitch2_eth_egress_vlan_ctag,
	rswitch2_eth_egress_vlan_hw_ctag,
	rswitch2_eth_egress_vlan_stag,
	rswitch2_eth_egress_vlan_hw_stag,
};

enum rswitch2_eth_ingress_vlan_mode {
	rswitch2_eth_ingress_vlan_incoming,
	rswitch2_eth_ingress_vlan_port_based,
};

struct ethport_config {
	char                portname[IFNAMSIZ];
	uint32_t                 virtual_irq;

	// Pre-defined
	uint8_t                   phy_id;

	// platform resources
	uint32_t                  start;                                  ///< @brief port registers Base address
	uint32_t                  size;                                   ///< @brief port registers Size
	uint32_t                  end;                                    ///< @brief port registers End
	uint32_t                  irq;
};


struct gwca_config {

	uint32_t                  virtual_irq;


	// platform resources
	uint32_t                  start;                                  ///< @brief port registers Base address
	uint32_t                  size;                                   ///< @brief port registers Size
	uint32_t                  end;                                    ///< @brief port registers End
	uint32_t                  iRQ;
};


/**
    @brief  RSWITCH FPGA Common configuration structure
*/
struct ethports_config {

	uint8_t                      eth_ports;                          ///< @brief Number of Ethernet AVB ports
	uint8_t                      agents;
	uint32_t                     port_bitmask;
	uint32_t                     portgwca_bitmask;
	uint32_t                     dbat_entry_num;
	uint32_t                     rx_queue_offset;
	uint32_t                     ris_bitmask;
	uint32_t                     tis_bitmask;
	struct ethport_config        eth_port[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS];
	struct gwca_config           gwca0;
};

enum rswitch2_ipv4_stream_frame_format_code {
	ipv4_notcp_noudp =1,
	ipv4_udp = 2,
	ipv4_tcp = 3,
};

enum rswitch2_ip_addr_type {
	ip_addr_ipv4,
	ip_addr_ipv6,
};

enum rswitch2_feature_include_exclude {
	feature_exclude,
	feature_include,
};

enum rswitch2_enable_disable {
	disable,
	enable,
};


enum rswitch2_priority_decode_mode {
	precedent,
	tos,
};

enum rswitch2_filter_offset_mode {
	offset,
	vlan_tag,
};

enum rswitch2_filter_unit_mode {
	mask,
	expand,
	precise,
};

struct  rswitch2_source_lock_config {
	uint32_t source_ports;
	uint32_t cpu;
	uint32_t source_port_number[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS];


};

struct rswitch2_fwd_port_error {
	uint32_t error[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t set_any;
};

struct rswitch2_proc_eth_err_counter {
	uint32_t min_frame_sz_err[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t tag_filter_err[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t frame_size_err[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t desc_q_oflow_err[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t desc_q_sec_err[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t csum_err[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
};
struct rswitch2_proc_rx_tx_counter {
	uint32_t eframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t pframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint64_t ebyte[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint64_t pbyte[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t mframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t bframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t uframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t phyerror[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t nibbleerror[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t crcerror[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t fragmisserror[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t cfragerror[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t fragerror[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t rmacfiltererror[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t undersizeerrorgood[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t undersizeerrorbad[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t oversizeerrorgood[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t oversizeerrorbad[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t txeframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t txpframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint64_t txebyte[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint64_t txpbyte[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t txerror[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t txbframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t txmframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
	uint32_t txuframe[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
};

struct rswitch2_proc_counter {
	struct rswitch2_proc_rx_tx_counter rx_tx_counter;
	struct rswitch2_proc_eth_err_counter err_counter;
};
struct rswitch2_fwd_counter {
	uint32_t ctfdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];        
	uint32_t ddfdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];          
	uint32_t lthfdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];        
	uint32_t ipfdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];        
	uint32_t ltwfdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];         
	uint32_t pbfdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];         
	uint32_t mhln[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];          
	uint32_t ihln[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];           
	uint32_t icrdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];          
	uint32_t wmrdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];          
	uint32_t ctrdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];          
	uint32_t ddrdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];          
	uint32_t lthrdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];         
	uint32_t iprdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];          
	uint32_t ltwrdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];         
	uint32_t pbrdn[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS + 1];
};

struct rswitch2_fwd_global_error {
	uint32_t arees;
	uint32_t l23uses;
	uint32_t l23uees;
	uint32_t vlantses;
	uint32_t vlantees;
	uint32_t mactses;
	uint32_t mactees;
	uint32_t lthtses;
	uint32_t lthtees;
	uint32_t macadas;
	uint32_t mactfs;
	uint32_t vlantfs;
	uint32_t lthtfs;
	uint32_t set_any;

};


struct rswitch2_fwd_error {
	struct rswitch2_fwd_port_error ddntfs;      
	struct rswitch2_fwd_port_error ddses;          
	struct rswitch2_fwd_port_error ddfes;        
	struct rswitch2_fwd_port_error ddes;        
	struct rswitch2_fwd_port_error wmiufs;         
	struct rswitch2_fwd_port_error wmisfs;         
	struct rswitch2_fwd_port_error wmffs;          
	struct rswitch2_fwd_port_error wmcfs;           
	struct rswitch2_fwd_port_error smhmfs;          
	struct rswitch2_fwd_port_error smhlfs;          
	struct rswitch2_fwd_port_error pbntfs;          
	struct rswitch2_fwd_port_error ltwvufs;          
	struct rswitch2_fwd_port_error ltwdufs;         
	struct rswitch2_fwd_port_error ltwsufs;          
	struct rswitch2_fwd_port_error ltwntfs;         
	struct rswitch2_fwd_port_error ltwvspfs;
	struct rswitch2_fwd_port_error ltwsspfs;
	struct rswitch2_fwd_port_error ltwdspfs;
	struct rswitch2_fwd_port_error lthufs;
	struct rswitch2_fwd_port_error lthntfs;
	struct rswitch2_fwd_port_error lthfsfs;
	struct rswitch2_fwd_port_error lthspfs;
	struct rswitch2_fwd_global_error global_error;
};
struct rswitch2_ipv_config {
	uint32_t ipv_update_enable;
	uint32_t ipv_value;


};

struct rswitch2_mirroring_config {
	uint32_t cpu_mirror_enable;
	uint32_t eth_mirror_enable;
	struct rswitch2_ipv_config    ipv_config;


};


struct rswitch2_destination_vector_config {
	uint32_t dest_eth_ports;
	uint32_t port_number[RENESAS_RSWITCH2_MAX_ETHERNET_PORTS];
	uint32_t cpu;

};

struct rswitch2_l2_fwd_mac_config_entry {

	uint32_t mac_learn_disable;
	uint32_t mac_dynamic_learn;
	uint32_t mac_security_learn;
	uint32_t                     mac[RSWITCH2_MAC_ID_LENGTH];
	struct  rswitch2_source_lock_config destination_source;
	struct  rswitch2_source_lock_config source_source;
	uint32_t csdn;
	struct rswitch2_mirroring_config mirroring_config;
	struct rswitch2_destination_vector_config    destination_vector_config;

};





struct rswitch2_l2_fwd_vlan_config_entry {

	uint32_t vlan_learn_disable;
	uint32_t vlan_security_learn;
	uint32_t vlan_id;
	struct  rswitch2_source_lock_config source_lock;
	uint32_t csdn;
	struct rswitch2_mirroring_config mirroring_config;
	struct rswitch2_destination_vector_config    destination_vector_config;
};

struct rswitch2_ipv_fwd_config_entry {
	uint32_t ipv_learn_disable;
	uint32_t ipv_dynamic_learn;
	uint32_t ipv_security_learn;
	enum rswitch2_ip_addr_type ip_addr_type;
	uint32_t ipaddr[RSWITCH2_IP_ADDR_LENGTH];
	struct  rswitch2_source_lock_config destination_source;
	struct  rswitch2_source_lock_config source_source;
	uint32_t routing_number;
	uint32_t csdn;
	struct rswitch2_mirroring_config mirroring_config;
	struct rswitch2_destination_vector_config    destination_vector_config;


};

struct   rswitch2_mac_aging_config {
	uint32_t bEnable;
	uint32_t on_off;
	uint32_t mac_aging_prescalar;
	uint32_t mac_aging_polling_mode;
	uint32_t mac_aging_security;
	uint32_t mac_aging_time_sec;

};


struct rswitch2_l2_fwd_config {
	uint32_t bEnable;
	uint32_t max_unsecure_hash_entry;
	uint32_t max_hash_collision;
	uint32_t mac_hash_eqn;
	struct   rswitch2_mac_aging_config mac_aging_config;
	uint32_t l2_fwd_mac_config_entries;
	struct rswitch2_l2_fwd_mac_config_entry l2_fwd_mac_config_entry[RSWITCH2_MAX_L2_FWD_ENTRIES];
	uint32_t max_unsecure_vlan_entry;
	uint32_t l2_fwd_vlan_config_entries;
	struct rswitch2_l2_fwd_vlan_config_entry l2_fwd_vlan_config_entry[RSWITCH2_MAX_L2_FWD_VLAN_ENTRIES];

};


struct rswitch2_ipv_fwd_config {

	uint32_t max_unsecure_hash_entry;
	uint32_t max_hash_collision;
	uint32_t ipv_hash_eqn;
	uint32_t ipv_fwd_config_entries;
	struct rswitch2_ipv_fwd_config_entry ipv_fwd_config_entry[RSWITCH2_MAX_IP_FWD_ENTRIES];


};

struct rswitch2_port_forwarding {
	uint32_t bEnable;
	enum rswitch2_enable_disable    force_frame_priority;
	enum rswitch2_enable_disable    ipv6_priority_decode;
	enum rswitch2_enable_disable    ipv4_priority_decode;
	enum rswitch2_priority_decode_mode  ipv4_priority_decode_mode;
	uint32_t security_learn;
	uint32_t csdn;
	struct rswitch2_mirroring_config mirroring_config;
	struct rswitch2_destination_vector_config    destination_vector_config;


};

struct rswitch2_fwd_gen_port_config {
	uint32_t portnumber;
	uint32_t cpu;
	uint32_t vlan_reject_unknown_secure;
	uint32_t vlan_reject_unknown;
	uint32_t vlan_search_active;
	uint32_t mac_hw_migration_active;
	uint32_t mac_hw_learn_active;
	uint32_t mac_reject_unknown_src_secure_addr;
	uint32_t mac_reject_unknown_src_addr;
	uint32_t mac_src_search_active;
	uint32_t mac_reject_unknown_dest_secure_addr;
	uint32_t mac_reject_unknown_dest_addr;
	uint32_t mac_dest_search_active;
	uint32_t ip_hw_migration_active;
	uint32_t ip_hw_learn_active;
	uint32_t ip_reject_unknown_src_secure_addr;
	uint32_t ip_reject_unknown_src_addr;
	uint32_t ip_src_search_active;
	uint32_t ip_reject_unknown_dest_secure_addr;
	uint32_t ip_reject_unknown_dest_addr;
	uint32_t ip_dest_search_active;
	uint32_t ipv6_extract_active;
	uint32_t ipv4_extract_active;
	uint32_t l2_stream_enabled;
	uint32_t ipv6_other_enabled;
	uint32_t ipv6_tcp_enabled;
	uint32_t ipv6_udp_enabled;
	uint32_t ipv4_other_enabled;
	uint32_t ipv4_tcp_enabled;
	uint32_t ipv4_udp_enabled;
	uint32_t l3_reject_unknown_secure_stream;
	uint32_t l3_reject_unknown_stream;
	uint32_t l3_tbl_active;
	struct rswitch2_port_forwarding port_forwarding;

};





struct rswitch2_fwd_gen_config {
	uint32_t vlan_mode;
	uint32_t ctag_tpid;
	uint32_t stag_tpid;
	uint32_t vlan_mode_config_only;
	uint32_t bEnable;
	struct rswitch2_fwd_gen_port_config fwd_gen_port_config[RENESAS_RSWITCH2_MAX_PORT_AGENT];



};

struct rswitch2_ipv4_stream_id {
	uint16_t unused1;
	uint8_t  unused2;
	uint8_t  unused3:5;
	uint8_t  frame_fmt:3;
	uint16_t stag;
	uint16_t ctag;
	uint16_t destination_port;
	uint16_t hash_value;
	uint32_t source_ip;
	uint32_t destination_ip;



} __packed;

struct rswitch2_ipv4_stream_fwd_entry {
	uint32_t destination_port;
	uint32_t source_port;
	uint32_t destination_ip_addr[RSWITCH2_IP_ADDR_LENGTH];
	uint32_t source_ip_addr[RSWITCH2_IP_ADDR_LENGTH];
	uint32_t                     dest_mac[RSWITCH2_MAC_ID_LENGTH];
	uint32_t                     src_mac[RSWITCH2_MAC_ID_LENGTH];
	uint32_t ctag_dei;
	uint32_t ctag_pcp;
	uint32_t ctag_vlan;
	uint32_t stag_dei;
	uint32_t stag_pcp;
	uint32_t stag_vlan;
	uint32_t security_learn;
	enum rswitch2_ipv4_stream_frame_format_code ipv4_stream_frame_format_code;
	struct  rswitch2_source_lock_config source_lock;
	uint32_t routing_number;
	uint32_t routing_valid;
	uint32_t csdn;
	uint32_t hash_value;
	struct rswitch2_ipv4_stream_id stream_id;
	uint32_t filter_number;
	uint32_t filter_enable;
	struct rswitch2_mirroring_config mirroring_config;
	struct rswitch2_destination_vector_config    destination_vector_config;

};

struct rswitch2_ipv4_hash_configuration {
	enum rswitch2_feature_include_exclude destination_port;
	enum rswitch2_feature_include_exclude source_port;
	enum rswitch2_feature_include_exclude destination_ip;
	enum rswitch2_feature_include_exclude source_ip;
	enum rswitch2_feature_include_exclude protocol;
	enum rswitch2_feature_include_exclude ctag_dei;
	enum rswitch2_feature_include_exclude ctag_pcp;
	enum rswitch2_feature_include_exclude ctag_vlan;
	enum rswitch2_feature_include_exclude stag_dei;
	enum rswitch2_feature_include_exclude stag_pcp;
	enum rswitch2_feature_include_exclude stag_vlan;
	enum rswitch2_feature_include_exclude dest_mac;
	enum rswitch2_feature_include_exclude src_mac;


};




struct rswitch2_ipv4_stream_fwd_config {
	uint32_t bEnable;
	struct rswitch2_ipv4_hash_configuration ipv4_hash_configuration;
	enum rswitch2_feature_include_exclude destination_port;
	enum rswitch2_feature_include_exclude destination_ip        ;
	enum rswitch2_feature_include_exclude source_ip;
	enum rswitch2_feature_include_exclude ctag_dei;
	enum rswitch2_feature_include_exclude ctag_pcp;
	enum rswitch2_feature_include_exclude ctag_vlan;
	enum rswitch2_feature_include_exclude stag_dei;
	enum rswitch2_feature_include_exclude stag_pcp;
	enum rswitch2_feature_include_exclude stag_vlan;
	uint32_t ipv4_stream_fwd_entries;
	struct rswitch2_ipv4_stream_fwd_entry ipv4_stream_fwd_entry[RSWITCH2_MAX_IPV4_STREAM_ENTRIES];




};

struct rswitch2_hash_configuration {
	uint32_t ipv4_hash_eqn;
	uint32_t ipv6_hash_eqn;

};

struct rswitch2_ipv4_ipv6_stream_config {
	struct rswitch2_ipv4_stream_fwd_config ipv4_stream_fwd_config;
	struct rswitch2_hash_configuration hash_configuration;

};

struct rswitch2_two_byte_filter {
	uint32_t filter_number;
	uint32_t offset;
	enum rswitch2_filter_offset_mode offset_mode;
	enum rswitch2_filter_unit_mode unit_mode;
	uint32_t value0;
	uint32_t value1;
	uint32_t filterid0;
	uint32_t filterid1;
	
};

struct rswitch2_three_byte_filter {
	uint32_t filter_number;
	uint32_t offset;
	enum rswitch2_filter_unit_mode unit_mode;
	uint32_t value0;
	uint32_t value1;
	uint32_t filterid0;
	uint32_t filterid1;
	
};

struct rswitch2_four_byte_filter {
	uint32_t filter_number;
	uint32_t offset;
	enum rswitch2_filter_unit_mode unit_mode;
	uint32_t value0;
	uint32_t value1;
	uint32_t filterid0;
	uint32_t filterid1;
	
};

struct rswitch2_range_filter {
	uint32_t filter_number;
	uint32_t offset;
	enum rswitch2_filter_offset_mode offset_mode;
	uint32_t range;
	uint32_t value0;
	uint32_t value1;
	uint32_t filterid0;
	uint32_t filterid1;
};

struct rswitch2_cascade_filter {
	uint32_t filter_number;
	struct rswitch2_destination_vector_config pframe_valid;
	struct rswitch2_destination_vector_config eframe_valid;
	uint32_t used_filter_ids;
	uint32_t used_filter_id_num[RSWITCH2_MAX_USED_CASCADE_FILTERS];
	
};

struct rswitch2_fwd_filter_config {
	uint32_t num_two_byte_filters;
	struct rswitch2_two_byte_filter two_byte[RSWITCH2_MAX_TWO_BYTE_FILTERS];
	uint32_t num_three_byte_filters;
	struct rswitch2_three_byte_filter three_byte[RSWITCH2_MAX_THREE_BYTE_FILTERS]; 
	uint32_t num_four_byte_filters;
	struct rswitch2_four_byte_filter four_byte[RSWITCH2_MAX_FOUR_BYTE_FILTERS];
	uint32_t num_range_filters;
	struct rswitch2_range_filter range[RSWITCH2_MAX_RANGE_FILTERS]; 
	uint32_t num_cascade_filters;
	struct rswitch2_cascade_filter cascade[RSWITCH2_MAX_CASCADE_FILTERS]; 
};

struct rswitch2_l3_stream_fwd {
	uint32_t bEnable;
	uint32_t max_l3_unsecure_entry;
	uint32_t max_l3_collision;
	uint32_t l3_hash_eqn;
	struct rswitch2_ipv4_ipv6_stream_config ipv4_ipv6_stream_config;
	struct rswitch2_fwd_filter_config fwd_filter_config;

};

struct rswitch2_l23_update_config  {
	uint32_t bEnable;
	struct rswitch2_destination_vector_config   routing_port_update;
	uint32_t routing_number;
	uint32_t rtag;
	enum rswitch2_enable_disable stag_dei_update;
	enum rswitch2_enable_disable stag_pcp_update;
	enum rswitch2_enable_disable stag_vlan_update;
	enum rswitch2_enable_disable ctag_dei_update;
	enum rswitch2_enable_disable ctag_pcp_update;
	enum rswitch2_enable_disable ctag_vlan_update;
	enum rswitch2_enable_disable src_mac_update;
	enum rswitch2_enable_disable dest_mac_update;
	enum rswitch2_enable_disable ttl_update;
	uint32_t  dest_mac[RSWITCH2_MAC_ID_LENGTH];
	uint32_t  stag_vlan;
	uint32_t  stag_pcp;
	uint32_t  stag_dei;
	uint32_t  ctag_vlan;
	uint32_t  ctag_pcp;
	uint32_t  ctag_dei;


};

struct rswitch2_l23_update {
	uint32_t entries;
	struct rswitch2_l23_update_config   l23_update_config[RSWITCH2_MAX_L2_3_UPDATE_ENTRIES];
};


struct rswitch2_mcast_entry {
	uint32_t entry_num;
	uint32_t base_entry;	
	uint32_t next_entries;
	uint32_t next_entry_num;
	uint32_t end_entry;
};


struct rswitch2_cpu_rx_mcast_config {
	uint32_t entries;
	struct rswitch2_mcast_entry entry[RSWITCH2_MAX_MCAST_CHAINS];
};

struct rswitch2_fwd_config {
	uint32_t config_done;
	struct rswitch2_fwd_gen_config    fwd_gen_config;
	struct rswitch2_l2_fwd_config l2_fwd_config;
	struct rswitch2_ipv_fwd_config ipv_fwd_config;
	struct rswitch2_l3_stream_fwd l3_stream_fwd;
	struct rswitch2_l23_update l23_update;
	struct rswitch2_cpu_rx_mcast_config rx_mcast_config;
};

struct rswitch2_eth_vlan_tag_config {
	uint32_t stag_dei;
	uint32_t stag_pcp;
	uint32_t stag_vlan;
	uint32_t ctag_dei;
	uint32_t ctag_pcp;
	uint32_t ctag_vlan;
};

struct rswitch2_eth_vlan_filtering {
	enum rswitch2_eth_frame_pass_reject    unknown_tag;
	enum rswitch2_eth_frame_pass_reject    scr_tag;
	enum rswitch2_eth_frame_pass_reject    sc_tag;
	enum rswitch2_eth_frame_pass_reject    cr_tag;
	enum rswitch2_eth_frame_pass_reject    c_tag;
	enum rswitch2_eth_frame_pass_reject    cosr_tag;
	enum rswitch2_eth_frame_pass_reject    cos_tag;
	enum rswitch2_eth_frame_pass_reject    r_tag;
	enum rswitch2_eth_frame_pass_reject    no_tag;
};

struct rswitch2_eth_vlan_config {
	uint32_t bEnable;
	enum rswitch2_eth_ingress_vlan_mode ingress_vlan_mode;
	enum rswitch2_eth_egress_vlan_mode egress_vlan_mode;
	struct rswitch2_eth_vlan_tag_config vlan_tag_config;
	struct rswitch2_eth_vlan_filtering vlan_filtering;
};

struct rswitch2_eth_port_config {
	uint32_t port_number;
        uint32_t cpu;
	struct rswitch2_eth_vlan_config eth_vlan_tag_config;

};

struct rswitch2_eth_config {
	uint32_t ports;
	struct rswitch2_eth_port_config eth_port_config[RENESAS_RSWITCH2_MAX_PORT_AGENT];

};

struct rswitch2_config {
	uint32_t chi_clk_mhz;
	struct rswitch2_eth_config eth_config;
	struct rswitch2_fwd_config fwd_config;
};
extern int rswitch2_fwd_init(struct ethports_config      *board_config);
extern int rswitch2_gwca_mcast_tbl_reset(void);
extern int rswitch2_fwd_exit(void);
extern void ptp_rswitch2_port_init(struct net_device *ndev, struct platform_device *pdev);
extern int ptp_rswitch2_init(struct platform_device *pdev);
extern int ptp_rswitch2_remove(struct platform_device *pdev);
#endif



