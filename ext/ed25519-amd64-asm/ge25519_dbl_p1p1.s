
# qhasm: int64 rp

# qhasm: int64 pp

# qhasm: input rp

# qhasm: input pp

# qhasm: int64 a0

# qhasm: int64 a1

# qhasm: int64 a2

# qhasm: int64 a3

# qhasm: stack64 a0_stack

# qhasm: stack64 a1_stack

# qhasm: stack64 a2_stack

# qhasm: stack64 a3_stack

# qhasm: int64 b0

# qhasm: int64 b1

# qhasm: int64 b2

# qhasm: int64 b3

# qhasm: stack64 b0_stack

# qhasm: stack64 b1_stack

# qhasm: stack64 b2_stack

# qhasm: stack64 b3_stack

# qhasm: int64 c0

# qhasm: int64 c1

# qhasm: int64 c2

# qhasm: int64 c3

# qhasm: stack64 c0_stack

# qhasm: stack64 c1_stack

# qhasm: stack64 c2_stack

# qhasm: stack64 c3_stack

# qhasm: int64 d0

# qhasm: int64 d1

# qhasm: int64 d2

# qhasm: int64 d3

# qhasm: stack64 d0_stack

# qhasm: stack64 d1_stack

# qhasm: stack64 d2_stack

# qhasm: stack64 d3_stack

# qhasm: int64 e0

# qhasm: int64 e1

# qhasm: int64 e2

# qhasm: int64 e3

# qhasm: stack64 e0_stack

# qhasm: stack64 e1_stack

# qhasm: stack64 e2_stack

# qhasm: stack64 e3_stack

# qhasm: int64 rx0

# qhasm: int64 rx1

# qhasm: int64 rx2

# qhasm: int64 rx3

# qhasm: stack64 rx0_stack

# qhasm: stack64 rx1_stack

# qhasm: stack64 rx2_stack

# qhasm: stack64 rx3_stack

# qhasm: int64 ry0

# qhasm: int64 ry1

# qhasm: int64 ry2

# qhasm: int64 ry3

# qhasm: int64 ry4

# qhasm: int64 rz0

# qhasm: int64 rz1

# qhasm: int64 rz2

# qhasm: int64 rz3

# qhasm: int64 rt0

# qhasm: int64 rt1

# qhasm: int64 rt2

# qhasm: int64 rt3

# qhasm: int64 mulr4

# qhasm: int64 mulr5

# qhasm: int64 mulr6

# qhasm: int64 mulr7

# qhasm: int64 mulr8

# qhasm: int64 mulrax

# qhasm: int64 mulrdx

# qhasm: int64 mulx0

# qhasm: int64 mulx1

# qhasm: int64 mulx2

# qhasm: int64 mulx3

# qhasm: int64 mulc

# qhasm: int64 mulzero

# qhasm: int64 muli38

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

# qhasm: int64 addt0

# qhasm: int64 addt1

# qhasm: int64 subt0

# qhasm: int64 subt1

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

# qhasm: enter crypto_sign_ed25519_amd64_64_ge25519_dbl_p1p1
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_ge25519_dbl_p1p1
.globl crypto_sign_ed25519_amd64_64_ge25519_dbl_p1p1
_crypto_sign_ed25519_amd64_64_ge25519_dbl_p1p1:
crypto_sign_ed25519_amd64_64_ge25519_dbl_p1p1:
mov %rsp,%r11
and $31,%r11
add $192,%r11
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

# qhasm:   squarerax = *(uint64 *)(pp + 8)
# asm 1: movq   8(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   8(<pp=%rsi),>squarerax=%rax
movq   8(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 0)
# asm 1: mulq  0(<pp=int64#2)
# asm 2: mulq  0(<pp=%rsi)
mulq  0(%rsi)

# qhasm:   a1 = squarerax
# asm 1: mov  <squarerax=int64#7,>a1=int64#5
# asm 2: mov  <squarerax=%rax,>a1=%r8
mov  %rax,%r8

# qhasm:   a2 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>a2=int64#6
# asm 2: mov  <squarerdx=%rdx,>a2=%r9
mov  %rdx,%r9

# qhasm:   squarerax = *(uint64 *)(pp + 16)
# asm 1: movq   16(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   16(<pp=%rsi),>squarerax=%rax
movq   16(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 8)
# asm 1: mulq  8(<pp=int64#2)
# asm 2: mulq  8(<pp=%rsi)
mulq  8(%rsi)

# qhasm:   a3 = squarerax 
# asm 1: mov  <squarerax=int64#7,>a3=int64#8
# asm 2: mov  <squarerax=%rax,>a3=%r10
mov  %rax,%r10

# qhasm:   squarer4 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer4=int64#9
# asm 2: mov  <squarerdx=%rdx,>squarer4=%r11
mov  %rdx,%r11

# qhasm:   squarerax = *(uint64 *)(pp + 24)
# asm 1: movq   24(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   24(<pp=%rsi),>squarerax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 16)
# asm 1: mulq  16(<pp=int64#2)
# asm 2: mulq  16(<pp=%rsi)
mulq  16(%rsi)

# qhasm:   squarer5 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer5=int64#10
# asm 2: mov  <squarerax=%rax,>squarer5=%r12
mov  %rax,%r12

# qhasm:   squarer6 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer6=int64#11
# asm 2: mov  <squarerdx=%rdx,>squarer6=%r13
mov  %rdx,%r13

# qhasm:   squarerax = *(uint64 *)(pp + 16)
# asm 1: movq   16(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   16(<pp=%rsi),>squarerax=%rax
movq   16(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 0)
# asm 1: mulq  0(<pp=int64#2)
# asm 2: mulq  0(<pp=%rsi)
mulq  0(%rsi)

# qhasm:   carry? a2 += squarerax
# asm 1: add  <squarerax=int64#7,<a2=int64#6
# asm 2: add  <squarerax=%rax,<a2=%r9
add  %rax,%r9

# qhasm:   carry? a3 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<a3=int64#8
# asm 2: adc <squarerdx=%rdx,<a3=%r10
adc %rdx,%r10

# qhasm:   squarer4 += 0 + carry
# asm 1: adc $0,<squarer4=int64#9
# asm 2: adc $0,<squarer4=%r11
adc $0,%r11

# qhasm:   squarerax = *(uint64 *)(pp + 24)
# asm 1: movq   24(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   24(<pp=%rsi),>squarerax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 8)
# asm 1: mulq  8(<pp=int64#2)
# asm 2: mulq  8(<pp=%rsi)
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

# qhasm:   squarerax = *(uint64 *)(pp + 24)
# asm 1: movq   24(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   24(<pp=%rsi),>squarerax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 0)
# asm 1: mulq  0(<pp=int64#2)
# asm 2: mulq  0(<pp=%rsi)
mulq  0(%rsi)

# qhasm:   carry? a3 += squarerax
# asm 1: add  <squarerax=int64#7,<a3=int64#8
# asm 2: add  <squarerax=%rax,<a3=%r10
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

# qhasm:   carry? a1 += a1
# asm 1: add  <a1=int64#5,<a1=int64#5
# asm 2: add  <a1=%r8,<a1=%r8
add  %r8,%r8

# qhasm:   carry? a2 += a2 + carry
# asm 1: adc <a2=int64#6,<a2=int64#6
# asm 2: adc <a2=%r9,<a2=%r9
adc %r9,%r9

# qhasm:   carry? a3 += a3 + carry
# asm 1: adc <a3=int64#8,<a3=int64#8
# asm 2: adc <a3=%r10,<a3=%r10
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

# qhasm:   squarerax = *(uint64 *)(pp + 0)
# asm 1: movq   0(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   0(<pp=%rsi),>squarerax=%rax
movq   0(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 0)
# asm 1: mulq  0(<pp=int64#2)
# asm 2: mulq  0(<pp=%rsi)
mulq  0(%rsi)

# qhasm:   a0 = squarerax
# asm 1: mov  <squarerax=int64#7,>a0=int64#12
# asm 2: mov  <squarerax=%rax,>a0=%r14
mov  %rax,%r14

# qhasm:   squaret1 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret1=int64#13
# asm 2: mov  <squarerdx=%rdx,>squaret1=%r15
mov  %rdx,%r15

# qhasm:   squarerax = *(uint64 *)(pp + 8)
# asm 1: movq   8(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   8(<pp=%rsi),>squarerax=%rax
movq   8(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 8)
# asm 1: mulq  8(<pp=int64#2)
# asm 2: mulq  8(<pp=%rsi)
mulq  8(%rsi)

# qhasm:   squaret2 = squarerax
# asm 1: mov  <squarerax=int64#7,>squaret2=int64#14
# asm 2: mov  <squarerax=%rax,>squaret2=%rbx
mov  %rax,%rbx

# qhasm:   squaret3 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret3=int64#15
# asm 2: mov  <squarerdx=%rdx,>squaret3=%rbp
mov  %rdx,%rbp

# qhasm:   squarerax = *(uint64 *)(pp + 16)
# asm 1: movq   16(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   16(<pp=%rsi),>squarerax=%rax
movq   16(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 16)
# asm 1: mulq  16(<pp=int64#2)
# asm 2: mulq  16(<pp=%rsi)
mulq  16(%rsi)

# qhasm:   carry? a1 += squaret1
# asm 1: add  <squaret1=int64#13,<a1=int64#5
# asm 2: add  <squaret1=%r15,<a1=%r8
add  %r15,%r8

# qhasm:   carry? a2 += squaret2 + carry
# asm 1: adc <squaret2=int64#14,<a2=int64#6
# asm 2: adc <squaret2=%rbx,<a2=%r9
adc %rbx,%r9

# qhasm:   carry? a3 += squaret3 + carry
# asm 1: adc <squaret3=int64#15,<a3=int64#8
# asm 2: adc <squaret3=%rbp,<a3=%r10
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

# qhasm:   squarerax = *(uint64 *)(pp + 24)
# asm 1: movq   24(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   24(<pp=%rsi),>squarerax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 24)
# asm 1: mulq  24(<pp=int64#2)
# asm 2: mulq  24(<pp=%rsi)
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
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   squarer4 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer4=int64#9
# asm 2: mov  <squarerax=%rax,>squarer4=%r11
mov  %rax,%r11

# qhasm:   squarerax = squarer5
# asm 1: mov  <squarer5=int64#10,>squarerax=int64#7
# asm 2: mov  <squarer5=%r12,>squarerax=%rax
mov  %r12,%rax

# qhasm:   squarer5 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer5=int64#10
# asm 2: mov  <squarerdx=%rdx,>squarer5=%r12
mov  %rdx,%r12

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer5 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer5=int64#10
# asm 2: add  <squarerax=%rax,<squarer5=%r12
add  %rax,%r12

