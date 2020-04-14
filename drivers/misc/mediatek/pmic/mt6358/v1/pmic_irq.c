/*
 * Copyright (C) 2018 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <generated/autoconf.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>

#include <mt-plat/aee.h>
#include <mt-plat/upmu_common.h>
#ifdef CONFIG_MTK_PMIC_WRAP_HAL
#include <mach/mtk_pmic_wrap.h>
#endif
#include <mach/mtk_pmic.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"

#include <mt-plat/mtk_ccci_common.h>
#include <linux/mfd/mt6358/core.h>

#ifndef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/10/21, add for charger code*/
#ifdef VENDOR_EDIT
/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/11/19, Add for charging */
#include <mt-plat/charger_type.h>
extern enum charger_type g_chr_type;
#endif /* VENDOR_EDIT */
#endif /*ODM_WT_EDIT*/


#ifdef VENDOR_EDIT
/* ChaoYing.Chen@BSP.Power.Basic.1056413, 2017/12/11, Add for print wakeup source */
unsigned int g_eint_pmic_num = 182;

#define PMIC_INT_REG_WIDTH  16
#define PMIC_INT_REG_NUMBER  16
u64 pmic_wakesrc_x_count[PMIC_INT_REG_NUMBER][PMIC_INT_REG_WIDTH] = {
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
	 { 0 },
};

