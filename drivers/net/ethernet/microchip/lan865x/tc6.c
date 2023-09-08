/*------------------------------------------------------------------------------------------------*/
/* Low Level Driver for OpenAlliance TC6 10BASE-T1S MACPHY via SPI protocol                       */
/* Copyright 2021, Microchip Technology Inc. and its subsidiaries.                                */
/*                                                                                                */
/* Redistribution and use in source and binary forms, with or without                             */
/* modification, are permitted provided that the following conditions are met:                    */
/*                                                                                                */
/* 1. Redistributions of source code must retain the above copyright notice, this                 */
/*    list of conditions and the following disclaimer.                                            */
/*                                                                                                */
/* 2. Redistributions in binary form must reproduce the above copyright notice,                   */
/*    this list of conditions and the following disclaimer in the documentation                   */
/*    and/or other materials provided with the distribution.                                      */
/*                                                                                                */
/* 3. Neither the name of the copyright holder nor the names of its                               */
/*    contributors may be used to endorse or promote products derived from                        */
/*    this software without specific prior written permission.                                    */
/*                                                                                                */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"                    */
/* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE                      */
/* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE                 */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE                   */
/* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL                     */
/* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR                     */
/* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER                     */
/* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,                  */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE                  */
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                           */
/*------------------------------------------------------------------------------------------------*/

#include <linux/types.h>
#include <linux/string.h>
#include "tc6.h"

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                          USER ADJUSTABLE                             */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

#define TC6_MAGIC           (0x48423578ul)
#define AVG_MIN_LEN         (TC6_CHUNK_BUF_SIZE)

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                    INTERNAL DEFINES AND VARIABLES                    */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

#define MASK(width) (((uint32_t)1u << (width)) - 1u)

#define FLD(pos, width) ((uint32_t)(((width) << 8u) | (pos)))
#define FLD_POS(f) ((f) & 255u)
#define FLD_MASK(f) MASK(((f) >> (8u & 255u)))

#define GET_VAL(_field, _v32) (((uint32_t)(_v32) >> (FLD_POS(_field))) & (FLD_MASK(_field)))
#define MK_MASK(_field, _val) (((uint32_t)(_val) & (FLD_MASK(_field))) << (FLD_POS(_field)))

/*
 * TX Data Header: 32-bit SPI TX Data Chunk Command Header
 */


/* Data fields {{{ */

#define HDR_DNC   FLD(31u, 1u) /* Data, Not Control */
#define HDR_SEQ   FLD(30u, 1u) /* Data Chunk Sequence */
#define HDR_NORX  FLD(29u, 1u) /* No Receive */
#define HDR_DV    FLD(28u, 1u) /* Data Valid */
#define HDR_VS    FLD(26u, 2u) /* Vendor Specific */
#define HDR_SV    FLD(20u, 1u) /* Start of Frame Valid */
#define HDR_SWO   FLD(16u, 4u) /* Start of Frame Word Offset */
#define HDR_EV    FLD(14u, 1u) /* End of Frame Valid */
#define HDR_EBO   FLD(8u, 6u) /* End of Frame Byte Offset */
#define HDR_TSC   FLD(6u, 2u) /* Transmit Frame Timestamp Capture */
#define HDR_P     FLD(0u, 1u) /* Header Parity Bit */

/* }}} */

/* Control Transaction fields {{{ */

#define HDR_C_DNC    FLD(31u, 1u) /* Data, Not Control */
#define HDR_C_HDRB   FLD(30u, 1u) /* TX Header Bad */
#define HDR_C_WNR    FLD(29u, 1u) /* Write, Not Read */
#define HDR_C_AID    FLD(28u, 1u) /* Address Increment Disable */
#define HDR_C_MMS    FLD(24u, 4u) /* Memory Map Selector */
#define HDR_C_ADDR   FLD(8u, 16u) /* Address */
#define HDR_C_LEN    FLD(1u, 7u) /* Length */
#define HDR_C_P      FLD(0u, 1u) /* Parity Bit */

/* }}} */

/*
 * RX Data Footer: 36-bit SPI RX Data Chunk Command Footer
 */

#define FTR_DNC      FLD(31u, 1u) /* Data, Not Control */
#define FTR_HDRB     FLD(30u, 1u) /* TX Header Bad */
#define FTR_SYNC     FLD(29u, 1u) /* Configuration Synchronized */
#define FTR_DV       FLD(28u, 1u) /* Data Valid */
#define FTR_MFD      FLD(27u, 1u) /* MAC Frame Drop */
#define FTR_VS       FLD(22u, 2u) /* Vendor Specific */
#define FTR_EXST     FLD(21u, 1u) /* Extended Status */
#define FTR_SV       FLD(20u, 1u) /* Start of Frame Valid */
#define FTR_SWO      FLD(16u, 4u) /* Start of Frame Word Offset */
#define FTR_RDA      FLD(15u, 1u) /* Receive Data Available */
#define FTR_EV       FLD(14u, 1u) /* End of Frame Valid */
#define FTR_EBO      FLD(8u, 6u) /* End of Frame Byte Offset */
#define FTR_RTSA     FLD(7u, 1u) /* Receive Frame Timestamp Added */
#define FTR_RTSP     FLD(6u, 1u) /* Receive Frame Timestamp Parity */
#define FTR_TXC      FLD(1u, 5u) /* Transmit Credits */
#define FTR_P        FLD(0u, 1u) /* Footer Parity Bit */

#define FTR_DATA_MASK  \
    (MK_MASK(FTR_SV, 1u) | MK_MASK(FTR_DV, 1u) | MK_MASK(FTR_EV, 1u))

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                      PRIVATE FUNCTION PROTOTYPES                     */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

