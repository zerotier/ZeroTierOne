
# qhasm: int64 tp

# qhasm: int64 pos

# qhasm: int64 b

# qhasm: int64 basep

# qhasm: input tp

# qhasm: input pos

# qhasm: input b

# qhasm: input basep

# qhasm: int64 mask

# qhasm: int64 u

# qhasm: int64 tysubx0

# qhasm: int64 tysubx1

# qhasm: int64 tysubx2

# qhasm: int64 tysubx3

# qhasm: int64 txaddy0

# qhasm: int64 txaddy1

# qhasm: int64 txaddy2

# qhasm: int64 txaddy3

# qhasm: int64 tt2d0

# qhasm: int64 tt2d1

# qhasm: int64 tt2d2

# qhasm: int64 tt2d3

# qhasm: int64 tt0

# qhasm: int64 tt1

# qhasm: int64 tt2

# qhasm: int64 tt3

# qhasm: int64 subt0

# qhasm: int64 subt1

# qhasm: int64 t

# qhasm: stack64 tp_stack

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

# qhasm: enter crypto_sign_ed25519_amd64_64_choose_t
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_choose_t
.globl crypto_sign_ed25519_amd64_64_choose_t
_crypto_sign_ed25519_amd64_64_choose_t:
crypto_sign_ed25519_amd64_64_choose_t:
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

# qhasm: tp_stack = tp
# asm 1: movq <tp=int64#1,>tp_stack=stack64#8
# asm 2: movq <tp=%rdi,>tp_stack=56(%rsp)
movq %rdi,56(%rsp)

# qhasm: pos *= 768
# asm 1: imulq  $768,<pos=int64#2,>pos=int64#1
# asm 2: imulq  $768,<pos=%rsi,>pos=%rdi
imulq  $768,%rsi,%rdi

# qhasm: mask = b
# asm 1: mov  <b=int64#3,>mask=int64#2
# asm 2: mov  <b=%rdx,>mask=%rsi
mov  %rdx,%rsi

# qhasm: (int64) mask >>= 7
# asm 1: sar  $7,<mask=int64#2
# asm 2: sar  $7,<mask=%rsi
sar  $7,%rsi

# qhasm: u = b
# asm 1: mov  <b=int64#3,>u=int64#5
# asm 2: mov  <b=%rdx,>u=%r8
mov  %rdx,%r8

# qhasm: u += mask
# asm 1: add  <mask=int64#2,<u=int64#5
# asm 2: add  <mask=%rsi,<u=%r8
add  %rsi,%r8

# qhasm: u ^= mask
# asm 1: xor  <mask=int64#2,<u=int64#5
# asm 2: xor  <mask=%rsi,<u=%r8
xor  %rsi,%r8

# qhasm: tysubx0 = 1
# asm 1: mov  $1,>tysubx0=int64#2
# asm 2: mov  $1,>tysubx0=%rsi
mov  $1,%rsi

# qhasm: tysubx1 = 0
# asm 1: mov  $0,>tysubx1=int64#6
# asm 2: mov  $0,>tysubx1=%r9
mov  $0,%r9

# qhasm: tysubx2 = 0
# asm 1: mov  $0,>tysubx2=int64#7
# asm 2: mov  $0,>tysubx2=%rax
mov  $0,%rax

# qhasm: tysubx3 = 0
# asm 1: mov  $0,>tysubx3=int64#8
# asm 2: mov  $0,>tysubx3=%r10
mov  $0,%r10

# qhasm: txaddy0 = 1
# asm 1: mov  $1,>txaddy0=int64#9
# asm 2: mov  $1,>txaddy0=%r11
mov  $1,%r11

# qhasm: txaddy1 = 0
# asm 1: mov  $0,>txaddy1=int64#10
# asm 2: mov  $0,>txaddy1=%r12
mov  $0,%r12

# qhasm: txaddy2 = 0
# asm 1: mov  $0,>txaddy2=int64#11
# asm 2: mov  $0,>txaddy2=%r13
mov  $0,%r13

# qhasm: txaddy3 = 0
# asm 1: mov  $0,>txaddy3=int64#12
# asm 2: mov  $0,>txaddy3=%r14
mov  $0,%r14

# qhasm: =? u - 1
# asm 1: cmp  $1,<u=int64#5
# asm 2: cmp  $1,<u=%r8
cmp  $1,%r8

# qhasm: t = *(uint64 *)(basep + 0 + pos)
# asm 1: movq   0(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   0(<basep=%rcx,<pos=%rdi),>t=%r15
movq   0(%rcx,%rdi),%r15

# qhasm: tysubx0 = t if =
# asm 1: cmove <t=int64#13,<tysubx0=int64#2
# asm 2: cmove <t=%r15,<tysubx0=%rsi
cmove %r15,%rsi

# qhasm: t = *(uint64 *)(basep + 8 + pos)
# asm 1: movq   8(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   8(<basep=%rcx,<pos=%rdi),>t=%r15
movq   8(%rcx,%rdi),%r15

# qhasm: tysubx1 = t if =
# asm 1: cmove <t=int64#13,<tysubx1=int64#6
# asm 2: cmove <t=%r15,<tysubx1=%r9
cmove %r15,%r9

# qhasm: t = *(uint64 *)(basep + 16 + pos)
# asm 1: movq   16(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   16(<basep=%rcx,<pos=%rdi),>t=%r15
movq   16(%rcx,%rdi),%r15

# qhasm: tysubx2 = t if =
# asm 1: cmove <t=int64#13,<tysubx2=int64#7
# asm 2: cmove <t=%r15,<tysubx2=%rax
cmove %r15,%rax

# qhasm: t = *(uint64 *)(basep + 24 + pos)
# asm 1: movq   24(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   24(<basep=%rcx,<pos=%rdi),>t=%r15
movq   24(%rcx,%rdi),%r15

# qhasm: tysubx3 = t if =
# asm 1: cmove <t=int64#13,<tysubx3=int64#8
# asm 2: cmove <t=%r15,<tysubx3=%r10
cmove %r15,%r10

# qhasm: t = *(uint64 *)(basep + 32 + pos)
# asm 1: movq   32(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   32(<basep=%rcx,<pos=%rdi),>t=%r15
movq   32(%rcx,%rdi),%r15

# qhasm: txaddy0 = t if =
# asm 1: cmove <t=int64#13,<txaddy0=int64#9
# asm 2: cmove <t=%r15,<txaddy0=%r11
cmove %r15,%r11

# qhasm: t = *(uint64 *)(basep + 40 + pos)
# asm 1: movq   40(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   40(<basep=%rcx,<pos=%rdi),>t=%r15
movq   40(%rcx,%rdi),%r15

# qhasm: txaddy1 = t if =
# asm 1: cmove <t=int64#13,<txaddy1=int64#10
# asm 2: cmove <t=%r15,<txaddy1=%r12
cmove %r15,%r12

# qhasm: t = *(uint64 *)(basep + 48 + pos)
# asm 1: movq   48(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   48(<basep=%rcx,<pos=%rdi),>t=%r15
movq   48(%rcx,%rdi),%r15

# qhasm: txaddy2 = t if =
# asm 1: cmove <t=int64#13,<txaddy2=int64#11
# asm 2: cmove <t=%r15,<txaddy2=%r13
cmove %r15,%r13

# qhasm: t = *(uint64 *)(basep + 56 + pos)
# asm 1: movq   56(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   56(<basep=%rcx,<pos=%rdi),>t=%r15
movq   56(%rcx,%rdi),%r15

