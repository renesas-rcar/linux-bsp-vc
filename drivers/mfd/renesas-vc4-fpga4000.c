// SPDX-License-Identifier: GPL-2.0

#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/reboot.h>
#include <linux/regmap.h>

#define REG_PRODUCT		0x0000
#define REG_PRODUCT_SIZE	4
#define REG_FPGAVERSION		0x0004
#define REG_FPGAVERSION_SIZE	4

#define REG_DIPSW		0x0020

#define REG_I2C_ADDRESS		0x0022

#define REG_RESET		0x0024
#define REG_RESET_DISB_VTHSENSE		BIT(5)
#define REG_RESET_PCIE			BIT(4)
#define REG_RESET_EXP_BRD		BIT(3)
#define REG_RESET_PHY			BIT(2)
#define REG_RESET_S4			BIT(1)
#define REG_RESET_SYSTEM		BIT(0)

#define REG_POWER_EN		0x0025
#define REG_POWER_EN_FIRST_BOOT		BIT(7)
#define REG_POWER_EN_RSENT1		BIT(3)
#define REG_POWER_EN_RSENT0		BIT(2)
#define REG_POWER_EN_EXP_BRD		BIT(1)
#define REG_POWER_EN_GLOBAL		BIT(0)

#define REG_CAN_GPIO_BASE	0x0040
#define REG_CAN_GPIO_NR		8

#define REG_MAX			0x0047

#define CAN_GPIO_BASE		0
#define CAN_GPIO_NR		64

#define TOTAL_GPIO_NR		CAN_GPIO_NR

/* ranges, in first..last form, that driver can read */
static const struct regmap_range readable_ranges[] = {
	regmap_reg_range(REG_PRODUCT,
			 REG_FPGAVERSION + REG_FPGAVERSION_SIZE - 1),
	regmap_reg_range(REG_DIPSW, REG_POWER_EN),
	regmap_reg_range(REG_CAN_GPIO_BASE, REG_CAN_GPIO_BASE + REG_CAN_GPIO_NR - 1),
};

/* ranges, in first..last form, that driver can write */
static const struct regmap_range writeable_ranges[] = {
	regmap_reg_range(REG_RESET, REG_POWER_EN),
	regmap_reg_range(REG_CAN_GPIO_BASE, REG_CAN_GPIO_BASE + REG_CAN_GPIO_NR - 1),
};

/* ranges, in first..last form, that hardware can change at runtime,
 * thus driver must not cache them */
static const struct regmap_range volatile_ranges[] = {
	regmap_reg_range(REG_DIPSW, REG_DIPSW),
};

static const struct regmap_access_table fpga4000_readable_table = {
	.yes_ranges = readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(readable_ranges),
};

static const struct regmap_access_table fpga4000_writeable_table = {
	.yes_ranges = writeable_ranges,
	.n_yes_ranges = ARRAY_SIZE(writeable_ranges),
};

static const struct regmap_access_table fpga4000_volatile_table = {
	.yes_ranges = volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(volatile_ranges),
};

static const struct regmap_config fpga4000_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = REG_MAX,
	.rd_table = &fpga4000_readable_table,
	.wr_table = &fpga4000_writeable_table,
	.volatile_table = &fpga4000_volatile_table,
};

struct fpga4000 {
	struct device *dev;
	struct regmap *regmap;
	uint8_t product[REG_PRODUCT_SIZE];
	uint8_t version[REG_FPGAVERSION_SIZE];

	struct notifier_block restart_nb;

	struct gpio_chip gpio;
};

static int fpga4000_restart_handler(struct notifier_block *nb,
		unsigned long mode, void *cmd)
{
	struct fpga4000 *fpga = container_of(nb, struct fpga4000, restart_nb);

	regmap_set_bits(fpga->regmap, REG_RESET, REG_RESET_SYSTEM);

	return NOTIFY_DONE;
}

static struct {
	struct regmap *regmap;
	void (*old_handler)(void);
} poweroff;

static void fpga4000_power_off(void)
{
	regmap_clear_bits(poweroff.regmap, REG_POWER_EN, REG_POWER_EN_GLOBAL);
	mdelay(500);
}

static void fpga4000_setup_poweroff(struct fpga4000 *fpga)
{
	if (cmpxchg(&poweroff.regmap, NULL, fpga->regmap) == NULL)
		poweroff.old_handler = xchg(&pm_power_off, fpga4000_power_off);
}

static void fpga4000_unsetup_poweroff(struct fpga4000 *fpga)
{
	if (poweroff.regmap == fpga->regmap) {
		if (is_kernel_text((unsigned long)poweroff.old_handler))
			pm_power_off = poweroff.old_handler;
		else
			pm_power_off = NULL;

		poweroff.regmap = NULL;
	}
}

static int gpio_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	if (offset >= CAN_GPIO_BASE && offset <= CAN_GPIO_BASE + CAN_GPIO_NR - 1)
		return GPIO_LINE_DIRECTION_OUT;

	return -EINVAL;
}