static void initializeSpiEntry(struct qspibuf *newEntry);
static int16_t getTrail(TC6_t *g, bool enqueueEmpty);
static void addEmptyChunks(TC6_t *g, struct qspibuf *entry, bool enqueueEmpty);
static void serviceData(TC6_t *g, bool enqueueEmpty);
static void serviceControl(TC6_t *g);
static bool spiTransaction(TC6_t *g, const uint8_t *pTx, uint8_t *pRx, uint16_t len, SpiOp_t op);
static void modifyCallback(TC6_t *g, struct register_operation *reg_op, bool success, uint32_t addr, uint32_t value);
static bool accessRegisters(TC6_t *g, enum register_op_type op, uint32_t addr, uint32_t value,
                            bool secure, uint32_t modifyMask, TC6_RegCallback_t callback, void *tag);
static void avgInitialize(TC6_t *g);
static void avgStoreNewVal(TC6_t *g, uint16_t len);
static uint16_t avgGetAverageVal(TC6_t *g);
static void processDataRx(TC6_t *g);

/* Protocol Implementation */
static uint16_t mk_ctrl_req(bool wnr, bool aid, uint32_t addr, uint8_t num_regs, const uint32_t *regs, uint8_t *buff, uint16_t size_of_buff);
static uint16_t mk_secure_ctrl_req(bool wnr, bool aid, uint32_t addr, uint8_t num_regs, const uint32_t *regs, uint8_t *buff, uint16_t size_of_buff);
static uint16_t read_rx_ctrl_buffer(const uint8_t *rx_buf, uint16_t rx_buf_size, uint32_t *regs_buf, uint8_t regs_buf_size, bool secure);
static uint16_t mk_data_tx(TC6_t *g, uint8_t *tx_buf, uint16_t tx_buf_len);
static void enqueue_rx_spi(TC6_t *g, const uint8_t *buff, uint16_t buf_len);
static void update_credit_cnt(TC6_t *g, const uint8_t *buff, uint16_t buf_len);

static inline void init_qtxeth_queue(struct qtxeth_queue *q, struct qtxeth *buffer, unsigned size)
    { q->buffer_ = buffer; q->size_ = size; q->enqueue_ = 0u; q->convert_ = 0u; }

/* enqueue */
static inline bool qtxeth_enqueue_ready(struct qtxeth_queue const *q) {
    return 0u != ((q->enqueue_ - q->convert_) < q->size_); }
static inline struct qtxeth *qtxeth_enqueue_ptr(struct qtxeth_queue const *q) {
    return &q->buffer_[(q->enqueue_ & (q->size_ - 1u))]; }
static inline void qtxeth_enqueue_done(struct qtxeth_queue *q) {
    ++q->enqueue_; }
static inline void qtxeth_enqueue_undo(struct qtxeth_queue *q) {
    --q->enqueue_; }
static inline unsigned qtxeth_enqueue_cap(struct qtxeth_queue const *q) {
    return q->convert_ + q->size_ - q->enqueue_; }

/* convert */
static inline bool qtxeth_convert_ready(struct qtxeth_queue const *q) {
    return (q->enqueue_ - q->convert_ - 1u) < q->size_; }
static inline struct qtxeth *qtxeth_convert_ptr(struct qtxeth_queue const *q) {
    return &q->buffer_[(q->convert_ & (q->size_ - 1u))]; }
static inline void qtxeth_convert_done(struct qtxeth_queue *q) {
    ++q->convert_; }
static inline void qtxeth_convert_undo(struct qtxeth_queue *q) {
    --q->convert_; }
static inline unsigned qtxeth_convert_cap(struct qtxeth_queue const *q) {
    return q->enqueue_ - q->convert_; }

/*
 * namespace: qspibuf
 * type: "struct qspibuf"
 * stages: stage1_transfer stage2_int stage3_process
 */

static inline void init_qspibuf_queue(struct qspibuf_queue *q, struct qspibuf *buffer, unsigned size)
    { q->buffer_ = buffer; q->size_ = size; q->stage1_transfer_ = 0u; q->stage2_int_ = 0u; q->stage3_process_ = 0u; }

/* stage1_transfer */
static inline bool qspibuf_stage1_transfer_ready(struct qspibuf_queue const *q) {
    return 0u != ((q->stage1_transfer_ - q->stage3_process_) < q->size_); }
static inline struct qspibuf *qspibuf_stage1_transfer_ptr(struct qspibuf_queue const *q) {
    return &q->buffer_[(q->stage1_transfer_ & (q->size_ - 1u))]; }
static inline void qspibuf_stage1_transfer_done(struct qspibuf_queue *q) {
    ++q->stage1_transfer_; }
static inline void qspibuf_stage1_transfer_undo(struct qspibuf_queue *q) {
    --q->stage1_transfer_; }
static inline unsigned qspibuf_stage1_transfer_cap(struct qspibuf_queue const *q) {
    return q->stage3_process_ + q->size_ - q->stage1_transfer_; }

/* stage2_int */
static inline bool qspibuf_stage2_int_ready(struct qspibuf_queue const *q) {
    return (q->stage1_transfer_ - q->stage2_int_ - 1u) < q->size_; }
static inline struct qspibuf *qspibuf_stage2_int_ptr(struct qspibuf_queue const *q) {
    return &q->buffer_[(q->stage2_int_ & (q->size_ - 1u))]; }
static inline void qspibuf_stage2_int_done(struct qspibuf_queue *q) {
    ++q->stage2_int_; }
static inline void qspibuf_stage2_int_undo(struct qspibuf_queue *q) {
    --q->stage2_int_; }
static inline unsigned qspibuf_stage2_int_cap(struct qspibuf_queue const *q) {
    return q->stage1_transfer_ - q->stage2_int_; }

/* stage3_process */
static inline bool qspibuf_stage3_process_ready(struct qspibuf_queue const *q) {
    return (q->stage2_int_ - q->stage3_process_ - 1u) < q->size_; }
static inline struct qspibuf *qspibuf_stage3_process_ptr(struct qspibuf_queue const *q) {
    return &q->buffer_[(q->stage3_process_ & (q->size_ - 1u))]; }