# qhasm: txaddy3 = t if =
# asm 1: cmove <t=int64#13,<txaddy3=int64#12
# asm 2: cmove <t=%r15,<txaddy3=%r14
cmove %r15,%r14

# qhasm: =? u - 2
# asm 1: cmp  $2,<u=int64#5
# asm 2: cmp  $2,<u=%r8
cmp  $2,%r8

# qhasm: t = *(uint64 *)(basep + 96 + pos)
# asm 1: movq   96(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   96(<basep=%rcx,<pos=%rdi),>t=%r15
movq   96(%rcx,%rdi),%r15

# qhasm: tysubx0 = t if =
# asm 1: cmove <t=int64#13,<tysubx0=int64#2
# asm 2: cmove <t=%r15,<tysubx0=%rsi
cmove %r15,%rsi

# qhasm: t = *(uint64 *)(basep + 104 + pos)
# asm 1: movq   104(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   104(<basep=%rcx,<pos=%rdi),>t=%r15
movq   104(%rcx,%rdi),%r15

# qhasm: tysubx1 = t if =
# asm 1: cmove <t=int64#13,<tysubx1=int64#6
# asm 2: cmove <t=%r15,<tysubx1=%r9
cmove %r15,%r9

# qhasm: t = *(uint64 *)(basep + 112 + pos)
# asm 1: movq   112(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   112(<basep=%rcx,<pos=%rdi),>t=%r15
movq   112(%rcx,%rdi),%r15

# qhasm: tysubx2 = t if =
# asm 1: cmove <t=int64#13,<tysubx2=int64#7
# asm 2: cmove <t=%r15,<tysubx2=%rax
cmove %r15,%rax

# qhasm: t = *(uint64 *)(basep + 120 + pos)
# asm 1: movq   120(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   120(<basep=%rcx,<pos=%rdi),>t=%r15
movq   120(%rcx,%rdi),%r15

# qhasm: tysubx3 = t if =
# asm 1: cmove <t=int64#13,<tysubx3=int64#8
# asm 2: cmove <t=%r15,<tysubx3=%r10
cmove %r15,%r10

# qhasm: t = *(uint64 *)(basep + 128 + pos)
# asm 1: movq   128(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   128(<basep=%rcx,<pos=%rdi),>t=%r15
movq   128(%rcx,%rdi),%r15

# qhasm: txaddy0 = t if =
# asm 1: cmove <t=int64#13,<txaddy0=int64#9
# asm 2: cmove <t=%r15,<txaddy0=%r11
cmove %r15,%r11

# qhasm: t = *(uint64 *)(basep + 136 + pos)
# asm 1: movq   136(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   136(<basep=%rcx,<pos=%rdi),>t=%r15
movq   136(%rcx,%rdi),%r15

# qhasm: txaddy1 = t if =
# asm 1: cmove <t=int64#13,<txaddy1=int64#10
# asm 2: cmove <t=%r15,<txaddy1=%r12
cmove %r15,%r12

# qhasm: t = *(uint64 *)(basep + 144 + pos)
# asm 1: movq   144(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   144(<basep=%rcx,<pos=%rdi),>t=%r15
movq   144(%rcx,%rdi),%r15

# qhasm: txaddy2 = t if =
# asm 1: cmove <t=int64#13,<txaddy2=int64#11
# asm 2: cmove <t=%r15,<txaddy2=%r13
cmove %r15,%r13

# qhasm: t = *(uint64 *)(basep + 152 + pos)
# asm 1: movq   152(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   152(<basep=%rcx,<pos=%rdi),>t=%r15
movq   152(%rcx,%rdi),%r15

# qhasm: txaddy3 = t if =
# asm 1: cmove <t=int64#13,<txaddy3=int64#12
# asm 2: cmove <t=%r15,<txaddy3=%r14
cmove %r15,%r14

# qhasm: =? u - 3
# asm 1: cmp  $3,<u=int64#5
# asm 2: cmp  $3,<u=%r8
cmp  $3,%r8

# qhasm: t = *(uint64 *)(basep + 192 + pos)
# asm 1: movq   192(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   192(<basep=%rcx,<pos=%rdi),>t=%r15
movq   192(%rcx,%rdi),%r15

# qhasm: tysubx0 = t if =
# asm 1: cmove <t=int64#13,<tysubx0=int64#2
# asm 2: cmove <t=%r15,<tysubx0=%rsi
cmove %r15,%rsi

# qhasm: t = *(uint64 *)(basep + 200 + pos)
# asm 1: movq   200(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   200(<basep=%rcx,<pos=%rdi),>t=%r15
movq   200(%rcx,%rdi),%r15

# qhasm: tysubx1 = t if =
# asm 1: cmove <t=int64#13,<tysubx1=int64#6
# asm 2: cmove <t=%r15,<tysubx1=%r9
cmove %r15,%r9

# qhasm: t = *(uint64 *)(basep + 208 + pos)
# asm 1: movq   208(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   208(<basep=%rcx,<pos=%rdi),>t=%r15
movq   208(%rcx,%rdi),%r15

# qhasm: tysubx2 = t if =
# asm 1: cmove <t=int64#13,<tysubx2=int64#7
# asm 2: cmove <t=%r15,<tysubx2=%rax
cmove %r15,%rax

# qhasm: t = *(uint64 *)(basep + 216 + pos)
# asm 1: movq   216(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   216(<basep=%rcx,<pos=%rdi),>t=%r15
movq   216(%rcx,%rdi),%r15

# qhasm: tysubx3 = t if =
# asm 1: cmove <t=int64#13,<tysubx3=int64#8
# asm 2: cmove <t=%r15,<tysubx3=%r10
cmove %r15,%r10

# qhasm: t = *(uint64 *)(basep + 224 + pos)
# asm 1: movq   224(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   224(<basep=%rcx,<pos=%rdi),>t=%r15
movq   224(%rcx,%rdi),%r15

# qhasm: txaddy0 = t if =
# asm 1: cmove <t=int64#13,<txaddy0=int64#9
# asm 2: cmove <t=%r15,<txaddy0=%r11
cmove %r15,%r11

# qhasm: t = *(uint64 *)(basep + 232 + pos)
# asm 1: movq   232(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   232(<basep=%rcx,<pos=%rdi),>t=%r15
movq   232(%rcx,%rdi),%r15

# qhasm: txaddy1 = t if =
# asm 1: cmove <t=int64#13,<txaddy1=int64#10
# asm 2: cmove <t=%r15,<txaddy1=%r12
cmove %r15,%r12

# qhasm: t = *(uint64 *)(basep + 240 + pos)
# asm 1: movq   240(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   240(<basep=%rcx,<pos=%rdi),>t=%r15
movq   240(%rcx,%rdi),%r15

# qhasm: txaddy2 = t if =
# asm 1: cmove <t=int64#13,<txaddy2=int64#11
# asm 2: cmove <t=%r15,<txaddy2=%r13
cmove %r15,%r13

# qhasm: t = *(uint64 *)(basep + 248 + pos)
# asm 1: movq   248(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   248(<basep=%rcx,<pos=%rdi),>t=%r15
movq   248(%rcx,%rdi),%r15

# qhasm: txaddy3 = t if =
# asm 1: cmove <t=int64#13,<txaddy3=int64#12
# asm 2: cmove <t=%r15,<txaddy3=%r14
cmove %r15,%r14

