/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth_emac.h
 * @brief GRETH Ethernet MAC (EMAC) structures and utility functions.
 *
 * This header defines the GRETH EMAC buffer descriptor structure (greth_bd),
 * hardware register mapping (greth_regs), PHY device information 
 * (phy_device_info), and the GRETH device runtime state (greth_netif_state)
 * 
 * It also provides inline utility functions such as EMACMACSrcAddrSet to
 * configure the MAC hardware address.
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

#ifndef GRETH_EMAC_H
#define GRETH_EMAC_H

#include "greth_netif.h"
#include "greth.h"
#include "greth_phy.h"
#include "lwip/sys.h"
#include <stdbool.h>
#include <rtems/rtems/intr.h>

/**
 * @struct greth_bd
 * @brief Ethernet MAC Buffer Descriptor structure.
 *
 * This structure represents a buffer descriptor used by the GRETH Ethernet
 * hardware. Each descriptor contains a control field and a pointer/address
 * to the data buffer. The structure is aligned to 8 bytes for proper
 * hardware access.
 */
struct greth_bd {
  volatile uint32_t ctrl; /**< Control field of Register */
  volatile uint32_t
    addr; /**< Address of payload field of pbuf assigned to BD*/
} __attribute__(( aligned( 8 ) ));

/** 
 * @struct greth_regs
 * @brief GRETH EMAC Configuration Registers 
 */
struct greth_regs {
  volatile uint32_t ctrl;         /**< Control Register */
  volatile uint32_t status;       /**< Status Register */
  volatile uint32_t mac_addr_msb; /**< MSB i.e. Bit 47-32 of MAC address */
  volatile uint32_t mac_addr_lsb; /**< LSB i.e. Bit 31-0 of MAC address */
  volatile uint32_t mdio_ctrl;    /**< MDIO control and status register */
  volatile uint32_t txdesc;       /**< First Transmit descriptor pointer */
  volatile uint32_t rxdesc;       /**< First Receive descriptor pointer */
};

/** 
 * @struct phy_device_info
 * @brief PHY device information and capabilities
 */
struct phy_device_info {
  unsigned int vendor; /**< Vendor identifier */
  unsigned int device; /**< Device identifier */
  unsigned int rev;    /**< Revision number */

  uint16_t adv;  /**< Advertised capabilities */
  uint16_t part; /**< Part number */

  uint16_t extadv;  /**< Extended advertised capabilities */
  uint16_t extpart; /**< Extended part number */

  uint16_t phyAddr;   /**< PHY address on the MDIO bus */
  uint16_t phyStatus; /**< Current PHY status register */
  uint16_t phyCtrl;   /**< PHY control register */
  bool     autoneg;   /**< Autonegotiation enabled/disabled */
};

/** 
 * @struct rx_comm_chn
 * @brief Receive channel state
 */
struct rx_comm_chn {
  struct greth_bd *desc_array;     /**< RX descriptors */
  unsigned int     head;           /**< Next RX descriptor to store */
  unsigned int     tail;           /**< Next RX descriptor to check */
  u32_t            freed_pbuf_len; /**< Freed pbuf length */
  struct pbuf    **rx_pbuf_ref;    /**< RX pbuf pointers */
};

/** 
 * @struct tx_comm_chn
 * @brief Transmit channel state
 */
struct tx_comm_chn {
  struct greth_bd *desc_array;  /**< TX descriptors */
  unsigned int     head;        /**< Next TX descriptor to queue */
  unsigned int     tail;        /**< Next TX descriptor to check */
  struct pbuf    **tx_pbuf_ref; /**< TX pbuf pointers */
};

/** 
 * @struct greth_netif_state
 * @brief GRETH device runtime state and configuration
 */
struct greth_netif_state {
  unsigned int        greth_id; /**< Device instance number */
  rtems_vector_number vector;   /**< Interrupt vector */

  unsigned int num_bd; /**< Number of TX descriptors */

  struct greth_regs *regs;    /**< GRETH MAC registers */
  sys_sem_t          irq_sem; /**< Interrupt semaphore */

  struct tx_comm_chn tx_channel;
  struct rx_comm_chn rx_channel;

  unsigned int     curr_bd_in_use; /**< TX descriptors in use */
  struct greth_bd *txdesc;         /**< TX descriptor array */
  struct greth_bd *rxdesc;         /**< RX descriptor array */

  struct phy_device_info phy_dev;       /**< PHY info */
  bool                   fd;            /**< Full duplex enabled */
  bool                   sp;            /**< Speed: 100=1, 10=0 */
  bool                   gb;            /**< Gigabit mode available */
  bool                   multicast;     /**< MAC supports Multicast */
  bool                   gbit_mac;      /**< MAC supports Gigabit */
  bool                   auto_neg;      /**< Autonegotiation enabled */
  struct timespec        auto_neg_time; /**< Autonegotiation time */

  /** @brief Statistics */

  unsigned int irq_treshold;
  bool         incoming_irq;
  err_t ( *process_irq_tx )( void *arg );
};
/***************************EMAC Specific fns******************** */
/**
 * @brief Set the GRETH MAC hardware address
 * 
 * This function programs the GRETH MAC registers with the provided 6-byte MAC 
 * address.
 * 
 * @param greth_dev Pointer to the GRETH device state structure
 * @param mac_addr   Pointer to a 6-byte MAC address array
 * 
 * @returns void
 */
static inline void greth_set_mac_addr(
  struct greth_netif_state *greth_dev,
  uint8_t                  *mac_addr
)
{
  greth_dev->regs->mac_addr_msb = ( mac_addr[ 0 ] << 8 ) | mac_addr[ 1 ];
  greth_dev->regs->mac_addr_lsb = ( mac_addr[ 2 ] << 24 ) |
                                  ( mac_addr[ 3 ] << 16 ) |
                                  ( mac_addr[ 4 ] << 8 ) | mac_addr[ 5 ];
  greth_debug_printf( "GRETH MAC MSB : %x\n", greth_dev->regs->mac_addr_msb );
  greth_debug_printf( "GRETH MAC LSB : %x\n", greth_dev->regs->mac_addr_lsb );
}

#endif /* GRETH_EMAC_H */
