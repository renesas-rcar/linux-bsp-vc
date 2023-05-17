// SPDX-License-Identifier: GPL-2.0+
/*
 * Marvell Base-T1 88Q3344 ES PHY driver
 *
 * This is a dummy driver for the engineering sample (ES) of the Marvell 88Q3344
 * Base-T1 ethernet driver. The PHY comes up fully initialized in 2.5G mode.
 * So main purpose is to provide base functionality as speed reporting,
 * master/slave setting and reporting link up/down.
 *
 */
#include <linux/phy.h>
#include <linux/errno.h>
#include <linux/ethtool.h>


static int log_level = LOGLEVEL_ERR;
module_param(log_level,int,0660);

//////////////////// Virture reg read/write via command mailbox ////////////////////

#define MRVL_AMG_CMD_MB_CTRL  0x1E200
#define MRVL_AMG_CMD_MB_INFO  0x1E208
#define MRVL_AMG_FW_Reg_First	0x1F8000
#define MRVL_AMG_FW_Reg_Last	0x1FFFFF

#define MRVL_88Q3344_ES_FW_VER 0x70010400

/* ---------------------------------------------------------*/

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


static int MRVL_AMG_MB_getStatus(struct phy_device *phydev) {
	int i = 0;
	u16 val = 0;

	while (i < 100)
	{
		val = phy_read_mmd(phydev, 0x1E, 0x47);
		//device->readReg(device, device->phyAddr, 0x1E, 0x47, &val);
		if ((val & 0x2) == 0x2) {
			mrvl_warn("MRVL_AMG_MB_getStatus error: transaction error");
			return -1;
		}
		if ((val & 0x1) == 0) {
			return 0;
		}
		i++;
		msleep(50);
	}

	mrvl_err("MRVL_AMG_MB_getStatus error: transaction timeout\n");
	return -2;
}

static int MRVL_AMG_MB_readReg32(struct phy_device *phydev, u32 regaddr, u32* regVal)
{
	u16 val = 0;
	int status;

	phy_write_mmd(phydev, 0x1E, 0x40, (regaddr & 0xFFFF));
	phy_write_mmd(phydev, 0x1E, 0x41, (regaddr & 0xFFFF0000) >> 16);
	phy_write_mmd(phydev, 0x1E, 0x44, 0);


	status = MRVL_AMG_MB_getStatus(phydev);
	if (status != 0) {
		return status;
	}

	//device->readReg(device, device->phyAddr, 0x1E, 0x45, &val);
	val = phy_read_mmd(phydev, 0x1E, 0x45);
	*regVal = val;
	//device->readReg(device, device->phyAddr, 0x1E, 0x46, &val);
	val = phy_read_mmd(phydev, 0x1E, 0x46);
	*regVal += val << 16;

	mrvl_info("MRVL_AMG_MB_readReg32: 0x%x=0x%x\n", regaddr, *regVal);

	return 0;
}

static int MRVL_AMG_MB_writeReg32(struct phy_device *phydev, u32 regaddr, u32 regVal)
{


	phy_write_mmd(phydev, 0x1E, 0x40, (regaddr & 0xFFFF));
	phy_write_mmd(phydev, 0x1E, 0x41, (regaddr & 0xFFFF0000) >> 16);

	phy_write_mmd(phydev, 0x1E, 0x42, (regVal & 0xFFFF));
	phy_write_mmd(phydev, 0x1E, 0x43, (regVal & 0xFFFF0000) >> 16);

	phy_write_mmd(phydev, 0x1E, 0x44, 1);

	mrvl_info("MRVL_AMG_MB_writeReg32: 0x%x=0x%x\n", regaddr, regVal);
	return MRVL_AMG_MB_getStatus(phydev);
}



bool MRVL_AMG_isVirtualReg(u32 regaddr)
{
	if (regaddr >= MRVL_AMG_FW_Reg_First && regaddr <= MRVL_AMG_FW_Reg_Last) {
		return true;
	}
	else {
		return false;
	}

}

static int MRVL_AMG_Virtual_getStatus(struct phy_device *phydev) {
	u32 regData = 0;
	int i = 0;

	while (i < 100)
	{
		MRVL_AMG_MB_readReg32(phydev, MRVL_AMG_CMD_MB_CTRL, &regData);
		if (((regData >> 4) & 0x1) == 0)
		{
			return 0;
		}
		i++;
		msleep(50);
	}

	mrvl_err("MRVL_AMG_Virtual_getStatus error: transaction timeout\n");
	return -1;
}

