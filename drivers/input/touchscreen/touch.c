/***************************************************
 * File:touch.c
 * VENDOR_EDIT
 * Copyright (c)  2008- 2030  Oppo Mobile communication Corp.ltd.
 * Description:
 *             tp dev
 * Version:1.0:
 * Date created:2016/09/02
 * Author: hao.wang@Bsp.Driver
 * TAG: BSP.TP.Init
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include "oppo_touchscreen/tp_devices.h"
#include "oppo_touchscreen/touchpanel_common.h"
#include <soc/oppo/oppo_project.h>
#include "touch.h"

#define MAX_LIMIT_DATA_LENGTH         100

#define HX83112A_NF_CHIP_NAME "HX83112A_NF"
#define HX83112B_NF_CHIP_NAME "HX83112B_NF"
#define TD4330_NF_CHIP_NAME "TD4330_NF"
#define NT36672A_NF_CHIP_NAME "NT36672A_NF"
#define NT36672_NF_CHIP_NAME "NT_NF36672"
#define TD4320_NF_CHIP_NAME "TD4320_NF"
/*if can not compile success, please update vendor/oppo_touchsreen*/
struct tp_dev_name tp_dev_names[] = {
     {TP_OFILM, "OFILM"},
     {TP_BIEL, "BIEL"},
     {TP_TRULY, "TRULY"},
     {TP_BOE, "BOE"},
     {TP_G2Y, "G2Y"},
     {TP_TPK, "TPK"},
     {TP_JDI, "JDI"},
     {TP_TIANMA, "TIANMA"},
     {TP_SAMSUNG, "SAMSUNG"},
     {TP_DSJM, "DSJM"},
     {TP_BOE_B8, "BOEB8"},
     {TP_INNOLUX, "INNOLUX"},
     {TP_HIMAX_DPT, "DPT"},
     {TP_AUO, "AUO"},
     {TP_DEPUTE, "DEPUTE"},
     {TP_HUAXING, "HUAXING"},
     {TP_HLT, "HLT"},
     {TP_DJN, "DJN"},
     {TP_UNKNOWN, "UNKNOWN"},
};

#define GET_TP_DEV_NAME(tp_type) ((tp_dev_names[tp_type].type == (tp_type))?tp_dev_names[tp_type].name:"UNMATCH")

int g_tp_dev_vendor = TP_UNKNOWN;
char *g_tp_chip_name;
static bool is_tp_type_got_in_match = false;    /*indicate whether the tp type is got in the process of ic match*/

