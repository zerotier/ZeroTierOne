
# qhasm: int32 input_0

# qhasm: int32 input_1

# qhasm: int32 input_2

# qhasm: int32 input_3

# qhasm: stack32 input_4

# qhasm: stack32 input_5

# qhasm: stack32 input_6

# qhasm: stack32 input_7

# qhasm: int32 caller_r4

# qhasm: int32 caller_r5

# qhasm: int32 caller_r6

# qhasm: int32 caller_r7

# qhasm: int32 caller_r8

# qhasm: int32 caller_r9

# qhasm: int32 caller_r10

# qhasm: int32 caller_r11

# qhasm: int32 caller_r14

# qhasm: reg128 caller_q4

# qhasm: reg128 caller_q5

# qhasm: reg128 caller_q6

# qhasm: reg128 caller_q7

# qhasm: startcode
.fpu neon
.text

# qhasm: constant sigma:
.align 2
sigma:

# qhasm:   const32 1634760805
.word 1634760805

# qhasm:   const32 857760878
.word 857760878

# qhasm:   const32 2036477234
.word 2036477234

# qhasm:   const32 1797285236
.word 1797285236

# qhasm: int128 abab

# qhasm: int128 diag0

# qhasm: int128 diag1

# qhasm: int128 diag2

# qhasm: int128 diag3

# qhasm: int128 a0

# qhasm: int128 a1

# qhasm: int128 a2

# qhasm: int128 a3

# qhasm: int128 b0

# qhasm: int128 b1

# qhasm: int128 b2

# qhasm: int128 b3

# qhasm: int128 next_diag0

# qhasm: int128 next_diag1

# qhasm: int128 next_diag2

# qhasm: int128 next_diag3

# qhasm: int128 next_a0

# qhasm: int128 next_a1

# qhasm: int128 next_a2

# qhasm: int128 next_a3

# qhasm: int128 next_b0

# qhasm: int128 next_b1

# qhasm: int128 next_b2

# qhasm: int128 next_b3

# qhasm: int128 x0x5x10x15

# qhasm: int128 x12x1x6x11

# qhasm: int128 x8x13x2x7

# qhasm: int128 x4x9x14x3

# qhasm: int128 x0x1x10x11

# qhasm: int128 x12x13x6x7

# qhasm: int128 x8x9x2x3

# qhasm: int128 x4x5x14x15

# qhasm: int128 x0x1x2x3

# qhasm: int128 x4x5x6x7

# qhasm: int128 x8x9x10x11

# qhasm: int128 x12x13x14x15

# qhasm: int128 m0m1m2m3

# qhasm: int128 m4m5m6m7

# qhasm: int128 m8m9m10m11

# qhasm: int128 m12m13m14m15

# qhasm: int128 start0

# qhasm: int128 start1

# qhasm: int128 start2

# qhasm: int128 start3

# qhasm: stack128 stack_start3

# qhasm: stack128 next_start2

# qhasm: stack128 next_start3

# qhasm: int128 k0k1k2k3

# qhasm: int128 k4k5k6k7

# qhasm: int128 k1n1k7k2

# qhasm: int128 n2n3n3n2

# qhasm: int128 k2k3k6k7

# qhasm: int128 nextblock

# qhasm: stack128 stack_q4

# qhasm: stack128 stack_q5

# qhasm: stack128 stack_q6

# qhasm: stack128 stack_q7

# qhasm: stack32 stack_r4

# qhasm: stack128 k2k3k6k7_stack

# qhasm: stack128 k1n1k7k2_stack

# qhasm: stack512 tmp

# qhasm: stack32 savec

# qhasm: int32 i

# qhasm: int32 ci

# qhasm: int32 mi

# qhasm: enter zt_salsa2012_armneon3_xor
.align 2
.global _zt_salsa2012_armneon3_xor
.global zt_salsa2012_armneon3_xor
.type _zt_salsa2012_armneon3_xor STT_FUNC
.type zt_salsa2012_armneon3_xor STT_FUNC
_zt_salsa2012_armneon3_xor:
zt_salsa2012_armneon3_xor:
sub sp,sp,#256

# qhasm: new stack_q4

# qhasm: new stack_q5

# qhasm: new stack_q6

# qhasm: new stack_q7

