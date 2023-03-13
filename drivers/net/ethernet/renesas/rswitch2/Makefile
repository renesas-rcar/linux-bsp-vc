# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the RSwitch2 ethernet switch device driver.
#

#
#  Build Options
#

rswitch2-objs := rswitch2_main.o

ifneq ($(CONFIG_RENESAS_RSWITCH2_PCI), )
	rswitch2-objs += rswitch2_pci.o

ifneq ($(CONFIG_RENESAS_RSWITCH2_PCI_FW), )
	rswitch2-objs += rswitch2_pci_fw.o
endif
endif
ifneq ($(CONFIG_RENESAS_RSWITCH2_PLATF), )
	rswitch2-objs += rswitch2_platf.o
endif

ifneq ($(CONFIG_RENESAS_RSWITCH2_GWCA), )
	rswitch2-objs += rswitch2_eth.o
endif

rswitch2-objs += rswitch2_fwd.o
rswitch2-objs += ../rtsn_ptp.o

obj-$(CONFIG_RENESAS_RSWITCH2) += rswitch2.o

