/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth.h
 * @brief GRETH2 (Gigabit Ethernet) driver definitions
 *
 * This header file contains the register definitions, bit masks,
 * and data structures required for the GRETH2 Ethernet driver.
 *
 * @ingroup GRETH2
 */

/*
 * Copyright (C) 2026 Frontgrade Gaisler AB

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _GR_ETH_
#define _GR_ETH_

#include "greth2_emac.h"
#include "lwip/def.h"

#define GRETH2_BUF_SIZE 4096
#define GRETH2_NUM_BD   128

/**< Tx BD CTRL */
#define GRETH2_TXD_CTRL_EN_BIT    31 /**< Enable */
#define GRETH2_TXD_CTRL_WRAP_BIT  30 /**< Wrap (last BD) */
#define GRETH2_TXD_CTRL_IRQ_BIT   29 /**< Interrupt enable */
#define GRETH2_TXD_CTRL_MORE_BIT  28 /**< More descriptors for packet */
#define GRETH2_TXD_CTRL_IPCS_BIT  27 /**< IP Checksum */
#define GRETH2_TXD_CTRL_UDPCS_BIT 26 /**< UDP Checksum */
#define GRETH2_TXD_CTRL_TCPCS_BIT 25 /**< TCP Checksum */
#define GRETH2_TXD_CTRL_LEN_BIT   0  /**< Length */

#define GRETH2_TXD_CTRL_EN    ( 1 << GRETH2_TXD_CTRL_EN_BIT )
#define GRETH2_TXD_CTRL_WRAP  ( 1 << GRETH2_TXD_CTRL_WRAP_BIT )
#define GRETH2_TXD_CTRL_IRQ   ( 1 << GRETH2_TXD_CTRL_IRQ_BIT )
#define GRETH2_TXD_CTRL_MORE  ( 1 << GRETH2_TXD_CTRL_MORE_BIT )
#define GRETH2_TXD_CTRL_IPCS  ( 1 << GRETH2_TXD_CTRL_IPCS_BIT )
#define GRETH2_TXD_CTRL_UDPCS ( 1 << GRETH2_TXD_CTRL_UDPCS_BIT )
#define GRETH2_TXD_CTRL_TCPCS ( 1 << GRETH2_TXD_CTRL_TCPCS_BIT )

#define GRETH2_TXD_CTRL_LEN_MASK 0x3fff /**< Mask for length field (13:0) */

#define GRETH2_TXD_CTRL_CS \
  ( GRETH2_TXD_CTRL_IPCS | GRETH2_TXD_CTRL_UDPCS | GRETH2_TXD_CTRL_TCPCS )

/**< Tx BD STS */
#define GRETH2_TXD_STS_LC_BIT 4 /**< Late Collision */
#define GRETH2_TXD_STS_AL_BIT 3 /**< Attempt limit error */
#define GRETH2_TXD_STS_UE_BIT 2 /**< Underrun error */
#define GRETH2_TXD_STS_CO_BIT 1 /**< Completed */
#define GRETH2_TXD_STS_ST_BIT 0 /**< Started */

#define GRETH2_TXD_STS_LC ( 1 << GRETH2_TXD_STS_LC_BIT )
#define GRETH2_TXD_STS_AL ( 1 << GRETH2_TXD_STS_AL_BIT )
#define GRETH2_TXD_STS_UE ( 1 << GRETH2_TXD_STS_UE_BIT )
#define GRETH2_TXD_STS_CO ( 1 << GRETH2_TXD_STS_CO_BIT )
#define GRETH2_TXD_STS_ST ( 1 << GRETH2_TXD_STS_ST_BIT )

#define GRETH2_TXD_STS_ERR \
  ( GRETH2_TXD_STS_LC | GRETH2_TXD_STS_AL | GRETH2_TXD_STS_UE )

/**< Rx BD CTRL */
#define GRETH2_RXD_CTRL_EN_BIT   31 /**< Enable */
#define GRETH2_RXD_CTRL_WRAP_BIT 30 /**< Wrap (last BD) */
#define GRETH2_RXD_CTRL_IRQ_BIT  29 /**< Interrupt enable */

#define GRETH2_RXD_CTRL_EN   ( 1 << GRETH2_RXD_CTRL_EN_BIT )
#define GRETH2_RXD_CTRL_WRAP ( 1 << GRETH2_RXD_CTRL_WRAP_BIT )
#define GRETH2_RXD_CTRL_IRQ  ( 1 << GRETH2_RXD_CTRL_IRQ_BIT )

