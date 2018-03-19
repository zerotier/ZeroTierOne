
# qhasm: int64 rp

# qhasm: input rp

# qhasm: int64 r0

# qhasm: int64 r1

# qhasm: int64 r2

# qhasm: int64 r3

# qhasm: int64 t0

# qhasm: int64 t1

# qhasm: int64 t2

# qhasm: int64 t3

# qhasm: int64 two63

# qhasm:   int64 caller1

# qhasm:   int64 caller2

# qhasm:   int64 caller3

# qhasm:   int64 caller4

# qhasm:   int64 caller5

# qhasm:   int64 caller6

# qhasm:   int64 caller7

# qhasm:   caller caller1

# qhasm:   caller caller2

# qhasm:   caller caller3

# qhasm:   caller caller4

# qhasm:   caller caller5

# qhasm:   caller caller6

# qhasm:   caller caller7

# qhasm:   stack64 caller1_stack

# qhasm:   stack64 caller2_stack

# qhasm:   stack64 caller3_stack

# qhasm:   stack64 caller4_stack

# qhasm:   stack64 caller5_stack

# qhasm:   stack64 caller6_stack

# qhasm:   stack64 caller7_stack

# qhasm: enter crypto_sign_ed25519_amd64_64_fe25519_freeze
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_fe25519_freeze
.globl crypto_sign_ed25519_amd64_64_fe25519_freeze
_crypto_sign_ed25519_amd64_64_fe25519_freeze:
crypto_sign_ed25519_amd64_64_fe25519_freeze:
mov %rsp,%r11
and $31,%r11
add $64,%r11
sub %r11,%rsp

# qhasm:   caller1_stack = caller1
# asm 1: movq <caller1=int64#9,>caller1_stack=stack64#1
# asm 2: movq <caller1=%r11,>caller1_stack=0(%rsp)
movq %r11,0(%rsp)

# qhasm:   caller2_stack = caller2
# asm 1: movq <caller2=int64#10,>caller2_stack=stack64#2
# asm 2: movq <caller2=%r12,>caller2_stack=8(%rsp)
movq %r12,8(%rsp)

# qhasm:   caller3_stack = caller3
# asm 1: movq <caller3=int64#11,>caller3_stack=stack64#3
# asm 2: movq <caller3=%r13,>caller3_stack=16(%rsp)
movq %r13,16(%rsp)

# qhasm:   caller4_stack = caller4
# asm 1: movq <caller4=int64#12,>caller4_stack=stack64#4
# asm 2: movq <caller4=%r14,>caller4_stack=24(%rsp)
movq %r14,24(%rsp)

# qhasm:   caller5_stack = caller5
# asm 1: movq <caller5=int64#13,>caller5_stack=stack64#5
# asm 2: movq <caller5=%r15,>caller5_stack=32(%rsp)
movq %r15,32(%rsp)

# qhasm:   caller6_stack = caller6
# asm 1: movq <caller6=int64#14,>caller6_stack=stack64#6
# asm 2: movq <caller6=%rbx,>caller6_stack=40(%rsp)
movq %rbx,40(%rsp)

# qhasm:   caller7_stack = caller7
# asm 1: movq <caller7=int64#15,>caller7_stack=stack64#7
# asm 2: movq <caller7=%rbp,>caller7_stack=48(%rsp)
movq %rbp,48(%rsp)

# qhasm: r0 = *(uint64 *) (rp + 0)
# asm 1: movq   0(<rp=int64#1),>r0=int64#2
# asm 2: movq   0(<rp=%rdi),>r0=%rsi
movq   0(%rdi),%rsi

# qhasm: r1 = *(uint64 *) (rp + 8)
# asm 1: movq   8(<rp=int64#1),>r1=int64#3
# asm 2: movq   8(<rp=%rdi),>r1=%rdx
movq   8(%rdi),%rdx

# qhasm: r2 = *(uint64 *) (rp + 16)
# asm 1: movq   16(<rp=int64#1),>r2=int64#4
# asm 2: movq   16(<rp=%rdi),>r2=%rcx
movq   16(%rdi),%rcx