# qhasm: stack_q4 bot = caller_q4 bot
# asm 1: vstr <caller_q4=reg128#5%bot,<stack_q4=stack128#1
# asm 2: vstr <caller_q4=d8,<stack_q4=[sp,#96]
vstr d8,[sp,#96]

# qhasm: stack_q4 top = caller_q4 top
# asm 1: vstr <caller_q4=reg128#5%top,<stack_q4=stack128#1
# asm 2: vstr <caller_q4=d9,<stack_q4=[sp,#104]
vstr d9,[sp,#104]

# qhasm: stack_q5 bot = caller_q5 bot
# asm 1: vstr <caller_q5=reg128#6%bot,<stack_q5=stack128#2
# asm 2: vstr <caller_q5=d10,<stack_q5=[sp,#112]
vstr d10,[sp,#112]

# qhasm: stack_q5 top = caller_q5 top
# asm 1: vstr <caller_q5=reg128#6%top,<stack_q5=stack128#2
# asm 2: vstr <caller_q5=d11,<stack_q5=[sp,#120]
vstr d11,[sp,#120]

# qhasm: stack_q6 bot = caller_q6 bot
# asm 1: vstr <caller_q6=reg128#7%bot,<stack_q6=stack128#3
# asm 2: vstr <caller_q6=d12,<stack_q6=[sp,#128]
vstr d12,[sp,#128]

# qhasm: stack_q6 top = caller_q6 top
# asm 1: vstr <caller_q6=reg128#7%top,<stack_q6=stack128#3
# asm 2: vstr <caller_q6=d13,<stack_q6=[sp,#136]
vstr d13,[sp,#136]

# qhasm: stack_q7 bot = caller_q7 bot
# asm 1: vstr <caller_q7=reg128#8%bot,<stack_q7=stack128#4
# asm 2: vstr <caller_q7=d14,<stack_q7=[sp,#144]
vstr d14,[sp,#144]

# qhasm: stack_q7 top = caller_q7 top
# asm 1: vstr <caller_q7=reg128#8%top,<stack_q7=stack128#4
# asm 2: vstr <caller_q7=d15,<stack_q7=[sp,#152]
vstr d15,[sp,#152]

# qhasm: new stack_r4

# qhasm: stack_r4 = caller_r4
# asm 1: str <caller_r4=int32#5,>stack_r4=stack32#2
# asm 2: str <caller_r4=r4,>stack_r4=[sp,#68]
str r4,[sp,#68]

# qhasm: int32 c

# qhasm: c = input_0
# asm 1: mov >c=int32#1,<input_0=int32#1
# asm 2: mov >c=r0,<input_0=r0
mov r0,r0

# qhasm: int32 m

# qhasm: m = input_1
# asm 1: mov >m=int32#2,<input_1=int32#2
# asm 2: mov >m=r1,<input_1=r1
mov r1,r1

# qhasm: int32 mlenlow

# qhasm: mlenlow = input_2
# asm 1: mov >mlenlow=int32#3,<input_2=int32#3
# asm 2: mov >mlenlow=r2,<input_2=r2
mov r2,r2

# qhasm: int32 mlenhigh

# qhasm: mlenhigh = input_3
# asm 1: mov >mlenhigh=int32#4,<input_3=int32#4
# asm 2: mov >mlenhigh=r3,<input_3=r3
mov r3,r3

# qhasm: int32 n

# qhasm: n = input_4
# asm 1: ldr >n=int32#5,<input_4=stack32#arg1
# asm 2: ldr >n=r4,<input_4=[sp,#256]
ldr r4,[sp,#256]

# qhasm: int32 k

# qhasm: k = input_5
# asm 1: ldr >k=int32#13,<input_5=stack32#arg2
# asm 2: ldr >k=r12,<input_5=[sp,#260]
ldr r12,[sp,#260]

# qhasm: k0k1k2k3 = mem128[k]
# asm 1: vld1.8 {>k0k1k2k3=reg128#1%bot->k0k1k2k3=reg128#1%top},[<k=int32#13]
# asm 2: vld1.8 {>k0k1k2k3=d0->k0k1k2k3=d1},[<k=r12]
vld1.8 {d0-d1},[r12]

# qhasm: k += 16
# asm 1: add <k=int32#13,<k=int32#13,#16
# asm 2: add <k=r12,<k=r12,#16
add r12,r12,#16

# qhasm: k4k5k6k7 = mem128[k]
# asm 1: vld1.8 {>k4k5k6k7=reg128#2%bot->k4k5k6k7=reg128#2%top},[<k=int32#13]
# asm 2: vld1.8 {>k4k5k6k7=d2->k4k5k6k7=d3},[<k=r12]
vld1.8 {d2-d3},[r12]

# qhasm: i = sigma
# asm 1: ldr >i=int32#13,=sigma
# asm 2: ldr >i=r12,=sigma
ldr r12,=sigma

# qhasm: start0 = mem128[i]
# asm 1: vld1.8 {>start0=reg128#3%bot->start0=reg128#3%top},[<i=int32#13]
# asm 2: vld1.8 {>start0=d4->start0=d5},[<i=r12]
vld1.8 {d4-d5},[r12]

# qhasm: 2x start1 = 0
# asm 1: vmov.i64 >start1=reg128#4,#0
# asm 2: vmov.i64 >start1=q3,#0
vmov.i64 q3,#0

# qhasm: start1 bot = mem64[n]                            
# asm 1: vld1.8 {<start1=reg128#4%bot},[<n=int32#5]
# asm 2: vld1.8 {<start1=d6},[<n=r4]
vld1.8 {d6},[r4]

# qhasm: start1 = start1[1] start1[0] start1[2,3]         
# asm 1: vext.32 <start1=reg128#4%bot,<start1=reg128#4%bot,<start1=reg128#4%bot,#1
# asm 2: vext.32 <start1=d6,<start1=d6,<start1=d6,#1
vext.32 d6,d6,d6,#1

# qhasm: start1 = start1[0,1] start1[1] k4k5k6k7[0]       
# asm 1: vext.32 <start1=reg128#4%top,<start1=reg128#4%bot,<k4k5k6k7=reg128#2%bot,#1
# asm 2: vext.32 <start1=d7,<start1=d6,<k4k5k6k7=d2,#1
vext.32 d7,d6,d2,#1

# qhasm: new k1n1k7k2

# qhasm: k1n1k7k2 = k0k1k2k3[1] start1[0] k1n1k7k2[2,3]   
# asm 1: vext.32 <k1n1k7k2=reg128#5%bot,<k0k1k2k3=reg128#1%bot,<start1=reg128#4%bot,#1
# asm 2: vext.32 <k1n1k7k2=d8,<k0k1k2k3=d0,<start1=d6,#1
vext.32 d8,d0,d6,#1

# qhasm: k1n1k7k2 = k1n1k7k2[0,1] k4k5k6k7[3] k0k1k2k3[2] 
# asm 1: vext.32 <k1n1k7k2=reg128#5%top,<k4k5k6k7=reg128#2%top,<k0k1k2k3=reg128#1%top,#1
# asm 2: vext.32 <k1n1k7k2=d9,<k4k5k6k7=d3,<k0k1k2k3=d1,#1
vext.32 d9,d3,d1,#1

# qhasm: k2k3k6k7 = k4k5k6k7
# asm 1: vmov >k2k3k6k7=reg128#6,<k4k5k6k7=reg128#2
# asm 2: vmov >k2k3k6k7=q5,<k4k5k6k7=q1
vmov q5,q1

# qhasm: k2k3k6k7 = k0k1k2k3[2,3] k2k3k6k7[2,3]
# asm 1: vmov <k2k3k6k7=reg128#6%bot,<k0k1k2k3=reg128#1%top
# asm 2: vmov <k2k3k6k7=d10,<k0k1k2k3=d1
vmov d10,d1

# qhasm: start1 = k4k5k6k7[1] k0k1k2k3[0] start1[2,3]     
# asm 1: vext.32 <start1=reg128#4%bot,<k4k5k6k7=reg128#2%bot,<k0k1k2k3=reg128#1%bot,#1
# asm 2: vext.32 <start1=d6,<k4k5k6k7=d2,<k0k1k2k3=d0,#1
vext.32 d6,d2,d0,#1

# qhasm: new k2k3k6k7_stack

# qhasm: k2k3k6k7_stack bot = k2k3k6k7 bot
# asm 1: vstr <k2k3k6k7=reg128#6%bot,<k2k3k6k7_stack=stack128#5
# asm 2: vstr <k2k3k6k7=d10,<k2k3k6k7_stack=[sp,#160]
vstr d10,[sp,#160]

# qhasm: k2k3k6k7_stack top = k2k3k6k7 top
# asm 1: vstr <k2k3k6k7=reg128#6%top,<k2k3k6k7_stack=stack128#5
# asm 2: vstr <k2k3k6k7=d11,<k2k3k6k7_stack=[sp,#168]
vstr d11,[sp,#168]

# qhasm: new k1n1k7k2_stack

# qhasm: k1n1k7k2_stack bot = k1n1k7k2 bot
# asm 1: vstr <k1n1k7k2=reg128#5%bot,<k1n1k7k2_stack=stack128#6
# asm 2: vstr <k1n1k7k2=d8,<k1n1k7k2_stack=[sp,#176]
vstr d8,[sp,#176]

# qhasm: k1n1k7k2_stack top = k1n1k7k2 top
# asm 1: vstr <k1n1k7k2=reg128#5%top,<k1n1k7k2_stack=stack128#6
# asm 2: vstr <k1n1k7k2=d9,<k1n1k7k2_stack=[sp,#184]
vstr d9,[sp,#184]

# qhasm: 2x n2n3n3n2 = 0
# asm 1: vmov.i64 >n2n3n3n2=reg128#1,#0
# asm 2: vmov.i64 >n2n3n3n2=q0,#0
vmov.i64 q0,#0

# qhasm:                         unsigned<? mlenlow - 128
# asm 1: cmp <mlenlow=int32#3,#128
# asm 2: cmp <mlenlow=r2,#128
cmp r2,#128

# qhasm: goto mlenlowbelow128 if unsigned<
blo ._mlenlowbelow128

# qhasm: mlenatleast128:
._mlenatleast128:

# qhasm:   new k2k3k6k7

# qhasm:   k2k3k6k7 bot = k2k3k6k7_stack bot
# asm 1: vldr <k2k3k6k7=reg128#2%bot,<k2k3k6k7_stack=stack128#5
# asm 2: vldr <k2k3k6k7=d2,<k2k3k6k7_stack=[sp,#160]
vldr d2,[sp,#160]

# qhasm:   k2k3k6k7 top = k2k3k6k7_stack top
# asm 1: vldr <k2k3k6k7=reg128#2%top,<k2k3k6k7_stack=stack128#5
# asm 2: vldr <k2k3k6k7=d3,<k2k3k6k7_stack=[sp,#168]
vldr d3,[sp,#168]

# qhasm:   new k1n1k7k2

# qhasm:   k1n1k7k2 bot = k1n1k7k2_stack bot
# asm 1: vldr <k1n1k7k2=reg128#5%bot,<k1n1k7k2_stack=stack128#6
# asm 2: vldr <k1n1k7k2=d8,<k1n1k7k2_stack=[sp,#176]
vldr d8,[sp,#176]

# qhasm:   k1n1k7k2 top = k1n1k7k2_stack top
# asm 1: vldr <k1n1k7k2=reg128#5%top,<k1n1k7k2_stack=stack128#6
# asm 2: vldr <k1n1k7k2=d9,<k1n1k7k2_stack=[sp,#184]
vldr d9,[sp,#184]

# qhasm:   n2n3n3n2 = n2n3n3n2[0,1] n2n3n3n2[1] n2n3n3n2[0]
# asm 1: vext.32 <n2n3n3n2=reg128#1%top,<n2n3n3n2=reg128#1%bot,<n2n3n3n2=reg128#1%bot,#1
# asm 2: vext.32 <n2n3n3n2=d1,<n2n3n3n2=d0,<n2n3n3n2=d0,#1
vext.32 d1,d0,d0,#1

# qhasm:   new diag2

# qhasm:   diag2 = diag2[0,1] k1n1k7k2[0,1]             
# asm 1: vmov <diag2=reg128#6%top,<k1n1k7k2=reg128#5%bot
# asm 2: vmov <diag2=d11,<k1n1k7k2=d8
vmov d11,d8

# qhasm:   diag2 = n2n3n3n2[3] k2k3k6k7[2] diag2[2,3]   
# asm 1: vext.32 <diag2=reg128#6%bot,<n2n3n3n2=reg128#1%top,<k2k3k6k7=reg128#2%top,#1
# asm 2: vext.32 <diag2=d10,<n2n3n3n2=d1,<k2k3k6k7=d3,#1
vext.32 d10,d1,d3,#1

# qhasm:   new diag3

# qhasm:   diag3 = diag3[0,1] k1n1k7k2[2,3]             
# asm 1: vmov <diag3=reg128#7%top,<k1n1k7k2=reg128#5%top
# asm 2: vmov <diag3=d13,<k1n1k7k2=d9
vmov d13,d9

# qhasm:   diag3 = k2k3k6k7[1] n2n3n3n2[2] diag3[2,3]   
# asm 1: vext.32 <diag3=reg128#7%bot,<k2k3k6k7=reg128#2%bot,<n2n3n3n2=reg128#1%top,#1
# asm 2: vext.32 <diag3=d12,<k2k3k6k7=d2,<n2n3n3n2=d1,#1
vext.32 d12,d2,d1,#1

# qhasm:   diag0 = start0
# asm 1: vmov >diag0=reg128#8,<start0=reg128#3
# asm 2: vmov >diag0=q7,<start0=q2
vmov q7,q2

# qhasm:   diag1 = start1
# asm 1: vmov >diag1=reg128#9,<start1=reg128#4
# asm 2: vmov >diag1=q8,<start1=q3
vmov q8,q3

# qhasm:   start2 = diag2
# asm 1: vmov >start2=reg128#10,<diag2=reg128#6
# asm 2: vmov >start2=q9,<diag2=q5
vmov q9,q5

# qhasm:   new stack_start3

# qhasm:   stack_start3 bot = diag3 bot
# asm 1: vstr <diag3=reg128#7%bot,<stack_start3=stack128#9
# asm 2: vstr <diag3=d12,<stack_start3=[sp,#224]
vstr d12,[sp,#224]

# qhasm:   stack_start3 top = diag3 top
# asm 1: vstr <diag3=reg128#7%top,<stack_start3=stack128#9
# asm 2: vstr <diag3=d13,<stack_start3=[sp,#232]
vstr d13,[sp,#232]

# qhasm:   2x nextblock = 0xff
# asm 1: vmov.i64 >nextblock=reg128#11,#0xff
# asm 2: vmov.i64 >nextblock=q10,#0xff
vmov.i64 q10,#0xff

# qhasm:   4x nextblock unsigned>>= 7
# asm 1: vshr.u32 >nextblock=reg128#11,<nextblock=reg128#11,#7
# asm 2: vshr.u32 >nextblock=q10,<nextblock=q10,#7
vshr.u32 q10,q10,#7

# qhasm:   2x n2n3n3n2 += nextblock
# asm 1: vadd.i64 >n2n3n3n2=reg128#1,<n2n3n3n2=reg128#1,<nextblock=reg128#11
# asm 2: vadd.i64 >n2n3n3n2=q0,<n2n3n3n2=q0,<nextblock=q10
vadd.i64 q0,q0,q10

# qhasm:   n2n3n3n2 = n2n3n3n2[0,1] n2n3n3n2[1] n2n3n3n2[0]
# asm 1: vext.32 <n2n3n3n2=reg128#1%top,<n2n3n3n2=reg128#1%bot,<n2n3n3n2=reg128#1%bot,#1
# asm 2: vext.32 <n2n3n3n2=d1,<n2n3n3n2=d0,<n2n3n3n2=d0,#1
vext.32 d1,d0,d0,#1

# qhasm:   new next_diag2

# qhasm:   next_diag2 = next_diag2[0,1] k1n1k7k2[0,1]
# asm 1: vmov <next_diag2=reg128#12%top,<k1n1k7k2=reg128#5%bot
# asm 2: vmov <next_diag2=d23,<k1n1k7k2=d8
vmov d23,d8

# qhasm:   next_diag2 = n2n3n3n2[3] k2k3k6k7[2] next_diag2[2,3]
# asm 1: vext.32 <next_diag2=reg128#12%bot,<n2n3n3n2=reg128#1%top,<k2k3k6k7=reg128#2%top,#1
# asm 2: vext.32 <next_diag2=d22,<n2n3n3n2=d1,<k2k3k6k7=d3,#1
vext.32 d22,d1,d3,#1

# qhasm:   new next_diag3

# qhasm:   next_diag3 = next_diag3[0,1] k1n1k7k2[2,3]
# asm 1: vmov <next_diag3=reg128#13%top,<k1n1k7k2=reg128#5%top
# asm 2: vmov <next_diag3=d25,<k1n1k7k2=d9
vmov d25,d9

# qhasm:   next_diag3 = k2k3k6k7[1] n2n3n3n2[2] next_diag3[2,3]
# asm 1: vext.32 <next_diag3=reg128#13%bot,<k2k3k6k7=reg128#2%bot,<n2n3n3n2=reg128#1%top,#1
# asm 2: vext.32 <next_diag3=d24,<k2k3k6k7=d2,<n2n3n3n2=d1,#1
vext.32 d24,d2,d1,#1

# qhasm:   2x n2n3n3n2 += nextblock
# asm 1: vadd.i64 >n2n3n3n2=reg128#1,<n2n3n3n2=reg128#1,<nextblock=reg128#11
# asm 2: vadd.i64 >n2n3n3n2=q0,<n2n3n3n2=q0,<nextblock=q10
vadd.i64 q0,q0,q10

# qhasm:   next_diag0 = diag0
# asm 1: vmov >next_diag0=reg128#2,<diag0=reg128#8
# asm 2: vmov >next_diag0=q1,<diag0=q7
vmov q1,q7

# qhasm:   next_diag1 = diag1
# asm 1: vmov >next_diag1=reg128#5,<diag1=reg128#9
# asm 2: vmov >next_diag1=q4,<diag1=q8
vmov q4,q8

# qhasm:   next_start2 bot = next_diag2 bot
# asm 1: vstr <next_diag2=reg128#12%bot,<next_start2=stack128#7
# asm 2: vstr <next_diag2=d22,<next_start2=[sp,#192]
vstr d22,[sp,#192]

# qhasm:   next_start2 top = next_diag2 top
# asm 1: vstr <next_diag2=reg128#12%top,<next_start2=stack128#7
# asm 2: vstr <next_diag2=d23,<next_start2=[sp,#200]
vstr d23,[sp,#200]

# qhasm:   next_start3 bot = next_diag3 bot
# asm 1: vstr <next_diag3=reg128#13%bot,<next_start3=stack128#8
# asm 2: vstr <next_diag3=d24,<next_start3=[sp,#208]
vstr d24,[sp,#208]

# qhasm:   next_start3 top = next_diag3 top
# asm 1: vstr <next_diag3=reg128#13%top,<next_start3=stack128#8
# asm 2: vstr <next_diag3=d25,<next_start3=[sp,#216]
vstr d25,[sp,#216]

# qhasm:   i = 12
# asm 1: ldr >i=int32#5,=12
# asm 2: ldr >i=r4,=12
ldr r4,=12

# qhasm:   mainloop2:
._mainloop2:

# qhasm:     4x a0 = diag1 + diag0
# asm 1: vadd.i32 >a0=reg128#11,<diag1=reg128#9,<diag0=reg128#8
# asm 2: vadd.i32 >a0=q10,<diag1=q8,<diag0=q7
vadd.i32 q10,q8,q7

# qhasm:    					4x next_a0 = next_diag1 + next_diag0
# asm 1: vadd.i32 >next_a0=reg128#14,<next_diag1=reg128#5,<next_diag0=reg128#2
# asm 2: vadd.i32 >next_a0=q13,<next_diag1=q4,<next_diag0=q1
vadd.i32 q13,q4,q1

# qhasm:     4x b0 = a0 << 7
# asm 1: vshl.i32 >b0=reg128#15,<a0=reg128#11,#7
# asm 2: vshl.i32 >b0=q14,<a0=q10,#7
vshl.i32 q14,q10,#7

# qhasm:     					4x next_b0 = next_a0 << 7
# asm 1: vshl.i32 >next_b0=reg128#16,<next_a0=reg128#14,#7
# asm 2: vshl.i32 >next_b0=q15,<next_a0=q13,#7
vshl.i32 q15,q13,#7

# qhasm:     4x b0 insert= a0 >> 25
# asm 1: vsri.i32 <b0=reg128#15,<a0=reg128#11,#25
# asm 2: vsri.i32 <b0=q14,<a0=q10,#25
vsri.i32 q14,q10,#25

# qhasm:     					4x next_b0 insert= next_a0 >> 25
# asm 1: vsri.i32 <next_b0=reg128#16,<next_a0=reg128#14,#25
# asm 2: vsri.i32 <next_b0=q15,<next_a0=q13,#25
vsri.i32 q15,q13,#25

# qhasm:        diag3 ^= b0
# asm 1: veor >diag3=reg128#7,<diag3=reg128#7,<b0=reg128#15
# asm 2: veor >diag3=q6,<diag3=q6,<b0=q14
veor q6,q6,q14

# qhasm:        					next_diag3 ^= next_b0
# asm 1: veor >next_diag3=reg128#11,<next_diag3=reg128#13,<next_b0=reg128#16
# asm 2: veor >next_diag3=q10,<next_diag3=q12,<next_b0=q15
veor q10,q12,q15

# qhasm:     4x a1 = diag0 + diag3
# asm 1: vadd.i32 >a1=reg128#13,<diag0=reg128#8,<diag3=reg128#7
# asm 2: vadd.i32 >a1=q12,<diag0=q7,<diag3=q6
vadd.i32 q12,q7,q6

# qhasm:     					4x next_a1 = next_diag0 + next_diag3
# asm 1: vadd.i32 >next_a1=reg128#14,<next_diag0=reg128#2,<next_diag3=reg128#11
# asm 2: vadd.i32 >next_a1=q13,<next_diag0=q1,<next_diag3=q10
vadd.i32 q13,q1,q10

# qhasm:     4x b1 = a1 << 9
# asm 1: vshl.i32 >b1=reg128#15,<a1=reg128#13,#9
# asm 2: vshl.i32 >b1=q14,<a1=q12,#9
vshl.i32 q14,q12,#9

# qhasm:     					4x next_b1 = next_a1 << 9
# asm 1: vshl.i32 >next_b1=reg128#16,<next_a1=reg128#14,#9
# asm 2: vshl.i32 >next_b1=q15,<next_a1=q13,#9
vshl.i32 q15,q13,#9

# qhasm:     4x b1 insert= a1 >> 23
# asm 1: vsri.i32 <b1=reg128#15,<a1=reg128#13,#23
# asm 2: vsri.i32 <b1=q14,<a1=q12,#23
vsri.i32 q14,q12,#23

# qhasm:     					4x next_b1 insert= next_a1 >> 23
# asm 1: vsri.i32 <next_b1=reg128#16,<next_a1=reg128#14,#23
# asm 2: vsri.i32 <next_b1=q15,<next_a1=q13,#23
vsri.i32 q15,q13,#23

# qhasm:        diag2 ^= b1
# asm 1: veor >diag2=reg128#6,<diag2=reg128#6,<b1=reg128#15
# asm 2: veor >diag2=q5,<diag2=q5,<b1=q14
veor q5,q5,q14

# qhasm:        					next_diag2 ^= next_b1
# asm 1: veor >next_diag2=reg128#12,<next_diag2=reg128#12,<next_b1=reg128#16
# asm 2: veor >next_diag2=q11,<next_diag2=q11,<next_b1=q15
veor q11,q11,q15

# qhasm:     4x a2 = diag3 + diag2
# asm 1: vadd.i32 >a2=reg128#13,<diag3=reg128#7,<diag2=reg128#6
# asm 2: vadd.i32 >a2=q12,<diag3=q6,<diag2=q5
vadd.i32 q12,q6,q5

# qhasm:             diag3 = diag3[3] diag3[0,1,2]
# asm 1: vext.32 >diag3=reg128#7,<diag3=reg128#7,<diag3=reg128#7,#3
# asm 2: vext.32 >diag3=q6,<diag3=q6,<diag3=q6,#3
vext.32 q6,q6,q6,#3

# qhasm:     					4x next_a2 = next_diag3 + next_diag2
# asm 1: vadd.i32 >next_a2=reg128#14,<next_diag3=reg128#11,<next_diag2=reg128#12
# asm 2: vadd.i32 >next_a2=q13,<next_diag3=q10,<next_diag2=q11
vadd.i32 q13,q10,q11

# qhasm:     4x b2 = a2 << 13
# asm 1: vshl.i32 >b2=reg128#15,<a2=reg128#13,#13
# asm 2: vshl.i32 >b2=q14,<a2=q12,#13
vshl.i32 q14,q12,#13

# qhasm:             					next_diag3 = next_diag3[3] next_diag3[0,1,2]
# asm 1: vext.32 >next_diag3=reg128#11,<next_diag3=reg128#11,<next_diag3=reg128#11,#3
# asm 2: vext.32 >next_diag3=q10,<next_diag3=q10,<next_diag3=q10,#3
vext.32 q10,q10,q10,#3

# qhasm:     					4x next_b2 = next_a2 << 13
# asm 1: vshl.i32 >next_b2=reg128#16,<next_a2=reg128#14,#13
# asm 2: vshl.i32 >next_b2=q15,<next_a2=q13,#13
vshl.i32 q15,q13,#13

# qhasm:     4x b2 insert= a2 >> 19
# asm 1: vsri.i32 <b2=reg128#15,<a2=reg128#13,#19
# asm 2: vsri.i32 <b2=q14,<a2=q12,#19
vsri.i32 q14,q12,#19

# qhasm:     					4x next_b2 insert= next_a2 >> 19
# asm 1: vsri.i32 <next_b2=reg128#16,<next_a2=reg128#14,#19
# asm 2: vsri.i32 <next_b2=q15,<next_a2=q13,#19
vsri.i32 q15,q13,#19

# qhasm:        diag1 ^= b2
# asm 1: veor >diag1=reg128#9,<diag1=reg128#9,<b2=reg128#15
# asm 2: veor >diag1=q8,<diag1=q8,<b2=q14
veor q8,q8,q14

# qhasm:        					next_diag1 ^= next_b2
# asm 1: veor >next_diag1=reg128#5,<next_diag1=reg128#5,<next_b2=reg128#16
# asm 2: veor >next_diag1=q4,<next_diag1=q4,<next_b2=q15
veor q4,q4,q15

# qhasm:     4x a3 = diag2 + diag1
# asm 1: vadd.i32 >a3=reg128#13,<diag2=reg128#6,<diag1=reg128#9
# asm 2: vadd.i32 >a3=q12,<diag2=q5,<diag1=q8
vadd.i32 q12,q5,q8

# qhasm:             diag2 = diag2[2,3] diag2[0,1]
# asm 1: vswp <diag2=reg128#6%bot,<diag2=reg128#6%top
# asm 2: vswp <diag2=d10,<diag2=d11
vswp d10,d11

# qhasm:     					4x next_a3 = next_diag2 + next_diag1
# asm 1: vadd.i32 >next_a3=reg128#14,<next_diag2=reg128#12,<next_diag1=reg128#5
# asm 2: vadd.i32 >next_a3=q13,<next_diag2=q11,<next_diag1=q4
vadd.i32 q13,q11,q4

# qhasm:     4x b3 = a3 << 18
# asm 1: vshl.i32 >b3=reg128#15,<a3=reg128#13,#18
# asm 2: vshl.i32 >b3=q14,<a3=q12,#18
vshl.i32 q14,q12,#18

# qhasm:             					next_diag2 = next_diag2[2,3] next_diag2[0,1]
# asm 1: vswp <next_diag2=reg128#12%bot,<next_diag2=reg128#12%top
# asm 2: vswp <next_diag2=d22,<next_diag2=d23
vswp d22,d23

# qhasm:     					4x next_b3 = next_a3 << 18
# asm 1: vshl.i32 >next_b3=reg128#16,<next_a3=reg128#14,#18
# asm 2: vshl.i32 >next_b3=q15,<next_a3=q13,#18
vshl.i32 q15,q13,#18

# qhasm:     4x b3 insert= a3 >> 14
# asm 1: vsri.i32 <b3=reg128#15,<a3=reg128#13,#14
# asm 2: vsri.i32 <b3=q14,<a3=q12,#14
vsri.i32 q14,q12,#14

# qhasm:             diag1 = diag1[1,2,3] diag1[0]
# asm 1: vext.32 >diag1=reg128#9,<diag1=reg128#9,<diag1=reg128#9,#1
# asm 2: vext.32 >diag1=q8,<diag1=q8,<diag1=q8,#1
vext.32 q8,q8,q8,#1

# qhasm:     					4x next_b3 insert= next_a3 >> 14
# asm 1: vsri.i32 <next_b3=reg128#16,<next_a3=reg128#14,#14
# asm 2: vsri.i32 <next_b3=q15,<next_a3=q13,#14
vsri.i32 q15,q13,#14

# qhasm:        diag0 ^= b3
# asm 1: veor >diag0=reg128#8,<diag0=reg128#8,<b3=reg128#15
# asm 2: veor >diag0=q7,<diag0=q7,<b3=q14
veor q7,q7,q14

# qhasm:             					next_diag1 = next_diag1[1,2,3] next_diag1[0]
# asm 1: vext.32 >next_diag1=reg128#5,<next_diag1=reg128#5,<next_diag1=reg128#5,#1
# asm 2: vext.32 >next_diag1=q4,<next_diag1=q4,<next_diag1=q4,#1
vext.32 q4,q4,q4,#1

# qhasm:        					next_diag0 ^= next_b3
# asm 1: veor >next_diag0=reg128#2,<next_diag0=reg128#2,<next_b3=reg128#16
# asm 2: veor >next_diag0=q1,<next_diag0=q1,<next_b3=q15
veor q1,q1,q15

# qhasm:     4x a0 = diag3 + diag0
# asm 1: vadd.i32 >a0=reg128#13,<diag3=reg128#7,<diag0=reg128#8
# asm 2: vadd.i32 >a0=q12,<diag3=q6,<diag0=q7
vadd.i32 q12,q6,q7

# qhasm:     					4x next_a0 = next_diag3 + next_diag0
# asm 1: vadd.i32 >next_a0=reg128#14,<next_diag3=reg128#11,<next_diag0=reg128#2
# asm 2: vadd.i32 >next_a0=q13,<next_diag3=q10,<next_diag0=q1
vadd.i32 q13,q10,q1

# qhasm:     4x b0 = a0 << 7
# asm 1: vshl.i32 >b0=reg128#15,<a0=reg128#13,#7
# asm 2: vshl.i32 >b0=q14,<a0=q12,#7
vshl.i32 q14,q12,#7

# qhasm:     					4x next_b0 = next_a0 << 7
# asm 1: vshl.i32 >next_b0=reg128#16,<next_a0=reg128#14,#7
# asm 2: vshl.i32 >next_b0=q15,<next_a0=q13,#7
vshl.i32 q15,q13,#7

# qhasm:     4x b0 insert= a0 >> 25
# asm 1: vsri.i32 <b0=reg128#15,<a0=reg128#13,#25
# asm 2: vsri.i32 <b0=q14,<a0=q12,#25
vsri.i32 q14,q12,#25

# qhasm:     					4x next_b0 insert= next_a0 >> 25
# asm 1: vsri.i32 <next_b0=reg128#16,<next_a0=reg128#14,#25
# asm 2: vsri.i32 <next_b0=q15,<next_a0=q13,#25
vsri.i32 q15,q13,#25

# qhasm:        diag1 ^= b0
# asm 1: veor >diag1=reg128#9,<diag1=reg128#9,<b0=reg128#15
# asm 2: veor >diag1=q8,<diag1=q8,<b0=q14
veor q8,q8,q14

# qhasm:        					next_diag1 ^= next_b0
# asm 1: veor >next_diag1=reg128#5,<next_diag1=reg128#5,<next_b0=reg128#16
# asm 2: veor >next_diag1=q4,<next_diag1=q4,<next_b0=q15
veor q4,q4,q15

# qhasm:     4x a1 = diag0 + diag1
# asm 1: vadd.i32 >a1=reg128#13,<diag0=reg128#8,<diag1=reg128#9
# asm 2: vadd.i32 >a1=q12,<diag0=q7,<diag1=q8
vadd.i32 q12,q7,q8

# qhasm:     					4x next_a1 = next_diag0 + next_diag1
# asm 1: vadd.i32 >next_a1=reg128#14,<next_diag0=reg128#2,<next_diag1=reg128#5
# asm 2: vadd.i32 >next_a1=q13,<next_diag0=q1,<next_diag1=q4
vadd.i32 q13,q1,q4

# qhasm:     4x b1 = a1 << 9
# asm 1: vshl.i32 >b1=reg128#15,<a1=reg128#13,#9
# asm 2: vshl.i32 >b1=q14,<a1=q12,#9
vshl.i32 q14,q12,#9

# qhasm:     					4x next_b1 = next_a1 << 9
# asm 1: vshl.i32 >next_b1=reg128#16,<next_a1=reg128#14,#9
# asm 2: vshl.i32 >next_b1=q15,<next_a1=q13,#9
vshl.i32 q15,q13,#9

# qhasm:     4x b1 insert= a1 >> 23
# asm 1: vsri.i32 <b1=reg128#15,<a1=reg128#13,#23
# asm 2: vsri.i32 <b1=q14,<a1=q12,#23
vsri.i32 q14,q12,#23

# qhasm:                   						unsigned>? i -= 2
# asm 1: subs <i=int32#5,<i=int32#5,#2
# asm 2: subs <i=r4,<i=r4,#2
subs r4,r4,#2

# qhasm:     					4x next_b1 insert= next_a1 >> 23
# asm 1: vsri.i32 <next_b1=reg128#16,<next_a1=reg128#14,#23
# asm 2: vsri.i32 <next_b1=q15,<next_a1=q13,#23
vsri.i32 q15,q13,#23

# qhasm:        diag2 ^= b1
# asm 1: veor >diag2=reg128#6,<diag2=reg128#6,<b1=reg128#15
# asm 2: veor >diag2=q5,<diag2=q5,<b1=q14
veor q5,q5,q14

# qhasm:        					next_diag2 ^= next_b1
# asm 1: veor >next_diag2=reg128#12,<next_diag2=reg128#12,<next_b1=reg128#16
# asm 2: veor >next_diag2=q11,<next_diag2=q11,<next_b1=q15
veor q11,q11,q15

# qhasm:     4x a2 = diag1 + diag2
# asm 1: vadd.i32 >a2=reg128#13,<diag1=reg128#9,<diag2=reg128#6
# asm 2: vadd.i32 >a2=q12,<diag1=q8,<diag2=q5
vadd.i32 q12,q8,q5

# qhasm:             diag1 = diag1[3] diag1[0,1,2]
# asm 1: vext.32 >diag1=reg128#9,<diag1=reg128#9,<diag1=reg128#9,#3
# asm 2: vext.32 >diag1=q8,<diag1=q8,<diag1=q8,#3
vext.32 q8,q8,q8,#3

# qhasm:     					4x next_a2 = next_diag1 + next_diag2
# asm 1: vadd.i32 >next_a2=reg128#14,<next_diag1=reg128#5,<next_diag2=reg128#12
# asm 2: vadd.i32 >next_a2=q13,<next_diag1=q4,<next_diag2=q11
vadd.i32 q13,q4,q11

# qhasm:     4x b2 = a2 << 13
# asm 1: vshl.i32 >b2=reg128#15,<a2=reg128#13,#13
# asm 2: vshl.i32 >b2=q14,<a2=q12,#13
vshl.i32 q14,q12,#13

# qhasm:             					next_diag1 = next_diag1[3] next_diag1[0,1,2]
# asm 1: vext.32 >next_diag1=reg128#5,<next_diag1=reg128#5,<next_diag1=reg128#5,#3
# asm 2: vext.32 >next_diag1=q4,<next_diag1=q4,<next_diag1=q4,#3
vext.32 q4,q4,q4,#3

# qhasm:     					4x next_b2 = next_a2 << 13
# asm 1: vshl.i32 >next_b2=reg128#16,<next_a2=reg128#14,#13
# asm 2: vshl.i32 >next_b2=q15,<next_a2=q13,#13
vshl.i32 q15,q13,#13

# qhasm:     4x b2 insert= a2 >> 19
# asm 1: vsri.i32 <b2=reg128#15,<a2=reg128#13,#19
# asm 2: vsri.i32 <b2=q14,<a2=q12,#19
vsri.i32 q14,q12,#19

# qhasm:     					4x next_b2 insert= next_a2 >> 19
# asm 1: vsri.i32 <next_b2=reg128#16,<next_a2=reg128#14,#19
# asm 2: vsri.i32 <next_b2=q15,<next_a2=q13,#19
vsri.i32 q15,q13,#19

# qhasm:        diag3 ^= b2
# asm 1: veor >diag3=reg128#7,<diag3=reg128#7,<b2=reg128#15
# asm 2: veor >diag3=q6,<diag3=q6,<b2=q14
veor q6,q6,q14

# qhasm:        					next_diag3 ^= next_b2
# asm 1: veor >next_diag3=reg128#11,<next_diag3=reg128#11,<next_b2=reg128#16
# asm 2: veor >next_diag3=q10,<next_diag3=q10,<next_b2=q15
veor q10,q10,q15

# qhasm:     4x a3 = diag2 + diag3
# asm 1: vadd.i32 >a3=reg128#13,<diag2=reg128#6,<diag3=reg128#7
# asm 2: vadd.i32 >a3=q12,<diag2=q5,<diag3=q6
vadd.i32 q12,q5,q6

# qhasm:             diag2 = diag2[2,3] diag2[0,1]
# asm 1: vswp <diag2=reg128#6%bot,<diag2=reg128#6%top
# asm 2: vswp <diag2=d10,<diag2=d11
vswp d10,d11

# qhasm:     					4x next_a3 = next_diag2 + next_diag3
# asm 1: vadd.i32 >next_a3=reg128#14,<next_diag2=reg128#12,<next_diag3=reg128#11
# asm 2: vadd.i32 >next_a3=q13,<next_diag2=q11,<next_diag3=q10
vadd.i32 q13,q11,q10

# qhasm:     4x b3 = a3 << 18
# asm 1: vshl.i32 >b3=reg128#15,<a3=reg128#13,#18
# asm 2: vshl.i32 >b3=q14,<a3=q12,#18
vshl.i32 q14,q12,#18

# qhasm:             					next_diag2 = next_diag2[2,3] next_diag2[0,1]
# asm 1: vswp <next_diag2=reg128#12%bot,<next_diag2=reg128#12%top
# asm 2: vswp <next_diag2=d22,<next_diag2=d23
vswp d22,d23

# qhasm:     					4x next_b3 = next_a3 << 18
# asm 1: vshl.i32 >next_b3=reg128#16,<next_a3=reg128#14,#18
# asm 2: vshl.i32 >next_b3=q15,<next_a3=q13,#18
vshl.i32 q15,q13,#18

# qhasm:     4x b3 insert= a3 >> 14
# asm 1: vsri.i32 <b3=reg128#15,<a3=reg128#13,#14
# asm 2: vsri.i32 <b3=q14,<a3=q12,#14
vsri.i32 q14,q12,#14

# qhasm:             diag3 = diag3[1,2,3] diag3[0]
# asm 1: vext.32 >diag3=reg128#7,<diag3=reg128#7,<diag3=reg128#7,#1
# asm 2: vext.32 >diag3=q6,<diag3=q6,<diag3=q6,#1
vext.32 q6,q6,q6,#1

# qhasm:     					4x next_b3 insert= next_a3 >> 14
# asm 1: vsri.i32 <next_b3=reg128#16,<next_a3=reg128#14,#14
# asm 2: vsri.i32 <next_b3=q15,<next_a3=q13,#14
vsri.i32 q15,q13,#14

# qhasm:        diag0 ^= b3
# asm 1: veor >diag0=reg128#8,<diag0=reg128#8,<b3=reg128#15
# asm 2: veor >diag0=q7,<diag0=q7,<b3=q14
veor q7,q7,q14

# qhasm:             					next_diag3 = next_diag3[1,2,3] next_diag3[0]
# asm 1: vext.32 >next_diag3=reg128#13,<next_diag3=reg128#11,<next_diag3=reg128#11,#1
# asm 2: vext.32 >next_diag3=q12,<next_diag3=q10,<next_diag3=q10,#1
vext.32 q12,q10,q10,#1

# qhasm:        					next_diag0 ^= next_b3
# asm 1: veor >next_diag0=reg128#2,<next_diag0=reg128#2,<next_b3=reg128#16
# asm 2: veor >next_diag0=q1,<next_diag0=q1,<next_b3=q15
veor q1,q1,q15

# qhasm:   goto mainloop2 if unsigned>
bhi ._mainloop2

# qhasm:   2x abab = 0xffffffff
# asm 1: vmov.i64 >abab=reg128#11,#0xffffffff
# asm 2: vmov.i64 >abab=q10,#0xffffffff
vmov.i64 q10,#0xffffffff

# qhasm:   new x4x9x14x3

# qhasm:   x4x9x14x3 bot = stack_start3 bot
# asm 1: vldr <x4x9x14x3=reg128#14%bot,<stack_start3=stack128#9
# asm 2: vldr <x4x9x14x3=d26,<stack_start3=[sp,#224]
vldr d26,[sp,#224]

# qhasm:   x4x9x14x3 top = stack_start3 top
# asm 1: vldr <x4x9x14x3=reg128#14%top,<stack_start3=stack128#9
# asm 2: vldr <x4x9x14x3=d27,<stack_start3=[sp,#232]
vldr d27,[sp,#232]

# qhasm:   4x x0x5x10x15 = diag0 + start0
# asm 1: vadd.i32 >x0x5x10x15=reg128#8,<diag0=reg128#8,<start0=reg128#3
# asm 2: vadd.i32 >x0x5x10x15=q7,<diag0=q7,<start0=q2
vadd.i32 q7,q7,q2

# qhasm:   4x x12x1x6x11 = diag1 + start1
# asm 1: vadd.i32 >x12x1x6x11=reg128#9,<diag1=reg128#9,<start1=reg128#4
# asm 2: vadd.i32 >x12x1x6x11=q8,<diag1=q8,<start1=q3
vadd.i32 q8,q8,q3

# qhasm:   4x x8x13x2x7 = diag2 + start2
# asm 1: vadd.i32 >x8x13x2x7=reg128#6,<diag2=reg128#6,<start2=reg128#10
# asm 2: vadd.i32 >x8x13x2x7=q5,<diag2=q5,<start2=q9
vadd.i32 q5,q5,q9

# qhasm:   4x x4x9x14x3 += diag3
# asm 1: vadd.i32 >x4x9x14x3=reg128#7,<x4x9x14x3=reg128#14,<diag3=reg128#7
# asm 2: vadd.i32 >x4x9x14x3=q6,<x4x9x14x3=q13,<diag3=q6
vadd.i32 q6,q13,q6

# qhasm:   x0x1x10x11 = x0x5x10x15
# asm 1: vmov >x0x1x10x11=reg128#10,<x0x5x10x15=reg128#8
# asm 2: vmov >x0x1x10x11=q9,<x0x5x10x15=q7
vmov q9,q7

# qhasm:   x12x13x6x7 = x12x1x6x11
# asm 1: vmov >x12x13x6x7=reg128#14,<x12x1x6x11=reg128#9
# asm 2: vmov >x12x13x6x7=q13,<x12x1x6x11=q8
vmov q13,q8

# qhasm:   x8x9x2x3 = x8x13x2x7
# asm 1: vmov >x8x9x2x3=reg128#15,<x8x13x2x7=reg128#6
# asm 2: vmov >x8x9x2x3=q14,<x8x13x2x7=q5
vmov q14,q5

# qhasm:   x4x5x14x15 = x4x9x14x3
# asm 1: vmov >x4x5x14x15=reg128#16,<x4x9x14x3=reg128#7
# asm 2: vmov >x4x5x14x15=q15,<x4x9x14x3=q6
vmov q15,q6

# qhasm:   x0x1x10x11 = (abab & x0x1x10x11) | (~abab & x12x1x6x11)
# asm 1: vbif <x0x1x10x11=reg128#10,<x12x1x6x11=reg128#9,<abab=reg128#11
# asm 2: vbif <x0x1x10x11=q9,<x12x1x6x11=q8,<abab=q10
vbif q9,q8,q10

# qhasm:   x12x13x6x7 = (abab & x12x13x6x7) | (~abab & x8x13x2x7)
# asm 1: vbif <x12x13x6x7=reg128#14,<x8x13x2x7=reg128#6,<abab=reg128#11
# asm 2: vbif <x12x13x6x7=q13,<x8x13x2x7=q5,<abab=q10
vbif q13,q5,q10

# qhasm:   x8x9x2x3 = (abab & x8x9x2x3) | (~abab & x4x9x14x3)
# asm 1: vbif <x8x9x2x3=reg128#15,<x4x9x14x3=reg128#7,<abab=reg128#11
# asm 2: vbif <x8x9x2x3=q14,<x4x9x14x3=q6,<abab=q10
vbif q14,q6,q10

# qhasm:   x4x5x14x15 = (abab & x4x5x14x15) | (~abab & x0x5x10x15)
# asm 1: vbif <x4x5x14x15=reg128#16,<x0x5x10x15=reg128#8,<abab=reg128#11
# asm 2: vbif <x4x5x14x15=q15,<x0x5x10x15=q7,<abab=q10
vbif q15,q7,q10

# qhasm:   x0x1x2x3 = x0x1x10x11
# asm 1: vmov >x0x1x2x3=reg128#6,<x0x1x10x11=reg128#10
# asm 2: vmov >x0x1x2x3=q5,<x0x1x10x11=q9
vmov q5,q9

# qhasm:   x4x5x6x7 = x4x5x14x15
# asm 1: vmov >x4x5x6x7=reg128#7,<x4x5x14x15=reg128#16
# asm 2: vmov >x4x5x6x7=q6,<x4x5x14x15=q15
vmov q6,q15

# qhasm:   x8x9x10x11 = x8x9x2x3
# asm 1: vmov >x8x9x10x11=reg128#8,<x8x9x2x3=reg128#15
# asm 2: vmov >x8x9x10x11=q7,<x8x9x2x3=q14
vmov q7,q14

# qhasm:   x12x13x14x15 = x12x13x6x7
# asm 1: vmov >x12x13x14x15=reg128#9,<x12x13x6x7=reg128#14
# asm 2: vmov >x12x13x14x15=q8,<x12x13x6x7=q13
vmov q8,q13

# qhasm:   x0x1x2x3 = x0x1x2x3[0,1] x8x9x2x3[2,3]
# asm 1: vmov <x0x1x2x3=reg128#6%top,<x8x9x2x3=reg128#15%top
# asm 2: vmov <x0x1x2x3=d11,<x8x9x2x3=d29
vmov d11,d29

# qhasm:   x4x5x6x7 = x4x5x6x7[0,1] x12x13x6x7[2,3]
# asm 1: vmov <x4x5x6x7=reg128#7%top,<x12x13x6x7=reg128#14%top
# asm 2: vmov <x4x5x6x7=d13,<x12x13x6x7=d27
vmov d13,d27

# qhasm:   x8x9x10x11 = x8x9x10x11[0,1] x0x1x10x11[2,3]
# asm 1: vmov <x8x9x10x11=reg128#8%top,<x0x1x10x11=reg128#10%top
# asm 2: vmov <x8x9x10x11=d15,<x0x1x10x11=d19
vmov d15,d19

# qhasm:   x12x13x14x15 = x12x13x14x15[0,1] x4x5x14x15[2,3]
# asm 1: vmov <x12x13x14x15=reg128#9%top,<x4x5x14x15=reg128#16%top
# asm 2: vmov <x12x13x14x15=d17,<x4x5x14x15=d31
vmov d17,d31

# qhasm:                      =? m - 0
# asm 1: cmp <m=int32#2,#0
# asm 2: cmp <m=r1,#0
cmp r1,#0

# qhasm:   goto nomessage2 if =
beq ._nomessage2

# qhasm:     m0m1m2m3 = mem128[m]
# asm 1: vld1.8 {>m0m1m2m3=reg128#10%bot->m0m1m2m3=reg128#10%top},[<m=int32#2]
# asm 2: vld1.8 {>m0m1m2m3=d18->m0m1m2m3=d19},[<m=r1]
vld1.8 {d18-d19},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m4m5m6m7 = mem128[m]
# asm 1: vld1.8 {>m4m5m6m7=reg128#14%bot->m4m5m6m7=reg128#14%top},[<m=int32#2]
# asm 2: vld1.8 {>m4m5m6m7=d26->m4m5m6m7=d27},[<m=r1]
vld1.8 {d26-d27},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m8m9m10m11 = mem128[m]
# asm 1: vld1.8 {>m8m9m10m11=reg128#15%bot->m8m9m10m11=reg128#15%top},[<m=int32#2]
# asm 2: vld1.8 {>m8m9m10m11=d28->m8m9m10m11=d29},[<m=r1]
vld1.8 {d28-d29},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m12m13m14m15 = mem128[m]
# asm 1: vld1.8 {>m12m13m14m15=reg128#16%bot->m12m13m14m15=reg128#16%top},[<m=int32#2]
# asm 2: vld1.8 {>m12m13m14m15=d30->m12m13m14m15=d31},[<m=r1]
vld1.8 {d30-d31},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     x0x1x2x3 ^= m0m1m2m3
# asm 1: veor >x0x1x2x3=reg128#6,<x0x1x2x3=reg128#6,<m0m1m2m3=reg128#10
# asm 2: veor >x0x1x2x3=q5,<x0x1x2x3=q5,<m0m1m2m3=q9
veor q5,q5,q9

# qhasm:     x4x5x6x7 ^= m4m5m6m7
# asm 1: veor >x4x5x6x7=reg128#7,<x4x5x6x7=reg128#7,<m4m5m6m7=reg128#14
# asm 2: veor >x4x5x6x7=q6,<x4x5x6x7=q6,<m4m5m6m7=q13
veor q6,q6,q13

# qhasm:     x8x9x10x11 ^= m8m9m10m11
# asm 1: veor >x8x9x10x11=reg128#8,<x8x9x10x11=reg128#8,<m8m9m10m11=reg128#15
# asm 2: veor >x8x9x10x11=q7,<x8x9x10x11=q7,<m8m9m10m11=q14
veor q7,q7,q14

# qhasm:     x12x13x14x15 ^= m12m13m14m15
# asm 1: veor >x12x13x14x15=reg128#9,<x12x13x14x15=reg128#9,<m12m13m14m15=reg128#16
# asm 2: veor >x12x13x14x15=q8,<x12x13x14x15=q8,<m12m13m14m15=q15
veor q8,q8,q15

# qhasm:   nomessage2:
._nomessage2:

# qhasm:   mem128[c] = x0x1x2x3
# asm 1: vst1.8 {<x0x1x2x3=reg128#6%bot-<x0x1x2x3=reg128#6%top},[<c=int32#1]
# asm 2: vst1.8 {<x0x1x2x3=d10-<x0x1x2x3=d11},[<c=r0]
vst1.8 {d10-d11},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x4x5x6x7
# asm 1: vst1.8 {<x4x5x6x7=reg128#7%bot-<x4x5x6x7=reg128#7%top},[<c=int32#1]
# asm 2: vst1.8 {<x4x5x6x7=d12-<x4x5x6x7=d13},[<c=r0]
vst1.8 {d12-d13},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x8x9x10x11
# asm 1: vst1.8 {<x8x9x10x11=reg128#8%bot-<x8x9x10x11=reg128#8%top},[<c=int32#1]
# asm 2: vst1.8 {<x8x9x10x11=d14-<x8x9x10x11=d15},[<c=r0]
vst1.8 {d14-d15},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x12x13x14x15
# asm 1: vst1.8 {<x12x13x14x15=reg128#9%bot-<x12x13x14x15=reg128#9%top},[<c=int32#1]
# asm 2: vst1.8 {<x12x13x14x15=d16-<x12x13x14x15=d17},[<c=r0]
vst1.8 {d16-d17},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   new x8x13x2x7

# qhasm:   x8x13x2x7 bot = next_start2 bot
# asm 1: vldr <x8x13x2x7=reg128#6%bot,<next_start2=stack128#7
# asm 2: vldr <x8x13x2x7=d10,<next_start2=[sp,#192]
vldr d10,[sp,#192]

# qhasm:   x8x13x2x7 top = next_start2 top
# asm 1: vldr <x8x13x2x7=reg128#6%top,<next_start2=stack128#7
# asm 2: vldr <x8x13x2x7=d11,<next_start2=[sp,#200]
vldr d11,[sp,#200]

# qhasm:   new x4x9x14x3

# qhasm:   x4x9x14x3 bot = next_start3 bot
# asm 1: vldr <x4x9x14x3=reg128#7%bot,<next_start3=stack128#8
# asm 2: vldr <x4x9x14x3=d12,<next_start3=[sp,#208]
vldr d12,[sp,#208]

# qhasm:   x4x9x14x3 top = next_start3 top
# asm 1: vldr <x4x9x14x3=reg128#7%top,<next_start3=stack128#8
# asm 2: vldr <x4x9x14x3=d13,<next_start3=[sp,#216]
vldr d13,[sp,#216]

# qhasm:   4x x0x5x10x15 = next_diag0 + start0
# asm 1: vadd.i32 >x0x5x10x15=reg128#2,<next_diag0=reg128#2,<start0=reg128#3
# asm 2: vadd.i32 >x0x5x10x15=q1,<next_diag0=q1,<start0=q2
vadd.i32 q1,q1,q2

# qhasm:   4x x12x1x6x11 = next_diag1 + start1
# asm 1: vadd.i32 >x12x1x6x11=reg128#5,<next_diag1=reg128#5,<start1=reg128#4
# asm 2: vadd.i32 >x12x1x6x11=q4,<next_diag1=q4,<start1=q3
vadd.i32 q4,q4,q3

# qhasm:   4x x8x13x2x7 += next_diag2
# asm 1: vadd.i32 >x8x13x2x7=reg128#6,<x8x13x2x7=reg128#6,<next_diag2=reg128#12
# asm 2: vadd.i32 >x8x13x2x7=q5,<x8x13x2x7=q5,<next_diag2=q11
vadd.i32 q5,q5,q11

# qhasm:   4x x4x9x14x3 += next_diag3
# asm 1: vadd.i32 >x4x9x14x3=reg128#7,<x4x9x14x3=reg128#7,<next_diag3=reg128#13
# asm 2: vadd.i32 >x4x9x14x3=q6,<x4x9x14x3=q6,<next_diag3=q12
vadd.i32 q6,q6,q12

# qhasm:   x0x1x10x11 = x0x5x10x15
# asm 1: vmov >x0x1x10x11=reg128#8,<x0x5x10x15=reg128#2
# asm 2: vmov >x0x1x10x11=q7,<x0x5x10x15=q1
vmov q7,q1

# qhasm:   x12x13x6x7 = x12x1x6x11
# asm 1: vmov >x12x13x6x7=reg128#9,<x12x1x6x11=reg128#5
# asm 2: vmov >x12x13x6x7=q8,<x12x1x6x11=q4
vmov q8,q4

# qhasm:   x8x9x2x3 = x8x13x2x7
# asm 1: vmov >x8x9x2x3=reg128#10,<x8x13x2x7=reg128#6
# asm 2: vmov >x8x9x2x3=q9,<x8x13x2x7=q5
vmov q9,q5

# qhasm:   x4x5x14x15 = x4x9x14x3
# asm 1: vmov >x4x5x14x15=reg128#12,<x4x9x14x3=reg128#7
# asm 2: vmov >x4x5x14x15=q11,<x4x9x14x3=q6
vmov q11,q6

# qhasm:   x0x1x10x11 = (abab & x0x1x10x11) | (~abab & x12x1x6x11)
# asm 1: vbif <x0x1x10x11=reg128#8,<x12x1x6x11=reg128#5,<abab=reg128#11
# asm 2: vbif <x0x1x10x11=q7,<x12x1x6x11=q4,<abab=q10
vbif q7,q4,q10

# qhasm:   x12x13x6x7 = (abab & x12x13x6x7) | (~abab & x8x13x2x7)
# asm 1: vbif <x12x13x6x7=reg128#9,<x8x13x2x7=reg128#6,<abab=reg128#11
# asm 2: vbif <x12x13x6x7=q8,<x8x13x2x7=q5,<abab=q10
vbif q8,q5,q10

# qhasm:   x8x9x2x3 = (abab & x8x9x2x3) | (~abab & x4x9x14x3)
# asm 1: vbif <x8x9x2x3=reg128#10,<x4x9x14x3=reg128#7,<abab=reg128#11
# asm 2: vbif <x8x9x2x3=q9,<x4x9x14x3=q6,<abab=q10
vbif q9,q6,q10

# qhasm:   x4x5x14x15 = (abab & x4x5x14x15) | (~abab & x0x5x10x15)
# asm 1: vbif <x4x5x14x15=reg128#12,<x0x5x10x15=reg128#2,<abab=reg128#11
# asm 2: vbif <x4x5x14x15=q11,<x0x5x10x15=q1,<abab=q10
vbif q11,q1,q10

# qhasm:   x0x1x2x3 = x0x1x10x11
# asm 1: vmov >x0x1x2x3=reg128#2,<x0x1x10x11=reg128#8
# asm 2: vmov >x0x1x2x3=q1,<x0x1x10x11=q7
vmov q1,q7

# qhasm:   x4x5x6x7 = x4x5x14x15
# asm 1: vmov >x4x5x6x7=reg128#5,<x4x5x14x15=reg128#12
# asm 2: vmov >x4x5x6x7=q4,<x4x5x14x15=q11
vmov q4,q11

# qhasm:   x8x9x10x11 = x8x9x2x3
# asm 1: vmov >x8x9x10x11=reg128#6,<x8x9x2x3=reg128#10
# asm 2: vmov >x8x9x10x11=q5,<x8x9x2x3=q9
vmov q5,q9

# qhasm:   x12x13x14x15 = x12x13x6x7
# asm 1: vmov >x12x13x14x15=reg128#7,<x12x13x6x7=reg128#9
# asm 2: vmov >x12x13x14x15=q6,<x12x13x6x7=q8
vmov q6,q8

# qhasm:   x0x1x2x3 = x0x1x2x3[0,1] x8x9x2x3[2,3]
# asm 1: vmov <x0x1x2x3=reg128#2%top,<x8x9x2x3=reg128#10%top
# asm 2: vmov <x0x1x2x3=d3,<x8x9x2x3=d19
vmov d3,d19

# qhasm:   x4x5x6x7 = x4x5x6x7[0,1] x12x13x6x7[2,3]
# asm 1: vmov <x4x5x6x7=reg128#5%top,<x12x13x6x7=reg128#9%top
# asm 2: vmov <x4x5x6x7=d9,<x12x13x6x7=d17
vmov d9,d17

# qhasm:   x8x9x10x11 = x8x9x10x11[0,1] x0x1x10x11[2,3]
# asm 1: vmov <x8x9x10x11=reg128#6%top,<x0x1x10x11=reg128#8%top
# asm 2: vmov <x8x9x10x11=d11,<x0x1x10x11=d15
vmov d11,d15

# qhasm:   x12x13x14x15 = x12x13x14x15[0,1] x4x5x14x15[2,3]
# asm 1: vmov <x12x13x14x15=reg128#7%top,<x4x5x14x15=reg128#12%top
# asm 2: vmov <x12x13x14x15=d13,<x4x5x14x15=d23
vmov d13,d23

# qhasm:                      =? m - 0
# asm 1: cmp <m=int32#2,#0
# asm 2: cmp <m=r1,#0
cmp r1,#0

# qhasm:   goto nomessage2next if =
beq ._nomessage2next

# qhasm:     m0m1m2m3 = mem128[m]
# asm 1: vld1.8 {>m0m1m2m3=reg128#8%bot->m0m1m2m3=reg128#8%top},[<m=int32#2]
# asm 2: vld1.8 {>m0m1m2m3=d14->m0m1m2m3=d15},[<m=r1]
vld1.8 {d14-d15},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m4m5m6m7 = mem128[m]
# asm 1: vld1.8 {>m4m5m6m7=reg128#9%bot->m4m5m6m7=reg128#9%top},[<m=int32#2]
# asm 2: vld1.8 {>m4m5m6m7=d16->m4m5m6m7=d17},[<m=r1]
vld1.8 {d16-d17},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m8m9m10m11 = mem128[m]
# asm 1: vld1.8 {>m8m9m10m11=reg128#10%bot->m8m9m10m11=reg128#10%top},[<m=int32#2]
# asm 2: vld1.8 {>m8m9m10m11=d18->m8m9m10m11=d19},[<m=r1]
vld1.8 {d18-d19},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m12m13m14m15 = mem128[m]
# asm 1: vld1.8 {>m12m13m14m15=reg128#11%bot->m12m13m14m15=reg128#11%top},[<m=int32#2]
# asm 2: vld1.8 {>m12m13m14m15=d20->m12m13m14m15=d21},[<m=r1]
vld1.8 {d20-d21},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     x0x1x2x3 ^= m0m1m2m3
# asm 1: veor >x0x1x2x3=reg128#2,<x0x1x2x3=reg128#2,<m0m1m2m3=reg128#8
# asm 2: veor >x0x1x2x3=q1,<x0x1x2x3=q1,<m0m1m2m3=q7
veor q1,q1,q7

# qhasm:     x4x5x6x7 ^= m4m5m6m7
# asm 1: veor >x4x5x6x7=reg128#5,<x4x5x6x7=reg128#5,<m4m5m6m7=reg128#9
# asm 2: veor >x4x5x6x7=q4,<x4x5x6x7=q4,<m4m5m6m7=q8
veor q4,q4,q8

# qhasm:     x8x9x10x11 ^= m8m9m10m11
# asm 1: veor >x8x9x10x11=reg128#6,<x8x9x10x11=reg128#6,<m8m9m10m11=reg128#10
# asm 2: veor >x8x9x10x11=q5,<x8x9x10x11=q5,<m8m9m10m11=q9
veor q5,q5,q9

# qhasm:     x12x13x14x15 ^= m12m13m14m15
# asm 1: veor >x12x13x14x15=reg128#7,<x12x13x14x15=reg128#7,<m12m13m14m15=reg128#11
# asm 2: veor >x12x13x14x15=q6,<x12x13x14x15=q6,<m12m13m14m15=q10
veor q6,q6,q10

# qhasm:   nomessage2next:
._nomessage2next:

# qhasm:   mem128[c] = x0x1x2x3
# asm 1: vst1.8 {<x0x1x2x3=reg128#2%bot-<x0x1x2x3=reg128#2%top},[<c=int32#1]
# asm 2: vst1.8 {<x0x1x2x3=d2-<x0x1x2x3=d3},[<c=r0]
vst1.8 {d2-d3},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x4x5x6x7
# asm 1: vst1.8 {<x4x5x6x7=reg128#5%bot-<x4x5x6x7=reg128#5%top},[<c=int32#1]
# asm 2: vst1.8 {<x4x5x6x7=d8-<x4x5x6x7=d9},[<c=r0]
vst1.8 {d8-d9},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x8x9x10x11
# asm 1: vst1.8 {<x8x9x10x11=reg128#6%bot-<x8x9x10x11=reg128#6%top},[<c=int32#1]
# asm 2: vst1.8 {<x8x9x10x11=d10-<x8x9x10x11=d11},[<c=r0]
vst1.8 {d10-d11},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x12x13x14x15
# asm 1: vst1.8 {<x12x13x14x15=reg128#7%bot-<x12x13x14x15=reg128#7%top},[<c=int32#1]
# asm 2: vst1.8 {<x12x13x14x15=d12-<x12x13x14x15=d13},[<c=r0]
vst1.8 {d12-d13},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   carry? mlenlow -= 128
# asm 1: subs <mlenlow=int32#3,<mlenlow=int32#3,#128
# asm 2: subs <mlenlow=r2,<mlenlow=r2,#128
subs r2,r2,#128

# qhasm:   mlenhigh -= 0 - carry
# asm 1: sbc <mlenhigh=int32#4,<mlenhigh=int32#4,#0
# asm 2: sbc <mlenhigh=r3,<mlenhigh=r3,#0
sbc r3,r3,#0

# qhasm:                         unsigned<? mlenlow - 128
# asm 1: cmp <mlenlow=int32#3,#128
# asm 2: cmp <mlenlow=r2,#128
cmp r2,#128

# qhasm: goto mlenatleast128 if !unsigned<
bhs ._mlenatleast128

# qhasm: mlenlowbelow128:
._mlenlowbelow128:

# qhasm:                        unsigned>? mlenhigh - 0
# asm 1: cmp <mlenhigh=int32#4,#0
# asm 2: cmp <mlenhigh=r3,#0
cmp r3,#0

# qhasm: goto mlenatleast128 if unsigned>
bhi ._mlenatleast128

# qhasm:              =? mlenlow - 0
# asm 1: cmp <mlenlow=int32#3,#0
# asm 2: cmp <mlenlow=r2,#0
cmp r2,#0

# qhasm: goto done if =
beq ._done

# qhasm: mlenatleast1:
._mlenatleast1:

# qhasm:                          unsigned<? mlenlow - 64
# asm 1: cmp <mlenlow=int32#3,#64
# asm 2: cmp <mlenlow=r2,#64
cmp r2,#64

# qhasm:   goto mlenatleast64 if !unsigned<
bhs ._mlenatleast64

# qhasm:     savec = c
# asm 1: str <c=int32#1,>savec=stack32#1
# asm 2: str <c=r0,>savec=[sp,#64]
str r0,[sp,#64]

# qhasm:     c = &tmp
# asm 1: lea >c=int32#1,<tmp=stack512#1
# asm 2: lea >c=r0,<tmp=[sp,#0]
add r0,sp,#0

# qhasm:                           =? m - 0
# asm 1: cmp <m=int32#2,#0
# asm 2: cmp <m=r1,#0
cmp r1,#0

# qhasm:     goto mlenatleast64 if =
beq ._mlenatleast64

# qhasm:     i = 0
# asm 1: ldr >i=int32#4,=0
# asm 2: ldr >i=r3,=0
ldr r3,=0

# qhasm:     mcopy:
._mcopy:

# qhasm:       mi = mem8[m + 0]
# asm 1: ldrb >mi=int32#5,[<m=int32#2,#0]
# asm 2: ldrb >mi=r4,[<m=r1,#0]
ldrb r4,[r1,#0]

# qhasm:       mem8[c + 0] = mi
# asm 1: strb <mi=int32#5,[<c=int32#1,#0]
# asm 2: strb <mi=r4,[<c=r0,#0]
strb r4,[r0,#0]

# qhasm:       m += 1
# asm 1: add <m=int32#2,<m=int32#2,#1
# asm 2: add <m=r1,<m=r1,#1
add r1,r1,#1

# qhasm:       c += 1
# asm 1: add <c=int32#1,<c=int32#1,#1
# asm 2: add <c=r0,<c=r0,#1
add r0,r0,#1

# qhasm:       i += 1
# asm 1: add <i=int32#4,<i=int32#4,#1
# asm 2: add <i=r3,<i=r3,#1
add r3,r3,#1

# qhasm:                   unsigned<? i - mlenlow
# asm 1: cmp <i=int32#4,<mlenlow=int32#3
# asm 2: cmp <i=r3,<mlenlow=r2
cmp r3,r2

# qhasm:     goto mcopy if unsigned<
blo ._mcopy

# qhasm:     mi = 0
# asm 1: ldr >mi=int32#2,=0
# asm 2: ldr >mi=r1,=0
ldr r1,=0

# qhasm:     pad:
._pad:

# qhasm:       mem8[c + 0] = mi
# asm 1: strb <mi=int32#2,[<c=int32#1,#0]
# asm 2: strb <mi=r1,[<c=r0,#0]
strb r1,[r0,#0]

# qhasm:       c += 1
# asm 1: add <c=int32#1,<c=int32#1,#1
# asm 2: add <c=r0,<c=r0,#1
add r0,r0,#1

# qhasm:       i += 1
# asm 1: add <i=int32#4,<i=int32#4,#1
# asm 2: add <i=r3,<i=r3,#1
add r3,r3,#1

# qhasm:                 unsigned<? i - 64
# asm 1: cmp <i=int32#4,#64
# asm 2: cmp <i=r3,#64
cmp r3,#64

# qhasm:     goto pad if unsigned<
blo ._pad

# qhasm:     c -= 64
# asm 1: sub <c=int32#1,<c=int32#1,#64
# asm 2: sub <c=r0,<c=r0,#64
sub r0,r0,#64

# qhasm:     m = &tmp
# asm 1: lea >m=int32#2,<tmp=stack512#1
# asm 2: lea >m=r1,<tmp=[sp,#0]
add r1,sp,#0

# qhasm:   mlenatleast64:
._mlenatleast64:

# qhasm:   new k2k3k6k7

# qhasm:   k2k3k6k7 bot = k2k3k6k7_stack bot
# asm 1: vldr <k2k3k6k7=reg128#2%bot,<k2k3k6k7_stack=stack128#5
# asm 2: vldr <k2k3k6k7=d2,<k2k3k6k7_stack=[sp,#160]
vldr d2,[sp,#160]

# qhasm:   k2k3k6k7 top = k2k3k6k7_stack top
# asm 1: vldr <k2k3k6k7=reg128#2%top,<k2k3k6k7_stack=stack128#5
# asm 2: vldr <k2k3k6k7=d3,<k2k3k6k7_stack=[sp,#168]
vldr d3,[sp,#168]

# qhasm:   new k1n1k7k2

# qhasm:   k1n1k7k2 bot = k1n1k7k2_stack bot
# asm 1: vldr <k1n1k7k2=reg128#5%bot,<k1n1k7k2_stack=stack128#6
# asm 2: vldr <k1n1k7k2=d8,<k1n1k7k2_stack=[sp,#176]
vldr d8,[sp,#176]

# qhasm:   k1n1k7k2 top = k1n1k7k2_stack top
# asm 1: vldr <k1n1k7k2=reg128#5%top,<k1n1k7k2_stack=stack128#6
# asm 2: vldr <k1n1k7k2=d9,<k1n1k7k2_stack=[sp,#184]
vldr d9,[sp,#184]

# qhasm:   n2n3n3n2 = n2n3n3n2[0,1] n2n3n3n2[1] n2n3n3n2[0]
# asm 1: vext.32 <n2n3n3n2=reg128#1%top,<n2n3n3n2=reg128#1%bot,<n2n3n3n2=reg128#1%bot,#1
# asm 2: vext.32 <n2n3n3n2=d1,<n2n3n3n2=d0,<n2n3n3n2=d0,#1
vext.32 d1,d0,d0,#1

# qhasm:   new start2

# qhasm:   start2 = start2[0,1] k1n1k7k2[0,1]             
# asm 1: vmov <start2=reg128#6%top,<k1n1k7k2=reg128#5%bot
# asm 2: vmov <start2=d11,<k1n1k7k2=d8
vmov d11,d8

# qhasm:   start2 = n2n3n3n2[3] k2k3k6k7[2] start2[2,3]   
# asm 1: vext.32 <start2=reg128#6%bot,<n2n3n3n2=reg128#1%top,<k2k3k6k7=reg128#2%top,#1
# asm 2: vext.32 <start2=d10,<n2n3n3n2=d1,<k2k3k6k7=d3,#1
vext.32 d10,d1,d3,#1

# qhasm:   new start3

# qhasm:   start3 = start3[0,1] k1n1k7k2[2,3]             
# asm 1: vmov <start3=reg128#7%top,<k1n1k7k2=reg128#5%top
# asm 2: vmov <start3=d13,<k1n1k7k2=d9
vmov d13,d9

# qhasm:   start3 = k2k3k6k7[1] n2n3n3n2[2] start3[2,3]   
# asm 1: vext.32 <start3=reg128#7%bot,<k2k3k6k7=reg128#2%bot,<n2n3n3n2=reg128#1%top,#1
# asm 2: vext.32 <start3=d12,<k2k3k6k7=d2,<n2n3n3n2=d1,#1
vext.32 d12,d2,d1,#1

# qhasm:   diag0 = start0
# asm 1: vmov >diag0=reg128#2,<start0=reg128#3
# asm 2: vmov >diag0=q1,<start0=q2
vmov q1,q2

# qhasm:   diag1 = start1
# asm 1: vmov >diag1=reg128#5,<start1=reg128#4
# asm 2: vmov >diag1=q4,<start1=q3
vmov q4,q3

# qhasm:   diag2 = start2
# asm 1: vmov >diag2=reg128#8,<start2=reg128#6
# asm 2: vmov >diag2=q7,<start2=q5
vmov q7,q5

# qhasm:   diag3 = start3
# asm 1: vmov >diag3=reg128#9,<start3=reg128#7
# asm 2: vmov >diag3=q8,<start3=q6
vmov q8,q6

# qhasm:   2x nextblock = 0xff
# asm 1: vmov.i64 >nextblock=reg128#10,#0xff
# asm 2: vmov.i64 >nextblock=q9,#0xff
vmov.i64 q9,#0xff

# qhasm:   4x nextblock unsigned>>= 7
# asm 1: vshr.u32 >nextblock=reg128#10,<nextblock=reg128#10,#7
# asm 2: vshr.u32 >nextblock=q9,<nextblock=q9,#7
vshr.u32 q9,q9,#7

# qhasm:   2x n2n3n3n2 += nextblock
# asm 1: vadd.i64 >n2n3n3n2=reg128#1,<n2n3n3n2=reg128#1,<nextblock=reg128#10
# asm 2: vadd.i64 >n2n3n3n2=q0,<n2n3n3n2=q0,<nextblock=q9
vadd.i64 q0,q0,q9

# qhasm:   i = 12
# asm 1: ldr >i=int32#4,=12
# asm 2: ldr >i=r3,=12
ldr r3,=12

# qhasm:   mainloop1:
._mainloop1:

# qhasm:     4x a0 = diag1 + diag0
# asm 1: vadd.i32 >a0=reg128#10,<diag1=reg128#5,<diag0=reg128#2
# asm 2: vadd.i32 >a0=q9,<diag1=q4,<diag0=q1
vadd.i32 q9,q4,q1

# qhasm:     4x b0 = a0 << 7
# asm 1: vshl.i32 >b0=reg128#11,<a0=reg128#10,#7
# asm 2: vshl.i32 >b0=q10,<a0=q9,#7
vshl.i32 q10,q9,#7

# qhasm:     4x b0 insert= a0 >> 25
# asm 1: vsri.i32 <b0=reg128#11,<a0=reg128#10,#25
# asm 2: vsri.i32 <b0=q10,<a0=q9,#25
vsri.i32 q10,q9,#25

# qhasm:        diag3 ^= b0
# asm 1: veor >diag3=reg128#9,<diag3=reg128#9,<b0=reg128#11
# asm 2: veor >diag3=q8,<diag3=q8,<b0=q10
veor q8,q8,q10

# qhasm:     4x a1 = diag0 + diag3
# asm 1: vadd.i32 >a1=reg128#10,<diag0=reg128#2,<diag3=reg128#9
# asm 2: vadd.i32 >a1=q9,<diag0=q1,<diag3=q8
vadd.i32 q9,q1,q8

# qhasm:     4x b1 = a1 << 9
# asm 1: vshl.i32 >b1=reg128#11,<a1=reg128#10,#9
# asm 2: vshl.i32 >b1=q10,<a1=q9,#9
vshl.i32 q10,q9,#9

# qhasm:     4x b1 insert= a1 >> 23
# asm 1: vsri.i32 <b1=reg128#11,<a1=reg128#10,#23
# asm 2: vsri.i32 <b1=q10,<a1=q9,#23
vsri.i32 q10,q9,#23

# qhasm:        diag2 ^= b1
# asm 1: veor >diag2=reg128#8,<diag2=reg128#8,<b1=reg128#11
# asm 2: veor >diag2=q7,<diag2=q7,<b1=q10
veor q7,q7,q10

# qhasm:     4x a2 = diag3 + diag2
# asm 1: vadd.i32 >a2=reg128#10,<diag3=reg128#9,<diag2=reg128#8
# asm 2: vadd.i32 >a2=q9,<diag3=q8,<diag2=q7
vadd.i32 q9,q8,q7

# qhasm:             diag3 = diag3[3] diag3[0,1,2]
# asm 1: vext.32 >diag3=reg128#9,<diag3=reg128#9,<diag3=reg128#9,#3
# asm 2: vext.32 >diag3=q8,<diag3=q8,<diag3=q8,#3
vext.32 q8,q8,q8,#3

# qhasm:     4x b2 = a2 << 13
# asm 1: vshl.i32 >b2=reg128#11,<a2=reg128#10,#13
# asm 2: vshl.i32 >b2=q10,<a2=q9,#13
vshl.i32 q10,q9,#13

# qhasm:     4x b2 insert= a2 >> 19
# asm 1: vsri.i32 <b2=reg128#11,<a2=reg128#10,#19
# asm 2: vsri.i32 <b2=q10,<a2=q9,#19
vsri.i32 q10,q9,#19

# qhasm:        diag1 ^= b2
# asm 1: veor >diag1=reg128#5,<diag1=reg128#5,<b2=reg128#11
# asm 2: veor >diag1=q4,<diag1=q4,<b2=q10
veor q4,q4,q10

# qhasm:     4x a3 = diag2 + diag1
# asm 1: vadd.i32 >a3=reg128#10,<diag2=reg128#8,<diag1=reg128#5
# asm 2: vadd.i32 >a3=q9,<diag2=q7,<diag1=q4
vadd.i32 q9,q7,q4

# qhasm:             diag2 = diag2[2,3] diag2[0,1]
# asm 1: vswp <diag2=reg128#8%bot,<diag2=reg128#8%top
# asm 2: vswp <diag2=d14,<diag2=d15
vswp d14,d15

# qhasm:     4x b3 = a3 << 18
# asm 1: vshl.i32 >b3=reg128#11,<a3=reg128#10,#18
# asm 2: vshl.i32 >b3=q10,<a3=q9,#18
vshl.i32 q10,q9,#18

# qhasm:     4x b3 insert= a3 >> 14
# asm 1: vsri.i32 <b3=reg128#11,<a3=reg128#10,#14
# asm 2: vsri.i32 <b3=q10,<a3=q9,#14
vsri.i32 q10,q9,#14

# qhasm:             diag1 = diag1[1,2,3] diag1[0]
# asm 1: vext.32 >diag1=reg128#5,<diag1=reg128#5,<diag1=reg128#5,#1
# asm 2: vext.32 >diag1=q4,<diag1=q4,<diag1=q4,#1
vext.32 q4,q4,q4,#1

# qhasm:        diag0 ^= b3
# asm 1: veor >diag0=reg128#2,<diag0=reg128#2,<b3=reg128#11
# asm 2: veor >diag0=q1,<diag0=q1,<b3=q10
veor q1,q1,q10

# qhasm:     4x a0 = diag3 + diag0
# asm 1: vadd.i32 >a0=reg128#10,<diag3=reg128#9,<diag0=reg128#2
# asm 2: vadd.i32 >a0=q9,<diag3=q8,<diag0=q1
vadd.i32 q9,q8,q1

# qhasm:     4x b0 = a0 << 7
# asm 1: vshl.i32 >b0=reg128#11,<a0=reg128#10,#7
# asm 2: vshl.i32 >b0=q10,<a0=q9,#7
vshl.i32 q10,q9,#7

# qhasm:     4x b0 insert= a0 >> 25
# asm 1: vsri.i32 <b0=reg128#11,<a0=reg128#10,#25
# asm 2: vsri.i32 <b0=q10,<a0=q9,#25
vsri.i32 q10,q9,#25

# qhasm:        diag1 ^= b0
# asm 1: veor >diag1=reg128#5,<diag1=reg128#5,<b0=reg128#11
# asm 2: veor >diag1=q4,<diag1=q4,<b0=q10
veor q4,q4,q10

# qhasm:     4x a1 = diag0 + diag1
# asm 1: vadd.i32 >a1=reg128#10,<diag0=reg128#2,<diag1=reg128#5
# asm 2: vadd.i32 >a1=q9,<diag0=q1,<diag1=q4
vadd.i32 q9,q1,q4

# qhasm:     4x b1 = a1 << 9
# asm 1: vshl.i32 >b1=reg128#11,<a1=reg128#10,#9
# asm 2: vshl.i32 >b1=q10,<a1=q9,#9
vshl.i32 q10,q9,#9

# qhasm:     4x b1 insert= a1 >> 23
# asm 1: vsri.i32 <b1=reg128#11,<a1=reg128#10,#23
# asm 2: vsri.i32 <b1=q10,<a1=q9,#23
vsri.i32 q10,q9,#23

# qhasm:                   						unsigned>? i -= 2
# asm 1: subs <i=int32#4,<i=int32#4,#2
# asm 2: subs <i=r3,<i=r3,#2
subs r3,r3,#2

# qhasm:        diag2 ^= b1
# asm 1: veor >diag2=reg128#8,<diag2=reg128#8,<b1=reg128#11
# asm 2: veor >diag2=q7,<diag2=q7,<b1=q10
veor q7,q7,q10

# qhasm:     4x a2 = diag1 + diag2
# asm 1: vadd.i32 >a2=reg128#10,<diag1=reg128#5,<diag2=reg128#8
# asm 2: vadd.i32 >a2=q9,<diag1=q4,<diag2=q7
vadd.i32 q9,q4,q7

# qhasm:             diag1 = diag1[3] diag1[0,1,2]
# asm 1: vext.32 >diag1=reg128#5,<diag1=reg128#5,<diag1=reg128#5,#3
# asm 2: vext.32 >diag1=q4,<diag1=q4,<diag1=q4,#3
vext.32 q4,q4,q4,#3

# qhasm:     4x b2 = a2 << 13
# asm 1: vshl.i32 >b2=reg128#11,<a2=reg128#10,#13
# asm 2: vshl.i32 >b2=q10,<a2=q9,#13
vshl.i32 q10,q9,#13

# qhasm:     4x b2 insert= a2 >> 19
# asm 1: vsri.i32 <b2=reg128#11,<a2=reg128#10,#19
# asm 2: vsri.i32 <b2=q10,<a2=q9,#19
vsri.i32 q10,q9,#19

# qhasm:        diag3 ^= b2
# asm 1: veor >diag3=reg128#9,<diag3=reg128#9,<b2=reg128#11
# asm 2: veor >diag3=q8,<diag3=q8,<b2=q10
veor q8,q8,q10

# qhasm:     4x a3 = diag2 + diag3
# asm 1: vadd.i32 >a3=reg128#10,<diag2=reg128#8,<diag3=reg128#9
# asm 2: vadd.i32 >a3=q9,<diag2=q7,<diag3=q8
vadd.i32 q9,q7,q8

# qhasm:             diag2 = diag2[2,3] diag2[0,1]
# asm 1: vswp <diag2=reg128#8%bot,<diag2=reg128#8%top
# asm 2: vswp <diag2=d14,<diag2=d15
vswp d14,d15

# qhasm:     4x b3 = a3 << 18
# asm 1: vshl.i32 >b3=reg128#11,<a3=reg128#10,#18
# asm 2: vshl.i32 >b3=q10,<a3=q9,#18
vshl.i32 q10,q9,#18

# qhasm:     4x b3 insert= a3 >> 14
# asm 1: vsri.i32 <b3=reg128#11,<a3=reg128#10,#14
# asm 2: vsri.i32 <b3=q10,<a3=q9,#14
vsri.i32 q10,q9,#14

# qhasm:             diag3 = diag3[1,2,3] diag3[0]
# asm 1: vext.32 >diag3=reg128#9,<diag3=reg128#9,<diag3=reg128#9,#1
# asm 2: vext.32 >diag3=q8,<diag3=q8,<diag3=q8,#1
vext.32 q8,q8,q8,#1

# qhasm:        diag0 ^= b3
# asm 1: veor >diag0=reg128#2,<diag0=reg128#2,<b3=reg128#11
# asm 2: veor >diag0=q1,<diag0=q1,<b3=q10
veor q1,q1,q10

# qhasm:   goto mainloop1 if unsigned>
bhi ._mainloop1

# qhasm:   2x abab = 0xffffffff
# asm 1: vmov.i64 >abab=reg128#10,#0xffffffff
# asm 2: vmov.i64 >abab=q9,#0xffffffff
vmov.i64 q9,#0xffffffff

# qhasm:   4x x0x5x10x15 = diag0 + start0
# asm 1: vadd.i32 >x0x5x10x15=reg128#2,<diag0=reg128#2,<start0=reg128#3
# asm 2: vadd.i32 >x0x5x10x15=q1,<diag0=q1,<start0=q2
vadd.i32 q1,q1,q2

# qhasm:   4x x12x1x6x11 = diag1 + start1
# asm 1: vadd.i32 >x12x1x6x11=reg128#5,<diag1=reg128#5,<start1=reg128#4
# asm 2: vadd.i32 >x12x1x6x11=q4,<diag1=q4,<start1=q3
vadd.i32 q4,q4,q3

# qhasm:   4x x8x13x2x7 = diag2 + start2
# asm 1: vadd.i32 >x8x13x2x7=reg128#6,<diag2=reg128#8,<start2=reg128#6
# asm 2: vadd.i32 >x8x13x2x7=q5,<diag2=q7,<start2=q5
vadd.i32 q5,q7,q5

# qhasm:   4x x4x9x14x3 = diag3 + start3
# asm 1: vadd.i32 >x4x9x14x3=reg128#7,<diag3=reg128#9,<start3=reg128#7
# asm 2: vadd.i32 >x4x9x14x3=q6,<diag3=q8,<start3=q6
vadd.i32 q6,q8,q6

# qhasm:   x0x1x10x11 = x0x5x10x15
# asm 1: vmov >x0x1x10x11=reg128#8,<x0x5x10x15=reg128#2
# asm 2: vmov >x0x1x10x11=q7,<x0x5x10x15=q1
vmov q7,q1

# qhasm:   x12x13x6x7 = x12x1x6x11
# asm 1: vmov >x12x13x6x7=reg128#9,<x12x1x6x11=reg128#5
# asm 2: vmov >x12x13x6x7=q8,<x12x1x6x11=q4
vmov q8,q4

# qhasm:   x8x9x2x3 = x8x13x2x7
# asm 1: vmov >x8x9x2x3=reg128#11,<x8x13x2x7=reg128#6
# asm 2: vmov >x8x9x2x3=q10,<x8x13x2x7=q5
vmov q10,q5

# qhasm:   x4x5x14x15 = x4x9x14x3
# asm 1: vmov >x4x5x14x15=reg128#12,<x4x9x14x3=reg128#7
# asm 2: vmov >x4x5x14x15=q11,<x4x9x14x3=q6
vmov q11,q6

# qhasm:   x0x1x10x11 = (abab & x0x1x10x11) | (~abab & x12x1x6x11)
# asm 1: vbif <x0x1x10x11=reg128#8,<x12x1x6x11=reg128#5,<abab=reg128#10
# asm 2: vbif <x0x1x10x11=q7,<x12x1x6x11=q4,<abab=q9
vbif q7,q4,q9

# qhasm:   x12x13x6x7 = (abab & x12x13x6x7) | (~abab & x8x13x2x7)
# asm 1: vbif <x12x13x6x7=reg128#9,<x8x13x2x7=reg128#6,<abab=reg128#10
# asm 2: vbif <x12x13x6x7=q8,<x8x13x2x7=q5,<abab=q9
vbif q8,q5,q9

# qhasm:   x8x9x2x3 = (abab & x8x9x2x3) | (~abab & x4x9x14x3)
# asm 1: vbif <x8x9x2x3=reg128#11,<x4x9x14x3=reg128#7,<abab=reg128#10
# asm 2: vbif <x8x9x2x3=q10,<x4x9x14x3=q6,<abab=q9
vbif q10,q6,q9

# qhasm:   x4x5x14x15 = (abab & x4x5x14x15) | (~abab & x0x5x10x15)
# asm 1: vbif <x4x5x14x15=reg128#12,<x0x5x10x15=reg128#2,<abab=reg128#10
# asm 2: vbif <x4x5x14x15=q11,<x0x5x10x15=q1,<abab=q9
vbif q11,q1,q9

# qhasm:   x0x1x2x3 = x0x1x10x11
# asm 1: vmov >x0x1x2x3=reg128#2,<x0x1x10x11=reg128#8
# asm 2: vmov >x0x1x2x3=q1,<x0x1x10x11=q7
vmov q1,q7

# qhasm:   x4x5x6x7 = x4x5x14x15
# asm 1: vmov >x4x5x6x7=reg128#5,<x4x5x14x15=reg128#12
# asm 2: vmov >x4x5x6x7=q4,<x4x5x14x15=q11
vmov q4,q11

# qhasm:   x8x9x10x11 = x8x9x2x3
# asm 1: vmov >x8x9x10x11=reg128#6,<x8x9x2x3=reg128#11
# asm 2: vmov >x8x9x10x11=q5,<x8x9x2x3=q10
vmov q5,q10

# qhasm:   x12x13x14x15 = x12x13x6x7
# asm 1: vmov >x12x13x14x15=reg128#7,<x12x13x6x7=reg128#9
# asm 2: vmov >x12x13x14x15=q6,<x12x13x6x7=q8
vmov q6,q8

# qhasm:   x0x1x2x3 = x0x1x2x3[0,1] x8x9x2x3[2,3]
# asm 1: vmov <x0x1x2x3=reg128#2%top,<x8x9x2x3=reg128#11%top
# asm 2: vmov <x0x1x2x3=d3,<x8x9x2x3=d21
vmov d3,d21

# qhasm:   x4x5x6x7 = x4x5x6x7[0,1] x12x13x6x7[2,3]
# asm 1: vmov <x4x5x6x7=reg128#5%top,<x12x13x6x7=reg128#9%top
# asm 2: vmov <x4x5x6x7=d9,<x12x13x6x7=d17
vmov d9,d17

# qhasm:   x8x9x10x11 = x8x9x10x11[0,1] x0x1x10x11[2,3]
# asm 1: vmov <x8x9x10x11=reg128#6%top,<x0x1x10x11=reg128#8%top
# asm 2: vmov <x8x9x10x11=d11,<x0x1x10x11=d15
vmov d11,d15

# qhasm:   x12x13x14x15 = x12x13x14x15[0,1] x4x5x14x15[2,3]
# asm 1: vmov <x12x13x14x15=reg128#7%top,<x4x5x14x15=reg128#12%top
# asm 2: vmov <x12x13x14x15=d13,<x4x5x14x15=d23
vmov d13,d23

# qhasm:                      =? m - 0
# asm 1: cmp <m=int32#2,#0
# asm 2: cmp <m=r1,#0
cmp r1,#0

# qhasm:   goto nomessage1 if =
beq ._nomessage1

# qhasm:     m0m1m2m3 = mem128[m]
# asm 1: vld1.8 {>m0m1m2m3=reg128#8%bot->m0m1m2m3=reg128#8%top},[<m=int32#2]
# asm 2: vld1.8 {>m0m1m2m3=d14->m0m1m2m3=d15},[<m=r1]
vld1.8 {d14-d15},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m4m5m6m7 = mem128[m]
# asm 1: vld1.8 {>m4m5m6m7=reg128#9%bot->m4m5m6m7=reg128#9%top},[<m=int32#2]
# asm 2: vld1.8 {>m4m5m6m7=d16->m4m5m6m7=d17},[<m=r1]
vld1.8 {d16-d17},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m8m9m10m11 = mem128[m]
# asm 1: vld1.8 {>m8m9m10m11=reg128#10%bot->m8m9m10m11=reg128#10%top},[<m=int32#2]
# asm 2: vld1.8 {>m8m9m10m11=d18->m8m9m10m11=d19},[<m=r1]
vld1.8 {d18-d19},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     m12m13m14m15 = mem128[m]
# asm 1: vld1.8 {>m12m13m14m15=reg128#11%bot->m12m13m14m15=reg128#11%top},[<m=int32#2]
# asm 2: vld1.8 {>m12m13m14m15=d20->m12m13m14m15=d21},[<m=r1]
vld1.8 {d20-d21},[r1]

# qhasm:     m += 16
# asm 1: add <m=int32#2,<m=int32#2,#16
# asm 2: add <m=r1,<m=r1,#16
add r1,r1,#16

# qhasm:     x0x1x2x3 ^= m0m1m2m3
# asm 1: veor >x0x1x2x3=reg128#2,<x0x1x2x3=reg128#2,<m0m1m2m3=reg128#8
# asm 2: veor >x0x1x2x3=q1,<x0x1x2x3=q1,<m0m1m2m3=q7
veor q1,q1,q7

# qhasm:     x4x5x6x7 ^= m4m5m6m7
# asm 1: veor >x4x5x6x7=reg128#5,<x4x5x6x7=reg128#5,<m4m5m6m7=reg128#9
# asm 2: veor >x4x5x6x7=q4,<x4x5x6x7=q4,<m4m5m6m7=q8
veor q4,q4,q8

# qhasm:     x8x9x10x11 ^= m8m9m10m11
# asm 1: veor >x8x9x10x11=reg128#6,<x8x9x10x11=reg128#6,<m8m9m10m11=reg128#10
# asm 2: veor >x8x9x10x11=q5,<x8x9x10x11=q5,<m8m9m10m11=q9
veor q5,q5,q9

# qhasm:     x12x13x14x15 ^= m12m13m14m15
# asm 1: veor >x12x13x14x15=reg128#7,<x12x13x14x15=reg128#7,<m12m13m14m15=reg128#11
# asm 2: veor >x12x13x14x15=q6,<x12x13x14x15=q6,<m12m13m14m15=q10
veor q6,q6,q10

# qhasm:   nomessage1:
._nomessage1:

# qhasm:   mem128[c] = x0x1x2x3
# asm 1: vst1.8 {<x0x1x2x3=reg128#2%bot-<x0x1x2x3=reg128#2%top},[<c=int32#1]
# asm 2: vst1.8 {<x0x1x2x3=d2-<x0x1x2x3=d3},[<c=r0]
vst1.8 {d2-d3},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x4x5x6x7
# asm 1: vst1.8 {<x4x5x6x7=reg128#5%bot-<x4x5x6x7=reg128#5%top},[<c=int32#1]
# asm 2: vst1.8 {<x4x5x6x7=d8-<x4x5x6x7=d9},[<c=r0]
vst1.8 {d8-d9},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x8x9x10x11
# asm 1: vst1.8 {<x8x9x10x11=reg128#6%bot-<x8x9x10x11=reg128#6%top},[<c=int32#1]
# asm 2: vst1.8 {<x8x9x10x11=d10-<x8x9x10x11=d11},[<c=r0]
vst1.8 {d10-d11},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:   mem128[c] = x12x13x14x15
# asm 1: vst1.8 {<x12x13x14x15=reg128#7%bot-<x12x13x14x15=reg128#7%top},[<c=int32#1]
# asm 2: vst1.8 {<x12x13x14x15=d12-<x12x13x14x15=d13},[<c=r0]
vst1.8 {d12-d13},[r0]

# qhasm:   c += 16
# asm 1: add <c=int32#1,<c=int32#1,#16
# asm 2: add <c=r0,<c=r0,#16
add r0,r0,#16

# qhasm:                           unsigned<? mlenlow - 64
# asm 1: cmp <mlenlow=int32#3,#64
# asm 2: cmp <mlenlow=r2,#64
cmp r2,#64

# qhasm:   goto xmlenatleast64 if !unsigned<
bhs ._xmlenatleast64

# qhasm:     i = 0
# asm 1: ldr >i=int32#4,=0
# asm 2: ldr >i=r3,=0
ldr r3,=0

# qhasm:     m = c - 64
# asm 1: sub >m=int32#2,<c=int32#1,#64
# asm 2: sub >m=r1,<c=r0,#64
sub r1,r0,#64

# qhasm:     c = savec
# asm 1: ldr >c=int32#1,<savec=stack32#1
# asm 2: ldr >c=r0,<savec=[sp,#64]
ldr r0,[sp,#64]

# qhasm:     ccopy:
._ccopy:

# qhasm:       ci = mem8[m + 0]
# asm 1: ldrb >ci=int32#5,[<m=int32#2,#0]
# asm 2: ldrb >ci=r4,[<m=r1,#0]
ldrb r4,[r1,#0]

# qhasm:       mem8[c + 0] = ci
# asm 1: strb <ci=int32#5,[<c=int32#1,#0]
# asm 2: strb <ci=r4,[<c=r0,#0]
strb r4,[r0,#0]

# qhasm:       m += 1
# asm 1: add <m=int32#2,<m=int32#2,#1
# asm 2: add <m=r1,<m=r1,#1
add r1,r1,#1

# qhasm:       c += 1
# asm 1: add <c=int32#1,<c=int32#1,#1
# asm 2: add <c=r0,<c=r0,#1
add r0,r0,#1

# qhasm:       i += 1
# asm 1: add <i=int32#4,<i=int32#4,#1
# asm 2: add <i=r3,<i=r3,#1
add r3,r3,#1

# qhasm:                   unsigned<? i - mlenlow
# asm 1: cmp <i=int32#4,<mlenlow=int32#3
# asm 2: cmp <i=r3,<mlenlow=r2
cmp r3,r2

# qhasm:     goto ccopy if unsigned<
blo ._ccopy

# qhasm:   xmlenatleast64:
._xmlenatleast64:

# qhasm:                      unsigned>? mlenlow -= 64
# asm 1: subs <mlenlow=int32#3,<mlenlow=int32#3,#64
# asm 2: subs <mlenlow=r2,<mlenlow=r2,#64
subs r2,r2,#64

# qhasm: goto mlenatleast1 if unsigned>
bhi ._mlenatleast1

# qhasm: done:
._done:

# qhasm: new caller_r4

# qhasm: caller_r4 = stack_r4
# asm 1: ldr >caller_r4=int32#5,<stack_r4=stack32#2
# asm 2: ldr >caller_r4=r4,<stack_r4=[sp,#68]
ldr r4,[sp,#68]

# qhasm: new caller_q4

# qhasm: new caller_q5

# qhasm: new caller_q6

# qhasm: new caller_q7

# qhasm: caller_q4 bot = stack_q4 bot
# asm 1: vldr <caller_q4=reg128#5%bot,<stack_q4=stack128#1
# asm 2: vldr <caller_q4=d8,<stack_q4=[sp,#96]
vldr d8,[sp,#96]

# qhasm: caller_q4 top = stack_q4 top
# asm 1: vldr <caller_q4=reg128#5%top,<stack_q4=stack128#1
# asm 2: vldr <caller_q4=d9,<stack_q4=[sp,#104]
vldr d9,[sp,#104]

# qhasm: caller_q5 bot = stack_q5 bot
# asm 1: vldr <caller_q5=reg128#6%bot,<stack_q5=stack128#2
# asm 2: vldr <caller_q5=d10,<stack_q5=[sp,#112]
vldr d10,[sp,#112]

# qhasm: caller_q5 top = stack_q5 top
# asm 1: vldr <caller_q5=reg128#6%top,<stack_q5=stack128#2
# asm 2: vldr <caller_q5=d11,<stack_q5=[sp,#120]
vldr d11,[sp,#120]

# qhasm: caller_q6 bot = stack_q6 bot
# asm 1: vldr <caller_q6=reg128#7%bot,<stack_q6=stack128#3
# asm 2: vldr <caller_q6=d12,<stack_q6=[sp,#128]
vldr d12,[sp,#128]

# qhasm: caller_q6 top = stack_q6 top
# asm 1: vldr <caller_q6=reg128#7%top,<stack_q6=stack128#3
# asm 2: vldr <caller_q6=d13,<stack_q6=[sp,#136]
vldr d13,[sp,#136]

# qhasm: caller_q7 bot = stack_q7 bot
# asm 1: vldr <caller_q7=reg128#8%bot,<stack_q7=stack128#4
# asm 2: vldr <caller_q7=d14,<stack_q7=[sp,#144]
vldr d14,[sp,#144]

# qhasm: caller_q7 top = stack_q7 top
# asm 1: vldr <caller_q7=reg128#8%top,<stack_q7=stack128#4
# asm 2: vldr <caller_q7=d15,<stack_q7=[sp,#152]
vldr d15,[sp,#152]

# qhasm: int32 result

# qhasm: result = 0
# asm 1: ldr >result=int32#1,=0
# asm 2: ldr >result=r0,=0
ldr r0,=0

# qhasm: return result
add sp,sp,#256
bx lr
