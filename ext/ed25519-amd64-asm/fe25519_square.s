
# qhasm: int64 rp

# qhasm: int64 xp

# qhasm: input rp

# qhasm: input xp

# qhasm: int64 r0

# qhasm: int64 r1

# qhasm: int64 r2

# qhasm: int64 r3

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

# qhasm: int64 squarer4

# qhasm: int64 squarer5

# qhasm: int64 squarer6

# qhasm: int64 squarer7

# qhasm: int64 squarer8

# qhasm: int64 squarerax

# qhasm: int64 squarerdx

# qhasm: int64 squaret1

# qhasm: int64 squaret2

# qhasm: int64 squaret3

# qhasm: int64 squarec

# qhasm: int64 squarezero

# qhasm: int64 squarei38

# qhasm: enter crypto_sign_ed25519_amd64_64_fe25519_square
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_fe25519_square
.globl crypto_sign_ed25519_amd64_64_fe25519_square
_crypto_sign_ed25519_amd64_64_fe25519_square:
crypto_sign_ed25519_amd64_64_fe25519_square:
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

# qhasm:   squarer7 = 0
# asm 1: mov  $0,>squarer7=int64#4
# asm 2: mov  $0,>squarer7=%rcx
mov  $0,%rcx

# qhasm:   squarerax = *(uint64 *)(xp + 8)
# asm 1: movq   8(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   8(<xp=%rsi),>squarerax=%rax
movq   8(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 0)
# asm 1: mulq  0(<xp=int64#2)
# asm 2: mulq  0(<xp=%rsi)
mulq  0(%rsi)

# qhasm:   r1 = squarerax
# asm 1: mov  <squarerax=int64#7,>r1=int64#5
# asm 2: mov  <squarerax=%rax,>r1=%r8
mov  %rax,%r8

# qhasm:   r2 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>r2=int64#6
# asm 2: mov  <squarerdx=%rdx,>r2=%r9
mov  %rdx,%r9

# qhasm:   squarerax = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   16(<xp=%rsi),>squarerax=%rax
movq   16(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 8)
# asm 1: mulq  8(<xp=int64#2)
# asm 2: mulq  8(<xp=%rsi)
mulq  8(%rsi)

# qhasm:   r3 = squarerax 
# asm 1: mov  <squarerax=int64#7,>r3=int64#8
# asm 2: mov  <squarerax=%rax,>r3=%r10
mov  %rax,%r10

# qhasm:   squarer4 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer4=int64#9
# asm 2: mov  <squarerdx=%rdx,>squarer4=%r11
mov  %rdx,%r11

# qhasm:   squarerax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   24(<xp=%rsi),>squarerax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 16)
# asm 1: mulq  16(<xp=int64#2)
# asm 2: mulq  16(<xp=%rsi)
mulq  16(%rsi)

# qhasm:   squarer5 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer5=int64#10
# asm 2: mov  <squarerax=%rax,>squarer5=%r12
mov  %rax,%r12

# qhasm:   squarer6 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer6=int64#11
# asm 2: mov  <squarerdx=%rdx,>squarer6=%r13
mov  %rdx,%r13

# qhasm:   squarerax = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   16(<xp=%rsi),>squarerax=%rax
movq   16(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 0)
# asm 1: mulq  0(<xp=int64#2)
# asm 2: mulq  0(<xp=%rsi)
mulq  0(%rsi)

# qhasm:   carry? r2 += squarerax
# asm 1: add  <squarerax=int64#7,<r2=int64#6
# asm 2: add  <squarerax=%rax,<r2=%r9
add  %rax,%r9

# qhasm:   carry? r3 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<r3=int64#8
# asm 2: adc <squarerdx=%rdx,<r3=%r10
adc %rdx,%r10

# qhasm:   squarer4 += 0 + carry
# asm 1: adc $0,<squarer4=int64#9
# asm 2: adc $0,<squarer4=%r11
adc $0,%r11

# qhasm:   squarerax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   24(<xp=%rsi),>squarerax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 8)
# asm 1: mulq  8(<xp=int64#2)
# asm 2: mulq  8(<xp=%rsi)
mulq  8(%rsi)

# qhasm:   carry? squarer4 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer4=int64#9
# asm 2: add  <squarerax=%rax,<squarer4=%r11
add  %rax,%r11

# qhasm:   carry? squarer5 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer5=int64#10
# asm 2: adc <squarerdx=%rdx,<squarer5=%r12
adc %rdx,%r12

