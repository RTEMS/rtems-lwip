/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth.h
 * @brief GRETH (Gigabit Ethernet) driver definitions
 *
 * This header file contains the register definitions, bit masks,
 * and data structures required for the GRETH Ethernet driver.
 *
 * @ingroup GRETH
 */

/*
 * Copyright (C) 2025 Prithvi Tambewagh
 * Copyright (C) 2025 Frontgrade Gaisler AB

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

#include "greth_emac.h"
#include "lwip/def.h"

struct greth_bd;

/**< Configuration Information */

typedef struct {
  void               *base_address;
  rtems_vector_number vector;
  uint32_t            txd_count;
  uint32_t            rxd_count;
} greth_configuration_t;

#define GRETH_TOTAL_BD   128
#define GRETH_MAXBUF_LEN 1520

/**< Tx BD */
#define GRETH_TXD_LEN_BIT      0
#define GRETH_TXD_ENABLE_BIT   11
#define GRETH_TXD_WRAP_BIT     12
#define GRETH_TXD_IRQ_BIT      13
#define GRETH_TXD_UNDERRUN_BIT 14
#define GRETH_TXD_RETLIM_BIT   15
#define GRETH_TXD_LATECOL_BIT  16
#define GRETH_TXD_MORE_BIT     17
#define GRETH_TXD_IPCS_BIT     18
#define GRETH_TXD_TCPCS_BIT    19
#define GRETH_TXD_UDPCS_BIT    20

#define GRETH_TXD_LEN      ( 0x7FF << GRETH_TXD_LEN_BIT )
#define GRETH_TXD_ENABLE   ( 1 << GRETH_TXD_ENABLE_BIT )
#define GRETH_TXD_WRAP     ( 1 << GRETH_TXD_WRAP_BIT )
#define GRETH_TXD_IRQ      ( 1 << GRETH_TXD_IRQ_BIT )
#define GRETH_TXD_UNDERRUN ( 1 << GRETH_TXD_UNDERRUN_BIT )
#define GRETH_TXD_RETLIM   ( 1 << GRETH_TXD_RETLIM_BIT )
#define GRETH_TXD_LATECOL  ( 1 << GRETH_TXD_LATECOL_BIT )
#define GRETH_TXD_MORE     ( 1 << GRETH_TXD_MORE_BIT )
#define GRETH_TXD_IPCS     ( 1 << GRETH_TXD_IPCS_BIT )
#define GRETH_TXD_TCPCS    ( 1 << GRETH_TXD_TCPCS_BIT )
#define GRETH_TXD_UDPCS    ( 1 << GRETH_TXD_UDPCS_BIT )

#define GRETH_TXD_ERR \
  ( GRETH_TXD_UNDERRUN | GRETH_TXD_RETLIM | GRETH_TXD_LATECOL )

#define GRETH_TXD_CS ( GRETH_TXD_IPCS | GRETH_TXD_TCPCS | GRETH_TXD_UDPCS )

/**< Rx BD */
#define GRETH_RXD_LEN_BIT     0
#define GRETH_RXD_ENABLE_BIT  11
#define GRETH_RXD_WRAP_BIT    12
#define GRETH_RXD_IRQ_BIT     13
#define GRETH_RXD_DRIBBLE_BIT 14
#define GRETH_RXD_TOOLONG_BIT 15
#define GRETH_RXD_CRCERR_BIT  16
#define GRETH_RXD_OVERRUN_BIT 17
#define GRETH_RXD_LENERR_BIT  18
#define GRETH_RXD_ID_BIT      19
#define GRETH_RXD_IR_BIT      20
#define GRETH_RXD_UD_BIT      21
#define GRETH_RXD_UR_BIT      22
#define GRETH_RXD_TD_BIT      23
#define GRETH_RXD_TR_BIT      24
#define GRETH_RXD_IF_BIT      25
#define GRETH_RXD_MC_BIT      26

#define GRETH_RXD_LEN     ( 0x7FF << GRETH_RXD_LEN_BIT )
#define GRETH_RXD_ENABLE  ( 1 << GRETH_RXD_ENABLE_BIT )
#define GRETH_RXD_WRAP    ( 1 << GRETH_RXD_WRAP_BIT )
#define GRETH_RXD_IRQ     ( 1 << GRETH_RXD_IRQ_BIT )
#define GRETH_RXD_DRIBBLE ( 1 << GRETH_RXD_DRIBBLE_BIT )
#define GRETH_RXD_TOOLONG ( 1 << GRETH_RXD_TOOLONG_BIT )
#define GRETH_RXD_CRCERR  ( 1 << GRETH_RXD_CRCERR_BIT )
#define GRETH_RXD_OVERRUN ( 1 << GRETH_RXD_OVERRUN_BIT )
#define GRETH_RXD_LENERR  ( 1 << GRETH_RXD_LENERR_BIT )
#define GRETH_RXD_ID      ( 1 << GRETH_RXD_ID_BIT )
#define GRETH_RXD_IR      ( 1 << GRETH_RXD_IR_BIT )
#define GRETH_RXD_UD      ( 1 << GRETH_RXD_UD_BIT )
#define GRETH_RXD_UR      ( 1 << GRETH_RXD_UR_BIT )
#define GRETH_RXD_TD      ( 1 << GRETH_RXD_TD_BIT )
#define GRETH_RXD_TR      ( 1 << GRETH_RXD_TR_BIT )
#define GRETH_RXD_IF      ( 1 << GRETH_RXD_IF_BIT )
#define GRETH_RXD_MC      ( 1 << GRETH_RXD_MC_BIT )

#define GRETH_RXD_ERR                                           \
  ( GRETH_RXD_OVERRUN | GRETH_RXD_DRIBBLE | GRETH_RXD_TOOLONG | \
    GRETH_RXD_CRCERR | GRETH_RXD_LENERR )

#define GRETH_RXD_CSERR ( GRETH_RXD_IR | GRETH_RXD_UR | GRETH_RXD_TR )

/**< CTRL Register */
#define GRETH_CTRL_TXEN_BIT  0
#define GRETH_CTRL_RXEN_BIT  1
#define GRETH_CTRL_TXIRQ_BIT 2
#define GRETH_CTRL_RXIRQ_BIT 3
#define GRETH_CTRL_FD_BIT    4
#define GRETH_CTRL_PRO_BIT   5
#define GRETH_CTRL_RST_BIT   6
#define GRETH_CTRL_SP_BIT    7
#define GRETH_CTRL_GB_BIT    8
#define GRETH_CTRL_BM_BIT    9
#define GRETH_CTRL_PI_BIT    10
#define GRETH_CTRL_ME_BIT    11
#define GRETH_CTRL_DD_BIT    12
#define GRETH_CTRL_RD_BIT    13
#define GRETH_CTRL_ED_BIT    14
#define GRETH_CTRL_TS_BIT    15
#define GRETH_CTRL_TC_BIT    23
#define GRETH_CTRL_MC_BIT    25
#define GRETH_CTRL_MI_BIT    26
#define GRETH_CTRL_GA_BIT    27