/*
* this function is used to judge whether the ic driver should be loaded
* For incell module, tp is defined by lcd module, so if we judge the tp ic
* by the boot command line of containing lcd string, we can also get tp type.
*/
bool __init tp_judge_ic_match(char * tp_ic_name)
{
    pr_err("[TP] tp_ic_name = %s \n", tp_ic_name);
    pr_err("[TP] boot_command_line = %s \n", boot_command_line);

/*
	is_tp_type_got_in_match = true;
	g_tp_dev_vendor = TP_DSJM;
#ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
	g_tp_chip_name = kzalloc(sizeof(TD4330_NF_CHIP_NAME), GFP_KERNEL);
	memcpy(g_tp_chip_name, TD4330_NF_CHIP_NAME, sizeof(TD4330_NF_CHIP_NAME));
#endif

return true;
*/




    switch(get_project()){
    case 18531:
    case 18561:
    case 18161:
        is_tp_type_got_in_match = true;
        if (strstr(tp_ic_name, "hx83112a_nf") && strstr(boot_command_line, "dsjm_himax83112")) {
            g_tp_dev_vendor = TP_DSJM;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(HX83112A_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, HX83112A_NF_CHIP_NAME, sizeof(HX83112A_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "hx83112b_nf") && strstr(boot_command_line, "djn_jdi_himax83112b")) {
            g_tp_dev_vendor = TP_DJN;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(HX83112B_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, HX83112B_NF_CHIP_NAME, sizeof(HX83112B_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "hx83112b_nf") && strstr(boot_command_line, "dsjm_jdi_himax83112b")) {
            g_tp_dev_vendor = TP_DSJM;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(HX83112B_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, HX83112B_NF_CHIP_NAME, sizeof(HX83112B_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "hx83112a_nf") && strstr(boot_command_line, "jdi_himax83112a")) {
            g_tp_dev_vendor = TP_JDI;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(HX83112A_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, HX83112A_NF_CHIP_NAME, sizeof(HX83112A_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "hx83112a_nf") && strstr(boot_command_line, "tm_himax83112")) {
            g_tp_dev_vendor = TP_TIANMA;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(HX83112A_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, HX83112A_NF_CHIP_NAME, sizeof(HX83112A_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "hx83112a_nf") && strstr(boot_command_line, "tianma_himax83112a")) {
            g_tp_dev_vendor = TP_TIANMA;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(HX83112A_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, HX83112A_NF_CHIP_NAME, sizeof(HX83112A_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "td4320_nf") && strstr(boot_command_line, "dsjm_jdi_td4330")) {
            g_tp_dev_vendor = TP_DSJM;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(TD4330_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, TD4330_NF_CHIP_NAME, sizeof(TD4330_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "td4320_nf") && strstr(boot_command_line, "dpt_jdi_td4330")) {
            g_tp_dev_vendor = TP_HIMAX_DPT;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(TD4330_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, TD4330_NF_CHIP_NAME, sizeof(TD4330_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "td4320_nf") && strstr(boot_command_line, "tianma_td4330")) {
            g_tp_dev_vendor = TP_TIANMA;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(TD4330_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, TD4330_NF_CHIP_NAME, sizeof(TD4330_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "tm_nt36670a")) {
            g_tp_dev_vendor = TP_TIANMA;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(NT36672A_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, NT36672A_NF_CHIP_NAME, sizeof(NT36672A_NF_CHIP_NAME));
            #endif
            return true;
        }
        break;
	case 18011:
    case 18311:
    case 18611:	
        is_tp_type_got_in_match = true;
        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "tianma_nt36672")) {
            g_tp_dev_vendor = TP_TIANMA;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(NT36672_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, NT36672_NF_CHIP_NAME, sizeof(NT36672_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "depute_nt36672")) {
            g_tp_dev_vendor = TP_DEPUTE;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(NT36672_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, NT36672_NF_CHIP_NAME, sizeof(NT36672_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "hx83112a_nf") && strstr(boot_command_line, "dsjm_himax83112")) {
            g_tp_dev_vendor = TP_DSJM;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(HX83112A_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, HX83112A_NF_CHIP_NAME, sizeof(HX83112A_NF_CHIP_NAME));
            #endif
            return true;
        }
        if (strstr(tp_ic_name, "td4320_nf") && strstr(boot_command_line, "truly_td4320")) {
            g_tp_dev_vendor = TP_TRULY;
            #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
            g_tp_chip_name = kzalloc(sizeof(TD4320_NF_CHIP_NAME), GFP_KERNEL);
            memcpy(g_tp_chip_name, TD4320_NF_CHIP_NAME, sizeof(TD4320_NF_CHIP_NAME));
            #endif
            return true;
        }
		break;
	case 17331:
        is_tp_type_got_in_match = true;
        if (strstr(tp_ic_name, "td4330") && strstr(boot_command_line, "jdi_td4330")) {
            g_tp_dev_vendor = TP_JDI;
            return true;
        }

        if (strstr(tp_ic_name, "himax83112b") && strstr(boot_command_line, "himax83112b")) {
            g_tp_dev_vendor = TP_HIMAX_DPT;
            return true;
        }

        if (strstr(tp_ic_name, "td4310") && strstr(boot_command_line, "boe")) {
            g_tp_dev_vendor = TP_BOE;
            return true;
        }

        if (strstr(tp_ic_name, "td4310") && strstr(boot_command_line, "truly")) {
            g_tp_dev_vendor = TP_TRULY;
            return true;
        }

        if (strstr(tp_ic_name, "td4310") && strstr(boot_command_line, "tianma_td4310")) {
            g_tp_dev_vendor = TP_TIANMA;
            return true;
        }

        if (strstr(tp_ic_name, "td4310") && strstr(boot_command_line, "dsjm")) {
            g_tp_dev_vendor = TP_DSJM;
            return true;
        }

        if (strstr(tp_ic_name, "td4310") && strstr(boot_command_line, "jdi_td4310")) {
            g_tp_dev_vendor = TP_JDI;
            return true;
        }

        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "tianma_nt36672")) {
            g_tp_dev_vendor = TP_TIANMA;
            return true;
        }
        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "jdi_nt36672")) {
            g_tp_dev_vendor = TP_JDI;
            return true;
        }
        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "dsjm_nt36672")) {
            g_tp_dev_vendor = TP_JDI;
            return true;
        }
        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "auo_nt36672")) {
            g_tp_dev_vendor = TP_AUO;
            return true;
        }
        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "depute_nt36672")) {
            g_tp_dev_vendor = TP_DEPUTE;
            return true;
        }
        break;	
    case 17061:
        is_tp_type_got_in_match = true;
        if (strstr(tp_ic_name, "td4310") && strstr(boot_command_line, "jdi_td4310")) {
            g_tp_dev_vendor = TP_JDI;
            return true;
        }

        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "truly_nt36672")) {
            g_tp_dev_vendor = TP_TRULY;
            return true;
        }
        break;
    case 17175:
        is_tp_type_got_in_match = true;
        if (strstr(tp_ic_name, "td4330") && strstr(boot_command_line, "jdi_td4330")) {
            g_tp_dev_vendor = TP_JDI;
            return true;
        }

        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "tianma_nt36672")) {
            g_tp_dev_vendor = TP_TIANMA;
            return true;
        }

        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "jdi_nt36672")) {
            g_tp_dev_vendor = TP_JDI;
            return true;
        }

        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "dsjm_nt36672")) {
            g_tp_dev_vendor = TP_JDI;
            return true;
        }
        if (strstr(tp_ic_name, "nt36672") && strstr(boot_command_line, "auo_nt36672")) {
            g_tp_dev_vendor = TP_AUO;
            return true;
        }
        if (strstr(tp_ic_name, "himax83112b") && strstr(boot_command_line, "jdi_hx83112a")) {
            g_tp_dev_vendor = TP_HIMAX_DPT;
            return true;
        }
        break;
    default:
        pr_err("Invalid project\n");
        break;
    }

    pr_err("Lcd module not found\n");
    return false;

   
}

