/******************************************************************
** Copyright (C), 2004-2017, OPPO Mobile Comm Corp., Ltd.
** VENDOR_EDIT
** File: - oppo_sensor_devinfo.c
** Description: Source file for sensor device infomation registered in scp.
** Version: 1.0
** Date : 2017/12/12
** Author: Fei.Mo@PSW.BSP.Sensor
**
** --------------------------- Revision History: ---------------------
* <version>	<date>		<author>              		<desc>
* Revision 1.0      2017/12/12        Fei.Mo@PSW.BSP.Sensor   	Created,need to sync acp info
*******************************************************************/

#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <hwmsensor.h>
#include <SCP_sensorHub.h>
#include "SCP_power_monitor.h"
#include <soc/oppo/device_info.h>
#include "sensor_devinfo.h"
#include <linux/gpio.h>
#include "scp_reg.h"
#include "scp_ipi.h"

#ifdef VENDOR_EDIT
/* Fuchun.Liao@BSP.CHG.Basic 2018/08/08 modify for sensor workaround */
#include <mt-plat/mtk_boot_common.h>
#include <mt-plat/mtk_rtc.h>

extern bool oppo_gauge_get_batt_authenticate(void);
#endif /* VENDOR_EDIT */

#define DEV_TAG                  	"[sensor_devinf] "
#define DEVINF_FUN(f)               	pr_debug(DEV_TAG"%s\n", __func__)
#define DEVINF_ERR(fmt, args...) 	pr_err(DEV_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

#define SENSOR_DEVINFO_SYNC_TIME  5000
#define SENSOR_DEVINFO_SYNC_FIRST_TIME  1000

struct delayed_work sensor_work;

static bool accel_init = false;
static bool mag_init = false;
static bool light_init = false;
static bool gyro_init = false;

//oppo sensor list
enum {
    OPPO_UNKNOW = -1,
    OPPO_ALSPS,
    OPPO_GSENSOR,
    OPPO_MAG,
    OPPO_GYRO,
};

struct devinfo {
	int sensortype;
	char* ic_name;
	char* vendor_name;
	int ic;
};

struct sensorlist {
	int sensortype;//oppo_sensor_type 
	int ic;
};

struct sensor_cali {
	int gain_als;
	int prox_offset[6];
	int gsensor_offset[3];
	int gyro_offset[3];
} *SensorData;

struct devinfo sensorinfo[] = {
	{ID_ACCELEROMETER,"lis3dh","ST",GSENSOR_LIS3DH},
	{ID_ACCELEROMETER,"lsm6ds3","ST",GSENSOR_LSM6DS3},
	{ID_ACCELEROMETER,"lis2hh12","ST",GSENSOR_LIS2HH12},
	{ID_ACCELEROMETER,"bmi160","BOSCH",GSENSOR_BMI160},
	{ID_ACCELEROMETER,"lsm6dsm","ST",GSENSOR_LSM6DSM},
	{ID_MAGNETIC,"akm09911","AKM",MAG_AKM09911},
	{ID_MAGNETIC,"mmc3530","MICROCHIP",MAG_MMC3530},
	{ID_MAGNETIC,"mmc5603","MICROCHIP",MAG_MMC5603},
	{ID_MAGNETIC,"mxg4300","MAGNACHIP",MAG_MXG4300},
	{ID_LIGHT,"TMD2725","AMS",ALSPS_TMD2725},
	{ID_LIGHT,"apds9922","AVAGOTECH",ALSPS_APSD9922},
	{ID_LIGHT,"STK3335","SensorTek",ALSPS_STK3335},
	{ID_LIGHT,"TSL2540+STK3331","SensorTek",ALSPS_UNION_TSL2540_STK3331},
	{ID_LIGHT,"APDS9925+STK3332","SensorTek",ALSPS_MULT_APDS9925_STK3332},
	{ID_LIGHT,"STK3331-A","SensorTek",ALSPS_STK3331},
	{ID_LIGHT,"STK2232","SensorTek",ALSPS_STK2232},
	{ID_GYROSCOPE,"lsm6ds3","ST",GYRO_LSM6DS3},
	{ID_GYROSCOPE,"bmi160","BOSCH",GYRO_BMI160},
	{ID_GYROSCOPE,"lsm6dsm","ST",GYRO_LSM6DSM},
};

