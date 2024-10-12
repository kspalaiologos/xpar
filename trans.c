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

#include "trans.h"

// TODO: Implement blocking and SSE-based transposition methods.
//       Needs profiling to tell if we really need the latter.
//       But we definitely need the former.

void trans2D(u8 * mat) {
  for (int i = 0; i < N; i++) {
    for (int j = i + 1; j < N; j++) {
      u8 temp = mat[i * N + j];
      mat[i * N + j] = mat[j * N + i];
      mat[j * N + i] = temp;
    }
  }
}

void trans3D(u8 * mat) {
  for (int a = 0; a < N; a++) {
    for (int b = 0; b < N; b++) {
      for (int c = a + 1; c < N; c++) {
        int index1 = a * N * N + b * N + c;
        int index2 = c * N * N + b * N + a;
        u8 temp = mat[index1];
        mat[index1] = mat[index2];
        mat[index2] = temp;
      }
    }
  }
}
