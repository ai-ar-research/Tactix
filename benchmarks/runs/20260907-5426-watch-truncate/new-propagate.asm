
/home/hwu/SAT-solving/build/pin-5426-watch-truncate-release/tactix:     file format elf64-x86-64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

0000000000413520 <_ZN6tactix16kernel_propagateERNS_6KernelEj>:
  413520:	push   %r15
  413522:	mov    %rdi,%r15
  413525:	push   %r14
  413527:	push   %r13
  413529:	push   %r12
  41352b:	push   %rbp
  41352c:	push   %rbx
  41352d:	sub    $0x78,%rsp
  413531:	mov    0xf4(%rdi),%eax
  413537:	mov    0xf0(%rdi),%ebx
  41353d:	mov    %esi,0x58(%rsp)
  413541:	cmp    %ebx,%eax
  413543:	jae    41354d <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x2d>
  413545:	mov    %eax,0xf0(%rdi)
  41354b:	mov    %eax,%ebx
  41354d:	lea    0x160(%r15),%rdi
  413554:	mov    %ebx,0x5c(%rsp)
  413558:	mov    %ebx,%eax
  41355a:	movl   $0xffffffff,0xf4(%r15)
  413565:	movq   $0x0,0x68(%rsp)
  41356e:	mov    %rdi,0x30(%rsp)
  413573:	jmp    41359c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x7c>
  413575:	nopl   (%rax)
  413578:	mov    0x58(%rsp),%edx
  41357c:	lea    0x68(%rsp),%rcx
  413581:	mov    %r15,%rdi
  413584:	callq  4127b0 <_ZN6tactixL17propagate_literalILb0ELb0ELb1EEEbRNS_6KernelEjjRm>
  413589:	mov    %eax,%r13d
  41358c:	mov    0xf0(%r15),%eax
  413593:	test   %r13b,%r13b
  413596:	jne    413df3 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x8d3>
  41359c:	mov    %eax,%edx
  41359e:	cmp    0xe8(%r15),%rdx
  4135a5:	jae    4141df <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xcbf>
  4135ab:	add    $0x1,%eax
  4135ae:	cmpl   $0xffffffff,0x58(%rsp)
  4135b3:	mov    %eax,0xf0(%r15)
  4135ba:	mov    0xd0(%r15),%rax
  4135c1:	mov    (%rax,%rdx,4),%esi
  4135c4:	jne    413578 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x58>
  4135c6:	mov    %esi,%eax
  4135c8:	mov    0x178(%r15),%r11
  4135cf:	mov    0x130(%r15),%rbx
  4135d6:	shr    %esi
  4135d8:	xor    $0x1,%eax
  4135db:	mov    0x160(%r15),%r8
  4135e2:	shl    $0x4,%rsi
  4135e6:	add    0xa0(%r15),%rsi
  4135ed:	mov    %eax,0x8(%rsp)
  4135f1:	shl    $0x4,%rax
  4135f5:	mov    0x88(%r15),%rbp
  4135fc:	mov    %rax,%r14
  4135ff:	lea    (%r11,%rax,1),%rax
  413603:	mov    %rbx,0x10(%rsp)
  413608:	mov    (%rax),%r9d
  41360b:	mov    0x4(%rax),%ecx
  41360e:	mov    %rsi,0x18(%rsp)
  413613:	mov    %rcx,%rdx
  413616:	add    %r9,%rcx
  413619:	lea    0x0(,%r9,4),%rdi
  413621:	shl    $0x2,%rcx
  413625:	lea    (%r8,%rdi,1),%r12
  413629:	lea    (%r8,%rcx,1),%rbx
  41362d:	sub    %rdi,%rcx
  413630:	sar    $0x2,%rcx
  413634:	mov    %rbx,(%rsp)
  413638:	lea    0x1f(%rcx),%rsi
  41363c:	shr    $0x5,%rsi
  413640:	lea    0x1(%rsi),%rdi
  413644:	mov    %rdi,0x20(%rsp)
  413649:	cmp    %rbx,%r12
  41364c:	je     4142da <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xdba>
  413652:	mov    %r14,0x48(%rsp)
  413657:	mov    %r12,%rbx
  41365a:	mov    %r12,%rdx
  41365d:	mov    %rdi,%r14
  413660:	mov    %r12,0x50(%rsp)
  413665:	jmpq   4137c2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x2a2>
  41366a:	nopw   0x0(%rax,%rax,1)
  413670:	add    $0x4,%rdx
  413674:	add    $0x4,%rbx
  413678:	test   %dil,%dil
  41367b:	jg     4137b8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x298>
  413681:	mov    0x178(%r15),%r10
  413688:	jne    4140c0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xba0>
  41368e:	mov    %ecx,%edi
  413690:	mov    0x18(%rsp),%r11
  413695:	shr    $0x2,%eax
  413698:	xor    $0x1,%edi
  41369b:	mov    %rdi,%r9
  41369e:	mov    (%r11),%r8d
  4136a1:	shl    $0x4,%r9
  4136a5:	add    %r9,%r10
  4136a8:	cmpb   $0x0,0x1e6d(%r15)
  4136b0:	je     4136c5 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x1a5>
  4136b2:	cmpb   $0x0,0x1e6e(%r15)
  4136ba:	je     4136c5 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x1a5>
  4136bc:	test   %r8d,%r8d
  4136bf:	jne    413bc8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x6a8>
  4136c5:	mov    0x4(%r10),%r9d
  4136c9:	test   %r9d,%r9d
  4136cc:	jne    413aa0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x580>
  4136d2:	mov    0x88(%r15),%r9
  4136d9:	mov    0x8(%rsp),%r11d
  4136de:	movd   %r8d,%xmm0
  4136e3:	movzwl 0xfbcf5(%rip),%r10d        # 50f3e0 <_ZN6tactix11BinaryIdMap5EMPTYE+0x18>
  4136eb:	movb   $0x1,(%r9,%rsi,1)
  4136f0:	mov    0x88(%r15),%rsi
  4136f7:	movb   $0xff,(%rsi,%rdi,1)
  4136fb:	mov    0xe8(%r15),%rdi
  413702:	mov    %rax,%rsi
  413705:	shl    $0x4,%rsi
  413709:	add    0xa0(%r15),%rsi
  413710:	movd   %edi,%xmm1
  413714:	mov    %r11d,0x8(%rsi)
  413718:	punpckldq %xmm1,%xmm0
  41371c:	mov    %r10w,0xc(%rsi)
  413721:	movq   %xmm0,(%rsi)
  413725:	lea    0x1(%rdi),%rsi
  413729:	mov    %rsi,0xe8(%r15)
  413730:	mov    0xd0(%r15),%rsi
  413737:	mov    %ecx,(%rsi,%rdi,4)
  41373a:	test   %r8d,%r8d
  41373d:	jne    41374f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x22f>
  41373f:	addq   $0x1,0x698(%r15)
  413747:	addl   $0x1,0x1c54(%r15)
  41374f:	mov    0x1de0(%r15),%rsi
  413756:	test   %rsi,%rsi
  413759:	je     413788 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x268>
  41375b:	cmpb   $0x0,0x1de8(%r15)
  413763:	jne    413788 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x268>
  413765:	cmpb   $0x0,0x220(%r15)
  41376d:	jne    413788 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x268>
  41376f:	and    $0x1,%ecx
  413772:	mov    (%rsi),%rsi
  413775:	cmp    $0x1,%cl
  413778:	sbb    %ecx,%ecx
  41377a:	and    $0x2,%ecx
  41377d:	sub    $0x1,%ecx
  413780:	mov    %cl,(%rsi,%rax,1)
  413783:	nopl   0x0(%rax,%rax,1)
  413788:	mov    0x1d48(%r15),%rsi
  41378f:	mov    0x1d50(%r15),%rcx
  413796:	subl   $0x1,0x11c(%r15)
  41379e:	sub    %rsi,%rcx
  4137a1:	cmp    %rcx,%rax
  4137a4:	jae    4137ac <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x28c>
  4137a6:	cmpb   $0x0,(%rsi,%rax,1)
  4137aa:	jne    4137b4 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x294>
  4137ac:	subl   $0x1,0x120(%r15)
  4137b4:	add    $0x1,%r14
  4137b8:	cmp    %rdx,(%rsp)
  4137bc:	je     413850 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x330>
  4137c2:	mov    (%rdx),%eax
  4137c4:	mov    %eax,%ecx
  4137c6:	mov    %eax,(%rbx)
  4137c8:	shr    %ecx
  4137ca:	mov    %ecx,%esi
  4137cc:	movzbl 0x0(%rbp,%rsi,1),%edi
  4137d1:	test   $0x1,%al
  4137d3:	jne    413670 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x150>
  4137d9:	mov    0x4(%rdx),%r13d
  4137dd:	lea    0x8(%rdx),%r10
  4137e1:	lea    0x8(%rbx),%r11
  4137e5:	mov    %r13d,0x4(%rbx)
  4137e9:	test   %dil,%dil
  4137ec:	jg     413a80 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x560>
  4137f2:	mov    0x10(%rsp),%rax
  4137f7:	mov    %r13d,%ecx
  4137fa:	lea    0x1(%r14),%r8
  4137fe:	shl    $0x2,%rcx
  413802:	lea    (%rax,%rcx,1),%rdx
  413806:	testb  $0x1,0x4(%rdx)
  41380a:	jne    413a90 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x570>
  413810:	mov    0x8(%rsp),%r9d
  413815:	mov    0x14(%rdx),%eax
  413818:	xor    0x18(%rdx),%eax
  41381b:	xor    %eax,%r9d
  41381e:	mov    %r9d,%eax
  413821:	mov    %rax,0x28(%rsp)
  413826:	movzbl 0x0(%rbp,%rax,1),%eax
  41382b:	mov    %al,0x20(%rsp)
  41382f:	test   %al,%al
  413831:	jle    413ab8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x598>
  413837:	lea    (%r9,%r9,1),%eax
  41383b:	mov    %r8,%r14
  41383e:	mov    %r10,%rdx
  413841:	mov    %eax,(%rbx)
  413843:	mov    %r11,%rbx
  413846:	cmp    %rdx,(%rsp)
  41384a:	jne    4137c2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x2a2>
  413850:	mov    %r14,0x20(%rsp)
  413855:	mov    0x178(%r15),%r11
  41385c:	xor    %r13d,%r13d
  41385f:	mov    0x48(%rsp),%r14
  413864:	mov    0x50(%rsp),%r12
  413869:	lea    (%r11,%r14,1),%rax
  41386d:	sub    %r12,%rbx
  413870:	mov    0x4(%rax),%edx
  413873:	mov    %rbx,%rcx
  413876:	sar    $0x2,%rcx
  41387a:	nopw   0x0(%rax,%rax,1)
  413880:	sub    %ecx,%edx
  413882:	add    0x190(%r15),%rdx
  413889:	cmpq   $0x0,0x218(%r15)
  413891:	mov    %rdx,0x190(%r15)
  413898:	mov    %ecx,0x4(%rax)
  41389b:	je     413dce <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x8ae>
  4138a1:	mov    %r13b,0x28(%rsp)
  4138a6:	mov    0x160(%r15),%rax
  4138ad:	xor    %ebp,%ebp
  4138af:	jmp    413903 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x3e3>
  4138b1:	nopl   0x0(%rax)
  4138b8:	sub    $0x1,%rdx
  4138bc:	lea    0x1(%rsi),%edi
  4138bf:	add    (%rbx),%esi
  4138c1:	mov    %rdx,0x190(%r15)
  4138c8:	mov    %edi,0x4(%rbx)
  4138cb:	mov    %r14d,(%rax,%rsi,4)
  4138cf:	mov    0x4(%rbx),%esi
  4138d2:	cmp    0x8(%rbx),%esi
  4138d5:	je     413c80 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x760>
  4138db:	sub    $0x1,%rdx
  4138df:	lea    0x1(%rsi),%ecx
  4138e2:	add    (%rbx),%esi
  4138e4:	mov    %rdx,0x190(%r15)
  4138eb:	add    $0x3,%rbp
  4138ef:	mov    %ecx,0x4(%rbx)
  4138f2:	mov    %r13d,(%rax,%rsi,4)
  4138f6:	cmp    0x218(%r15),%rbp
  4138fd:	jae    413dc8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x8a8>
  413903:	mov    0x200(%r15),%rcx
  41390a:	lea    0x0(,%rbp,4),%rsi
  413912:	mov    0x4(%rcx,%rsi,1),%r14d
  413917:	mov    0x8(%rcx,%rsi,1),%r13d
  41391c:	mov    (%rcx,%rbp,4),%ecx
  41391f:	add    %r14d,%r14d
  413922:	shl    $0x4,%rcx
  413926:	lea    (%r11,%rcx,1),%rbx
  41392a:	mov    0x4(%rbx),%esi
  41392d:	cmp    0x8(%rbx),%esi
  413930:	jne    4138b8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x398>
  413932:	mov    0x170(%r15),%r10
  413939:	mov    0x30(%rsp),%rdi
  41393e:	cmp    %rax,%r10
  413941:	je     413f30 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xa10>
  413947:	mov    0x168(%r15),%r11
  41394e:	sub    %rax,%r10
  413951:	mov    %esi,%r12d
  413954:	sub    %rax,%r11
  413957:	mov    (%rbx),%eax
  413959:	mov    %r11,%r9
  41395c:	sar    $0x2,%r9
  413960:	add    %r12,%rax
  413963:	mov    %r9,%rdx
  413966:	cmp    %r9,%rax
  413969:	je     413e70 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x950>
  41396f:	test   %esi,%esi
  413971:	je     413e00 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x8e0>
  413977:	lea    (%rsi,%rsi,1),%r12d
  41397b:	mov    %r12d,(%rsp)
  41397f:	lea    (%r9,%r12,1),%rsi
  413983:	sar    $0x2,%r10
  413987:	cmp    %rsi,%r10
  41398a:	jae    4139de <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x4be>
  41398c:	cmp    $0x3,%rsi
  413990:	jbe    4142bc <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd9c>
  413996:	mov    $0x4,%eax
  41399b:	nopl   0x0(%rax,%rax,1)
  4139a0:	add    %rax,%rax
  4139a3:	lea    -0x1(%rax),%rdx
  4139a7:	cmp    %rsi,%rdx
  4139aa:	jb     4139a0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x480>
  4139ac:	mov    %rdx,%rsi
  4139af:	mov    %rcx,0x10(%rsp)
  4139b4:	mov    %rdi,0x8(%rsp)
  4139b9:	callq  424850 <_ZNSt6vectorIjSaIjEE7reserveEm>
  4139be:	mov    0x10(%rsp),%rcx
  4139c3:	mov    0x8(%rsp),%rdi
  4139c8:	mov    0x168(%r15),%rdx
  4139cf:	sub    0x160(%r15),%rdx
  4139d6:	sar    $0x2,%rdx
  4139da:	lea    (%rdx,%r12,1),%rsi
  4139de:	mov    %rcx,0x18(%rsp)
  4139e3:	mov    %edx,0x8(%rsp)
  4139e7:	mov    %rdx,0x10(%rsp)
  4139ec:	callq  424910 <_ZNSt6vectorIjSaIjEE6resizeEm>
  4139f1:	mov    0x18(%rsp),%rcx
  4139f6:	mov    0x178(%r15),%r11
  4139fd:	mov    0x10(%rsp),%rdx
  413a02:	mov    0x160(%r15),%rax
  413a09:	lea    (%r11,%rcx,1),%r9
  413a0d:	mov    %edx,%edx
  413a0f:	mov    0x4(%r9),%esi
  413a13:	lea    (%rax,%rdx,4),%rdi
  413a17:	mov    (%r9),%edx
  413a1a:	lea    (%rax,%rdx,4),%r10
  413a1e:	mov    %esi,%edx
  413a20:	shl    $0x2,%rdx
  413a24:	cmp    $0x4,%rdx
  413a28:	jbe    414272 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd52>
  413a2e:	mov    %r10,%rsi
  413a31:	mov    %r9,0x18(%rsp)
  413a36:	mov    %rcx,0x10(%rsp)
  413a3b:	callq  4023b0 <memmove@plt>
  413a40:	mov    0x10(%rsp),%rcx
  413a45:	mov    0x18(%rsp),%r9
  413a4a:	mov    0x178(%r15),%r11
  413a51:	mov    0x160(%r15),%rax
  413a58:	lea    (%r11,%rcx,1),%rbx
  413a5c:	mov    0x4(%rbx),%esi
  413a5f:	mov    0x8(%rsp),%edi
  413a63:	mov    0x190(%r15),%rdx
  413a6a:	mov    %edi,(%r9)
  413a6d:	mov    (%rsp),%edi
  413a70:	lea    -0x1(%r12,%rdx,1),%rdx
  413a75:	mov    %edi,0x8(%r9)
  413a79:	jmpq   4138bc <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x39c>
  413a7e:	xchg   %ax,%ax
  413a80:	mov    %r10,%rdx
  413a83:	mov    %r11,%rbx
  413a86:	jmpq   4137b8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x298>
  413a8b:	nopl   0x0(%rax,%rax,1)
  413a90:	mov    %r8,%r14
  413a93:	mov    %r10,%rdx
  413a96:	jmpq   4137b8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x298>
  413a9b:	nopl   0x0(%rax,%rax,1)
  413aa0:	mov    (%r10),%r10d
  413aa3:	mov    0x160(%r15),%r9
  413aaa:	prefetcht2 (%r9,%r10,4)
  413aaf:	jmpq   4136d2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x1b2>
  413ab4:	nopl   0x0(%rax)
  413ab8:	mov    (%rdx),%esi
  413aba:	mov    0x8(%rdx),%eax
  413abd:	lea    0x14(%rdx),%r12
  413ac1:	mov    %r12,0x38(%rsp)
  413ac6:	shl    $0x2,%rsi
  413aca:	shl    $0x2,%rax
  413ace:	lea    (%r12,%rsi,1),%rdi
  413ad2:	add    %rax,%r12
  413ad5:	mov    %r12,0x40(%rsp)
  413ada:	cmp    %rax,%rsi
  413add:	je     413f69 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xa49>
  413ae3:	mov    %r12,%rax
  413ae6:	jmp    413afd <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x5dd>
  413ae8:	nopl   0x0(%rax,%rax,1)
  413af0:	add    $0x4,%rax
  413af4:	cmp    %rax,%rdi
  413af7:	je     413e30 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x910>
  413afd:	mov    (%rax),%esi
  413aff:	cmpb   $0x0,0x0(%rbp,%rsi,1)
  413b04:	mov    %rsi,%r12
  413b07:	js     413af0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x5d0>
  413b09:	mov    0x38(%rsp),%rdi
  413b0e:	mov    %rax,%rcx
  413b11:	mov    0x208(%r15),%rsi
  413b18:	mov    %r9d,0x14(%rdx)
  413b1c:	mov    %r12d,0x18(%rdx)
  413b20:	sub    %rdi,%rcx
  413b23:	mov    0x8(%rsp),%edi
  413b27:	sar    $0x2,%rcx
  413b2b:	mov    %ecx,0x8(%rdx)
  413b2e:	mov    0x200(%r15),%rdx
  413b35:	mov    %edi,(%rax)
  413b37:	mov    0x218(%r15),%rcx
  413b3e:	mov    %rsi,%rax
  413b41:	sub    %rdx,%rax
  413b44:	lea    0x3(%rcx),%rdi
  413b48:	sar    $0x2,%rax
  413b4c:	cmp    %rdi,%rax
  413b4f:	jae    413b9c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x67c>
  413b51:	add    %rax,%rax
  413b54:	cmp    %rdx,%rsi
  413b57:	mov    $0x3,%edx
  413b5c:	mov    %r10,0x28(%rsp)
  413b61:	cmove  %rdx,%rax
  413b65:	mov    %r9d,0x20(%rsp)
  413b6a:	cmp    %rax,%rdi
  413b6d:	cmovae %rdi,%rax
  413b71:	lea    0x200(%r15),%rdi
  413b78:	mov    %rax,%rsi
  413b7b:	callq  424910 <_ZNSt6vectorIjSaIjEE6resizeEm>
  413b80:	mov    0x28(%rsp),%r10
  413b85:	mov    0x20(%rsp),%r9d
  413b8a:	mov    0x218(%r15),%rcx
  413b91:	mov    0x200(%r15),%rdx
  413b98:	lea    0x3(%rcx),%rdi
  413b9c:	lea    0x0(,%rcx,4),%rax
  413ba4:	mov    %r12d,(%rdx,%rcx,4)
  413ba8:	add    $0x2,%r14
  413bac:	mov    %r9d,0x4(%rdx,%rax,1)
  413bb1:	mov    %rdi,0x218(%r15)
  413bb8:	mov    %r13d,0x8(%rdx,%rax,1)
  413bbd:	mov    %r10,%rdx
  413bc0:	jmpq   4137b8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x298>
  413bc5:	nopl   (%rax)
  413bc8:	cmpb   $0x0,0xc(%r11)
  413bcd:	je     4136c5 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x1a5>
  413bd3:	mov    0xb8(%r15),%r9
  413bda:	mov    0x8(%rsp),%r11d
  413bdf:	mov    %r11d,(%r9,%rax,4)
  413be3:	mov    0x18(%rsp),%r11
  413be8:	addq   $0x1,0x18e8(%r15)
  413bf0:	mov    0x8(%r11),%r9d
  413bf4:	mov    0x4(%r10),%r11d
  413bf8:	test   %r11d,%r11d
  413bfb:	jne    4142c6 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xda6>
  413c01:	mov    0x88(%r15),%r10
  413c08:	movd   %r8d,%xmm0
  413c0d:	movb   $0x1,(%r10,%rsi,1)
  413c12:	mov    0x88(%r15),%rsi
  413c19:	mov    $0x101,%r10d
  413c1f:	movb   $0xff,(%rsi,%rdi,1)
  413c23:	mov    0xe8(%r15),%rdi
  413c2a:	mov    %rax,%rsi
  413c2d:	shl    $0x4,%rsi
  413c31:	add    0xa0(%r15),%rsi
  413c38:	movd   %edi,%xmm5
  413c3c:	mov    %r9d,0x8(%rsi)
  413c40:	punpckldq %xmm5,%xmm0
  413c44:	mov    %r10w,0xc(%rsi)
  413c49:	movq   %xmm0,(%rsi)
  413c4d:	lea    0x1(%rdi),%rsi
  413c51:	mov    %rsi,0xe8(%r15)
  413c58:	mov    0xd0(%r15),%rsi
  413c5f:	mov    %ecx,(%rsi,%rdi,4)
  413c62:	mov    0x1de0(%r15),%rsi
  413c69:	test   %rsi,%rsi
  413c6c:	jne    41375b <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x23b>
  413c72:	jmpq   413788 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x268>
  413c77:	nopw   0x0(%rax,%rax,1)
  413c80:	mov    0x170(%r15),%r10
  413c87:	mov    0x30(%rsp),%rdi
  413c8c:	cmp    %rax,%r10
  413c8f:	je     413ef0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x9d0>
  413c95:	mov    0x168(%r15),%r11
  413c9c:	sub    %rax,%r10
  413c9f:	mov    %esi,%r12d
  413ca2:	sub    %rax,%r11
  413ca5:	mov    (%rbx),%eax
  413ca7:	mov    %r11,%r9
  413caa:	sar    $0x2,%r9
  413cae:	add    %r12,%rax
  413cb1:	mov    %r9,%rdx
  413cb4:	cmp    %r9,%rax
  413cb7:	je     413eb0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x990>
  413cbd:	test   %esi,%esi
  413cbf:	je     413e18 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x8f8>
  413cc5:	lea    (%rsi,%rsi,1),%r12d
  413cc9:	mov    %r12,%r14
  413ccc:	add    %r12,%r9
  413ccf:	sar    $0x2,%r10
  413cd3:	cmp    %r9,%r10
  413cd6:	jae    413d29 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x809>
  413cd8:	cmp    $0x3,%r9
  413cdc:	jbe    4142b2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd92>
  413ce2:	mov    $0x4,%eax
  413ce7:	nopw   0x0(%rax,%rax,1)
  413cf0:	add    %rax,%rax
  413cf3:	lea    -0x1(%rax),%rsi
  413cf7:	cmp    %r9,%rsi
  413cfa:	jb     413cf0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x7d0>
  413cfc:	mov    %rcx,0x8(%rsp)
  413d01:	mov    %rdi,(%rsp)
  413d05:	callq  424850 <_ZNSt6vectorIjSaIjEE7reserveEm>
  413d0a:	mov    0x168(%r15),%rdx
  413d11:	sub    0x160(%r15),%rdx
  413d18:	sar    $0x2,%rdx
  413d1c:	mov    0x8(%rsp),%rcx
  413d21:	mov    (%rsp),%rdi
  413d25:	lea    (%rdx,%r12,1),%r9
  413d29:	mov    %r9,%rsi
  413d2c:	mov    %rcx,0x10(%rsp)
  413d31:	mov    %edx,(%rsp)
  413d34:	mov    %rdx,0x8(%rsp)
  413d39:	callq  424910 <_ZNSt6vectorIjSaIjEE6resizeEm>
  413d3e:	mov    0x10(%rsp),%rcx
  413d43:	mov    0x178(%r15),%r11
  413d4a:	mov    0x8(%rsp),%rdx
  413d4f:	mov    0x160(%r15),%rax
  413d56:	lea    (%r11,%rcx,1),%r9
  413d5a:	mov    %edx,%edx
  413d5c:	mov    0x4(%r9),%esi
  413d60:	lea    (%rax,%rdx,4),%rdi
  413d64:	mov    (%r9),%edx
  413d67:	lea    (%rax,%rdx,4),%r10
  413d6b:	mov    %esi,%edx
  413d6d:	shl    $0x2,%rdx
  413d71:	cmp    $0x4,%rdx
  413d75:	jbe    414289 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd69>
  413d7b:	mov    %r10,%rsi
  413d7e:	mov    %r9,0x8(%rsp)
  413d83:	callq  4023b0 <memmove@plt>
  413d88:	mov    0x10(%rsp),%rcx
  413d8d:	mov    0x8(%rsp),%r9
  413d92:	mov    0x178(%r15),%r11
  413d99:	mov    0x160(%r15),%rax
  413da0:	lea    (%r11,%rcx,1),%rbx
  413da4:	mov    0x4(%rbx),%esi
  413da7:	mov    0x190(%r15),%rdx
  413dae:	mov    (%rsp),%edi
  413db1:	mov    %r14d,0x8(%r9)
  413db5:	mov    %edi,(%r9)
  413db8:	lea    -0x1(%r12,%rdx,1),%rdx
  413dbd:	jmpq   4138df <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x3bf>
  413dc2:	nopw   0x0(%rax,%rax,1)
  413dc8:	movzbl 0x28(%rsp),%r13d
  413dce:	movq   $0x0,0x218(%r15)
  413dd9:	mov    0x20(%rsp),%rax
  413dde:	add    %rax,0x68(%rsp)
  413de3:	mov    0xf0(%r15),%eax
  413dea:	test   %r13b,%r13b
  413ded:	je     41359c <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x7c>
  413df3:	mov    0x5c(%rsp),%ebx
  413df7:	mov    %r13d,%ecx
  413dfa:	jmpq   4141e5 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xcc5>
  413dff:	nop
  413e00:	movl   $0x1,(%rsp)
  413e07:	mov    $0x1,%r12d
  413e0d:	jmpq   41397f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x45f>
  413e12:	nopw   0x0(%rax,%rax,1)
  413e18:	mov    $0x1,%r12d
  413e1e:	mov    $0x1,%r14d
  413e24:	jmpq   413ccc <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x7ac>
  413e29:	nopl   0x0(%rax)
  413e30:	mov    0x40(%rsp),%rdi
  413e35:	lea    0x1c(%rdx),%rax
  413e39:	cmp    %rax,%rdi
  413e3c:	jne    413e55 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x935>
  413e3e:	jmpq   413f7b <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xa5b>
  413e43:	nopl   0x0(%rax,%rax,1)
  413e48:	add    $0x4,%rax
  413e4c:	cmp    %rax,%rdi
  413e4f:	je     413f7b <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xa5b>
  413e55:	mov    (%rax),%esi
  413e57:	cmpb   $0x0,0x0(%rbp,%rsi,1)
  413e5c:	mov    %rsi,%r12
  413e5f:	js     413e48 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x928>
  413e61:	jmpq   413b09 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x5e9>
  413e66:	nopw   %cs:0x0(%rax,%rax,1)
  413e70:	cmp    %r10,%r11
  413e73:	jae    41396f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x44f>
  413e79:	lea    0x1(%rax),%rsi
  413e7d:	mov    %rcx,(%rsp)
  413e81:	callq  424910 <_ZNSt6vectorIjSaIjEE6resizeEm>
  413e86:	mov    0x178(%r15),%r11
  413e8d:	mov    (%rsp),%rcx
  413e91:	addl   $0x1,0x8(%rbx)
  413e95:	mov    0x190(%r15),%rdx
  413e9c:	lea    (%r11,%rcx,1),%rbx
  413ea0:	mov    0x160(%r15),%rax
  413ea7:	mov    0x4(%rbx),%esi
  413eaa:	jmpq   4138bc <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x39c>
  413eaf:	nop
  413eb0:	cmp    %r10,%r11
  413eb3:	jae    413cbd <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x79d>
  413eb9:	lea    0x1(%rax),%rsi
  413ebd:	mov    %rcx,(%rsp)
  413ec1:	callq  424910 <_ZNSt6vectorIjSaIjEE6resizeEm>
  413ec6:	mov    0x178(%r15),%r11
  413ecd:	mov    (%rsp),%rcx
  413ed1:	addl   $0x1,0x8(%rbx)
  413ed5:	mov    0x190(%r15),%rdx
  413edc:	lea    (%r11,%rcx,1),%rbx
  413ee0:	mov    0x160(%r15),%rax
  413ee7:	mov    0x4(%rbx),%esi
  413eea:	jmpq   4138df <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x3bf>
  413eef:	nop
  413ef0:	lea    0x160(%r15),%rdi
  413ef7:	mov    $0x3,%esi
  413efc:	mov    %rcx,0x8(%rsp)
  413f01:	mov    %rdi,(%rsp)
  413f05:	callq  424850 <_ZNSt6vectorIjSaIjEE7reserveEm>
  413f0a:	mov    0x4(%rbx),%esi
  413f0d:	mov    0x8(%rsp),%rcx
  413f12:	mov    0x160(%r15),%rax
  413f19:	mov    0x170(%r15),%r10
  413f20:	mov    (%rsp),%rdi
  413f24:	jmpq   413c95 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x775>
  413f29:	nopl   0x0(%rax)
  413f30:	lea    0x160(%r15),%rdi
  413f37:	mov    $0x3,%esi
  413f3c:	mov    %rcx,0x8(%rsp)
  413f41:	mov    %rdi,(%rsp)
  413f45:	callq  424850 <_ZNSt6vectorIjSaIjEE7reserveEm>
  413f4a:	mov    0x4(%rbx),%esi
  413f4d:	mov    0x8(%rsp),%rcx
  413f52:	mov    0x160(%r15),%rax
  413f59:	mov    0x170(%r15),%r10
  413f60:	mov    (%rsp),%rdi
  413f64:	jmpq   413947 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x427>
  413f69:	lea    0x1c(%rdx),%rax
  413f6d:	mov    %r12,%rdi
  413f70:	cmp    %rax,0x40(%rsp)
  413f75:	jne    413e55 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x935>
  413f7b:	cmpb   $0x0,0x20(%rsp)
  413f80:	jne    4142e2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xdc2>
  413f86:	add    0x130(%r15),%rcx
  413f8d:	mov    %r9d,%esi
  413f90:	mov    (%rcx),%eax
  413f92:	lea    0x14(%rcx),%rdx
  413f96:	shr    %esi
  413f98:	xor    %ecx,%ecx
  413f9a:	shl    $0x2,%rax
  413f9e:	lea    (%rdx,%rax,1),%rdi
  413fa2:	je     413fcb <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xaab>
  413fa4:	nopl   0x0(%rax)
  413fa8:	mov    (%rdx),%eax
  413faa:	shr    %eax
  413fac:	cmp    %eax,%esi
  413fae:	je     413fc2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xaa2>
  413fb0:	shl    $0x4,%rax
  413fb4:	add    0xa0(%r15),%rax
  413fbb:	mov    (%rax),%eax
  413fbd:	cmp    %eax,%ecx
  413fbf:	cmovb  %eax,%ecx
  413fc2:	add    $0x4,%rdx
  413fc6:	cmp    %rdi,%rdx
  413fc9:	jne    413fa8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xa88>
  413fcb:	mov    %r9d,%eax
  413fce:	xor    $0x1,%eax
  413fd1:	mov    %rax,%rdx
  413fd4:	shl    $0x4,%rdx
  413fd8:	add    0x178(%r15),%rdx
  413fdf:	mov    0x4(%rdx),%r8d
  413fe3:	test   %r8d,%r8d
  413fe6:	jne    4142a0 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd80>
  413fec:	mov    0x88(%r15),%rdx
  413ff3:	mov    0x28(%rsp),%rbx
  413ff8:	xor    %edi,%edi
  413ffa:	movb   $0x1,(%rdx,%rbx,1)
  413ffe:	mov    0x88(%r15),%rdx
  414005:	movb   $0xff,(%rdx,%rax,1)
  414009:	mov    %rsi,%rax
  41400c:	mov    0xe8(%r15),%rdx
  414013:	shl    $0x4,%rax
  414017:	add    0xa0(%r15),%rax
  41401e:	mov    %ecx,(%rax)
  414020:	mov    %edx,0x4(%rax)
  414023:	mov    %r13d,0x8(%rax)
  414027:	mov    %di,0xc(%rax)
  41402b:	lea    0x1(%rdx),%rax
  41402f:	mov    %rax,0xe8(%r15)
  414036:	mov    0xd0(%r15),%rax
  41403d:	mov    %r9d,(%rax,%rdx,4)
  414041:	test   %ecx,%ecx
  414043:	je     414254 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd34>
  414049:	mov    0x1de0(%r15),%rdx
  414050:	test   %rdx,%rdx
  414053:	je     414080 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb60>
  414055:	cmpb   $0x0,0x1de8(%r15)
  41405d:	jne    414080 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb60>
  41405f:	cmpb   $0x0,0x220(%r15)
  414067:	jne    414080 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb60>
  414069:	mov    %r9d,%eax
  41406c:	mov    (%rdx),%rdx
  41406f:	and    $0x1,%eax
  414072:	cmp    $0x1,%al
  414074:	sbb    %eax,%eax
  414076:	and    $0x2,%eax
  414079:	sub    $0x1,%eax
  41407c:	mov    %al,(%rdx,%rsi,1)
  41407f:	nop
  414080:	mov    0x1d48(%r15),%rdx
  414087:	mov    0x1d50(%r15),%rax
  41408e:	subl   $0x1,0x11c(%r15)
  414096:	sub    %rdx,%rax
  414099:	cmp    %rax,%rsi
  41409c:	jb     414245 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd25>
  4140a2:	subl   $0x1,0x120(%r15)
  4140aa:	add    $0x2,%r14
  4140ae:	mov    %r10,%rdx
  4140b1:	mov    %r11,%rbx
  4140b4:	jmpq   4137b8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x298>
  4140b9:	nopl   0x0(%rax)
  4140c0:	mov    0x8(%rsp),%eax
  4140c4:	mov    %r14,0x20(%rsp)
  4140c9:	mov    %r10,%r9
  4140cc:	movb   $0x1,0x2d1(%r15)
  4140d4:	mov    0x50(%rsp),%r12
  4140d9:	movl   $0xffffffff,0x2d4(%r15)
  4140e4:	mov    0x48(%rsp),%r14
  4140e9:	mov    %eax,0x2d8(%r15)
  4140f0:	mov    %ecx,0x2dc(%r15)
  4140f7:	mov    %r9,%r11
  4140fa:	lea    (%r9,%r14,1),%rax
  4140fe:	cmp    %rdx,(%rsp)
  414102:	je     414312 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xdf2>
  414108:	mov    (%rsp),%rsi
  41410c:	sub    %rdx,%rsi
  41410f:	lea    -0x4(%rsi),%rdi
  414113:	cmp    $0x8,%rdi
  414117:	jbe    4141d8 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xcb8>
  41411d:	lea    0x4(%rdx),%rcx
  414121:	mov    %rbx,%r8
  414124:	sub    %rcx,%r8
  414127:	xor    %ecx,%ecx
  414129:	cmp    $0x8,%r8
  41412d:	ja     414160 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc40>
  41412f:	nop
  414130:	mov    (%rdx,%rcx,1),%edi
  414133:	mov    %edi,(%rbx,%rcx,1)
  414136:	add    $0x4,%rcx
  41413a:	cmp    %rsi,%rcx
  41413d:	jne    414130 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc10>
  41413f:	lea    (%rbx,%rsi,1),%rcx
  414143:	mov    0x4(%rax),%edx
  414146:	mov    $0x1,%r13d
  41414c:	sub    %r12,%rcx
  41414f:	sar    $0x2,%rcx
  414153:	jmpq   413880 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x360>
  414158:	nopl   0x0(%rax,%rax,1)
  414160:	shr    $0x2,%rdi
  414164:	add    $0x1,%rdi
  414168:	mov    %rdi,%r8
  41416b:	shr    $0x2,%r8
  41416f:	shl    $0x4,%r8
  414173:	nopl   0x0(%rax,%rax,1)
  414178:	movdqu (%rdx,%rcx,1),%xmm2
  41417d:	movups %xmm2,(%rbx,%rcx,1)
  414181:	add    $0x10,%rcx
  414185:	cmp    %r8,%rcx
  414188:	jne    414178 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc58>
  41418a:	test   $0x3,%dil
  41418e:	je     41413f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc1f>
  414190:	and    $0xfffffffffffffffc,%rdi
  414194:	mov    (%rsp),%r8
  414198:	lea    0x0(,%rdi,4),%rcx
  4141a0:	add    %rcx,%rdx
  4141a3:	add    %rbx,%rcx
  4141a6:	mov    (%rdx),%edi
  4141a8:	mov    %edi,(%rcx)
  4141aa:	lea    0x4(%rdx),%rdi
  4141ae:	cmp    %rdi,%r8
  4141b1:	je     41413f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc1f>
  4141b3:	mov    0x4(%rdx),%edi
  4141b6:	mov    %edi,0x4(%rcx)
  4141b9:	lea    0x8(%rdx),%rdi
  4141bd:	cmp    %rdi,%r8
  4141c0:	je     41413f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc1f>
  4141c6:	mov    0x8(%rdx),%edx
  4141c9:	mov    %edx,0x8(%rcx)
  4141cc:	jmpq   41413f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc1f>
  4141d1:	nopl   0x0(%rax)
  4141d8:	xor    %ecx,%ecx
  4141da:	jmpq   414130 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xc10>
  4141df:	mov    0x5c(%rsp),%ebx
  4141e3:	xor    %ecx,%ecx
  4141e5:	movdqu 0x678(%r15),%xmm4
  4141ee:	mov    0x68(%rsp),%rdx
  4141f3:	mov    %eax,%edi
  4141f5:	sub    %ebx,%edi
  4141f7:	cmpb   $0x0,0x1dd8(%r15)
  4141ff:	movq   %rdi,%xmm0
  414204:	movq   %rdx,%xmm3
  414209:	punpcklqdq %xmm3,%xmm0
  41420d:	paddq  %xmm4,%xmm0
  414211:	movups %xmm0,0x678(%r15)
  414219:	jne    414225 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd05>
  41421b:	cmpb   $0x0,0x124(%r15)
  414223:	je     414269 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd49>
  414225:	mov    0x1dd0(%r15),%rax
  41422c:	test   %rax,%rax
  41422f:	je     414234 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd14>
  414231:	add    %rdx,(%rax)
  414234:	add    $0x78,%rsp
  414238:	mov    %ecx,%eax
  41423a:	pop    %rbx
  41423b:	pop    %rbp
  41423c:	pop    %r12
  41423e:	pop    %r13
  414240:	pop    %r14
  414242:	pop    %r15
  414244:	retq   
  414245:	cmpb   $0x0,(%rdx,%rsi,1)
  414249:	jne    4140aa <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb8a>
  41424f:	jmpq   4140a2 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb82>
  414254:	addq   $0x1,0x698(%r15)
  41425c:	addl   $0x1,0x1c54(%r15)
  414264:	jmpq   414049 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xb29>
  414269:	add    %rdx,0x688(%r15)
  414270:	jmp    414225 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xd05>
  414272:	mov    %r9,%rbx
  414275:	jne    413a5f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x53f>
  41427b:	mov    (%r10),%edx
  41427e:	mov    %edx,(%rdi)
  414280:	mov    0x4(%r9),%esi
  414284:	jmpq   413a5f <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x53f>
  414289:	mov    %r9,%rbx
  41428c:	jne    413da7 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x887>
  414292:	mov    (%r10),%edx
  414295:	mov    %edx,(%rdi)
  414297:	mov    0x4(%r9),%esi
  41429b:	jmpq   413da7 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x887>
  4142a0:	mov    (%rdx),%edi
  4142a2:	mov    0x160(%r15),%rdx
  4142a9:	prefetcht2 (%rdx,%rdi,4)
  4142ad:	jmpq   413fec <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xacc>
  4142b2:	mov    $0x3,%esi
  4142b7:	jmpq   413cfc <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x7dc>
  4142bc:	mov    $0x3,%edx
  4142c1:	jmpq   4139ac <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x48c>
  4142c6:	mov    (%r10),%r11d
  4142c9:	mov    0x160(%r15),%r10
  4142d0:	prefetcht2 (%r10,%r11,4)
  4142d5:	jmpq   413c01 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x6e1>
  4142da:	xor    %r13d,%r13d
  4142dd:	jmpq   413880 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x360>
  4142e2:	mov    %r8,0x20(%rsp)
  4142e7:	mov    0x48(%rsp),%r14
  4142ec:	mov    %r10,%rdx
  4142ef:	mov    %r11,%rbx
  4142f2:	movb   $0x1,0x2d1(%r15)
  4142fa:	mov    0x50(%rsp),%r12
  4142ff:	mov    %r13d,0x2d4(%r15)
  414306:	mov    0x178(%r15),%r9
  41430d:	jmpq   4140f7 <_ZN6tactix16kernel_propagateERNS_6KernelEj+0xbd7>
  414312:	mov    $0x1,%r13d
  414318:	jmpq   41386d <_ZN6tactix16kernel_propagateERNS_6KernelEj+0x34d>

Disassembly of section .fini:
