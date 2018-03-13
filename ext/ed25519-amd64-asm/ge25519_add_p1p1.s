
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

# qhasm: int64 x0

# qhasm: int64 x1

# qhasm: int64 x2

# qhasm: int64 x3

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

# qhasm: enter crypto_sign_ed25519_amd64_64_ge25519_add_p1p1
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_ge25519_add_p1p1
.globl crypto_sign_ed25519_amd64_64_ge25519_add_p1p1
_crypto_sign_ed25519_amd64_64_ge25519_add_p1p1:
crypto_sign_ed25519_amd64_64_ge25519_add_p1p1:
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

# qhasm:   carry? a0 -= *(uint64 *)(pp + 0) 
# asm 1: subq 0(<pp=int64#2),<a0=int64#3
# asm 2: subq 0(<pp=%rsi),<a0=%rdx
subq 0(%rsi),%rdx

# qhasm:   carry? a1 -= *(uint64 *)(pp + 8) - carry
# asm 1: sbbq 8(<pp=int64#2),<a1=int64#5
# asm 2: sbbq 8(<pp=%rsi),<a1=%r8
sbbq 8(%rsi),%r8

# qhasm:   carry? a2 -= *(uint64 *)(pp + 16) - carry
# asm 1: sbbq 16(<pp=int64#2),<a2=int64#6
# asm 2: sbbq 16(<pp=%rsi),<a2=%r9
sbbq 16(%rsi),%r9

# qhasm:   carry? a3 -= *(uint64 *)(pp + 24) - carry
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

# qhasm:   carry? b0 += *(uint64 *)(pp + 0) 
# asm 1: addq 0(<pp=int64#2),<b0=int64#8
# asm 2: addq 0(<pp=%rsi),<b0=%r10
addq 0(%rsi),%r10

# qhasm:   carry? b1 += *(uint64 *)(pp + 8) + carry
# asm 1: adcq 8(<pp=int64#2),<b1=int64#9
# asm 2: adcq 8(<pp=%rsi),<b1=%r11
adcq 8(%rsi),%r11

# qhasm:   carry? b2 += *(uint64 *)(pp + 16) + carry
# asm 1: adcq 16(<pp=int64#2),<b2=int64#10
# asm 2: adcq 16(<pp=%rsi),<b2=%r12
adcq 16(%rsi),%r12

# qhasm:   carry? b3 += *(uint64 *)(pp + 24) + carry
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

# qhasm: t10 = *(uint64 *)(qp + 32)
# asm 1: movq   32(<qp=int64#4),>t10=int64#3
# asm 2: movq   32(<qp=%rcx),>t10=%rdx
movq   32(%rcx),%rdx

# qhasm: t11 = *(uint64 *)(qp + 40)
# asm 1: movq   40(<qp=int64#4),>t11=int64#5
# asm 2: movq   40(<qp=%rcx),>t11=%r8
movq   40(%rcx),%r8

# qhasm: t12 = *(uint64 *)(qp + 48)
# asm 1: movq   48(<qp=int64#4),>t12=int64#6
# asm 2: movq   48(<qp=%rcx),>t12=%r9
movq   48(%rcx),%r9

# qhasm: t13 = *(uint64 *)(qp + 56)
# asm 1: movq   56(<qp=int64#4),>t13=int64#7
# asm 2: movq   56(<qp=%rcx),>t13=%rax
movq   56(%rcx),%rax

# qhasm: t20 = t10
# asm 1: mov  <t10=int64#3,>t20=int64#8
# asm 2: mov  <t10=%rdx,>t20=%r10
mov  %rdx,%r10

# qhasm: t21 = t11
# asm 1: mov  <t11=int64#5,>t21=int64#9
# asm 2: mov  <t11=%r8,>t21=%r11
mov  %r8,%r11

# qhasm: t22 = t12
# asm 1: mov  <t12=int64#6,>t22=int64#10
# asm 2: mov  <t12=%r9,>t22=%r12
mov  %r9,%r12

# qhasm: t23 = t13
# asm 1: mov  <t13=int64#7,>t23=int64#11
# asm 2: mov  <t13=%rax,>t23=%r13
mov  %rax,%r13

# qhasm:   carry? t10 -= *(uint64 *) (qp + 0) 
# asm 1: subq 0(<qp=int64#4),<t10=int64#3
# asm 2: subq 0(<qp=%rcx),<t10=%rdx
subq 0(%rcx),%rdx

# qhasm:   carry? t11 -= *(uint64 *) (qp + 8) - carry
# asm 1: sbbq 8(<qp=int64#4),<t11=int64#5
# asm 2: sbbq 8(<qp=%rcx),<t11=%r8
sbbq 8(%rcx),%r8

# qhasm:   carry? t12 -= *(uint64 *) (qp + 16) - carry
# asm 1: sbbq 16(<qp=int64#4),<t12=int64#6
# asm 2: sbbq 16(<qp=%rcx),<t12=%r9
sbbq 16(%rcx),%r9

# qhasm:   carry? t13 -= *(uint64 *) (qp + 24) - carry
# asm 1: sbbq 24(<qp=int64#4),<t13=int64#7
# asm 2: sbbq 24(<qp=%rcx),<t13=%rax
sbbq 24(%rcx),%rax

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

# qhasm:   carry? t10 -= subt1
# asm 1: sub  <subt1=int64#13,<t10=int64#3
# asm 2: sub  <subt1=%r15,<t10=%rdx
sub  %r15,%rdx

# qhasm:   carry? t11 -= subt0 - carry
# asm 1: sbb  <subt0=int64#12,<t11=int64#5
# asm 2: sbb  <subt0=%r14,<t11=%r8
sbb  %r14,%r8

# qhasm:   carry? t12 -= subt0 - carry
# asm 1: sbb  <subt0=int64#12,<t12=int64#6
# asm 2: sbb  <subt0=%r14,<t12=%r9
sbb  %r14,%r9

# qhasm:   carry? t13 -= subt0 - carry
# asm 1: sbb  <subt0=int64#12,<t13=int64#7
# asm 2: sbb  <subt0=%r14,<t13=%rax
sbb  %r14,%rax

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#13,<subt0=int64#12
# asm 2: cmovc <subt1=%r15,<subt0=%r14
cmovc %r15,%r14

# qhasm:   t10 -= subt0
# asm 1: sub  <subt0=int64#12,<t10=int64#3
# asm 2: sub  <subt0=%r14,<t10=%rdx
sub  %r14,%rdx

# qhasm:   carry? t20 += *(uint64 *) (qp + 0) 
# asm 1: addq 0(<qp=int64#4),<t20=int64#8
# asm 2: addq 0(<qp=%rcx),<t20=%r10
addq 0(%rcx),%r10

# qhasm:   carry? t21 += *(uint64 *) (qp + 8) + carry
# asm 1: adcq 8(<qp=int64#4),<t21=int64#9
# asm 2: adcq 8(<qp=%rcx),<t21=%r11
adcq 8(%rcx),%r11

