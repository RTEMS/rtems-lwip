/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @defgroup GRETH GRETH lwIP Driver
 * @brief THis group contains documentation for code for GRETH lwIP driver
 */

/**
 * @file
 *
 * @ingroup GRETH
 *
 * @brief This source file contains various functions to initialize GRETH lwIP
 * Driver and perform RX and Gigabit as well as non-Gigabit TX operations.
 *
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

#include "lwip/init.h"
#if LWIP_VERSION_MAJOR >= 2
#include "lwip/timeouts.h"
#else /*LWIP_VERSION_MAJOR*/
#include "lwip/timers.h"
#endif                /*LWIP_VERSION_MAJOR*/
#include "lwip/sys.h" /*!<includes - lwip/opt.h, lwip/err.h, arch/sys_arch.h */
#include "lwip/tcpip.h" /* includes - lwip/opt.h, lwip/api_msg.h, lwip/netifapi.h
                       , lwip/pbuf.h, lwip/api.h, lwip/sys.h, lwip/timers.h,
                       lwip/netif.h */
#include "lwip/stats.h"   /* includes - lwip/mem.h, lwip/memp.h, lwip/opt.h */
#include "netif/etharp.h" /* includes - lwip/ip.h, lwip/netif.h, lwip/ip_addr.h
                          , lwip/pbuf.h */
#include <lwip/netifapi.h>
#include <lwip/netif.h>
#include <lwip/snmp.h>

#include "greth_emac.h"
#include "greth_mdio.h"
#include "greth_netif.h"
#include "eth_lwip.h"
#include "hw_greth.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include "lwipbspopts.h"

#define IFNAME0 'g'
#define IFNAME1 'r'

#define MAX_TRANSFER_UNIT 1500

#define GRETH_CHECKSUMS                                  \
  ( NETIF_CHECKSUM_GEN_ICMP | NETIF_CHECKSUM_GEN_ICMP6 | \
    NETIF_CHECKSUM_CHECK_ICMP | NETIF_CHECKSUM_CHECK_ICMP6 )

#define RBUF_SIZE 1512

/* Used when reading from memory written by GRETH DMA unit */
#ifndef GRETH_MEM_LOAD
#define GRETH_MEM_LOAD( addr ) ( *(volatile unsigned int *) ( addr ) )
#endif

#define MIN_PKT_LEN 60

/**************************Forward Declarations********************************/

/* A. Interrupt and control structure related functions */
static err_t greth_init_control_structures( struct netif *netif );
static err_t greth_install_interrupt_handlers(
  struct greth_netif_state *nf_state
);
SYS_IRQ_HANDLER_FNC( greth_interrupt_handler );
static void greth_process_irq_request( void *argument );
static void greth_process_irq( void *argument );

/* B. initialization of GRETH lwIP Driver */
static void  greth_lwip_netif_init( struct netif *netif );
err_t        greth_init_dev_and_lwip_netif( struct netif *netif );
static err_t greth_init_buffer_descriptors(
  struct greth_netif_state *nf_state
);
static err_t greth_init_hw( struct netif *netif );
static void  greth_print_init_info( struct greth_netif_state *nf_state );
static void  greth_rx_pbuf_refill( struct greth_netif_state *nf_state );

/* C. Transmission related Functions */
static err_t greth_process_irq_tx( void *arg );
static err_t greth_process_irq_tx_gbit( void *arg );
static err_t greth_send_gbit( struct netif *netif, struct pbuf *p_start );
static err_t greth_send( struct netif *netif, struct pbuf *p );

/* D. Reception related Functions */
static void greth_process_irq_rx( struct netif *ntf );

/**
 * @brief Initialize semaphore and thread for interrupt
 * @param netif Pointer to lwIP netif structure
 * @retval ERR_OK Successful Thread and Semaphore creation
 * @retval err_t Error in Thread or Semaphore creation
 */
static err_t greth_init_control_structures( struct netif *netif )
{
  err_t                     res;
  sys_thread_t              tx_thread_id;
  struct greth_netif_state *nf_state;

  nf_state = netif->state;

  res = sys_sem_new( &nf_state->irq_sem, 0 );
  if ( res != ERR_OK ) {
    printk( "\nERROR! semaphore creation error - 0x%08lx\n", (long) res );
    return res;
  }
  tx_thread_id = sys_thread_new(
    "GRLW",
    greth_process_irq_request,
    netif,
    4096,
    1
  );
  if ( tx_thread_id == 0 ) {
    printk( "\nERROR! lwip interrupt thread not created" );
    return ERR_MEM;
  }
  return ERR_OK;
}

/**
 * @brief Installing Interrupt handlers for TX and RX
 *
 * @param nf_state Pointer to GRETH Device State Structure
 * @retval ERR_OK Successful installation of Interrupt handlers
 * @retval ERR_IF Failure to install interrupt handlers
 */
static err_t greth_install_interrupt_handlers(
  struct greth_netif_state *nf_state
)
{
  err_t res;

  greth_debug_printf(
    "[DBG] greth_install_interrupt_handlers: Setting GRETH Interrupt (TX/RX) Vector Number : %u\n",
    nf_state->vector
  );
  res = sys_request_irq(
    nf_state->vector,
    greth_interrupt_handler,
    0,
    "interrupt",
    nf_state
  );
  if ( res != 0 ) {
    greth_debug_printf(
      "[ERR] greth_install_interrupt_handlers: Failed to install Interrupt handler; returned : %d\n",
      res
    );
    return ERR_IF;
  }
  return ERR_OK;
}

/**
 * @brief Interrupt Service Routine (ISR) for GRETH Ethernet driver.
 *
 * This handler is invoked whenever a GRETH interrupt occurs. It performs the
 * following actions:
 * - Retrieves GRETH driver state in the interrupt context.
 * - Masks the interrupt source to prevent re-entry until it is serviced.
 * - Signals the pending interrupt semaphore to wake up the driver task.
 *
 * @param arg Context provided by the SYS_IRQ_HANDLER_FNC macro
 *                (driver state pointer retrieved internally).
 */
SYS_IRQ_HANDLER_FNC( greth_interrupt_handler )
{
  struct greth_netif_state *nf_state = (struct greth_netif_state *)
    sys_irq_handler_get_context();

  if ( nf_state != NULL ) {
    sys_arch_mask_interrupt_source( nf_state->vector );

    sys_sem_signal_from_ISR( &nf_state->irq_sem );
  }
}

