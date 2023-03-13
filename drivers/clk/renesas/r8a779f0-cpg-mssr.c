// SPDX-License-Identifier: GPL-2.0
/*
 * r8a779f0 Clock Pulse Generator / Module Standby and Software Reset
 *
 * Copyright (C) 2021 Renesas Electronics Corp.
 *
 * Based on r8a779a0-cpg-mssr.c
 */

#include <linux/bug.h>
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/soc/renesas/rcar-rst.h>

#include <dt-bindings/clock/r8a779f0-cpg-mssr.h>

#include "renesas-cpg-mssr.h"
#include "rcar-gen4-cpg.h"

enum r8a779f0_clk_types {
	CLK_TYPE_CD_FIRST = CLK_TYPE_GEN4_SOC_BASE,
	CLK_TYPE_CD_MOSC = CLK_TYPE_CD_FIRST,
	CLK_TYPE_CD_PLL,
	CLK_TYPE_CD_PLLO,
	CLK_TYPE_CD_SYS,
	CLK_TYPE_CD_UHSB,
	CLK_TYPE_CD_CANFD,
	CLK_TYPE_CD_MSPI,
};

enum clk_ids {
	/* Core Clock Outputs exported to DT */
	LAST_DT_CORE_CLK = R8A779F0_LAST_CORE_CLK,

	/* External Input Clocks */
	CLK_EXTAL,
	CLK_EXTALR,

	/* Internal Core Clocks */
	CLK_MAIN,
	CLK_PLL1,
	CLK_PLL2,
	CLK_PLL3,
	CLK_PLL5,
	CLK_PLL6,
	CLK_PLL1_DIV2,
	CLK_PLL2_DIV2,
	CLK_PLL3_DIV2,
	CLK_PLL5_DIV2,
	CLK_PLL5_DIV4,
	CLK_PLL6_DIV2,
	CLK_S0,
	CLK_SDSRC,
	CLK_RPCSRC,
	CLK_OCO,

	CD_CLK_MOSC,
	CD_CLK_HSIOSC,
	CD_CLK_LSIOSC,
	CD_CLK_HVIOSC,
	CD_CLK_PLL,
	CD_CLK_PLLO,
	CD_CLK_IOSC,
	CD_CLK_SYS,
	CD_CLK_UHSB,
	CD_CLK_HSB,
	CD_CLK_LSB,

	/* Module Clocks */
	MOD_CLK_BASE
};