# qhasm:   squarerax = squarer6
# asm 1: mov  <squarer6=int64#11,>squarerax=int64#7
# asm 2: mov  <squarer6=%r13,>squarerax=%rax
mov  %r13,%rax

# qhasm:   squarer6 = 0
# asm 1: mov  $0,>squarer6=int64#11
# asm 2: mov  $0,>squarer6=%r13
mov  $0,%r13

# qhasm:   squarer6 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer6=int64#11
# asm 2: adc <squarerdx=%rdx,<squarer6=%r13
adc %rdx,%r13

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer6 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer6=int64#11
# asm 2: add  <squarerax=%rax,<squarer6=%r13
add  %rax,%r13

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
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

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

# qhasm:   carry? a0 += squarer4
# asm 1: add  <squarer4=int64#9,<a0=int64#12
# asm 2: add  <squarer4=%r11,<a0=%r14
add  %r11,%r14

# qhasm:   carry? a1 += squarer5 + carry
# asm 1: adc <squarer5=int64#10,<a1=int64#5
# asm 2: adc <squarer5=%r12,<a1=%r8
adc %r12,%r8

# qhasm:   carry? a2 += squarer6 + carry
# asm 1: adc <squarer6=int64#11,<a2=int64#6
# asm 2: adc <squarer6=%r13,<a2=%r9
adc %r13,%r9

# qhasm:   carry? a3 += squarer7 + carry
# asm 1: adc <squarer7=int64#4,<a3=int64#8
# asm 2: adc <squarer7=%rcx,<a3=%r10
adc %rcx,%r10

# qhasm:   squarezero = 0
# asm 1: mov  $0,>squarezero=int64#3
# asm 2: mov  $0,>squarezero=%rdx
mov  $0,%rdx

# qhasm:   squarer8 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<squarer8=int64#7
# asm 2: adc <squarezero=%rdx,<squarer8=%rax
adc %rdx,%rax

# qhasm:   squarer8 *= 38
# asm 1: imulq  $38,<squarer8=int64#7,>squarer8=int64#4
# asm 2: imulq  $38,<squarer8=%rax,>squarer8=%rcx
imulq  $38,%rax,%rcx

# qhasm:   carry? a0 += squarer8
# asm 1: add  <squarer8=int64#4,<a0=int64#12
# asm 2: add  <squarer8=%rcx,<a0=%r14
add  %rcx,%r14

# qhasm:   carry? a1 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<a1=int64#5
# asm 2: adc <squarezero=%rdx,<a1=%r8
adc %rdx,%r8

# qhasm:   carry? a2 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<a2=int64#6
# asm 2: adc <squarezero=%rdx,<a2=%r9
adc %rdx,%r9

# qhasm:   carry? a3 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<a3=int64#8
# asm 2: adc <squarezero=%rdx,<a3=%r10
adc %rdx,%r10

# qhasm:   squarezero += squarezero + carry
# asm 1: adc <squarezero=int64#3,<squarezero=int64#3
# asm 2: adc <squarezero=%rdx,<squarezero=%rdx
adc %rdx,%rdx

# qhasm:   squarezero *= 38
# asm 1: imulq  $38,<squarezero=int64#3,>squarezero=int64#3
# asm 2: imulq  $38,<squarezero=%rdx,>squarezero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   a0 += squarezero
# asm 1: add  <squarezero=int64#3,<a0=int64#12
# asm 2: add  <squarezero=%rdx,<a0=%r14
add  %rdx,%r14

# qhasm: a0_stack = a0
# asm 1: movq <a0=int64#12,>a0_stack=stack64#8
# asm 2: movq <a0=%r14,>a0_stack=56(%rsp)
movq %r14,56(%rsp)

# qhasm: a1_stack = a1
# asm 1: movq <a1=int64#5,>a1_stack=stack64#9
# asm 2: movq <a1=%r8,>a1_stack=64(%rsp)
movq %r8,64(%rsp)

# qhasm: a2_stack = a2
# asm 1: movq <a2=int64#6,>a2_stack=stack64#10
# asm 2: movq <a2=%r9,>a2_stack=72(%rsp)
movq %r9,72(%rsp)

# qhasm: a3_stack = a3
# asm 1: movq <a3=int64#8,>a3_stack=stack64#11
# asm 2: movq <a3=%r10,>a3_stack=80(%rsp)
movq %r10,80(%rsp)

# qhasm:   squarer7 = 0
# asm 1: mov  $0,>squarer7=int64#4
# asm 2: mov  $0,>squarer7=%rcx
mov  $0,%rcx

# qhasm:   squarerax = *(uint64 *)(pp + 40)
# asm 1: movq   40(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   40(<pp=%rsi),>squarerax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 32)
# asm 1: mulq  32(<pp=int64#2)
# asm 2: mulq  32(<pp=%rsi)
mulq  32(%rsi)

# qhasm:   b1 = squarerax
# asm 1: mov  <squarerax=int64#7,>b1=int64#5
# asm 2: mov  <squarerax=%rax,>b1=%r8
mov  %rax,%r8

# qhasm:   b2 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>b2=int64#6
# asm 2: mov  <squarerdx=%rdx,>b2=%r9
mov  %rdx,%r9

# qhasm:   squarerax = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   48(<pp=%rsi),>squarerax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 40)
# asm 1: mulq  40(<pp=int64#2)
# asm 2: mulq  40(<pp=%rsi)
mulq  40(%rsi)

# qhasm:   b3 = squarerax 
# asm 1: mov  <squarerax=int64#7,>b3=int64#8
# asm 2: mov  <squarerax=%rax,>b3=%r10
mov  %rax,%r10

# qhasm:   squarer4 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer4=int64#9
# asm 2: mov  <squarerdx=%rdx,>squarer4=%r11
mov  %rdx,%r11

# qhasm:   squarerax = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   56(<pp=%rsi),>squarerax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 48)
# asm 1: mulq  48(<pp=int64#2)
# asm 2: mulq  48(<pp=%rsi)
mulq  48(%rsi)

# qhasm:   squarer5 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer5=int64#10
# asm 2: mov  <squarerax=%rax,>squarer5=%r12
mov  %rax,%r12

# qhasm:   squarer6 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer6=int64#11
# asm 2: mov  <squarerdx=%rdx,>squarer6=%r13
mov  %rdx,%r13

# qhasm:   squarerax = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   48(<pp=%rsi),>squarerax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 32)
# asm 1: mulq  32(<pp=int64#2)
# asm 2: mulq  32(<pp=%rsi)
mulq  32(%rsi)

# qhasm:   carry? b2 += squarerax
# asm 1: add  <squarerax=int64#7,<b2=int64#6
# asm 2: add  <squarerax=%rax,<b2=%r9
add  %rax,%r9

# qhasm:   carry? b3 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<b3=int64#8
# asm 2: adc <squarerdx=%rdx,<b3=%r10
adc %rdx,%r10

# qhasm:   squarer4 += 0 + carry
# asm 1: adc $0,<squarer4=int64#9
# asm 2: adc $0,<squarer4=%r11
adc $0,%r11

# qhasm:   squarerax = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   56(<pp=%rsi),>squarerax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 40)
# asm 1: mulq  40(<pp=int64#2)
# asm 2: mulq  40(<pp=%rsi)
mulq  40(%rsi)

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

# qhasm:   squarerax = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   56(<pp=%rsi),>squarerax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 32)
# asm 1: mulq  32(<pp=int64#2)
# asm 2: mulq  32(<pp=%rsi)
mulq  32(%rsi)

# qhasm:   carry? b3 += squarerax
# asm 1: add  <squarerax=int64#7,<b3=int64#8
# asm 2: add  <squarerax=%rax,<b3=%r10
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

# qhasm:   carry? b1 += b1
# asm 1: add  <b1=int64#5,<b1=int64#5
# asm 2: add  <b1=%r8,<b1=%r8
add  %r8,%r8

# qhasm:   carry? b2 += b2 + carry
# asm 1: adc <b2=int64#6,<b2=int64#6
# asm 2: adc <b2=%r9,<b2=%r9
adc %r9,%r9

# qhasm:   carry? b3 += b3 + carry
# asm 1: adc <b3=int64#8,<b3=int64#8
# asm 2: adc <b3=%r10,<b3=%r10
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

# qhasm:   squarerax = *(uint64 *)(pp + 32)
# asm 1: movq   32(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   32(<pp=%rsi),>squarerax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 32)
# asm 1: mulq  32(<pp=int64#2)
# asm 2: mulq  32(<pp=%rsi)
mulq  32(%rsi)

# qhasm:   b0 = squarerax
# asm 1: mov  <squarerax=int64#7,>b0=int64#12
# asm 2: mov  <squarerax=%rax,>b0=%r14
mov  %rax,%r14

# qhasm:   squaret1 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret1=int64#13
# asm 2: mov  <squarerdx=%rdx,>squaret1=%r15
mov  %rdx,%r15

# qhasm:   squarerax = *(uint64 *)(pp + 40)
# asm 1: movq   40(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   40(<pp=%rsi),>squarerax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 40)
# asm 1: mulq  40(<pp=int64#2)
# asm 2: mulq  40(<pp=%rsi)
mulq  40(%rsi)

# qhasm:   squaret2 = squarerax
# asm 1: mov  <squarerax=int64#7,>squaret2=int64#14
# asm 2: mov  <squarerax=%rax,>squaret2=%rbx
mov  %rax,%rbx

# qhasm:   squaret3 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret3=int64#15
# asm 2: mov  <squarerdx=%rdx,>squaret3=%rbp
mov  %rdx,%rbp

# qhasm:   squarerax = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   48(<pp=%rsi),>squarerax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 48)
# asm 1: mulq  48(<pp=int64#2)
# asm 2: mulq  48(<pp=%rsi)
mulq  48(%rsi)

# qhasm:   carry? b1 += squaret1
# asm 1: add  <squaret1=int64#13,<b1=int64#5
# asm 2: add  <squaret1=%r15,<b1=%r8
add  %r15,%r8

# qhasm:   carry? b2 += squaret2 + carry
# asm 1: adc <squaret2=int64#14,<b2=int64#6
# asm 2: adc <squaret2=%rbx,<b2=%r9
adc %rbx,%r9

# qhasm:   carry? b3 += squaret3 + carry
# asm 1: adc <squaret3=int64#15,<b3=int64#8
# asm 2: adc <squaret3=%rbp,<b3=%r10
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

# qhasm:   squarerax = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   56(<pp=%rsi),>squarerax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 56)
# asm 1: mulq  56(<pp=int64#2)
# asm 2: mulq  56(<pp=%rsi)
mulq  56(%rsi)

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
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   squarer4 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer4=int64#9
# asm 2: mov  <squarerax=%rax,>squarer4=%r11
mov  %rax,%r11

