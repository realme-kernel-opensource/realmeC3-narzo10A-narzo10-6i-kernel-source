/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <extcon_usb.h>

#ifdef VENDOR_EDIT
//Fuchun.Liao@BSP.CHG.Basic 2017/11/22 add for otg
#include <linux/of_gpio.h>
#endif /* VENDOR_EDIT */
#define RET_SUCCESS 0
#define RET_FAIL 1
#ifdef VENDOR_EDIT
/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/11/19, Add for otg */
struct platform_device *musb_pltfm_dev = NULL;
#define OTGID_GPIO_MODE 1
#define OTGID_IRQ_MODE  0
static bool otg_isr_enable;
static int mtk_idpin_irqnum;
static struct pinctrl *pinctrl;
static struct pinctrl_state *pinctrl_iddig;
int iddig_gpio_mode(int mode);
extern bool get_otg_switch(void);
#endif /* VENDOR_EDIT */

struct usb_iddig_info {
	struct device *dev;
	struct gpio_desc *id_gpiod;
	int id_irq;
	unsigned long id_swdebounce;
	unsigned long id_hwdebounce;
	struct delayed_work id_delaywork;
	struct pinctrl *pinctrl;
	struct pinctrl_state *id_init;
	struct pinctrl_state *id_enable;
	struct pinctrl_state *id_disable;
};


enum idpin_state {
	IDPIN_OUT,
	IDPIN_IN_HOST,
	IDPIN_IN_DEVICE,
};

static const struct of_device_id otg_iddig_of_match[] = {
	{.compatible = "mediatek,usb_iddig_bi_eint"},
	{},
};

static enum idpin_state mtk_idpin_cur_stat = IDPIN_OUT;

static void mtk_set_iddig_out_detect(struct usb_iddig_info *info)
{
	irq_set_irq_type(info->id_irq, IRQF_TRIGGER_HIGH);
	enable_irq(info->id_irq);
}

static void mtk_set_iddig_in_detect(struct usb_iddig_info *info)
{
	irq_set_irq_type(info->id_irq, IRQF_TRIGGER_LOW);
	enable_irq(info->id_irq);
}


#ifdef VENDOR_EDIT
/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/11/25, modify for otg */
int otg_is_exist=0;
#endif
static void iddig_mode_switch(struct work_struct *work)
{
	struct usb_iddig_info *info = container_of(to_delayed_work(work),
						    struct usb_iddig_info,
						    id_delaywork);

#ifdef VENDOR_EDIT
/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2018/04/16, modify for otg */
//	if (get_otg_switch() == false){
//		return ;
//	}
#endif
	if (mtk_idpin_cur_stat == IDPIN_OUT) {
		mtk_idpin_cur_stat = IDPIN_IN_HOST;
		mt_usbhost_connect();
		mt_vbus_on();
		mtk_set_iddig_out_detect(info);
		#ifdef VENDOR_EDIT
		/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/11/25, modify for otg */
		otg_is_exist = 1;
		#endif
	} else {
		#ifdef VENDOR_EDIT
		/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/11/25, modify for otg */
		otg_is_exist = 0;
		#endif
		mtk_idpin_cur_stat = IDPIN_OUT;
		mt_usbhost_disconnect();
		mt_vbus_off();
		mtk_set_iddig_in_detect(info);
	}
}

static irqreturn_t iddig_eint_isr(int irqnum, void *data)
{
#ifdef VENDOR_EDIT
//Jianwei.Ye@BSP.CHG.Basic 2019/09/07 modify for otg
	struct usb_iddig_info *info = platform_get_drvdata(musb_pltfm_dev);
#else
	struct usb_iddig_info *info = data;
#endif
	disable_irq_nosync(irqnum);
	schedule_delayed_work(&info->id_delaywork,
		msecs_to_jiffies(info->id_swdebounce));

	return IRQ_HANDLED;
}

