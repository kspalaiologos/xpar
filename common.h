/*
   Copyright (C) 2022-2024 Kamila Szewczyk

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _COMMON_H_
#define _COMMON_H_

#include "config.h"

#if !defined(HAVE_STDINT_H)
  #error "Here, have a nickel kid. Go buy yourself a real computer."
#endif

#include <stdint.h>
#include <stddef.h>

#define COPY(dst, src, n) for (int i = 0; i < n; i++) dst = src;
#define MIN(a, b) ((a) < (b) ? (a) : (b))
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int8_t i8; typedef int16_t i16; typedef int32_t i32;
typedef size_t sz;

// ============================================================================
//  Reed-Solomon code parameters (223 bytes of input, 32 bytes of parity).
// ============================================================================
#define K 223
#define N 255
#define T 32

#endif
