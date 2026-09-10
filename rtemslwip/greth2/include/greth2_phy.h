/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth2_phy.h
 * @brief GRETH2 PHY driver definitions and functions
 *
 * This header file contains MII register definitions, PHY capabilities,
 * and function prototypes for initializing and autonegotiating PHY devices
 * connected to the GRETH2 Ethernet controller.
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

#ifndef GRETH2_PHY_H
#define GRETH2_PHY_H

#include "greth2_emac.h"

struct greth2_netif_state;
struct phy_device_info;

#define GRETH2_AUTONEGO_TIMEOUT_MS 7000

extern const struct timespec greth2_tan;

#ifdef __cplusplus
extern "C" {
#endif

enum greth2_phy_regmap {
  GRETH2_PHY_CTRL,
  GRETH2_PHY_STATUS,
  GRETH2_PHY_ID1,
  GRETH2_PHY_ID2,
  GRETH2_PHY_AUTONEG_ADVERT,
  GRETH2_PHY_AUTONEG_LPA,
  GRETH2_PHY_AUTONEG_EXP,
  GRETH2_PHY_AUTONEG_NPT,
  GRETH2_PHY_AUTONEG_LP_RNP,
  GRETH2_PHY_MS_CTRL,
  GRETH2_PHY_MS_STS,
  GRETH2_PHY_PSE_CTRL,
  GRETH2_PHY_PSE_STS,
  GRETH2_PHY_MMD_CTRL,
  GRETH2_PHY_MMD_ADDR,
  GRETH2_PHY_EXT_STATUS,
};

/* PHY CTRL */
#define GRETH2_PHY_CTRL_RST_BIT    15
#define GRETH2_PHY_CTRL_LP_BIT     14
#define GRETH2_PHY_CTRL_LSP_BIT    13
#define GRETH2_PHY_CTRL_ANEG_BIT   12
#define GRETH2_PHY_CTRL_PWD_BIT    11
#define GRETH2_PHY_CTRL_ISO_BIT    10
#define GRETH2_PHY_CTRL_RANEG_BIT  9
#define GRETH2_PHY_CTRL_DM_BIT     8
#define GRETH2_PHY_CTRL_COLTST_BIT 7
#define GRETH2_PHY_CTRL_MSP_BIT    6
#define GRETH2_PHY_CTRL_UNI_BIT    5

#define GRETH2_PHY_CTRL_RST    ( 1 << GRETH2_PHY_CTRL_RST_BIT )
#define GRETH2_PHY_CTRL_LP     ( 1 << GRETH2_PHY_CTRL_LP_BIT )
#define GRETH2_PHY_CTRL_LSP    ( 1 << GRETH2_PHY_CTRL_LSP_BIT )
#define GRETH2_PHY_CTRL_ANEG   ( 1 << GRETH2_PHY_CTRL_ANEG_BIT )
#define GRETH2_PHY_CTRL_PWD    ( 1 << GRETH2_PHY_CTRL_PWD_BIT )
#define GRETH2_PHY_CTRL_ISO    ( 1 << GRETH2_PHY_CTRL_ISO_BIT )
#define GRETH2_PHY_CTRL_RANEG  ( 1 << GRETH2_PHY_CTRL_RANEG_BIT )
#define GRETH2_PHY_CTRL_DM     ( 1 << GRETH2_PHY_CTRL_DM_BIT )
#define GRETH2_PHY_CTRL_COLTST ( 1 << GRETH2_PHY_CTRL_COLTST_BIT )
#define GRETH2_PHY_CTRL_MSP    ( 1 << GRETH2_PHY_CTRL_MSP_BIT )
#define GRETH2_PHY_CTRL_UNI    ( 1 << GRETH2_PHY_CTRL_UNI_BIT )

/* PHY STS */
#define GRETH2_PHY_STATUS_A100BT4_BIT    15
#define GRETH2_PHY_STATUS_A100BX_FD_BIT  14
#define GRETH2_PHY_STATUS_A100BX_HD_BIT  13
#define GRETH2_PHY_STATUS_10MB_FD_BIT    12
#define GRETH2_PHY_STATUS_10MB_HD_BIT    11
#define GRETH2_PHY_STATUS_A100BT2_FD_BIT 10
#define GRETH2_PHY_STATUS_A100BT2_HD_BIT 9
#define GRETH2_PHY_STATUS_EXT_STS_BIT    8
#define GRETH2_PHY_STATUS_UNI_BIT        7
#define GRETH2_PHY_STATUS_PRESUP_BIT     6
#define GRETH2_PHY_STATUS_ANEG_COMP_BIT  5
#define GRETH2_PHY_STATUS_RF_BIT         4
#define GRETH2_PHY_STATUS_ANEG_BIT       3
#define GRETH2_PHY_STATUS_LS_BIT         2
#define GRETH2_PHY_STATUS_JABBER_BIT     1
#define GRETH2_PHY_STATUS_EXT_CAP_BIT    0

#define GRETH2_PHY_STATUS_A100BT4    ( 1 << GRETH2_PHY_STATUS_A100BT4_BIT )
#define GRETH2_PHY_STATUS_A100BX_FD  ( 1 << GRETH2_PHY_STATUS_A100BX_FD_BIT )
#define GRETH2_PHY_STATUS_A100BX_HD  ( 1 << GRETH2_PHY_STATUS_A100BX_HD_BIT )
#define GRETH2_PHY_STATUS_10MB_FD    ( 1 << GRETH2_PHY_STATUS_10MB_FD_BIT )
#define GRETH2_PHY_STATUS_10MB_HD    ( 1 << GRETH2_PHY_STATUS_10MB_HD_BIT )
#define GRETH2_PHY_STATUS_A100BT2_FD ( 1 << GRETH2_PHY_STATUS_A100BT2_FD_BIT )
#define GRETH2_PHY_STATUS_A100BT2_HD ( 1 << GRETH2_PHY_STATUS_A100BT2_HD_BIT )
#define GRETH2_PHY_STATUS_EXT_STS    ( 1 << GRETH2_PHY_STATUS_EXT_STS_BIT )
#define GRETH2_PHY_STATUS_UNI        ( 1 << GRETH2_PHY_STATUS_UNI_BIT )
#define GRETH2_PHY_STATUS_PRESUP     ( 1 << GRETH2_PHY_STATUS_PRESUP_BIT )
#define GRETH2_PHY_STATUS_ANEG_COMP  ( 1 << GRETH2_PHY_STATUS_ANEG_COMP_BIT )
#define GRETH2_PHY_STATUS_RF         ( 1 << GRETH2_PHY_STATUS_RF_BIT )
#define GRETH2_PHY_STATUS_ANEG       ( 1 << GRETH2_PHY_STATUS_ANEG_BIT )
#define GRETH2_PHY_STATUS_LS         ( 1 << GRETH2_PHY_STATUS_LS_BIT )
#define GRETH2_PHY_STATUS_JABBER     ( 1 << GRETH2_PHY_STATUS_JABBER_BIT )
#define GRETH2_PHY_STATUS_EXT_CAP    ( 1 << GRETH2_PHY_STATUS_EXT_CAP_BIT )

/* PHY ID1 */
#define GRETH2_PHY_ID1_OUI_MASK 0xffff

/* PHY ID2 */
#define GRETH2_PHY_ID2_OUI_BIT    10
#define GRETH2_PHY_ID2_VENDOR_BIT 4
#define GRETH2_PHY_ID2_REV_BIT    0

#define GRETH2_PHY_ID2_OUI_MASK    0x3f
#define GRETH2_PHY_ID2_VENDOR_MASK 0x3f
#define GRETH2_PHY_ID2_REV_MASK    0xf

/* PHY EXT STS */
#define GRETH2_PHY_EXT_STS_1000X_FD_BIT 15
#define GRETH2_PHY_EXT_STS_1000X_HD_BIT 14
#define GRETH2_PHY_EXT_STS_1000T_FD_BIT 13
#define GRETH2_PHY_EXT_STS_1000T_HD_BIT 12

#define GRETH2_PHY_EXT_STS_1000X_FD ( 1 << GRETH2_PHY_EXT_STS_1000X_FD_BIT )
#define GRETH2_PHY_EXT_STS_1000X_HD ( 1 << GRETH2_PHY_EXT_STS_1000X_HD_BIT )
#define GRETH2_PHY_EXT_STS_1000T_FD ( 1 << GRETH2_PHY_EXT_STS_1000T_FD_BIT )
#define GRETH2_PHY_EXT_STS_1000T_HD ( 1 << GRETH2_PHY_EXT_STS_1000T_HD_BIT )

/* MII registers */
#define GRETH2_MII_EXTPRT_1000FD_BIT 11
#define GRETH2_MII_EXTPRT_1000HD_BIT 10

#define GRETH2_MII_EXTPRT_1000FD ( 1 << GRETH2_MII_EXTPRT_1000FD_BIT )
#define GRETH2_MII_EXTPRT_1000HD ( 1 << GRETH2_MII_EXTPRT_1000HD_BIT )

#define GRETH2_MII_EXTADV_1000FD_BIT 9
#define GRETH2_MII_EXTADV_1000HD_BIT 8

#define GRETH2_MII_EXTADV_1000FD ( 1 << GRETH2_MII_EXTADV_1000FD_BIT )
#define GRETH2_MII_EXTADV_1000HD ( 1 << GRETH2_MII_EXTADV_1000HD_BIT )

#define GRETH2_MII_100T4_BIT   9
#define GRETH2_MII_100TXFD_BIT 8
#define GRETH2_MII_100TXHD_BIT 7
#define GRETH2_MII_10FD_BIT    6
#define GRETH2_MII_10HD_BIT    5

#define GRETH2_MII_100T4   ( 1 << GRETH2_MII_100T4_BIT )
#define GRETH2_MII_100TXFD ( 1 << GRETH2_MII_100TXFD_BIT )
#define GRETH2_MII_100TXHD ( 1 << GRETH2_MII_100TXHD_BIT )
#define GRETH2_MII_10FD    ( 1 << GRETH2_MII_10FD_BIT )
#define GRETH2_MII_10HD    ( 1 << GRETH2_MII_10HD_BIT )

struct greth2_phy_ops {
  err_t ( *write )(
    struct greth2_netif_state *greth2_dev,
    uint32_t                   phy_addr,
    uint32_t                   reg,
    uint16_t                   val
  );
  err_t ( *read )(
    struct greth2_netif_state *greth2_dev,
    uint32_t                   phy_addr,
    uint32_t                   reg,
    uint16_t                  *val
  );
  err_t ( *init )(
    struct greth2_netif_state *greth2_dev,
    uint32_t                   phy_addr,
    bool                       force
  );
};

/**
 * @brief Initialize the GRETH2 PHY driver.
 *
 * This function initializes the GRETH2 PHY driver by setting up the necessary
 * function pointers for PHY register access and autonegotiation operations. It
 * should be called during the GRETH2 network interface initialization process to
 * ensure that PHY operations are properly configured before any PHY
 * interactions occur.
 *
 * @param greth2_dev Pointer to the GRETH2 device state structure to initialize.
 *
 */
void greth2_phy_drv_init( struct greth2_netif_state *greth2_dev );

/**
 * @brief Reset the GRETH2 PHY device.
 *
 * This function performs a hardware reset of the PHY connected to the GRETH2
 * Ethernet MAC. It reads the PHY control register, asserts the reset bit, and
 * waits for the reset to complete. After the reset, the PHY status register is
 * read to update the internal state.
 *
 * @param greth2_dev Pointer to the GRETH2 device state structure.
 * @param phy_addr   Address of the PHY device to reset.
 *
 * @note This function is blocking until the PHY indicates that the reset
 * process has completed.
 */
err_t greth2_phy_reset(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr
);

/**
 * @brief Write a value to a PHY register.
 *
 * This function writes a 16-bit value to a specified register of the PHY
 * device.
 *
 * @param greth2_dev Pointer to the GRETH2 device state structure.
 * @param phy_addr   Address of the PHY device.
 * @param reg        Register address to write to.
 * @param val        Value to write to the register.
 *
 * @return err_t
 * @retval ERR_OK  Write operation successful.
 * @retval ERR_IF  Write operation failed.
 */
err_t greth2_phy_reg_write(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr,
  uint32_t                   reg,
  uint16_t                   val
);

/**
 * @brief Read a value from a PHY register.
 *
 * This function reads a 16-bit value from a specified register of the PHY
 * device.
 *
 * @param greth2_dev Pointer to the GRETH2 device state structure.
 * @param phy_addr   Address of the PHY device.
 * @param reg        Register address to read from.
 * @param val        Pointer to store the read value.
 *
 * @return err_t
 * @retval ERR_OK  Read operation successful.
 * @retval ERR_IF  Read operation failed.
 */
err_t greth2_phy_reg_read(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr,
  uint32_t                   reg,
  uint16_t                  *val
);

/**
 * @brief Initialize the PHY device.
 *
 * This function initializes the PHY device. If an external MDIO controller
 * exists, it will reads the initialization status from the controller and
 * will start autonegotiation if it has not been performed yet. If no external
 * MDIO controller is present, it will directly start autonegotiation with the
 * PHY device. The function will block until autonegotiation is completed or
 * times out.
 *
 * @param greth2_dev Pointer to the GRETH2 device state structure.
 * @param phy_addr   Address of the PHY device.
 * @param force      If true, force autonegotiation even if already completed or
 * failed.
 *
 * @return err_t
 * @retval ERR_OK  Autonegotiation started successfully.
 * @retval ERR_IF  Autonegotiation failed to start.
 */
err_t greth2_phy_init(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr,
  bool                       force
);

/**
 * @brief Perform post-autonegotiation PHY configuration.
 *
 * This function should be called after the autonegotiation process has
 * completed to perform any necessary configuration based on the results of
 * autonegotiation.
 *
 * @param greth2_dev Pointer to the GRETH2 device state structure.
 * @param phy_addr   Address of the PHY device.
 *
 * @return err_t
 * @retval ERR_OK  Post-autonegotiation configuration successful.
 * @retval ERR_IF  Post-autonegotiation configuration failed.
 */
err_t greth2_phy_post_auto_negotiate(
  struct greth2_netif_state *greth2_dev,
  uint32_t                   phy_addr
);

#ifdef __cplusplus
}
#endif
#endif /* GRETH2_PHY_H */