# qhasm: =? u - 4
# asm 1: cmp  $4,<u=int64#5
# asm 2: cmp  $4,<u=%r8
cmp  $4,%r8

# qhasm: t = *(uint64 *)(basep + 288 + pos)
# asm 1: movq   288(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   288(<basep=%rcx,<pos=%rdi),>t=%r15
movq   288(%rcx,%rdi),%r15

# qhasm: tysubx0 = t if =
# asm 1: cmove <t=int64#13,<tysubx0=int64#2
# asm 2: cmove <t=%r15,<tysubx0=%rsi
cmove %r15,%rsi

# qhasm: t = *(uint64 *)(basep + 296 + pos)
# asm 1: movq   296(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   296(<basep=%rcx,<pos=%rdi),>t=%r15
movq   296(%rcx,%rdi),%r15

# qhasm: tysubx1 = t if =
# asm 1: cmove <t=int64#13,<tysubx1=int64#6
# asm 2: cmove <t=%r15,<tysubx1=%r9
cmove %r15,%r9

# qhasm: t = *(uint64 *)(basep + 304 + pos)
# asm 1: movq   304(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   304(<basep=%rcx,<pos=%rdi),>t=%r15
movq   304(%rcx,%rdi),%r15

# qhasm: tysubx2 = t if =
# asm 1: cmove <t=int64#13,<tysubx2=int64#7
# asm 2: cmove <t=%r15,<tysubx2=%rax
cmove %r15,%rax

# qhasm: t = *(uint64 *)(basep + 312 + pos)
# asm 1: movq   312(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   312(<basep=%rcx,<pos=%rdi),>t=%r15
movq   312(%rcx,%rdi),%r15

# qhasm: tysubx3 = t if =
# asm 1: cmove <t=int64#13,<tysubx3=int64#8
# asm 2: cmove <t=%r15,<tysubx3=%r10
cmove %r15,%r10

# qhasm: t = *(uint64 *)(basep + 320 + pos)
# asm 1: movq   320(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   320(<basep=%rcx,<pos=%rdi),>t=%r15
movq   320(%rcx,%rdi),%r15

# qhasm: txaddy0 = t if =
# asm 1: cmove <t=int64#13,<txaddy0=int64#9
# asm 2: cmove <t=%r15,<txaddy0=%r11
cmove %r15,%r11

# qhasm: t = *(uint64 *)(basep + 328 + pos)
# asm 1: movq   328(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   328(<basep=%rcx,<pos=%rdi),>t=%r15
movq   328(%rcx,%rdi),%r15

# qhasm: txaddy1 = t if =
# asm 1: cmove <t=int64#13,<txaddy1=int64#10
# asm 2: cmove <t=%r15,<txaddy1=%r12
cmove %r15,%r12

# qhasm: t = *(uint64 *)(basep + 336 + pos)
# asm 1: movq   336(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   336(<basep=%rcx,<pos=%rdi),>t=%r15
movq   336(%rcx,%rdi),%r15

# qhasm: txaddy2 = t if =
# asm 1: cmove <t=int64#13,<txaddy2=int64#11
# asm 2: cmove <t=%r15,<txaddy2=%r13
cmove %r15,%r13

# qhasm: t = *(uint64 *)(basep + 344 + pos)
# asm 1: movq   344(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   344(<basep=%rcx,<pos=%rdi),>t=%r15
movq   344(%rcx,%rdi),%r15

# qhasm: txaddy3 = t if =
# asm 1: cmove <t=int64#13,<txaddy3=int64#12
# asm 2: cmove <t=%r15,<txaddy3=%r14
cmove %r15,%r14

# qhasm: =? u - 5
# asm 1: cmp  $5,<u=int64#5
# asm 2: cmp  $5,<u=%r8
cmp  $5,%r8

# qhasm: t = *(uint64 *)(basep + 384 + pos)
# asm 1: movq   384(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   384(<basep=%rcx,<pos=%rdi),>t=%r15
movq   384(%rcx,%rdi),%r15

# qhasm: tysubx0 = t if =
# asm 1: cmove <t=int64#13,<tysubx0=int64#2
# asm 2: cmove <t=%r15,<tysubx0=%rsi
cmove %r15,%rsi

# qhasm: t = *(uint64 *)(basep + 392 + pos)
# asm 1: movq   392(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   392(<basep=%rcx,<pos=%rdi),>t=%r15
movq   392(%rcx,%rdi),%r15

# qhasm: tysubx1 = t if =
# asm 1: cmove <t=int64#13,<tysubx1=int64#6
# asm 2: cmove <t=%r15,<tysubx1=%r9
cmove %r15,%r9

# qhasm: t = *(uint64 *)(basep + 400 + pos)
# asm 1: movq   400(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   400(<basep=%rcx,<pos=%rdi),>t=%r15
movq   400(%rcx,%rdi),%r15

# qhasm: tysubx2 = t if =
# asm 1: cmove <t=int64#13,<tysubx2=int64#7
# asm 2: cmove <t=%r15,<tysubx2=%rax
cmove %r15,%rax

# qhasm: t = *(uint64 *)(basep + 408 + pos)
# asm 1: movq   408(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   408(<basep=%rcx,<pos=%rdi),>t=%r15
movq   408(%rcx,%rdi),%r15

# qhasm: tysubx3 = t if =
# asm 1: cmove <t=int64#13,<tysubx3=int64#8
# asm 2: cmove <t=%r15,<tysubx3=%r10
cmove %r15,%r10

# qhasm: t = *(uint64 *)(basep + 416 + pos)
# asm 1: movq   416(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   416(<basep=%rcx,<pos=%rdi),>t=%r15
movq   416(%rcx,%rdi),%r15

# qhasm: txaddy0 = t if =
# asm 1: cmove <t=int64#13,<txaddy0=int64#9
# asm 2: cmove <t=%r15,<txaddy0=%r11
cmove %r15,%r11

# qhasm: t = *(uint64 *)(basep + 424 + pos)
# asm 1: movq   424(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   424(<basep=%rcx,<pos=%rdi),>t=%r15
movq   424(%rcx,%rdi),%r15

# qhasm: txaddy1 = t if =
# asm 1: cmove <t=int64#13,<txaddy1=int64#10
# asm 2: cmove <t=%r15,<txaddy1=%r12
cmove %r15,%r12

# qhasm: t = *(uint64 *)(basep + 432 + pos)
# asm 1: movq   432(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   432(<basep=%rcx,<pos=%rdi),>t=%r15
movq   432(%rcx,%rdi),%r15

# qhasm: txaddy2 = t if =
# asm 1: cmove <t=int64#13,<txaddy2=int64#11
# asm 2: cmove <t=%r15,<txaddy2=%r13
cmove %r15,%r13

# qhasm: t = *(uint64 *)(basep + 440 + pos)
# asm 1: movq   440(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   440(<basep=%rcx,<pos=%rdi),>t=%r15
movq   440(%rcx,%rdi),%r15

# qhasm: txaddy3 = t if =
# asm 1: cmove <t=int64#13,<txaddy3=int64#12
# asm 2: cmove <t=%r15,<txaddy3=%r14
cmove %r15,%r14

# qhasm: =? u - 6
# asm 1: cmp  $6,<u=int64#5
# asm 2: cmp  $6,<u=%r8
cmp  $6,%r8

# qhasm: t = *(uint64 *)(basep + 480 + pos)
# asm 1: movq   480(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   480(<basep=%rcx,<pos=%rdi),>t=%r15
movq   480(%rcx,%rdi),%r15