static int gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	struct fpga4000 *fpga = container_of(gc, struct fpga4000, gpio);
	unsigned int reg, mask, val;
	int ret;

	if (offset >= CAN_GPIO_BASE && offset <= CAN_GPIO_BASE + CAN_GPIO_NR - 1) {
		reg = REG_CAN_GPIO_BASE + (offset / 8);
		mask = 1 << (offset % 8);

		ret = regmap_read(fpga->regmap, reg, &val);
		if (ret < 0)
			return ret;

		return !!(val & mask);
	}

	return -EINVAL;
}

static void gpio_set(struct gpio_chip *gc, unsigned int offset, int value)
{
	struct fpga4000 *fpga = container_of(gc, struct fpga4000, gpio);
	unsigned int reg, mask;

	if (offset >= CAN_GPIO_BASE && offset <= CAN_GPIO_BASE + CAN_GPIO_NR - 1) {
		reg = REG_CAN_GPIO_BASE + (offset / 8);
		mask = 1 << (offset % 8);

		regmap_update_bits(fpga->regmap, reg, mask, value ? mask : 0);
	}
}

#define PROD_VER_ATTR(name, size)				\
static ssize_t name##_show(struct device *dev,			\
		struct device_attribute *attr, char *buf)	\
{								\
	struct fpga4000 *fpga = dev_get_drvdata(dev);		\
	size_t sz;						\
								\
	sz = hex_dump_to_buffer(fpga->name, size, 0, 1, buf, PAGE_SIZE, 0); \
	buf[sz++] = '\n';					\
	return sz;						\
}								\
static DEVICE_ATTR_RO(name)

PROD_VER_ATTR(product, REG_PRODUCT_SIZE);
PROD_VER_ATTR(version, REG_FPGAVERSION_SIZE);

static ssize_t dipsw_show(struct device *dev, unsigned int mask, char *buf)
{
	struct fpga4000 *fpga = dev_get_drvdata(dev);
	unsigned int val;
	ssize_t ret;

	ret = regmap_read(fpga->regmap, REG_DIPSW, &val);
	if (ret < 0)
		return ret;

	/* bits in REG_DIPSW are inverted, 0 = ON, 1 = OFF */
	return sprintf(buf, "%s\n", (val & mask) ? "OFF" : "ON");
}

#define DIPSW_ATTR(name, mask)					\
static ssize_t name##_show(struct device *dev,			\
		struct device_attribute *attr, char *buf)	\
{								\
	return dipsw_show(dev, mask, buf);			\
}								\
static DEVICE_ATTR_RO(name)

DIPSW_ATTR(sw3, BIT(3));
DIPSW_ATTR(sw4, BIT(4));
DIPSW_ATTR(sw5, BIT(5));
DIPSW_ATTR(sw6, BIT(6));

static ssize_t reg_mask_show(struct device *dev,
		unsigned int reg, unsigned int mask, char *buf)
{
	struct fpga4000 *fpga = dev_get_drvdata(dev);
	unsigned int val;
	ssize_t ret;

	ret = regmap_read(fpga->regmap, reg, &val);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", !!(val & mask));
}

static ssize_t reg_mask_store(struct device *dev,
		unsigned int reg, unsigned int mask,
		const char *buf, size_t count)
{
	struct fpga4000 *fpga = dev_get_drvdata(dev);
	bool b;
	ssize_t ret;

	ret = kstrtobool(buf, &b);
	if (ret < 0)
		return ret;

	if (b)
		ret = regmap_set_bits(fpga->regmap, reg, mask);
	else
		ret = regmap_clear_bits(fpga->regmap, reg, mask);

	return ret < 0 ? ret : count;
}

#define REG_MASK_ATTR(name, reg, mask)				\
static ssize_t name##_show(struct device *dev,			\
		struct device_attribute *attr, char *buf)	\
{								\
	return reg_mask_show(dev, reg, mask, buf);		\
}								\
static ssize_t name##_store(struct device *dev,			\
		struct device_attribute *attr, const char *buf, size_t count) \
{								\
	return reg_mask_store(dev, reg, mask, buf, count);	\
}								\
static DEVICE_ATTR_RW(name)

#define REG_MASK_ATTR_RO(name, reg, mask)			\
static ssize_t name##_show(struct device *dev,			\
		struct device_attribute *attr, char *buf)	\
{								\
	return reg_mask_show(dev, reg, mask, buf);		\
}								\
static DEVICE_ATTR_RO(name)

REG_MASK_ATTR(disb_vthsense_reset, REG_RESET, REG_RESET_DISB_VTHSENSE);
REG_MASK_ATTR(pcie_reset, REG_RESET, REG_RESET_PCIE);
REG_MASK_ATTR(exp_brd_reset, REG_RESET, REG_RESET_EXP_BRD);
REG_MASK_ATTR(phy_reset, REG_RESET, REG_RESET_PHY);
REG_MASK_ATTR_RO(first_boot, REG_POWER_EN, REG_POWER_EN_FIRST_BOOT);
REG_MASK_ATTR(rsent1_power, REG_POWER_EN, REG_POWER_EN_RSENT1);
REG_MASK_ATTR(rsent0_power, REG_POWER_EN, REG_POWER_EN_RSENT0);
REG_MASK_ATTR(exp_brd_power, REG_POWER_EN, REG_POWER_EN_EXP_BRD);

