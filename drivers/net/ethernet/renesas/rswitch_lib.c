// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/netdevice.h>

#include "rswitch_desc.h"
#include "rswitch_lib.h"

struct rswitch_tx_ring {
	unsigned int size;
	unsigned int head;
	unsigned int tail;
	struct device *map_dev;
	struct rswitch_ext_desc *desc;
	dma_addr_t desc_dma;
	struct sk_buff *skb[0];
};

struct rswitch_rx_ring {
	unsigned int size;
	unsigned int read_pos;
	unsigned int alloc_pos;
	struct device *map_dev;
	struct rswitch_ext_ts_desc *desc;
	dma_addr_t desc_dma;

	struct sk_buff *skb;
	size_t skb_total_len;

	void *rx_buf[0];
};

#define TX_RING_DESC_SIZE(size) \
	(size + 1) * sizeof(struct rswitch_ext_desc)
#define RX_RING_DESC_SIZE(size) \
	(size + 1) * sizeof(struct rswitch_ext_ts_desc)

#define INC_RING_POS(ring, pos) do {	\
	pos++;				\
	if (pos == ring->size)		\
		pos = 0;		\
} while (0)


struct rswitch_tx_ring *rswitch_tx_ring_create(struct device *map_dev,
		unsigned int size)
{
	struct rswitch_tx_ring *ring;
	unsigned int i;

	ring = kzalloc(struct_size(ring, skb, size), GFP_KERNEL);
	if (!ring)
		return NULL;

	ring->size = size;
	ring->map_dev = map_dev;

	ring->desc = dma_alloc_coherent(map_dev, TX_RING_DESC_SIZE(size),
			&ring->desc_dma, GFP_KERNEL);
	if (!ring->desc) {
		kfree(ring);
		return NULL;
	}

	for (i = 0; i < size; i++)
		ring->desc[i].die_dt = DT_EEMPTY;

	ring->desc[size].die_dt = DT_LINKFIX;
	ring->desc[size].dptrl = cpu_to_le32(lower_32_bits(ring->desc_dma));
	ring->desc[size].dptrh = upper_32_bits(ring->desc_dma);

	return ring;
}
EXPORT_SYMBOL(rswitch_tx_ring_create);

dma_addr_t rswitch_tx_ring_dma_addr(struct rswitch_tx_ring *ring)
{
	return ring->desc_dma;
}
EXPORT_SYMBOL(rswitch_tx_ring_dma_addr);

static bool map_tx_desc(struct rswitch_tx_ring *ring, unsigned int pos,
		void *data, u16 size, u8 info0)
{
	dma_addr_t dma_addr;

	dma_addr = dma_map_single(ring->map_dev, data, size, DMA_TO_DEVICE);
	if (dma_mapping_error(ring->map_dev, dma_addr))
		return false;

	ring->desc[pos].dptrl = cpu_to_le32(lower_32_bits(dma_addr));
	ring->desc[pos].dptrh = upper_32_bits(dma_addr);
	ring->desc[pos].info_ds = cpu_to_le16(size | (info0 << 12));

	return true;
}

static void unmap_tx_desc(struct rswitch_tx_ring *ring, unsigned int pos)
{
	dma_addr_t dma_addr;
	u16 size;

	dma_addr = le32_to_cpu(ring->desc[pos].dptrl);
	dma_addr |= ((dma_addr_t)ring->desc[pos].dptrh) << 32;
	size = le16_to_cpu(ring->desc[pos].info_ds) & TX_DS;

	dma_unmap_single(ring->map_dev, dma_addr, size, DMA_TO_DEVICE);

	ring->desc[pos].die_dt = DT_EEMPTY;
}

int rswitch_tx_ring_add_skb(struct rswitch_tx_ring *ring, struct sk_buff *skb,
		u8 info0, u64 info1)
{
	int have_desc, num_desc, i;
	unsigned int pos, pos2;
	void *data;
	u16 chunk;

	if (skb_put_padto(skb, ETH_ZLEN)) {
		printk("XXX can't pad\n");
		return 0;	/* skb_put_padto() frees skb on error */
	}

	have_desc = (ring->head >= ring->tail) ?
		ring->size - ring->head + ring->tail :
		ring->tail - ring->head;
	num_desc = (skb->len + RSWITCH_MAX_DESC_SZ - 1) / RSWITCH_MAX_DESC_SZ;
	if (have_desc < num_desc)
		return -ENOSPC;

	data = skb->data;
	chunk = skb->len % RSWITCH_MAX_DESC_SZ;

	if (!map_tx_desc(ring, ring->head, data, chunk, info0))
		return -ENOMEM;

	for (i = 1, pos = ring->head; i < num_desc; i++) {
		INC_RING_POS(ring, pos);
		data += chunk;
		chunk = RSWITCH_MAX_DESC_SZ;

		if (!map_tx_desc(ring, pos, data, chunk, 0)) {
			pos2 = ring->head;
			while (pos2 != pos) {
				unmap_tx_desc(ring, pos2);
				INC_RING_POS(ring, pos2);
			}
			return -ENOMEM;
		}

		if (i == num_desc - 1)
			ring->desc[pos].die_dt = DT_FEND | DIE;
		else
			ring->desc[pos].die_dt = DT_FMID;
	}

	ring->desc[ring->head].info1 = cpu_to_le64(info1);
	ring->skb[pos] = skb;	/* store in last entry */

	if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		skb_tx_timestamp(skb);
	}

	dma_wmb();

	if (num_desc > 1) {
		ring->desc[ring->head].die_dt = DT_FSTART;
		ring->head = pos;
	} else
		ring->desc[ring->head].die_dt = DT_FSINGLE | DIE;

	INC_RING_POS(ring, ring->head);
	return 0;
}
EXPORT_SYMBOL(rswitch_tx_ring_add_skb);

