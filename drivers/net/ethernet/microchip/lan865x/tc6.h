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
#ifndef TC6_H_
#define TC6_H_

#include <asm-generic/bug.h>
#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                            DEFINITIONS                               */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/**
 * \brief Platform specific assertion call. Maybe defined to nothing
 */
#ifndef TC6_ASSERT
#define TC6_ASSERT(condition)  BUG_ON(condition?false:true)
#endif

/**
 * \brief Set the maximum amount of parallel TC6 instances. 1 means a single MACPHY hardware is attached
 */
#ifndef TC6_MAX_INSTANCES
#define TC6_MAX_INSTANCES   (3)
#endif

/**
 * \brief Defines the additional payload caused by the TC6 protocol (Footer or Header)
 * \note This value is defined by the Open Alliance, do not modify.
 */
#ifndef TC6_HEADER_SIZE
#define TC6_HEADER_SIZE     (4)
#endif

/**
 * \brief Defines the data payload length of a single TC6 chunk
 * \note The only valid values are either 32 or 64
 */
#ifndef TC6_CHUNK_SIZE
#define TC6_CHUNK_SIZE      (64)
#endif

/**
 * \brief Defines the entire payload length of a single TC6 chunk including header / footer.
 * \note Do not modify.
 */
#ifndef TC6_CHUNK_BUF_SIZE
#define TC6_CHUNK_BUF_SIZE  (TC6_CHUNK_SIZE + TC6_HEADER_SIZE)
#endif

/**
 * \brief Set the queue length for control data
 * \note Given length must be power of 2 (2^n).
 */
#ifndef REG_OP_ARRAY_SIZE
#define REG_OP_ARRAY_SIZE   (12)
#endif

/**
 * \brief Defines the maximum burst length of an SPI transfer. Meaning number of TC6 chunks with a single SPI transaction.
 * \note Depending on the situation, less chunks maybe transfered within a single transaction.
 */
#ifndef TC6_CHUNKS_XACT
#define TC6_CHUNKS_XACT     (22)
#endif
/**
 * \brief Defines the queue size for holding entire MOSI and MISO data
 */
#ifndef SPI_FULL_BUFFERS
#define SPI_FULL_BUFFERS    (4)
#endif

/**
 * \brief Defines the queue size for holding pointer to Ethernet frames coming out of the TCP/IP stack.
 * \note Only a reference to the payload is stored, not the entire payload it self.
 */
#ifndef TC6_TX_ETH_QSIZE
#define TC6_TX_ETH_QSIZE    (1024)
#endif

/**
 * \brief Defines the maximum amount of simultaneous control register operations
 * \note Do not modify.
 */
#ifndef TC6_MAX_CNTRL_VARS
#define TC6_MAX_CNTRL_VARS  (1)
#endif

/**
 * \brief Defines the length of the history buffer used to calculate the next average receive length
 */
#ifndef AVG_ARRAY_LEN
#define AVG_ARRAY_LEN       (5)
#endif

struct TC6_t;
typedef struct TC6_t TC6_t;

#define TC6STUB_INST_UNKNOWN (0xFF)
#define TC6PRINT_DEBUG_INFO

typedef enum
{
    TC6Error_Succeeded,         /** No error occurred */
    TC6Error_NoHardware,        /** MISO data implies that there is no MACPHY hardware available */
    TC6Error_UnexpectedSv,      /** Unexpected Start Valid Flag  */
    TC6Error_UnexpectedDvEv,    /** Unexpected Data Valid or End Valid Flag  */
    TC6Error_BadChecksum,       /** Checksum in footer is wrong */
    TC6Error_UnexpectedCtrl,    /** Unexpected control packet received */
    TC6Error_BadTxData,         /** Header Bad Flag received */
    TC6Error_SyncLost,          /** Sync Flag is no longer set */
    TC6Error_SpiError,          /** The SPI transaction was reported to be not successful */
} TC6_Error_t;