static const struct cpg_core_clk r8a779f0_core_clks[] __initconst = {
	/* External Clock Inputs */
	DEF_INPUT("extal",      CLK_EXTAL),
	DEF_INPUT("extalr",     CLK_EXTALR),

	/* Internal Core Clocks */
	DEF_BASE(".main", CLK_MAIN,	CLK_TYPE_GEN4_MAIN, CLK_EXTAL),
	DEF_BASE(".pll1", CLK_PLL1,	CLK_TYPE_GEN4_PLL1, CLK_MAIN),
	DEF_BASE(".pll3", CLK_PLL3,	CLK_TYPE_GEN4_PLL3, CLK_MAIN),
	DEF_BASE(".pll2", CLK_PLL2,	CLK_TYPE_GEN4_PLL2, CLK_MAIN),
	DEF_BASE(".pll6", CLK_PLL6,	CLK_TYPE_GEN4_PLL6, CLK_MAIN),
	DEF_BASE(".pll5", CLK_PLL5,     CLK_TYPE_GEN4_PLL5, CLK_MAIN),

	DEF_FIXED(".pll1_div2",	CLK_PLL1_DIV2,	CLK_PLL1,	2, 1),
	DEF_FIXED(".pll2_div2",	CLK_PLL2_DIV2,	CLK_PLL2,	2, 1),
	DEF_FIXED(".pll3_div2",	CLK_PLL3_DIV2,	CLK_PLL3,	2, 1),
	DEF_FIXED(".pll5_div2",	CLK_PLL5_DIV2,	CLK_PLL5,	2, 1),
	DEF_FIXED(".pll5_div4",	CLK_PLL5_DIV4,	CLK_PLL5_DIV2,	2, 1),
	DEF_FIXED(".pll6_div2",	CLK_PLL6_DIV2,	CLK_PLL6,	2, 1),
	DEF_FIXED(".s0",	CLK_S0,		CLK_PLL1_DIV2,	2, 1),
	DEF_FIXED(".sdsrc",	CLK_SDSRC,	CLK_PLL5_DIV2,	2, 1),
	DEF_RATE(".oco",	CLK_OCO,	32768),

	DEF_BASE(".rpcsrc",	CLK_RPCSRC,		CLK_TYPE_GEN4_RPCSRC, CLK_PLL5),
	DEF_BASE(".rpc",	R8A779F0_CLK_RPC,	CLK_TYPE_GEN4_RPC, CLK_RPCSRC),
	DEF_BASE("rpcd2",	R8A779F0_CLK_RPCD2,	CLK_TYPE_GEN4_RPCD2, R8A779F0_CLK_RPC),

	/* Core Clock Outputs */
	DEF_GEN4_Z("z0",	R8A779F0_CLK_Z0,	CLK_TYPE_GEN4_Z,	CLK_PLL2,	2, 0),
	DEF_GEN4_Z("z1",	R8A779F0_CLK_Z1,	CLK_TYPE_GEN4_Z,	CLK_PLL2,	2, 0),
	DEF_FIXED("s0d2",	R8A779F0_CLK_S0D2,	CLK_S0,		2, 1),
	DEF_FIXED("s0d3",	R8A779F0_CLK_S0D3,	CLK_S0,		3, 1),
	DEF_FIXED("s0d4",	R8A779F0_CLK_S0D4,	CLK_S0,		4, 1),
	DEF_FIXED("cl16m",	R8A779F0_CLK_CL16M,	CLK_S0,		48, 1),
	DEF_FIXED("s0d2_mm",	R8A779F0_CLK_S0D2_MM,	CLK_S0,		2, 1),
	DEF_FIXED("s0d3_mm",	R8A779F0_CLK_S0D3_MM,	CLK_S0,		3, 1),
	DEF_FIXED("s0d4_mm",	R8A779F0_CLK_S0D4_MM,	CLK_S0,		4, 1),
	DEF_FIXED("cl16m_mm",	R8A779F0_CLK_CL16M_MM,	CLK_S0,		48, 1),
	DEF_FIXED("s0d2_rt",	R8A779F0_CLK_S0D2_RT,	CLK_S0,		2, 1),
	DEF_FIXED("s0d3_rt",	R8A779F0_CLK_S0D3_RT,	CLK_S0,		3, 1),
	DEF_FIXED("s0d4_rt",	R8A779F0_CLK_S0D4_RT,	CLK_S0,		4, 1),
	DEF_FIXED("s0d6_rt",	R8A779F0_CLK_S0D6_RT,	CLK_S0,		6, 1),
	DEF_FIXED("cl16m_rt",	R8A779F0_CLK_CL16M_RT,	CLK_S0,		48, 1),
	DEF_FIXED("s0d3_per",	R8A779F0_CLK_S0D3_PER,	CLK_S0,		3, 1),
	DEF_FIXED("s0d6_per",	R8A779F0_CLK_S0D6_PER,	CLK_S0,		6, 1),
	DEF_FIXED("s0d12_per",	R8A779F0_CLK_S0D12_PER,	CLK_S0,		12, 1),
	DEF_FIXED("s0d24_per",	R8A779F0_CLK_S0D24_PER,	CLK_S0,		24, 1),
	DEF_FIXED("cl16m_per",	R8A779F0_CLK_CL16M_PER,	CLK_S0,		48, 1),
	DEF_FIXED("s0d2_hsc",	R8A779F0_CLK_S0D2_HSC,	CLK_S0,		2, 1),
	DEF_FIXED("s0d3_hsc",	R8A779F0_CLK_S0D3_HSC,	CLK_S0,		3, 1),
	DEF_FIXED("s0d4_hsc",	R8A779F0_CLK_S0D4_HSC,	CLK_S0,		4, 1),
	DEF_FIXED("s0d6_hsc",	R8A779F0_CLK_S0D6_HSC,	CLK_S0,		6, 1),
	DEF_FIXED("s0d12_hsc",	R8A779F0_CLK_S0D12_HSC,	CLK_S0,		12, 1),
	DEF_FIXED("cl16m_hsc",	R8A779F0_CLK_CL16M_HSC,	CLK_S0,		48, 1),
	DEF_FIXED("s0d2_cc",	R8A779F0_CLK_S0D2_CC,	CLK_S0,		2, 1),
	DEF_FIXED("rsw",	R8A779F0_CLK_RSW2,	CLK_PLL5,	2, 1),
	DEF_FIXED("cbfusa",	R8A779F0_CLK_CBFUSA,	CLK_EXTAL,	2, 1),
	DEF_FIXED("cpex",	R8A779F0_CLK_CPEX,	CLK_EXTAL,	2, 1),
	DEF_FIXED("sasyncper", R8A779F0_CLK_SASYNCPER, CLK_PLL5_DIV4, 3, 1),
	DEF_FIXED("sasyncperd1", R8A779F0_CLK_SASYNCPERD1, R8A779F0_CLK_SASYNCPER, 1, 1),
	DEF_FIXED("sasyncperd2", R8A779F0_CLK_SASYNCPERD2, R8A779F0_CLK_SASYNCPER, 2, 1),
	DEF_FIXED("sasyncperd4", R8A779F0_CLK_SASYNCPERD4, R8A779F0_CLK_SASYNCPER, 4, 1),

	DEF_GEN4_SD("sd0",	R8A779F0_CLK_SD0,	CLK_SDSRC,	0x870),
	DEF_DIV6P1("mso",       R8A779F0_CLK_MSO,       CLK_PLL5_DIV4, 0x087C),

	DEF_GEN4_OSC("osc",	R8A779F0_CLK_OSC,	CLK_EXTAL,	8),
	DEF_GEN4_MDSEL("r",	R8A779F0_CLK_R, 29, CLK_EXTALR, 1, CLK_OCO, 1),

	/* Control domain internal clocks */

	/* rate of CLK_MOSC is fixed, depends on MD 14:13 */
	DEF_TYPE("cd_mosc",	CD_CLK_MOSC,	CLK_TYPE_CD_MOSC),

	/* rates of CLK_HSIOSC, CLK_LSIOSC, CLK_HVIOSC are fixed */
	DEF_RATE("cd_hsiosc",	CD_CLK_HSIOSC,	200 * 1000 * 1000),
	DEF_RATE("cd_lsiosc",	CD_CLK_LSIOSC,	240 * 1000),
	DEF_RATE("cd_hviosc",	CD_CLK_HVIOSC,	4 * 1000 * 1000),

	/* rate of CLK_PLL is fixed, depends om MD 40:39 */
	DEF_TYPE("cd_pll",	CD_CLK_PLL,	CLK_TYPE_CD_PLL),

	/* CLK_PLLO is divided from CLK_PLL */
	DEF_TYPE("cd_pllo",	CD_CLK_PLLO,	CLK_TYPE_CD_PLLO),

	/* CLK_IOSC sourced from CLK_HSIOSC unless that is stopped, which
	 * happens only in complex standby modes */
	DEF_FIXED("cd_iosc",	CD_CLK_IOSC,	CD_CLK_HSIOSC, 1, 1),

	/* CLK_SYS sourced from either CLK_PLLO or CLK_IOSC */
	DEF_TYPE("cd_sys",	CD_CLK_SYS,	CLK_TYPE_CD_SYS),

	/* CLK_UHSB / CLK_HSB / CLK_LSB defined per Table 13.6 */
	DEF_TYPE("cd_uhsb",	CD_CLK_UHSB,	CLK_TYPE_CD_UHSB),
	DEF_FIXED("cd_hsb",	CD_CLK_HSB,	CD_CLK_UHSB, 2, 1),
	DEF_FIXED("cd_lsb",	CD_CLK_LSB,	CD_CLK_UHSB, 4, 1),

	/* Control domain clock outputs */
	DEF_TYPE("cd_canfd",	R8A779F0_CLK_CD_CANFD,	CLK_TYPE_CD_CANFD),
	DEF_TYPE("cd_mspi",	R8A779F0_CLK_CD_MSPI,	CLK_TYPE_CD_MSPI),
};

