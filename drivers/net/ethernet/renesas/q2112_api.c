// File Name: q2112_api.cpp
// Summary:   88Q2110/88Q2112 API
//            Definition functions for DUT control and status access
// Version:   v1.24
// Date:      5/17/2021
// Comments:  SW API for 88Q2110/88Q2112
//
// Copyright(c) 2021 Marvell. All rights reserved.
// This file is subject to the limited use license agreement
// by and between Marvell and you, your employer or other entity on behalf of whom you act.
// In the absence of such license agreement the following file is subject
// to Marvell’s standard Limited Use License Agreement.


//#include <stdio.h>
//#include <stdint.h>
//#include <math.h>
//#include "mdio.h"
//#include "q2112_api.h"
//#include "configuration.h"
// 1000 BASE-T1 Operating Mode
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

uint16_t getModelNum(struct rswitch_etha *etha, uint16_t phyAddr);
uint16_t getRevNum(struct rswitch_etha *etha, uint16_t phyAddr);
void softReset(struct rswitch_etha *etha, uint16_t phyAddr);
void setMasterSlave(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t forceMaster);
uint16_t getMasterSlave(struct rswitch_etha *etha, uint16_t phyAddr);
bool checkLink(struct rswitch_etha *etha, uint16_t phyAddr);
bool getRealTimeLinkStatus(struct rswitch_etha *etha, uint16_t phyAddr);
bool getLatchedLinkStatus(struct rswitch_etha *etha, uint16_t phyAddr);
bool getAnegEnabled(struct rswitch_etha *etha, uint16_t phyAddr);
bool getAnegDone (struct rswitch_etha *etha, uint16_t phyAddr);
uint16_t getSpeed (struct rswitch_etha *etha, uint16_t phyAddr);
void initQ2112Fe(struct rswitch_etha *etha, uint16_t phyAddr);
bool initQ2112Ge(struct rswitch_etha *etha, uint16_t phyAddr);
bool setAneg(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t forceMaster, uint16_t flags);
bool manualSetOperateModeSetting(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t selfMode);
bool applyOperateModeConfig(struct rswitch_etha *etha, uint16_t phyAddr);
bool isLinkPartnerInLegacyMode(struct rswitch_etha *etha, uint16_t phyAddr);

#if 0
uint16_t getSQIReading_8Level(struct rswitch_etha *etha, uint16_t phyAddr);
uint16_t getSQIReading_16Level(struct rswitch_etha *etha, uint16_t phyAddr);
bool getCQIReading(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t *IL, uint16_t *RL);
bool getVCTReading(struct rswitch_etha *etha, uint16_t phyAddr, double *distanceToFault, char *cable_status);

bool setupForcedSpeedDuringLinkup(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t targetSpeed);
#endif

bool initQ2112Ge_SingleSendS(struct rswitch_etha *etha, uint16_t phyAddr);

// ================================================
// Code from internal shared configuration functions
// - _applyQ2112GeSetting: Speed 1000 related configuration
// - _applyQ2112FeSetting: Speed 100 related configuration
// - _applyQ2112AnegSetting: ANEG related configuration
// - _applyQ2112GeSoftReset: Reset for 1000 rate
// - _applyQ2112FeSoftReset: Reset for 100 rate
// ================================================

#define sleep(ms) mdelay(ms)
#define loadOperateModeSetting(etha, phyAddr) MRVL_88Q2112_MODE_DEFAULT
#define saveOperateModeSetting(etha, phyAddr, mode) 1

static uint16_t regRead(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t devAddr, uint16_t regAddr)
{
	return rswitch_etha_set_access(etha, true, phyAddr, devAddr, regAddr, 0);
}

static void regWrite(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t devAddr, uint16_t regAddr, uint16_t data)
{
	rswitch_etha_set_access(etha, false, phyAddr, devAddr, regAddr, data);
}

