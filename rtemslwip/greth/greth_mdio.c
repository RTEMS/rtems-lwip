/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth_mdio.c
 * @brief MDIO (Management Data Input/Output) interface functions for GRETH.
 *
 * This file implements functions to read and write PHY registers
 * via the MDIO interface for the GRETH Ethernet driver.
 * It provides blocking access to PHY registers and handles
 * basic error reporting on failed reads.
 *
 * Main functionalities:
 * - Reading a PHY register using `greth_phy_reg_read()`.
 * - Writing a PHY register using `greth_phy_reg_write()`.
 *
 * @ingroup GRETH
 */

/*
 * Copyright (C) 2025 Prithvi Tambewagh

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

#include "greth_emac.h"
#include "greth_mdio.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rtems/dev/io.h>

static inline void greth_phy_wait_for_mdio_ready(
  struct greth_netif_state *greth_device
)
{
  while ( greth_device->regs->mdio_ctrl & GRETH_MDIO_BUSY ) {
    _IO_Relax();
  }
}

err_t greth_phy_reg_read(
  struct greth_netif_state *greth_device,
  uint32_t                  phy_addr,
  uint32_t                  reg_addr,
  uint16_t                 *data
)
{
  greth_phy_wait_for_mdio_ready( greth_device );
  greth_device->regs->mdio_ctrl = ( phy_addr << GRETH_MDIO_PHYADR_BIT ) |
                                  ( reg_addr << GRETH_MDIO_REGADR_BIT ) |
                                  GRETH_MDIO_READ;
  greth_phy_wait_for_mdio_ready( greth_device );

  if ( greth_device->regs->mdio_ctrl & GRETH_MDIO_LINKFAIL ) {
    return ERR_IF;
  }

  *data = ( greth_device->regs->mdio_ctrl & GRETH_MDIO_DATA ) >>
          GRETH_MDIO_DATA_BIT;

  return ERR_OK;
}

err_t greth_phy_reg_write(
  struct greth_netif_state *greth_device,
  uint32_t                  phy_addr,
  uint32_t                  reg_addr,
  uint16_t                  value
)
{
  greth_phy_wait_for_mdio_ready( greth_device );
  greth_device->regs->mdio_ctrl =
    ( ( value << GRETH_MDIO_DATA_BIT ) |
      ( phy_addr << GRETH_MDIO_PHYADR_BIT ) |
      ( reg_addr << GRETH_MDIO_REGADR_BIT ) | GRETH_MDIO_WRITE );
  greth_phy_wait_for_mdio_ready( greth_device );

  if ( greth_device->regs->mdio_ctrl & GRETH_MDIO_LINKFAIL ) {
    return ERR_IF;
  }

  return ERR_OK;
}
