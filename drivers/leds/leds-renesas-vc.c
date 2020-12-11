/*
 * Copyright 2020 Renesas Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

#define VC_LED_GROUPS		2
#define VC_NUM_LED_PER_GROUP	8
#define VC_CPLD_MAX_LEDS	(VC_LED_GROUPS * VC_NUM_LED_PER_GROUP)

/* LED Initial state*/
enum led_initial_state {
	LED_INIT_STATE_ON,
	LED_INIT_STATE_OFF,
	LED_INIT_STATE_KEEP,
};

/* LED Driver Output State */
#define LEDOUT_ON		0x0
#define LEDOUT_OFF		0x1

#define ldev_to_led(c)		container_of(c, struct vc_cpld_led, ldev)

struct vc_cpld_led {
	bool active;
	unsigned int led_group;
	unsigned int led_reg;
	struct led_classdev ldev;
	struct vc_cpld_priv *priv;
	enum led_initial_state initial_state;
};

struct vc_cpld_priv {
	struct vc_cpld_led leds[VC_CPLD_MAX_LEDS];
	struct regmap *regmap;
};

static int
vc_cpld_set_ledout(struct vc_cpld_priv *priv, struct vc_cpld_led *led,
		    u8 val)
{
	unsigned int mask = led->led_reg;
	unsigned int addr = led->led_group;

	if (val)
		val = mask;

	return regmap_update_bits(priv->regmap, addr, mask, val);
}

static int
vc_cpld_get_ledout(struct vc_cpld_priv *priv, struct vc_cpld_led *led,
		u8 *val)
{
	unsigned int mask = led->led_reg;
	unsigned int addr = led->led_group;
	unsigned int regval = 0;
	int ret;

	ret = regmap_read(priv->regmap, addr, &regval);
	if (regval & mask)
		*val = LEDOUT_OFF;
	else
		*val = LEDOUT_ON;

	return ret;
}

static int
vc_cpld_brightness_set(struct led_classdev *led_cdev,
			enum led_brightness brightness)
{
	struct vc_cpld_led *led = ldev_to_led(led_cdev);
	struct vc_cpld_priv *priv = led->priv;
	int err = 0;

	switch (brightness) {
	case 0:
		err = vc_cpld_set_ledout(priv, led, LEDOUT_OFF);
		break;
	case LED_FULL:
		err = vc_cpld_set_ledout(priv, led, LEDOUT_ON);
		break;
	default:
		break;
	}

	return err;
}

static void
vc_cpld_destroy_devices(struct vc_cpld_priv *priv, unsigned int j)
{
	int i = j;

	while (--i >= 0) {
		if (priv->leds[i].active)
			led_classdev_unregister(&priv->leds[i].ldev);
	}
}

static int
vc_cpld_configure(struct device *dev,
		struct vc_cpld_priv *priv)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < VC_CPLD_MAX_LEDS; i++) {
		struct vc_cpld_led *led = &priv->leds[i];

		if (!led->active)
			continue;

		led->priv = priv;
		led->ldev.brightness_set_blocking = vc_cpld_brightness_set;
		led->ldev.max_brightness = LED_FULL;

		if (led->initial_state == LED_INIT_STATE_ON) {
			vc_cpld_set_ledout(priv, led, LEDOUT_ON);
			led->ldev.brightness = LED_FULL;
		} else if (led->initial_state == LED_INIT_STATE_OFF) {
			vc_cpld_set_ledout(priv, led, LEDOUT_OFF);
			led->ldev.brightness = 0;
		} else if (led->initial_state == LED_INIT_STATE_KEEP) {
			u8 cur_state;
			int ret = vc_cpld_get_ledout(priv, led, &cur_state);
			if (!ret && cur_state == LEDOUT_ON)
				led->ldev.brightness = LED_FULL;
		}

		err = led_classdev_register(dev, &led->ldev);
		if (err < 0) {
			dev_err(dev, "couldn't register LED %s\n",
				led->ldev.name);
			goto exit;
		}
	}

	return 0;

exit:
	vc_cpld_destroy_devices(priv, i);
	return err;
}

static const struct regmap_config vc_cpld_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x1,
};

static const struct of_device_id of_vc_cpld_leds_match[] = {
	{ .compatible = "renesas,vc-cpld-led"},
	{},
};
MODULE_DEVICE_TABLE(of, of_vc_cpld_leds_match);

static int
vc_cpld_probe(struct i2c_client *client,
	       const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node, *child;
	struct device *dev = &client->dev;
	const struct of_device_id *match;
	struct vc_cpld_priv *priv;
	int err, count, reg, group, led_idx;
	const char *state;

	match = of_match_device(of_vc_cpld_leds_match, dev);
	if (!match)
		return -ENODEV;

	count = of_get_child_count(np);
	if (!count || count > VC_CPLD_MAX_LEDS) {
		dev_err(dev, "Wrong number (%d) of child led nodes in device-tree\n", count);
		return -EINVAL;
	}

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->regmap = devm_regmap_init_i2c(client, &vc_cpld_regmap);
	if (IS_ERR(priv->regmap)) {
		err = PTR_ERR(priv->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", err);
		return err;
	}

	i2c_set_clientdata(client, priv);

	led_idx = 0;
	for_each_child_of_node(np, child) {
		err = of_property_read_u32(child, "group", &group);
		if (err) {
			of_node_put(child);
			return err;
		}
		if (group < 0 || group >= VC_LED_GROUPS) {
			of_node_put(child);
			return -EINVAL;
		}

		err = of_property_read_u32(child, "reg", &reg);
		if (err) {
			of_node_put(child);
			return err;
		}
		if (reg < 0 || reg >= (1<<VC_NUM_LED_PER_GROUP)) {
			of_node_put(child);
			return -EINVAL;
		}

		priv->leds[led_idx].active = true;
		priv->leds[led_idx].ldev.name =
			of_get_property(child, "label", NULL) ? : child->name;
		priv->leds[led_idx].ldev.default_trigger =
			of_get_property(child, "linux,default-trigger", NULL);
		priv->leds[led_idx].led_group = group;
		priv->leds[led_idx].led_reg = reg;

		priv->leds[led_idx].initial_state = LED_INIT_STATE_KEEP;
		state = of_get_property(child, "default-state", NULL);
		if (state) {
			if (!strcmp(state, "on"))
				priv->leds[led_idx].initial_state = LED_INIT_STATE_ON;
			else if (!strcmp(state, "off"))
				priv->leds[led_idx].initial_state = LED_INIT_STATE_OFF;
		}

		led_idx++;
	}
	return vc_cpld_configure(dev, priv);
}

static int
vc_cpld_remove(struct i2c_client *client)
{
	struct vc_cpld_priv *priv = i2c_get_clientdata(client);

	vc_cpld_destroy_devices(priv, VC_CPLD_MAX_LEDS);

	return 0;
}

static const struct i2c_device_id vc_cpld_id[] = {
	{ "vc_cpld_led" },
	{},
};
MODULE_DEVICE_TABLE(i2c, vc_cpld_id);

static struct i2c_driver vc_cpld_driver = {
	.driver = {
		.name = "renesas_vc_cpld_leds",
		.of_match_table = of_match_ptr(of_vc_cpld_leds_match),
	},
	.probe = vc_cpld_probe,
	.remove = vc_cpld_remove,
	.id_table = vc_cpld_id,
};

module_i2c_driver(vc_cpld_driver);

MODULE_AUTHOR("Vito Colagiacomo <vito.colagiacomo@renesas.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Renesas Vehicle Computer CPLD LED driver");
