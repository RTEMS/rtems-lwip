/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth2_phy.c
 * @brief GRETH PHY control and autonegotiation functions.
 *
 * This file provides functions to control and configure the PHY device
 * connected to the GRETH Ethernet MAC. It includes routines to reset the PHY,
 * start and monitor autonegotiation, determine link parameters (speed, duplex),
 * and perform post-autonegotiation configuration based on PHY and MAC
 * capabilities.
 *
 * The following functions are included:
 * - greth2_phy_reset(): Perform a hardware reset of the PHY.
 * - greth2_phy_init(): Initiate autonegotiation and wait for
 * completion.
 * - greth2_phy_auto_negotiate(): Monitor autonegotiation, determine link
 * speed, duplex mode, and update the GRETH device state.
 * - greth2_phy_post_auto_negotiate(): Read PHY manufacturing information and
 * finalize PHY configuration after autonegotiation.
 *
 * Constants:
 * - greth2_tan: Timeout value for PHY autonegotiation derived from
 *   GRETH2_AUTONEGO_TIMEOUT_MS.
 * @ingroup GRETH
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

#include "greth2_phy.h"
#include "greth2_emac.h"
#include "greth2_ext_mdio.h"
#include "greth2_netif.h"

#include <rtems/dev/io.h>

extern struct greth2_phy_ops greth2_int_phy_ops;
extern struct greth2_phy_ops greth2_ext_phy_ops;

/**
 * @brief Timeout value for GRETH PHY autonegotiation.
 *
 * This constant specifies the timeout for the GRETH PHY autonegotiation
 * process expressed as a `struct timespec`. The `tv_sec` field contains the
 * timeout in seconds, and `tv_nsec` contains the remaining nanoseconds.
 *
 * @note The timeout value is derived from `GRETH2_AUTONEGO_TIMEOUT_MS`.
 */
const struct timespec greth2_tan = {
  GRETH2_AUTONEGO_TIMEOUT_MS / 1000,
  ( GRETH2_AUTONEGO_TIMEOUT_MS % 1000 ) * 1000000
};

/****** Common PHY API ******/

static err_t greth2_phy_wait_for_auto_negotiate(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr
)
{
  struct phy_device_info *phy_dev = &greth2_dev->phy_devs[ phy_addr ];
  struct timespec         tstart;
  struct timespec         tnow;

  timespecclear( &greth2_dev->auto_neg_time );
  if ( rtems_clock_get_uptime( &tstart ) != RTEMS_SUCCESSFUL ) {
    printk( "\nrtems_clock_get_uptime failed\n" );
    return ERR_IF;
  }
  greth2_dev->phy_ops->read(
    greth2_dev,
    phy_addr,
    GRETH2_PHY_STATUS,
    &phy_dev->phyStatus
  );
  while ( !( phy_dev->phyStatus & GRETH2_PHY_STATUS_ANEG_COMP ) ) {
    if ( rtems_clock_get_uptime( &tnow ) != RTEMS_SUCCESSFUL ) {
      printk( "rtems_clock_get_uptime failed\n" );
      return ERR_IF;
    }
    timespecsub( &tnow, &tstart, &greth2_dev->auto_neg_time );
    if ( timespeccmp( &greth2_dev->auto_neg_time, &greth2_tan, > ) ) {
      return ERR_IF;
    }
    rtems_task_wake_after( rtems_clock_get_ticks_per_second() / 32 );
    greth2_dev->phy_ops->read(
      greth2_dev,
      phy_addr,
      GRETH2_PHY_STATUS,
      &phy_dev->phyStatus
    );
  }
  return ERR_OK;
}