# qhasm: r3 = *(uint64 *) (rp + 24)
# asm 1: movq   24(<rp=int64#1),>r3=int64#5
# asm 2: movq   24(<rp=%rdi),>r3=%r8
movq   24(%rdi),%r8

# qhasm: t0 = r0
# asm 1: mov  <r0=int64#2,>t0=int64#6
# asm 2: mov  <r0=%rsi,>t0=%r9
mov  %rsi,%r9

# qhasm: t1 = r1
# asm 1: mov  <r1=int64#3,>t1=int64#7
# asm 2: mov  <r1=%rdx,>t1=%rax
mov  %rdx,%rax

# qhasm: t2 = r2
# asm 1: mov  <r2=int64#4,>t2=int64#8
# asm 2: mov  <r2=%rcx,>t2=%r10
mov  %rcx,%r10

# qhasm: t3 = r3
# asm 1: mov  <r3=int64#5,>t3=int64#9
# asm 2: mov  <r3=%r8,>t3=%r11
mov  %r8,%r11

# qhasm: two63 = 1
# asm 1: mov  $1,>two63=int64#10
# asm 2: mov  $1,>two63=%r12
mov  $1,%r12

# qhasm: two63 <<= 63
# asm 1: shl  $63,<two63=int64#10
# asm 2: shl  $63,<two63=%r12
shl  $63,%r12

# qhasm: carry? t0 += 19
# asm 1: add  $19,<t0=int64#6
# asm 2: add  $19,<t0=%r9
add  $19,%r9

# qhasm: carry? t1 += 0 + carry
# asm 1: adc $0,<t1=int64#7
# asm 2: adc $0,<t1=%rax
adc $0,%rax

# qhasm: carry? t2 += 0 + carry
# asm 1: adc $0,<t2=int64#8
# asm 2: adc $0,<t2=%r10
adc $0,%r10

# qhasm: carry? t3 += two63 + carry
# asm 1: adc <two63=int64#10,<t3=int64#9
# asm 2: adc <two63=%r12,<t3=%r11
adc %r12,%r11

# qhasm: r0 = t0 if carry
# asm 1: cmovc <t0=int64#6,<r0=int64#2
# asm 2: cmovc <t0=%r9,<r0=%rsi
cmovc %r9,%rsi

# qhasm: r1 = t1 if carry
# asm 1: cmovc <t1=int64#7,<r1=int64#3
# asm 2: cmovc <t1=%rax,<r1=%rdx
cmovc %rax,%rdx

# qhasm: r2 = t2 if carry
# asm 1: cmovc <t2=int64#8,<r2=int64#4
# asm 2: cmovc <t2=%r10,<r2=%rcx
cmovc %r10,%rcx

# qhasm: r3 = t3 if carry
# asm 1: cmovc <t3=int64#9,<r3=int64#5
# asm 2: cmovc <t3=%r11,<r3=%r8
cmovc %r11,%r8

# qhasm: t0 = r0
# asm 1: mov  <r0=int64#2,>t0=int64#6
# asm 2: mov  <r0=%rsi,>t0=%r9
mov  %rsi,%r9

# qhasm: t1 = r1
# asm 1: mov  <r1=int64#3,>t1=int64#7
# asm 2: mov  <r1=%rdx,>t1=%rax
mov  %rdx,%rax

# qhasm: t2 = r2
# asm 1: mov  <r2=int64#4,>t2=int64#8
# asm 2: mov  <r2=%rcx,>t2=%r10
mov  %rcx,%r10

# qhasm: t3 = r3
# asm 1: mov  <r3=int64#5,>t3=int64#9
# asm 2: mov  <r3=%r8,>t3=%r11
mov  %r8,%r11

# qhasm: carry? t0 += 19
# asm 1: add  $19,<t0=int64#6
# asm 2: add  $19,<t0=%r9
add  $19,%r9

# qhasm: carry? t1 += 0 + carry
# asm 1: adc $0,<t1=int64#7
# asm 2: adc $0,<t1=%rax
adc $0,%rax

