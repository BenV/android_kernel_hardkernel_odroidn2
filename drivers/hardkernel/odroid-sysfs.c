/*
 * ODROID sysfs support for extra feature enhancement
 *
 * Copyright (C) 2014, Hardkernel Co,.Ltd
 *   Author: Charles Park <charles.park@hardkernel.com>
 *   Author: Dongjin Kim <tobetter@gmail.com>
 *
 * This driver has been modified to support ODROID-N1.
 *   Modified by Joy Cho <joycho78@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/sysfs.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <asm/setup.h>

MODULE_AUTHOR("Hardkernel Co,.Ltd");
MODULE_DESCRIPTION("SYSFS driver for ODROID hardware");
MODULE_LICENSE("GPL");

static struct hrtimer input_timer;
static struct input_dev *input_dev;
static int keycode[] = { KEY_POWER, };
static int key_release_seconds;

static enum hrtimer_restart input_timer_function(struct hrtimer *timer)
{
	key_release_seconds = 0;
	input_report_key(input_dev, KEY_POWER, 0);
	input_sync(input_dev);

	return HRTIMER_NORESTART;
}

static int init_input_dev(void)
{
	int error = 0;

/***********************************************************************
 * virtual key init (Power Off Key)
 ***********************************************************************/
	input_dev = input_allocate_device();
	if (!input_dev)
		return -ENOMEM;

	input_dev->name = "vt-input";
	input_dev->phys = "vt-input/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x16B4;
	input_dev->id.product = 0x0701;
	input_dev->id.version = 0x0001;
	input_dev->keycode = keycode;
	input_dev->keycodesize = sizeof(keycode[0]);
	input_dev->keycodemax = ARRAY_SIZE(keycode);

	set_bit(EV_KEY, input_dev->evbit);
	set_bit(KEY_POWER & KEY_MAX, input_dev->keybit);

	error = input_register_device(input_dev);
	if (error) {
		input_free_device(input_dev);
		return error;
	}

	pr_info("%s input driver registered!!\n", "Virtual-Key");

	hrtimer_init(&input_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	input_timer.function = input_timer_function;

	return error;
}


static ssize_t set_poweroff_trigger(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	int error = 0;

	if (sscanf(buf, "%d\n", &val) == 0)
		return  -EINVAL;

	if (error != 0)
		return -error;

	/* Emulate power button by software */
	if ((val != 0) && (val < 5)) {
		if (!key_release_seconds) {
			key_release_seconds = val;
			input_report_key(input_dev, KEY_POWER, 1);

			hrtimer_start(&input_timer,
					ktime_set(key_release_seconds, 0),
					HRTIMER_MODE_REL);

			input_sync(input_dev);
		}
	}

	return count;
}

static struct class_attribute odroid_class_attrs[] = {
	__ATTR(poweroff_trigger, 0220, NULL, set_poweroff_trigger),
	__ATTR_NULL,
};

static struct class odroid_class = {
	.name = "odroid",
	.owner = THIS_MODULE,
	.class_attrs = odroid_class_attrs,
};

static int odroid_sysfs_probe(struct platform_device *pdev)
{
	int error = 0;

#ifdef CONFIG_USE_OF
	struct device_node *node;

	if (pdev->dev.of_node)
		node = pdev->dev.of_node;
#endif

	if (input_dev == NULL)
		error = init_input_dev();

	return error;
}

static int odroid_sysfs_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int odroid_sysfs_suspend(struct platform_device *dev, pm_message_t state)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int odroid_sysfs_resume(struct platform_device *dev)
{
	pr_info("%s\n", __func__);

	return  0;
}
#endif

static const struct of_device_id odroid_sysfs_dt[] = {
	{ .compatible = "odroid-sysfs", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, odroid_sysfs_dt);

static struct platform_driver odroid_sysfs_driver = {
	.driver = {
		.name = "odroid-sysfs",
		.owner = THIS_MODULE,
		.of_match_table = odroid_sysfs_dt,
	},
	.probe = odroid_sysfs_probe,
	.remove = odroid_sysfs_remove,
#ifdef CONFIG_PM_SLEEP
	.suspend = odroid_sysfs_suspend,
	.resume = odroid_sysfs_resume,
#endif
};

static int __init odroid_sysfs_init(void)
{
	int error = class_register(&odroid_class);

	if (error < 0)
		return error;

	return platform_driver_register(&odroid_sysfs_driver);
}

static void __exit odroid_sysfs_exit(void)
{
	platform_driver_unregister(&odroid_sysfs_driver);
	class_unregister(&odroid_class);
}

module_init(odroid_sysfs_init);
module_exit(odroid_sysfs_exit);
