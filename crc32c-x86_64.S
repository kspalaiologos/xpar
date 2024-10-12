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

   See: ”Fast CRC Computation for Generic Polynomials Using PCLMULQDQ
        Instruction” (V. Gopal, E. Ozturk, et al., 2009)
*/

.text
.extern crc32c_tabular

.globl crc32_x86_64_choose
.type crc32_x86_64_choose, @function
crc32_x86_64_choose:
  xorl %eax, %eax
  xchgq %rbx, %rsi
  cpuid
  xchgq %rbx, %rsi
  testl %eax, %eax
  je .unsupported
  movl $1, %eax
  xchgq %rbx, %rsi
  cpuid
  xchgq %rbx, %rsi
  notl %ecx
  testl $0x100002, %ecx
  jne .unsupported
  leaq crc32c_sse42(%rip), %rax
  retq
.unsupported:
  leaq crc32c_tabular(%rip), %rax
  retq

.globl crc32c_sse42
.type crc32c_sse42, @function

/* Vaguely based on Peter Cawley's fast-crc32 (MIT). This version avoids
   some unnecessary compiler unrolling and reduces icache bloat in
   colder paths by using linear time algorithms, overall improving
   the performance of the program. */
xnmp: /* CRC(1 << n) */
  movl %edi, %ecx
  notb %cl
  movl $1, %eax
  shll %cl, %eax
  testb $32, %dil
  je .no_low_bits
  xorl %ecx, %ecx
  crc32l %ecx, %eax
.no_low_bits:
  cmpl $64, %edi
  jb .end_xnmp
  shrl $6, %edi
  leal -1(%rdi), %ecx
  movl %edi, %edx
  andl $7, %edx
  je .xnmp_process_1way
  xorl %r8d, %r8d
  xorl %esi, %esi
.xnmp_1way_loop:
  movl %eax, %eax
  crc32q %r8, %rax
  incl %esi
  cmpl %esi, %edx
  jne .xnmp_1way_loop
  subl %esi, %edi
.xnmp_process_1way:
  cmpl $7, %ecx
  jb .end_xnmp
  xorl %ecx, %ecx
.xnmp_8way_loop:
  movl %eax, %eax
  crc32q %rcx, %rax
  crc32q %rcx, %rax
  crc32q %rcx, %rax
  crc32q %rcx, %rax
  crc32q %rcx, %rax
  crc32q %rcx, %rax
  crc32q %rcx, %rax
  crc32q %rcx, %rax
  addl $-8, %edi
  jne .xnmp_8way_loop
.end_xnmp:
  retq

.k1: .long 0x47db8317, 0x47db8317
.k2: .long 0x493c7d27, 0x493c7d27
.k3: .long 0xba4fc28e, 0xba4fc28e
crc32c_sse42:
  pushq %rbp
  pushq %r15
  pushq %r14
  pushq %r13
  pushq %r12
  pushq %rbx
  subq $56, %rsp
  movq %rdx, %r14
  movq %rsi, %rbx
  movl %edi, %r15d
  notl %r15d
  testq %rdx, %rdx
  sete %al
  testb $7, %bl
  sete %cl
  orb %al, %cl
  jne .crc32_aligned1
  movq %rbx, %rax
.crc32_align1:
  movq %r14, %rcx
  crc32b (%rbx), %r15d
  incq %rbx
  decq %r14
  incq %rax
  cmpq $1, %rcx
  je .crc32_aligned8
  movl %eax, %ecx
  andl $7, %ecx
  jne .crc32_align1
  jmp .crc32_aligned8
.crc32_aligned1:
  movq %rbx, %rax
.crc32_aligned8:
  cmpq $8, %r14
  jb .crc32_block192
  andl $8, %eax
  je .crc32_block192
  movl %r15d, %r15d
  crc32q (%rbx), %r15
  addq $8, %rbx
  addq $-8, %r14