static inline void qspibuf_stage3_process_done(struct qspibuf_queue *q) {
    ++q->stage3_process_; }
static inline void qspibuf_stage3_process_undo(struct qspibuf_queue *q) {
    --q->stage3_process_; }
static inline unsigned qspibuf_stage3_process_cap(struct qspibuf_queue const *q) {
    return q->stage2_int_ - q->stage3_process_; }

/*
 * namespace: regop
 * type: "struct register_operation"
 * stages: enqueue send int event
 */

static inline void init_regop_queue(struct regop_queue *q, struct register_operation *buffer, unsigned size)
    { q->buffer_ = buffer; q->size_ = size; q->enqueue_ = 0u; q->send_ = 0u; q->int_ = 0u; q->event_ = 0u; }

/* enqueue */
static inline bool regop_enqueue_ready(struct regop_queue const *q) {
    return 0u != ((q->enqueue_ - q->event_) < q->size_); }
static inline struct register_operation *regop_enqueue_ptr(struct regop_queue const *q) {
    return &q->buffer_[(q->enqueue_ & (q->size_ - 1u))]; }
static inline void regop_enqueue_done(struct regop_queue *q) {
    ++q->enqueue_; }
static inline void regop_enqueue_undo(struct regop_queue *q) {
    --q->enqueue_; }
static inline unsigned regop_enqueue_cap(struct regop_queue const *q) {
    return q->event_ + q->size_ - q->enqueue_; }

/* send */
static inline bool regop_send_ready(struct regop_queue const *q) {
    return (q->enqueue_ - q->send_ - 1u) < q->size_; }
static inline struct register_operation *regop_send_ptr(struct regop_queue const *q) {
    return &q->buffer_[(q->send_ & (q->size_ - 1u))]; }
static inline void regop_send_done(struct regop_queue *q) {
    ++q->send_; }
static inline void regop_send_undo(struct regop_queue *q) {
    --q->send_; }
static inline unsigned regop_send_cap(struct regop_queue const *q) {
    return q->enqueue_ - q->send_; }

/* int */
static inline bool regop_int_ready(struct regop_queue const *q) {
    return (q->send_ - q->int_ - 1u) < q->size_; }
static inline struct register_operation *regop_int_ptr(struct regop_queue const *q) {
    return &q->buffer_[(q->int_ & (q->size_ - 1u))]; }
static inline void regop_int_done(struct regop_queue *q) {
    ++q->int_; }
static inline void regop_int_undo(struct regop_queue *q) {
    --q->int_; }
static inline unsigned regop_int_cap(struct regop_queue const *q) {
    return q->send_ - q->int_; }

/* event */
static inline bool regop_event_ready(struct regop_queue const *q) {
    return (q->int_ - q->event_ - 1u) < q->size_; }
static inline struct register_operation *regop_event_ptr(struct regop_queue const *q) {
    return &q->buffer_[(q->event_ & (q->size_ - 1u))]; }
static inline void regop_event_done(struct regop_queue *q) {
    ++q->event_; }
static inline void regop_event_undo(struct regop_queue *q) {
    --q->event_; }
static inline unsigned regop_event_cap(struct regop_queue const *q) {
    return q->int_ - q->event_; }


/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                         PUBLIC FUNCTIONS                             */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

void TC6_Init(TC6_t *g)
{
	(void)memset(g, 0, sizeof(TC6_t));
	avgInitialize(g);
	g->prot.txc = 24;
	init_qtxeth_queue(&g->prot.eth_q, g->prot.tx_eth_buffer, TC6_TX_ETH_QSIZE);
	init_qspibuf_queue(&g->qSpi, g->spiBuf, SPI_FULL_BUFFERS);
	init_regop_queue(&g->regop_q, g->regop_storage, REG_OP_ARRAY_SIZE);
}

void TC6_Reset(TC6_t *g)
{
    struct qtxeth_queue *qEth = &g->prot.eth_q;
    struct regop_queue *qReg = &g->regop_q;

    /* Wait for pending SPI transactions */
    while(SPI_OP_INVALID != g->currentOp) {};

    /* Callback Ethernet Data Event listeners */
    while (qtxeth_convert_ready(qEth)) {
        struct qtxeth *entry = qtxeth_convert_ptr(qEth);
        if (NULL != entry->txCallback) {
            entry->txCallback(g, entry->ptr, entry->len, entry->priv, g->gTag);
        }
        qtxeth_convert_done(qEth);
    }
    while(regop_send_ready(qReg)) {
        regop_send_done(qReg);
    }
    while(regop_int_ready(qReg)) {
        regop_int_done(qReg);
    }
    while(regop_event_ready(qReg)) {
        struct register_operation *entry = regop_event_ptr(qReg);
        if (NULL != entry->callback) {
           entry->callback(g, false, entry->regAddr, entry->modifyValue, entry->tag, g->gTag);
        }
        regop_event_done(qReg);
    }
    if (g->prot.eth_started) {
        TC6_CB_OnRxEthernetPacket(g, false, 0, g->gTag);
    }

    /* Clean all states and queues */
    (void)memset(&g->prot, 0, sizeof(g->prot));
    init_qtxeth_queue(&g->prot.eth_q, g->prot.tx_eth_buffer, TC6_TX_ETH_QSIZE);
    init_qspibuf_queue(&g->qSpi, g->spiBuf, SPI_FULL_BUFFERS);
    init_regop_queue(&g->regop_q, g->regop_storage, REG_OP_ARRAY_SIZE);

    /* Set protocol defaults */
    g->prot.txc = 24;
}

void TC6_Service(TC6_t *g, bool interruptLevel)
{
    if (!g->intContext) {
        serviceControl(g);
        if (g->enableData) {
            processDataRx(g);
            serviceData(g, !interruptLevel || g->prot.eth_error);
            processDataRx(g);
        }
    }
}