/**
 * \brief Callback when ever a transmission of RAW Ethernet packet was finished.
 * \note This function may be implemented by the integrator and passed as argument with TC6_SendRawEthernetPacket().
   \param pInst - The pointer returned by TC6_Init.
 * \param pTx - Exact the same pointer as has been given along with the TC6_SendRawEthernetPacket function.
 * \param len - Exact the same length as has been given along with the TC6_SendRawEthernetPacket function.
 * \param pTag - Tag pointer which was given along TC6_SendRawEthernetPacket function.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
typedef void (*TC6_RawTxCallback_t)(TC6_t *pInst, const uint8_t *pTx, uint16_t len, void *pTag, void *pGlobalTag);

/**
 * \brief Callback when ever a register access was finished.
 * \note This function may be implemented by the integrator and passed as argument with TC6_ReadRegister() or TC6_WriteRegister() or TC6_ReadModifyWriteRegister().
 * \note It is safe inside this callback to call again TC6_ReadRegister() or TC6_WriteRegister() or TC6_ReadModifyWriteRegister().
 * \param pInst - The pointer returned by TC6_Init.
 * \param success - true, if the register could be accessed without errors. false, there was an error while trying to access the register.
 * \param addr - The register address, as passed a long with TC6_ReadRegister() or TC6_WriteRegister() or TC6_ReadModifyWriteRegister().
 * \param value - The register value. If this belongs to a write request, it is the same value as given along with TC6_WriteRegister(). In case of a read, this holds the register read value. In case of TC6_ReadModifyWriteRegister() it holds the final value, which was written back into the MAC/PHY.
 * \param pTag - Tag pointer which was given along with the register access functions.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
typedef void (*TC6_RegCallback_t)(TC6_t *pInst, bool success, uint32_t addr, uint32_t value, void *pTag, void *pGlobalTag);

struct qtxeth {
	void *priv;
	TC6_RawTxCallback_t txCallback;
	const uint8_t *ptr;
	uint16_t len;
};

#define TC6_CNTRL_BUF_SIZE  ((2 + (TC6_MAX_CNTRL_VARS * 2)) * 4)
#define TC6_SPI_BUF_SIZE    (TC6_CHUNKS_XACT * TC6_CHUNK_BUF_SIZE)

struct qspibuf {
    uint8_t txBuff[TC6_SPI_BUF_SIZE];
    uint8_t rxBuff[TC6_SPI_BUF_SIZE];
    uint16_t length;
};

enum register_op_type
{
    REGISTER_OP_INVALLID,
    REGISTER_OP_WRITE,
    REGISTER_OP_READ,
    REGISTER_OP_READWRITE_STAGE1,
    REGISTER_OP_READWRITE_STAGE2
};

struct register_operation {
    uint8_t tx_buf[TC6_CNTRL_BUF_SIZE];
    uint8_t rx_buf[TC6_CNTRL_BUF_SIZE];
    TC6_RegCallback_t callback;
    void *tag;
    enum register_op_type op;
    uint32_t modifyValue;
    uint32_t modifyMask;
    uint32_t regAddr;
    uint16_t length;
    bool secure;
};

typedef enum {
    MemOp_Write = 0,
    MemOp_ReadModifyWrite = 1,
    MemOp_Read = 2
} MemoryOp_t;

/**
 * \brief Structure holding a register read/write
 */
typedef struct {
    uint32_t address;
    uint32_t value;
    uint32_t mask;
    MemoryOp_t op;
    bool secure;
} MemoryMap_t;

struct qtxeth_queue {
    struct qtxeth *buffer_;
    unsigned size_;
    unsigned enqueue_, convert_;
};

struct qspibuf_queue {
    struct qspibuf *buffer_;
    unsigned size_;
    unsigned stage1_transfer_, stage2_int_, stage3_process_;
};

struct regop_queue {
    struct register_operation *buffer_;
    unsigned size_;
    unsigned enqueue_, send_, int_, event_;
};

/* Internal structures {{{ */

struct spi_prot {
    struct qtxeth tx_eth_buffer[TC6_TX_ETH_QSIZE];
    struct qtxeth_queue eth_q;
    uint16_t offsetEth;
    uint16_t offsetRx;
    uint8_t seq_num;
    uint8_t txc;
    bool synced;
    bool exst_locked;
    bool eth_started;
    bool eth_error;
    bool rda;
};

