#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <asm/div64.h>

/* Global register offsets */
#define CTL0		0x00
#define CTL1		0x04
#define CTL2		0x08
#define STR0		0x10

/* Interrupt register offsets */
#define K_OFFSET(k, reg)	((k) * 0x30 + reg)
#define INTFk(k)	K_OFFSET(k, 0x00)
#define INTMSKk(k)	K_OFFSET(k, 0x04)
#define INTFCk(k)	K_OFFSET(k, 0x08)

/* Channel register offsets */
#define M_OFFSET(m, reg)	(((m) + 1) * 0x80 + (reg))
#define CFGm0(m)	M_OFFSET(m, 0x00)
#define CFGm1(m)	M_OFFSET(m, 0x04)
#define CFGm2(m)	M_OFFSET(m, 0x08)
#define CFGm3(m)	M_OFFSET(m, 0x0c)
#define CFGm4(m)	M_OFFSET(m, 0x10)
#define SEUPm(m)	M_OFFSET(m, 0x18)
#define HOLDm(m)	M_OFFSET(m, 0x1c)
#define IDLEm(m)	M_OFFSET(m, 0x20)
#define INDAm(m)	M_OFFSET(m, 0x24)
#define RASTADm(m)	M_OFFSET(m, 0x28)
#define CFSETm(m)	M_OFFSET(m, 0x2c)
#define SSTXm(m)	M_OFFSET(m, 0x30)
#define CSTSm(m)	M_OFFSET(m, 0x34)
#define CSTCm(m)	M_OFFSET(m, 0x38)
#define SSELm(m)	M_OFFSET(m, 0x3c)
#define TXDAm0(m)	M_OFFSET(m, 0x40)
#define FRERSTm(m)	M_OFFSET(m, 0x4c)
#define RXDAm0(m)	M_OFFSET(m, 0x50)
#define CSTRm(m)	M_OFFSET(m, 0x58)
#define CFCNTm(m)	M_OFFSET(m, 0x5c)
#define FSRXm(m)	M_OFFSET(m, 0x60)
#define FRERSTCm(m)	M_OFFSET(m, 0x64)
#define CESTm(m)	M_OFFSET(m, 0x68)
#define CESTCm(m)	M_OFFSET(m, 0x6c)

#define CTL0_EN			BIT(0)

#define CTL1_CSP_BIT(cs)	BIT(cs)

#define CFGm0_TXE		BIT(29)
#define CFGm0_RXE		BIT(28)
#define CFGm0_FIFO_MODE		(2 << 24)

#define CFGm1_CPOL		BIT(25)
#define CFGm1_CPHA		BIT(24)
#define CFGm1_DIR_LSB_FIRST	BIT(20)
#define CFGm1_FIDL		BIT(17)
#define CFGm1_CSRI		BIT(16)

#define CFGm3_PRCS_SHIFT	8
#define CFGm3_CDIV_SHIFT	0

#define CFGm4_8_STAGE_FIFO	(0 << 0)
#define CFGm4_16_STAGE_FIFO	(1 << 0)
#define CFGm4_32_STAGE_FIFO	(2 << 0)

#define CSTSm_ACTFS		BIT(1)
#define CSTSm_CHENS		BIT(0)

#define CSTCm_CHENC		BIT(0)

#define SSELm_CSR_BIT(cs)	BIT(cs)

#define CSTRm_FIRXN(val)	(((val) >> 8) & 0xff)
#define CSTRm_TXRQF		BIT(5)
#define CSTRm_RXRQF		BIT(4)
#define CSTRm_ACTF		BIT(1)
#define CSTRm_CHEN		BIT(0)

#define MSPI_MAX_SPEED_HZ	(20 * 1000 * 1000)

#define MSPI_MIN_FRAME_LEN	2
#define MSPI_MAX_FRAME_LEN	128

struct mspi {
	struct spi_controller *ctlr;
	void __iomem *regs;

	struct clk *clk;
	unsigned long clk_rate;

	u32 ctl1;