# qhasm:   squarerax = squarer5
# asm 1: mov  <squarer5=int64#10,>squarerax=int64#7
# asm 2: mov  <squarer5=%r12,>squarerax=%rax
mov  %r12,%rax

# qhasm:   squarer5 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer5=int64#10
# asm 2: mov  <squarerdx=%rdx,>squarer5=%r12
mov  %rdx,%r12

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer5 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer5=int64#10
# asm 2: add  <squarerax=%rax,<squarer5=%r12
add  %rax,%r12

# qhasm:   squarerax = squarer6
# asm 1: mov  <squarer6=int64#11,>squarerax=int64#7
# asm 2: mov  <squarer6=%r13,>squarerax=%rax
mov  %r13,%rax

# qhasm:   squarer6 = 0
# asm 1: mov  $0,>squarer6=int64#11
# asm 2: mov  $0,>squarer6=%r13
mov  $0,%r13

# qhasm:   squarer6 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer6=int64#11
# asm 2: adc <squarerdx=%rdx,<squarer6=%r13
adc %rdx,%r13

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer6 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer6=int64#11
# asm 2: add  <squarerax=%rax,<squarer6=%r13
add  %rax,%r13

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
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

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

# qhasm:   carry? b0 += squarer4
# asm 1: add  <squarer4=int64#9,<b0=int64#12
# asm 2: add  <squarer4=%r11,<b0=%r14
add  %r11,%r14

# qhasm:   carry? b1 += squarer5 + carry
# asm 1: adc <squarer5=int64#10,<b1=int64#5
# asm 2: adc <squarer5=%r12,<b1=%r8
adc %r12,%r8

# qhasm:   carry? b2 += squarer6 + carry
# asm 1: adc <squarer6=int64#11,<b2=int64#6
# asm 2: adc <squarer6=%r13,<b2=%r9
adc %r13,%r9

# qhasm:   carry? b3 += squarer7 + carry
# asm 1: adc <squarer7=int64#4,<b3=int64#8
# asm 2: adc <squarer7=%rcx,<b3=%r10
adc %rcx,%r10

# qhasm:   squarezero = 0
# asm 1: mov  $0,>squarezero=int64#3
# asm 2: mov  $0,>squarezero=%rdx
mov  $0,%rdx

# qhasm:   squarer8 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<squarer8=int64#7
# asm 2: adc <squarezero=%rdx,<squarer8=%rax
adc %rdx,%rax

# qhasm:   squarer8 *= 38
# asm 1: imulq  $38,<squarer8=int64#7,>squarer8=int64#4
# asm 2: imulq  $38,<squarer8=%rax,>squarer8=%rcx
imulq  $38,%rax,%rcx

# qhasm:   carry? b0 += squarer8
# asm 1: add  <squarer8=int64#4,<b0=int64#12
# asm 2: add  <squarer8=%rcx,<b0=%r14
add  %rcx,%r14

# qhasm:   carry? b1 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<b1=int64#5
# asm 2: adc <squarezero=%rdx,<b1=%r8
adc %rdx,%r8

# qhasm:   carry? b2 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<b2=int64#6
# asm 2: adc <squarezero=%rdx,<b2=%r9
adc %rdx,%r9

# qhasm:   carry? b3 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<b3=int64#8
# asm 2: adc <squarezero=%rdx,<b3=%r10
adc %rdx,%r10

# qhasm:   squarezero += squarezero + carry
# asm 1: adc <squarezero=int64#3,<squarezero=int64#3
# asm 2: adc <squarezero=%rdx,<squarezero=%rdx
adc %rdx,%rdx

# qhasm:   squarezero *= 38
# asm 1: imulq  $38,<squarezero=int64#3,>squarezero=int64#3
# asm 2: imulq  $38,<squarezero=%rdx,>squarezero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   b0 += squarezero
# asm 1: add  <squarezero=int64#3,<b0=int64#12
# asm 2: add  <squarezero=%rdx,<b0=%r14
add  %rdx,%r14

# qhasm: b0_stack = b0
# asm 1: movq <b0=int64#12,>b0_stack=stack64#12
# asm 2: movq <b0=%r14,>b0_stack=88(%rsp)
movq %r14,88(%rsp)

# qhasm: b1_stack = b1
# asm 1: movq <b1=int64#5,>b1_stack=stack64#13
# asm 2: movq <b1=%r8,>b1_stack=96(%rsp)
movq %r8,96(%rsp)

# qhasm: b2_stack = b2
# asm 1: movq <b2=int64#6,>b2_stack=stack64#14
# asm 2: movq <b2=%r9,>b2_stack=104(%rsp)
movq %r9,104(%rsp)

# qhasm: b3_stack = b3
# asm 1: movq <b3=int64#8,>b3_stack=stack64#15
# asm 2: movq <b3=%r10,>b3_stack=112(%rsp)
movq %r10,112(%rsp)

# qhasm:   squarer7 = 0
# asm 1: mov  $0,>squarer7=int64#4
# asm 2: mov  $0,>squarer7=%rcx
mov  $0,%rcx

# qhasm:   squarerax = *(uint64 *)(pp + 72)
# asm 1: movq   72(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   72(<pp=%rsi),>squarerax=%rax
movq   72(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 64)
# asm 1: mulq  64(<pp=int64#2)
# asm 2: mulq  64(<pp=%rsi)
mulq  64(%rsi)

# qhasm:   c1 = squarerax
# asm 1: mov  <squarerax=int64#7,>c1=int64#5
# asm 2: mov  <squarerax=%rax,>c1=%r8
mov  %rax,%r8

# qhasm:   c2 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>c2=int64#6
# asm 2: mov  <squarerdx=%rdx,>c2=%r9
mov  %rdx,%r9

# qhasm:   squarerax = *(uint64 *)(pp + 80)
# asm 1: movq   80(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   80(<pp=%rsi),>squarerax=%rax
movq   80(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 72)
# asm 1: mulq  72(<pp=int64#2)
# asm 2: mulq  72(<pp=%rsi)
mulq  72(%rsi)

# qhasm:   c3 = squarerax 
# asm 1: mov  <squarerax=int64#7,>c3=int64#8
# asm 2: mov  <squarerax=%rax,>c3=%r10
mov  %rax,%r10

# qhasm:   squarer4 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer4=int64#9
# asm 2: mov  <squarerdx=%rdx,>squarer4=%r11
mov  %rdx,%r11

# qhasm:   squarerax = *(uint64 *)(pp + 88)
# asm 1: movq   88(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   88(<pp=%rsi),>squarerax=%rax
movq   88(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 80)
# asm 1: mulq  80(<pp=int64#2)
# asm 2: mulq  80(<pp=%rsi)
mulq  80(%rsi)

# qhasm:   squarer5 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer5=int64#10
# asm 2: mov  <squarerax=%rax,>squarer5=%r12
mov  %rax,%r12

# qhasm:   squarer6 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer6=int64#11
# asm 2: mov  <squarerdx=%rdx,>squarer6=%r13
mov  %rdx,%r13

# qhasm:   squarerax = *(uint64 *)(pp + 80)
# asm 1: movq   80(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   80(<pp=%rsi),>squarerax=%rax
movq   80(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 64)
# asm 1: mulq  64(<pp=int64#2)
# asm 2: mulq  64(<pp=%rsi)
mulq  64(%rsi)

# qhasm:   carry? c2 += squarerax
# asm 1: add  <squarerax=int64#7,<c2=int64#6
# asm 2: add  <squarerax=%rax,<c2=%r9
add  %rax,%r9

# qhasm:   carry? c3 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<c3=int64#8
# asm 2: adc <squarerdx=%rdx,<c3=%r10
adc %rdx,%r10

# qhasm:   squarer4 += 0 + carry
# asm 1: adc $0,<squarer4=int64#9
# asm 2: adc $0,<squarer4=%r11
adc $0,%r11

# qhasm:   squarerax = *(uint64 *)(pp + 88)
# asm 1: movq   88(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   88(<pp=%rsi),>squarerax=%rax
movq   88(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 72)
# asm 1: mulq  72(<pp=int64#2)
# asm 2: mulq  72(<pp=%rsi)
mulq  72(%rsi)

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

# qhasm:   squarerax = *(uint64 *)(pp + 88)
# asm 1: movq   88(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   88(<pp=%rsi),>squarerax=%rax
movq   88(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 64)
# asm 1: mulq  64(<pp=int64#2)
# asm 2: mulq  64(<pp=%rsi)
mulq  64(%rsi)

# qhasm:   carry? c3 += squarerax
# asm 1: add  <squarerax=int64#7,<c3=int64#8
# asm 2: add  <squarerax=%rax,<c3=%r10
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

# qhasm:   carry? c1 += c1
# asm 1: add  <c1=int64#5,<c1=int64#5
# asm 2: add  <c1=%r8,<c1=%r8
add  %r8,%r8

# qhasm:   carry? c2 += c2 + carry
# asm 1: adc <c2=int64#6,<c2=int64#6
# asm 2: adc <c2=%r9,<c2=%r9
adc %r9,%r9

# qhasm:   carry? c3 += c3 + carry
# asm 1: adc <c3=int64#8,<c3=int64#8
# asm 2: adc <c3=%r10,<c3=%r10
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

# qhasm:   squarerax = *(uint64 *)(pp + 64)
# asm 1: movq   64(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   64(<pp=%rsi),>squarerax=%rax
movq   64(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 64)
# asm 1: mulq  64(<pp=int64#2)
# asm 2: mulq  64(<pp=%rsi)
mulq  64(%rsi)

# qhasm:   c0 = squarerax
# asm 1: mov  <squarerax=int64#7,>c0=int64#12
# asm 2: mov  <squarerax=%rax,>c0=%r14
mov  %rax,%r14

# qhasm:   squaret1 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret1=int64#13
# asm 2: mov  <squarerdx=%rdx,>squaret1=%r15
mov  %rdx,%r15

# qhasm:   squarerax = *(uint64 *)(pp + 72)
# asm 1: movq   72(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   72(<pp=%rsi),>squarerax=%rax
movq   72(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 72)
# asm 1: mulq  72(<pp=int64#2)
# asm 2: mulq  72(<pp=%rsi)
mulq  72(%rsi)

# qhasm:   squaret2 = squarerax
# asm 1: mov  <squarerax=int64#7,>squaret2=int64#14
# asm 2: mov  <squarerax=%rax,>squaret2=%rbx
mov  %rax,%rbx

# qhasm:   squaret3 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret3=int64#15
# asm 2: mov  <squarerdx=%rdx,>squaret3=%rbp
mov  %rdx,%rbp

# qhasm:   squarerax = *(uint64 *)(pp + 80)
# asm 1: movq   80(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   80(<pp=%rsi),>squarerax=%rax
movq   80(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 80)
# asm 1: mulq  80(<pp=int64#2)
# asm 2: mulq  80(<pp=%rsi)
mulq  80(%rsi)

