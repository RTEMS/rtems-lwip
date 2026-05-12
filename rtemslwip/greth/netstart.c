/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file netstart.c
 * @brief Ethernet LwIP basic initialization.
 *
 * This file provides the implementation of LwIP network interface
 * initialization.
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

#include "lwip/tcpip.h"
#include "lwip/netifapi.h"
#include "eth_lwip_defaults.h"
#include "eth_lwip.h"

#include "greth_netif.h"
#include "greth_emac.h"

/**
 * @brief Initialize and start the networking interface with given parameters.
 *
 * This function sets up a network interface using LwIP. It configures the
 * MAC address, initializes the TCP/IP stack, sets IP addresses, netmask,
 * and gateway, and adds the network interface to LwIP. It also sets the
 * interface as default and brings it up.
 * 
 * This function does not pass a `struct lwip_greth_hw_cfg` to the `state`
 * parameter of `netif_add()`, this means that the instance will default to
 * index 0 and the PHY address will be auto-detected.
 * If such a structure is passed, it can be discarded after `netif_add()` returns.
 *
 * @param net_interface Pointer to the LwIP network interface structure.
 * @param ipaddr        Pointer to the desired IP address (ip_addr_t).
 * @param netmask       Pointer to the desired network mask (ip_addr_t).
 * @param gateway       Pointer to the desired gateway address (ip_addr_t).
 * @param mac_addr      Pointer to 6-byte MAC address. If NULL, a default
 *                      MAC address defined by `ETH_MAC_ADDR` is used.
 *
 * @return int Status code
 * @retval ERR_OK       Networking started successfully
 * @retval NETIF_ADD_ERR Failed to add the network interface
 */
int start_networking(
  struct netif *net_interface,
  ip_addr_t    *ipaddr,
  ip_addr_t    *netmask,
  ip_addr_t    *gateway,
  uint8_t      *mac_addr
)
{
  int8_t retVal = ERR_OK;

  struct netif *netif = net_interface;
  struct netif *netif_tmp;

  u8_t default_mac[ MAC_ADDR_LEN ] = ETH_MAC_ADDR;

  if ( mac_addr == NULL ) {
    mac_addr = default_mac;
  }

  eth_lwip_set_hwaddr( netif, mac_addr );
  tcpip_init( NULL, NULL );

  ip4_addr_t ip_addr, net_mask, gw_addr;

  ip_addr.addr = ipaddr->u_addr.ip4.addr;
  net_mask.addr = netmask->u_addr.ip4.addr;
  gw_addr.addr = gateway->u_addr.ip4.addr;

  netif_tmp = netif_add(
    netif,
    &ip_addr,
    &net_mask,
    &gw_addr,
    NULL,
    greth_init_dev_and_lwip_netif,
    tcpip_input
  );

  if ( netif_tmp == NULL ) {
    return NETIF_ADD_ERR;
  }

  netif_set_default( netif );

  netifapi_netif_set_up( netif );

  return retVal;
}