static const struct mssr_mod_clk r8a779f0_mod_clks[] __initconst = {
	DEF_MOD("hscif0",	514,	R8A779F0_CLK_SASYNCPERD2),
	DEF_MOD("hscif1",	515,	R8A779F0_CLK_SASYNCPERD2),
	DEF_MOD("hscif2",	516,	R8A779F0_CLK_SASYNCPERD2),
	DEF_MOD("hscif3",	517,	R8A779F0_CLK_SASYNCPERD2),
	DEF_MOD("i2c0",		518,	R8A779F0_CLK_S0D6_PER),
	DEF_MOD("i2c1",		519,	R8A779F0_CLK_S0D6_PER),
	DEF_MOD("i2c2",		520,	R8A779F0_CLK_S0D6_PER),
	DEF_MOD("i2c3",		521,	R8A779F0_CLK_S0D6_PER),
	DEF_MOD("i2c4",		522,	R8A779F0_CLK_S0D6_PER),
	DEF_MOD("i2c5",		523,	R8A779F0_CLK_S0D6_PER),
	DEF_MOD("msi0",		618,	R8A779F0_CLK_MSO),
	DEF_MOD("msi1",		619,	R8A779F0_CLK_MSO),
	DEF_MOD("msi2",		620,	R8A779F0_CLK_MSO),
	DEF_MOD("msi3",		621,	R8A779F0_CLK_MSO),
	DEF_MOD("pcie0",	624,	R8A779F0_CLK_S0D2),
	DEF_MOD("pcie1",	625,	R8A779F0_CLK_S0D2),
	DEF_MOD("rpc",		629,	R8A779F0_CLK_RPCD2),
	DEF_MOD("rtdm0",	630,	R8A779F0_CLK_S0D2),
	DEF_MOD("rtdm1",	631,	R8A779F0_CLK_S0D2),
	DEF_MOD("rtdm2",	700,	R8A779F0_CLK_S0D2),
	DEF_MOD("rtdm3",	701,	R8A779F0_CLK_S0D2),

	DEF_MOD("scif0",	702,	R8A779F0_CLK_SASYNCPERD4),
	DEF_MOD("scif1",	703,	R8A779F0_CLK_SASYNCPERD4),
	DEF_MOD("scif3",	704,	R8A779F0_CLK_SASYNCPERD4),
	DEF_MOD("scif4",	705,	R8A779F0_CLK_SASYNCPERD4),
	DEF_MOD("sdhi0",	706,	R8A779F0_CLK_SD0),
	DEF_MOD("sydm1",	709,	R8A779F0_CLK_S0D3),
	DEF_MOD("sydm2",	710,	R8A779F0_CLK_S0D3),
	DEF_MOD("tmu0",		713,	R8A779F0_CLK_S0D6_RT),
	DEF_MOD("tmu1",		714,	R8A779F0_CLK_S0D6_RT),
	DEF_MOD("tmu2",		715,	R8A779F0_CLK_S0D6_RT),
	DEF_MOD("tmu3",		716,	R8A779F0_CLK_S0D6_RT),
	DEF_MOD("tmu4",		717,	R8A779F0_CLK_S0D6_RT),
	DEF_MOD("ships0",	719,	R8A779F0_CLK_S0D2),
	DEF_MOD("caip_wrapper",	720,	R8A779F0_CLK_S0D2),
	DEF_MOD("caiplite0",	721,	R8A779F0_CLK_S0D2),
	DEF_MOD("caiplite1",	722,	R8A779F0_CLK_S0D2),
	DEF_MOD("caiplite2",	723,	R8A779F0_CLK_S0D2),
	DEF_MOD("caiplite3",	724,	R8A779F0_CLK_S0D2),
	DEF_MOD("caiplite4",	725,	R8A779F0_CLK_S0D2),
	DEF_MOD("caiplite5",	726,	R8A779F0_CLK_S0D2),
	DEF_MOD("caiplite6",	727,	R8A779F0_CLK_S0D2),
	DEF_MOD("caiplite7",	728,	R8A779F0_CLK_S0D2),
	DEF_MOD("wcaiplite0",	903,	R8A779F0_CLK_S0D2),
	DEF_MOD("wcaiplite1",	904,	R8A779F0_CLK_S0D2),
	DEF_MOD("wcaiplite2",	905,	R8A779F0_CLK_S0D2),
	DEF_MOD("wcaiplite3",	906,	R8A779F0_CLK_S0D2),
	DEF_MOD("wdt",		907,	R8A779F0_CLK_R),
	DEF_MOD("cmt0",		910,	R8A779F0_CLK_R),
	DEF_MOD("cmt1",		911,	R8A779F0_CLK_R),
	DEF_MOD("cmt2",		912,	R8A779F0_CLK_R),
	DEF_MOD("cmt3",		913,	R8A779F0_CLK_R),
	DEF_MOD("pfc0",		915,	R8A779F0_CLK_CL16M),
	DEF_MOD("rsw2",		1505,	R8A779F0_CLK_RSW2),
	DEF_MOD("ethphy",	1506,	R8A779F0_CLK_S0D2),
	DEF_MOD("ships1",	1511,	R8A779F0_CLK_S0D2),
	DEF_MOD("ships2",	1512,	R8A779F0_CLK_S0D2),
	DEF_MOD("ufs0",		1514,	R8A779F0_CLK_S0D4),
	DEF_MOD("thermal",      919,    R8A779F0_CLK_CL16M),
};