/* }}} */

typedef enum
{ SPI_OP_INVALID,
  SPI_OP_DATA,
  SPI_OP_REG
} SpiOp_t;

struct average_rx_length
{
    uint16_t length[AVG_ARRAY_LEN];
    uint16_t remainingTrail;
    uint8_t pos;
};

/** Integrator needs to allocate this structure. But the elements must not be accessed.
 *  They are getting filled via function calls only.
 */
struct TC6_t
{
    struct qspibuf spiBuf[SPI_FULL_BUFFERS];
    struct qspibuf_queue qSpi;
    struct register_operation regop_storage[REG_OP_ARRAY_SIZE];
    struct regop_queue regop_q;
    struct spi_prot prot;
    struct average_rx_length avg;
    void *gTag;
    volatile SpiOp_t currentOp;
    uint32_t magic;
    uint8_t instance;
    uint8_t remainingTrail;
    bool enableData;
    bool intContext;
};

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                            PUBLIC API                                */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/** \brief Initializes the lwIP Interface Driver for TC6.
 *  \param pGlobalTag - This pointer will be returned back with any callback of this component. Maybe set to NULL.
 *  \note Must be called before any other functions of this component.
 *  \return Filled structure for further usage with other functions of this component. Or NULL, if there was an error.
 */
void TC6_Init(TC6_t *pInst);

/** \brief Reset all internal state machines and queues
 *  \param pInst - The pointer returned by TC6_Init.
 */
void TC6_Reset(TC6_t *pInst);

/** \brief Services the hardware and the protocol stack.
 *  \note Must be called when a) TC6 Interrupt became active (level triggered) or b) when the TC6_CB_OnNeedService() callback was raised.
 *  \param pInst - The pointer returned by TC6_Init.
 *  \param interruptLevel - The level of the interrupt pin. false is interpreted as interrupt active. true is interpreted as currently no interrupt issued.
 */
void TC6_Service(TC6_t *pInst, bool interruptLevel);

/** \brief Enables or disables outgoing / incoming data traffic.
 *  \note It make sense to enable data transfer, when configuring the registers is done.
 *  \param pInst - The pointer returned by TC6_Init.
 *  \param interruptLevel - The level of the interrupt pin. false is interpreted as interrupt active. true is interpreted as currently no interrupt issued.
 */
void TC6_EnableData(TC6_t *pInst, bool enable);

/** \brief Returns the current instance number of the given TC6 pointer
 *  \param pInst - The pointer returned by TC6_Init.
 *  \return Instance number, starting with 0 for the first instance
 */
uint8_t TC6_GetInstance(TC6_t *pInst);

/** \brief Returns the TC6 related status variables such as TX and RX credit counter and if the controller is in sync state.
 *  \param pInst - The pointer returned by TC6_Init.
 *  \param pTxCredit - Pointer to a TX credit variable. This function writes the current TX credit value into this variable. NULL pointer is accepted.
 *  \param pRxCredit - Pointer to a RX credit variable. This function writes the current RX value into this variable. NULL pointer is accepted.
 *  \param pSynced - Pointer to a synced variable. This function writes the current synced state into this variable. NULL pointer is accepted.
 */
void TC6_GetState(TC6_t *pInst, uint8_t *pTxCredit, uint8_t *pRxCredit, bool *pSynced);

/** \brief Sends a raw Ethernet packet.
 *  \param pInst - The pointer returned by TC6_Init.
 *  \param PTx - Filled byte array holding an entire Ethernet packet. Warning, the buffer must stay valid until TC6_CB_OnTxRawEthernetPacket callback with this pointer as parameter was called.
 *  \param len - Length of the byte array.
 *  \param pTag - Any pointer the integrator wants to give. It will be returned in TC6_CB_OnTxRawEthernetPacket.
 *  \return true, on success. false, otherwise.
 */
bool TC6_SendRawEthernetPacket(TC6_t *pInst, const uint8_t *pTx, uint16_t len, TC6_RawTxCallback_t txCallback, void *tag);

