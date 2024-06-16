// SPDX-License-Identifier: GPL-2.0+
/*
 * Marvell Base-T1 88Q211x PHY driver
 *
 */
#include <linux/phy.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include "marvell_88q2110.h"

#define NOT_USED 0


static int log_level = LOGLEVEL_ERR;
module_param(log_level,int,0660);

/* ---------------------------------------------------------*/


// See if Auto-Negotiation is enabled
// @param phyAddr bootstrap address of the PHY
// @return true if enabled, false otherwise
static bool mv88q211x_is_aneg_enabled(struct phy_device *phydev)
{
	uint16_t reg_val;;

	reg_val = phy_read_mmd(phydev, 7, 0x0200);

	return ((reg_val & MRVL_88Q2112_AN_ENABLE) == MRVL_88Q2112_AN_ENABLE);
}

// Get current data rate – negotiated or set directly
// @param phyAddr bootstrap address of the PHY
// @return 0x1 if 1000 BASE-T1, 0x0 if 100 BASE-T1
uint16_t mv88q211x_get_speed(struct phy_device *phydev)
{
	uint16_t reg_val;

	if (mv88q211x_is_aneg_enabled(phydev)) {
		reg_val = phy_read_mmd(phydev, 7, 0x801a);

		return ((reg_val & 0x4000) >> 14);
	}
	else {
		reg_val = phy_read_mmd(phydev, 1, 0x0834);
		return (reg_val & 0x0001);
	}
}

// @param phyAddr bootstrap address of the PHY
// @return PHY revision MRVL_88Q2112_XX
uint16_t get_revision(struct phy_device *phydev)
{
	uint16_t revision;

	revision = phy_read_mmd(phydev, 1, 0x0003);

	return (revision & 0x000F);
}

// See if Link Partner signalled LEGACY mode during Training stage of 1000 BASE-T1.
// During PAM2 training stage, PHYs can exchange PHY Revision information.
// 88Q2112 revisions Z1/A0/A1 will signal MRVL_88Q2112_MODE_LEGACY if Init sequence is executed using API V1.10 or later version.
// This method is used with A2 only, as Z1/A0/A1 are always in LEGACY mode.
// @param phyAddr bootstrap address of the PHY
// @return true if Link Partner in LEGACY mode and speed is 1000 BASE-T1, false otherwise.
static bool is_link_partner_in_legacy_mode(struct phy_device *phydev)
{
	uint16_t speed;
	uint16_t phy_rev;

	speed = mv88q211x_get_speed(phydev);

	if (speed == MRVL_88Q2112_100BASE_T1)
		return false;

	phy_rev = get_revision(phydev);

	if (phy_rev == MRVL_88Q2112_A2) {
		uint16_t mode;

		mode = phy_read_mmd(phydev, 1, 0x0903);

		if ((MRVL_88Q2112_MODE_MSK & mode) == MRVL_88Q2112_MODE_LEGACY)
			return true;
	}

	return false;
}