struct sensorlist oppo_sensor[] = {
	{OPPO_ALSPS,ALSPS_UNKNOW},
	{OPPO_GSENSOR,GSENSOR_UNKNOW},
	{OPPO_MAG,MAG_UNKNOW},
	{OPPO_GYRO,GYRO_UNKNOW},
};

static void parse_sensor_devinfo(int32_t info)
{
	int32_t devinfo = info;
	int i = 0;

	for (i = 0; i < sizeof(sensorinfo)/sizeof(struct devinfo); i++) {
		devinfo = info;
		if (sensorinfo[i].sensortype == ID_ACCELEROMETER) {
			if (accel_init) {
				continue;
			}
			devinfo &= 0x00f00000;
			devinfo = devinfo >> 20;
			if (devinfo == sensorinfo[i].ic) {
				DEVINF_ERR("%s\n",sensorinfo[i].ic_name);
				register_device_proc("Sensor_accel", sensorinfo[i].ic_name, sensorinfo[i].vendor_name);
				oppo_sensor[OPPO_GSENSOR].ic = sensorinfo[i].ic;
				accel_init = true;
			}
		} else if (sensorinfo[i].sensortype == ID_MAGNETIC) {
			if (mag_init) {
				continue;
			}
			devinfo &= 0x000f0000;
			devinfo = devinfo >> 16;
			if (devinfo == sensorinfo[i].ic) {
				DEVINF_ERR("%s\n",sensorinfo[i].ic_name);
				register_device_proc("Sensor_msensor", sensorinfo[i].ic_name, sensorinfo[i].vendor_name);
				oppo_sensor[OPPO_MAG].ic = sensorinfo[i].ic;
				mag_init = true;
			}
		} else if (sensorinfo[i].sensortype == ID_LIGHT) {
			if (light_init) {
				continue;
			}
			devinfo &= 0x0f000000;
			devinfo = devinfo >> 24;
			if (devinfo == sensorinfo[i].ic) {
				DEVINF_ERR("%s\n",sensorinfo[i].ic_name);
				register_device_proc("Sensor_alsps", sensorinfo[i].ic_name, sensorinfo[i].vendor_name);
				oppo_sensor[OPPO_ALSPS].ic = sensorinfo[i].ic;
				light_init = true;
			}
		} else if (sensorinfo[i].sensortype == ID_GYROSCOPE) {
			if (gyro_init) {
				continue;
			}
			devinfo &= 0x0000f000;
			devinfo = devinfo >> 12;
			if (devinfo == sensorinfo[i].ic) {
				DEVINF_ERR("%s\n",sensorinfo[i].ic_name);
				register_device_proc("Sensor_gyro", sensorinfo[i].ic_name, sensorinfo[i].vendor_name);
				oppo_sensor[OPPO_GYRO].ic = sensorinfo[i].ic;
				gyro_init = true;
			}
		}
	}

}