void TC6_EnableData(TC6_t *g, bool enable)
{
    g->enableData = enable;
    TC6_CB_OnNeedService(g, g->gTag);
}

uint8_t TC6_GetInstance(TC6_t *g)
{
    return g ? g->instance : 0xFFu;
}

void TC6_GetState(TC6_t *g, uint8_t *pTxCredit, uint8_t *pRxCredit, bool *pSynced)
{
   if (NULL != pTxCredit) {
      *pTxCredit = g->prot.txc;
   }
   if (NULL != pRxCredit) {
      *pRxCredit = (uint8_t)g->prot.rda;
   }
   if (NULL != pSynced) {
      *pSynced = g->prot.synced;
   }
}

bool TC6_SendRawEthernetPacket(TC6_t *g, const uint8_t *pTx, uint16_t len, TC6_RawTxCallback_t txCallback, void *pTag)
{
    bool success = true;

    if (!g || !pTx || !len) {
        success = false;
    }
    if (success && g->enableData) {
        struct qtxeth_queue *q = &g->prot.eth_q;
        struct qtxeth *entry;
        success = false;

        if (qtxeth_enqueue_ready(q)) {
            entry = qtxeth_enqueue_ptr(q);
            entry->ptr = pTx;
            entry->len = len;
            entry->txCallback = txCallback;
            entry->priv = pTag;
            qtxeth_enqueue_done(q);

	    TC6_CB_OnNeedService(g, g->gTag);
            success = true;
        }
    }
    return success;
}

bool TC6_ReadRegister(TC6_t *g, uint32_t addr, bool secure, TC6_RegCallback_t rxCallback, void *tag)
{
    return accessRegisters(g, REGISTER_OP_READ, addr
        , 0    /* value */
        , secure
        , 0    /* mask */
        , rxCallback
        , tag);
}

bool TC6_WriteRegister(TC6_t *g, uint32_t addr, uint32_t value, bool secure, TC6_RegCallback_t txCallback, void *tag)
{
    return accessRegisters(g, REGISTER_OP_WRITE, addr
        , value
        , secure
        , 0    /* mask */
        , txCallback
        , tag);
}

bool TC6_ReadModifyWriteRegister(TC6_t *g, uint32_t addr, uint32_t value, uint32_t mask, bool secure, TC6_RegCallback_t modifyCallback, void *tag)
{
    return accessRegisters(g, REGISTER_OP_READWRITE_STAGE1, addr
        , value
        , secure
        , mask
        , modifyCallback
        , tag);
}

uint16_t TC6_MultipleRegisterAccess(TC6_t *g, const MemoryMap_t *pMap, uint16_t mapLength, TC6_RegCallback_t multipleCallback, void *pTag)
{
	uint16_t i = 0;
	uint16_t t = 0;
	bool full = false;
	for (; (i < mapLength) && !full; i++) {
		switch(pMap[i].op) {
			case MemOp_Write:
				if (accessRegisters(g, REGISTER_OP_WRITE, pMap[i].address
							, pMap[i].value
							, pMap[i].secure
							, 0                 /* mask */
							, multipleCallback
							, pTag))
				{
					t++;
				} else {
					full = true;
				}
				break;
			case MemOp_ReadModifyWrite:
				if (accessRegisters(g, REGISTER_OP_READWRITE_STAGE1, pMap[i].address
							, pMap[i].value
							, pMap[i].secure
							, pMap[i].mask
							, multipleCallback
							, pTag))
				{
					t++;
				} else {
					full = true;
				}
				break;
			case MemOp_Read:
				if (accessRegisters(g, REGISTER_OP_READ, pMap[i].address
							, 0                 /* value */
							, pMap[i].secure
							, 0                 /* mask */
							, multipleCallback
							, pTag))
				{
					t++;
				} else {
					full = true;
				}
				break;
			default:
				i = 0;
				full = true;
				break;
		}
	}
	return t;
}

void TC6_UnlockExtendedStatus(TC6_t *g)
{
    g->prot.exst_locked = false;
}

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                  PRIVATE FUNCTION IMPLEMENTATIONS                    */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

static void initializeSpiEntry(struct qspibuf *newEntry)
{
    newEntry->length = 0;
#ifdef DEBUG
    (void)memset(newEntry->txBuff, 0xCC, sizeof(newEntry->txBuff));
    (void)memset(newEntry->rxBuff, 0xCD, sizeof(newEntry->rxBuff));
#endif
}

static int16_t getTrail(TC6_t *g, bool enqueueEmpty)
{
    int16_t  trail = 0;
    if (0u != g->remainingTrail) {
        trail = g->remainingTrail;
    } else if (enqueueEmpty) {
        trail = avgGetAverageVal(g);
    } else {} /* MISRA enforced termination */
    return trail;
}

static void addEmptyChunks(TC6_t *g, struct qspibuf *entry, bool enqueueEmpty)
{
    int16_t trail = getTrail(g, enqueueEmpty);
    if (trail > 0) {
        int16_t i = 0;
        /* Fill up buffer with empty chunks, so RX gets the opportunity to transmit quicker */
        for (; (i < trail) && (entry->length < sizeof(entry->txBuff)); i += TC6_CHUNK_SIZE) {
            uint8_t *p = &entry->txBuff[entry->length];
            p[0] = 0x80;
            p[1] = 0x0;
            p[2] = 0x0;
            p[3] = 0x0;
            (void)memset(&p[4], 0x00, TC6_CHUNK_SIZE);
            entry->length += TC6_CHUNK_BUF_SIZE;
        }
        if (i < trail) {
            /* There was not enough buffer to enqueue all empty chunks */
            g->remainingTrail = (trail - i);
        } else {
            g->remainingTrail = 0;
        }
    }
}

