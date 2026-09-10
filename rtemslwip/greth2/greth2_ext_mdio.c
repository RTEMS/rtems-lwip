/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth2_ext_mdio.c
 * @brief MDIO (Management Data Input/Output) Serial Bus Controller interface
 * functions for GRETH2.
 *
 * This file implements functions to read and write PHY registers
 * via the MDIO interface for the GRETH2 Ethernet driver.
 * It provides blocking access to PHY registers and handles
 * basic error reporting on failed reads.
 *
 * Main functionalities:
 * - Reading a PHY register using `greth2_ext_phy_reg_read()`.
 * - Writing a PHY register using `greth2_ext_phy_reg_write()`.
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

#include "greth2_ext_mdio.h"
#include "greth2_emac.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rtems/dev/io.h>

/**
 * @brief   Reads a PHY register using MDIO.
 *
 * @param greth2_device Pointer to the GRETH2 device state structure.
 * @param phy_addr PHY Address.
 * @param reg_addr Register Number to be read.
 * @param data Pointer where the read value shall be written.
 *
 * @retval ERR_OK read successful.
 * @retval ERR_IF read unsuccessful.
 *
 **/
static err_t greth2_ext_phy_reg_read(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  uint32_t                   reg_addr,
  uint16_t                  *data
);

/**
 * @brief   Writes a PHY register using MDIO.
 *
 * @param greth2_device Pointer to the GRETH2 device state structure.
 * @param phy_addr PHY Address.
 * @param reg_addr Register Number to be read.
 * @param value Value to be written.
 *
 * @retval ERR_OK write successful.
 * @retval ERR_IF write unsuccessful.
 *
 **/
static err_t greth2_ext_phy_reg_write(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  uint32_t                   reg_addr,
  uint16_t                   value
);

static inline void mdio_ctrl_wait_for_job_done(
  struct greth2_netif_state *greth2_device
)
{
  while (
    !( greth2_device->ext_mdio_regs->apb_job & MDIO_CTRL_APB_JOB_DONE )
  ) {
    _IO_Relax();
  }
}

static inline void mdio_ctrl_wait_for_job_active(
  struct greth2_netif_state *greth2_device
)
{
  while ( greth2_device->ext_mdio_regs->apb_job & MDIO_CTRL_APB_JOB_ACTIVE ) {
    _IO_Relax();
  }
}

err_t mdio_ctrl_get_phy_init_status(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  struct mdio_ctrl_phy_cfg  *cfg
)
{
  uint32_t phy_reg;

  if ( phy_addr > 31 ) {
    return ERR_IF;
  }

  phy_reg = greth2_device->ext_mdio_regs->phy_reg[ phy_addr ];

  cfg->status = phy_reg & MDIO_CTRL_PHY_INIT_STS_MASK;
  if (
    cfg->status == MDIO_CTRL_PHY_INIT_STS_SUCCESS ||
    cfg->status == MDIO_CTRL_PHY_INIT_STS_DEFERRED
  ) {
    /* If autonegotiation was successful, also parse out the speed and duplex
     * info */
    cfg->autoneg_fd = ( phy_reg & MDIO_CTRL_PHY_AUTONEG_MD ) != 0;
    cfg->speed = ( phy_reg >> MDIO_CTRL_PHY_AUTONEG_SP_BIT ) &
                 MDIO_CTRL_PHY_AUTONEG_SP_MASK;
  }

  return ERR_OK;
}

err_t mdio_ctrl_is_phy_init(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  bool                      *is_init
)
{
  if ( phy_addr > 31 ) {
    return ERR_IF;
  }

  *is_init = ( greth2_device->ext_mdio_regs->init_bitmask &
               ( 1 << phy_addr ) ) != 0;
  return ERR_OK;
}

static err_t greth2_ext_phy_reg_read(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  uint32_t                   reg_addr,
  uint16_t                  *data
)
{
  uint32_t job;

  if ( phy_addr > 31 || reg_addr > 31 ) {
    return ERR_IF;
  }

  job = ( phy_addr << MDIO_CTRL_APB_JOB_PHYADDR_BIT ) |
        ( reg_addr << MDIO_CTRL_APB_JOB_REGADDR_BIT );

  mdio_ctrl_wait_for_job_active( greth2_device );
  greth2_device->ext_mdio_regs->apb_job = job;
  mdio_ctrl_wait_for_job_done( greth2_device );

  if ( greth2_device->ext_mdio_regs->apb_job & MDIO_CTRL_APB_JOB_LINK_FAIL ) {
    return ERR_IF;
  }

  *data = ( greth2_device->ext_mdio_regs->apb_job >>
            MDIO_CTRL_APB_JOB_DATA_BIT ) &
          MDIO_CTRL_APB_JOB_DATA_MASK;
  return ERR_OK;
}

static err_t greth2_ext_phy_reg_write(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  uint32_t                   reg_addr,
  uint16_t                   value
)
{
  uint32_t job;

  if ( phy_addr > 31 || reg_addr > 31 ) {
    return ERR_IF;
  }

  job = ( phy_addr << MDIO_CTRL_APB_JOB_PHYADDR_BIT ) |
        ( reg_addr << MDIO_CTRL_APB_JOB_REGADDR_BIT ) |
        ( value << MDIO_CTRL_APB_JOB_DATA_BIT ) | MDIO_CTRL_APB_JOB_WRITE;

  mdio_ctrl_wait_for_job_active( greth2_device );
  greth2_device->ext_mdio_regs->apb_job = job;
  mdio_ctrl_wait_for_job_done( greth2_device );

  if ( greth2_device->ext_mdio_regs->apb_job & MDIO_CTRL_APB_JOB_LINK_FAIL ) {
    return ERR_IF;
  }

  return ERR_OK;
}

struct greth2_phy_ops greth2_ext_phy_ops = {
  .read = greth2_ext_phy_reg_read,
  .write = greth2_ext_phy_reg_write,
};