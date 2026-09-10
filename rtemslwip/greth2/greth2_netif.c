/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @defgroup GRETH2 lwIP Driver
 * @brief This group contains documentation for code for GRETH2 lwIP driver
 */

/**
 * @file
 *
 * @ingroup GRETH2
 *
 * @brief This source file contains various functions to initialize GRETH2 lwIP
 * Driver and perform RX and Gigabit as well as non-Gigabit TX operations.
 *
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

#include "lwip/init.h"
#if LWIP_VERSION_MAJOR >= 2
#include "lwip/timeouts.h"
#else /*LWIP_VERSION_MAJOR*/
#include "lwip/timers.h"
#endif /*LWIP_VERSION_MAJOR*/
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/stats.h"
#include "netif/etharp.h"
#include <lwip/netif.h>
#include <lwip/netifapi.h>
#include <lwip/snmp.h>

#include "hw_greth2.h"
#include "eth_lwip.h"

#include "greth2.h"
#include "greth2_emac.h"
#include "greth2_ext_mdio.h"
#include "greth2_netif.h"

#include "lwipbspopts.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define IFNAME0 'g'
#define IFNAME1 'r'

#ifdef USE_JUMBO_FRAMES
#define MAX_TRANSFER_UNIT 9216
#else
#define MAX_TRANSFER_UNIT 1500
#endif

/* Used when reading from memory written by GRETH2 DMA unit */
#ifndef GRETH2_MEM_LOAD
#define GRETH2_MEM_LOAD( addr ) ( *(volatile unsigned int *) ( addr ) )
#endif

#define GRETH2_CHECKSUMS                                 \
  ( NETIF_CHECKSUM_GEN_ICMP | NETIF_CHECKSUM_GEN_ICMP6 | \
    NETIF_CHECKSUM_CHECK_ICMP | NETIF_CHECKSUM_CHECK_ICMP6 )

#define MIN_PKT_LEN 60

/**************************Forward Declarations********************************/

/* A. Interrupt and control structure related functions */
static err_t greth2_init_control_structures( struct netif *netif );
static err_t greth2_install_interrupt_handlers(
  struct greth2_netif_state *nf_state
);
SYS_IRQ_HANDLER_FNC( greth2_interrupt_handler );
static void greth2_process_irq_request( void *argument );
static void greth2_process_irq( void *argument );

/* B. initialization of GRETH2 lwIP Driver */
static void  greth2_lwip_netif_init( struct netif *netif );
err_t        greth2_init_dev_and_lwip_netif( struct netif *netif );
static err_t greth2_init_buffer_descriptors(
  struct greth2_netif_state *nf_state
);
static err_t greth2_init_hw( struct netif *netif );
static void  greth2_print_init_info( struct greth2_netif_state *nf_state );
static void  greth2_rx_pbuf_refill( struct greth2_netif_state *nf_state );

/* C. Transmission related Functions */
static err_t greth2_process_irq_tx( void *arg );
static err_t greth2_send( struct netif *netif, struct pbuf *p_start );

/* D. Reception related Functions */
static void greth2_process_irq_rx( struct netif *ntf );

/**
 * @brief Helper function to split a pointer into high and low 32-bit parts for
 * register writes
 *
 * Some GRETH2 registers require 64-bit addresses to be written as two separate
 * 32-bit values (high and low). This function takes a pointer and splits it
 * into the appropriate high and low parts based on the pointer size of the
 * platform. It ensures correct handling for both 32-bit and 64-bit
 * architectures.
 *
 * @param val The pointer value to be split and written to registers
 * @param high Pointer to store the high 32 bits of the address
 * @param low Pointer to store the low 32 bits of the address
 *
 */
static inline void greth2_split_ptr_write(
  uintptr_t          val,
  volatile uint32_t *reg_msb,
  volatile uint32_t *reg_lsb
)
{
#if __SIZEOF_POINTER__ == 8
  *reg_msb = (uint32_t) ( val >> 32 );
#elif __SIZEOF_POINTER__ == 4
  *reg_msb = 0;
#else
#error "Unsupported pointer size for GRETH2 driver"
#endif
  *reg_lsb = (uint32_t) val;
}

/**
 * @brief Initialize semaphore and thread for interrupt
 * @param netif Pointer to lwIP netif structure
 * @retval ERR_OK Successful Thread and Semaphore creation
 * @retval err_t Error in Thread or Semaphore creation
 */
static err_t greth2_init_control_structures( struct netif *netif )
{
  err_t                      res;
  sys_thread_t               tx_thread_id;
  struct greth2_netif_state *nf_state;

  nf_state = netif->state;

  res = sys_sem_new( &nf_state->irq_sem, 0 );
  if ( res != ERR_OK ) {
    sys_arch_printk(
      "\nERROR! semaphore creation error - 0x%08lx\n",
      (long) res
    );
    return res;
  }
  tx_thread_id = sys_thread_new(
    "GRLW",
    greth2_process_irq_request,
    netif,
    4096,
    1
  );
  if ( tx_thread_id == 0 ) {
    sys_arch_printk( "\nERROR! lwip interrupt thread not created" );
    return ERR_MEM;
  }
  return ERR_OK;
}

/**
 * @brief Installing Interrupt handlers for TX and RX
 *
 * @param nf_state Pointer to GRETH2 Device State Structure
 * @retval ERR_OK Successful installation of Interrupt handlers
 * @retval ERR_IF Failure to install interrupt handlers
 */
