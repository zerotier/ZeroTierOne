
# qhasm: int64 rp

# qhasm: int64 pp

# qhasm: input rp

# qhasm: input pp

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

# qhasm: int64 rx0

# qhasm: int64 rx1

# qhasm: int64 rx2

# qhasm: int64 rx3

# qhasm: int64 ry0

# qhasm: int64 ry1

# qhasm: int64 ry2

# qhasm: int64 ry3

# qhasm: int64 rz0

# qhasm: int64 rz1

# qhasm: int64 rz2

# qhasm: int64 rz3

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

# qhasm: enter crypto_sign_ed25519_amd64_64_ge25519_p1p1_to_p2
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_ge25519_p1p1_to_p2
.globl crypto_sign_ed25519_amd64_64_ge25519_p1p1_to_p2
_crypto_sign_ed25519_amd64_64_ge25519_p1p1_to_p2:
crypto_sign_ed25519_amd64_64_ge25519_p1p1_to_p2:
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

# qhasm:   mulr4 = 0
# asm 1: mov  $0,>mulr4=int64#4
# asm 2: mov  $0,>mulr4=%rcx
mov  $0,%rcx

# qhasm:   mulr5 = 0
# asm 1: mov  $0,>mulr5=int64#5
# asm 2: mov  $0,>mulr5=%r8
mov  $0,%r8

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#6
# asm 2: mov  $0,>mulr6=%r9
mov  $0,%r9

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#8
# asm 2: mov  $0,>mulr7=%r10
mov  $0,%r10

