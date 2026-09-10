/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth2_ext_mdio.h
 * @brief GRETH2 External MDIO interface functions
 *
 * This header provides function prototypes for reading and writing PHY
 * registers over the MDIO interface for GRETH2 Ethernet device.
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

#ifndef GRETH2_EXT_MDIO_H
#define GRETH2_EXT_MDIO_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <lwip/sys.h>

#ifdef __cplusplus
extern "C" {
#endif

struct greth2_netif_state;

/**
 * @brief MDIO Serial Bus Controller register layout
 *
 * The MDIO controller is an MDIO bus master with built in clock generation.
 *
 * @param status MDIO Status Register.
 * @param ctrl MDIO Control Register.
 * @param apb_job MDIO APB Job Register for triggering MDIO operations and
 * checking status of operations.
 * @param init_bitmask Bitmask for selecting which PHYs to initialize during
 * autonegotiation. Read-only.
 * @param reserved Reserved space in the register map.
 * @param phy_reg PHY Specific Registers. Read-only.
 */
struct mdio_ctrl_regs {
  uint32_t status;
  uint32_t ctrl;
  uint32_t apb_job;
  uint32_t init_bitmask;
  uint32_t reserved[ ( 0x80 - 0x10 ) / 4 ];
  uint32_t phy_reg[ 32 ];
};

/**< MDIO CTRL Status Register */
#define MDIO_CTRL_STATUS_CFG_ST_BIT   3
#define MDIO_CTRL_STATUS_CFG_INIT_BIT 2
#define MDIO_CTRL_STATUS_BUSY_BIT     1
#define MDIO_CTRL_STATUS_IRQ_BIT      0

#define MDIO_CTRL_STATUS_CFG_INIT ( 1 << MDIO_CTRL_STATUS_CFG_INIT_BIT )
#define MDIO_CTRL_STATUS_BUSY     ( 1 << MDIO_CTRL_STATUS_BUSY_BIT )
#define MDIO_CTRL_STATUS_IRQ      ( 1 << MDIO_CTRL_STATUS_IRQ_BIT )

#define MDIO_CTRL_STATUS_CFG_ST_MASK 0x3

/**< MDIO CTRL Control Register */
#define MDIO_CTRL_CTRL_PER_CFG_BIT 1
#define MDIO_CTRL_CTRL_IRQ_BIT     0

#define MDIO_CTRL_CTRL_PER_CFG ( 1 << MDIO_CTRL_CTRL_PER_CFG_BIT )
#define MDIO_CTRL_CTRL_IRQ     ( 1 << MDIO_CTRL_CTRL_IRQ_BIT )

/**< MDIO CTRL APB MDIO Job Register */
#define MDIO_CTRL_APB_JOB_DATA_BIT      16
#define MDIO_CTRL_APB_JOB_REGADDR_BIT   11
#define MDIO_CTRL_APB_JOB_PHYADDR_BIT   6
#define MDIO_CTRL_APB_JOB_WRITE_BIT     5
#define MDIO_CTRL_APB_JOB_LINK_FAIL_BIT 2
#define MDIO_CTRL_APB_JOB_DONE_BIT      1
#define MDIO_CTRL_APB_JOB_ACTIVE_BIT    0

#define MDIO_CTRL_APB_JOB_WRITE     ( 1 << MDIO_CTRL_APB_JOB_WRITE_BIT )
#define MDIO_CTRL_APB_JOB_LINK_FAIL ( 1 << MDIO_CTRL_APB_JOB_LINK_FAIL_BIT )
#define MDIO_CTRL_APB_JOB_DONE      ( 1 << MDIO_CTRL_APB_JOB_DONE_BIT )
#define MDIO_CTRL_APB_JOB_ACTIVE    ( 1 << MDIO_CTRL_APB_JOB_ACTIVE_BIT )

#define MDIO_CTRL_APB_JOB_DATA_MASK    0xffff
#define MDIO_CTRL_APB_JOB_PHYADDR_MASK 0x1f
#define MDIO_CTRL_APB_JOB_REGADDR_MASK 0x1f

/**< MDIO CTRL PHY Specific Register */
#define MDIO_CTRL_PHY_AUTONEG_MD_BIT 5
#define MDIO_CTRL_PHY_AUTONEG_SP_BIT 3
#define MDIO_CTRL_PHY_INIT_STS_BIT   0

#define MDIO_CTRL_PHY_AUTONEG_MD ( 1 << MDIO_CTRL_PHY_AUTONEG_MD_BIT )

#define MDIO_CTRL_PHY_AUTONEG_SP_MASK 0x3
#define MDIO_CTRL_PHY_INIT_STS_MASK   0x7

enum mdio_ctrl_phy_init_sts {
  MDIO_CTRL_PHY_INIT_STS_NOT_PERFORMED = 0,
  MDIO_CTRL_PHY_INIT_STS_RUNNING = 1,
  MDIO_CTRL_PHY_INIT_STS_SUCCESS = 2,
  MDIO_CTRL_PHY_INIT_STS_FAILED = 3,
  MDIO_CTRL_PHY_INIT_STS_TIMEOUT = 4,
  MDIO_CTRL_PHY_INIT_STS_DEFERRED = 5
};

enum mdio_ctrl_phy_sp {
  MDIO_CTRL_PHY_SP_10 = 0,
  MDIO_CTRL_PHY_SP_100 = 1,
  MDIO_CTRL_PHY_SP_1000 = 2
};

/**
 * @brief PHY initialization status and autonegotiation settings
 *
 * This structure is used to return the PHY initialization status and
 * autonegotiation settings from the MDIO controller after autonegotiation is
 * performed. The `autoneg_fd` and `speed` fields are only valid if `status` is
 * `MDIO_CTRL_PHY_INIT_STS_SUCCESS`.
 *
 * @param status PHY initialization status.
 * @param autoneg_fd True if in full duplex mode, false if in half duplex mode.
 * @param speed Speed setting of the PHY: 0=10Mbps, 1=100Mbps, 2=1000Mbps.
 *
 */
struct mdio_ctrl_phy_cfg {
  enum mdio_ctrl_phy_init_sts status;
  bool                        autoneg_fd;
  enum mdio_ctrl_phy_sp       speed;
};

/**
 * @brief Gets the PHY initialization status from the MDIO controller.
 *
 * Read the MDIO controller PHY status register to determine the status of PHY
 * initialization during autonegotiation.
 * Optionally, the autonegotiation speed and duplex settings can be read from
 * the PHY status register.
 *
 * @param greth2_device Pointer to the GRETH2 device state structure.
 * @param phy_addr PHY address for which to get the initialization status.
 * @param cfg Mandatory pointer to a structure to be filled with PHY
 * configuration.
 *
 * @retval ERR_OK PHY initialization status read successfully.
 * @retval ERR_IF Invalid PHY address.
 *
 */
err_t mdio_ctrl_get_phy_init_status(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  struct mdio_ctrl_phy_cfg  *cfg
);

/**
 * @brief Checks if a PHY has been initialized by the MDIO controller.
 *
 * This function checks if the MDIO controller has been configured to initialize
 * the PHY at the specified address during initialization.
 *
 * @param greth2_device Pointer to the GRETH2 device state structure.
 * @param phy_addr PHY address to check. [0-31]
 * @param is_init Pointer to a boolean variable that will be set to true if the
 * PHY is initialized, false otherwise.
 *
 * @retval ERR_OK PHY initialization status read successfully.
 * @retval ERR_IF Invalid PHY address.
 */
err_t mdio_ctrl_is_phy_init(
  struct greth2_netif_state *greth2_device,
  uint32_t                   phy_addr,
  bool                      *is_init
);

#ifdef __cplusplus
}
#endif

#endif /* GRETH2_EXT_MDIO_H */