# qhasm:   carry? c1 += squaret1
# asm 1: add  <squaret1=int64#13,<c1=int64#5
# asm 2: add  <squaret1=%r15,<c1=%r8
add  %r15,%r8

# qhasm:   carry? c2 += squaret2 + carry
# asm 1: adc <squaret2=int64#14,<c2=int64#6
# asm 2: adc <squaret2=%rbx,<c2=%r9
adc %rbx,%r9

# qhasm:   carry? c3 += squaret3 + carry
# asm 1: adc <squaret3=int64#15,<c3=int64#8
# asm 2: adc <squaret3=%rbp,<c3=%r10
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

# qhasm:   squarerax = *(uint64 *)(pp + 88)
# asm 1: movq   88(<pp=int64#2),>squarerax=int64#7
# asm 2: movq   88(<pp=%rsi),>squarerax=%rax
movq   88(%rsi),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)(pp + 88)
# asm 1: mulq  88(<pp=int64#2)
# asm 2: mulq  88(<pp=%rsi)
mulq  88(%rsi)

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
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   squarer4 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer4=int64#9
# asm 2: mov  <squarerax=%rax,>squarer4=%r11
mov  %rax,%r11

# qhasm:   squarerax = squarer5
# asm 1: mov  <squarer5=int64#10,>squarerax=int64#7
# asm 2: mov  <squarer5=%r12,>squarerax=%rax
mov  %r12,%rax

# qhasm:   squarer5 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer5=int64#10
# asm 2: mov  <squarerdx=%rdx,>squarer5=%r12
mov  %rdx,%r12

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer5 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer5=int64#10
# asm 2: add  <squarerax=%rax,<squarer5=%r12
add  %rax,%r12

# qhasm:   squarerax = squarer6
# asm 1: mov  <squarer6=int64#11,>squarerax=int64#7
# asm 2: mov  <squarer6=%r13,>squarerax=%rax
mov  %r13,%rax

# qhasm:   squarer6 = 0
# asm 1: mov  $0,>squarer6=int64#11
# asm 2: mov  $0,>squarer6=%r13
mov  $0,%r13

# qhasm:   squarer6 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer6=int64#11
# asm 2: adc <squarerdx=%rdx,<squarer6=%r13
adc %rdx,%r13

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer6 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer6=int64#11
# asm 2: add  <squarerax=%rax,<squarer6=%r13
add  %rax,%r13

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
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

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

# qhasm:   carry? c0 += squarer4
# asm 1: add  <squarer4=int64#9,<c0=int64#12
# asm 2: add  <squarer4=%r11,<c0=%r14
add  %r11,%r14

# qhasm:   carry? c1 += squarer5 + carry
# asm 1: adc <squarer5=int64#10,<c1=int64#5
# asm 2: adc <squarer5=%r12,<c1=%r8
adc %r12,%r8

# qhasm:   carry? c2 += squarer6 + carry
# asm 1: adc <squarer6=int64#11,<c2=int64#6
# asm 2: adc <squarer6=%r13,<c2=%r9
adc %r13,%r9

# qhasm:   carry? c3 += squarer7 + carry
# asm 1: adc <squarer7=int64#4,<c3=int64#8
# asm 2: adc <squarer7=%rcx,<c3=%r10
adc %rcx,%r10

# qhasm:   squarezero = 0
# asm 1: mov  $0,>squarezero=int64#3
# asm 2: mov  $0,>squarezero=%rdx
mov  $0,%rdx

# qhasm:   squarer8 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<squarer8=int64#7
# asm 2: adc <squarezero=%rdx,<squarer8=%rax
adc %rdx,%rax

# qhasm:   squarer8 *= 38
# asm 1: imulq  $38,<squarer8=int64#7,>squarer8=int64#4
# asm 2: imulq  $38,<squarer8=%rax,>squarer8=%rcx
imulq  $38,%rax,%rcx

# qhasm:   carry? c0 += squarer8
# asm 1: add  <squarer8=int64#4,<c0=int64#12
# asm 2: add  <squarer8=%rcx,<c0=%r14
add  %rcx,%r14

# qhasm:   carry? c1 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<c1=int64#5
# asm 2: adc <squarezero=%rdx,<c1=%r8
adc %rdx,%r8

# qhasm:   carry? c2 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<c2=int64#6
# asm 2: adc <squarezero=%rdx,<c2=%r9
adc %rdx,%r9

# qhasm:   carry? c3 += squarezero + carry
# asm 1: adc <squarezero=int64#3,<c3=int64#8
# asm 2: adc <squarezero=%rdx,<c3=%r10
adc %rdx,%r10

# qhasm:   squarezero += squarezero + carry
# asm 1: adc <squarezero=int64#3,<squarezero=int64#3
# asm 2: adc <squarezero=%rdx,<squarezero=%rdx
adc %rdx,%rdx

# qhasm:   squarezero *= 38
# asm 1: imulq  $38,<squarezero=int64#3,>squarezero=int64#3
# asm 2: imulq  $38,<squarezero=%rdx,>squarezero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   c0 += squarezero
# asm 1: add  <squarezero=int64#3,<c0=int64#12
# asm 2: add  <squarezero=%rdx,<c0=%r14
add  %rdx,%r14

# qhasm:   carry? c0 += c0 
# asm 1: add  <c0=int64#12,<c0=int64#12
# asm 2: add  <c0=%r14,<c0=%r14
add  %r14,%r14

# qhasm:   carry? c1 += c1 + carry
# asm 1: adc <c1=int64#5,<c1=int64#5
# asm 2: adc <c1=%r8,<c1=%r8
adc %r8,%r8

# qhasm:   carry? c2 += c2 + carry
# asm 1: adc <c2=int64#6,<c2=int64#6
# asm 2: adc <c2=%r9,<c2=%r9
adc %r9,%r9

# qhasm:   carry? c3 += c3 + carry
# asm 1: adc <c3=int64#8,<c3=int64#8
# asm 2: adc <c3=%r10,<c3=%r10
adc %r10,%r10

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#3
# asm 2: mov  $0,>addt0=%rdx
mov  $0,%rdx

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#4
# asm 2: mov  $38,>addt1=%rcx
mov  $38,%rcx

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#3,<addt1=int64#4
# asm 2: cmovae <addt0=%rdx,<addt1=%rcx
cmovae %rdx,%rcx

# qhasm:   carry? c0 += addt1
# asm 1: add  <addt1=int64#4,<c0=int64#12
# asm 2: add  <addt1=%rcx,<c0=%r14
add  %rcx,%r14

# qhasm:   carry? c1 += addt0 + carry
# asm 1: adc <addt0=int64#3,<c1=int64#5
# asm 2: adc <addt0=%rdx,<c1=%r8
adc %rdx,%r8

# qhasm:   carry? c2 += addt0 + carry
# asm 1: adc <addt0=int64#3,<c2=int64#6
# asm 2: adc <addt0=%rdx,<c2=%r9
adc %rdx,%r9

# qhasm:   carry? c3 += addt0 + carry
# asm 1: adc <addt0=int64#3,<c3=int64#8
# asm 2: adc <addt0=%rdx,<c3=%r10
adc %rdx,%r10

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#4,<addt0=int64#3
# asm 2: cmovc <addt1=%rcx,<addt0=%rdx
cmovc %rcx,%rdx

# qhasm:   c0 += addt0
# asm 1: add  <addt0=int64#3,<c0=int64#12
# asm 2: add  <addt0=%rdx,<c0=%r14
add  %rdx,%r14

# qhasm: c0_stack = c0
# asm 1: movq <c0=int64#12,>c0_stack=stack64#16
# asm 2: movq <c0=%r14,>c0_stack=120(%rsp)
movq %r14,120(%rsp)

# qhasm: c1_stack = c1
# asm 1: movq <c1=int64#5,>c1_stack=stack64#17
# asm 2: movq <c1=%r8,>c1_stack=128(%rsp)
movq %r8,128(%rsp)

# qhasm: c2_stack = c2
# asm 1: movq <c2=int64#6,>c2_stack=stack64#18
# asm 2: movq <c2=%r9,>c2_stack=136(%rsp)
movq %r9,136(%rsp)

# qhasm: c3_stack = c3
# asm 1: movq <c3=int64#8,>c3_stack=stack64#19
# asm 2: movq <c3=%r10,>c3_stack=144(%rsp)
movq %r10,144(%rsp)

# qhasm: d0 = 0
# asm 1: mov  $0,>d0=int64#3
# asm 2: mov  $0,>d0=%rdx
mov  $0,%rdx

# qhasm: d1 = 0
# asm 1: mov  $0,>d1=int64#4
# asm 2: mov  $0,>d1=%rcx
mov  $0,%rcx

# qhasm: d2 = 0
# asm 1: mov  $0,>d2=int64#5
# asm 2: mov  $0,>d2=%r8
mov  $0,%r8

# qhasm: d3 = 0
# asm 1: mov  $0,>d3=int64#6
# asm 2: mov  $0,>d3=%r9
mov  $0,%r9

# qhasm:   carry? d0 -= a0_stack 
# asm 1: subq <a0_stack=stack64#8,<d0=int64#3
# asm 2: subq <a0_stack=56(%rsp),<d0=%rdx
subq 56(%rsp),%rdx

# qhasm:   carry? d1 -= a1_stack - carry
# asm 1: sbbq <a1_stack=stack64#9,<d1=int64#4
# asm 2: sbbq <a1_stack=64(%rsp),<d1=%rcx
sbbq 64(%rsp),%rcx

# qhasm:   carry? d2 -= a2_stack - carry
# asm 1: sbbq <a2_stack=stack64#10,<d2=int64#5
# asm 2: sbbq <a2_stack=72(%rsp),<d2=%r8
sbbq 72(%rsp),%r8

# qhasm:   carry? d3 -= a3_stack - carry
# asm 1: sbbq <a3_stack=stack64#11,<d3=int64#6
# asm 2: sbbq <a3_stack=80(%rsp),<d3=%r9
sbbq 80(%rsp),%r9

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#7
# asm 2: mov  $0,>subt0=%rax
mov  $0,%rax

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#8
# asm 2: mov  $38,>subt1=%r10
mov  $38,%r10

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#7,<subt1=int64#8
# asm 2: cmovae <subt0=%rax,<subt1=%r10
cmovae %rax,%r10

# qhasm:   carry? d0 -= subt1
# asm 1: sub  <subt1=int64#8,<d0=int64#3
# asm 2: sub  <subt1=%r10,<d0=%rdx
sub  %r10,%rdx

# qhasm:   carry? d1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#7,<d1=int64#4
# asm 2: sbb  <subt0=%rax,<d1=%rcx
sbb  %rax,%rcx