static err_t greth2_install_interrupt_handlers(
  struct greth2_netif_state *nf_state
)
{
  err_t res;

  greth2_debug_printf(
    "[DBG] greth2_install_interrupt_handlers: Setting GRETH2 "
    "Interrupt (TX/RX) Vector Number : %u\n",
    nf_state->vector
  );
  res = sys_request_irq(
    nf_state->vector,
    greth2_interrupt_handler,
    0,
    "interrupt",
    nf_state
  );
  if ( res != 0 ) {
    greth2_debug_printf(
      "[ERR] greth2_install_interrupt_handlers: Failed to install "
      "Interrupt handler; returned : %d\n",
      res
    );
    return ERR_IF;
  }
  return ERR_OK;
}

/**
 * @brief Interrupt Service Routine (ISR) for GRETH2 Ethernet driver.
 *
 * This handler is invoked whenever a GRETH2 interrupt occurs. It performs the
 * following actions:
 * - Retrieves GRETH2 driver state in the interrupt context.
 * - Signals the pending interrupt semaphore to wake up the driver task.
 *
 * @param arg Context provided by the SYS_IRQ_HANDLER_FNC macro
 *                (driver state pointer retrieved internally).
 */
SYS_IRQ_HANDLER_FNC( greth2_interrupt_handler )
{
  struct greth2_netif_state *nf_state = (struct greth2_netif_state *)
    sys_irq_handler_get_context();

  if ( nf_state != NULL ) {
    sys_arch_mask_interrupt_source( nf_state->vector );

    sys_sem_signal_from_ISR( &nf_state->irq_sem );
  }
}

/**
 * @brief Background interrupt processing task
 * - This function runs in an infinite loop, while waiting on an interrupt
 *   pending semaphore signaled by GRETH2 ISR ('greth2_interrupt_handler`)
 * - Once unblocked, it schedules actual interrupt processing routine to run in
 * lwIP TCP/IP thread context
 *
 * @param argument Pointer to lwIP netif struct
 *
 */
static void greth2_process_irq_request( void *argument )
{
  struct netif              *netif = (struct netif *) argument;
  struct greth2_netif_state *nf_state;

  nf_state = netif->state;

  for ( ;; ) {
    sys_arch_sem_wait( &nf_state->irq_sem, 0 );
    tcpip_callback( (tcpip_callback_fn) greth2_process_irq, netif );
  }
}

/**
 * @brief Process interrupt events for GRETH2 lwIP Driver
 *
 * This function is scheduled from @ref greth2_process_irq_request() and handles
 * deferred interrupt processing outside the ISR context. It:
 * - Reads and clears the interrupt status register.
 * - Checks for RX or TX events and dispatches to the appropriate handler.
 * - Unmasks the interrupt source after servicing the interrupt.
 *
 * @param argument Pointer to the lwIP netif struct.
 *
 * @note
 * - Runs in the TCP/IP thread context, not directly in the hardware ISR.
 * - RX events trigger @ref greth2_process_irq_rx().
 * - TX events trigger @ref greth2_process_irq_tx()
 */
static void greth2_process_irq( void *argument )
{
  struct netif              *netif = (struct netif *) argument;
  struct greth2_netif_state *nf_state = netif->state;
  uint32_t                   status;
  uint32_t                   ctrl;

  greth2_debug_printf( "[DBG] greth2_process_irq: Entered...\n" );
  if ( nf_state == NULL ) {
    greth2_debug_printf(
      "[MSG] greth2_process_irq: GRETH2 Driver not initialized; "
      "greth2_netif_state = NULL"
    );
    return;
  }

  status = nf_state->regs->status;
  nf_state->regs->status = status;

  ctrl = nf_state->regs->ctrl;

  if (
    ( ctrl & GRETH2_CTRL_RXIRQ ) &&
    ( status & ( GRETH2_STATUS_RXERR | GRETH2_STATUS_RXIRQ ) )
  ) {
    greth2_process_irq_rx( netif );
  }
  if (
    ( ctrl & GRETH2_CTRL_TXIRQ ) &&
    ( status & ( GRETH2_STATUS_TXERR | GRETH2_STATUS_TXIRQ ) ) &&
    ( nf_state->incoming_irq )
  ) {
    greth2_process_irq_tx( netif );
    nf_state->incoming_irq = false;
  }

  sys_arch_unmask_interrupt_source( nf_state->vector );
}

/************************Initialization****************************************/

/**
 * @brief Configure and initialize lwIP netif struct fields for GRETH2.
 *
 * This helper function fills in the essential parameters of an lwIP
 * @ref netif structure for the GRETH2 Ethernet driver. It sets the hostname,
 * interface name, output function, MTU, and link flags.
 *
 * @param netif Pointer to the lwIP netif struct to configure. The function
 *              modifies its fields in place.
 */