static int  MRVL_AMG_Virtual_clearStatus(struct phy_device *phydev) {
	u32 regData = 0;

	MRVL_AMG_MB_readReg32(phydev, MRVL_AMG_CMD_MB_CTRL, &regData);
	regData &= ~(0x10);
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_CTRL, regData);

	return 0;
}

static int  MRVL_AMG_Virtual_readReg32(struct phy_device *phydev, u32 regaddr, u32* regVal)
{
	u32 regData = 0;
	u32 tmp = 0;
	int status = -1;

	if (MRVL_AMG_isVirtualReg(regaddr) == false) {
		mrvl_err("MRVL_AMG_Virtual_readReg32 error: not a virtual reg");
		return -1;
	}

	//read reg
	tmp |= 0x2;
	tmp |= (regaddr >> 2) << 4;
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_INFO, tmp & 0xffff);
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_INFO + 4, (tmp >> 16) & 0xffff);

	//execute
	MRVL_AMG_MB_readReg32(phydev, MRVL_AMG_CMD_MB_CTRL, &regData);
	regData &= 0xfff0;
	regData |= 0xdb;
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_CTRL, regData);

	status = MRVL_AMG_Virtual_getStatus(phydev);

	if (status != 0) {
		mrvl_err("MRVL_AMG_Virtual_readReg32 error: reg 0x%x \n", regaddr);
		return status;
	}

	//read back
	MRVL_AMG_MB_readReg32(phydev, MRVL_AMG_CMD_MB_INFO, &regData);
	*regVal = regData & 0xffff;
	MRVL_AMG_MB_readReg32(phydev, MRVL_AMG_CMD_MB_INFO + 4, &regData);
	*regVal += (regData & 0xffff) << 16;

	mrvl_info("MRVL_AMG_Virtual_readReg32: reg 0x%x = 0x%x\n", regaddr, regVal);

	return 0;
}

static int  MRVL_AMG_Virtual_writeReg32(struct phy_device *phydev, u32 regaddr, u32 regVal)
{
	u32 regData = 0;
	u32 tmp = 0;
	int status = -1;

	if (MRVL_AMG_isVirtualReg(regaddr) == false) {
		mrvl_err("MRVL_AMG_Virtual_writeReg32 error: not a virtual reg\n");
		return -1;
	}

	//write reg
	tmp |= 0x3;
	tmp |= (regaddr >> 2) << 4;
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_INFO, tmp & 0xffff);
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_INFO + 4, (tmp >> 16) & 0xffff);
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_INFO + 8, regVal & 0xffff);
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_INFO + 12, (regVal >> 16) & 0xffff);

	//execute
	MRVL_AMG_MB_readReg32(phydev, MRVL_AMG_CMD_MB_CTRL, &regData);
	regData &= 0xfff0;
	regData |= 0xdb;
	MRVL_AMG_MB_writeReg32(phydev, MRVL_AMG_CMD_MB_CTRL, regData);

	status = MRVL_AMG_Virtual_getStatus(phydev);

	if (status != 0) {
		mrvl_err("MRVL_AMG_Virtual_writeReg32 error: reg 0x%x \n", regaddr);
		return status;
	}

	mrvl_info("MRVL_AMG_Virtual_writeReg32: reg 0x%x = 0x%x\n", regaddr, regVal);
	return 0;
}





/* ---------------------------------------------------------*/

// See if Auto-Negotiation is enabled
// @param phyAddr bootstrap address of the PHY
// @return true if enabled, false otherwise
static bool mv88q3344_es_aneg_enabled(struct phy_device *phydev)
{
	return false;
}




static inline int mv88q3344_es_hwmon_config(struct phy_device *phydev, bool enable)
{
	mrvl_info("mv88q3344_es_hwmon_config()\n");
	return 0;
}

static int mv88q3344_es_power_down(struct phy_device *phydev)
{

	mrvl_info("mv88q3344_es_power_down()\n");


	return 0;
}

static int mv88q3344_es_power_up(struct phy_device *phydev)
{

	mrvl_info("mv88q3344_es_power_up()\n");


	return 0;
}

static int mv88q3344_es_reset(struct phy_device *phydev)
{
	mrvl_info("mv88q3344_es_reset()\n");


	return 0;
}


/*
static MRVL_APHY_STATUS MRVL_AMG_GetFirmwareInfo(MRVL_DEV_PTR device, MRVL_AMG_FirmwareInfo* fwInfo)
{
	MRVL_APHY_U32 regData = 0;
	MRVL_AMG_Virtual_readReg32(device, 0x1F8000, &regData);

	fwInfo->candidateRevision = regData & 0xFF;
	fwInfo->minorRevision = (regData >> 8) & 0xFF;
	fwInfo->majorRevision = (regData >> 16) & 0xFFF;
	fwInfo->streamRevision = (regData >> 28) & 0xF;

	MRVL_AMG_MB_readReg32(device, 0x103F8, &regData);
	fwInfo->heartbeat = regData;
	MRVL_AMG_MB_readReg32(device, 0x103FC, &regData);
	fwInfo->heartbeat |= MRVL_APHY_U64 (regData) << 32;

	return MRVL_APHY_STATUS_OK;
}
*/