# qhasm: tysubx0 = t if =
# asm 1: cmove <t=int64#13,<tysubx0=int64#2
# asm 2: cmove <t=%r15,<tysubx0=%rsi
cmove %r15,%rsi

# qhasm: t = *(uint64 *)(basep + 488 + pos)
# asm 1: movq   488(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   488(<basep=%rcx,<pos=%rdi),>t=%r15
movq   488(%rcx,%rdi),%r15

# qhasm: tysubx1 = t if =
# asm 1: cmove <t=int64#13,<tysubx1=int64#6
# asm 2: cmove <t=%r15,<tysubx1=%r9
cmove %r15,%r9

# qhasm: t = *(uint64 *)(basep + 496 + pos)
# asm 1: movq   496(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   496(<basep=%rcx,<pos=%rdi),>t=%r15
movq   496(%rcx,%rdi),%r15

# qhasm: tysubx2 = t if =
# asm 1: cmove <t=int64#13,<tysubx2=int64#7
# asm 2: cmove <t=%r15,<tysubx2=%rax
cmove %r15,%rax

# qhasm: t = *(uint64 *)(basep + 504 + pos)
# asm 1: movq   504(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   504(<basep=%rcx,<pos=%rdi),>t=%r15
movq   504(%rcx,%rdi),%r15

# qhasm: tysubx3 = t if =
# asm 1: cmove <t=int64#13,<tysubx3=int64#8
# asm 2: cmove <t=%r15,<tysubx3=%r10
cmove %r15,%r10

# qhasm: t = *(uint64 *)(basep + 512 + pos)
# asm 1: movq   512(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   512(<basep=%rcx,<pos=%rdi),>t=%r15
movq   512(%rcx,%rdi),%r15

# qhasm: txaddy0 = t if =
# asm 1: cmove <t=int64#13,<txaddy0=int64#9
# asm 2: cmove <t=%r15,<txaddy0=%r11
cmove %r15,%r11

# qhasm: t = *(uint64 *)(basep + 520 + pos)
# asm 1: movq   520(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   520(<basep=%rcx,<pos=%rdi),>t=%r15
movq   520(%rcx,%rdi),%r15

# qhasm: txaddy1 = t if =
# asm 1: cmove <t=int64#13,<txaddy1=int64#10
# asm 2: cmove <t=%r15,<txaddy1=%r12
cmove %r15,%r12

# qhasm: t = *(uint64 *)(basep + 528 + pos)
# asm 1: movq   528(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   528(<basep=%rcx,<pos=%rdi),>t=%r15
movq   528(%rcx,%rdi),%r15

# qhasm: txaddy2 = t if =
# asm 1: cmove <t=int64#13,<txaddy2=int64#11
# asm 2: cmove <t=%r15,<txaddy2=%r13
cmove %r15,%r13

# qhasm: t = *(uint64 *)(basep + 536 + pos)
# asm 1: movq   536(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   536(<basep=%rcx,<pos=%rdi),>t=%r15
movq   536(%rcx,%rdi),%r15

# qhasm: txaddy3 = t if =
# asm 1: cmove <t=int64#13,<txaddy3=int64#12
# asm 2: cmove <t=%r15,<txaddy3=%r14
cmove %r15,%r14

# qhasm: =? u - 7
# asm 1: cmp  $7,<u=int64#5
# asm 2: cmp  $7,<u=%r8
cmp  $7,%r8

# qhasm: t = *(uint64 *)(basep + 576 + pos)
# asm 1: movq   576(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   576(<basep=%rcx,<pos=%rdi),>t=%r15
movq   576(%rcx,%rdi),%r15

# qhasm: tysubx0 = t if =
# asm 1: cmove <t=int64#13,<tysubx0=int64#2
# asm 2: cmove <t=%r15,<tysubx0=%rsi
cmove %r15,%rsi

# qhasm: t = *(uint64 *)(basep + 584 + pos)
# asm 1: movq   584(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   584(<basep=%rcx,<pos=%rdi),>t=%r15
movq   584(%rcx,%rdi),%r15

# qhasm: tysubx1 = t if =
# asm 1: cmove <t=int64#13,<tysubx1=int64#6
# asm 2: cmove <t=%r15,<tysubx1=%r9
cmove %r15,%r9

# qhasm: t = *(uint64 *)(basep + 592 + pos)
# asm 1: movq   592(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   592(<basep=%rcx,<pos=%rdi),>t=%r15
movq   592(%rcx,%rdi),%r15

# qhasm: tysubx2 = t if =
# asm 1: cmove <t=int64#13,<tysubx2=int64#7
# asm 2: cmove <t=%r15,<tysubx2=%rax
cmove %r15,%rax

# qhasm: t = *(uint64 *)(basep + 600 + pos)
# asm 1: movq   600(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   600(<basep=%rcx,<pos=%rdi),>t=%r15
movq   600(%rcx,%rdi),%r15

# qhasm: tysubx3 = t if =
# asm 1: cmove <t=int64#13,<tysubx3=int64#8
# asm 2: cmove <t=%r15,<tysubx3=%r10
cmove %r15,%r10

# qhasm: t = *(uint64 *)(basep + 608 + pos)
# asm 1: movq   608(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   608(<basep=%rcx,<pos=%rdi),>t=%r15
movq   608(%rcx,%rdi),%r15

# qhasm: txaddy0 = t if =
# asm 1: cmove <t=int64#13,<txaddy0=int64#9
# asm 2: cmove <t=%r15,<txaddy0=%r11
cmove %r15,%r11

# qhasm: t = *(uint64 *)(basep + 616 + pos)
# asm 1: movq   616(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   616(<basep=%rcx,<pos=%rdi),>t=%r15
movq   616(%rcx,%rdi),%r15

# qhasm: txaddy1 = t if =
# asm 1: cmove <t=int64#13,<txaddy1=int64#10
# asm 2: cmove <t=%r15,<txaddy1=%r12
cmove %r15,%r12

# qhasm: t = *(uint64 *)(basep + 624 + pos)
# asm 1: movq   624(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   624(<basep=%rcx,<pos=%rdi),>t=%r15
movq   624(%rcx,%rdi),%r15

# qhasm: txaddy2 = t if =
# asm 1: cmove <t=int64#13,<txaddy2=int64#11
# asm 2: cmove <t=%r15,<txaddy2=%r13
cmove %r15,%r13

# qhasm: t = *(uint64 *)(basep + 632 + pos)
# asm 1: movq   632(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   632(<basep=%rcx,<pos=%rdi),>t=%r15
movq   632(%rcx,%rdi),%r15

# qhasm: txaddy3 = t if =
# asm 1: cmove <t=int64#13,<txaddy3=int64#12
# asm 2: cmove <t=%r15,<txaddy3=%r14
cmove %r15,%r14

# qhasm: =? u - 8
# asm 1: cmp  $8,<u=int64#5
# asm 2: cmp  $8,<u=%r8
cmp  $8,%r8

# qhasm: t = *(uint64 *)(basep + 672 + pos)
# asm 1: movq   672(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   672(<basep=%rcx,<pos=%rdi),>t=%r15
movq   672(%rcx,%rdi),%r15

# qhasm: tysubx0 = t if =
# asm 1: cmove <t=int64#13,<tysubx0=int64#2
# asm 2: cmove <t=%r15,<tysubx0=%rsi
cmove %r15,%rsi