static const u16 r8a779f0_cd_mod_control_regs[] = {
	0x1000,		/* 0 = MSR_RSCFD */
	0x1010,		/* 1 = MSR_FLXA */
	0x1030,		/* 2 = MSR_ETNB */
	0x1040,		/* 3 = MSR_RSENT */
	0x1050,		/* 4 = MSR_MSPI */
	0x1060,		/* 5 = MSR_RLIN3 */
	0x10f0,		/* 6 = MSR_RIIC */
	0x1130,		/* 7 = MSR_TAUD */
	0x1140,		/* 8 = MSR_TAUJ_ISO */
	0x1180,		/* 9 = MSR_OSTM */
};

static const struct mssr_mod_clk r8a779f0_cd_mod_clks[] __initconst = {
	DEF_CD_MOD("rscfd0",	0,	CD_CLK_HSB),
	DEF_CD_MOD("rscfd1",	1,	CD_CLK_HSB),
	DEF_CD_MOD("etnb0",	200,	CD_CLK_HSB),
	DEF_CD_MOD("mspi0",	400,	CD_CLK_HSB),
	DEF_CD_MOD("mspi1",	401,	CD_CLK_HSB),
	DEF_CD_MOD("mspi2",	402,	CD_CLK_HSB),
	DEF_CD_MOD("mspi3",	403,	CD_CLK_HSB),
	DEF_CD_MOD("mspi4",	404,	CD_CLK_HSB),
	DEF_CD_MOD("mspi5",	405,	CD_CLK_HSB),
	DEF_CD_MOD("riic0",	600,	CD_CLK_LSB),
	DEF_CD_MOD("ostm0",	900,	CD_CLK_HSB),
};

