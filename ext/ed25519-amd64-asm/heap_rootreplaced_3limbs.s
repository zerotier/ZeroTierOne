
# qhasm: int64 hp

# qhasm: int64 hlen

# qhasm: int64 sp

# qhasm: int64 pp

# qhasm: input hp

# qhasm: input hlen

# qhasm: input sp

# qhasm: int64 prc

# qhasm: int64 plc

# qhasm: int64 pc

# qhasm: int64 d

# qhasm: int64 spp

# qhasm: int64 sprc

# qhasm: int64 spc

# qhasm: int64 c0

# qhasm: int64 c1

# qhasm: int64 c2

# qhasm: int64 c3

# qhasm: int64 t0

# qhasm: int64 t1

# qhasm: int64 t2

# qhasm: int64 t3

# qhasm: int64 p0

# qhasm: int64 p1

# qhasm: int64 p2

# qhasm: int64 p3

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

# qhasm: enter crypto_sign_ed25519_amd64_64_heap_rootreplaced_3limbs
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_heap_rootreplaced_3limbs
.globl crypto_sign_ed25519_amd64_64_heap_rootreplaced_3limbs
_crypto_sign_ed25519_amd64_64_heap_rootreplaced_3limbs:
crypto_sign_ed25519_amd64_64_heap_rootreplaced_3limbs:
mov %rsp,%r11
and $31,%r11
add $64,%r11
sub %r11,%rsp

# qhasm: caller1_stack = caller1
# asm 1: movq <caller1=int64#9,>caller1_stack=stack64#1
# asm 2: movq <caller1=%r11,>caller1_stack=0(%rsp)
movq %r11,0(%rsp)

# qhasm: caller2_stack = caller2
# asm 1: movq <caller2=int64#10,>caller2_stack=stack64#2
# asm 2: movq <caller2=%r12,>caller2_stack=8(%rsp)
movq %r12,8(%rsp)

# qhasm: caller3_stack = caller3
# asm 1: movq <caller3=int64#11,>caller3_stack=stack64#3
# asm 2: movq <caller3=%r13,>caller3_stack=16(%rsp)
movq %r13,16(%rsp)

# qhasm: caller4_stack = caller4
# asm 1: movq <caller4=int64#12,>caller4_stack=stack64#4
# asm 2: movq <caller4=%r14,>caller4_stack=24(%rsp)
movq %r14,24(%rsp)

# qhasm: caller5_stack = caller5
# asm 1: movq <caller5=int64#13,>caller5_stack=stack64#5
# asm 2: movq <caller5=%r15,>caller5_stack=32(%rsp)
movq %r15,32(%rsp)

# qhasm: caller6_stack = caller6
# asm 1: movq <caller6=int64#14,>caller6_stack=stack64#6
# asm 2: movq <caller6=%rbx,>caller6_stack=40(%rsp)
movq %rbx,40(%rsp)

# qhasm: caller7_stack = caller7
# asm 1: movq <caller7=int64#15,>caller7_stack=stack64#7
# asm 2: movq <caller7=%rbp,>caller7_stack=48(%rsp)
movq %rbp,48(%rsp)

# qhasm: pp = 0
# asm 1: mov  $0,>pp=int64#4
# asm 2: mov  $0,>pp=%rcx
mov  $0,%rcx

# qhasm: siftdownloop:
._siftdownloop:

# qhasm: prc = pp
# asm 1: mov  <pp=int64#4,>prc=int64#5
# asm 2: mov  <pp=%rcx,>prc=%r8
mov  %rcx,%r8

# qhasm: prc *= 2
# asm 1: imulq  $2,<prc=int64#5,>prc=int64#5
# asm 2: imulq  $2,<prc=%r8,>prc=%r8
imulq  $2,%r8,%r8

# qhasm: pc = prc
# asm 1: mov  <prc=int64#5,>pc=int64#6
# asm 2: mov  <prc=%r8,>pc=%r9
mov  %r8,%r9

# qhasm: prc += 2
# asm 1: add  $2,<prc=int64#5
# asm 2: add  $2,<prc=%r8
add  $2,%r8