static void greth2_lwip_netif_init( struct netif *netif )
{
#if MIB2_STATS || LWIP_NETIF_HOSTNAME
  struct greth2_netif_state *nf_state = netif->state;
#endif
#if LWIP_CHECKSUM_CTRL_PER_NETIF
  uint16_t checksum_opts = GRETH2_CHECKSUMS;
#endif

#if LWIP_NETIF_HOSTNAME
  netif->hostname = malloc( 16 );
  snprintf( netif->hostname, 16, "greth%u", nf_state->greth2_id, );
#endif

  netif->name[ 0 ] = IFNAME0;
  netif->name[ 1 ] = IFNAME1;

  netif->output = etharp_output;

  netif->mtu = MAX_TRANSFER_UNIT;

  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_CHECKSUM_CTRL_PER_NETIF && IP_FRAG
  checksum_opts |= NETIF_CHECKSUM_GEN_UDP | NETIF_CHECKSUM_GEN_TCP |
                   NETIF_CHECKSUM_GEN_IP;
#endif /* LWIP_CHECKSUM_CTRL_PER_NETIF && IP_FRAG */

  NETIF_SET_CHECKSUM_CTRL( netif, checksum_opts );

  MIB2_INIT_NETIF(
    netif,
    snmp_ifType_ethernet_csmacd,
    ( nf_state->gb ? 1000 : ( nf_state->sp ? 100 : 10 ) ) * 1e6
  );
}

/**
 * @brief Perform overall initialization of the GRETH2 device and its lwIP
 *        netif struct.
 *
 * This function
 * - attaches the GRETH2 driver
 * - allocates and initializes the @ref greth2_netif_state structure
 * - configures the lwIP @ref netif structure
 * - sets the hardware MAC address
 * - initializes the GRETH2 hardware
 * - initializes control structures
 * - sets up buffer descriptors
 * - refills RX packet buffers for all RX descriptors
 * - installs interrupt handlers
 * It prints progress messages for each step of initialization.
 *
 * @param netif Pointer to the lwIP network interface structure associated with
 *              this GRETH2 device.
 *
 * @retval ERR_OK   Initialization completed successfully.
 * @retval ERR_IF   Driver attach failed, @ref greth2_netif_state allocation
 *                  failed, or GRETH2 device already initialized.
 * @retval other    Error code returned by low-level GRETH2 initialization
 *                  functions (e.g., @ref greth2_init_hw(),
 *                  @ref greth2_init_control_structures(),
 *                  @ref greth2_install_interrupt_handlers()).
 */
err_t greth2_init_dev_and_lwip_netif( struct netif *netif )
{
  err_t                      ret;
  struct greth2_netif_state *nf_state;
  struct lwip_greth2_hw_cfg *hw_cfg = netif->state;
  int                        greth2_attach;

  nf_state = calloc( 1, sizeof( struct greth2_netif_state ) );
  if ( !nf_state ) {
    greth2_debug_printf(
      "[ERR] greth2_init_dev_and_lwip_netif: Failed to allocate memory "
      "for GRETH2 network interface state\n"
    );
    return ERR_IF;
  }
  if ( hw_cfg ) {
    nf_state->greth2_id = hw_cfg->greth2_id;
    nf_state->reset_phy_addr = hw_cfg->phy_addr;
  }
  netif->state = nf_state;

  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: GRETH2 Network Interface "
    "State created successfully\n"
  );

  greth2_attach = rtems_lwip_greth2_driver_attach( nf_state );
  if ( greth2_attach != 0 ) {
    ret = ERR_IF;
    free( nf_state );
    return ret;
  }

  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: GRETH2 driver attached "
    "successfully\n"
  );

  greth2_set_mac_addr( nf_state, netif->hwaddr );

  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: GRETH2 MAC (Hardware) Address "
    "initialized successfully\n"
  );

  if ( ( ret = greth2_init_hw( netif ) ) != ERR_OK ) {
    greth2_debug_printf( "[ERR] greth2_init_hw: %d\n", ret );
    free( nf_state );
    return ret;
  }

  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: GRETH2 Hardware initialized "
    "successfully\n"
  );

  greth2_lwip_netif_init( netif );
  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: lwIP Network Interface "
    "initialized successfully\n"
  );

  if ( ( ret = greth2_init_control_structures( netif ) ) != ERR_OK ) {
    greth2_debug_printf( "greth2_init_control_structures (ERR): %d\n", ret );
    free( nf_state );
    return ret;
  }
  greth2_print_init_info( nf_state );

  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: GRETH2 Control Structures "
    "initialized successfully\n"
  );

  ret = greth2_init_buffer_descriptors( nf_state );
  if ( ret != ERR_OK ) {
    greth2_debug_printf( "greth2_init_buffer_descriptors (ERR): %d\n", ret );
    free( nf_state );
    return ret;
  }

  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: GRETH2 Buffer Descriptors "
    "initialized successfully\n"
  );

  for ( int i = 0; i < GRETH2_NUM_BD; i++ ) {
    greth2_rx_pbuf_refill( nf_state );
  }
  nf_state->curr_bd_in_use = 0;
  nf_state->rx_channel.head = 0;

  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: GRETH2 RX PBUFs refilled "
    "successfully\n"
  );

  if ( ( ret = greth2_install_interrupt_handlers( nf_state ) ) != ERR_OK ) {
    greth2_debug_printf(
      "greth2_install_interrupt_handlers (ERR): %d\n",
      ret
    );
    free( nf_state );
    return ret;
  }

  greth2_debug_printf(
    "greth2_init_dev_and_lwip_netif: GRETH2 Interrupt Handler "
    "initialized successfully\n"
  );

  nf_state->regs->ctrl |= GRETH2_CTRL_RXEN;

  return ERR_OK;
}

