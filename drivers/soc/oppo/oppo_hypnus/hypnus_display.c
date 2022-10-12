/*
 * Copyright (C) 2019 OPPO, Inc.
 * Author: cuixiaogang <cuixiaogang@oppo.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/fb.h>

#if defined(CONFIG_ARCH_MSM) || defined(CONFIG_ARCH_QCOM)
#ifdef CONFIG_DRM_MSM
#include <linux/msm_drm_notify.h>
#include <linux/dsi_oppo_support.h>
#else
#include "mdss_fb.h"
#include "mdss_panel.h"
#include "mdss_mdp.h"
#endif
#endif

#ifdef CONFIG_MTK_PLATFORM
#include "mtkfb.h"
#endif

#include "hypnus.h"

#ifdef CONFIG_DRM_MSM
int fb_cb(struct notifier_block *nb, unsigned long val, void *data)
{
	struct msm_drm_notifier *mdn = data;
	struct hypnus_data *hypdata = hypnus_get_hypdata();
	int blank, screen_on = 1;
	bool valid = false;
	unsigned long flags;

	if (!mdn || (mdn->id != 0))
		return NOTIFY_OK;

	if (!hypdata || val != MSM_DRM_EARLY_EVENT_BLANK)
		return NOTIFY_OK;

	blank = *(int*)mdn->data;
	pr_debug("event %d,blank %d\n", (int)val, blank);
	switch (blank) {
		case MSM_DRM_BLANK_UNBLANK:
		screen_on = 1;
		valid = true;
		break;
		case MSM_DRM_BLANK_POWERDOWN:
		screen_on = 0;
		valid = true;
		break;
		default:
		valid = false;
		break;
	}
	if (valid && screen_on != hypdata->screen_on) {
		spin_lock_irqsave(&hypdata->lock, flags);
		hypdata->screen_on = screen_on;
		spin_unlock_irqrestore(&hypdata->lock, flags);
		complete(&hypdata->wait_event);
	}
	pr_debug("screen is %d\n", (int)hypdata->screen_on);
	return NOTIFY_OK;
}
#else
int fb_cb(struct notifier_block *nb, unsigned long val, void *data)
{
	bool screen_on;
	struct hypnus_data *hypdata = hypnus_get_hypdata();
	unsigned long flags;
#if defined(CONFIG_ARCH_MSM) || defined(CONFIG_ARCH_QCOM)
	int *fb_data = NULL;
	struct fb_event *evdata = data;
#endif
	if (!hypdata)
		return NOTIFY_OK;

	spin_lock_irqsave(&hypdata->lock, flags);
#if defined(CONFIG_ARCH_MSM) || defined(CONFIG_ARCH_QCOM)
	switch (val) {
	case FB_EVENT_BLANK:
		if (evdata && evdata->data) {
			fb_data = evdata->data;
			if (*fb_data == FB_BLANK_UNBLANK) {
				screen_on = 1;
			} else if (*fb_data == FB_BLANK_POWERDOWN)
				screen_on = 0;
		}
		break;
	case FB_EVENT_SUSPEND:
		break;
	default:
		break;
	}
#elif defined(CONFIG_MTK_PLATFORM)
	screen_on = val ? true : false;
#else
	pr_err("screen_on is not implemented!\n");
#endif
	if (screen_on != hypdata->screen_on) {
		hypdata->screen_on = screen_on;
		spin_unlock_irqrestore(&hypdata->lock, flags);
		complete(&hypdata->wait_event);
		pr_debug("screen is %d\n", (int)screen_on);
	} else {
		spin_unlock_irqrestore(&hypdata->lock, flags);
	}

	return NOTIFY_OK;
}
#endif /* CONFIG_DRM_MSM */

static struct notifier_block fb_nb = {
	.notifier_call = fb_cb,
	.priority = INT_MAX,
};

int __init display_info_register(struct hypnus_data *hypdata)
{
#if defined(CONFIG_ARCH_MSM) || defined(CONFIG_ARCH_QCOM)
#ifdef CONFIG_DRM_MSM
	msm_drm_register_client(&fb_nb);
#else
	fb_register_client(&fb_nb);
#endif /* CONFIG_DRM_MSM */
#elif defined(CONFIG_MTK_PLATFORM)
	//mtkfb_register_client(&fb_nb);
#else
#endif

#if defined(CONFIG_ARCH_MSM) || defined(CONFIG_ARCH_QCOM)
#ifdef CONFIG_DRM_MSM
	/*
	 * comment out this code for DRM because we met a problem.
	 * The status is wrong after device boot up util screen on/off again
	 */
	//hypdata->screen_on = (get_oppo_display_power_status() == OPPO_DISPLAY_POWER_ON);
	hypdata->screen_on = 1;
#else
	hypdata->screen_on = 1;
#endif
#endif
	return 0;
}

void __exit display_info_unregister(struct hypnus_data *hypdata)
{
#if defined(CONFIG_ARCH_MSM) || defined(CONFIG_ARCH_QCOM)
#ifdef CONFIG_DRM_MSM
	msm_drm_unregister_client(&fb_nb);
#else
	fb_unregister_client(&fb_nb);
#endif /* CONFIG_DRM_MSM */
#elif defined(CONFIG_MTK_PLATFORM)
	//mtkfb_unregister_client(&fb_nb);
#else
#endif
}