/*
* For separate lcd and tp, tp can be distingwished by gpio pins
* different project may have different combination, if needed,
* add your combination with project distingwished by is_project function.
*/
static void tp_get_vendor_via_pin(struct hw_resource *hw_res, struct panel_info *panel_data)
{
    panel_data->tp_type = TP_UNKNOWN;
    return;
}

/*
* If no gpio pins used to distingwish tp module, maybe have other ways(like command line)
* add your way of getting vendor info with project distingwished by is_project function.
*/
static void tp_get_vendor_separate(struct hw_resource *hw_res, struct panel_info *panel_data)
{
    if (is_project(OPPO_17197)) {
        panel_data->tp_type = TP_SAMSUNG;
        if (strstr(boot_command_line, "samsung_ams628nw_lsi")) {
            memcpy(panel_data->manufacture_info.version, "0xDB091G", 8);
        }
        return;
    }

    panel_data->tp_type = TP_UNKNOWN;
    return;
}

int tp_util_get_vendor(struct hw_resource *hw_res, struct panel_info *panel_data)
{
    char* vendor;

    #ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
    if (g_tp_chip_name != NULL) {
        panel_data->chip_name = g_tp_chip_name;
    }
    #endif
    panel_data->test_limit_name = kzalloc(MAX_LIMIT_DATA_LENGTH, GFP_KERNEL);
    if (panel_data->test_limit_name == NULL) {
        pr_err("[TP]panel_data.test_limit_name kzalloc error\n");
    }

    panel_data->extra= kzalloc(MAX_LIMIT_DATA_LENGTH, GFP_KERNEL);
    if (panel_data->extra == NULL) {
        pr_err("[TP]panel_data.test_limit_name kzalloc error\n");
    }

    /*TP is first distingwished by gpio pins, and then by other ways*/
    if (is_tp_type_got_in_match) {
/*
	memcpy(panel_data->manufacture_info.version, "0xDD075A", 8);

*/		
        panel_data->tp_type = g_tp_dev_vendor;
        if (is_project(OPPO_17331)) {
            if (strstr(boot_command_line, "tianma_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xDC0662", 8);
            } else if (strstr(boot_command_line, "jdi_nt36672") || strstr(boot_command_line, "dsjm_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xDC0661", 8);
            } else if (strstr(boot_command_line, "auo_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xDC0663", 8);
            } else {
                pr_err("dismatch TP vendor\n");
            }
        }
        if(is_project(OPPO_17061)) {
            if (strstr(boot_command_line, "truly_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xCC0273", 8);
            }
        }
        if(is_project(OPPO_17175)) {
            if (strstr(boot_command_line, "tianma_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xDC0982", 8);
            }else if (strstr(boot_command_line, "jdi_nt36672") || strstr(boot_command_line, "dsjm_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xDC0981", 8);
            }else if (strstr(boot_command_line, "auo_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xDC0983", 8);
            }
        }
        if(is_project(OPPO_18531)
            || is_project(OPPO_18561)
            || is_project(OPPO_18161)) {
            if (strstr(boot_command_line, "dsjm_himax83112")) {
                memcpy(panel_data->manufacture_info.version, "0xBD1203", 8);
            }
            if (strstr(boot_command_line, "djn_jdi_himax83112b")) {
                memcpy(panel_data->manufacture_info.version, "0xCC1250", 8);
            }
            if (strstr(boot_command_line, "dsjm_jdi_himax83112b")) {
                memcpy(panel_data->manufacture_info.version, "0xDD0755", 8);
            }
            if (strstr(boot_command_line, "jdi_himax83112a")) {
                memcpy(panel_data->manufacture_info.version, "0xDD0750", 8);
            }
            if (strstr(boot_command_line, "tm_himax83112")) {
                memcpy(panel_data->manufacture_info.version, "0xDD0751", 8);
            }
            if (strstr(boot_command_line, "tianma_himax83112a")) {
                memcpy(panel_data->manufacture_info.version, "0xDD0751", 8);
            }
            if (strstr(boot_command_line, "dsjm_jdi_td4330")) {
                memcpy(panel_data->manufacture_info.version, "0xDD075E", 8);
            }
            if (strstr(boot_command_line, "dpt_jdi_td4330")) {
                memcpy(panel_data->manufacture_info.version, "0xDD075D", 8);
            }
            if (strstr(boot_command_line, "tianma_td4330")) {
                memcpy(panel_data->manufacture_info.version, "0xDD075A", 8);
            }
            if (strstr(boot_command_line, "tm_nt36670a")) {
                memcpy(panel_data->manufacture_info.version, "0xDD0752", 8);
            }
        }
		if(is_project(OPPO_18311)
			|| is_project(OPPO_18011)|| is_project(OPPO_18611)) {
            if (strstr(boot_command_line, "tianma_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xBD1201", 8);
            }else if (strstr(boot_command_line, "depute_nt36672")) {
                memcpy(panel_data->manufacture_info.version, "0xBD1202", 8);
            }else if (strstr(boot_command_line, "dsjm_himax83112")) {
                memcpy(panel_data->manufacture_info.version, "0xBD1203", 8);
            }else if (strstr(boot_command_line, "truly_td4320")) {
                memcpy(panel_data->manufacture_info.version, "0xBD1204", 8);
            }
        }
    }



	else if (is_project(OPPO_19531)) {
        panel_data->tp_type = TP_SAMSUNG;
    } else if (gpio_is_valid(hw_res->id1_gpio) || gpio_is_valid(hw_res->id2_gpio) || gpio_is_valid(hw_res->id3_gpio)) {
        tp_get_vendor_via_pin(hw_res, panel_data);
    } else {
        tp_get_vendor_separate(hw_res, panel_data);
    }

    if (panel_data->tp_type == TP_UNKNOWN) {
        pr_err("[TP]%s type is unknown\n", __func__);
        return 0;
    }

    vendor = GET_TP_DEV_NAME(panel_data->tp_type);

    strcpy(panel_data->manufacture_info.manufacture, vendor);
    switch(get_project()) {
    case OPPO_18561:
    case OPPO_18161:
        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
                "tp/18561/FW_%s_%s.img",
                panel_data->chip_name, vendor);

        if (panel_data->test_limit_name) {
            snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
                "tp/18561/LIMIT_%s_%s.img",
                panel_data->chip_name, vendor);
        }

        if (panel_data->extra) {
            snprintf(panel_data->extra, MAX_LIMIT_DATA_LENGTH,
                "tp/18561/BOOT_FW_%s_%s.ihex",
                panel_data->chip_name, vendor);
        }
        break;
    default:
        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
                "tp/%d/FW_%s_%s.img",
                get_project(), panel_data->chip_name, vendor);

        if (panel_data->test_limit_name) {
            snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
                "tp/%d/LIMIT_%s_%s.img",
                get_project(), panel_data->chip_name, vendor);
        }

        if (panel_data->extra) {
        snprintf(panel_data->extra, MAX_LIMIT_DATA_LENGTH,
                "tp/%d/BOOT_FW_%s_%s.ihex",
                get_project(), panel_data->chip_name, vendor);
        }
    }
	panel_data->manufacture_info.fw_path = panel_data->fw_name;