# qhasm:   carry? t22 += *(uint64 *) (qp + 16) + carry
# asm 1: adcq 16(<qp=int64#4),<t22=int64#10
# asm 2: adcq 16(<qp=%rcx),<t22=%r12
adcq 16(%rcx),%r12

# qhasm:   carry? t23 += *(uint64 *) (qp + 24) + carry
# asm 1: adcq 24(<qp=int64#4),<t23=int64#11
# asm 2: adcq 24(<qp=%rcx),<t23=%r13
adcq 24(%rcx),%r13

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

# qhasm:   carry? t20 += addt1
# asm 1: add  <addt1=int64#13,<t20=int64#8
# asm 2: add  <addt1=%r15,<t20=%r10
add  %r15,%r10

# qhasm:   carry? t21 += addt0 + carry
# asm 1: adc <addt0=int64#12,<t21=int64#9
# asm 2: adc <addt0=%r14,<t21=%r11
adc %r14,%r11

# qhasm:   carry? t22 += addt0 + carry
# asm 1: adc <addt0=int64#12,<t22=int64#10
# asm 2: adc <addt0=%r14,<t22=%r12
adc %r14,%r12

# qhasm:   carry? t23 += addt0 + carry
# asm 1: adc <addt0=int64#12,<t23=int64#11
# asm 2: adc <addt0=%r14,<t23=%r13
adc %r14,%r13

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#13,<addt0=int64#12
# asm 2: cmovc <addt1=%r15,<addt0=%r14
cmovc %r15,%r14

# qhasm:   t20 += addt0
# asm 1: add  <addt0=int64#12,<t20=int64#8
# asm 2: add  <addt0=%r14,<t20=%r10
add  %r14,%r10

# qhasm: t10_stack = t10
# asm 1: movq <t10=int64#3,>t10_stack=stack64#16
# asm 2: movq <t10=%rdx,>t10_stack=120(%rsp)
movq %rdx,120(%rsp)

# qhasm: t11_stack = t11
# asm 1: movq <t11=int64#5,>t11_stack=stack64#17
# asm 2: movq <t11=%r8,>t11_stack=128(%rsp)
movq %r8,128(%rsp)

# qhasm: t12_stack = t12
# asm 1: movq <t12=int64#6,>t12_stack=stack64#18
# asm 2: movq <t12=%r9,>t12_stack=136(%rsp)
movq %r9,136(%rsp)

# qhasm: t13_stack = t13
# asm 1: movq <t13=int64#7,>t13_stack=stack64#19
# asm 2: movq <t13=%rax,>t13_stack=144(%rsp)
movq %rax,144(%rsp)

# qhasm: t20_stack = t20
# asm 1: movq <t20=int64#8,>t20_stack=stack64#20
# asm 2: movq <t20=%r10,>t20_stack=152(%rsp)
movq %r10,152(%rsp)

# qhasm: t21_stack = t21
# asm 1: movq <t21=int64#9,>t21_stack=stack64#21
# asm 2: movq <t21=%r11,>t21_stack=160(%rsp)
movq %r11,160(%rsp)

# qhasm: t22_stack = t22
# asm 1: movq <t22=int64#10,>t22_stack=stack64#22
# asm 2: movq <t22=%r12,>t22_stack=168(%rsp)
movq %r12,168(%rsp)

# qhasm: t23_stack = t23
# asm 1: movq <t23=int64#11,>t23_stack=stack64#23
# asm 2: movq <t23=%r13,>t23_stack=176(%rsp)
movq %r13,176(%rsp)

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

# qhasm:   mulrax = t10_stack
# asm 1: movq <t10_stack=stack64#16,>mulrax=int64#7
# asm 2: movq <t10_stack=120(%rsp),>mulrax=%rax
movq 120(%rsp),%rax

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

# qhasm:   mulrax = t11_stack
# asm 1: movq <t11_stack=stack64#17,>mulrax=int64#7
# asm 2: movq <t11_stack=128(%rsp),>mulrax=%rax
movq 128(%rsp),%rax

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

# qhasm:   mulrax = t12_stack
# asm 1: movq <t12_stack=stack64#18,>mulrax=int64#7
# asm 2: movq <t12_stack=136(%rsp),>mulrax=%rax
movq 136(%rsp),%rax

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

# qhasm:   mulrax = t13_stack
# asm 1: movq <t13_stack=stack64#19,>mulrax=int64#7
# asm 2: movq <t13_stack=144(%rsp),>mulrax=%rax
movq 144(%rsp),%rax

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

# qhasm:   mulrax = t10_stack
# asm 1: movq <t10_stack=stack64#16,>mulrax=int64#7
# asm 2: movq <t10_stack=120(%rsp),>mulrax=%rax
movq 120(%rsp),%rax

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

# qhasm:   mulrax = t11_stack
# asm 1: movq <t11_stack=stack64#17,>mulrax=int64#7
# asm 2: movq <t11_stack=128(%rsp),>mulrax=%rax
movq 128(%rsp),%rax

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

# qhasm:   mulrax = t12_stack
# asm 1: movq <t12_stack=stack64#18,>mulrax=int64#7
# asm 2: movq <t12_stack=136(%rsp),>mulrax=%rax
movq 136(%rsp),%rax

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

# qhasm:   mulrax = t13_stack
# asm 1: movq <t13_stack=stack64#19,>mulrax=int64#7
# asm 2: movq <t13_stack=144(%rsp),>mulrax=%rax
movq 144(%rsp),%rax

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

# qhasm:   mulrax = t10_stack
# asm 1: movq <t10_stack=stack64#16,>mulrax=int64#7
# asm 2: movq <t10_stack=120(%rsp),>mulrax=%rax
movq 120(%rsp),%rax

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

# qhasm:   mulrax = t11_stack
# asm 1: movq <t11_stack=stack64#17,>mulrax=int64#7
# asm 2: movq <t11_stack=128(%rsp),>mulrax=%rax
movq 128(%rsp),%rax

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

# qhasm:   mulrax = t12_stack
# asm 1: movq <t12_stack=stack64#18,>mulrax=int64#7
# asm 2: movq <t12_stack=136(%rsp),>mulrax=%rax
movq 136(%rsp),%rax

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

# qhasm:   mulrax = t13_stack
# asm 1: movq <t13_stack=stack64#19,>mulrax=int64#7
# asm 2: movq <t13_stack=144(%rsp),>mulrax=%rax
movq 144(%rsp),%rax

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

# qhasm:   mulrax = t10_stack
# asm 1: movq <t10_stack=stack64#16,>mulrax=int64#7
# asm 2: movq <t10_stack=120(%rsp),>mulrax=%rax
movq 120(%rsp),%rax

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

# qhasm:   mulrax = t11_stack
# asm 1: movq <t11_stack=stack64#17,>mulrax=int64#7
# asm 2: movq <t11_stack=128(%rsp),>mulrax=%rax
movq 128(%rsp),%rax

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