# qhasm:   mulx0 = *(uint64 *)(pp + 0)
# asm 1: movq   0(<pp=int64#2),>mulx0=int64#9
# asm 2: movq   0(<pp=%rsi),>mulx0=%r11
movq   0(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   96(<pp=%rsi),>mulrax=%rax
movq   96(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   rx0 = mulrax
# asm 1: mov  <mulrax=int64#7,>rx0=int64#10
# asm 2: mov  <mulrax=%rax,>rx0=%r12
mov  %rax,%r12

# qhasm:   rx1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>rx1=int64#11
# asm 2: mov  <mulrdx=%rdx,>rx1=%r13
mov  %rdx,%r13

# qhasm:   mulrax = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   104(<pp=%rsi),>mulrax=%rax
movq   104(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? rx1 += mulrax
# asm 1: add  <mulrax=int64#7,<rx1=int64#11
# asm 2: add  <mulrax=%rax,<rx1=%r13
add  %rax,%r13

# qhasm:   rx2 = 0
# asm 1: mov  $0,>rx2=int64#12
# asm 2: mov  $0,>rx2=%r14
mov  $0,%r14

# qhasm:   rx2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rx2=int64#12
# asm 2: adc <mulrdx=%rdx,<rx2=%r14
adc %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   112(<pp=%rsi),>mulrax=%rax
movq   112(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#12
# asm 2: add  <mulrax=%rax,<rx2=%r14
add  %rax,%r14

# qhasm:   rx3 = 0
# asm 1: mov  $0,>rx3=int64#13
# asm 2: mov  $0,>rx3=%r15
mov  $0,%r15

# qhasm:   rx3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rx3=int64#13
# asm 2: adc <mulrdx=%rdx,<rx3=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   120(<pp=%rsi),>mulrax=%rax
movq   120(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#13
# asm 2: add  <mulrax=%rax,<rx3=%r15
add  %rax,%r15

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr4=%rcx
adc %rdx,%rcx

# qhasm:   mulx1 = *(uint64 *)(pp + 8)
# asm 1: movq   8(<pp=int64#2),>mulx1=int64#9
# asm 2: movq   8(<pp=%rsi),>mulx1=%r11
movq   8(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   96(<pp=%rsi),>mulrax=%rax
movq   96(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? rx1 += mulrax
# asm 1: add  <mulrax=int64#7,<rx1=int64#11
# asm 2: add  <mulrax=%rax,<rx1=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   104(<pp=%rsi),>mulrax=%rax
movq   104(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#12
# asm 2: add  <mulrax=%rax,<rx2=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx2 += mulc
# asm 1: add  <mulc=int64#14,<rx2=int64#12
# asm 2: add  <mulc=%rbx,<rx2=%r14
add  %rbx,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   112(<pp=%rsi),>mulrax=%rax
movq   112(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? rx3 += mulrax 
# asm 1: add  <mulrax=int64#7,<rx3=int64#13
# asm 2: add  <mulrax=%rax,<rx3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx3 += mulc
# asm 1: add  <mulc=int64#14,<rx3=int64#13
# asm 2: add  <mulc=%rbx,<rx3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   120(<pp=%rsi),>mulrax=%rax
movq   120(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulr5 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr5=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr5=%r8
adc %rdx,%r8

# qhasm:   mulx2 = *(uint64 *)(pp + 16)
# asm 1: movq   16(<pp=int64#2),>mulx2=int64#9
# asm 2: movq   16(<pp=%rsi),>mulx2=%r11
movq   16(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   96(<pp=%rsi),>mulrax=%rax
movq   96(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#12
# asm 2: add  <mulrax=%rax,<rx2=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   104(<pp=%rsi),>mulrax=%rax
movq   104(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#13
# asm 2: add  <mulrax=%rax,<rx3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx3 += mulc
# asm 1: add  <mulc=int64#14,<rx3=int64#13
# asm 2: add  <mulc=%rbx,<rx3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   112(<pp=%rsi),>mulrax=%rax
movq   112(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   120(<pp=%rsi),>mulrax=%rax
movq   120(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#5
# asm 2: add  <mulrax=%rax,<mulr5=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#14,<mulr5=int64#5
# asm 2: add  <mulc=%rbx,<mulr5=%r8
add  %rbx,%r8

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr6=%r9
adc %rdx,%r9

# qhasm:   mulx3 = *(uint64 *)(pp + 24)
# asm 1: movq   24(<pp=int64#2),>mulx3=int64#9
# asm 2: movq   24(<pp=%rsi),>mulx3=%r11
movq   24(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   96(<pp=%rsi),>mulrax=%rax
movq   96(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#13
# asm 2: add  <mulrax=%rax,<rx3=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   104(<pp=%rsi),>mulrax=%rax
movq   104(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   112(<pp=%rsi),>mulrax=%rax
movq   112(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#5
# asm 2: add  <mulrax=%rax,<mulr5=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#14,<mulr5=int64#5
# asm 2: add  <mulc=%rbx,<mulr5=%r8
add  %rbx,%r8

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   120(<pp=%rsi),>mulrax=%rax
movq   120(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#6
# asm 2: add  <mulrax=%rax,<mulr6=%r9
add  %rax,%r9

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#14,<mulr6=int64#6
# asm 2: add  <mulc=%rbx,<mulr6=%r9
add  %rbx,%r9

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#8
# asm 2: adc <mulrdx=%rdx,<mulr7=%r10
adc %rdx,%r10

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#4,>mulrax=int64#7
# asm 2: mov  <mulr4=%rcx,>mulrax=%rax
mov  %rcx,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#4
# asm 2: mov  <mulrax=%rax,>mulr4=%rcx
mov  %rax,%rcx

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr5=%r8,>mulrax=%rax
mov  %r8,%rax

# qhasm:   mulr5 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>mulr5=int64#5
# asm 2: mov  <mulrdx=%rdx,>mulr5=%r8
mov  %rdx,%r8

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#5
# asm 2: add  <mulrax=%rax,<mulr5=%r8
add  %rax,%r8

# qhasm:   mulrax = mulr6
# asm 1: mov  <mulr6=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr6=%r9,>mulrax=%rax
mov  %r9,%rax

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#6
# asm 2: mov  $0,>mulr6=%r9
mov  $0,%r9

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr6=%r9
adc %rdx,%r9

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#6
# asm 2: add  <mulrax=%rax,<mulr6=%r9
add  %rax,%r9

# qhasm:   mulrax = mulr7
# asm 1: mov  <mulr7=int64#8,>mulrax=int64#7
# asm 2: mov  <mulr7=%r10,>mulrax=%rax
mov  %r10,%rax

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#8
# asm 2: mov  $0,>mulr7=%r10
mov  $0,%r10

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#8
# asm 2: adc <mulrdx=%rdx,<mulr7=%r10
adc %rdx,%r10

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr7 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr7=int64#8
# asm 2: add  <mulrax=%rax,<mulr7=%r10
add  %rax,%r10

# qhasm:   mulr8 = 0
# asm 1: mov  $0,>mulr8=int64#7
# asm 2: mov  $0,>mulr8=%rax
mov  $0,%rax

# qhasm:   mulr8 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr8=int64#7
# asm 2: adc <mulrdx=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   carry? rx0 += mulr4
# asm 1: add  <mulr4=int64#4,<rx0=int64#10
# asm 2: add  <mulr4=%rcx,<rx0=%r12
add  %rcx,%r12

# qhasm:   carry? rx1 += mulr5 + carry
# asm 1: adc <mulr5=int64#5,<rx1=int64#11
# asm 2: adc <mulr5=%r8,<rx1=%r13
adc %r8,%r13

# qhasm:   carry? rx2 += mulr6 + carry
# asm 1: adc <mulr6=int64#6,<rx2=int64#12
# asm 2: adc <mulr6=%r9,<rx2=%r14
adc %r9,%r14

# qhasm:   carry? rx3 += mulr7 + carry
# asm 1: adc <mulr7=int64#8,<rx3=int64#13
# asm 2: adc <mulr7=%r10,<rx3=%r15
adc %r10,%r15

# qhasm:   mulzero = 0
# asm 1: mov  $0,>mulzero=int64#3
# asm 2: mov  $0,>mulzero=%rdx
mov  $0,%rdx

# qhasm:   mulr8 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulr8=int64#7
# asm 2: adc <mulzero=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   mulr8 *= 38
# asm 1: imulq  $38,<mulr8=int64#7,>mulr8=int64#4
# asm 2: imulq  $38,<mulr8=%rax,>mulr8=%rcx
imulq  $38,%rax,%rcx

# qhasm:   carry? rx0 += mulr8
# asm 1: add  <mulr8=int64#4,<rx0=int64#10
# asm 2: add  <mulr8=%rcx,<rx0=%r12
add  %rcx,%r12

# qhasm:   carry? rx1 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<rx1=int64#11
# asm 2: adc <mulzero=%rdx,<rx1=%r13
adc %rdx,%r13

# qhasm:   carry? rx2 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<rx2=int64#12
# asm 2: adc <mulzero=%rdx,<rx2=%r14
adc %rdx,%r14

# qhasm:   carry? rx3 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<rx3=int64#13
# asm 2: adc <mulzero=%rdx,<rx3=%r15
adc %rdx,%r15

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulzero=int64#3
# asm 2: adc <mulzero=%rdx,<mulzero=%rdx
adc %rdx,%rdx

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#3,>mulzero=int64#3
# asm 2: imulq  $38,<mulzero=%rdx,>mulzero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   rx0 += mulzero
# asm 1: add  <mulzero=int64#3,<rx0=int64#10
# asm 2: add  <mulzero=%rdx,<rx0=%r12
add  %rdx,%r12

# qhasm: *(uint64 *)(rp + 0) = rx0
# asm 1: movq   <rx0=int64#10,0(<rp=int64#1)
# asm 2: movq   <rx0=%r12,0(<rp=%rdi)
movq   %r12,0(%rdi)

# qhasm: *(uint64 *)(rp + 8) = rx1
# asm 1: movq   <rx1=int64#11,8(<rp=int64#1)
# asm 2: movq   <rx1=%r13,8(<rp=%rdi)
movq   %r13,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = rx2
# asm 1: movq   <rx2=int64#12,16(<rp=int64#1)
# asm 2: movq   <rx2=%r14,16(<rp=%rdi)
movq   %r14,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = rx3
# asm 1: movq   <rx3=int64#13,24(<rp=int64#1)
# asm 2: movq   <rx3=%r15,24(<rp=%rdi)
movq   %r15,24(%rdi)

# qhasm:   mulr4 = 0
# asm 1: mov  $0,>mulr4=int64#4
# asm 2: mov  $0,>mulr4=%rcx
mov  $0,%rcx

# qhasm:   mulr5 = 0
# asm 1: mov  $0,>mulr5=int64#5
# asm 2: mov  $0,>mulr5=%r8
mov  $0,%r8

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#6
# asm 2: mov  $0,>mulr6=%r9
mov  $0,%r9

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#8
# asm 2: mov  $0,>mulr7=%r10
mov  $0,%r10

# qhasm:   mulx0 = *(uint64 *)(pp + 64)
# asm 1: movq   64(<pp=int64#2),>mulx0=int64#9
# asm 2: movq   64(<pp=%rsi),>mulx0=%r11
movq   64(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 32)
# asm 1: movq   32(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   32(<pp=%rsi),>mulrax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   ry0 = mulrax
# asm 1: mov  <mulrax=int64#7,>ry0=int64#10
# asm 2: mov  <mulrax=%rax,>ry0=%r12
mov  %rax,%r12

# qhasm:   ry1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>ry1=int64#11
# asm 2: mov  <mulrdx=%rdx,>ry1=%r13
mov  %rdx,%r13

# qhasm:   mulrax = *(uint64 *)(pp + 40)
# asm 1: movq   40(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   40(<pp=%rsi),>mulrax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? ry1 += mulrax
# asm 1: add  <mulrax=int64#7,<ry1=int64#11
# asm 2: add  <mulrax=%rax,<ry1=%r13
add  %rax,%r13

# qhasm:   ry2 = 0
# asm 1: mov  $0,>ry2=int64#12
# asm 2: mov  $0,>ry2=%r14
mov  $0,%r14

# qhasm:   ry2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<ry2=int64#12
# asm 2: adc <mulrdx=%rdx,<ry2=%r14
adc %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   48(<pp=%rsi),>mulrax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? ry2 += mulrax
# asm 1: add  <mulrax=int64#7,<ry2=int64#12
# asm 2: add  <mulrax=%rax,<ry2=%r14
add  %rax,%r14

# qhasm:   ry3 = 0
# asm 1: mov  $0,>ry3=int64#13
# asm 2: mov  $0,>ry3=%r15
mov  $0,%r15

# qhasm:   ry3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<ry3=int64#13
# asm 2: adc <mulrdx=%rdx,<ry3=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   56(<pp=%rsi),>mulrax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? ry3 += mulrax
# asm 1: add  <mulrax=int64#7,<ry3=int64#13
# asm 2: add  <mulrax=%rax,<ry3=%r15
add  %rax,%r15

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr4=%rcx
adc %rdx,%rcx

# qhasm:   mulx1 = *(uint64 *)(pp + 72)
# asm 1: movq   72(<pp=int64#2),>mulx1=int64#9
# asm 2: movq   72(<pp=%rsi),>mulx1=%r11
movq   72(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 32)
# asm 1: movq   32(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   32(<pp=%rsi),>mulrax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? ry1 += mulrax
# asm 1: add  <mulrax=int64#7,<ry1=int64#11
# asm 2: add  <mulrax=%rax,<ry1=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 40)
# asm 1: movq   40(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   40(<pp=%rsi),>mulrax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? ry2 += mulrax
# asm 1: add  <mulrax=int64#7,<ry2=int64#12
# asm 2: add  <mulrax=%rax,<ry2=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? ry2 += mulc
# asm 1: add  <mulc=int64#14,<ry2=int64#12
# asm 2: add  <mulc=%rbx,<ry2=%r14
add  %rbx,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   48(<pp=%rsi),>mulrax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? ry3 += mulrax 
# asm 1: add  <mulrax=int64#7,<ry3=int64#13
# asm 2: add  <mulrax=%rax,<ry3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? ry3 += mulc
# asm 1: add  <mulc=int64#14,<ry3=int64#13
# asm 2: add  <mulc=%rbx,<ry3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   56(<pp=%rsi),>mulrax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulr5 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr5=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr5=%r8
adc %rdx,%r8

# qhasm:   mulx2 = *(uint64 *)(pp + 80)
# asm 1: movq   80(<pp=int64#2),>mulx2=int64#9
# asm 2: movq   80(<pp=%rsi),>mulx2=%r11
movq   80(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 32)
# asm 1: movq   32(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   32(<pp=%rsi),>mulrax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? ry2 += mulrax
# asm 1: add  <mulrax=int64#7,<ry2=int64#12
# asm 2: add  <mulrax=%rax,<ry2=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 40)
# asm 1: movq   40(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   40(<pp=%rsi),>mulrax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? ry3 += mulrax
# asm 1: add  <mulrax=int64#7,<ry3=int64#13
# asm 2: add  <mulrax=%rax,<ry3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? ry3 += mulc
# asm 1: add  <mulc=int64#14,<ry3=int64#13
# asm 2: add  <mulc=%rbx,<ry3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   48(<pp=%rsi),>mulrax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   56(<pp=%rsi),>mulrax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#5
# asm 2: add  <mulrax=%rax,<mulr5=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#14,<mulr5=int64#5
# asm 2: add  <mulc=%rbx,<mulr5=%r8
add  %rbx,%r8

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr6=%r9
adc %rdx,%r9

# qhasm:   mulx3 = *(uint64 *)(pp + 88)
# asm 1: movq   88(<pp=int64#2),>mulx3=int64#9
# asm 2: movq   88(<pp=%rsi),>mulx3=%r11
movq   88(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 32)
# asm 1: movq   32(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   32(<pp=%rsi),>mulrax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? ry3 += mulrax
# asm 1: add  <mulrax=int64#7,<ry3=int64#13
# asm 2: add  <mulrax=%rax,<ry3=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 40)
# asm 1: movq   40(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   40(<pp=%rsi),>mulrax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   48(<pp=%rsi),>mulrax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#5
# asm 2: add  <mulrax=%rax,<mulr5=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#14,<mulr5=int64#5
# asm 2: add  <mulc=%rbx,<mulr5=%r8
add  %rbx,%r8

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   56(<pp=%rsi),>mulrax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#6
# asm 2: add  <mulrax=%rax,<mulr6=%r9
add  %rax,%r9

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#14,<mulr6=int64#6
# asm 2: add  <mulc=%rbx,<mulr6=%r9
add  %rbx,%r9

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#8
# asm 2: adc <mulrdx=%rdx,<mulr7=%r10
adc %rdx,%r10

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#4,>mulrax=int64#7
# asm 2: mov  <mulr4=%rcx,>mulrax=%rax
mov  %rcx,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#4
# asm 2: mov  <mulrax=%rax,>mulr4=%rcx
mov  %rax,%rcx

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr5=%r8,>mulrax=%rax
mov  %r8,%rax

# qhasm:   mulr5 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>mulr5=int64#5
# asm 2: mov  <mulrdx=%rdx,>mulr5=%r8
mov  %rdx,%r8

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#5
# asm 2: add  <mulrax=%rax,<mulr5=%r8
add  %rax,%r8

# qhasm:   mulrax = mulr6
# asm 1: mov  <mulr6=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr6=%r9,>mulrax=%rax
mov  %r9,%rax

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#6
# asm 2: mov  $0,>mulr6=%r9
mov  $0,%r9

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr6=%r9
adc %rdx,%r9

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#6
# asm 2: add  <mulrax=%rax,<mulr6=%r9
add  %rax,%r9

# qhasm:   mulrax = mulr7
# asm 1: mov  <mulr7=int64#8,>mulrax=int64#7
# asm 2: mov  <mulr7=%r10,>mulrax=%rax
mov  %r10,%rax

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#8
# asm 2: mov  $0,>mulr7=%r10
mov  $0,%r10

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#8
# asm 2: adc <mulrdx=%rdx,<mulr7=%r10
adc %rdx,%r10

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr7 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr7=int64#8
# asm 2: add  <mulrax=%rax,<mulr7=%r10
add  %rax,%r10

# qhasm:   mulr8 = 0
# asm 1: mov  $0,>mulr8=int64#7
# asm 2: mov  $0,>mulr8=%rax
mov  $0,%rax

# qhasm:   mulr8 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr8=int64#7
# asm 2: adc <mulrdx=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   carry? ry0 += mulr4
# asm 1: add  <mulr4=int64#4,<ry0=int64#10
# asm 2: add  <mulr4=%rcx,<ry0=%r12
add  %rcx,%r12

# qhasm:   carry? ry1 += mulr5 + carry
# asm 1: adc <mulr5=int64#5,<ry1=int64#11
# asm 2: adc <mulr5=%r8,<ry1=%r13
adc %r8,%r13

# qhasm:   carry? ry2 += mulr6 + carry
# asm 1: adc <mulr6=int64#6,<ry2=int64#12
# asm 2: adc <mulr6=%r9,<ry2=%r14
adc %r9,%r14

# qhasm:   carry? ry3 += mulr7 + carry
# asm 1: adc <mulr7=int64#8,<ry3=int64#13
# asm 2: adc <mulr7=%r10,<ry3=%r15
adc %r10,%r15

# qhasm:   mulzero = 0
# asm 1: mov  $0,>mulzero=int64#3
# asm 2: mov  $0,>mulzero=%rdx
mov  $0,%rdx

# qhasm:   mulr8 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulr8=int64#7
# asm 2: adc <mulzero=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   mulr8 *= 38
# asm 1: imulq  $38,<mulr8=int64#7,>mulr8=int64#4
# asm 2: imulq  $38,<mulr8=%rax,>mulr8=%rcx
imulq  $38,%rax,%rcx

# qhasm:   carry? ry0 += mulr8
# asm 1: add  <mulr8=int64#4,<ry0=int64#10
# asm 2: add  <mulr8=%rcx,<ry0=%r12
add  %rcx,%r12

# qhasm:   carry? ry1 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<ry1=int64#11
# asm 2: adc <mulzero=%rdx,<ry1=%r13
adc %rdx,%r13

# qhasm:   carry? ry2 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<ry2=int64#12
# asm 2: adc <mulzero=%rdx,<ry2=%r14
adc %rdx,%r14

# qhasm:   carry? ry3 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<ry3=int64#13
# asm 2: adc <mulzero=%rdx,<ry3=%r15
adc %rdx,%r15

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulzero=int64#3
# asm 2: adc <mulzero=%rdx,<mulzero=%rdx
adc %rdx,%rdx

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#3,>mulzero=int64#3
# asm 2: imulq  $38,<mulzero=%rdx,>mulzero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   ry0 += mulzero
# asm 1: add  <mulzero=int64#3,<ry0=int64#10
# asm 2: add  <mulzero=%rdx,<ry0=%r12
add  %rdx,%r12

# qhasm: *(uint64 *)(rp + 32) = ry0
# asm 1: movq   <ry0=int64#10,32(<rp=int64#1)
# asm 2: movq   <ry0=%r12,32(<rp=%rdi)
movq   %r12,32(%rdi)

# qhasm: *(uint64 *)(rp + 40) = ry1
# asm 1: movq   <ry1=int64#11,40(<rp=int64#1)
# asm 2: movq   <ry1=%r13,40(<rp=%rdi)
movq   %r13,40(%rdi)

# qhasm: *(uint64 *)(rp + 48) = ry2
# asm 1: movq   <ry2=int64#12,48(<rp=int64#1)
# asm 2: movq   <ry2=%r14,48(<rp=%rdi)
movq   %r14,48(%rdi)

# qhasm: *(uint64 *)(rp + 56) = ry3
# asm 1: movq   <ry3=int64#13,56(<rp=int64#1)
# asm 2: movq   <ry3=%r15,56(<rp=%rdi)
movq   %r15,56(%rdi)

# qhasm:   mulr4 = 0
# asm 1: mov  $0,>mulr4=int64#4
# asm 2: mov  $0,>mulr4=%rcx
mov  $0,%rcx

# qhasm:   mulr5 = 0
# asm 1: mov  $0,>mulr5=int64#5
# asm 2: mov  $0,>mulr5=%r8
mov  $0,%r8

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#6
# asm 2: mov  $0,>mulr6=%r9
mov  $0,%r9

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#8
# asm 2: mov  $0,>mulr7=%r10
mov  $0,%r10

# qhasm:   mulx0 = *(uint64 *)(pp + 32)
# asm 1: movq   32(<pp=int64#2),>mulx0=int64#9
# asm 2: movq   32(<pp=%rsi),>mulx0=%r11
movq   32(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   96(<pp=%rsi),>mulrax=%rax
movq   96(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   rz0 = mulrax
# asm 1: mov  <mulrax=int64#7,>rz0=int64#10
# asm 2: mov  <mulrax=%rax,>rz0=%r12
mov  %rax,%r12

# qhasm:   rz1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>rz1=int64#11
# asm 2: mov  <mulrdx=%rdx,>rz1=%r13
mov  %rdx,%r13

# qhasm:   mulrax = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   104(<pp=%rsi),>mulrax=%rax
movq   104(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? rz1 += mulrax
# asm 1: add  <mulrax=int64#7,<rz1=int64#11
# asm 2: add  <mulrax=%rax,<rz1=%r13
add  %rax,%r13

# qhasm:   rz2 = 0
# asm 1: mov  $0,>rz2=int64#12
# asm 2: mov  $0,>rz2=%r14
mov  $0,%r14

# qhasm:   rz2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rz2=int64#12
# asm 2: adc <mulrdx=%rdx,<rz2=%r14
adc %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   112(<pp=%rsi),>mulrax=%rax
movq   112(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? rz2 += mulrax
# asm 1: add  <mulrax=int64#7,<rz2=int64#12
# asm 2: add  <mulrax=%rax,<rz2=%r14
add  %rax,%r14

# qhasm:   rz3 = 0
# asm 1: mov  $0,>rz3=int64#13
# asm 2: mov  $0,>rz3=%r15
mov  $0,%r15

# qhasm:   rz3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rz3=int64#13
# asm 2: adc <mulrdx=%rdx,<rz3=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   120(<pp=%rsi),>mulrax=%rax
movq   120(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? rz3 += mulrax
# asm 1: add  <mulrax=int64#7,<rz3=int64#13
# asm 2: add  <mulrax=%rax,<rz3=%r15
add  %rax,%r15

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr4=%rcx
adc %rdx,%rcx

# qhasm:   mulx1 = *(uint64 *)(pp + 40)
# asm 1: movq   40(<pp=int64#2),>mulx1=int64#9
# asm 2: movq   40(<pp=%rsi),>mulx1=%r11
movq   40(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   96(<pp=%rsi),>mulrax=%rax
movq   96(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? rz1 += mulrax
# asm 1: add  <mulrax=int64#7,<rz1=int64#11
# asm 2: add  <mulrax=%rax,<rz1=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   104(<pp=%rsi),>mulrax=%rax
movq   104(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? rz2 += mulrax
# asm 1: add  <mulrax=int64#7,<rz2=int64#12
# asm 2: add  <mulrax=%rax,<rz2=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rz2 += mulc
# asm 1: add  <mulc=int64#14,<rz2=int64#12
# asm 2: add  <mulc=%rbx,<rz2=%r14
add  %rbx,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   112(<pp=%rsi),>mulrax=%rax
movq   112(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? rz3 += mulrax 
# asm 1: add  <mulrax=int64#7,<rz3=int64#13
# asm 2: add  <mulrax=%rax,<rz3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rz3 += mulc
# asm 1: add  <mulc=int64#14,<rz3=int64#13
# asm 2: add  <mulc=%rbx,<rz3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   120(<pp=%rsi),>mulrax=%rax
movq   120(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulr5 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr5=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr5=%r8
adc %rdx,%r8

# qhasm:   mulx2 = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>mulx2=int64#9
# asm 2: movq   48(<pp=%rsi),>mulx2=%r11
movq   48(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   96(<pp=%rsi),>mulrax=%rax
movq   96(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? rz2 += mulrax
# asm 1: add  <mulrax=int64#7,<rz2=int64#12
# asm 2: add  <mulrax=%rax,<rz2=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   104(<pp=%rsi),>mulrax=%rax
movq   104(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? rz3 += mulrax
# asm 1: add  <mulrax=int64#7,<rz3=int64#13
# asm 2: add  <mulrax=%rax,<rz3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rz3 += mulc
# asm 1: add  <mulc=int64#14,<rz3=int64#13
# asm 2: add  <mulc=%rbx,<rz3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   112(<pp=%rsi),>mulrax=%rax
movq   112(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   120(<pp=%rsi),>mulrax=%rax
movq   120(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#5
# asm 2: add  <mulrax=%rax,<mulr5=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#14,<mulr5=int64#5
# asm 2: add  <mulc=%rbx,<mulr5=%r8
add  %rbx,%r8

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr6=%r9
adc %rdx,%r9

# qhasm:   mulx3 = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>mulx3=int64#9
# asm 2: movq   56(<pp=%rsi),>mulx3=%r11
movq   56(%rsi),%r11

# qhasm:   mulrax = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   96(<pp=%rsi),>mulrax=%rax
movq   96(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? rz3 += mulrax
# asm 1: add  <mulrax=int64#7,<rz3=int64#13
# asm 2: add  <mulrax=%rax,<rz3=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   104(<pp=%rsi),>mulrax=%rax
movq   104(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#4
# asm 2: add  <mulrax=%rax,<mulr4=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#14,<mulr4=int64#4
# asm 2: add  <mulc=%rbx,<mulr4=%rcx
add  %rbx,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   112(<pp=%rsi),>mulrax=%rax
movq   112(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#5
# asm 2: add  <mulrax=%rax,<mulr5=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#14,<mulr5=int64#5
# asm 2: add  <mulc=%rbx,<mulr5=%r8
add  %rbx,%r8

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulrax=int64#7
# asm 2: movq   120(<pp=%rsi),>mulrax=%rax
movq   120(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#6
# asm 2: add  <mulrax=%rax,<mulr6=%r9
add  %rax,%r9

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#14,<mulr6=int64#6
# asm 2: add  <mulc=%rbx,<mulr6=%r9
add  %rbx,%r9

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#8
# asm 2: adc <mulrdx=%rdx,<mulr7=%r10
adc %rdx,%r10

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#4,>mulrax=int64#7
# asm 2: mov  <mulr4=%rcx,>mulrax=%rax
mov  %rcx,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#2
# asm 2: mov  <mulrax=%rax,>mulr4=%rsi
mov  %rax,%rsi

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr5=%r8,>mulrax=%rax
mov  %r8,%rax

# qhasm:   mulr5 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>mulr5=int64#4
# asm 2: mov  <mulrdx=%rdx,>mulr5=%rcx
mov  %rdx,%rcx

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrax = mulr6
# asm 1: mov  <mulr6=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr6=%r9,>mulrax=%rax
mov  %r9,%rax

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#5
# asm 2: mov  $0,>mulr6=%r8
mov  $0,%r8

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr6=%r8
adc %rdx,%r8

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#5
# asm 2: add  <mulrax=%rax,<mulr6=%r8
add  %rax,%r8

# qhasm:   mulrax = mulr7
# asm 1: mov  <mulr7=int64#8,>mulrax=int64#7
# asm 2: mov  <mulr7=%r10,>mulrax=%rax
mov  %r10,%rax

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#6
# asm 2: mov  $0,>mulr7=%r9
mov  $0,%r9

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr7=%r9
adc %rdx,%r9

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

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

# qhasm:   carry? rz0 += mulr4
# asm 1: add  <mulr4=int64#2,<rz0=int64#10
# asm 2: add  <mulr4=%rsi,<rz0=%r12
add  %rsi,%r12

# qhasm:   carry? rz1 += mulr5 + carry
# asm 1: adc <mulr5=int64#4,<rz1=int64#11
# asm 2: adc <mulr5=%rcx,<rz1=%r13
adc %rcx,%r13

# qhasm:   carry? rz2 += mulr6 + carry
# asm 1: adc <mulr6=int64#5,<rz2=int64#12
# asm 2: adc <mulr6=%r8,<rz2=%r14
adc %r8,%r14

# qhasm:   carry? rz3 += mulr7 + carry
# asm 1: adc <mulr7=int64#6,<rz3=int64#13
# asm 2: adc <mulr7=%r9,<rz3=%r15
adc %r9,%r15

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

# qhasm:   carry? rz0 += mulr8
# asm 1: add  <mulr8=int64#3,<rz0=int64#10
# asm 2: add  <mulr8=%rdx,<rz0=%r12
add  %rdx,%r12

# qhasm:   carry? rz1 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rz1=int64#11
# asm 2: adc <mulzero=%rsi,<rz1=%r13
adc %rsi,%r13

# qhasm:   carry? rz2 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rz2=int64#12
# asm 2: adc <mulzero=%rsi,<rz2=%r14
adc %rsi,%r14

# qhasm:   carry? rz3 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rz3=int64#13
# asm 2: adc <mulzero=%rsi,<rz3=%r15
adc %rsi,%r15

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulzero=int64#2
# asm 2: adc <mulzero=%rsi,<mulzero=%rsi
adc %rsi,%rsi

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#2,>mulzero=int64#2
# asm 2: imulq  $38,<mulzero=%rsi,>mulzero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   rz0 += mulzero
# asm 1: add  <mulzero=int64#2,<rz0=int64#10
# asm 2: add  <mulzero=%rsi,<rz0=%r12
add  %rsi,%r12

# qhasm: *(uint64 *)(rp + 64) = rz0
# asm 1: movq   <rz0=int64#10,64(<rp=int64#1)
# asm 2: movq   <rz0=%r12,64(<rp=%rdi)
movq   %r12,64(%rdi)

# qhasm: *(uint64 *)(rp + 72) = rz1
# asm 1: movq   <rz1=int64#11,72(<rp=int64#1)
# asm 2: movq   <rz1=%r13,72(<rp=%rdi)
movq   %r13,72(%rdi)

# qhasm: *(uint64 *)(rp + 80) = rz2
# asm 1: movq   <rz2=int64#12,80(<rp=int64#1)
# asm 2: movq   <rz2=%r14,80(<rp=%rdi)
movq   %r14,80(%rdi)

# qhasm: *(uint64 *)(rp + 88) = rz3
# asm 1: movq   <rz3=int64#13,88(<rp=int64#1)
# asm 2: movq   <rz3=%r15,88(<rp=%rdi)
movq   %r15,88(%rdi)

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