#ifdef VENDOR_EDIT
/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/11/19, Add for otg */
int iddig_gpio_mode(int mode)
{
	int retval;
	struct usb_iddig_info *info = platform_get_drvdata(musb_pltfm_dev);

	if(musb_pltfm_dev != NULL) {
	      if(mode == OTGID_GPIO_MODE) {
			printk("iddig_gpio_mode OTGID_GPIO_MODE\n");
			if(otg_isr_enable == 1) {
				free_irq(mtk_idpin_irqnum, NULL);
				otg_isr_enable = 0;
			}
			if(otg_is_exist == 1) {
				schedule_delayed_work(&info->id_delaywork, msecs_to_jiffies(info->id_swdebounce));
				mdelay(5);
			}
			pinctrl = devm_pinctrl_get(&musb_pltfm_dev->dev);
			if (IS_ERR(pinctrl))
				dev_err(&musb_pltfm_dev->dev, "Cannot find usb pinctrl!\n");
			else {
				pinctrl_iddig = pinctrl_lookup_state(pinctrl, "id_output_low");
			if (IS_ERR(pinctrl_iddig))
				dev_err(&musb_pltfm_dev->dev, "Cannot find usb pinctrl id_output_low\n");
			else
				pinctrl_select_state(pinctrl, pinctrl_iddig);

			}

	     } else if(mode == OTGID_IRQ_MODE) {
			printk("iddig_gpio_mode OTGID_IRQ_MODE\n");
			pinctrl = devm_pinctrl_get(&musb_pltfm_dev->dev);
			if (IS_ERR(pinctrl))
				dev_err(&musb_pltfm_dev->dev, "Cannot find usb pinctrl!\n");
			else {
				pinctrl_iddig = pinctrl_lookup_state(pinctrl, "id_init");
				if (IS_ERR(pinctrl_iddig))
					dev_err(&musb_pltfm_dev->dev, "Cannot find usb pinctrl id_init\n");
				else
					pinctrl_select_state(pinctrl, pinctrl_iddig);
			}
			mdelay(5);
			if(otg_is_exist == 1) {
				retval = request_irq(mtk_idpin_irqnum, iddig_eint_isr, IRQF_TRIGGER_HIGH, "iddig_eint", NULL);
			} else {
				retval = request_irq(mtk_idpin_irqnum, iddig_eint_isr, IRQF_TRIGGER_LOW, "iddig_eint", NULL);
			}
			if (retval < 0) {
				dev_info(&musb_pltfm_dev->dev, "failed to request handler for ID IRQ\n");
				return retval;
			}
			otg_isr_enable = 1;
	    }
	    return 0;
    } else {
            return -1;
    }
}
/*
int mtk_eint_iddig_init(void)
{
	 int retval = 0;
	 enable_irq(iddig_eint_num);
	 retval = request_irq(iddig_eint_num, mt_usb_ext_iddig_int, IRQF_TRIGGER_LOW, "USB_IDDIG", NULL);
	 printk("mtk_eint_iddig_init\n");
	 //INIT_DELAYED_WORK(&mtk_xhci_delaywork, mtk_xhci_mode_switch);

	 return retval;
}

void mtk_eint_iddig_deinit(void)
{
	 free_irq(iddig_eint_num, NULL);
	 if(iddig_req_host == 1) {
		 iddig_req_host = 0;
		 printk("mtk_eint_iddig_deinit\n");
		 queue_delayed_work(mtk_musb->st_wq, &mtk_musb->host_work, msecs_to_jiffies(sw_deboun_time));
	 }

	 //cancel_delayed_work(&mtk_xhci_delaywork);
 }
 */
void mtk_xhci_eint_iddig_gpio_mode(void)
{
    iddig_gpio_mode(1);
}
#endif /* VENDOR_EDIT */

static int otg_iddig_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
#ifndef VENDOR_EDIT
//Fuchun.Liao@BSP.CHG.Basic 2017/11/22 modify for otg
	struct device_node *node = dev->of_node;
#else
	struct device_node *node = NULL;
#endif /* VENDOR_EDIT */
	struct usb_iddig_info *info;
	struct pinctrl *pinctrl;
	u32 ints[2] = {0, 0};

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;

#ifndef VENDOR_EDIT
//Fuchun.Liao@BSP.CHG.Basic 2017/11/22 modify for otg
	info->id_irq = irq_of_parse_and_map(node, 0);
	if (info->id_irq < 0)
		return -ENODEV;