void _applyQ2112GeSetting (struct rswitch_etha *etha,  uint16_t phyAddr, uint16_t isAneg ) {
    uint16_t regData = 0;

	sleep(2);
    if (0x0 != isAneg)
        regWrite(etha, phyAddr, 7, 0x0200, MRVL_88Q2112_AN_ENABLE | MRVL_88Q2112_AN_RESTART);
    else
        regWrite(etha, phyAddr, 7, 0x0200, MRVL_88Q2112_AN_DISABLE);

    switch (getRevNum(etha, phyAddr)) {
    case MRVL_88Q2112_A2:       // fall-through to MRVL_88Q2112_A1 intentional
    case MRVL_88Q2112_A1:       // fall-through to MRVL_88Q2112_A0 intentional
    case MRVL_88Q2112_A0:
        regWrite(etha, phyAddr, 1, 0x0900, 0x4000);

        regData = regRead(etha, phyAddr, 1, 0x0834);
        regData = (regData & 0xFFF0) | 0x0001;
        regWrite(etha, phyAddr, 1, 0x0834, regData);

        regWrite(etha, phyAddr, 3, 0xFFE4, 0x07B5);
        regWrite(etha, phyAddr, 3, 0xFFE4, 0x06B6);
        sleep(5);

        regWrite(etha, phyAddr, 3, 0xFFDE, 0x402F);
        regWrite(etha, phyAddr, 3, 0xFE2A, 0x3C3D);
        regWrite(etha, phyAddr, 3, 0xFE34, 0x4040);
        regWrite(etha, phyAddr, 3, 0xFE4B, 0x9337);
        regWrite(etha, phyAddr, 3, 0xFE2A, 0x3C1D);
        regWrite(etha, phyAddr, 3, 0xFE34, 0x0040);
        regWrite(etha, phyAddr, 7, 0x8032, 0x0064);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0A01);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0C01);
        regWrite(etha, phyAddr, 3, 0xFE0F, 0x0000);
        regWrite(etha, phyAddr, 3, 0x800C, 0x0000);
        regWrite(etha, phyAddr, 3, 0x801D, 0x0800);
        regWrite(etha, phyAddr, 3, 0xFC00, 0x01C0);
        regWrite(etha, phyAddr, 3, 0xFC17, 0x0425);
        regWrite(etha, phyAddr, 3, 0xFC94, 0x5470);
        regWrite(etha, phyAddr, 3, 0xFC95, 0x0055);
        regWrite(etha, phyAddr, 3, 0xFC19, 0x08D8);
        regWrite(etha, phyAddr, 3, 0xFC1a, 0x0110);
        regWrite(etha, phyAddr, 3, 0xFC1b, 0x0A10);
        regWrite(etha, phyAddr, 3, 0xFC3A, 0x2725);
        regWrite(etha, phyAddr, 3, 0xFC61, 0x2627);
        regWrite(etha, phyAddr, 3, 0xFC3B, 0x1612);
        regWrite(etha, phyAddr, 3, 0xFC62, 0x1C12);
        regWrite(etha, phyAddr, 3, 0xFC9D, 0x6367);
        regWrite(etha, phyAddr, 3, 0xFC9E, 0x8060);
        regWrite(etha, phyAddr, 3, 0xFC00, 0x01C8);
        regWrite(etha, phyAddr, 3, 0x8000, 0x0000);
        regWrite(etha, phyAddr, 3, 0x8016, 0x0011);

        if (MRVL_88Q2112_A0 != getRevNum(etha, phyAddr))
            regWrite(etha, phyAddr, 3, 0xFDA3, 0x1800);

        regWrite(etha, phyAddr, 3, 0xFE02, 0x00C0);
        regWrite(etha, phyAddr, 3, 0xFFDB, 0x0010);
        regWrite(etha, phyAddr, 3, 0xFFF3, 0x0020);
        regWrite(etha, phyAddr, 3, 0xFE40, 0x00A6);

        regWrite(etha, phyAddr, 3, 0xFE60, 0x0000);
        regWrite(etha, phyAddr, 3, 0xFE04, 0x0008);
        regWrite(etha, phyAddr, 3, 0xFE2A, 0x3C3D);
        regWrite(etha, phyAddr, 3, 0xFE4B, 0x9334);

        regWrite(etha, phyAddr, 3, 0xFC10, 0xF600);
        regWrite(etha, phyAddr, 3, 0xFC11, 0x073D);
        regWrite(etha, phyAddr, 3, 0xFC12, 0x000D);
        regWrite(etha, phyAddr, 3, 0xFC13, 0x0010);
        break;

    default:    // Z1 case
        // port init.
        regWrite(etha, phyAddr, 3, 0x0000, 0x0000);
        regWrite(etha, phyAddr, 1, 0x0900, 0x0000);
        regWrite(etha, phyAddr, 3, 0x800d, 0x0000);
        // Link LED
        regWrite(etha, phyAddr, 3, 0x8000, 0x0000);
        regWrite(etha, phyAddr, 3, 0x8016, 0x0011);
        // restore default from 100M
        regWrite(etha, phyAddr, 3, 0xFE05, 0x0000);
        regWrite(etha, phyAddr, 3, 0xFE07, 0x6A10);
        regWrite(etha, phyAddr, 3, 0xFB95, 0x5720);
        regWrite(etha, phyAddr, 3, 0xFE5D, 0x175C);
        regWrite(etha, phyAddr, 3, 0x8016, 0x0071);
        //set speed
        regData = regRead(etha, phyAddr, 1, 0x0834);
        regData = (regData & 0xFFF0) | 0x0001;
        regWrite(etha, phyAddr, 1, 0x0834, regData);
        sleep(500);
        // Init code
        regWrite(etha, phyAddr, 3, 0xFE12, 0x000E);
        regWrite(etha, phyAddr, 3, 0xFE05, 0x05AA);
        regWrite(etha, phyAddr, 3, 0xFE04, 0x0016);
        regWrite(etha, phyAddr, 3, 0xFE07, 0x681F);
        regWrite(etha, phyAddr, 3, 0xFE5D, 0x045C);
        regWrite(etha, phyAddr, 3, 0xFE7C, 0x001E);
        regWrite(etha, phyAddr, 3, 0xFC00, 0x01C0);
        regWrite(etha, phyAddr, 7, 0x8032, 0x0020);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0012);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0A12);
        regWrite(etha, phyAddr, 7, 0x8032, 0x003C);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0001);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0A01);
        regWrite(etha, phyAddr, 3, 0xFC10, 0xD870);
        regWrite(etha, phyAddr, 3, 0xFC11, 0x1522);
        regWrite(etha, phyAddr, 3, 0xFC12, 0x07FA);
        regWrite(etha, phyAddr, 3, 0xFC13, 0x010B);
        regWrite(etha, phyAddr, 3, 0xFC15, 0x35A4);
        regWrite(etha, phyAddr, 3, 0xFC2D, 0x3C34);
        regWrite(etha, phyAddr, 3, 0xFC2E, 0x104B);
        regWrite(etha, phyAddr, 3, 0xFC2F, 0x1C15);
        regWrite(etha, phyAddr, 3, 0xFC30, 0x3C3C);
        regWrite(etha, phyAddr, 3, 0xFC31, 0x3C3C);
        regWrite(etha, phyAddr, 3, 0xFC3A, 0x2A2A);
        regWrite(etha, phyAddr, 3, 0xFC61, 0x2829);
        regWrite(etha, phyAddr, 3, 0xFC3B, 0x0E0E);
        regWrite(etha, phyAddr, 3, 0xFC62, 0x1C12);
        regWrite(etha, phyAddr, 3, 0xFC32, 0x03D2);
        regWrite(etha, phyAddr, 3, 0xFC46, 0x0200);
        regWrite(etha, phyAddr, 3, 0xFC86, 0x0401);
        regWrite(etha, phyAddr, 3, 0xFC4E, 0x1820);
        regWrite(etha, phyAddr, 3, 0xFC9C, 0x0101);
        regWrite(etha, phyAddr, 3, 0xFC95, 0x007A);
        regWrite(etha, phyAddr, 3, 0xFC3E, 0x221F);
        regWrite(etha, phyAddr, 3, 0xFC3F, 0x0A08);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x020E);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0077);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x0210);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0088);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x0215);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00AA);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x01D5);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00AA);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x0216);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00AB);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x01D6);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00AB);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x0213);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00A0);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x01D3);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00A0);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x0214);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00AB);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x01D4);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00AB);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x046B);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x00FA);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x046C);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x01F4);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x046E);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x01F4);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x0455);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0320);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x0416);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0323);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x0004);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0000);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03CC);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0055);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03CD);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0055);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03CE);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0022);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03CF);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0022);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03D0);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0022);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03D1);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0022);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03E4);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0055);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03E5);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0055);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03E6);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0022);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03E7);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0022);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03E8);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0022);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x03E9);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0022);
        regWrite(etha, phyAddr, 3, 0xFC03, 0x040C);
        regWrite(etha, phyAddr, 3, 0xFC04, 0x0033);
        regWrite(etha, phyAddr, 3, 0xFC5D, 0x06BF);
        regWrite(etha, phyAddr, 3, 0xFC89, 0x0003);
        regWrite(etha, phyAddr, 3, 0xFC5C, 0x007F);
        regWrite(etha, phyAddr, 3, 0xFC69, 0x383A);
        regWrite(etha, phyAddr, 3, 0xFC6A, 0x383A);
        regWrite(etha, phyAddr, 3, 0xFC6B, 0x0082);
        regWrite(etha, phyAddr, 3, 0xFC6F, 0x888F);
        regWrite(etha, phyAddr, 3, 0xFC70, 0x0D1A);
        regWrite(etha, phyAddr, 3, 0xFC71, 0x0505);
        regWrite(etha, phyAddr, 3, 0xFC72, 0x090C);
        regWrite(etha, phyAddr, 3, 0xFC73, 0x0C0F);
        regWrite(etha, phyAddr, 3, 0xFC74, 0x0400);
        regWrite(etha, phyAddr, 3, 0xFC75, 0x0103);
        regWrite(etha, phyAddr, 3, 0xFC7A, 0x081E);
        regWrite(etha, phyAddr, 3, 0xFC8C, 0xBC40);
        regWrite(etha, phyAddr, 3, 0xFC8D, 0x9830);
        regWrite(etha, phyAddr, 3, 0xFC91, 0x0000);
        regWrite(etha, phyAddr, 3, 0xFC63, 0x4440);
        regWrite(etha, phyAddr, 3, 0xFC64, 0x3C3F);
        regWrite(etha, phyAddr, 3, 0xFC65, 0x783C);
        regWrite(etha, phyAddr, 3, 0xFC66, 0x0002);
        regWrite(etha, phyAddr, 3, 0xFC7B, 0x7818);
        regWrite(etha, phyAddr, 3, 0xFC7C, 0xC440);
        regWrite(etha, phyAddr, 3, 0xFC7D, 0x5360);
        regWrite(etha, phyAddr, 3, 0xFC5F, 0x4034);
        regWrite(etha, phyAddr, 3, 0xFC60, 0x7858);
        regWrite(etha, phyAddr, 3, 0xFC7E, 0x003F);
        regWrite(etha, phyAddr, 3, 0xFC8E, 0x0003);
        regWrite(etha, phyAddr, 3, 0xFC57, 0x1820);
        regWrite(etha, phyAddr, 3, 0xFC00, 0x01C8);
        regWrite(etha, phyAddr, 3, 0xFC93, 0x141C);
        regWrite(etha, phyAddr, 3, 0xFC9B, 0x0091);
        regWrite(etha, phyAddr, 3, 0xFC94, 0x6D88);
        regWrite(etha, phyAddr, 3, 0xFE4A, 0x5653);
        regWrite(etha, phyAddr, 3, 0x0900, 0x8000);
        break;
    }
}

