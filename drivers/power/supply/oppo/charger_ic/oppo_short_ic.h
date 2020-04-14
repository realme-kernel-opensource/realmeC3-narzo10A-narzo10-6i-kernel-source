/**********************************************************************************
* Copyright (c)  2017-2019  Guangdong OPPO Mobile Comm Corp., Ltd
* VENDOR_EDIT
* Description: For short circuit battery check
* Version   : 1.0
* Date      : 2018-05-24
* Author    : tongfeng.Huang@PhoneSW.BSP		   	
* ------------------------------ Revision History: --------------------------------
* <version>       <date>        	<author>              		<desc>
* Revision 1.0    2018-05-24  	tongfeng.Huang@PhoneSW.BSP    		Created for new short IC
***********************************************************************************/

#ifndef _OPPO_SHORT_IC_H_
#define _OPPO_SHORT_IC_H_


#define OPPO_SHORT_IC_CHIP_ID_REG					0x00
#define OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_REG		0x02
#define OPPO_SHORT_IC_WORK_MODE_REG					0x03
#define OPPO_SHORT_IC_OTP_REG						0x08

#define OPPO_SHORT_IC_TEMP_VOLT_DROP_THRESH_VAL		0x44
struct oppo_short_ic{
	struct i2c_client				 *client;
	struct device					 *dev;
	struct delayed_work 			 oppo_short_ic_init_work;

	bool 							 b_oppo_short_ic_exist;
	bool							 b_factory_id_get;
	int								 volt_drop_threshold;
	bool							 b_volt_drop_set;
	bool							 b_work_mode_set;
	int								 otp_error_cnt;
	int								 otp_error_value;
    atomic_t                         suspended;
};

bool oppo_short_ic_otp_check(void);
int oppo_short_ic_set_volt_threshold(struct oppo_chg_chip *chip);
bool oppo_short_ic_is_exist(struct oppo_chg_chip *chip);
int oppo_short_ic_get_otp_error_value(struct oppo_chg_chip *chip);

#endif

