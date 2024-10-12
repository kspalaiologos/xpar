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

#ifndef _TRANS_H_
#define _TRANS_H_

#include "common.h"

// ============================================================================
//  Interlacing. Due to the Singleton bound, the error-correcting capability
//  of the (255,223)-RS code is limited to 16 errors - (N-K)/2. To increase the
//  practical burst error correcting capability we employ interlacing. For
//  example, if n = 255, this allows us to correct 4080 contiguous errors in
//  a 65025 byte block.
//
//  These methods transpose NxN and NxNxN arrays, respectively.
// ============================================================================
void trans2D(u8 * mat);
void trans3D(u8 * mat);

#endif