static err_t greth2_phy_id_device(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr
)
{
  struct phy_device_info *phy_dev = &greth2_dev->phy_devs[ phy_addr ];

  if ( phy_addr > 31 ) {
    return ERR_IF;
  }

  uint16_t phy_id1, phy_id2;

  greth2_dev->phy_ops->read( greth2_dev, phy_addr, GRETH2_PHY_ID1, &phy_id1 );
  greth2_dev->phy_ops->read( greth2_dev, phy_addr, GRETH2_PHY_ID2, &phy_id2 );

  phy_dev->vendor = phy_id1 << 3 | ( ( ( phy_id2 >> GRETH2_PHY_ID2_OUI_BIT ) &
                                       GRETH2_PHY_ID2_OUI_MASK )
                                     << 19 );
  phy_dev->rev = phy_id2 & GRETH2_PHY_ID2_REV_MASK;
  phy_dev->device = ( phy_id2 >> GRETH2_PHY_ID2_VENDOR_BIT ) &
                    GRETH2_PHY_ID2_VENDOR_MASK;

  return ERR_OK;
}

static err_t greth2_phy_auto_negotiate(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr
)
{
  struct phy_device_info *phy_dev = &greth2_dev->phy_devs[ phy_addr ];

  if ( phy_addr > 31 ) {
    return ERR_IF;
  }

  greth2_dev->gb = false;
  greth2_dev->fd = false;
  greth2_dev->sp = false;
  greth2_dev->auto_neg = false;

  if ( !( phy_dev->phyCtrl & GRETH2_PHY_CTRL_ANEG ) ) {
    return ERR_IF;
  }

  greth2_debug_printf(
    "greth2_phy.c: Autonegotiation started: check on cable if "
    "it's connected!\n"
  );

  if ( greth2_phy_wait_for_auto_negotiate( greth2_dev, phy_addr ) != ERR_OK ) {
    return ERR_IF;
  }
  greth2_dev->phy_ops->read(
    greth2_dev,
    phy_addr,
    GRETH2_PHY_AUTONEG_ADVERT,
    &phy_dev->adv
  );

  greth2_dev->phy_ops->read(
    greth2_dev,
    phy_addr,
    GRETH2_PHY_AUTONEG_LPA,
    &phy_dev->part
  );
  if ( phy_dev->phyStatus & GRETH2_PHY_STATUS_EXT_STS ) {
    greth2_dev->phy_ops->read(
      greth2_dev,
      phy_addr,
      GRETH2_PHY_MS_CTRL,
      &phy_dev->extadv
    );
    greth2_dev->phy_ops->read(
      greth2_dev,
      phy_addr,
      GRETH2_PHY_MS_STS,
      &phy_dev->extpart
    );

    if (
      ( phy_dev->extadv & GRETH2_MII_EXTADV_1000FD ) &&
      ( phy_dev->extpart & GRETH2_MII_EXTPRT_1000FD )
    ) {
      greth2_dev->gb = true;
      greth2_dev->fd = true;
    } else if (
      ( phy_dev->extadv & GRETH2_MII_EXTADV_1000HD ) &&
      ( phy_dev->extpart & GRETH2_MII_EXTPRT_1000HD )
    ) {
      greth2_dev->gb = true;
      greth2_dev->fd = false;
    }
  }
  if ( greth2_dev->gb == false ) {
    if (
      ( phy_dev->adv & GRETH2_MII_100TXFD ) &&
      ( phy_dev->part & GRETH2_MII_100TXFD )
    ) {
      greth2_dev->sp = true;
      greth2_dev->fd = true;
    } else if (
      ( phy_dev->adv & GRETH2_MII_100TXHD ) &&
      ( phy_dev->part & GRETH2_MII_100TXHD )
    ) {
      greth2_dev->sp = true;
      greth2_dev->fd = false;
    } else if (
      ( phy_dev->adv & GRETH2_MII_10FD ) && ( phy_dev->part & GRETH2_MII_10FD )
    ) {
      greth2_dev->fd = true;
    }
  }
  phy_dev->autoneg = true;
  return ERR_OK;
}

