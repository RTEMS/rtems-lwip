/* SPDX-License-Identifier: BSD-2-Clause */

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

#include "greth_emac.h"
#include <bsp.h>
#include <greth.h>
#include <grlib/ambapp.h>
#include <stdio.h>

#ifdef __riscv
#include <bsp/irq.h>
#endif

#define RDA_COUNT 32
#define TDA_COUNT 32

int rtems_lwip_greth_driver_attach( struct greth_netif_state *greth_chip )
{
  struct ambapp_dev      *adev;
  struct ambapp_apb_info *apb;
  unsigned int            dev_id = greth_chip->greth_id;

  adev = (void *) (uintptr_t) ambapp_for_each(
    ambapp_plb(),
    ( OPTIONS_ALL | OPTIONS_APB_SLVS ),
    VENDOR_GAISLER,
    GAISLER_ETHMAC,
    ambapp_find_by_idx,
    &dev_id
  );
  if ( adev ) {
    apb = DEV_TO_APB( adev );

    greth_chip->regs = (struct greth_regs *) (uintptr_t) apb->start;
    #ifdef __riscv
    greth_chip->vector = RISCV_INTERRUPT_VECTOR_EXTERNAL( apb->common.irq );
    #else
    greth_chip->vector = apb->common.irq;
    #endif

    greth_debug_printf( "(DEBUG) GRETH Base Addr. : 0x%08x\n", apb->start );
  } else {
    greth_debug_printf( "GRETH device not found on AMBA bus\n" );
    return -1;
  }

  greth_debug_printf(
    "(DEBUG) Vector Number obtained : %d\n",
    apb->common.irq
  );
  greth_debug_printf( "(DEBUG) Vector Number set : %d\n", greth_chip->vector );
  greth_chip->regs->ctrl = GRETH_CTRL_RST;
  greth_debug_printf( "GRETH CTRL Reg. : 0x%08x\n", greth_chip->regs->ctrl );

  return 0;
}