bool rswitch_tx_ring_handle_complete(struct rswitch_tx_ring *ring,
		struct net_device_stats *stats)
{
	struct sk_buff *skb;
	bool ret = false;

	while ((ring->desc[ring->tail].die_dt & DT_MASK) == DT_FEMPTY) {
		ret = true;
		unmap_tx_desc(ring, ring->tail);
		skb = ring->skb[ring->tail];
		if (skb) {
			if (stats) {
				stats->tx_packets++;
				stats->tx_bytes += skb->len;
			}
			dev_kfree_skb_any(skb);
			ring->skb[ring->tail] = NULL;
		}
		INC_RING_POS(ring, ring->tail);
	}

	return ret;
}
EXPORT_SYMBOL(rswitch_tx_ring_handle_complete);

void rswitch_tx_ring_destroy(struct rswitch_tx_ring *ring)
{
	struct sk_buff *skb;

	while (ring->desc[ring->tail].die_dt != DT_EEMPTY) {
		unmap_tx_desc(ring, ring->tail);
		skb = ring->skb[ring->tail];
		if (skb) {
			dev_kfree_skb(skb);
			ring->skb[ring->tail] = NULL;
		}
		INC_RING_POS(ring, ring->tail);
	}

	dma_free_coherent(ring->map_dev, TX_RING_DESC_SIZE(ring->size),
			ring->desc, ring->desc_dma);
	kfree(ring);
}
EXPORT_SYMBOL(rswitch_tx_ring_destroy);

static bool alloc_map_rx_buf(struct rswitch_rx_ring *ring, unsigned int pos)
{
	dma_addr_t dma_addr;

	if (!ring->rx_buf[pos])
		ring->rx_buf[pos] = netdev_alloc_frag(RSWITCH_RX_BUF_SIZE);
	if (!ring->rx_buf[pos])
		return false;

	dma_addr = dma_map_single(ring->map_dev,
			ring->rx_buf[pos] + NET_SKB_PAD + NET_IP_ALIGN,
			RSWITCH_RX_BUF_SIZE - NET_SKB_PAD - NET_IP_ALIGN,
			DMA_FROM_DEVICE);
	if (dma_mapping_error(ring->map_dev, dma_addr))
		return false;

	ring->desc[pos].dptrl = cpu_to_le32(lower_32_bits(dma_addr));
	ring->desc[pos].dptrh = upper_32_bits(dma_addr);
	ring->desc[pos].info_ds = cpu_to_le16(RSWITCH_MAX_DESC_SZ);
	dma_wmb();
	ring->desc[pos].die_dt = DT_FEMPTY | DIE;

	return true;
}

static void unmap_rx_buf(struct rswitch_rx_ring *ring, unsigned int pos)
{
	dma_addr_t dma_addr;

	dma_addr = le32_to_cpu(ring->desc[pos].dptrl);
	dma_addr |= ((dma_addr_t)ring->desc[pos].dptrh) << 32;

	dma_unmap_single(ring->map_dev, dma_addr,
			RSWITCH_RX_BUF_SIZE - NET_SKB_PAD - NET_IP_ALIGN,
			DMA_FROM_DEVICE);

	/* mark unmapped entry with DT_EEMPTY */
	ring->desc[pos].die_dt = DT_EEMPTY;
}

static void free_rx_buf(struct rswitch_rx_ring *ring, unsigned int pos)
{
	if (ring->rx_buf[pos]) {
		skb_free_frag(ring->rx_buf[pos]);
		ring->rx_buf[pos] = NULL;
	}
}

