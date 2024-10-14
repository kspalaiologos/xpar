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

.text

.globl crc32c_x86_64_cpuflags
.type crc32c_x86_64_cpuflags, @function
/*
  Probe the CPU features, keeping in mind that the OS can disable
  AVX512F and AVX512VL support. The return value is a bitfield:
  rax & (1 << 0) - SSE4.2 support.    rax & (1 << 1) - PCLMULQDQ support.
  rax & (1 << 2) - AVX512F support.   rax & (1 << 3) - AVX512VL support.
*/
crc32c_x86_64_cpuflags:
  pushq %rbx
  movl $1, %eax
  xorl %ecx, %ecx
  cpuid
  movl %ecx, %eax
  shrl $20, %eax
  andl $0x01, %eax
  movl %ecx, %esi
  andl $0x02, %esi
  orl %eax, %esi
  notl %ecx
  testl $0x18000000, %ecx
  jne .no_osxsave
  xorl %ecx, %ecx
  xgetbv
  notl %eax
  testb $0x06, %al
  jne .no_osxsave
  movl $0x07, %eax
  xorl %ecx, %ecx
  cpuid
  movl %ebx, %eax
  shrl $14, %eax
  andl $0x04, %eax
  shrl $31, %ebx
  leal (%rax,%rbx,8), %eax
  orl %eax, %esi
.no_osxsave:
  movl %esi, %eax
  popq %rbx
  retq

.globl crc32c_small_x86_64_sse42
.type crc32c_small_x86_64_sse42, @function
/* A simple SSE4.2 implementation particularly suited for
   small buffer sizes. */
crc32c_small_x86_64_sse42:
  movl %edi, %eax
  cmpq $8, %rdx
  jb .no_byte_padding
  leaq -8(%rdx), %rcx
  movl %ecx, %edi
  shrl $3, %edi
  incl %edi
  andl $7, %edi
  je .no_qword_padding
  shll $3, %edi
  xorl %r8d, %r8d
.qword_loop:
  movl %eax, %eax
  crc32q (%rsi,%r8), %rax
  addq $8, %r8
  cmpq %r8, %rdi
  jne .qword_loop
  subq %r8, %rdx
  addq %r8, %rsi
.no_qword_padding:
  cmpq $56, %rcx
  jb .no_byte_padding
.8way_qword_loop:
  movl %eax, %eax
  crc32q   (%rsi), %rax
  crc32q  8(%rsi), %rax
  crc32q 16(%rsi), %rax
  crc32q 24(%rsi), %rax
  crc32q 32(%rsi), %rax
  crc32q 40(%rsi), %rax
  crc32q 48(%rsi), %rax
  crc32q 56(%rsi), %rax
  addq $-64, %rdx
  addq $64, %rsi
  cmpq $7, %rdx
  ja .8way_qword_loop
.no_byte_padding:
  testq %rdx, %rdx
  je .done
  xorl %ecx, %ecx
.byte_loop:
  crc32b (%rsi,%rcx), %eax
  incq %rcx
  cmpq %rcx, %rdx
  jne .byte_loop
.done:
  retq

.section .note.GNU-stack,"",@progbits