#define GRETH_CTRL_TXEN  ( 1 << GRETH_CTRL_TXEN_BIT )
#define GRETH_CTRL_RXEN  ( 1 << GRETH_CTRL_RXEN_BIT )
#define GRETH_CTRL_TXIRQ ( 1 << GRETH_CTRL_TXIRQ_BIT )
#define GRETH_CTRL_RXIRQ ( 1 << GRETH_CTRL_RXIRQ_BIT )
#define GRETH_CTRL_FD    ( 1 << GRETH_CTRL_FD_BIT )
#define GRETH_CTRL_PRO   ( 1 << GRETH_CTRL_PRO_BIT )
#define GRETH_CTRL_RST   ( 1 << GRETH_CTRL_RST_BIT )
#define GRETH_CTRL_SP    ( 1 << GRETH_CTRL_SP_BIT )
#define GRETH_CTRL_GB    ( 1 << GRETH_CTRL_GB_BIT )
#define GRETH_CTRL_BM    ( 1 << GRETH_CTRL_BM_BIT )
#define GRETH_CTRL_PI    ( 1 << GRETH_CTRL_PI_BIT )
#define GRETH_CTRL_ME    ( 1 << GRETH_CTRL_ME_BIT )
#define GRETH_CTRL_DD    ( 1 << GRETH_CTRL_DD_BIT )
#define GRETH_CTRL_RD    ( 1 << GRETH_CTRL_RD_BIT )
#define GRETH_CTRL_ED    ( 1 << GRETH_CTRL_ED_BIT )
#define GRETH_CTRL_TS    ( 1 << GRETH_CTRL_TS_BIT )
#define GRETH_CTRL_TC    ( 1 << GRETH_CTRL_TC_BIT )
#define GRETH_CTRL_MC    ( 1 << GRETH_CTRL_MC_BIT )
#define GRETH_CTRL_MI    ( 1 << GRETH_CTRL_MI_BIT )
#define GRETH_CTRL_GA    ( 1 << GRETH_CTRL_GA_BIT )

/**< Status Register */
#define GRETH_STATUS_RXERR_BIT    0
#define GRETH_STATUS_TXERR_BIT    1
#define GRETH_STATUS_RXIRQ_BIT    2
#define GRETH_STATUS_TXIRQ_BIT    3
#define GRETH_STATUS_RXAHBERR_BIT 4
#define GRETH_STATUS_TXAHBERR_BIT 5
#define GRETH_STATUS_TS_BIT       6
#define GRETH_STATUS_IA_BIT       7
#define GRETH_STATUS_PS_BIT       8

#define GRETH_STATUS_RXERR    ( 1 << GRETH_STATUS_RXERR_BIT )
#define GRETH_STATUS_TXERR    ( 1 << GRETH_STATUS_TXERR_BIT )
#define GRETH_STATUS_RXIRQ    ( 1 << GRETH_STATUS_RXIRQ_BIT )
#define GRETH_STATUS_TXIRQ    ( 1 << GRETH_STATUS_TXIRQ_BIT )
#define GRETH_STATUS_RXAHBERR ( 1 << GRETH_STATUS_RXAHBERR_BIT )
#define GRETH_STATUS_TXAHBERR ( 1 << GRETH_STATUS_TXAHBERR_BIT )
#define GRETH_STATUS_TS       ( 1 << GRETH_STATUS_TS_BIT )
#define GRETH_STATUS_IA       ( 1 << GRETH_STATUS_IA_BIT )
#define GRETH_STATUS_PS       ( 1 << GRETH_STATUS_PS_BIT )

/**< MDIO Control  */
#define GRETH_MDIO_WRITE_BIT    0
#define GRETH_MDIO_READ_BIT     1
#define GRETH_MDIO_LINKFAIL_BIT 2
#define GRETH_MDIO_BUSY_BIT     3
#define GRETH_MDIO_REGADR_BIT   6
#define GRETH_MDIO_PHYADR_BIT   11
#define GRETH_MDIO_DATA_BIT     16

#define GRETH_MDIO_WRITE    ( 1 << GRETH_MDIO_WRITE_BIT )
#define GRETH_MDIO_READ     ( 1 << GRETH_MDIO_READ_BIT )
#define GRETH_MDIO_LINKFAIL ( 1 << GRETH_MDIO_LINKFAIL_BIT )
#define GRETH_MDIO_BUSY     ( 1 << GRETH_MDIO_BUSY_BIT )
#define GRETH_MDIO_REGADR   ( 0x1F << GRETH_MDIO_REGADR_BIT )
#define GRETH_MDIO_PHYADR   ( 0x1F << GRETH_MDIO_PHYADR_BIT )
#define GRETH_MDIO_DATA     ( 0xFFFF << GRETH_MDIO_DATA_BIT )

#endif
