/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth_phy.c
 * @brief GRETH PHY control and autonegotiation functions.
 *
 * This file provides functions to control and configure the PHY device
 * connected to the GRETH Ethernet MAC. It includes routines to reset the PHY,
 * start and monitor autonegotiation, determine link parameters (speed, duplex),
 * and perform post-autonegotiation configuration based on PHY and MAC 
 * capabilities.
 *
 * The following functions are included:
 * - greth_phy_reset(): Perform a hardware reset of the PHY.
 * - greth_phy_start_auto_negotiate(): Initiate autonegotiation and wait for 
 * completion.
 * - greth_phy_auto_negotiate(): Monitor autonegotiation, determine link speed, 
 * duplex mode, and update the GRETH device state.
 * - greth_phy_post_auto_negotiate(): Read PHY manufacturing information and finalize
 *   PHY configuration after autonegotiation.
 *
 * Constants:
 * - greth_tan: Timeout value for PHY autonegotiation derived from
 *   GRETH_AUTONEGO_TIMEOUT_MS.
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

#include "greth_phy.h"
#include "greth_emac.h"
#include "greth_netif.h"

#include <rtems/dev/io.h>

/**
 * @brief Timeout value for GRETH PHY autonegotiation.
 *
 * This constant specifies the timeout for the GRETH PHY autonegotiation process
 * expressed as a `struct timespec`. The `tv_sec` field contains the timeout in 
 * seconds, and `tv_nsec` contains the remaining nanoseconds.
 *
 * @note The timeout value is derived from `GRETH_AUTONEGO_TIMEOUT_MS`.
 */
const struct timespec greth_tan = {
  GRETH_AUTONEGO_TIMEOUT_MS / 1000,
  ( GRETH_AUTONEGO_TIMEOUT_MS % 1000 ) * 1000000
};

/**
 * @brief Reset the GRETH PHY device.
 *
 * This function performs a hardware reset of the PHY connected to the GRETH 
 * Ethernet MAC. It reads the PHY control register, asserts the reset bit, and 
 * waits for the reset to complete. After the reset, the PHY status register is 
 * read to update the internal state.
 *
 * @param greth_dev Pointer to the GRETH device state structure.
 *
 * @note This function is blocking until the PHY indicates that the reset 
 * process has completed.
 */
void greth_phy_reset( struct greth_netif_state *greth_dev )
{
  if ( greth_dev->phy_dev.phyAddr == 0 ) {
    greth_dev->phy_dev.phyAddr = ( greth_dev->regs->mdio_ctrl &
                                   GRETH_MDIO_PHYADR ) >>
                                 GRETH_MDIO_PHYADR_BIT;
  }

  greth_phy_reg_write(
    greth_dev,
    greth_dev->phy_dev.phyAddr,
    GRETH_PHY_CTRL,
    GRETH_PHY_CTRL_RST
  );
  do {
    _IO_Relax();
    greth_phy_reg_read(
      greth_dev,
      greth_dev->phy_dev.phyAddr,
      GRETH_PHY_CTRL,
      &greth_dev->phy_dev.phyCtrl
    );
  } while ( greth_dev->phy_dev.phyCtrl & GRETH_PHY_CTRL_RST );
}

/**
 * @brief Start autonegotiation with the PHY device.
 *
 * This function performs a PHY reset, configures the PHY for autonegotiation 
 * (disabling gigabit advertisement if the MAC does not support it), and then 
 * initiates the autonegotiation process. It waits for autonegotiation to 
 * complete by calling `greth_phy_auto_negotiate`.
 *
 * @param greth_dev Pointer to the GRETH device state structure.
 * @param phy_dev   Pointer to the PHY device information structure.
 *
 * @return bool
 * @retval TRUE  Autonegotiation completed successfully.
 * @retval FALSE Autonegotiation failed or timed out.
 *
 * @note This function is blocking until autonegotiation completes.
 */
err_t greth_phy_start_auto_negotiate(
  struct greth_netif_state *greth_dev,
  struct phy_device_info   *phy_dev
)
{
  greth_phy_reset( greth_dev );
  greth_phy_reg_read(
    greth_dev,
    phy_dev->phyAddr,
    GRETH_PHY_STATUS,
    &phy_dev->phyStatus
  );
  greth_phy_reg_read(
    greth_dev,
    phy_dev->phyAddr,
    GRETH_PHY_CTRL,
    &phy_dev->phyCtrl
  );
  if (
    ( !greth_dev->gbit_mac ) &&
    ( phy_dev->phyStatus & GRETH_PHY_STATUS_EXT_STS )
  ) {
    greth_phy_reg_write( greth_dev, phy_dev->phyAddr, GRETH_PHY_MS_CTRL, 0 );
  }
  if ( phy_dev->phyStatus & GRETH_PHY_STATUS_ANEG ) {
    greth_phy_reg_write(
      greth_dev,
      phy_dev->phyAddr,
      GRETH_PHY_CTRL,
      ( phy_dev->phyCtrl | GRETH_PHY_CTRL_RANEG | GRETH_PHY_CTRL_ANEG )
    );
    greth_phy_reg_read(
      greth_dev,
      phy_dev->phyAddr,
      GRETH_PHY_CTRL,
      &phy_dev->phyCtrl
    );
  }

  return greth_phy_auto_negotiate( greth_dev, &greth_dev->phy_dev );
}

