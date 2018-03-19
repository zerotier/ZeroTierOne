
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

# qhasm: enter crypto_sign_ed25519_amd64_64_fe25519_mul
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_fe25519_mul
.globl crypto_sign_ed25519_amd64_64_fe25519_mul
_crypto_sign_ed25519_amd64_64_fe25519_mul:
crypto_sign_ed25519_amd64_64_fe25519_mul:
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

# qhasm:   mulr4 = 0
# asm 1: mov  $0,>mulr4=int64#5
# asm 2: mov  $0,>mulr4=%r8
mov  $0,%r8

# qhasm:   mulr5 = 0
# asm 1: mov  $0,>mulr5=int64#6
# asm 2: mov  $0,>mulr5=%r9
mov  $0,%r9

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#8
# asm 2: mov  $0,>mulr6=%r10
mov  $0,%r10

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#9
# asm 2: mov  $0,>mulr7=%r11
mov  $0,%r11

# qhasm:   mulx0 = *(uint64 *)(xp + 0)
# asm 1: movq   0(<xp=int64#2),>mulx0=int64#10
# asm 2: movq   0(<xp=%rsi),>mulx0=%r12
movq   0(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(yp + 0)
# asm 1: movq   0(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   0(<yp=%rcx),>mulrax=%rax
movq   0(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   r0 = mulrax
# asm 1: mov  <mulrax=int64#7,>r0=int64#11
# asm 2: mov  <mulrax=%rax,>r0=%r13
mov  %rax,%r13

# qhasm:   r1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>r1=int64#12
# asm 2: mov  <mulrdx=%rdx,>r1=%r14
mov  %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(yp + 8)
# asm 1: movq   8(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   8(<yp=%rcx),>mulrax=%rax
movq   8(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? r1 += mulrax
# asm 1: add  <mulrax=int64#7,<r1=int64#12
# asm 2: add  <mulrax=%rax,<r1=%r14
add  %rax,%r14

# qhasm:   r2 = 0
# asm 1: mov  $0,>r2=int64#13
# asm 2: mov  $0,>r2=%r15
mov  $0,%r15

# qhasm:   r2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<r2=int64#13
# asm 2: adc <mulrdx=%rdx,<r2=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(yp + 16)
# asm 1: movq   16(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   16(<yp=%rcx),>mulrax=%rax
movq   16(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? r2 += mulrax
# asm 1: add  <mulrax=int64#7,<r2=int64#13
# asm 2: add  <mulrax=%rax,<r2=%r15
add  %rax,%r15

# qhasm:   r3 = 0
# asm 1: mov  $0,>r3=int64#14
# asm 2: mov  $0,>r3=%rbx
mov  $0,%rbx

# qhasm:   r3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<r3=int64#14
# asm 2: adc <mulrdx=%rdx,<r3=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(yp + 24)
# asm 1: movq   24(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   24(<yp=%rcx),>mulrax=%rax
movq   24(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? r3 += mulrax
# asm 1: add  <mulrax=int64#7,<r3=int64#14
# asm 2: add  <mulrax=%rax,<r3=%rbx
add  %rax,%rbx

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr4=%r8
adc %rdx,%r8

# qhasm:   mulx1 = *(uint64 *)(xp + 8)
# asm 1: movq   8(<xp=int64#2),>mulx1=int64#10
# asm 2: movq   8(<xp=%rsi),>mulx1=%r12
movq   8(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(yp + 0)
# asm 1: movq   0(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   0(<yp=%rcx),>mulrax=%rax
movq   0(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? r1 += mulrax
# asm 1: add  <mulrax=int64#7,<r1=int64#12
# asm 2: add  <mulrax=%rax,<r1=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(yp + 8)
# asm 1: movq   8(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   8(<yp=%rcx),>mulrax=%rax
movq   8(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? r2 += mulrax
# asm 1: add  <mulrax=int64#7,<r2=int64#13
# asm 2: add  <mulrax=%rax,<r2=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? r2 += mulc
# asm 1: add  <mulc=int64#15,<r2=int64#13
# asm 2: add  <mulc=%rbp,<r2=%r15
add  %rbp,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(yp + 16)
# asm 1: movq   16(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   16(<yp=%rcx),>mulrax=%rax
movq   16(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? r3 += mulrax 
# asm 1: add  <mulrax=int64#7,<r3=int64#14
# asm 2: add  <mulrax=%rax,<r3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? r3 += mulc
# asm 1: add  <mulc=int64#15,<r3=int64#14
# asm 2: add  <mulc=%rbp,<r3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(yp + 24)
# asm 1: movq   24(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   24(<yp=%rcx),>mulrax=%rax
movq   24(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#5
# asm 2: add  <mulrax=%rax,<mulr4=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#15,<mulr4=int64#5
# asm 2: add  <mulc=%rbp,<mulr4=%r8
add  %rbp,%r8

# qhasm:   mulr5 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr5=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr5=%r9
adc %rdx,%r9

# qhasm:   mulx2 = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>mulx2=int64#10
# asm 2: movq   16(<xp=%rsi),>mulx2=%r12
movq   16(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(yp + 0)
# asm 1: movq   0(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   0(<yp=%rcx),>mulrax=%rax
movq   0(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? r2 += mulrax
# asm 1: add  <mulrax=int64#7,<r2=int64#13
# asm 2: add  <mulrax=%rax,<r2=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(yp + 8)
# asm 1: movq   8(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   8(<yp=%rcx),>mulrax=%rax
movq   8(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? r3 += mulrax
# asm 1: add  <mulrax=int64#7,<r3=int64#14
# asm 2: add  <mulrax=%rax,<r3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? r3 += mulc
# asm 1: add  <mulc=int64#15,<r3=int64#14
# asm 2: add  <mulc=%rbp,<r3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(yp + 16)
# asm 1: movq   16(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   16(<yp=%rcx),>mulrax=%rax
movq   16(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#5
# asm 2: add  <mulrax=%rax,<mulr4=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#15,<mulr4=int64#5
# asm 2: add  <mulc=%rbp,<mulr4=%r8
add  %rbp,%r8

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(yp + 24)
# asm 1: movq   24(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   24(<yp=%rcx),>mulrax=%rax
movq   24(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#6
# asm 2: add  <mulrax=%rax,<mulr5=%r9
add  %rax,%r9

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#15,<mulr5=int64#6
# asm 2: add  <mulc=%rbp,<mulr5=%r9
add  %rbp,%r9

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#8
# asm 2: adc <mulrdx=%rdx,<mulr6=%r10
adc %rdx,%r10

# qhasm:   mulx3 = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>mulx3=int64#2
# asm 2: movq   24(<xp=%rsi),>mulx3=%rsi
movq   24(%rsi),%rsi

# qhasm:   mulrax = *(uint64 *)(yp + 0)
# asm 1: movq   0(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   0(<yp=%rcx),>mulrax=%rax
movq   0(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#2
# asm 2: mul  <mulx3=%rsi
mul  %rsi

# qhasm:   carry? r3 += mulrax
# asm 1: add  <mulrax=int64#7,<r3=int64#14
# asm 2: add  <mulrax=%rax,<r3=%rbx
add  %rax,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#10
# asm 2: mov  $0,>mulc=%r12
mov  $0,%r12

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#10
# asm 2: adc <mulrdx=%rdx,<mulc=%r12
adc %rdx,%r12

# qhasm:   mulrax = *(uint64 *)(yp + 8)
# asm 1: movq   8(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   8(<yp=%rcx),>mulrax=%rax
movq   8(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#2
# asm 2: mul  <mulx3=%rsi
mul  %rsi

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#5
# asm 2: add  <mulrax=%rax,<mulr4=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#10,<mulr4=int64#5
# asm 2: add  <mulc=%r12,<mulr4=%r8
add  %r12,%r8

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#10
# asm 2: mov  $0,>mulc=%r12
mov  $0,%r12

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#10
# asm 2: adc <mulrdx=%rdx,<mulc=%r12
adc %rdx,%r12

# qhasm:   mulrax = *(uint64 *)(yp + 16)
# asm 1: movq   16(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   16(<yp=%rcx),>mulrax=%rax
movq   16(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#2
# asm 2: mul  <mulx3=%rsi
mul  %rsi

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#6
# asm 2: add  <mulrax=%rax,<mulr5=%r9
add  %rax,%r9

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#10,<mulr5=int64#6
# asm 2: add  <mulc=%r12,<mulr5=%r9
add  %r12,%r9

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#10
# asm 2: mov  $0,>mulc=%r12
mov  $0,%r12

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#10
# asm 2: adc <mulrdx=%rdx,<mulc=%r12
adc %rdx,%r12

# qhasm:   mulrax = *(uint64 *)(yp + 24)
# asm 1: movq   24(<yp=int64#4),>mulrax=int64#7
# asm 2: movq   24(<yp=%rcx),>mulrax=%rax
movq   24(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#2
# asm 2: mul  <mulx3=%rsi
mul  %rsi

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#8
# asm 2: add  <mulrax=%rax,<mulr6=%r10
add  %rax,%r10

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#10,<mulr6=int64#8
# asm 2: add  <mulc=%r12,<mulr6=%r10
add  %r12,%r10

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#9
# asm 2: adc <mulrdx=%rdx,<mulr7=%r11
adc %rdx,%r11

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr4=%r8,>mulrax=%rax
mov  %r8,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#2
# asm 2: mov  <mulrax=%rax,>mulr4=%rsi
mov  %rax,%rsi

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr5=%r9,>mulrax=%rax
mov  %r9,%rax

# qhasm:   mulr5 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>mulr5=int64#4
# asm 2: mov  <mulrdx=%rdx,>mulr5=%rcx
mov  %rdx,%rcx

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrax = mulr6
# asm 1: mov  <mulr6=int64#8,>mulrax=int64#7
# asm 2: mov  <mulr6=%r10,>mulrax=%rax
mov  %r10,%rax

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#5
# asm 2: mov  $0,>mulr6=%r8
mov  $0,%r8

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr6=%r8
adc %rdx,%r8

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#5
# asm 2: add  <mulrax=%rax,<mulr6=%r8
add  %rax,%r8

# qhasm:   mulrax = mulr7
# asm 1: mov  <mulr7=int64#9,>mulrax=int64#7
# asm 2: mov  <mulr7=%r11,>mulrax=%rax
mov  %r11,%rax

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#6
# asm 2: mov  $0,>mulr7=%r9
mov  $0,%r9

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr7=%r9
adc %rdx,%r9

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38

# qhasm:   carry? mulr7 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr7=int64#6
# asm 2: add  <mulrax=%rax,<mulr7=%r9
add  %rax,%r9

# qhasm:   mulr8 = 0
# asm 1: mov  $0,>mulr8=int64#7
# asm 2: mov  $0,>mulr8=%rax
mov  $0,%rax

# qhasm:   mulr8 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr8=int64#7
# asm 2: adc <mulrdx=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   carry? r0 += mulr4
# asm 1: add  <mulr4=int64#2,<r0=int64#11
# asm 2: add  <mulr4=%rsi,<r0=%r13
add  %rsi,%r13

# qhasm:   carry? r1 += mulr5 + carry
# asm 1: adc <mulr5=int64#4,<r1=int64#12
# asm 2: adc <mulr5=%rcx,<r1=%r14
adc %rcx,%r14

# qhasm:   carry? r2 += mulr6 + carry
# asm 1: adc <mulr6=int64#5,<r2=int64#13
# asm 2: adc <mulr6=%r8,<r2=%r15
adc %r8,%r15

# qhasm:   carry? r3 += mulr7 + carry
# asm 1: adc <mulr7=int64#6,<r3=int64#14
# asm 2: adc <mulr7=%r9,<r3=%rbx
adc %r9,%rbx

# qhasm:   mulzero = 0
# asm 1: mov  $0,>mulzero=int64#2
# asm 2: mov  $0,>mulzero=%rsi
mov  $0,%rsi

# qhasm:   mulr8 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulr8=int64#7
# asm 2: adc <mulzero=%rsi,<mulr8=%rax
adc %rsi,%rax

# qhasm:   mulr8 *= 38
# asm 1: imulq  $38,<mulr8=int64#7,>mulr8=int64#3
# asm 2: imulq  $38,<mulr8=%rax,>mulr8=%rdx
imulq  $38,%rax,%rdx

# qhasm:   carry? r0 += mulr8
# asm 1: add  <mulr8=int64#3,<r0=int64#11
# asm 2: add  <mulr8=%rdx,<r0=%r13
add  %rdx,%r13

# qhasm:   carry? r1 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<r1=int64#12
# asm 2: adc <mulzero=%rsi,<r1=%r14
adc %rsi,%r14

# qhasm:   carry? r2 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<r2=int64#13
# asm 2: adc <mulzero=%rsi,<r2=%r15
adc %rsi,%r15

# qhasm:   carry? r3 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<r3=int64#14
# asm 2: adc <mulzero=%rsi,<r3=%rbx
adc %rsi,%rbx

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulzero=int64#2
# asm 2: adc <mulzero=%rsi,<mulzero=%rsi
adc %rsi,%rsi

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#2,>mulzero=int64#2
# asm 2: imulq  $38,<mulzero=%rsi,>mulzero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   r0 += mulzero
# asm 1: add  <mulzero=int64#2,<r0=int64#11
# asm 2: add  <mulzero=%rsi,<r0=%r13
add  %rsi,%r13

# qhasm: *(uint64 *)(rp + 8) = r1
# asm 1: movq   <r1=int64#12,8(<rp=int64#1)
# asm 2: movq   <r1=%r14,8(<rp=%rdi)
movq   %r14,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = r2
# asm 1: movq   <r2=int64#13,16(<rp=int64#1)
# asm 2: movq   <r2=%r15,16(<rp=%rdi)
movq   %r15,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = r3
# asm 1: movq   <r3=int64#14,24(<rp=int64#1)
# asm 2: movq   <r3=%rbx,24(<rp=%rdi)
movq   %rbx,24(%rdi)

# qhasm: *(uint64 *)(rp + 0) = r0
# asm 1: movq   <r0=int64#11,0(<rp=int64#1)
# asm 2: movq   <r0=%r13,0(<rp=%rdi)
movq   %r13,0(%rdi)

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