static void serviceData(TC6_t *g, bool enqueueEmpty)
{
	if (g->enableData && (SPI_OP_INVALID == g->currentOp) && (qspibuf_stage1_transfer_ready(&g->qSpi))) {
		uint16_t maxTxLen;
		/**********************************/
		/* Try to enqueue Ethernet chunks */
		/**********************************/
		struct qspibuf *entry = qspibuf_stage1_transfer_ptr(&g->qSpi);
		initializeSpiEntry(entry);

		/**************************************/
		/* TX Data is getting generated here: */
		/**************************************/
		maxTxLen = g->prot.txc * TC6_CHUNK_BUF_SIZE;
		if (maxTxLen > sizeof(entry->txBuff)) {
			maxTxLen = sizeof(entry->txBuff);
		}
		entry->length = mk_data_tx(g, entry->txBuff, maxTxLen);
		addEmptyChunks(g, entry, (enqueueEmpty || g->prot.rda));

		if (entry->length) {
			/* Call enqueue before actual SPI transfer to avoid race error with Interrupt handler */
			qspibuf_stage1_transfer_done(&g->qSpi);
			if (!spiTransaction(g, entry->txBuff, entry->rxBuff, entry->length, SPI_OP_DATA)) {
				/* SPI driver is currently busy, restore previous states */
				qspibuf_stage1_transfer_undo(&g->qSpi);
			}
		}
	}
}

static void serviceControl(TC6_t *g)
{
	/***************/
	/* Control TX  */
	/***************/
	if (SPI_OP_INVALID == g->currentOp) {
		if (regop_send_ready(&g->regop_q)) {
			/**************/
			/* CONTROL TX */
			/**************/
			struct register_operation *reg_op = regop_send_ptr(&g->regop_q);

			/* Call enqueue before actual SPI transfer to avoid race error with Interrupt handler */
			regop_send_done(&g->regop_q);

			g->currentOp = SPI_OP_REG;
			if (!TC6Stub_SpiTransaction(g, reg_op->tx_buf, reg_op->rx_buf, reg_op->length, g->gTag)) {
				g->currentOp = SPI_OP_INVALID;
				regop_send_undo(&g->regop_q);
			}
		}
	}

	/****************************************/
	/* CONTROL RX and callback higher layer */
	/****************************************/
	while(regop_event_ready(&g->regop_q)) {
		struct register_operation *reg_op;
		uint32_t regVal = 0xFFFFFFFF;
		uint16_t num;
		reg_op = regop_event_ptr(&g->regop_q);
		num = read_rx_ctrl_buffer(reg_op->rx_buf, reg_op->length, &regVal, sizeof(regVal), reg_op->secure);
		if (REGISTER_OP_READWRITE_STAGE1 == reg_op->op) {
			modifyCallback(g, reg_op, (0u != num), reg_op->regAddr, regVal);
		} else if (NULL != reg_op->callback) {
			reg_op->callback(g, (0u != num), reg_op->regAddr, regVal, reg_op->tag, g->gTag);
		} else if (0u == num) {
			TC6_CB_OnError(g, TC6Error_NoHardware, g->gTag);
		} else {} /* MISRA enforced termination */
		regop_event_done(&g->regop_q);
	}
}

static bool spiTransaction(TC6_t *g, const uint8_t *pTx, uint8_t *pRx, uint16_t len, SpiOp_t op)
{
    bool success = false;
    if (g->currentOp == SPI_OP_INVALID) {
        g->currentOp = op;
        success = TC6Stub_SpiTransaction(g, pTx, pRx, len, g->gTag);
        if (!success) {
            g->currentOp = SPI_OP_INVALID;
        }
    }
    return success;
}

static void modifyCallback(TC6_t *g, struct register_operation *reg_op, bool success, uint32_t addr, uint32_t value)
{
    uint32_t val = value;
    if (success) {
        val = (val & ~reg_op->modifyMask) | reg_op->modifyValue;
        if (!accessRegisters(g, REGISTER_OP_READWRITE_STAGE2, addr
                            , val
                            , reg_op->secure
                            , 0    /* mask */
                            , reg_op->callback
                            , reg_op->tag)) {
            TC6Stub_Print(g, g->gTag, "ReadModifyWrite out of memory, increase REG_OP_ARRAY_SIZE\r\n");
        }
    } else {
        if (NULL != reg_op->callback) {
            reg_op->callback(g, false, addr, 0, reg_op->tag, g->gTag);
        }
    }
}

static bool accessRegisters(TC6_t *g, enum register_op_type op, uint32_t addr, uint32_t value, bool secure, uint32_t modifyMask, TC6_RegCallback_t callback, void *tag)
{
    struct register_operation *reg_op;
    uint16_t payloadSize = 0;
    bool write = true;
    bool success = false;

    if (regop_enqueue_ready(&g->regop_q)) {
        success = true;
        switch(op) {
        case REGISTER_OP_WRITE:
            write = true;
            break;
        case REGISTER_OP_READ:
            write = false;
            break;
        case REGISTER_OP_READWRITE_STAGE1:
            write = false;
            break;
        case REGISTER_OP_READWRITE_STAGE2:
            write = true;
            break;
        case REGISTER_OP_INVALLID:
        default:
            success = false;
            break;
        }
    }
    if (success) {
        reg_op = regop_enqueue_ptr(&g->regop_q);
#ifdef DEBUG
        (void)memset(reg_op, 0xCC, sizeof(struct register_operation));
#endif
        (void)memset(reg_op->tx_buf, 0x00, sizeof(reg_op->tx_buf));
        if (secure) {
            payloadSize = mk_secure_ctrl_req(write, false /* autoIncrement */, addr, 1 /* Array Len */, &value, reg_op->tx_buf, sizeof(reg_op->tx_buf));
        } else {
            payloadSize = mk_ctrl_req(write, false /* autoIncrement */, addr, 1 /* Array Len */, &value, reg_op->tx_buf, sizeof(reg_op->tx_buf));
        }
        if (payloadSize == 0u) {
            TC6Stub_Print(g, g->gTag, "mk_xx_ctrl_req failed\r\n");
            success = false;
        }
    }
    if (success) {
        reg_op->regAddr = addr;
        reg_op->length = payloadSize;
        reg_op->op = op;
        reg_op->secure = secure;
        reg_op->callback = callback;
        reg_op->tag = tag;
        reg_op->modifyValue = value;
        reg_op->modifyMask = modifyMask;

        regop_enqueue_done(&g->regop_q);
		TC6_CB_OnNeedService(g, g->gTag);
    }
    return success;
}