# qhasm: carry? t2 += 0 + carry
# asm 1: adc $0,<t2=int64#8
# asm 2: adc $0,<t2=%r10
adc $0,%r10

# qhasm: carry? t3 += two63 + carry
# asm 1: adc <two63=int64#10,<t3=int64#9
# asm 2: adc <two63=%r12,<t3=%r11
adc %r12,%r11

# qhasm: r0 = t0 if carry
# asm 1: cmovc <t0=int64#6,<r0=int64#2
# asm 2: cmovc <t0=%r9,<r0=%rsi
cmovc %r9,%rsi

# qhasm: r1 = t1 if carry
# asm 1: cmovc <t1=int64#7,<r1=int64#3
# asm 2: cmovc <t1=%rax,<r1=%rdx
cmovc %rax,%rdx

# qhasm: r2 = t2 if carry
# asm 1: cmovc <t2=int64#8,<r2=int64#4
# asm 2: cmovc <t2=%r10,<r2=%rcx
cmovc %r10,%rcx

# qhasm: r3 = t3 if carry
# asm 1: cmovc <t3=int64#9,<r3=int64#5
# asm 2: cmovc <t3=%r11,<r3=%r8
cmovc %r11,%r8

# qhasm: *(uint64 *)(rp + 0) = r0
# asm 1: movq   <r0=int64#2,0(<rp=int64#1)
# asm 2: movq   <r0=%rsi,0(<rp=%rdi)
movq   %rsi,0(%rdi)

# qhasm: *(uint64 *)(rp + 8) = r1
# asm 1: movq   <r1=int64#3,8(<rp=int64#1)
# asm 2: movq   <r1=%rdx,8(<rp=%rdi)
movq   %rdx,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = r2
# asm 1: movq   <r2=int64#4,16(<rp=int64#1)
# asm 2: movq   <r2=%rcx,16(<rp=%rdi)
movq   %rcx,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = r3
# asm 1: movq   <r3=int64#5,24(<rp=int64#1)
# asm 2: movq   <r3=%r8,24(<rp=%rdi)
movq   %r8,24(%rdi)

# qhasm:   caller1 = caller1_stack
# asm 1: movq <caller1_stack=stack64#1,>caller1=int64#9
# asm 2: movq <caller1_stack=0(%rsp),>caller1=%r11
movq 0(%rsp),%r11

# qhasm:   caller2 = caller2_stack
# asm 1: movq <caller2_stack=stack64#2,>caller2=int64#10
# asm 2: movq <caller2_stack=8(%rsp),>caller2=%r12
movq 8(%rsp),%r12

# qhasm:   caller3 = caller3_stack
# asm 1: movq <caller3_stack=stack64#3,>caller3=int64#11
# asm 2: movq <caller3_stack=16(%rsp),>caller3=%r13
movq 16(%rsp),%r13

# qhasm:   caller4 = caller4_stack
# asm 1: movq <caller4_stack=stack64#4,>caller4=int64#12
# asm 2: movq <caller4_stack=24(%rsp),>caller4=%r14
movq 24(%rsp),%r14

# qhasm:   caller5 = caller5_stack
# asm 1: movq <caller5_stack=stack64#5,>caller5=int64#13
# asm 2: movq <caller5_stack=32(%rsp),>caller5=%r15
movq 32(%rsp),%r15

# qhasm:   caller6 = caller6_stack
# asm 1: movq <caller6_stack=stack64#6,>caller6=int64#14
# asm 2: movq <caller6_stack=40(%rsp),>caller6=%rbx
movq 40(%rsp),%rbx

# qhasm:   caller7 = caller7_stack
# asm 1: movq <caller7_stack=stack64#7,>caller7=int64#15
# asm 2: movq <caller7_stack=48(%rsp),>caller7=%rbp
movq 48(%rsp),%rbp

# qhasm: leave
add %r11,%rsp
mov %rdi,%rax
mov %rsi,%rdx
ret
