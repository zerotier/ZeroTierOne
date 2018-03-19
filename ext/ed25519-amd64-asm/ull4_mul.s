
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

# qhasm: int64 r4

# qhasm: int64 r5

# qhasm: int64 r6

# qhasm: int64 r7

# qhasm: int64 c

# qhasm: int64 zero

# qhasm: int64 rax

# qhasm: int64 rdx

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

# qhasm: enter crypto_sign_ed25519_amd64_64_ull4_mul 
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_ull4_mul
.globl crypto_sign_ed25519_amd64_64_ull4_mul
_crypto_sign_ed25519_amd64_64_ull4_mul:
crypto_sign_ed25519_amd64_64_ull4_mul:
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

# qhasm: yp = yp
# asm 1: mov  <yp=int64#3,>yp=int64#4
# asm 2: mov  <yp=%rdx,>yp=%rcx
mov  %rdx,%rcx

# qhasm: r4 = 0
# asm 1: mov  $0,>r4=int64#5
# asm 2: mov  $0,>r4=%r8
mov  $0,%r8

# qhasm: r5 = 0
# asm 1: mov  $0,>r5=int64#6
# asm 2: mov  $0,>r5=%r9
mov  $0,%r9

# qhasm: r6 = 0
# asm 1: mov  $0,>r6=int64#8
# asm 2: mov  $0,>r6=%r10
mov  $0,%r10

# qhasm: r7 = 0
# asm 1: mov  $0,>r7=int64#9
# asm 2: mov  $0,>r7=%r11
mov  $0,%r11

# qhasm: zero = 0
# asm 1: mov  $0,>zero=int64#10
# asm 2: mov  $0,>zero=%r12
mov  $0,%r12

# qhasm: rax = *(uint64 *)(xp + 0)
# asm 1: movq   0(<xp=int64#2),>rax=int64#7
# asm 2: movq   0(<xp=%rsi),>rax=%rax
movq   0(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp +  0)
# asm 1: mulq  0(<yp=int64#4)
# asm 2: mulq  0(<yp=%rcx)
mulq  0(%rcx)

# qhasm: r0 = rax
# asm 1: mov  <rax=int64#7,>r0=int64#11
# asm 2: mov  <rax=%rax,>r0=%r13
mov  %rax,%r13

# qhasm: c = rdx
# asm 1: mov  <rdx=int64#3,>c=int64#12
# asm 2: mov  <rdx=%rdx,>c=%r14
mov  %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 0)
# asm 1: movq   0(<xp=int64#2),>rax=int64#7
# asm 2: movq   0(<xp=%rsi),>rax=%rax
movq   0(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp +  8)
# asm 1: mulq  8(<yp=int64#4)
# asm 2: mulq  8(<yp=%rcx)
mulq  8(%rcx)

# qhasm: r1 = rax
# asm 1: mov  <rax=int64#7,>r1=int64#13
# asm 2: mov  <rax=%rax,>r1=%r15
mov  %rax,%r15

# qhasm: carry? r1 += c
# asm 1: add  <c=int64#12,<r1=int64#13
# asm 2: add  <c=%r14,<r1=%r15
add  %r14,%r15

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 0)
# asm 1: movq   0(<xp=int64#2),>rax=int64#7
# asm 2: movq   0(<xp=%rsi),>rax=%rax
movq   0(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp + 16)
# asm 1: mulq  16(<yp=int64#4)
# asm 2: mulq  16(<yp=%rcx)
mulq  16(%rcx)

# qhasm: r2 = rax
# asm 1: mov  <rax=int64#7,>r2=int64#14
# asm 2: mov  <rax=%rax,>r2=%rbx
mov  %rax,%rbx

# qhasm: carry? r2 += c
# asm 1: add  <c=int64#12,<r2=int64#14
# asm 2: add  <c=%r14,<r2=%rbx
add  %r14,%rbx

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 0)
# asm 1: movq   0(<xp=int64#2),>rax=int64#7
# asm 2: movq   0(<xp=%rsi),>rax=%rax
movq   0(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp + 24)
# asm 1: mulq  24(<yp=int64#4)
# asm 2: mulq  24(<yp=%rcx)
mulq  24(%rcx)

# qhasm: r3 = rax
# asm 1: mov  <rax=int64#7,>r3=int64#15
# asm 2: mov  <rax=%rax,>r3=%rbp
mov  %rax,%rbp

# qhasm: carry? r3 += c
# asm 1: add  <c=int64#12,<r3=int64#15
# asm 2: add  <c=%r14,<r3=%rbp
add  %r14,%rbp

# qhasm: r4 += rdx + carry
# asm 1: adc <rdx=int64#3,<r4=int64#5
# asm 2: adc <rdx=%rdx,<r4=%r8
adc %rdx,%r8

# qhasm: rax = *(uint64 *)(xp + 8)
# asm 1: movq   8(<xp=int64#2),>rax=int64#7
# asm 2: movq   8(<xp=%rsi),>rax=%rax
movq   8(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp +  0)
# asm 1: mulq  0(<yp=int64#4)
# asm 2: mulq  0(<yp=%rcx)
mulq  0(%rcx)

