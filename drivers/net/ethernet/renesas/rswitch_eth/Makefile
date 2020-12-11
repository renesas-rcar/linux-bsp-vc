# A.Kamal (RSWITCH)
#
# Makefile for the R-SWITCH Ethernet device driver.
#

#
#  Build Options
#

# Include DEBUG functions
#AK: Suppress GCC 4.9 warning for date & time 
EXTRA_CFLAGS += -D_RSWITCH_ETH_DEBUG -Wno-date-time

obj-$(CONFIG_RENESAS_RSWITCH_ETH) += rswitch_eth.o

rswitch_eth-objs := rswitch_eth_main.o  
# A.Kamal (RSWITCH)