# qhasm:   squarer6 += 0 + carry
# asm 1: adc $0,<squarer6=int64#11
# asm 2: adc $0,<squarer6=%r13
adc $0,%r13

# qhasm:   squarerax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   24(<xp=%rsi),>squarerax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 0)
# asm 1: mulq  0(<xp=int64#2)
# asm 2: mulq  0(<xp=%rsi)
mulq  0(%rsi)

# qhasm:   carry? r3 += squarerax
# asm 1: add  <squarerax=int64#7,<r3=int64#8
# asm 2: add  <squarerax=%rax,<r3=%r10
add  %rax,%r10

# qhasm:   carry? squarer4 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer4=int64#9
# asm 2: adc <squarerdx=%rdx,<squarer4=%r11
adc %rdx,%r11

# qhasm:   carry? squarer5 += 0 + carry
# asm 1: adc $0,<squarer5=int64#10
# asm 2: adc $0,<squarer5=%r12
adc $0,%r12

# qhasm:   carry? squarer6 += 0 + carry
# asm 1: adc $0,<squarer6=int64#11
# asm 2: adc $0,<squarer6=%r13
adc $0,%r13

# qhasm:   squarer7 += 0 + carry
# asm 1: adc $0,<squarer7=int64#4
# asm 2: adc $0,<squarer7=%rcx
adc $0,%rcx

# qhasm:   carry? r1 += r1
# asm 1: add  <r1=int64#5,<r1=int64#5
# asm 2: add  <r1=%r8,<r1=%r8
add  %r8,%r8

# qhasm:   carry? r2 += r2 + carry
# asm 1: adc <r2=int64#6,<r2=int64#6
# asm 2: adc <r2=%r9,<r2=%r9
adc %r9,%r9

# qhasm:   carry? r3 += r3 + carry
# asm 1: adc <r3=int64#8,<r3=int64#8
# asm 2: adc <r3=%r10,<r3=%r10
adc %r10,%r10

# qhasm:   carry? squarer4 += squarer4 + carry
# asm 1: adc <squarer4=int64#9,<squarer4=int64#9
# asm 2: adc <squarer4=%r11,<squarer4=%r11
adc %r11,%r11

# qhasm:   carry? squarer5 += squarer5 + carry
# asm 1: adc <squarer5=int64#10,<squarer5=int64#10
# asm 2: adc <squarer5=%r12,<squarer5=%r12
adc %r12,%r12

# qhasm:   carry? squarer6 += squarer6 + carry
# asm 1: adc <squarer6=int64#11,<squarer6=int64#11
# asm 2: adc <squarer6=%r13,<squarer6=%r13
adc %r13,%r13

# qhasm:   squarer7 += squarer7 + carry
# asm 1: adc <squarer7=int64#4,<squarer7=int64#4
# asm 2: adc <squarer7=%rcx,<squarer7=%rcx
adc %rcx,%rcx

# qhasm:   squarerax = *(uint64 *)(xp + 0)
# asm 1: movq   0(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   0(<xp=%rsi),>squarerax=%rax
movq   0(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 0)
# asm 1: mulq  0(<xp=int64#2)
# asm 2: mulq  0(<xp=%rsi)
mulq  0(%rsi)

# qhasm:   r0 = squarerax
# asm 1: mov  <squarerax=int64#7,>r0=int64#12
# asm 2: mov  <squarerax=%rax,>r0=%r14
mov  %rax,%r14

# qhasm:   squaret1 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret1=int64#13
# asm 2: mov  <squarerdx=%rdx,>squaret1=%r15
mov  %rdx,%r15

# qhasm:   squarerax = *(uint64 *)(xp + 8)
# asm 1: movq   8(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   8(<xp=%rsi),>squarerax=%rax
movq   8(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 8)
# asm 1: mulq  8(<xp=int64#2)
# asm 2: mulq  8(<xp=%rsi)
mulq  8(%rsi)

# qhasm:   squaret2 = squarerax
# asm 1: mov  <squarerax=int64#7,>squaret2=int64#14
# asm 2: mov  <squarerax=%rax,>squaret2=%rbx
mov  %rax,%rbx

# qhasm:   squaret3 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret3=int64#15
# asm 2: mov  <squarerdx=%rdx,>squaret3=%rbp
mov  %rdx,%rbp

