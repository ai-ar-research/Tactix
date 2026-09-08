
/home/hwu/SAT-solving/build/pin-5419-rephase-root-release/tactix:     file format elf64-x86-64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

0000000000413470 <_ZN6tactix16kernel_propagateERNS_6KernelEj>:
  413470:	push   %r15
  413472:	push   %r14
  413474:	push   %r13
  413476:	mov    %rdi,%r13
  413479:	push   %r12
  41347b:	push   %rbp
  41347c:	push   %rbx
  41347d:	sub    $0x78,%rsp
  413481:	mov    0xf4(%rdi),%eax
  413487:	mov    0xf0(%rdi),%ebx
  41348d:	mov    %esi,0x58(%rsp)
  413491:	cmp    %ebx,%eax
  413493:	jae    41349d <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x2d>
  413495:	mov    %eax,0xf0(%rdi)
  41349b:	mov    %eax,%ebx
  41349d:	lea    0x160(%r13),%rdi
  4134a4:	mov    %ebx,0x5c(%rsp)
  4134a8:	mov    %ebx,%eax
  4134aa:	mov    %r13,%rbp
  4134ad:	movl   $0xffffffff,0xf4(%r13)
  4134b8:	movq   $0x0,0x68(%rsp)
  4134c1:	mov    %rdi,0x30(%rsp)
  4134c6:	jmp    4134f3 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x83>
  4134c8:	nopl   0x0(%rax,%rax,1)
  4134d0:	mov    0x58(%rsp),%edx
  4134d4:	lea    0x68(%rsp),%rcx
  4134d9:	mov    %rbp,%rdi
  4134dc:	callq  412670 <_ZN6tactixL17propagate_literalILb0ELb0ELb1EEEbRNS_6KernelEjjRm>
  4134e1:	mov    %eax,%r14d
  4134e4:	mov    0xf0(%rbp),%eax
  4134ea:	test   %r14b,%r14b
  4134ed:	jne    413dd2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x962>
  4134f3:	mov    %eax,%edx
  4134f5:	cmp    0xe8(%rbp),%rdx
  4134fc:	jae    4141ff <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd8f>
  413502:	add    $0x1,%eax
  413505:	cmpl   $0xffffffff,0x58(%rsp)
  41350a:	mov    %eax,0xf0(%rbp)
  413510:	mov    0xd0(%rbp),%rax
  413517:	mov    (%rax,%rdx,4),%esi
  41351a:	jne    4134d0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x60>
  41351c:	mov    %esi,%eax
  41351e:	mov    0x178(%rbp),%r11
  413525:	mov    0x130(%rbp),%rbx
  41352c:	shr    %esi
  41352e:	xor    $0x1,%eax
  413531:	mov    0x160(%rbp),%r8
  413538:	shl    $0x4,%rsi
  41353c:	add    0xa0(%rbp),%rsi
  413543:	mov    %eax,0x8(%rsp)
  413547:	shl    $0x4,%rax
  41354b:	mov    0x88(%rbp),%r14
  413552:	mov    %rax,0x48(%rsp)
  413557:	add    %r11,%rax
  41355a:	mov    (%rax),%r9d
  41355d:	mov    0x4(%rax),%edx
  413560:	mov    %rbx,0x10(%rsp)
  413565:	mov    %rsi,0x18(%rsp)
  41356a:	mov    %rdx,%rdi
  41356d:	add    %r9,%rdx
  413570:	lea    0x0(,%r9,4),%rcx
  413578:	shl    $0x2,%rdx
  41357c:	lea    (%r8,%rcx,1),%r12
  413580:	lea    (%r8,%rdx,1),%rbx
  413584:	sub    %rcx,%rdx
  413587:	sar    $0x2,%rdx
  41358b:	mov    %rbx,(%rsp)
  41358f:	lea    0x1f(%rdx),%rcx
  413593:	shr    $0x5,%rcx
  413597:	lea    0x1(%rcx),%r15
  41359b:	mov    %r15,0x38(%rsp)
  4135a0:	cmp    %rbx,%r12
  4135a3:	je     4142fd <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe8d>
  4135a9:	mov    %r12,0x50(%rsp)
  4135ae:	mov    %r12,%rbx
  4135b1:	mov    %r12,%rdx
  4135b4:	jmpq   413708 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x298>
  4135b9:	nopl   0x0(%rax)
  4135c0:	add    $0x4,%rdx
  4135c4:	add    $0x4,%rbx
  4135c8:	test   %dil,%dil
  4135cb:	jg     4136fe <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x28e>
  4135d1:	mov    0x178(%rbp),%r10
  4135d8:	jne    4140e0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc70>
  4135de:	mov    %ecx,%edi
  4135e0:	mov    0x18(%rsp),%r11
  4135e5:	shr    $0x2,%eax
  4135e8:	xor    $0x1,%edi
  4135eb:	mov    %rdi,%r9
  4135ee:	mov    (%r11),%r8d
  4135f1:	shl    $0x4,%r9
  4135f5:	add    %r9,%r10
  4135f8:	cmpb   $0x0,0x1e6d(%rbp)
  4135ff:	je     413613 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x1a3>
  413601:	cmpb   $0x0,0x1e6e(%rbp)
  413608:	je     413613 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x1a3>
  41360a:	test   %r8d,%r8d
  41360d:	jne    413b88 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x718>
  413613:	mov    0x4(%r10),%r9d
  413617:	test   %r9d,%r9d
  41361a:	jne    413a60 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x5f0>
  413620:	mov    0x88(%rbp),%r9
  413627:	mov    0x8(%rsp),%r11d
  41362c:	movd   %r8d,%xmm0
  413631:	movb   $0x1,(%r9,%rsi,1)
  413636:	mov    0x88(%rbp),%rsi
  41363d:	movb   $0xff,(%rsi,%rdi,1)
  413641:	mov    %rax,%rsi
  413644:	mov    0xe8(%rbp),%rdi
  41364b:	shl    $0x4,%rsi
  41364f:	add    0xa0(%rbp),%rsi
  413656:	mov    %r11d,0x8(%rsi)
  41365a:	movd   %edi,%xmm1
  41365e:	movzwl 0xfbd7a(%rip),%r11d        # 50f3e0 <_ZN6tactix11BinaryIdMap5EMPTYE+0x18>
  413666:	punpckldq %xmm1,%xmm0
  41366a:	mov    %r11w,0xc(%rsi)
  41366f:	movq   %xmm0,(%rsi)
  413673:	lea    0x1(%rdi),%rsi
  413677:	mov    %rsi,0xe8(%rbp)
  41367e:	mov    0xd0(%rbp),%rsi
  413685:	mov    %ecx,(%rsi,%rdi,4)
  413688:	test   %r8d,%r8d
  41368b:	jne    41369c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x22c>
  41368d:	addq   $0x1,0x698(%rbp)
  413695:	addl   $0x1,0x1c54(%rbp)
  41369c:	mov    0x1de0(%rbp),%rsi
  4136a3:	test   %rsi,%rsi
  4136a6:	je     4136d0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x260>
  4136a8:	cmpb   $0x0,0x1de8(%rbp)
  4136af:	jne    4136d0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x260>
  4136b1:	cmpb   $0x0,0x220(%rbp)
  4136b8:	jne    4136d0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x260>
  4136ba:	and    $0x1,%ecx
  4136bd:	mov    (%rsi),%rsi
  4136c0:	cmp    $0x1,%cl
  4136c3:	sbb    %ecx,%ecx
  4136c5:	and    $0x2,%ecx
  4136c8:	sub    $0x1,%ecx
  4136cb:	mov    %cl,(%rsi,%rax,1)
  4136ce:	xchg   %ax,%ax
  4136d0:	mov    0x1d48(%rbp),%rsi
  4136d7:	mov    0x1d50(%rbp),%rcx
  4136de:	subl   $0x1,0x11c(%rbp)
  4136e5:	sub    %rsi,%rcx
  4136e8:	cmp    %rcx,%rax
  4136eb:	jae    4136f3 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x283>
  4136ed:	cmpb   $0x0,(%rsi,%rax,1)
  4136f1:	jne    4136fa <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x28a>
  4136f3:	subl   $0x1,0x120(%rbp)
  4136fa:	add    $0x1,%r15
  4136fe:	cmp    %rdx,(%rsp)
  413702:	je     4137a0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x330>
  413708:	mov    (%rdx),%eax
  41370a:	mov    %eax,%ecx
  41370c:	mov    %eax,(%rbx)
  41370e:	shr    %ecx
  413710:	mov    %ecx,%esi
  413712:	movzbl (%r14,%rsi,1),%edi
  413717:	test   $0x1,%al
  413719:	jne    4135c0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x150>
  41371f:	mov    0x4(%rdx),%r13d
  413723:	lea    0x8(%rdx),%r10
  413727:	lea    0x8(%rbx),%r11
  41372b:	mov    %r13d,0x4(%rbx)
  41372f:	test   %dil,%dil
  413732:	jg     413a40 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x5d0>
  413738:	mov    0x10(%rsp),%rax
  41373d:	mov    %r13d,%ecx
  413740:	lea    0x1(%r15),%r8
  413744:	shl    $0x2,%rcx
  413748:	lea    (%rax,%rcx,1),%rdx
  41374c:	testb  $0x1,0x4(%rdx)
  413750:	jne    413a50 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x5e0>
  413756:	mov    0x8(%rsp),%r9d
  41375b:	mov    0x14(%rdx),%eax
  41375e:	xor    0x18(%rdx),%eax
  413761:	xor    %eax,%r9d
  413764:	mov    %r9d,%eax
  413767:	mov    %rax,0x28(%rsp)
  41376c:	movzbl (%r14,%rax,1),%eax
  413771:	mov    %al,0x20(%rsp)
  413775:	test   %al,%al
  413777:	jle    413a78 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x608>
  41377d:	lea    (%r9,%r9,1),%eax
  413781:	mov    %r8,%r15
  413784:	mov    %r10,%rdx
  413787:	mov    %eax,(%rbx)
  413789:	mov    %r11,%rbx
  41378c:	cmp    %rdx,(%rsp)
  413790:	jne    413708 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x298>
  413796:	nopw   %cs:0x0(%rax,%rax,1)
  4137a0:	mov    0x178(%rbp),%r11
  4137a7:	mov    0x48(%rsp),%rax
  4137ac:	mov    %r15,0x38(%rsp)
  4137b1:	xor    %r14d,%r14d
  4137b4:	mov    0x50(%rsp),%r12
  4137b9:	add    %r11,%rax
  4137bc:	sub    %r12,%rbx
  4137bf:	mov    0x4(%rax),%edi
  4137c2:	mov    %rbx,%rdx
  4137c5:	sar    $0x2,%rdx
  4137c9:	nopl   0x0(%rax)
  4137d0:	mov    0xc(%rax),%ecx
  4137d3:	sub    %edx,%edi
  4137d5:	add    0x190(%rbp),%rdi
  4137dc:	cmpq   $0x0,0x218(%rbp)
  4137e4:	mov    %rdi,0x190(%rbp)
  4137eb:	mov    %edx,0x4(%rax)
  4137ee:	mov    %ecx,0xc(%rax)
  4137f1:	je     413dae <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x93e>
  4137f7:	mov    0x160(%rbp),%r13
  4137fe:	xor    %r9d,%r9d
  413801:	mov    %r14b,0x48(%rsp)
  413806:	mov    %r9,%r14
  413809:	mov    %r13,%rax
  41380c:	jmp    413874 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x404>
  41380e:	xchg   %ax,%ax
  413810:	sub    $0x1,%rdi
  413814:	mov    %r12,%rbx
  413817:	lea    0x1(%rdx),%esi
  41381a:	add    (%rbx),%edx
  41381c:	mov    %rdi,0x190(%rbp)
  413823:	mov    %esi,0x4(%rbx)
  413826:	mov    %r9d,(%rax,%rdx,4)
  41382a:	mov    0x4(%rbx),%esi
  41382d:	cmp    0x8(%rbx),%esi
  413830:	je     413c40 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x7d0>
  413836:	sub    $0x1,%rdi
  41383a:	lea    0x1(%rsi),%edx
  41383d:	mov    %rdi,0x190(%rbp)
  413844:	mov    %r13d,%ecx
  413847:	add    (%rbx),%esi
  413849:	mov    %edx,0x4(%rbx)
  41384c:	mov    (%rsp),%edx
  41384f:	xor    $0x1,%ecx
  413852:	add    $0x3,%r14
  413856:	movzbl %cl,%ecx
  413859:	mov    %r15d,(%rax,%rsi,4)
  41385d:	and    $0xfffffffe,%edx
  413860:	or     %ecx,%edx
  413862:	mov    %edx,0xc(%r12)
  413867:	cmp    0x218(%rbp),%r14
  41386e:	jae    413da8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x938>
  413874:	mov    0x200(%rbp),%r9
  41387b:	lea    0x0(,%r14,4),%r10
  413883:	mov    (%r9,%r14,4),%ecx
  413887:	mov    0x8(%r9,%r10,1),%r15d
  41388c:	mov    0x4(%r9,%r10,1),%r9d
  413891:	shl    $0x4,%rcx
  413895:	lea    (%r11,%rcx,1),%r12
  413899:	add    %r9d,%r9d
  41389c:	mov    0xc(%r12),%esi
  4138a1:	mov    0x4(%r12),%edx
  4138a6:	mov    %esi,%r13d
  4138a9:	mov    %edx,%ebx
  4138ab:	not    %esi
  4138ad:	and    $0xfffffffe,%r13d
  4138b1:	and    $0x1,%ebx
  4138b4:	or     %ebx,%r13d
  4138b7:	mov    %r13d,(%rsp)
  4138bb:	mov    %esi,%r13d
  4138be:	and    $0x1,%r13d
  4138c2:	cmp    0x8(%r12),%edx
  4138c7:	jne    413810 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x3a0>
  4138cd:	mov    0x170(%rbp),%rdi
  4138d4:	mov    0x30(%rsp),%r8
  4138d9:	cmp    %rax,%rdi
  4138dc:	je     413f40 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xad0>
  4138e2:	mov    0x168(%rbp),%r10
  4138e9:	sub    %rax,%rdi
  4138ec:	mov    %edx,%r11d
  4138ef:	sub    %rax,%r10
  4138f2:	mov    (%r12),%eax
  4138f6:	mov    %r10,%rsi
  4138f9:	sar    $0x2,%rsi
  4138fd:	add    %r11,%rax
  413900:	mov    %rsi,%rbx
  413903:	cmp    %rsi,%rax
  413906:	je     413e60 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x9f0>
  41390c:	test   %edx,%edx
  41390e:	je     413de8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x978>
  413914:	lea    (%rdx,%rdx,1),%eax
  413917:	mov    %eax,0x10(%rsp)
  41391b:	mov    %rax,0x8(%rsp)
  413920:	mov    0x8(%rsp),%rax
  413925:	sar    $0x2,%rdi
  413929:	add    %rax,%rsi
  41392c:	cmp    %rsi,%rdi
  41392f:	jae    413990 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x520>
  413931:	cmp    $0x3,%rsi
  413935:	jbe    4142df <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe6f>
  41393b:	mov    $0x4,%eax
  413940:	add    %rax,%rax
  413943:	lea    -0x1(%rax),%rdx
  413947:	cmp    %rsi,%rdx
  41394a:	jb     413940 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x4d0>
  41394c:	mov    %rdx,%rsi
  41394f:	mov    %r8,%rdi
  413952:	mov    %r9d,0x28(%rsp)
  413957:	mov    %rcx,0x20(%rsp)
  41395c:	mov    %r8,0x18(%rsp)
  413961:	callq  424a40 <_ZNSt6vectorIjSaIjEE7reserveEm>
  413966:	mov    0x8(%rsp),%rax
  41396b:	mov    0x28(%rsp),%r9d
  413970:	mov    0x168(%rbp),%rbx
  413977:	sub    0x160(%rbp),%rbx
  41397e:	sar    $0x2,%rbx
  413982:	mov    0x20(%rsp),%rcx
  413987:	mov    0x18(%rsp),%r8
  41398c:	lea    (%rbx,%rax,1),%rsi
  413990:	mov    %r8,%rdi
  413993:	mov    %r9d,0x28(%rsp)
  413998:	mov    %rcx,0x20(%rsp)
  41399d:	mov    %ebx,0x18(%rsp)
  4139a1:	mov    %ebx,%ebx
  4139a3:	callq  424b00 <_ZNSt6vectorIjSaIjEE6resizeEm>
  4139a8:	mov    0x20(%rsp),%rcx
  4139ad:	mov    0x28(%rsp),%r9d
  4139b2:	mov    0x178(%rbp),%r11
  4139b9:	mov    0x160(%rbp),%rax
  4139c0:	lea    (%r11,%rcx,1),%r10
  4139c4:	lea    (%rax,%rbx,4),%rdi
  4139c8:	mov    0x4(%r10),%ebx
  4139cc:	mov    (%r10),%esi
  4139cf:	mov    %rbx,%rdx
  4139d2:	shl    $0x2,%rbx
  4139d6:	lea    (%rax,%rsi,4),%rsi
  4139da:	cmp    $0x4,%rbx
  4139de:	jbe    414296 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe26>
  4139e4:	mov    %rbx,%rdx
  4139e7:	mov    %r10,0x40(%rsp)
  4139ec:	callq  4023b0 <memmove@plt>
  4139f1:	mov    0x20(%rsp),%rcx
  4139f6:	mov    0x28(%rsp),%r9d
  4139fb:	mov    0x178(%rbp),%r11
  413a02:	mov    0x160(%rbp),%rax
  413a09:	mov    0x40(%rsp),%r10
  413a0e:	lea    (%r11,%rcx,1),%rbx
  413a12:	mov    0x4(%rbx),%edx
  413a15:	mov    0x18(%rsp),%edi
  413a19:	mov    0x190(%rbp),%rsi
  413a20:	mov    %edi,(%r10)
  413a23:	mov    0x10(%rsp),%edi
  413a27:	mov    %edi,0x8(%r10)
  413a2b:	mov    0x8(%rsp),%rdi
  413a30:	lea    -0x1(%rdi,%rsi,1),%rdi
  413a35:	jmpq   413817 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x3a7>
  413a3a:	nopw   0x0(%rax,%rax,1)
  413a40:	mov    %r10,%rdx
  413a43:	mov    %r11,%rbx
  413a46:	jmpq   4136fe <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x28e>
  413a4b:	nopl   0x0(%rax,%rax,1)
  413a50:	mov    %r8,%r15
  413a53:	mov    %r10,%rdx
  413a56:	jmpq   4136fe <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x28e>
  413a5b:	nopl   0x0(%rax,%rax,1)
  413a60:	mov    (%r10),%r10d
  413a63:	mov    0x160(%rbp),%r9
  413a6a:	prefetcht2 (%r9,%r10,4)
  413a6f:	jmpq   413620 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x1b0>
  413a74:	nopl   0x0(%rax)
  413a78:	mov    (%rdx),%esi
  413a7a:	mov    0x8(%rdx),%eax
  413a7d:	lea    0x14(%rdx),%r12
  413a81:	mov    %r12,0x38(%rsp)
  413a86:	shl    $0x2,%rsi
  413a8a:	shl    $0x2,%rax
  413a8e:	lea    (%r12,%rsi,1),%rdi
  413a92:	add    %rax,%r12
  413a95:	mov    %r12,0x40(%rsp)
  413a9a:	cmp    %rax,%rsi
  413a9d:	je     413f8a <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb1a>
  413aa3:	mov    %r12,%rax
  413aa6:	jmp    413abd <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x64d>
  413aa8:	nopl   0x0(%rax,%rax,1)
  413ab0:	add    $0x4,%rax
  413ab4:	cmp    %rax,%rdi
  413ab7:	je     413e20 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x9b0>
  413abd:	mov    (%rax),%esi
  413abf:	cmpb   $0x0,(%r14,%rsi,1)
  413ac4:	mov    %rsi,%r12
  413ac7:	js     413ab0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x640>
  413ac9:	mov    0x38(%rsp),%rdi
  413ace:	mov    %rax,%rcx
  413ad1:	mov    0x208(%rbp),%rsi
  413ad8:	mov    %r9d,0x14(%rdx)
  413adc:	mov    %r12d,0x18(%rdx)
  413ae0:	sub    %rdi,%rcx
  413ae3:	mov    0x8(%rsp),%edi
  413ae7:	sar    $0x2,%rcx
  413aeb:	mov    %ecx,0x8(%rdx)
  413aee:	mov    0x200(%rbp),%rdx
  413af5:	mov    %edi,(%rax)
  413af7:	mov    0x218(%rbp),%rcx
  413afe:	mov    %rsi,%rax
  413b01:	sub    %rdx,%rax
  413b04:	lea    0x3(%rcx),%rdi
  413b08:	sar    $0x2,%rax
  413b0c:	cmp    %rdi,%rax
  413b0f:	jae    413b5c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x6ec>
  413b11:	add    %rax,%rax
  413b14:	cmp    %rdx,%rsi
  413b17:	mov    $0x3,%edx
  413b1c:	mov    %r10,0x28(%rsp)
  413b21:	cmove  %rdx,%rax
  413b25:	mov    %r9d,0x20(%rsp)
  413b2a:	cmp    %rax,%rdi
  413b2d:	cmovae %rdi,%rax
  413b31:	lea    0x200(%rbp),%rdi
  413b38:	mov    %rax,%rsi
  413b3b:	callq  424b00 <_ZNSt6vectorIjSaIjEE6resizeEm>
  413b40:	mov    0x28(%rsp),%r10
  413b45:	mov    0x20(%rsp),%r9d
  413b4a:	mov    0x218(%rbp),%rcx
  413b51:	mov    0x200(%rbp),%rdx
  413b58:	lea    0x3(%rcx),%rdi
  413b5c:	lea    0x0(,%rcx,4),%rax
  413b64:	mov    %r12d,(%rdx,%rcx,4)
  413b68:	add    $0x2,%r15
  413b6c:	mov    %r9d,0x4(%rdx,%rax,1)
  413b71:	mov    %rdi,0x218(%rbp)
  413b78:	mov    %r13d,0x8(%rdx,%rax,1)
  413b7d:	mov    %r10,%rdx
  413b80:	jmpq   4136fe <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x28e>
  413b85:	nopl   (%rax)
  413b88:	cmpb   $0x0,0xc(%r11)
  413b8d:	je     413613 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x1a3>
  413b93:	mov    0xb8(%rbp),%r9
  413b9a:	mov    0x8(%rsp),%r11d
  413b9f:	mov    %r11d,(%r9,%rax,4)
  413ba3:	mov    0x18(%rsp),%r11
  413ba8:	addq   $0x1,0x18e8(%rbp)
  413bb0:	mov    0x8(%r11),%r9d
  413bb4:	mov    0x4(%r10),%r11d
  413bb8:	test   %r11d,%r11d
  413bbb:	jne    4142e9 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe79>
  413bc1:	mov    0x88(%rbp),%r10
  413bc8:	movd   %r8d,%xmm0
  413bcd:	movb   $0x1,(%r10,%rsi,1)
  413bd2:	mov    0x88(%rbp),%rsi
  413bd9:	mov    $0x101,%r10d
  413bdf:	movb   $0xff,(%rsi,%rdi,1)
  413be3:	mov    0xe8(%rbp),%rdi
  413bea:	mov    %rax,%rsi
  413bed:	shl    $0x4,%rsi
  413bf1:	add    0xa0(%rbp),%rsi
  413bf8:	movd   %edi,%xmm5
  413bfc:	mov    %r9d,0x8(%rsi)
  413c00:	punpckldq %xmm5,%xmm0
  413c04:	mov    %r10w,0xc(%rsi)
  413c09:	movq   %xmm0,(%rsi)
  413c0d:	lea    0x1(%rdi),%rsi
  413c11:	mov    %rsi,0xe8(%rbp)
  413c18:	mov    0xd0(%rbp),%rsi
  413c1f:	mov    %ecx,(%rsi,%rdi,4)
  413c22:	mov    0x1de0(%rbp),%rsi
  413c29:	test   %rsi,%rsi
  413c2c:	jne    4136a8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x238>
  413c32:	jmpq   4136d0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x260>
  413c37:	nopw   0x0(%rax,%rax,1)
  413c40:	mov    0x170(%rbp),%r9
  413c47:	mov    0x30(%rsp),%r8
  413c4c:	cmp    %rax,%r9
  413c4f:	je     413ef8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xa88>
  413c55:	mov    0x168(%rbp),%r10
  413c5c:	sub    %rax,%r9
  413c5f:	mov    %esi,%r11d
  413c62:	sub    %rax,%r10
  413c65:	mov    (%rbx),%eax
  413c67:	mov    %r10,%rdi
  413c6a:	sar    $0x2,%rdi
  413c6e:	add    %r11,%rax
  413c71:	mov    %rdi,%rdx
  413c74:	cmp    %rdi,%rax
  413c77:	je     413eb0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xa40>
  413c7d:	test   %esi,%esi
  413c7f:	je     413e00 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x990>
  413c85:	lea    (%rsi,%rsi,1),%eax
  413c88:	mov    %eax,0x10(%rsp)
  413c8c:	mov    %rax,0x8(%rsp)
  413c91:	mov    0x8(%rsp),%rax
  413c96:	sar    $0x2,%r9
  413c9a:	lea    (%rdi,%rax,1),%rsi
  413c9e:	cmp    %rsi,%r9
  413ca1:	jae    413cfe <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x88e>
  413ca3:	cmp    $0x3,%rsi
  413ca7:	jbe    4142d5 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe65>
  413cad:	mov    $0x4,%eax
  413cb2:	nopw   0x0(%rax,%rax,1)
  413cb8:	add    %rax,%rax
  413cbb:	lea    -0x1(%rax),%rdx
  413cbf:	cmp    %rsi,%rdx
  413cc2:	jb     413cb8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x848>
  413cc4:	mov    %rdx,%rsi
  413cc7:	mov    %r8,%rdi
  413cca:	mov    %rcx,0x20(%rsp)
  413ccf:	mov    %r8,0x18(%rsp)
  413cd4:	callq  424a40 <_ZNSt6vectorIjSaIjEE7reserveEm>
  413cd9:	mov    0x8(%rsp),%rax
  413cde:	mov    0x20(%rsp),%rcx
  413ce3:	mov    0x168(%rbp),%rdx
  413cea:	sub    0x160(%rbp),%rdx
  413cf1:	sar    $0x2,%rdx
  413cf5:	mov    0x18(%rsp),%r8
  413cfa:	lea    (%rdx,%rax,1),%rsi
  413cfe:	mov    %r8,%rdi
  413d01:	mov    %rcx,0x28(%rsp)
  413d06:	mov    %edx,0x18(%rsp)
  413d0a:	mov    %rdx,0x20(%rsp)
  413d0f:	callq  424b00 <_ZNSt6vectorIjSaIjEE6resizeEm>
  413d14:	mov    0x28(%rsp),%rcx
  413d19:	mov    0x178(%rbp),%r11
  413d20:	mov    0x20(%rsp),%rdx
  413d25:	mov    0x160(%rbp),%rax
  413d2c:	lea    (%r11,%rcx,1),%r9
  413d30:	mov    %edx,%edx
  413d32:	mov    0x4(%r9),%esi
  413d36:	lea    (%rax,%rdx,4),%rdi
  413d3a:	mov    (%r9),%edx
  413d3d:	lea    (%rax,%rdx,4),%r10
  413d41:	mov    %esi,%edx
  413d43:	shl    $0x2,%rdx
  413d47:	cmp    $0x4,%rdx
  413d4b:	jbe    4142ac <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe3c>
  413d51:	mov    %r10,%rsi
  413d54:	mov    %r9,0x20(%rsp)
  413d59:	callq  4023b0 <memmove@plt>
  413d5e:	mov    0x28(%rsp),%rcx
  413d63:	mov    0x20(%rsp),%r9
  413d68:	mov    0x178(%rbp),%r11
  413d6f:	mov    0x160(%rbp),%rax
  413d76:	lea    (%r11,%rcx,1),%rbx
  413d7a:	mov    0x4(%rbx),%esi
  413d7d:	mov    0x18(%rsp),%edi
  413d81:	mov    0x190(%rbp),%rdx
  413d88:	mov    %edi,(%r9)
  413d8b:	mov    0x10(%rsp),%edi
  413d8f:	mov    %edi,0x8(%r9)
  413d93:	mov    0x8(%rsp),%rdi
  413d98:	lea    -0x1(%rdi,%rdx,1),%rdi
  413d9d:	jmpq   41383a <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x3ca>
  413da2:	nopw   0x0(%rax,%rax,1)
  413da8:	movzbl 0x48(%rsp),%r14d
  413dae:	movq   $0x0,0x218(%rbp)
  413db9:	mov    0x38(%rsp),%rax
  413dbe:	add    %rax,0x68(%rsp)
  413dc3:	mov    0xf0(%rbp),%eax
  413dc9:	test   %r14b,%r14b
  413dcc:	je     4134f3 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x83>
  413dd2:	mov    0x5c(%rsp),%ebx
  413dd6:	mov    %r14d,%r12d
  413dd9:	mov    %rbp,%r13
  413ddc:	jmpq   414209 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd99>
  413de1:	nopl   0x0(%rax)
  413de8:	movq   $0x1,0x8(%rsp)
  413df1:	movl   $0x1,0x10(%rsp)
  413df9:	jmpq   413920 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x4b0>
  413dfe:	xchg   %ax,%ax
  413e00:	movq   $0x1,0x8(%rsp)
  413e09:	movl   $0x1,0x10(%rsp)
  413e11:	jmpq   413c91 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x821>
  413e16:	nopw   %cs:0x0(%rax,%rax,1)
  413e20:	mov    0x40(%rsp),%rdi
  413e25:	lea    0x1c(%rdx),%rax
  413e29:	cmp    %rax,%rdi
  413e2c:	jne    413e45 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x9d5>
  413e2e:	jmpq   413f9c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb2c>
  413e33:	nopl   0x0(%rax,%rax,1)
  413e38:	add    $0x4,%rax
  413e3c:	cmp    %rax,%rdi
  413e3f:	je     413f9c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb2c>
  413e45:	mov    (%rax),%esi
  413e47:	cmpb   $0x0,(%r14,%rsi,1)
  413e4c:	mov    %rsi,%r12
  413e4f:	js     413e38 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x9c8>
  413e51:	jmpq   413ac9 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x659>
  413e56:	nopw   %cs:0x0(%rax,%rax,1)
  413e60:	cmp    %rdi,%r10
  413e63:	jae    41390c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x49c>
  413e69:	lea    0x1(%rax),%rsi
  413e6d:	mov    %r8,%rdi
  413e70:	mov    %r9d,0x10(%rsp)
  413e75:	mov    %rcx,0x8(%rsp)
  413e7a:	callq  424b00 <_ZNSt6vectorIjSaIjEE6resizeEm>
  413e7f:	mov    0x8(%rsp),%rcx
  413e84:	mov    0x178(%rbp),%r11
  413e8b:	addl   $0x1,0x8(%r12)
  413e91:	mov    0x190(%rbp),%rdi
  413e98:	lea    (%r11,%rcx,1),%rbx
  413e9c:	mov    0x160(%rbp),%rax
  413ea3:	mov    0x10(%rsp),%r9d
  413ea8:	mov    0x4(%rbx),%edx
  413eab:	jmpq   413817 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x3a7>
  413eb0:	cmp    %r9,%r10
  413eb3:	jae    413c7d <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x80d>
  413eb9:	lea    0x1(%rax),%rsi
  413ebd:	mov    %r8,%rdi
  413ec0:	mov    %rcx,0x8(%rsp)
  413ec5:	callq  424b00 <_ZNSt6vectorIjSaIjEE6resizeEm>
  413eca:	mov    0x178(%rbp),%r11
  413ed1:	addl   $0x1,0x8(%rbx)
  413ed5:	mov    0x8(%rsp),%rcx
  413eda:	mov    0x190(%rbp),%rdi
  413ee1:	mov    0x160(%rbp),%rax
  413ee8:	lea    (%r11,%rcx,1),%rbx
  413eec:	mov    0x4(%rbx),%esi
  413eef:	jmpq   41383a <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x3ca>
  413ef4:	nopl   0x0(%rax)
  413ef8:	lea    0x160(%rbp),%r8
  413eff:	mov    $0x3,%esi
  413f04:	mov    %rcx,0x10(%rsp)
  413f09:	mov    %r8,%rdi
  413f0c:	mov    %r8,0x8(%rsp)
  413f11:	callq  424a40 <_ZNSt6vectorIjSaIjEE7reserveEm>
  413f16:	mov    0x4(%rbx),%esi
  413f19:	mov    0x10(%rsp),%rcx
  413f1e:	mov    0x160(%rbp),%rax
  413f25:	mov    0x170(%rbp),%r9
  413f2c:	mov    0x8(%rsp),%r8
  413f31:	jmpq   413c55 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x7e5>
  413f36:	nopw   %cs:0x0(%rax,%rax,1)
  413f40:	lea    0x160(%rbp),%r8
  413f47:	mov    $0x3,%esi
  413f4c:	mov    %r9d,0x18(%rsp)
  413f51:	mov    %r8,%rdi
  413f54:	mov    %rcx,0x10(%rsp)
  413f59:	mov    %r8,0x8(%rsp)
  413f5e:	callq  424a40 <_ZNSt6vectorIjSaIjEE7reserveEm>
  413f63:	mov    0x4(%r12),%edx
  413f68:	mov    0x18(%rsp),%r9d
  413f6d:	mov    0x160(%rbp),%rax
  413f74:	mov    0x170(%rbp),%rdi
  413f7b:	mov    0x10(%rsp),%rcx
  413f80:	mov    0x8(%rsp),%r8
  413f85:	jmpq   4138e2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x472>
  413f8a:	lea    0x1c(%rdx),%rax
  413f8e:	mov    %r12,%rdi
  413f91:	cmp    %rax,0x40(%rsp)
  413f96:	jne    413e45 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x9d5>
  413f9c:	cmpb   $0x0,0x20(%rsp)
  413fa1:	jne    414305 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe95>
  413fa7:	add    0x130(%rbp),%rcx
  413fae:	mov    %r9d,%esi
  413fb1:	mov    (%rcx),%eax
  413fb3:	lea    0x14(%rcx),%rdx
  413fb7:	shr    %esi
  413fb9:	xor    %ecx,%ecx
  413fbb:	shl    $0x2,%rax
  413fbf:	lea    (%rdx,%rax,1),%rdi
  413fc3:	je     413feb <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb7b>
  413fc5:	nopl   (%rax)
  413fc8:	mov    (%rdx),%eax
  413fca:	shr    %eax
  413fcc:	cmp    %eax,%esi
  413fce:	je     413fe2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb72>
  413fd0:	shl    $0x4,%rax
  413fd4:	add    0xa0(%rbp),%rax
  413fdb:	mov    (%rax),%eax
  413fdd:	cmp    %eax,%ecx
  413fdf:	cmovb  %eax,%ecx
  413fe2:	add    $0x4,%rdx
  413fe6:	cmp    %rdi,%rdx
  413fe9:	jne    413fc8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb58>
  413feb:	mov    %r9d,%eax
  413fee:	xor    $0x1,%eax
  413ff1:	mov    %rax,%rdx
  413ff4:	shl    $0x4,%rdx
  413ff8:	add    0x178(%rbp),%rdx
  413fff:	mov    0x4(%rdx),%r8d
  414003:	test   %r8d,%r8d
  414006:	jne    4142c3 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe53>
  41400c:	mov    0x88(%rbp),%rdx
  414013:	mov    0x28(%rsp),%rbx
  414018:	xor    %edi,%edi
  41401a:	movb   $0x1,(%rdx,%rbx,1)
  41401e:	mov    0x88(%rbp),%rdx
  414025:	movb   $0xff,(%rdx,%rax,1)
  414029:	mov    %rsi,%rax
  41402c:	mov    0xe8(%rbp),%rdx
  414033:	shl    $0x4,%rax
  414037:	add    0xa0(%rbp),%rax
  41403e:	mov    %ecx,(%rax)
  414040:	mov    %edx,0x4(%rax)
  414043:	mov    %r13d,0x8(%rax)
  414047:	mov    %di,0xc(%rax)
  41404b:	lea    0x1(%rdx),%rax
  41404f:	mov    %rax,0xe8(%rbp)
  414056:	mov    0xd0(%rbp),%rax
  41405d:	mov    %r9d,(%rax,%rdx,4)
  414061:	test   %ecx,%ecx
  414063:	je     414279 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe09>
  414069:	mov    0x1de0(%rbp),%rdx
  414070:	test   %rdx,%rdx
  414073:	je     4140a0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc30>
  414075:	cmpb   $0x0,0x1de8(%rbp)
  41407c:	jne    4140a0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc30>
  41407e:	cmpb   $0x0,0x220(%rbp)
  414085:	jne    4140a0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc30>
  414087:	mov    %r9d,%eax
  41408a:	mov    (%rdx),%rdx
  41408d:	and    $0x1,%eax
  414090:	cmp    $0x1,%al
  414092:	sbb    %eax,%eax
  414094:	and    $0x2,%eax
  414097:	sub    $0x1,%eax
  41409a:	mov    %al,(%rdx,%rsi,1)
  41409d:	nopl   (%rax)
  4140a0:	mov    0x1d48(%rbp),%rdx
  4140a7:	mov    0x1d50(%rbp),%rax
  4140ae:	subl   $0x1,0x11c(%rbp)
  4140b5:	sub    %rdx,%rax
  4140b8:	cmp    %rax,%rsi
  4140bb:	jb     41426a <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xdfa>
  4140c1:	subl   $0x1,0x120(%rbp)
  4140c8:	add    $0x2,%r15
  4140cc:	mov    %r10,%rdx
  4140cf:	mov    %r11,%rbx
  4140d2:	jmpq   4136fe <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x28e>
  4140d7:	nopw   0x0(%rax,%rax,1)
  4140e0:	mov    0x8(%rsp),%eax
  4140e4:	mov    %r15,0x38(%rsp)
  4140e9:	mov    %r10,%r9
  4140ec:	movb   $0x1,0x2d1(%rbp)
  4140f3:	mov    0x50(%rsp),%r12
  4140f8:	movl   $0xffffffff,0x2d4(%rbp)
  414102:	mov    %eax,0x2d8(%rbp)
  414108:	mov    %ecx,0x2dc(%rbp)
  41410e:	mov    0x48(%rsp),%rax
  414113:	mov    %r9,%r11
  414116:	add    %r9,%rax
  414119:	cmp    %rdx,(%rsp)
  41411d:	je     41432f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xebf>
  414123:	mov    (%rsp),%rsi
  414127:	sub    %rdx,%rsi
  41412a:	lea    -0x4(%rsi),%rdi
  41412e:	cmp    $0x8,%rdi
  414132:	jbe    4141f8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd88>
  414138:	lea    0x4(%rdx),%rcx
  41413c:	mov    %rbx,%r8
  41413f:	sub    %rcx,%r8
  414142:	xor    %ecx,%ecx
  414144:	cmp    $0x8,%r8
  414148:	ja     414180 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd10>
  41414a:	nopw   0x0(%rax,%rax,1)
  414150:	mov    (%rdx,%rcx,1),%edi
  414153:	mov    %edi,(%rbx,%rcx,1)
  414156:	add    $0x4,%rcx
  41415a:	cmp    %rsi,%rcx
  41415d:	jne    414150 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xce0>
  41415f:	lea    (%rbx,%rsi,1),%rdx
  414163:	mov    0x4(%rax),%edi
  414166:	mov    $0x1,%r14d
  41416c:	sub    %r12,%rdx
  41416f:	sar    $0x2,%rdx
  414173:	jmpq   4137d0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x360>
  414178:	nopl   0x0(%rax,%rax,1)
  414180:	shr    $0x2,%rdi
  414184:	add    $0x1,%rdi
  414188:	mov    %rdi,%r8
  41418b:	shr    $0x2,%r8
  41418f:	shl    $0x4,%r8
  414193:	nopl   0x0(%rax,%rax,1)
  414198:	movdqu (%rdx,%rcx,1),%xmm2
  41419d:	movups %xmm2,(%rbx,%rcx,1)
  4141a1:	add    $0x10,%rcx
  4141a5:	cmp    %r8,%rcx
  4141a8:	jne    414198 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd28>
  4141aa:	test   $0x3,%dil
  4141ae:	je     41415f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xcef>
  4141b0:	and    $0xfffffffffffffffc,%rdi
  4141b4:	mov    (%rsp),%r15
  4141b8:	lea    0x0(,%rdi,4),%rcx
  4141c0:	add    %rcx,%rdx
  4141c3:	add    %rbx,%rcx
  4141c6:	mov    (%rdx),%edi
  4141c8:	mov    %edi,(%rcx)
  4141ca:	lea    0x4(%rdx),%rdi
  4141ce:	cmp    %rdi,%r15
  4141d1:	je     41415f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xcef>
  4141d3:	mov    0x4(%rdx),%edi
  4141d6:	mov    %edi,0x4(%rcx)
  4141d9:	lea    0x8(%rdx),%rdi
  4141dd:	cmp    %rdi,%r15
  4141e0:	je     41415f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xcef>
  4141e6:	mov    0x8(%rdx),%edx
  4141e9:	mov    %edx,0x8(%rcx)
  4141ec:	jmpq   41415f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xcef>
  4141f1:	nopl   0x0(%rax)
  4141f8:	xor    %ecx,%ecx
  4141fa:	jmpq   414150 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xce0>
  4141ff:	mov    0x5c(%rsp),%ebx
  414203:	mov    %rbp,%r13
  414206:	xor    %r12d,%r12d
  414209:	movdqu 0x678(%r13),%xmm4
  414212:	mov    0x68(%rsp),%rdx
  414217:	mov    %eax,%edi
  414219:	sub    %ebx,%edi
  41421b:	cmpb   $0x0,0x1dd8(%r13)
  414223:	movq   %rdi,%xmm0
  414228:	movq   %rdx,%xmm3
  41422d:	punpcklqdq %xmm3,%xmm0
  414231:	paddq  %xmm4,%xmm0
  414235:	movups %xmm0,0x678(%r13)
  41423d:	jne    414249 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xdd9>
  41423f:	cmpb   $0x0,0x124(%r13)
  414247:	je     41428d <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xe1d>
  414249:	mov    0x1dd0(%r13),%rax
  414250:	test   %rax,%rax
  414253:	je     414258 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xde8>
  414255:	add    %rdx,(%rax)
  414258:	add    $0x78,%rsp
  41425c:	mov    %r12d,%eax
  41425f:	pop    %rbx
  414260:	pop    %rbp
  414261:	pop    %r12
  414263:	pop    %r13
  414265:	pop    %r14
  414267:	pop    %r15
  414269:	retq   
  41426a:	cmpb   $0x0,(%rdx,%rsi,1)
  41426e:	jne    4140c8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc58>
  414274:	jmpq   4140c1 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc51>
  414279:	addq   $0x1,0x698(%rbp)
  414281:	addl   $0x1,0x1c54(%rbp)
  414288:	jmpq   414069 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xbf9>
  41428d:	add    %rdx,0x688(%r13)
  414294:	jmp    414249 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xdd9>
  414296:	mov    %r10,%rbx
  414299:	jne    413a15 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x5a5>
  41429f:	mov    (%rsi),%edx
  4142a1:	mov    %edx,(%rdi)
  4142a3:	mov    0x4(%r10),%edx
  4142a7:	jmpq   413a15 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x5a5>
  4142ac:	mov    %r9,%rbx
  4142af:	jne    413d7d <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x90d>
  4142b5:	mov    (%r10),%edx
  4142b8:	mov    %edx,(%rdi)
  4142ba:	mov    0x4(%r9),%esi
  4142be:	jmpq   413d7d <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x90d>
  4142c3:	mov    (%rdx),%edi
  4142c5:	mov    0x160(%rbp),%rdx
  4142cc:	prefetcht2 (%rdx,%rdi,4)
  4142d0:	jmpq   41400c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb9c>
  4142d5:	mov    $0x3,%edx
  4142da:	jmpq   413cc4 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x854>
  4142df:	mov    $0x3,%edx
  4142e4:	jmpq   41394c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x4dc>
  4142e9:	mov    (%r10),%r11d
  4142ec:	mov    0x160(%rbp),%r10
  4142f3:	prefetcht2 (%r10,%r11,4)
  4142f8:	jmpq   413bc1 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x751>
  4142fd:	xor    %r14d,%r14d
  414300:	jmpq   4137d0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x360>
  414305:	mov    %r8,0x38(%rsp)
  41430a:	mov    0x50(%rsp),%r12
  41430f:	mov    %r10,%rdx
  414312:	mov    %r11,%rbx
  414315:	movb   $0x1,0x2d1(%rbp)
  41431c:	mov    0x178(%rbp),%r9
  414323:	mov    %r13d,0x2d4(%rbp)
  41432a:	jmpq   41410e <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc9e>
  41432f:	mov    $0x1,%r14d
  414335:	jmpq   4137bc <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x34c>

Disassembly of section .fini:
