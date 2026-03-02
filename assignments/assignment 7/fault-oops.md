# Trace captured during kernel crash
echo "hello world" > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041bfb000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 154 Comm: sh Tainted: G           O       6.1.44 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc8/0x390
sp : ffffffc008e0bd20
x29: ffffffc008e0bd80 x28: ffffff8001b2ea00 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 000000000000000c x22: 000000000000000c x21: ffffffc008e0bdc0
x20: 000000556aeaaad0 x19: ffffff8001b5ab00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc000787000 x3 : ffffffc008e0bdc0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 0000000000000000 ]---

# Data captured and analysis
1. Debug print shows that the error is due to a null pointer dereference "Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000"
2. The pc which is the program counter shows exactly which assembly instruction generated the error which is "pc : faulty_write+0x10/0x20 [faulty]"
3. Then we have the general purpose register which are used by the program , this comes in handy when we disassemble the particular .ko file to find the assembly instruction 

# Disassembly 
1. We use the objdump command to disassemble file . We first need to make sure that this is the correct objdump for our particular architecture . This objdump program file for our architecture will be in buildroot/output/host/bin .
2. Disassembly of file gives us the following 

aditya@Ubuntu:~/assignment-4-adityaganeshcu2k/buildroot/output/build/ldd-d8aab69bc8641a6510266742439c1f42308c3b94/misc-modules$ ../../../host/bin/aarch64-buildroot-linux-gnu-objdump -S faulty.ko

faulty.ko:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000000000 <faulty_write>:
   0:	d2800001 	mov	x1, #0x0                   	// #0
   4:	d2800000 	mov	x0, #0x0                   	// #0
   8:	d503233f 	paciasp
   c:	d50323bf 	autiasp
  10:	b900003f 	str	wzr, [x1]
  14:	d65f03c0 	ret
  18:	d503201f 	nop
  1c:	d503201f 	nop

3. Here the thing to notice is the pc value given durnig kernel crash was :-faulty_write+0x10/0x20 [faulty] 
4. This tells us the crash happened in 0x10 which according to disassembly corresponds to str wzr,[x1] . Since our architecture is arm-64 bit I referred to this pdf chapter 8 "https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/Armv8-A%20Instruction%20Set%20Architecture.pdf?revision=ebf53406-04fd-4c67-a485-1b329febfb3e&utm_source=chatgpt.com"

 5. According to the arm v8 instruction architecture we see that the instruction means to *(uint32_t *)x1 = 0; which means we are dereferencing x1 . 
 
 6. Now comapring this to the faulty.c we see that this error occured inside faulty_write as mentioned in the disassembly and after going to this function we can see we are executing this line *(int *)0 = 0; which causes the crash.

