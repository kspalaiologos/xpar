;   Copyright (C) 2022-2024 Kamila Szewczyk
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation; either version 3 of the License, or
;   (at your option) any later version.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program. If not, see <http://www.gnu.org/licenses/>.

section .text

global crc32c_x86_64_cpuflags
; Probe the CPU features, keeping in mind that the OS can disable
; AVX512F and AVX512VL support. The return value is a bitfield:
; rax & (1 << 0) - SSE4.2 support.    rax & (1 << 1) - PCLMULQDQ support.
; rax & (1 << 2) - AVX512F support.   rax & (1 << 3) - AVX512VL support.
crc32c_x86_64_cpuflags:
  push rbx
  mov eax, 1
  xor ecx, ecx
  cpuid
  mov eax, ecx
  shr eax, 20
  and eax, 0x01
  mov esi, ecx
  and esi, 0x02
  or esi, eax
  not ecx
  test ecx, 0x18000000
  jne .no_osxsave
  xor ecx, ecx
  xgetbv
  not eax
  test al, 0x06
  jne .no_osxsave
  mov eax, 0x07
  xor ecx, ecx
  cpuid
  mov eax, ebx
  shr eax, 14
  and eax, 0x04
  shr ebx, 31
  lea eax, [rax + 8 * rbx]
  or esi, eax
.no_osxsave:
  mov eax, esi
  pop rbx
  ret

global crc32c_small_x86_64_sse42
; A simple SSE4.2 implementation particularly suited for small buffers.
crc32c_small_x86_64_sse42:
  mov eax, edi
  cmp rdx, 8
  jb .no_byte_padding
  lea rcx, qword [rdx - 8]
  mov edi, ecx
  shr edi, 3
  inc edi
  and edi, 7
  je .no_qword_padding
  shl edi, 3
  xor r8d, r8d
.qword_loop:
  crc32 rax, qword [rsi + r8]
  add r8, 8
  cmp rdi, r8
  jne .qword_loop
  sub rdx, r8
  add rsi, r8
.no_qword_padding:
  cmp rcx, 56
  jb .no_byte_padding
.8way_qword_loop:
  crc32 rax, qword [rsi]
  crc32 rax, qword [rsi + 8]
  crc32 rax, qword [rsi + 16]
  crc32 rax, qword [rsi + 24]
  crc32 rax, qword [rsi + 32]
  crc32 rax, qword [rsi + 40]
  crc32 rax, qword [rsi + 48]
  crc32 rax, qword [rsi + 56]
  sub rdx, 64
  add rsi, 64
  cmp rdx, 7
  ja .8way_qword_loop
.no_byte_padding:
  test rdx, rdx
  je .done
  xor ecx, ecx
.byte_loop:
  crc32 eax, byte [rsi + rcx]
  inc rcx
  cmp rdx, rcx
  jne .byte_loop
.done:
  retq

; Some *cough* compilers use leading underscores. Please them.
; Is there a better solution?

global _crc32c_x86_64_cpuflags
global _crc32c_small_x86_64_sse42
_crc32c_x86_64_cpuflags:
  jmp crc32c_x86_64_cpuflags
_crc32c_small_x86_64_sse42:
  jmp crc32c_small_x86_64_sse42


; Need .GNU-stack to mark the stack as non-executable on ELF targets.
%ifdef ELF
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif