/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file greth2_lwip.c
 * @brief Ethernet LwIP interface functions for GRETH driver.
 *
 * This file provides the implementation of LwIP network interface
 * initialization, DHCP information retrieval, MAC address handling,
 * and IP address conversion utilities for the GRETH Ethernet driver.
 *
 * The main functionalities include:
 * - Starting and initializing network interfaces with static IP or DHCP.
 * - Retrieving DHCP-assigned IP address, netmask, and gateway.
 * - Displaying network interface statistics.
 * - Setting and converting MAC addresses to string format.
 * - Converting IP addresses to dotted-decimal string representation.
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

#include "eth_lwip.h"
#include "eth_lwip_defaults.h"

#include "lwip/dhcp.h"
#include "lwip/netifapi.h"
#include "lwip/stats.h"
#include "lwip/tcpip.h"

#include "netif/etharp.h"

#include <arpa/inet.h>

#include "greth2_emac.h"
#include "greth2_netif.h"

#include <inttypes.h>
#include <stdio.h>

#ifndef MAX_EMAC_INSTANCE
#define MAX_EMAC_INSTANCE 4
#endif /*MAX_EMAC_INSTANCE*/

static struct netif eth_lwip_netifs[ MAX_EMAC_INSTANCE ];
static void         eth_lwip_ip2str( ip_addr_t ip, char *ip_string );

/**
 * @brief Print the DHCP-assigned IP information for the default netif.
 *
 * This function retrieves the first LwIP network interface (instance 0)
 * and checks if a DHCP address has been assigned. If a DHCP lease exists,
 * it prints the IP address, netmask, and gateway in decimal notation.
 * Otherwise, it prints "dhcp not bound".
 *
 * @param void
 * @returns void
 */
void eth_lwip_get_dhcp_info( void )
{
  struct netif *netif = eth_lwip_get_netif( 0 );

  if ( dhcp_supplied_address( netif ) ) {
    char ip_string[ 16 ];
    eth_lwip_ip2str( netif->ip_addr, ip_string );
    printf( "Address: %s\n", ip_string );
    eth_lwip_ip2str( netif->netmask, ip_string );
    printf( "Netmask: %s\n", ip_string );
    eth_lwip_ip2str( netif->gw, ip_string );
    printf( "Gateway: %s\n", ip_string );
  } else {
    printf( "dhcp not bound\n" );
  }
}

/**
 * @brief Display the current status of the LwIP network interface.
 *
 * This function prints network interface statistics using `stats_display()`.
 * It can be used as a command-line utility or called programmatically to
 * check the current state of the network interface.
 *
 * @return Always returns 0.
 */
int eth_lwip_get_netif_status_cmd( void )
{
  stats_display();
  return 0;
}

/**
 * @brief Retrieve a pointer to a LwIP network interface instance.
 *
 * This function returns the network interface structure corresponding to
 * the given instance number. It can be used to access a specific network
 * interface managed by the Ethernet/LwIP layer.
 *
 * @param instance_number Index of the network interface to retrieve.
 *
 * @return struct netif* Pointer to the network interface structure.
 * @retval NULL If the instance number is out of range.
 */
struct netif *eth_lwip_get_netif( uint32_t instance_number )
{
  if ( instance_number >= MAX_EMAC_INSTANCE ) {
    return NULL;
  }
  return &eth_lwip_netifs[ instance_number ];
}

/**
 * @brief Convert an IP address to a dotted-decimal string.
 *
 * This function converts an `ip_addr_t` (IPv4) address to a human-readable
 * dotted-decimal string (e.g., "192.168.0.1") and stores it in the provided
 * buffer.
 *
 * @param ip     The IP address to convert.
 * @param ip_string  Pointer to a buffer of at least 16 bytes where the
 * resulting string will be stored.
 *
 * @note The function currently handles only IPv4 addresses.
 */
static void eth_lwip_ip2str( ip_addr_t ip, char *ip_string )
{
  uint32_t addr;
#if LWIP_IPV6
  addr = ip.u_addr.ip4.addr;
#else
  addr = ip.addr;
#endif

  addr = lwip_ntohl( addr );
  snprintf(
    ip_string,
    16,
    "%" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 "",
    ( addr >> 24 ),
    ( ( addr >> 16 ) & 0xff ),
    ( ( addr >> 8 ) & 0xff ),
    ( addr & 0xff )
  );
}

/**
 * @brief Set the hardware (MAC) address of a LwIP network interface.
 *
 * This function copies the given 6-byte MAC address into the network interface
 * structure and updates the hardware address length. Optionally, it prints
 * the MAC address if `DEBUG` is enabled.
 *
 * @param netif     Pointer to the LwIP network interface structure.
 * @param mac_addr  Pointer to a 6-byte array containing the MAC address.
 *
 * @note The MAC address must be 6 bytes long.
 * @note If `DEBUG` is defined, the function prints the set MAC address to
 *       the console using `eth_lwip_get_hwaddr_str()`.
 */
void eth_lwip_set_hwaddr( struct netif *netif, uint8_t *mac_addr )
{
  /* set MAC hardware address */
  for ( unsigned int i = 0; i < MAC_ADDR_LEN; i++ ) {
    netif->hwaddr[ i ] = mac_addr[ i ];
  }
  netif->hwaddr_len = MAC_ADDR_LEN;

#ifdef DEBUG
  char mac_string[ 18 ];
  eth_lwip_get_hwaddr_str( netif, mac_string );
  printf( "Setting MAC... %s\r\n", mac_string );
#endif
}

/**
 * @brief Convert the MAC address of a network interface to a string.
 *
 * This function converts the hardware (MAC) address of the given LwIP
 * network interface into a human-readable string in the format
 * "XX:XX:XX:XX:XX:XX" using uppercase hexadecimal digits.
 *
 * @param netif    Pointer to the LwIP network interface structure.
 * @param mac_string   Pointer to a buffer where the resulting string will be
 * stored The buffer must be at least 18 bytes to hold the full MAC string plus
 * the null terminator.
 */
void eth_lwip_get_hwaddr_str( struct netif *netif, char *mac_string )
{
  unsigned int outindex = 0;

  for ( unsigned int index = 0; index < netif->hwaddr_len; index++ ) {
    char ch;
    if ( index ) {
      mac_string[ outindex++ ] = ':';
    }
    ch = ( netif->hwaddr[ index ] >> 4 );
    mac_string[ outindex++ ] = ( ch < 10 ) ? ( ch + '0' ) : ( ch - 10 + 'A' );
    ch = ( netif->hwaddr[ index ] & 0xf );
    mac_string[ outindex++ ] = ( ch < 10 ) ? ( ch + '0' ) : ( ch - 10 + 'A' );
  }
  mac_string[ outindex ] = '\0';
}
