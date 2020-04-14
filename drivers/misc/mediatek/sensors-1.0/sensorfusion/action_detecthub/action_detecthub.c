/* free_fallhub motion sensor driver
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
#include <hwmsensor.h>
#include "action_detecthub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define ACTION_DETECT_TAG                   "[action_detecthub] "
#define ACTION_DETECT_FUN(f)                pr_err(ACTION_DETECT_TAG"%s\n", __func__)
#define ACTION_DETECT_PR_ERR(fmt, args...)  pr_err(ACTION_DETECT_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define ACTION_DETECT_LOG(fmt, args...)     pr_err(ACTION_DETECT_TAG fmt, ##args)

static struct fusion_init_info action_detecthub_init_info;

static int action_detect_get_data(int *x, int *y, int *z, int *scalar, int *status)
{
    int err = 0;
    struct data_unit_t data;
    uint64_t time_stamp = 0;

    err = sensor_get_data_from_hub(ID_ACTION_DETECT, &data);
    if (err < 0)
    {
        ACTION_DETECT_LOG("sensor_get_data_from_hub fail!!\n");
        return -1;
    }

    time_stamp = data.time_stamp;
    *x = data.action_detect_data_t.value;
    *y = data.action_detect_data_t.report_count;

    ACTION_DETECT_LOG("value = %d, report_count = %d\n", *x, *y);

    return 0;
}

static int action_detect_open_report_data(int open)
{
    return 0;
}

static int action_detect_enable_nodata(int en)
{
    ACTION_DETECT_LOG("enable nodata, en = %d\n", en);

    return sensor_enable_to_hub(ID_ACTION_DETECT, en);
}

static int action_detect_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return sensor_set_delay_to_hub(ID_ACTION_DETECT, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int action_detect_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    action_detect_set_delay(samplingPeriodNs);
#endif

    ACTION_DETECT_LOG("samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",samplingPeriodNs, maxBatchReportLatencyNs);

    return sensor_batch_to_hub(ID_ACTION_DETECT, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int action_detect_flush(void)
{
    return sensor_flush_to_hub(ID_ACTION_DETECT);
}

static int action_detect_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    ACTION_DETECT_LOG("recv data, flush_action = %d, value = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action, event->action_detect_data_t.value, event->action_detect_data_t.report_count, (int64_t)event->time_stamp);


    if (event->flush_action == DATA_ACTION)
    {
        action_detect_data_report(event->action_detect_data_t.value, event->action_detect_data_t.report_count, (int64_t)event->time_stamp);
    }
    else if (event->flush_action == FLUSH_ACTION)
    {
        err = action_detect_flush_report();
    }

    return err;
}

static int action_detecthub_local_init(void)
{
    struct fusion_control_path ctl = {0};
    struct fusion_data_path data = {0};
    int err = 0;

    ctl.open_report_data = action_detect_open_report_data;
    ctl.enable_nodata = action_detect_enable_nodata;
    ctl.set_delay = action_detect_set_delay;
    ctl.batch = action_detect_batch;
    ctl.flush = action_detect_flush;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
    #ifdef VENDOR_EDIT
    //ctl.is_support_wake_lock = true;
    #endif
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
    #ifdef VENDOR_EDIT
    //ctl.is_support_wake_lock = true;
    #endif
#else
#endif

    err = fusion_register_control_path(&ctl, ID_ACTION_DETECT);
    if (err)
    {
        ACTION_DETECT_LOG("register action_detect control path err\n");
        goto exit;
    }

    data.get_data = action_detect_get_data;
    data.vender_div = 1000;
    err = fusion_register_data_path(&data, ID_ACTION_DETECT);
    if (err)
    {
        ACTION_DETECT_LOG("register action_detect data path err\n");
        goto exit;
    }

    err = scp_sensorHub_data_registration(ID_ACTION_DETECT, action_detect_recv_data);
    if (err < 0)
    {
        ACTION_DETECT_LOG("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
    return 0;
exit:
    return -1;
}

static int action_detecthub_local_uninit(void)
{
    return 0;
}

static struct fusion_init_info action_detecthub_init_info = {
    .name = "action_detect_hub",
    .init = action_detecthub_local_init,
    .uninit = action_detecthub_local_uninit,
};

static int __init action_detecthub_init(void)
{
    fusion_driver_add(&action_detecthub_init_info, ID_ACTION_DETECT);
    return 0;
}

static void __exit action_detecthub_exit(void)
{
    ACTION_DETECT_FUN();
}

module_init(action_detecthub_init);
module_exit(action_detecthub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("cheny@oppo.com");