# qhasm:   carry? d2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#7,<d2=int64#5
# asm 2: sbb  <subt0=%rax,<d2=%r8
sbb  %rax,%r8

# qhasm:   carry? d3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#7,<d3=int64#6
# asm 2: sbb  <subt0=%rax,<d3=%r9
sbb  %rax,%r9

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#8,<subt0=int64#7
# asm 2: cmovc <subt1=%r10,<subt0=%rax
cmovc %r10,%rax

# qhasm:   d0 -= subt0
# asm 1: sub  <subt0=int64#7,<d0=int64#3
# asm 2: sub  <subt0=%rax,<d0=%rdx
sub  %rax,%rdx

# qhasm: d0_stack = d0
# asm 1: movq <d0=int64#3,>d0_stack=stack64#8
# asm 2: movq <d0=%rdx,>d0_stack=56(%rsp)
movq %rdx,56(%rsp)

# qhasm: d1_stack = d1
# asm 1: movq <d1=int64#4,>d1_stack=stack64#9
# asm 2: movq <d1=%rcx,>d1_stack=64(%rsp)
movq %rcx,64(%rsp)

# qhasm: d2_stack = d2
# asm 1: movq <d2=int64#5,>d2_stack=stack64#10
# asm 2: movq <d2=%r8,>d2_stack=72(%rsp)
movq %r8,72(%rsp)

# qhasm: d3_stack = d3
# asm 1: movq <d3=int64#6,>d3_stack=stack64#11
# asm 2: movq <d3=%r9,>d3_stack=80(%rsp)
movq %r9,80(%rsp)

# qhasm: e0 = 0
# asm 1: mov  $0,>e0=int64#7
# asm 2: mov  $0,>e0=%rax
mov  $0,%rax

# qhasm: e1 = 0
# asm 1: mov  $0,>e1=int64#8
# asm 2: mov  $0,>e1=%r10
mov  $0,%r10

# qhasm: e2 = 0
# asm 1: mov  $0,>e2=int64#9
# asm 2: mov  $0,>e2=%r11
mov  $0,%r11

# qhasm: e3 = 0
# asm 1: mov  $0,>e3=int64#10
# asm 2: mov  $0,>e3=%r12
mov  $0,%r12

# qhasm:   carry? e0 -= b0_stack 
# asm 1: subq <b0_stack=stack64#12,<e0=int64#7
# asm 2: subq <b0_stack=88(%rsp),<e0=%rax
subq 88(%rsp),%rax

# qhasm:   carry? e1 -= b1_stack - carry
# asm 1: sbbq <b1_stack=stack64#13,<e1=int64#8
# asm 2: sbbq <b1_stack=96(%rsp),<e1=%r10
sbbq 96(%rsp),%r10

# qhasm:   carry? e2 -= b2_stack - carry
# asm 1: sbbq <b2_stack=stack64#14,<e2=int64#9
# asm 2: sbbq <b2_stack=104(%rsp),<e2=%r11
sbbq 104(%rsp),%r11

# qhasm:   carry? e3 -= b3_stack - carry
# asm 1: sbbq <b3_stack=stack64#15,<e3=int64#10
# asm 2: sbbq <b3_stack=112(%rsp),<e3=%r12
sbbq 112(%rsp),%r12

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#11
# asm 2: mov  $0,>subt0=%r13
mov  $0,%r13

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#12
# asm 2: mov  $38,>subt1=%r14
mov  $38,%r14

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#11,<subt1=int64#12
# asm 2: cmovae <subt0=%r13,<subt1=%r14
cmovae %r13,%r14

# qhasm:   carry? e0 -= subt1
# asm 1: sub  <subt1=int64#12,<e0=int64#7
# asm 2: sub  <subt1=%r14,<e0=%rax
sub  %r14,%rax

# qhasm:   carry? e1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<e1=int64#8
# asm 2: sbb  <subt0=%r13,<e1=%r10
sbb  %r13,%r10

# qhasm:   carry? e2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<e2=int64#9
# asm 2: sbb  <subt0=%r13,<e2=%r11
sbb  %r13,%r11

# qhasm:   carry? e3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<e3=int64#10
# asm 2: sbb  <subt0=%r13,<e3=%r12
sbb  %r13,%r12

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#12,<subt0=int64#11
# asm 2: cmovc <subt1=%r14,<subt0=%r13
cmovc %r14,%r13

# qhasm:   e0 -= subt0
# asm 1: sub  <subt0=int64#11,<e0=int64#7
# asm 2: sub  <subt0=%r13,<e0=%rax
sub  %r13,%rax

# qhasm: e0_stack = e0
# asm 1: movq <e0=int64#7,>e0_stack=stack64#20
# asm 2: movq <e0=%rax,>e0_stack=152(%rsp)
movq %rax,152(%rsp)

# qhasm: e1_stack = e1
# asm 1: movq <e1=int64#8,>e1_stack=stack64#21
# asm 2: movq <e1=%r10,>e1_stack=160(%rsp)
movq %r10,160(%rsp)

# qhasm: e2_stack = e2
# asm 1: movq <e2=int64#9,>e2_stack=stack64#22
# asm 2: movq <e2=%r11,>e2_stack=168(%rsp)
movq %r11,168(%rsp)

# qhasm: e3_stack = e3
# asm 1: movq <e3=int64#10,>e3_stack=stack64#23
# asm 2: movq <e3=%r12,>e3_stack=176(%rsp)
movq %r12,176(%rsp)

# qhasm: rz0 = d0
# asm 1: mov  <d0=int64#3,>rz0=int64#7
# asm 2: mov  <d0=%rdx,>rz0=%rax
mov  %rdx,%rax

# qhasm: rz1 = d1
# asm 1: mov  <d1=int64#4,>rz1=int64#8
# asm 2: mov  <d1=%rcx,>rz1=%r10
mov  %rcx,%r10

# qhasm: rz2 = d2
# asm 1: mov  <d2=int64#5,>rz2=int64#9
# asm 2: mov  <d2=%r8,>rz2=%r11
mov  %r8,%r11

# qhasm: rz3 = d3
# asm 1: mov  <d3=int64#6,>rz3=int64#10
# asm 2: mov  <d3=%r9,>rz3=%r12
mov  %r9,%r12

# qhasm:   carry? rz0 += b0_stack 
# asm 1: addq <b0_stack=stack64#12,<rz0=int64#7
# asm 2: addq <b0_stack=88(%rsp),<rz0=%rax
addq 88(%rsp),%rax

# qhasm:   carry? rz1 += b1_stack + carry
# asm 1: adcq <b1_stack=stack64#13,<rz1=int64#8
# asm 2: adcq <b1_stack=96(%rsp),<rz1=%r10
adcq 96(%rsp),%r10

# qhasm:   carry? rz2 += b2_stack + carry
# asm 1: adcq <b2_stack=stack64#14,<rz2=int64#9
# asm 2: adcq <b2_stack=104(%rsp),<rz2=%r11
adcq 104(%rsp),%r11

# qhasm:   carry? rz3 += b3_stack + carry
# asm 1: adcq <b3_stack=stack64#15,<rz3=int64#10
# asm 2: adcq <b3_stack=112(%rsp),<rz3=%r12
adcq 112(%rsp),%r12

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#11
# asm 2: mov  $0,>addt0=%r13
mov  $0,%r13

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#12
# asm 2: mov  $38,>addt1=%r14
mov  $38,%r14

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#11,<addt1=int64#12
# asm 2: cmovae <addt0=%r13,<addt1=%r14
cmovae %r13,%r14

# qhasm:   carry? rz0 += addt1
# asm 1: add  <addt1=int64#12,<rz0=int64#7
# asm 2: add  <addt1=%r14,<rz0=%rax
add  %r14,%rax

# qhasm:   carry? rz1 += addt0 + carry
# asm 1: adc <addt0=int64#11,<rz1=int64#8
# asm 2: adc <addt0=%r13,<rz1=%r10
adc %r13,%r10

# qhasm:   carry? rz2 += addt0 + carry
# asm 1: adc <addt0=int64#11,<rz2=int64#9
# asm 2: adc <addt0=%r13,<rz2=%r11
adc %r13,%r11

# qhasm:   carry? rz3 += addt0 + carry
# asm 1: adc <addt0=int64#11,<rz3=int64#10
# asm 2: adc <addt0=%r13,<rz3=%r12
adc %r13,%r12

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#12,<addt0=int64#11
# asm 2: cmovc <addt1=%r14,<addt0=%r13
cmovc %r14,%r13

# qhasm:   rz0 += addt0
# asm 1: add  <addt0=int64#11,<rz0=int64#7
# asm 2: add  <addt0=%r13,<rz0=%rax
add  %r13,%rax

# qhasm: *(uint64 *) (rp + 32) = rz0
# asm 1: movq   <rz0=int64#7,32(<rp=int64#1)
# asm 2: movq   <rz0=%rax,32(<rp=%rdi)
movq   %rax,32(%rdi)

# qhasm: *(uint64 *) (rp + 40) = rz1
# asm 1: movq   <rz1=int64#8,40(<rp=int64#1)
# asm 2: movq   <rz1=%r10,40(<rp=%rdi)
movq   %r10,40(%rdi)

# qhasm: *(uint64 *) (rp + 48) = rz2
# asm 1: movq   <rz2=int64#9,48(<rp=int64#1)
# asm 2: movq   <rz2=%r11,48(<rp=%rdi)
movq   %r11,48(%rdi)

# qhasm: *(uint64 *) (rp + 56) = rz3
# asm 1: movq   <rz3=int64#10,56(<rp=int64#1)
# asm 2: movq   <rz3=%r12,56(<rp=%rdi)
movq   %r12,56(%rdi)

# qhasm:   carry? d0 -= b0_stack 
# asm 1: subq <b0_stack=stack64#12,<d0=int64#3
# asm 2: subq <b0_stack=88(%rsp),<d0=%rdx
subq 88(%rsp),%rdx

# qhasm:   carry? d1 -= b1_stack - carry
# asm 1: sbbq <b1_stack=stack64#13,<d1=int64#4
# asm 2: sbbq <b1_stack=96(%rsp),<d1=%rcx
sbbq 96(%rsp),%rcx

# qhasm:   carry? d2 -= b2_stack - carry
# asm 1: sbbq <b2_stack=stack64#14,<d2=int64#5
# asm 2: sbbq <b2_stack=104(%rsp),<d2=%r8
sbbq 104(%rsp),%r8