static unsigned int init_sensor_calibraiton_paramater(void)
{
	struct device_node *np = NULL;
	int ret = 0;
	int i = 0;
	u32 sign[3] = {0};
	u32 prox_offset[6] = {0};
	u32 gsensor_offset[3] = {0};
	u32 gyro_offset[3] = {0};

	if (!SensorData) {
		DEVINF_ERR("SensorData NULL\n ");
		return -ENOMEM;
	}

	np = of_find_node_by_name(NULL,"oppo_sensor");
	if(!np){
		DEVINF_ERR("error1\n");
		return 0;
	}
	//light
	ret = of_property_read_u32(np,"gain_als",&(SensorData->gain_als));
	if(ret) {
		DEVINF_ERR("error2\n");
	}
	//prox
	ret = of_property_read_u32_array(np, "prox_offset", prox_offset, ARRAY_SIZE(prox_offset));
	if (ret) {
		DEVINF_ERR("error3 %d\n",ret);
	} else {
		for (i = 0; i < 6; i ++) {
			SensorData->prox_offset[i] = prox_offset[i];
		}
	}
	//gsensor
	ret = of_property_read_u32_array(np, "gsensor_offset", gsensor_offset, ARRAY_SIZE(gsensor_offset));
	if(ret) {
		DEVINF_ERR("error4 %d\n",ret);
	} else {
		for (i = 0;i < 3 ;i ++) {
			SensorData->gsensor_offset[i] = gsensor_offset[i];
		}
	}

	ret = of_property_read_u32_array(np, "gsensor_sign",sign, ARRAY_SIZE(sign));
	if(ret) {
		DEVINF_ERR("error5 %d\n",ret);
	}
	for (i = 0;i < 3 ;i ++) {
		if (sign[i])
			SensorData->gsensor_offset[i] = (-1) * SensorData->gsensor_offset[i];
	}
	DEVINF_ERR("gsensor_sign(%d %d %d)\n",sign[0],sign[1],sign[2]);
	//gyro
	ret = of_property_read_u32_array(np, "gyro_offset", gyro_offset, ARRAY_SIZE(gyro_offset));
	if(ret) {
		DEVINF_ERR("error6 %d\n",ret);
	} else {
		for (i = 0;i < 3 ;i ++) {
			SensorData->gyro_offset[i] = gyro_offset[i];
		}
	}

	for (i = 0;i < 3 ;i ++) {
		sign[i] = 0;
	}

	ret = of_property_read_u32_array(np, "gyro_sign",sign, ARRAY_SIZE(sign));
	if(ret) {
		DEVINF_ERR("error7 %d\n",ret);
	}
	for (i = 0;i < 3 ;i ++) {
		if (sign[i])
			SensorData->gyro_offset[i] = (-1) * SensorData->gyro_offset[i];
	}
	DEVINF_ERR("gyro_sign(%d %d %d)\n",sign[0],sign[1],sign[2]);

	DEVINF_ERR("gain_als(%d) prox_offset(%d %d %d, %d %d %d) gsensor_offset(%d %d %d) gyro_offset(%d %d %d)\n",
		SensorData->gain_als,
		SensorData->prox_offset[0],SensorData->prox_offset[1],SensorData->prox_offset[2],
		SensorData->prox_offset[3],SensorData->prox_offset[4],SensorData->prox_offset[5],
		SensorData->gsensor_offset[0],SensorData->gsensor_offset[1],SensorData->gsensor_offset[2],
		SensorData->gyro_offset[0],SensorData->gyro_offset[1],SensorData->gyro_offset[2]);

	return 1;
}

int update_sensor_parameter(int sensortye ,int * data)
{
	if (!SensorData || !data) {
		DEVINF_ERR("SensorData NULL\n ");
		return -ENOMEM;
	}

	if (ID_ACCELEROMETER == sensortye) {
		SensorData->gsensor_offset[0] = data[0];
		SensorData->gsensor_offset[1] = data[1];
		SensorData->gsensor_offset[2] = data[2];
	} else if (ID_LIGHT == sensortye) {
		SensorData->gain_als = *data;
	} else if (ID_PROXIMITY == sensortye) {
		SensorData->prox_offset[0] = data[0];
		SensorData->prox_offset[1] = data[1];
		SensorData->prox_offset[2] = data[2];
		SensorData->prox_offset[3] = data[3];
		SensorData->prox_offset[4] = data[4];
		SensorData->prox_offset[5] = data[5];
	} else if (ID_GYROSCOPE == sensortye) {
		SensorData->gyro_offset[0] = data[0];
		SensorData->gyro_offset[1] = data[1];
		SensorData->gyro_offset[2] = data[2];
	}
	DEVINF_ERR("gain_als(%d) prox_offset(%d %d %d, %d %d %d) gsensor_offset(%d %d %d) gyro_offset(%d %d %d)\n",
		SensorData->gain_als,
		SensorData->prox_offset[0],SensorData->prox_offset[1], SensorData->prox_offset[2],
		SensorData->prox_offset[3],SensorData->prox_offset[4], SensorData->prox_offset[5],
		SensorData->gsensor_offset[0],SensorData->gsensor_offset[1],SensorData->gsensor_offset[2],
		SensorData->gyro_offset[0],SensorData->gyro_offset[1],SensorData->gyro_offset[2]);

	return 0;

}