/* FIXME: use settings array */
void apply_setting_1gbit(struct phy_device *phydev, bool aneg)
{
	int i;
	uint16_t reg_val;
	uint16_t phy_rev;

	if (aneg)
		phy_write_mmd(phydev, 7, 0x0200,
			MRVL_88Q2112_AN_ENABLE | MRVL_88Q2112_AN_RESTART);
	else
		phy_write_mmd(phydev, 7, 0x0200, MRVL_88Q2112_AN_DISABLE);

	phy_rev = get_revision(phydev);
	switch (phy_rev) {
	case MRVL_88Q2112_A2:       // fall-through to MRVL_88Q2112_A1 intentional
	case MRVL_88Q2112_A1:       // fall-through to MRVL_88Q2112_A0 intentional
	case MRVL_88Q2112_A0:
		phy_write_mmd(phydev, 1, 0x0900, 0x4000);

		reg_val = phy_read_mmd(phydev, 1, 0x0834);
		reg_val = (reg_val & 0xFFF0) | 0x0001;
		phy_write_mmd(phydev, 1, 0x0834, reg_val);

		phy_write_mmd(phydev, 3, 0xFFE4, 0x07B5);
		phy_write_mmd(phydev, 3, 0xFFE4, 0x06B6);
		mdelay(5);

		phy_write_mmd(phydev, 3, 0xFFDE, 0x402F);
		phy_write_mmd(phydev, 3, 0xFE2A, 0x3C3D);
		phy_write_mmd(phydev, 3, 0xFE34, 0x4040);
		phy_write_mmd(phydev, 3, 0xFE4B, 0x9337);
		phy_write_mmd(phydev, 3, 0xFE2A, 0x3C1D);
		phy_write_mmd(phydev, 3, 0xFE34, 0x0040);
		phy_write_mmd(phydev, 7, 0x8032, 0x0064);
		phy_write_mmd(phydev, 7, 0x8031, 0x0A01);
		phy_write_mmd(phydev, 7, 0x8031, 0x0C01);
		phy_write_mmd(phydev, 3, 0xFE0F, 0x0000);
		phy_write_mmd(phydev, 3, 0x800C, 0x0000);
		phy_write_mmd(phydev, 3, 0x801D, 0x0800);
		phy_write_mmd(phydev, 3, 0xFC00, 0x01C0);
		phy_write_mmd(phydev, 3, 0xFC17, 0x0425);
		phy_write_mmd(phydev, 3, 0xFC94, 0x5470);
		phy_write_mmd(phydev, 3, 0xFC95, 0x0055);
		phy_write_mmd(phydev, 3, 0xFC19, 0x08D8);
		phy_write_mmd(phydev, 3, 0xFC1a, 0x0110);
		phy_write_mmd(phydev, 3, 0xFC1b, 0x0A10);
		phy_write_mmd(phydev, 3, 0xFC3A, 0x2725);
		phy_write_mmd(phydev, 3, 0xFC61, 0x2627);
		phy_write_mmd(phydev, 3, 0xFC3B, 0x1612);
		phy_write_mmd(phydev, 3, 0xFC62, 0x1C12);
		phy_write_mmd(phydev, 3, 0xFC9D, 0x6367);
		phy_write_mmd(phydev, 3, 0xFC9E, 0x8060);
		phy_write_mmd(phydev, 3, 0xFC00, 0x01C8);
		phy_write_mmd(phydev, 3, 0x8000, 0x0000);
		phy_write_mmd(phydev, 3, 0x8016, 0x0011);

		if (phy_rev != MRVL_88Q2112_A0)
			phy_write_mmd(phydev, 3, 0xFDA3, 0x1800);

		phy_write_mmd(phydev, 3, 0xFE02, 0x00C0);
		phy_write_mmd(phydev, 3, 0xFFDB, 0x0010);
		phy_write_mmd(phydev, 3, 0xFFF3, 0x0020);
		phy_write_mmd(phydev, 3, 0xFE40, 0x00A6);

		phy_write_mmd(phydev, 3, 0xFE60, 0x0000);
		phy_write_mmd(phydev, 3, 0xFE04, 0x0008);
		phy_write_mmd(phydev, 3, 0xFE2A, 0x3C3D);
		phy_write_mmd(phydev, 3, 0xFE4B, 0x9334);

		phy_write_mmd(phydev, 3, 0xFC10, 0xF600);
		phy_write_mmd(phydev, 3, 0xFC11, 0x073D);
		phy_write_mmd(phydev, 3, 0xFC12, 0x000D);
		phy_write_mmd(phydev, 3, 0xFC13, 0x0010);
	break;

	default:    // Z1 case
		// port init.
		phy_write_mmd(phydev, 3, 0x0000, 0x0000);
		phy_write_mmd(phydev, 1, 0x0900, 0x0000);
		phy_write_mmd(phydev, 3, 0x800d, 0x0000);
		// Link LED
		phy_write_mmd(phydev, 3, 0x8000, 0x0000);
		phy_write_mmd(phydev, 3, 0x8016, 0x0011);
		// restore default from 100M
		phy_write_mmd(phydev, 3, 0xFE05, 0x0000);
		phy_write_mmd(phydev, 3, 0xFE07, 0x6A10);
		phy_write_mmd(phydev, 3, 0xFB95, 0x5720);
		phy_write_mmd(phydev, 3, 0xFE5D, 0x175C);
		phy_write_mmd(phydev, 3, 0x8016, 0x0071);
		//set speed
		reg_val = phy_read_mmd(phydev, 1, 0x0834);
		reg_val = (reg_val & 0xFFF0) | 0x0001;
		phy_write_mmd(phydev, 1, 0x0834, reg_val);
		mdelay(500);

		/* Z1 init code */
		for (i = 0; i < ARRAY_SIZE(z1_init_data); i++)
			phy_write_mmd(phydev, z1_init_data[i].devad, z1_init_data[i].regnum, z1_init_data[i].val);
	break;
	}
}


static void apply_op_mode_cfg(struct phy_device *phydev)
{
	uint16_t phy_rev;

	phy_rev = get_revision(phydev);
	if(phy_rev == MRVL_88Q2112_A1) {
		phy_write_mmd(phydev, 3, 0xFDB8, 0x0001);    //Set A1 to legacy mode
		phy_write_mmd(phydev, 1, 0x0902,
				MRVL_88Q2112_MODE_LEGACY | MRVL_88Q2112_MODE_ADVERTISE);
	}
	else if((phy_rev == MRVL_88Q2112_A0) || (phy_rev == MRVL_88Q2112_Z1))
		phy_write_mmd(phydev, 1, 0x0902,
				MRVL_88Q2112_MODE_LEGACY | MRVL_88Q2112_MODE_ADVERTISE);
	else if(phy_rev == MRVL_88Q2112_A2) {
		// Set back to default compliant mode setting
		phy_write_mmd(phydev, 3, 0xFDB8, 0x0000);
		phy_write_mmd(phydev, 3, 0xFD3D, 0x0000);

		phy_write_mmd(phydev, 1, 0x0902, MRVL_88Q2112_MODE_DEFAULT |
											MRVL_88Q2112_MODE_ADVERTISE);
	}
}

// Get current master/slave setting
// @param phyAddr bootstrap address of the PHY
// @return 0x1 if master, 0x0 if slave
static bool is_master(struct phy_device *phydev)
{
	int ret;

	ret = phy_read_mmd(phydev, 7, 0x8001);

	return (((ret >> 14) & 0x0001) == 0x0001);
}

