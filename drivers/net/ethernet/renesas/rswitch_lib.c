// SPDX-License-Identifier: GPL-2.0

#include <linux/etherdevice.h>
#include <linux/module.h>

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


struct rswitch_vport_frontend {
	struct rswitch_vport_desc *rvpd;

	struct net_device *ndev;
	struct napi_struct napi;

	struct rswitch_tx_ring *tx_ring;
	struct rswitch_rx_ring *rx_ring;

	spinlock_t lock;
};

#define RVP_READ(rvpd, field) \
	readl(rvpd->fe_reg_base + rvpd->field ## _offset)
#define RVP_WRITE(rvpd, field, val) \
	writel((val), rvpd->fe_reg_base + rvpd->field ## _offset)

static irqreturn_t rvpf_irq_handler(int irq, void *data)
{
	struct rswitch_vport_frontend *rvpf = data;
	struct rswitch_vport_desc *rvpd = rvpf->rvpd;
	u32 tx_status, rx_status;

	tx_status = RVP_READ(rvpd, tx_irq_status) & rvpd->tx_irq_bit;
	if (tx_status)
		RVP_WRITE(rvpd, tx_irq_status, tx_status);

	rx_status = RVP_READ(rvpd, rx_irq_status) & rvpd->rx_irq_bit;
	if (rx_status)
		RVP_WRITE(rvpd, rx_irq_status, rx_status);

	if (!tx_status && !rx_status)
		return IRQ_NONE;

	if (napi_schedule_prep(&rvpf->napi)) {
		spin_lock(&rvpf->lock);
		RVP_WRITE(rvpd, tx_irq_dis, rvpd->tx_irq_bit);
		RVP_WRITE(rvpd, rx_irq_dis, rvpd->rx_irq_bit);
		spin_unlock(&rvpf->lock);

		__napi_schedule(&rvpf->napi);
	}

	return IRQ_HANDLED;
}

static int rvpf_open(struct net_device *ndev)
{
	struct rswitch_vport_frontend *rvpf = netdev_priv(ndev);
	struct rswitch_vport_desc *rvpd = rvpf->rvpd;
	int ret;
	unsigned long flags;
	bool can_destroy_rings = true;

	rvpf->tx_ring = rswitch_tx_ring_create(rvpd->dma_dev, 256);
	if (!rvpf->tx_ring) {
		netdev_err(ndev, "could not create tx ring\n");
		ret = -ENOMEM;
		goto tx_ring_create_failed;
	}
	rvpd->fe_tx_ring_dma = rswitch_tx_ring_dma_addr(rvpf->tx_ring);

	rvpf->rx_ring = rswitch_rx_ring_create(rvpd->dma_dev, 256);
	if (!rvpf->rx_ring) {
		netdev_err(ndev, "could not create rx ring\n");
		ret = -ENOMEM;
		goto rx_ring_create_failed;
	}
	rvpd->fe_rx_ring_dma = rswitch_rx_ring_dma_addr(rvpf->rx_ring);

	ret = rvpd->ops->attach_frontend_rings(rvpd);
	if (ret) {
		netdev_err(ndev, "could not attach rings\n");
		goto attach_rings_failed;
	}

	ret = request_irq(rvpd->fe_irq, rvpf_irq_handler,
			IRQF_SHARED, netdev_name(ndev), rvpf);
	if (ret) {
		netdev_err(ndev, "could not request irq");
		goto request_irq_failed;
	}

	spin_lock_irqsave(&rvpf->lock, flags);
	RVP_WRITE(rvpd, tx_irq_ena, rvpd->tx_irq_bit);
	RVP_WRITE(rvpd, rx_irq_ena, rvpd->rx_irq_bit);
	spin_unlock_irqrestore(&rvpf->lock, flags);

	netif_start_queue(ndev);
	napi_enable(&rvpf->napi);

	return 0;

request_irq_failed:
	can_destroy_rings = (rvpd->ops->detach_frontend_rings(rvpd) == 0);
attach_rings_failed:
	if (can_destroy_rings)
		rswitch_rx_ring_destroy(rvpf->rx_ring);
rx_ring_create_failed:
	if (can_destroy_rings)
		rswitch_tx_ring_destroy(rvpf->tx_ring);
tx_ring_create_failed:
	return ret;
}

static int rvpf_close(struct net_device *ndev)
{
	struct rswitch_vport_frontend *rvpf = netdev_priv(ndev);
	struct rswitch_vport_desc *rvpd = rvpf->rvpd;
	unsigned long flags;
	bool can_destroy_rings;

	napi_disable(&rvpf->napi);
	netif_stop_queue(ndev);

	spin_lock_irqsave(&rvpf->lock, flags);
	RVP_WRITE(rvpd, tx_irq_dis, rvpd->tx_irq_bit);
	RVP_WRITE(rvpd, rx_irq_dis, rvpd->rx_irq_bit);
	spin_unlock_irqrestore(&rvpf->lock, flags);

	free_irq(rvpd->fe_irq, rvpf);

	can_destroy_rings = (rvpd->ops->detach_frontend_rings(rvpd) == 0);
	if (can_destroy_rings) {
		rswitch_rx_ring_destroy(rvpf->rx_ring);
		rswitch_tx_ring_destroy(rvpf->tx_ring);
	} else {
		netdev_warn(ndev, "rings not detached - skipping destroy\n");
	}

	return 0;
}

static int rvpf_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct rswitch_vport_frontend *rvpf = netdev_priv(ndev);
	struct rswitch_vport_desc *rvpd = rvpf->rvpd;
	int ret, ring_ret;

	ring_ret = rswitch_tx_ring_add_skb(rvpf->tx_ring, skb, 0, rvpd->info1_to_be);
	if (ring_ret == 0) {
		RVP_WRITE(rvpd, kick_tx, rvpd->kick_tx_val);
		ret = NETDEV_TX_OK;
	} else {
		if (ring_ret == -ENOSPC)
			netif_stop_subqueue(ndev, 0);
		ret = NETDEV_TX_BUSY;
	}

	return ret;
}

static int rvpf_napi_poll(struct napi_struct *napi, int budget)
{
	struct net_device *ndev = napi->dev;
	struct rswitch_vport_frontend *rvpf = netdev_priv(ndev);
	struct rswitch_vport_desc *rvpd = rvpf->rvpd;
	struct sk_buff *skb;
	int work_done = 0;
	unsigned long flags;

	if (rswitch_tx_ring_handle_complete(rvpf->tx_ring, &ndev->stats))
		netif_wake_subqueue(ndev, 0);

	while (work_done < budget) {
		skb = rswitch_rx_ring_process(rvpf->rx_ring, false);
		if (skb) {
			skb->protocol = eth_type_trans(skb, ndev);
			ndev->stats.rx_packets++;
			ndev->stats.rx_bytes += skb->len;
			napi_gro_receive(napi, skb);
		}

		work_done++;
		if (!rswitch_rx_ring_need_process(rvpf->rx_ring))
			break;
	}

	if (work_done < budget && napi_complete_done(napi, work_done)) {
		spin_lock_irqsave(&rvpf->lock, flags);
		RVP_WRITE(rvpd, tx_irq_ena, rvpd->tx_irq_bit);
		RVP_WRITE(rvpd, rx_irq_ena, rvpd->rx_irq_bit);
		spin_unlock_irqrestore(&rvpf->lock, flags);
	}

	return work_done;
}

static struct net_device_stats *rvpf_get_stats(struct net_device *ndev)
{
	return &ndev->stats;
}

static const struct net_device_ops rvpf_netdev_ops = {
	.ndo_open = rvpf_open,
	.ndo_stop = rvpf_close,
	.ndo_start_xmit = rvpf_start_xmit,
	.ndo_get_stats = rvpf_get_stats,
};

int rswitch_vport_frontend_run(struct rswitch_vport_desc *rvpd)
{
	struct net_device *ndev;
	struct rswitch_vport_frontend *rvpf;
	int ret;

	ndev = alloc_etherdev(sizeof(*rvpf));
	if (!ndev) {
		printk(KERN_ERR "could not alloc frontend netdev\n");
		return -ENOMEM;
	}

	SET_NETDEV_DEV(ndev, rvpd->dma_dev);
	ndev->netdev_ops = &rvpf_netdev_ops;
	eth_hw_addr_random(ndev);

	rvpf = netdev_priv(ndev);
	rvpf->ndev = ndev;
	rvpf->rvpd = rvpd;
	spin_lock_init(&rvpf->lock);

	ret = rvpd->ops->vport_register(rvpd);
	if (ret) {
		printk(KERN_ERR "could not register rswitch vport\n");
		goto rvp_register_failed;
	}

	snprintf(ndev->name, IFNAMSIZ, "rvpfront%d", rvpd->id);
	netif_napi_add(ndev, &rvpf->napi, rvpf_napi_poll, 64);

	ret = register_netdev(ndev);
	if (ret) {
		netdev_err(ndev, "could not register\n");
		goto netdev_register_failed;
	}

	rvpd->rvpf = rvpf;
	return 0;

netdev_register_failed:
	netif_napi_del(&rvpf->napi);
	rvpd->ops->vport_unregister(rvpd);
rvp_register_failed:
	free_netdev(ndev);
	return ret;
}
EXPORT_SYMBOL(rswitch_vport_frontend_run);

void rswitch_vport_frontend_stop(struct rswitch_vport_desc *rvpd)
{
	struct rswitch_vport_frontend *rvpf = rvpd->rvpf;

	unregister_netdev(rvpf->ndev);
	netif_napi_del(&rvpf->napi);
	rvpd->ops->vport_unregister(rvpd);
	free_netdev(rvpf->ndev);
}
EXPORT_SYMBOL(rswitch_vport_frontend_stop);

MODULE_LICENSE("GPL v2");