# qhasm: pc += 1
# asm 1: add  $1,<pc=int64#6
# asm 2: add  $1,<pc=%r9
add  $1,%r9

# qhasm: unsigned>? hlen - prc
# asm 1: cmp  <prc=int64#5,<hlen=int64#2
# asm 2: cmp  <prc=%r8,<hlen=%rsi
cmp  %r8,%rsi
# comment:fp stack unchanged by jump

# qhasm: goto siftuploop if !unsigned>
jbe ._siftuploop

# qhasm: sprc = *(uint64 *)(hp + prc * 8)
# asm 1: movq   (<hp=int64#1,<prc=int64#5,8),>sprc=int64#7
# asm 2: movq   (<hp=%rdi,<prc=%r8,8),>sprc=%rax
movq   (%rdi,%r8,8),%rax

# qhasm: sprc <<= 5
# asm 1: shl  $5,<sprc=int64#7
# asm 2: shl  $5,<sprc=%rax
shl  $5,%rax

# qhasm: sprc += sp
# asm 1: add  <sp=int64#3,<sprc=int64#7
# asm 2: add  <sp=%rdx,<sprc=%rax
add  %rdx,%rax

# qhasm: spc = *(uint64 *)(hp + pc * 8)
# asm 1: movq   (<hp=int64#1,<pc=int64#6,8),>spc=int64#8
# asm 2: movq   (<hp=%rdi,<pc=%r9,8),>spc=%r10
movq   (%rdi,%r9,8),%r10

# qhasm: spc <<= 5
# asm 1: shl  $5,<spc=int64#8
# asm 2: shl  $5,<spc=%r10
shl  $5,%r10

# qhasm: spc += sp
# asm 1: add  <sp=int64#3,<spc=int64#8
# asm 2: add  <sp=%rdx,<spc=%r10
add  %rdx,%r10

# qhasm: c0 = *(uint64 *)(spc +  0)
# asm 1: movq   0(<spc=int64#8),>c0=int64#9
# asm 2: movq   0(<spc=%r10),>c0=%r11
movq   0(%r10),%r11

# qhasm: c1 = *(uint64 *)(spc +  8)
# asm 1: movq   8(<spc=int64#8),>c1=int64#10
# asm 2: movq   8(<spc=%r10),>c1=%r12
movq   8(%r10),%r12

# qhasm: c2 = *(uint64 *)(spc + 16)
# asm 1: movq   16(<spc=int64#8),>c2=int64#11
# asm 2: movq   16(<spc=%r10),>c2=%r13
movq   16(%r10),%r13

# qhasm: carry? c0 -= *(uint64 *)(sprc +  0)
# asm 1: subq 0(<sprc=int64#7),<c0=int64#9
# asm 2: subq 0(<sprc=%rax),<c0=%r11
subq 0(%rax),%r11

# qhasm: carry? c1 -= *(uint64 *)(sprc +  8) - carry
# asm 1: sbbq 8(<sprc=int64#7),<c1=int64#10
# asm 2: sbbq 8(<sprc=%rax),<c1=%r12
sbbq 8(%rax),%r12

# qhasm: carry? c2 -= *(uint64 *)(sprc + 16) - carry
# asm 1: sbbq 16(<sprc=int64#7),<c2=int64#11
# asm 2: sbbq 16(<sprc=%rax),<c2=%r13
sbbq 16(%rax),%r13

# qhasm: pc = prc if carry
# asm 1: cmovc <prc=int64#5,<pc=int64#6
# asm 2: cmovc <prc=%r8,<pc=%r9
cmovc %r8,%r9

# qhasm: spc = sprc if carry
# asm 1: cmovc <sprc=int64#7,<spc=int64#8
# asm 2: cmovc <sprc=%rax,<spc=%r10
cmovc %rax,%r10

# qhasm: spc -= sp
# asm 1: sub  <sp=int64#3,<spc=int64#8
# asm 2: sub  <sp=%rdx,<spc=%r10
sub  %rdx,%r10

# qhasm: (uint64) spc >>= 5
# asm 1: shr  $5,<spc=int64#8
# asm 2: shr  $5,<spc=%r10
shr  $5,%r10

