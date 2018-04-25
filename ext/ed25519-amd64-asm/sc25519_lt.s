
# qhasm: int64 xp

# qhasm: int64 yp

# qhasm: int64 ret

# qhasm: input xp

# qhasm: input yp

# qhasm: output ret

# qhasm: int64 t0

# qhasm: int64 t1

# qhasm: int64 t2

# qhasm: int64 t3

# qhasm: int64 doof

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

# qhasm:   stack64 caller4_stack

# qhasm:   stack64 caller5_stack

# qhasm:   stack64 caller6_stack

# qhasm:   stack64 caller7_stack

# qhasm: enter crypto_sign_ed25519_amd64_64_sc25519_lt
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_sc25519_lt
.globl crypto_sign_ed25519_amd64_64_sc25519_lt
_crypto_sign_ed25519_amd64_64_sc25519_lt:
crypto_sign_ed25519_amd64_64_sc25519_lt:
mov %rsp,%r11
and $31,%r11
add $0,%r11
sub %r11,%rsp

# qhasm: t0 = *(uint64 *)(xp +  0)
# asm 1: movq   0(<xp=int64#1),>t0=int64#3
# asm 2: movq   0(<xp=%rdi),>t0=%rdx
movq   0(%rdi),%rdx

# qhasm: t1 = *(uint64 *)(xp +  8)
# asm 1: movq   8(<xp=int64#1),>t1=int64#4
# asm 2: movq   8(<xp=%rdi),>t1=%rcx
movq   8(%rdi),%rcx

# qhasm: t2 = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#1),>t2=int64#5
# asm 2: movq   16(<xp=%rdi),>t2=%r8
movq   16(%rdi),%r8

# qhasm: t3 = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#1),>t3=int64#1
# asm 2: movq   24(<xp=%rdi),>t3=%rdi
movq   24(%rdi),%rdi

# qhasm: carry? t0 -= *(uint64 *)(yp +  0)
# asm 1: subq 0(<yp=int64#2),<t0=int64#3
# asm 2: subq 0(<yp=%rsi),<t0=%rdx
subq 0(%rsi),%rdx

# qhasm: carry? t1 -= *(uint64 *)(yp +  8) - carry
# asm 1: sbbq 8(<yp=int64#2),<t1=int64#4
# asm 2: sbbq 8(<yp=%rsi),<t1=%rcx
sbbq 8(%rsi),%rcx

# qhasm: carry? t2 -= *(uint64 *)(yp + 16) - carry
# asm 1: sbbq 16(<yp=int64#2),<t2=int64#5
# asm 2: sbbq 16(<yp=%rsi),<t2=%r8
sbbq 16(%rsi),%r8

# qhasm: carry? t3 -= *(uint64 *)(yp + 24) - carry
# asm 1: sbbq 24(<yp=int64#2),<t3=int64#1
# asm 2: sbbq 24(<yp=%rsi),<t3=%rdi
sbbq 24(%rsi),%rdi

# qhasm: ret = 0
# asm 1: mov  $0,>ret=int64#1
# asm 2: mov  $0,>ret=%rdi
mov  $0,%rdi

# qhasm: doof = 1
# asm 1: mov  $1,>doof=int64#2
# asm 2: mov  $1,>doof=%rsi
mov  $1,%rsi

# qhasm: ret = doof if carry
# asm 1: cmovc <doof=int64#2,<ret=int64#1
# asm 2: cmovc <doof=%rsi,<ret=%rdi
cmovc %rsi,%rdi

# qhasm: leave
add %r11,%rsp
mov %rdi,%rax
mov %rsi,%rdx
ret
