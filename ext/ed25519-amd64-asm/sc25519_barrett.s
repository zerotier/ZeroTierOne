
# qhasm: int64 rp

# qhasm: int64 xp

# qhasm: input rp

# qhasm: input xp

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

# qhasm: int64 q23

# qhasm: int64 q24

# qhasm: int64 q30

# qhasm: int64 q31

# qhasm: int64 q32

# qhasm: int64 q33

# qhasm: int64 r20

# qhasm: int64 r21

# qhasm: int64 r22

# qhasm: int64 r23

# qhasm: int64 r24

# qhasm: int64 r0

# qhasm: int64 r1

# qhasm: int64 r2

# qhasm: int64 r3

# qhasm: int64 t0

# qhasm: int64 t1

# qhasm: int64 t2

# qhasm: int64 t3

# qhasm: int64 rax

# qhasm: int64 rdx

# qhasm: int64 c

# qhasm: int64 zero

# qhasm: int64 mask

# qhasm: int64 nmask

# qhasm: stack64 q30_stack

# qhasm: stack64 q31_stack

# qhasm: stack64 q32_stack

# qhasm: stack64 q33_stack

# qhasm: enter crypto_sign_ed25519_amd64_64_sc25519_barrett
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_sc25519_barrett
.globl crypto_sign_ed25519_amd64_64_sc25519_barrett
_crypto_sign_ed25519_amd64_64_sc25519_barrett:
crypto_sign_ed25519_amd64_64_sc25519_barrett:
mov %rsp,%r11
and $31,%r11
add $96,%r11
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

# qhasm: zero ^= zero
# asm 1: xor  <zero=int64#4,<zero=int64#4
# asm 2: xor  <zero=%rcx,<zero=%rcx
xor  %rcx,%rcx

# qhasm: q30 ^= q30
# asm 1: xor  <q30=int64#5,<q30=int64#5
# asm 2: xor  <q30=%r8,<q30=%r8
xor  %r8,%r8

# qhasm: q31 ^= q31
# asm 1: xor  <q31=int64#6,<q31=int64#6
# asm 2: xor  <q31=%r9,<q31=%r9
xor  %r9,%r9

# qhasm: q32 ^= q32
# asm 1: xor  <q32=int64#8,<q32=int64#8
# asm 2: xor  <q32=%r10,<q32=%r10
xor  %r10,%r10

# qhasm: q33 ^= q33
# asm 1: xor  <q33=int64#9,<q33=int64#9
# asm 2: xor  <q33=%r11,<q33=%r11
xor  %r11,%r11