void soft_reset_100mbit(struct phy_device *phydev)
{
	uint16_t reg_val = 0;
	uint16_t phy_rev;

	phy_rev = get_revision(phydev);

	/* A2/A1/A0 */
	if (phy_rev != MRVL_88Q2112_Z1) {
		phy_write_mmd(phydev, 3, 0x0900, 0x8000);
		phy_write_mmd(phydev, 3, 0xFA07, 0x0200);
	}
	else {    /* Z1 Case */
		reg_val = phy_read_mmd(phydev, 3, 0x0900);
		phy_write_mmd(phydev, 3, 0x0900, reg_val | 0x8000);
		mdelay(5);
	}
}

void soft_reset_1gbit(struct phy_device *phydev)
{
	uint16_t reg_val;
	uint16_t phy_rev;

	phy_rev = get_revision(phydev);

    /* A2/A1/A0 */
	if (phy_rev != MRVL_88Q2112_Z1) {
		if (mv88q211x_is_aneg_enabled(phydev)) {
			phy_write_mmd(phydev, 3, 0xFFF3, 0x0024);
		}
		//enable low-power mode
		reg_val = phy_read_mmd(phydev, 1, 0x0000);
		phy_write_mmd(phydev, 1, 0x0000, reg_val | 0x0800);

		phy_write_mmd(phydev, 3, 0xFFF3, 0x0020);
		phy_write_mmd(phydev, 3, 0xFFE4, 0x000C);
		mdelay(1);

		phy_write_mmd(phydev, 3, 0xffe4, 0x06B6);

		// disable low-power mode
		phy_write_mmd(phydev, 1, 0x0000, reg_val & 0xF7FF);
		mdelay(1);

		phy_write_mmd(phydev, 3, 0xFC47, 0x0030);
		phy_write_mmd(phydev, 3, 0xFC47, 0x0031);
		phy_write_mmd(phydev, 3, 0xFC47, 0x0030);
		phy_write_mmd(phydev, 3, 0xFC47, 0x0000);
		phy_write_mmd(phydev, 3, 0xFC47, 0x0001);
		phy_write_mmd(phydev, 3, 0xFC47, 0x0000);

		phy_write_mmd(phydev, 3, 0x0900, 0x8000);

		phy_write_mmd(phydev, 1, 0x0900, 0x0000);
		phy_write_mmd(phydev, 3, 0xFFE4, 0x000C);
	}
	else {  // Z1 Case
		reg_val = phy_read_mmd(phydev, 3, 0x0900);
		phy_write_mmd(phydev, 3, 0x0900, reg_val | 0x8000);
		mdelay(5);
	}
}

// Init GE PHY with single SEND_S
// Update to send single SEND_S after PHY initialized to GE speed with proper MASTER/SLAVE setting (PHY sends multiple SEND_S by default).
// @param phyAddr bootstrap address of the PHY
static void single_send_S_1gbit(struct phy_device *phydev)
{
	phy_write_mmd(phydev, 7, 0x8032, 0x0);
	phy_write_mmd(phydev, 7, 0x8031, 0xA1F);
	phy_write_mmd(phydev, 7, 0x8031, 0xC1F);
	phy_write_mmd(phydev, 7, 0x8032, 0x1);
	phy_write_mmd(phydev, 7, 0x8031, 0xA1B);
	phy_write_mmd(phydev, 7, 0x8031, 0xC1B);
	phy_write_mmd(phydev, 7, 0x8032, 0xB);
	phy_write_mmd(phydev, 7, 0x8031, 0xA1C);
	phy_write_mmd(phydev, 7, 0x8031, 0xC1C);
	phy_write_mmd(phydev, 3, 0x800C, 0x8);
	phy_write_mmd(phydev, 7, 0x8035, 0x27B8);
	phy_write_mmd(phydev, 7, 0x8036, 0x25);

	if (is_master(phydev)) {
		phy_write_mmd(phydev, 7, 0x8032, 0x5A);
		phy_write_mmd(phydev, 7, 0x8031, 0xA01);
		phy_write_mmd(phydev, 7, 0x8031, 0xC01);
		phy_write_mmd(phydev, 7, 0x8032, 0x1D);
		phy_write_mmd(phydev, 7, 0x8031, 0xA22);
		phy_write_mmd(phydev, 7, 0x8031, 0xC22);
		phy_write_mmd(phydev, 3, 0xFE04, 0x8);
	}
	else {
		phy_write_mmd(phydev, 7, 0x8032, 0x64);
		phy_write_mmd(phydev, 7, 0x8031, 0xA01);
		phy_write_mmd(phydev, 7, 0x8031, 0xC01);
		phy_write_mmd(phydev, 7, 0x8032, 0x1A);
		phy_write_mmd(phydev, 7, 0x8031, 0xA22);
		phy_write_mmd(phydev, 7, 0x8031, 0xC22);
		phy_write_mmd(phydev, 3, 0xFE04, 0x10);
	}
}