# qhasm:   mulrax = t12_stack
# asm 1: movq <t12_stack=stack64#18,>mulrax=int64#7
# asm 2: movq <t12_stack=136(%rsp),>mulrax=%rax
movq 136(%rsp),%rax

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

# qhasm:   mulrax = t13_stack
# asm 1: movq <t13_stack=stack64#19,>mulrax=int64#7
# asm 2: movq <t13_stack=144(%rsp),>mulrax=%rax
movq 144(%rsp),%rax

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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

# qhasm:   mulrax = t20_stack
# asm 1: movq <t20_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <t20_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   rx0 = mulrax
# asm 1: mov  <mulrax=int64#7,>rx0=int64#11
# asm 2: mov  <mulrax=%rax,>rx0=%r13
mov  %rax,%r13

# qhasm:   rx1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>rx1=int64#12
# asm 2: mov  <mulrdx=%rdx,>rx1=%r14
mov  %rdx,%r14

# qhasm:   mulrax = t21_stack
# asm 1: movq <t21_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <t21_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? rx1 += mulrax
# asm 1: add  <mulrax=int64#7,<rx1=int64#12
# asm 2: add  <mulrax=%rax,<rx1=%r14
add  %rax,%r14

# qhasm:   rx2 = 0
# asm 1: mov  $0,>rx2=int64#13
# asm 2: mov  $0,>rx2=%r15
mov  $0,%r15

# qhasm:   rx2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rx2=int64#13
# asm 2: adc <mulrdx=%rdx,<rx2=%r15
adc %rdx,%r15

# qhasm:   mulrax = t22_stack
# asm 1: movq <t22_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <t22_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#13
# asm 2: add  <mulrax=%rax,<rx2=%r15
add  %rax,%r15

# qhasm:   rx3 = 0
# asm 1: mov  $0,>rx3=int64#14
# asm 2: mov  $0,>rx3=%rbx
mov  $0,%rbx

# qhasm:   rx3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rx3=int64#14
# asm 2: adc <mulrdx=%rdx,<rx3=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = t23_stack
# asm 1: movq <t23_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <t23_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#14
# asm 2: add  <mulrax=%rax,<rx3=%rbx
add  %rax,%rbx

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr4=%r8
adc %rdx,%r8

# qhasm:   mulx1 = b1_stack
# asm 1: movq <b1_stack=stack64#13,>mulx1=int64#10
# asm 2: movq <b1_stack=96(%rsp),>mulx1=%r12
movq 96(%rsp),%r12

# qhasm:   mulrax = t20_stack
# asm 1: movq <t20_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <t20_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? rx1 += mulrax
# asm 1: add  <mulrax=int64#7,<rx1=int64#12
# asm 2: add  <mulrax=%rax,<rx1=%r14
add  %rax,%r14

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = t21_stack
# asm 1: movq <t21_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <t21_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#13
# asm 2: add  <mulrax=%rax,<rx2=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx2 += mulc
# asm 1: add  <mulc=int64#15,<rx2=int64#13
# asm 2: add  <mulc=%rbp,<rx2=%r15
add  %rbp,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = t22_stack
# asm 1: movq <t22_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <t22_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? rx3 += mulrax 
# asm 1: add  <mulrax=int64#7,<rx3=int64#14
# asm 2: add  <mulrax=%rax,<rx3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx3 += mulc
# asm 1: add  <mulc=int64#15,<rx3=int64#14
# asm 2: add  <mulc=%rbp,<rx3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = t23_stack
# asm 1: movq <t23_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <t23_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

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

# qhasm:   mulrax = t20_stack
# asm 1: movq <t20_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <t20_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? rx2 += mulrax
# asm 1: add  <mulrax=int64#7,<rx2=int64#13
# asm 2: add  <mulrax=%rax,<rx2=%r15
add  %rax,%r15

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = t21_stack
# asm 1: movq <t21_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <t21_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#14
# asm 2: add  <mulrax=%rax,<rx3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rx3 += mulc
# asm 1: add  <mulc=int64#15,<rx3=int64#14
# asm 2: add  <mulc=%rbp,<rx3=%rbx
add  %rbp,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = t22_stack
# asm 1: movq <t22_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <t22_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

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

# qhasm:   mulrax = t23_stack
# asm 1: movq <t23_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <t23_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

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

# qhasm:   mulrax = t20_stack
# asm 1: movq <t20_stack=stack64#20,>mulrax=int64#7
# asm 2: movq <t20_stack=152(%rsp),>mulrax=%rax
movq 152(%rsp),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#10
# asm 2: mul  <mulx3=%r12
mul  %r12

# qhasm:   carry? rx3 += mulrax
# asm 1: add  <mulrax=int64#7,<rx3=int64#14
# asm 2: add  <mulrax=%rax,<rx3=%rbx
add  %rax,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#15
# asm 2: mov  $0,>mulc=%rbp
mov  $0,%rbp

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#15
# asm 2: adc <mulrdx=%rdx,<mulc=%rbp
adc %rdx,%rbp

# qhasm:   mulrax = t21_stack
# asm 1: movq <t21_stack=stack64#21,>mulrax=int64#7
# asm 2: movq <t21_stack=160(%rsp),>mulrax=%rax
movq 160(%rsp),%rax

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

# qhasm:   mulrax = t22_stack
# asm 1: movq <t22_stack=stack64#22,>mulrax=int64#7
# asm 2: movq <t22_stack=168(%rsp),>mulrax=%rax
movq 168(%rsp),%rax

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

# qhasm:   mulrax = t23_stack
# asm 1: movq <t23_stack=stack64#23,>mulrax=int64#7
# asm 2: movq <t23_stack=176(%rsp),>mulrax=%rax
movq 176(%rsp),%rax

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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

# qhasm:   carry? rx0 += mulr4
# asm 1: add  <mulr4=int64#5,<rx0=int64#11
# asm 2: add  <mulr4=%r8,<rx0=%r13
add  %r8,%r13

# qhasm:   carry? rx1 += mulr5 + carry
# asm 1: adc <mulr5=int64#6,<rx1=int64#12
# asm 2: adc <mulr5=%r9,<rx1=%r14
adc %r9,%r14

# qhasm:   carry? rx2 += mulr6 + carry
# asm 1: adc <mulr6=int64#8,<rx2=int64#13
# asm 2: adc <mulr6=%r10,<rx2=%r15
adc %r10,%r15

# qhasm:   carry? rx3 += mulr7 + carry
# asm 1: adc <mulr7=int64#9,<rx3=int64#14
# asm 2: adc <mulr7=%r11,<rx3=%rbx
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

# qhasm:   carry? rx0 += mulr8
# asm 1: add  <mulr8=int64#5,<rx0=int64#11
# asm 2: add  <mulr8=%r8,<rx0=%r13
add  %r8,%r13

# qhasm:   carry? rx1 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<rx1=int64#12
# asm 2: adc <mulzero=%rdx,<rx1=%r14
adc %rdx,%r14