struct rswitch_rx_ring *rswitch_rx_ring_create(struct device *map_dev,
		unsigned int size)
{
	struct rswitch_rx_ring *ring;
	unsigned int i;

	ring = kzalloc(struct_size(ring, rx_buf, size), GFP_KERNEL);
	if (!ring)
		goto alloc_ring_failed;

	ring->size = size;
	ring->map_dev = map_dev;

	ring->desc = dma_alloc_coherent(map_dev, RX_RING_DESC_SIZE(size),
			&ring->desc_dma, GFP_KERNEL);
	if (!ring->desc)
		goto alloc_desc_failed;

	for (i = 0; i < size; i++) {
		if (!alloc_map_rx_buf(ring, i))
			goto alloc_map_buf_failed;
	}

	ring->desc[size].die_dt = DT_LINKFIX;
	ring->desc[size].dptrl = cpu_to_le32(lower_32_bits(ring->desc_dma));
	ring->desc[size].dptrh = upper_32_bits(ring->desc_dma);

	return ring;

	do {
		i--;
		unmap_rx_buf(ring, i);
alloc_map_buf_failed:
		free_rx_buf(ring, i);
	} while (i > 0);

alloc_desc_failed:
	kfree(ring);
alloc_ring_failed:
	return NULL;
}
EXPORT_SYMBOL(rswitch_rx_ring_create);

dma_addr_t rswitch_rx_ring_dma_addr(struct rswitch_rx_ring *ring)
{
	return ring->desc_dma;
}
EXPORT_SYMBOL(rswitch_rx_ring_dma_addr);

struct sk_buff *rswitch_rx_ring_process(struct rswitch_rx_ring *ring,
		bool handle_ts)
{
	struct sk_buff *ret_skb = NULL;
	struct rswitch_ext_ts_desc *desc;
	int dt;
	void *buf;
	u16 size;

	while (!ret_skb) {
		desc = &ring->desc[ring->read_pos];
		dt = desc->die_dt & DT_MASK;
		if (dt == DT_FEMPTY || dt == DT_EEMPTY)
			break;

		unmap_rx_buf(ring, ring->read_pos);
		dma_rmb();

		buf = ring->rx_buf[ring->read_pos];
		size = le16_to_cpu(desc->info_ds) & RX_DS;

		switch (dt) {
		case DT_FSINGLE:
		case DT_FSTART:
			if (ring->skb)
				dev_kfree_skb_any(ring->skb);
			ring->skb = build_skb(buf, RSWITCH_RX_BUF_SIZE);
			if (!ring->skb)
				break;
			skb_checksum_none_assert(ring->skb);
			skb_reserve(ring->skb, NET_SKB_PAD + NET_IP_ALIGN);
			skb_put(ring->skb, size);
			ring->skb_total_len = size;
			ring->rx_buf[ring->read_pos] = NULL;
			if (dt == DT_FSINGLE) {
				ret_skb = ring->skb;
				ring->skb = NULL;
			}
			break;

		case DT_FMID:
		case DT_FEND:
			if (!ring->skb)
				break;
			skb_add_rx_frag(ring->skb,
					skb_shinfo(ring->skb)->nr_frags,
					virt_to_page(buf),
					offset_in_page(buf) + NET_SKB_PAD + NET_IP_ALIGN,
					size, RSWITCH_RX_BUF_SIZE);
			ring->skb_total_len += size;
			ring->rx_buf[ring->read_pos] = NULL;
			if (dt == DT_FEND) {
				ret_skb = ring->skb;
				ring->skb = NULL;
			}
			break;
		}

		if (ret_skb && handle_ts) {
			struct skb_shared_hwtstamps *shhwtstamps;
			struct timespec64 ts;

			shhwtstamps = skb_hwtstamps(ret_skb);
			memset(shhwtstamps, 0, sizeof(*shhwtstamps));
			ts.tv_sec = (u64)le32_to_cpu(desc->ts_sec);
			ts.tv_nsec = le32_to_cpu(desc->ts_nsec & 0x3FFFFFFF);
			shhwtstamps->hwtstamp = timespec64_to_ktime(ts);
		}

		INC_RING_POS(ring, ring->read_pos);
	}

	while ((ring->desc[ring->alloc_pos].die_dt & DT_MASK) == DT_EEMPTY) {
		if (!alloc_map_rx_buf(ring, ring->alloc_pos))
			break;
		INC_RING_POS(ring, ring->alloc_pos);
	}

	return ret_skb;
}
EXPORT_SYMBOL(rswitch_rx_ring_process);

bool rswitch_rx_ring_need_process(struct rswitch_rx_ring *ring)
{
	return (ring->desc[ring->read_pos].die_dt & DT_MASK) != DT_FEMPTY ||
	       (ring->desc[ring->alloc_pos].die_dt & DT_MASK) == DT_EEMPTY;
}
EXPORT_SYMBOL(rswitch_rx_ring_need_process);

void rswitch_rx_ring_destroy(struct rswitch_rx_ring *ring)
{
	unsigned int i;

	if (ring->skb)
		dev_kfree_skb_any(ring->skb);

	for (i = 0; i < ring->size; i++) {
		if ((ring->desc[i].die_dt & DT_MASK) != DT_EEMPTY)
			unmap_rx_buf(ring, i);
		free_rx_buf(ring, i);
	}

	dma_free_coherent(ring->map_dev, RX_RING_DESC_SIZE(ring->size),
			ring->desc, ring->desc_dma);
	kfree(ring);
}
EXPORT_SYMBOL(rswitch_rx_ring_destroy);

MODULE_LICENSE("GPL v2");