/* FIXME: use settings array */
static void apply_setting_100mbit(struct phy_device *phydev, bool aneg)
{
	u16 reg_val;
	u16 phy_rev;

	mdelay(1);
	if (aneg)
		phy_write_mmd(phydev, 7, 0x0200,
					  MRVL_88Q2112_AN_ENABLE | MRVL_88Q2112_AN_RESTART);
	else
		phy_write_mmd(phydev, 7, 0x0200, MRVL_88Q2112_AN_DISABLE);

	phy_rev = get_revision(phydev);

    /* A2/A1/A0 */
	if (phy_rev != MRVL_88Q2112_Z1) {
		phy_write_mmd(phydev, 3, 0xFA07, 0x0202);

		reg_val = phy_read_mmd(phydev, 1, 0x0834);
		reg_val = reg_val & 0xFFF0;
		phy_write_mmd(phydev, 1, 0x0834, reg_val);
		mdelay(5);

		phy_write_mmd(phydev, 3, 0x8000, 0x0000);
		phy_write_mmd(phydev, 3, 0x8100, 0x0200);
		phy_write_mmd(phydev, 3, 0xFA1E, 0x0002);
		phy_write_mmd(phydev, 3, 0xFE5C, 0x2402);
		phy_write_mmd(phydev, 3, 0xFA12, 0x001F);
		phy_write_mmd(phydev, 3, 0xFA0C, 0x9E05);
		phy_write_mmd(phydev, 3, 0xFBDD, 0x6862);
		phy_write_mmd(phydev, 3, 0xFBDE, 0x736E);
		phy_write_mmd(phydev, 3, 0xFBDF, 0x7F79);
		phy_write_mmd(phydev, 3, 0xFBE0, 0x8A85);
		phy_write_mmd(phydev, 3, 0xFBE1, 0x9790);
		phy_write_mmd(phydev, 3, 0xFBE3, 0xA39D);
		phy_write_mmd(phydev, 3, 0xFBE4, 0xB0AA);
		phy_write_mmd(phydev, 3, 0xFBE5, 0x00B8);
		phy_write_mmd(phydev, 3, 0xFBFD, 0x0D0A);
		phy_write_mmd(phydev, 3, 0xFBFE, 0x0906);
		phy_write_mmd(phydev, 3, 0x801D, 0x8000);
		phy_write_mmd(phydev, 3, 0x8016, 0x0011);
	}
	else {  /* Z1 Case */
		// port init.
		phy_write_mmd(phydev, 3, 0x0000, 0x0000);
		phy_write_mmd(phydev, 1, 0x0900, 0x0000);
		phy_write_mmd(phydev, 3, 0x800D, 0x0000);
		// Link LED
		phy_write_mmd(phydev, 3, 0x8000, 0x0000);
		phy_write_mmd(phydev, 3, 0x8016, 0x0011);
		//set speed
		reg_val = phy_read_mmd(phydev, 1, 0x0834);
		reg_val = reg_val & 0xFFF0;
		phy_write_mmd(phydev, 1, 0x0834, reg_val);
		mdelay(500);

		// Init code
		phy_write_mmd(phydev, 3, 0x8000, 0x0000);
		phy_write_mmd(phydev, 3, 0xFE05, 0x3DAA);
		phy_write_mmd(phydev, 3, 0xFE07, 0x6BFF);
		phy_write_mmd(phydev, 3, 0xFB95, 0x52F0);
		phy_write_mmd(phydev, 3, 0xFE5D, 0x171C);
		phy_write_mmd(phydev, 3, 0x8016, 0x0011);
		phy_write_mmd(phydev, 3, 0x0900, 0x8000);
	}
}

// Initialize for 100 BASE-T1
// @param phyAddr bootstrap address of the PHY
// @return void
static void init_100mbit(struct phy_device *phydev)
{
	apply_setting_100mbit(phydev, MRVL_88Q2112_AN_DISABLE);
	soft_reset_100mbit(phydev);
}


// Switch speed during linkup (ANEG is disabled)
// Change speed on the fly with linkup may cause SLAVE to stay with previous speed.
// This function ensure device go through a clean procedure and linkup with new speed.
// Apply this function in linkedup DUT and LP: both devices perform speed switch to the same speed.
// @param phyAddr bootstrap address of the PHY
// @param targetSpeed the target speed to switch to
static int setup_forced_speed_during_linkup(struct phy_device *phydev, u16 speed)
{
	u16 reg_val;

	if (mv88q211x_is_aneg_enabled(phydev))
		return -EINVAL;

	/* Set target speed */
	phy_write_mmd(phydev, 1, 0x0000, phy_read_mmd(phydev, 1, 0x0000) | 0x0800);
	mdelay(10);
	if (MRVL_88Q2112_1000BASE_T1 == speed) {
		reg_val = phy_read_mmd(phydev, 1, 0x0834);

		phy_write_mmd(phydev, 1, 0x0834, ((reg_val & 0xFFF0) | 0x0001));
		phy_write_mmd(phydev, 3, 0xffe4, 0x07B6);
	}
	else {
		phy_read_mmd(phydev, 1, 0x0834);
		phy_write_mmd(phydev, 1, 0x0834, (reg_val & 0xFFF0));
	}
	reg_val = phy_read_mmd(phydev, 1, 0x0000);
	phy_write_mmd(phydev, 1, 0x0000, (reg_val));
	mdelay(1);

	return 0;
}