void _applyQ2112FeSetting(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t isAneg) {
    uint16_t regData = 0;

    sleep(1);
    if (0x0 != isAneg)
        regWrite(etha, phyAddr, 7, 0x0200, MRVL_88Q2112_AN_ENABLE | MRVL_88Q2112_AN_RESTART);
    else
        regWrite(etha, phyAddr, 7, 0x0200, MRVL_88Q2112_AN_DISABLE);

    if (MRVL_88Q2112_Z1 != getRevNum(etha, phyAddr)) {    // A2/A1/A0
        regWrite(etha, phyAddr, 3, 0xFA07, 0x0202);

        regData = regRead(etha, phyAddr, 1, 0x0834);
        regData = regData & 0xFFF0;
        regWrite(etha, phyAddr, 1, 0x0834, regData);
        sleep(5);

        regWrite(etha, phyAddr, 3, 0x8000, 0x0000);
        regWrite(etha, phyAddr, 3, 0x8100, 0x0200);
        regWrite(etha, phyAddr, 3, 0xFA1E, 0x0002);
        regWrite(etha, phyAddr, 3, 0xFE5C, 0x2402);
        regWrite(etha, phyAddr, 3, 0xFA12, 0x001F);
        regWrite(etha, phyAddr, 3, 0xFA0C, 0x9E05);
        regWrite(etha, phyAddr, 3, 0xFBDD, 0x6862);
        regWrite(etha, phyAddr, 3, 0xFBDE, 0x736E);
        regWrite(etha, phyAddr, 3, 0xFBDF, 0x7F79);
        regWrite(etha, phyAddr, 3, 0xFBE0, 0x8A85);
        regWrite(etha, phyAddr, 3, 0xFBE1, 0x9790);
        regWrite(etha, phyAddr, 3, 0xFBE3, 0xA39D);
        regWrite(etha, phyAddr, 3, 0xFBE4, 0xB0AA);
        regWrite(etha, phyAddr, 3, 0xFBE5, 0x00B8);
        regWrite(etha, phyAddr, 3, 0xFBFD, 0x0D0A);
        regWrite(etha, phyAddr, 3, 0xFBFE, 0x0906);
        regWrite(etha, phyAddr, 3, 0x801D, 0x8000);
        regWrite(etha, phyAddr, 3, 0x8016, 0x0011);
    }
    else {  // Z1 Case
        // port init.
        regWrite(etha, phyAddr, 3, 0x0000, 0x0000);
        regWrite(etha, phyAddr, 1, 0x0900, 0x0000);
        regWrite(etha, phyAddr, 3, 0x800D, 0x0000);
        // Link LED
        regWrite(etha, phyAddr, 3, 0x8000, 0x0000);
        regWrite(etha, phyAddr, 3, 0x8016, 0x0011);
        //set speed
        regData = regRead(etha, phyAddr, 1, 0x0834);
        regData = regData & 0xFFF0;
        regWrite(etha, phyAddr, 1, 0x0834, regData);
        sleep(500);
        // Init code
        regWrite(etha, phyAddr, 3, 0x8000, 0x0000);
        regWrite(etha, phyAddr, 3, 0xFE05, 0x3DAA);
        regWrite(etha, phyAddr, 3, 0xFE07, 0x6BFF);
        regWrite(etha, phyAddr, 3, 0xFB95, 0x52F0);
        regWrite(etha, phyAddr, 3, 0xFE5D, 0x171C);
        regWrite(etha, phyAddr, 3, 0x8016, 0x0011);
        regWrite(etha, phyAddr, 3, 0x0900, 0x8000);
    }
}

void _applyQ2112AnegSetting(struct rswitch_etha *etha, uint16_t phyAddr) {
    uint16_t regDataAuto = 0;
    if (MRVL_88Q2112_Z1 != getRevNum(etha, phyAddr)) {

        regDataAuto = regRead(etha, phyAddr, 7, 0x8032);
        regWrite(etha, phyAddr, 7, 0x8032, regDataAuto | 0x0001);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0013);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0a13);

        regDataAuto = regRead(etha, phyAddr, 7, 0x8032);
        regDataAuto = (0xFC00 & regDataAuto) | 0x0012;
        regWrite(etha, phyAddr, 7, 0x8032, regDataAuto);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0016);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0a16);

        regDataAuto = regRead(etha, phyAddr, 7, 0x8032);
        regDataAuto = (0xFC00 & regDataAuto) | 0x64;
        regWrite(etha, phyAddr, 7, 0x8032, regDataAuto);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0017);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0a17);

        regWrite(etha, phyAddr, 3, 0x800C, 0x0008);
        regWrite(etha, phyAddr, 3, 0xFE04, 0x0016);
    }
    else {  // Z1 Case
        regWrite(etha, phyAddr, 7, 0x8032, 0x0200);
        regWrite(etha, phyAddr, 7, 0x8031, 0x0a03);
    }
}

void _applyQ2112GeSoftReset(struct rswitch_etha *etha, uint16_t phyAddr) {
    uint16_t regDataAuto = 0;

    if (MRVL_88Q2112_Z1 != getRevNum(etha, phyAddr)) {    // A2/A1/A0
        if (getAnegEnabled(etha, phyAddr)) {
            regWrite(etha, phyAddr, 3, 0xFFF3, 0x0024);
        }
        //enable low-power mode
        regDataAuto = regRead(etha, phyAddr, 1, 0x0000);
        regWrite(etha, phyAddr, 1, 0x0000, regDataAuto | 0x0800);

        regWrite(etha, phyAddr, 3, 0xFFF3, 0x0020);
        regWrite(etha, phyAddr, 3, 0xFFE4, 0x000C);
        sleep(1);

        regWrite(etha, phyAddr, 3, 0xffe4, 0x06B6);

        // disable low-power mode
        regWrite(etha, phyAddr, 1, 0x0000, regDataAuto & 0xF7FF);
        sleep(1);

        regWrite(etha, phyAddr, 3, 0xFC47, 0x0030);
        regWrite(etha, phyAddr, 3, 0xFC47, 0x0031);
        regWrite(etha, phyAddr, 3, 0xFC47, 0x0030);
        regWrite(etha, phyAddr, 3, 0xFC47, 0x0000);
        regWrite(etha, phyAddr, 3, 0xFC47, 0x0001);
        regWrite(etha, phyAddr, 3, 0xFC47, 0x0000);

        regWrite(etha, phyAddr, 3, 0x0900, 0x8000);

        regWrite(etha, phyAddr, 1, 0x0900, 0x0000);
        regWrite(etha, phyAddr, 3, 0xFFE4, 0x000C);
    }
    else {  // Z1 Case
        regDataAuto = regRead(etha, phyAddr, 3, 0x0900);
        regWrite(etha, phyAddr, 3, 0x0900, regDataAuto | 0x8000);
        sleep(5);
    }
}

