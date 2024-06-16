// SPDX-License-Identifier: GPL-2.0+
/*
 * Marvell Base-T1 88Q211x PHY driver
 *
 */

#include <linux/kernel.h>

#define MARVELL_PHY_ID_88Q2110		0x002b0980
#define MARVELL_PHY_ID_MASK		0xfffffff0

#define MRVL_88Q2112_MODE_LEGACY    0x06B0
#define MRVL_88Q2112_MODE_DEFAULT   0x0000
#define MRVL_88Q2112_MODE_MSK       0x07F0
#define MRVL_88Q2112_MODE_ADVERTISE 0x0002
#define MRVL_88Q2112_MODE_ERROR     0xFFFF

// PHY Revisions
#define MRVL_88Q2112_A2     0x0003
#define MRVL_88Q2112_A1     0x0002
#define MRVL_88Q2112_A0     0x0001
#define MRVL_88Q2112_Z1     0x0000

// Current Speed
#define MRVL_88Q2112_1000BASE_T1    0x0001
#define MRVL_88Q2112_100BASE_T1     0x0000

// Auto-Negotiation Controls - reg 7.0x0200
#define MRVL_88Q2112_AN_DISABLE     0x0000
#define MRVL_88Q2112_AN_RESET       0x8000
#define MRVL_88Q2112_AN_ENABLE      0x1000
#define MRVL_88Q2112_AN_RESTART     0x0200

// Auto-Negotiation Status - reg 7.0x0201
#define MRVL_88Q2112_AN_COMPLETE    0x0020

// Auto-Negotiation Flags - reg 7.0x0203
#define MRVL_88Q2112_AN_FE_ABILITY      0x0020
#define MRVL_88Q2112_AN_GE_ABILITY      0x0080
#define MRVL_88Q2112_AN_PREFER_MASTER   0x0010

#define MRVL_88Q2112_PRODUCT_ID         0x0018

// PHY Init Return flags
#define MRVL_88Q2112_INIT_DONE              0x0001
#define MRVL_88Q2112_INIT_ERROR_ARGS        0x0100
#define MRVL_88Q2112_INIT_ERROR_SMI         0x0200
#define MRVL_88Q2112_INIT_ERROR_SAVE_MODE   0x0400
#define MRVL_88Q2112_INIT_ERROR_INIT_GE     0x0800
#define MRVL_88Q2112_INIT_ERROR_FAILED      0x1000

// PHY Init (and InitAN) Flag Arguments
#define MRVL_88Q2112_INIT_MASTER        0x0001
#define MRVL_88Q2112_INIT_AN_ENABLE     0x0002
#define MRVL_88Q2112_INIT_FE            0x0004
#define MRVL_88Q2112_INIT_GE            0x0008

// Link-Up Timeout
#define MRVL_88Q2112_LINKUP_TIMEOUT     200

// Single SEND_S Flag: apply to both LP and DUT
#define MRVL_88Q2112_SINGLE_SEND_S_ENABLE	1

struct m88q_mmd_data {
	 int devad;
	 u32 regnum;
	 u16 val;
};


