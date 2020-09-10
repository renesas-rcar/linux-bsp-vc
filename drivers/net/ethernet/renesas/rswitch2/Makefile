# A.Kamal (RSWITCH2)
#
# Makefile for the R-SWITCH2 Ethernet device driver.
#

#
#  Build Options
#

# Include DEBUG functions
#AK: Suppress GCC 4.9 warning for date & time 
EXTRA_CFLAGS += -D_RSWITCH2_DEBUG -Wno-date-time

obj-$(CONFIG_RENESAS_RSWITCH2) += rswitch2.o 

rswitch2-objs := rswitch2_eth.o rswitch2_fwd.o rswitch2_ptp.o rswitch2_bridge.o
# A.Kamal (RSWITCH2)