# qhasm:   carry? rx2 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<rx2=int64#13
# asm 2: adc <mulzero=%rdx,<rx2=%r15
adc %rdx,%r15

# qhasm:   carry? rx3 += mulzero + carry
# asm 1: adc <mulzero=int64#3,<rx3=int64#14
# asm 2: adc <mulzero=%rdx,<rx3=%rbx
adc %rdx,%rbx

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#3,<mulzero=int64#3
# asm 2: adc <mulzero=%rdx,<mulzero=%rdx
adc %rdx,%rdx

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#3,>mulzero=int64#3
# asm 2: imulq  $38,<mulzero=%rdx,>mulzero=%rdx
imulq  $38,%rdx,%rdx

# qhasm:   rx0 += mulzero
# asm 1: add  <mulzero=int64#3,<rx0=int64#11
# asm 2: add  <mulzero=%rdx,<rx0=%r13
add  %rdx,%r13

# qhasm: ry0 = rx0
# asm 1: mov  <rx0=int64#11,>ry0=int64#3
# asm 2: mov  <rx0=%r13,>ry0=%rdx
mov  %r13,%rdx

# qhasm: ry1 = rx1
# asm 1: mov  <rx1=int64#12,>ry1=int64#5
# asm 2: mov  <rx1=%r14,>ry1=%r8
mov  %r14,%r8

# qhasm: ry2 = rx2
# asm 1: mov  <rx2=int64#13,>ry2=int64#6
# asm 2: mov  <rx2=%r15,>ry2=%r9
mov  %r15,%r9

# qhasm: ry3 = rx3
# asm 1: mov  <rx3=int64#14,>ry3=int64#7
# asm 2: mov  <rx3=%rbx,>ry3=%rax
mov  %rbx,%rax

# qhasm:   carry? ry0 += a0_stack 
# asm 1: addq <a0_stack=stack64#8,<ry0=int64#3
# asm 2: addq <a0_stack=56(%rsp),<ry0=%rdx
addq 56(%rsp),%rdx

# qhasm:   carry? ry1 += a1_stack + carry
# asm 1: adcq <a1_stack=stack64#9,<ry1=int64#5
# asm 2: adcq <a1_stack=64(%rsp),<ry1=%r8
adcq 64(%rsp),%r8

# qhasm:   carry? ry2 += a2_stack + carry
# asm 1: adcq <a2_stack=stack64#10,<ry2=int64#6
# asm 2: adcq <a2_stack=72(%rsp),<ry2=%r9
adcq 72(%rsp),%r9

# qhasm:   carry? ry3 += a3_stack + carry
# asm 1: adcq <a3_stack=stack64#11,<ry3=int64#7
# asm 2: adcq <a3_stack=80(%rsp),<ry3=%rax
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

# qhasm:   carry? ry0 += addt1
# asm 1: add  <addt1=int64#9,<ry0=int64#3
# asm 2: add  <addt1=%r11,<ry0=%rdx
add  %r11,%rdx

# qhasm:   carry? ry1 += addt0 + carry
# asm 1: adc <addt0=int64#8,<ry1=int64#5
# asm 2: adc <addt0=%r10,<ry1=%r8
adc %r10,%r8

# qhasm:   carry? ry2 += addt0 + carry
# asm 1: adc <addt0=int64#8,<ry2=int64#6
# asm 2: adc <addt0=%r10,<ry2=%r9
adc %r10,%r9

# qhasm:   carry? ry3 += addt0 + carry
# asm 1: adc <addt0=int64#8,<ry3=int64#7
# asm 2: adc <addt0=%r10,<ry3=%rax
adc %r10,%rax

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#9,<addt0=int64#8
# asm 2: cmovc <addt1=%r11,<addt0=%r10
cmovc %r11,%r10

# qhasm:   ry0 += addt0
# asm 1: add  <addt0=int64#8,<ry0=int64#3
# asm 2: add  <addt0=%r10,<ry0=%rdx
add  %r10,%rdx

# qhasm:   carry? rx0 -= a0_stack 
# asm 1: subq <a0_stack=stack64#8,<rx0=int64#11
# asm 2: subq <a0_stack=56(%rsp),<rx0=%r13
subq 56(%rsp),%r13

# qhasm:   carry? rx1 -= a1_stack - carry
# asm 1: sbbq <a1_stack=stack64#9,<rx1=int64#12
# asm 2: sbbq <a1_stack=64(%rsp),<rx1=%r14
sbbq 64(%rsp),%r14

# qhasm:   carry? rx2 -= a2_stack - carry
# asm 1: sbbq <a2_stack=stack64#10,<rx2=int64#13
# asm 2: sbbq <a2_stack=72(%rsp),<rx2=%r15
sbbq 72(%rsp),%r15

# qhasm:   carry? rx3 -= a3_stack - carry
# asm 1: sbbq <a3_stack=stack64#11,<rx3=int64#14
# asm 2: sbbq <a3_stack=80(%rsp),<rx3=%rbx
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

# qhasm:   carry? rx0 -= subt1
# asm 1: sub  <subt1=int64#9,<rx0=int64#11
# asm 2: sub  <subt1=%r11,<rx0=%r13
sub  %r11,%r13

# qhasm:   carry? rx1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#8,<rx1=int64#12
# asm 2: sbb  <subt0=%r10,<rx1=%r14
sbb  %r10,%r14

# qhasm:   carry? rx2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#8,<rx2=int64#13
# asm 2: sbb  <subt0=%r10,<rx2=%r15
sbb  %r10,%r15

# qhasm:   carry? rx3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#8,<rx3=int64#14
# asm 2: sbb  <subt0=%r10,<rx3=%rbx
sbb  %r10,%rbx

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#9,<subt0=int64#8
# asm 2: cmovc <subt1=%r11,<subt0=%r10
cmovc %r11,%r10

# qhasm:   rx0 -= subt0
# asm 1: sub  <subt0=int64#8,<rx0=int64#11
# asm 2: sub  <subt0=%r10,<rx0=%r13
sub  %r10,%r13

# qhasm: *(uint64 *) (rp + 0) = rx0
# asm 1: movq   <rx0=int64#11,0(<rp=int64#1)
# asm 2: movq   <rx0=%r13,0(<rp=%rdi)
movq   %r13,0(%rdi)

# qhasm: *(uint64 *) (rp + 8) = rx1
# asm 1: movq   <rx1=int64#12,8(<rp=int64#1)
# asm 2: movq   <rx1=%r14,8(<rp=%rdi)
movq   %r14,8(%rdi)

# qhasm: *(uint64 *) (rp + 16) = rx2
# asm 1: movq   <rx2=int64#13,16(<rp=int64#1)
# asm 2: movq   <rx2=%r15,16(<rp=%rdi)
movq   %r15,16(%rdi)

# qhasm: *(uint64 *) (rp + 24) = rx3
# asm 1: movq   <rx3=int64#14,24(<rp=int64#1)
# asm 2: movq   <rx3=%rbx,24(<rp=%rdi)
movq   %rbx,24(%rdi)

