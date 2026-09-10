/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth2_emac.h
 * @brief GRETH2 Ethernet MAC (EMAC) 2 structures and utility functions.
 *
 * This header defines the GRETH2 EMAC buffer descriptor structure (greth2_bd),
 * hardware register mapping (greth2_regs), PHY device information
 * (phy_device_info), and the GRETH2 device runtime state (greth2_netif_state)
 *
 * It also provides inline utility functions such as greth2_set_mac_addr to
 * configure the MAC hardware address.
 *
 * @ingroup GRETH2
 */

/*
 * Copyright (C) 2026 Frontgrade Gaisler AB
 *
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

#ifndef GRETH2_EMAC_H
#define GRETH2_EMAC_H

#include <stdio.h>

#include "greth2.h"
#include "greth2_hw_cfg.h"
#include "greth2_netif.h"
#include "greth2_phy.h"
#include "lwip/sys.h"
#include <rtems/rtems/intr.h>
#include <stdbool.h>

#if GRETH2_DEBUG
#define greth2_debug_printf( ... ) iprintf( __VA_ARGS__ )
#else
#define greth2_debug_printf( ... )
#endif

/**
 * @struct greth2_bd
 * @brief Ethernet MAC Buffer Descriptor structure.
 *
 * This structure represents a buffer descriptor used by the GRETH2 Ethernet
 * hardware. Each descriptor contains a control field and a pointer/address
 * to the data buffer. The structure is aligned to 8 bytes for proper
 * hardware access.
 */
struct greth2_bd {
  volatile uint32_t ctrl;
  volatile uint32_t status;
  volatile uint32_t addr_msb;
  volatile uint32_t addr_lsb;
  volatile uint64_t timestamp;
  volatile uint32_t reserved[ 2 ];
} __attribute__(( aligned( 8 ) ));

/**
 * @struct greth2_regs
 * @brief GRETH2 EMAC Configuratin Registers
 */
struct greth2_regs {
  volatile uint32_t ctrl;
  volatile uint32_t status;
  volatile uint32_t mac_addr_msb;
  volatile uint32_t mac_addr_lsb;
  volatile uint32_t mdio_ctrl;
  volatile uint32_t txdesc_msb;
  volatile uint32_t txdesc_lsb;
  volatile uint32_t reserved[ 2 ];
  volatile uint32_t rxdesc_msb;
  volatile uint32_t rxdesc_lsb;
  volatile uint32_t edcl_ip;
  volatile uint64_t hash_table;
  volatile uint32_t edcl_mac_addr_msb;
  volatile uint32_t edcl_mac_addr_lsb;
  volatile uint32_t reserved1;
  volatile uint32_t capabilities;
};

/**
 * @brief GRETH2 hardware capabilities
 *
 * This structure represents the capabilities of the GRETH2 Ethernet MAC
 * hardware, as reported by the capabilities register.
 *
 * @param extern_mdio Set to true when GRETH2 is configured to use an external
 * MDIO controller. The MDIO related functions built into the GRETH2 will not be
 * available.
 * @param multicast Set to true when the core supports multicast address
 * reception.
 * @param mdio_irq Set to true when the core supports MDIO interrupts.
 * @param timestamping Set to true when the core supports timestamps.
 *
 */
struct greth2_capabilities {
  bool extern_mdio;
  bool multicast;
  bool mdio_irq;
  bool timestamping;
};

/**
 * @struct phy_device_info
 * @brief PHY device information and capabilities
 */
struct phy_device_info {
  unsigned int vendor; /**< Organizationally Unique Identifier */
  unsigned int device; /**< Manufacturer’s Model Number */
  unsigned int rev;    /**< Revision number */

  uint16_t adv;  /**< Advertised capabilities */
  uint16_t part; /**< Part number */

  uint16_t extadv;  /**< Extended advertised capabilities */
  uint16_t extpart; /**< Extended part number */

  uint16_t phyStatus; /**< Current PHY status register */
  uint16_t phyCtrl;   /**< PHY control register */
  bool     autoneg;   /**< Autonegotiation enabled/disabled */
};

struct tx_comm_chn {
  struct greth2_bd *desc_array;  /**< TX descriptors */
  unsigned int      head;        /**< Next TX descriptor to queue */
  unsigned int      tail;        /**< Next TX descriptor to check */
  struct pbuf     **tx_pbuf_ref; /**< TX pbuf pointers */
};

struct rx_comm_chn {
  struct greth2_bd *desc_array;  /**< RX descriptors */
  unsigned int      head;        /**< Next RX descriptor to store */
  unsigned int      tail;        /**< Next RX descriptor to check */
  struct pbuf     **rx_pbuf_ref; /**< RX pbuf pointers */
};

/**
 * @struct greth2_netif_state
 * @brief GRETH2 device runtime state and configuration
 */
struct greth2_netif_state {
  unsigned int greth2_id; /**< GRETH2 device instance number */

  struct greth2_regs    *regs;          /**< GRETH2 MAC registers */
  struct mdio_ctrl_regs *ext_mdio_regs; /**< MDIO controller registers */

  sys_sem_t irq_sem; /**< Interrupt semaphore */

  /**
   * @struct tx_comm_chn
   * @brief Transmit channel state
   */
  struct tx_comm_chn tx_channel;

  /**
   * @struct rx_comm_chn
   * @brief Receive channel state
   */
  struct rx_comm_chn rx_channel;

  unsigned int        curr_bd_in_use;  /**< TX descriptors in use */
  rtems_vector_number vector;          /**< Interrupt vector */
  rtems_vector_number ext_mdio_vector; /**< Interrupt vector */

  struct phy_device_info phy_devs[ 32 ]; /**< PHY info */
  uint32_t               reset_phy_addr; /**< Reset PHY address */
  struct greth2_phy_ops *phy_ops;        /**< PHY operations */
  bool                   fd;             /**< Full duplex enabled */
  bool                   sp;             /**< Speed: 100=1, 10=0 */
  bool                   gb;             /**< Gigabit mode available */
  bool                   auto_neg;       /**< Autonegotiation enabled */
  struct timespec        auto_neg_time;  /**< Autonegotiation time */

  struct greth2_capabilities caps;

  unsigned int irq_treshold;
  bool         incoming_irq;
};

/***************************EMAC Specific fns******************** */
/**
 * @brief Set the GRETH2 MAC hardware address
 *
 * This function programs the GRETH2 MAC registers with the provided 6-byte MAC
 * address.
 *
 * @param greth2_dev Pointer to the GRETH2 device state structure
 * @param macAddr   Pointer to a 6-byte MAC address array
 *
 * @returns void
 */
static inline void greth2_set_mac_addr(
  struct greth2_netif_state *greth2_dev,
  uint8_t                   *mac_addr
)
{
  greth2_dev->regs->mac_addr_msb = ( mac_addr[ 0 ] << 8 ) | mac_addr[ 1 ];
  greth2_dev->regs->mac_addr_lsb = ( mac_addr[ 2 ] << 24 ) |
                                   ( mac_addr[ 3 ] << 16 ) |
                                   ( mac_addr[ 4 ] << 8 ) | mac_addr[ 5 ];
  greth2_debug_printf(
    "GRETH2 MAC MSB : %x\n",
    greth2_dev->regs->mac_addr_msb
  );
  greth2_debug_printf(
    "GRETH2 MAC LSB : %x\n",
    greth2_dev->regs->mac_addr_lsb
  );
}

#endif /* GRETH2_EMAC_H */