/**< Rx BD STS */
#define GRETH2_RXD_STS_CO_BIT  31 /**< Completed */
#define GRETH2_RXD_STS_ST_BIT  30 /**< Started */
#define GRETH2_RXD_STS_LE_BIT  29 /**< Length error */
#define GRETH2_RXD_STS_OE_BIT  28 /**< Overrun error */
#define GRETH2_RXD_STS_CE_BIT  27 /**< CRC error */
#define GRETH2_RXD_STS_FT_BIT  26 /**< Frame too long */
#define GRETH2_RXD_STS_AE_BIT  25 /**< Alignment error */
#define GRETH2_RXD_STS_MC_BIT  24 /**< Multicast address */
#define GRETH2_RXD_STS_ID_BIT  23 /**< IP detected */
#define GRETH2_RXD_STS_IR_BIT  22 /**< IP error */
#define GRETH2_RXD_STS_UD_BIT  21 /**< UDP detected */
#define GRETH2_RXD_STS_UR_BIT  20 /**< UDP error */
#define GRETH2_RXD_STS_TD_BIT  19 /**< TCP detected */
#define GRETH2_RXD_STS_TR_BIT  18 /**< TCP error */
#define GRETH2_RXD_STS_IF_BIT  17 /**< IP fragment */
#define GRETH2_RXD_STS_LEN_BIT 0  /**< Length */

#define GRETH2_RXD_STS_CO ( 1 << GRETH2_RXD_STS_CO_BIT )
#define GRETH2_RXD_STS_ST ( 1 << GRETH2_RXD_STS_ST_BIT )
#define GRETH2_RXD_STS_LE ( 1 << GRETH2_RXD_STS_LE_BIT )
#define GRETH2_RXD_STS_OE ( 1 << GRETH2_RXD_STS_OE_BIT )
#define GRETH2_RXD_STS_CE ( 1 << GRETH2_RXD_STS_CE_BIT )
#define GRETH2_RXD_STS_FT ( 1 << GRETH2_RXD_STS_FT_BIT )
#define GRETH2_RXD_STS_AE ( 1 << GRETH2_RXD_STS_AE_BIT )
#define GRETH2_RXD_STS_MC ( 1 << GRETH2_RXD_STS_MC_BIT )
#define GRETH2_RXD_STS_ID ( 1 << GRETH2_RXD_STS_ID_BIT )
#define GRETH2_RXD_STS_IR ( 1 << GRETH2_RXD_STS_IR_BIT )
#define GRETH2_RXD_STS_UD ( 1 << GRETH2_RXD_STS_UD_BIT )
#define GRETH2_RXD_STS_UR ( 1 << GRETH2_RXD_STS_UR_BIT )
#define GRETH2_RXD_STS_TD ( 1 << GRETH2_RXD_STS_TD_BIT )
#define GRETH2_RXD_STS_TR ( 1 << GRETH2_RXD_STS_TR_BIT )
#define GRETH2_RXD_STS_IF ( 1 << GRETH2_RXD_STS_IF_BIT )

#define GRETH2_RXD_STS_ERR                                      \
  ( GRETH2_RXD_STS_LE | GRETH2_RXD_STS_OE | GRETH2_RXD_STS_CE | \
    GRETH2_RXD_STS_FT | GRETH2_RXD_STS_AE )

#define GRETH2_RXD_STS_CSERR \
  ( GRETH2_RXD_STS_IR | GRETH2_RXD_STS_UR | GRETH2_RXD_STS_TR )

#define GRETH2_RXD_STS_LEN_MASK 0x3fff /**< Mask for length field (13:0) */

/**< CTRL Register */
#define GRETH2_CTRL_TS_BIT    15 /**< Timestamp enable */
#define GRETH2_CTRL_DD_BIT    12 /**< Disable duplex detection */
#define GRETH2_CTRL_ME_BIT    11 /**< Multicast Enable */
#define GRETH2_CTRL_PI_BIT    10 /**< PHY status change IRQ */
#define GRETH2_CTRL_GB_BIT    8  /**< Gigabit mode */
#define GRETH2_CTRL_BM_BIT    9  /**< Burst mode */
#define GRETH2_CTRL_SP_BIT    7  /**< Speed mode */
#define GRETH2_CTRL_RST_BIT   6  /**< Reset MAC */
#define GRETH2_CTRL_PM_BIT    5  /**< Promiscuous (receive all) */
#define GRETH2_CTRL_FD_BIT    4  /**< Full Duplex */
#define GRETH2_CTRL_RXIRQ_BIT 3  /**< Receive Enable  */
#define GRETH2_CTRL_TXIRQ_BIT 2  /**< Transmit Enable */
#define GRETH2_CTRL_RXEN_BIT  1  /**< Receive Enable  */
#define GRETH2_CTRL_TXEN_BIT  0  /**< Transmit Enable */