# qhasm: t = *(uint64 *)(basep + 680 + pos)
# asm 1: movq   680(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   680(<basep=%rcx,<pos=%rdi),>t=%r15
movq   680(%rcx,%rdi),%r15

# qhasm: tysubx1 = t if =
# asm 1: cmove <t=int64#13,<tysubx1=int64#6
# asm 2: cmove <t=%r15,<tysubx1=%r9
cmove %r15,%r9

# qhasm: t = *(uint64 *)(basep + 688 + pos)
# asm 1: movq   688(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   688(<basep=%rcx,<pos=%rdi),>t=%r15
movq   688(%rcx,%rdi),%r15

# qhasm: tysubx2 = t if =
# asm 1: cmove <t=int64#13,<tysubx2=int64#7
# asm 2: cmove <t=%r15,<tysubx2=%rax
cmove %r15,%rax

# qhasm: t = *(uint64 *)(basep + 696 + pos)
# asm 1: movq   696(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   696(<basep=%rcx,<pos=%rdi),>t=%r15
movq   696(%rcx,%rdi),%r15

# qhasm: tysubx3 = t if =
# asm 1: cmove <t=int64#13,<tysubx3=int64#8
# asm 2: cmove <t=%r15,<tysubx3=%r10
cmove %r15,%r10

# qhasm: t = *(uint64 *)(basep + 704 + pos)
# asm 1: movq   704(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   704(<basep=%rcx,<pos=%rdi),>t=%r15
movq   704(%rcx,%rdi),%r15

# qhasm: txaddy0 = t if =
# asm 1: cmove <t=int64#13,<txaddy0=int64#9
# asm 2: cmove <t=%r15,<txaddy0=%r11
cmove %r15,%r11

# qhasm: t = *(uint64 *)(basep + 712 + pos)
# asm 1: movq   712(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   712(<basep=%rcx,<pos=%rdi),>t=%r15
movq   712(%rcx,%rdi),%r15

# qhasm: txaddy1 = t if =
# asm 1: cmove <t=int64#13,<txaddy1=int64#10
# asm 2: cmove <t=%r15,<txaddy1=%r12
cmove %r15,%r12

# qhasm: t = *(uint64 *)(basep + 720 + pos)
# asm 1: movq   720(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   720(<basep=%rcx,<pos=%rdi),>t=%r15
movq   720(%rcx,%rdi),%r15

# qhasm: txaddy2 = t if =
# asm 1: cmove <t=int64#13,<txaddy2=int64#11
# asm 2: cmove <t=%r15,<txaddy2=%r13
cmove %r15,%r13

# qhasm: t = *(uint64 *)(basep + 728 + pos)
# asm 1: movq   728(<basep=int64#4,<pos=int64#1),>t=int64#13
# asm 2: movq   728(<basep=%rcx,<pos=%rdi),>t=%r15
movq   728(%rcx,%rdi),%r15

# qhasm: txaddy3 = t if =
# asm 1: cmove <t=int64#13,<txaddy3=int64#12
# asm 2: cmove <t=%r15,<txaddy3=%r14
cmove %r15,%r14

# qhasm: signed<? b - 0
# asm 1: cmp  $0,<b=int64#3
# asm 2: cmp  $0,<b=%rdx
cmp  $0,%rdx

# qhasm: t = tysubx0
# asm 1: mov  <tysubx0=int64#2,>t=int64#13
# asm 2: mov  <tysubx0=%rsi,>t=%r15
mov  %rsi,%r15

# qhasm: tysubx0 = txaddy0 if signed<
# asm 1: cmovl <txaddy0=int64#9,<tysubx0=int64#2
# asm 2: cmovl <txaddy0=%r11,<tysubx0=%rsi
cmovl %r11,%rsi

# qhasm: txaddy0 = t if signed<
# asm 1: cmovl <t=int64#13,<txaddy0=int64#9
# asm 2: cmovl <t=%r15,<txaddy0=%r11
cmovl %r15,%r11

# qhasm: t = tysubx1
# asm 1: mov  <tysubx1=int64#6,>t=int64#13
# asm 2: mov  <tysubx1=%r9,>t=%r15
mov  %r9,%r15

# qhasm: tysubx1 = txaddy1 if signed<
# asm 1: cmovl <txaddy1=int64#10,<tysubx1=int64#6
# asm 2: cmovl <txaddy1=%r12,<tysubx1=%r9
cmovl %r12,%r9

# qhasm: txaddy1 = t if signed<
# asm 1: cmovl <t=int64#13,<txaddy1=int64#10
# asm 2: cmovl <t=%r15,<txaddy1=%r12
cmovl %r15,%r12

# qhasm: t = tysubx2
# asm 1: mov  <tysubx2=int64#7,>t=int64#13
# asm 2: mov  <tysubx2=%rax,>t=%r15
mov  %rax,%r15

# qhasm: tysubx2 = txaddy2 if signed<
# asm 1: cmovl <txaddy2=int64#11,<tysubx2=int64#7
# asm 2: cmovl <txaddy2=%r13,<tysubx2=%rax
cmovl %r13,%rax

# qhasm: txaddy2 = t if signed<
# asm 1: cmovl <t=int64#13,<txaddy2=int64#11
# asm 2: cmovl <t=%r15,<txaddy2=%r13
cmovl %r15,%r13

# qhasm: t = tysubx3
# asm 1: mov  <tysubx3=int64#8,>t=int64#13
# asm 2: mov  <tysubx3=%r10,>t=%r15
mov  %r10,%r15

# qhasm: tysubx3 = txaddy3 if signed<
# asm 1: cmovl <txaddy3=int64#12,<tysubx3=int64#8
# asm 2: cmovl <txaddy3=%r14,<tysubx3=%r10
cmovl %r14,%r10

# qhasm: txaddy3 = t if signed<
# asm 1: cmovl <t=int64#13,<txaddy3=int64#12
# asm 2: cmovl <t=%r15,<txaddy3=%r14
cmovl %r15,%r14

# qhasm: tp = tp_stack
# asm 1: movq <tp_stack=stack64#8,>tp=int64#13
# asm 2: movq <tp_stack=56(%rsp),>tp=%r15
movq 56(%rsp),%r15

# qhasm: *(uint64 *)(tp + 0) = tysubx0
# asm 1: movq   <tysubx0=int64#2,0(<tp=int64#13)
# asm 2: movq   <tysubx0=%rsi,0(<tp=%r15)
movq   %rsi,0(%r15)

# qhasm: *(uint64 *)(tp + 8) = tysubx1
# asm 1: movq   <tysubx1=int64#6,8(<tp=int64#13)
# asm 2: movq   <tysubx1=%r9,8(<tp=%r15)
movq   %r9,8(%r15)

# qhasm: *(uint64 *)(tp + 16) = tysubx2
# asm 1: movq   <tysubx2=int64#7,16(<tp=int64#13)
# asm 2: movq   <tysubx2=%rax,16(<tp=%r15)
movq   %rax,16(%r15)

# qhasm: *(uint64 *)(tp + 24) = tysubx3
# asm 1: movq   <tysubx3=int64#8,24(<tp=int64#13)
# asm 2: movq   <tysubx3=%r10,24(<tp=%r15)
movq   %r10,24(%r15)

# qhasm: *(uint64 *)(tp + 32) = txaddy0
# asm 1: movq   <txaddy0=int64#9,32(<tp=int64#13)
# asm 2: movq   <txaddy0=%r11,32(<tp=%r15)
movq   %r11,32(%r15)