/**
 * @brief Background interrupt processing task
 * - This function runs in an infinite loop, while waiting on an interrupt
 * pending semaphore signaled by GRETH ISR ('greth_interrupt_handler`)
 * - Once unblocked, it schedules actual interrupt processing routine to run in
 * lwIP TCP/IP thread context
 *
 * @param argument Void Pointer to lwIP netif struct
 *
 * @retval void
 */
static void greth_process_irq_request( void *argument )
{
  struct netif             *netif = (struct netif *) argument;
  struct greth_netif_state *nf_state;

  nf_state = netif->state;

  for ( ;; ) {
    sys_arch_sem_wait( &nf_state->irq_sem, 0 );
    tcpip_callback( (tcpip_callback_fn) greth_process_irq, netif );
  }
}

/**
 * @brief Process interrupt events for GRETH lwIP Driver
 *
 * This function is scheduled from @ref greth_process_irq_request() and handles
 * deferred interrupt processing outside the ISR context. It:
 * - Reads and clears the interrupt status register.
 * - Checks for RX or TX events and dispatches to the appropriate handler.
 * - Unmasks the interrupt source after servicing the interrupt.
 *
 * @param argument Pointer to the lwIP netif struct.
 *
 * @note
 * - Runs in the TCP/IP thread context, not directly in the hardware ISR.
 * - RX events trigger @ref greth_process_irq_rx().
 * - TX events trigger @ref greth_process_irq_tx() or
 *   @ref greth_process_irq_tx_gbit() depending on hardware capabilities.
 * - Interrupts are masked in the ISR ( @ref greth_interrupt_handler() ) and
 * unmasked here once processed.
 */
static void greth_process_irq( void *argument )
{
  struct netif             *netif = (struct netif *) argument;
  struct greth_netif_state *nf_state = netif->state;
  uint32_t                  status;
  uint32_t                  ctrl;

  greth_debug_printf( "[DBG] greth_process_irq: Entered...\n" );

  if ( nf_state == NULL ) {
    greth_debug_printf(
      "[MSG] greth_process_irq: GRETH Driver not initialized; greth_netif_state = NULL"
    );
    return;
  }

  status = nf_state->regs->status;
  nf_state->regs->status = status;

  ctrl = nf_state->regs->ctrl;

  if (
    ( ctrl & GRETH_CTRL_RXIRQ ) &&
    ( status & ( GRETH_STATUS_RXERR | GRETH_STATUS_RXIRQ ) )
  ) {
    greth_process_irq_rx( netif );
  }
  if (
    ( ctrl & GRETH_CTRL_TXIRQ ) &&
    ( status & ( GRETH_STATUS_TXERR | GRETH_STATUS_TXIRQ ) &&
      nf_state->incoming_irq )
  ) {
    nf_state->process_irq_tx( netif );
    nf_state->incoming_irq = false;
  }

  sys_arch_unmask_interrupt_source( nf_state->vector );
}

/************************Initialization****************************************/

/**
 * @brief Configure and initialize lwIP netif struct fields for GRETH.
 *
 * This helper function fills in the essential parameters of an lwIP
 * @ref netif structure for the GRETH Ethernet driver. It sets the hostname,
 * interface name, output function, MTU, and link flags.
 *
 * @param netif Pointer to the lwIP netif struct to configure. The function
 *              modifies its fields in place.
 */
static void greth_lwip_netif_init( struct netif *netif )
{
#if MIB2_STATS || LWIP_NETIF_HOSTNAME || LWIP_CHECKSUM_CTRL_PER_NETIF
  struct greth_netif_state *nf_state = netif->state;
#endif
#if LWIP_CHECKSUM_CTRL_PER_NETIF
  uint16_t checksum_opts = GRETH_CHECKSUMS;
#endif

#if LWIP_NETIF_HOSTNAME
  netif->hostname = malloc( 8 );
  snprintf( netif->hostname, 8, "greth%u", nf_state->greth_id );
#endif

  netif->name[ 0 ] = IFNAME0;
  netif->name[ 1 ] = IFNAME1;

  netif->output = etharp_output;

  /* maximum transfer unit */
  netif->mtu = MAX_TRANSFER_UNIT;

  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_CHECKSUM_CTRL_PER_NETIF
#if IP_FRAG
  checksum_opts |= NETIF_CHECKSUM_GEN_UDP | NETIF_CHECKSUM_GEN_TCP |
                   NETIF_CHECKSUM_GEN_IP;
#endif /* IP_FRAG */
  if ( !nf_state->gbit_mac ) {
    checksum_opts |= NETIF_CHECKSUM_CHECK_UDP | NETIF_CHECKSUM_CHECK_TCP |
                     NETIF_CHECKSUM_CHECK_IP;
  }
#endif /* LWIP_CHECKSUM_CTRL_PER_NETIF */

  NETIF_SET_CHECKSUM_CTRL( netif, checksum_opts );

  MIB2_INIT_NETIF(
    netif,
    snmp_ifType_ethernet_csmacd,
    ( nf_state->gb ? 1000 : ( nf_state->sp ? 100 : 10 ) ) * 1e6
  );
}

/**
 * @brief Perform overall initialization of the GRETH device and its lwIP
 *        netif struct.
 *
 * This function
 * - attaches the GRETH driver
 * - allocates and initializes the @ref greth_netif_state structure
 * - configures the lwIP @ref netif structure
 * - sets the hardware MAC address
 * - initializes the GRETH hardware
 * - initializes control structures
 * - sets up buffer descriptors
 * - refills RX packet buffers for all RX descriptors
 * - installs interrupt handlers
 * It prints progress messages for each step of initialization.
 *
 * @param netif Pointer to the lwIP network interface structure associated with
 *              this GRETH device.
 *
 * @retval ERR_OK   Initialization completed successfully.
 * @retval ERR_IF   Driver attach failed, @ref greth_netif_state allocation
 *                  failed, or GRETH device already initialized.
 * @retval other    Error code returned by low-level GRETH initialization
 *                  functions (e.g., @ref greth_init_hw(),
 *                  @ref greth_init_control_structures(),
 *                  @ref greth_install_interrupt_handlers()).
 */
