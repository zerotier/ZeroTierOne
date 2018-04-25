
# qhasm: int64 rp

# qhasm: int64 pp

# qhasm: int64 qp

# qhasm: input rp

# qhasm: input pp

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

# qhasm: enter crypto_sign_ed25519_amd64_64_ge25519_nielsadd_p1p1
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_ge25519_nielsadd_p1p1
.globl crypto_sign_ed25519_amd64_64_ge25519_nielsadd_p1p1
_crypto_sign_ed25519_amd64_64_ge25519_nielsadd_p1p1:
crypto_sign_ed25519_amd64_64_ge25519_nielsadd_p1p1:
mov %rsp,%r11
and $31,%r11
add $128,%r11
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

# qhasm: qp = qp
# asm 1: mov  <qp=int64#3,>qp=int64#4
# asm 2: mov  <qp=%rdx,>qp=%rcx
mov  %rdx,%rcx

# qhasm: a0 = *(uint64 *)(pp + 32)
# asm 1: movq   32(<pp=int64#2),>a0=int64#3
# asm 2: movq   32(<pp=%rsi),>a0=%rdx
movq   32(%rsi),%rdx

# qhasm: a1 = *(uint64 *)(pp + 40)
# asm 1: movq   40(<pp=int64#2),>a1=int64#5
# asm 2: movq   40(<pp=%rsi),>a1=%r8
movq   40(%rsi),%r8

# qhasm: a2 = *(uint64 *)(pp + 48)
# asm 1: movq   48(<pp=int64#2),>a2=int64#6
# asm 2: movq   48(<pp=%rsi),>a2=%r9
movq   48(%rsi),%r9

# qhasm: a3 = *(uint64 *)(pp + 56)
# asm 1: movq   56(<pp=int64#2),>a3=int64#7
# asm 2: movq   56(<pp=%rsi),>a3=%rax
movq   56(%rsi),%rax

# qhasm: b0 = a0
# asm 1: mov  <a0=int64#3,>b0=int64#8
# asm 2: mov  <a0=%rdx,>b0=%r10
mov  %rdx,%r10

# qhasm: b1 = a1
# asm 1: mov  <a1=int64#5,>b1=int64#9
# asm 2: mov  <a1=%r8,>b1=%r11
mov  %r8,%r11

# qhasm: b2 = a2
# asm 1: mov  <a2=int64#6,>b2=int64#10
# asm 2: mov  <a2=%r9,>b2=%r12
mov  %r9,%r12

# qhasm: b3 = a3
# asm 1: mov  <a3=int64#7,>b3=int64#11
# asm 2: mov  <a3=%rax,>b3=%r13
mov  %rax,%r13

# qhasm:   carry? a0 -= *(uint64 *) (pp + 0) 
# asm 1: subq 0(<pp=int64#2),<a0=int64#3
# asm 2: subq 0(<pp=%rsi),<a0=%rdx
subq 0(%rsi),%rdx

# qhasm:   carry? a1 -= *(uint64 *) (pp + 8) - carry
# asm 1: sbbq 8(<pp=int64#2),<a1=int64#5
# asm 2: sbbq 8(<pp=%rsi),<a1=%r8
sbbq 8(%rsi),%r8

# qhasm:   carry? a2 -= *(uint64 *) (pp + 16) - carry
# asm 1: sbbq 16(<pp=int64#2),<a2=int64#6
# asm 2: sbbq 16(<pp=%rsi),<a2=%r9
sbbq 16(%rsi),%r9

# qhasm:   carry? a3 -= *(uint64 *) (pp + 24) - carry
# asm 1: sbbq 24(<pp=int64#2),<a3=int64#7
# asm 2: sbbq 24(<pp=%rsi),<a3=%rax
sbbq 24(%rsi),%rax

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#12
# asm 2: mov  $0,>subt0=%r14
mov  $0,%r14

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#13
# asm 2: mov  $38,>subt1=%r15
mov  $38,%r15

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#12,<subt1=int64#13
# asm 2: cmovae <subt0=%r14,<subt1=%r15
cmovae %r14,%r15

# qhasm:   carry? a0 -= subt1
# asm 1: sub  <subt1=int64#13,<a0=int64#3
# asm 2: sub  <subt1=%r15,<a0=%rdx
sub  %r15,%rdx

# qhasm:   carry? a1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#12,<a1=int64#5
# asm 2: sbb  <subt0=%r14,<a1=%r8
sbb  %r14,%r8

# qhasm:   carry? a2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#12,<a2=int64#6
# asm 2: sbb  <subt0=%r14,<a2=%r9
sbb  %r14,%r9

# qhasm:   carry? a3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#12,<a3=int64#7
# asm 2: sbb  <subt0=%r14,<a3=%rax
sbb  %r14,%rax

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#13,<subt0=int64#12
# asm 2: cmovc <subt1=%r15,<subt0=%r14
cmovc %r15,%r14

# qhasm:   a0 -= subt0
# asm 1: sub  <subt0=int64#12,<a0=int64#3
# asm 2: sub  <subt0=%r14,<a0=%rdx
sub  %r14,%rdx

# qhasm:   carry? b0 += *(uint64 *) (pp + 0) 
# asm 1: addq 0(<pp=int64#2),<b0=int64#8
# asm 2: addq 0(<pp=%rsi),<b0=%r10
addq 0(%rsi),%r10

# qhasm:   carry? b1 += *(uint64 *) (pp + 8) + carry
# asm 1: adcq 8(<pp=int64#2),<b1=int64#9
# asm 2: adcq 8(<pp=%rsi),<b1=%r11
adcq 8(%rsi),%r11

# qhasm:   carry? b2 += *(uint64 *) (pp + 16) + carry
# asm 1: adcq 16(<pp=int64#2),<b2=int64#10
# asm 2: adcq 16(<pp=%rsi),<b2=%r12
adcq 16(%rsi),%r12

# qhasm:   carry? b3 += *(uint64 *) (pp + 24) + carry
# asm 1: adcq 24(<pp=int64#2),<b3=int64#11
# asm 2: adcq 24(<pp=%rsi),<b3=%r13
adcq 24(%rsi),%r13

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#12
# asm 2: mov  $0,>addt0=%r14
mov  $0,%r14

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#13
# asm 2: mov  $38,>addt1=%r15
mov  $38,%r15

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#12,<addt1=int64#13
# asm 2: cmovae <addt0=%r14,<addt1=%r15
cmovae %r14,%r15

# qhasm:   carry? b0 += addt1
# asm 1: add  <addt1=int64#13,<b0=int64#8
# asm 2: add  <addt1=%r15,<b0=%r10
add  %r15,%r10

# qhasm:   carry? b1 += addt0 + carry
# asm 1: adc <addt0=int64#12,<b1=int64#9
# asm 2: adc <addt0=%r14,<b1=%r11
adc %r14,%r11

# qhasm:   carry? b2 += addt0 + carry
# asm 1: adc <addt0=int64#12,<b2=int64#10
# asm 2: adc <addt0=%r14,<b2=%r12
adc %r14,%r12