// Get link status including link, local rx, remote rx and descrambler lock(speed FE/100m) status.
// @param phyAddr bootstrap address of the PHY
// @return true if link is up, false otherwise
static bool check_link(struct phy_device *phydev)
{
	u16 reg_data1, reg_data2, reg_data3;

	if (MRVL_88Q2112_1000BASE_T1 == mv88q211x_get_speed(phydev)) {
		reg_data1 = phy_read_mmd(phydev, 3, 0x0901);
		reg_data1 = phy_read_mmd(phydev, 3, 0x0901);	// Read twice: link latches low status
		reg_data2 = phy_read_mmd(phydev, 7, 0x8001);	// local and remote receiver status
		reg_data3 = phy_read_mmd(phydev, 3, 0xFD9D);	// local receiver status 2
		return (0x0004 == (reg_data1 & 0x0004)) && (0x3000 == (reg_data2 & 0x3000))
				&& (0x0010 == (reg_data3 & 0x0010));
	}
	else {
		reg_data1 = phy_read_mmd(phydev, 3, 0x8109);	// link
		reg_data2 = phy_read_mmd(phydev, 3, 0x8108);	// local and remote receiver status
		reg_data3 = phy_read_mmd(phydev, 3, 0x8230);	// descrambler lock status
		return (0x0004 == (reg_data1 & 0x0004)) && (0x3000 == (reg_data2 & 0x3000))
				&& (0x1 == (reg_data3 & 0x1));
	}
}

static void set_master_slave(struct phy_device *phydev, bool forced_master)
{
	int ret;

	ret = phy_read_mmd(phydev, 1, 0x0834);

	if (forced_master)
		ret |= 0x4000;
	else
		ret &= 0xBFFF;

	phy_write_mmd(phydev, 1, 0x0834, ret);
}

// Obtain 88Q211x model number from a register. Useful to verify SMI connection.
// @param phyAddr bootstrap address of the PHY
// @return constant value MRVL_88Q2112_PRODUCT_ID read from a register
static uint16_t get_product_id(struct phy_device *phydev)
{
	uint16_t product_id;

	product_id = phy_read_mmd(phydev, 1, 0x0003);
	return ((product_id & 0x03F0) >> 4);
}

// Sample pseudo-code to initialize PHY in 100/1000 BASE-T1 mode WITHOUT Auto-Neg.
// @param phyAddr bootstrap address of the PHY
// @param flags bitwise flags to set PHY parameters:
//        MRVL_88Q2112_INIT_MASTER
//        MRVL_88Q2112_INIT_FE
//        MRVL_88Q2112_INIT_GE
// @return see MRVL_88Q2112_INIT_#### return codes
static int phy_init(struct phy_device *phydev, uint16_t flags)
{
	int i;
	uint16_t product_id;

	bool linkup_before_set_mode = false;

	for(i = 0; i < 10; i++) {
		product_id = get_product_id(phydev);
		mrvl_info("%s(): product_id = %d\n", __FUNCTION__, product_id);
		if (product_id == MRVL_88Q2112_PRODUCT_ID)
			break;
	}

	if (product_id != MRVL_88Q2112_PRODUCT_ID)
		return -EINVAL;

    // 2) Init Sequence
	set_master_slave(phydev, (MRVL_88Q2112_INIT_MASTER & flags));

	if (check_link(phydev)) {
		linkup_before_set_mode = true;
	}

	if ((MRVL_88Q2112_INIT_GE & flags) == MRVL_88Q2112_INIT_GE) {

		if (linkup_before_set_mode) {
			/* If the link is up before init mode, appy setup_forced_speed_during_linkup */
			setup_forced_speed_during_linkup(phydev, MRVL_88Q2112_1000BASE_T1);
		}

		apply_setting_1gbit(phydev, MRVL_88Q2112_AN_DISABLE);

		/* If the link is down, apply init script directly */
		if (MRVL_88Q2112_SINGLE_SEND_S_ENABLE) {
			single_send_S_1gbit(phydev);
		}

		apply_op_mode_cfg(phydev);
		soft_reset_1gbit(phydev);
	}
	else if ((MRVL_88Q2112_INIT_FE & flags) == MRVL_88Q2112_INIT_FE) {
		if (linkup_before_set_mode) {
			//If the link is up before init mode, appy setup_forced_speed_during_linkup
			setup_forced_speed_during_linkup(phydev, MRVL_88Q2112_100BASE_T1);
		}

		init_100mbit(phydev);
	}
	else {
		return -MRVL_88Q2112_INIT_ERROR_ARGS;
	}

	// 3) Timeout until link-up and compatibility monitor
	for (i = 0; i < MRVL_88Q2112_LINKUP_TIMEOUT; i = i + 10) {
		mdelay(10);
		if (check_link(phydev)) {
			// link up, exit with success
			mrvl_notice("%s(): Link is up\n", __FUNCTION__);
			phydev->link = 1;
			return 0;
		}
		else {
			// 1000 BASE-T1: During training stage, PHYs can exchange PHY Revision information.
			// A2 only: if link partner signaled LEGACY mode, exit early and reconfigure to support
			// THIS WILL ONLY WORK IF LINK PARTNER 88Q2110/88Q2112 Z1/A0/A1 EXECUTED INIT SEQUENCE (using API V1.10 or later version)!
			if (is_link_partner_in_legacy_mode(phydev))
				break;
		}
		//printk("%s:%d Link is down\n", __FUNCTION__, __LINE__);
		phydev->link = 0;
		phydev->speed = SPEED_UNKNOWN;

	}

	// 4) 1000 BASE-T1: If link is not established through normal process, here are some possibilities:
//   a) Local Master/Slave setting is incorrect - rerun function with correct setting
//   b) Link Partner failed to initialize - system-level problem handled in upper layers
//   c) Link Partner is Legacy 88Q2112 Z1/A0/A1. Signaled LEGACY in step (3) - restart timer
//   d) Link Partner is 88Q2112 Z1/A0/A1 revision and did not execute API V1.10 or later version.
//      Switch mode and restart timer.

	if (MRVL_88Q2112_1000BASE_T1 == mv88q211x_get_speed(phydev)) {
		apply_op_mode_cfg(phydev);

		// There is no need to toggle reset or execute init sequence again.
		for (i = 0; i < MRVL_88Q2112_LINKUP_TIMEOUT; i = i + 10) {
			mdelay(10);
			if (check_link(phydev)) {
				// link up, exit with success
				mrvl_notice("%s(): Link is up\n", __FUNCTION__);
				phydev->link = 1;
				//return MRVL_88Q2112_INIT_DONE;
				return 0;
			}
			// no else statement
		}
		mrvl_warn("%s(): Link is down\n", __FUNCTION__);
		phydev->link = 0;
		phydev->speed = SPEED_UNKNOWN;
	}

	// Init failed - no link within expected time.
	// 1000 BASE-T1: a loop could be implemented
	// to toggle back into MRVL_88Q2112_MODE_DEFAULT and try again.
	return 0;
}