static struct attribute *fpga4000_attrs[] = {
	&dev_attr_product.attr,
	&dev_attr_version.attr,
	&dev_attr_sw3.attr,
	&dev_attr_sw4.attr,
	&dev_attr_sw5.attr,
	&dev_attr_sw6.attr,
	&dev_attr_disb_vthsense_reset.attr,
	&dev_attr_pcie_reset.attr,
	&dev_attr_exp_brd_reset.attr,
	&dev_attr_phy_reset.attr,
	&dev_attr_first_boot.attr,
	&dev_attr_rsent1_power.attr,
	&dev_attr_rsent0_power.attr,
	&dev_attr_exp_brd_power.attr,
	NULL
};
ATTRIBUTE_GROUPS(fpga4000);

/* This is a macro, not static inline, to avoid warning about variable-size
 * buf */
#define dev_info_with_dump(dev, prefix, data, size) do { \
	char buf[3 * size]; \
	hex_dump_to_buffer(data, size, 0, 1, buf, sizeof(buf), 0); \
	dev_info(dev, "%s: %s\n", prefix, buf); \
} while (0)

static int fpga4000_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct fpga4000 *fpga;
	int ret;

	fpga = devm_kzalloc(dev, sizeof(*fpga), GFP_KERNEL);
	if (!fpga)
		return -ENOMEM;

	fpga->dev = dev;
	i2c_set_clientdata(client, fpga);

	fpga->regmap = devm_regmap_init_i2c(client, &fpga4000_regmap_config);
	if (IS_ERR(fpga->regmap)) {
		dev_err(dev, "regmap init: %pe\n", fpga->regmap);
		return PTR_ERR(fpga->regmap);
	}

	ret = regmap_bulk_read(fpga->regmap, REG_PRODUCT, fpga->product,
			REG_PRODUCT_SIZE);
	if (ret) {
		dev_err(dev, "read product id: %pe\n", ERR_PTR(ret));
		return ret;
	}
	dev_info_with_dump(dev, "product", fpga->product, REG_PRODUCT_SIZE);

	ret = regmap_bulk_read(fpga->regmap, REG_FPGAVERSION, fpga->version,
			REG_FPGAVERSION_SIZE);
	if (ret) {
		dev_err(dev, "read version: %pe\n", ERR_PTR(ret));
		return ret;
	}
	dev_info_with_dump(dev, "version", fpga->version, REG_FPGAVERSION_SIZE);

	fpga->gpio.label = client->name;
	fpga->gpio.parent = dev;
	fpga->gpio.owner = THIS_MODULE;
	fpga->gpio.base = -1;
	fpga->gpio.ngpio = TOTAL_GPIO_NR;
	fpga->gpio.can_sleep = true;
	fpga->gpio.get_direction = gpio_get_direction;
	fpga->gpio.get = gpio_get;
	fpga->gpio.set = gpio_set;

	ret = devm_gpiochip_add_data(dev, &fpga->gpio, NULL);
	if (ret) {
		dev_err(dev, "register gpiochip: %pe\n", ERR_PTR(ret));
		return ret;
	}

	fpga->restart_nb.notifier_call = fpga4000_restart_handler;
	fpga->restart_nb.priority = 200;
	ret = register_restart_handler(&fpga->restart_nb);
	if (ret) {
		dev_err(dev, "register_restart_handler: %pe\n", ERR_PTR(ret));
		return ret;
	}

	fpga4000_setup_poweroff(fpga);

	return 0;
}

static int fpga4000_remove(struct i2c_client *client)
{
	struct fpga4000 *fpga = i2c_get_clientdata(client);

	fpga4000_unsetup_poweroff(fpga);
	unregister_restart_handler(&fpga->restart_nb);

	return 0;
}

static const struct of_device_id fpga4000_match[] = {
	{ .compatible = "renesas,vc4-fpga4000" },
	{}
};
MODULE_DEVICE_TABLE(of, fpga4000_match);

static const struct i2c_device_id fpga4000_id[] = {
	{ "renesas-vc4-fpga4000" },
	{}
}
MODULE_DEVICE_TABLE(i2c,fpga4000_id);

static struct i2c_driver fpga4000_driver = {
	.driver = {
		.name = "renesas-vc4-fpga4000",
		.of_match_table = of_match_ptr(fpga4000_match),
		.dev_groups = fpga4000_groups,
	},
	.probe = fpga4000_probe,
	.remove = fpga4000_remove,
	.id_table = fpga4000_id,
};

module_i2c_driver(fpga4000_driver);

MODULE_AUTHOR("Nikita Yushchenko <nikita.yoush@cogentembedded.com>");
MODULE_LICENSE("GPL v2");