/**
 * @brief Perform PHY autonegotiation and determine link parameters.
 *
 * This function performs the autonegotiation process between the GRETH MAC
 * and the connected PHY. It waits until autonegotiation completes or times out,
 * and sets the link speed, duplex mode, and autonegotiation status in the
 * @ref `greth_netif_state` structure.
 *
 * @param greth_dev Pointer to the GRETH device state structure.
 * @param phy_dev   Pointer to the PHY device information structure.
 *
 * @return bool
 * @retval true  Autonegotiation completed successfully.
 * @retval false Autonegotiation failed or timed out.
 *
 * @note This function is blocking and waits for autonegotiation completion.
 * @note After this function, the `greth_dev` structure fields `sp`, `gb`, `fd`,
 *       and `auto_neg` are updated to reflect the negotiated speed, gigabit
 *       support, full-duplex mode, and autonegotiation status, respectively.
 */
err_t greth_phy_auto_negotiate(
  struct greth_netif_state *greth_dev,
  struct phy_device_info   *phy_dev
)
{
  greth_debug_printf(
    "greth_phy.c: Autonegotiation started: check on cable if it's connected!\n"
  );
  uint16_t tmp1;
  greth_dev->gb = greth_dev->fd = greth_dev->sp = greth_dev->auto_neg = 0;
  struct timespec tstart, tnow;
  timespecclear( &greth_dev->auto_neg_time );
  if ( phy_dev->phyCtrl & GRETH_PHY_CTRL_ANEG ) {
    greth_dev->auto_neg = 1;
    if ( rtems_clock_get_uptime( &tstart ) != RTEMS_SUCCESSFUL ) {
      printk( "\nrtems_clock_get_uptime failed\n" );
      return ERR_IF;
    }
    greth_phy_reg_read(
      greth_dev,
      phy_dev->phyAddr,
      GRETH_PHY_STATUS,
      &phy_dev->phyStatus
    );
    while ( !( phy_dev->phyStatus & GRETH_PHY_STATUS_ANEG_COMP ) ) {
      if ( rtems_clock_get_uptime( &tnow ) != RTEMS_SUCCESSFUL ) {
        printk( "rtems_clock_get_uptime failed\n" );
        return ERR_IF;
      }
      timespecsub( &tnow, &tstart, &greth_dev->auto_neg_time );
      if ( timespeccmp( &greth_dev->auto_neg_time, &greth_tan, > ) ) {
        greth_dev->auto_neg = -1; /* Failed */
        greth_phy_reg_read( greth_dev, phy_dev->phyAddr, 0, &tmp1 );
        greth_dev->gb = ( phy_dev->phyCtrl & GRETH_PHY_CTRL_MSP ) &&
                        !( phy_dev->phyCtrl & GRETH_PHY_CTRL_LSP );
        greth_dev->sp = !( phy_dev->phyCtrl & GRETH_PHY_CTRL_MSP ) &&
                        ( phy_dev->phyCtrl & GRETH_PHY_CTRL_LSP );
        greth_dev->fd = phy_dev->phyCtrl & GRETH_PHY_CTRL_DM;
        phy_dev->autoneg = false;
        return ERR_IF;
      }
      rtems_task_wake_after( rtems_clock_get_ticks_per_second() / 32 );
      greth_phy_reg_read(
        greth_dev,
        phy_dev->phyAddr,
        GRETH_PHY_STATUS,
        &phy_dev->phyStatus
      );
    }
    greth_phy_reg_read(
      greth_dev,
      phy_dev->phyAddr,
      GRETH_PHY_AUTONEG_ADVERT,
      &greth_dev->phy_dev.adv
    );

    greth_phy_reg_read(
      greth_dev,
      phy_dev->phyAddr,
      GRETH_PHY_AUTONEG_LPA,
      &greth_dev->phy_dev.part
    );
    if ( phy_dev->phyStatus & GRETH_PHY_STATUS_EXT_STS ) {
      greth_phy_reg_read(
        greth_dev,
        phy_dev->phyAddr,
        GRETH_PHY_MS_CTRL,
        &greth_dev->phy_dev.extadv
      );
      greth_phy_reg_read(
        greth_dev,
        phy_dev->phyAddr,
        GRETH_PHY_MS_STS,
        &greth_dev->phy_dev.extpart
      );
      if (
        ( greth_dev->phy_dev.extadv & GRETH_MII_EXTADV_1000FD ) &&
        ( greth_dev->phy_dev.extpart & GRETH_MII_EXTPRT_1000FD )
      ) {
        greth_dev->gb = true;
        greth_dev->fd = true;
      } else if (
        ( greth_dev->phy_dev.extadv & GRETH_MII_EXTADV_1000HD ) &&
        ( greth_dev->phy_dev.extpart & GRETH_MII_EXTPRT_1000HD )
      ) {
        greth_dev->gb = true;
        greth_dev->fd = false;
      }
    }
    if ( !greth_dev->gbit_mac || !greth_dev->gb ) {
      if (
        ( greth_dev->phy_dev.adv & GRETH_MII_100TXFD ) &&
        ( greth_dev->phy_dev.part & GRETH_MII_100TXFD )
      ) {
        greth_dev->sp = true;
        greth_dev->fd = true;
      } else if (
        ( greth_dev->phy_dev.adv & GRETH_MII_100TXHD ) &&
        ( greth_dev->phy_dev.part & GRETH_MII_100TXHD )
      ) {
        greth_dev->sp = true;
        greth_dev->fd = false;
      } else if (
        ( greth_dev->phy_dev.adv & GRETH_MII_10FD ) &&
        ( greth_dev->phy_dev.part & GRETH_MII_10FD )
      ) {
        greth_dev->fd = true;
      }
    }
  }
  phy_dev->autoneg = true;
  return ERR_OK;
}