void _applyQ2112FeSoftReset(struct rswitch_etha *etha, uint16_t phyAddr) {
    uint16_t regData = 0;
    if (MRVL_88Q2112_Z1 != getRevNum(etha, phyAddr)) {    // A2/A1/A0
        regWrite(etha, phyAddr, 3, 0x0900, 0x8000);
        regWrite(etha, phyAddr, 3, 0xFA07, 0x0200);
    }
    else {    // Z1 Case
        regData = regRead(etha, phyAddr, 3, 0x0900);
        regWrite(etha, phyAddr, 3, 0x0900, regData | 0x8000);
        sleep(5);
    }
}

// ================================================
// End of code from internal shared configuration functions
// ================================================


// Obtain 88Q211x model number from a register. Useful to verify SMI connection.
// @param phyAddr bootstrap address of the PHY
// @return constant value MRVL_88Q2112_PRODUCT_ID read from a register
uint16_t getModelNum ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    uint16_t modelNum = regRead(etha, phyAddr, 1, 0x0003);
    return ( (modelNum & 0x03F0) >> 4 );
}

// Obtain 88Q211x revision number from a register.
// @param phyAddr bootstrap address of the PHY
// @return PHY revision MRVL_88Q2112_XX
uint16_t getRevNum ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    uint16_t revision = regRead(etha, phyAddr, 1, 0x0003);
    return revision & 0x000F;
}

// Software Reset procedure
// @param phyAddr bootstrap address of the PHY
// @return void
void softReset ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    if (MRVL_88Q2112_1000BASE_T1 == getSpeed(etha, phyAddr))     // 1000 BASE-T1
        _applyQ2112GeSoftReset(etha, phyAddr);
    else    // 100 BASE-T1
        _applyQ2112FeSoftReset(etha, phyAddr);
}

// Set Master/Slave mode of the PHY by software
// @param phyAddr bootstrap address of the PHY
// @param forceMaster   non-zero (Master)   otherwise (Slave)
// @return void
void setMasterSlave ( struct rswitch_etha *etha, uint16_t phyAddr, uint16_t forceMaster ) {
    uint16_t regData = regRead(etha, phyAddr, 1, 0x0834);
    if (0x0 != forceMaster)
        regData |= 0x4000;
    else
        regData &= 0xBFFF;
    regWrite(etha, phyAddr, 1, 0x0834, regData);
}

// Get current master/slave setting
// @param phyAddr bootstrap address of the PHY
// @return 0x1 if master, 0x0 if slave
uint16_t getMasterSlave ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    return ( (regRead(etha, phyAddr, 7, 0x8001) >> 14) & 0x0001 );
}

// Get link status including link, local rx, remote rx and descrambler lock(speed FE/100m) status.
// @param phyAddr bootstrap address of the PHY
// @return true if link is up, false otherwise
bool checkLink ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    uint16_t retData1, retData2, retData3;

	if (MRVL_88Q2112_1000BASE_T1 == getSpeed(etha, phyAddr)) {
        retData1 = regRead(etha, phyAddr, 3, 0x0901);
        retData1 = regRead(etha, phyAddr, 3, 0x0901);	// Read twice: link latches low status
        retData2 = regRead(etha, phyAddr, 7, 0x8001);	// local and remote receiver status
		retData3 = regRead(etha, phyAddr, 3, 0xFD9D);	// local receiver status 2
		return (0x0004 == (retData1 & 0x0004)) && (0x3000 == (retData2 & 0x3000)) && (0x0010 == (retData3 & 0x0010));
    }
    else {
        retData1 = regRead(etha, phyAddr, 3, 0x8109);	// link
        retData2 = regRead(etha, phyAddr, 3, 0x8108);	// local and remote receiver status
		retData3 = regRead(etha, phyAddr, 3, 0x8230);	// descrambler lock status
		return (0x0004 == (retData1 & 0x0004)) && (0x3000 == (retData2 & 0x3000)) && (0x1 == (retData3 & 0x1));
    }
}

// Get real time PMA link status
// @param phyAddr bootstrap address of the PHY
// @return true if link is up, false otherwise
bool getRealTimeLinkStatus ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    uint16_t retData1 = 0;
	retData1 = getSpeed(etha, phyAddr);
    if (MRVL_88Q2112_1000BASE_T1 == getSpeed(etha, phyAddr)) {
        retData1 = regRead(etha, phyAddr, 3, 0x0901);
        retData1 = regRead(etha, phyAddr, 3, 0x0901);    // Read twice: register latches low value
    }
    else {
        retData1 = regRead(etha, phyAddr, 3, 0x8109);
    }
    return (0x0 != (retData1 & 0x0004));
}

// Get latched link status – find out if link was down since last time checked
// @param phyAddr bootstrap address of the PHY
// @return true if link was up since last check, false otherwise
bool getLatchedLinkStatus ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    uint16_t retData = regRead(etha, phyAddr, 7, 0x0201);
    return ( 0x0 != (retData & 0x0004) );
}

// See if Auto-Negotiation is enabled
// @param phyAddr bootstrap address of the PHY
// @return true if enabled, false otherwise
bool getAnegEnabled ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    return ( 0x0 != (regRead(etha, phyAddr, 7, 0x0200) & MRVL_88Q2112_AN_ENABLE) );
}

// See if Auto-Negotiation completed successfully
// @param phyAddr bootstrap address of the PHY
// @return true if done, false otherwise
bool getAnegDone ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    return ( 0x0 != (regRead(etha, phyAddr, 7, 0x0201) & MRVL_88Q2112_AN_COMPLETE) );
}

// Get current data rate – negotiated or set directly
// @param phyAddr bootstrap address of the PHY
// @return 0x1 if 1000 BASE-T1, 0x0 if 100 BASE-T1
uint16_t getSpeed ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    if (getAnegEnabled(etha, phyAddr))
        return ( (regRead(etha, phyAddr, 7, 0x801a) & 0x4000) >> 14 );
    else
        return (regRead(etha, phyAddr, 1, 0x0834) & 0x0001);
}

// Initialize for 100 BASE-T1
// @param phyAddr bootstrap address of the PHY
// @return void
void initQ2112Fe ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    _applyQ2112FeSetting(etha, phyAddr, MRVL_88Q2112_AN_DISABLE);
    _applyQ2112FeSoftReset(etha, phyAddr);
}

// Initialize for 1000 BASE-T1
// @param phyAddr bootstrap address of the PHY
// @return true if PHY initialized successfully, false otherwise
bool initQ2112Ge ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    _applyQ2112GeSetting(etha, phyAddr, MRVL_88Q2112_AN_DISABLE);
    if (!applyOperateModeConfig(etha, phyAddr))
        return false;
    _applyQ2112GeSoftReset(etha, phyAddr);
    return true;
}