# qhasm:   squarerax = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   16(<xp=%rsi),>squarerax=%rax
movq   16(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 16)
# asm 1: mulq  16(<xp=int64#2)
# asm 2: mulq  16(<xp=%rsi)
mulq  16(%rsi)

# qhasm:   carry? r1 += squaret1
# asm 1: add  <squaret1=int64#13,<r1=int64#5
# asm 2: add  <squaret1=%r15,<r1=%r8
add  %r15,%r8

# qhasm:   carry? r2 += squaret2 + carry
# asm 1: adc <squaret2=int64#14,<r2=int64#6
# asm 2: adc <squaret2=%rbx,<r2=%r9
adc %rbx,%r9

# qhasm:   carry? r3 += squaret3 + carry
# asm 1: adc <squaret3=int64#15,<r3=int64#8
# asm 2: adc <squaret3=%rbp,<r3=%r10
adc %rbp,%r10

# qhasm:   carry? squarer4 += squarerax + carry
# asm 1: adc <squarerax=int64#7,<squarer4=int64#9
# asm 2: adc <squarerax=%rax,<squarer4=%r11
adc %rax,%r11

# qhasm:   carry? squarer5 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer5=int64#10
# asm 2: adc <squarerdx=%rdx,<squarer5=%r12
adc %rdx,%r12

# qhasm:   carry? squarer6 += 0 + carry
# asm 1: adc $0,<squarer6=int64#11
# asm 2: adc $0,<squarer6=%r13
adc $0,%r13

# qhasm:   squarer7 += 0 + carry 
# asm 1: adc $0,<squarer7=int64#4
# asm 2: adc $0,<squarer7=%rcx
adc $0,%rcx

# qhasm:   squarerax = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>squarerax=int64#7
# asm 2: movq   24(<xp=%rsi),>squarerax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(xp + 24)
# asm 1: mulq  24(<xp=int64#2)
# asm 2: mulq  24(<xp=%rsi)
mulq  24(%rsi)

# qhasm:   carry? squarer6 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer6=int64#11
# asm 2: add  <squarerax=%rax,<squarer6=%r13
add  %rax,%r13

# qhasm:   squarer7 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer7=int64#4
# asm 2: adc <squarerdx=%rdx,<squarer7=%rcx
adc %rdx,%rcx

# qhasm:   squarerax = squarer4
# asm 1: mov  <squarer4=int64#9,>squarerax=int64#7
# asm 2: mov  <squarer4=%r11,>squarerax=%rax
mov  %r11,%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38

# qhasm:   squarer4 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer4=int64#2
# asm 2: mov  <squarerax=%rax,>squarer4=%rsi
mov  %rax,%rsi

# qhasm:   squarerax = squarer5
# asm 1: mov  <squarer5=int64#10,>squarerax=int64#7
# asm 2: mov  <squarer5=%r12,>squarerax=%rax
mov  %r12,%rax

# qhasm:   squarer5 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer5=int64#9
# asm 2: mov  <squarerdx=%rdx,>squarer5=%r11
mov  %rdx,%r11

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38

# qhasm:   carry? squarer5 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer5=int64#9
# asm 2: add  <squarerax=%rax,<squarer5=%r11
add  %rax,%r11

# qhasm:   squarerax = squarer6
# asm 1: mov  <squarer6=int64#11,>squarerax=int64#7
# asm 2: mov  <squarer6=%r13,>squarerax=%rax
mov  %r13,%rax

# qhasm:   squarer6 = 0
# asm 1: mov  $0,>squarer6=int64#10
# asm 2: mov  $0,>squarer6=%r12
mov  $0,%r12

# qhasm:   squarer6 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer6=int64#10
# asm 2: adc <squarerdx=%rdx,<squarer6=%r12
adc %rdx,%r12

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38

# qhasm:   carry? squarer6 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer6=int64#10
# asm 2: add  <squarerax=%rax,<squarer6=%r12
add  %rax,%r12

# qhasm:   squarerax = squarer7
# asm 1: mov  <squarer7=int64#4,>squarerax=int64#7
# asm 2: mov  <squarer7=%rcx,>squarerax=%rax
mov  %rcx,%rax

# qhasm:   squarer7 = 0
# asm 1: mov  $0,>squarer7=int64#4
# asm 2: mov  $0,>squarer7=%rcx
mov  $0,%rcx

# qhasm:   squarer7 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer7=int64#4
# asm 2: adc <squarerdx=%rdx,<squarer7=%rcx
adc %rdx,%rcx

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38

