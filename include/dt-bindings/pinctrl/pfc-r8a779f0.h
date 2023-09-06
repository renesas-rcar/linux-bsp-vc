// SPDX-License-Identifier: GPL-2.0

#ifndef _DT_BINDINGS_PINCTRL_PFC_R8A779F0_H
#define _DT_BINDINGS_PINCTRL_PFC_R8A779F0_H

/*
 * Values for FCLACTL registers of the Noise Filter & Edge/Level Detector
 *
 * Note: using direct register values as values for pinconf parameters will
 *       only work properly if all values fit into 24 bits.
 */


#define IRQ_DETECTOR_MODE_BYPASS		0x80
#define IRQ_DETECTOR_MODE_NONE			0x00
#define IRQ_DETECTOR_MODE_EDGE_RISING		0x01
#define IRQ_DETECTOR_MODE_EDGE_FALLING		0x02
#define IRQ_DETECTOR_MODE_EDGE_BOTH		0x03
#define IRQ_DETECTOR_MODE_LEVEL_LOW		0x04
#define IRQ_DETECTOR_MODE_LEVEL_HIGH		0x05

#endif
