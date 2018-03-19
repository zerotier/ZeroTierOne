
# qhasm: int64 rp

# qhasm: int64 xp

# qhasm: int64 yp

# qhasm: input rp

# qhasm: input xp

# qhasm: input yp

# qhasm: int64 r0

# qhasm: int64 r1

# qhasm: int64 r2

# qhasm: int64 r3

# qhasm: int64 t0

# qhasm: int64 t1

# qhasm: int64 t2

# qhasm: int64 t3

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

# qhasm:   stack64 caller4_stack

# qhasm:   stack64 caller5_stack

# qhasm:   stack64 caller6_stack

# qhasm:   stack64 caller7_stack

# qhasm: enter crypto_sign_ed25519_amd64_64_sc25519_add
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_sc25519_add
.globl crypto_sign_ed25519_amd64_64_sc25519_add
_crypto_sign_ed25519_amd64_64_sc25519_add:
crypto_sign_ed25519_amd64_64_sc25519_add:
mov %rsp,%r11
and $31,%r11
add $32,%r11
sub %r11,%rsp

# qhasm: caller4_stack = caller4
# asm 1: movq <caller4=int64#12,>caller4_stack=stack64#1
# asm 2: movq <caller4=%r14,>caller4_stack=0(%rsp)
movq %r14,0(%rsp)

# qhasm: caller5_stack = caller5
# asm 1: movq <caller5=int64#13,>caller5_stack=stack64#2
# asm 2: movq <caller5=%r15,>caller5_stack=8(%rsp)
movq %r15,8(%rsp)

# qhasm: caller6_stack = caller6
# asm 1: movq <caller6=int64#14,>caller6_stack=stack64#3
# asm 2: movq <caller6=%rbx,>caller6_stack=16(%rsp)
movq %rbx,16(%rsp)

# qhasm: r0 = *(uint64 *)(xp +  0)
# asm 1: movq   0(<xp=int64#2),>r0=int64#4
# asm 2: movq   0(<xp=%rsi),>r0=%rcx
movq   0(%rsi),%rcx

# qhasm: r1 = *(uint64 *)(xp +  8)
# asm 1: movq   8(<xp=int64#2),>r1=int64#5
# asm 2: movq   8(<xp=%rsi),>r1=%r8
movq   8(%rsi),%r8

# qhasm: r2 = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>r2=int64#6
# asm 2: movq   16(<xp=%rsi),>r2=%r9
movq   16(%rsi),%r9

# qhasm: r3 = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>r3=int64#2
# asm 2: movq   24(<xp=%rsi),>r3=%rsi
movq   24(%rsi),%rsi

# qhasm: carry? r0 += *(uint64 *)(yp +  0)
# asm 1: addq 0(<yp=int64#3),<r0=int64#4
# asm 2: addq 0(<yp=%rdx),<r0=%rcx
addq 0(%rdx),%rcx

# qhasm: carry? r1 += *(uint64 *)(yp +  8) + carry
# asm 1: adcq 8(<yp=int64#3),<r1=int64#5
# asm 2: adcq 8(<yp=%rdx),<r1=%r8
adcq 8(%rdx),%r8

# qhasm: carry? r2 += *(uint64 *)(yp + 16) + carry
# asm 1: adcq 16(<yp=int64#3),<r2=int64#6
# asm 2: adcq 16(<yp=%rdx),<r2=%r9
adcq 16(%rdx),%r9

# qhasm: r3 += *(uint64 *)(yp + 24) + carry
# asm 1: adcq 24(<yp=int64#3),<r3=int64#2
# asm 2: adcq 24(<yp=%rdx),<r3=%rsi
adcq 24(%rdx),%rsi

# qhasm: t0 = r0
# asm 1: mov  <r0=int64#4,>t0=int64#3
# asm 2: mov  <r0=%rcx,>t0=%rdx
mov  %rcx,%rdx

# qhasm: t1 = r1
# asm 1: mov  <r1=int64#5,>t1=int64#7
# asm 2: mov  <r1=%r8,>t1=%rax
mov  %r8,%rax

