/* ffdhub sensor driver
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
#include "ffdhub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define FFD_TAG                   "[ffdhub] "
#define FFD_FUN(f)                pr_err(FFD_TAG"%s\n", __func__)
#define FFD_PR_ERR(fmt, args...)  pr_err(FFD_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define FFD_LOG(fmt, args...)     pr_err(FFD_TAG fmt, ##args)

static struct fusion_init_info ffdhub_init_info;

// for motor down if free fall
#ifdef CONFIG_OPPO_MOTOR
extern void oppo_motor_downward(void);
#endif

static int ffd_get_data(int *x, int *y, int *z, int *scalar, int *status)
{
    int err = 0;
    struct data_unit_t data;
    uint64_t time_stamp = 0;

    err = sensor_get_data_from_hub(ID_FFD, &data);
    if (err < 0)
    {
        FFD_LOG("sensor_get_data_from_hub fail!!\n");
        return -1;
    }

    time_stamp = data.time_stamp;
    *x = data.ffd_data_t.value;
    *y = data.ffd_data_t.report_count;

    FFD_LOG("ffd value = %d, report_count = %d\n", *x, *y);

    return 0;
}

static int ffd_open_report_data(int open)
{
    return 0;
}

static int ffd_enable_nodata(int en)
{
    FFD_LOG("ffd enable nodata, en = %d\n", en);

    return sensor_enable_to_hub(ID_FFD, en);
}

static int ffd_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    unsigned int delayms = 0;

    delayms = delay / 1000 / 1000;
    return sensor_set_delay_to_hub(ID_FFD, delayms);
#elif defined CONFIG_NANOHUB
    return 0;
#else
    return 0;
#endif
}

static int ffd_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ffd_set_delay(samplingPeriodNs);
#endif

    FFD_LOG("ffd: samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",samplingPeriodNs, maxBatchReportLatencyNs);

    return sensor_batch_to_hub(ID_FFD, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int ffd_flush(void)
{
    return sensor_flush_to_hub(ID_FFD);
}

static int ffd_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    FFD_LOG("ffd recv data, flush_action = %d, ffd value = %d, report_count = %d, timestamp = %lld\n",
        event->flush_action, event->ffd_data_t.value, event->ffd_data_t.report_count, (int64_t)event->time_stamp);

#ifdef CONFIG_OPPO_MOTOR
// for motor down if freefall
    if (event->ffd_data_t.value) {
        oppo_motor_downward();
    }
#endif

    if (event->flush_action == DATA_ACTION)
    {
        ffd_data_report(event->ffd_data_t.value, event->ffd_data_t.report_count, (int64_t)event->time_stamp);
    }
    else if (event->flush_action == FLUSH_ACTION)
    {
        err = ffd_flush_report();
    }

    return err;
}

static int ffdhub_local_init(void)
{
    struct fusion_control_path ctl = {0};
    struct fusion_data_path data = {0};
    int err = 0;

    FFD_PR_ERR("ffdhub_local_init start.\n");

    ctl.open_report_data = ffd_open_report_data;
    ctl.enable_nodata = ffd_enable_nodata;
    ctl.set_delay = ffd_set_delay;
    ctl.batch = ffd_batch;
    ctl.flush = ffd_flush;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
    ctl.is_report_input_direct = true;
    ctl.is_support_batch = false;
#else
#endif

    err = fusion_register_control_path(&ctl, ID_FFD);
    if (err)
    {
        FFD_PR_ERR("register ffd control path err\n");
        goto exit;
    }

    data.get_data = ffd_get_data;
    data.vender_div = 1000;
    err = fusion_register_data_path(&data, ID_FFD);
    if (err)
    {
        FFD_PR_ERR("register ffd data path err\n");
        goto exit;
    }

    err = scp_sensorHub_data_registration(ID_FFD, ffd_recv_data);
    if (err < 0)
    {
        FFD_PR_ERR("SCP_sensorHub_data_registration failed\n");
        goto exit;
    }

    FFD_PR_ERR("ffdhub_local_init over.\n");

    return 0;
exit:
    return -1;
}

static int ffdhub_local_uninit(void)
{
    return 0;
}

static struct fusion_init_info ffdhub_init_info = {
    .name = "ffd_hub",
    .init = ffdhub_local_init,
    .uninit = ffdhub_local_uninit,
};

static int __init ffdhub_init(void)
{
    fusion_driver_add(&ffdhub_init_info, ID_FFD);
    return 0;
}

static void __exit ffdhub_exit(void)
{
    FFD_FUN();
}

module_init(ffdhub_init);
module_exit(ffdhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("zhq@oppo.com");