# qhasm: carry? r1 += rax
# asm 1: add  <rax=int64#7,<r1=int64#13
# asm 2: add  <rax=%rax,<r1=%r15
add  %rax,%r15

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 8)
# asm 1: movq   8(<xp=int64#2),>rax=int64#7
# asm 2: movq   8(<xp=%rsi),>rax=%rax
movq   8(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp +  8)
# asm 1: mulq  8(<yp=int64#4)
# asm 2: mulq  8(<yp=%rcx)
mulq  8(%rcx)

# qhasm: carry? r2 += rax
# asm 1: add  <rax=int64#7,<r2=int64#14
# asm 2: add  <rax=%rax,<r2=%rbx
add  %rax,%rbx

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r2 += c
# asm 1: add  <c=int64#12,<r2=int64#14
# asm 2: add  <c=%r14,<r2=%rbx
add  %r14,%rbx

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 8)
# asm 1: movq   8(<xp=int64#2),>rax=int64#7
# asm 2: movq   8(<xp=%rsi),>rax=%rax
movq   8(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp + 16)
# asm 1: mulq  16(<yp=int64#4)
# asm 2: mulq  16(<yp=%rcx)
mulq  16(%rcx)

# qhasm: carry? r3 += rax 
# asm 1: add  <rax=int64#7,<r3=int64#15
# asm 2: add  <rax=%rax,<r3=%rbp
add  %rax,%rbp

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r3 += c
# asm 1: add  <c=int64#12,<r3=int64#15
# asm 2: add  <c=%r14,<r3=%rbp
add  %r14,%rbp

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 8)
# asm 1: movq   8(<xp=int64#2),>rax=int64#7
# asm 2: movq   8(<xp=%rsi),>rax=%rax
movq   8(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp + 24)
# asm 1: mulq  24(<yp=int64#4)
# asm 2: mulq  24(<yp=%rcx)
mulq  24(%rcx)

# qhasm: carry? r4 += rax
# asm 1: add  <rax=int64#7,<r4=int64#5
# asm 2: add  <rax=%rax,<r4=%r8
add  %rax,%r8

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r4 += c
# asm 1: add  <c=int64#12,<r4=int64#5
# asm 2: add  <c=%r14,<r4=%r8
add  %r14,%r8

# qhasm: r5 += rdx + carry
# asm 1: adc <rdx=int64#3,<r5=int64#6
# asm 2: adc <rdx=%rdx,<r5=%r9
adc %rdx,%r9

# qhasm: rax = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>rax=int64#7
# asm 2: movq   16(<xp=%rsi),>rax=%rax
movq   16(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp +  0)
# asm 1: mulq  0(<yp=int64#4)
# asm 2: mulq  0(<yp=%rcx)
mulq  0(%rcx)

# qhasm: carry? r2 += rax
# asm 1: add  <rax=int64#7,<r2=int64#14
# asm 2: add  <rax=%rax,<r2=%rbx
add  %rax,%rbx

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>rax=int64#7
# asm 2: movq   16(<xp=%rsi),>rax=%rax
movq   16(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp +  8)
# asm 1: mulq  8(<yp=int64#4)
# asm 2: mulq  8(<yp=%rcx)
mulq  8(%rcx)

# qhasm: carry? r3 += rax
# asm 1: add  <rax=int64#7,<r3=int64#15
# asm 2: add  <rax=%rax,<r3=%rbp
add  %rax,%rbp

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r3 += c
# asm 1: add  <c=int64#12,<r3=int64#15
# asm 2: add  <c=%r14,<r3=%rbp
add  %r14,%rbp

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>rax=int64#7
# asm 2: movq   16(<xp=%rsi),>rax=%rax
movq   16(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp + 16)
# asm 1: mulq  16(<yp=int64#4)
# asm 2: mulq  16(<yp=%rcx)
mulq  16(%rcx)

# qhasm: carry? r4 += rax
# asm 1: add  <rax=int64#7,<r4=int64#5
# asm 2: add  <rax=%rax,<r4=%r8
add  %rax,%r8

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r4 += c
# asm 1: add  <c=int64#12,<r4=int64#5
# asm 2: add  <c=%r14,<r4=%r8
add  %r14,%r8

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>rax=int64#7
# asm 2: movq   16(<xp=%rsi),>rax=%rax
movq   16(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp + 24)
# asm 1: mulq  24(<yp=int64#4)
# asm 2: mulq  24(<yp=%rcx)
mulq  24(%rcx)

# qhasm: carry? r5 += rax
# asm 1: add  <rax=int64#7,<r5=int64#6
# asm 2: add  <rax=%rax,<r5=%r9
add  %rax,%r9

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r5 += c
# asm 1: add  <c=int64#12,<r5=int64#6
# asm 2: add  <c=%r14,<r5=%r9
add  %r14,%r9