/*
	panel_data->firmware_headfile.firmware_data = FW_18531_TD4330_NF_DSJM;
	panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_TD4330_NF_DSJM);
*/

    switch(get_project()) {
    case OPPO_18531:
        if (strstr(boot_command_line, "dsjm_himax83112")) {
                panel_data->firmware_headfile.firmware_data = FW_18311_HX83112A_NF_DSJM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18311_HX83112A_NF_DSJM);
        } else if (strstr(boot_command_line, "dsjm_jdi_himax83112b")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_HX83112B_NF_DSJM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_HX83112B_NF_DSJM);
        } else if (strstr(boot_command_line, "jdi_himax83112a")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_HX83112A_NF_JDI;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_HX83112A_NF_JDI);
        } else if (strstr(boot_command_line, "tm_himax83112")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_HX83112A_NF_TM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_HX83112A_NF_TM);
        } else if (strstr(boot_command_line, "dsjm_jdi_td4330")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_TD4330_NF_DSJM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_TD4330_NF_DSJM);
        } else if (strstr(boot_command_line, "dpt_jdi_td4330")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_TD4330_NF_DPT;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_TD4330_NF_DPT);
        } else if (strstr(boot_command_line, "tianma_td4330")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_TD4330_NF_TM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_TD4330_NF_TM);
        } else if (strstr(boot_command_line, "tm_nt36670a")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_NT36672A_NF_TM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_NT36672A_NF_TM);
        } else {
            panel_data->firmware_headfile.firmware_data = NULL;
            panel_data->firmware_headfile.firmware_size = 0;
        }
        break;
    case OPPO_18561:
    case OPPO_18161:
        if (strstr(boot_command_line, "dsjm_himax83112")) {
                panel_data->firmware_headfile.firmware_data = FW_18311_HX83112A_NF_DSJM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18311_HX83112A_NF_DSJM);
        } else if (strstr(boot_command_line, "djn_jdi_himax83112b")) {
                panel_data->firmware_headfile.firmware_data = FW_18561_HX83112B_NF_DJN;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18561_HX83112B_NF_DJN);
        } else if (strstr(boot_command_line, "dsjm_jdi_himax83112b")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_HX83112B_NF_DSJM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_HX83112B_NF_DSJM);
        } else if (strstr(boot_command_line, "jdi_himax83112a")) {
                panel_data->firmware_headfile.firmware_data = FW_18561_HX83112A_NF_JDI;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18561_HX83112A_NF_JDI);
        } else if (strstr(boot_command_line, "tm_himax83112")) {
                panel_data->firmware_headfile.firmware_data = FW_18561_HX83112A_NF_TM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18561_HX83112A_NF_TM);
        } else if (strstr(boot_command_line, "tianma_himax83112a")) {
                panel_data->firmware_headfile.firmware_data = FW_18561_HX83112A_NF_TM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18561_HX83112A_NF_TM);
        } else if (strstr(boot_command_line, "dsjm_jdi_td4330")) {
                panel_data->firmware_headfile.firmware_data = FW_18561_TD4330_NF_DSJM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18561_TD4330_NF_DSJM);
        } else if (strstr(boot_command_line, "dpt_jdi_td4330")) {
                panel_data->firmware_headfile.firmware_data = FW_18561_TD4330_NF_DPT;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18561_TD4330_NF_DPT);
        } else if (strstr(boot_command_line, "tianma_td4330")) {
                panel_data->firmware_headfile.firmware_data = FW_18561_TD4330_NF_TM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18561_TD4330_NF_TM);
        } else if (strstr(boot_command_line, "tm_nt36670a")) {
                panel_data->firmware_headfile.firmware_data = FW_18531_NT36672A_NF_TM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18531_NT36672A_NF_TM);
        } else {
            panel_data->firmware_headfile.firmware_data = NULL;
            panel_data->firmware_headfile.firmware_size = 0;
        }
        break;
	case OPPO_18311:
    case OPPO_18011:
    case OPPO_18611:	
        if (strstr(boot_command_line, "depute_nt36672")) {    //noflash
                panel_data->firmware_headfile.firmware_data = FW_18311_NT36672A_NF_DEPUTE;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18311_NT36672A_NF_DEPUTE);
        }else if (strstr(boot_command_line, "tianma_nt36672")) {
                panel_data->firmware_headfile.firmware_data = FW_18311_NT36672A_NF_TIANMA;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18311_NT36672A_NF_TIANMA);
        }else if (strstr(boot_command_line, "dsjm_himax83112")) {
                panel_data->firmware_headfile.firmware_data = FW_18311_HX83112A_NF_DSJM;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18311_HX83112A_NF_DSJM);
        }else if (strstr(boot_command_line, "truly_td4320")) {
                panel_data->firmware_headfile.firmware_data = FW_18311_TD4320_NF_TRULY;
                panel_data->firmware_headfile.firmware_size = sizeof(FW_18311_TD4320_NF_TRULY);
        }else{
            panel_data->firmware_headfile.firmware_data = NULL;
            panel_data->firmware_headfile.firmware_size = 0;
        }
        break;
    default:
        panel_data->firmware_headfile.firmware_data = NULL;
        panel_data->firmware_headfile.firmware_size = 0;
    }

    pr_info("Vendor:%s\n", vendor);
    pr_info("Fw:%s\n", panel_data->fw_name);
    pr_info("Limit:%s\n", panel_data->test_limit_name==NULL?"NO Limit":panel_data->test_limit_name);
    pr_info("Extra:%s\n", panel_data->extra==NULL?"NO Extra":panel_data->extra);
    pr_info("is matched %d, type %d\n", is_tp_type_got_in_match, panel_data->tp_type);
    return 0;
}