	unsigned long last_update_dividers_rate;
	u16 cfg3;
	unsigned int clk_ticks_per_bit;
};

static inline void mspi_write8(struct mspi *mspi, unsigned int offset, u8 val)
{
	iowrite8(val, mspi->regs + offset);
	//printk("[%04x] <- %02x\n", offset, val);
}

static inline u8 mspi_read8(struct mspi *mspi, unsigned int offset)
{
	u8 ret = ioread8(mspi->regs + offset);
	//printk("[%04x] -> %02x\n", offset, ret);
	return ret;
}

static inline void mspi_write16(struct mspi *mspi, unsigned int offset, u16 val)
{
	iowrite16(val, mspi->regs + offset);
	//printk("[%04x] <- %04x\n", offset, val);
}

static inline u16 mspi_read16(struct mspi *mspi, unsigned int offset)
{
	u16 ret = ioread16(mspi->regs + offset);
	//printk("[%04x] -> %04x\n", offset, ret);
	return ret;
}

static inline void mspi_write32(struct mspi *mspi, unsigned int offset, u32 val)
{
	iowrite32(val, mspi->regs + offset);
	//printk("[%04x] <- %08x\n", offset, val);
}

static inline u32 mspi_read32(struct mspi *mspi, unsigned int offset)
{
	u32 ret = ioread32(mspi->regs + offset);
	//printk("[%04x] -> %08x\n", offset, ret);
	return ret;
}

static unsigned int xfer_frame_len(struct spi_transfer *xfer)
{
	return xfer->bits_per_word;
}

static unsigned int xfer_frame_storage_bytes(struct spi_transfer *xfer)
{
	unsigned int frame_len = xfer_frame_len(xfer);

	if (frame_len <= 8)
		return 1;
	else if (frame_len <= 16)
		return 2;
	else
		return 4 * DIV_ROUND_UP(frame_len, 32);
}

static unsigned int xfer_frame_count(struct spi_transfer *xfer)
{
	return xfer->len / xfer_frame_storage_bytes(xfer);
}

static inline u32 mspi_delay_to_ticks(struct mspi *mspi, struct spi_delay *delay)
{
	u32 nsec;
	u64 x;
	u32 rem;

	if (delay->value == 0)
		return 0;

	if (delay->unit == SPI_DELAY_UNIT_USECS)
		nsec = delay->value * 1000;
	else if (delay->unit == SPI_DELAY_UNIT_NSECS)
		nsec = delay->value;
	else if (delay->unit == SPI_DELAY_UNIT_SCK)
		return delay->value;
	else
		return 0;

	/* tick is (1_000_000_000 / clk_rate) nsecs
	 * result is (nsec / tick) = (nsec * clk_rate) / 1_000_000_000 */
	x = nsec * (u64)mspi->clk_rate;
	rem = do_div(x, 1000 * 1000 * 1000);
	return rem ? (x + 1) : x;
}

static int mspi_update_dividers(struct mspi *mspi, unsigned long rate)
{
	unsigned long x;
	unsigned int prcs, cdiv, prcs_div;

	if (rate == 0)
		return -EINVAL;
	if (rate == mspi->last_update_dividers_rate)
		return 0;

	/* From CFGm3 register docs:
	 *   rate = clk_rate / (4^prcs * cdiv * 2)
	 * Needs to find prcs and cdiv, such that
	 *   0 <= prcs <= 3
	 *   1 <= cdiv <= 31
	 *   rate calculated as above is as close as requested, but does not
	 *     exceed it.
	 *
	 * Lower prcs is, better accuracy of actual_rate becomes possible.
	 *
	 * Start with prcs = 0 and calculate cdiv needed to not exceed requested
	 * rate. If that is in range, accept that solution. Otherwise, try
	 * increasing prcs.
	 */

	prcs = 0;
	x = mspi->clk_rate / 2;		/* x is clk_rate / (4^prcs * 2) */
	prcs_div = 1;			/* prcs_div is 4^prcs */
	do {
		if (unlikely(x == 0))
			return -EINVAL;
		cdiv = DIV_ROUND_UP(x, rate);
		if (cdiv <= 31)
			break;
		if (prcs == 3)
			return -EINVAL;
		prcs++;
		x /= 4;
		prcs_div *= 4;
	} while (1);

	mspi->last_update_dividers_rate = rate;
	mspi->cfg3 = (prcs << CFGm3_PRCS_SHIFT) | (cdiv << CFGm3_CDIV_SHIFT);
	mspi->clk_ticks_per_bit = 2 * prcs_div * cdiv;
	//printk("SPI cfg3=%x  max-rate=%ld kHz  new-rate=%ld kHz  (%lx)\n",
	//	mspi->cfg3, rate/1000, mspi->clk_rate/mspi->clk_ticks_per_bit/1000, (long int)mspi->regs);
	return 0;
}