const char *pmic_interrupt_status_name[PMIC_INT_REG_NUMBER][PMIC_INT_REG_WIDTH] = {
	{
	[0] = " INT_VPROC11_OC",
	[1] = " INT_VPROC12_OC",
	[2] = " INT_VCORE_OC",
	[3] = " INT_VGPU_OC",
	[4] = " INT_VMODEM_OC",
	[5] = " INT_VDRAM1_OC",
	[6] = " INT_VS1_OC",
	[7] = " INT_VS2_OC",
	[8] = " INT_VPA_OC",
	[9] = " INT_VCORE_PREOC",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " ",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " INT_VFE28_OC",
	[1] = " INT_VXO22_OC",
	[2] = " INT_VRF18_OC",
	[3] = " INT_VRF12_OC",
	[4] = " INT_VEFUSE_OC",
	[5] = " INT_VCN33_OC",
	[6] = " INT_VCN28_OC",
	[7] = " INT_VCN18_OC",
	[8] = " INT_VCAMA1_OC",
	[9] = " INT_VCAMA2_OC",
	[10] = " INT_VCAMD_OC",
	[11] = " INT_VCAMIO_OC",
	[12] = " INT_VLDO28_OC",
	[13] = " INT_VA12_OC",
	[14] = " INT_VAUX18_OC",
	[15] = " INT_VAUD28_OC",
	},
	{
	[0] = " INT_VIO28_OC",
	[1] = " INT_VIO18_OC",
	[2] = " INT_VSRAM_PROC11_OC",
	[3] = " INT_VSRAM_PROC12_OC",
	[4] = " INT_VSRAM_OTHERS_OC",
	[5] = " INT_VSRAM_GPU_OC",
	[6] = " INT_VDRAM2_OC",
	[7] = " INT_VMC_OC",
	[8] = " INT_VMCH_OC",
	[9] = " INT_VEMC_OC",
	[10] = " INT_VSIM1_OC",
	[11] = " INT_VSIM2_OC",
	[12] = " INT_VIBR_OC",
	[13] = " INT_VUSB_OC",
	[14] = " INT_VBIF28_OC",
	[15] = " ",
	},
	{
	[0] = " INT_PWRKEY",
	[1] = " INT_HOMEKEY",
	[2] = " INT_PWRKEY_R",
	[3] = " INT_HOMEKEY_R",
	[4] = " INT_NI_LBAT_INT",
	[5] = " INT_CHRDET",
	[6] = " INT_CHRDET_EDGE",
	[7] = " INT_VCDT_HV_DET",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " ",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " INT_RTC",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " ",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " INT_FG_BAT0_H",
	[1] = " INT_FG_BAT0_L",
	[2] = " INT_FG_CUR_H",
	[3] = " INT_FG_CUR_L",
	[4] = " INT_FG_ZCV",
	[5] = " INT_FG_BAT1_H",
	[6] = " INT_FG_BAT1_L",
	[7] = " INT_FG_N_CHARGE_L",
	[8] = " INT_FG_IAVG_H",
	[9] = " INT_FG_IAVG_L",
	[10] = " INT_FG_TIME_H",
	[11] = " INT_FG_DISCHARGE",
	[12] = " INT_FG_CHARGE",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " INT_BATON_LV",
	[1] = " INT_BATON_HT",
	[2] = " INT_BATON_BAT_IN",
	[3] = " INT_BATON_BAT_OUT",
	[4] = " INT_BIF",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " INT_BAT_H",
	[1] = " INT_BAT_L",
	[2] = " INT_BAT2_H",
	[3] = " INT_BAT2_L",
	[4] = " INT_BAT_TEMP_H",
	[5] = " INT_BAT_TEMP_L",
	[6] = " INT_AUXADC_IMP",
	[7] = " INT_NAG_C_DLTV",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " ",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " INT_AUDIO",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " INT_ACCDET",
	[6] = " INT_ACCDET_EINT0",
	[7] = " INT_ACCDET_EINT1",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
		{
	[0] = " ",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " INT_SPI_CMD_ALERT",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
	{
	[0] = " ",
	[1] = " ",
	[2] = " ",
	[3] = " ",
	[4] = " ",
	[5] = " ",
	[6] = " ",
	[7] = " ",
	[8] = " ",
	[9] = " ",
	[10] = " ",
	[11] = " ",
	[12] = " ",
	[13] = " ",
	[14] = " ",
	[15] = " ",
	},
};
void mt_pmic_clear_wakesrc_count(void)
{
	int i = 0;
	int j = 0;

	for (i = 0; i < PMIC_INT_REG_NUMBER; i++){
		for (j = 0; j < PMIC_INT_REG_WIDTH; j++){
			pmic_wakesrc_x_count[i][j] = 0;
		}
	}
}
EXPORT_SYMBOL(mt_pmic_clear_wakesrc_count);
#endif /* VENDOR_EDIT */

static struct pmic_sp_irq buck_irqs[][PMIC_INT_WIDTH] = {
	{
		PMIC_SP_IRQ_GEN(1, INT_VPROC11_OC),
		PMIC_SP_IRQ_GEN(1, INT_VPROC12_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCORE_OC),
		PMIC_SP_IRQ_GEN(1, INT_VGPU_OC),
		PMIC_SP_IRQ_GEN(1, INT_VMODEM_OC),
		PMIC_SP_IRQ_GEN(1, INT_VDRAM1_OC),
		PMIC_SP_IRQ_GEN(1, INT_VS1_OC),
		PMIC_SP_IRQ_GEN(1, INT_VS2_OC),
		PMIC_SP_IRQ_GEN(1, INT_VPA_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCORE_PREOC),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq ldo_irqs[][PMIC_INT_WIDTH] = {
	{
		PMIC_SP_IRQ_GEN(1, INT_VFE28_OC),
		PMIC_SP_IRQ_GEN(1, INT_VXO22_OC),
		PMIC_SP_IRQ_GEN(1, INT_VRF18_OC),
		PMIC_SP_IRQ_GEN(1, INT_VRF12_OC),
		PMIC_SP_IRQ_GEN(1, INT_VEFUSE_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCN33_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCN28_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCN18_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCAMA1_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCAMA2_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCAMD_OC),
		PMIC_SP_IRQ_GEN(1, INT_VCAMIO_OC),
		PMIC_SP_IRQ_GEN(1, INT_VLDO28_OC),
		PMIC_SP_IRQ_GEN(1, INT_VA12_OC),
		PMIC_SP_IRQ_GEN(1, INT_VAUX18_OC),
		PMIC_SP_IRQ_GEN(1, INT_VAUD28_OC),
	},
	{
		PMIC_SP_IRQ_GEN(1, INT_VIO28_OC),
		PMIC_SP_IRQ_GEN(1, INT_VIO18_OC),
		PMIC_SP_IRQ_GEN(1, INT_VSRAM_PROC11_OC),
		PMIC_SP_IRQ_GEN(1, INT_VSRAM_PROC12_OC),
		PMIC_SP_IRQ_GEN(1, INT_VSRAM_OTHERS_OC),
		PMIC_SP_IRQ_GEN(1, INT_VSRAM_GPU_OC),
		PMIC_SP_IRQ_GEN(1, INT_VDRAM2_OC),
		PMIC_SP_IRQ_GEN(1, INT_VMC_OC),
		PMIC_SP_IRQ_GEN(1, INT_VMCH_OC),
		PMIC_SP_IRQ_GEN(1, INT_VEMC_OC),
		PMIC_SP_IRQ_GEN(1, INT_VSIM1_OC),
		PMIC_SP_IRQ_GEN(1, INT_VSIM2_OC),
		PMIC_SP_IRQ_GEN(1, INT_VIBR_OC),
		PMIC_SP_IRQ_GEN(1, INT_VUSB_OC),
		PMIC_SP_IRQ_GEN(1, INT_VBIF28_OC),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq psc_irqs[][PMIC_INT_WIDTH] = {
	{
		PMIC_SP_IRQ_GEN(1, INT_PWRKEY),
		PMIC_SP_IRQ_GEN(1, INT_HOMEKEY),
		PMIC_SP_IRQ_GEN(1, INT_PWRKEY_R),
		PMIC_SP_IRQ_GEN(1, INT_HOMEKEY_R),
		PMIC_SP_IRQ_GEN(1, INT_NI_LBAT_INT),
		PMIC_SP_IRQ_GEN(1, INT_CHRDET),
		PMIC_SP_IRQ_GEN(1, INT_CHRDET_EDGE),
		PMIC_SP_IRQ_GEN(1, INT_VCDT_HV_DET),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq sck_irqs[][PMIC_INT_WIDTH] = {
	{
		PMIC_SP_IRQ_GEN(1, INT_RTC),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq bm_irqs[][PMIC_INT_WIDTH] = {
	{
		PMIC_SP_IRQ_GEN(1, INT_FG_BAT0_H),
		PMIC_SP_IRQ_GEN(1, INT_FG_BAT0_L),
		PMIC_SP_IRQ_GEN(1, INT_FG_CUR_H),
		PMIC_SP_IRQ_GEN(1, INT_FG_CUR_L),
		PMIC_SP_IRQ_GEN(1, INT_FG_ZCV),
		PMIC_SP_IRQ_GEN(1, INT_FG_BAT1_H),
		PMIC_SP_IRQ_GEN(1, INT_FG_BAT1_L),
		PMIC_SP_IRQ_GEN(1, INT_FG_N_CHARGE_L),
		PMIC_SP_IRQ_GEN(1, INT_FG_IAVG_H),
		PMIC_SP_IRQ_GEN(1, INT_FG_IAVG_L),
		PMIC_SP_IRQ_GEN(1, INT_FG_TIME_H),
		PMIC_SP_IRQ_GEN(1, INT_FG_DISCHARGE),
		PMIC_SP_IRQ_GEN(1, INT_FG_CHARGE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
	{
		PMIC_SP_IRQ_GEN(1, INT_BATON_LV),
		PMIC_SP_IRQ_GEN(1, INT_BATON_HT),
		PMIC_SP_IRQ_GEN(1, INT_BATON_BAT_IN),
		PMIC_SP_IRQ_GEN(1, INT_BATON_BAT_OUT),
		PMIC_SP_IRQ_GEN(1, INT_BIF),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq hk_irqs[][PMIC_INT_WIDTH] = {
	{
		PMIC_SP_IRQ_GEN(1, INT_BAT_H),
		PMIC_SP_IRQ_GEN(1, INT_BAT_L),
		PMIC_SP_IRQ_GEN(1, INT_BAT2_H),
		PMIC_SP_IRQ_GEN(1, INT_BAT2_L),
		PMIC_SP_IRQ_GEN(1, INT_BAT_TEMP_H),
		PMIC_SP_IRQ_GEN(1, INT_BAT_TEMP_L),
		PMIC_SP_IRQ_GEN(1, INT_AUXADC_IMP),
		PMIC_SP_IRQ_GEN(1, INT_NAG_C_DLTV),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq aud_irqs[][PMIC_INT_WIDTH] = {
	{
		PMIC_SP_IRQ_GEN(1, INT_AUDIO),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(1, INT_ACCDET),
		PMIC_SP_IRQ_GEN(1, INT_ACCDET_EINT0),
		PMIC_SP_IRQ_GEN(1, INT_ACCDET_EINT1),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

static struct pmic_sp_irq misc_irqs[][PMIC_INT_WIDTH] = {
	{
		PMIC_SP_IRQ_GEN(1, INT_SPI_CMD_ALERT),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
		PMIC_SP_IRQ_GEN(0, NO_USE),
	},
};

struct pmic_sp_interrupt sp_interrupts[] = {
	PMIC_SP_INTS_GEN(BUCK_TOP, 1, buck_irqs, 0),
	PMIC_SP_INTS_GEN(LDO_TOP, 2, ldo_irqs, 1),
	PMIC_SP_INTS_GEN(PSC_TOP, 1, psc_irqs, 2),
	PMIC_SP_INTS_GEN(SCK_TOP, 1, sck_irqs, 3),
	PMIC_SP_INTS_GEN(BM_TOP, 2, bm_irqs, 4),
	PMIC_SP_INTS_GEN(HK_TOP, 1, hk_irqs, 5),
	PMIC_SP_INTS_GEN(AUD_TOP, 1, aud_irqs, 7),
	PMIC_SP_INTS_GEN(MISC_TOP, 1, misc_irqs, 8),
};

unsigned int sp_interrupt_size = ARRAY_SIZE(sp_interrupts);

struct legacy_pmic_callback {
	bool has_requested;
	void (*callback)(void);
};
static struct device *pmic_dev;
static struct legacy_pmic_callback pmic_cbs[300];

/* KEY Int Handler */
irqreturn_t key_int_handler(int irq, void *data)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned int hwirq = irqd_to_hwirq(&desc->irq_data);

#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_KPD_PWRKEY_USE_PMIC)
	switch (hwirq) {
	case INT_PWRKEY:
		IRQLOG("Press pwrkey %d\n",
			pmic_get_register_value(PMIC_PWRKEY_DEB));
		kpd_pwrkey_pmic_handler(0x1);
		break;
	case INT_PWRKEY_R:
		IRQLOG("Release pwrkey %d\n",
			pmic_get_register_value(PMIC_PWRKEY_DEB));
		kpd_pwrkey_pmic_handler(0x0);
		break;
	case INT_HOMEKEY:
		IRQLOG("Press homekey %d\n",
			pmic_get_register_value(PMIC_HOMEKEY_DEB));
		kpd_pmic_rstkey_handler(0x1);
		break;
	case INT_HOMEKEY_R:
		IRQLOG("Release homekey %d\n",
			pmic_get_register_value(PMIC_HOMEKEY_DEB));
		kpd_pmic_rstkey_handler(0x0);
		break;
	}
#endif
	return IRQ_HANDLED;
}

irqreturn_t legacy_pmic_int_handler(int irq, void *data)
{
	struct legacy_pmic_callback *pmic_cb = data;

	pmic_cb->callback();
	return IRQ_HANDLED;
}

#ifndef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/10/21, add for charger code*/
#ifndef VENDOR_EDIT
//Fuchun.Liao@BSP.CHG.Basic 2018/01/01 modify for oppo chaarger
/* Chrdet Int Handler */
#if (CONFIG_MTK_GAUGE_VERSION != 30)
void chrdet_int_handler(void)
{
	IRQLOG("[chrdet_int_handler]CHRDET status = %d....\n",
		pmic_get_register_value(PMIC_RGS_CHRDET));
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (!upmu_get_rgs_chrdet()) {
		int boot_mode = 0;

		boot_mode = get_boot_mode();

		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		|| boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			IRQLOG("[chrdet_int_handler] Unplug Charger/USB\n");
#ifdef CONFIG_MTK_RTC
			mt_power_off();
#endif
		}
	}
#endif
	/*pmic_set_register_value(PMIC_RG_USBDL_RST, 1); MT6358 not support */
#if defined(CONFIG_MTK_SMART_BATTERY)
	do_chrdet_int_task();
#endif
}
#endif /* CONFIG_MTK_GAUGE_VERSION != 30 */

#else	//VENDOR_EDIT


extern bool oppo_chg_wake_update_work(void);
extern bool mt_usb_is_device(void);
extern bool oppo_vooc_get_fastchg_started(void);
extern int oppo_vooc_get_adapter_update_status(void);
extern void oppo_vooc_reset_fastchg_after_usbout(void);
extern void oppo_chg_clear_chargerid_info(void);
extern void oppo_chg_set_chargerid_switch_val(int);
#ifdef VENDOR_EDIT
//Bingyuan.Liu@BSP.TP.Function, 2019/10/28, Add for informing tp driver of usb state
extern void switch_usb_state(int usb_state);
#endif /*VENDOR_EDIT*/

#if defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/07/11, sjc Add for charging */
extern void oppo_chg_set_charger_type_unknown(void);
#endif

#ifdef VENDOR_EDIT
//PengNan@BSP.CHG.Basic, 2018/01/20, add for bq24190 chargertype detect.
//extern int get_oppo_short_check_fast_to_normal(void);
#endif /*VENDOR_EDIT*/
void chrdet_int_handler(void)
{
	pr_err("[chrdet_int_handler]CHRDET status = %d....\n",
		pmic_get_register_value(PMIC_RGS_CHRDET));
	if (mt_usb_is_device() == false) {
		return;
	}
#ifdef VENDOR_EDIT
    //Bingyuan.Liu@BSP.TP.Function, 2019/10/28, Add for informing tp driver of usb state
	switch_usb_state(upmu_get_rgs_chrdet());
#endif /*VENDOR_EDIT*/
	if(oppo_vooc_get_fastchg_started() == true && oppo_vooc_get_adapter_update_status() != 1){
		pr_err("[do_charger_detect] opchg_get_prop_fast_chg_started = true!\n");
       // if (get_oppo_short_check_fast_to_normal() == 0) {
            if (!upmu_get_rgs_chrdet()) {
                g_chr_type = CHARGER_UNKNOWN;
            }
        //}
		return;
	}
	if (upmu_get_rgs_chrdet()){
		pr_err("[chrdet_int_handler]charger in\n");

		//mt_usb_connect();
	} else {
		pr_err("[chrdet_int_handler]charger out\n");
		oppo_vooc_reset_fastchg_after_usbout();
		if(oppo_vooc_get_fastchg_started() == false) {
				oppo_chg_set_chargerid_switch_val(0);
				oppo_chg_clear_chargerid_info();
		}
#if defined(VENDOR_EDIT) && defined(CONFIG_OPPO_CHARGER_MT6370_TYPEC)
/* Jianchao.Shi@BSP.CHG.Basic, 2019/07/11, sjc Add for charging */
		oppo_chg_set_charger_type_unknown();
#else
		g_chr_type = CHARGER_UNKNOWN;
#endif
		//mt_usb_disconnect();
	}
	printk("g_chr_type = %d\n",g_chr_type);
	oppo_chg_wake_update_work();
}
#endif /* VENDOR_EDIT */

#endif /*ODM_WT_EDIT*/

#ifdef VENDOR_EDIT
/* ChaoYing.Chen@BSP.Power.Basic.1056413, 2017/12/11, Add for print wakeup source */
int pmic_int_check(char * wakeup_name)
{
	unsigned int spNo, sp_conNo, j;
	unsigned int status_reg;
	unsigned int sp_int_status = 0;
	int ret = -1;

	for (spNo = 0; spNo < ARRAY_SIZE(sp_interrupts); spNo++) {
		for (sp_conNo = 0; sp_conNo < sp_interrupts[spNo].con_len; sp_conNo++) {
			status_reg = sp_interrupts[spNo].status + 0x6 * sp_conNo;
			sp_int_status = upmu_get_reg_value(status_reg);
			IRQLOG("[PMIC_INT] after, Reg[0x%x]=0x%x\n", status_reg, sp_int_status);

       		for (j = 0; j < PMIC_INT_WIDTH; j++) {
				if ((sp_int_status) & (1 << j)) {
					IRQLOG("[PMIC_INT][%s]\n", sp_interrupts[spNo].sp_irqs[sp_conNo][j].name);
					strcpy(wakeup_name, sp_interrupts[spNo].sp_irqs[sp_conNo][j].name);

					ret = (2*spNo+sp_conNo)*j;
					pmic_wakesrc_x_count[2*spNo+sp_conNo][j]++;
				}
			}
		}
	}
	return ret;
	return 0;
}

EXPORT_SYMBOL(pmic_int_check);
#endif /* VENDOR_EDIT */


/*
 * PMIC Interrupt service
 */
void pmic_enable_interrupt(enum PMIC_IRQ_ENUM intNo, unsigned int en, char *str)
{
	int ret;
	unsigned int irq;
	const char *name;
	struct legacy_pmic_callback *pmic_cb = &pmic_cbs[intNo];
	struct irq_desc *desc;

	if (intNo == INT_ENUM_MAX) {
		pr_notice(PMICTAG "[%s] disable intNo=%d\n", __func__, intNo);
		return;
	} else if (pmic_cb->callback == NULL) {
		pr_notice(PMICTAG "[%s] No callback at intNo=%d\n",
			__func__, intNo);
		return;
	}
	irq = mt6358_irq_get_virq(pmic_dev->parent, intNo);
	if (!irq) {
		pr_notice(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	name = mt6358_irq_get_name(pmic_dev->parent, intNo);
	if (name == NULL) {
		pr_notice(PMICTAG "[%s] no irq name at intNo=%d\n",
			__func__, intNo);
		return;
	}
	if (en == 1) {
		if (!(pmic_cb->has_requested)) {
			ret = devm_request_threaded_irq(pmic_dev, irq, NULL,
				legacy_pmic_int_handler, IRQF_TRIGGER_HIGH,
				name, pmic_cb);
			if (ret < 0)
				pr_notice(PMICTAG "[%s] request %s irq fail\n",
					  __func__, name);
			else
				pmic_cb->has_requested = true;
		} else
			enable_irq(irq);
	} else if (en == 0 && pmic_cb->has_requested)
		disable_irq_nosync(irq);
	desc = irq_to_desc(irq);
	pr_info("[%s] intNo=%d, en=%d, depth=%d\n",
		__func__, intNo, en, desc->depth);
}

void pmic_register_interrupt_callback(enum PMIC_IRQ_ENUM intNo,
		void (EINT_FUNC_PTR) (void))
{
	struct legacy_pmic_callback *pmic_cb = &pmic_cbs[intNo];

	if (intNo == INT_ENUM_MAX) {
		pr_info(PMICTAG "[%s] disable intNo=%d\n", __func__, intNo);
		return;
	}
	pr_info("[%s] intNo=%d, callback=%pf\n",
		__func__, intNo, EINT_FUNC_PTR);
	pmic_cb->callback = EINT_FUNC_PTR;
}

#if ENABLE_ALL_OC_IRQ
static unsigned int vio18_oc_times;

/* General OC Int Handler */
static void oc_int_handler(enum PMIC_IRQ_ENUM intNo, const char *int_name)
{
	char oc_str[30] = "";
	unsigned int spNo, sp_conNo, sp_irqNo;
	unsigned int times;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_notice(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	times = sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].times;

	IRQLOG("[%s] int name=%s\n", __func__, int_name);
	switch (intNo) {
	case INT_VCN33_OC:
		/* keep OC interrupt and keep tracking */
		pr_notice(PMICTAG "[PMIC_INT] PMIC OC: %s\n", int_name);
		break;
	case INT_VIO18_OC:
		pr_notice("LDO_DEGTD_SEL=0x%x\n",
			pmic_get_register_value(PMIC_LDO_DEGTD_SEL));
		pr_notice("RG_INT_EN_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_EN_VIO18_OC));
		pr_notice("RG_INT_MASK_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_MASK_VIO18_OC));
		pr_notice("RG_INT_STATUS_VIO18_OC=0x%x\n",
			pmic_get_register_value(PMIC_RG_INT_STATUS_VIO18_OC));
		pr_notice("RG_INT_RAW_STATUS_VIO18_OC=0x%x\n",
			pmic_get_register_value(
				PMIC_RG_INT_RAW_STATUS_VIO18_OC));
		pr_notice("DA_VIO18_OCFB_EN=0x%x\n",
			pmic_get_register_value(PMIC_DA_VIO18_OCFB_EN));
		pr_notice("RG_LDO_VIO18_OCFB_EN=0x%x\n",
			pmic_get_register_value(PMIC_RG_LDO_VIO18_OCFB_EN));
		vio18_oc_times++;
		if (vio18_oc_times >= 2) {
			snprintf(oc_str, 30, "PMIC OC:%s", int_name);
			aee_kernel_warning(
				oc_str,
				"\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
				int_name);
			pmic_enable_interrupt(intNo, 0, "PMIC");
			pr_notice("disable OC interrupt: %s\n", int_name);
		}
		break;
	default:
		/* issue AEE exception and disable OC interrupt */
		kernel_dump_exception_reg();
		snprintf(oc_str, 30, "PMIC OC:%s", int_name);
		aee_kernel_warning(oc_str,
			"\nCRDISPATCH_KEY:PMIC OC\nOC Interrupt: %s",
			int_name);
		pmic_enable_interrupt(intNo, 0, "PMIC");
		pr_notice(PMICTAG "[PMIC_INT] disable OC interrupt: %s\n"
			, int_name);
		break;
	}
}

static void md_oc_int_handler(enum PMIC_IRQ_ENUM intNo, const char *int_name)
{
	int ret = 0;
	int data_int32 = 0;
	char oc_str[30] = "";

	switch (intNo) {
	case INT_VPA_OC:
		data_int32 = 1 << 0;
		break;
	case INT_VFE28_OC:
		data_int32 = 1 << 1;
		break;
	case INT_VRF12_OC:
		data_int32 = 1 << 2;
		break;
	case INT_VRF18_OC:
		data_int32 = 1 << 3;
		break;
	default:
		break;
	}
	snprintf(oc_str, 30, "PMIC OC:%s", int_name);
#ifdef CONFIG_MTK_CCCI_DEVICES
	aee_kernel_warning(oc_str, "\nCRDISPATCH_KEY:MD OC\nOC Interrupt: %s"
			, int_name);
	ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_PMIC_INTR,
					(char *)&data_int32, 4);
#endif
	if (ret)
		pr_notice("[%s] - exec_ccci_kern_func_by_md_id - msg fail\n"
			  , __func__);
	pr_info("[%s]Send msg pass\n", __func__);
}

/* register general oc interrupt handler */
void pmic_register_oc_interrupt_callback(enum PMIC_IRQ_ENUM intNo)
{
	unsigned int spNo, sp_conNo, sp_irqNo;

	if (pmic_check_intNo(intNo, &spNo, &sp_conNo, &sp_irqNo)) {
		pr_notice(PMICTAG "[%s] fail intNo=%d\n", __func__, intNo);
		return;
	}
	IRQLOG("[%s] intNo=%d\n", __func__, intNo);
	switch (intNo) {
	case INT_VPA_OC:
	case INT_VFE28_OC:
	case INT_VRF12_OC:
	case INT_VRF18_OC:
		sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].oc_callback =
							md_oc_int_handler;
		break;
	default:
		sp_interrupts[spNo].sp_irqs[sp_conNo][sp_irqNo].oc_callback =
							oc_int_handler;
		break;
	}
}

/* register and enable all oc interrupt */
void register_all_oc_interrupts(void)
{
	enum PMIC_IRQ_ENUM oc_int;

	/* BUCK OC */
	for (oc_int = INT_VPROC11_OC; oc_int <= INT_VPA_OC; oc_int++) {
		pmic_register_oc_interrupt_callback(oc_int);
		pmic_enable_interrupt(oc_int, 1, "PMIC");
	}
	/* LDO OC */
	for (oc_int = INT_VFE28_OC; oc_int <= INT_VBIF28_OC; oc_int++) {
		switch (oc_int) {
		case INT_VSIM1_OC:
		case INT_VSIM2_OC:
		case INT_VMCH_OC:
		case INT_VCAMA1_OC:
		case INT_VCAMA2_OC:
		case INT_VCAMD_OC:
		case INT_VCAMIO_OC:
			IRQLOG("[PMIC_INT] non-enabled OC: %d\n", oc_int);
			break;
#if 0
		case INT_VCAMA_OC:
			IRQLOG("[PMIC_INT] OC:%d enabled after power on\n"
				, oc_int);
			pmic_register_oc_interrupt_callback(oc_int);
			break;
#endif
		default:
			pmic_register_oc_interrupt_callback(oc_int);
			pmic_enable_interrupt(oc_int, 1, "PMIC");
			break;
		}
	}
}
#endif

void PMIC_EINT_SETTING(struct platform_device *pdev)
{
	int ret = 0;

	pmic_dev = &pdev->dev;
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "pwrkey"),
		NULL, key_int_handler, IRQF_TRIGGER_NONE,
		"pwrkey", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request PWRKEY irq fail\n");
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "pwrkey_r"),
		NULL, key_int_handler, IRQF_TRIGGER_NONE,
		"pwrkey_r", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request PWRKEY_R irq fail\n");
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "homekey"),
		NULL, key_int_handler, IRQF_TRIGGER_NONE,
		"homekey", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request HOMEKEY irq fail\n");
	ret = devm_request_threaded_irq(&pdev->dev,
		platform_get_irq_byname(pdev, "homekey_r"),
		NULL, key_int_handler, IRQF_TRIGGER_NONE,
		"homekey_r", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request HOMEKEY_R irq fail\n");
#ifndef ODM_WT_EDIT
/*Shouli.Wang@ODM_WT.BSP.CHG 2019/10/21, add for charger code*/
#ifdef VENDOR_EDIT
/* Qiao.Hu@BSP.BaseDrv.CHG.Basic, 2017/11/30, modify for charger */
	pmic_register_interrupt_callback(INT_CHRDET_EDGE, chrdet_int_handler);
	pmic_enable_interrupt(INT_CHRDET_EDGE, 1, "PMIC");
#endif
#endif
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Interrupt Driver");
MODULE_LICENSE("GPL");