# qhasm: *(uint64 *)(tp + 40) = txaddy1
# asm 1: movq   <txaddy1=int64#10,40(<tp=int64#13)
# asm 2: movq   <txaddy1=%r12,40(<tp=%r15)
movq   %r12,40(%r15)

# qhasm: *(uint64 *)(tp + 48) = txaddy2
# asm 1: movq   <txaddy2=int64#11,48(<tp=int64#13)
# asm 2: movq   <txaddy2=%r13,48(<tp=%r15)
movq   %r13,48(%r15)

# qhasm: *(uint64 *)(tp + 56) = txaddy3
# asm 1: movq   <txaddy3=int64#12,56(<tp=int64#13)
# asm 2: movq   <txaddy3=%r14,56(<tp=%r15)
movq   %r14,56(%r15)

# qhasm: tt2d0 = 0
# asm 1: mov  $0,>tt2d0=int64#2
# asm 2: mov  $0,>tt2d0=%rsi
mov  $0,%rsi

# qhasm: tt2d1 = 0
# asm 1: mov  $0,>tt2d1=int64#6
# asm 2: mov  $0,>tt2d1=%r9
mov  $0,%r9

# qhasm: tt2d2 = 0
# asm 1: mov  $0,>tt2d2=int64#7
# asm 2: mov  $0,>tt2d2=%rax
mov  $0,%rax

# qhasm: tt2d3 = 0
# asm 1: mov  $0,>tt2d3=int64#8
# asm 2: mov  $0,>tt2d3=%r10
mov  $0,%r10

# qhasm: =? u - 1
# asm 1: cmp  $1,<u=int64#5
# asm 2: cmp  $1,<u=%r8
cmp  $1,%r8

# qhasm: t = *(uint64 *)(basep + 64 + pos)
# asm 1: movq   64(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   64(<basep=%rcx,<pos=%rdi),>t=%r11
movq   64(%rcx,%rdi),%r11

# qhasm: tt2d0 = t if =
# asm 1: cmove <t=int64#9,<tt2d0=int64#2
# asm 2: cmove <t=%r11,<tt2d0=%rsi
cmove %r11,%rsi

# qhasm: t = *(uint64 *)(basep + 72 + pos)
# asm 1: movq   72(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   72(<basep=%rcx,<pos=%rdi),>t=%r11
movq   72(%rcx,%rdi),%r11

# qhasm: tt2d1 = t if =
# asm 1: cmove <t=int64#9,<tt2d1=int64#6
# asm 2: cmove <t=%r11,<tt2d1=%r9
cmove %r11,%r9

# qhasm: t = *(uint64 *)(basep + 80 + pos)
# asm 1: movq   80(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   80(<basep=%rcx,<pos=%rdi),>t=%r11
movq   80(%rcx,%rdi),%r11

# qhasm: tt2d2 = t if =
# asm 1: cmove <t=int64#9,<tt2d2=int64#7
# asm 2: cmove <t=%r11,<tt2d2=%rax
cmove %r11,%rax

# qhasm: t = *(uint64 *)(basep + 88 + pos)
# asm 1: movq   88(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   88(<basep=%rcx,<pos=%rdi),>t=%r11
movq   88(%rcx,%rdi),%r11

# qhasm: tt2d3 = t if =
# asm 1: cmove <t=int64#9,<tt2d3=int64#8
# asm 2: cmove <t=%r11,<tt2d3=%r10
cmove %r11,%r10

# qhasm: =? u - 2
# asm 1: cmp  $2,<u=int64#5
# asm 2: cmp  $2,<u=%r8
cmp  $2,%r8

# qhasm: t = *(uint64 *)(basep + 160 + pos)
# asm 1: movq   160(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   160(<basep=%rcx,<pos=%rdi),>t=%r11
movq   160(%rcx,%rdi),%r11

# qhasm: tt2d0 = t if =
# asm 1: cmove <t=int64#9,<tt2d0=int64#2
# asm 2: cmove <t=%r11,<tt2d0=%rsi
cmove %r11,%rsi

# qhasm: t = *(uint64 *)(basep + 168 + pos)
# asm 1: movq   168(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   168(<basep=%rcx,<pos=%rdi),>t=%r11
movq   168(%rcx,%rdi),%r11

# qhasm: tt2d1 = t if =
# asm 1: cmove <t=int64#9,<tt2d1=int64#6
# asm 2: cmove <t=%r11,<tt2d1=%r9
cmove %r11,%r9

# qhasm: t = *(uint64 *)(basep + 176 + pos)
# asm 1: movq   176(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   176(<basep=%rcx,<pos=%rdi),>t=%r11
movq   176(%rcx,%rdi),%r11

# qhasm: tt2d2 = t if =
# asm 1: cmove <t=int64#9,<tt2d2=int64#7
# asm 2: cmove <t=%r11,<tt2d2=%rax
cmove %r11,%rax

# qhasm: t = *(uint64 *)(basep + 184 + pos)
# asm 1: movq   184(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   184(<basep=%rcx,<pos=%rdi),>t=%r11
movq   184(%rcx,%rdi),%r11

# qhasm: tt2d3 = t if =
# asm 1: cmove <t=int64#9,<tt2d3=int64#8
# asm 2: cmove <t=%r11,<tt2d3=%r10
cmove %r11,%r10

# qhasm: =? u - 3
# asm 1: cmp  $3,<u=int64#5
# asm 2: cmp  $3,<u=%r8
cmp  $3,%r8

# qhasm: t = *(uint64 *)(basep + 256 + pos)
# asm 1: movq   256(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   256(<basep=%rcx,<pos=%rdi),>t=%r11
movq   256(%rcx,%rdi),%r11

# qhasm: tt2d0 = t if =
# asm 1: cmove <t=int64#9,<tt2d0=int64#2
# asm 2: cmove <t=%r11,<tt2d0=%rsi
cmove %r11,%rsi

# qhasm: t = *(uint64 *)(basep + 264 + pos)
# asm 1: movq   264(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   264(<basep=%rcx,<pos=%rdi),>t=%r11
movq   264(%rcx,%rdi),%r11

# qhasm: tt2d1 = t if =
# asm 1: cmove <t=int64#9,<tt2d1=int64#6
# asm 2: cmove <t=%r11,<tt2d1=%r9
cmove %r11,%r9

# qhasm: t = *(uint64 *)(basep + 272 + pos)
# asm 1: movq   272(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   272(<basep=%rcx,<pos=%rdi),>t=%r11
movq   272(%rcx,%rdi),%r11

# qhasm: tt2d2 = t if =
# asm 1: cmove <t=int64#9,<tt2d2=int64#7
# asm 2: cmove <t=%r11,<tt2d2=%rax
cmove %r11,%rax

# qhasm: t = *(uint64 *)(basep + 280 + pos)
# asm 1: movq   280(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   280(<basep=%rcx,<pos=%rdi),>t=%r11
movq   280(%rcx,%rdi),%r11

# qhasm: tt2d3 = t if =
# asm 1: cmove <t=int64#9,<tt2d3=int64#8
# asm 2: cmove <t=%r11,<tt2d3=%r10
cmove %r11,%r10

# qhasm: =? u - 4
# asm 1: cmp  $4,<u=int64#5
# asm 2: cmp  $4,<u=%r8
cmp  $4,%r8

# qhasm: t = *(uint64 *)(basep + 352 + pos)
# asm 1: movq   352(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   352(<basep=%rcx,<pos=%rdi),>t=%r11
movq   352(%rcx,%rdi),%r11