.crc32_block192:
  cmpq $192, %r14
  jb .crc32_do_remainder
  leaq -8(%r14), %rax
  shrq $3, %rax
  movabsq $0x1642c8590b21642d, %rcx
  mulq %rcx
  shrq %rdx
  leaq (,%rdx,8), %rax
  leaq (%rax,%rax,2), %r12
  movdqu (%rbx), %xmm6
  movdqu 16(%rbx), %xmm5
  movdqu 32(%rbx), %xmm4
  movdqu 48(%rbx), %xmm1
  movdqu 64(%rbx), %xmm3
  movdqu 80(%rbx), %xmm0
  movdqu 96(%rbx), %xmm2
  movd %r15d, %xmm11
  pxor %xmm6, %xmm11
  imulq $112, %rdx, %rax
  leaq -184(%r14), %rcx
  movq %rdx, %rsi
  shlq $4, %rsi
  leaq (%rsi,%rsi,2), %r8
  cmpq $192, %rcx
  movq %rdx, 24(%rsp)
  jb .crc32_block192_early_exit
  movq %rdx, %rcx
  shlq $7, %rcx
  leaq (%rcx,%rdx,8), %rcx
  shlq $5, %rdx
  leaq (%rdx,%rdx,4), %rdx
  leaq 208(%rbx), %rsi
  xorl %r13d, %r13d
  movl $0x2ad91c30, %edi
  movq %rdi, %xmm6
  pmovsxdq .k1(%rip), %xmm7
  xorl %ebp, %ebp
  xorl %r15d, %r15d
.crc32_block192_loop:
  movdqa %xmm11, %xmm9
  pclmulqdq $0x00, %xmm6, %xmm9
  movdqa %xmm11, %xmm8
  pclmulqdq $0x11, %xmm7, %xmm8
  movdqu -96(%rsi), %xmm11
  pxor %xmm9, %xmm11
  movdqa %xmm5, %xmm9
  pclmulqdq $0x00, %xmm6, %xmm9
  movdqa %xmm5, %xmm10
  pclmulqdq $0x11, %xmm7, %xmm10
  pxor %xmm8, %xmm11
  movdqu -80(%rsi), %xmm5
  pxor %xmm9, %xmm5
  movdqa %xmm4, %xmm8
  pclmulqdq $0x00, %xmm6, %xmm8
  movdqa %xmm4, %xmm9
  pclmulqdq $0x11, %xmm7, %xmm9
  pxor %xmm10, %xmm5
  movdqu -64(%rsi), %xmm4
  pxor %xmm8, %xmm4
  movdqa %xmm1, %xmm8
  pclmulqdq $0x00, %xmm6, %xmm8
  movdqa %xmm1, %xmm10
  pclmulqdq $0x11, %xmm7, %xmm10
  pxor %xmm9, %xmm4
  movdqu -48(%rsi), %xmm1
  pxor %xmm8, %xmm1
  movdqa %xmm3, %xmm8
  pclmulqdq $0x00, %xmm6, %xmm8
  movdqa %xmm3, %xmm9
  pclmulqdq $0x11, %xmm7, %xmm9
  pxor %xmm10, %xmm1
  movdqu -32(%rsi), %xmm3
  pxor %xmm8, %xmm3
  movdqa %xmm0, %xmm8
  pclmulqdq $0x00, %xmm6, %xmm8
  movdqa %xmm0, %xmm10
  pclmulqdq $0x11, %xmm7, %xmm10
  pxor %xmm9, %xmm3
  movdqu -16(%rsi), %xmm0
  pxor %xmm8, %xmm0
  movdqa %xmm2, %xmm8
  pclmulqdq $0x00, %xmm6, %xmm8
  movdqa %xmm2, %xmm9
  pxor %xmm10, %xmm0
  movdqu (%rsi), %xmm2
  pxor %xmm8, %xmm2
  pclmulqdq $0x11, %xmm7, %xmm9
  pxor %xmm9, %xmm2
  crc32q (%rbx,%rax), %r13
  crc32q (%rbx,%rcx), %rbp
  crc32q (%rbx,%rdx), %r15
  crc32q 8(%rbx,%rax), %r13
  crc32q 8(%rbx,%rcx), %rbp
  crc32q 8(%rbx,%rdx), %r15
  crc32q 16(%rbx,%rax), %r13
  crc32q 16(%rbx,%rcx), %rbp
  movq %r14, %rdi
  crc32q 16(%rbx,%rdx), %r15
  addq $-184, %r14
  addq $-368, %rdi
  addq $24, %rbx
  addq $112, %rsi
  cmpq $191, %rdi
  ja .crc32_block192_loop
  addq %rax, %rbx
  jmp .crc32_block192_barrett
.crc32_block192_early_exit:
  addq %rax, %rbx
  xorl %r15d, %r15d
  xorl %ebp, %ebp
  xorl %r13d, %r13d
