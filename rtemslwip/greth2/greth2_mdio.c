/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth2_mdio.c
 * @brief MDIO (Management Data Input/Output) interface functions for GRETH2.
 *
 * This file implements functions to read and write PHY registers
 * via the MDIO interface for the GRETH2 Ethernet driver.
 * It provides blocking access to PHY registers and handles
 * basic error reporting on failed reads.
 *
 * Main functionalities:
 * - Reading a PHY register using `greth2_phy_reg_read()`.
 * - Writing a PHY register using `greth2_phy_reg_write()`.
 *
 * @ingroup GRETH2
 */

/*
 * Copyright (C) 2026 Frontgrade Gaisler

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

#include "greth2_emac.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rtems/dev/io.h>

/**
 * @brief   Reads a PHY register using MDIO.
 *
 * @param   baseAddr      Base Address of the MDIO Module Registers.
 * @param   phy_addr       PHY Address.
 * @param   reg_addr        Register Number to be read.
 * @param   data       Pointer where the read value shall be written.
 *
 * @retval TRUE read successful.
 * @retval FALSE read unsuccessful.
 *
 **/
static err_t greth2_int_phy_reg_read(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  uint32_t                   reg_addr,
  uint16_t                  *data
);

/**
 * @brief   Writes a PHY register using MDIO.
 *
 * @param   baseAddr      Base Address of the MDIO Module Registers.
 * @param   phy_addr       PHY Address.
 * @param   reg_addr        Register Number to be read.
 * @param   value        Value to be written.
 *
 **/
static err_t greth2_int_phy_reg_write(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  uint32_t                   reg_addr,
  uint16_t                   value
);

static inline void greth2_phy_wait_for_mdio_ready(
  struct greth2_netif_state *greth2_device
)
{
  while ( greth2_device->regs->mdio_ctrl & GRETH2_MDIO_BUSY ) {
    _IO_Relax();
  }
}

static err_t greth2_int_phy_reg_read(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  uint32_t                   reg_addr,
  uint16_t                  *data
)
{
  if ( phy_addr > 31 || reg_addr > 31 ) {
    return ERR_IF;
  }

  greth2_phy_wait_for_mdio_ready( greth2_device );
  greth2_device->regs->mdio_ctrl = ( ( phy_addr & GRETH2_MDIO_PHYADDR_MASK )
                                     << GRETH2_MDIO_PHYADDR_BIT ) |
                                   ( ( reg_addr & GRETH2_MDIO_REGADDR_MASK )
                                     << GRETH2_MDIO_REGADDR_BIT ) |
                                   GRETH2_MDIO_READ;
  greth2_phy_wait_for_mdio_ready( greth2_device );

  if ( greth2_device->regs->mdio_ctrl & GRETH2_MDIO_LINKFAIL ) {
    return ERR_IF;
  }

  *data = ( greth2_device->regs->mdio_ctrl >> GRETH2_MDIO_DATA_BIT ) &
          GRETH2_MDIO_DATA_MASK;

  return ERR_OK;
}

static err_t greth2_int_phy_reg_write(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  uint32_t                   reg_addr,
  uint16_t                   value
)
{
  if ( phy_addr > 31 || reg_addr > 31 ) {
    return ERR_IF;
  }

  greth2_phy_wait_for_mdio_ready( greth2_device );
  greth2_device->regs->mdio_ctrl =
    ( ( value << GRETH2_MDIO_DATA_BIT ) |
      ( ( phy_addr & GRETH2_MDIO_PHYADDR_MASK ) << GRETH2_MDIO_PHYADDR_BIT ) |
      ( ( reg_addr & GRETH2_MDIO_REGADDR_MASK ) << GRETH2_MDIO_REGADDR_BIT ) |
      GRETH2_MDIO_WRITE );
  greth2_phy_wait_for_mdio_ready( greth2_device );

  if ( greth2_device->regs->mdio_ctrl & GRETH2_MDIO_LINKFAIL ) {
    return ERR_IF;
  }

  return ERR_OK;
}

struct greth2_phy_ops greth2_int_phy_ops = {
  .read = greth2_int_phy_reg_read,
  .write = greth2_int_phy_reg_write,
};