/**
 * @brief Deinitialize and free memory allocated for GRETH2 buffer descriptors.
 *
 * This function releases the memory allocated for the transmit (TX) and
 * receive (RX) buffer descriptors and their associated pbuf references.
 *
 * @param nf_state Pointer to the GRETH2 network interface state structure.
 * 
 */
static void greth2_deinit_buffer_descriptors(
  struct greth2_netif_state *nf_state
)
{
  struct tx_comm_chn *txch = &nf_state->tx_channel;
  struct rx_comm_chn *rxch = &nf_state->rx_channel;

  if ( txch->desc_array ) {
    free( txch->desc_array );
    txch->desc_array = NULL;
  }
  if ( rxch->desc_array ) {
    free( rxch->desc_array );
    rxch->desc_array = NULL;
  }
  if ( txch->tx_pbuf_ref ) {
    free( txch->tx_pbuf_ref );
    txch->tx_pbuf_ref = NULL;
  }
  if ( rxch->rx_pbuf_ref ) {
    free( rxch->rx_pbuf_ref );
    rxch->rx_pbuf_ref = NULL;
  }
}

/**
 * @brief Initializes the GRETH2 transmit (TX) and receive (RX) descriptors.
 *
 * This function sets up the TX and RX buffer descriptor (BD) circular arrays
 * for the GRETH2 driver. It allocates memory for the descriptors and associated
 * pbuf references, initializes control and address fields of descriptors, and
 * links the descriptor arrays to the GRETH2 hardware registers.
 *
 * For TX descriptors:
 * - If the MAC is non-gigabit, each descriptor's `addr` field is allocated
 * memory  with proper alignment (`MEM_ALIGN_BOUNDARY`) and `ctrl` is set to 0.
 * - If the MAC is gigabit, `addr` and `ctrl` are initialized to 0.
 *
 * For RX descriptors:
 * - Each descriptor's `ctrl` field is initialized with `GRETH2_RXD_ENABLE` and
 *   `GRETH2_RXD_IRQ`
 * - `addr` is initialized to 0.
 *
 * @param nf_state Pointer to the GRETH2 network interface state structure.
 */
static err_t greth2_init_buffer_descriptors(
  struct greth2_netif_state *nf_state
)
{
  struct tx_comm_chn *txch = &nf_state->tx_channel;
  struct rx_comm_chn *rxch = &nf_state->rx_channel;

  txch->tx_pbuf_ref = calloc( GRETH2_NUM_BD, sizeof( *txch->tx_pbuf_ref ) );
  if ( !txch->tx_pbuf_ref ) {
    greth2_debug_printf(
      "[ERR] greth2_init_buffer_descriptors: Failed to allocate memory "
      "for TX pbuf references\n"
    );
    greth2_deinit_buffer_descriptors( nf_state );
    return ERR_MEM;
  }
  rxch->rx_pbuf_ref = calloc( GRETH2_NUM_BD, sizeof( *rxch->rx_pbuf_ref ) );
  if ( !rxch->rx_pbuf_ref ) {
    greth2_debug_printf(
      "[ERR] greth2_init_buffer_descriptors: Failed to allocate memory "
      "for RX pbuf references\n"
    );
    greth2_deinit_buffer_descriptors( nf_state );
    return ERR_MEM;
  }

  txch->desc_array = (struct greth2_bd *)
    aligned_alloc( GRETH2_BUF_SIZE, GRETH2_BUF_SIZE );
  if ( !txch->desc_array ) {
    greth2_debug_printf(
      "[ERR] greth2_init_buffer_descriptors: Failed to allocate memory "
      "for TX descriptors\n"
    );
    greth2_deinit_buffer_descriptors( nf_state );
    return ERR_MEM;
  }
  rxch->desc_array = (struct greth2_bd *)
    aligned_alloc( GRETH2_BUF_SIZE, GRETH2_BUF_SIZE );
  if ( !rxch->desc_array ) {
    greth2_debug_printf(
      "[ERR] greth2_init_buffer_descriptors: Failed to allocate memory "
      "for RX descriptors\n"
    );
    greth2_deinit_buffer_descriptors( nf_state );
    return ERR_MEM;
  }
  memset( rxch->desc_array, 0, GRETH2_BUF_SIZE );
  memset( txch->desc_array, 0, GRETH2_BUF_SIZE );

  greth2_debug_printf(
    "[DBG] No. of TX Descriptors: %d\n",
    (int) GRETH2_NUM_BD
  );
  greth2_debug_printf(
    "[DBG] No. of RX Descriptors: %d\n",
    (int) GRETH2_NUM_BD
  );

  greth2_debug_printf(
    "[DBG] greth2_init_buffer_descriptors: txch->desc_array[0] => %p\n",
    txch->desc_array
  );

  greth2_debug_printf(
    "[DBG] greth2_init_buffer_descriptors: rxch->desc_array[0] => %p\n",
    rxch->desc_array
  );

  for ( int i = 0; i < GRETH2_NUM_BD; i++ ) {
    rxch->desc_array[ i ].ctrl = GRETH2_RXD_CTRL_EN | GRETH2_RXD_CTRL_IRQ;
  }

  txch->head = txch->tail = 0;
  rxch->head = rxch->tail = 0;

  nf_state->irq_treshold = GRETH2_NUM_BD * 3 / 4;

  greth2_split_ptr_write(
    (uintptr_t) rxch->desc_array,
    &nf_state->regs->rxdesc_msb,
    &nf_state->regs->rxdesc_lsb
  );

  greth2_split_ptr_write(
    (uintptr_t) txch->desc_array,
    &nf_state->regs->txdesc_msb,
    &nf_state->regs->txdesc_lsb
  );

  greth2_debug_printf(
    "[DBG] greth2_init_buffer_descriptors: after : reg->rxdesc => %p\n",
    (void *) (uintptr_t) nf_state->regs->rxdesc_lsb
  );

  return ERR_OK;
}