# qhasm:   carry? d3 -= b3_stack - carry
# asm 1: sbbq <b3_stack=stack64#15,<d3=int64#6
# asm 2: sbbq <b3_stack=112(%rsp),<d3=%r9
sbbq 112(%rsp),%r9

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#11
# asm 2: mov  $0,>subt0=%r13
mov  $0,%r13

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#12
# asm 2: mov  $38,>subt1=%r14
mov  $38,%r14

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#11,<subt1=int64#12
# asm 2: cmovae <subt0=%r13,<subt1=%r14
cmovae %r13,%r14

# qhasm:   carry? d0 -= subt1
# asm 1: sub  <subt1=int64#12,<d0=int64#3
# asm 2: sub  <subt1=%r14,<d0=%rdx
sub  %r14,%rdx

# qhasm:   carry? d1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<d1=int64#4
# asm 2: sbb  <subt0=%r13,<d1=%rcx
sbb  %r13,%rcx

# qhasm:   carry? d2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<d2=int64#5
# asm 2: sbb  <subt0=%r13,<d2=%r8
sbb  %r13,%r8

# qhasm:   carry? d3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<d3=int64#6
# asm 2: sbb  <subt0=%r13,<d3=%r9
sbb  %r13,%r9

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#12,<subt0=int64#11
# asm 2: cmovc <subt1=%r14,<subt0=%r13
cmovc %r14,%r13

# qhasm:   d0 -= subt0
# asm 1: sub  <subt0=int64#11,<d0=int64#3
# asm 2: sub  <subt0=%r13,<d0=%rdx
sub  %r13,%rdx

# qhasm: *(uint64 *)(rp + 64) = d0
# asm 1: movq   <d0=int64#3,64(<rp=int64#1)
# asm 2: movq   <d0=%rdx,64(<rp=%rdi)
movq   %rdx,64(%rdi)

# qhasm: *(uint64 *)(rp + 72) = d1
# asm 1: movq   <d1=int64#4,72(<rp=int64#1)
# asm 2: movq   <d1=%rcx,72(<rp=%rdi)
movq   %rcx,72(%rdi)

# qhasm: *(uint64 *)(rp + 80) = d2
# asm 1: movq   <d2=int64#5,80(<rp=int64#1)
# asm 2: movq   <d2=%r8,80(<rp=%rdi)
movq   %r8,80(%rdi)

# qhasm: *(uint64 *)(rp + 88) = d3
# asm 1: movq   <d3=int64#6,88(<rp=int64#1)
# asm 2: movq   <d3=%r9,88(<rp=%rdi)
movq   %r9,88(%rdi)

# qhasm:   carry? rz0 -= c0_stack 
# asm 1: subq <c0_stack=stack64#16,<rz0=int64#7
# asm 2: subq <c0_stack=120(%rsp),<rz0=%rax
subq 120(%rsp),%rax

# qhasm:   carry? rz1 -= c1_stack - carry
# asm 1: sbbq <c1_stack=stack64#17,<rz1=int64#8
# asm 2: sbbq <c1_stack=128(%rsp),<rz1=%r10
sbbq 128(%rsp),%r10

# qhasm:   carry? rz2 -= c2_stack - carry
# asm 1: sbbq <c2_stack=stack64#18,<rz2=int64#9
# asm 2: sbbq <c2_stack=136(%rsp),<rz2=%r11
sbbq 136(%rsp),%r11

# qhasm:   carry? rz3 -= c3_stack - carry
# asm 1: sbbq <c3_stack=stack64#19,<rz3=int64#10
# asm 2: sbbq <c3_stack=144(%rsp),<rz3=%r12
sbbq 144(%rsp),%r12

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#3
# asm 2: mov  $0,>subt0=%rdx
mov  $0,%rdx

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#4
# asm 2: mov  $38,>subt1=%rcx
mov  $38,%rcx

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#3,<subt1=int64#4
# asm 2: cmovae <subt0=%rdx,<subt1=%rcx
cmovae %rdx,%rcx

# qhasm:   carry? rz0 -= subt1
# asm 1: sub  <subt1=int64#4,<rz0=int64#7
# asm 2: sub  <subt1=%rcx,<rz0=%rax
sub  %rcx,%rax

# qhasm:   carry? rz1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#3,<rz1=int64#8
# asm 2: sbb  <subt0=%rdx,<rz1=%r10
sbb  %rdx,%r10

# qhasm:   carry? rz2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#3,<rz2=int64#9
# asm 2: sbb  <subt0=%rdx,<rz2=%r11
sbb  %rdx,%r11

# qhasm:   carry? rz3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#3,<rz3=int64#10
# asm 2: sbb  <subt0=%rdx,<rz3=%r12
sbb  %rdx,%r12

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#4,<subt0=int64#3
# asm 2: cmovc <subt1=%rcx,<subt0=%rdx
cmovc %rcx,%rdx

# qhasm:   rz0 -= subt0
# asm 1: sub  <subt0=int64#3,<rz0=int64#7
# asm 2: sub  <subt0=%rdx,<rz0=%rax
sub  %rdx,%rax

# qhasm: *(uint64 *) (rp + 96) = rz0
# asm 1: movq   <rz0=int64#7,96(<rp=int64#1)
# asm 2: movq   <rz0=%rax,96(<rp=%rdi)
movq   %rax,96(%rdi)

# qhasm: *(uint64 *) (rp + 104) = rz1
# asm 1: movq   <rz1=int64#8,104(<rp=int64#1)
# asm 2: movq   <rz1=%r10,104(<rp=%rdi)
movq   %r10,104(%rdi)

# qhasm: *(uint64 *) (rp + 112) = rz2
# asm 1: movq   <rz2=int64#9,112(<rp=int64#1)
# asm 2: movq   <rz2=%r11,112(<rp=%rdi)
movq   %r11,112(%rdi)

# qhasm: *(uint64 *) (rp + 120) = rz3
# asm 1: movq   <rz3=int64#10,120(<rp=int64#1)
# asm 2: movq   <rz3=%r12,120(<rp=%rdi)
movq   %r12,120(%rdi)

# qhasm: rx0 = *(uint64 *)(pp + 0)
# asm 1: movq   0(<pp=int64#2),>rx0=int64#3
# asm 2: movq   0(<pp=%rsi),>rx0=%rdx
movq   0(%rsi),%rdx

# qhasm: rx1 = *(uint64 *)(pp + 8)
# asm 1: movq   8(<pp=int64#2),>rx1=int64#4
# asm 2: movq   8(<pp=%rsi),>rx1=%rcx
movq   8(%rsi),%rcx

# qhasm: rx2 = *(uint64 *)(pp + 16)
# asm 1: movq   16(<pp=int64#2),>rx2=int64#5
# asm 2: movq   16(<pp=%rsi),>rx2=%r8
movq   16(%rsi),%r8

# qhasm: rx3 = *(uint64 *)(pp + 24)
# asm 1: movq   24(<pp=int64#2),>rx3=int64#6
# asm 2: movq   24(<pp=%rsi),>rx3=%r9
movq   24(%rsi),%r9

# qhasm:   carry? rx0 += *(uint64 *)(pp + 32) 
# asm 1: addq 32(<pp=int64#2),<rx0=int64#3
# asm 2: addq 32(<pp=%rsi),<rx0=%rdx
addq 32(%rsi),%rdx

# qhasm:   carry? rx1 += *(uint64 *)(pp + 40) + carry
# asm 1: adcq 40(<pp=int64#2),<rx1=int64#4
# asm 2: adcq 40(<pp=%rsi),<rx1=%rcx
adcq 40(%rsi),%rcx

# qhasm:   carry? rx2 += *(uint64 *)(pp + 48) + carry
# asm 1: adcq 48(<pp=int64#2),<rx2=int64#5
# asm 2: adcq 48(<pp=%rsi),<rx2=%r8
adcq 48(%rsi),%r8

# qhasm:   carry? rx3 += *(uint64 *)(pp + 56) + carry
# asm 1: adcq 56(<pp=int64#2),<rx3=int64#6
# asm 2: adcq 56(<pp=%rsi),<rx3=%r9
adcq 56(%rsi),%r9

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#2
# asm 2: mov  $0,>addt0=%rsi
mov  $0,%rsi

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#7
# asm 2: mov  $38,>addt1=%rax
mov  $38,%rax

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#2,<addt1=int64#7
# asm 2: cmovae <addt0=%rsi,<addt1=%rax
cmovae %rsi,%rax

# qhasm:   carry? rx0 += addt1
# asm 1: add  <addt1=int64#7,<rx0=int64#3
# asm 2: add  <addt1=%rax,<rx0=%rdx
add  %rax,%rdx

# qhasm:   carry? rx1 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx1=int64#4
# asm 2: adc <addt0=%rsi,<rx1=%rcx
adc %rsi,%rcx

# qhasm:   carry? rx2 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx2=int64#5
# asm 2: adc <addt0=%rsi,<rx2=%r8
adc %rsi,%r8

# qhasm:   carry? rx3 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx3=int64#6
# asm 2: adc <addt0=%rsi,<rx3=%r9
adc %rsi,%r9

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#7,<addt0=int64#2
# asm 2: cmovc <addt1=%rax,<addt0=%rsi
cmovc %rax,%rsi

# qhasm:   rx0 += addt0
# asm 1: add  <addt0=int64#2,<rx0=int64#3
# asm 2: add  <addt0=%rsi,<rx0=%rdx
add  %rsi,%rdx

# qhasm: rx0_stack = rx0
# asm 1: movq <rx0=int64#3,>rx0_stack=stack64#12
# asm 2: movq <rx0=%rdx,>rx0_stack=88(%rsp)
movq %rdx,88(%rsp)

# qhasm: rx1_stack = rx1
# asm 1: movq <rx1=int64#4,>rx1_stack=stack64#13
# asm 2: movq <rx1=%rcx,>rx1_stack=96(%rsp)
movq %rcx,96(%rsp)

# qhasm: rx2_stack = rx2
# asm 1: movq <rx2=int64#5,>rx2_stack=stack64#14
# asm 2: movq <rx2=%r8,>rx2_stack=104(%rsp)
movq %r8,104(%rsp)

# qhasm: rx3_stack = rx3
# asm 1: movq <rx3=int64#6,>rx3_stack=stack64#15
# asm 2: movq <rx3=%r9,>rx3_stack=112(%rsp)
movq %r9,112(%rsp)

# qhasm:   squarer7 = 0
# asm 1: mov  $0,>squarer7=int64#2
# asm 2: mov  $0,>squarer7=%rsi
mov  $0,%rsi