static const struct m88q_mmd_data z1_init_data[] = {
		{ 3, 0xFE12, 0x000E },
		{ 3, 0xFE05, 0x05AA },
		{ 3, 0xFE04, 0x0016 },
		{ 3, 0xFE07, 0x681F },
		{ 3, 0xFE5D, 0x045C },
		{ 3, 0xFE7C, 0x001E },
		{ 3, 0xFC00, 0x01C0 },
		{ 7, 0x8032, 0x0020 },
		{ 7, 0x8031, 0x0012 },
		{ 7, 0x8031, 0x0A12 },
		{ 7, 0x8032, 0x003C },
		{ 7, 0x8031, 0x0001 },
		{ 7, 0x8031, 0x0A01 },
		{ 3, 0xFC10, 0xD870 },
		{ 3, 0xFC11, 0x1522 },
		{ 3, 0xFC12, 0x07FA },
		{ 3, 0xFC13, 0x010B },
		{ 3, 0xFC15, 0x35A4 },
		{ 3, 0xFC2D, 0x3C34 },
		{ 3, 0xFC2E, 0x104B },
		{ 3, 0xFC2F, 0x1C15 },
		{ 3, 0xFC30, 0x3C3C },
		{ 3, 0xFC31, 0x3C3C },
		{ 3, 0xFC3A, 0x2A2A },
		{ 3, 0xFC61, 0x2829 },
		{ 3, 0xFC3B, 0x0E0E },
		{ 3, 0xFC62, 0x1C12 },
		{ 3, 0xFC32, 0x03D2 },
		{ 3, 0xFC46, 0x0200 },
		{ 3, 0xFC86, 0x0401 },
		{ 3, 0xFC4E, 0x1820 },
		{ 3, 0xFC9C, 0x0101 },
		{ 3, 0xFC95, 0x007A },
		{ 3, 0xFC3E, 0x221F },
		{ 3, 0xFC3F, 0x0A08 },
		{ 3, 0xFC03, 0x020E },
		{ 3, 0xFC04, 0x0077 },
		{ 3, 0xFC03, 0x0210 },
		{ 3, 0xFC04, 0x0088 },
		{ 3, 0xFC03, 0x0215 },
		{ 3, 0xFC04, 0x00AA },
		{ 3, 0xFC03, 0x01D5 },
		{ 3, 0xFC04, 0x00AA },
		{ 3, 0xFC03, 0x0216 },
		{ 3, 0xFC04, 0x00AB },
		{ 3, 0xFC03, 0x01D6 },
		{ 3, 0xFC04, 0x00AB },
		{ 3, 0xFC03, 0x0213 },
		{ 3, 0xFC04, 0x00A0 },
		{ 3, 0xFC03, 0x01D3 },
		{ 3, 0xFC04, 0x00A0 },
		{ 3, 0xFC03, 0x0214 },
		{ 3, 0xFC04, 0x00AB },
		{ 3, 0xFC03, 0x01D4 },
		{ 3, 0xFC04, 0x00AB },
		{ 3, 0xFC03, 0x046B },
		{ 3, 0xFC04, 0x00FA },
		{ 3, 0xFC03, 0x046C },
		{ 3, 0xFC04, 0x01F4 },
		{ 3, 0xFC03, 0x046E },
		{ 3, 0xFC04, 0x01F4 },
		{ 3, 0xFC03, 0x0455 },
		{ 3, 0xFC04, 0x0320 },
		{ 3, 0xFC03, 0x0416 },
		{ 3, 0xFC04, 0x0323 },
		{ 3, 0xFC03, 0x0004 },
		{ 3, 0xFC04, 0x0000 },
		{ 3, 0xFC03, 0x03CC },
		{ 3, 0xFC04, 0x0055 },
		{ 3, 0xFC03, 0x03CD },
		{ 3, 0xFC04, 0x0055 },
		{ 3, 0xFC03, 0x03CE },
		{ 3, 0xFC04, 0x0022 },
		{ 3, 0xFC03, 0x03CF },
		{ 3, 0xFC04, 0x0022 },
		{ 3, 0xFC03, 0x03D0 },
		{ 3, 0xFC04, 0x0022 },
		{ 3, 0xFC03, 0x03D1 },
		{ 3, 0xFC04, 0x0022 },
		{ 3, 0xFC03, 0x03E4 },
		{ 3, 0xFC04, 0x0055 },
		{ 3, 0xFC03, 0x03E5 },
		{ 3, 0xFC04, 0x0055 },
		{ 3, 0xFC03, 0x03E6 },
		{ 3, 0xFC04, 0x0022 },
		{ 3, 0xFC03, 0x03E7 },
		{ 3, 0xFC04, 0x0022 },
		{ 3, 0xFC03, 0x03E8 },
		{ 3, 0xFC04, 0x0022 },
		{ 3, 0xFC03, 0x03E9 },
		{ 3, 0xFC04, 0x0022 },
		{ 3, 0xFC03, 0x040C },
		{ 3, 0xFC04, 0x0033 },
		{ 3, 0xFC5D, 0x06BF },
		{ 3, 0xFC89, 0x0003 },
		{ 3, 0xFC5C, 0x007F },
		{ 3, 0xFC69, 0x383A },
		{ 3, 0xFC6A, 0x383A },
		{ 3, 0xFC6B, 0x0082 },
		{ 3, 0xFC6F, 0x888F },
		{ 3, 0xFC70, 0x0D1A },
		{ 3, 0xFC71, 0x0505 },
		{ 3, 0xFC72, 0x090C },
		{ 3, 0xFC73, 0x0C0F },
		{ 3, 0xFC74, 0x0400 },
		{ 3, 0xFC75, 0x0103 },
		{ 3, 0xFC7A, 0x081E },
		{ 3, 0xFC8C, 0xBC40 },
		{ 3, 0xFC8D, 0x9830 },
		{ 3, 0xFC91, 0x0000 },
		{ 3, 0xFC63, 0x4440 },
		{ 3, 0xFC64, 0x3C3F },
		{ 3, 0xFC65, 0x783C },
		{ 3, 0xFC66, 0x0002 },
		{ 3, 0xFC7B, 0x7818 },
		{ 3, 0xFC7C, 0xC440 },
		{ 3, 0xFC7D, 0x5360 },
		{ 3, 0xFC5F, 0x4034 },
		{ 3, 0xFC60, 0x7858 },
		{ 3, 0xFC7E, 0x003F },
		{ 3, 0xFC8E, 0x0003 },
		{ 3, 0xFC57, 0x1820 },
		{ 3, 0xFC00, 0x01C8 },
		{ 3, 0xFC93, 0x141C },
		{ 3, 0xFC9B, 0x0091 },
		{ 3, 0xFC94, 0x6D88 },
		{ 3, 0xFE4A, 0x5653 },
		{ 3, 0x0900, 0x8000 },
};

