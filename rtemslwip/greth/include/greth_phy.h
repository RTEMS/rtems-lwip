/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth_phy.h
 * @brief GRETH PHY driver definitions and functions
 *
 * This header file contains MII register definitions, PHY capabilities,
 * and function prototypes for initializing and autonegotiating PHY devices
 * connected to the GRETH Ethernet controller.
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

#pragma once

#ifndef __DRV_PHY_H
#define __DRV_PHY_H

#include <stdbool.h>

#include "greth_emac.h"
#include "greth_mdio.h"
#include "greth_phy.h"

struct greth_netif_state;
struct phy_device_info;

#define GRETH_AUTONEGO_TIMEOUT_MS 7000

extern const struct timespec greth_tan;

#ifdef __cplusplus
extern "C" {
#endif

enum greth_phy_regmap {
  GRETH_PHY_CTRL,
  GRETH_PHY_STATUS,
  GRETH_PHY_ID1,
  GRETH_PHY_ID2,
  GRETH_PHY_AUTONEG_ADVERT,
  GRETH_PHY_AUTONEG_LPA,
  GRETH_PHY_AUTONEG_EXP,
  GRETH_PHY_AUTONEG_NPT,
  GRETH_PHY_AUTONEG_LP_RNP,
  GRETH_PHY_MS_CTRL,
  GRETH_PHY_MS_STS,
  GRETH_PHY_PSE_CTRL,
  GRETH_PHY_PSE_STS,
  GRETH_PHY_MMD_CTRL,
  GRETH_PHY_MMD_ADDR,
  GRETH_PHY_EXT_STATUS,
};

/* PHY CTRL */
#define GRETH_PHY_CTRL_RST_BIT    15
#define GRETH_PHY_CTRL_LP_BIT     14
#define GRETH_PHY_CTRL_LSP_BIT    13
#define GRETH_PHY_CTRL_ANEG_BIT   12
#define GRETH_PHY_CTRL_PWD_BIT    11
#define GRETH_PHY_CTRL_ISO_BIT    10
#define GRETH_PHY_CTRL_RANEG_BIT  9
#define GRETH_PHY_CTRL_DM_BIT     8
#define GRETH_PHY_CTRL_COLTST_BIT 7
#define GRETH_PHY_CTRL_MSP_BIT    6
#define GRETH_PHY_CTRL_UNI_BIT    5

#define GRETH_PHY_CTRL_RST    ( 1 << GRETH_PHY_CTRL_RST_BIT )
#define GRETH_PHY_CTRL_LP     ( 1 << GRETH_PHY_CTRL_LP_BIT )
#define GRETH_PHY_CTRL_LSP    ( 1 << GRETH_PHY_CTRL_LSP_BIT )
#define GRETH_PHY_CTRL_ANEG   ( 1 << GRETH_PHY_CTRL_ANEG_BIT )
#define GRETH_PHY_CTRL_PWD    ( 1 << GRETH_PHY_CTRL_PWD_BIT )
#define GRETH_PHY_CTRL_ISO    ( 1 << GRETH_PHY_CTRL_ISO_BIT )
#define GRETH_PHY_CTRL_RANEG  ( 1 << GRETH_PHY_CTRL_RANEG_BIT )
#define GRETH_PHY_CTRL_DM     ( 1 << GRETH_PHY_CTRL_DM_BIT )
#define GRETH_PHY_CTRL_COLTST ( 1 << GRETH_PHY_CTRL_COLTST_BIT )
#define GRETH_PHY_CTRL_MSP    ( 1 << GRETH_PHY_CTRL_MSP_BIT )
#define GRETH_PHY_CTRL_UNI    ( 1 << GRETH_PHY_CTRL_UNI_BIT )

/* PHY STS */
#define GRETH_PHY_STATUS_A100BT4_BIT    15
#define GRETH_PHY_STATUS_A100BX_FD_BIT  14
#define GRETH_PHY_STATUS_A100BX_HD_BIT  13
#define GRETH_PHY_STATUS_10MB_FD_BIT    12
#define GRETH_PHY_STATUS_10MB_HD_BIT    11
#define GRETH_PHY_STATUS_A100BT2_FD_BIT 10
#define GRETH_PHY_STATUS_A100BT2_HD_BIT 9
#define GRETH_PHY_STATUS_EXT_STS_BIT    8
#define GRETH_PHY_STATUS_UNI_BIT        7
#define GRETH_PHY_STATUS_PRESUP_BIT     6
#define GRETH_PHY_STATUS_ANEG_COMP_BIT  5
#define GRETH_PHY_STATUS_RF_BIT         4
#define GRETH_PHY_STATUS_ANEG_BIT       3
#define GRETH_PHY_STATUS_LS_BIT         2
#define GRETH_PHY_STATUS_JABBER_BIT     1
#define GRETH_PHY_STATUS_EXT_CAP_BIT    0

#define GRETH_PHY_STATUS_A100BT4    ( 1 << GRETH_PHY_STATUS_A100BT4_BIT )
#define GRETH_PHY_STATUS_A100BX_FD  ( 1 << GRETH_PHY_STATUS_A100BX_FD_BIT )
#define GRETH_PHY_STATUS_A100BX_HD  ( 1 << GRETH_PHY_STATUS_A100BX_HD_BIT )
#define GRETH_PHY_STATUS_10MB_FD    ( 1 << GRETH_PHY_STATUS_10MB_FD_BIT )
#define GRETH_PHY_STATUS_10MB_HD    ( 1 << GRETH_PHY_STATUS_10MB_HD_BIT )
#define GRETH_PHY_STATUS_A100BT2_FD ( 1 << GRETH_PHY_STATUS_A100BT2_FD_BIT )
#define GRETH_PHY_STATUS_A100BT2_HD ( 1 << GRETH_PHY_STATUS_A100BT2_HD_BIT )
#define GRETH_PHY_STATUS_EXT_STS    ( 1 << GRETH_PHY_STATUS_EXT_STS_BIT )
#define GRETH_PHY_STATUS_UNI        ( 1 << GRETH_PHY_STATUS_UNI_BIT )
#define GRETH_PHY_STATUS_PRESUP     ( 1 << GRETH_PHY_STATUS_PRESUP_BIT )
#define GRETH_PHY_STATUS_ANEG_COMP  ( 1 << GRETH_PHY_STATUS_ANEG_COMP_BIT )
#define GRETH_PHY_STATUS_RF         ( 1 << GRETH_PHY_STATUS_RF_BIT )
#define GRETH_PHY_STATUS_ANEG       ( 1 << GRETH_PHY_STATUS_ANEG_BIT )
#define GRETH_PHY_STATUS_LS         ( 1 << GRETH_PHY_STATUS_LS_BIT )
#define GRETH_PHY_STATUS_JABBER     ( 1 << GRETH_PHY_STATUS_JABBER_BIT )
#define GRETH_PHY_STATUS_EXT_CAP    ( 1 << GRETH_PHY_STATUS_EXT_CAP_BIT )

/* PHY ID1 */
#define GRETH_PHY_ID1_OUI_MASK 0xffff

/* PHY ID2 */
#define GRETH_PHY_ID2_OUI_BIT    10
#define GRETH_PHY_ID2_VENDOR_BIT 4
#define GRETH_PHY_ID2_REV_BIT    0

#define GRETH_PHY_ID2_OUI_MASK    0x3f
#define GRETH_PHY_ID2_VENDOR_MASK 0x3f
#define GRETH_PHY_ID2_REV_MASK    0xf

/* PHY EXT STS */
#define GRETH_PHY_EXT_STS_1000X_FD_BIT 15
#define GRETH_PHY_EXT_STS_1000X_HD_BIT 14
#define GRETH_PHY_EXT_STS_1000T_FD_BIT 13
#define GRETH_PHY_EXT_STS_1000T_HD_BIT 12

#define GRETH_PHY_EXT_STS_1000X_FD ( 1 << GRETH_PHY_EXT_STS_1000X_FD_BIT )
#define GRETH_PHY_EXT_STS_1000X_HD ( 1 << GRETH_PHY_EXT_STS_1000X_HD_BIT )
#define GRETH_PHY_EXT_STS_1000T_FD ( 1 << GRETH_PHY_EXT_STS_1000T_FD_BIT )
#define GRETH_PHY_EXT_STS_1000T_HD ( 1 << GRETH_PHY_EXT_STS_1000T_HD_BIT )

/* MII registers */
#define GRETH_MII_EXTPRT_1000FD_BIT 11
#define GRETH_MII_EXTPRT_1000HD_BIT 10

#define GRETH_MII_EXTPRT_1000FD ( 1 << GRETH_MII_EXTPRT_1000FD_BIT )
#define GRETH_MII_EXTPRT_1000HD ( 1 << GRETH_MII_EXTPRT_1000HD_BIT )

#define GRETH_MII_EXTADV_1000FD_BIT 9
#define GRETH_MII_EXTADV_1000HD_BIT 8

#define GRETH_MII_EXTADV_1000FD ( 1 << GRETH_MII_EXTADV_1000FD_BIT )
#define GRETH_MII_EXTADV_1000HD ( 1 << GRETH_MII_EXTADV_1000HD_BIT )

#define GRETH_MII_100T4_BIT   9
#define GRETH_MII_100TXFD_BIT 8
#define GRETH_MII_100TXHD_BIT 7
#define GRETH_MII_10FD_BIT    6
#define GRETH_MII_10HD_BIT    5

#define GRETH_MII_100T4   ( 1 << GRETH_MII_100T4_BIT )
#define GRETH_MII_100TXFD ( 1 << GRETH_MII_100TXFD_BIT )
#define GRETH_MII_100TXHD ( 1 << GRETH_MII_100TXHD_BIT )
#define GRETH_MII_10FD    ( 1 << GRETH_MII_10FD_BIT )
#define GRETH_MII_10HD    ( 1 << GRETH_MII_10HD_BIT )

/**
 * Resets PHY
 *
 * @param greth_dev    Pointer to GRETH Device struct
 * @param phy_dev      Pointer to PHY Device struct
 *
 * @note Calling this function is blocking until PHY indicates the reset process
 * is complete.
 */
void greth_phy_reset( struct greth_netif_state *greth_dev );

/**
 * This function starts autonegotiaon with the other PHY device connected
 * to the GRETH PHY.
 *
 * @param greth_dev    Pointer to GRETH Device struct
 * @param phy_dev      Pointer to PHY Device struct
 *
 *  @return ERR_OK if autonegotiation succesful, ERR_IF if autonegotiation failed
 */
err_t greth_phy_start_auto_negotiate(
  struct greth_netif_state *greth_dev,
  struct phy_device_info   *phy_dev
);

/**
 * This function does Autonegotiates with the PHYC device connected
 * to the PHY of GRETH. It will wait till the autonegotiation completes.
 *
 * @param greth_dev    Pointer to GRETH Device struct
 * @param phy_dev      Pointer to PHY Device struct
 *
 * @return ERR_OK if autonegotiation succesful, ERR_IF if autonegotiation failed
 *
 */
err_t greth_phy_auto_negotiate(
  struct greth_netif_state *greth_dev,
  struct phy_device_info   *phy_dev
);

/**
 * This function examines, whether autonegotiation is done and if yes, then
 * completes things post it. Must be called after greth_phy_start_auto_negotiate.
 *
 * @param greth_dev    Pointer to GRETH Device struct
 * @param phy_dev      Pointer to PHY Device struct
 *
 * @return ERR_OK if post-autonegotiation procedure succesful and done, ERR_IF if
 * post-autonegotiation procedure failed.
 */
err_t greth_phy_post_auto_negotiate(
  struct greth_netif_state *greth_dev,
  struct phy_device_info   *phy_dev
);

#ifdef __cplusplus
}
#endif
#endif /* __DRV_PHY_H */