# qhasm: rax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>rax=int64#7
# asm 2: movq   24(<xp=%rsi),>rax=%rax
movq   24(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU3
mulq  crypto_sign_ed25519_amd64_64_MU3

# qhasm: q23 = rax
# asm 1: mov  <rax=int64#7,>q23=int64#10
# asm 2: mov  <rax=%rax,>q23=%r12
mov  %rax,%r12

# qhasm: c = rdx
# asm 1: mov  <rdx=int64#3,>c=int64#11
# asm 2: mov  <rdx=%rdx,>c=%r13
mov  %rdx,%r13

# qhasm: rax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>rax=int64#7
# asm 2: movq   24(<xp=%rsi),>rax=%rax
movq   24(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU4
mulq  crypto_sign_ed25519_amd64_64_MU4

# qhasm: q24 = rax
# asm 1: mov  <rax=int64#7,>q24=int64#12
# asm 2: mov  <rax=%rax,>q24=%r14
mov  %rax,%r14

# qhasm: carry? q24 += c
# asm 1: add  <c=int64#11,<q24=int64#12
# asm 2: add  <c=%r13,<q24=%r14
add  %r13,%r14

# qhasm: q30 += rdx + carry
# asm 1: adc <rdx=int64#3,<q30=int64#5
# asm 2: adc <rdx=%rdx,<q30=%r8
adc %rdx,%r8

# qhasm: rax = *(uint64 *)(xp + 32)
# asm 1: movq   32(<xp=int64#2),>rax=int64#7
# asm 2: movq   32(<xp=%rsi),>rax=%rax
movq   32(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU2
mulq  crypto_sign_ed25519_amd64_64_MU2

# qhasm: carry? q23 += rax
# asm 1: add  <rax=int64#7,<q23=int64#10
# asm 2: add  <rax=%rax,<q23=%r12
add  %rax,%r12

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#11
# asm 2: mov  $0,>c=%r13
mov  $0,%r13

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#11
# asm 2: adc <rdx=%rdx,<c=%r13
adc %rdx,%r13

# qhasm: rax = *(uint64 *)(xp + 32)
# asm 1: movq   32(<xp=int64#2),>rax=int64#7
# asm 2: movq   32(<xp=%rsi),>rax=%rax
movq   32(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU3
mulq  crypto_sign_ed25519_amd64_64_MU3

# qhasm: carry? q24 += rax
# asm 1: add  <rax=int64#7,<q24=int64#12
# asm 2: add  <rax=%rax,<q24=%r14
add  %rax,%r14

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q24 += c
# asm 1: add  <c=int64#11,<q24=int64#12
# asm 2: add  <c=%r13,<q24=%r14
add  %r13,%r14

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#11
# asm 2: mov  $0,>c=%r13
mov  $0,%r13

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#11
# asm 2: adc <rdx=%rdx,<c=%r13
adc %rdx,%r13

# qhasm: rax = *(uint64 *)(xp + 32)
# asm 1: movq   32(<xp=int64#2),>rax=int64#7
# asm 2: movq   32(<xp=%rsi),>rax=%rax
movq   32(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU4
mulq  crypto_sign_ed25519_amd64_64_MU4

# qhasm: carry? q30 += rax 
# asm 1: add  <rax=int64#7,<q30=int64#5
# asm 2: add  <rax=%rax,<q30=%r8
add  %rax,%r8

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q30 += c
# asm 1: add  <c=int64#11,<q30=int64#5
# asm 2: add  <c=%r13,<q30=%r8
add  %r13,%r8

# qhasm: q31 += rdx + carry
# asm 1: adc <rdx=int64#3,<q31=int64#6
# asm 2: adc <rdx=%rdx,<q31=%r9
adc %rdx,%r9

# qhasm: rax = *(uint64 *)(xp + 40)
# asm 1: movq   40(<xp=int64#2),>rax=int64#7
# asm 2: movq   40(<xp=%rsi),>rax=%rax
movq   40(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU1
mulq  crypto_sign_ed25519_amd64_64_MU1

# qhasm: carry? q23 += rax
# asm 1: add  <rax=int64#7,<q23=int64#10
# asm 2: add  <rax=%rax,<q23=%r12
add  %rax,%r12

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#11
# asm 2: mov  $0,>c=%r13
mov  $0,%r13

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#11
# asm 2: adc <rdx=%rdx,<c=%r13
adc %rdx,%r13

# qhasm: rax = *(uint64 *)(xp + 40)
# asm 1: movq   40(<xp=int64#2),>rax=int64#7
# asm 2: movq   40(<xp=%rsi),>rax=%rax
movq   40(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU2
mulq  crypto_sign_ed25519_amd64_64_MU2

# qhasm: carry? q24 += rax
# asm 1: add  <rax=int64#7,<q24=int64#12
# asm 2: add  <rax=%rax,<q24=%r14
add  %rax,%r14

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q24 += c
# asm 1: add  <c=int64#11,<q24=int64#12
# asm 2: add  <c=%r13,<q24=%r14
add  %r13,%r14

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#11
# asm 2: mov  $0,>c=%r13
mov  $0,%r13

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#11
# asm 2: adc <rdx=%rdx,<c=%r13
adc %rdx,%r13

# qhasm: rax = *(uint64 *)(xp + 40)
# asm 1: movq   40(<xp=int64#2),>rax=int64#7
# asm 2: movq   40(<xp=%rsi),>rax=%rax
movq   40(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU3
mulq  crypto_sign_ed25519_amd64_64_MU3

# qhasm: carry? q30 += rax
# asm 1: add  <rax=int64#7,<q30=int64#5
# asm 2: add  <rax=%rax,<q30=%r8
add  %rax,%r8

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q30 += c
# asm 1: add  <c=int64#11,<q30=int64#5
# asm 2: add  <c=%r13,<q30=%r8
add  %r13,%r8

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#11
# asm 2: mov  $0,>c=%r13
mov  $0,%r13

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#11
# asm 2: adc <rdx=%rdx,<c=%r13
adc %rdx,%r13

# qhasm: rax = *(uint64 *)(xp + 40)
# asm 1: movq   40(<xp=int64#2),>rax=int64#7
# asm 2: movq   40(<xp=%rsi),>rax=%rax
movq   40(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU4
mulq  crypto_sign_ed25519_amd64_64_MU4

# qhasm: carry? q31 += rax 
# asm 1: add  <rax=int64#7,<q31=int64#6
# asm 2: add  <rax=%rax,<q31=%r9
add  %rax,%r9

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q31 += c
# asm 1: add  <c=int64#11,<q31=int64#6
# asm 2: add  <c=%r13,<q31=%r9
add  %r13,%r9

# qhasm: q32 += rdx + carry
# asm 1: adc <rdx=int64#3,<q32=int64#8
# asm 2: adc <rdx=%rdx,<q32=%r10
adc %rdx,%r10

# qhasm: rax = *(uint64 *)(xp + 48)
# asm 1: movq   48(<xp=int64#2),>rax=int64#7
# asm 2: movq   48(<xp=%rsi),>rax=%rax
movq   48(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU0
mulq  crypto_sign_ed25519_amd64_64_MU0

# qhasm: carry? q23 += rax
# asm 1: add  <rax=int64#7,<q23=int64#10
# asm 2: add  <rax=%rax,<q23=%r12
add  %rax,%r12

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#10
# asm 2: mov  $0,>c=%r12
mov  $0,%r12

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#10
# asm 2: adc <rdx=%rdx,<c=%r12
adc %rdx,%r12

# qhasm: rax = *(uint64 *)(xp + 48)
# asm 1: movq   48(<xp=int64#2),>rax=int64#7
# asm 2: movq   48(<xp=%rsi),>rax=%rax
movq   48(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU1
mulq  crypto_sign_ed25519_amd64_64_MU1

# qhasm: carry? q24 += rax
# asm 1: add  <rax=int64#7,<q24=int64#12
# asm 2: add  <rax=%rax,<q24=%r14
add  %rax,%r14

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q24 += c
# asm 1: add  <c=int64#10,<q24=int64#12
# asm 2: add  <c=%r12,<q24=%r14
add  %r12,%r14

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#10
# asm 2: mov  $0,>c=%r12
mov  $0,%r12

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#10
# asm 2: adc <rdx=%rdx,<c=%r12
adc %rdx,%r12

# qhasm: rax = *(uint64 *)(xp + 48)
# asm 1: movq   48(<xp=int64#2),>rax=int64#7
# asm 2: movq   48(<xp=%rsi),>rax=%rax
movq   48(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU2
mulq  crypto_sign_ed25519_amd64_64_MU2

# qhasm: carry? q30 += rax
# asm 1: add  <rax=int64#7,<q30=int64#5
# asm 2: add  <rax=%rax,<q30=%r8
add  %rax,%r8

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q30 += c
# asm 1: add  <c=int64#10,<q30=int64#5
# asm 2: add  <c=%r12,<q30=%r8
add  %r12,%r8

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#10
# asm 2: mov  $0,>c=%r12
mov  $0,%r12

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#10
# asm 2: adc <rdx=%rdx,<c=%r12
adc %rdx,%r12

# qhasm: rax = *(uint64 *)(xp + 48)
# asm 1: movq   48(<xp=int64#2),>rax=int64#7
# asm 2: movq   48(<xp=%rsi),>rax=%rax
movq   48(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU3
mulq  crypto_sign_ed25519_amd64_64_MU3

# qhasm: carry? q31 += rax
# asm 1: add  <rax=int64#7,<q31=int64#6
# asm 2: add  <rax=%rax,<q31=%r9
add  %rax,%r9

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q31 += c
# asm 1: add  <c=int64#10,<q31=int64#6
# asm 2: add  <c=%r12,<q31=%r9
add  %r12,%r9

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#10
# asm 2: mov  $0,>c=%r12
mov  $0,%r12

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#10
# asm 2: adc <rdx=%rdx,<c=%r12
adc %rdx,%r12

# qhasm: rax = *(uint64 *)(xp + 48)
# asm 1: movq   48(<xp=int64#2),>rax=int64#7
# asm 2: movq   48(<xp=%rsi),>rax=%rax
movq   48(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU4
mulq  crypto_sign_ed25519_amd64_64_MU4

# qhasm: carry? q32 += rax 
# asm 1: add  <rax=int64#7,<q32=int64#8
# asm 2: add  <rax=%rax,<q32=%r10
add  %rax,%r10

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q32 += c
# asm 1: add  <c=int64#10,<q32=int64#8
# asm 2: add  <c=%r12,<q32=%r10
add  %r12,%r10

# qhasm: q33 += rdx + carry
# asm 1: adc <rdx=int64#3,<q33=int64#9
# asm 2: adc <rdx=%rdx,<q33=%r11
adc %rdx,%r11

# qhasm: rax = *(uint64 *)(xp + 56)
# asm 1: movq   56(<xp=int64#2),>rax=int64#7
# asm 2: movq   56(<xp=%rsi),>rax=%rax
movq   56(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU0
mulq  crypto_sign_ed25519_amd64_64_MU0

# qhasm: carry? q24 += rax
# asm 1: add  <rax=int64#7,<q24=int64#12
# asm 2: add  <rax=%rax,<q24=%r14
add  %rax,%r14

# qhasm: free q24

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#10
# asm 2: mov  $0,>c=%r12
mov  $0,%r12

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#10
# asm 2: adc <rdx=%rdx,<c=%r12
adc %rdx,%r12

# qhasm: rax = *(uint64 *)(xp + 56)
# asm 1: movq   56(<xp=int64#2),>rax=int64#7
# asm 2: movq   56(<xp=%rsi),>rax=%rax
movq   56(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU1
mulq  crypto_sign_ed25519_amd64_64_MU1

# qhasm: carry? q30 += rax
# asm 1: add  <rax=int64#7,<q30=int64#5
# asm 2: add  <rax=%rax,<q30=%r8
add  %rax,%r8

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q30 += c
# asm 1: add  <c=int64#10,<q30=int64#5
# asm 2: add  <c=%r12,<q30=%r8
add  %r12,%r8

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#10
# asm 2: mov  $0,>c=%r12
mov  $0,%r12

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#10
# asm 2: adc <rdx=%rdx,<c=%r12
adc %rdx,%r12

# qhasm: q30_stack = q30
# asm 1: movq <q30=int64#5,>q30_stack=stack64#8
# asm 2: movq <q30=%r8,>q30_stack=56(%rsp)
movq %r8,56(%rsp)

# qhasm: rax = *(uint64 *)(xp + 56)
# asm 1: movq   56(<xp=int64#2),>rax=int64#7
# asm 2: movq   56(<xp=%rsi),>rax=%rax
movq   56(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU2
mulq  crypto_sign_ed25519_amd64_64_MU2

# qhasm: carry? q31 += rax
# asm 1: add  <rax=int64#7,<q31=int64#6
# asm 2: add  <rax=%rax,<q31=%r9
add  %rax,%r9

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q31 += c
# asm 1: add  <c=int64#10,<q31=int64#6
# asm 2: add  <c=%r12,<q31=%r9
add  %r12,%r9

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#5
# asm 2: mov  $0,>c=%r8
mov  $0,%r8

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#5
# asm 2: adc <rdx=%rdx,<c=%r8
adc %rdx,%r8

# qhasm: q31_stack = q31
# asm 1: movq <q31=int64#6,>q31_stack=stack64#9
# asm 2: movq <q31=%r9,>q31_stack=64(%rsp)
movq %r9,64(%rsp)

# qhasm: rax = *(uint64 *)(xp + 56)
# asm 1: movq   56(<xp=int64#2),>rax=int64#7
# asm 2: movq   56(<xp=%rsi),>rax=%rax
movq   56(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU3
mulq  crypto_sign_ed25519_amd64_64_MU3

# qhasm: carry? q32 += rax
# asm 1: add  <rax=int64#7,<q32=int64#8
# asm 2: add  <rax=%rax,<q32=%r10
add  %rax,%r10

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? q32 += c
# asm 1: add  <c=int64#5,<q32=int64#8
# asm 2: add  <c=%r8,<q32=%r10
add  %r8,%r10

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#5
# asm 2: mov  $0,>c=%r8
mov  $0,%r8

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#5
# asm 2: adc <rdx=%rdx,<c=%r8
adc %rdx,%r8

# qhasm: q32_stack = q32
# asm 1: movq <q32=int64#8,>q32_stack=stack64#10
# asm 2: movq <q32=%r10,>q32_stack=72(%rsp)
movq %r10,72(%rsp)

# qhasm: rax = *(uint64 *)(xp + 56)
# asm 1: movq   56(<xp=int64#2),>rax=int64#7
# asm 2: movq   56(<xp=%rsi),>rax=%rax
movq   56(%rsi),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_MU4
mulq  crypto_sign_ed25519_amd64_64_MU4

# qhasm: carry? q33 += rax 
# asm 1: add  <rax=int64#7,<q33=int64#9
# asm 2: add  <rax=%rax,<q33=%r11
add  %rax,%r11

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: q33 += c
# asm 1: add  <c=int64#5,<q33=int64#9
# asm 2: add  <c=%r8,<q33=%r11
add  %r8,%r11

# qhasm: q33_stack = q33
# asm 1: movq <q33=int64#9,>q33_stack=stack64#11
# asm 2: movq <q33=%r11,>q33_stack=80(%rsp)
movq %r11,80(%rsp)

# qhasm: rax = q30_stack
# asm 1: movq <q30_stack=stack64#8,>rax=int64#7
# asm 2: movq <q30_stack=56(%rsp),>rax=%rax
movq 56(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER0
mulq  crypto_sign_ed25519_amd64_64_ORDER0

# qhasm: r20 = rax
# asm 1: mov  <rax=int64#7,>r20=int64#5
# asm 2: mov  <rax=%rax,>r20=%r8
mov  %rax,%r8

# qhasm: c = rdx
# asm 1: mov  <rdx=int64#3,>c=int64#6
# asm 2: mov  <rdx=%rdx,>c=%r9
mov  %rdx,%r9

# qhasm: rax = q30_stack
# asm 1: movq <q30_stack=stack64#8,>rax=int64#7
# asm 2: movq <q30_stack=56(%rsp),>rax=%rax
movq 56(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER1
mulq  crypto_sign_ed25519_amd64_64_ORDER1

# qhasm: r21 = rax
# asm 1: mov  <rax=int64#7,>r21=int64#8
# asm 2: mov  <rax=%rax,>r21=%r10
mov  %rax,%r10

# qhasm: carry? r21 += c
# asm 1: add  <c=int64#6,<r21=int64#8
# asm 2: add  <c=%r9,<r21=%r10
add  %r9,%r10

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#6
# asm 2: mov  $0,>c=%r9
mov  $0,%r9

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#6
# asm 2: adc <rdx=%rdx,<c=%r9
adc %rdx,%r9

# qhasm: rax = q30_stack
# asm 1: movq <q30_stack=stack64#8,>rax=int64#7
# asm 2: movq <q30_stack=56(%rsp),>rax=%rax
movq 56(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER2
mulq  crypto_sign_ed25519_amd64_64_ORDER2

# qhasm: r22 = rax
# asm 1: mov  <rax=int64#7,>r22=int64#9
# asm 2: mov  <rax=%rax,>r22=%r11
mov  %rax,%r11

# qhasm: carry? r22 += c
# asm 1: add  <c=int64#6,<r22=int64#9
# asm 2: add  <c=%r9,<r22=%r11
add  %r9,%r11

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#6
# asm 2: mov  $0,>c=%r9
mov  $0,%r9

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#6
# asm 2: adc <rdx=%rdx,<c=%r9
adc %rdx,%r9

# qhasm: rax = q30_stack
# asm 1: movq <q30_stack=stack64#8,>rax=int64#7
# asm 2: movq <q30_stack=56(%rsp),>rax=%rax
movq 56(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER3
mulq  crypto_sign_ed25519_amd64_64_ORDER3

# qhasm: free rdx

# qhasm: r23 = rax
# asm 1: mov  <rax=int64#7,>r23=int64#10
# asm 2: mov  <rax=%rax,>r23=%r12
mov  %rax,%r12

# qhasm: r23 += c
# asm 1: add  <c=int64#6,<r23=int64#10
# asm 2: add  <c=%r9,<r23=%r12
add  %r9,%r12

# qhasm: rax = q31_stack
# asm 1: movq <q31_stack=stack64#9,>rax=int64#7
# asm 2: movq <q31_stack=64(%rsp),>rax=%rax
movq 64(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER0
mulq  crypto_sign_ed25519_amd64_64_ORDER0

# qhasm: carry? r21 += rax
# asm 1: add  <rax=int64#7,<r21=int64#8
# asm 2: add  <rax=%rax,<r21=%r10
add  %rax,%r10

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#6
# asm 2: mov  $0,>c=%r9
mov  $0,%r9

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#6
# asm 2: adc <rdx=%rdx,<c=%r9
adc %rdx,%r9

# qhasm: rax = q31_stack
# asm 1: movq <q31_stack=stack64#9,>rax=int64#7
# asm 2: movq <q31_stack=64(%rsp),>rax=%rax
movq 64(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER1
mulq  crypto_sign_ed25519_amd64_64_ORDER1

# qhasm: carry? r22 += rax
# asm 1: add  <rax=int64#7,<r22=int64#9
# asm 2: add  <rax=%rax,<r22=%r11
add  %rax,%r11

# qhasm: rdx += zero + carry
# asm 1: adc <zero=int64#4,<rdx=int64#3
# asm 2: adc <zero=%rcx,<rdx=%rdx
adc %rcx,%rdx

# qhasm: carry? r22 += c
# asm 1: add  <c=int64#6,<r22=int64#9
# asm 2: add  <c=%r9,<r22=%r11
add  %r9,%r11

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#4
# asm 2: mov  $0,>c=%rcx
mov  $0,%rcx

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#4
# asm 2: adc <rdx=%rdx,<c=%rcx
adc %rdx,%rcx

# qhasm: rax = q31_stack
# asm 1: movq <q31_stack=stack64#9,>rax=int64#7
# asm 2: movq <q31_stack=64(%rsp),>rax=%rax
movq 64(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER2
mulq  crypto_sign_ed25519_amd64_64_ORDER2

# qhasm: free rdx

# qhasm: r23 += rax 
# asm 1: add  <rax=int64#7,<r23=int64#10
# asm 2: add  <rax=%rax,<r23=%r12
add  %rax,%r12

# qhasm: r23 += c
# asm 1: add  <c=int64#4,<r23=int64#10
# asm 2: add  <c=%rcx,<r23=%r12
add  %rcx,%r12

# qhasm: rax = q32_stack
# asm 1: movq <q32_stack=stack64#10,>rax=int64#7
# asm 2: movq <q32_stack=72(%rsp),>rax=%rax
movq 72(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER0
mulq  crypto_sign_ed25519_amd64_64_ORDER0

# qhasm: carry? r22 += rax
# asm 1: add  <rax=int64#7,<r22=int64#9
# asm 2: add  <rax=%rax,<r22=%r11
add  %rax,%r11

# qhasm: c = 0
# asm 1: mov  $0,>c=int64#4
# asm 2: mov  $0,>c=%rcx
mov  $0,%rcx

# qhasm: c += rdx + carry
# asm 1: adc <rdx=int64#3,<c=int64#4
# asm 2: adc <rdx=%rdx,<c=%rcx
adc %rdx,%rcx

# qhasm: rax = q32_stack
# asm 1: movq <q32_stack=stack64#10,>rax=int64#7
# asm 2: movq <q32_stack=72(%rsp),>rax=%rax
movq 72(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER1
mulq  crypto_sign_ed25519_amd64_64_ORDER1

# qhasm: free rdx

# qhasm: r23 += rax
# asm 1: add  <rax=int64#7,<r23=int64#10
# asm 2: add  <rax=%rax,<r23=%r12
add  %rax,%r12

# qhasm: r23 += c
# asm 1: add  <c=int64#4,<r23=int64#10
# asm 2: add  <c=%rcx,<r23=%r12
add  %rcx,%r12

# qhasm: rax = q33_stack
# asm 1: movq <q33_stack=stack64#11,>rax=int64#7
# asm 2: movq <q33_stack=80(%rsp),>rax=%rax
movq 80(%rsp),%rax

# qhasm: (uint128) rdx rax = rax * *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER0
mulq  crypto_sign_ed25519_amd64_64_ORDER0

# qhasm: free rdx

# qhasm: r23 += rax
# asm 1: add  <rax=int64#7,<r23=int64#10
# asm 2: add  <rax=%rax,<r23=%r12
add  %rax,%r12

# qhasm: r0 = *(uint64 *)(xp +  0)
# asm 1: movq   0(<xp=int64#2),>r0=int64#3
# asm 2: movq   0(<xp=%rsi),>r0=%rdx
movq   0(%rsi),%rdx

# qhasm: carry? r0 -= r20
# asm 1: sub  <r20=int64#5,<r0=int64#3
# asm 2: sub  <r20=%r8,<r0=%rdx
sub  %r8,%rdx

# qhasm: t0 = r0
# asm 1: mov  <r0=int64#3,>t0=int64#4
# asm 2: mov  <r0=%rdx,>t0=%rcx
mov  %rdx,%rcx

# qhasm: r1 = *(uint64 *)(xp +  8)
# asm 1: movq   8(<xp=int64#2),>r1=int64#5
# asm 2: movq   8(<xp=%rsi),>r1=%r8
movq   8(%rsi),%r8

# qhasm: carry? r1 -= r21 - carry
# asm 1: sbb  <r21=int64#8,<r1=int64#5
# asm 2: sbb  <r21=%r10,<r1=%r8
sbb  %r10,%r8

# qhasm: t1 = r1
# asm 1: mov  <r1=int64#5,>t1=int64#6
# asm 2: mov  <r1=%r8,>t1=%r9
mov  %r8,%r9

# qhasm: r2 = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>r2=int64#7
# asm 2: movq   16(<xp=%rsi),>r2=%rax
movq   16(%rsi),%rax

# qhasm: carry? r2 -= r22 - carry
# asm 1: sbb  <r22=int64#9,<r2=int64#7
# asm 2: sbb  <r22=%r11,<r2=%rax
sbb  %r11,%rax

# qhasm: t2 = r2
# asm 1: mov  <r2=int64#7,>t2=int64#8
# asm 2: mov  <r2=%rax,>t2=%r10
mov  %rax,%r10

# qhasm: r3 = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>r3=int64#2
# asm 2: movq   24(<xp=%rsi),>r3=%rsi
movq   24(%rsi),%rsi

# qhasm: r3 -= r23 - carry
# asm 1: sbb  <r23=int64#10,<r3=int64#2
# asm 2: sbb  <r23=%r12,<r3=%rsi
sbb  %r12,%rsi

# qhasm: t3 = r3
# asm 1: mov  <r3=int64#2,>t3=int64#9
# asm 2: mov  <r3=%rsi,>t3=%r11
mov  %rsi,%r11

# qhasm: carry? t0 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER0
# asm 1: sub  crypto_sign_ed25519_amd64_64_ORDER0,<t0=int64#4
# asm 2: sub  crypto_sign_ed25519_amd64_64_ORDER0,<t0=%rcx
sub  crypto_sign_ed25519_amd64_64_ORDER0,%rcx

# qhasm: carry? t1 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER1 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER1,<t1=int64#6
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER1,<t1=%r9
sbb  crypto_sign_ed25519_amd64_64_ORDER1,%r9

# qhasm: carry? t2 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER2 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER2,<t2=int64#8
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER2,<t2=%r10
sbb  crypto_sign_ed25519_amd64_64_ORDER2,%r10

# qhasm: unsigned<? t3 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER3 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER3,<t3=int64#9
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER3,<t3=%r11
sbb  crypto_sign_ed25519_amd64_64_ORDER3,%r11

# qhasm: r0 = t0 if !unsigned<
# asm 1: cmovae <t0=int64#4,<r0=int64#3
# asm 2: cmovae <t0=%rcx,<r0=%rdx
cmovae %rcx,%rdx

# qhasm: t0 = r0
# asm 1: mov  <r0=int64#3,>t0=int64#4
# asm 2: mov  <r0=%rdx,>t0=%rcx
mov  %rdx,%rcx

# qhasm: r1 = t1 if !unsigned<
# asm 1: cmovae <t1=int64#6,<r1=int64#5
# asm 2: cmovae <t1=%r9,<r1=%r8
cmovae %r9,%r8

# qhasm: t1 = r1
# asm 1: mov  <r1=int64#5,>t1=int64#6
# asm 2: mov  <r1=%r8,>t1=%r9
mov  %r8,%r9

# qhasm: r2 = t2 if !unsigned<
# asm 1: cmovae <t2=int64#8,<r2=int64#7
# asm 2: cmovae <t2=%r10,<r2=%rax
cmovae %r10,%rax

# qhasm: t2 = r2
# asm 1: mov  <r2=int64#7,>t2=int64#8
# asm 2: mov  <r2=%rax,>t2=%r10
mov  %rax,%r10

# qhasm: r3 = t3 if !unsigned<
# asm 1: cmovae <t3=int64#9,<r3=int64#2
# asm 2: cmovae <t3=%r11,<r3=%rsi
cmovae %r11,%rsi

# qhasm: t3 = r3
# asm 1: mov  <r3=int64#2,>t3=int64#9
# asm 2: mov  <r3=%rsi,>t3=%r11
mov  %rsi,%r11

# qhasm: carry? t0 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER0
# asm 1: sub  crypto_sign_ed25519_amd64_64_ORDER0,<t0=int64#4
# asm 2: sub  crypto_sign_ed25519_amd64_64_ORDER0,<t0=%rcx
sub  crypto_sign_ed25519_amd64_64_ORDER0,%rcx

# qhasm: carry? t1 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER1 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER1,<t1=int64#6
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER1,<t1=%r9
sbb  crypto_sign_ed25519_amd64_64_ORDER1,%r9

# qhasm: carry? t2 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER2 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER2,<t2=int64#8
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER2,<t2=%r10
sbb  crypto_sign_ed25519_amd64_64_ORDER2,%r10

# qhasm: unsigned<? t3 -= *(uint64 *) &crypto_sign_ed25519_amd64_64_ORDER3 - carry
# asm 1: sbb  crypto_sign_ed25519_amd64_64_ORDER3,<t3=int64#9
# asm 2: sbb  crypto_sign_ed25519_amd64_64_ORDER3,<t3=%r11
sbb  crypto_sign_ed25519_amd64_64_ORDER3,%r11

# qhasm: r0 = t0 if !unsigned<
# asm 1: cmovae <t0=int64#4,<r0=int64#3
# asm 2: cmovae <t0=%rcx,<r0=%rdx
cmovae %rcx,%rdx

# qhasm: r1 = t1 if !unsigned<
# asm 1: cmovae <t1=int64#6,<r1=int64#5
# asm 2: cmovae <t1=%r9,<r1=%r8
cmovae %r9,%r8

# qhasm: r2 = t2 if !unsigned<
# asm 1: cmovae <t2=int64#8,<r2=int64#7
# asm 2: cmovae <t2=%r10,<r2=%rax
cmovae %r10,%rax

# qhasm: r3 = t3 if !unsigned<
# asm 1: cmovae <t3=int64#9,<r3=int64#2
# asm 2: cmovae <t3=%r11,<r3=%rsi
cmovae %r11,%rsi

# qhasm: *(uint64 *)(rp +  0) = r0
# asm 1: movq   <r0=int64#3,0(<rp=int64#1)
# asm 2: movq   <r0=%rdx,0(<rp=%rdi)
movq   %rdx,0(%rdi)

# qhasm: *(uint64 *)(rp +  8) = r1
# asm 1: movq   <r1=int64#5,8(<rp=int64#1)
# asm 2: movq   <r1=%r8,8(<rp=%rdi)
movq   %r8,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = r2
# asm 1: movq   <r2=int64#7,16(<rp=int64#1)
# asm 2: movq   <r2=%rax,16(<rp=%rdi)
movq   %rax,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = r3
# asm 1: movq   <r3=int64#2,24(<rp=int64#1)
# asm 2: movq   <r3=%rsi,24(<rp=%rdi)
movq   %rsi,24(%rdi)

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