static int mspi_setup(struct spi_device *spi)
{
	struct mspi *mspi = spi_controller_get_devdata(spi->controller);

	/* FIXME: It is only possible to update hardware CS polatiry in CTL1 reg
	 *        when communication is disabled. This suggests that first
	 *        setup() for all devices on the bus shall be called, and only
	 *        then any communication starts. This somehow contradicts the
	 *        probe architecture under linux, where drivers are probed
	 *        dynamically and independent of each other, and each can start
	 *        communication immediately after probe.
	 *
	 *        Current solution is: write to CTL1 in prepare hardware call.
	 *        Possible drawbacks:
	 *        - physical state of CS signal is left unconfigured until then
	 *          (but, it was unconfigured before calling setup() and thus
	 *          keeping in unconfigured a bit more is not doing worse)
	 *        - if other device on the same bus is actively communicating,
	 *          there can be no new prepare call before this device starts
	 *          to communicate. But, this is hard to resolve cleanly,
	 *          because can't write to CTL1 without disabling the bus,
	 *          and temporary bus disable will break logic elsewhere, e.g.
	 *          keeping CS between messages for 'cs_change' flag
	 */

	if (spi->mode & SPI_CS_HIGH)
		mspi->ctl1 |= CTL1_CSP_BIT(spi->chip_select);
	else
		mspi->ctl1 &= ~CTL1_CSP_BIT(spi->chip_select);

	return 0;
}

static int mspi_prepare_transfer_hardware(struct spi_controller *ctlr)
{
	struct mspi *mspi = spi_controller_get_devdata(ctlr);
	unsigned long clk_rate;
	int ret;

	ret = clk_prepare_enable(mspi->clk);
	if (ret) {
		dev_err(&ctlr->dev, "could not enable SPI clock\n");
		return ret;
	}
	clk_rate = clk_get_rate(mspi->clk);
	if (clk_rate != mspi->clk_rate) {
		mspi->clk_rate = clk_rate;
		mspi->last_update_dividers_rate = 0;
	}

	mspi_write32(mspi, CTL1, mspi->ctl1);
	mspi_write8(mspi, CTL0, CTL0_EN);

	return 0;
}

static int mspi_unprepare_transfer_hardware(struct spi_controller *ctlr)
{
	struct mspi *mspi = spi_controller_get_devdata(ctlr);

	mspi_write8(mspi, CTL0, 0);

	clk_disable_unprepare(mspi->clk);

	return 0;
}

static int mspi_prepare_message(struct spi_controller *ctlr,
		struct spi_message *msg)
{
	struct spi_transfer *xfer;

	/* validate that len and bits_per_word is in range */
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		unsigned int frame_count = xfer_frame_count(xfer);
		unsigned int frame_len = xfer_frame_len(xfer);

		if (frame_count < 1 || frame_count > 0xFFFF)
			return -EINVAL;