/*
 * CPG Clock Data
 */
/*
 *   MD	 EXTAL		PLL1	PLL2	PLL3	PLL5	PLL6	OSC
 * 14 13 (MHz)
 * ----------------------------------------------------------------
 * 0  0	 16.66 / 1	x200	x150	x200	x200	x134	/15
 * 0  1	 20    / 1	x160	x120	x160	x160	x106	/19
 * 1  0	 Prohibited setting
 * 1  1	 40    / 2	x160	x120	x160	x160	x106	/38
 */
#define CPG_PLL_CONFIG_INDEX(md)	((((md) & BIT(14)) >> 13) | \
					 (((md) & BIT(13)) >> 13))

static const struct rcar_gen4_cpg_pll_config cpg_pll_configs[4] = {
	/* EXTAL div	PLL1 mult/div	PLL2 mult/div	PLL3 mult/div	PLL4 mult/div	PLL5 mult/div	PLL6 mult/div	OSC prediv */
	{ 1,		200,	1,	150,	1,	200,	1,	0,	0,	200,	1,	134,	1,	16,	},
	{ 1,		160,	1,	120,	1,	160,	1,	0,	0,	160,	1,	106,	1,	19,	},
	{ 0,		0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	},
	{ 2,		160,	1,	120,	1,	160,	1,	0,	0,	160,	1,	106,	1,	32,	},
};