/**
 * @brief Initializes the GRETH2 hardware and PHY for a network interface.
 *
 * This function performs hardware initialization and PHY configuration for the
 * GRETH2 (Gigabit Ethernet) driver. It resets the GRETH2 controller, initializes
 * PHY settings, performs autonegotiation, and sets up the network interface
 * linkoutput functions according to the autonegotiated speed.
 *
 * Steps performed:
 * - Reset the GRETH2 controller and initialize control registers.
 * - Reset the PHY using greth2_phy_reset().
 * - Configure PHY control registers if necessary.
 * - Start autonegotiation using greth2_phy_auto_negotiate() and validate with
 *    greth2_phy_post_auto_negotiate().
 * - Configure link output function for gigabit or normal MAC.
 * - Enable RX, RX IRQ in GRETH2 control register and set flags of duplex and
 * speed settings
 *
 * @param netif Pointer to the LWIP network interface structure.
 *
 * @return ERR_OK if initialization was successful,
 *         ERR_IF if autonegotiation or post-autonegotiation failed.
 */
static err_t greth2_init_hw( struct netif *netif )
{
  err_t ret;

  struct greth2_netif_state *nf_state = (struct greth2_netif_state *)
                                          netif->state;

  greth2_phy_drv_init( nf_state );

  greth2_debug_printf( "[MSG] greth2_hw_init: GRETH2 Driver Reset\n" );
  greth2_phy_reset( nf_state, nf_state->reset_phy_addr );

  ret = greth2_phy_init( nf_state, nf_state->reset_phy_addr, false );
  if ( ret != ERR_OK ) {
    greth2_debug_printf(
      "[ERR] greth2_hw_init : Autonegotiation Unsuccessful.\n"
    );
    return ret;
  }

  nf_state->auto_neg = true;
  greth2_debug_printf( "[MSG] greth2_hw_init: Autonegotiation Successful\n" );
  ret = greth2_phy_post_auto_negotiate( nf_state, nf_state->reset_phy_addr );
  if ( ret != ERR_OK ) {
    greth2_debug_printf(
      "[ERR] greth2_hw_init : Post-Autonegotiation Unsuccessful.\n"
    );
    return ret;
  }
  greth2_debug_printf(
    "[MSG] greth2_hw_init: Post-Autonegotiation Successful\n"
  );

  netif->linkoutput = greth2_send;

  nf_state->regs->status = 0xFFFFFFFF;
  nf_state->regs->ctrl = GRETH2_CTRL_TXIRQ | GRETH2_CTRL_RXIRQ |
                         ( nf_state->fd ? GRETH2_CTRL_FD : 0 ) |
                         ( nf_state->sp ? GRETH2_CTRL_SP : 0 ) |
                         ( nf_state->gb ? GRETH2_CTRL_GB : 0 ) |
                         ( nf_state->caps.multicast ? GRETH2_CTRL_ME : 0 );

  return ERR_OK;
}

/**
 * @brief Prints GRETH2 driver and PHY initialization information.
 *
 * This function outputs details about the attached GRETH2 network driver and
 * the connected PHY device, including vendor, device ID, revision, and the
 * current operating mode (speed and duplex). It also prints the
 * autonegotiation time.
 *
 * @param nf_state Pointer to the GRETH2 network interface state structure.
 */