int get_sensor_parameter(int sensortye ,int * data)
{
	if (!SensorData || !data) {
		DEVINF_ERR("SensorData NULL\n ");
		return -ENOMEM;
	}

	if (ID_ACCELEROMETER == sensortye) {
		data[0] = SensorData->gsensor_offset[0];
		data[1] = SensorData->gsensor_offset[1];
		data[2] = SensorData->gsensor_offset[2];
	} else if (ID_LIGHT == sensortye) {
		if (SensorData->gain_als == 0)
			SensorData->gain_als = 1000;
		*data = SensorData->gain_als;
	} else if (ID_PROXIMITY == sensortye) {
		data[0] = SensorData->prox_offset[0];
		data[1] = SensorData->prox_offset[1];
		data[2] = SensorData->prox_offset[2];
		data[3] = SensorData->prox_offset[3];
		data[4] = SensorData->prox_offset[4];
		data[5] = SensorData->prox_offset[5];
	} else if (ID_GYROSCOPE == sensortye) {
		data[0] = SensorData->gyro_offset[0];
		data[1] = SensorData->gyro_offset[1];
		data[2] = SensorData->gyro_offset[2];
	}

	return 0;

}

int get_sensor_name(int sensortye)
{
	if (ID_ACCELEROMETER == sensortye) {
		return oppo_sensor[OPPO_GSENSOR].ic;
	} else if (ID_LIGHT == sensortye) {
		return oppo_sensor[OPPO_ALSPS].ic;
	} else if (ID_PROXIMITY == sensortye) {
		return oppo_sensor[OPPO_ALSPS].ic;
	} else if (ID_GYROSCOPE == sensortye) {
		return oppo_sensor[OPPO_GYRO].ic;
	} else if (ID_MAGNETIC == sensortye) {
		return oppo_sensor[OPPO_MAG].ic;
	}

	return 0;//unknow
}
static int sensor_i2c_gpios[2] = {81, 84};
static int soft_reset_flag = 0;
extern void scp_wdt_reset(enum scp_core_id cpu_id);