static inline int mv88q211x_hwmon_config(struct phy_device *phydev, bool enable)
{
	mrvl_info("mv88q211x_hwmon_config()\n");
	return 0;
}

static int mv88q211x_power_down(struct phy_device *phydev)
{
	u16 reg_val;

	mrvl_info("mv88q211x_power_down()\n");

	//enable low-power mode
	reg_val = phy_read_mmd(phydev, 1, 0x0000);
	phy_write_mmd(phydev, 1, 0x0000, reg_val | 0x0800);



#if NOT_USED
	return phy_set_bits_mmd(phydev, MDIO_MMD_VEND2, MV_V2_PORT_CTRL,
				MV_V2_PORT_CTRL_PWRDOWN);
#endif
	return 0;
}

static int mv88q211x_power_up(struct phy_device *phydev)
{
	u16 reg_val;

	mrvl_info("mv88q211x_power_up()\n");


	// disable low-power mode
	reg_val = phy_read_mmd(phydev, 1, 0x0000);
	phy_write_mmd(phydev, 1, 0x0000, reg_val & 0xF7FF);
	mdelay(1);

	genphy_c45_read_status(phydev);


	#if NOT_USED
	struct mv3310_priv *priv = dev_get_drvdata(&phydev->mdio.dev);
	int ret;

	ret = phy_clear_bits_mmd(phydev, MDIO_MMD_VEND2, MV_V2_PORT_CTRL,
				 MV_V2_PORT_CTRL_PWRDOWN);

	if (phydev->drv->phy_id != MARVELL_PHY_ID_88X3310 ||
	    priv->firmware_ver < 0x00030000)
		return ret;

	return phy_set_bits_mmd(phydev, MDIO_MMD_VEND2, MV_V2_PORT_CTRL,
				MV_V2_PORT_CTRL_SWRST);
#endif
	return 0;
}

static int mv88q211x_reset(struct phy_device *phydev)
{
	mrvl_info("mv88q211x_reset()\n");
#if NOT_USED
	int val, err;

	err = phy_modify_mmd(phydev, MDIO_MMD_PCS, unit + MDIO_CTRL1,
			     MDIO_CTRL1_RESET, MDIO_CTRL1_RESET);
	if (err < 0)
		return err;

	return phy_read_mmd_poll_timeout(phydev, MDIO_MMD_PCS,
					 unit + MDIO_CTRL1, val,
					 !(val & MDIO_CTRL1_RESET),
					 5000, 100000, true);

#endif
	return 0;
}

static int mv88q211x_probe(struct phy_device *phydev)
{
	phydev->priv = &log_level;
	phydev->is_c45 = 1;

	mrvl_info("mv88q211x_probe()\n");


#if NOT_USED

	uint16_t get_product_id ( struct rswitch_etha *etha, uint16_t phyAddr ) {
	    uint16_t modelNum = phy_read_mmd(phydev, 1, 0x0003);
	    return ( (modelNum & 0x03F0) >> 4 );
	}
#endif
	return 0;

}

static void mv88q211x_remove(struct phy_device *phydev)
{
	mrvl_info("mv88q211x_remove()\n");
	mv88q211x_hwmon_config(phydev, false);
}

static int mv88q211x_suspend(struct phy_device *phydev)
{
	mrvl_info("mv88q211x_suspend()\n");
	return mv88q211x_power_down(phydev);
}

static int mv88q211x_resume(struct phy_device *phydev)
{
	int ret;

	mrvl_info("mv88q211x_resume()\n");

	ret = mv88q211x_power_up(phydev);
	if (ret)
		return ret;

	return mv88q211x_hwmon_config(phydev, true);
}

