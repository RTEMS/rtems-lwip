/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file lwipbspopts.h
 * @brief GRETH/LwIP BSP configuration options
 *
 * This header defines LwIP buffer pool sizes, ARP configuration, and
 * related Ethernet BSP options for GRETH-based network interfaces.
 * It allows selecting dynamic or static ARP entries, ARP timer intervals,
 * and pbuf pool sizes.
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

#include <legacy_lwipopts.h>

#ifndef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE 1600 /* Number of pbufs in the pool */
#else
/* 128 RX buffers, 128 TX buffers minimum */
#if PBUF_POOL_SIZE < 256
#error "PBUF_POOL_SIZE must be at least 256 for GRETH lwIP driver"
#endif /* PBUF_POOL_SIZE < 256 */
#endif /* PBUF_POOL_SIZE */

#define PBUF_POOL_BUFSIZE 2048 /**< Size of each pbuf buffer */

#ifndef TCP_MSS
#define TCP_MSS 1460
#endif

#ifndef TCP_WND
#define TCP_WND 65535
#endif /* TCP_WND */

#ifndef TCP_SND_BUF
#define TCP_SND_BUF 65535
#endif /* TCP_SND_BUF */

#ifndef LWIP_TCP_SACK_OUT
#define LWIP_TCP_SACK_OUT 1
#endif /* LWIP_TCP_SACK_OUT */

#ifndef TCP_QUEUE_OOSEQ
#define TCP_QUEUE_OOSEQ 1
#endif /* TCP_QUEUE_OOSEQ */

#ifndef IP_FORWARD
#define IP_FORWARD 1
#endif /* IP_FORWARD */

#ifndef IP_FRAG
#define IP_FRAG 1
#endif /* IP_FRAG */

#define GRETH 1