.crc32_block192_barrett:
  movl $0xf20c0dfe, %eax
  movq %rax, %xmm6
  movdqa %xmm11, %xmm7
  pclmulqdq $0x00, %xmm6, %xmm7
  pxor %xmm5, %xmm7
  pmovsxdq .k2(%rip), %xmm5
  pclmulqdq $0x11, %xmm5, %xmm11
  pxor %xmm7, %xmm11
  movdqa %xmm11, %xmm7
  pclmulqdq $0x00, %xmm6, %xmm7
  pxor %xmm4, %xmm7
  pclmulqdq $0x11, %xmm5, %xmm11
  pxor %xmm7, %xmm11
  movdqa %xmm1, %xmm4
  pclmulqdq $0x00, %xmm6, %xmm4
  pxor %xmm3, %xmm4
  pclmulqdq $0x11, %xmm5, %xmm1
  pxor %xmm4, %xmm1
  pclmulqdq $0x00, %xmm0, %xmm6
  pxor %xmm2, %xmm6
  pclmulqdq $0x11, %xmm5, %xmm0
  pxor %xmm6, %xmm0
  movl $0x3da6d0cb, %eax
  movq %rax, %xmm2
  movdqa %xmm11, %xmm3
  pclmulqdq $0x00, %xmm2, %xmm3
  pxor %xmm1, %xmm3
  pmovzxdq .k3(%rip), %xmm1
  pclmulqdq $0x11, %xmm1, %xmm11
  pxor %xmm3, %xmm11
  pclmulqdq $0x00, %xmm11, %xmm2
  pxor %xmm0, %xmm2
  pclmulqdq $0x11, %xmm1, %xmm11
  crc32q (%rbx), %r13
  crc32q (%rbx,%r12), %rbp
  crc32q (%rbx,%r8), %r15
  crc32q 8(%rbx), %r13
  crc32q 8(%rbx,%r12), %rbp
  crc32q 8(%rbx,%r8), %r15
  crc32q 16(%rbx), %r13
  crc32q 16(%rbx,%r12), %rbp
  pxor %xmm2, %xmm11
  movdqa %xmm11, 32(%rsp)
  crc32q 16(%rbx,%r8), %r15
  leal 31(,%r8,8), %edi
  movq %r8, 16(%rsp)
  callq xnmp
  movd %r13d, %xmm0
  movd %eax, %xmm1
  pclmulqdq $0x00, %xmm0, %xmm1
  movdqa %xmm1, (%rsp)
  leal 31(,%r12,8), %edi
  callq xnmp
  movd %ebp, %xmm0
  movd %eax, %xmm1
  pclmulqdq $0x00, %xmm0, %xmm1
  movdqa 32(%rsp), %xmm0
  movq %xmm0, %rax
  xorl %r13d, %r13d
  crc32q %rax, %r13
  pextrq $1, %xmm0, %rax
  pxor (%rsp), %xmm1
  movdqa %xmm1, (%rsp)
  crc32q %rax, %r13
  movq 24(%rsp), %rax
  shll $6, %eax
  leal (%rax,%rax,8), %edi
  addl $31, %edi
  callq xnmp
  movd %r13d, %xmm0
  movd %eax, %xmm1
  pclmulqdq $0x00, %xmm0, %xmm1
  pxor (%rsp), %xmm1
  movq %xmm1, %rax
  movq 16(%rsp), %rcx
  xorq 24(%rbx,%rcx), %rax
  crc32q %rax, %r15
  addq %rcx, %rbx
  addq $32, %rbx
  addq $-192, %r14
.crc32_do_remainder:
  cmpq $8, %r14
  jb .crc32_try_final1
.crc32_final8:
  movl %r15d, %r15d
  crc32q (%rbx), %r15
  addq $8, %rbx
  addq $-8, %r14
  cmpq $7, %r14
  ja .crc32_final8
.crc32_try_final1:
  testq %r14, %r14
  je .crc32_end
  xorl %eax, %eax
.crc32_final1:
  crc32b (%rbx,%rax), %r15d
  incq %rax
  cmpq %rax, %r14
  jne .crc32_final1
.crc32_end:
  notl %r15d
  movl %r15d, %eax
  addq $56, %rsp
  popq %rbx
  popq %r12
  popq %r13
  popq %r14
  popq %r15
  popq %rbp
  retq

.section .note.GNU-stack,"",@progbits