err_t greth_init_dev_and_lwip_netif( struct netif *netif )
{
  err_t                     ret;
  struct greth_netif_state *nf_state;
  int                       greth_attach;

  nf_state = calloc( 1, sizeof( struct greth_netif_state ) );
  if ( !nf_state ) {
    greth_debug_printf(
      "[ERR] greth_init_dev_and_lwip_netif: Failed to allocate memory "
      "for GRETH network interface state\n"
    );
    return ERR_IF;
  }

  /* User provided GRETH ID */
  if ( netif->state ) {
    nf_state->greth_id = *(unsigned int *) netif->state;
  }

  netif->state = nf_state;

  greth_attach = rtems_lwip_greth_driver_attach( nf_state );
  if ( greth_attach != 0 ) {
    free( nf_state );
    return ERR_IF;
  }
  greth_debug_printf(
    "greth_init_dev_and_lwip_netif: GRETH driver attached successfully\n"
  );

  greth_set_mac_addr( nf_state, netif->hwaddr );
  greth_debug_printf(
    "greth_init_dev_and_lwip_netif: GRETH MAC (Hardware) Address initialized successfully\n"
  );

  if ( ( ret = greth_init_hw( netif ) ) != ERR_OK ) {
    greth_debug_printf( "[ERR] greth_init_hw: %d\n", ret );
    free( nf_state );
    return ret;
  }
  greth_debug_printf(
    "greth_init_dev_and_lwip_netif: GRETH Hardware initialized successfully\n"
  );

  greth_lwip_netif_init( netif );
  greth_debug_printf(
    "greth_init_dev_and_lwip_netif: lwIP Network Interface initialized successfully\n"
  );

  if ( ( ret = greth_init_control_structures( netif ) ) != ERR_OK ) {
    greth_debug_printf( "greth_init_control_structures (ERR): %d\n", ret );
    free( nf_state );
    return ret;
  }
  greth_print_init_info( nf_state );
  greth_debug_printf(
    "greth_init_dev_and_lwip_netif: GRETH Control Structures initialized successfully\n"
  );

  ret = greth_init_buffer_descriptors( nf_state );
  if ( ret != ERR_OK ) {
    greth_debug_printf( "greth_init_buffer_descriptors (ERR): %d\n", ret );
    free( nf_state );
    return ret;
  }
  greth_debug_printf(
    "greth_init_dev_and_lwip_netif: GRETH Buffer Descriptors initialized successfully\n"
  );

  nf_state->rx_channel.head = 0;
  for ( int i = 0; i < nf_state->num_bd; i++ ) {
    greth_rx_pbuf_refill( nf_state );
  }
  nf_state->curr_bd_in_use = 0;
  greth_debug_printf(
    "greth_init_dev_and_lwip_netif: GRETH RX PBUFs refilled successfully\n"
  );

  if ( ( ret = greth_install_interrupt_handlers( nf_state ) ) != ERR_OK ) {
    greth_debug_printf( "greth_install_interrupt_handlers (ERR): %d\n", ret );
    free( nf_state );
    return ret;
  }
  greth_debug_printf(
    "greth_init_dev_and_lwip_netif: GRETH Interrupt Handler initialized successfully\n"
  );

  nf_state->regs->ctrl |= GRETH_CTRL_RXEN;

  return ERR_OK;
}

/**
 * @brief Get the number of GRETH buffer descriptors.
 *
 * This function returns the number of buffer descriptors used by the GRETH
 * device. The value depends on whether the interface is operating in gigabit
 * mode or standard mode, in which case the number is determined from the
 * hardware status register.
 *
 * @param nf_state Pointer to the GRETH network interface state structure.
 *
 * @return The number of buffer descriptors as a `size_t`.
 *         - If `nf_state->gb` is true, returns 128 descriptors.
 *         - Otherwise, returns 128 multiplied by 2^N, where N is extracted
 *           from bits [27:24] of `nf_state->regs->status`.
 */
size_t get_number_of_descriptors( struct greth_netif_state *nf_state )
{
  if ( nf_state->gb ) {
    return 128;
  } else {
    return ( 128 * ( 1U << ( ( nf_state->regs->status >> 24 ) & 0xF ) ) );
  }
}

/**
 * @brief Deinitialize and free memory allocated for GRETH buffer descriptors.
 *
 * This function releases the memory allocated for the transmit (TX) and
 * receive (RX) buffer descriptors and their associated pbuf references.
 *
 * @param nf_state Pointer to the GRETH network interface state structure.
 * 
 */
static void greth_deinit_buffer_descriptors(
  struct greth_netif_state *nf_state
)
{
  if ( nf_state->tx_channel.desc_array ) {
    free( nf_state->tx_channel.desc_array );
    nf_state->tx_channel.desc_array = NULL;
  }
  if ( nf_state->rx_channel.desc_array ) {
    free( nf_state->rx_channel.desc_array );
    nf_state->rx_channel.desc_array = NULL;
  }
  if ( nf_state->tx_channel.tx_pbuf_ref ) {
    free( nf_state->tx_channel.tx_pbuf_ref );
    nf_state->tx_channel.tx_pbuf_ref = NULL;
  }
  if ( nf_state->rx_channel.rx_pbuf_ref ) {
    free( nf_state->rx_channel.rx_pbuf_ref );
    nf_state->rx_channel.rx_pbuf_ref = NULL;
  }
}

/**
 * @brief Initializes the GRETH transmit (TX) and receive (RX) descriptors.
 *
 * This function sets up the TX and RX buffer descriptor (BD) circular arrays
 * for the GRETH driver. It allocates memory for the descriptors and associated
 * pbuf references, initializes control and address fields of descriptors, and
 * links the descriptor arrays to the GRETH hardware registers.
 *
 * For TX descriptors:
 * - If the MAC is non-gigabit, each descriptor's `addr` field is allocated
 * memory  with proper alignment (`MEM_ALIGN_BOUNDARY`) and `ctrl` is set to 0.
 * - If the MAC is gigabit, `addr` and `ctrl` are initialized to 0.
 *
 * For RX descriptors:
 * - Each descriptor's `ctrl` field is initialized with `GRETH_RXD_ENABLE` and
 *   `GRETH_RXD_IRQ`
 * - `addr` is initialized to 0.
 *
 * @param nf_state Pointer to the GRETH network interface state structure.
 */