static void avgInitialize(TC6_t *g)
{
    uint8_t i;
    for (i=0; i < AVG_ARRAY_LEN; i++) {
        g->avg.length[i] = AVG_MIN_LEN;
    }
    g->avg.pos = 0;
}

static void avgStoreNewVal(TC6_t *g, uint16_t len)
{
    g->avg.length[g->avg.pos] = (len > AVG_MIN_LEN) ? len : AVG_MIN_LEN;
    g->avg.pos = (g->avg.pos + 1) % AVG_ARRAY_LEN;
}

static uint16_t avgGetAverageVal(TC6_t *g)
{
    uint16_t sum = 0;
    uint16_t avg;
    uint16_t i;
    for (i=0; i < AVG_ARRAY_LEN; i++) {
        sum += g->avg.length[i];
    }
    avg = sum / AVG_ARRAY_LEN;
    avg = ((avg + TC6_CHUNK_SIZE - 1) / TC6_CHUNK_SIZE) * TC6_CHUNK_SIZE; /* Integer round up */
    return avg;
}

static void processDataRx(TC6_t *g)
{
    /*******************************/
    /* DATA RX & Free up SPI Queue */
    /*******************************/
    while (qspibuf_stage3_process_ready(&g->qSpi)) {
        struct qspibuf *entry = qspibuf_stage3_process_ptr(&g->qSpi);

        enqueue_rx_spi(g, entry->rxBuff, entry->length);
        qspibuf_stage3_process_done(&g->qSpi);
    }
}

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*              CALLBACK FUNCTION FROM PROTOCOL STATEMACHINE            */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

static void on_tx_eth_done(TC6_t *g, const uint8_t *ptr, uint16_t len, TC6_RawTxCallback_t txCallback, void *priv)
{
    if (NULL != txCallback) {
        txCallback(g, ptr, len, priv, g->gTag);
    }
}

static void on_rx_slice(TC6_t *g, const uint8_t *buff, uint16_t offset, uint16_t buf_len, bool rtsa, bool rtsp)
{
    /* TODO: handle timestamp (RTSA / RTSP) */
    (void)rtsa;
    (void)rtsp;
    TC6_CB_OnRxEthernetSlice(g, buff, offset, buf_len, g->gTag);
}

static void on_rx_done(TC6_t *g, uint16_t buf_len, bool mfd)
{
    bool success = !mfd && !g->prot.eth_error;
    g->prot.eth_error = false;
    if (success) {
      avgStoreNewVal(g, buf_len);
      TC6_CB_OnRxEthernetPacket(g, true, buf_len, g->gTag);
    } else {
      TC6_CB_OnRxEthernetPacket(g, false, 0, g->gTag);
    }
}

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                         PROTOCOL STATEMACHINE                        */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

static inline void value2net(uint32_t value, uint8_t *buf)
{
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)value;
}

static inline uint32_t net2value(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3]);
}

static uint32_t get_parity(uint32_t p)
{
    uint32_t v = p;
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return ~v & 1u; /* odd parity */
}

/* Control Transaction API {{{ */

/*
 * The functions below create regular or secure control transaction buffer
 * ready for an SPI Tx transfer.  The register values for the 'write' operation
 * are taken from the 'tx_buf'.  The content for the SPI transfer is generated
 * into the 'tx_buf'.
 *
 * @wnr: Write, not read.
 * @aid: Disable address increment.
 * @addr:
 *     Upper 16 bit: Memory map selector (MMS).
 *     Lower 16 bit: Address.
 * @num_regs: Number of registers to read or write.
 * @tx_buf: Buffer for SPI Tx transfer.
 *     If operation is 'write', then the first 'num_regs' elements of the
 *     'tx_buf' must contain values to write.
 * @tx_buf_size: size of the 'tx_buf' in bytes.
 *
 * Returns number of the 'tx_buf' bytes to be sent using the SPI Tx transfer.
 * In case of error the function returns 0 and the buffer remains unchanged.
 *
 * SECURITY NOTE:
 *
 * The places for turn-around- or other gaps in the 'tx_buf' are kept
 * unchanged.  If needed, the application must clean up the buffer before
 * transfer it to the function.
 */

static uint16_t mk_ctrl_req(bool wnr, bool aid, uint32_t addr,
        uint8_t num_regs, const uint32_t *regs,
        uint8_t *tx_buf, uint16_t tx_buf_size)
{
    const uint32_t hdr =
        MK_MASK(HDR_C_DNC, 0) |
        MK_MASK(HDR_C_WNR, wnr) |
        MK_MASK(HDR_C_AID, aid) |
        MK_MASK(HDR_C_MMS, addr >> 16) |
        MK_MASK(HDR_C_ADDR, addr) |
        MK_MASK(HDR_C_LEN, num_regs - 1u);
    uint16_t i;
    bool success = true;

    if ((num_regs < 1u) || (num_regs > 128u)) {
        success = false;
    }

    if ((num_regs + 2u) > (tx_buf_size / 4u)) {
        success = false;
    }

    if (success) {
        if (wnr) {
            uint8_t *dst = tx_buf;

            dst = &dst[num_regs * 4u];
            for (i = num_regs; i > 0u; i--) {
                uint32_t v = regs[i - 1u];

                value2net(v, dst);
                dst = &dst[-4];
            }
        }

        value2net(hdr | MK_MASK(HDR_C_P, get_parity(hdr)), tx_buf);
    }
    return success ? ((num_regs * 4u) + 8u) : 0u;
}