// Enable Auto-Negotiation
// @param phyAddr bootstrap address of the PHY
// @param forceMaster non-zero: force local PHY to be master
// @param flags supported rates MRVL_88Q2112_AN_XXXX   (see 3.1 Definitions)
// @return true if successfully initialized, false on error
//  Example: setAneg(phyAddr, 0, MRVL_88Q2112_AN_GE_ABILITY | MRVL_88Q2112_AN_FE_ABILITY)
bool setAneg ( struct rswitch_etha *etha, uint16_t phyAddr, uint16_t forceMaster, uint16_t flags ) {
    uint16_t regDataAuto, anAbility = 0;

    if (0x0 != forceMaster)
        anAbility = MRVL_88Q2112_AN_PREFER_MASTER;

    // Disable AN if no data rates supported
    if (0x0 == (flags & (MRVL_88Q2112_AN_GE_ABILITY | MRVL_88Q2112_AN_FE_ABILITY))) {
        regWrite(etha, phyAddr, 7, 0x0203, MRVL_88Q2112_AN_DISABLE);
        regWrite(etha, phyAddr, 7, 0x0202, 0x0001);
        regWrite(etha, phyAddr, 7, 0x0200, MRVL_88Q2112_AN_RESET);
        return true;
    }

    // Initialize for 1 or more data rates
    if (0x0 != (flags & MRVL_88Q2112_AN_GE_ABILITY)) {
        _applyQ2112GeSetting(etha, phyAddr, MRVL_88Q2112_AN_ENABLE);
        if (!applyOperateModeConfig(etha, phyAddr))
            return false;
        anAbility |= MRVL_88Q2112_AN_GE_ABILITY;
    }

    if (0x0 != (flags & MRVL_88Q2112_AN_FE_ABILITY)) {
        _applyQ2112FeSetting(etha, phyAddr, MRVL_88Q2112_AN_ENABLE);
        anAbility |= MRVL_88Q2112_AN_FE_ABILITY;
    }

    _applyQ2112AnegSetting(etha, phyAddr);

    // Set Ability registers
    regWrite(etha, phyAddr, 7, 0x0203, anAbility);

    // Force master if needed
    regDataAuto = regRead(etha, phyAddr, 7, 0x0202);
    if (0x0 != forceMaster)
        regDataAuto |= 0x1000;
    else
        regDataAuto &= 0xEFFF;
    regWrite(etha, phyAddr, 7, 0x0202, regDataAuto);

    // Reset routines to restart Auto-Negotiation
    if (0x0 != (flags & MRVL_88Q2112_AN_GE_ABILITY))
        _applyQ2112GeSoftReset(etha, phyAddr);
    if (0x0 != (flags & MRVL_88Q2112_AN_FE_ABILITY))
        _applyQ2112FeSoftReset(etha, phyAddr);
    return true;
}

// Save initial operating mode for the A2 PHY in memory
// @param phyAddr bootstrap address of the PHY
// @param selfMode operating mode MRVL_88Q2112_MODE_XXXX   (see 3.1 Definitions)
// @return true if successfully saved the mode, false on error
bool manualSetOperateModeSetting ( struct rswitch_etha *etha, uint16_t phyAddr, uint16_t selfMode ) {
    if (MRVL_88Q2112_A2 == getRevNum(etha, phyAddr))
        return saveOperateModeSetting(etha, phyAddr, selfMode & MRVL_88Q2112_MODE_MSK);
    else
        return saveOperateModeSetting(etha, phyAddr, MRVL_88Q2112_MODE_LEGACY);
}

// Fetch current operating mode from memory and apply in PHY
// @param phyAddr bootstrap address of the PHY
// @return true if successfully applied the mode, false on error
bool applyOperateModeConfig ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    uint16_t opMode = 0;
    bool result = false;
    switch (getRevNum(etha, phyAddr)) {

    case MRVL_88Q2112_A1:
        regWrite(etha, phyAddr, 3, 0xFDB8, 0x0001);    //Set A1 to legacy mode
        regWrite(etha, phyAddr, 1, 0x0902, MRVL_88Q2112_MODE_LEGACY | MRVL_88Q2112_MODE_ADVERTISE);
        result = true;
        break;

    case MRVL_88Q2112_A0:   // fall-through to MRVL_88Q2112_Z1 intentional
    case MRVL_88Q2112_Z1:
        regWrite(etha, phyAddr, 1, 0x0902, MRVL_88Q2112_MODE_LEGACY | MRVL_88Q2112_MODE_ADVERTISE);
        result = true;
        break;

    case MRVL_88Q2112_A2:
        opMode = loadOperateModeSetting(etha, phyAddr);
        if (MRVL_88Q2112_MODE_ERROR != opMode) {
            if (MRVL_88Q2112_MODE_LEGACY == opMode) {
                // Enable 1000 BASE-T1 legacy mode support
                regWrite(etha, phyAddr, 3, 0xFDB8, 0x0001);
                regWrite(etha, phyAddr, 3, 0xFD3D, 0x0C14);
            }
            else {
                // Set back to default compliant mode setting
                regWrite(etha, phyAddr, 3, 0xFDB8, 0x0000);
                regWrite(etha, phyAddr, 3, 0xFD3D, 0x0000);
            }
            regWrite(etha, phyAddr, 1, 0x0902, opMode | MRVL_88Q2112_MODE_ADVERTISE);
            result = true;
        }
        // no else statement
        break;

    default:    // error case - unexpected revision
        break;
    }

    return result;
}

// See if Link Partner signalled LEGACY mode during Training stage of 1000 BASE-T1.
// During PAM2 training stage, PHYs can exchange PHY Revision information.
// 88Q2112 revisions Z1/A0/A1 will signal MRVL_88Q2112_MODE_LEGACY if Init sequence is executed using API V1.10 or later version.
// This method is used with A2 only, as Z1/A0/A1 are always in LEGACY mode.
// @param phyAddr bootstrap address of the PHY
// @return true if Link Partner in LEGACY mode and speed is 1000 BASE-T1, false otherwise.
bool isLinkPartnerInLegacyMode ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    if (MRVL_88Q2112_100BASE_T1 == getSpeed(etha, phyAddr))
        return false;
    if (MRVL_88Q2112_A2 == getRevNum(etha, phyAddr))
        if (MRVL_88Q2112_MODE_LEGACY == (regRead(etha, phyAddr, 1, 0x0903) & MRVL_88Q2112_MODE_MSK))
            return true;
    // If not A2 and did not receive LEGACY register indicators from Link Partner, return false
    return false;
}

#if 0
// Signal Quality Indicator (SQI) with 8 levels (7 is best quality)
// @param phyAddr bootstrap address of the PHY
// @return 8-level quality indicator
uint16_t getSQIReading_8Level ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    uint16_t SQI = 0;
    uint16_t regVal = 0;

    //check link
    if (!getRealTimeLinkStatus(phyAddr)) {
        return 0;
    }

    // read different register for different rates
    if (MRVL_88Q2112_100BASE_T1 == getSpeed(phyAddr)) {  // 100 BASE-T1
        regVal = regRead(etha, phyAddr, 3, 0x8230);
        SQI = (regVal & 0xE000) >> 13;
    }
    else {  // 1000 BASE-T1
        if (MRVL_88Q2112_Z1 != getRevNum(phyAddr)) {    // A2/A1/A0
			regVal = regRead(etha, phyAddr, 3, 0xFC5D);
			regWrite(etha, phyAddr, 3, 0xFC5D, (regVal & 0xFF00) | 0x00AC);
			SQI = (regRead(etha, phyAddr, 3, 0xFC88) >> 1) & 0x07;
			if (!getRealTimeLinkStatus(phyAddr)) {
				SQI = 0;
			}

        }
        else {  // Z1 Case
            uint16_t trigVal = 0;
            regWrite(etha, phyAddr, 3, 0xFC5D, 0x06BF);

            //request SQI value
            trigVal = regRead(etha, phyAddr, 3, 0xFC89);
            regVal = trigVal | 0x1;
            regWrite(etha, phyAddr, 3, 0xFC89, regVal);
            sleep(5); //delay 5ms

            //read SQI value
            regWrite(etha, phyAddr, 3, 0xFC5D, 0x86BF);
            regVal = regRead(etha, phyAddr, 3, 0xFC88);
            SQI = (regVal & 0xE) >> 1;

            // release the request
            regWrite(etha, phyAddr, 3, 0xFC5D, 0x06BF);
            regWrite(etha, phyAddr, 3, 0xFC89, trigVal);
        }
    }
    return SQI;
}

