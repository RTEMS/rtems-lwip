/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Copyright (C) 2025 Prithvi Tambewagh
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

#include "greth2_emac.h"
#include "greth2_ext_mdio.h"
#include <bsp.h>
#include <greth2.h>
#include <grlib/ambapp.h>
#include <stdio.h>

#ifdef __riscv
#include <bsp/irq.h>
#endif

static void greth2_read_capabilities( struct greth2_netif_state *nf_state )
{
  uint32_t caps_reg = nf_state->regs->capabilities;

  nf_state->caps.extern_mdio = ( caps_reg & GRETH2_CAP_EXT_MDIO ) != 0;
  nf_state->caps.multicast = ( caps_reg & GRETH2_CAP_MC ) != 0;
  nf_state->caps.mdio_irq = ( caps_reg & GRETH2_CAP_IRQ_MDIO ) != 0;
  nf_state->caps.timestamping = ( caps_reg & GRETH2_CAP_TS ) != 0;
}

static int rtems_lwip_ext_mdio_attach( struct greth2_netif_state *greth2_chip )
{
  struct ambapp_dev      *adev;
  struct ambapp_apb_info *apb;

  adev = (void *) (uintptr_t) ambapp_for_each(
    ambapp_plb(),
    ( OPTIONS_ALL | OPTIONS_APB_SLVS ),
    VENDOR_GAISLER,
    GAISLER_MDIO_CTRL,
    ambapp_find_by_idx,
    NULL
  );

  if ( !adev ) {
    return -1;
  }

  apb = DEV_TO_APB( adev );
  greth2_chip->ext_mdio_regs = (struct mdio_ctrl_regs *) (uintptr_t)
                                 apb->start;

#if defined( LEON3_IRQAMP_IRQMAP )
  greth2_chip->ext_mdio_vector = leon3_irqmap_get_unchecked( apb->common.irq );
#elif defined( __riscv )
  greth2_chip->ext_mdio_vector = RISCV_INTERRUPT_VECTOR_EXTERNAL(
    apb->common.irq
  );
#else
  greth2_chip->ext_mdio_vector = apb->common.irq;
#endif

  greth2_chip->ext_mdio_regs->ctrl = MDIO_CTRL_CTRL_PER_CFG;

  return 0;
}

int rtems_lwip_greth2_driver_attach( struct greth2_netif_state *greth2_chip )
{
  struct ambapp_dev      *adev;
  struct ambapp_apb_info *apb;
  unsigned int            idx = greth2_chip->greth2_id;

  adev = (void *) (uintptr_t) ambapp_for_each(
    ambapp_plb(),
    ( OPTIONS_ALL | OPTIONS_APB_SLVS ),
    VENDOR_GAISLER,
    GAISLER_GRETH2,
    ambapp_find_by_idx,
    (void *) &idx
  );

  if ( adev ) {
    apb = DEV_TO_APB( adev );

    greth2_chip->regs = (void *) (uintptr_t) apb->start;
    greth2_debug_printf(
      "(DEBUG) GRETH Base Addr. : %p\n",
      greth2_chip->regs
    );
#ifdef __riscv
    greth2_chip->vector = RISCV_INTERRUPT_VECTOR_EXTERNAL( apb->common.irq );
#else
    greth2_chip->vector = apb->common.irq;
#endif

  } else {
    greth2_debug_printf( "GRETH 2 device not found on AMBA bus\n" );
    return -1;
  }

  greth2_chip->regs->ctrl = GRETH2_CTRL_RST;

  greth2_read_capabilities( greth2_chip );
  if ( greth2_chip->caps.extern_mdio ) {
    if ( rtems_lwip_ext_mdio_attach( greth2_chip ) != 0 ) {
      greth2_debug_printf( "Failed to attach external MDIO controller\n" );
      return -1;
    }
  } else {
    if ( greth2_chip->reset_phy_addr == 0 ) {
      greth2_chip->reset_phy_addr = ( greth2_chip->regs->mdio_ctrl >>
                                      GRETH2_MDIO_PHYADDR_BIT ) &
                                    GRETH2_MDIO_PHYADDR_MASK;
    }
  }

  greth2_debug_printf(
    "(DEBUG) Vector Number obtained : %d\n",
    apb->common.irq
  );
  greth2_debug_printf(
    "(DEBUG) Vector Number set : %d\n",
    greth2_chip->vector
  );
  greth2_chip->regs->ctrl = GRETH2_CTRL_RST;
  greth2_debug_printf( "GRETH CTRL Reg. : 0x%08x\n", greth2_chip->regs->ctrl );

  return 0;
}