		/* FIXME: support for unalign frame length is not implemented
		 *        in read/write routines */
		//if (frame_len < MSPI_MIN_FRAME_LEN ||
		//    frame_len > MSPI_MAX_FRAME_LEN)
		//	return -EINVAL;
		switch (frame_len) {
		case 8:
		case 16:
		case 32:
		case 64:
		case 96:
		case 128:
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static void mspi_write_frame(struct mspi *mspi, const void *p,
		unsigned int frame_len)
{
	/* With the default most-significant-bit-first transmition, the bits
	 * from the first byte of the buffer shall be sent first. This
	 * corresponds to big-endian in-buffer store of 16- and 32-bit words.
	 *
	 * FIXME: this code only supports 8/16/32/64/96/128 frame length.
	 *        Both storage and processing for misaligned frame length is
	 *        unclear. E.g. for 35-bit frame, where bits 32..34 are stored
	 *        in memory and to which bit positions of the register they
	 *        shall be written
	 */
	const unsigned int chan = 0;

	if (frame_len <= 8) {
		const u8 *p8 = p;
		mspi_write8(mspi, TXDAm0(chan), *p8);
	} else if (frame_len <= 16) {
		const __be16 *p16 = p;
		mspi_write16(mspi, TXDAm0(chan), be16_to_cpu(*p16));
	} else {
		const __be32 *p32 = p;
		do {
			mspi_write32(mspi, TXDAm0(chan), be32_to_cpu(*p32));
			if (frame_len <= 32)
				break;
			p32++;
			frame_len -= 32;
		} while (1);
	}
}

static void mspi_read_frame(struct mspi *mspi, void *p, unsigned int frame_len)
{
	const unsigned int chan = 0;

	if (frame_len <= 8) {
		u8 *p8 = p;
		*p8 = mspi_read8(mspi, RXDAm0(chan));
	} else if (frame_len <= 16) {
		__be16 *p16 = p;
		*p16 = cpu_to_be16(mspi_read16(mspi, RXDAm0(chan)));
	} else {
		__be32 *p32 = p;
		do {
			*p32 = cpu_to_be32(mspi_read32(mspi, RXDAm0(chan)));
			if (frame_len <= 32)
				break;
			p32++;
			frame_len -= 32;
		} while (1);
	}
}

static int mspi_transfer_one(struct spi_controller *ctlr,
		struct spi_device *spi, struct spi_transfer *xfer)
{
	struct mspi *mspi = spi_controller_get_devdata(ctlr);
	unsigned int frame_count = xfer_frame_count(xfer);
	unsigned int frame_len = xfer_frame_len(xfer);
	unsigned int frame_storage_bytes = xfer_frame_storage_bytes(xfer);
	unsigned int half_fifo_words, tail_words, rx_remaining, tx_remaining;
	const void *tx_p;
	void *rx_p;
	int xfer_is_last_in_msg =
		list_is_last(&xfer->transfer_list, &ctlr->cur_msg->transfers);
	int i, ret;
	u32 val;
	unsigned long timeout;
	const unsigned int chan = 0;

	val = CFGm0_FIFO_MODE;
	if (xfer->tx_buf)
		val |= CFGm0_TXE;
	if (xfer->rx_buf)
		val |= CFGm0_RXE;
	mspi_write32(mspi, CFGm0(chan), val);

	val = 0;
	if (spi->mode & SPI_CPOL)
		val |= CFGm1_CPOL;
	if (spi->mode & SPI_CPHA)
		val |= CFGm1_CPHA;
	if (spi->mode & SPI_LSB_FIRST)
		val |= CFGm1_DIR_LSB_FIRST;
	if (spi->mode & SPI_CS_WORD)
		val |= CFGm1_FIDL;
	/* CSRI=1 means "set CS to inactive after xfer */
	if ((xfer->cs_change && !xfer_is_last_in_msg) ||
	    (!xfer->cs_change && xfer_is_last_in_msg))
		val |= CFGm1_CSRI;
	mspi_write32(mspi, CFGm1(chan), val);

	mspi_write32(mspi, CFGm2(chan), frame_len);

	ret = mspi_update_dividers(mspi, xfer->speed_hz);
	if (ret) {
		dev_err(&ctlr->dev, "failed to find clock dividers\n");
		return ret;
	}
	mspi_write16(mspi, CFGm3(chan), mspi->cfg3);

	/* Have 512 bytes of RAM, that is 256 bytes per direction.
	 *
	 * With single channel in use, 32 stages are available if frame length
	 * is not more than 64 bits
	 */
	if (frame_len <= 64) {
		val = CFGm4_32_STAGE_FIFO;
		half_fifo_words = 16;
	} else {
		val = CFGm4_16_STAGE_FIFO;
		half_fifo_words = 8;
	}
	mspi_write32(mspi, CFGm4(chan), val);
	mspi_write32(mspi, RASTADm(chan), 0);

	/* with CPHA=0, setup delay must be not less than half of bit time */
	if (!(spi->mode & SPI_CPHA))
		val = DIV_ROUND_UP(mspi->clk_ticks_per_bit, 2);
	else
		val = 1;
	val = min_t(typeof(val), val, 0xfff);
	mspi_write16(mspi, SEUPm(chan), val);

	val = mspi_delay_to_ticks(mspi, &xfer->delay);
	val = max_t(typeof(val), val, 1);
	val = min_t(typeof(val), val, 0xfff);
	mspi_write16(mspi, HOLDm(chan), val);

	val = mspi_delay_to_ticks(mspi, &xfer->cs_change_delay);
	val = max_t(typeof(val), val, 1);
	val = min_t(typeof(val), val, 0xfff);
	mspi_write16(mspi, IDLEm(chan), val);

	val = mspi_delay_to_ticks(mspi, &xfer->word_delay);
	val = min_t(typeof(val), val, 0xfff);
	mspi_write16(mspi, INDAm(chan), val);

	mspi_write16(mspi, CFSETm(chan), frame_count);

	mspi_write16(mspi, SSELm(chan), SSELm_CSR_BIT(spi->chip_select));

	mspi_write8(mspi, CSTSm(chan), CSTSm_CHENS | CSTSm_ACTFS);

	tx_p = xfer->tx_buf ? xfer->tx_buf : NULL;
	tx_remaining = xfer->tx_buf ? frame_count : 0;
	rx_p = xfer->rx_buf ? xfer->rx_buf : NULL;
	rx_remaining = xfer->rx_buf ? frame_count : 0;

	timeout = jiffies + HZ;
	do {
		val = mspi_read16(mspi, CSTRm(chan));

		/* After i/o with the last frame completes, ACTF and CHEN flags
		 * are auto-cleared. Per docs, this happen if CFG0.FCCE is zero,
		 * but docs also require that to be zero in fifo mode
		 */
		if (!(val & CSTRm_CHEN))
			break;

		/* If i/o is still in progress, need to read half of fifo on
		 * RXRQF flag, and write half of fifo on TXRQF flag */
		if (rx_p && (val & CSTRm_RXRQF)) {
			for (i = 0; rx_remaining && i < half_fifo_words; i++) {
				mspi_read_frame(mspi, rx_p, frame_len);
				rx_p += frame_storage_bytes;
				rx_remaining--;
				timeout = jiffies + HZ;
			}
		}

		if (tx_p && (val & CSTRm_TXRQF)) {
			for (i = 0; tx_remaining && i < half_fifo_words; i++) {
				mspi_write_frame(mspi, tx_p, frame_len);
				tx_p += frame_storage_bytes;
				tx_remaining--;
				timeout = jiffies + HZ;
			}
		}
	} while (time_before(jiffies, timeout));

	/* After i/o completes, CSTR.FIRXN contains number of words left in
	 * FIFO */
	tail_words = CSTRm_FIRXN(val);
	for (i = 0; i < tail_words && rx_remaining; i++) {
		mspi_read_frame(mspi, rx_p, frame_len);
		rx_p += frame_storage_bytes;
		rx_remaining--;
	}

	ret = 0;

	if ((val & CSTRm_CHEN) || tx_remaining || rx_remaining) {
		dev_err(&ctlr->dev,
			"i/o did not complete properly: en=%d count=%d remaining tx %d rx %d\n",
			!!(val & CSTRm_CHEN), frame_count,
			tx_remaining, rx_remaining);
		ret = -EIO;

		if (val & CSTRm_CHEN) {
			mspi_write8(mspi, CSTCm(chan), CSTCm_CHENC);
			timeout = jiffies + HZ;
			do {
				val = mspi_read16(mspi, CSTRm(chan));
			} while ((val & CSTRm_CHEN) &&
					time_before(jiffies, timeout));
			if (val & CSTRm_CHEN)
				dev_err(&ctlr->dev,
					"could not disable channel\n");
		}
	}

	val = mspi_read8(mspi, CESTm(chan));
	if (val) {
		dev_err(&ctlr->dev, "i/o error, status=0x%02x\n", val);
		ret = -EIO;
	}

	return ret;
}

static int mspi_probe(struct platform_device *pdev)
{
	struct spi_controller *ctlr;
	struct mspi *mspi;
	u32 num_cs;
	unsigned long rate;
	int ret;

	ctlr = devm_spi_alloc_master(&pdev->dev, sizeof(*mspi));
	if (!ctlr)
		return -ENOMEM;
	mspi = spi_controller_get_devdata(ctlr);
	mspi->ctlr = ctlr;
	dev_set_drvdata(&pdev->dev, mspi);

	mspi->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mspi->regs)) {
		dev_err(&pdev->dev, "devm_platform_iomap_resource() failed\n");
		return PTR_ERR(mspi->regs);
	}