/**
 * @brief Start autonegotiation with the PHY device.
 *
 * This function configures the PHY for autonegotiation
 * (disabling gigabit advertisement if the MAC does not support it), and then
 * initiates the autonegotiation process. It waits for autonegotiation to
 * complete by calling `greth2_phy_auto_negotiate`.
 *
 * @param greth2_dev Pointer to the GRETH device state structure.
 * @param phy_addr   Address of the PHY device to negotiate with.
 *
 * @return err_t
 * @retval ERR_OK  Autonegotiation completed successfully.
 * @retval ERR_IF  Autonegotiation failed or timed out.
 *
 * @note This function is blocking until autonegotiation completes.
 */
static err_t greth2_phy_start_auto_negotiate(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr
)
{
  struct phy_device_info *phy_dev = &greth2_dev->phy_devs[ phy_addr ];

  greth2_dev->phy_ops->read(
    greth2_dev,
    phy_addr,
    GRETH2_PHY_STATUS,
    &phy_dev->phyStatus
  );
  greth2_dev->phy_ops->read(
    greth2_dev,
    phy_addr,
    GRETH2_PHY_CTRL,
    &phy_dev->phyCtrl
  );

  if ( phy_dev->phyStatus & GRETH2_PHY_STATUS_ANEG ) {
    greth2_dev->phy_ops->write(
      greth2_dev,
      phy_addr,
      GRETH2_PHY_CTRL,
      ( phy_dev->phyCtrl | GRETH2_PHY_CTRL_ANEG | GRETH2_PHY_CTRL_RANEG )
    );
    greth2_dev->phy_ops->read(
      greth2_dev,
      phy_addr,
      GRETH2_PHY_CTRL,
      &phy_dev->phyCtrl
    );
  }
  return greth2_phy_auto_negotiate( greth2_dev, phy_addr );
}

/**
 * @brief Perform post-autonegotiation PHY configuration.
 *
 * This function is called after the autonegotiation process has completed.
 * It reads PHY manufacturing information (vendor, device, revision), updates
 * the GRETH device link parameters, and performs any necessary PHY resets or
 * special configurations depending on PHY type and MAC capabilities.
 *
 * @param greth2_dev Pointer to the GRETH device state structure.
 * @param phy_dev   Pointer to the PHY device information structure.
 *
 * @return err_t
 * @retval ERR_OK  Post-autonegotiation configuration succeeded.
 * @retval ERR_IF  Autonegotiation was not completed, so post-configuration
 *                was not performed.
 */
err_t greth2_phy_post_auto_negotiate(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr
)
{
  struct phy_device_info *phy_dev = &greth2_dev->phy_devs[ phy_addr ];

  if ( phy_addr > 31 ) {
    return ERR_IF;
  }

  greth2_dev->phy_ops->read(
    greth2_dev,
    phy_addr,
    GRETH2_PHY_CTRL,
    &phy_dev->phyCtrl
  );

  if ( phy_dev->autoneg == true ) {
    greth2_dev->phy_ops->read(
      greth2_dev,
      phy_addr,
      GRETH2_PHY_STATUS,
      &phy_dev->phyStatus
    );

    /* Read out PHY info if extended registers are available */
    if ( phy_dev->phyStatus & GRETH2_PHY_STATUS_EXT_CAP ) {
      greth2_phy_id_device( greth2_dev, phy_addr );
    }
  } else if ( !( phy_dev->phyCtrl & GRETH2_PHY_CTRL_ANEG ) ) {
    uint16_t ctrl;

    ctrl = greth2_dev->sp ? GRETH2_PHY_CTRL_LSP : 0;
    greth2_dev->phy_ops->write( greth2_dev, phy_addr, GRETH2_PHY_CTRL, ctrl );
    greth2_dev->gb = false;
    greth2_dev->sp = false;
    greth2_dev->fd = false;
  } else {
    return ERR_IF;
  }

  return ERR_OK;
}

/****** Internal MDIO Controller API ******/