static err_t greth_init_buffer_descriptors(
  struct greth_netif_state *nf_state
)
{
  struct tx_comm_chn *txch = &nf_state->tx_channel;
  struct rx_comm_chn *rxch = &nf_state->rx_channel;
  size_t              bd_table_size;

  unsigned int num_desc = get_number_of_descriptors( nf_state );

  nf_state->num_bd = num_desc;

  bd_table_size = nf_state->num_bd * sizeof( struct greth_bd );

  greth_debug_printf(
    "[MSG] Size of all TX descriptors : %zu\n",
    bd_table_size
  );

  txch->tx_pbuf_ref = calloc( nf_state->num_bd, sizeof( *txch->tx_pbuf_ref ) );
  if ( !txch->tx_pbuf_ref ) {
    greth_debug_printf(
      "[ERR] greth_init_buffer_descriptors: Failed to allocate memory "
      "for TX pbuf references\n"
    );
    greth_deinit_buffer_descriptors( nf_state );
    return ERR_MEM;
  }

  rxch->rx_pbuf_ref = calloc( nf_state->num_bd, sizeof( *rxch->rx_pbuf_ref ) );
  if ( !rxch->rx_pbuf_ref ) {
    greth_debug_printf(
      "[ERR] greth_init_buffer_descriptors: Failed to allocate memory "
      "for RX pbuf references\n"
    );
    greth_deinit_buffer_descriptors( nf_state );
    return ERR_MEM;
  }

  txch->desc_array = (struct greth_bd *)
    aligned_alloc( bd_table_size, bd_table_size );
  if ( !txch->desc_array ) {
    greth_debug_printf(
      "[ERR] greth_init_buffer_descriptors: Failed to allocate memory "
      "for TX descriptors\n"
    );
    greth_deinit_buffer_descriptors( nf_state );
    return ERR_MEM;
  }

  rxch->desc_array = (struct greth_bd *)
    aligned_alloc( bd_table_size, bd_table_size );
  if ( !rxch->desc_array ) {
    greth_debug_printf(
      "[ERR] greth_init_buffer_descriptors: Failed to allocate memory "
      "for RX descriptors\n"
    );
    greth_deinit_buffer_descriptors( nf_state );
    return ERR_MEM;
  }
  memset( rxch->desc_array, 0, bd_table_size );
  memset( txch->desc_array, 0, bd_table_size );

  greth_debug_printf( "[DBG] No. of Descriptors: %d\n", nf_state->num_bd );

  greth_debug_printf(
    "[DBG] greth2_init_buffer_descriptors: txch->desc_array[0] => %p\n",
    txch->desc_array
  );

  greth_debug_printf(
    "[DBG] greth2_init_buffer_descriptors: rxch->desc_array[0] => %p\n",
    rxch->desc_array
  );

  txch->head = txch->tail = 0;
  rxch->head = rxch->tail = 0;

  nf_state->irq_treshold = nf_state->num_bd * 3 / 4;

  for ( int i = 0; i < nf_state->num_bd; i++ ) {
    rxch->desc_array[ i ].ctrl = GRETH_RXD_ENABLE | GRETH_RXD_IRQ;
  }

  nf_state->regs->txdesc = (uintptr_t) txch->desc_array;
  nf_state->regs->rxdesc = (uintptr_t) rxch->desc_array;

  greth_debug_printf(
    "[DBG] greth_init_buffer_descriptors: after : regs->rxdesc => 0x%08x\n",
    nf_state->regs->rxdesc
  );

  return ERR_OK;
}

/**
 * @brief Initializes the GRETH hardware and PHY for a network interface.
 *
 * This function performs hardware initialization and PHY configuration for the
 * GRETH (Gigabit Ethernet) driver. It resets the GRETH controller, initializes
 * PHY settings, performs autonegotiation, and sets up the network interface
 * linkoutput functions according to the autonegotiated speed.
 *
 * Steps performed:
 * - Reset the GRETH controller and initialize control registers.
 * - Reset the PHY using greth_phy_reset().
 * - Check MAC type (gigabit or non-gigabit)
 * - Configure PHY control registers if necessary.
 * - Start autonegotiation using greth_phy_auto_negotiate() and validate with
 *    greth_phy_post_auto_negotiate().
 * - Configure link output function for gigabit or normal MAC.
 * - Set TX interrupt generation threshold.
 * - Set maximum fragment size for non-gigabit MAC.
 * - Enable RX, RX IRQ in GRETH control register and set flags of duplex and
 * speed settings
 *
 * @param netif Pointer to the LWIP network interface structure.
 *
 * @return ERR_OK if initialization was successful,
 *         ERR_IF if autonegotiation or post-autonegotiation failed.
 */
static err_t greth_init_hw( struct netif *netif )
{
  struct greth_netif_state *nf_state = (struct greth_netif_state *)
                                         netif->state;
  uint16_t                  tmp1;
  err_t                     ret;

  struct phy_device_info *phy = &nf_state->phy_dev;

  greth_debug_printf( "[MSG] greth_hw_init: GRETH Driver Reset\n" );
  greth_phy_reset( nf_state );

  nf_state->gbit_mac = ( nf_state->regs->ctrl & GRETH_CTRL_GA ) != 0;
  nf_state->multicast = ( nf_state->regs->ctrl & GRETH_CTRL_MC ) != 0;

  if ( !nf_state->gbit_mac && ( phy->phyStatus & GRETH_PHY_STATUS_EXT_STS ) ) {
    greth_phy_reg_read( nf_state, phy->phyAddr, GRETH_PHY_MS_CTRL, &tmp1 );
  }

  if ( phy->phyStatus & GRETH_PHY_STATUS_ANEG ) {
    tmp1 = phy->phyCtrl | GRETH_PHY_CTRL_RANEG | GRETH_PHY_CTRL_ANEG;
    greth_phy_reg_write( nf_state, phy->phyAddr, GRETH_PHY_CTRL, tmp1 );
    greth_phy_reg_read(
      nf_state,
      phy->phyAddr,
      GRETH_PHY_CTRL,
      &phy->phyCtrl
    );
  }
  greth_debug_printf( "greth_hw_init: Autonegotiation Started\n" );
  ret = greth_phy_auto_negotiate( nf_state, &nf_state->phy_dev );
  if ( ret != ERR_OK ) {
    greth_debug_printf(
      "[ERR] greth_hw_init : Autonegotiation Unsuccessful.\n"
    );
    return ret;
  }
  greth_debug_printf( "[MSG] greth_hw_init: Autonegotiation Successful\n" );
  ret = greth_phy_post_auto_negotiate( nf_state, &nf_state->phy_dev );
  if ( ret != ERR_OK ) {
    greth_debug_printf(
      "[ERR] greth_hw_init : Post-Autonegotiation Unsuccessful.\n"
    );
    return ret;
  }
  greth_debug_printf(
    "[MSG] greth_hw_init: Post-Autonegotiation Successful\n"
  );

  if ( nf_state->gbit_mac ) {
    netif->linkoutput = greth_send_gbit;
    nf_state->process_irq_tx = greth_process_irq_tx_gbit;

    greth_debug_printf( "[DBG] Using greth_send_gbit as linkoutput\n" );
  } else {
    netif->linkoutput = greth_send;
    nf_state->process_irq_tx = greth_process_irq_tx;

    greth_debug_printf( "[DBG] Using greth_send as linkoutput\n" );
  }

  nf_state->regs->status = 0xFFFFFFFF;
  nf_state->regs->ctrl = GRETH_CTRL_TXIRQ | GRETH_CTRL_RXIRQ |
                         ( nf_state->fd ? GRETH_CTRL_FD : 0 ) |
                         ( nf_state->sp ? GRETH_CTRL_SP : 0 ) |
                         ( nf_state->gb ? GRETH_CTRL_GB : 0 ) |
                         ( nf_state->multicast ? GRETH_CTRL_ME : 0 );

  return ERR_OK;
}