# qhasm:   squarerax = rx1_stack
# asm 1: movq <rx1_stack=stack64#13,>squarerax=int64#7
# asm 2: movq <rx1_stack=96(%rsp),>squarerax=%rax
movq 96(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx0_stack
# asm 1: mulq  <rx0_stack=stack64#12
# asm 2: mulq  <rx0_stack=88(%rsp)
mulq  88(%rsp)

# qhasm:   rx1 = squarerax
# asm 1: mov  <squarerax=int64#7,>rx1=int64#4
# asm 2: mov  <squarerax=%rax,>rx1=%rcx
mov  %rax,%rcx

# qhasm:   rx2 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>rx2=int64#5
# asm 2: mov  <squarerdx=%rdx,>rx2=%r8
mov  %rdx,%r8

# qhasm:   squarerax = rx2_stack
# asm 1: movq <rx2_stack=stack64#14,>squarerax=int64#7
# asm 2: movq <rx2_stack=104(%rsp),>squarerax=%rax
movq 104(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx1_stack
# asm 1: mulq  <rx1_stack=stack64#13
# asm 2: mulq  <rx1_stack=96(%rsp)
mulq  96(%rsp)

# qhasm:   rx3 = squarerax 
# asm 1: mov  <squarerax=int64#7,>rx3=int64#6
# asm 2: mov  <squarerax=%rax,>rx3=%r9
mov  %rax,%r9

# qhasm:   squarer4 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer4=int64#8
# asm 2: mov  <squarerdx=%rdx,>squarer4=%r10
mov  %rdx,%r10

# qhasm:   squarerax = rx3_stack
# asm 1: movq <rx3_stack=stack64#15,>squarerax=int64#7
# asm 2: movq <rx3_stack=112(%rsp),>squarerax=%rax
movq 112(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx2_stack
# asm 1: mulq  <rx2_stack=stack64#14
# asm 2: mulq  <rx2_stack=104(%rsp)
mulq  104(%rsp)

# qhasm:   squarer5 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer5=int64#9
# asm 2: mov  <squarerax=%rax,>squarer5=%r11
mov  %rax,%r11

# qhasm:   squarer6 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer6=int64#10
# asm 2: mov  <squarerdx=%rdx,>squarer6=%r12
mov  %rdx,%r12

# qhasm:   squarerax = rx2_stack
# asm 1: movq <rx2_stack=stack64#14,>squarerax=int64#7
# asm 2: movq <rx2_stack=104(%rsp),>squarerax=%rax
movq 104(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx0_stack
# asm 1: mulq  <rx0_stack=stack64#12
# asm 2: mulq  <rx0_stack=88(%rsp)
mulq  88(%rsp)

# qhasm:   carry? rx2 += squarerax
# asm 1: add  <squarerax=int64#7,<rx2=int64#5
# asm 2: add  <squarerax=%rax,<rx2=%r8
add  %rax,%r8

# qhasm:   carry? rx3 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<rx3=int64#6
# asm 2: adc <squarerdx=%rdx,<rx3=%r9
adc %rdx,%r9

# qhasm:   squarer4 += 0 + carry
# asm 1: adc $0,<squarer4=int64#8
# asm 2: adc $0,<squarer4=%r10
adc $0,%r10

# qhasm:   squarerax = rx3_stack
# asm 1: movq <rx3_stack=stack64#15,>squarerax=int64#7
# asm 2: movq <rx3_stack=112(%rsp),>squarerax=%rax
movq 112(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx1_stack
# asm 1: mulq  <rx1_stack=stack64#13
# asm 2: mulq  <rx1_stack=96(%rsp)
mulq  96(%rsp)

# qhasm:   carry? squarer4 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer4=int64#8
# asm 2: add  <squarerax=%rax,<squarer4=%r10
add  %rax,%r10

# qhasm:   carry? squarer5 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer5=int64#9
# asm 2: adc <squarerdx=%rdx,<squarer5=%r11
adc %rdx,%r11

# qhasm:   squarer6 += 0 + carry
# asm 1: adc $0,<squarer6=int64#10
# asm 2: adc $0,<squarer6=%r12
adc $0,%r12

# qhasm:   squarerax = rx3_stack
# asm 1: movq <rx3_stack=stack64#15,>squarerax=int64#7
# asm 2: movq <rx3_stack=112(%rsp),>squarerax=%rax
movq 112(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx0_stack
# asm 1: mulq  <rx0_stack=stack64#12
# asm 2: mulq  <rx0_stack=88(%rsp)
mulq  88(%rsp)

# qhasm:   carry? rx3 += squarerax
# asm 1: add  <squarerax=int64#7,<rx3=int64#6
# asm 2: add  <squarerax=%rax,<rx3=%r9
add  %rax,%r9

# qhasm:   carry? squarer4 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer4=int64#8
# asm 2: adc <squarerdx=%rdx,<squarer4=%r10
adc %rdx,%r10

# qhasm:   carry? squarer5 += 0 + carry
# asm 1: adc $0,<squarer5=int64#9
# asm 2: adc $0,<squarer5=%r11
adc $0,%r11

# qhasm:   carry? squarer6 += 0 + carry
# asm 1: adc $0,<squarer6=int64#10
# asm 2: adc $0,<squarer6=%r12
adc $0,%r12

# qhasm:   squarer7 += 0 + carry
# asm 1: adc $0,<squarer7=int64#2
# asm 2: adc $0,<squarer7=%rsi
adc $0,%rsi

# qhasm:   carry? rx1 += rx1
# asm 1: add  <rx1=int64#4,<rx1=int64#4
# asm 2: add  <rx1=%rcx,<rx1=%rcx
add  %rcx,%rcx

# qhasm:   carry? rx2 += rx2 + carry
# asm 1: adc <rx2=int64#5,<rx2=int64#5
# asm 2: adc <rx2=%r8,<rx2=%r8
adc %r8,%r8

# qhasm:   carry? rx3 += rx3 + carry
# asm 1: adc <rx3=int64#6,<rx3=int64#6
# asm 2: adc <rx3=%r9,<rx3=%r9
adc %r9,%r9

# qhasm:   carry? squarer4 += squarer4 + carry
# asm 1: adc <squarer4=int64#8,<squarer4=int64#8
# asm 2: adc <squarer4=%r10,<squarer4=%r10
adc %r10,%r10

# qhasm:   carry? squarer5 += squarer5 + carry
# asm 1: adc <squarer5=int64#9,<squarer5=int64#9
# asm 2: adc <squarer5=%r11,<squarer5=%r11
adc %r11,%r11

# qhasm:   carry? squarer6 += squarer6 + carry
# asm 1: adc <squarer6=int64#10,<squarer6=int64#10
# asm 2: adc <squarer6=%r12,<squarer6=%r12
adc %r12,%r12

# qhasm:   squarer7 += squarer7 + carry
# asm 1: adc <squarer7=int64#2,<squarer7=int64#2
# asm 2: adc <squarer7=%rsi,<squarer7=%rsi
adc %rsi,%rsi

# qhasm:   squarerax = rx0_stack
# asm 1: movq <rx0_stack=stack64#12,>squarerax=int64#7
# asm 2: movq <rx0_stack=88(%rsp),>squarerax=%rax
movq 88(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx0_stack
# asm 1: mulq  <rx0_stack=stack64#12
# asm 2: mulq  <rx0_stack=88(%rsp)
mulq  88(%rsp)

# qhasm:   rx0 = squarerax
# asm 1: mov  <squarerax=int64#7,>rx0=int64#11
# asm 2: mov  <squarerax=%rax,>rx0=%r13
mov  %rax,%r13

# qhasm:   squaret1 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret1=int64#12
# asm 2: mov  <squarerdx=%rdx,>squaret1=%r14
mov  %rdx,%r14

# qhasm:   squarerax = rx1_stack
# asm 1: movq <rx1_stack=stack64#13,>squarerax=int64#7
# asm 2: movq <rx1_stack=96(%rsp),>squarerax=%rax
movq 96(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx1_stack
# asm 1: mulq  <rx1_stack=stack64#13
# asm 2: mulq  <rx1_stack=96(%rsp)
mulq  96(%rsp)

# qhasm:   squaret2 = squarerax
# asm 1: mov  <squarerax=int64#7,>squaret2=int64#13
# asm 2: mov  <squarerax=%rax,>squaret2=%r15
mov  %rax,%r15

# qhasm:   squaret3 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squaret3=int64#14
# asm 2: mov  <squarerdx=%rdx,>squaret3=%rbx
mov  %rdx,%rbx

# qhasm:   squarerax = rx2_stack
# asm 1: movq <rx2_stack=stack64#14,>squarerax=int64#7
# asm 2: movq <rx2_stack=104(%rsp),>squarerax=%rax
movq 104(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx2_stack
# asm 1: mulq  <rx2_stack=stack64#14
# asm 2: mulq  <rx2_stack=104(%rsp)
mulq  104(%rsp)

# qhasm:   carry? rx1 += squaret1
# asm 1: add  <squaret1=int64#12,<rx1=int64#4
# asm 2: add  <squaret1=%r14,<rx1=%rcx
add  %r14,%rcx

# qhasm:   carry? rx2 += squaret2 + carry
# asm 1: adc <squaret2=int64#13,<rx2=int64#5
# asm 2: adc <squaret2=%r15,<rx2=%r8
adc %r15,%r8

# qhasm:   carry? rx3 += squaret3 + carry
# asm 1: adc <squaret3=int64#14,<rx3=int64#6
# asm 2: adc <squaret3=%rbx,<rx3=%r9
adc %rbx,%r9

# qhasm:   carry? squarer4 += squarerax + carry
# asm 1: adc <squarerax=int64#7,<squarer4=int64#8
# asm 2: adc <squarerax=%rax,<squarer4=%r10
adc %rax,%r10

# qhasm:   carry? squarer5 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer5=int64#9
# asm 2: adc <squarerdx=%rdx,<squarer5=%r11
adc %rdx,%r11

# qhasm:   carry? squarer6 += 0 + carry
# asm 1: adc $0,<squarer6=int64#10
# asm 2: adc $0,<squarer6=%r12
adc $0,%r12

# qhasm:   squarer7 += 0 + carry 
# asm 1: adc $0,<squarer7=int64#2
# asm 2: adc $0,<squarer7=%rsi
adc $0,%rsi

# qhasm:   squarerax = rx3_stack
# asm 1: movq <rx3_stack=stack64#15,>squarerax=int64#7
# asm 2: movq <rx3_stack=112(%rsp),>squarerax=%rax
movq 112(%rsp),%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * rx3_stack
# asm 1: mulq  <rx3_stack=stack64#15
# asm 2: mulq  <rx3_stack=112(%rsp)
mulq  112(%rsp)

# qhasm:   carry? squarer6 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer6=int64#10
# asm 2: add  <squarerax=%rax,<squarer6=%r12
add  %rax,%r12

# qhasm:   squarer7 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer7=int64#2
# asm 2: adc <squarerdx=%rdx,<squarer7=%rsi
adc %rdx,%rsi

# qhasm:   squarerax = squarer4
# asm 1: mov  <squarer4=int64#8,>squarerax=int64#7
# asm 2: mov  <squarer4=%r10,>squarerax=%rax
mov  %r10,%rax

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   squarer4 = squarerax
# asm 1: mov  <squarerax=int64#7,>squarer4=int64#8
# asm 2: mov  <squarerax=%rax,>squarer4=%r10
mov  %rax,%r10

# qhasm:   squarerax = squarer5
# asm 1: mov  <squarer5=int64#9,>squarerax=int64#7
# asm 2: mov  <squarer5=%r11,>squarerax=%rax
mov  %r11,%rax

# qhasm:   squarer5 = squarerdx
# asm 1: mov  <squarerdx=int64#3,>squarer5=int64#9
# asm 2: mov  <squarerdx=%rdx,>squarer5=%r11
mov  %rdx,%r11

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer5 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer5=int64#9
# asm 2: add  <squarerax=%rax,<squarer5=%r11
add  %rax,%r11

# qhasm:   squarerax = squarer6
# asm 1: mov  <squarer6=int64#10,>squarerax=int64#7
# asm 2: mov  <squarer6=%r12,>squarerax=%rax
mov  %r12,%rax

# qhasm:   squarer6 = 0
# asm 1: mov  $0,>squarer6=int64#10
# asm 2: mov  $0,>squarer6=%r12
mov  $0,%r12

# qhasm:   squarer6 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer6=int64#10
# asm 2: adc <squarerdx=%rdx,<squarer6=%r12
adc %rdx,%r12

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer6 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer6=int64#10
# asm 2: add  <squarerax=%rax,<squarer6=%r12
add  %rax,%r12

# qhasm:   squarerax = squarer7
# asm 1: mov  <squarer7=int64#2,>squarerax=int64#7
# asm 2: mov  <squarer7=%rsi,>squarerax=%rax
mov  %rsi,%rax

# qhasm:   squarer7 = 0
# asm 1: mov  $0,>squarer7=int64#2
# asm 2: mov  $0,>squarer7=%rsi
mov  $0,%rsi

# qhasm:   squarer7 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer7=int64#2
# asm 2: adc <squarerdx=%rdx,<squarer7=%rsi
adc %rdx,%rsi

# qhasm:   (uint128) squarerdx squarerax = squarerax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? squarer7 += squarerax
# asm 1: add  <squarerax=int64#7,<squarer7=int64#2
# asm 2: add  <squarerax=%rax,<squarer7=%rsi
add  %rax,%rsi

# qhasm:   squarer8 = 0
# asm 1: mov  $0,>squarer8=int64#7
# asm 2: mov  $0,>squarer8=%rax
mov  $0,%rax

# qhasm:   squarer8 += squarerdx + carry
# asm 1: adc <squarerdx=int64#3,<squarer8=int64#7
# asm 2: adc <squarerdx=%rdx,<squarer8=%rax
adc %rdx,%rax

# qhasm:   carry? rx0 += squarer4
# asm 1: add  <squarer4=int64#8,<rx0=int64#11
# asm 2: add  <squarer4=%r10,<rx0=%r13
add  %r10,%r13

# qhasm:   carry? rx1 += squarer5 + carry
# asm 1: adc <squarer5=int64#9,<rx1=int64#4
# asm 2: adc <squarer5=%r11,<rx1=%rcx
adc %r11,%rcx

# qhasm:   carry? rx2 += squarer6 + carry
# asm 1: adc <squarer6=int64#10,<rx2=int64#5
# asm 2: adc <squarer6=%r12,<rx2=%r8
adc %r12,%r8

# qhasm:   carry? rx3 += squarer7 + carry
# asm 1: adc <squarer7=int64#2,<rx3=int64#6
# asm 2: adc <squarer7=%rsi,<rx3=%r9
adc %rsi,%r9

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

# qhasm:   carry? rx0 += squarer8
# asm 1: add  <squarer8=int64#3,<rx0=int64#11
# asm 2: add  <squarer8=%rdx,<rx0=%r13
add  %rdx,%r13

# qhasm:   carry? rx1 += squarezero + carry
# asm 1: adc <squarezero=int64#2,<rx1=int64#4
# asm 2: adc <squarezero=%rsi,<rx1=%rcx
adc %rsi,%rcx

# qhasm:   carry? rx2 += squarezero + carry
# asm 1: adc <squarezero=int64#2,<rx2=int64#5
# asm 2: adc <squarezero=%rsi,<rx2=%r8
adc %rsi,%r8

# qhasm:   carry? rx3 += squarezero + carry
# asm 1: adc <squarezero=int64#2,<rx3=int64#6
# asm 2: adc <squarezero=%rsi,<rx3=%r9
adc %rsi,%r9

# qhasm:   squarezero += squarezero + carry
# asm 1: adc <squarezero=int64#2,<squarezero=int64#2
# asm 2: adc <squarezero=%rsi,<squarezero=%rsi
adc %rsi,%rsi

# qhasm:   squarezero *= 38
# asm 1: imulq  $38,<squarezero=int64#2,>squarezero=int64#2
# asm 2: imulq  $38,<squarezero=%rsi,>squarezero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   rx0 += squarezero
# asm 1: add  <squarezero=int64#2,<rx0=int64#11
# asm 2: add  <squarezero=%rsi,<rx0=%r13
add  %rsi,%r13

# qhasm:   carry? rx0 += d0_stack 
# asm 1: addq <d0_stack=stack64#8,<rx0=int64#11
# asm 2: addq <d0_stack=56(%rsp),<rx0=%r13
addq 56(%rsp),%r13

# qhasm:   carry? rx1 += d1_stack + carry
# asm 1: adcq <d1_stack=stack64#9,<rx1=int64#4
# asm 2: adcq <d1_stack=64(%rsp),<rx1=%rcx
adcq 64(%rsp),%rcx

# qhasm:   carry? rx2 += d2_stack + carry
# asm 1: adcq <d2_stack=stack64#10,<rx2=int64#5
# asm 2: adcq <d2_stack=72(%rsp),<rx2=%r8
adcq 72(%rsp),%r8

# qhasm:   carry? rx3 += d3_stack + carry
# asm 1: adcq <d3_stack=stack64#11,<rx3=int64#6
# asm 2: adcq <d3_stack=80(%rsp),<rx3=%r9
adcq 80(%rsp),%r9

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#2
# asm 2: mov  $0,>addt0=%rsi
mov  $0,%rsi

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#3
# asm 2: mov  $38,>addt1=%rdx
mov  $38,%rdx

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#2,<addt1=int64#3
# asm 2: cmovae <addt0=%rsi,<addt1=%rdx
cmovae %rsi,%rdx

# qhasm:   carry? rx0 += addt1
# asm 1: add  <addt1=int64#3,<rx0=int64#11
# asm 2: add  <addt1=%rdx,<rx0=%r13
add  %rdx,%r13

# qhasm:   carry? rx1 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx1=int64#4
# asm 2: adc <addt0=%rsi,<rx1=%rcx
adc %rsi,%rcx

# qhasm:   carry? rx2 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx2=int64#5
# asm 2: adc <addt0=%rsi,<rx2=%r8
adc %rsi,%r8

# qhasm:   carry? rx3 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx3=int64#6
# asm 2: adc <addt0=%rsi,<rx3=%r9
adc %rsi,%r9

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#3,<addt0=int64#2
# asm 2: cmovc <addt1=%rdx,<addt0=%rsi
cmovc %rdx,%rsi

# qhasm:   rx0 += addt0
# asm 1: add  <addt0=int64#2,<rx0=int64#11
# asm 2: add  <addt0=%rsi,<rx0=%r13
add  %rsi,%r13

# qhasm:   carry? rx0 += e0_stack 
# asm 1: addq <e0_stack=stack64#20,<rx0=int64#11
# asm 2: addq <e0_stack=152(%rsp),<rx0=%r13
addq 152(%rsp),%r13

# qhasm:   carry? rx1 += e1_stack + carry
# asm 1: adcq <e1_stack=stack64#21,<rx1=int64#4
# asm 2: adcq <e1_stack=160(%rsp),<rx1=%rcx
adcq 160(%rsp),%rcx

# qhasm:   carry? rx2 += e2_stack + carry
# asm 1: adcq <e2_stack=stack64#22,<rx2=int64#5
# asm 2: adcq <e2_stack=168(%rsp),<rx2=%r8
adcq 168(%rsp),%r8

# qhasm:   carry? rx3 += e3_stack + carry
# asm 1: adcq <e3_stack=stack64#23,<rx3=int64#6
# asm 2: adcq <e3_stack=176(%rsp),<rx3=%r9
adcq 176(%rsp),%r9

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#2
# asm 2: mov  $0,>addt0=%rsi
mov  $0,%rsi

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#3
# asm 2: mov  $38,>addt1=%rdx
mov  $38,%rdx

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#2,<addt1=int64#3
# asm 2: cmovae <addt0=%rsi,<addt1=%rdx
cmovae %rsi,%rdx

# qhasm:   carry? rx0 += addt1
# asm 1: add  <addt1=int64#3,<rx0=int64#11
# asm 2: add  <addt1=%rdx,<rx0=%r13
add  %rdx,%r13

# qhasm:   carry? rx1 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx1=int64#4
# asm 2: adc <addt0=%rsi,<rx1=%rcx
adc %rsi,%rcx

# qhasm:   carry? rx2 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx2=int64#5
# asm 2: adc <addt0=%rsi,<rx2=%r8
adc %rsi,%r8

# qhasm:   carry? rx3 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rx3=int64#6
# asm 2: adc <addt0=%rsi,<rx3=%r9
adc %rsi,%r9

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#3,<addt0=int64#2
# asm 2: cmovc <addt1=%rdx,<addt0=%rsi
cmovc %rdx,%rsi

# qhasm:   rx0 += addt0
# asm 1: add  <addt0=int64#2,<rx0=int64#11
# asm 2: add  <addt0=%rsi,<rx0=%r13
add  %rsi,%r13

# qhasm: *(uint64 *)(rp + 0) = rx0
# asm 1: movq   <rx0=int64#11,0(<rp=int64#1)
# asm 2: movq   <rx0=%r13,0(<rp=%rdi)
movq   %r13,0(%rdi)

# qhasm: *(uint64 *)(rp + 8) = rx1
# asm 1: movq   <rx1=int64#4,8(<rp=int64#1)
# asm 2: movq   <rx1=%rcx,8(<rp=%rdi)
movq   %rcx,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = rx2
# asm 1: movq   <rx2=int64#5,16(<rp=int64#1)
# asm 2: movq   <rx2=%r8,16(<rp=%rdi)
movq   %r8,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = rx3
# asm 1: movq   <rx3=int64#6,24(<rp=int64#1)
# asm 2: movq   <rx3=%r9,24(<rp=%rdi)
movq   %r9,24(%rdi)

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