# qhasm:   carry? b3 += addt0 + carry
# asm 1: adc <addt0=int64#12,<b3=int64#11
# asm 2: adc <addt0=%r14,<b3=%r13
adc %r14,%r13

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#13,<addt0=int64#12
# asm 2: cmovc <addt1=%r15,<addt0=%r14
cmovc %r15,%r14

# qhasm:   b0 += addt0
# asm 1: add  <addt0=int64#12,<b0=int64#8
# asm 2: add  <addt0=%r14,<b0=%r10
add  %r14,%r10

# qhasm: a0_stack = a0
# asm 1: movq <a0=int64#3,>a0_stack=stack64#8
# asm 2: movq <a0=%rdx,>a0_stack=56(%rsp)
movq %rdx,56(%rsp)

# qhasm: a1_stack = a1
# asm 1: movq <a1=int64#5,>a1_stack=stack64#9
# asm 2: movq <a1=%r8,>a1_stack=64(%rsp)
movq %r8,64(%rsp)

# qhasm: a2_stack = a2
# asm 1: movq <a2=int64#6,>a2_stack=stack64#10
# asm 2: movq <a2=%r9,>a2_stack=72(%rsp)
movq %r9,72(%rsp)

# qhasm: a3_stack = a3
# asm 1: movq <a3=int64#7,>a3_stack=stack64#11
# asm 2: movq <a3=%rax,>a3_stack=80(%rsp)
movq %rax,80(%rsp)

# qhasm: b0_stack = b0
# asm 1: movq <b0=int64#8,>b0_stack=stack64#12
# asm 2: movq <b0=%r10,>b0_stack=88(%rsp)
movq %r10,88(%rsp)

# qhasm: b1_stack = b1
# asm 1: movq <b1=int64#9,>b1_stack=stack64#13
# asm 2: movq <b1=%r11,>b1_stack=96(%rsp)
movq %r11,96(%rsp)

# qhasm: b2_stack = b2
# asm 1: movq <b2=int64#10,>b2_stack=stack64#14
# asm 2: movq <b2=%r12,>b2_stack=104(%rsp)
movq %r12,104(%rsp)

# qhasm: b3_stack = b3
# asm 1: movq <b3=int64#11,>b3_stack=stack64#15
# asm 2: movq <b3=%r13,>b3_stack=112(%rsp)
movq %r13,112(%rsp)

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