/**
 * @brief Prints GRETH driver and PHY initialization information.
 *
 * This function outputs details about the attached GRETH network driver and
 * the connected PHY device, including vendor, device ID, revision, and the
 * current operating mode (speed and duplex). It also optionally prints the
 * autonegotiation time if GRETH_AUTONEGO_PRINT_TIME is defined.
 *
 * @param nf_state Pointer to the GRETH network interface state structure.
 */
static void greth_print_init_info( struct greth_netif_state *nf_state )
{
  greth_debug_printf( "\ngreth: driver attached\n" );
  if ( !nf_state->auto_neg ) {
    greth_debug_printf(
      "Auto negotiation timed out. Selecting default config\n"
    );
  }
  greth_debug_printf( "**** PHY ****\n" );
  greth_debug_printf(
    "Vendor: %x   Device: %x   Revision: %d\n",
    nf_state->phy_dev.vendor,
    nf_state->phy_dev.device,
    nf_state->phy_dev.rev
  );
  greth_debug_printf( "Current Operating Mode: \n" );
  if ( nf_state->gb ) {
    greth_debug_printf( "1 Gbit " );
  } else if ( nf_state->sp ) {
    greth_debug_printf( "100 Mbit " );
  } else {
    greth_debug_printf( "10 Mbit " );
  }
  if ( nf_state->fd ) {
    greth_debug_printf( "Full Duplex\n" );
  } else {
    greth_debug_printf( "Half Duplex\n" );
  }
  if ( nf_state->phy_dev.autoneg ) {
    greth_debug_printf(
      "Autonegotiation Time: %d ms\n\n",
      (uint32_t) ( nf_state->auto_neg_time.tv_sec * 1000 +
                   nf_state->auto_neg_time.tv_nsec / 1000000 )
    );
  }
}

/**
 * @brief Refills the next RX buffer descriptor with a new pbuf.
 *
 * This function allocates a pbuf from the PBUF_POOL and assigns it to the
 * current RX buffer descriptor in the GRETH device. It updates the descriptor's
 * address and control fields, and advances the RX head pointer. The last
 * descriptor in the ring is marked with the WRAP flag.
 *
 * @param nf_state Pointer to the GRETH network interface state structure.
 *
 * @note If pbuf allocation fails, the corresponding entry in array of pbuf
 *        reference is set to NULL and an error message is printed.
 * @note Control flags set for each descriptor:
 *       - GRETH_RXD_ENABLE : Enables reception for this descriptor
 *       - GRETH_RXD_IRQ    : Generates an interrupt when packet is received
 *       - GRETH_RXD_WRAP   : Set only for the last descriptor in the ring
 * @note Uses `pbuf_alloc()` to allocate memory for incoming Ethernet frames.
 */
static void greth_rx_pbuf_refill( struct greth_netif_state *nf_state )
{
  struct rx_comm_chn *rxch = &nf_state->rx_channel;
  unsigned int        head = rxch->head;
  struct pbuf        *p;

  p = pbuf_alloc( PBUF_RAW, PBUF_POOL_BUFSIZE, PBUF_POOL );
  if ( !p ) {
    greth_debug_printf(
      "[ERR] greth_rx_pbuf_refill: pbuf_alloc failed at head=%u\n",
      head
    );
    rxch->rx_pbuf_ref[ head ] = NULL;
    return;
  }

  rxch->rx_pbuf_ref[ head ] = p;

  rxch->desc_array[ head ].addr = (uintptr_t) ( p->payload );
  rxch->desc_array[ head ].ctrl = GRETH_RXD_ENABLE | GRETH_RXD_IRQ;

  rxch->head = ( head + 1 ) % nf_state->num_bd;
}

/***********************Transmission related Functions************************/

/**
 * @brief Reclaims transmitted pbufs and updates TX buffer descriptors.
 *
 * Go through the TX buffer descriptors from tail to head and check if they
 * have been processed by the hardware (indicated by the CO status bit).
 *
 * For each completed descriptor:
 * - Update transmission statistics (packets, bytes, collisions, errors).
 * - Free the associated pbuf chain if it exists.
 * - Clear the pbuf reference and decrement the count of descriptors in use.
 * - Advance the tail pointer to the next descriptor.
 *
 * @param nf_state Pointer to the GRETH network interface state structure.
 */
static void greth_tx_pbuf_reclaim( struct netif *ntf )
{
  struct greth_netif_state *nf_state = (struct greth_netif_state *) ntf->state;
  struct tx_comm_chn       *txch = &nf_state->tx_channel;

  while ( txch->tail != txch->head ) {
    struct greth_bd *curr_bd = &txch->desc_array[ txch->tail ];
    uint32_t         ctrl = GRETH_MEM_LOAD( &curr_bd->ctrl );

    /* BD not yet processed but head is above -> break */
    if ( ctrl & GRETH_TXD_ENABLE ) {
      break;
    }

    if ( __predict_false( ctrl & GRETH_TXD_ERR ) ) {
      MIB2_STATS_NETIF_INC( ntf, ifouterrors );
      if ( ctrl & GRETH_TXD_LATECOL ) {
        LINK_STATS_INC( link.err );
      } else if ( ctrl & GRETH_TXD_RETLIM ) {
        LINK_STATS_INC( link.err );
      } else if ( ctrl & GRETH_TXD_UNDERRUN ) {
        LINK_STATS_INC( link.memerr );
      }
    }

    free( (void *) (uintptr_t) curr_bd->addr );

    nf_state->curr_bd_in_use = nf_state->curr_bd_in_use > 0
                                 ? nf_state->curr_bd_in_use - 1
                                 : 0;
    txch->tail = ( txch->tail + 1 ) % nf_state->num_bd;
  }
}