# qhasm: t2 = r2
# asm 1: mov  <r2=int64#6,>t2=int64#8
# asm 2: mov  <r2=%r9,>t2=%r10
mov  %r9,%r10

# qhasm: t3 = r3
# asm 1: mov  <r3=int64#2,>t3=int64#12
# asm 2: mov  <r3=%rsi,>t3=%r14
mov  %rsi,%r14

# qhasm: carry? t0 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER0
# asm 1: sub  crypto_sign_ed25519_amd64_64_ORDER0,<t0=int64#3
# asm 2: sub  crypto_sign_ed25519_amd64_64_ORDER0,<t0=%rdx
sub  crypto_sign_ed25519_amd64_64_ORDER0,%rdx

# qhasm: carry? t1 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER1 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER1,<t1=int64#7
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER1,<t1=%rax
sbb  crypto_sign_ed25519_amd64_64_ORDER1,%rax

# qhasm: carry? t2 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER2 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER2,<t2=int64#8
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER2,<t2=%r10
sbb  crypto_sign_ed25519_amd64_64_ORDER2,%r10

# qhasm: unsigned<? t3 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER3 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER3,<t3=int64#12
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER3,<t3=%r14
sbb  crypto_sign_ed25519_amd64_64_ORDER3,%r14

# qhasm: r0 = t0 if !unsigned<
# asm 1: cmovae <t0=int64#3,<r0=int64#4
# asm 2: cmovae <t0=%rdx,<r0=%rcx
cmovae %rdx,%rcx

# qhasm: r1 = t1 if !unsigned<
# asm 1: cmovae <t1=int64#7,<r1=int64#5
# asm 2: cmovae <t1=%rax,<r1=%r8
cmovae %rax,%r8

# qhasm: r2 = t2 if !unsigned<
# asm 1: cmovae <t2=int64#8,<r2=int64#6
# asm 2: cmovae <t2=%r10,<r2=%r9
cmovae %r10,%r9

# qhasm: r3 = t3 if !unsigned<
# asm 1: cmovae <t3=int64#12,<r3=int64#2
# asm 2: cmovae <t3=%r14,<r3=%rsi
cmovae %r14,%rsi

# qhasm: *(uint64 *)(rp +  0) = r0
# asm 1: movq   <r0=int64#4,0(<rp=int64#1)
# asm 2: movq   <r0=%rcx,0(<rp=%rdi)
movq   %rcx,0(%rdi)

# qhasm: *(uint64 *)(rp +  8) = r1
# asm 1: movq   <r1=int64#5,8(<rp=int64#1)
# asm 2: movq   <r1=%r8,8(<rp=%rdi)
movq   %r8,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = r2
# asm 1: movq   <r2=int64#6,16(<rp=int64#1)
# asm 2: movq   <r2=%r9,16(<rp=%rdi)
movq   %r9,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = r3
# asm 1: movq   <r3=int64#2,24(<rp=int64#1)
# asm 2: movq   <r3=%rsi,24(<rp=%rdi)
movq   %rsi,24(%rdi)

# qhasm: caller4 = caller4_stack
# asm 1: movq <caller4_stack=stack64#1,>caller4=int64#12
# asm 2: movq <caller4_stack=0(%rsp),>caller4=%r14
movq 0(%rsp),%r14

# qhasm: caller5 = caller5_stack
# asm 1: movq <caller5_stack=stack64#2,>caller5=int64#13
# asm 2: movq <caller5_stack=8(%rsp),>caller5=%r15
movq 8(%rsp),%r15

# qhasm: caller6 = caller6_stack
# asm 1: movq <caller6_stack=stack64#3,>caller6=int64#14
# asm 2: movq <caller6_stack=16(%rsp),>caller6=%rbx
movq 16(%rsp),%rbx

# qhasm: leave
add %r11,%rsp
mov %rdi,%rax
mov %rsi,%rdx
ret
