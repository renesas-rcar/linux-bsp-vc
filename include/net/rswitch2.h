/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LINUX_NET_RSWITCH2_H
#define __LINUX_NET_RSWITCH2_H

struct net_device;

extern bool rswitch2_is_physical_port(struct net_device *ndev, struct net_device *ref_ndev);

#endif