# qhasm: spp = *(uint64 *)(hp + pp * 8)
# asm 1: movq   (<hp=int64#1,<pp=int64#4,8),>spp=int64#5
# asm 2: movq   (<hp=%rdi,<pp=%rcx,8),>spp=%r8
movq   (%rdi,%rcx,8),%r8

# qhasm: *(uint64 *)(hp + pp * 8) = spc
# asm 1: movq  <spc=int64#8,(<hp=int64#1,<pp=int64#4,8)
# asm 2: movq  <spc=%r10,(<hp=%rdi,<pp=%rcx,8)
movq  %r10,(%rdi,%rcx,8)

# qhasm: *(uint64 *)(hp + pc * 8) = spp
# asm 1: movq  <spp=int64#5,(<hp=int64#1,<pc=int64#6,8)
# asm 2: movq  <spp=%r8,(<hp=%rdi,<pc=%r9,8)
movq  %r8,(%rdi,%r9,8)

# qhasm: pp = pc
# asm 1: mov  <pc=int64#6,>pp=int64#4
# asm 2: mov  <pc=%r9,>pp=%rcx
mov  %r9,%rcx
# comment:fp stack unchanged by jump

# qhasm: goto siftdownloop
jmp ._siftdownloop

# qhasm: siftuploop:
._siftuploop:

# qhasm: pc = pp
# asm 1: mov  <pp=int64#4,>pc=int64#2
# asm 2: mov  <pp=%rcx,>pc=%rsi
mov  %rcx,%rsi

# qhasm: pp -= 1
# asm 1: sub  $1,<pp=int64#4
# asm 2: sub  $1,<pp=%rcx
sub  $1,%rcx

# qhasm: (uint64) pp >>= 1
# asm 1: shr  $1,<pp=int64#4
# asm 2: shr  $1,<pp=%rcx
shr  $1,%rcx

# qhasm: unsigned>? pc - 0 
# asm 1: cmp  $0,<pc=int64#2
# asm 2: cmp  $0,<pc=%rsi
cmp  $0,%rsi
# comment:fp stack unchanged by jump

# qhasm: goto end if !unsigned>
jbe ._end

# qhasm: spp = *(uint64 *)(hp + pp * 8)
# asm 1: movq   (<hp=int64#1,<pp=int64#4,8),>spp=int64#5
# asm 2: movq   (<hp=%rdi,<pp=%rcx,8),>spp=%r8
movq   (%rdi,%rcx,8),%r8

# qhasm: spc = *(uint64 *)(hp + pc * 8)
# asm 1: movq   (<hp=int64#1,<pc=int64#2,8),>spc=int64#6
# asm 2: movq   (<hp=%rdi,<pc=%rsi,8),>spc=%r9
movq   (%rdi,%rsi,8),%r9

# qhasm: spp <<= 5
# asm 1: shl  $5,<spp=int64#5
# asm 2: shl  $5,<spp=%r8
shl  $5,%r8

# qhasm: spc <<= 5
# asm 1: shl  $5,<spc=int64#6
# asm 2: shl  $5,<spc=%r9
shl  $5,%r9

# qhasm: spc += sp
# asm 1: add  <sp=int64#3,<spc=int64#6
# asm 2: add  <sp=%rdx,<spc=%r9
add  %rdx,%r9

# qhasm: spp += sp
# asm 1: add  <sp=int64#3,<spp=int64#5
# asm 2: add  <sp=%rdx,<spp=%r8
add  %rdx,%r8

# qhasm: c0 = *(uint64 *)(spc +  0)
# asm 1: movq   0(<spc=int64#6),>c0=int64#7
# asm 2: movq   0(<spc=%r9),>c0=%rax
movq   0(%r9),%rax

# qhasm: c1 = *(uint64 *)(spc +  8)
# asm 1: movq   8(<spc=int64#6),>c1=int64#8
# asm 2: movq   8(<spc=%r9),>c1=%r10
movq   8(%r9),%r10

# qhasm: c2 = *(uint64 *)(spc + 16)
# asm 1: movq   16(<spc=int64#6),>c2=int64#9
# asm 2: movq   16(<spc=%r9),>c2=%r11
movq   16(%r9),%r11