# qhasm: *(uint64 *) (rp + 64) = ry0
# asm 1: movq   <ry0=int64#3,64(<rp=int64#1)
# asm 2: movq   <ry0=%rdx,64(<rp=%rdi)
movq   %rdx,64(%rdi)

# qhasm: *(uint64 *) (rp + 72) = ry1
# asm 1: movq   <ry1=int64#5,72(<rp=int64#1)
# asm 2: movq   <ry1=%r8,72(<rp=%rdi)
movq   %r8,72(%rdi)

# qhasm: *(uint64 *) (rp + 80) = ry2
# asm 1: movq   <ry2=int64#6,80(<rp=int64#1)
# asm 2: movq   <ry2=%r9,80(<rp=%rdi)
movq   %r9,80(%rdi)

# qhasm: *(uint64 *) (rp + 88) = ry3
# asm 1: movq   <ry3=int64#7,88(<rp=int64#1)
# asm 2: movq   <ry3=%rax,88(<rp=%rdi)
movq   %rax,88(%rdi)

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

# qhasm:   mulrax = *(uint64 *)(qp + 96)
# asm 1: movq   96(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   96(<qp=%rcx),>mulrax=%rax
movq   96(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 104)
# asm 1: movq   104(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   104(<qp=%rcx),>mulrax=%rax
movq   104(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 112)
# asm 1: movq   112(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   112(<qp=%rcx),>mulrax=%rax
movq   112(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 120)
# asm 1: movq   120(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   120(<qp=%rcx),>mulrax=%rax
movq   120(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 96)
# asm 1: movq   96(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   96(<qp=%rcx),>mulrax=%rax
movq   96(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 104)
# asm 1: movq   104(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   104(<qp=%rcx),>mulrax=%rax
movq   104(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 112)
# asm 1: movq   112(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   112(<qp=%rcx),>mulrax=%rax
movq   112(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 120)
# asm 1: movq   120(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   120(<qp=%rcx),>mulrax=%rax
movq   120(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 96)
# asm 1: movq   96(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   96(<qp=%rcx),>mulrax=%rax
movq   96(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 104)
# asm 1: movq   104(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   104(<qp=%rcx),>mulrax=%rax
movq   104(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 112)
# asm 1: movq   112(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   112(<qp=%rcx),>mulrax=%rax
movq   112(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 120)
# asm 1: movq   120(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   120(<qp=%rcx),>mulrax=%rax
movq   120(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 96)
# asm 1: movq   96(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   96(<qp=%rcx),>mulrax=%rax
movq   96(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 104)
# asm 1: movq   104(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   104(<qp=%rcx),>mulrax=%rax
movq   104(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 112)
# asm 1: movq   112(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   112(<qp=%rcx),>mulrax=%rax
movq   112(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 120)
# asm 1: movq   120(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   120(<qp=%rcx),>mulrax=%rax
movq   120(%rcx),%rax

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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

# qhasm:   carry? c0 += mulr4
# asm 1: add  <mulr4=int64#5,<c0=int64#11
# asm 2: add  <mulr4=%r8,<c0=%r13
add  %r8,%r13

# qhasm:   carry? c1 += mulr5 + carry
# asm 1: adc <mulr5=int64#6,<c1=int64#12
# asm 2: adc <mulr5=%r9,<c1=%r14
adc %r9,%r14

# qhasm:   carry? c2 += mulr6 + carry
# asm 1: adc <mulr6=int64#8,<c2=int64#13
# asm 2: adc <mulr6=%r10,<c2=%r15
adc %r10,%r15

# qhasm:   carry? c3 += mulr7 + carry
# asm 1: adc <mulr7=int64#9,<c3=int64#14
# asm 2: adc <mulr7=%r11,<c3=%rbx
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

# qhasm:   carry? c0 += mulr8
# asm 1: add  <mulr8=int64#5,<c0=int64#11
# asm 2: add  <mulr8=%r8,<c0=%r13
add  %r8,%r13

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

# qhasm: c0_stack = c0
# asm 1: movq <c0=int64#11,>c0_stack=stack64#8
# asm 2: movq <c0=%r13,>c0_stack=56(%rsp)
movq %r13,56(%rsp)

# qhasm: c1_stack = c1
# asm 1: movq <c1=int64#12,>c1_stack=stack64#9
# asm 2: movq <c1=%r14,>c1_stack=64(%rsp)
movq %r14,64(%rsp)

# qhasm: c2_stack = c2
# asm 1: movq <c2=int64#13,>c2_stack=stack64#10
# asm 2: movq <c2=%r15,>c2_stack=72(%rsp)
movq %r15,72(%rsp)

# qhasm: c3_stack = c3
# asm 1: movq <c3=int64#14,>c3_stack=stack64#11
# asm 2: movq <c3=%rbx,>c3_stack=80(%rsp)
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