/** \brief Reads from MAC / Phy registers
 *  \param pInst - The pointer returned by TC6_Init.
 *  \param addr - The 32 Bit register offset.
 *  \param secure - true, if TC6 secure control communication shall be used. false, otherwise.
 *  \param rxCallback - Pointer to a callback handler. May left NULL.
 *  \param pTag - Any pointer. Will be given back in given rxCallback. May left NULL.
 *  \return true, on success. false, otherwise.
 */
bool TC6_ReadRegister(TC6_t *pInst, uint32_t addr, bool secure, TC6_RegCallback_t rxCallback, void *pTag);

/** \brief Writes to MAC / Phy registers
 *  \param pInst - The pointer returned by TC6_Init.
 *  \param addr - The 32 Bit register offset.
 *  \param value - The new 32 Bit register value.
 *  \param secure - true, if TC6 secure control communication shall be used. false, otherwise.
 *  \param txCallback - Pointer to a callback handler. May left NULL.
 *  \param pTag - Any pointer. Will be given back in given txCallback. May left NULL.
 *  \return true, on success. false, otherwise.
 */
bool TC6_WriteRegister(TC6_t *pInst, uint32_t addr, uint32_t value, bool secure, TC6_RegCallback_t txCallback, void *pTag);

/** \brief Reads, modifies and writes back the changed value to MAC / Phy registers
 *  \param pInst - The pointer returned by TC6_Init.
 *  \param addr - The 32 Bit register offset.
 *  \param value - The 32 Bit register bit values to be changed. This value will be set to register only if mask on the corresponding position is set to 1.
 *  \param mask - The 32 Bit register bit mask. Only Bits set to 1 will be changed accordingly to value.
 *  \param secure - true, if TC6 secure control communication shall be used. false, otherwise.
 *  \param modifyCallback - Pointer to a callback handler. May left NULL.
 *  \param pTag - Any pointer. Will be given back in given modifyCallback. May left NULL.
 *  \return true, on success. false, otherwise.
 */
bool TC6_ReadModifyWriteRegister(TC6_t *pInst, uint32_t addr, uint32_t value, uint32_t mask, bool secure, TC6_RegCallback_t modifyCallback, void *pTag);

/** \brief Execute a list of register commands
 *  \param pInst - The pointer returned by TC6_Init.
 *  \param pMap - Array of memory commands
 *  \param mapLength - The length of the given array.
 *  \param multipleCallback - Pointer to a callback handler, it will be called for every single entry of the memory map. May left NULL.
 *  \param pTag - Any pointer. Will be given back in given modifyCallback. May left NULL.
 *  \return The amount of register commands enqueued. May return 0 when queue is total full. May return less then mapLength when queue is partly full.
 */
uint16_t TC6_MultipleRegisterAccess(TC6_t *pInst, const MemoryMap_t *pMap, uint16_t mapLength, TC6_RegCallback_t multipleCallback, void *pTag);

/** \brief Reenable the reporting of extended status flag via TC6_CB_OnExtendedStatus() callback.
 *  \note This feature was introduced to not trigger thousands of extended status callbacks, when there is a lot of traffic ongoing.
 *  \param pInst - The pointer returned by TC6_Init.
 */