	mspi->clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR(mspi->clk)) {
		dev_err(&pdev->dev, "devm_clk_get() failed\n");
		return PTR_ERR(mspi->clk);
	}
	rate = clk_get_rate(mspi->clk);

	ret = of_property_read_u32(pdev->dev.of_node, "num-cs", &num_cs);
	if (ret) {
		dev_err(&pdev->dev, "can't read num-cs property\n");
		return ret;
	}
	if (num_cs < 1 || num_cs > 8) {
		dev_err(&pdev->dev, "invalid num-cs value\n");
		return -EINVAL;
	}

	ctlr->bus_num = pdev->id;
	ctlr->num_chipselect = num_cs;
	ctlr->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH |
			  SPI_LSB_FIRST | SPI_CS_WORD;
	ctlr->min_speed_hz = rate / (64 * 62);
	ctlr->max_speed_hz = min_t(u32, rate / 2, MSPI_MAX_SPEED_HZ);
	ctlr->dev.of_node = pdev->dev.of_node;
	ctlr->auto_runtime_pm = true;

	ctlr->setup = mspi_setup;
	ctlr->prepare_transfer_hardware = mspi_prepare_transfer_hardware;
	ctlr->unprepare_transfer_hardware = mspi_unprepare_transfer_hardware;
	ctlr->prepare_message = mspi_prepare_message;
	ctlr->transfer_one = mspi_transfer_one;
	dev_info(&pdev->dev, "MSPI @%08llx ref clock is %ldkHz\n", pdev->resource->start, rate/1000);

	pm_runtime_enable(&pdev->dev);

	ret = devm_spi_register_controller(&pdev->dev, ctlr);
	if (ret < 0) {
		dev_err(&pdev->dev, "devm_spi_register_controller() failed\n");
		goto out_pm_disable;
	}

	return 0;

out_pm_disable:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int mspi_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static int __maybe_unused mspi_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mspi_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mspi_pm_ops, mspi_suspend, mspi_resume);

static const struct of_device_id mspi_match[] = {
	{ .compatible = "renesas,mspi-r8a779f0" },
	{}
};
MODULE_DEVICE_TABLE(of, mspi_match);

static struct platform_driver mspi_drv = {
	.probe		= mspi_probe,
	.remove		= mspi_remove,
	.driver		= {
		.name		= "mspi",
		.of_match_table = mspi_match,
		.pm		= &mspi_pm_ops,
	},
};

module_platform_driver(mspi_drv);

MODULE_DESCRIPTION("Renesas MSPI SPI Controller Interface Driver");
MODULE_AUTHOR("Nikita Yushchenko <nikita.yoush@cogentembedded.com>");
MODULE_LICENSE("GPL v2");