/**
 * @brief Reclaims transmitted pbufs and updates TX buffer descriptors.
 *
 * Go through the TX buffer descriptors from tail to head and check if they
 * have been processed by the hardware (indicated by the CO status bit).
 *
 * For each completed descriptor:
 * - Update transmission statistics (packets, bytes, collisions, errors).
 * - Free the associated pbuf chain if it exists.
 * - Clear the pbuf reference and decrement the count of descriptors in use.
 * - Advance the tail pointer to the next descriptor.
 *
 * @param nf_state Pointer to the GRETH network interface state structure.
 */
static void greth_tx_pbuf_reclaim_gbit( struct netif *ntf )
{
  struct greth_netif_state *nf_state = (struct greth_netif_state *) ntf->state;
  struct tx_comm_chn       *txch = &nf_state->tx_channel;

  while ( txch->tail != txch->head ) {
    struct greth_bd *curr_bd = &txch->desc_array[ txch->tail ];
    uint32_t         ctrl = GRETH_MEM_LOAD( &curr_bd->ctrl );

    /* BD not yet processed but head is above -> break */
    if ( ctrl & GRETH_TXD_ENABLE ) {
      break;
    }

    if ( __predict_false( ctrl & GRETH_TXD_ERR ) ) {
      MIB2_STATS_NETIF_INC( ntf, ifouterrors );
      if ( ctrl & GRETH_TXD_LATECOL ) {
        LINK_STATS_INC( link.err );
      } else if ( ctrl & GRETH_TXD_RETLIM ) {
        LINK_STATS_INC( link.err );
      } else if ( ctrl & GRETH_TXD_UNDERRUN ) {
        LINK_STATS_INC( link.memerr );
      }
    }

    if ( txch->tx_pbuf_ref[ txch->tail ] ) {
      pbuf_free( txch->tx_pbuf_ref[ txch->tail ] );
    }
    nf_state->curr_bd_in_use = nf_state->curr_bd_in_use > 0
                                 ? nf_state->curr_bd_in_use - 1
                                 : 0;
    txch->tail = ( txch->tail + 1 ) % nf_state->num_bd;
  }
}

/**
 * @brief Handles TX interrupt for gigabit communication.
 *
 * This function processes a TX interrupt for the gigabit speed.
 * It checks if the current TX buffer descriptor has completed transmission,
 * frees the associated pbuf chain, updates the TX head pointer, and ensures
 * proper interrupt re-enabling. Re-entrancy is detected to prevent nested
 * handling of the same TX interrupt.
 *
 * @param arg Pointer to the lwIP network interface structure (`struct netif *`)
 *
 * @return ERR_OK if the interrupt was handled successfully,
 *         ERR_IF if a re-entrant interrupt is detected or the descriptor is
 *                still in use.
 */
static err_t greth_process_irq_tx_gbit( void *arg )
{
  struct netif *netif = (struct netif *) arg;

  greth_debug_printf( "[TX-GBIT-IRQ] Entered TX GBIT IRQ...\n" );

  greth_tx_pbuf_reclaim_gbit( netif );

  greth_debug_printf( "[TX-GBIT-IRQ] Exited TX GBIT IRQ\n" );

  return ERR_OK;
}

static void greth_prepare_desc(
  struct greth_netif_state *nf_state,
  struct greth_bd          *bd,
  struct pbuf              *p,
  bool                      checksum_offload
)
{
  uint32_t ctrl = p->len & GRETH_TXD_LEN;

  if ( checksum_offload ) {
    ctrl |= GRETH_TXD_CS;
  }

  if ( p->len != p->tot_len ) {
    ctrl |= GRETH_TXD_MORE;
  } else if ( nf_state->incoming_irq ) {
    ctrl |= GRETH_TXD_IRQ;
  }

  bd->addr = (uint32_t) (uintptr_t) p->payload;

  greth_debug_printf(
    "[DBG] txch->desc_array[txch->head].addr => %p\n",
    (void *) (uintptr_t) bd->addr
  );

  bd->ctrl = ctrl;
}

/**
 * @brief Sends a packet over the gigabit.
 *
 * This function transmits a packet represented as a pbuf chain over a GRETH
 * gigabit MAC interface. It maps each pbuf fragment to a transmit descriptor,
 * handles padding for short packets, updates control flags, and enables
 * transmission.
 *
 * Steps performed:
 * - Validates that the current buffer descriptor is free.
 * - Counts the number of fragments in the pbuf chain.
 * - Ensures sufficient TX descriptors are available for the packet.
 * - Assigns each pbuf to a TX descriptor, sets control flags (`GRETH_TXD_MORE`,
 * `GRETH_TXD_IRQ`, `GRETH_TXD_WRAP`), and updates `curr_bd_in_use`.
 * - Adds padding if the packet length is below `MIN_PKT_LEN`.
 * - Enables transmission by setting `GRETH_CTRL_TXEN` in the control register.
 *
 * @param netif Pointer to the lwIP network interface structure.
 * @param p_start Pointer to the first pbuf of the packet to send.
 *
 * @return ERR_OK if the packet was successfully queued for transmission,
 *         ERR_IF if re-entrancy is detected or the current buffer descriptor is
 *                still in use,
 *         ERR_MEM if the pbuf chain cannot be sent due to insufficient
 *                 descriptors.
 */