static int mv88q211x_config_init(struct phy_device *phydev)
{
	int ret;
	mrvl_info("mv88q211x_config_init()\n");

	mrvl_info("mv88q211x: phydev->port: %i\n", phydev->port);

	phydev->master_slave_get = MASTER_SLAVE_CFG_UNKNOWN;
	if(!phydev->is_c45) {
		mrvl_info("mv88q211x_config_init() Not c45 !\n");
		phydev->is_c45 = 1;
	}
	phydev->autoneg = AUTONEG_DISABLE;

	ret = phy_init(phydev, (MRVL_88Q2112_INIT_GE | MRVL_88Q2112_INIT_MASTER));

	mrvl_info("mv88q211x_config_init() ret=%d\n", ret);

	return ret;
}

static int mv88q211x_get_features(struct phy_device *phydev)
{

	int ret;

	ret = genphy_c45_pma_read_abilities(phydev);
	if (ret)
		return ret;
	linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT1_Full_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_1000baseT1_Full_BIT, phydev->supported);



#if NOT_USED

	if (mv3310_has_pma_ngbaset_quirk(phydev)) {
		val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD,
				   MDIO_PMA_NG_EXTABLE);
		if (val < 0)
			return val;

		linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_NG_EXTABLE_2_5GBT);

		linkmode_mod_bit(ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_NG_EXTABLE_5GBT);
	}
#endif

	return 0;
}


#define MDIO_PMA_CTRL2_BASET1		0x003D  /* BASE-T1 type */
#define MDIO_PMA_EXTABLE_BT1		0x0800	/* BASE-T1 ability */
#define MDIO_PMA_PMD_BT1	18	/* BASE-T1 PMA/PMD extended ability */
#define MDIO_PMA_PMD_BT1_CTRL	2100	/* BASE-T1 PMA/PMD control register */

/* BASE-T1 PMA/PMD control register */
#define MDIO_PMA_PMD_BT1_CTRL_CFG_MST	0x4000 /* MASTER-SLAVE config value */


/**
 * mv88q211x_c45_pma_baset1_setup_master_slave - configures forced master/slave
 * role of BaseT1 devices.
 * @phydev: target phy_device struct
 */
static int mv88q211x_c45_pma_baset1_setup_master_slave(struct phy_device *phydev)
{
	int ctl = 0;

	switch (phydev->master_slave_set) {
		case MASTER_SLAVE_CFG_MASTER_PREFERRED:
		case MASTER_SLAVE_CFG_MASTER_FORCE:
			ctl = MDIO_PMA_PMD_BT1_CTRL_CFG_MST;
			break;
		case MASTER_SLAVE_CFG_SLAVE_FORCE:
		case MASTER_SLAVE_CFG_SLAVE_PREFERRED:
			break;
		case MASTER_SLAVE_CFG_UNKNOWN:
		case MASTER_SLAVE_CFG_UNSUPPORTED:
			return 0;
	default:
		mrvl_warn("Unsupported Master/Slave mode\n");
		return -EOPNOTSUPP;
	}

	return phy_modify_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1_CTRL,
				 MDIO_PMA_PMD_BT1_CTRL_CFG_MST, ctl);
}


static int mv88q211x_config_aneg(struct phy_device *phydev)
{
	bool changed = false;
	int ret;

	mrvl_notice("mv88q211x_config_aneg(): speed=%d   autoneg=%d    master_slave_state=%d\n", phydev->speed, phydev->autoneg, phydev->master_slave_state);

	if (phydev->autoneg == AUTONEG_DISABLE) {

		/* If speed not specified, get current setting */
		if(phydev->speed == SPEED_UNKNOWN) {
			genphy_c45_read_pma(phydev);
		} else if (phydev->speed == SPEED_100) {
			mrvl_warn("Setting 100Mbit/s\n");
			init_100mbit(phydev);
		} else if (phydev->speed == SPEED_1000) {
			mrvl_warn("Setting 1Gbit/s\n");

			apply_setting_1gbit(phydev, MRVL_88Q2112_AN_DISABLE);

			/* If the link is down, apply init script directly */
			if (MRVL_88Q2112_SINGLE_SEND_S_ENABLE) {
				single_send_S_1gbit(phydev);
			}

			apply_op_mode_cfg(phydev);
			soft_reset_1gbit(phydev);
		}

		ret = genphy_c45_pma_setup_forced(phydev);
		if(ret != 0) {
			mrvl_warn("PMA forced setup failed: %d\n", ret);
			return ret;
		}
		else {
			ret = mv88q211x_c45_pma_baset1_setup_master_slave(phydev);
			if(ret != 0) {
				mrvl_warn("PMA baset1_setup_master_slave failed: %d\n", ret);
				return ret;
			}
			changed = true;
		}
		return genphy_c45_check_and_restart_aneg(phydev, changed);
	}
	else {
		mrvl_err("Auto-negotiation currently not supported");
		return -EOPNOTSUPP;

	}
#if NOT_USED
	bool changed = false;
	u16 reg;
	int ret;

	ret = mv3310_config_mdix(phydev);
	if (ret < 0)
		return ret;

	if (phydev->autoneg == AUTONEG_DISABLE)
		return genphy_c45_pma_setup_forced(phydev);

	ret = genphy_c45_an_config_aneg(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	/* Clause 45 has no standardized support for 1000BaseT, therefore
	 * use vendor registers for this mode.
	 */
	reg = linkmode_adv_to_mii_ctrl1000_t(phydev->advertising);
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN, MV_AN_CTRL1000,
			     ADVERTISE_1000FULL | ADVERTISE_1000HALF, reg);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	return genphy_c45_check_and_restart_aneg(phydev, changed);
#endif

	return 0;
}

