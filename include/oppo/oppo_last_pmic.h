/***************************************************************
** Copyright (C),  2019,  OPPO Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oppo_last_pmic.h
** Description : PMIC PONSTS POFFSTS
** Version : 1.0
** Date : 2019/6/27
** Author : Wen.Luo@PSW.BSP.Kernel.Stability
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Wen.Luo          2019/06/27        1.0           Build this moudle
******************************************************************/


#ifndef _OPPO_LAST_PMIC_
#define _OPPO_LAST_PMIC_
typedef struct {
	u16 pon_sts;
	u16 poff_sts;
	u16 pmic_misc[10];
}BootReason_Pon_Poff;


#define PMIC_BITS_U_INT 		16
#define PMIC_BIT_UINT(nr)		((nr) / PMIC_BITS_U_INT)
#define PMIC_BIT_MASK(nr)       (1UL << ((nr) % PMIC_BITS_U_INT))
#define REG_INDEX_MAX_ID		16
static inline int pmic_test_bit(int nr, u16 *addr)
{
	return 1UL & (addr[PMIC_BIT_UINT(nr)] >> (nr & (PMIC_BITS_U_INT-1)));
}
//#define PMIC_BIT_TEST(nr, addr) do { (__u16 *)(addr))[(nr) >> 4] & (1U << ((nr) & 15)); } while(0)

/**
 *data from MT6358_PMIC_DATA_SHEET_V1.4
 *PMIC Register SPEC
 */
//PONSTS
static u16 pon_mask = 31;
static char * const DesPon_STS[REG_INDEX_MAX_ID] = {
	"STS_PWRKEY",			/*0 PWRKEY press*/
	"STS_RTCA",
	"STS_CHRIN",
	"STS_CPAR",
	"STS_REBOOT",
};

//POFFSTS
static u16 poff_mask = 9648;  /*mask bit :10010110110000*/
static char * const DesPoff_STS[REG_INDEX_MAX_ID] = {
	"STS_UVLO",
	"STS_PGFAIL",
	"STS_PSOC",
	"STS_THRDN",
	"STS_WRST",			/*4 Warm Reset*/
	"STS_CRST",
	"STS_PKEYLP",
	"STS_NORMOFF",
	"STS_BWDT",			/*8 BWDT*/
	"STS_DDLO",
	"STS_WDT",
	"STS_PUPSRC",
	"STS_KEYPWR",		/*12 Critical power is turned off during system on*/
	"STS_PKSP",
};

/**
*MT6358_TOP_RST_STATUS		0x152
*MT6358_PG_SDN_STS0			0x16
*MT6358_OC_SDN_STS0			0x1a
*MT6358_BUCK_TOP_OC_CON0
*MT6358_BUCK_TOP_ELR0
*MT6358_THERMALSTATUS
*MT6358_STRUP_CON4
*MT6358_TOP_RST_MISC
*MT6358_TOP_CLK_TRIM
*/
enum PMIC_MISC {
	TOP_RST_STATUS = 0,
	PG_SDN_STS0 =16,
	OC_SDN_STS0 = 32,
	BUCK_TOP_OC_CON0 = 48,
	BUCK_TOP_ELR0 = 64,
	THERMALSTATUS = 80,
	STRUP_CON4 =96,
	TOP_RST_MISC = 112,
	TOP_CLK_TRIM = 128,
};

#endif /* _OPPO_LAST_PMIC_ */