static err_t greth_send_gbit( struct netif *netif, struct pbuf *p_start )
{
  struct pbuf              *p;
  struct greth_netif_state *nf_state = (struct greth_netif_state *)
                                         netif->state;
  struct tx_comm_chn       *txch = &nf_state->tx_channel;
  unsigned int              diff;
  unsigned int              frags = 1;
  bool                      checksum_offload;
  uint16_t                  pkt_len = p_start->tot_len;

  greth_debug_printf( "[DBG] greth_send_gbit: greth_send_gbit entered!!!\n" );

  if (
    GRETH_MEM_LOAD( &txch->desc_array[ txch->head ].ctrl ) & GRETH_TXD_ENABLE
  ) {
    greth_debug_printf(
      "[ERR] greth_send_gbit: GRETH is still using current Buffer Descriptor.\n"
    );
    return ERR_IF;
  }

  p = p_start;
  while ( p->len != p->tot_len ) {
    frags++;
    p = p->next;
  }
  greth_debug_printf(
    "[DBG] greth_send_gbit: Total number of fragments => %d\n",
    frags
  );

  if ( frags > nf_state->num_bd ) {
    greth_debug_printf(
      "[ERR] greth_send_gbit: PBUF-chain cannot be sent. Increase descriptor count.\n"
    );
    return ERR_MEM;
  }

  if ( frags > ( diff = ( nf_state->num_bd - nf_state->curr_bd_in_use ) ) ) {
    greth_tx_pbuf_reclaim_gbit( netif );
    diff = nf_state->num_bd - nf_state->curr_bd_in_use;
    if ( __predict_false( frags > diff ) ) {
      greth_debug_printf(
        "greth_send_gbit (ERR): Required number of Buffer Descriptors (BDs) Not Available\n"
      );
      greth_debug_printf(
        "greth_send_gbit: Total BDs required : %d; Available BDs : %d\n",
        frags,
        diff
      );
      return ERR_MEM;
    }
  }

  if (
    NETIF_CHECKSUM_ENABLED(
      netif,
      NETIF_CHECKSUM_GEN_IP | NETIF_CHECKSUM_GEN_UDP | NETIF_CHECKSUM_GEN_TCP
    )
  ) {
    checksum_offload = false;
  } else {
    checksum_offload = true;
  }

  txch->tx_pbuf_ref[ txch->head ] = p_start;
  pbuf_ref( p_start );

  p = p_start;
  while ( p->len != p->tot_len ) {
    greth_debug_printf( "[MSG] greth_send_gbit: inside while loop\n" );

    greth_prepare_desc(
      nf_state,
      &txch->desc_array[ txch->head ],
      p,
      checksum_offload
    );

    nf_state->curr_bd_in_use++;
    txch->head = ( txch->head + 1 ) % nf_state->num_bd;
    txch->tx_pbuf_ref[ txch->head ] = NULL;

    p = p->next;
  }
  greth_debug_printf( "[MSG] greth_send_gbit: exited while loop\n" );

  /** @todo Create new pbuf for padding and zero payload instead of using unitialized memory */
  if ( pkt_len < MIN_PKT_LEN ) {
    greth_debug_printf( "[MSG] greth_send: padding required and used\n" );
    p->len = p->tot_len = MIN_PKT_LEN;
  }

  if ( nf_state->curr_bd_in_use >= nf_state->irq_treshold ) {
    nf_state->incoming_irq = true;
  }

  greth_prepare_desc(
    nf_state,
    &txch->desc_array[ txch->head ],
    p,
    checksum_offload
  );

  /* Make padding invisible to lwIP */
  if ( pkt_len < MIN_PKT_LEN ) {
    p->len = p->tot_len = pkt_len;
    pkt_len = MIN_PKT_LEN;
  }

  nf_state->curr_bd_in_use++;
  txch->head = ( txch->head + 1 ) % nf_state->num_bd;

  for ( unsigned int i = 0; i < frags; i++ ) {
    unsigned int idx = ( txch->head + nf_state->num_bd - i - 1 ) %
                       nf_state->num_bd;

    txch->desc_array[ idx ].ctrl |= GRETH_TXD_ENABLE;
  }

  nf_state->regs->ctrl |= GRETH_CTRL_TXEN;

  MIB2_STATS_NETIF_INC( netif, ifoutucastpkts );
  MIB2_STATS_NETIF_ADD( netif, ifoutoctets, pkt_len );
  LINK_STATS_INC( link.xmit );

  greth_debug_printf(
    "[MSG] greth_send_gbit: exiting greth_send_gbit!!!\n\n"
  );
  return ERR_OK;
}

/**
 * @brief Handles TX interrupt for non-gigabit speed.
 *
 * This function processes a transmit (TX) interrupt for a standard
 * (non-gigabit) communication. It checks whether the current TX descriptor has
 * completed transmission, if yes, frees the associated pbuf, updates the TX
 * head pointer, and re-enables the TX interrupt. Re-entrancy is detected to
 * prevent nested handling of the same TX interrupt.
 *
 * @param arg Pointer to the LWIP network interface structure (`struct netif *`).
 *
 * @return 0 on successful handling of the TX interrupt,
 *         ERR_IF if a re-entrant interrupt is detected or the descriptor is
 * still in use.
 */
static err_t greth_process_irq_tx( void *arg )
{
  struct netif *netif = (struct netif *) arg;

  greth_debug_printf( "[TX-IRQ] Entered TX IRQ\n" );

  greth_tx_pbuf_reclaim( netif );

  greth_debug_printf( "[TX-IRQ] Exited TX IRQ\n" );

  return ERR_OK;
}

/**
 * @brief Sends a packet over the non-gigabit.
 *
 * This function transmits a packet represented as a single pbuf over a standard
 * GRETH MAC interface. It copies the packet payload into the current transmit
 * buffer descriptor, handles padding for packets smaller than the Ethernet
 * minimum frame size, updates control flags, and enables transmission.
 *
 * Steps performed:
 * - Validates that the current TX buffer descriptor is free.
 * - Copies the payload of the pbuf chain into the descriptor buffer.
 * - Adds padding if the packet length is below `MIN_PKT_LEN`.
 * - Frees the pbuf chain after copying the data.
 * - Updates descriptor control flags (`GRETH_TXD_ENABLE`, `GRETH_TXD_WRAP`,
 * `GRETH_TXD_IRQ`) and increments `curr_bd_in_use`.
 * - Enables transmission by setting `GRETH_CTRL_TXEN` in the control register.
 * - Debug prints provide detailed logging of descriptor addresses, lengths, and
 * control flags.
 *
 * @param netif Pointer to the LWIP network interface structure.
 * @param p_start Pointer to the first pbuf of the packet to send.
 *
 * @return ERR_OK if the packet was successfully queued for transmission,
 *         ERR_IF if re-entrancy is detected or the current buffer descriptor
 *                is still in use.
 */