#define GRETH2_CTRL_TS    ( 1 << GRETH2_CTRL_TS_BIT )
#define GRETH2_CTRL_DD    ( 1 << GRETH2_CTRL_DD_BIT )
#define GRETH2_CTRL_ME    ( 1 << GRETH2_CTRL_ME_BIT )
#define GRETH2_CTRL_PI    ( 1 << GRETH2_CTRL_PI_BIT )
#define GRETH2_CTRL_BM    ( 1 << GRETH2_CTRL_BM_BIT )
#define GRETH2_CTRL_GB    ( 1 << GRETH2_CTRL_GB_BIT )
#define GRETH2_CTRL_SP    ( 1 << GRETH2_CTRL_SP_BIT )
#define GRETH2_CTRL_RST   ( 1 << GRETH2_CTRL_RST_BIT )
#define GRETH2_CTRL_PM    ( 1 << GRETH2_CTRL_PM_BIT )
#define GRETH2_CTRL_FD    ( 1 << GRETH2_CTRL_FD_BIT )
#define GRETH2_CTRL_RXIRQ ( 1 << GRETH2_CTRL_RXIRQ_BIT )
#define GRETH2_CTRL_TXIRQ ( 1 << GRETH2_CTRL_TXIRQ_BIT )
#define GRETH2_CTRL_RXEN  ( 1 << GRETH2_CTRL_RXEN_BIT )
#define GRETH2_CTRL_TXEN  ( 1 << GRETH2_CTRL_TXEN_BIT )

/**< Status Register */
#define GRETH2_STATUS_PS_BIT       8 /**< PHY Status change */
#define GRETH2_STATUS_IA_BIT       7 /**< Invalid address */
#define GRETH2_STATUS_TS_BIT       6 /**< Too small */
#define GRETH2_STATUS_TXAHBERR_BIT 5 /**< Transmitter AHB Error */
#define GRETH2_STATUS_RXAHBERR_BIT 4 /**< Receiver AHB Error */
#define GRETH2_STATUS_TXIRQ_BIT    3 /**< Transmit Error IRQ */
#define GRETH2_STATUS_RXIRQ_BIT    2 /**< Receive Frame IRQ */
#define GRETH2_STATUS_TXERR_BIT    1 /**< Transmit Error IRQ */
#define GRETH2_STATUS_RXERR_BIT    0 /**< Receive Error */

#define GRETH2_STATUS_PS       ( 1 << GRETH2_STATUS_PS_BIT )
#define GRETH2_STATUS_IA       ( 1 << GRETH2_STATUS_IA_BIT )
#define GRETH2_STATUS_TS       ( 1 << GRETH2_STATUS_TS_BIT )
#define GRETH2_STATUS_TXAHBERR ( 1 << GRETH2_STATUS_TXAHBERR_BIT )
#define GRETH2_STATUS_RXAHBERR ( 1 << GRETH2_STATUS_RXAHBERR_BIT )
#define GRETH2_STATUS_TXIRQ    ( 1 << GRETH2_STATUS_TXIRQ_BIT )
#define GRETH2_STATUS_RXIRQ    ( 1 << GRETH2_STATUS_RXIRQ_BIT )
#define GRETH2_STATUS_TXERR    ( 1 << GRETH2_STATUS_TXERR_BIT )
#define GRETH2_STATUS_RXERR    ( 1 << GRETH2_STATUS_RXERR_BIT )

/**< MDIO Control  */
#define GRETH2_MDIO_DATA_BIT     16
#define GRETH2_MDIO_PHYADDR_BIT  11
#define GRETH2_MDIO_REGADDR_BIT  6
#define GRETH2_MDIO_BUSY_BIT     3
#define GRETH2_MDIO_LINKFAIL_BIT 2
#define GRETH2_MDIO_READ_BIT     1
#define GRETH2_MDIO_WRITE_BIT    0

#define GRETH2_MDIO_DATA_MASK    0xffffU
#define GRETH2_MDIO_PHYADDR_MASK 0x1fU
#define GRETH2_MDIO_REGADDR_MASK 0x1fU

#define GRETH2_MDIO_BUSY     ( 1U << GRETH2_MDIO_BUSY_BIT )
#define GRETH2_MDIO_LINKFAIL ( 1U << GRETH2_MDIO_LINKFAIL_BIT )
#define GRETH2_MDIO_READ     ( 1U << GRETH2_MDIO_READ_BIT )
#define GRETH2_MDIO_WRITE    ( 1U << GRETH2_MDIO_WRITE_BIT )

/**< Capabilities Register */

#define GRETH2_CAP_EXT_MDIO_BIT 5
#define GRETH2_CAP_MC_BIT       2
#define GRETH2_CAP_IRQ_MDIO_BIT 1
#define GRETH2_CAP_TS_BIT       0

#define GRETH2_CAP_EXT_MDIO ( 1 << GRETH2_CAP_EXT_MDIO_BIT )
#define GRETH2_CAP_MC       ( 1 << GRETH2_CAP_MC_BIT )
#define GRETH2_CAP_IRQ_MDIO ( 1 << GRETH2_CAP_IRQ_MDIO_BIT )
#define GRETH2_CAP_TS       ( 1 << GRETH2_CAP_TS_BIT )

#endif