// Signal Quality Indicator (SQI) with 16 levels (15 is best quality)
// @param phyAddr bootstrap address of the PHY
// @return quality indicator
uint16_t getSQIReading_16Level ( struct rswitch_etha *etha, uint16_t phyAddr ) {
    uint16_t SQI = 0;
    uint16_t regVal = 0;

    //check link
    if (!getRealTimeLinkStatus(phyAddr))
        return 0;

    // read different register for different rates
    if (MRVL_88Q2112_100BASE_T1 == getSpeed(phyAddr)) {  // 100 BASE-T1
        regVal = regRead(etha, phyAddr, 3, 0x8230);
        SQI = (regVal & 0xF000) >> 12;
    }
    else {  // 1000 BASE-T1
        if (MRVL_88Q2112_Z1 != getRevNum(phyAddr)) {    // A2/A1/A0
			// change sqi offset
			regVal = regRead(etha, phyAddr, 3, 0xFC5D);
			regWrite(etha, phyAddr, 3, 0xFC5D, (regVal & 0xFF00) | 0x00AC);

			// SQI register
			SQI = regRead(etha, phyAddr, 3, 0xFC88) & 0xF;

			if (!getRealTimeLinkStatus(phyAddr)) {
				SQI = 0;
			}
        }
        else {  // Z1 Case
            uint16_t trigVal = 0;
            regWrite(etha, phyAddr, 3, 0xFC5D, 0x06BF);

            //request SQI value
            trigVal = regRead(etha, phyAddr, 3, 0xFC89);
            regVal = trigVal | 0x1;
            regWrite(etha, phyAddr, 3, 0xFC89, regVal);
            sleep(5); //delay 5ms

            //read SQI value
            regWrite(etha, phyAddr, 3, 0xFC5D, 0x86BF);
            regVal = regRead(etha, phyAddr, 3, 0xFC88);
            SQI = regVal & 0xF;

            // release the request
            regWrite(etha, phyAddr, 3, 0xFC5D, 0x06BF);
            regWrite(etha, phyAddr, 3, 0xFC89, trigVal);
        }
    }
    return SQI;
}

// Cable Quality Indicator (CQI) providing Insertion Loss (IL) and Return Loss (RL)
// Valid for A2/A1/A0 only, not Z1
// @param phyAddr bootstrap address of the PHY
// @return true when measurement complete, false if link down or on error
bool getCQIReading ( struct rswitch_etha *etha, uint16_t phyAddr, uint16_t *IL, uint16_t *RL ) {
    uint16_t regVal = 0;
    uint16_t tmpData = 0;
    uint16_t rb_orig = 0;

    // Only for A2/A1/A0
    if (MRVL_88Q2112_Z1 == getRevNum(phyAddr))
        return false;

    //check link
    if (!getRealTimeLinkStatus(phyAddr))
        return false;

    // procedure differs between data rates
    if (MRVL_88Q2112_100BASE_T1 == getSpeed(phyAddr)) {  // 100 BASE-T1

        int16_t cqi_il_offset = 16;
        int16_t cqi_rl_offset = -29;

        rb_orig = regRead(etha, phyAddr, 3, 0xfb80);
        regWrite(etha, phyAddr, 3, 0xfb80, rb_orig & 0xfff7);

        regVal = regRead(etha, phyAddr, 3, 0xfbee) & 0xfff0 | 0x000a;
        regWrite(etha, phyAddr, 3, 0xfbee, regVal);
        regWrite(etha, phyAddr, 3, 0xfbe8, 0xd08e);
        regWrite(etha, phyAddr, 3, 0xfbe9, 0x3001);
        regWrite(etha, phyAddr, 3, 0xfbea, 0x774f);
        regWrite(etha, phyAddr, 3, 0xfbeb, 0x1040);

        cqi_il_offset = cqi_il_offset * 2;
        if (cqi_il_offset<0) {
            cqi_il_offset = cqi_il_offset + 256;
        }
        regWrite(etha, phyAddr, 3, 0xfbec, 64 + cqi_il_offset * 256);
        regWrite(etha, phyAddr, 3, 0xfbed, 0x783c);

        regVal = regRead(etha, phyAddr, 3, 0xfbef) & 0xFF00 | 0x0018;
        regWrite(etha, phyAddr, 3, 0xfbef, regVal);

        cqi_rl_offset = cqi_rl_offset * 2;
        if (cqi_rl_offset<0) {
            cqi_rl_offset = cqi_rl_offset + 256;
        }
        regWrite(etha, phyAddr, 3, 0xfbf0, 64 + cqi_rl_offset * 256);


        regVal = regRead(etha, phyAddr, 3, 0xfbee) | 0x0030;
        regWrite(etha, phyAddr, 3, 0xfbee, regVal);
        regWrite(etha, phyAddr, 3, 0xfbee, regVal & 0xffcf);
        regWrite(etha, phyAddr, 3, 0xfbee, regVal);

        regVal = regRead(etha, phyAddr, 3, 0xfb80);
        regWrite(etha, phyAddr, 3, 0xfb80, regVal | 0x0008);

        regVal = regRead(etha, phyAddr, 3, 0xfba1);
        regWrite(etha, phyAddr, 3, 0xfba1, regVal | 0x0001);
        regVal = regRead(etha, phyAddr, 3, 0xfb94);
        regWrite(etha, phyAddr, 3, 0xfb94, regVal | 0x0001);

        int cnt = 0;
        while (true) {
            regVal = regRead(etha, phyAddr, 3, 0xfbf4);
            if (0x0 == (regVal & 0x3000))
                break;
            sleep(10);
            cnt++;
            if (cnt > 100)
                break;
        }


        regVal = regRead(etha, phyAddr, 3, 0xfbf4);
        IL = (regVal & 0x00f0) >> 4;
        RL = (regVal & 0x0f00) >> 8;


        regVal = regRead(etha, phyAddr, 3, 0xfba1);
        regWrite(etha, phyAddr, 3, 0xfba1, regVal & 0xfffe);

        regVal = regRead(etha, phyAddr, 3, 0xfb94);
        regWrite(etha, phyAddr, 3, 0xfb94, regVal & 0xfffe);

        regVal = regRead(etha, phyAddr, 3, 0xfbee);
        regWrite(etha, phyAddr, 3, 0xfbee, regVal & 0xfffe);


        regWrite(etha, phyAddr, 3, 0xfb80, rb_orig);
    }
    else {  // 1000 BASE-T1

        int16_t cqi_il_offset = 31;
        int16_t cqi_rl_offset = -26;

        rb_orig = regRead(etha, phyAddr, 3, 0xfc00);
        regWrite(etha, phyAddr, 3, 0xfc00, rb_orig & 0xfff7);

        cqi_il_offset = cqi_il_offset * 2;
        if (cqi_il_offset<0) {
            cqi_il_offset = cqi_il_offset + 256;
        }
        regWrite(etha, phyAddr, 3, 0xfc64, 63 + cqi_il_offset * 256);

        cqi_rl_offset = cqi_rl_offset * 4;
        if (cqi_rl_offset<0) {
            cqi_rl_offset = cqi_rl_offset + 256;
        }
        regWrite(etha, phyAddr, 3, 0xfc7c, 64 + cqi_rl_offset * 256);


        regVal = regRead(etha, phyAddr, 3, 0xfc16);
        regWrite(etha, phyAddr, 3, 0xfc16, regVal | 0x0001);


        regVal = regRead(etha, phyAddr, 3, 0xfc89);
        regWrite(etha, phyAddr, 3, 0xfc89, regVal & 0xFFED);
        regWrite(etha, phyAddr, 3, 0xfc89, regVal | 0x0012);

        sleep(500);


        regVal = regRead(etha, phyAddr, 3, 0xfc00);
        regWrite(etha, phyAddr, 3, 0xfc00, regVal | 0x0008);


        regVal = regRead(etha, phyAddr, 3, 0xfc66);
        IL = (regVal & 0xf000) >> 12;
        regVal = regRead(etha, phyAddr, 3, 0xfc80);
        RL = regVal / 64;

        if (IL > 15) IL = 15;
        if (RL > 15) RL = 15;


        regVal = regRead(etha, phyAddr, 3, 0xfc16);
        regWrite(etha, phyAddr, 3, 0xfc16, regVal & 0xFFFE);


        regWrite(etha, phyAddr, 3, 0xfc00, rb_orig);
    }
    return true;
}