static int __init r8a779f0_cpg_mssr_init(struct device *dev)
{
	const struct rcar_gen4_cpg_pll_config *cpg_pll_config;
	u32 cpg_mode;
	int error;

	error = rcar_rst_read_mode_pins(&cpg_mode);
	if (error)
		return error;

	cpg_pll_config = &cpg_pll_configs[CPG_PLL_CONFIG_INDEX(cpg_mode)];
	if (!cpg_pll_config->extal_div) {
		dev_err(dev, "Prohibited setting (cpg_mode=0x%x)\n", cpg_mode);
		return -EINVAL;
	}

	return rcar_gen4_cpg_init(cpg_pll_config, CLK_EXTALR, cpg_mode);
}

#define CLKD_PLLC_OFFSET		0x120
#define PLLO_DIV_SHIFT			0
#define PLLO_DIV_WIDTH			3
#define PLLO_PARENT			"cd_pll"
static const struct clk_div_table pllo_div_table[] = {
	{ .val = 1, .div = 1 },
	{ .val = 2, .div = 2 },
	{}
};

#define CLKC_CPUS_OFFSET		0x108
#define SYS_PARENT_SHIFT		0
#define SYS_PARENT_WIDTH		1
static const char * const sys_parents[] = { "cd_pllo", "cd_iosc" };

#define CLKC_RCANS_OFFSET		0x158
#define RCAN_DIV_SHIFT			0
#define RCAN_DIV_WIDTH			2
#define RCAN_PARENT			"cd_mosc"
static const struct clk_div_table rcan_div_table[] = {
	{ .val = 1, .div = 1 },
	{ .val = 2, .div = 2 },
	{ .val = 3, .div = 4 },
	{}
};

#define CLKC_MSPIS_OFFSET		0x178
#define MSPI_PARENT_SHIFT		0
#define MSPI_PARENT_WIDTH		1
static const char * const mspi_parents[] = { "cd_mosc", "cd_hsb" };

static struct clk * __init r8a779f0_cpg_clk_register(struct device *dev,
	const struct cpg_core_clk *core, const struct cpg_mssr_info *info,
	struct clk **clks, void __iomem *base,
	struct raw_notifier_head *notifiers)
{
	u64 mode_pins;
	int md_bits;
	int error;
	unsigned long rate;
	int div;