static void sensor_dev_work(struct work_struct *work)
{
	int err = 0;
	static int retry = 0;
	struct data_unit_t data;
#ifndef ODM_WT_EDIT
// Jixiaopan@ODM_WT.BSP.Sensors.Config, 2019/11/09, Add for bringup sensors
	int temp_cali[6] = {0};
    int prox_cali_to_scp[3] = {0};
#endif/*ODM_WT_EDIT*/
	int i;

	err = sensor_get_data_from_hub(ID_OPPO_SENSOR, &data);

	DEVINF_ERR("0x%x, err %d\n",data.data[0],err);

	if (!err)
	{
		parse_sensor_devinfo(data.data[0]);
#ifdef VENDOR_EDIT
/* Fuchun.Liao@BSP.CHG.Basic 2018/08/08 modify for sensor i2c error workaround */
		if (!light_init && !mag_init)
		{
			if(get_boot_mode() == NORMAL_BOOT
#ifdef ODM_WT_EDIT
/* Sidong.Zhao@BSP.CHG.Basic 2019/11/05 cause kernel panic */
			&& (0)
#else		
			&& oppo_gauge_get_batt_authenticate() == true
#endif /*ODM_WT_EDIT*/
			&& oppo_get_rtc_sensor_cause_panic_value() == 0)
			{
				for (i = 0; i < ARRAY_SIZE(sensor_i2c_gpios); i++)
				{
					err = gpio_request(sensor_i2c_gpios[i], NULL);
					if (!err)
					{
						gpio_direction_output(sensor_i2c_gpios[i], 0);
						mdelay(10);
						gpio_direction_output(sensor_i2c_gpios[i], 1);
					}
				}
				mdelay(10);
				oppo_rtc_mark_sensor_cause_panic();
				panic("ALSPS&Mag init fail\n");
			} else {
				if (soft_reset_flag == 0)
				{
					scp_wdt_reset(SCP_A_ID);
					retry--;
					err = -1;
					soft_reset_flag = 0xFF;
					pr_err("soft_reset_flag : warm reset scp one time\n");
					goto RETRY_GET_SENSOR;
				}
				pr_err("rtc_sensor_cause_panic have occured\n");
			}
		}
		oppo_clear_rtc_sensor_cause_panic();
#endif /* VENDOR_EDIT */
#ifdef ODM_WT_EDIT
// Jixiaopan@ODM_WT.BSP.Sensors.Config, 2019/11/09, Add for bringup sensors
		DEVINF_ERR("nothing to do\n");
#else
		get_sensor_parameter(ID_LIGHT, temp_cali);
		err = sensor_set_cmd_to_hub(ID_LIGHT, CUST_ACTION_SET_CALI, (void*)&temp_cali[0]);
		DEVINF_ERR("set als factory cali=%d, res=%d\n",temp_cali[0], err);
		msleep(20);
		get_sensor_parameter(ID_PROXIMITY, temp_cali);
		if (temp_cali[0] >= 0)
		{
			prox_cali_to_scp[0] = (temp_cali[3] << 16) | temp_cali[0];
			prox_cali_to_scp[1] = (temp_cali[4] << 16) | temp_cali[1];
			prox_cali_to_scp[2] = (temp_cali[5] << 16) | temp_cali[2];

			err = sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_SET_CALI, (void*)prox_cali_to_scp);
			DEVINF_ERR("set ps factory cali (%d %d %d, %d %d %d), res=%d\n", temp_cali[0], temp_cali[1], temp_cali[2], temp_cali[3], temp_cali[4], temp_cali[5], err);
		}
#endif/*ODM_WT_EDIT*/
	}
RETRY_GET_SENSOR:
	if ((retry < 3) && err) {
		schedule_delayed_work(&sensor_work, msecs_to_jiffies(SENSOR_DEVINFO_SYNC_TIME));
		retry ++;
	}
}

static int __init sensordev_init(void)
{
	struct sensor_cali *Data = kzalloc(sizeof(struct sensor_cali), GFP_KERNEL);
	DEVINF_ERR("sensordev_init data=%p\n",Data);
	if (!Data) {
		DEVINF_ERR("kzalloc err\n ");
		return -ENOMEM;
	}

	SensorData = Data;

	DEVINF_ERR("init_sensor_calibraiton_paramater\n");

	init_sensor_calibraiton_paramater();

	INIT_DELAYED_WORK(&sensor_work, sensor_dev_work);
#ifndef ODM_WT_EDIT
//Li Tao@ODM_WT.BSP.Sensors.Config, 2020/03/10, Add for fixing crash issue
	schedule_delayed_work(&sensor_work, msecs_to_jiffies(SENSOR_DEVINFO_SYNC_FIRST_TIME));
#endif
	return 0;
}

late_initcall(sensordev_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sensor devinfo");
MODULE_AUTHOR("Fei.Mo@oppo.com");