static uint16_t mk_secure_ctrl_req(bool wnr, bool aid, uint32_t addr,
        uint8_t num_regs, const uint32_t *regs,
        uint8_t *tx_buf, uint16_t tx_buf_size)
{
    const uint32_t hdr =
        MK_MASK(HDR_C_DNC, 0) |
        MK_MASK(HDR_C_WNR, wnr) |
        MK_MASK(HDR_C_AID, aid) |
        MK_MASK(HDR_C_MMS, addr >> 16) |
        MK_MASK(HDR_C_ADDR, addr) |
        MK_MASK(HDR_C_LEN, num_regs - 1u);
    uint16_t i;
    bool success = true;

    if ((num_regs < 1u) || (num_regs > 128u)) {
        success = false;
    }

    if (success && (((num_regs * 2u) + 2u) > (tx_buf_size / 4u))) {
        success = false;
    }

    if (success) {
        if (wnr) {
            uint8_t *dst = tx_buf;

            dst = &dst[num_regs * 8u];
            for (i = num_regs; i > 0u; i--) {
                uint32_t v = regs[i - 1u];

                value2net(~v, dst);
                value2net(v, &dst[-4]);
                dst = &dst[-8];
            }
        }

        value2net(hdr | MK_MASK(HDR_C_P, get_parity(hdr)), tx_buf);
    }
    return success ? ((num_regs * 8u) + 8u) : 0u;
}

static uint16_t read_rx_ctrl_buffer(const uint8_t *rx_buf, uint16_t rx_buf_size,
        uint32_t *regs_buf, uint8_t regs_buf_size, bool secure)
{
    uint16_t i;
    uint16_t num = 0;
    const uint8_t *src;

    if (secure) {
        if ((0u == (rx_buf_size % 8u)) && ((rx_buf_size / 8u) >= 2u)) {
            num = (rx_buf_size / 8u) - 1u;
            if ((regs_buf_size / 4u) < num) {
                num = 0;
            }
        }

        src = rx_buf;
        for (i = 0; i < num; i++) {
            src = &src[8];
            if (net2value(src) != ~net2value(&src[4])) {
                num = 0;
            }
        }
        src = rx_buf;
        for (i = 0; i < num; i++) {
            src = &src[8];
            regs_buf[i] = net2value(src);
        }
    } else {
        num = (rx_buf_size - 8u) / 4u;
        if ((regs_buf_size / 4u) < num) {
            num = 0;
        } else {
            src = &rx_buf[8];
            for (i = 0; i < num; i++) {
                regs_buf[i] = net2value(src);
                src = &src[4];
            }
        }
    }
    return num;
}

/* }}} */

/* Tx API {{{ */

/*
 * Maps longest possible slice from unprocessed ETH payload onto a free SPI
 * chunk space.
 */
static uint16_t process_tx(TC6_t *g, uint8_t *tx_buf)
{
    struct qtxeth_queue *q = &g->prot.eth_q;
    struct qtxeth *entry;
    uint32_t header = MK_MASK(HDR_DNC, 1) | MK_MASK(HDR_DV, 1);
    uint16_t tocopy_len;
    uint16_t padded_len;
    uint16_t retVal = 0;

    if (qtxeth_convert_ready(q)) {
        entry = qtxeth_convert_ptr(q);
#ifdef DEBUG
        (void)memset(tx_buf, 0xCD, TC6_CHUNK_BUF_SIZE);
#endif
        header |= MK_MASK(HDR_SEQ, g->prot.seq_num++);
        if (!g->prot.offsetEth) {
            header |= MK_MASK(HDR_SV, 1) | MK_MASK(HDR_SWO, 0);
        }

        tocopy_len = entry->len - g->prot.offsetEth;
        tocopy_len = (tocopy_len <= TC6_CHUNK_SIZE) ? tocopy_len : TC6_CHUNK_SIZE;
        padded_len = TC6_CHUNK_SIZE - tocopy_len;

        memcpy(&tx_buf[TC6_HEADER_SIZE], &entry->ptr[g->prot.offsetEth], tocopy_len);
        if (0u != padded_len) {
		(void)memset(&tx_buf[TC6_HEADER_SIZE + tocopy_len], 0x0, padded_len);
        }
        g->prot.offsetEth += tocopy_len;
        if (g->prot.offsetEth == entry->len) {
            if (entry->len < TC6_CHUNK_SIZE) {
                /* Test hack to round up tiny Ethernet frames to full chunk size. */
		header |= MK_MASK(HDR_EV, 1u) | MK_MASK(HDR_EBO, TC6_CHUNK_SIZE - 1u);
            } else {
                header |= MK_MASK(HDR_EV, 1u) | MK_MASK(HDR_EBO, tocopy_len - 1u);
            }
            g->prot.offsetEth = 0;
            on_tx_eth_done(g, entry->ptr, entry->len, entry->txCallback, entry->priv);
            qtxeth_convert_done(q);
        }
        header |= MK_MASK(HDR_P, get_parity(header));
        value2net(header, tx_buf);
        retVal = TC6_CHUNK_BUF_SIZE;
    }
    return retVal;
}

static uint16_t mk_data_tx(TC6_t *g, uint8_t *tx_buf, uint16_t tx_buf_len)
{
    uint16_t pos = 0;
    uint16_t result;
    while (pos < tx_buf_len) {
        result = process_tx(g, &tx_buf[pos]);
        if (!result) {
            break;
        }
        pos += result;
    }
    return pos;
}

/* }}} */

/* Rx API {{{ */

static inline void signal_rx_error(TC6_t *g, TC6_Error_t err)
{
    g->prot.eth_error = true;
    TC6_CB_OnError(g, err, g->gTag);
}