void TC6_UnlockExtendedStatus(TC6_t *pInst);

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                        CALLBACK SECTION                              */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/**
 * \brief Callback when ever this component needs to be serviced by calling TC6_Service()
 * \note This function must be implemented by the integrator.
 * \note Do not call TC6_Service() within this callback! Set a flag or raise an event to do it in the cyclic task context.
 * \warning !! THIS FUNCTION MAY GET CALLED FROM TASK AND INTERRUPT CONTEXT !!
 * \param pInst - The pointer returned by TC6_Init.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
extern void TC6_CB_OnNeedService(TC6_t *pInst, void *pGlobalTag);

/**
 * \brief Callback when ever a slice of an Ethernet packet was received.
 * \note This function must be implemented by the integrator.
 * \note In most cases this function will not give an entire Ethernet frame. It will be only a piece of it. The integrator needs to combine it.
 * \param pInst - The pointer returned by TC6_Init.
 * \param pRx - Filled byte array holding the received Ethernet packet
 * \param offset - 0, if this is the start of a new Ethernet frame. Otherwise the offset to which this slide of payload belongs to.
 * \param len - Length of the byte array.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
extern void TC6_CB_OnRxEthernetSlice(TC6_t *pInst, const uint8_t *pRx, uint16_t offset, uint16_t len, void *pGlobalTag);

/**
 * \brief Callback when ever an Ethernet packet was received. This will notify the integrator, that now all chunks very reported by TC6_CB_OnRxEthernetPacket and the data can be processed.
 * \note This function must be implemented by the integrator.
 * \param pInst - The pointer returned by TC6_Init.
 * \param success - true, if the received Ethernet frame was received without errors. false, if there were errors.
 * \param len - Length of the entire Ethernet frame. This is all length reported TC6_CB_OnRxEthernetPacket combined.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
extern void TC6_CB_OnRxEthernetPacket(TC6_t *pInst, bool success, uint16_t len, void *pGlobalTag);

/**
 * \brief Callback when ever an error occurred.
 * \note This function must be implemented by the integrator.
 * \param pInst - The pointer returned by TC6_Init.
 * \param err - Enumeration value holding the actual error condition.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
extern void TC6_CB_OnError(TC6_t *pInst, TC6_Error_t err, void *pGlobalTag);

/**
 * \brief Callback when ever TC6 packet was received, which had the extended flag set. This means, that the user should read (and clear) at least Status0 and Status1 registers.
 * \note The integrator MUST call TC6_UnlockExtendedStatus() whenever he is ready to process the next extended status flag. Reenabling it to early may trigger thousands of events per second.
 * \note This function must be implemented by the integrator.
 * \param pInst - The pointer returned by TC6_Init.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
extern void TC6_CB_OnExtendedStatus(TC6_t *pInst, void *pGlobalTag);

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                         DEVICE DEPENDEND STUB CALLBABACKS            */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/**
 * \brief User may use this function to initialize the SPI hardware and driver
 * \param tc6instance - The instance number of the hardware. Starting with 0 for the first.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
extern void TC6Stub_InitializeSpi(TC6_t *pInst, void *pGlobalTag);
#ifdef TC6PRINT_DEBUG_INFO
/**
 * \brief Users implementation of printing function. It may print to UART/Console/Network or may be implemented empty, so messages are getting discarded.
 * \param tc6instance - The instance number of the hardware. Starting with 0 for the first.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 */
extern void TC6Stub_Print(TC6_t *pInst, void *pGlobalTag, const char *statement, ... ) __attribute__ ((format (gnu_printf, 3, 4)));
#endif
/**
 * \brief Users implementation of SPI transfer function.
 * \note The implementation may be synchronous or asynchronous. But in any case the TC6Stub_CB_OnSpiBufferDone() must be called, when the SPI transaction is over!
 * \param tc6instance - The instance number of the hardware. Starting with 0 for the first.
 * \param pTx - Pointer to the MOSI data. The pointer stays valid until user calls TC6Stub_CB_OnSpiBufferDone()
 * \param pRx - Pointer to the MISO Buffer. The pointer stays valid until user calls TC6Stub_CB_OnSpiBufferDone()
 * \param len - The length of both buffers (pTx and pRx). The entire length must be transfered via SPI.
 * \param pGlobalTag - The exact same pointer, which was given along with the TC6_Init() function.
 * \return true, if the SPI data was enqueued/transfered. false, there was an error.
 */
extern bool TC6Stub_SpiTransaction(TC6_t *pInst, const uint8_t *pTx, uint8_t *pRx, uint16_t len, void *pGlobalTag);

/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/
/*                        CALLBACK SECTION                              */
/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/** 
 * \brief The user must call this function once the SPI transaction is finished.
 * \note This function may be called direct in the implementation of TC6Stub_SpiTransaction, or any timer later and also from interrupt context.
 * \param tc6instance - The instance number of the hardware. Starting with 0 for the first.
 * \param success - true, if SPI transaction was successful. false, otherwise.
 */
extern void TC6Stub_CB_OnSpiBufferDone(TC6_t *pInst, bool success);

#ifdef __cplusplus
}
#endif
#endif /* TC6_H_ */
