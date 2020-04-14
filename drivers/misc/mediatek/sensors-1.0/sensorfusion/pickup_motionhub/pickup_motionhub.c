/* pickup_motionhub sensor driver
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
#include "pickup_motionhub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define PICKUP_MOTION_TAG                   "[pickup_motionhub] "
#define PICKUP_MOTION_FUN(f)                pr_err(PICKUP_MOTION_TAG"%s\n", __func__)
#define PICKUP_MOTION_PR_ERR(fmt, args...)  pr_err(PICKUP_MOTION_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define PICKUP_MOTION_LOG(fmt, args...)     pr_err(PICKUP_MOTION_TAG fmt, ##args)

static struct fusion_init_info pickup_motionhub_init_info;

static int pickup_motion_get_data(int *x, int *y, int *z, int *scalar, int *status)
{
    int err = 0;
    struct data_unit_t data;
    uint64_t time_stamp = 0;

    err = sensor_get_data_from_hub(ID_PICKUP_MOTION, &data);
    if (err < 0)
    {
        PICKUP_MOTION_LOG("sensor_get_data_from_hub fail!!\n");
        return -1;
    }

    time_stamp = data.time_stamp;
    *x = data.pickup_motion_data_t.value;
    *y = data.pickup_motion_data_t.report_count;

    PICKUP_MOTION_LOG("pickup_motion value = %d, report_count = %d\n", *x, *y);

    return 0;
}

static int pickup_motion_open_report_data(int open)
{
    return 0;
}

static int pickup_motion_enable_nodata(int en)
{
    PICKUP_MOTION_LOG("pickup_motion enable nodata, en = %d\n", en);

    return sensor_enable_to_hub(ID_PICKUP_MOTION, en);
}

static int pickup_motion_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return sensor_set_delay_to_hub(ID_PICKUP_MOTION, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int pickup_motion_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    pickup_motion_set_delay(samplingPeriodNs);
#endif

    PICKUP_MOTION_LOG("pickup_motion: samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",samplingPeriodNs, maxBatchReportLatencyNs);

    return sensor_batch_to_hub(ID_PICKUP_MOTION, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int pickup_motion_flush(void)
{
    return sensor_flush_to_hub(ID_PICKUP_MOTION);
}

static int pickup_motion_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    PICKUP_MOTION_LOG("pickup_motion recv data, flush_action = %d, pickup_motion value = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action, event->pickup_motion_data_t.value, event->pickup_motion_data_t.report_count, (int64_t)event->time_stamp);

    if (event->flush_action == DATA_ACTION)
    {
        pickup_motion_data_report(event->pickup_motion_data_t.value, event->pickup_motion_data_t.report_count, (int64_t)event->time_stamp);
    }
    else if (event->flush_action == FLUSH_ACTION)
    {
        err = pickup_motion_flush_report();
    }

    return err;
}

static int pickup_motionhub_local_init(void)
{
    struct fusion_control_path ctl = {0};
    struct fusion_data_path data = {0};
    int err = 0;

    ctl.open_report_data = pickup_motion_open_report_data;
    ctl.enable_nodata = pickup_motion_enable_nodata;
    ctl.set_delay = pickup_motion_set_delay;
    ctl.batch = pickup_motion_batch;
    ctl.flush = pickup_motion_flush;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#else
#endif

    err = fusion_register_control_path(&ctl, ID_PICKUP_MOTION);
    if (err)
    {
        PICKUP_MOTION_PR_ERR("register pickup_motion control path err\n");
        goto exit;
    }

    data.get_data = pickup_motion_get_data;
    data.vender_div = 1000;
    err = fusion_register_data_path(&data, ID_PICKUP_MOTION);
    if (err)
    {
        PICKUP_MOTION_PR_ERR("register pickup_motion data path err\n");
        goto exit;
    }

    err = scp_sensorHub_data_registration(ID_PICKUP_MOTION, pickup_motion_recv_data);
    if (err < 0)
    {
        PICKUP_MOTION_PR_ERR("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }
    return 0;
exit:
    return -1;
}

static int pickup_motionhub_local_uninit(void)
{
    return 0;
}

static struct fusion_init_info pickup_motionhub_init_info = {
    .name = "pickup_motion_hub",
    .init = pickup_motionhub_local_init,
    .uninit = pickup_motionhub_local_uninit,
};

static int __init pickup_motionhub_init(void)
{
    fusion_driver_add(&pickup_motionhub_init_info, ID_PICKUP_MOTION);
    return 0;
}

static void __exit pickup_motionhub_exit(void)
{
    PICKUP_MOTION_FUN();
}

module_init(pickup_motionhub_init);
module_exit(pickup_motionhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("zhq@oppo.com");

