# A.Kamal (RSWITCH)
#
# Makefile for the Renesas R-Switch  Forwarding Engine device driver.
#

#
#  Build Options
#
#AK: Suppress GCC 4.9 warning for date & time 
# Include DEBUG functions 
EXTRA_CFLAGS += -D_RSWITCH_FWD_DEBUG -Wno-date-time


obj-$(CONFIG_RENESAS_RSWITCH_FWD) += rswitch_fwd.o
rswitch_fwd-objs := rswitch_fwd_main.o
# A.Kamal (RSWITCH)
