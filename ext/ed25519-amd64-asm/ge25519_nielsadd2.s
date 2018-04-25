
# qhasm: int64 rp

# qhasm: int64 qp

# qhasm: input rp

# qhasm: input qp

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

# qhasm: int64 f0

# qhasm: int64 f1

# qhasm: int64 f2

# qhasm: int64 f3

# qhasm: stack64 f0_stack

# qhasm: stack64 f1_stack

# qhasm: stack64 f2_stack

# qhasm: stack64 f3_stack

# qhasm: int64 g0

# qhasm: int64 g1

# qhasm: int64 g2

# qhasm: int64 g3

# qhasm: stack64 g0_stack

# qhasm: stack64 g1_stack

# qhasm: stack64 g2_stack

# qhasm: stack64 g3_stack

# qhasm: int64 h0

# qhasm: int64 h1

# qhasm: int64 h2

# qhasm: int64 h3

# qhasm: stack64 h0_stack

# qhasm: stack64 h1_stack

# qhasm: stack64 h2_stack

# qhasm: stack64 h3_stack

# qhasm: int64 qt0

# qhasm: int64 qt1

# qhasm: int64 qt2

# qhasm: int64 qt3

# qhasm: stack64 qt0_stack

# qhasm: stack64 qt1_stack

# qhasm: stack64 qt2_stack

# qhasm: stack64 qt3_stack

# qhasm: int64 t10

# qhasm: int64 t11

# qhasm: int64 t12

# qhasm: int64 t13

# qhasm: stack64 t10_stack

# qhasm: stack64 t11_stack

# qhasm: stack64 t12_stack

# qhasm: stack64 t13_stack

# qhasm: int64 t20

# qhasm: int64 t21

# qhasm: int64 t22

# qhasm: int64 t23

# qhasm: stack64 t20_stack

# qhasm: stack64 t21_stack

# qhasm: stack64 t22_stack

# qhasm: stack64 t23_stack

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

# qhasm: int64 addt0

# qhasm: int64 addt1

# qhasm: int64 subt0

# qhasm: int64 subt1

# qhasm: enter crypto_sign_ed25519_amd64_64_ge25519_nielsadd2
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_ge25519_nielsadd2
.globl crypto_sign_ed25519_amd64_64_ge25519_nielsadd2
_crypto_sign_ed25519_amd64_64_ge25519_nielsadd2:
crypto_sign_ed25519_amd64_64_ge25519_nielsadd2:
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

# qhasm: a0 = *(uint64 *)(rp + 32)
# asm 1: movq   32(<rp=int64#1),>a0=int64#3
# asm 2: movq   32(<rp=%rdi),>a0=%rdx
movq   32(%rdi),%rdx

# qhasm: a1 = *(uint64 *)(rp + 40)
# asm 1: movq   40(<rp=int64#1),>a1=int64#4
# asm 2: movq   40(<rp=%rdi),>a1=%rcx
movq   40(%rdi),%rcx

# qhasm: a2 = *(uint64 *)(rp + 48)
# asm 1: movq   48(<rp=int64#1),>a2=int64#5
# asm 2: movq   48(<rp=%rdi),>a2=%r8
movq   48(%rdi),%r8

# qhasm: a3 = *(uint64 *)(rp + 56)
# asm 1: movq   56(<rp=int64#1),>a3=int64#6
# asm 2: movq   56(<rp=%rdi),>a3=%r9
movq   56(%rdi),%r9

# qhasm: b0 = a0
# asm 1: mov  <a0=int64#3,>b0=int64#7
# asm 2: mov  <a0=%rdx,>b0=%rax
mov  %rdx,%rax

# qhasm: b1 = a1
# asm 1: mov  <a1=int64#4,>b1=int64#8
# asm 2: mov  <a1=%rcx,>b1=%r10
mov  %rcx,%r10

# qhasm: b2 = a2
# asm 1: mov  <a2=int64#5,>b2=int64#9
# asm 2: mov  <a2=%r8,>b2=%r11
mov  %r8,%r11

# qhasm: b3 = a3
# asm 1: mov  <a3=int64#6,>b3=int64#10
# asm 2: mov  <a3=%r9,>b3=%r12
mov  %r9,%r12

# qhasm:   carry? a0 -= *(uint64 *) (rp + 0) 
# asm 1: subq 0(<rp=int64#1),<a0=int64#3
# asm 2: subq 0(<rp=%rdi),<a0=%rdx
subq 0(%rdi),%rdx

# qhasm:   carry? a1 -= *(uint64 *) (rp + 8) - carry
# asm 1: sbbq 8(<rp=int64#1),<a1=int64#4
# asm 2: sbbq 8(<rp=%rdi),<a1=%rcx
sbbq 8(%rdi),%rcx

# qhasm:   carry? a2 -= *(uint64 *) (rp + 16) - carry
# asm 1: sbbq 16(<rp=int64#1),<a2=int64#5
# asm 2: sbbq 16(<rp=%rdi),<a2=%r8
sbbq 16(%rdi),%r8

# qhasm:   carry? a3 -= *(uint64 *) (rp + 24) - carry
# asm 1: sbbq 24(<rp=int64#1),<a3=int64#6
# asm 2: sbbq 24(<rp=%rdi),<a3=%r9
sbbq 24(%rdi),%r9

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

# qhasm:   carry? a0 -= subt1
# asm 1: sub  <subt1=int64#12,<a0=int64#3
# asm 2: sub  <subt1=%r14,<a0=%rdx
sub  %r14,%rdx

# qhasm:   carry? a1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<a1=int64#4
# asm 2: sbb  <subt0=%r13,<a1=%rcx
sbb  %r13,%rcx

# qhasm:   carry? a2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<a2=int64#5
# asm 2: sbb  <subt0=%r13,<a2=%r8
sbb  %r13,%r8

# qhasm:   carry? a3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#11,<a3=int64#6
# asm 2: sbb  <subt0=%r13,<a3=%r9
sbb  %r13,%r9

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#12,<subt0=int64#11
# asm 2: cmovc <subt1=%r14,<subt0=%r13
cmovc %r14,%r13

# qhasm:   a0 -= subt0
# asm 1: sub  <subt0=int64#11,<a0=int64#3
# asm 2: sub  <subt0=%r13,<a0=%rdx
sub  %r13,%rdx

# qhasm:   carry? b0 += *(uint64 *) (rp + 0) 
# asm 1: addq 0(<rp=int64#1),<b0=int64#7
# asm 2: addq 0(<rp=%rdi),<b0=%rax
addq 0(%rdi),%rax

# qhasm:   carry? b1 += *(uint64 *) (rp + 8) + carry
# asm 1: adcq 8(<rp=int64#1),<b1=int64#8
# asm 2: adcq 8(<rp=%rdi),<b1=%r10
adcq 8(%rdi),%r10

# qhasm:   carry? b2 += *(uint64 *) (rp + 16) + carry
# asm 1: adcq 16(<rp=int64#1),<b2=int64#9
# asm 2: adcq 16(<rp=%rdi),<b2=%r11
adcq 16(%rdi),%r11

# qhasm:   carry? b3 += *(uint64 *) (rp + 24) + carry
# asm 1: adcq 24(<rp=int64#1),<b3=int64#10
# asm 2: adcq 24(<rp=%rdi),<b3=%r12
adcq 24(%rdi),%r12

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

# qhasm:   carry? b0 += addt1
# asm 1: add  <addt1=int64#12,<b0=int64#7
# asm 2: add  <addt1=%r14,<b0=%rax
add  %r14,%rax

# qhasm:   carry? b1 += addt0 + carry
# asm 1: adc <addt0=int64#11,<b1=int64#8
# asm 2: adc <addt0=%r13,<b1=%r10
adc %r13,%r10

# qhasm:   carry? b2 += addt0 + carry
# asm 1: adc <addt0=int64#11,<b2=int64#9
# asm 2: adc <addt0=%r13,<b2=%r11
adc %r13,%r11

# qhasm:   carry? b3 += addt0 + carry
# asm 1: adc <addt0=int64#11,<b3=int64#10
# asm 2: adc <addt0=%r13,<b3=%r12
adc %r13,%r12

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#12,<addt0=int64#11
# asm 2: cmovc <addt1=%r14,<addt0=%r13
cmovc %r14,%r13

# qhasm:   b0 += addt0
# asm 1: add  <addt0=int64#11,<b0=int64#7
# asm 2: add  <addt0=%r13,<b0=%rax
add  %r13,%rax

# qhasm: a0_stack = a0
# asm 1: movq <a0=int64#3,>a0_stack=stack64#8
# asm 2: movq <a0=%rdx,>a0_stack=56(%rsp)
movq %rdx,56(%rsp)

# qhasm: a1_stack = a1
# asm 1: movq <a1=int64#4,>a1_stack=stack64#9
# asm 2: movq <a1=%rcx,>a1_stack=64(%rsp)
movq %rcx,64(%rsp)

# qhasm: a2_stack = a2
# asm 1: movq <a2=int64#5,>a2_stack=stack64#10
# asm 2: movq <a2=%r8,>a2_stack=72(%rsp)
movq %r8,72(%rsp)

# qhasm: a3_stack = a3
# asm 1: movq <a3=int64#6,>a3_stack=stack64#11
# asm 2: movq <a3=%r9,>a3_stack=80(%rsp)
movq %r9,80(%rsp)

# qhasm: b0_stack = b0
# asm 1: movq <b0=int64#7,>b0_stack=stack64#12
# asm 2: movq <b0=%rax,>b0_stack=88(%rsp)
movq %rax,88(%rsp)

# qhasm: b1_stack = b1
# asm 1: movq <b1=int64#8,>b1_stack=stack64#13
# asm 2: movq <b1=%r10,>b1_stack=96(%rsp)
movq %r10,96(%rsp)

# qhasm: b2_stack = b2
# asm 1: movq <b2=int64#9,>b2_stack=stack64#14
# asm 2: movq <b2=%r11,>b2_stack=104(%rsp)
movq %r11,104(%rsp)

# qhasm: b3_stack = b3
# asm 1: movq <b3=int64#10,>b3_stack=stack64#15
# asm 2: movq <b3=%r12,>b3_stack=112(%rsp)
movq %r12,112(%rsp)

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