# qhasm: tt2d0 = t if =
# asm 1: cmove <t=int64#9,<tt2d0=int64#2
# asm 2: cmove <t=%r11,<tt2d0=%rsi
cmove %r11,%rsi

# qhasm: t = *(uint64 *)(basep + 360 + pos)
# asm 1: movq   360(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   360(<basep=%rcx,<pos=%rdi),>t=%r11
movq   360(%rcx,%rdi),%r11

# qhasm: tt2d1 = t if =
# asm 1: cmove <t=int64#9,<tt2d1=int64#6
# asm 2: cmove <t=%r11,<tt2d1=%r9
cmove %r11,%r9

# qhasm: t = *(uint64 *)(basep + 368 + pos)
# asm 1: movq   368(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   368(<basep=%rcx,<pos=%rdi),>t=%r11
movq   368(%rcx,%rdi),%r11

# qhasm: tt2d2 = t if =
# asm 1: cmove <t=int64#9,<tt2d2=int64#7
# asm 2: cmove <t=%r11,<tt2d2=%rax
cmove %r11,%rax

# qhasm: t = *(uint64 *)(basep + 376 + pos)
# asm 1: movq   376(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   376(<basep=%rcx,<pos=%rdi),>t=%r11
movq   376(%rcx,%rdi),%r11

# qhasm: tt2d3 = t if =
# asm 1: cmove <t=int64#9,<tt2d3=int64#8
# asm 2: cmove <t=%r11,<tt2d3=%r10
cmove %r11,%r10

# qhasm: =? u - 5
# asm 1: cmp  $5,<u=int64#5
# asm 2: cmp  $5,<u=%r8
cmp  $5,%r8

# qhasm: t = *(uint64 *)(basep + 448 + pos)
# asm 1: movq   448(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   448(<basep=%rcx,<pos=%rdi),>t=%r11
movq   448(%rcx,%rdi),%r11

# qhasm: tt2d0 = t if =
# asm 1: cmove <t=int64#9,<tt2d0=int64#2
# asm 2: cmove <t=%r11,<tt2d0=%rsi
cmove %r11,%rsi

# qhasm: t = *(uint64 *)(basep + 456 + pos)
# asm 1: movq   456(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   456(<basep=%rcx,<pos=%rdi),>t=%r11
movq   456(%rcx,%rdi),%r11

# qhasm: tt2d1 = t if =
# asm 1: cmove <t=int64#9,<tt2d1=int64#6
# asm 2: cmove <t=%r11,<tt2d1=%r9
cmove %r11,%r9

# qhasm: t = *(uint64 *)(basep + 464 + pos)
# asm 1: movq   464(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   464(<basep=%rcx,<pos=%rdi),>t=%r11
movq   464(%rcx,%rdi),%r11

# qhasm: tt2d2 = t if =
# asm 1: cmove <t=int64#9,<tt2d2=int64#7
# asm 2: cmove <t=%r11,<tt2d2=%rax
cmove %r11,%rax

# qhasm: t = *(uint64 *)(basep + 472 + pos)
# asm 1: movq   472(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   472(<basep=%rcx,<pos=%rdi),>t=%r11
movq   472(%rcx,%rdi),%r11

# qhasm: tt2d3 = t if =
# asm 1: cmove <t=int64#9,<tt2d3=int64#8
# asm 2: cmove <t=%r11,<tt2d3=%r10
cmove %r11,%r10

# qhasm: =? u - 6
# asm 1: cmp  $6,<u=int64#5
# asm 2: cmp  $6,<u=%r8
cmp  $6,%r8

# qhasm: t = *(uint64 *)(basep + 544 + pos)
# asm 1: movq   544(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   544(<basep=%rcx,<pos=%rdi),>t=%r11
movq   544(%rcx,%rdi),%r11

# qhasm: tt2d0 = t if =
# asm 1: cmove <t=int64#9,<tt2d0=int64#2
# asm 2: cmove <t=%r11,<tt2d0=%rsi
cmove %r11,%rsi

# qhasm: t = *(uint64 *)(basep + 552 + pos)
# asm 1: movq   552(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   552(<basep=%rcx,<pos=%rdi),>t=%r11
movq   552(%rcx,%rdi),%r11

# qhasm: tt2d1 = t if =
# asm 1: cmove <t=int64#9,<tt2d1=int64#6
# asm 2: cmove <t=%r11,<tt2d1=%r9
cmove %r11,%r9

# qhasm: t = *(uint64 *)(basep + 560 + pos)
# asm 1: movq   560(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   560(<basep=%rcx,<pos=%rdi),>t=%r11
movq   560(%rcx,%rdi),%r11

# qhasm: tt2d2 = t if =
# asm 1: cmove <t=int64#9,<tt2d2=int64#7
# asm 2: cmove <t=%r11,<tt2d2=%rax
cmove %r11,%rax

# qhasm: t = *(uint64 *)(basep + 568 + pos)
# asm 1: movq   568(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   568(<basep=%rcx,<pos=%rdi),>t=%r11
movq   568(%rcx,%rdi),%r11

# qhasm: tt2d3 = t if =
# asm 1: cmove <t=int64#9,<tt2d3=int64#8
# asm 2: cmove <t=%r11,<tt2d3=%r10
cmove %r11,%r10

# qhasm: =? u - 7
# asm 1: cmp  $7,<u=int64#5
# asm 2: cmp  $7,<u=%r8
cmp  $7,%r8

# qhasm: t = *(uint64 *)(basep + 640 + pos)
# asm 1: movq   640(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   640(<basep=%rcx,<pos=%rdi),>t=%r11
movq   640(%rcx,%rdi),%r11

# qhasm: tt2d0 = t if =
# asm 1: cmove <t=int64#9,<tt2d0=int64#2
# asm 2: cmove <t=%r11,<tt2d0=%rsi
cmove %r11,%rsi

# qhasm: t = *(uint64 *)(basep + 648 + pos)
# asm 1: movq   648(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   648(<basep=%rcx,<pos=%rdi),>t=%r11
movq   648(%rcx,%rdi),%r11

# qhasm: tt2d1 = t if =
# asm 1: cmove <t=int64#9,<tt2d1=int64#6
# asm 2: cmove <t=%r11,<tt2d1=%r9
cmove %r11,%r9

# qhasm: t = *(uint64 *)(basep + 656 + pos)
# asm 1: movq   656(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   656(<basep=%rcx,<pos=%rdi),>t=%r11
movq   656(%rcx,%rdi),%r11

# qhasm: tt2d2 = t if =
# asm 1: cmove <t=int64#9,<tt2d2=int64#7
# asm 2: cmove <t=%r11,<tt2d2=%rax
cmove %r11,%rax

# qhasm: t = *(uint64 *)(basep + 664 + pos)
# asm 1: movq   664(<basep=int64#4,<pos=int64#1),>t=int64#9
# asm 2: movq   664(<basep=%rcx,<pos=%rdi),>t=%r11
movq   664(%rcx,%rdi),%r11

# qhasm: tt2d3 = t if =
# asm 1: cmove <t=int64#9,<tt2d3=int64#8
# asm 2: cmove <t=%r11,<tt2d3=%r10
cmove %r11,%r10

# qhasm: =? u - 8
# asm 1: cmp  $8,<u=int64#5
# asm 2: cmp  $8,<u=%r8
cmp  $8,%r8