static inline void process_rx(TC6_t *g, const uint8_t *buff, uint16_t buf_len)
{
    const uint8_t *fptr = &buff[buf_len - TC6_HEADER_SIZE];
    uint32_t sv;
    uint32_t ev;
    uint32_t rtsa = 0;
    uint32_t rtsp = 0;
    uint32_t mfd;
    uint32_t sbo;
    uint32_t ebo;
    uint32_t footer;
    uint16_t len;
    bool twoFrames;

    footer = net2value(fptr);
    if (0u != (footer & FTR_DATA_MASK)) {
        bool success = true;

        sv = GET_VAL(FTR_SV, footer);
        sbo = sv ? (GET_VAL(FTR_SWO, footer) * 4u) : 0u;

        ev = GET_VAL(FTR_EV, footer);
        ebo = ev ? (GET_VAL(FTR_EBO, footer) + 1u) : TC6_CHUNK_SIZE;

        mfd = GET_VAL(FTR_MFD, footer);
        twoFrames = (ebo <= sbo);

        if (twoFrames) {
            /* Two ETH frames in chunk */
            on_rx_slice(g, buff, g->prot.offsetRx, (uint16_t)ebo, rtsa, rtsp);
            on_rx_done(g, (uint16_t)(g->prot.offsetRx + ebo), mfd);
            g->prot.offsetRx = 0;
            len = (uint16_t)(TC6_CHUNK_SIZE - sbo);
        } else {
            /* Single Eth frame in chunk */
            len = (uint16_t)(ebo - sbo);
        }

        if (!twoFrames && sv && g->prot.eth_started) {
            signal_rx_error(g, TC6Error_UnexpectedSv);
            success = false;
        }

        if (success && !g->prot.eth_started && !g->prot.eth_error && !sv) {
            signal_rx_error(g, TC6Error_UnexpectedDvEv);
            success = false;
        }

        if ((!sv || twoFrames) && g->prot.eth_error) {
            /* Wait for next start valid flag to clear error flag */
            success = false;
        }

        if (success) {
            if (0u != sv) {
                rtsa = GET_VAL(FTR_RTSA, footer);
                rtsp = GET_VAL(FTR_RTSP, footer);
            }

            g->prot.eth_started = true;
            g->prot.eth_error = false;
            on_rx_slice(g, &buff[sbo], g->prot.offsetRx, len, rtsa, rtsp);

            if (!twoFrames && ev) {
                uint16_t offset = g->prot.offsetRx;
                g->prot.eth_started = false;
                g->prot.offsetRx = 0;
                on_rx_done(g, offset + len, mfd);
            } else {
                g->prot.offsetRx += len;
            }
        }
    } else {
        g->prot.eth_error = false;
    }
}

static void enqueue_rx_spi(TC6_t *g, const uint8_t *buff, uint16_t buf_len)
{
    uint32_t processed;
    bool success = true;
    if (!buf_len || (buf_len % TC6_CHUNK_BUF_SIZE)) {
        success = false;
    }
    for (processed = 0; success && (processed < buf_len); processed += TC6_CHUNK_BUF_SIZE) {
        uint32_t footer = net2value( &buff[processed + TC6_CHUNK_SIZE] );

        if ((0x0u == footer) || (0xFFFFFFFFu == footer)) {
            signal_rx_error(g, TC6Error_NoHardware);
            success = false;
        }
        if (success && get_parity(footer)) {
            signal_rx_error(g, TC6Error_BadChecksum);
            success = false;
        }
        if (success && !GET_VAL(FTR_DNC, footer)) {
            signal_rx_error(g, TC6Error_UnexpectedCtrl);
            success = false;
        }
        if (success && GET_VAL(FTR_HDRB, footer)) {
            signal_rx_error(g, TC6Error_BadTxData);
            success = false;
        }
        g->prot.synced = GET_VAL(FTR_SYNC, footer);
        if (success && !g->prot.synced) {
            signal_rx_error(g, TC6Error_SyncLost);
            success = false;
        }
        if (success) {
            if (!g->prot.exst_locked) {
                if (0u != GET_VAL(FTR_EXST, footer)) {
                    g->prot.exst_locked = true;
                    TC6_CB_OnExtendedStatus(g, g->gTag);
                }
            }
            process_rx(g, &buff[processed], TC6_CHUNK_BUF_SIZE);
        }
    }
}

static void update_credit_cnt(TC6_t *g, const uint8_t *buff, uint16_t buf_len)
{
    const uint8_t *const last_fptr = &buff[buf_len - TC6_HEADER_SIZE];
    uint32_t footer = net2value(last_fptr);
    bool success = true;

    if (0u != get_parity(footer)) {
        success = false;
    }
    if (success && GET_VAL(FTR_HDRB, footer)) {
        success = false;
    }
    if (success && !GET_VAL(FTR_SYNC, footer)) {
        success = false;
    }
    if (success) {
        g->prot.txc = GET_VAL(FTR_TXC, footer);
        g->prot.rda = GET_VAL(FTR_RDA, footer);
    }
}

/* }}} */

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                  CALLBACK SECTION FROM TC6 STUB                      */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/*
 * This function is called from the interrupt.
 */
void TC6Stub_CB_OnSpiBufferDone(TC6_t *g, bool success)
{
	struct qspibuf *entry;

	g->intContext = true;
	if (!success) {
		signal_rx_error(g, TC6Error_SpiError);
	}
	switch (g->currentOp) {
		case SPI_OP_DATA:
			if (!qspibuf_stage2_int_ready(&g->qSpi)) {
				break;
			}
			entry = qspibuf_stage2_int_ptr(&g->qSpi);
			update_credit_cnt(g, entry->rxBuff, entry->length);
			qspibuf_stage2_int_done(&g->qSpi);
			break;
		case SPI_OP_REG:
			if(!regop_int_ready(&g->regop_q)) {
				break;
			}
			regop_int_done(&g->regop_q);
			break;
		case SPI_OP_INVALID:
		default:
			break;
	}
	g->currentOp = SPI_OP_INVALID;
	g->intContext = false;
	TC6_CB_OnNeedService(g, g->gTag);
}