// Virtual Cable Test (VCT) intended to find faults in cables and improper termination
// @param phyAddr bootstrap address of the PHY
// @param &distanceToFault if test successful, distance to fault will be stored here
// @param &cable_status if test successful, cable status will be stored here
// @return true when measurement complete, false if test failed
//     In case method returns true, interpret results as follows:
//     cable_status     distanceToFault     result
//     't'              0                   proper termination
//     'o'              value               open circuit at length "value"
//     's'              value               short circuit at length "value"
//     No other results possible
bool getVCTReading ( struct rswitch_etha *etha, uint16_t phyAddr, double *distanceToFault, char *cable_status ) {
    uint16_t reg_fec9 = 0;
    uint16_t retData = 0;
    uint16_t cnt = 0;
    uint16_t vct_dis = 0;
    uint16_t vct_amp = 0;
    uint16_t vct_polarity = 0;

    // Ignore Wire Activity
    reg_fec9 = regRead(etha, phyAddr, 3, 0xfec9);
    regWrite(etha, phyAddr, 3, 0xfec9, reg_fec9 | 0x0080);

    // Set incoming ADC sign bit
    retData = regRead(etha, phyAddr, 3, 0xfec3);
    regWrite(etha, phyAddr, 3, 0xfec3, retData | 0x2000);

    retData = regRead(etha, phyAddr, 3, 0xfe5d);
    regWrite(etha, phyAddr, 3, 0xfe5d, retData & 0xEFFF);

    // Adjust thresholds
    regWrite(etha, phyAddr, 3, 0xfec4, 0x0f1f);
    regWrite(etha, phyAddr, 3, 0xfec7, 0x1115);

    // Enable TDR (self-clear register)
    retData = regRead(etha, phyAddr, 3, 0xfec3);
    regWrite(etha, phyAddr, 3, 0xfec3, retData | 0x4000);
    sleep(210); //delay 210ms

    // Check if test is done
    while (true) {
        //make sure previous VCT is done
        retData = regRead(etha, phyAddr, 3, 0xFEDB);
        if (0x0 == (retData & 0x0400))   //done = 0x0400, busy = 0
            sleep(20); //delay 20ms
        else
            break;  // test done
        cnt++;
        if (cnt > 20) { // Timeout error condition
            regWrite(etha, phyAddr, 3, 0xfec9, reg_fec9);
            return false;
        }
    }

    // Capture results
    retData = regRead(etha, phyAddr, 3, 0xfedb);
    vct_dis = retData & 0x03ff;
    retData = regRead(etha, phyAddr, 3, 0xfedc);
    vct_amp = retData & 0x007f;
    vct_polarity = retData & 0x0080;

    // process results
    if (0x0 == vct_dis) {
        distanceToFault = 0;
    }
    else {
        distanceToFault = (vct_dis - 39) * 0.12931;
    }
    if ((0x0 == vct_dis) && (0x0 != vct_polarity)) {
        cable_status = 't';
    }
    else if (0x0 != vct_polarity) {
        cable_status = 'o';
    }
    else {
        cable_status = 's';
    }

    // stop test
    regWrite(etha, phyAddr, 3, 0xfec9, reg_fec9);
    return true;
}
#endif

// Switch speed during linkup (ANEG is disabled)
// Change speed on the fly with linkup may cause SLAVE to stay with previous speed.
// This function ensure device go through a clean procedure and linkup with new speed.
// Apply this function in linkedup DUT and LP: both devices perform speed switch to the same speed.
// @param phyAddr bootstrap address of the PHY
// @param targetSpeed the target speed to switch to
bool setupForcedSpeedDuringLinkup(struct rswitch_etha *etha, uint16_t phyAddr, uint16_t targetSpeed) {
	if (getAnegEnabled(etha, phyAddr)) return false;

	//Set target speed
	regWrite(etha, phyAddr, 1, 0x0000, regRead(etha, phyAddr, 1, 0x0000) | 0x0800);
	sleep(10);
	if (MRVL_88Q2112_1000BASE_T1 == targetSpeed) {
		regWrite(etha, phyAddr, 1, 0x0834, (regRead(etha, phyAddr, 1, 0x0834) & 0xFFF0) | 0x0001);
		regWrite(etha, phyAddr, 3, 0xffe4, 0x07B6);
	}
	else {
		regWrite(etha, phyAddr, 1, 0x0834, regRead(etha, phyAddr, 1, 0x0834) & 0xFFF0);
	}
	regWrite(etha, phyAddr, 1, 0x0000, regRead(etha, phyAddr, 1, 0x0000) & 0xF7FF);
	sleep(1);

	//apply the init script according to target speed.
	if (MRVL_88Q2112_1000BASE_T1 == targetSpeed) {
		 return initQ2112Ge(etha, phyAddr);
	}
	else {
		initQ2112Fe(etha, phyAddr);
	}
	return true;
}

// Init GE PHY with single SEND_S
// Update to send single SEND_S after PHY initialized to GE speed with proper MASTER/SLAVE setting (PHY sends multiple SEND_S by default).
// @param phyAddr bootstrap address of the PHY
bool initQ2112Ge_SingleSendS(struct rswitch_etha *etha, uint16_t phyAddr)
{
	_applyQ2112GeSetting(etha, phyAddr, MRVL_88Q2112_AN_DISABLE);

	regWrite(etha, phyAddr, 7, 0x8032, 0x0);
	regWrite(etha, phyAddr, 7, 0x8031, 0xA1F);
	regWrite(etha, phyAddr, 7, 0x8031, 0xC1F);
	regWrite(etha, phyAddr, 7, 0x8032, 0x1);
	regWrite(etha, phyAddr, 7, 0x8031, 0xA1B);
	regWrite(etha, phyAddr, 7, 0x8031, 0xC1B);
	regWrite(etha, phyAddr, 7, 0x8032, 0xB);
	regWrite(etha, phyAddr, 7, 0x8031, 0xA1C);
	regWrite(etha, phyAddr, 7, 0x8031, 0xC1C);
	regWrite(etha, phyAddr, 3, 0x800C, 0x8);
	regWrite(etha, phyAddr, 7, 0x8035, 0x27B8);
	regWrite(etha, phyAddr, 7, 0x8036, 0x25);

	if (0 != getMasterSlave(etha, phyAddr)) {
		regWrite(etha, phyAddr, 7, 0x8032, 0x5A);
		regWrite(etha, phyAddr, 7, 0x8031, 0xA01);
		regWrite(etha, phyAddr, 7, 0x8031, 0xC01);
		regWrite(etha, phyAddr, 7, 0x8032, 0x1D);
		regWrite(etha, phyAddr, 7, 0x8031, 0xA22);
		regWrite(etha, phyAddr, 7, 0x8031, 0xC22);
		regWrite(etha, phyAddr, 3, 0xFE04, 0x8);
	}
	else {
		regWrite(etha, phyAddr, 7, 0x8032, 0x64);
        regWrite(etha, phyAddr, 7, 0x8031, 0xA01);
        regWrite(etha, phyAddr, 7, 0x8031, 0xC01);
        regWrite(etha, phyAddr, 7, 0x8032, 0x1A);
        regWrite(etha, phyAddr, 7, 0x8031, 0xA22);
        regWrite(etha, phyAddr, 7, 0x8031, 0xC22);
        regWrite(etha, phyAddr, 3, 0xFE04, 0x10);
	}

	if (!applyOperateModeConfig(etha, phyAddr))
		return false;
	_applyQ2112GeSoftReset(etha, phyAddr);
	return true;
}