# qhasm: t = *(uint64 *)(basep + 736 + pos)
# asm 1: movq   736(<basep=int64#4,<pos=int64#1),>t=int64#5
# asm 2: movq   736(<basep=%rcx,<pos=%rdi),>t=%r8
movq   736(%rcx,%rdi),%r8

# qhasm: tt2d0 = t if =
# asm 1: cmove <t=int64#5,<tt2d0=int64#2
# asm 2: cmove <t=%r8,<tt2d0=%rsi
cmove %r8,%rsi

# qhasm: t = *(uint64 *)(basep + 744 + pos)
# asm 1: movq   744(<basep=int64#4,<pos=int64#1),>t=int64#5
# asm 2: movq   744(<basep=%rcx,<pos=%rdi),>t=%r8
movq   744(%rcx,%rdi),%r8

# qhasm: tt2d1 = t if =
# asm 1: cmove <t=int64#5,<tt2d1=int64#6
# asm 2: cmove <t=%r8,<tt2d1=%r9
cmove %r8,%r9

# qhasm: t = *(uint64 *)(basep + 752 + pos)
# asm 1: movq   752(<basep=int64#4,<pos=int64#1),>t=int64#5
# asm 2: movq   752(<basep=%rcx,<pos=%rdi),>t=%r8
movq   752(%rcx,%rdi),%r8

# qhasm: tt2d2 = t if =
# asm 1: cmove <t=int64#5,<tt2d2=int64#7
# asm 2: cmove <t=%r8,<tt2d2=%rax
cmove %r8,%rax

# qhasm: t = *(uint64 *)(basep + 760 + pos)
# asm 1: movq   760(<basep=int64#4,<pos=int64#1),>t=int64#1
# asm 2: movq   760(<basep=%rcx,<pos=%rdi),>t=%rdi
movq   760(%rcx,%rdi),%rdi

# qhasm: tt2d3 = t if =
# asm 1: cmove <t=int64#1,<tt2d3=int64#8
# asm 2: cmove <t=%rdi,<tt2d3=%r10
cmove %rdi,%r10

# qhasm: tt0 = 0
# asm 1: mov  $0,>tt0=int64#1
# asm 2: mov  $0,>tt0=%rdi
mov  $0,%rdi

# qhasm: tt1 = 0
# asm 1: mov  $0,>tt1=int64#4
# asm 2: mov  $0,>tt1=%rcx
mov  $0,%rcx

# qhasm: tt2 = 0
# asm 1: mov  $0,>tt2=int64#5
# asm 2: mov  $0,>tt2=%r8
mov  $0,%r8

# qhasm: tt3 = 0
# asm 1: mov  $0,>tt3=int64#9
# asm 2: mov  $0,>tt3=%r11
mov  $0,%r11

# qhasm: carry? tt0 -= tt2d0
# asm 1: sub  <tt2d0=int64#2,<tt0=int64#1
# asm 2: sub  <tt2d0=%rsi,<tt0=%rdi
sub  %rsi,%rdi

# qhasm: carry? tt1 -= tt2d1 - carry
# asm 1: sbb  <tt2d1=int64#6,<tt1=int64#4
# asm 2: sbb  <tt2d1=%r9,<tt1=%rcx
sbb  %r9,%rcx

# qhasm: carry? tt2 -= tt2d2 - carry
# asm 1: sbb  <tt2d2=int64#7,<tt2=int64#5
# asm 2: sbb  <tt2d2=%rax,<tt2=%r8
sbb  %rax,%r8

# qhasm: carry? tt3 -= tt2d3 - carry
# asm 1: sbb  <tt2d3=int64#8,<tt3=int64#9
# asm 2: sbb  <tt2d3=%r10,<tt3=%r11
sbb  %r10,%r11

# qhasm: subt0 = 0
# asm 1: mov  $0,>subt0=int64#10
# asm 2: mov  $0,>subt0=%r12
mov  $0,%r12

# qhasm: subt1 = 38
# asm 1: mov  $38,>subt1=int64#11
# asm 2: mov  $38,>subt1=%r13
mov  $38,%r13

# qhasm: subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#10,<subt1=int64#11
# asm 2: cmovae <subt0=%r12,<subt1=%r13
cmovae %r12,%r13

# qhasm: carry? tt0 -= subt1
# asm 1: sub  <subt1=int64#11,<tt0=int64#1
# asm 2: sub  <subt1=%r13,<tt0=%rdi
sub  %r13,%rdi

# qhasm: carry? tt1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#10,<tt1=int64#4
# asm 2: sbb  <subt0=%r12,<tt1=%rcx
sbb  %r12,%rcx

# qhasm: carry? tt2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#10,<tt2=int64#5
# asm 2: sbb  <subt0=%r12,<tt2=%r8
sbb  %r12,%r8

# qhasm: carry? tt3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#10,<tt3=int64#9
# asm 2: sbb  <subt0=%r12,<tt3=%r11
sbb  %r12,%r11

# qhasm: subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#11,<subt0=int64#10
# asm 2: cmovc <subt1=%r13,<subt0=%r12
cmovc %r13,%r12

# qhasm: tt0 -= subt0
# asm 1: sub  <subt0=int64#10,<tt0=int64#1
# asm 2: sub  <subt0=%r12,<tt0=%rdi
sub  %r12,%rdi

# qhasm: signed<? b - 0
# asm 1: cmp  $0,<b=int64#3
# asm 2: cmp  $0,<b=%rdx
cmp  $0,%rdx

# qhasm: tt2d0 = tt0 if signed<
# asm 1: cmovl <tt0=int64#1,<tt2d0=int64#2
# asm 2: cmovl <tt0=%rdi,<tt2d0=%rsi
cmovl %rdi,%rsi

# qhasm: tt2d1 = tt1 if signed<
# asm 1: cmovl <tt1=int64#4,<tt2d1=int64#6
# asm 2: cmovl <tt1=%rcx,<tt2d1=%r9
cmovl %rcx,%r9

# qhasm: tt2d2 = tt2 if signed<
# asm 1: cmovl <tt2=int64#5,<tt2d2=int64#7
# asm 2: cmovl <tt2=%r8,<tt2d2=%rax
cmovl %r8,%rax

# qhasm: tt2d3 = tt3 if signed<
# asm 1: cmovl <tt3=int64#9,<tt2d3=int64#8
# asm 2: cmovl <tt3=%r11,<tt2d3=%r10
cmovl %r11,%r10

# qhasm: *(uint64 *)(tp + 64) = tt2d0
# asm 1: movq   <tt2d0=int64#2,64(<tp=int64#13)
# asm 2: movq   <tt2d0=%rsi,64(<tp=%r15)
movq   %rsi,64(%r15)

# qhasm: *(uint64 *)(tp + 72) = tt2d1
# asm 1: movq   <tt2d1=int64#6,72(<tp=int64#13)
# asm 2: movq   <tt2d1=%r9,72(<tp=%r15)
movq   %r9,72(%r15)

# qhasm: *(uint64 *)(tp + 80) = tt2d2
# asm 1: movq   <tt2d2=int64#7,80(<tp=int64#13)
# asm 2: movq   <tt2d2=%rax,80(<tp=%r15)
movq   %rax,80(%r15)

# qhasm: *(uint64 *)(tp + 88) = tt2d3
# asm 1: movq   <tt2d3=int64#8,88(<tp=int64#13)
# asm 2: movq   <tt2d3=%r10,88(<tp=%r15)
movq   %r10,88(%r15)

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