# qhasm: r6 += rdx + carry
# asm 1: adc <rdx=int64#3,<r6=int64#8
# asm 2: adc <rdx=%rdx,<r6=%r10
adc %rdx,%r10

# qhasm: rax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>rax=int64#7
# asm 2: movq   24(<xp=%rsi),>rax=%rax
movq   24(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp +  0)
# asm 1: mulq  0(<yp=int64#4)
# asm 2: mulq  0(<yp=%rcx)
mulq  0(%rcx)

# qhasm: carry? r3 += rax
# asm 1: add  <rax=int64#7,<r3=int64#15
# asm 2: add  <rax=%rax,<r3=%rbp
add  %rax,%rbp

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>rax=int64#7
# asm 2: movq   24(<xp=%rsi),>rax=%rax
movq   24(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp +  8)
# asm 1: mulq  8(<yp=int64#4)
# asm 2: mulq  8(<yp=%rcx)
mulq  8(%rcx)

# qhasm: carry? r4 += rax
# asm 1: add  <rax=int64#7,<r4=int64#5
# asm 2: add  <rax=%rax,<r4=%r8
add  %rax,%r8

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r4 += c
# asm 1: add  <c=int64#12,<r4=int64#5
# asm 2: add  <c=%r14,<r4=%r8
add  %r14,%r8

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>rax=int64#7
# asm 2: movq   24(<xp=%rsi),>rax=%rax
movq   24(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp + 16)
# asm 1: mulq  16(<yp=int64#4)
# asm 2: mulq  16(<yp=%rcx)
mulq  16(%rcx)

# qhasm: carry? r5 += rax
# asm 1: add  <rax=int64#7,<r5=int64#6
# asm 2: add  <rax=%rax,<r5=%r9
add  %rax,%r9

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r5 += c
# asm 1: add  <c=int64#12,<r5=int64#6
# asm 2: add  <c=%r14,<r5=%r9
add  %r14,%r9

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#12
# asm 2: mov  $0,>c=%r14
mov  $0,%r14

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#12
# asm 2: adc <rdx=%rdx,<c=%r14
adc %rdx,%r14

# qhasm: rax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>rax=int64#7
# asm 2: movq   24(<xp=%rsi),>rax=%rax
movq   24(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *)(yp + 24)
# asm 1: mulq  24(<yp=int64#4)
# asm 2: mulq  24(<yp=%rcx)
mulq  24(%rcx)

# qhasm: carry? r6 += rax
# asm 1: add  <rax=int64#7,<r6=int64#8
# asm 2: add  <rax=%rax,<r6=%r10
add  %rax,%r10

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#10,<rdx=int64#3
# asm 2: adc <zero=%r12,<rdx=%rdx
adc %r12,%rdx

# qhasm: carry? r6 += c
# asm 1: add  <c=int64#12,<r6=int64#8
# asm 2: add  <c=%r14,<r6=%r10
add  %r14,%r10

# qhasm: r7 += rdx + carry
# asm 1: adc <rdx=int64#3,<r7=int64#9
# asm 2: adc <rdx=%rdx,<r7=%r11
adc %rdx,%r11

# qhasm: *(uint64 *)(rp +  0) = r0
# asm 1: movq   <r0=int64#11,0(<rp=int64#1)
# asm 2: movq   <r0=%r13,0(<rp=%rdi)
movq   %r13,0(%rdi)

# qhasm: *(uint64 *)(rp +  8) = r1
# asm 1: movq   <r1=int64#13,8(<rp=int64#1)
# asm 2: movq   <r1=%r15,8(<rp=%rdi)
movq   %r15,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = r2
# asm 1: movq   <r2=int64#14,16(<rp=int64#1)
# asm 2: movq   <r2=%rbx,16(<rp=%rdi)
movq   %rbx,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = r3
# asm 1: movq   <r3=int64#15,24(<rp=int64#1)
# asm 2: movq   <r3=%rbp,24(<rp=%rdi)
movq   %rbp,24(%rdi)

# qhasm: *(uint64 *)(rp + 32) = r4
# asm 1: movq   <r4=int64#5,32(<rp=int64#1)
# asm 2: movq   <r4=%r8,32(<rp=%rdi)
movq   %r8,32(%rdi)

# qhasm: *(uint64 *)(rp + 40) = r5
# asm 1: movq   <r5=int64#6,40(<rp=int64#1)
# asm 2: movq   <r5=%r9,40(<rp=%rdi)
movq   %r9,40(%rdi)

# qhasm: *(uint64 *)(rp + 48) = r6
# asm 1: movq   <r6=int64#8,48(<rp=int64#1)
# asm 2: movq   <r6=%r10,48(<rp=%rdi)
movq   %r10,48(%rdi)

# qhasm: *(uint64 *)(rp + 56) = r7
# asm 1: movq   <r7=int64#9,56(<rp=int64#1)
# asm 2: movq   <r7=%r11,56(<rp=%rdi)
movq   %r11,56(%rdi)

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