static int mv88q3344_es_match_phy_dev(struct phy_device *phydev)
{
	int i;

	if (!phydev->is_c45)
		return false;

	for (i = 1; i < ARRAY_SIZE(phydev->c45_ids.device_ids); i++) {
		if (phydev->c45_ids.device_ids[i] == 0xffffffff)
			continue;

		/* For engineering sample, all ids are zeroes */
		if (phydev->c45_ids.device_ids[i] != 0)
			return false;
	}

	return true;
}

static int mv88q3344_es_probe(struct phy_device *phydev)
{
	u32 regData = 0;

	phydev->priv = &log_level;

	mrvl_info("mv88q3344_es_probe(): phydev->port: %i  mdio-bus-name: '%s' mdio-bus-id '%s'\n", phydev->port, phydev->mdio.bus->name, phydev->mdio.bus->id);

	MRVL_AMG_Virtual_readReg32(phydev, 0x1F8000, &regData);
//[   18.872127] fwInfo 0.4.1.7
	// FW version: 7.1.	4.0

	mrvl_info("fwInfo reg: 0x%.8x", regData);

	mrvl_info("fwInfo %1d.%1d.%1d.%1d",
			(regData >> 28) & 0xF,
			(regData >> 16) & 0xFFF,  (regData >> 8) & 0xFF, regData & 0xFF);

	if(regData != MRVL_88Q3344_ES_FW_VER) {
		mrvl_warn("Unexpected firmware (0x%.8x)", regData);
		return -ENODEV;
	}

	return 0;
}

static void mv88q3344_es_remove(struct phy_device *phydev)
{
	mrvl_info("mv88q3344_es_remove()\n");

}

static int mv88q3344_es_suspend(struct phy_device *phydev)
{
	mrvl_info("mv88q3344_es_suspend()\n");
	return 0;
}

static int mv88q3344_es_resume(struct phy_device *phydev)
{
	mrvl_info("mv88q3344_es_resume()\n");
	return 0;
}

static int mv88q3344_es_config_init(struct phy_device *phydev)
{
	mrvl_info("mv88q3344_es_config_init()\n");
	return 0;
}

static int mv88q3344_es_get_features(struct phy_device *phydev)
{

	mrvl_info("mv88q3344_es_get_features()\n");

	phydev->speed = 2500;

	linkmode_empty(phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_MII_BIT, phydev->supported);
	//linkmode_set_bit(ETHTOOL_LINK_MODE_2500baseT1_Full_BIT, phydev->supported);

	linkmode_and(phydev->advertising, phydev->advertising,
		     phydev->supported);

	phydev->master_slave_get = MASTER_SLAVE_CFG_UNKNOWN;

	phydev->autoneg = AUTONEG_DISABLE;

	return 0;
}

/**
 * mv88q3344_es_setup_master_slave - configures forced master/slave
 * role of BaseT1 devices.
 * @phydev: target phy_device struct
 */
static bool mv88q3344_es_is_master(struct phy_device *phydev)
{
	u16 reg_val;

	reg_val = phy_read_mmd(phydev, 0x01, 0x00);
	if((reg_val & BIT(0)) == BIT(0)) {
		return true;
	}
	return false;

	return 0;
}


/**
 * mv88q3344_es_setup_master_slave - configures forced master/slave
 * role of BaseT1 devices.
 * @phydev: target phy_device struct
 */
static int mv88q3344_es_set_master_slave(struct phy_device *phydev)
{
	u16 reg_val;

	reg_val = phy_read_mmd(phydev, 0x01, 0x00);


	switch (phydev->master_slave_set) {
		case MASTER_SLAVE_CFG_MASTER_PREFERRED:
		case MASTER_SLAVE_CFG_MASTER_FORCE:
			reg_val |= BIT(0);
			break;
		case MASTER_SLAVE_CFG_SLAVE_FORCE:
		case MASTER_SLAVE_CFG_SLAVE_PREFERRED:
			reg_val &= ~BIT(0);
			break;
		case MASTER_SLAVE_CFG_UNKNOWN:
		case MASTER_SLAVE_CFG_UNSUPPORTED:
			return 0;
	default:
		mrvl_warn("Unsupported Master/Slave mode\n");
		return -EOPNOTSUPP;
	}

	phy_write_mmd(phydev, 0x01, 0x00, reg_val);
	phy_write_mmd(phydev, 7, 0x200, 0x200);

	return 0;
}