err_t greth2_int_phy_init(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr,
  bool                       force
)
{
  (void) force;

  if ( phy_addr > 31 ) {
    return ERR_IF;
  }

  return greth2_phy_start_auto_negotiate( greth2_dev, phy_addr );
}

/****** External MDIO Controller API ******/

static err_t greth2_ext_phy_init(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr,
  bool                       force
)
{
  struct mdio_ctrl_phy_cfg phy_cfg;
  bool                     is_init;

  if ( phy_addr > 31 ) {
    return ERR_IF;
  }

  if ( force ) {
    return greth2_phy_start_auto_negotiate( greth2_dev, phy_addr );
  }

  mdio_ctrl_is_phy_init( greth2_dev, phy_addr, &is_init );
  if ( is_init ) {
    mdio_ctrl_get_phy_init_status( greth2_dev, phy_addr, &phy_cfg );
    /* PHY autonegotiation was performed at initialization */
    if ( phy_cfg.status == MDIO_CTRL_PHY_INIT_STS_SUCCESS ) {
      greth2_dev->phy_devs[ phy_addr ].autoneg = true;
      greth2_dev->sp = phy_cfg.speed == MDIO_CTRL_PHY_SP_100;
      greth2_dev->gb = phy_cfg.speed == MDIO_CTRL_PHY_SP_1000;
      greth2_dev->fd = phy_cfg.autoneg_fd;
      return ERR_OK;
    } else if ( phy_cfg.status == MDIO_CTRL_PHY_INIT_STS_RUNNING ) {
      /* PHY autonegotiation is still running, wait for it to complete */
      return greth2_phy_wait_for_auto_negotiate( greth2_dev, phy_addr );
    } else {
      /* PHY autonegotiation has not been started, start it now */
      return greth2_phy_start_auto_negotiate( greth2_dev, phy_addr );
    }
  }

  return greth2_phy_start_auto_negotiate( greth2_dev, phy_addr );
}

/*** Public API ***/

err_t greth2_phy_reg_read(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr,
  uint32_t                   reg,
  uint16_t                  *val
)
{
  return greth2_dev->phy_ops->read( greth2_dev, phy_addr, reg, val );
}

err_t greth2_phy_reg_write(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr,
  uint32_t                   reg,
  uint16_t                   val
)
{
  return greth2_dev->phy_ops->write( greth2_dev, phy_addr, reg, val );
}

err_t greth2_phy_reset(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr
)
{
  struct phy_device_info *phy_dev;

  if ( phy_addr > 31 ) {
    return ERR_IF;
  }

  phy_dev = &greth2_dev->phy_devs[ phy_addr ];
  greth2_dev->phy_ops->write(
    greth2_dev,
    phy_addr,
    GRETH2_PHY_CTRL,
    GRETH2_PHY_CTRL_RST
  );
  do {
    _IO_Relax();
    greth2_dev->phy_ops->read(
      greth2_dev,
      phy_addr,
      GRETH2_PHY_CTRL,
      &phy_dev->phyCtrl
    );
  } while ( phy_dev->phyCtrl & GRETH2_PHY_CTRL_RST );

  greth2_dev->phy_ops->read(
    greth2_dev,
    phy_addr,
    GRETH2_PHY_STATUS,
    &phy_dev->phyStatus
  );
  return ERR_OK;
}

err_t greth2_phy_init(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr,
  bool                       force
)
{
  return greth2_dev->phy_ops->init( greth2_dev, phy_addr, force );
}

void greth2_phy_drv_init( struct greth2_netif_state *greth2_dev )
{
  if ( greth2_dev->caps.extern_mdio ) {
    greth2_dev->phy_ops = &greth2_ext_phy_ops;
    greth2_dev->phy_ops->init = greth2_ext_phy_init;
  } else {
    greth2_dev->phy_ops = &greth2_int_phy_ops;
    greth2_dev->phy_ops->init = greth2_int_phy_init;
  }
}