# qhasm:   carry? squarer7 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer7=int64#4
# asm 2: add  <squarerax=%rax,<squarer7=%rcx
add  %rax,%rcx

# qhasm:   squarer8 = 0
# asm 1: mov  $0,>squarer8=int64#7
# asm 2: mov  $0,>squarer8=%rax
mov  $0,%rax

# qhasm:   squarer8 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer8=int64#7
# asm 2: adc <squarerdx=%rdx,<squarer8=%rax
adc %rdx,%rax

# qhasm:   carry? r0 += squarer4
# asm 1: add  <squarer4=int64#2,<r0=int64#12
# asm 2: add  <squarer4=%rsi,<r0=%r14
add  %rsi,%r14

# qhasm:   carry? r1 += squarer5 + carry
# asm 1: adc <squarer5=int64#9,<r1=int64#5
# asm 2: adc <squarer5=%r11,<r1=%r8
adc %r11,%r8

# qhasm:   carry? r2 += squarer6 + carry
# asm 1: adc <squarer6=int64#10,<r2=int64#6
# asm 2: adc <squarer6=%r12,<r2=%r9
adc %r12,%r9

# qhasm:   carry? r3 += squarer7 + carry
# asm 1: adc <squarer7=int64#4,<r3=int64#8
# asm 2: adc <squarer7=%rcx,<r3=%r10
adc %rcx,%r10

# qhasm:   squarezero = 0
# asm 1: mov  $0,>squarezero=int64#2
# asm 2: mov  $0,>squarezero=%rsi
mov  $0,%rsi

# qhasm:   squarer8 += squarezero + carry
# asm 1: adc <squarezero=int64#2,<squarer8=int64#7
# asm 2: adc <squarezero=%rsi,<squarer8=%rax
adc %rsi,%rax

# qhasm:   squarer8 *= 38
# asm 1: imulq  $38,<squarer8=int64#7,>squarer8=int64#3
# asm 2: imulq  $38,<squarer8=%rax,>squarer8=%rdx
imulq  $38,%rax,%rdx

# qhasm:   carry? r0 += squarer8
# asm 1: add  <squarer8=int64#3,<r0=int64#12
# asm 2: add  <squarer8=%rdx,<r0=%r14
add  %rdx,%r14

# qhasm:   carry? r1 += squarezero + carry
# asm 1: adc <squarezero=int64#2,<r1=int64#5
# asm 2: adc <squarezero=%rsi,<r1=%r8
adc %rsi,%r8

# qhasm:   carry? r2 += squarezero + carry
# asm 1: adc <squarezero=int64#2,<r2=int64#6
# asm 2: adc <squarezero=%rsi,<r2=%r9
adc %rsi,%r9

# qhasm:   carry? r3 += squarezero + carry
# asm 1: adc <squarezero=int64#2,<r3=int64#8
# asm 2: adc <squarezero=%rsi,<r3=%r10
adc %rsi,%r10

# qhasm:   squarezero += squarezero + carry
# asm 1: adc <squarezero=int64#2,<squarezero=int64#2
# asm 2: adc <squarezero=%rsi,<squarezero=%rsi
adc %rsi,%rsi

# qhasm:   squarezero *= 38
# asm 1: imulq  $38,<squarezero=int64#2,>squarezero=int64#2
# asm 2: imulq  $38,<squarezero=%rsi,>squarezero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   r0 += squarezero
# asm 1: add  <squarezero=int64#2,<r0=int64#12
# asm 2: add  <squarezero=%rsi,<r0=%r14
add  %rsi,%r14

# qhasm: *(uint64 *)(rp + 8) = r1
# asm 1: movq   <r1=int64#5,8(<rp=int64#1)
# asm 2: movq   <r1=%r8,8(<rp=%rdi)
movq   %r8,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = r2
# asm 1: movq   <r2=int64#6,16(<rp=int64#1)
# asm 2: movq   <r2=%r9,16(<rp=%rdi)
movq   %r9,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = r3
# asm 1: movq   <r3=int64#8,24(<rp=int64#1)
# asm 2: movq   <r3=%r10,24(<rp=%rdi)
movq   %r10,24(%rdi)

# qhasm: *(uint64 *)(rp + 0) = r0
# asm 1: movq   <r0=int64#12,0(<rp=int64#1)
# asm 2: movq   <r0=%r14,0(<rp=%rdi)
movq   %r14,0(%rdi)

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
