// SPDX-License-Identifier: GPL-2.0

#ifndef RSWITCH_VPORT_XEN_H
#define RSWITCH_VPORT_XEN_H

#include <xen/interface/event_channel.h>
#include "rswitch_lib.h"

struct rswitch_vport_xen_shm {
	struct rswitch_vport_desc rvpd;

	evtchn_port_t be_to_fe_chn;
	evtchn_port_t fe_to_be_chn;

	/* Command protocol:
	 * - sender writes command code,
	 * - interrupt handler in receiver writes BE_COMMAND_RECEIVED
	 *   (needed for multiple interrupt handler calls not caused multiple
	 *   command execution)
	 * - after completing the command, receiver writes BE_COMMAND_DONE
	 */
	int fe_to_be_command;
#define BE_COMMAND_DONE		1
#define BE_COMMAND_IN_PROGRESS	2
#define BE_COMMAND_REGISTER	3
#define BE_COMMAND_UNREGISTER	4
#define BE_COMMAND_ATTACH_RINGS	5
#define BE_COMMAND_DETACH_RINGS	6
	int fe_to_be_result;
};

#endif
