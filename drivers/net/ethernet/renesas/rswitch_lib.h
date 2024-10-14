// SPDX-License-Identifier: GPL-2.0

#ifndef RSWITCH_LIB_H
#define RSWITCH_LIB_H

#include <linux/if_ether.h>
#include <linux/netdevice.h>


struct rswitch_tx_ring;
struct rswitch_rx_ring;
struct device;
struct sk_buff;
struct net_device_stats;

/* Allocate and pre-format tx ring with (size) extended descriptors and
 * a LINKFIX descriptor at the end pointing back to the ring start */
struct rswitch_tx_ring *rswitch_tx_ring_create(struct device *map_dev,
		unsigned int size);

/* Allocate and pre-format rx ring with (size) extended timestamp discriptors
 * and a LINKFIX descriptor at the end pointing back to the ring start.
 *
 * Allocate a receive buffer for each entry, and make the entries point
 * to the buffers.
 */
struct rswitch_rx_ring *rswitch_rx_ring_create(struct device *map_dev,
		unsigned int size);

/* Deallocate TX ring.
 *
 * Caller must ensure that the ring is no longer used by hardware.
 */
void rswitch_tx_ring_destroy(struct rswitch_tx_ring *ring);

/* Deallocate RX ring and any buffers it currently points to.
 *
 * Caller must ensure that the ring is no longer used by the hardware.
 */
void rswitch_rx_ring_destroy(struct rswitch_rx_ring *ring);

/* Return dma_addr_t for the allocated ring */
dma_addr_t rswitch_tx_ring_dma_addr(struct rswitch_tx_ring *ring);
dma_addr_t rswitch_rx_ring_dma_addr(struct rswitch_rx_ring *ring);

/* Map skb and place it into the TX ring.
 *
 * The given info0 and info1 values are put into the filled descriptors
 * (the first one for multi-descriptor case).
 *
 * Returns 0 on success, -ENOSPC on no room in the ring, and -ENOMEM on failure
 * to map skb.
 *
 * On successful return, skb ownership is transferred to the library,
 * it will be freed at transfer completion. It is caller's responsibility
 * to trigger the ring processing (although the new skb can get processed
 * immediately if the hardware is already processing the ring).
 *
 * On error return, skb ownership is not transferred. It is caller's
 * responsibility to throttle netdev queue processing.
 *
 * For TX timestamping, caller shall fill bits in info1, setup future timestamp
 * queue handling, and undo that in case of error return. This function will
 * only set SKBTX_IN_PROGRESS and call skb_tx_timestamp() just before making
 * new descriptor(s) visible to hardware.
 *
 * Locking is CALLER responsibility.
 */
int rswitch_tx_ring_add_skb(struct rswitch_tx_ring *ring, struct sk_buff *skb,
		u8 info0, u64 info1);

/* Handle TX completions, frees skbs, update stats.
 *
 * Return true if there was some progress.
 *
 * Locking is CALLER responsibility.
 */
bool rswitch_tx_ring_handle_complete(struct rswitch_tx_ring *ring,
		struct net_device_stats *stats);

/* Extract skb from RX ring, if available, and refill the ring with more
 * buffers, if possible.
 *
 * Locking is CALLER responsibility.
 */
struct sk_buff *rswitch_rx_ring_process(struct rswitch_rx_ring *ring,
		bool handle_ts);

/* Check if RX ring processing is needed.
 *
 * Locking is CALLER responsibility.
 */
bool rswitch_rx_ring_need_process(struct rswitch_rx_ring *ring);


struct rswitch_private;
struct rswitch_vport_backend;
struct rswitch_vport_frontend;

struct rswitch_vport_desc;
struct rswitch_vport_desc_ops {
	int (*vport_register)(struct rswitch_vport_desc *rvpd);
	void (*vport_unregister)(struct rswitch_vport_desc *rvpd);

	int (*attach_frontend_rings)(struct rswitch_vport_desc *rvpd);
	int (*detach_frontend_rings)(struct rswitch_vport_desc *rvpd);
};

struct rswitch_vport_desc {
	const struct rswitch_vport_desc_ops *ops;

	/* Requested vport id, or RSWITCH_VPORT_ID_DYNAMIC for allocation */
	unsigned int id;
#define RSWITCH_VPORT_ID_DYNAMIC  ~(0u)

	/* GWCA interrupt index to deliver frontend interrupts */
	unsigned int irq_index;

	/* OSID for GWCA operation with frontend chains */
	unsigned int osid;

	/* Rswitch register base, valid at frontend side
	 * (can be set to NULL if frontend driver and rswitch driver are in the
	 * same kernel) */
	void __iomem *fe_reg_base;

	/* Interrupt number to deliver frontend interrupts, requestable at
	 * frontend side
	 * (can be set to -1 if frontend driver and rswitch driver are in the
	 * same kernel) */
	int fe_irq;

	/* Device struct for DMA operations on frontend chains
	 * (can be set to NULL if frontend driver and rswitch driver are in the
	 * same kernel) */
	struct device *dma_dev;

	/* Rswitch driver's private pointer */
	struct rswitch_vport_backend *rvpb;

	/* Rswitch lib's private pointer */
	struct rswitch_vport_frontend *rvpf;

	/* Frontend driver's private pointer */
	void *fe_drv_priv;

	/* Frontend Tx and Rx DMA addresses, accessible via the above OSID
	 * - filled by lib, used by rswitch driver */
	dma_addr_t fe_tx_ring_dma;
	dma_addr_t fe_rx_ring_dma;

	/* Operation details - filled by rswitch driver, used by lib */

	u64 info1_to_be;	/* info1 for sending to backend */
	u64 info1_to_fwd;	/* info1 for sending to switch forwarder */

	u32 kick_tx_offset;	/* register to kick tx ring */
	u32 kick_tx_val;	/* value to kick tx ring */

	u32 tx_irq_ena_offset, rx_irq_ena_offset;
	u32 tx_irq_dis_offset, rx_irq_dis_offset;
	u32 tx_irq_status_offset, rx_irq_status_offset;
	u32 tx_irq_bit, rx_irq_bit;

	u8 mac_addr[ETH_ALEN];
};

/* Provide full lifecycle of vport frontend device */
int rswitch_vport_frontend_run(struct rswitch_vport_desc *rvpd);
void rswitch_vport_frontend_stop(struct rswitch_vport_desc *rvpd);


/* Rswitch driver exports */

/* Vport registration
 * (FIXME: support multiple rswitch instances)*/
int rswitch_vport_register(struct rswitch_vport_desc *rvpd);
void rswitch_vport_unregister(struct rswitch_vport_desc *rvpd);

int rswitch_vport_attach_frontend_rings(struct rswitch_vport_desc *rvpd);
int rswitch_vport_detach_frontend_rings(struct rswitch_vport_desc *rvpd);

#endif