# qhasm:   mulx0 = a0_stack
# asm 1: movq <a0_stack=stack64#8,>mulx0=int64#10
# asm 2: movq <a0_stack=56(%rsp),>mulx0=%r12
movq 56(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 0)
# asm 1: movq   0(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   0(<qp=%rcx),>mulrax=%rax
movq   0(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   a0 = mulrax
# asm 1: mov  <mulrax=int64#7,>a0=int64#11
# asm 2: mov  <mulrax=%rax,>a0=%r13
mov  %rax,%r13

# qhasm:   a1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>a1=int64#12
# asm 2: mov  <mulrdx=%rdx,>a1=%r14
mov  %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(qp + 8)
# asm 1: movq   8(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   8(<qp=%rcx),>mulrax=%rax
movq   8(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? a1 += mulrax
# asm 1: add  <mulrax=int64#7,<a1=int64#12
# asm 2: add  <mulrax=%rax,<a1=%r14
add  %rax,%r14

# qhasm:   a2 = 0
# asm 1: mov  $0,>a2=int64#13
# asm 2: mov  $0,>a2=%r15
mov  $0,%r15

# qhasm:   a2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<a2=int64#13
# asm 2: adc <mulrdx=%rdx,<a2=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(qp + 16)
# asm 1: movq   16(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   16(<qp=%rcx),>mulrax=%rax
movq   16(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? a2 += mulrax
# asm 1: add  <mulrax=int64#7,<a2=int64#13
# asm 2: add  <mulrax=%rax,<a2=%r15
add  %rax,%r15

# qhasm:   a3 = 0
# asm 1: mov  $0,>a3=int64#14
# asm 2: mov  $0,>a3=%rbx
mov  $0,%rbx

# qhasm:   a3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<a3=int64#14
# asm 2: adc <mulrdx=%rdx,<a3=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 24)
# asm 1: movq   24(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   24(<qp=%rcx),>mulrax=%rax
movq   24(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? a3 += mulrax
# asm 1: add  <mulrax=int64#7,<a3=int64#14
# asm 2: add  <mulrax=%rax,<a3=%rbx
add  %rax,%rbx

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr4=%r8
adc %rdx,%r8

# qhasm:   mulx1 = a1_stack
# asm 1: movq <a1_stack=stack64#9,>mulx1=int64#10
# asm 2: movq <a1_stack=64(%rsp),>mulx1=%r12
movq 64(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 0)
# asm 1: movq   0(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   0(<qp=%rcx),>mulrax=%rax
movq   0(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? a1 += mulrax
# asm 1: add  <mulrax=int64#7,<a1=int64#12
# asm 2: add  <mulrax=%rax,<a1=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 8)
# asm 1: movq   8(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   8(<qp=%rcx),>mulrax=%rax
movq   8(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? a2 += mulrax
# asm 1: add  <mulrax=int64#7,<a2=int64#13
# asm 2: add  <mulrax=%rax,<a2=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? a2 += mulc
# asm 1: add  <mulc=int64#15,<a2=int64#13
# asm 2: add  <mulc=%rbp,<a2=%r15
add  %rbp,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 16)
# asm 1: movq   16(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   16(<qp=%rcx),>mulrax=%rax
movq   16(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? a3 += mulrax 
# asm 1: add  <mulrax=int64#7,<a3=int64#14
# asm 2: add  <mulrax=%rax,<a3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? a3 += mulc
# asm 1: add  <mulc=int64#15,<a3=int64#14
# asm 2: add  <mulc=%rbp,<a3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 24)
# asm 1: movq   24(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   24(<qp=%rcx),>mulrax=%rax
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

# qhasm:   mulx2 = a2_stack
# asm 1: movq <a2_stack=stack64#10,>mulx2=int64#10
# asm 2: movq <a2_stack=72(%rsp),>mulx2=%r12
movq 72(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 0)
# asm 1: movq   0(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   0(<qp=%rcx),>mulrax=%rax
movq   0(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? a2 += mulrax
# asm 1: add  <mulrax=int64#7,<a2=int64#13
# asm 2: add  <mulrax=%rax,<a2=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 8)
# asm 1: movq   8(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   8(<qp=%rcx),>mulrax=%rax
movq   8(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? a3 += mulrax
# asm 1: add  <mulrax=int64#7,<a3=int64#14
# asm 2: add  <mulrax=%rax,<a3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? a3 += mulc
# asm 1: add  <mulc=int64#15,<a3=int64#14
# asm 2: add  <mulc=%rbp,<a3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 16)
# asm 1: movq   16(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   16(<qp=%rcx),>mulrax=%rax
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

# qhasm:   mulrax = *(uint64 *)(qp + 24)
# asm 1: movq   24(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   24(<qp=%rcx),>mulrax=%rax
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

# qhasm:   mulx3 = a3_stack
# asm 1: movq <a3_stack=stack64#11,>mulx3=int64#10
# asm 2: movq <a3_stack=80(%rsp),>mulx3=%r12
movq 80(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 0)
# asm 1: movq   0(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   0(<qp=%rcx),>mulrax=%rax
movq   0(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
mul  %r12

# qhasm:   carry? a3 += mulrax
# asm 1: add  <mulrax=int64#7,<a3=int64#14
# asm 2: add  <mulrax=%rax,<a3=%rbx
add  %rax,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 8)
# asm 1: movq   8(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   8(<qp=%rcx),>mulrax=%rax
movq   8(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
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

# qhasm:   mulrax = *(uint64 *)(qp + 16)
# asm 1: movq   16(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   16(<qp=%rcx),>mulrax=%rax
movq   16(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
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

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 24)
# asm 1: movq   24(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   24(<qp=%rcx),>mulrax=%rax
movq   24(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
mul  %r12

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#8
# asm 2: add  <mulrax=%rax,<mulr6=%r10
add  %rax,%r10

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#15,<mulr6=int64#8
# asm 2: add  <mulc=%rbp,<mulr6=%r10
add  %rbp,%r10

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#9
# asm 2: adc <mulrdx=%rdx,<mulr7=%r11
adc %rdx,%r11

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr4=%r8,>mulrax=%rax
mov  %r8,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#5
# asm 2: mov  <mulrax=%rax,>mulr4=%r8
mov  %rax,%r8

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr5=%r9,>mulrax=%rax
mov  %r9,%rax

# qhasm:   mulr5 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>mulr5=int64#6
# asm 2: mov  <mulrdx=%rdx,>mulr5=%r9
mov  %rdx,%r9

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#6
# asm 2: add  <mulrax=%rax,<mulr5=%r9
add  %rax,%r9

# qhasm:   mulrax = mulr6
# asm 1: mov  <mulr6=int64#8,>mulrax=int64#7
# asm 2: mov  <mulr6=%r10,>mulrax=%rax
mov  %r10,%rax

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#8
# asm 2: mov  $0,>mulr6=%r10
mov  $0,%r10

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#8
# asm 2: adc <mulrdx=%rdx,<mulr6=%r10
adc %rdx,%r10

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#8
# asm 2: add  <mulrax=%rax,<mulr6=%r10
add  %rax,%r10

# qhasm:   mulrax = mulr7
# asm 1: mov  <mulr7=int64#9,>mulrax=int64#7
# asm 2: mov  <mulr7=%r11,>mulrax=%rax
mov  %r11,%rax

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#9
# asm 2: mov  $0,>mulr7=%r11
mov  $0,%r11

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#9
# asm 2: adc <mulrdx=%rdx,<mulr7=%r11
adc %rdx,%r11

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr7 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr7=int64#9
# asm 2: add  <mulrax=%rax,<mulr7=%r11
add  %rax,%r11

# qhasm:   mulr8 = 0
# asm 1: mov  $0,>mulr8=int64#7
# asm 2: mov  $0,>mulr8=%rax
mov  $0,%rax

# qhasm:   mulr8 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr8=int64#7
# asm 2: adc <mulrdx=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   carry? a0 += mulr4
# asm 1: add  <mulr4=int64#5,<a0=int64#11
# asm 2: add  <mulr4=%r8,<a0=%r13
add  %r8,%r13

# qhasm:   carry? a1 += mulr5 + carry
# asm 1: adc <mulr5=int64#6,<a1=int64#12
# asm 2: adc <mulr5=%r9,<a1=%r14
adc %r9,%r14

# qhasm:   carry? a2 += mulr6 + carry
# asm 1: adc <mulr6=int64#8,<a2=int64#13
# asm 2: adc <mulr6=%r10,<a2=%r15
adc %r10,%r15

# qhasm:   carry? a3 += mulr7 + carry
# asm 1: adc <mulr7=int64#9,<a3=int64#14
# asm 2: adc <mulr7=%r11,<a3=%rbx
adc %r11,%rbx

# qhasm:   mulzero = 0
# asm 1: mov  $0,>mulzero=int64#3
# asm 2: mov  $0,>mulzero=%rdx
mov  $0,%rdx

# qhasm:   mulr8 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulr8=int64#7
# asm 2: adc <mulzero=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   mulr8 *= 38
# asm 1: imulq  $38,<mulr8=int64#7,>mulr8=int64#5
# asm 2: imulq  $38,<mulr8=%rax,>mulr8=%r8
imulq  $38,%rax,%r8

# qhasm:   carry? a0 += mulr8
# asm 1: add  <mulr8=int64#5,<a0=int64#11
# asm 2: add  <mulr8=%r8,<a0=%r13
add  %r8,%r13

# qhasm:   carry? a1 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<a1=int64#12
# asm 2: adc <mulzero=%rdx,<a1=%r14
adc %rdx,%r14

# qhasm:   carry? a2 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<a2=int64#13
# asm 2: adc <mulzero=%rdx,<a2=%r15
adc %rdx,%r15

# qhasm:   carry? a3 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<a3=int64#14
# asm 2: adc <mulzero=%rdx,<a3=%rbx
adc %rdx,%rbx

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulzero=int64#3
# asm 2: adc <mulzero=%rdx,<mulzero=%rdx
adc %rdx,%rdx

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#3,>mulzero=int64#3
# asm 2: imulq  $38,<mulzero=%rdx,>mulzero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   a0 += mulzero
# asm 1: add  <mulzero=int64#3,<a0=int64#11
# asm 2: add  <mulzero=%rdx,<a0=%r13
add  %rdx,%r13

# qhasm: a0_stack = a0
# asm 1: movq <a0=int64#11,>a0_stack=stack64#8
# asm 2: movq <a0=%r13,>a0_stack=56(%rsp)
movq %r13,56(%rsp)

# qhasm: a1_stack = a1
# asm 1: movq <a1=int64#12,>a1_stack=stack64#9
# asm 2: movq <a1=%r14,>a1_stack=64(%rsp)
movq %r14,64(%rsp)

# qhasm: a2_stack = a2
# asm 1: movq <a2=int64#13,>a2_stack=stack64#10
# asm 2: movq <a2=%r15,>a2_stack=72(%rsp)
movq %r15,72(%rsp)

# qhasm: a3_stack = a3
# asm 1: movq <a3=int64#14,>a3_stack=stack64#11
# asm 2: movq <a3=%rbx,>a3_stack=80(%rsp)
movq %rbx,80(%rsp)

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

# qhasm:   mulx0 = b0_stack
# asm 1: movq <b0_stack=stack64#12,>mulx0=int64#10
# asm 2: movq <b0_stack=88(%rsp),>mulx0=%r12
movq 88(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   32(<qp=%rcx),>mulrax=%rax
movq   32(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   e0 = mulrax
# asm 1: mov  <mulrax=int64#7,>e0=int64#11
# asm 2: mov  <mulrax=%rax,>e0=%r13
mov  %rax,%r13

# qhasm:   e1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>e1=int64#12
# asm 2: mov  <mulrdx=%rdx,>e1=%r14
mov  %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   40(<qp=%rcx),>mulrax=%rax
movq   40(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? e1 += mulrax
# asm 1: add  <mulrax=int64#7,<e1=int64#12
# asm 2: add  <mulrax=%rax,<e1=%r14
add  %rax,%r14

# qhasm:   e2 = 0
# asm 1: mov  $0,>e2=int64#13
# asm 2: mov  $0,>e2=%r15
mov  $0,%r15

# qhasm:   e2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<e2=int64#13
# asm 2: adc <mulrdx=%rdx,<e2=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   48(<qp=%rcx),>mulrax=%rax
movq   48(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? e2 += mulrax
# asm 1: add  <mulrax=int64#7,<e2=int64#13
# asm 2: add  <mulrax=%rax,<e2=%r15
add  %rax,%r15

# qhasm:   e3 = 0
# asm 1: mov  $0,>e3=int64#14
# asm 2: mov  $0,>e3=%rbx
mov  $0,%rbx

# qhasm:   e3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<e3=int64#14
# asm 2: adc <mulrdx=%rdx,<e3=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   56(<qp=%rcx),>mulrax=%rax
movq   56(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? e3 += mulrax
# asm 1: add  <mulrax=int64#7,<e3=int64#14
# asm 2: add  <mulrax=%rax,<e3=%rbx
add  %rax,%rbx

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr4=%r8
adc %rdx,%r8

# qhasm:   mulx1 = b1_stack
# asm 1: movq <b1_stack=stack64#13,>mulx1=int64#10
# asm 2: movq <b1_stack=96(%rsp),>mulx1=%r12
movq 96(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   32(<qp=%rcx),>mulrax=%rax
movq   32(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? e1 += mulrax
# asm 1: add  <mulrax=int64#7,<e1=int64#12
# asm 2: add  <mulrax=%rax,<e1=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   40(<qp=%rcx),>mulrax=%rax
movq   40(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? e2 += mulrax
# asm 1: add  <mulrax=int64#7,<e2=int64#13
# asm 2: add  <mulrax=%rax,<e2=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? e2 += mulc
# asm 1: add  <mulc=int64#15,<e2=int64#13
# asm 2: add  <mulc=%rbp,<e2=%r15
add  %rbp,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   48(<qp=%rcx),>mulrax=%rax
movq   48(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? e3 += mulrax 
# asm 1: add  <mulrax=int64#7,<e3=int64#14
# asm 2: add  <mulrax=%rax,<e3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? e3 += mulc
# asm 1: add  <mulc=int64#15,<e3=int64#14
# asm 2: add  <mulc=%rbp,<e3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   56(<qp=%rcx),>mulrax=%rax
movq   56(%rcx),%rax

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

# qhasm:   mulx2 = b2_stack
# asm 1: movq <b2_stack=stack64#14,>mulx2=int64#10
# asm 2: movq <b2_stack=104(%rsp),>mulx2=%r12
movq 104(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   32(<qp=%rcx),>mulrax=%rax
movq   32(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? e2 += mulrax
# asm 1: add  <mulrax=int64#7,<e2=int64#13
# asm 2: add  <mulrax=%rax,<e2=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   40(<qp=%rcx),>mulrax=%rax
movq   40(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? e3 += mulrax
# asm 1: add  <mulrax=int64#7,<e3=int64#14
# asm 2: add  <mulrax=%rax,<e3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? e3 += mulc
# asm 1: add  <mulc=int64#15,<e3=int64#14
# asm 2: add  <mulc=%rbp,<e3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   48(<qp=%rcx),>mulrax=%rax
movq   48(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   56(<qp=%rcx),>mulrax=%rax
movq   56(%rcx),%rax

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

# qhasm:   mulx3 = b3_stack
# asm 1: movq <b3_stack=stack64#15,>mulx3=int64#10
# asm 2: movq <b3_stack=112(%rsp),>mulx3=%r12
movq 112(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   32(<qp=%rcx),>mulrax=%rax
movq   32(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
mul  %r12

# qhasm:   carry? e3 += mulrax
# asm 1: add  <mulrax=int64#7,<e3=int64#14
# asm 2: add  <mulrax=%rax,<e3=%rbx
add  %rax,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   40(<qp=%rcx),>mulrax=%rax
movq   40(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
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

# qhasm:   mulrax = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   48(<qp=%rcx),>mulrax=%rax
movq   48(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
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

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   56(<qp=%rcx),>mulrax=%rax
movq   56(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
mul  %r12

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#8
# asm 2: add  <mulrax=%rax,<mulr6=%r10
add  %rax,%r10

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#15,<mulr6=int64#8
# asm 2: add  <mulc=%rbp,<mulr6=%r10
add  %rbp,%r10

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#9
# asm 2: adc <mulrdx=%rdx,<mulr7=%r11
adc %rdx,%r11

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr4=%r8,>mulrax=%rax
mov  %r8,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#5
# asm 2: mov  <mulrax=%rax,>mulr4=%r8
mov  %rax,%r8

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr5=%r9,>mulrax=%rax
mov  %r9,%rax

# qhasm:   mulr5 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>mulr5=int64#6
# asm 2: mov  <mulrdx=%rdx,>mulr5=%r9
mov  %rdx,%r9

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr5 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr5=int64#6
# asm 2: add  <mulrax=%rax,<mulr5=%r9
add  %rax,%r9

# qhasm:   mulrax = mulr6
# asm 1: mov  <mulr6=int64#8,>mulrax=int64#7
# asm 2: mov  <mulr6=%r10,>mulrax=%rax
mov  %r10,%rax

# qhasm:   mulr6 = 0
# asm 1: mov  $0,>mulr6=int64#8
# asm 2: mov  $0,>mulr6=%r10
mov  $0,%r10

# qhasm:   mulr6 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr6=int64#8
# asm 2: adc <mulrdx=%rdx,<mulr6=%r10
adc %rdx,%r10

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#8
# asm 2: add  <mulrax=%rax,<mulr6=%r10
add  %rax,%r10

# qhasm:   mulrax = mulr7
# asm 1: mov  <mulr7=int64#9,>mulrax=int64#7
# asm 2: mov  <mulr7=%r11,>mulrax=%rax
mov  %r11,%rax

# qhasm:   mulr7 = 0
# asm 1: mov  $0,>mulr7=int64#9
# asm 2: mov  $0,>mulr7=%r11
mov  $0,%r11

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#9
# asm 2: adc <mulrdx=%rdx,<mulr7=%r11
adc %rdx,%r11

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   carry? mulr7 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr7=int64#9
# asm 2: add  <mulrax=%rax,<mulr7=%r11
add  %rax,%r11

# qhasm:   mulr8 = 0
# asm 1: mov  $0,>mulr8=int64#7
# asm 2: mov  $0,>mulr8=%rax
mov  $0,%rax

# qhasm:   mulr8 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr8=int64#7
# asm 2: adc <mulrdx=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   carry? e0 += mulr4
# asm 1: add  <mulr4=int64#5,<e0=int64#11
# asm 2: add  <mulr4=%r8,<e0=%r13
add  %r8,%r13

# qhasm:   carry? e1 += mulr5 + carry
# asm 1: adc <mulr5=int64#6,<e1=int64#12
# asm 2: adc <mulr5=%r9,<e1=%r14
adc %r9,%r14

# qhasm:   carry? e2 += mulr6 + carry
# asm 1: adc <mulr6=int64#8,<e2=int64#13
# asm 2: adc <mulr6=%r10,<e2=%r15
adc %r10,%r15

# qhasm:   carry? e3 += mulr7 + carry
# asm 1: adc <mulr7=int64#9,<e3=int64#14
# asm 2: adc <mulr7=%r11,<e3=%rbx
adc %r11,%rbx

# qhasm:   mulzero = 0
# asm 1: mov  $0,>mulzero=int64#3
# asm 2: mov  $0,>mulzero=%rdx
mov  $0,%rdx

# qhasm:   mulr8 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulr8=int64#7
# asm 2: adc <mulzero=%rdx,<mulr8=%rax
adc %rdx,%rax

# qhasm:   mulr8 *= 38
# asm 1: imulq  $38,<mulr8=int64#7,>mulr8=int64#5
# asm 2: imulq  $38,<mulr8=%rax,>mulr8=%r8
imulq  $38,%rax,%r8

# qhasm:   carry? e0 += mulr8
# asm 1: add  <mulr8=int64#5,<e0=int64#11
# asm 2: add  <mulr8=%r8,<e0=%r13
add  %r8,%r13

# qhasm:   carry? e1 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<e1=int64#12
# asm 2: adc <mulzero=%rdx,<e1=%r14
adc %rdx,%r14

# qhasm:   carry? e2 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<e2=int64#13
# asm 2: adc <mulzero=%rdx,<e2=%r15
adc %rdx,%r15

# qhasm:   carry? e3 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<e3=int64#14
# asm 2: adc <mulzero=%rdx,<e3=%rbx
adc %rdx,%rbx

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulzero=int64#3
# asm 2: adc <mulzero=%rdx,<mulzero=%rdx
adc %rdx,%rdx

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#3,>mulzero=int64#3
# asm 2: imulq  $38,<mulzero=%rdx,>mulzero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   e0 += mulzero
# asm 1: add  <mulzero=int64#3,<e0=int64#11
# asm 2: add  <mulzero=%rdx,<e0=%r13
add  %rdx,%r13

# qhasm: h0 = e0
# asm 1: mov  <e0=int64#11,>h0=int64#3
# asm 2: mov  <e0=%r13,>h0=%rdx
mov  %r13,%rdx

# qhasm: h1 = e1
# asm 1: mov  <e1=int64#12,>h1=int64#5
# asm 2: mov  <e1=%r14,>h1=%r8
mov  %r14,%r8

# qhasm: h2 = e2
# asm 1: mov  <e2=int64#13,>h2=int64#6
# asm 2: mov  <e2=%r15,>h2=%r9
mov  %r15,%r9

# qhasm: h3 = e3
# asm 1: mov  <e3=int64#14,>h3=int64#7
# asm 2: mov  <e3=%rbx,>h3=%rax
mov  %rbx,%rax

# qhasm:   carry? e0 -= a0_stack 
# asm 1: subq <a0_stack=stack64#8,<e0=int64#11
# asm 2: subq <a0_stack=56(%rsp),<e0=%r13
subq 56(%rsp),%r13

# qhasm:   carry? e1 -= a1_stack - carry
# asm 1: sbbq <a1_stack=stack64#9,<e1=int64#12
# asm 2: sbbq <a1_stack=64(%rsp),<e1=%r14
sbbq 64(%rsp),%r14

# qhasm:   carry? e2 -= a2_stack - carry
# asm 1: sbbq <a2_stack=stack64#10,<e2=int64#13
# asm 2: sbbq <a2_stack=72(%rsp),<e2=%r15
sbbq 72(%rsp),%r15

# qhasm:   carry? e3 -= a3_stack - carry
# asm 1: sbbq <a3_stack=stack64#11,<e3=int64#14
# asm 2: sbbq <a3_stack=80(%rsp),<e3=%rbx
sbbq 80(%rsp),%rbx

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#8
# asm 2: mov  $0,>subt0=%r10
mov  $0,%r10

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#9
# asm 2: mov  $38,>subt1=%r11
mov  $38,%r11

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#8,<subt1=int64#9
# asm 2: cmovae <subt0=%r10,<subt1=%r11
cmovae %r10,%r11

# qhasm:   carry? e0 -= subt1
# asm 1: sub  <subt1=int64#9,<e0=int64#11
# asm 2: sub  <subt1=%r11,<e0=%r13
sub  %r11,%r13

# qhasm:   carry? e1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#8,<e1=int64#12
# asm 2: sbb  <subt0=%r10,<e1=%r14
sbb  %r10,%r14

# qhasm:   carry? e2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#8,<e2=int64#13
# asm 2: sbb  <subt0=%r10,<e2=%r15
sbb  %r10,%r15

# qhasm:   carry? e3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#8,<e3=int64#14
# asm 2: sbb  <subt0=%r10,<e3=%rbx
sbb  %r10,%rbx

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#9,<subt0=int64#8
# asm 2: cmovc <subt1=%r11,<subt0=%r10
cmovc %r11,%r10

# qhasm:   e0 -= subt0
# asm 1: sub  <subt0=int64#8,<e0=int64#11
# asm 2: sub  <subt0=%r10,<e0=%r13
sub  %r10,%r13

# qhasm:   carry? h0 += a0_stack 
# asm 1: addq <a0_stack=stack64#8,<h0=int64#3
# asm 2: addq <a0_stack=56(%rsp),<h0=%rdx
addq 56(%rsp),%rdx

# qhasm:   carry? h1 += a1_stack + carry
# asm 1: adcq <a1_stack=stack64#9,<h1=int64#5
# asm 2: adcq <a1_stack=64(%rsp),<h1=%r8
adcq 64(%rsp),%r8

# qhasm:   carry? h2 += a2_stack + carry
# asm 1: adcq <a2_stack=stack64#10,<h2=int64#6
# asm 2: adcq <a2_stack=72(%rsp),<h2=%r9
adcq 72(%rsp),%r9

# qhasm:   carry? h3 += a3_stack + carry
# asm 1: adcq <a3_stack=stack64#11,<h3=int64#7
# asm 2: adcq <a3_stack=80(%rsp),<h3=%rax
adcq 80(%rsp),%rax

# qhasm:   addt0 = 0
# asm 1: mov  $0,>addt0=int64#8
# asm 2: mov  $0,>addt0=%r10
mov  $0,%r10

# qhasm:   addt1 = 38
# asm 1: mov  $38,>addt1=int64#9
# asm 2: mov  $38,>addt1=%r11
mov  $38,%r11

# qhasm:   addt1 = addt0 if !carry
# asm 1: cmovae <addt0=int64#8,<addt1=int64#9
# asm 2: cmovae <addt0=%r10,<addt1=%r11
cmovae %r10,%r11

# qhasm:   carry? h0 += addt1
# asm 1: add  <addt1=int64#9,<h0=int64#3
# asm 2: add  <addt1=%r11,<h0=%rdx
add  %r11,%rdx

# qhasm:   carry? h1 += addt0 + carry
# asm 1: adc <addt0=int64#8,<h1=int64#5
# asm 2: adc <addt0=%r10,<h1=%r8
adc %r10,%r8

# qhasm:   carry? h2 += addt0 + carry
# asm 1: adc <addt0=int64#8,<h2=int64#6
# asm 2: adc <addt0=%r10,<h2=%r9
adc %r10,%r9

# qhasm:   carry? h3 += addt0 + carry
# asm 1: adc <addt0=int64#8,<h3=int64#7
# asm 2: adc <addt0=%r10,<h3=%rax
adc %r10,%rax

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#9,<addt0=int64#8
# asm 2: cmovc <addt1=%r11,<addt0=%r10
cmovc %r11,%r10

# qhasm:   h0 += addt0
# asm 1: add  <addt0=int64#8,<h0=int64#3
# asm 2: add  <addt0=%r10,<h0=%rdx
add  %r10,%rdx

# qhasm: *(uint64 *)(rp + 64) = h0
# asm 1: movq   <h0=int64#3,64(<rp=int64#1)
# asm 2: movq   <h0=%rdx,64(<rp=%rdi)
movq   %rdx,64(%rdi)

# qhasm: *(uint64 *)(rp + 72) = h1
# asm 1: movq   <h1=int64#5,72(<rp=int64#1)
# asm 2: movq   <h1=%r8,72(<rp=%rdi)
movq   %r8,72(%rdi)

# qhasm: *(uint64 *)(rp + 80) = h2
# asm 1: movq   <h2=int64#6,80(<rp=int64#1)
# asm 2: movq   <h2=%r9,80(<rp=%rdi)
movq   %r9,80(%rdi)

# qhasm: *(uint64 *)(rp + 88) = h3
# asm 1: movq   <h3=int64#7,88(<rp=int64#1)
# asm 2: movq   <h3=%rax,88(<rp=%rdi)
movq   %rax,88(%rdi)

# qhasm: *(uint64 *)(rp + 0) = e0
# asm 1: movq   <e0=int64#11,0(<rp=int64#1)
# asm 2: movq   <e0=%r13,0(<rp=%rdi)
movq   %r13,0(%rdi)

# qhasm: *(uint64 *)(rp + 8) = e1
# asm 1: movq   <e1=int64#12,8(<rp=int64#1)
# asm 2: movq   <e1=%r14,8(<rp=%rdi)
movq   %r14,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = e2
# asm 1: movq   <e2=int64#13,16(<rp=int64#1)
# asm 2: movq   <e2=%r15,16(<rp=%rdi)
movq   %r15,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = e3
# asm 1: movq   <e3=int64#14,24(<rp=int64#1)
# asm 2: movq   <e3=%rbx,24(<rp=%rdi)
movq   %rbx,24(%rdi)

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

# qhasm:   mulx0 = *(uint64 *)(pp + 96)
# asm 1: movq   96(<pp=int64#2),>mulx0=int64#10
# asm 2: movq   96(<pp=%rsi),>mulx0=%r12
movq   96(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   64(<qp=%rcx),>mulrax=%rax
movq   64(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   c0 = mulrax
# asm 1: mov  <mulrax=int64#7,>c0=int64#11
# asm 2: mov  <mulrax=%rax,>c0=%r13
mov  %rax,%r13

# qhasm:   c1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>c1=int64#12
# asm 2: mov  <mulrdx=%rdx,>c1=%r14
mov  %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   72(<qp=%rcx),>mulrax=%rax
movq   72(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? c1 += mulrax
# asm 1: add  <mulrax=int64#7,<c1=int64#12
# asm 2: add  <mulrax=%rax,<c1=%r14
add  %rax,%r14

# qhasm:   c2 = 0
# asm 1: mov  $0,>c2=int64#13
# asm 2: mov  $0,>c2=%r15
mov  $0,%r15

# qhasm:   c2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<c2=int64#13
# asm 2: adc <mulrdx=%rdx,<c2=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   80(<qp=%rcx),>mulrax=%rax
movq   80(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? c2 += mulrax
# asm 1: add  <mulrax=int64#7,<c2=int64#13
# asm 2: add  <mulrax=%rax,<c2=%r15
add  %rax,%r15

# qhasm:   c3 = 0
# asm 1: mov  $0,>c3=int64#14
# asm 2: mov  $0,>c3=%rbx
mov  $0,%rbx

# qhasm:   c3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<c3=int64#14
# asm 2: adc <mulrdx=%rdx,<c3=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   88(<qp=%rcx),>mulrax=%rax
movq   88(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? c3 += mulrax
# asm 1: add  <mulrax=int64#7,<c3=int64#14
# asm 2: add  <mulrax=%rax,<c3=%rbx
add  %rax,%rbx

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr4=%r8
adc %rdx,%r8

# qhasm:   mulx1 = *(uint64 *)(pp + 104)
# asm 1: movq   104(<pp=int64#2),>mulx1=int64#10
# asm 2: movq   104(<pp=%rsi),>mulx1=%r12
movq   104(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   64(<qp=%rcx),>mulrax=%rax
movq   64(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? c1 += mulrax
# asm 1: add  <mulrax=int64#7,<c1=int64#12
# asm 2: add  <mulrax=%rax,<c1=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   72(<qp=%rcx),>mulrax=%rax
movq   72(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? c2 += mulrax
# asm 1: add  <mulrax=int64#7,<c2=int64#13
# asm 2: add  <mulrax=%rax,<c2=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? c2 += mulc
# asm 1: add  <mulc=int64#15,<c2=int64#13
# asm 2: add  <mulc=%rbp,<c2=%r15
add  %rbp,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   80(<qp=%rcx),>mulrax=%rax
movq   80(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? c3 += mulrax 
# asm 1: add  <mulrax=int64#7,<c3=int64#14
# asm 2: add  <mulrax=%rax,<c3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? c3 += mulc
# asm 1: add  <mulc=int64#15,<c3=int64#14
# asm 2: add  <mulc=%rbp,<c3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   88(<qp=%rcx),>mulrax=%rax
movq   88(%rcx),%rax

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

# qhasm:   mulx2 = *(uint64 *)(pp + 112)
# asm 1: movq   112(<pp=int64#2),>mulx2=int64#10
# asm 2: movq   112(<pp=%rsi),>mulx2=%r12
movq   112(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   64(<qp=%rcx),>mulrax=%rax
movq   64(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? c2 += mulrax
# asm 1: add  <mulrax=int64#7,<c2=int64#13
# asm 2: add  <mulrax=%rax,<c2=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   72(<qp=%rcx),>mulrax=%rax
movq   72(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? c3 += mulrax
# asm 1: add  <mulrax=int64#7,<c3=int64#14
# asm 2: add  <mulrax=%rax,<c3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? c3 += mulc
# asm 1: add  <mulc=int64#15,<c3=int64#14
# asm 2: add  <mulc=%rbp,<c3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   80(<qp=%rcx),>mulrax=%rax
movq   80(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   88(<qp=%rcx),>mulrax=%rax
movq   88(%rcx),%rax

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

# qhasm:   mulx3 = *(uint64 *)(pp + 120)
# asm 1: movq   120(<pp=int64#2),>mulx3=int64#10
# asm 2: movq   120(<pp=%rsi),>mulx3=%r12
movq   120(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   64(<qp=%rcx),>mulrax=%rax
movq   64(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
mul  %r12

# qhasm:   carry? c3 += mulrax
# asm 1: add  <mulrax=int64#7,<c3=int64#14
# asm 2: add  <mulrax=%rax,<c3=%rbx
add  %rax,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   72(<qp=%rcx),>mulrax=%rax
movq   72(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
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

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   80(<qp=%rcx),>mulrax=%rax
movq   80(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
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

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   88(<qp=%rcx),>mulrax=%rax
movq   88(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
mul  %r12

# qhasm:   carry? mulr6 += mulrax
# asm 1: add  <mulrax=int64#7,<mulr6=int64#8
# asm 2: add  <mulrax=%rax,<mulr6=%r10
add  %rax,%r10

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? mulr6 += mulc
# asm 1: add  <mulc=int64#15,<mulr6=int64#8
# asm 2: add  <mulc=%rbp,<mulr6=%r10
add  %rbp,%r10

# qhasm:   mulr7 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr7=int64#9
# asm 2: adc <mulrdx=%rdx,<mulr7=%r11
adc %rdx,%r11

# qhasm:   mulrax = mulr4
# asm 1: mov  <mulr4=int64#5,>mulrax=int64#7
# asm 2: mov  <mulr4=%r8,>mulrax=%rax
mov  %r8,%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * *(uint64 *)&crypto_sign_ed25519_amd64_64_38
mulq  crypto_sign_ed25519_amd64_64_38(%rip)

# qhasm:   mulr4 = mulrax
# asm 1: mov  <mulrax=int64#7,>mulr4=int64#4
# asm 2: mov  <mulrax=%rax,>mulr4=%rcx
mov  %rax,%rcx

# qhasm:   mulrax = mulr5
# asm 1: mov  <mulr5=int64#6,>mulrax=int64#7
# asm 2: mov  <mulr5=%r9,>mulrax=%rax
mov  %r9,%rax

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
# asm 1: mov  <mulr6=int64#8,>mulrax=int64#7
# asm 2: mov  <mulr6=%r10,>mulrax=%rax
mov  %r10,%rax

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
# asm 1: mov  <mulr7=int64#9,>mulrax=int64#7
# asm 2: mov  <mulr7=%r11,>mulrax=%rax
mov  %r11,%rax

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

# qhasm:   carry? c0 += mulr4
# asm 1: add  <mulr4=int64#4,<c0=int64#11
# asm 2: add  <mulr4=%rcx,<c0=%r13
add  %rcx,%r13

# qhasm:   carry? c1 += mulr5 + carry
# asm 1: adc <mulr5=int64#5,<c1=int64#12
# asm 2: adc <mulr5=%r8,<c1=%r14
adc %r8,%r14

# qhasm:   carry? c2 += mulr6 + carry
# asm 1: adc <mulr6=int64#6,<c2=int64#13
# asm 2: adc <mulr6=%r9,<c2=%r15
adc %r9,%r15

# qhasm:   carry? c3 += mulr7 + carry
# asm 1: adc <mulr7=int64#8,<c3=int64#14
# asm 2: adc <mulr7=%r10,<c3=%rbx
adc %r10,%rbx

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

# qhasm:   carry? c0 += mulr8
# asm 1: add  <mulr8=int64#4,<c0=int64#11
# asm 2: add  <mulr8=%rcx,<c0=%r13
add  %rcx,%r13

# qhasm:   carry? c1 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<c1=int64#12
# asm 2: adc <mulzero=%rdx,<c1=%r14
adc %rdx,%r14

# qhasm:   carry? c2 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<c2=int64#13
# asm 2: adc <mulzero=%rdx,<c2=%r15
adc %rdx,%r15

# qhasm:   carry? c3 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<c3=int64#14
# asm 2: adc <mulzero=%rdx,<c3=%rbx
adc %rdx,%rbx

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulzero=int64#3
# asm 2: adc <mulzero=%rdx,<mulzero=%rdx
adc %rdx,%rdx

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#3,>mulzero=int64#3
# asm 2: imulq  $38,<mulzero=%rdx,>mulzero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   c0 += mulzero
# asm 1: add  <mulzero=int64#3,<c0=int64#11
# asm 2: add  <mulzero=%rdx,<c0=%r13
add  %rdx,%r13

# qhasm: f0 = *(uint64 *)(pp + 64)
# asm 1: movq   64(<pp=int64#2),>f0=int64#3
# asm 2: movq   64(<pp=%rsi),>f0=%rdx
movq   64(%rsi),%rdx

# qhasm: f1 = *(uint64 *)(pp + 72)
# asm 1: movq   72(<pp=int64#2),>f1=int64#4
# asm 2: movq   72(<pp=%rsi),>f1=%rcx
movq   72(%rsi),%rcx

# qhasm: f2 = *(uint64 *)(pp + 80)
# asm 1: movq   80(<pp=int64#2),>f2=int64#5
# asm 2: movq   80(<pp=%rsi),>f2=%r8
movq   80(%rsi),%r8

# qhasm: f3 = *(uint64 *)(pp + 88)
# asm 1: movq   88(<pp=int64#2),>f3=int64#2
# asm 2: movq   88(<pp=%rsi),>f3=%rsi
movq   88(%rsi),%rsi

# qhasm:   carry? f0 += f0 
# asm 1: add  <f0=int64#3,<f0=int64#3
# asm 2: add  <f0=%rdx,<f0=%rdx
add  %rdx,%rdx

# qhasm:   carry? f1 += f1 + carry
# asm 1: adc <f1=int64#4,<f1=int64#4
# asm 2: adc <f1=%rcx,<f1=%rcx
adc %rcx,%rcx

# qhasm:   carry? f2 += f2 + carry
# asm 1: adc <f2=int64#5,<f2=int64#5
# asm 2: adc <f2=%r8,<f2=%r8
adc %r8,%r8

# qhasm:   carry? f3 += f3 + carry
# asm 1: adc <f3=int64#2,<f3=int64#2
# asm 2: adc <f3=%rsi,<f3=%rsi
adc %rsi,%rsi

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
# asm 1: add  <addt1=int64#7,<f0=int64#3
# asm 2: add  <addt1=%rax,<f0=%rdx
add  %rax,%rdx

# qhasm:   carry? f1 += addt0 + carry
# asm 1: adc <addt0=int64#6,<f1=int64#4
# asm 2: adc <addt0=%r9,<f1=%rcx
adc %r9,%rcx

# qhasm:   carry? f2 += addt0 + carry
# asm 1: adc <addt0=int64#6,<f2=int64#5
# asm 2: adc <addt0=%r9,<f2=%r8
adc %r9,%r8

# qhasm:   carry? f3 += addt0 + carry
# asm 1: adc <addt0=int64#6,<f3=int64#2
# asm 2: adc <addt0=%r9,<f3=%rsi
adc %r9,%rsi

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#7,<addt0=int64#6
# asm 2: cmovc <addt1=%rax,<addt0=%r9
cmovc %rax,%r9

# qhasm:   f0 += addt0
# asm 1: add  <addt0=int64#6,<f0=int64#3
# asm 2: add  <addt0=%r9,<f0=%rdx
add  %r9,%rdx

# qhasm: g0 = f0
# asm 1: mov  <f0=int64#3,>g0=int64#6
# asm 2: mov  <f0=%rdx,>g0=%r9
mov  %rdx,%r9

# qhasm: g1 = f1
# asm 1: mov  <f1=int64#4,>g1=int64#7
# asm 2: mov  <f1=%rcx,>g1=%rax
mov  %rcx,%rax

# qhasm: g2 = f2
# asm 1: mov  <f2=int64#5,>g2=int64#8
# asm 2: mov  <f2=%r8,>g2=%r10
mov  %r8,%r10

# qhasm: g3 = f3
# asm 1: mov  <f3=int64#2,>g3=int64#9
# asm 2: mov  <f3=%rsi,>g3=%r11
mov  %rsi,%r11

# qhasm:   carry? f0 -= c0 
# asm 1: sub  <c0=int64#11,<f0=int64#3
# asm 2: sub  <c0=%r13,<f0=%rdx
sub  %r13,%rdx

# qhasm:   carry? f1 -= c1 - carry
# asm 1: sbb  <c1=int64#12,<f1=int64#4
# asm 2: sbb  <c1=%r14,<f1=%rcx
sbb  %r14,%rcx

# qhasm:   carry? f2 -= c2 - carry
# asm 1: sbb  <c2=int64#13,<f2=int64#5
# asm 2: sbb  <c2=%r15,<f2=%r8
sbb  %r15,%r8

# qhasm:   carry? f3 -= c3 - carry
# asm 1: sbb  <c3=int64#14,<f3=int64#2
# asm 2: sbb  <c3=%rbx,<f3=%rsi
sbb  %rbx,%rsi

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#10
# asm 2: mov  $0,>subt0=%r12
mov  $0,%r12

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#15
# asm 2: mov  $38,>subt1=%rbp
mov  $38,%rbp

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#10,<subt1=int64#15
# asm 2: cmovae <subt0=%r12,<subt1=%rbp
cmovae %r12,%rbp

# qhasm:   carry? f0 -= subt1
# asm 1: sub  <subt1=int64#15,<f0=int64#3
# asm 2: sub  <subt1=%rbp,<f0=%rdx
sub  %rbp,%rdx

# qhasm:   carry? f1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#10,<f1=int64#4
# asm 2: sbb  <subt0=%r12,<f1=%rcx
sbb  %r12,%rcx

# qhasm:   carry? f2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#10,<f2=int64#5
# asm 2: sbb  <subt0=%r12,<f2=%r8
sbb  %r12,%r8

# qhasm:   carry? f3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#10,<f3=int64#2
# asm 2: sbb  <subt0=%r12,<f3=%rsi
sbb  %r12,%rsi

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#15,<subt0=int64#10
# asm 2: cmovc <subt1=%rbp,<subt0=%r12
cmovc %rbp,%r12

# qhasm:   f0 -= subt0
# asm 1: sub  <subt0=int64#10,<f0=int64#3
# asm 2: sub  <subt0=%r12,<f0=%rdx
sub  %r12,%rdx

# qhasm:   carry? g0 += c0 
# asm 1: add  <c0=int64#11,<g0=int64#6
# asm 2: add  <c0=%r13,<g0=%r9
add  %r13,%r9

# qhasm:   carry? g1 += c1 + carry
# asm 1: adc <c1=int64#12,<g1=int64#7
# asm 2: adc <c1=%r14,<g1=%rax
adc %r14,%rax

# qhasm:   carry? g2 += c2 + carry
# asm 1: adc <c2=int64#13,<g2=int64#8
# asm 2: adc <c2=%r15,<g2=%r10
adc %r15,%r10

# qhasm:   carry? g3 += c3 + carry
# asm 1: adc <c3=int64#14,<g3=int64#9
# asm 2: adc <c3=%rbx,<g3=%r11
adc %rbx,%r11

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

# qhasm: *(uint64 *)(rp + 32) = g0
# asm 1: movq   <g0=int64#6,32(<rp=int64#1)
# asm 2: movq   <g0=%r9,32(<rp=%rdi)
movq   %r9,32(%rdi)

# qhasm: *(uint64 *)(rp + 40) = g1
# asm 1: movq   <g1=int64#7,40(<rp=int64#1)
# asm 2: movq   <g1=%rax,40(<rp=%rdi)
movq   %rax,40(%rdi)

# qhasm: *(uint64 *)(rp + 48) = g2
# asm 1: movq   <g2=int64#8,48(<rp=int64#1)
# asm 2: movq   <g2=%r10,48(<rp=%rdi)
movq   %r10,48(%rdi)

# qhasm: *(uint64 *)(rp + 56) = g3
# asm 1: movq   <g3=int64#9,56(<rp=int64#1)
# asm 2: movq   <g3=%r11,56(<rp=%rdi)
movq   %r11,56(%rdi)

# qhasm: *(uint64 *)(rp + 96) = f0
# asm 1: movq   <f0=int64#3,96(<rp=int64#1)
# asm 2: movq   <f0=%rdx,96(<rp=%rdi)
movq   %rdx,96(%rdi)

# qhasm: *(uint64 *)(rp + 104) = f1
# asm 1: movq   <f1=int64#4,104(<rp=int64#1)
# asm 2: movq   <f1=%rcx,104(<rp=%rdi)
movq   %rcx,104(%rdi)

# qhasm: *(uint64 *)(rp + 112) = f2
# asm 1: movq   <f2=int64#5,112(<rp=int64#1)
# asm 2: movq   <f2=%r8,112(<rp=%rdi)
movq   %r8,112(%rdi)

# qhasm: *(uint64 *)(rp + 120) = f3
# asm 1: movq   <f3=int64#2,120(<rp=int64#1)
# asm 2: movq   <f3=%rsi,120(<rp=%rdi)
movq   %rsi,120(%rdi)

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