	switch (core->type) {
	case CLK_TYPE_CD_MOSC:
		error = rcar_rst_read_mode_pins_64(&mode_pins);
		if (error)
			return ERR_PTR(error);
		md_bits = (mode_pins >> 13) & 3;
		switch (md_bits) {
		case 0:
			rate = 16 * 1000 * 1000;
			break;
		case 1:
			rate = 20 * 1000 * 1000;
			break;
		case 3:
			rate = 40 * 1000 * 1000;
			break;
		default:
			dev_err(dev, "Prohibited setting: md 14:13 is %d\n",
					md_bits);
			return ERR_PTR(-EINVAL);
		}
		return clk_register_fixed_rate(NULL, core->name, NULL, 0, rate);

	case CLK_TYPE_CD_PLL:
		error = rcar_rst_read_mode_pins_64(&mode_pins);
		if (error)
			return ERR_PTR(error);
		md_bits = (mode_pins >> 39) & 3;
		switch (md_bits) {
		case 3:
			rate = 800 * 1000 * 1000;
			break;
		case 2:
			rate = 640 * 1000 * 1000;
			break;
		default:
			rate = 480 * 1000 * 1000;
			break;
		}
		return clk_register_fixed_rate(NULL, core->name, NULL, 0, rate);

	case CLK_TYPE_CD_PLLO:
		return clk_register_divider_table(NULL, core->name,
				PLLO_PARENT, 0,
				cpg_mssr_cd_base(dev) + CLKD_PLLC_OFFSET,
				PLLO_DIV_SHIFT, PLLO_DIV_WIDTH,
				CLK_DIVIDER_READ_ONLY,
				pllo_div_table, NULL);

	case CLK_TYPE_CD_SYS:
		return clk_register_mux(NULL, core->name,
				sys_parents, ARRAY_SIZE(sys_parents), 0,
				cpg_mssr_cd_base(dev) + CLKC_CPUS_OFFSET,
				SYS_PARENT_SHIFT, SYS_PARENT_WIDTH,
				CLK_MUX_READ_ONLY, NULL);

	case CLK_TYPE_CD_UHSB:
		if (ioread32(cpg_mssr_cd_base(dev) + CLKC_CPUS_OFFSET) & 1) {
			/* CLK_SYS sourced from CLK_IOSC - divider depends
			 * on MD 40:39 */
			error = rcar_rst_read_mode_pins_64(&mode_pins);
			if (error)
				return ERR_PTR(error);
			md_bits = (mode_pins >> 39) & 3;
			switch (md_bits) {
			case 3:
				div = 5;	/* 200 -> 40 */
				break;
			case 2:
				div = 4;	/* 200 -> 50 */
				break;
			default:
				div = 3;	/* 200 -> 66.67 */
				break;
			}
		} else {
			/* CLK_SYS sourced from CLK_PLLO - divider is 5 */
			div = 5;
		}
		return clk_register_fixed_factor(NULL, core->name,
					"cd_sys", 0, 1, div);

	case CLK_TYPE_CD_CANFD:
		return clk_register_divider_table(NULL, core->name,
				RCAN_PARENT, 0,
				cpg_mssr_cd_base(dev) + CLKC_RCANS_OFFSET,
				RCAN_DIV_SHIFT, RCAN_DIV_WIDTH,
				CLK_DIVIDER_READ_ONLY,
				rcan_div_table, NULL);

	case CLK_TYPE_CD_MSPI:
		return clk_register_mux(NULL, core->name,
				mspi_parents, ARRAY_SIZE(mspi_parents), 0,
				cpg_mssr_cd_base(dev) + CLKC_MSPIS_OFFSET,
				MSPI_PARENT_SHIFT, MSPI_PARENT_WIDTH,
				CLK_MUX_READ_ONLY, NULL);

	default:
		return rcar_gen4_cpg_clk_register(dev, core, info, clks, base,
				notifiers);
	}
}

const struct cpg_mssr_info r8a779f0_cpg_mssr_info __initconst = {
	/* Core Clocks */
	.core_clks = r8a779f0_core_clks,
	.num_core_clks = ARRAY_SIZE(r8a779f0_core_clks),
	.last_dt_core_clk = LAST_DT_CORE_CLK,
	.num_total_core_clks = MOD_CLK_BASE,

	/* Module Clocks */
	.mod_clks = r8a779f0_mod_clks,
	.num_mod_clks = ARRAY_SIZE(r8a779f0_mod_clks),
	.num_hw_mod_clks = 24 * 32,

	/* Control domain module clocks */
	.cd_mod_control_regs = r8a779f0_cd_mod_control_regs,
	.num_cd_mod_control_regs = ARRAY_SIZE(r8a779f0_cd_mod_control_regs),
	.cd_mod_clks = r8a779f0_cd_mod_clks,
	.num_cd_mod_clks = ARRAY_SIZE(r8a779f0_cd_mod_clks),

	/* Callbacks */
	.init = r8a779f0_cpg_mssr_init,
	.cpg_clk_register = r8a779f0_cpg_clk_register,

	.reg_layout = CLK_REG_LAYOUT_RCAR_GEN4,
};