# qhasm:   mulx0 = a0_stack
# asm 1: movq <a0_stack=stack64#8,>mulx0=int64#9
# asm 2: movq <a0_stack=56(%rsp),>mulx0=%r11
movq 56(%rsp),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 0)
# asm 1: movq   0(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   0(<qp=%rsi),>mulrax=%rax
movq   0(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   a0 = mulrax
# asm 1: mov  <mulrax=int64#7,>a0=int64#10
# asm 2: mov  <mulrax=%rax,>a0=%r12
mov  %rax,%r12

# qhasm:   a1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>a1=int64#11
# asm 2: mov  <mulrdx=%rdx,>a1=%r13
mov  %rdx,%r13

# qhasm:   mulrax = *(uint64 *)(qp + 8)
# asm 1: movq   8(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   8(<qp=%rsi),>mulrax=%rax
movq   8(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? a1 += mulrax
# asm 1: add  <mulrax=int64#7,<a1=int64#11
# asm 2: add  <mulrax=%rax,<a1=%r13
add  %rax,%r13

# qhasm:   a2 = 0
# asm 1: mov  $0,>a2=int64#12
# asm 2: mov  $0,>a2=%r14
mov  $0,%r14

# qhasm:   a2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<a2=int64#12
# asm 2: adc <mulrdx=%rdx,<a2=%r14
adc %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(qp + 16)
# asm 1: movq   16(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   16(<qp=%rsi),>mulrax=%rax
movq   16(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? a2 += mulrax
# asm 1: add  <mulrax=int64#7,<a2=int64#12
# asm 2: add  <mulrax=%rax,<a2=%r14
add  %rax,%r14

# qhasm:   a3 = 0
# asm 1: mov  $0,>a3=int64#13
# asm 2: mov  $0,>a3=%r15
mov  $0,%r15

# qhasm:   a3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<a3=int64#13
# asm 2: adc <mulrdx=%rdx,<a3=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(qp + 24)
# asm 1: movq   24(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   24(<qp=%rsi),>mulrax=%rax
movq   24(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? a3 += mulrax
# asm 1: add  <mulrax=int64#7,<a3=int64#13
# asm 2: add  <mulrax=%rax,<a3=%r15
add  %rax,%r15

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr4=%rcx
adc %rdx,%rcx

# qhasm:   mulx1 = a1_stack
# asm 1: movq <a1_stack=stack64#9,>mulx1=int64#9
# asm 2: movq <a1_stack=64(%rsp),>mulx1=%r11
movq 64(%rsp),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 0)
# asm 1: movq   0(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   0(<qp=%rsi),>mulrax=%rax
movq   0(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? a1 += mulrax
# asm 1: add  <mulrax=int64#7,<a1=int64#11
# asm 2: add  <mulrax=%rax,<a1=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 8)
# asm 1: movq   8(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   8(<qp=%rsi),>mulrax=%rax
movq   8(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? a2 += mulrax
# asm 1: add  <mulrax=int64#7,<a2=int64#12
# asm 2: add  <mulrax=%rax,<a2=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? a2 += mulc
# asm 1: add  <mulc=int64#14,<a2=int64#12
# asm 2: add  <mulc=%rbx,<a2=%r14
add  %rbx,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 16)
# asm 1: movq   16(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   16(<qp=%rsi),>mulrax=%rax
movq   16(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? a3 += mulrax 
# asm 1: add  <mulrax=int64#7,<a3=int64#13
# asm 2: add  <mulrax=%rax,<a3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? a3 += mulc
# asm 1: add  <mulc=int64#14,<a3=int64#13
# asm 2: add  <mulc=%rbx,<a3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 24)
# asm 1: movq   24(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   24(<qp=%rsi),>mulrax=%rax
movq   24(%rsi),%rax

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

# qhasm:   mulx2 = a2_stack
# asm 1: movq <a2_stack=stack64#10,>mulx2=int64#9
# asm 2: movq <a2_stack=72(%rsp),>mulx2=%r11
movq 72(%rsp),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 0)
# asm 1: movq   0(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   0(<qp=%rsi),>mulrax=%rax
movq   0(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? a2 += mulrax
# asm 1: add  <mulrax=int64#7,<a2=int64#12
# asm 2: add  <mulrax=%rax,<a2=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 8)
# asm 1: movq   8(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   8(<qp=%rsi),>mulrax=%rax
movq   8(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? a3 += mulrax
# asm 1: add  <mulrax=int64#7,<a3=int64#13
# asm 2: add  <mulrax=%rax,<a3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? a3 += mulc
# asm 1: add  <mulc=int64#14,<a3=int64#13
# asm 2: add  <mulc=%rbx,<a3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 16)
# asm 1: movq   16(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   16(<qp=%rsi),>mulrax=%rax
movq   16(%rsi),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 24)
# asm 1: movq   24(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   24(<qp=%rsi),>mulrax=%rax
movq   24(%rsi),%rax

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

# qhasm:   mulx3 = a3_stack
# asm 1: movq <a3_stack=stack64#11,>mulx3=int64#9
# asm 2: movq <a3_stack=80(%rsp),>mulx3=%r11
movq 80(%rsp),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 0)
# asm 1: movq   0(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   0(<qp=%rsi),>mulrax=%rax
movq   0(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? a3 += mulrax
# asm 1: add  <mulrax=int64#7,<a3=int64#13
# asm 2: add  <mulrax=%rax,<a3=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 8)
# asm 1: movq   8(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   8(<qp=%rsi),>mulrax=%rax
movq   8(%rsi),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 16)
# asm 1: movq   16(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   16(<qp=%rsi),>mulrax=%rax
movq   16(%rsi),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 24)
# asm 1: movq   24(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   24(<qp=%rsi),>mulrax=%rax
movq   24(%rsi),%rax

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

# qhasm:   carry? a0 += mulr4
# asm 1: add  <mulr4=int64#4,<a0=int64#10
# asm 2: add  <mulr4=%rcx,<a0=%r12
add  %rcx,%r12

# qhasm:   carry? a1 += mulr5 + carry
# asm 1: adc <mulr5=int64#5,<a1=int64#11
# asm 2: adc <mulr5=%r8,<a1=%r13
adc %r8,%r13

# qhasm:   carry? a2 += mulr6 + carry
# asm 1: adc <mulr6=int64#6,<a2=int64#12
# asm 2: adc <mulr6=%r9,<a2=%r14
adc %r9,%r14

# qhasm:   carry? a3 += mulr7 + carry
# asm 1: adc <mulr7=int64#8,<a3=int64#13
# asm 2: adc <mulr7=%r10,<a3=%r15
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

# qhasm:   carry? a0 += mulr8
# asm 1: add  <mulr8=int64#4,<a0=int64#10
# asm 2: add  <mulr8=%rcx,<a0=%r12
add  %rcx,%r12

# qhasm:   carry? a1 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<a1=int64#11
# asm 2: adc <mulzero=%rdx,<a1=%r13
adc %rdx,%r13

# qhasm:   carry? a2 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<a2=int64#12
# asm 2: adc <mulzero=%rdx,<a2=%r14
adc %rdx,%r14

# qhasm:   carry? a3 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<a3=int64#13
# asm 2: adc <mulzero=%rdx,<a3=%r15
adc %rdx,%r15

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulzero=int64#3
# asm 2: adc <mulzero=%rdx,<mulzero=%rdx
adc %rdx,%rdx

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#3,>mulzero=int64#3
# asm 2: imulq  $38,<mulzero=%rdx,>mulzero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   a0 += mulzero
# asm 1: add  <mulzero=int64#3,<a0=int64#10
# asm 2: add  <mulzero=%rdx,<a0=%r12
add  %rdx,%r12

# qhasm: a0_stack = a0
# asm 1: movq <a0=int64#10,>a0_stack=stack64#8
# asm 2: movq <a0=%r12,>a0_stack=56(%rsp)
movq %r12,56(%rsp)

# qhasm: a1_stack = a1
# asm 1: movq <a1=int64#11,>a1_stack=stack64#9
# asm 2: movq <a1=%r13,>a1_stack=64(%rsp)
movq %r13,64(%rsp)

# qhasm: a2_stack = a2
# asm 1: movq <a2=int64#12,>a2_stack=stack64#10
# asm 2: movq <a2=%r14,>a2_stack=72(%rsp)
movq %r14,72(%rsp)

# qhasm: a3_stack = a3
# asm 1: movq <a3=int64#13,>a3_stack=stack64#11
# asm 2: movq <a3=%r15,>a3_stack=80(%rsp)
movq %r15,80(%rsp)

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

# qhasm:   mulx0 = b0_stack
# asm 1: movq <b0_stack=stack64#12,>mulx0=int64#9
# asm 2: movq <b0_stack=88(%rsp),>mulx0=%r11
movq 88(%rsp),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   32(<qp=%rsi),>mulrax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   e0 = mulrax
# asm 1: mov  <mulrax=int64#7,>e0=int64#10
# asm 2: mov  <mulrax=%rax,>e0=%r12
mov  %rax,%r12

# qhasm:   e1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>e1=int64#11
# asm 2: mov  <mulrdx=%rdx,>e1=%r13
mov  %rdx,%r13

# qhasm:   mulrax = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   40(<qp=%rsi),>mulrax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? e1 += mulrax
# asm 1: add  <mulrax=int64#7,<e1=int64#11
# asm 2: add  <mulrax=%rax,<e1=%r13
add  %rax,%r13

# qhasm:   e2 = 0
# asm 1: mov  $0,>e2=int64#12
# asm 2: mov  $0,>e2=%r14
mov  $0,%r14

# qhasm:   e2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<e2=int64#12
# asm 2: adc <mulrdx=%rdx,<e2=%r14
adc %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   48(<qp=%rsi),>mulrax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? e2 += mulrax
# asm 1: add  <mulrax=int64#7,<e2=int64#12
# asm 2: add  <mulrax=%rax,<e2=%r14
add  %rax,%r14

# qhasm:   e3 = 0
# asm 1: mov  $0,>e3=int64#13
# asm 2: mov  $0,>e3=%r15
mov  $0,%r15

# qhasm:   e3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<e3=int64#13
# asm 2: adc <mulrdx=%rdx,<e3=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   56(<qp=%rsi),>mulrax=%rax
movq   56(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? e3 += mulrax
# asm 1: add  <mulrax=int64#7,<e3=int64#13
# asm 2: add  <mulrax=%rax,<e3=%r15
add  %rax,%r15

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr4=%rcx
adc %rdx,%rcx

# qhasm:   mulx1 = b1_stack
# asm 1: movq <b1_stack=stack64#13,>mulx1=int64#9
# asm 2: movq <b1_stack=96(%rsp),>mulx1=%r11
movq 96(%rsp),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   32(<qp=%rsi),>mulrax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? e1 += mulrax
# asm 1: add  <mulrax=int64#7,<e1=int64#11
# asm 2: add  <mulrax=%rax,<e1=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   40(<qp=%rsi),>mulrax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? e2 += mulrax
# asm 1: add  <mulrax=int64#7,<e2=int64#12
# asm 2: add  <mulrax=%rax,<e2=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? e2 += mulc
# asm 1: add  <mulc=int64#14,<e2=int64#12
# asm 2: add  <mulc=%rbx,<e2=%r14
add  %rbx,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   48(<qp=%rsi),>mulrax=%rax
movq   48(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? e3 += mulrax 
# asm 1: add  <mulrax=int64#7,<e3=int64#13
# asm 2: add  <mulrax=%rax,<e3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? e3 += mulc
# asm 1: add  <mulc=int64#14,<e3=int64#13
# asm 2: add  <mulc=%rbx,<e3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   56(<qp=%rsi),>mulrax=%rax
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

# qhasm:   mulx2 = b2_stack
# asm 1: movq <b2_stack=stack64#14,>mulx2=int64#9
# asm 2: movq <b2_stack=104(%rsp),>mulx2=%r11
movq 104(%rsp),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   32(<qp=%rsi),>mulrax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? e2 += mulrax
# asm 1: add  <mulrax=int64#7,<e2=int64#12
# asm 2: add  <mulrax=%rax,<e2=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   40(<qp=%rsi),>mulrax=%rax
movq   40(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? e3 += mulrax
# asm 1: add  <mulrax=int64#7,<e3=int64#13
# asm 2: add  <mulrax=%rax,<e3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? e3 += mulc
# asm 1: add  <mulc=int64#14,<e3=int64#13
# asm 2: add  <mulc=%rbx,<e3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   48(<qp=%rsi),>mulrax=%rax
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

# qhasm:   mulrax = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   56(<qp=%rsi),>mulrax=%rax
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

# qhasm:   mulx3 = b3_stack
# asm 1: movq <b3_stack=stack64#15,>mulx3=int64#9
# asm 2: movq <b3_stack=112(%rsp),>mulx3=%r11
movq 112(%rsp),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   32(<qp=%rsi),>mulrax=%rax
movq   32(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? e3 += mulrax
# asm 1: add  <mulrax=int64#7,<e3=int64#13
# asm 2: add  <mulrax=%rax,<e3=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   40(<qp=%rsi),>mulrax=%rax
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

# qhasm:   mulrax = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   48(<qp=%rsi),>mulrax=%rax
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

# qhasm:   mulrax = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   56(<qp=%rsi),>mulrax=%rax
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

# qhasm:   carry? e0 += mulr4
# asm 1: add  <mulr4=int64#4,<e0=int64#10
# asm 2: add  <mulr4=%rcx,<e0=%r12
add  %rcx,%r12

# qhasm:   carry? e1 += mulr5 + carry
# asm 1: adc <mulr5=int64#5,<e1=int64#11
# asm 2: adc <mulr5=%r8,<e1=%r13
adc %r8,%r13

# qhasm:   carry? e2 += mulr6 + carry
# asm 1: adc <mulr6=int64#6,<e2=int64#12
# asm 2: adc <mulr6=%r9,<e2=%r14
adc %r9,%r14

# qhasm:   carry? e3 += mulr7 + carry
# asm 1: adc <mulr7=int64#8,<e3=int64#13
# asm 2: adc <mulr7=%r10,<e3=%r15
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

# qhasm:   carry? e0 += mulr8
# asm 1: add  <mulr8=int64#4,<e0=int64#10
# asm 2: add  <mulr8=%rcx,<e0=%r12
add  %rcx,%r12

# qhasm:   carry? e1 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<e1=int64#11
# asm 2: adc <mulzero=%rdx,<e1=%r13
adc %rdx,%r13

# qhasm:   carry? e2 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<e2=int64#12
# asm 2: adc <mulzero=%rdx,<e2=%r14
adc %rdx,%r14

# qhasm:   carry? e3 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<e3=int64#13
# asm 2: adc <mulzero=%rdx,<e3=%r15
adc %rdx,%r15

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulzero=int64#3
# asm 2: adc <mulzero=%rdx,<mulzero=%rdx
adc %rdx,%rdx

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#3,>mulzero=int64#3
# asm 2: imulq  $38,<mulzero=%rdx,>mulzero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   e0 += mulzero
# asm 1: add  <mulzero=int64#3,<e0=int64#10
# asm 2: add  <mulzero=%rdx,<e0=%r12
add  %rdx,%r12

# qhasm: h0 = e0
# asm 1: mov  <e0=int64#10,>h0=int64#3
# asm 2: mov  <e0=%r12,>h0=%rdx
mov  %r12,%rdx

# qhasm: h1 = e1
# asm 1: mov  <e1=int64#11,>h1=int64#4
# asm 2: mov  <e1=%r13,>h1=%rcx
mov  %r13,%rcx

# qhasm: h2 = e2
# asm 1: mov  <e2=int64#12,>h2=int64#5
# asm 2: mov  <e2=%r14,>h2=%r8
mov  %r14,%r8

# qhasm: h3 = e3
# asm 1: mov  <e3=int64#13,>h3=int64#6
# asm 2: mov  <e3=%r15,>h3=%r9
mov  %r15,%r9

# qhasm:   carry? e0 -= a0_stack 
# asm 1: subq <a0_stack=stack64#8,<e0=int64#10
# asm 2: subq <a0_stack=56(%rsp),<e0=%r12
subq 56(%rsp),%r12

# qhasm:   carry? e1 -= a1_stack - carry
# asm 1: sbbq <a1_stack=stack64#9,<e1=int64#11
# asm 2: sbbq <a1_stack=64(%rsp),<e1=%r13
sbbq 64(%rsp),%r13

# qhasm:   carry? e2 -= a2_stack - carry
# asm 1: sbbq <a2_stack=stack64#10,<e2=int64#12
# asm 2: sbbq <a2_stack=72(%rsp),<e2=%r14
sbbq 72(%rsp),%r14

# qhasm:   carry? e3 -= a3_stack - carry
# asm 1: sbbq <a3_stack=stack64#11,<e3=int64#13
# asm 2: sbbq <a3_stack=80(%rsp),<e3=%r15
sbbq 80(%rsp),%r15

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

# qhasm:   carry? e0 -= subt1
# asm 1: sub  <subt1=int64#8,<e0=int64#10
# asm 2: sub  <subt1=%r10,<e0=%r12
sub  %r10,%r12

# qhasm:   carry? e1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#7,<e1=int64#11
# asm 2: sbb  <subt0=%rax,<e1=%r13
sbb  %rax,%r13

# qhasm:   carry? e2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#7,<e2=int64#12
# asm 2: sbb  <subt0=%rax,<e2=%r14
sbb  %rax,%r14

# qhasm:   carry? e3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#7,<e3=int64#13
# asm 2: sbb  <subt0=%rax,<e3=%r15
sbb  %rax,%r15

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#8,<subt0=int64#7
# asm 2: cmovc <subt1=%r10,<subt0=%rax
cmovc %r10,%rax

# qhasm:   e0 -= subt0
# asm 1: sub  <subt0=int64#7,<e0=int64#10
# asm 2: sub  <subt0=%rax,<e0=%r12
sub  %rax,%r12

# qhasm:   carry? h0 += a0_stack 
# asm 1: addq <a0_stack=stack64#8,<h0=int64#3
# asm 2: addq <a0_stack=56(%rsp),<h0=%rdx
addq 56(%rsp),%rdx

# qhasm:   carry? h1 += a1_stack + carry
# asm 1: adcq <a1_stack=stack64#9,<h1=int64#4
# asm 2: adcq <a1_stack=64(%rsp),<h1=%rcx
adcq 64(%rsp),%rcx

# qhasm:   carry? h2 += a2_stack + carry
# asm 1: adcq <a2_stack=stack64#10,<h2=int64#5
# asm 2: adcq <a2_stack=72(%rsp),<h2=%r8
adcq 72(%rsp),%r8

# qhasm:   carry? h3 += a3_stack + carry
# asm 1: adcq <a3_stack=stack64#11,<h3=int64#6
# asm 2: adcq <a3_stack=80(%rsp),<h3=%r9
adcq 80(%rsp),%r9

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#7
# asm 2: mov  $0,>addt0=%rax
mov  $0,%rax

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#8
# asm 2: mov  $38,>addt1=%r10
mov  $38,%r10

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#7,<addt1=int64#8
# asm 2: cmovae <addt0=%rax,<addt1=%r10
cmovae %rax,%r10

# qhasm:   carry? h0 += addt1
# asm 1: add  <addt1=int64#8,<h0=int64#3
# asm 2: add  <addt1=%r10,<h0=%rdx
add  %r10,%rdx

# qhasm:   carry? h1 += addt0 + carry
# asm 1: adc <addt0=int64#7,<h1=int64#4
# asm 2: adc <addt0=%rax,<h1=%rcx
adc %rax,%rcx

# qhasm:   carry? h2 += addt0 + carry
# asm 1: adc <addt0=int64#7,<h2=int64#5
# asm 2: adc <addt0=%rax,<h2=%r8
adc %rax,%r8

# qhasm:   carry? h3 += addt0 + carry
# asm 1: adc <addt0=int64#7,<h3=int64#6
# asm 2: adc <addt0=%rax,<h3=%r9
adc %rax,%r9

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#8,<addt0=int64#7
# asm 2: cmovc <addt1=%r10,<addt0=%rax
cmovc %r10,%rax

# qhasm:   h0 += addt0
# asm 1: add  <addt0=int64#7,<h0=int64#3
# asm 2: add  <addt0=%rax,<h0=%rdx
add  %rax,%rdx

# qhasm: h0_stack = h0
# asm 1: movq <h0=int64#3,>h0_stack=stack64#8
# asm 2: movq <h0=%rdx,>h0_stack=56(%rsp)
movq %rdx,56(%rsp)

# qhasm: h1_stack = h1
# asm 1: movq <h1=int64#4,>h1_stack=stack64#9
# asm 2: movq <h1=%rcx,>h1_stack=64(%rsp)
movq %rcx,64(%rsp)

# qhasm: h2_stack = h2
# asm 1: movq <h2=int64#5,>h2_stack=stack64#10
# asm 2: movq <h2=%r8,>h2_stack=72(%rsp)
movq %r8,72(%rsp)

# qhasm: h3_stack = h3
# asm 1: movq <h3=int64#6,>h3_stack=stack64#11
# asm 2: movq <h3=%r9,>h3_stack=80(%rsp)
movq %r9,80(%rsp)

# qhasm: e0_stack = e0
# asm 1: movq <e0=int64#10,>e0_stack=stack64#12
# asm 2: movq <e0=%r12,>e0_stack=88(%rsp)
movq %r12,88(%rsp)

# qhasm: e1_stack = e1
# asm 1: movq <e1=int64#11,>e1_stack=stack64#13
# asm 2: movq <e1=%r13,>e1_stack=96(%rsp)
movq %r13,96(%rsp)

# qhasm: e2_stack = e2
# asm 1: movq <e2=int64#12,>e2_stack=stack64#14
# asm 2: movq <e2=%r14,>e2_stack=104(%rsp)
movq %r14,104(%rsp)

# qhasm: e3_stack = e3
# asm 1: movq <e3=int64#13,>e3_stack=stack64#15
# asm 2: movq <e3=%r15,>e3_stack=112(%rsp)
movq %r15,112(%rsp)

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

# qhasm:   mulx0 = *(uint64 *)(rp + 96)
# asm 1: movq   96(<rp=int64#1),>mulx0=int64#9
# asm 2: movq   96(<rp=%rdi),>mulx0=%r11
movq   96(%rdi),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   64(<qp=%rsi),>mulrax=%rax
movq   64(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   c0 = mulrax
# asm 1: mov  <mulrax=int64#7,>c0=int64#10
# asm 2: mov  <mulrax=%rax,>c0=%r12
mov  %rax,%r12

# qhasm:   c1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>c1=int64#11
# asm 2: mov  <mulrdx=%rdx,>c1=%r13
mov  %rdx,%r13

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   72(<qp=%rsi),>mulrax=%rax
movq   72(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? c1 += mulrax
# asm 1: add  <mulrax=int64#7,<c1=int64#11
# asm 2: add  <mulrax=%rax,<c1=%r13
add  %rax,%r13

# qhasm:   c2 = 0
# asm 1: mov  $0,>c2=int64#12
# asm 2: mov  $0,>c2=%r14
mov  $0,%r14

# qhasm:   c2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<c2=int64#12
# asm 2: adc <mulrdx=%rdx,<c2=%r14
adc %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   80(<qp=%rsi),>mulrax=%rax
movq   80(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? c2 += mulrax
# asm 1: add  <mulrax=int64#7,<c2=int64#12
# asm 2: add  <mulrax=%rax,<c2=%r14
add  %rax,%r14

# qhasm:   c3 = 0
# asm 1: mov  $0,>c3=int64#13
# asm 2: mov  $0,>c3=%r15
mov  $0,%r15

# qhasm:   c3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<c3=int64#13
# asm 2: adc <mulrdx=%rdx,<c3=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   88(<qp=%rsi),>mulrax=%rax
movq   88(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#9
# asm 2: mul  <mulx0=%r11
mul  %r11

# qhasm:   carry? c3 += mulrax
# asm 1: add  <mulrax=int64#7,<c3=int64#13
# asm 2: add  <mulrax=%rax,<c3=%r15
add  %rax,%r15

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr4=%rcx
adc %rdx,%rcx

# qhasm:   mulx1 = *(uint64 *)(rp + 104)
# asm 1: movq   104(<rp=int64#1),>mulx1=int64#9
# asm 2: movq   104(<rp=%rdi),>mulx1=%r11
movq   104(%rdi),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   64(<qp=%rsi),>mulrax=%rax
movq   64(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? c1 += mulrax
# asm 1: add  <mulrax=int64#7,<c1=int64#11
# asm 2: add  <mulrax=%rax,<c1=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   72(<qp=%rsi),>mulrax=%rax
movq   72(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? c2 += mulrax
# asm 1: add  <mulrax=int64#7,<c2=int64#12
# asm 2: add  <mulrax=%rax,<c2=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? c2 += mulc
# asm 1: add  <mulc=int64#14,<c2=int64#12
# asm 2: add  <mulc=%rbx,<c2=%r14
add  %rbx,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   80(<qp=%rsi),>mulrax=%rax
movq   80(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#9
# asm 2: mul  <mulx1=%r11
mul  %r11

# qhasm:   carry? c3 += mulrax 
# asm 1: add  <mulrax=int64#7,<c3=int64#13
# asm 2: add  <mulrax=%rax,<c3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? c3 += mulc
# asm 1: add  <mulc=int64#14,<c3=int64#13
# asm 2: add  <mulc=%rbx,<c3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   88(<qp=%rsi),>mulrax=%rax
movq   88(%rsi),%rax

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

# qhasm:   mulx2 = *(uint64 *)(rp + 112)
# asm 1: movq   112(<rp=int64#1),>mulx2=int64#9
# asm 2: movq   112(<rp=%rdi),>mulx2=%r11
movq   112(%rdi),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   64(<qp=%rsi),>mulrax=%rax
movq   64(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? c2 += mulrax
# asm 1: add  <mulrax=int64#7,<c2=int64#12
# asm 2: add  <mulrax=%rax,<c2=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   72(<qp=%rsi),>mulrax=%rax
movq   72(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#9
# asm 2: mul  <mulx2=%r11
mul  %r11

# qhasm:   carry? c3 += mulrax
# asm 1: add  <mulrax=int64#7,<c3=int64#13
# asm 2: add  <mulrax=%rax,<c3=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? c3 += mulc
# asm 1: add  <mulc=int64#14,<c3=int64#13
# asm 2: add  <mulc=%rbx,<c3=%r15
add  %rbx,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   80(<qp=%rsi),>mulrax=%rax
movq   80(%rsi),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   88(<qp=%rsi),>mulrax=%rax
movq   88(%rsi),%rax

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

# qhasm:   mulx3 = *(uint64 *)(rp + 120)
# asm 1: movq   120(<rp=int64#1),>mulx3=int64#9
# asm 2: movq   120(<rp=%rdi),>mulx3=%r11
movq   120(%rdi),%r11

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   64(<qp=%rsi),>mulrax=%rax
movq   64(%rsi),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#9
# asm 2: mul  <mulx3=%r11
mul  %r11

# qhasm:   carry? c3 += mulrax
# asm 1: add  <mulrax=int64#7,<c3=int64#13
# asm 2: add  <mulrax=%rax,<c3=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#14
# asm 2: mov  $0,>mulc=%rbx
mov  $0,%rbx

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#14
# asm 2: adc <mulrdx=%rdx,<mulc=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   72(<qp=%rsi),>mulrax=%rax
movq   72(%rsi),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   80(<qp=%rsi),>mulrax=%rax
movq   80(%rsi),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#2),>mulrax=int64#7
# asm 2: movq   88(<qp=%rsi),>mulrax=%rax
movq   88(%rsi),%rax

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

# qhasm:   carry? c0 += mulr4
# asm 1: add  <mulr4=int64#2,<c0=int64#10
# asm 2: add  <mulr4=%rsi,<c0=%r12
add  %rsi,%r12

# qhasm:   carry? c1 += mulr5 + carry
# asm 1: adc <mulr5=int64#4,<c1=int64#11
# asm 2: adc <mulr5=%rcx,<c1=%r13
adc %rcx,%r13

# qhasm:   carry? c2 += mulr6 + carry
# asm 1: adc <mulr6=int64#5,<c2=int64#12
# asm 2: adc <mulr6=%r8,<c2=%r14
adc %r8,%r14

# qhasm:   carry? c3 += mulr7 + carry
# asm 1: adc <mulr7=int64#6,<c3=int64#13
# asm 2: adc <mulr7=%r9,<c3=%r15
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

# qhasm:   carry? c0 += mulr8
# asm 1: add  <mulr8=int64#3,<c0=int64#10
# asm 2: add  <mulr8=%rdx,<c0=%r12
add  %rdx,%r12

# qhasm:   carry? c1 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<c1=int64#11
# asm 2: adc <mulzero=%rsi,<c1=%r13
adc %rsi,%r13

# qhasm:   carry? c2 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<c2=int64#12
# asm 2: adc <mulzero=%rsi,<c2=%r14
adc %rsi,%r14

# qhasm:   carry? c3 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<c3=int64#13
# asm 2: adc <mulzero=%rsi,<c3=%r15
adc %rsi,%r15

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulzero=int64#2
# asm 2: adc <mulzero=%rsi,<mulzero=%rsi
adc %rsi,%rsi

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#2,>mulzero=int64#2
# asm 2: imulq  $38,<mulzero=%rsi,>mulzero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   c0 += mulzero
# asm 1: add  <mulzero=int64#2,<c0=int64#10
# asm 2: add  <mulzero=%rsi,<c0=%r12
add  %rsi,%r12

# qhasm: f0 = *(uint64 *)(rp + 64)
# asm 1: movq   64(<rp=int64#1),>f0=int64#2
# asm 2: movq   64(<rp=%rdi),>f0=%rsi
movq   64(%rdi),%rsi

# qhasm: f1 = *(uint64 *)(rp + 72)
# asm 1: movq   72(<rp=int64#1),>f1=int64#3
# asm 2: movq   72(<rp=%rdi),>f1=%rdx
movq   72(%rdi),%rdx

# qhasm: f2 = *(uint64 *)(rp + 80)
# asm 1: movq   80(<rp=int64#1),>f2=int64#4
# asm 2: movq   80(<rp=%rdi),>f2=%rcx
movq   80(%rdi),%rcx

# qhasm: f3 = *(uint64 *)(rp + 88)
# asm 1: movq   88(<rp=int64#1),>f3=int64#5
# asm 2: movq   88(<rp=%rdi),>f3=%r8
movq   88(%rdi),%r8

# qhasm:   carry? f0 += f0 
# asm 1: add  <f0=int64#2,<f0=int64#2
# asm 2: add  <f0=%rsi,<f0=%rsi
add  %rsi,%rsi

# qhasm:   carry? f1 += f1 + carry
# asm 1: adc <f1=int64#3,<f1=int64#3
# asm 2: adc <f1=%rdx,<f1=%rdx
adc %rdx,%rdx

# qhasm:   carry? f2 += f2 + carry
# asm 1: adc <f2=int64#4,<f2=int64#4
# asm 2: adc <f2=%rcx,<f2=%rcx
adc %rcx,%rcx

# qhasm:   carry? f3 += f3 + carry
# asm 1: adc <f3=int64#5,<f3=int64#5
# asm 2: adc <f3=%r8,<f3=%r8
adc %r8,%r8

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#6
# asm 2: mov  $0,>addt0=%r9
mov  $0,%r9

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#7
# asm 2: mov  $38,>addt1=%rax
mov  $38,%rax

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#6,<addt1=int64#7
# asm 2: cmovae <addt0=%r9,<addt1=%rax
cmovae %r9,%rax

# qhasm:   carry? f0 += addt1
# asm 1: add  <addt1=int64#7,<f0=int64#2
# asm 2: add  <addt1=%rax,<f0=%rsi
add  %rax,%rsi

# qhasm:   carry? f1 += addt0 + carry
# asm 1: adc <addt0=int64#6,<f1=int64#3
# asm 2: adc <addt0=%r9,<f1=%rdx
adc %r9,%rdx

# qhasm:   carry? f2 += addt0 + carry
# asm 1: adc <addt0=int64#6,<f2=int64#4
# asm 2: adc <addt0=%r9,<f2=%rcx
adc %r9,%rcx

# qhasm:   carry? f3 += addt0 + carry
# asm 1: adc <addt0=int64#6,<f3=int64#5
# asm 2: adc <addt0=%r9,<f3=%r8
adc %r9,%r8

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#7,<addt0=int64#6
# asm 2: cmovc <addt1=%rax,<addt0=%r9
cmovc %rax,%r9

# qhasm:   f0 += addt0
# asm 1: add  <addt0=int64#6,<f0=int64#2
# asm 2: add  <addt0=%r9,<f0=%rsi
add  %r9,%rsi

# qhasm: g0 = f0
# asm 1: mov  <f0=int64#2,>g0=int64#6
# asm 2: mov  <f0=%rsi,>g0=%r9
mov  %rsi,%r9

# qhasm: g1 = f1
# asm 1: mov  <f1=int64#3,>g1=int64#7
# asm 2: mov  <f1=%rdx,>g1=%rax
mov  %rdx,%rax

# qhasm: g2 = f2
# asm 1: mov  <f2=int64#4,>g2=int64#8
# asm 2: mov  <f2=%rcx,>g2=%r10
mov  %rcx,%r10

# qhasm: g3 = f3
# asm 1: mov  <f3=int64#5,>g3=int64#9
# asm 2: mov  <f3=%r8,>g3=%r11
mov  %r8,%r11

# qhasm:   carry? f0 -= c0 
# asm 1: sub  <c0=int64#10,<f0=int64#2
# asm 2: sub  <c0=%r12,<f0=%rsi
sub  %r12,%rsi

# qhasm:   carry? f1 -= c1 - carry
# asm 1: sbb  <c1=int64#11,<f1=int64#3
# asm 2: sbb  <c1=%r13,<f1=%rdx
sbb  %r13,%rdx

# qhasm:   carry? f2 -= c2 - carry
# asm 1: sbb  <c2=int64#12,<f2=int64#4
# asm 2: sbb  <c2=%r14,<f2=%rcx
sbb  %r14,%rcx

# qhasm:   carry? f3 -= c3 - carry
# asm 1: sbb  <c3=int64#13,<f3=int64#5
# asm 2: sbb  <c3=%r15,<f3=%r8
sbb  %r15,%r8

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#14
# asm 2: mov  $0,>subt0=%rbx
mov  $0,%rbx

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#15
# asm 2: mov  $38,>subt1=%rbp
mov  $38,%rbp

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#14,<subt1=int64#15
# asm 2: cmovae <subt0=%rbx,<subt1=%rbp
cmovae %rbx,%rbp

# qhasm:   carry? f0 -= subt1
# asm 1: sub  <subt1=int64#15,<f0=int64#2
# asm 2: sub  <subt1=%rbp,<f0=%rsi
sub  %rbp,%rsi

# qhasm:   carry? f1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#14,<f1=int64#3
# asm 2: sbb  <subt0=%rbx,<f1=%rdx
sbb  %rbx,%rdx

# qhasm:   carry? f2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#14,<f2=int64#4
# asm 2: sbb  <subt0=%rbx,<f2=%rcx
sbb  %rbx,%rcx

# qhasm:   carry? f3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#14,<f3=int64#5
# asm 2: sbb  <subt0=%rbx,<f3=%r8
sbb  %rbx,%r8

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#15,<subt0=int64#14
# asm 2: cmovc <subt1=%rbp,<subt0=%rbx
cmovc %rbp,%rbx

# qhasm:   f0 -= subt0
# asm 1: sub  <subt0=int64#14,<f0=int64#2
# asm 2: sub  <subt0=%rbx,<f0=%rsi
sub  %rbx,%rsi

# qhasm:   carry? g0 += c0 
# asm 1: add  <c0=int64#10,<g0=int64#6
# asm 2: add  <c0=%r12,<g0=%r9
add  %r12,%r9

# qhasm:   carry? g1 += c1 + carry
# asm 1: adc <c1=int64#11,<g1=int64#7
# asm 2: adc <c1=%r13,<g1=%rax
adc %r13,%rax

# qhasm:   carry? g2 += c2 + carry
# asm 1: adc <c2=int64#12,<g2=int64#8
# asm 2: adc <c2=%r14,<g2=%r10
adc %r14,%r10

# qhasm:   carry? g3 += c3 + carry
# asm 1: adc <c3=int64#13,<g3=int64#9
# asm 2: adc <c3=%r15,<g3=%r11
adc %r15,%r11

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#10
# asm 2: mov  $0,>addt0=%r12
mov  $0,%r12

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#11
# asm 2: mov  $38,>addt1=%r13
mov  $38,%r13

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#10,<addt1=int64#11
# asm 2: cmovae <addt0=%r12,<addt1=%r13
cmovae %r12,%r13

# qhasm:   carry? g0 += addt1
# asm 1: add  <addt1=int64#11,<g0=int64#6
# asm 2: add  <addt1=%r13,<g0=%r9
add  %r13,%r9

# qhasm:   carry? g1 += addt0 + carry
# asm 1: adc <addt0=int64#10,<g1=int64#7
# asm 2: adc <addt0=%r12,<g1=%rax
adc %r12,%rax

# qhasm:   carry? g2 += addt0 + carry
# asm 1: adc <addt0=int64#10,<g2=int64#8
# asm 2: adc <addt0=%r12,<g2=%r10
adc %r12,%r10

# qhasm:   carry? g3 += addt0 + carry
# asm 1: adc <addt0=int64#10,<g3=int64#9
# asm 2: adc <addt0=%r12,<g3=%r11
adc %r12,%r11

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#11,<addt0=int64#10
# asm 2: cmovc <addt1=%r13,<addt0=%r12
cmovc %r13,%r12

# qhasm:   g0 += addt0
# asm 1: add  <addt0=int64#10,<g0=int64#6
# asm 2: add  <addt0=%r12,<g0=%r9
add  %r12,%r9

# qhasm: g0_stack = g0
# asm 1: movq <g0=int64#6,>g0_stack=stack64#16
# asm 2: movq <g0=%r9,>g0_stack=120(%rsp)
movq %r9,120(%rsp)

# qhasm: g1_stack = g1
# asm 1: movq <g1=int64#7,>g1_stack=stack64#17
# asm 2: movq <g1=%rax,>g1_stack=128(%rsp)
movq %rax,128(%rsp)

# qhasm: g2_stack = g2
# asm 1: movq <g2=int64#8,>g2_stack=stack64#18
# asm 2: movq <g2=%r10,>g2_stack=136(%rsp)
movq %r10,136(%rsp)

# qhasm: g3_stack = g3
# asm 1: movq <g3=int64#9,>g3_stack=stack64#19
# asm 2: movq <g3=%r11,>g3_stack=144(%rsp)
movq %r11,144(%rsp)

# qhasm: f0_stack = f0
# asm 1: movq <f0=int64#2,>f0_stack=stack64#20
# asm 2: movq <f0=%rsi,>f0_stack=152(%rsp)
movq %rsi,152(%rsp)

# qhasm: f1_stack = f1
# asm 1: movq <f1=int64#3,>f1_stack=stack64#21
# asm 2: movq <f1=%rdx,>f1_stack=160(%rsp)
movq %rdx,160(%rsp)

# qhasm: f2_stack = f2
# asm 1: movq <f2=int64#4,>f2_stack=stack64#22
# asm 2: movq <f2=%rcx,>f2_stack=168(%rsp)
movq %rcx,168(%rsp)

# qhasm: f3_stack = f3
# asm 1: movq <f3=int64#5,>f3_stack=stack64#23
# asm 2: movq <f3=%r8,>f3_stack=176(%rsp)
movq %r8,176(%rsp)

# qhasm:   mulr4 = 0
# asm 1: mov  $0,>mulr4=int64#2
# asm 2: mov  $0,>mulr4=%rsi
mov  $0,%rsi

# qhasm:   mulr5 = 0
# asm 1: mov  $0,>mulr5=int64#4
# asm 2: mov  $0,>mulr5=%rcx
mov  $0,%rcx

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#5
# asm 2: mov  $0,>mulr6=%r8
mov  $0,%r8

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#6
# asm 2: mov  $0,>mulr7=%r9
mov  $0,%r9

# qhasm:   mulx0 = e0_stack
# asm 1: movq <e0_stack=stack64#12,>mulx0=int64#8
# asm 2: movq <e0_stack=88(%rsp),>mulx0=%r10
movq 88(%rsp),%r10

# qhasm:   mulrax = f0_stack
# asm 1: movq <f0_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <f0_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   rx0 = mulrax
# asm 1: mov  <mulrax=int64#7,>rx0=int64#9
# asm 2: mov  <mulrax=%rax,>rx0=%r11
mov  %rax,%r11

# qhasm:   rx1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>rx1=int64#10
# asm 2: mov  <mulrdx=%rdx,>rx1=%r12
mov  %rdx,%r12

# qhasm:   mulrax = f1_stack
# asm 1: movq <f1_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <f1_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rx1 += mulrax
# asm 1: add  <mulrax=int64#7,<rx1=int64#10
# asm 2: add  <mulrax=%rax,<rx1=%r12
add  %rax,%r12

# qhasm:   rx2 = 0
# asm 1: mov  $0,>rx2=int64#11
# asm 2: mov  $0,>rx2=%r13
mov  $0,%r13

# qhasm:   rx2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rx2=int64#11
# asm 2: adc <mulrdx=%rdx,<rx2=%r13
adc %rdx,%r13

# qhasm:   mulrax = f2_stack
# asm 1: movq <f2_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <f2_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#11
# asm 2: add  <mulrax=%rax,<rx2=%r13
add  %rax,%r13

# qhasm:   rx3 = 0
# asm 1: mov  $0,>rx3=int64#12
# asm 2: mov  $0,>rx3=%r14
mov  $0,%r14

# qhasm:   rx3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rx3=int64#12
# asm 2: adc <mulrdx=%rdx,<rx3=%r14
adc %rdx,%r14

# qhasm:   mulrax = f3_stack
# asm 1: movq <f3_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <f3_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#12
# asm 2: add  <mulrax=%rax,<rx3=%r14
add  %rax,%r14

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#2
# asm 2: adc <mulrdx=%rdx,<mulr4=%rsi
adc %rdx,%rsi

# qhasm:   mulx1 = e1_stack
# asm 1: movq <e1_stack=stack64#13,>mulx1=int64#8
# asm 2: movq <e1_stack=96(%rsp),>mulx1=%r10
movq 96(%rsp),%r10

# qhasm:   mulrax = f0_stack
# asm 1: movq <f0_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <f0_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rx1 += mulrax
# asm 1: add  <mulrax=int64#7,<rx1=int64#10
# asm 2: add  <mulrax=%rax,<rx1=%r12
add  %rax,%r12

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f1_stack
# asm 1: movq <f1_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <f1_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#11
# asm 2: add  <mulrax=%rax,<rx2=%r13
add  %rax,%r13

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx2 += mulc
# asm 1: add  <mulc=int64#13,<rx2=int64#11
# asm 2: add  <mulc=%r15,<rx2=%r13
add  %r15,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f2_stack
# asm 1: movq <f2_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <f2_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rx3 += mulrax 
# asm 1: add  <mulrax=int64#7,<rx3=int64#12
# asm 2: add  <mulrax=%rax,<rx3=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx3 += mulc
# asm 1: add  <mulc=int64#13,<rx3=int64#12
# asm 2: add  <mulc=%r15,<rx3=%r14
add  %r15,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f3_stack
# asm 1: movq <f3_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <f3_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulr5 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr5=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr5=%rcx
adc %rdx,%rcx

# qhasm:   mulx2 = e2_stack
# asm 1: movq <e2_stack=stack64#14,>mulx2=int64#8
# asm 2: movq <e2_stack=104(%rsp),>mulx2=%r10
movq 104(%rsp),%r10

# qhasm:   mulrax = f0_stack
# asm 1: movq <f0_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <f0_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#11
# asm 2: add  <mulrax=%rax,<rx2=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f1_stack
# asm 1: movq <f1_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <f1_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#12
# asm 2: add  <mulrax=%rax,<rx3=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx3 += mulc
# asm 1: add  <mulc=int64#13,<rx3=int64#12
# asm 2: add  <mulc=%r15,<rx3=%r14
add  %r15,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f2_stack
# asm 1: movq <f2_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <f2_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f3_stack
# asm 1: movq <f3_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <f3_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#13,<mulr5=int64#4
# asm 2: add  <mulc=%r15,<mulr5=%rcx
add  %r15,%rcx

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr6=%r8
adc %rdx,%r8

# qhasm:   mulx3 = e3_stack
# asm 1: movq <e3_stack=stack64#15,>mulx3=int64#8
# asm 2: movq <e3_stack=112(%rsp),>mulx3=%r10
movq 112(%rsp),%r10

# qhasm:   mulrax = f0_stack
# asm 1: movq <f0_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <f0_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#12
# asm 2: add  <mulrax=%rax,<rx3=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f1_stack
# asm 1: movq <f1_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <f1_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f2_stack
# asm 1: movq <f2_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <f2_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#13,<mulr5=int64#4
# asm 2: add  <mulc=%r15,<mulr5=%rcx
add  %r15,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f3_stack
# asm 1: movq <f3_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <f3_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#5
# asm 2: add  <mulrax=%rax,<mulr6=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#13,<mulr6=int64#5
# asm 2: add  <mulc=%r15,<mulr6=%r8
add  %r15,%r8

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr7=%r9
adc %rdx,%r9

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#2,>mulrax=int64#7
# asm 2: mov  <mulr4=%rsi,>mulrax=%rax
mov  %rsi,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#2
# asm 2: mov  <mulrax=%rax,>mulr4=%rsi
mov  %rax,%rsi

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#4,>mulrax=int64#7
# asm 2: mov  <mulr5=%rcx,>mulrax=%rax
mov  %rcx,%rax

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
# asm 1: mov  <mulr6=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr6=%r8,>mulrax=%rax
mov  %r8,%rax

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
# asm 1: mov  <mulr7=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr7=%r9,>mulrax=%rax
mov  %r9,%rax

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

# qhasm:   carry? rx0 += mulr4
# asm 1: add  <mulr4=int64#2,<rx0=int64#9
# asm 2: add  <mulr4=%rsi,<rx0=%r11
add  %rsi,%r11

# qhasm:   carry? rx1 += mulr5 + carry
# asm 1: adc <mulr5=int64#4,<rx1=int64#10
# asm 2: adc <mulr5=%rcx,<rx1=%r12
adc %rcx,%r12

# qhasm:   carry? rx2 += mulr6 + carry
# asm 1: adc <mulr6=int64#5,<rx2=int64#11
# asm 2: adc <mulr6=%r8,<rx2=%r13
adc %r8,%r13

# qhasm:   carry? rx3 += mulr7 + carry
# asm 1: adc <mulr7=int64#6,<rx3=int64#12
# asm 2: adc <mulr7=%r9,<rx3=%r14
adc %r9,%r14

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

# qhasm:   carry? rx0 += mulr8
# asm 1: add  <mulr8=int64#3,<rx0=int64#9
# asm 2: add  <mulr8=%rdx,<rx0=%r11
add  %rdx,%r11

# qhasm:   carry? rx1 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rx1=int64#10
# asm 2: adc <mulzero=%rsi,<rx1=%r12
adc %rsi,%r12

# qhasm:   carry? rx2 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rx2=int64#11
# asm 2: adc <mulzero=%rsi,<rx2=%r13
adc %rsi,%r13

# qhasm:   carry? rx3 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rx3=int64#12
# asm 2: adc <mulzero=%rsi,<rx3=%r14
adc %rsi,%r14

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulzero=int64#2
# asm 2: adc <mulzero=%rsi,<mulzero=%rsi
adc %rsi,%rsi

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#2,>mulzero=int64#2
# asm 2: imulq  $38,<mulzero=%rsi,>mulzero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   rx0 += mulzero
# asm 1: add  <mulzero=int64#2,<rx0=int64#9
# asm 2: add  <mulzero=%rsi,<rx0=%r11
add  %rsi,%r11

# qhasm: *(uint64 *)(rp + 0) = rx0
# asm 1: movq   <rx0=int64#9,0(<rp=int64#1)
# asm 2: movq   <rx0=%r11,0(<rp=%rdi)
movq   %r11,0(%rdi)

# qhasm: *(uint64 *)(rp + 8) = rx1
# asm 1: movq   <rx1=int64#10,8(<rp=int64#1)
# asm 2: movq   <rx1=%r12,8(<rp=%rdi)
movq   %r12,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = rx2
# asm 1: movq   <rx2=int64#11,16(<rp=int64#1)
# asm 2: movq   <rx2=%r13,16(<rp=%rdi)
movq   %r13,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = rx3
# asm 1: movq   <rx3=int64#12,24(<rp=int64#1)
# asm 2: movq   <rx3=%r14,24(<rp=%rdi)
movq   %r14,24(%rdi)

# qhasm:   mulr4 = 0
# asm 1: mov  $0,>mulr4=int64#2
# asm 2: mov  $0,>mulr4=%rsi
mov  $0,%rsi

# qhasm:   mulr5 = 0
# asm 1: mov  $0,>mulr5=int64#4
# asm 2: mov  $0,>mulr5=%rcx
mov  $0,%rcx

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#5
# asm 2: mov  $0,>mulr6=%r8
mov  $0,%r8

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#6
# asm 2: mov  $0,>mulr7=%r9
mov  $0,%r9

# qhasm:   mulx0 = h0_stack
# asm 1: movq <h0_stack=stack64#8,>mulx0=int64#8
# asm 2: movq <h0_stack=56(%rsp),>mulx0=%r10
movq 56(%rsp),%r10

# qhasm:   mulrax = g0_stack
# asm 1: movq <g0_stack=stack64#16,>mulrax=int64#7
# asm 2: movq <g0_stack=120(%rsp),>mulrax=%rax
movq 120(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   ry0 = mulrax
# asm 1: mov  <mulrax=int64#7,>ry0=int64#9
# asm 2: mov  <mulrax=%rax,>ry0=%r11
mov  %rax,%r11

# qhasm:   ry1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>ry1=int64#10
# asm 2: mov  <mulrdx=%rdx,>ry1=%r12
mov  %rdx,%r12

# qhasm:   mulrax = g1_stack
# asm 1: movq <g1_stack=stack64#17,>mulrax=int64#7
# asm 2: movq <g1_stack=128(%rsp),>mulrax=%rax
movq 128(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? ry1 += mulrax
# asm 1: add  <mulrax=int64#7,<ry1=int64#10
# asm 2: add  <mulrax=%rax,<ry1=%r12
add  %rax,%r12

# qhasm:   ry2 = 0
# asm 1: mov  $0,>ry2=int64#11
# asm 2: mov  $0,>ry2=%r13
mov  $0,%r13

# qhasm:   ry2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<ry2=int64#11
# asm 2: adc <mulrdx=%rdx,<ry2=%r13
adc %rdx,%r13

# qhasm:   mulrax = g2_stack
# asm 1: movq <g2_stack=stack64#18,>mulrax=int64#7
# asm 2: movq <g2_stack=136(%rsp),>mulrax=%rax
movq 136(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? ry2 += mulrax
# asm 1: add  <mulrax=int64#7,<ry2=int64#11
# asm 2: add  <mulrax=%rax,<ry2=%r13
add  %rax,%r13

# qhasm:   ry3 = 0
# asm 1: mov  $0,>ry3=int64#12
# asm 2: mov  $0,>ry3=%r14
mov  $0,%r14

# qhasm:   ry3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<ry3=int64#12
# asm 2: adc <mulrdx=%rdx,<ry3=%r14
adc %rdx,%r14

# qhasm:   mulrax = g3_stack
# asm 1: movq <g3_stack=stack64#19,>mulrax=int64#7
# asm 2: movq <g3_stack=144(%rsp),>mulrax=%rax
movq 144(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? ry3 += mulrax
# asm 1: add  <mulrax=int64#7,<ry3=int64#12
# asm 2: add  <mulrax=%rax,<ry3=%r14
add  %rax,%r14

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#2
# asm 2: adc <mulrdx=%rdx,<mulr4=%rsi
adc %rdx,%rsi

# qhasm:   mulx1 = h1_stack
# asm 1: movq <h1_stack=stack64#9,>mulx1=int64#8
# asm 2: movq <h1_stack=64(%rsp),>mulx1=%r10
movq 64(%rsp),%r10

# qhasm:   mulrax = g0_stack
# asm 1: movq <g0_stack=stack64#16,>mulrax=int64#7
# asm 2: movq <g0_stack=120(%rsp),>mulrax=%rax
movq 120(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? ry1 += mulrax
# asm 1: add  <mulrax=int64#7,<ry1=int64#10
# asm 2: add  <mulrax=%rax,<ry1=%r12
add  %rax,%r12

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g1_stack
# asm 1: movq <g1_stack=stack64#17,>mulrax=int64#7
# asm 2: movq <g1_stack=128(%rsp),>mulrax=%rax
movq 128(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? ry2 += mulrax
# asm 1: add  <mulrax=int64#7,<ry2=int64#11
# asm 2: add  <mulrax=%rax,<ry2=%r13
add  %rax,%r13

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? ry2 += mulc
# asm 1: add  <mulc=int64#13,<ry2=int64#11
# asm 2: add  <mulc=%r15,<ry2=%r13
add  %r15,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g2_stack
# asm 1: movq <g2_stack=stack64#18,>mulrax=int64#7
# asm 2: movq <g2_stack=136(%rsp),>mulrax=%rax
movq 136(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? ry3 += mulrax 
# asm 1: add  <mulrax=int64#7,<ry3=int64#12
# asm 2: add  <mulrax=%rax,<ry3=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? ry3 += mulc
# asm 1: add  <mulc=int64#13,<ry3=int64#12
# asm 2: add  <mulc=%r15,<ry3=%r14
add  %r15,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g3_stack
# asm 1: movq <g3_stack=stack64#19,>mulrax=int64#7
# asm 2: movq <g3_stack=144(%rsp),>mulrax=%rax
movq 144(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulr5 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr5=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr5=%rcx
adc %rdx,%rcx

# qhasm:   mulx2 = h2_stack
# asm 1: movq <h2_stack=stack64#10,>mulx2=int64#8
# asm 2: movq <h2_stack=72(%rsp),>mulx2=%r10
movq 72(%rsp),%r10

# qhasm:   mulrax = g0_stack
# asm 1: movq <g0_stack=stack64#16,>mulrax=int64#7
# asm 2: movq <g0_stack=120(%rsp),>mulrax=%rax
movq 120(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? ry2 += mulrax
# asm 1: add  <mulrax=int64#7,<ry2=int64#11
# asm 2: add  <mulrax=%rax,<ry2=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g1_stack
# asm 1: movq <g1_stack=stack64#17,>mulrax=int64#7
# asm 2: movq <g1_stack=128(%rsp),>mulrax=%rax
movq 128(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? ry3 += mulrax
# asm 1: add  <mulrax=int64#7,<ry3=int64#12
# asm 2: add  <mulrax=%rax,<ry3=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? ry3 += mulc
# asm 1: add  <mulc=int64#13,<ry3=int64#12
# asm 2: add  <mulc=%r15,<ry3=%r14
add  %r15,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g2_stack
# asm 1: movq <g2_stack=stack64#18,>mulrax=int64#7
# asm 2: movq <g2_stack=136(%rsp),>mulrax=%rax
movq 136(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g3_stack
# asm 1: movq <g3_stack=stack64#19,>mulrax=int64#7
# asm 2: movq <g3_stack=144(%rsp),>mulrax=%rax
movq 144(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#13,<mulr5=int64#4
# asm 2: add  <mulc=%r15,<mulr5=%rcx
add  %r15,%rcx

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr6=%r8
adc %rdx,%r8

# qhasm:   mulx3 = h3_stack
# asm 1: movq <h3_stack=stack64#11,>mulx3=int64#8
# asm 2: movq <h3_stack=80(%rsp),>mulx3=%r10
movq 80(%rsp),%r10

# qhasm:   mulrax = g0_stack
# asm 1: movq <g0_stack=stack64#16,>mulrax=int64#7
# asm 2: movq <g0_stack=120(%rsp),>mulrax=%rax
movq 120(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? ry3 += mulrax
# asm 1: add  <mulrax=int64#7,<ry3=int64#12
# asm 2: add  <mulrax=%rax,<ry3=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g1_stack
# asm 1: movq <g1_stack=stack64#17,>mulrax=int64#7
# asm 2: movq <g1_stack=128(%rsp),>mulrax=%rax
movq 128(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g2_stack
# asm 1: movq <g2_stack=stack64#18,>mulrax=int64#7
# asm 2: movq <g2_stack=136(%rsp),>mulrax=%rax
movq 136(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#13,<mulr5=int64#4
# asm 2: add  <mulc=%r15,<mulr5=%rcx
add  %r15,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = g3_stack
# asm 1: movq <g3_stack=stack64#19,>mulrax=int64#7
# asm 2: movq <g3_stack=144(%rsp),>mulrax=%rax
movq 144(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#5
# asm 2: add  <mulrax=%rax,<mulr6=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#13,<mulr6=int64#5
# asm 2: add  <mulc=%r15,<mulr6=%r8
add  %r15,%r8

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr7=%r9
adc %rdx,%r9

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#2,>mulrax=int64#7
# asm 2: mov  <mulr4=%rsi,>mulrax=%rax
mov  %rsi,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#2
# asm 2: mov  <mulrax=%rax,>mulr4=%rsi
mov  %rax,%rsi

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#4,>mulrax=int64#7
# asm 2: mov  <mulr5=%rcx,>mulrax=%rax
mov  %rcx,%rax

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
# asm 1: mov  <mulr6=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr6=%r8,>mulrax=%rax
mov  %r8,%rax

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
# asm 1: mov  <mulr7=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr7=%r9,>mulrax=%rax
mov  %r9,%rax

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

# qhasm:   carry? ry0 += mulr4
# asm 1: add  <mulr4=int64#2,<ry0=int64#9
# asm 2: add  <mulr4=%rsi,<ry0=%r11
add  %rsi,%r11

# qhasm:   carry? ry1 += mulr5 + carry
# asm 1: adc <mulr5=int64#4,<ry1=int64#10
# asm 2: adc <mulr5=%rcx,<ry1=%r12
adc %rcx,%r12

# qhasm:   carry? ry2 += mulr6 + carry
# asm 1: adc <mulr6=int64#5,<ry2=int64#11
# asm 2: adc <mulr6=%r8,<ry2=%r13
adc %r8,%r13

# qhasm:   carry? ry3 += mulr7 + carry
# asm 1: adc <mulr7=int64#6,<ry3=int64#12
# asm 2: adc <mulr7=%r9,<ry3=%r14
adc %r9,%r14

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

# qhasm:   carry? ry0 += mulr8
# asm 1: add  <mulr8=int64#3,<ry0=int64#9
# asm 2: add  <mulr8=%rdx,<ry0=%r11
add  %rdx,%r11

# qhasm:   carry? ry1 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<ry1=int64#10
# asm 2: adc <mulzero=%rsi,<ry1=%r12
adc %rsi,%r12

# qhasm:   carry? ry2 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<ry2=int64#11
# asm 2: adc <mulzero=%rsi,<ry2=%r13
adc %rsi,%r13

# qhasm:   carry? ry3 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<ry3=int64#12
# asm 2: adc <mulzero=%rsi,<ry3=%r14
adc %rsi,%r14

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulzero=int64#2
# asm 2: adc <mulzero=%rsi,<mulzero=%rsi
adc %rsi,%rsi

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#2,>mulzero=int64#2
# asm 2: imulq  $38,<mulzero=%rsi,>mulzero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   ry0 += mulzero
# asm 1: add  <mulzero=int64#2,<ry0=int64#9
# asm 2: add  <mulzero=%rsi,<ry0=%r11
add  %rsi,%r11

# qhasm: *(uint64 *)(rp + 32) = ry0
# asm 1: movq   <ry0=int64#9,32(<rp=int64#1)
# asm 2: movq   <ry0=%r11,32(<rp=%rdi)
movq   %r11,32(%rdi)

# qhasm: *(uint64 *)(rp + 40) = ry1
# asm 1: movq   <ry1=int64#10,40(<rp=int64#1)
# asm 2: movq   <ry1=%r12,40(<rp=%rdi)
movq   %r12,40(%rdi)

# qhasm: *(uint64 *)(rp + 48) = ry2
# asm 1: movq   <ry2=int64#11,48(<rp=int64#1)
# asm 2: movq   <ry2=%r13,48(<rp=%rdi)
movq   %r13,48(%rdi)

# qhasm: *(uint64 *)(rp + 56) = ry3
# asm 1: movq   <ry3=int64#12,56(<rp=int64#1)
# asm 2: movq   <ry3=%r14,56(<rp=%rdi)
movq   %r14,56(%rdi)

# qhasm:   mulr4 = 0
# asm 1: mov  $0,>mulr4=int64#2
# asm 2: mov  $0,>mulr4=%rsi
mov  $0,%rsi

# qhasm:   mulr5 = 0
# asm 1: mov  $0,>mulr5=int64#4
# asm 2: mov  $0,>mulr5=%rcx
mov  $0,%rcx

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#5
# asm 2: mov  $0,>mulr6=%r8
mov  $0,%r8

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#6
# asm 2: mov  $0,>mulr7=%r9
mov  $0,%r9

# qhasm:   mulx0 = g0_stack
# asm 1: movq <g0_stack=stack64#16,>mulx0=int64#8
# asm 2: movq <g0_stack=120(%rsp),>mulx0=%r10
movq 120(%rsp),%r10

# qhasm:   mulrax = f0_stack
# asm 1: movq <f0_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <f0_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   rz0 = mulrax
# asm 1: mov  <mulrax=int64#7,>rz0=int64#9
# asm 2: mov  <mulrax=%rax,>rz0=%r11
mov  %rax,%r11

# qhasm:   rz1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>rz1=int64#10
# asm 2: mov  <mulrdx=%rdx,>rz1=%r12
mov  %rdx,%r12

# qhasm:   mulrax = f1_stack
# asm 1: movq <f1_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <f1_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rz1 += mulrax
# asm 1: add  <mulrax=int64#7,<rz1=int64#10
# asm 2: add  <mulrax=%rax,<rz1=%r12
add  %rax,%r12

# qhasm:   rz2 = 0
# asm 1: mov  $0,>rz2=int64#11
# asm 2: mov  $0,>rz2=%r13
mov  $0,%r13

# qhasm:   rz2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rz2=int64#11
# asm 2: adc <mulrdx=%rdx,<rz2=%r13
adc %rdx,%r13

# qhasm:   mulrax = f2_stack
# asm 1: movq <f2_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <f2_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rz2 += mulrax
# asm 1: add  <mulrax=int64#7,<rz2=int64#11
# asm 2: add  <mulrax=%rax,<rz2=%r13
add  %rax,%r13

# qhasm:   rz3 = 0
# asm 1: mov  $0,>rz3=int64#12
# asm 2: mov  $0,>rz3=%r14
mov  $0,%r14

# qhasm:   rz3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rz3=int64#12
# asm 2: adc <mulrdx=%rdx,<rz3=%r14
adc %rdx,%r14

# qhasm:   mulrax = f3_stack
# asm 1: movq <f3_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <f3_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rz3 += mulrax
# asm 1: add  <mulrax=int64#7,<rz3=int64#12
# asm 2: add  <mulrax=%rax,<rz3=%r14
add  %rax,%r14

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#2
# asm 2: adc <mulrdx=%rdx,<mulr4=%rsi
adc %rdx,%rsi

# qhasm:   mulx1 = g1_stack
# asm 1: movq <g1_stack=stack64#17,>mulx1=int64#8
# asm 2: movq <g1_stack=128(%rsp),>mulx1=%r10
movq 128(%rsp),%r10

# qhasm:   mulrax = f0_stack
# asm 1: movq <f0_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <f0_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rz1 += mulrax
# asm 1: add  <mulrax=int64#7,<rz1=int64#10
# asm 2: add  <mulrax=%rax,<rz1=%r12
add  %rax,%r12

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f1_stack
# asm 1: movq <f1_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <f1_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rz2 += mulrax
# asm 1: add  <mulrax=int64#7,<rz2=int64#11
# asm 2: add  <mulrax=%rax,<rz2=%r13
add  %rax,%r13

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rz2 += mulc
# asm 1: add  <mulc=int64#13,<rz2=int64#11
# asm 2: add  <mulc=%r15,<rz2=%r13
add  %r15,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f2_stack
# asm 1: movq <f2_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <f2_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rz3 += mulrax 
# asm 1: add  <mulrax=int64#7,<rz3=int64#12
# asm 2: add  <mulrax=%rax,<rz3=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rz3 += mulc
# asm 1: add  <mulc=int64#13,<rz3=int64#12
# asm 2: add  <mulc=%r15,<rz3=%r14
add  %r15,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f3_stack
# asm 1: movq <f3_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <f3_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulr5 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr5=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr5=%rcx
adc %rdx,%rcx

# qhasm:   mulx2 = g2_stack
# asm 1: movq <g2_stack=stack64#18,>mulx2=int64#8
# asm 2: movq <g2_stack=136(%rsp),>mulx2=%r10
movq 136(%rsp),%r10

# qhasm:   mulrax = f0_stack
# asm 1: movq <f0_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <f0_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? rz2 += mulrax
# asm 1: add  <mulrax=int64#7,<rz2=int64#11
# asm 2: add  <mulrax=%rax,<rz2=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f1_stack
# asm 1: movq <f1_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <f1_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? rz3 += mulrax
# asm 1: add  <mulrax=int64#7,<rz3=int64#12
# asm 2: add  <mulrax=%rax,<rz3=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rz3 += mulc
# asm 1: add  <mulc=int64#13,<rz3=int64#12
# asm 2: add  <mulc=%r15,<rz3=%r14
add  %r15,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f2_stack
# asm 1: movq <f2_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <f2_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f3_stack
# asm 1: movq <f3_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <f3_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#13,<mulr5=int64#4
# asm 2: add  <mulc=%r15,<mulr5=%rcx
add  %r15,%rcx

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr6=%r8
adc %rdx,%r8

# qhasm:   mulx3 = g3_stack
# asm 1: movq <g3_stack=stack64#19,>mulx3=int64#8
# asm 2: movq <g3_stack=144(%rsp),>mulx3=%r10
movq 144(%rsp),%r10

# qhasm:   mulrax = f0_stack
# asm 1: movq <f0_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <f0_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? rz3 += mulrax
# asm 1: add  <mulrax=int64#7,<rz3=int64#12
# asm 2: add  <mulrax=%rax,<rz3=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f1_stack
# asm 1: movq <f1_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <f1_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f2_stack
# asm 1: movq <f2_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <f2_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#13,<mulr5=int64#4
# asm 2: add  <mulc=%r15,<mulr5=%rcx
add  %r15,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = f3_stack
# asm 1: movq <f3_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <f3_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#5
# asm 2: add  <mulrax=%rax,<mulr6=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#13,<mulr6=int64#5
# asm 2: add  <mulc=%r15,<mulr6=%r8
add  %r15,%r8

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr7=%r9
adc %rdx,%r9

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#2,>mulrax=int64#7
# asm 2: mov  <mulr4=%rsi,>mulrax=%rax
mov  %rsi,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#2
# asm 2: mov  <mulrax=%rax,>mulr4=%rsi
mov  %rax,%rsi

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#4,>mulrax=int64#7
# asm 2: mov  <mulr5=%rcx,>mulrax=%rax
mov  %rcx,%rax

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
# asm 1: mov  <mulr6=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr6=%r8,>mulrax=%rax
mov  %r8,%rax

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
# asm 1: mov  <mulr7=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr7=%r9,>mulrax=%rax
mov  %r9,%rax

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
# asm 1: add  <mulr4=int64#2,<rz0=int64#9
# asm 2: add  <mulr4=%rsi,<rz0=%r11
add  %rsi,%r11

# qhasm:   carry? rz1 += mulr5 + carry
# asm 1: adc <mulr5=int64#4,<rz1=int64#10
# asm 2: adc <mulr5=%rcx,<rz1=%r12
adc %rcx,%r12

# qhasm:   carry? rz2 += mulr6 + carry
# asm 1: adc <mulr6=int64#5,<rz2=int64#11
# asm 2: adc <mulr6=%r8,<rz2=%r13
adc %r8,%r13

# qhasm:   carry? rz3 += mulr7 + carry
# asm 1: adc <mulr7=int64#6,<rz3=int64#12
# asm 2: adc <mulr7=%r9,<rz3=%r14
adc %r9,%r14

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
# asm 1: add  <mulr8=int64#3,<rz0=int64#9
# asm 2: add  <mulr8=%rdx,<rz0=%r11
add  %rdx,%r11

# qhasm:   carry? rz1 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rz1=int64#10
# asm 2: adc <mulzero=%rsi,<rz1=%r12
adc %rsi,%r12

# qhasm:   carry? rz2 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rz2=int64#11
# asm 2: adc <mulzero=%rsi,<rz2=%r13
adc %rsi,%r13

# qhasm:   carry? rz3 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rz3=int64#12
# asm 2: adc <mulzero=%rsi,<rz3=%r14
adc %rsi,%r14

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulzero=int64#2
# asm 2: adc <mulzero=%rsi,<mulzero=%rsi
adc %rsi,%rsi

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#2,>mulzero=int64#2
# asm 2: imulq  $38,<mulzero=%rsi,>mulzero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   rz0 += mulzero
# asm 1: add  <mulzero=int64#2,<rz0=int64#9
# asm 2: add  <mulzero=%rsi,<rz0=%r11
add  %rsi,%r11

# qhasm: *(uint64 *)(rp + 64) = rz0
# asm 1: movq   <rz0=int64#9,64(<rp=int64#1)
# asm 2: movq   <rz0=%r11,64(<rp=%rdi)
movq   %r11,64(%rdi)

# qhasm: *(uint64 *)(rp + 72) = rz1
# asm 1: movq   <rz1=int64#10,72(<rp=int64#1)
# asm 2: movq   <rz1=%r12,72(<rp=%rdi)
movq   %r12,72(%rdi)

# qhasm: *(uint64 *)(rp + 80) = rz2
# asm 1: movq   <rz2=int64#11,80(<rp=int64#1)
# asm 2: movq   <rz2=%r13,80(<rp=%rdi)
movq   %r13,80(%rdi)

# qhasm: *(uint64 *)(rp + 88) = rz3
# asm 1: movq   <rz3=int64#12,88(<rp=int64#1)
# asm 2: movq   <rz3=%r14,88(<rp=%rdi)
movq   %r14,88(%rdi)

# qhasm:   mulr4 = 0
# asm 1: mov  $0,>mulr4=int64#2
# asm 2: mov  $0,>mulr4=%rsi
mov  $0,%rsi

# qhasm:   mulr5 = 0
# asm 1: mov  $0,>mulr5=int64#4
# asm 2: mov  $0,>mulr5=%rcx
mov  $0,%rcx

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#5
# asm 2: mov  $0,>mulr6=%r8
mov  $0,%r8

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#6
# asm 2: mov  $0,>mulr7=%r9
mov  $0,%r9

# qhasm:   mulx0 = e0_stack
# asm 1: movq <e0_stack=stack64#12,>mulx0=int64#8
# asm 2: movq <e0_stack=88(%rsp),>mulx0=%r10
movq 88(%rsp),%r10

# qhasm:   mulrax = h0_stack
# asm 1: movq <h0_stack=stack64#8,>mulrax=int64#7
# asm 2: movq <h0_stack=56(%rsp),>mulrax=%rax
movq 56(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   rt0 = mulrax
# asm 1: mov  <mulrax=int64#7,>rt0=int64#9
# asm 2: mov  <mulrax=%rax,>rt0=%r11
mov  %rax,%r11

# qhasm:   rt1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>rt1=int64#10
# asm 2: mov  <mulrdx=%rdx,>rt1=%r12
mov  %rdx,%r12

# qhasm:   mulrax = h1_stack
# asm 1: movq <h1_stack=stack64#9,>mulrax=int64#7
# asm 2: movq <h1_stack=64(%rsp),>mulrax=%rax
movq 64(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rt1 += mulrax
# asm 1: add  <mulrax=int64#7,<rt1=int64#10
# asm 2: add  <mulrax=%rax,<rt1=%r12
add  %rax,%r12

# qhasm:   rt2 = 0
# asm 1: mov  $0,>rt2=int64#11
# asm 2: mov  $0,>rt2=%r13
mov  $0,%r13

# qhasm:   rt2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rt2=int64#11
# asm 2: adc <mulrdx=%rdx,<rt2=%r13
adc %rdx,%r13

# qhasm:   mulrax = h2_stack
# asm 1: movq <h2_stack=stack64#10,>mulrax=int64#7
# asm 2: movq <h2_stack=72(%rsp),>mulrax=%rax
movq 72(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rt2 += mulrax
# asm 1: add  <mulrax=int64#7,<rt2=int64#11
# asm 2: add  <mulrax=%rax,<rt2=%r13
add  %rax,%r13

# qhasm:   rt3 = 0
# asm 1: mov  $0,>rt3=int64#12
# asm 2: mov  $0,>rt3=%r14
mov  $0,%r14

# qhasm:   rt3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rt3=int64#12
# asm 2: adc <mulrdx=%rdx,<rt3=%r14
adc %rdx,%r14

# qhasm:   mulrax = h3_stack
# asm 1: movq <h3_stack=stack64#11,>mulrax=int64#7
# asm 2: movq <h3_stack=80(%rsp),>mulrax=%rax
movq 80(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#8
# asm 2: mul  <mulx0=%r10
mul  %r10

# qhasm:   carry? rt3 += mulrax
# asm 1: add  <mulrax=int64#7,<rt3=int64#12
# asm 2: add  <mulrax=%rax,<rt3=%r14
add  %rax,%r14

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#2
# asm 2: adc <mulrdx=%rdx,<mulr4=%rsi
adc %rdx,%rsi

# qhasm:   mulx1 = e1_stack
# asm 1: movq <e1_stack=stack64#13,>mulx1=int64#8
# asm 2: movq <e1_stack=96(%rsp),>mulx1=%r10
movq 96(%rsp),%r10

# qhasm:   mulrax = h0_stack
# asm 1: movq <h0_stack=stack64#8,>mulrax=int64#7
# asm 2: movq <h0_stack=56(%rsp),>mulrax=%rax
movq 56(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rt1 += mulrax
# asm 1: add  <mulrax=int64#7,<rt1=int64#10
# asm 2: add  <mulrax=%rax,<rt1=%r12
add  %rax,%r12

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h1_stack
# asm 1: movq <h1_stack=stack64#9,>mulrax=int64#7
# asm 2: movq <h1_stack=64(%rsp),>mulrax=%rax
movq 64(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rt2 += mulrax
# asm 1: add  <mulrax=int64#7,<rt2=int64#11
# asm 2: add  <mulrax=%rax,<rt2=%r13
add  %rax,%r13

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rt2 += mulc
# asm 1: add  <mulc=int64#13,<rt2=int64#11
# asm 2: add  <mulc=%r15,<rt2=%r13
add  %r15,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h2_stack
# asm 1: movq <h2_stack=stack64#10,>mulrax=int64#7
# asm 2: movq <h2_stack=72(%rsp),>mulrax=%rax
movq 72(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? rt3 += mulrax 
# asm 1: add  <mulrax=int64#7,<rt3=int64#12
# asm 2: add  <mulrax=%rax,<rt3=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rt3 += mulc
# asm 1: add  <mulc=int64#13,<rt3=int64#12
# asm 2: add  <mulc=%r15,<rt3=%r14
add  %r15,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h3_stack
# asm 1: movq <h3_stack=stack64#11,>mulrax=int64#7
# asm 2: movq <h3_stack=80(%rsp),>mulrax=%rax
movq 80(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#8
# asm 2: mul  <mulx1=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulr5 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr5=int64#4
# asm 2: adc <mulrdx=%rdx,<mulr5=%rcx
adc %rdx,%rcx

# qhasm:   mulx2 = e2_stack
# asm 1: movq <e2_stack=stack64#14,>mulx2=int64#8
# asm 2: movq <e2_stack=104(%rsp),>mulx2=%r10
movq 104(%rsp),%r10

# qhasm:   mulrax = h0_stack
# asm 1: movq <h0_stack=stack64#8,>mulrax=int64#7
# asm 2: movq <h0_stack=56(%rsp),>mulrax=%rax
movq 56(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? rt2 += mulrax
# asm 1: add  <mulrax=int64#7,<rt2=int64#11
# asm 2: add  <mulrax=%rax,<rt2=%r13
add  %rax,%r13

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h1_stack
# asm 1: movq <h1_stack=stack64#9,>mulrax=int64#7
# asm 2: movq <h1_stack=64(%rsp),>mulrax=%rax
movq 64(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? rt3 += mulrax
# asm 1: add  <mulrax=int64#7,<rt3=int64#12
# asm 2: add  <mulrax=%rax,<rt3=%r14
add  %rax,%r14

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rt3 += mulc
# asm 1: add  <mulc=int64#13,<rt3=int64#12
# asm 2: add  <mulc=%r15,<rt3=%r14
add  %r15,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h2_stack
# asm 1: movq <h2_stack=stack64#10,>mulrax=int64#7
# asm 2: movq <h2_stack=72(%rsp),>mulrax=%rax
movq 72(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h3_stack
# asm 1: movq <h3_stack=stack64#11,>mulrax=int64#7
# asm 2: movq <h3_stack=80(%rsp),>mulrax=%rax
movq 80(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#8
# asm 2: mul  <mulx2=%r10
mul  %r10

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#13,<mulr5=int64#4
# asm 2: add  <mulc=%r15,<mulr5=%rcx
add  %r15,%rcx

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr6=%r8
adc %rdx,%r8

# qhasm:   mulx3 = e3_stack
# asm 1: movq <e3_stack=stack64#15,>mulx3=int64#8
# asm 2: movq <e3_stack=112(%rsp),>mulx3=%r10
movq 112(%rsp),%r10

# qhasm:   mulrax = h0_stack
# asm 1: movq <h0_stack=stack64#8,>mulrax=int64#7
# asm 2: movq <h0_stack=56(%rsp),>mulrax=%rax
movq 56(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? rt3 += mulrax
# asm 1: add  <mulrax=int64#7,<rt3=int64#12
# asm 2: add  <mulrax=%rax,<rt3=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h1_stack
# asm 1: movq <h1_stack=stack64#9,>mulrax=int64#7
# asm 2: movq <h1_stack=64(%rsp),>mulrax=%rax
movq 64(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr4 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr4=int64#2
# asm 2: add  <mulrax=%rax,<mulr4=%rsi
add  %rax,%rsi

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr4 += mulc
# asm 1: add  <mulc=int64#13,<mulr4=int64#2
# asm 2: add  <mulc=%r15,<mulr4=%rsi
add  %r15,%rsi

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h2_stack
# asm 1: movq <h2_stack=stack64#10,>mulrax=int64#7
# asm 2: movq <h2_stack=72(%rsp),>mulrax=%rax
movq 72(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#4
# asm 2: add  <mulrax=%rax,<mulr5=%rcx
add  %rax,%rcx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr5 += mulc
# asm 1: add  <mulc=int64#13,<mulr5=int64#4
# asm 2: add  <mulc=%r15,<mulr5=%rcx
add  %r15,%rcx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#13
# asm 2: mov  $0,>mulc=%r15
mov  $0,%r15

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#13
# asm 2: adc <mulrdx=%rdx,<mulc=%r15
adc %rdx,%r15

# qhasm:   mulrax = h3_stack
# asm 1: movq <h3_stack=stack64#11,>mulrax=int64#7
# asm 2: movq <h3_stack=80(%rsp),>mulrax=%rax
movq 80(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#8
# asm 2: mul  <mulx3=%r10
mul  %r10

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#5
# asm 2: add  <mulrax=%rax,<mulr6=%r8
add  %rax,%r8

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#13,<mulr6=int64#5
# asm 2: add  <mulc=%r15,<mulr6=%r8
add  %r15,%r8

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#6
# asm 2: adc <mulrdx=%rdx,<mulr7=%r9
adc %rdx,%r9

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#2,>mulrax=int64#7
# asm 2: mov  <mulr4=%rsi,>mulrax=%rax
mov  %rsi,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#2
# asm 2: mov  <mulrax=%rax,>mulr4=%rsi
mov  %rax,%rsi

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#4,>mulrax=int64#7
# asm 2: mov  <mulr5=%rcx,>mulrax=%rax
mov  %rcx,%rax

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
# asm 1: mov  <mulr6=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr6=%r8,>mulrax=%rax
mov  %r8,%rax

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
# asm 1: mov  <mulr7=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr7=%r9,>mulrax=%rax
mov  %r9,%rax

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

# qhasm:   carry? rt0 += mulr4
# asm 1: add  <mulr4=int64#2,<rt0=int64#9
# asm 2: add  <mulr4=%rsi,<rt0=%r11
add  %rsi,%r11

# qhasm:   carry? rt1 += mulr5 + carry
# asm 1: adc <mulr5=int64#4,<rt1=int64#10
# asm 2: adc <mulr5=%rcx,<rt1=%r12
adc %rcx,%r12

# qhasm:   carry? rt2 += mulr6 + carry
# asm 1: adc <mulr6=int64#5,<rt2=int64#11
# asm 2: adc <mulr6=%r8,<rt2=%r13
adc %r8,%r13

# qhasm:   carry? rt3 += mulr7 + carry
# asm 1: adc <mulr7=int64#6,<rt3=int64#12
# asm 2: adc <mulr7=%r9,<rt3=%r14
adc %r9,%r14

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

# qhasm:   carry? rt0 += mulr8
# asm 1: add  <mulr8=int64#3,<rt0=int64#9
# asm 2: add  <mulr8=%rdx,<rt0=%r11
add  %rdx,%r11

# qhasm:   carry? rt1 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rt1=int64#10
# asm 2: adc <mulzero=%rsi,<rt1=%r12
adc %rsi,%r12

# qhasm:   carry? rt2 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rt2=int64#11
# asm 2: adc <mulzero=%rsi,<rt2=%r13
adc %rsi,%r13

# qhasm:   carry? rt3 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rt3=int64#12
# asm 2: adc <mulzero=%rsi,<rt3=%r14
adc %rsi,%r14

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulzero=int64#2
# asm 2: adc <mulzero=%rsi,<mulzero=%rsi
adc %rsi,%rsi

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#2,>mulzero=int64#2
# asm 2: imulq  $38,<mulzero=%rsi,>mulzero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   rt0 += mulzero
# asm 1: add  <mulzero=int64#2,<rt0=int64#9
# asm 2: add  <mulzero=%rsi,<rt0=%r11
add  %rsi,%r11

# qhasm: *(uint64 *)(rp + 96) = rt0
# asm 1: movq   <rt0=int64#9,96(<rp=int64#1)
# asm 2: movq   <rt0=%r11,96(<rp=%rdi)
movq   %r11,96(%rdi)

# qhasm: *(uint64 *)(rp + 104) = rt1
# asm 1: movq   <rt1=int64#10,104(<rp=int64#1)
# asm 2: movq   <rt1=%r12,104(<rp=%rdi)
movq   %r12,104(%rdi)

# qhasm: *(uint64 *)(rp + 112) = rt2
# asm 1: movq   <rt2=int64#11,112(<rp=int64#1)
# asm 2: movq   <rt2=%r13,112(<rp=%rdi)
movq   %r13,112(%rdi)

# qhasm: *(uint64 *)(rp + 120) = rt3
# asm 1: movq   <rt3=int64#12,120(<rp=int64#1)
# asm 2: movq   <rt3=%r14,120(<rp=%rdi)
movq   %r14,120(%rdi)

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
