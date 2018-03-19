
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

# qhasm: enter crypto_sign_ed25519_amd64_64_fe25519_sub
.text
.p2align 5
.globl _crypto_sign_ed25519_amd64_64_fe25519_sub
.globl crypto_sign_ed25519_amd64_64_fe25519_sub
_crypto_sign_ed25519_amd64_64_fe25519_sub:
crypto_sign_ed25519_amd64_64_fe25519_sub:
mov %rsp,%r11
and $31,%r11
add $0,%r11
sub %r11,%rsp

# qhasm: r0 = *(uint64 *)(xp + 0)
# asm 1: movq   0(<xp=int64#2),>r0=int64#4
# asm 2: movq   0(<xp=%rsi),>r0=%rcx
movq   0(%rsi),%rcx

# qhasm: r1 = *(uint64 *)(xp + 8)
# asm 1: movq   8(<xp=int64#2),>r1=int64#5
# asm 2: movq   8(<xp=%rsi),>r1=%r8
movq   8(%rsi),%r8

# qhasm: r2 = *(uint64 *)(xp + 16)
# asm 1: movq   16(<xp=int64#2),>r2=int64#6
# asm 2: movq   16(<xp=%rsi),>r2=%r9
movq   16(%rsi),%r9

# qhasm: r3 = *(uint64 *)(xp + 24)
# asm 1: movq   24(<xp=int64#2),>r3=int64#2
# asm 2: movq   24(<xp=%rsi),>r3=%rsi
movq   24(%rsi),%rsi

# qhasm:   carry? r0 -= *(uint64 *)(yp + 0) 
# asm 1: subq 0(<yp=int64#3),<r0=int64#4
# asm 2: subq 0(<yp=%rdx),<r0=%rcx
subq 0(%rdx),%rcx

# qhasm:   carry? r1 -= *(uint64 *)(yp + 8) - carry
# asm 1: sbbq 8(<yp=int64#3),<r1=int64#5
# asm 2: sbbq 8(<yp=%rdx),<r1=%r8
sbbq 8(%rdx),%r8

# qhasm:   carry? r2 -= *(uint64 *)(yp + 16) - carry
# asm 1: sbbq 16(<yp=int64#3),<r2=int64#6
# asm 2: sbbq 16(<yp=%rdx),<r2=%r9
sbbq 16(%rdx),%r9

# qhasm:   carry? r3 -= *(uint64 *)(yp + 24) - carry
# asm 1: sbbq 24(<yp=int64#3),<r3=int64#2
# asm 2: sbbq 24(<yp=%rdx),<r3=%rsi
sbbq 24(%rdx),%rsi

# qhasm:   subt0 = 0
# asm 1: mov  $0,>subt0=int64#3
# asm 2: mov  $0,>subt0=%rdx
mov  $0,%rdx

# qhasm:   subt1 = 38
# asm 1: mov  $38,>subt1=int64#7
# asm 2: mov  $38,>subt1=%rax
mov  $38,%rax

# qhasm:   subt1 = subt0 if !carry
# asm 1: cmovae <subt0=int64#3,<subt1=int64#7
# asm 2: cmovae <subt0=%rdx,<subt1=%rax
cmovae %rdx,%rax

# qhasm:   carry? r0 -= subt1
# asm 1: sub  <subt1=int64#7,<r0=int64#4
# asm 2: sub  <subt1=%rax,<r0=%rcx
sub  %rax,%rcx

# qhasm:   carry? r1 -= subt0 - carry
# asm 1: sbb  <subt0=int64#3,<r1=int64#5
# asm 2: sbb  <subt0=%rdx,<r1=%r8
sbb  %rdx,%r8

# qhasm:   carry? r2 -= subt0 - carry
# asm 1: sbb  <subt0=int64#3,<r2=int64#6
# asm 2: sbb  <subt0=%rdx,<r2=%r9
sbb  %rdx,%r9

# qhasm:   carry? r3 -= subt0 - carry
# asm 1: sbb  <subt0=int64#3,<r3=int64#2
# asm 2: sbb  <subt0=%rdx,<r3=%rsi
sbb  %rdx,%rsi

# qhasm:   subt0 = subt1 if carry
# asm 1: cmovc <subt1=int64#7,<subt0=int64#3
# asm 2: cmovc <subt1=%rax,<subt0=%rdx
cmovc %rax,%rdx

# qhasm:   r0 -= subt0
# asm 1: sub  <subt0=int64#3,<r0=int64#4
# asm 2: sub  <subt0=%rdx,<r0=%rcx
sub  %rdx,%rcx

# qhasm: *(uint64 *)(rp + 0) = r0
# asm 1: movq   <r0=int64#4,0(<rp=int64#1)
# asm 2: movq   <r0=%rcx,0(<rp=%rdi)
movq   %rcx,0(%rdi)

# qhasm: *(uint64 *)(rp + 8) = r1
# asm 1: movq   <r1=int64#5,8(<rp=int64#1)
# asm 2: movq   <r1=%r8,8(<rp=%rdi)
movq   %r8,8(%rdi)

# qhasm: *(uint64 *)(rp + 16) = r2
# asm 1: movq   <r2=int64#6,16(<rp=int64#1)
# asm 2: movq   <r2=%r9,16(<rp=%rdi)
movq   %r9,16(%rdi)

# qhasm: *(uint64 *)(rp + 24) = r3
# asm 1: movq   <r3=int64#2,24(<rp=int64#1)
# asm 2: movq   <r3=%rsi,24(<rp=%rdi)
movq   %rsi,24(%rdi)

# qhasm: leave
add %r11,%rsp
mov %rdi,%rax
mov %rsi,%rdx
ret