static int mv88q3344_es_config_aneg(struct phy_device *phydev)
{
	u16 reg_val;

	mrvl_info("mv88q3344_es_config_aneg(before): link=%d  speed=%d   autoneg=%d    master_slave_state=%d\n", phydev->link, phydev->speed, phydev->autoneg, phydev->master_slave_state);

	mrvl_info("mv88q3344_es_config_aneg(): master_slave_set: %d\n", phydev->master_slave_set);

	reg_val = phy_read_mmd(phydev, 0x1, 0x906);

	phydev->link = (reg_val & 0x1);
#if 0
	if(phydev->link)
		phydev->speed = SPEED_2500;
	else
		phydev->speed = SPEED_UNKNOWN;
#endif

	if((phydev->master_slave_set != MASTER_SLAVE_CFG_UNKNOWN) && (phydev->master_slave_set != MASTER_SLAVE_CFG_UNSUPPORTED)) {
		mv88q3344_es_set_master_slave(phydev);
		phydev->master_slave_state = phydev->master_slave_set;
	}

	mrvl_info("mv88q3344_es_config_aneg(after):  link=%d  speed=%d   autoneg=%d    master_slave_state=%d\n", phydev->link, phydev->speed, phydev->autoneg, phydev->master_slave_state);

	return 0;
}

static int mv88q3344_es_aneg_done(struct phy_device *phydev)
{
	mrvl_info("mv88q3344_es_aneg_done()\n");

	return 0;
}





static int mv88q3344_es_read_status(struct phy_device *phydev)
{
	u16 reg_val;

	//pr_info("mv88q3344_es_read_status()\n");
	reg_val = phy_read_mmd(phydev, 0x1, 0x906);

	phydev->link = (reg_val & 0x1);

	phydev->master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;
	phydev->master_slave_get = MASTER_SLAVE_CFG_UNKNOWN;

	if(mv88q3344_es_is_master(phydev)) {
		phydev->master_slave_get = MASTER_SLAVE_CFG_MASTER_FORCE;
		phydev->master_slave_state = MASTER_SLAVE_STATE_MASTER;
	} else {
		phydev->master_slave_get = MASTER_SLAVE_CFG_SLAVE_FORCE;
		phydev->master_slave_state = MASTER_SLAVE_STATE_SLAVE;
	}

	return 0;
}


static int mv88q3344_es_get_tunable(struct phy_device *phydev,
	struct ethtool_tunable *tuna, void *data)
{
	mrvl_info("mv88q3344_es_get_tunable()\n");

	return -EOPNOTSUPP;
}

static int mv88q3344_es_set_tunable(struct phy_device *phydev,
	struct ethtool_tunable *tuna, const void *data)
{
	mrvl_info("mv88q3344_es_set_tunable()\n");
	return -EOPNOTSUPP;
}

#define MARVELL_PHY_ID_88Q3344_ES 	0x00000000
#define MARVELL_PHY_ID_MASK			0xfffffff0

static struct phy_driver mv88q3344_es_drivers[] = {

{
		.phy_id = MARVELL_PHY_ID_88Q3344_ES,
		.phy_id_mask = MARVELL_PHY_ID_MASK,
		.name = "mv88q3344_es",
		.soft_reset = mv88q3344_es_reset,
		.probe = mv88q3344_es_probe,
		.match_phy_device = mv88q3344_es_match_phy_dev,
		.suspend = mv88q3344_es_suspend,
		.resume = mv88q3344_es_resume,
		.config_init = mv88q3344_es_config_init,
		.config_aneg = mv88q3344_es_config_aneg,
		.aneg_done = mv88q3344_es_aneg_done,
		.read_status = mv88q3344_es_read_status,
		.get_tunable = mv88q3344_es_get_tunable,
		.set_tunable = mv88q3344_es_set_tunable,
		.get_features = mv88q3344_es_get_features,
		.remove = mv88q3344_es_remove,
	},
};

module_phy_driver(mv88q3344_es_drivers);

static struct mdio_device_id __maybe_unused mv88q3344_es_tbl[] = {
		{MARVELL_PHY_ID_88Q3344_ES, MARVELL_PHY_ID_MASK},
		{},
};
MODULE_DEVICE_TABLE(mdio, mv88q3344_es_tbl);
MODULE_DESCRIPTION("Marvell 2.5G Base-T1 ES Ethernet PHY driver (MV88Q3344ES)");
MODULE_LICENSE("GPL");