#define __THIS_FILE__   ((strrchr(__FILE__, '/') == NULL) ?  ( __FILE__ ) :  (strrchr(__FILE__, '/') + 1))


#define mrvl_dbg(ARG, ...)  _mrvl_pr(phydev, LOGLEVEL_DEBUG, "PHY[%s:%d]: " ARG , (__THIS_FILE__), (__LINE__),   ##__VA_ARGS__)
#define mrvl_info(ARG, ...)  _mrvl_pr(phydev, LOGLEVEL_INFO, "PHY[%s:%d]: " ARG , (__THIS_FILE__), (__LINE__),   ##__VA_ARGS__)
#define mrvl_notice(ARG, ...)  _mrvl_pr(phydev, LOGLEVEL_NOTICE, "PHY[%s:%d]: " ARG , (__THIS_FILE__), (__LINE__),  ##__VA_ARGS__)
#define mrvl_warn(ARG, ...)  _mrvl_pr(phydev, LOGLEVEL_WARNING, "PHY[%s:%d]: " ARG , (__THIS_FILE__), (__LINE__),  ##__VA_ARGS__)
#define mrvl_err(ARG, ...)  _mrvl_pr(phydev, LOGLEVEL_ERR, "PHY[%s:%d]: " ARG , (__THIS_FILE__), (__LINE__),  ##__VA_ARGS__)
#define mrvl_crit(ARG, ...)  _mrvl_pr(phydev, LOGLEVEL_CRIT, "PHY[%s:%d]: " ARG , (__THIS_FILE__), (__LINE__),  ##__VA_ARGS__)

static inline int _mrvl_pr(struct phy_device *phydev,  int level, const char *fmt, ...) {
	int ret;
	va_list args;
	int *log_level_ptr = (int*)phydev->priv;


	if(level <= *log_level_ptr) {
		va_start(args, fmt);
		ret = vprintk(fmt, args);
		va_end(args);
	}
	return ret;
}