# qhasm:   mulx0 = c0_stack
# asm 1: movq <c0_stack=stack64#8,>mulx0=int64#10
# asm 2: movq <c0_stack=56(%rsp),>mulx0=%r12
movq 56(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D0
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D0,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D0,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D0,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D1
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D1,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D1,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D1,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D2
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D2,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D2,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D2,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D3
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D3,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D3,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D3,%rax

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

# qhasm:   mulx1 = c1_stack
# asm 1: movq <c1_stack=stack64#9,>mulx1=int64#10
# asm 2: movq <c1_stack=64(%rsp),>mulx1=%r12
movq 64(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D0
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D0,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D0,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D0,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D1
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D1,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D1,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D1,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D2
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D2,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D2,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D2,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D3
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D3,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D3,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D3,%rax

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

# qhasm:   mulx2 = c2_stack
# asm 1: movq <c2_stack=stack64#10,>mulx2=int64#10
# asm 2: movq <c2_stack=72(%rsp),>mulx2=%r12
movq 72(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D0
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D0,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D0,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D0,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D1
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D1,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D1,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D1,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D2
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D2,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D2,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D2,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D3
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D3,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D3,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D3,%rax

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

# qhasm:   mulx3 = c3_stack
# asm 1: movq <c3_stack=stack64#11,>mulx3=int64#10
# asm 2: movq <c3_stack=80(%rsp),>mulx3=%r12
movq 80(%rsp),%r12

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D0
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D0,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D0,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D0,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D1
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D1,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D1,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D1,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D2
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D2,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D2,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D2,%rax

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

# qhasm:   mulrax = *(uint64 *)&crypto_sign_ed25519_amd64_64_EC2D3
# asm 1: movq crypto_sign_ed25519_amd64_64_EC2D3,>mulrax=int64#7
# asm 2: movq crypto_sign_ed25519_amd64_64_EC2D3,>mulrax=%rax
movq crypto_sign_ed25519_amd64_64_EC2D3,%rax

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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
mulq  crypto_sign_ed25519_amd64_64_38

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

# qhasm:   carry? c0 += mulr4
# asm 1: add  <mulr4=int64#5,<c0=int64#11
# asm 2: add  <mulr4=%r8,<c0=%r13
add  %r8,%r13

# qhasm:   carry? c1 += mulr5 + carry
# asm 1: adc <mulr5=int64#6,<c1=int64#12
# asm 2: adc <mulr5=%r9,<c1=%r14
adc %r9,%r14

# qhasm:   carry? c2 += mulr6 + carry
# asm 1: adc <mulr6=int64#8,<c2=int64#13
# asm 2: adc <mulr6=%r10,<c2=%r15
adc %r10,%r15

# qhasm:   carry? c3 += mulr7 + carry
# asm 1: adc <mulr7=int64#9,<c3=int64#14
# asm 2: adc <mulr7=%r11,<c3=%rbx
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

# qhasm:   carry? c0 += mulr8
# asm 1: add  <mulr8=int64#5,<c0=int64#11
# asm 2: add  <mulr8=%r8,<c0=%r13
add  %r8,%r13

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

# qhasm: c0_stack = c0
# asm 1: movq <c0=int64#11,>c0_stack=stack64#8
# asm 2: movq <c0=%r13,>c0_stack=56(%rsp)
movq %r13,56(%rsp)

# qhasm: c1_stack = c1
# asm 1: movq <c1=int64#12,>c1_stack=stack64#9
# asm 2: movq <c1=%r14,>c1_stack=64(%rsp)
movq %r14,64(%rsp)

# qhasm: c2_stack = c2
# asm 1: movq <c2=int64#13,>c2_stack=stack64#10
# asm 2: movq <c2=%r15,>c2_stack=72(%rsp)
movq %r15,72(%rsp)

# qhasm: c3_stack = c3
# asm 1: movq <c3=int64#14,>c3_stack=stack64#11
# asm 2: movq <c3=%rbx,>c3_stack=80(%rsp)
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

# qhasm:   mulx0 = *(uint64 *)(pp + 64)
# asm 1: movq   64(<pp=int64#2),>mulx0=int64#10
# asm 2: movq   64(<pp=%rsi),>mulx0=%r12
movq   64(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   64(<qp=%rcx),>mulrax=%rax
movq   64(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   rt0 = mulrax
# asm 1: mov  <mulrax=int64#7,>rt0=int64#11
# asm 2: mov  <mulrax=%rax,>rt0=%r13
mov  %rax,%r13

# qhasm:   rt1 = mulrdx
# asm 1: mov  <mulrdx=int64#3,>rt1=int64#12
# asm 2: mov  <mulrdx=%rdx,>rt1=%r14
mov  %rdx,%r14

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   72(<qp=%rcx),>mulrax=%rax
movq   72(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? rt1 += mulrax
# asm 1: add  <mulrax=int64#7,<rt1=int64#12
# asm 2: add  <mulrax=%rax,<rt1=%r14
add  %rax,%r14

# qhasm:   rt2 = 0
# asm 1: mov  $0,>rt2=int64#13
# asm 2: mov  $0,>rt2=%r15
mov  $0,%r15

# qhasm:   rt2 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rt2=int64#13
# asm 2: adc <mulrdx=%rdx,<rt2=%r15
adc %rdx,%r15

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   80(<qp=%rcx),>mulrax=%rax
movq   80(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? rt2 += mulrax
# asm 1: add  <mulrax=int64#7,<rt2=int64#13
# asm 2: add  <mulrax=%rax,<rt2=%r15
add  %rax,%r15

# qhasm:   rt3 = 0
# asm 1: mov  $0,>rt3=int64#14
# asm 2: mov  $0,>rt3=%rbx
mov  $0,%rbx

# qhasm:   rt3 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<rt3=int64#14
# asm 2: adc <mulrdx=%rdx,<rt3=%rbx
adc %rdx,%rbx

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   88(<qp=%rcx),>mulrax=%rax
movq   88(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx0
# asm 1: mul  <mulx0=int64#10
# asm 2: mul  <mulx0=%r12
mul  %r12

# qhasm:   carry? rt3 += mulrax
# asm 1: add  <mulrax=int64#7,<rt3=int64#14
# asm 2: add  <mulrax=%rax,<rt3=%rbx
add  %rax,%rbx

# qhasm:   mulr4 += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulr4=int64#5
# asm 2: adc <mulrdx=%rdx,<mulr4=%r8
adc %rdx,%r8

# qhasm:   mulx1 = *(uint64 *)(pp + 72)
# asm 1: movq   72(<pp=int64#2),>mulx1=int64#10
# asm 2: movq   72(<pp=%rsi),>mulx1=%r12
movq   72(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   64(<qp=%rcx),>mulrax=%rax
movq   64(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx1
# asm 1: mul  <mulx1=int64#10
# asm 2: mul  <mulx1=%r12
mul  %r12

# qhasm:   carry? rt1 += mulrax
# asm 1: add  <mulrax=int64#7,<rt1=int64#12
# asm 2: add  <mulrax=%rax,<rt1=%r14
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

# qhasm:   carry? rt2 += mulrax
# asm 1: add  <mulrax=int64#7,<rt2=int64#13
# asm 2: add  <mulrax=%rax,<rt2=%r15
add  %rax,%r15

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rt2 += mulc
# asm 1: add  <mulc=int64#15,<rt2=int64#13
# asm 2: add  <mulc=%rbp,<rt2=%r15
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

# qhasm:   carry? rt3 += mulrax 
# asm 1: add  <mulrax=int64#7,<rt3=int64#14
# asm 2: add  <mulrax=%rax,<rt3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rt3 += mulc
# asm 1: add  <mulc=int64#15,<rt3=int64#14
# asm 2: add  <mulc=%rbp,<rt3=%rbx
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

# qhasm:   mulx2 = *(uint64 *)(pp + 80)
# asm 1: movq   80(<pp=int64#2),>mulx2=int64#10
# asm 2: movq   80(<pp=%rsi),>mulx2=%r12
movq   80(%rsi),%r12

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   64(<qp=%rcx),>mulrax=%rax
movq   64(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx2
# asm 1: mul  <mulx2=int64#10
# asm 2: mul  <mulx2=%r12
mul  %r12

# qhasm:   carry? rt2 += mulrax
# asm 1: add  <mulrax=int64#7,<rt2=int64#13
# asm 2: add  <mulrax=%rax,<rt2=%r15
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

# qhasm:   carry? rt3 += mulrax
# asm 1: add  <mulrax=int64#7,<rt3=int64#14
# asm 2: add  <mulrax=%rax,<rt3=%rbx
add  %rax,%rbx

# qhasm:   mulrdx += 0 + carry
# asm 1: adc $0,<mulrdx=int64#3
# asm 2: adc $0,<mulrdx=%rdx
adc $0,%rdx

# qhasm:   carry? rt3 += mulc
# asm 1: add  <mulc=int64#15,<rt3=int64#14
# asm 2: add  <mulc=%rbp,<rt3=%rbx
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

# qhasm:   mulx3 = *(uint64 *)(pp + 88)
# asm 1: movq   88(<pp=int64#2),>mulx3=int64#2
# asm 2: movq   88(<pp=%rsi),>mulx3=%rsi
movq   88(%rsi),%rsi

# qhasm:   mulrax = *(uint64 *)(qp + 64)
# asm 1: movq   64(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   64(<qp=%rcx),>mulrax=%rax
movq   64(%rcx),%rax

# qhasm:   (uint128) mulrdx mulrax = mulrax * mulx3
# asm 1: mul  <mulx3=int64#2
# asm 2: mul  <mulx3=%rsi
mul  %rsi

# qhasm:   carry? rt3 += mulrax
# asm 1: add  <mulrax=int64#7,<rt3=int64#14
# asm 2: add  <mulrax=%rax,<rt3=%rbx
add  %rax,%rbx

# qhasm:   mulc = 0
# asm 1: mov  $0,>mulc=int64#10
# asm 2: mov  $0,>mulc=%r12
mov  $0,%r12

# qhasm:   mulc += mulrdx + carry
# asm 1: adc <mulrdx=int64#3,<mulc=int64#10
# asm 2: adc <mulrdx=%rdx,<mulc=%r12
adc %rdx,%r12

# qhasm:   mulrax = *(uint64 *)(qp + 72)
# asm 1: movq   72(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   72(<qp=%rcx),>mulrax=%rax
movq   72(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 80)
# asm 1: movq   80(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   80(<qp=%rcx),>mulrax=%rax
movq   80(%rcx),%rax

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

# qhasm:   mulrax = *(uint64 *)(qp + 88)
# asm 1: movq   88(<qp=int64#4),>mulrax=int64#7
# asm 2: movq   88(<qp=%rcx),>mulrax=%rax
movq   88(%rcx),%rax

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

# qhasm:   carry? rt0 += mulr4
# asm 1: add  <mulr4=int64#2,<rt0=int64#11
# asm 2: add  <mulr4=%rsi,<rt0=%r13
add  %rsi,%r13

# qhasm:   carry? rt1 += mulr5 + carry
# asm 1: adc <mulr5=int64#4,<rt1=int64#12
# asm 2: adc <mulr5=%rcx,<rt1=%r14
adc %rcx,%r14

# qhasm:   carry? rt2 += mulr6 + carry
# asm 1: adc <mulr6=int64#5,<rt2=int64#13
# asm 2: adc <mulr6=%r8,<rt2=%r15
adc %r8,%r15

# qhasm:   carry? rt3 += mulr7 + carry
# asm 1: adc <mulr7=int64#6,<rt3=int64#14
# asm 2: adc <mulr7=%r9,<rt3=%rbx
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

# qhasm:   carry? rt0 += mulr8
# asm 1: add  <mulr8=int64#3,<rt0=int64#11
# asm 2: add  <mulr8=%rdx,<rt0=%r13
add  %rdx,%r13

# qhasm:   carry? rt1 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rt1=int64#12
# asm 2: adc <mulzero=%rsi,<rt1=%r14
adc %rsi,%r14

# qhasm:   carry? rt2 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rt2=int64#13
# asm 2: adc <mulzero=%rsi,<rt2=%r15
adc %rsi,%r15

# qhasm:   carry? rt3 += mulzero + carry
# asm 1: adc <mulzero=int64#2,<rt3=int64#14
# asm 2: adc <mulzero=%rsi,<rt3=%rbx
adc %rsi,%rbx

# qhasm:   mulzero += mulzero + carry
# asm 1: adc <mulzero=int64#2,<mulzero=int64#2
# asm 2: adc <mulzero=%rsi,<mulzero=%rsi
adc %rsi,%rsi

# qhasm:   mulzero *= 38
# asm 1: imulq  $38,<mulzero=int64#2,>mulzero=int64#2
# asm 2: imulq  $38,<mulzero=%rsi,>mulzero=%rsi
imulq  $38,%rsi,%rsi

# qhasm:   rt0 += mulzero
# asm 1: add  <mulzero=int64#2,<rt0=int64#11
# asm 2: add  <mulzero=%rsi,<rt0=%r13
add  %rsi,%r13

# qhasm:   carry? rt0 += rt0 
# asm 1: add  <rt0=int64#11,<rt0=int64#11
# asm 2: add  <rt0=%r13,<rt0=%r13
add  %r13,%r13

# qhasm:   carry? rt1 += rt1 + carry
# asm 1: adc <rt1=int64#12,<rt1=int64#12
# asm 2: adc <rt1=%r14,<rt1=%r14
adc %r14,%r14

# qhasm:   carry? rt2 += rt2 + carry
# asm 1: adc <rt2=int64#13,<rt2=int64#13
# asm 2: adc <rt2=%r15,<rt2=%r15
adc %r15,%r15

# qhasm:   carry? rt3 += rt3 + carry
# asm 1: adc <rt3=int64#14,<rt3=int64#14
# asm 2: adc <rt3=%rbx,<rt3=%rbx
adc %rbx,%rbx

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

# qhasm:   carry? rt0 += addt1
# asm 1: add  <addt1=int64#3,<rt0=int64#11
# asm 2: add  <addt1=%rdx,<rt0=%r13
add  %rdx,%r13

# qhasm:   carry? rt1 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rt1=int64#12
# asm 2: adc <addt0=%rsi,<rt1=%r14
adc %rsi,%r14

# qhasm:   carry? rt2 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rt2=int64#13
# asm 2: adc <addt0=%rsi,<rt2=%r15
adc %rsi,%r15

# qhasm:   carry? rt3 += addt0 + carry
# asm 1: adc <addt0=int64#2,<rt3=int64#14
# asm 2: adc <addt0=%rsi,<rt3=%rbx
adc %rsi,%rbx

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#3,<addt0=int64#2
# asm 2: cmovc <addt1=%rdx,<addt0=%rsi
cmovc %rdx,%rsi

# qhasm:   rt0 += addt0
# asm 1: add  <addt0=int64#2,<rt0=int64#11
# asm 2: add  <addt0=%rsi,<rt0=%r13
add  %rsi,%r13

# qhasm: rz0 = rt0
# asm 1: mov  <rt0=int64#11,>rz0=int64#2
# asm 2: mov  <rt0=%r13,>rz0=%rsi
mov  %r13,%rsi

# qhasm: rz1 = rt1
# asm 1: mov  <rt1=int64#12,>rz1=int64#3
# asm 2: mov  <rt1=%r14,>rz1=%rdx
mov  %r14,%rdx

# qhasm: rz2 = rt2
# asm 1: mov  <rt2=int64#13,>rz2=int64#4
# asm 2: mov  <rt2=%r15,>rz2=%rcx
mov  %r15,%rcx

# qhasm: rz3 = rt3
# asm 1: mov  <rt3=int64#14,>rz3=int64#5
# asm 2: mov  <rt3=%rbx,>rz3=%r8
mov  %rbx,%r8

# qhasm:   carry? rz0 += c0_stack 
# asm 1: addq <c0_stack=stack64#8,<rz0=int64#2
# asm 2: addq <c0_stack=56(%rsp),<rz0=%rsi
addq 56(%rsp),%rsi

# qhasm:   carry? rz1 += c1_stack + carry
# asm 1: adcq <c1_stack=stack64#9,<rz1=int64#3
# asm 2: adcq <c1_stack=64(%rsp),<rz1=%rdx
adcq 64(%rsp),%rdx

# qhasm:   carry? rz2 += c2_stack + carry
# asm 1: adcq <c2_stack=stack64#10,<rz2=int64#4
# asm 2: adcq <c2_stack=72(%rsp),<rz2=%rcx
adcq 72(%rsp),%rcx

# qhasm:   carry? rz3 += c3_stack + carry
# asm 1: adcq <c3_stack=stack64#11,<rz3=int64#5
# asm 2: adcq <c3_stack=80(%rsp),<rz3=%r8
adcq 80(%rsp),%r8

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

# qhasm:   carry? rz0 += addt1
# asm 1: add  <addt1=int64#7,<rz0=int64#2
# asm 2: add  <addt1=%rax,<rz0=%rsi
add  %rax,%rsi

# qhasm:   carry? rz1 += addt0 + carry
# asm 1: adc <addt0=int64#6,<rz1=int64#3
# asm 2: adc <addt0=%r9,<rz1=%rdx
adc %r9,%rdx

# qhasm:   carry? rz2 += addt0 + carry
# asm 1: adc <addt0=int64#6,<rz2=int64#4
# asm 2: adc <addt0=%r9,<rz2=%rcx
adc %r9,%rcx

# qhasm:   carry? rz3 += addt0 + carry
# asm 1: adc <addt0=int64#6,<rz3=int64#5
# asm 2: adc <addt0=%r9,<rz3=%r8
adc %r9,%r8

# qhasm:   addt0 = addt1 if carry
# asm 1: cmovc <addt1=int64#7,<addt0=int64#6
# asm 2: cmovc <addt1=%rax,<addt0=%r9
cmovc %rax,%r9

# qhasm:   rz0 += addt0
# asm 1: add  <addt0=int64#6,<rz0=int64#2
# asm 2: add  <addt0=%r9,<rz0=%rsi
add  %r9,%rsi

# qhasm:   carry? rt0 -= c0_stack 
# asm 1: subq <c0_stack=stack64#8,<rt0=int64#11
# asm 2: subq <c0_stack=56(%rsp),<rt0=%r13
subq 56(%rsp),%r13

# qhasm:   carry? rt1 -= c1_stack - carry
# asm 1: sbbq <c1_stack=stack64#9,<rt1=int64#12
# asm 2: sbbq <c1_stack=64(%rsp),<rt1=%r14
sbbq 64(%rsp),%r14

# qhasm:   carry? rt2 -= c2_stack - carry
# asm 1: sbbq <c2_stack=stack64#10,<rt2=int64#13
# asm 2: sbbq <c2_stack=72(%rsp),<rt2=%r15
sbbq 72(%rsp),%r15

# qhasm:   carry? rt3 -= c3_stack - carry
# asm 1: sbbq <c3_stack=stack64#11,<rt3=int64#14
# asm 2: sbbq <c3_stack=80(%rsp),<rt3=%rbx
sbbq 80(%rsp),%rbx

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#6
# asm 2: mov  $0,>subt0=%r9
mov  $0,%r9

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#7
# asm 2: mov  $38,>subt1=%rax
mov  $38,%rax

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#6,<subt1=int64#7
# asm 2: cmovae <subt0=%r9,<subt1=%rax
cmovae %r9,%rax

# qhasm:   carry? rt0 -= subt1
# asm 1: sub  <subt1=int64#7,<rt0=int64#11
# asm 2: sub  <subt1=%rax,<rt0=%r13
sub  %rax,%r13

# qhasm:   carry? rt1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#6,<rt1=int64#12
# asm 2: sbb  <subt0=%r9,<rt1=%r14
sbb  %r9,%r14

# qhasm:   carry? rt2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#6,<rt2=int64#13
# asm 2: sbb  <subt0=%r9,<rt2=%r15
sbb  %r9,%r15

# qhasm:   carry? rt3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#6,<rt3=int64#14
# asm 2: sbb  <subt0=%r9,<rt3=%rbx
sbb  %r9,%rbx

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#7,<subt0=int64#6
# asm 2: cmovc <subt1=%rax,<subt0=%r9
cmovc %rax,%r9

# qhasm:   rt0 -= subt0
# asm 1: sub  <subt0=int64#6,<rt0=int64#11
# asm 2: sub  <subt0=%r9,<rt0=%r13
sub  %r9,%r13

# qhasm: *(uint64 *)(rp + 32) = rz0
# asm 1: movq   <rz0=int64#2,32(<rp=int64#1)
# asm 2: movq   <rz0=%rsi,32(<rp=%rdi)
movq   %rsi,32(%rdi)

# qhasm: *(uint64 *)(rp + 40) = rz1
# asm 1: movq   <rz1=int64#3,40(<rp=int64#1)
# asm 2: movq   <rz1=%rdx,40(<rp=%rdi)
movq   %rdx,40(%rdi)

# qhasm: *(uint64 *)(rp + 48) = rz2
# asm 1: movq   <rz2=int64#4,48(<rp=int64#1)
# asm 2: movq   <rz2=%rcx,48(<rp=%rdi)
movq   %rcx,48(%rdi)

# qhasm: *(uint64 *)(rp + 56) = rz3
# asm 1: movq   <rz3=int64#5,56(<rp=int64#1)
# asm 2: movq   <rz3=%r8,56(<rp=%rdi)
movq   %r8,56(%rdi)

# qhasm: *(uint64 *)(rp + 96) = rt0
# asm 1: movq   <rt0=int64#11,96(<rp=int64#1)
# asm 2: movq   <rt0=%r13,96(<rp=%rdi)
movq   %r13,96(%rdi)

# qhasm: *(uint64 *)(rp + 104) = rt1
# asm 1: movq   <rt1=int64#12,104(<rp=int64#1)
# asm 2: movq   <rt1=%r14,104(<rp=%rdi)
movq   %r14,104(%rdi)

# qhasm: *(uint64 *)(rp + 112) = rt2
# asm 1: movq   <rt2=int64#13,112(<rp=int64#1)
# asm 2: movq   <rt2=%r15,112(<rp=%rdi)
movq   %r15,112(%rdi)

# qhasm: *(uint64 *)(rp + 120) = rt3
# asm 1: movq   <rt3=int64#14,120(<rp=int64#1)
# asm 2: movq   <rt3=%rbx,120(<rp=%rdi)
movq   %rbx,120(%rdi)

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