# qhasm: carry? c0 -= *(uint64 *)(spp +  0)
# asm 1: subq 0(<spp=int64#5),<c0=int64#7
# asm 2: subq 0(<spp=%r8),<c0=%rax
subq 0(%r8),%rax

# qhasm: carry? c1 -= *(uint64 *)(spp +  8) - carry
# asm 1: sbbq 8(<spp=int64#5),<c1=int64#8
# asm 2: sbbq 8(<spp=%r8),<c1=%r10
sbbq 8(%r8),%r10

# qhasm: carry? c2 -= *(uint64 *)(spp + 16) - carry
# asm 1: sbbq 16(<spp=int64#5),<c2=int64#9
# asm 2: sbbq 16(<spp=%r8),<c2=%r11
sbbq 16(%r8),%r11
# comment:fp stack unchanged by jump

# qhasm: goto end if carry
jc ._end

# qhasm: spc -= sp
# asm 1: sub  <sp=int64#3,<spc=int64#6
# asm 2: sub  <sp=%rdx,<spc=%r9
sub  %rdx,%r9

# qhasm: (uint64) spc >>= 5
# asm 1: shr  $5,<spc=int64#6
# asm 2: shr  $5,<spc=%r9
shr  $5,%r9

# qhasm: spp -= sp
# asm 1: sub  <sp=int64#3,<spp=int64#5
# asm 2: sub  <sp=%rdx,<spp=%r8
sub  %rdx,%r8

# qhasm: (uint64) spp >>= 5
# asm 1: shr  $5,<spp=int64#5
# asm 2: shr  $5,<spp=%r8
shr  $5,%r8

# qhasm: *(uint64 *)(hp + pp * 8) = spc
# asm 1: movq  <spc=int64#6,(<hp=int64#1,<pp=int64#4,8)
# asm 2: movq  <spc=%r9,(<hp=%rdi,<pp=%rcx,8)
movq  %r9,(%rdi,%rcx,8)

# qhasm: *(uint64 *)(hp + pc * 8) = spp
# asm 1: movq  <spp=int64#5,(<hp=int64#1,<pc=int64#2,8)
# asm 2: movq  <spp=%r8,(<hp=%rdi,<pc=%rsi,8)
movq  %r8,(%rdi,%rsi,8)
# comment:fp stack unchanged by jump

# qhasm: goto siftuploop
jmp ._siftuploop

# qhasm: end:
._end:

# qhasm: caller1 = caller1_stack
# asm 1: movq <caller1_stack=stack64#1,>caller1=int64#9
# asm 2: movq <caller1_stack=0(%rsp),>caller1=%r11
movq 0(%rsp),%r11

# qhasm: caller2 = caller2_stack
# asm 1: movq <caller2_stack=stack64#2,>caller2=int64#10
# asm 2: movq <caller2_stack=8(%rsp),>caller2=%r12
movq 8(%rsp),%r12

# qhasm: caller3 = caller3_stack
# asm 1: movq <caller3_stack=stack64#3,>caller3=int64#11
# asm 2: movq <caller3_stack=16(%rsp),>caller3=%r13
movq 16(%rsp),%r13

# qhasm: caller4 = caller4_stack
# asm 1: movq <caller4_stack=stack64#4,>caller4=int64#12
# asm 2: movq <caller4_stack=24(%rsp),>caller4=%r14
movq 24(%rsp),%r14

# qhasm: caller5 = caller5_stack
# asm 1: movq <caller5_stack=stack64#5,>caller5=int64#13
# asm 2: movq <caller5_stack=32(%rsp),>caller5=%r15
movq 32(%rsp),%r15

# qhasm: caller6 = caller6_stack
# asm 1: movq <caller6_stack=stack64#6,>caller6=int64#14
# asm 2: movq <caller6_stack=40(%rsp),>caller6=%rbx
movq 40(%rsp),%rbx

# qhasm: caller7 = caller7_stack
# asm 1: movq <caller7_stack=stack64#7,>caller7=int64#15
# asm 2: movq <caller7_stack=48(%rsp),>caller7=%rbp
movq 48(%rsp),%rbp

# qhasm: leave
add %r11,%rsp
mov %rdi,%rax
mov %rsi,%rdx
ret