static err_t greth_send( struct netif *netif, struct pbuf *p_start )
{
  unsigned int              pkt_len, padlen = 0;
  struct pbuf              *p;
  struct netif             *network = netif;
  struct greth_netif_state *nf_state = (struct greth_netif_state *)
                                         network->state;
  volatile struct greth_bd *curr_bd;
  struct tx_comm_chn       *txch = &nf_state->tx_channel;
  void                     *payload;

  greth_debug_printf( "[DBG] greth_send: Entered...\n" );

  greth_debug_printf(
    "[DBG] greth_send: Current tx_idx_in_chain = %u\n",
    txch->head
  );

  curr_bd = &txch->desc_array[ txch->head ];
  if ( GRETH_MEM_LOAD( &curr_bd->ctrl ) & GRETH_TXD_ENABLE ) {
    greth_debug_printf(
      "[ERR] greth_send: GRETH is still using current Buffer Descriptor.\n"
    );
    return ERR_USE;
  }

  p = p_start;
  pbuf_ref( p_start );

  pkt_len = p_start->tot_len;
  if ( pkt_len < MIN_PKT_LEN ) {
    padlen = MIN_PKT_LEN - pkt_len;
    pkt_len = MIN_PKT_LEN;
  }

  payload = malloc( pkt_len );
  if ( !payload ) {
    return ERR_MEM;
  }

  char *cursor = payload;
  while ( p->len != p->tot_len ) {
    memcpy( (void *) cursor, (char *) p->payload, p->len );
    cursor += p->len;

    p = p->next;
  }

  memcpy( (void *) cursor, (char *) p->payload, p->len );
  cursor += p->len;
  pbuf_free( p_start );

  if ( padlen ) {
    memset( (void *) cursor, 0, padlen );
    cursor += padlen;
  }

  nf_state->curr_bd_in_use++;
  txch->head = ( txch->head + 1 ) % nf_state->num_bd;

  curr_bd->addr = (uint32_t) (uintptr_t) payload;
  curr_bd->ctrl = GRETH_TXD_ENABLE | GRETH_TXD_IRQ | pkt_len;
  greth_debug_printf(
    "[DBG] greth_send: Packet construction complete. Sending using greth_send\n"
  );

  nf_state->regs->ctrl |= GRETH_CTRL_TXEN;

  MIB2_STATS_NETIF_INC( netif, ifoutucastpkts );
  MIB2_STATS_NETIF_ADD( netif, ifoutoctets, pkt_len );
  LINK_STATS_INC( link.xmit );

  return ERR_OK;
}

/************************Reception related Functions***************************/

/**
 * @brief Handles GRETH receive (RX) interrupts.
 *
 * This function is called when an RX interrupt occurs. It processes received
 * Ethernet frames by examining the RX buffer descriptors, checking for errors,
 * updating statistics, passing valid packets to the lwIP stack, and refilling
 * the RX descriptors with new pbufs.
 *
 * Steps performed:
 * 1. Continuously checks the current RX descriptor whether not enabled.
 * 2. Reads the length and status from the descriptor control field.
 * 3. Checks for errors such as:
 *    - GRETH_RXD_TOOLONG
 *    - GRETH_RXD_DRIBBLE
 *    - GRETH_RXD_CRCERR
 *    - GRETH_RXD_OVERRUN
 *    - GRETH_RXD_LENERR
 * 4. If no errors:
 *    - Updates the packet length in the pbuf.
 *    - Passes the pbuf to the network stack via `ntf->input()`.
 *    - Updates lwIP link statistics.
 * 5. If errors:
 *    - Frees the current pbuf.
 * 6. Refills RX descriptor with a new pbuf using @ref greth_rx_pbuf_refill()
 * 7. Updates the RX descriptor control flags (`GRETH_RXD_ENABLE`,
 * `GRETH_RXD_IRQ`).
 *
 * @param ntf Pointer to the LWIP network interface structure.
 */
static void greth_process_irq_rx( struct netif *ntf )
{
  struct greth_netif_state *nf_state = (struct greth_netif_state *) ntf->state;
  struct rx_comm_chn       *rxch = &nf_state->rx_channel;
  uint32_t                  status;

  greth_debug_printf( "[RX-IRQ] Entered RX IRQ\n" );

  while (
    !( ( status = GRETH_MEM_LOAD( &rxch->desc_array[ rxch->head ].ctrl ) ) &
       GRETH_RXD_ENABLE )
  ) {
    struct pbuf *p = rxch->rx_pbuf_ref[ rxch->head ];

    if ( __predict_false( status & GRETH_RXD_ERR ) ) {
      MIB2_STATS_NETIF_INC( ntf, ifinerrors );
      if ( status & GRETH_RXD_TOOLONG ) {
        LINK_STATS_INC( link.lenerr );
      }
      if ( status & GRETH_RXD_DRIBBLE ) {
        LINK_STATS_INC( link.err );
      }
      if ( status & GRETH_RXD_CRCERR ) {
        LINK_STATS_INC( link.chkerr );
      }
      if ( status & GRETH_RXD_OVERRUN ) {
        LINK_STATS_INC( link.memerr );
      }
      if ( status & GRETH_RXD_LENERR ) {
        LINK_STATS_INC( link.lenerr );
      }
      pbuf_free( p );

      greth_debug_printf(
        "[ERR] greth_process_irq_rx: Error in received packet. "
        "Status=0x%08x\n",
        status
      );
    } else if (
      __predict_false(
        nf_state->gbit_mac && !( status & GRETH_RXD_IF ) &&
        ( status & GRETH_RXD_CSERR )
      )
    ) {
      MIB2_STATS_NETIF_INC( ntf, ifinerrors );
      LINK_STATS_INC( link.chkerr );
      pbuf_free( p );

      greth_debug_printf(
        "[ERR] greth2_process_irq_rx: Checksum error in received packet. "
        "Status=0x%08x\n",
        status
      );
    } else {
      uint16_t pkt_len;

      greth_debug_printf( "[DBG] rxch->head => %d\n", rxch->head );
      pkt_len = status & GRETH_RXD_LEN;

      MIB2_STATS_NETIF_INC( ntf, ifinucastpkts );
      MIB2_STATS_NETIF_ADD( ntf, ifinoctets, pkt_len );
      LINK_STATS_INC( link.recv );

#if LWIP_CHECKSUM_CTRL_PER_NETIF
      if ( nf_state->gbit_mac && ( status & GRETH_RXD_IF ) ) {
        ntf->chksum_flags |= NETIF_CHECKSUM_CHECK_IP |
                             NETIF_CHECKSUM_CHECK_UDP |
                             NETIF_CHECKSUM_CHECK_TCP;
      }
#endif

      p->len = p->tot_len = pkt_len;
      if ( ntf->input( p, ntf ) != ERR_OK ) {
        LINK_STATS_INC( link.memerr );
        LINK_STATS_INC( link.drop );
        pbuf_free( p );
      }

#if LWIP_CHECKSUM_CTRL_PER_NETIF
      if ( nf_state->gbit_mac && ( status & GRETH_RXD_IF ) ) {
        ntf->chksum_flags &= ~(
          NETIF_CHECKSUM_CHECK_IP | NETIF_CHECKSUM_CHECK_UDP |
          NETIF_CHECKSUM_CHECK_TCP
        );
      }
#endif
    }

    greth_rx_pbuf_refill( nf_state );

    greth_debug_printf( "rxch incremented head => %d\n", rxch->head );
    greth_debug_printf(
      "rxch new pbuf => %p\n",
      (void *) (uintptr_t) rxch->desc_array[ rxch->head ].addr
    );
  }

  nf_state->regs->ctrl |= GRETH_CTRL_RXEN;

  greth_debug_printf( "[RX-IRQ] Exited RX IRQ\n" );
}