static void greth_phy_id_device( struct greth_netif_state *greth_dev )
{
  struct phy_device_info *phy_dev = &greth_dev->phy_dev;
  uint16_t                phy_id1, phy_id2;

  greth_phy_reg_read( greth_dev, phy_dev->phyAddr, GRETH_PHY_ID1, &phy_id1 );
  greth_phy_reg_read( greth_dev, phy_dev->phyAddr, GRETH_PHY_ID2, &phy_id2 );

  phy_dev->vendor = phy_id1 << 3 | ( ( ( phy_id2 >> GRETH_PHY_ID2_OUI_BIT ) &
                                       GRETH_PHY_ID2_OUI_MASK )
                                     << 19 );
  phy_dev->rev = phy_id2 & GRETH_PHY_ID2_REV_MASK;
  phy_dev->device = ( phy_id2 >> GRETH_PHY_ID2_VENDOR_BIT ) &
                    GRETH_PHY_ID2_VENDOR_MASK;
}

/**
 * @brief Perform post-autonegotiation PHY configuration.
 *
 * This function is called after the autonegotiation process has completed.
 * It reads PHY manufacturing information (vendor, device, revision), updates
 * the GRETH device link parameters, and performs any necessary PHY resets or
 * special configurations depending on PHY type and MAC capabilities.
 *
 * @param greth_dev Pointer to the GRETH device state structure.
 * @param phy_dev   Pointer to the PHY device information structure.
 *
 * @return err_t
 * @retval ERR_OK  Post-autonegotiation configuration succeeded.
 * @retval ERR_IF  Autonegotiation was not completed, so post-configuration
 *                 was not performed.
 */
err_t greth_phy_post_auto_negotiate(
  struct greth_netif_state *greth_dev,
  struct phy_device_info   *phy_dev
)
{
  if ( phy_dev->autoneg ) {
    phy_dev->vendor = 0;
    phy_dev->device = 0;
    phy_dev->rev = 0;
    greth_phy_reg_read(
      greth_dev,
      phy_dev->phyAddr,
      GRETH_PHY_STATUS,
      &phy_dev->phyStatus
    );

    uint16_t tmp3;

    /* Read out PHY info if extended registers are available */
    if ( phy_dev->phyStatus & GRETH_PHY_STATUS_EXT_CAP ) {
      greth_phy_id_device( greth_dev );
    }

    if (
      ( phy_dev->phyStatus & GRETH_PHY_STATUS_EXT_CAP ) &&
      ( phy_dev->vendor == 0x005043 ) && ( phy_dev->device == 0x0C )
    ) {
      if (
        ( ( greth_dev->gb ) && !( greth_dev->gbit_mac ) ) ||
        !(( phy_dev->phyCtrl & GRETH_PHY_CTRL_ANEG ))
      ) {
        uint16_t ctrl;
        ctrl = greth_dev->sp ? GRETH_PHY_CTRL_LSP : 0;
        tmp3 = GRETH_PHY_CTRL_RST;
        greth_phy_reg_write(
          greth_dev,
          phy_dev->phyAddr,
          GRETH_PHY_CTRL,
          ctrl
        );
        greth_phy_reg_write(
          greth_dev,
          phy_dev->phyAddr,
          GRETH_PHY_CTRL,
          tmp3
        );
        greth_dev->gb = false;
        greth_dev->sp = false;
        greth_dev->fd = false;
      }
    } else {
      if (
        ( ( greth_dev->gb ) && !( greth_dev->gbit_mac ) ) ||
        !(( phy_dev->phyCtrl & GRETH_PHY_CTRL_ANEG ))
      ) {
        uint16_t ctrl;
        ctrl = greth_dev->sp ? GRETH_PHY_CTRL_LSP : 0;
        tmp3 = GRETH_PHY_CTRL_RST;
        greth_phy_reg_write(
          greth_dev,
          phy_dev->phyAddr,
          GRETH_PHY_CTRL,
          ctrl
        );
        greth_dev->gb = false;
        greth_dev->sp = false;
        greth_dev->fd = false;
      }
    }

    return ERR_OK;
  } else {
    return ERR_IF;
  }
}