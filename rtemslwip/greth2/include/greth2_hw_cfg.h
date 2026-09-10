/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth2_hw_cfg.h
 * @brief GRETH2 hardware configuration functions
 *
 * This header provides function prototypes for configuring the GRETH2
 * Ethernet device hardware parameters.
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

#ifndef GRETH2_HW_CFG_H
#define GRETH2_HW_CFG_H

#include <greth2_ext_mdio.h>
#include <lwip/sys.h>

/**
 * @brief GRETH2 hardware configuration for lwIP initialization
 *
 * This structure is used to pass hardware-specific configuration parameters to
 * the GRETH2 initialization function. It includes the device ID for driver
 * attachment and a callback function for external PHY initialization.
 *
 * This structure is passed to the driver at initialization by assigning it
 * to the `state` field of the lwIP `netif` struct.
 *
 * @param greth2_id The unique identifier for the GRETH2 device, used for driver
 * attachment.
 * @param phy_addr The address of the PHY device on the MDIO bus, used for
 * driver attachment and PHY configuration.
 *
 */
struct lwip_greth2_hw_cfg {
  unsigned int greth2_id;
  unsigned int phy_addr;
};

#endif