static void greth2_print_init_info( struct greth2_netif_state *nf_state )
{
  greth2_debug_printf( "\ngreth: driver attached\n" );
  if ( !nf_state->auto_neg ) {
    greth2_debug_printf(
      "Auto negotiation timed out. Selecting default config\n"
    );
  }

  if ( nf_state->caps.extern_mdio ) {
    greth2_debug_printf( "**** External MDIO PHY ****\n" );
  } else {
    greth2_debug_printf( "**** PHY ****\n" );
    greth2_debug_printf(
      "Vendor: %x   Device: %x   Revision: %d\n",
      nf_state->phy_devs[ nf_state->reset_phy_addr ].vendor,
      nf_state->phy_devs[ nf_state->reset_phy_addr ].device,
      nf_state->phy_devs[ nf_state->reset_phy_addr ].rev
    );
  }
  greth2_debug_printf( "Current Operating Mode: " );
  if ( nf_state->gb ) {
    greth2_debug_printf( "1 Gbit " );
  } else if ( nf_state->sp ) {
    greth2_debug_printf( "100 Mbit " );
  } else {
    greth2_debug_printf( "10 Mbit " );
  }
  if ( nf_state->fd ) {
    greth2_debug_printf( "Full Duplex\n" );
  } else {
    greth2_debug_printf( "Half Duplex\n" );
  }
  if ( nf_state->phy_devs[ nf_state->reset_phy_addr ].autoneg ) {
    greth2_debug_printf(
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
 * current RX buffer descriptor in the GRETH2 device. It updates the descriptor's
 * address and control fields, and advances the RX head pointer.
 *
 * @param nf_state Pointer to the GRETH2 network interface state structure.
 *
 * @note If pbuf allocation fails, the corresponding entry in array of pbuf
 *        reference is set to NULL and an error message is printed.
 * @note Control flags set for each descriptor:
 *       - GRETH2_RXD_ENABLE : Enables reception for this descriptor
 *       - GRETH2_RXD_IRQ    : Generates an interrupt when packet is received
 * @note Uses `pbuf_alloc()` to allocate memory for incoming Ethernet frames.
 */
static void greth2_rx_pbuf_refill( struct greth2_netif_state *nf_state )
{
  struct rx_comm_chn *rxch = &nf_state->rx_channel;
  unsigned int        head = rxch->head;
  struct pbuf        *p;

  p = pbuf_alloc( PBUF_RAW, PBUF_POOL_BUFSIZE, PBUF_POOL );
  if ( !p ) {
    greth2_debug_printf(
      "[ERR] greth2_rx_pbuf_refill: pbuf_alloc failed at head=%u\n",
      head
    );
    rxch->rx_pbuf_ref[ head ] = NULL;
    return;
  }

  rxch->rx_pbuf_ref[ head ] = p;

  greth2_split_ptr_write(
    (uintptr_t) ( p->payload ),
    &rxch->desc_array[ head ].addr_msb,
    &rxch->desc_array[ head ].addr_lsb
  );
  rxch->desc_array[ head ].status = 0;
  rxch->desc_array[ head ].ctrl = GRETH2_RXD_CTRL_EN | GRETH2_RXD_CTRL_IRQ;

  rxch->head = ( head + 1 ) % GRETH2_NUM_BD;
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
 * @param nf_state Pointer to the GRETH2 network interface state structure.
 */
static void greth2_tx_pbuf_reclaim( struct netif *ntf )
{
  struct greth2_netif_state *nf_state = (struct greth2_netif_state *)
                                          ntf->state;
  struct tx_comm_chn        *txch = &nf_state->tx_channel;

  while ( txch->tail != txch->head ) {
    struct greth2_bd *curr_bd = &txch->desc_array[ txch->tail ];
    uint32_t          status = GRETH2_MEM_LOAD( &curr_bd->status );

    /* BD not yet processed but head is above -> break */
    if ( !( status & GRETH2_TXD_STS_CO ) ) {
      break;
    }

    if ( __predict_false( status & GRETH2_TXD_STS_ERR ) ) {
      MIB2_STATS_NETIF_INC( ntf, ifouterrors );
      if ( status & GRETH2_TXD_STS_LC ) {
        LINK_STATS_INC( link.err );
      } else if ( status & GRETH2_TXD_STS_AL ) {
        LINK_STATS_INC( link.err );
      } else if ( status & GRETH2_TXD_STS_UE ) {
        LINK_STATS_INC( link.memerr );
      }
    }

    MIB2_STATS_NETIF_ADD(
      ntf,
      ifoutoctets,
      curr_bd->ctrl & GRETH2_TXD_CTRL_LEN_MASK
    );

    if ( !( curr_bd->ctrl & GRETH2_TXD_CTRL_MORE ) ) {
      LINK_STATS_INC( link.xmit );
      MIB2_STATS_NETIF_INC( ntf, ifoutucastpkts );
    }

    if ( txch->tx_pbuf_ref[ txch->tail ] ) {
      pbuf_free( txch->tx_pbuf_ref[ txch->tail ] );
    }

    nf_state->curr_bd_in_use = nf_state->curr_bd_in_use > 0
                                 ? nf_state->curr_bd_in_use - 1
                                 : 0;
    txch->tail = ( txch->tail + 1 ) % GRETH2_NUM_BD;
  }
}

/**
 * @brief Handles TX interrupt.
 *
 * This function processes a TX interrupt.
 * It checks if the current TX buffer descriptor has completed transmission,
 * frees the associated pbuf chain and updates the TX head pointer.
 *
 * Currently, this function is never called as TX interrupts are not enabled
 * in the driver, but it is implemented for future use if TX IRQs are enabled.
 *
 * @param arg Pointer to the lwIP network interface structure (`struct netif *`)
 *
 * @return ERR_OK if the interrupt was handled successfully,
 *         ERR_IF if the descriptor is still in use.
 */
static err_t greth2_process_irq_tx( void *arg )
{
  struct netif *netif = (struct netif *) arg;

  greth2_debug_printf( "[TX-IRQ] Entered TX IRQ...\n" );

  greth2_tx_pbuf_reclaim( netif );

  greth2_debug_printf( "[TX-IRQ] Exited TX IRQ\n" );

  return ERR_OK;
}

static void greth2_prepare_desc(
  struct greth2_netif_state *nf_state,
  struct greth2_bd          *bd,
  struct pbuf               *p,
  bool                       checksum_offload
)
{
  uint32_t ctrl = p->len & GRETH2_TXD_CTRL_LEN_MASK;

  if ( checksum_offload ) {
    ctrl |= GRETH2_TXD_CTRL_CS;
  }

  if ( p->len != p->tot_len ) {
    ctrl |= GRETH2_TXD_CTRL_MORE;
  } else if ( nf_state->incoming_irq ) {
    ctrl |= GRETH2_TXD_CTRL_IRQ;
  }

  greth2_split_ptr_write(
    (uintptr_t) p->payload,
    &bd->addr_msb,
    &bd->addr_lsb
  );

  greth2_debug_printf(
    "[DBG] txch->desc_array[txch->head].addr => %p\n",
    (void *) (uintptr_t) bd->addr_lsb
  );

  bd->status = 0;
  bd->ctrl = ctrl;
}

/**
 * @brief Sends a packet.
 *
 * This function transmits a packet represented as a pbuf chain over a GRETH2
 * MAC interface. It maps each pbuf fragment to a transmit descriptor,
 * handles padding for short packets, updates control flags, and enables
 * transmission.
 *
 * Steps performed:
 * - Validates that the current buffer descriptor is free.
 * - Counts the number of fragments in the pbuf chain.
 * - Ensures sufficient TX descriptors are available for the packet.
 * - Assigns each pbuf to a TX descriptor, sets control flags
 * (`GRETH2_TXD_MORE`, `GRETH2_TXD_IRQ`), and updates
 * `curr_bd_in_use`.
 * - Adds padding if the packet length is below `MIN_PKT_LEN`.
 * - Enables transmission by setting `GRETH2_CTRL_TXEN` in the control register.
 *
 * @param netif Pointer to the lwIP network interface structure.
 * @param p_start Pointer to the first pbuf of the packet to send.
 *
 * @return ERR_OK if the packet was successfully queued for transmission,
 *         ERR_IF if the current buffer descriptor is still in use,
 *         ERR_MEM if the pbuf chain cannot be sent due to insufficient
 *                 descriptors.
 */
static err_t greth2_send( struct netif *netif, struct pbuf *p_start )
{
  struct pbuf               *p;
  struct greth2_netif_state *nf_state = (struct greth2_netif_state *)
                                          netif->state;
  struct tx_comm_chn        *txch = &nf_state->tx_channel;
  unsigned int               diff;
  unsigned int               frags = 1;
  uint16_t                   pkt_len = p_start->tot_len;
  bool                       checksum_offload;

  greth2_debug_printf( "[DBG] greth2_send: greth2_send entered!!!\n" );

  if (
    !( GRETH2_MEM_LOAD( &txch->desc_array[ txch->head ].status ) &
       GRETH2_TXD_STS_CO ) &&
    ( GRETH2_MEM_LOAD( &txch->desc_array[ txch->head ].ctrl ) &
      GRETH2_TXD_CTRL_EN )
  ) {
    greth2_debug_printf(
      "[ERR] greth2_send: GRETH2 is still using current Buffer "
      "Descriptor.\n"
    );
    return ERR_IF;
  }

  p = p_start;
  while ( p->tot_len != p->len ) {
    frags++;
    p = p->next;
  }
  greth2_debug_printf(
    "[DBG] greth2_send: Total number of fragments => %d\n",
    frags
  );

  if ( frags > GRETH2_NUM_BD ) {
    greth2_debug_printf( "[ERR] greth2_send: PBUF-chain cannot be sent.\n" );
    return ERR_MEM;
  }

  /* If the number of descriptors needed exceeds the available descriptors, try to reclaim some */
  if ( frags > ( diff = ( GRETH2_NUM_BD - nf_state->curr_bd_in_use ) ) ) {
    greth2_tx_pbuf_reclaim( netif );

    diff = GRETH2_NUM_BD - nf_state->curr_bd_in_use;
    if ( frags > diff ) {
      greth2_debug_printf(
        "greth2_send (ERR): Required number of Buffer "
        "Descriptors (BDs) Not Available\n"
      );
      greth2_debug_printf(
        "greth2_send: Total BDs required : %d; More BDs required : %d\n",
        frags,
        diff
      );
      return ERR_BUF;
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
    greth2_debug_printf( "[MSG] greth2_send: inside while loop\n" );

    greth2_prepare_desc(
      nf_state,
      &txch->desc_array[ txch->head ],
      p,
      checksum_offload
    );

    nf_state->curr_bd_in_use++;
    txch->head = ( txch->head + 1 ) % GRETH2_NUM_BD;
    txch->tx_pbuf_ref[ txch->head ] = NULL;

    p = p->next;
  }

  greth2_debug_printf( "[MSG] greth2_send: exited while loop\n" );

  /**
   * @todo: Create new pbuf for padding and zero payload instead of using
   * uninitialized memory
   */
  if ( pkt_len < MIN_PKT_LEN ) {
    greth2_debug_printf( "[MSG] greth2_send: padding required and used\n" );
    p->len = p->tot_len = MIN_PKT_LEN;
  }

  if ( nf_state->curr_bd_in_use >= nf_state->irq_treshold ) {
    nf_state->incoming_irq = true;
  }

  greth2_prepare_desc(
    nf_state,
    &txch->desc_array[ txch->head ],
    p,
    checksum_offload
  );

  /* Make padding invisible to lwIP */
  if ( pkt_len < MIN_PKT_LEN ) {
    p->len = p->tot_len = pkt_len;
  }

  nf_state->curr_bd_in_use++;
  txch->head = ( txch->head + 1 ) % GRETH2_NUM_BD;

  /* Batch-enable all descriptors, then kick TXEN */
  for ( unsigned int i = 0; i < frags; i++ ) {
    unsigned int idx = ( txch->head + GRETH2_NUM_BD - i - 1 ) % GRETH2_NUM_BD;
    txch->desc_array[ idx ].ctrl |= GRETH2_TXD_CTRL_EN;
  }

  nf_state->regs->ctrl |= GRETH2_CTRL_TXEN;

  greth2_debug_printf( "[MSG] greth2_send: exiting greth2_send!!!\n\n" );
  return ERR_OK;
}

/************************Reception related Functions***************************/

/**
 * @brief Handles GRETH2 receive (RX) interrupts.
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
 *    - GRETH2_RXD_TOOLONG
 *    - GRETH2_RXD_DRIBBLE
 *    - GRETH2_RXD_CRCERR
 *    - GRETH2_RXD_OVERRUN
 *    - GRETH2_RXD_LENERR
 * 4. If no errors:
 *    - Updates the packet length in the pbuf.
 *    - Passes the pbuf to the network stack via `ntf->input()`.
 *    - Updates lwIP link statistics.
 * 5. If errors:
 *    - Frees the current pbuf.
 * 6. Refills RX descriptor with a new pbuf using @ref greth2_rx_pbuf_refill()
 * 7. Updates the RX descriptor control flags (`GRETH2_RXD_ENABLE`,
 * `GRETH2_RXD_IRQ`).
 *
 * @param ntf Pointer to the LWIP network interface structure.
 */
static void greth2_process_irq_rx( struct netif *ntf )
{
  struct greth2_netif_state *nf_state = (struct greth2_netif_state *)
                                          ntf->state;
  struct rx_comm_chn        *rxch = &nf_state->rx_channel;
  uint32_t                   status;

  greth2_debug_printf( "[RX-IRQ] Entered RX IRQ\n" );

  while (
    ( ( status = GRETH2_MEM_LOAD( &rxch->desc_array[ rxch->head ].status ) ) &
      GRETH2_RXD_STS_CO )
  ) {
    struct pbuf *p = rxch->rx_pbuf_ref[ rxch->head ];

    if ( __predict_false( status & GRETH2_RXD_STS_ERR ) ) {
      MIB2_STATS_NETIF_INC( ntf, ifinerrors );
      if ( status & GRETH2_RXD_STS_FT ) {
        LINK_STATS_INC( link.lenerr );
      }
      if ( status & GRETH2_RXD_STS_AE ) {
        LINK_STATS_INC( link.err );
      }
      if ( status & GRETH2_RXD_STS_CE ) {
        LINK_STATS_INC( link.chkerr );
      }
      if ( status & GRETH2_RXD_STS_OE ) {
        LINK_STATS_INC( link.memerr );
      }
      if ( status & GRETH2_RXD_STS_LE ) {
        LINK_STATS_INC( link.lenerr );
      }

      greth2_debug_printf(
        "[ERR] greth2_process_irq_rx: Error in received packet. "
        "Status=0x%08x\n",
        status
      );
      pbuf_free( p );
    } else if (
      __predict_false(
        !( status & GRETH2_RXD_STS_IF ) && ( status & GRETH2_RXD_STS_CSERR )
      )
    ) {
      MIB2_STATS_NETIF_INC( ntf, ifinerrors );
      LINK_STATS_INC( link.chkerr );

      greth2_debug_printf(
        "[ERR] greth2_process_irq_rx: Checksum error in received packet. "
        "Status=0x%08x\n",
        status
      );
      pbuf_free( p );
    } else {
      uint16_t pkt_len;

#if LWIP_CHECKSUM_CTRL_PER_NETIF
      /* IP Fragment, enable checksum calculations */
      if ( status & GRETH2_RXD_STS_IF ) {
        ntf->chksum_flags |=
          ( NETIF_CHECKSUM_CHECK_IP | NETIF_CHECKSUM_CHECK_UDP |
            NETIF_CHECKSUM_CHECK_TCP );
      }
#endif /* LWIP_CHECKSUM_CTRL_PER_NETIF */

      greth2_debug_printf( "[DBG] rxch->head => %d\n", rxch->head );
      pkt_len = status & GRETH2_RXD_STS_LEN_MASK;

      p->len = p->tot_len = pkt_len;
      if ( ntf->input( p, ntf ) != ERR_OK ) {
        LINK_STATS_INC( link.memerr );
        LINK_STATS_INC( link.drop );
        pbuf_free( p );
      }
      MIB2_STATS_NETIF_INC( ntf, ifinucastpkts );
      MIB2_STATS_NETIF_ADD( ntf, ifinoctets, pkt_len );
      LINK_STATS_INC( link.recv );

#if LWIP_CHECKSUM_CTRL_PER_NETIF
      /* Clear checksum flags */
      if ( status & GRETH2_RXD_STS_IF ) {
        ntf->chksum_flags &= ~(
          NETIF_CHECKSUM_CHECK_IP | NETIF_CHECKSUM_CHECK_UDP |
          NETIF_CHECKSUM_CHECK_TCP
        );
      }
#endif /* LWIP_CHECKSUM_CTRL_PER_NETIF */
    }

    greth2_rx_pbuf_refill( nf_state );

    greth2_debug_printf( "rxch incremented head => %d\n", rxch->head );
    greth2_debug_printf(
      "rxch new pbuf => %p\n",
      (void *) (uintptr_t) rxch->desc_array[ rxch->head ].addr_lsb
    );
  }

  nf_state->regs->ctrl |= GRETH2_CTRL_RXEN;

  greth2_debug_printf( "[RX-IRQ] Exited RX IRQ\n" );
}