#else
	printk("otg_iddig_probe\n");
	node = of_find_matching_node(node, otg_iddig_of_match);
	if(node != NULL) {
		mtk_idpin_irqnum = irq_of_parse_and_map(node, 0);
	}
	else {
		printk("otg_iddig_probe_node is none\n");
	}
	printk("iddig gpio num = %d\n", mtk_idpin_irqnum);
	info->id_irq = mtk_idpin_irqnum;
	musb_pltfm_dev = pdev;
#endif /* VENDOR_EDIT */

	pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR(pinctrl)) {
		dev_info(&pdev->dev, "No find id pinctrl!\n");
	} else {
		info->pinctrl = pinctrl;
		info->id_init = pinctrl_lookup_state(pinctrl, "id_init");
		if (IS_ERR(info->id_init))
			dev_info(&pdev->dev, "No find pinctrl id_init\n");
		else
			pinctrl_select_state(info->pinctrl, info->id_init);
		info->id_enable = pinctrl_lookup_state(pinctrl, "id_enable");
		info->id_disable = pinctrl_lookup_state(pinctrl, "id_disable");
		if (IS_ERR(info->id_enable))
			dev_info(&pdev->dev, "No find pinctrl iddig_enable\n");
		if (IS_ERR(info->id_disable))
			dev_info(&pdev->dev, "No find pinctrl iddig_disable\n");
	}



	info->pinctrl = pinctrl;

	info->id_init = pinctrl_lookup_state(pinctrl, "id_init");
	if (IS_ERR(info->id_init))
		dev_info(&pdev->dev, "No find pinctrl id_init\n");
	else
		pinctrl_select_state(info->pinctrl, info->id_init);

#ifndef VENDOR_EDIT
//Jianwei.Ye@BSP.CHG.Basic 2019/09/07 modify for otg
	info->id_enable = pinctrl_lookup_state(pinctrl, "id_enable");
	info->id_disable = pinctrl_lookup_state(pinctrl, "id_disable");
	if (IS_ERR(info->id_enable))
		dev_info(&pdev->dev, "No find pinctrl iddig_enable\n");
	if (IS_ERR(info->id_disable))
		dev_info(&pdev->dev, "No find pinctrl iddig_disable\n");
#endif

	ret = of_property_read_u32_array(node, "debounce",
		ints, ARRAY_SIZE(ints));
	if (!ret)
		info->id_hwdebounce = ints[1];

	info->id_swdebounce = msecs_to_jiffies(50);

	INIT_DELAYED_WORK(&info->id_delaywork, iddig_mode_switch);
#ifndef VENDOR_EDIT
//Jianwei.Ye@BSP.CHG.Basic 2019/09/07 modify for otg
	ret = devm_request_irq(dev, info->id_irq, iddig_eint_isr,
					0, pdev->name, info);
	if (ret < 0) {
		dev_info(dev, "failed to request handler for ID IRQ\n");
		return ret;
	}
#endif

	platform_set_drvdata(pdev, info);

	#ifdef VENDOR_EDIT
	/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/11/19, Add for otg */
	otg_isr_enable = 0;
	iddig_gpio_mode(OTGID_GPIO_MODE);
	#endif /* VENDOR_EDIT */

	return 0;
}

static int otg_iddig_remove(struct platform_device *pdev)
{
	struct usb_iddig_info *info = platform_get_drvdata(pdev);

	disable_irq_nosync(mtk_idpin_irqnum);
	if(otg_isr_enable == 1) {
		free_irq(mtk_idpin_irqnum, NULL);
		otg_isr_enable = 0;
	}
	cancel_delayed_work(&info->id_delaywork);

	return 0;
}

static struct platform_driver otg_iddig_driver = {
	.probe = otg_iddig_probe,
	.remove = otg_iddig_remove,
	.driver = {
		.name = "otg_iddig",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(otg_iddig_of_match),
	},
};

static int __init otg_iddig_init(void)
{
	return platform_driver_register(&otg_iddig_driver);
}
late_initcall(otg_iddig_init);

static void __exit otg_iddig_cleanup(void)
{
	platform_driver_unregister(&otg_iddig_driver);
}

module_exit(otg_iddig_cleanup);