// Sample pseudo-code to initialize PHY in 100/1000 BASE-T1 mode WITHOUT Auto-Neg.
// @param phyAddr bootstrap address of the PHY
// @param flags bitwise flags to set PHY parameters:
//        MRVL_88Q2112_INIT_MASTER
//        MRVL_88Q2112_INIT_FE
//        MRVL_88Q2112_INIT_GE
// @param addedTimeMarginMS system-dependent extra margin until link-up in milliseconds
// @return see MRVL_88Q2112_INIT_#### return codes
static uint16_t phyInit (struct rswitch_etha *etha, uint16_t phyAddr, uint16_t flags, uint16_t addedTimeMarginMS ) {
    int i;
    bool smi_ok = false;
	bool linkup_before_set_mode = false;

	printk("%s:%d\n", __FUNCTION__, __LINE__);
	// Sanity check on addedTimeMarginMS (overflow)
    if ( (addedTimeMarginMS + MRVL_88Q2112_LINKUP_TIMEOUT) < MRVL_88Q2112_LINKUP_TIMEOUT )
        return MRVL_88Q2112_INIT_ERROR_ARGS;

    // 1) Ensure functional communication to PHY SMI
    for (i=0;i<100;i++) {
        if (MRVL_88Q2112_PRODUCT_ID == getModelNum(etha, phyAddr)) {
            // SMI success - proceed to config
            smi_ok = true;
            break;
        }
		mdelay(1);
    }
    if (!smi_ok)    // ERROR: failed to establish SMI access
        return MRVL_88Q2112_INIT_ERROR_SMI;

    // 2) Init Sequence
    setMasterSlave(etha, phyAddr, MRVL_88Q2112_INIT_MASTER & flags);

	if (checkLink(etha, phyAddr)) {
		linkup_before_set_mode = true;
	}
	if (0x0 != (MRVL_88Q2112_INIT_GE & flags)) {
		// manualSetOperateModeSetting supports Z1/A0/A1/A2
		// 1000 BASE-T1: Detect PHY revision, save initial compatibility mode (Default/Legacy), and Initialize.
		if (!manualSetOperateModeSetting(etha, phyAddr, MRVL_88Q2112_MODE_DEFAULT))
			return MRVL_88Q2112_INIT_ERROR_SAVE_MODE;
		if (linkup_before_set_mode) {
			//If the link is up before init mode, appy setupForcedSpeedDuringLinkup
			if(!setupForcedSpeedDuringLinkup(etha, phyAddr, MRVL_88Q2112_1000BASE_T1))
				return MRVL_88Q2112_INIT_ERROR_INIT_GE;
		}
		else {
			//If the link is down, appy init script directly
			if (MRVL_88Q2112_SINGLE_SEND_S_ENABLE) {
				// option to enable single send_s
				if (!initQ2112Ge_SingleSendS(etha, phyAddr))
					return MRVL_88Q2112_INIT_ERROR_INIT_GE;
			}
			else {
				// option to use default send_s settings
				if (!initQ2112Ge(etha, phyAddr))
					return MRVL_88Q2112_INIT_ERROR_INIT_GE;
			}

			if (!initQ2112Ge(etha, phyAddr))
				return MRVL_88Q2112_INIT_ERROR_INIT_GE;
		}

	}
	else if (0x0 != (MRVL_88Q2112_INIT_FE & flags)) {
		// 100 BASE-T1 Initialization
		if (linkup_before_set_mode) {
			//If the link is up before init mode, appy setupForcedSpeedDuringLinkup
			setupForcedSpeedDuringLinkup(etha, phyAddr, MRVL_88Q2112_100BASE_T1);
		}
		else {
			//If the link is down, appy init script directly
			initQ2112Fe(etha, phyAddr);
		}

	}
	else {
		return MRVL_88Q2112_INIT_ERROR_ARGS;
	}

    // 3) Timeout until link-up and compatibility monitor
    for (i=0; i<MRVL_88Q2112_LINKUP_TIMEOUT + addedTimeMarginMS; i=i+10) {
		mdelay(10);
        if (checkLink(etha, phyAddr)) {
            // link up, exit with success
        	printk("%s:%d INIT_DONE\n", __FUNCTION__, __LINE__);
            return MRVL_88Q2112_INIT_DONE;
        }
        else {
            // 1000 BASE-T1: During training stage, PHYs can exchange PHY Revision information.
            // A2 only: if link partner signalled LEGACY mode, exit early and reconfigure to support
            // THIS WILL ONLY WORK IF LINK PARTNER 88Q2110/88Q2112 Z1/A0/A1 EXECUTED INIT SEQUENCE (using API V1.10 or later version)!
            if (isLinkPartnerInLegacyMode(etha, phyAddr))
                break;
        }
    }

    if (MRVL_88Q2112_1000BASE_T1 == getSpeed(etha, phyAddr)) {
        // 4) 1000 BASE-T1: If link is not established through normal process, here are some possibilities:
        //   a) Local Master/Slave setting is incorrect - rerun function with correct setting
        //   b) Link Partner failed to initialize - system-level problem handled in upper layers
        //   c) Link Partner is Legacy 88Q2112 Z1/A0/A1. Signalled LEGACY in step (3) - restart timer
        //   d) Link Partner is 88Q2112 Z1/A0/A1 revision and did not execute API V1.10 or later version.
        //      Switch mode and restart timer.
        if (!manualSetOperateModeSetting(etha, phyAddr, MRVL_88Q2112_MODE_LEGACY))
            return MRVL_88Q2112_INIT_ERROR_SAVE_MODE;
        if (!applyOperateModeConfig(etha, phyAddr))
            return MRVL_88Q2112_INIT_ERROR_SAVE_MODE;

        // There is no need to toggle reset or execute init sequence again.
        for (i=0; i<MRVL_88Q2112_LINKUP_TIMEOUT; i=i+10) {
			mdelay(10);
            if (checkLink(etha, phyAddr)) {
                // link up, exit with success
            	printk("%s:%d INIT_DONE\n", __FUNCTION__, __LINE__);
                return MRVL_88Q2112_INIT_DONE;
            }
            // no else statement
        }
    }

    // Init failed - no link within expected time.
    // 1000 BASE-T1: a loop could be implemented
    // to toggle back into MRVL_88Q2112_MODE_DEFAULT and try again.
    return MRVL_88Q2112_INIT_ERROR_FAILED;
}