static int mv88q211x_aneg_done(struct phy_device *phydev)
{
	mrvl_info("mv88q211x_aneg_done()\n");
#if NOT_USED
	int val;

	val = phy_read_mmd(phydev, MDIO_MMD_PCS, MV_PCS_BASE_R + MDIO_STAT1);
	if (val < 0)
		return val;

	if (val & MDIO_STAT1_LSTATUS)
		return 1;

	return genphy_c45_aneg_done(phydev);
#endif
	return 0;
}


/**
 * mv88q211x_c45_pma_baset1_read_master_slave - read forced master/slave
 * configuration
 * @phydev: target phy_device struct
 */

int mv88q211x_c45_pma_baset1_read_master_slave(struct phy_device *phydev)
{
	int val;

	phydev->master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;
	phydev->master_slave_get = MASTER_SLAVE_CFG_UNKNOWN;

	val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1_CTRL);
	if (val < 0)
		return val;

	if (val & MDIO_PMA_PMD_BT1_CTRL_CFG_MST) {
		phydev->master_slave_get = MASTER_SLAVE_CFG_MASTER_FORCE;
		phydev->master_slave_state = MASTER_SLAVE_STATE_MASTER;
	} else {
		phydev->master_slave_get = MASTER_SLAVE_CFG_SLAVE_FORCE;
		phydev->master_slave_state = MASTER_SLAVE_STATE_SLAVE;
	}

	return 0;
}


static int mv88q211x_read_status(struct phy_device *phydev)
{
	int ret;

	//mrvl_info("mv88q211x_read_status()\n");

	phydev->master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;


	ret = mv88q211x_c45_pma_baset1_read_master_slave(phydev);
	ret = genphy_c45_read_status(phydev);

	phydev->link = check_link(phydev);

	if(phydev->link) {
		if(mv88q211x_get_speed(phydev)) {
			phydev->speed = SPEED_1000;
		} else {
			phydev->speed = SPEED_100;
		}
	}

	if(is_master(phydev))
		phydev->master_slave_state = MASTER_SLAVE_STATE_MASTER;
	else
		phydev->master_slave_state = MASTER_SLAVE_STATE_SLAVE;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = genphy_c45_read_lpa(phydev);
		if (ret)
			return ret;

		phy_resolve_aneg_linkmode(phydev);
	} else {
		ret = genphy_c45_read_pma(phydev);
	}

	return ret;
}


static int mv88q211x_get_tunable(struct phy_device *phydev,
	struct ethtool_tunable *tuna, void *data)
{
	mrvl_info("mv88q211x_get_tunable()\n");

#if NOT_USED
	switch (tuna->id) {
	case ETHTOOL_PHY_EDPD:
		return mv3310_get_edpd(phydev, data);
	default:
		return -EOPNOTSUPP;
	}
#else
	return -EOPNOTSUPP;
#endif

}

static int mv88q211x_set_tunable(struct phy_device *phydev,
	struct ethtool_tunable *tuna, const void *data)
{
	mrvl_info("mv88q211x_set_tunable()\n");
#if NOT_USED
	switch (tuna->id) {
	case ETHTOOL_PHY_EDPD:
		return mv3310_set_edpd(phydev, *(u16 *)data);
	default:
		return -EOPNOTSUPP;
	}
#else
	return -EOPNOTSUPP;
#endif
}



static struct phy_driver mv88q211x_drivers[] = {

{
		.phy_id = MARVELL_PHY_ID_88Q2110,
		.phy_id_mask = MARVELL_PHY_ID_MASK,
		.name = "mv88q211x",
		.soft_reset = mv88q211x_reset,
		.probe = mv88q211x_probe,
		.suspend = mv88q211x_suspend,
		.resume = mv88q211x_resume,
		.config_init = mv88q211x_config_init,
		.config_aneg = mv88q211x_config_aneg,
		.aneg_done = mv88q211x_aneg_done,
		.read_status = mv88q211x_read_status,
		.get_tunable = mv88q211x_get_tunable,
		.set_tunable = mv88q211x_set_tunable,
		.get_features = mv88q211x_get_features,
		.remove = mv88q211x_remove,
	},
};

module_phy_driver(mv88q211x_drivers);

static struct mdio_device_id __maybe_unused mv88q211x_tbl[] = {
		{MARVELL_PHY_ID_88Q2110, MARVELL_PHY_ID_MASK},
		{},
};
MODULE_DEVICE_TABLE(mdio, mv88q211x_tbl);
MODULE_DESCRIPTION("Marvell Base-T1 Ethernet PHY driver (MV88Q221x)");
MODULE_LICENSE("GPL");
