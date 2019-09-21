# Copyright (c) 2016, Chris Fallin <cfallin@c1f.net>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE. 

.text

# Context frame is:
#     rip
#     rbp
#     r15
#     r14
#     r13
#     r12
#     rbx
#
# Thread start needs to set rdi, rsi, and rdx, but these aren't part of the
# frame, so we vector through a trampoline that sets r15 -> rdi, r14 -> rsi,
# r13 -> rdx, and jumps through r12.
trampoline:
    movq %r15, %rdi
    movq %r14, %rsi
    movq %r13, %rdx
    jmp *%r12

# args: stacktop (rdi), entry (rsi), data1 (rdx), data2 (rcx), data3 (r8)
#
# must set up frame to call entry with data1 in rdi, data2 in rsi, and data3
# in rcx
#
# also must ensure top-of-stack after return (into entry function) is
# 16-byte-aligned. In other words, the top of the context frame (next word
# after rip) pictured above is 16-byte-aligned.
#
# callee-saves are: rbx, rbp, r12-r15.
.global nk_arch_create_ctx
nk_arch_create_ctx:
    # Align stack to 16-byte boundary.
    andq $-15, %rdi
    subq $8, %rdi
    # push a fake return address, and a null word to align stack.
    subq $16, %rdi
    movq $0, 8(%rdi)
    movq $0, 0(%rdi)
    # Push context frame: rip, rbp, r15, r14, r13, r12, rbx.
    subq $56, %rdi
    movabsq $trampoline, %rax
    movq %rax, 48(%rdi)         # rip
    movq $0, 40(%rdi)           # rbp
    movq %rdx, 32(%rdi)         # data1 -> r15 in frame -> rdi in trampoline
    movq %rcx, 24(%rdi)         # data2 -> r14 in frame -> rsi in trampoline
    movq %r8,  16(%rdi)         # data3 -> r13 in frame -> rdx in trampoline
    movq %rsi,  8(%rdi)         # entry -> r12 in frame -> rip in trampoline
    movq $0,    0(%rdi)         # rbx
    movq %rdi, %rax
    retq

# args: void **fromstack (rdi), void *tostack (rsi), int msg (rdx)
.global nk_arch_switch_ctx
nk_arch_switch_ctx:
    pushq %rbp
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    movq %rsp, (%rdi)
    movq %rsi, %rsp
    popq %rbx
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popq %rbp
    movq %rdx, %rax
    retq
