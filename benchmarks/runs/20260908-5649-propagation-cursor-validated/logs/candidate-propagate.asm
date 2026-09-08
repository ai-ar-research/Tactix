
/home/hwu/SAT-solving/build/pin-propagation-cursor-repair-5647-release/tactix:     file format elf64-x86-64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004136f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)>:
  4136f0:	41 57                	push   %r15
  4136f2:	49 89 ff             	mov    %rdi,%r15
  4136f5:	41 56                	push   %r14
  4136f7:	41 55                	push   %r13
  4136f9:	41 54                	push   %r12
  4136fb:	55                   	push   %rbp
  4136fc:	53                   	push   %rbx
  4136fd:	48 83 ec 78          	sub    $0x78,%rsp
  413701:	8b 87 f4 00 00 00    	mov    0xf4(%rdi),%eax
  413707:	44 8b a7 f0 00 00 00 	mov    0xf0(%rdi),%r12d
  41370e:	83 fe ff             	cmp    $0xffffffff,%esi
  413711:	0f 84 d6 00 00 00    	je     4137ed <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xfd>
  413717:	89 f5                	mov    %esi,%ebp
  413719:	44 39 e0             	cmp    %r12d,%eax
  41371c:	0f 82 bd 00 00 00    	jb     4137df <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xef>
  413722:	41 c7 87 f4 00 00 00 	movl   $0xffffffff,0xf4(%r15)
  413729:	ff ff ff ff 
  41372d:	44 89 e3             	mov    %r12d,%ebx
  413730:	48 c7 44 24 68 00 00 	movq   $0x0,0x68(%rsp)
  413737:	00 00 
  413739:	eb 29                	jmp    413764 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x74>
  41373b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  413740:	49 8b 97 d0 00 00 00 	mov    0xd0(%r15),%rdx
  413747:	48 8d 4c 24 68       	lea    0x68(%rsp),%rcx
  41374c:	4c 89 ff             	mov    %r15,%rdi
  41374f:	83 c3 01             	add    $0x1,%ebx
  413752:	8b 34 82             	mov    (%rdx,%rax,4),%esi
  413755:	89 ea                	mov    %ebp,%edx
  413757:	e8 d4 f0 ff ff       	callq  412830 <bool tactix::propagate_literal<false, false, true>(tactix::Kernel&, unsigned int, unsigned int, unsigned long&)>
  41375c:	84 c0                	test   %al,%al
  41375e:	0f 85 5d 0e 00 00    	jne    4145c1 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xed1>
  413764:	89 d8                	mov    %ebx,%eax
  413766:	49 3b 87 e8 00 00 00 	cmp    0xe8(%r15),%rax
  41376d:	72 d1                	jb     413740 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x50>
  41376f:	45 31 c0             	xor    %r8d,%r8d
  413772:	48 8b 54 24 68       	mov    0x68(%rsp),%rdx
  413777:	89 d8                	mov    %ebx,%eax
  413779:	41 89 9f f0 00 00 00 	mov    %ebx,0xf0(%r15)
  413780:	f3 41 0f 6f af 78 06 	movdqu 0x678(%r15),%xmm5
  413787:	00 00 
  413789:	44 29 e0             	sub    %r12d,%eax
  41378c:	41 80 bf d8 1d 00 00 	cmpb   $0x0,0x1dd8(%r15)
  413793:	00 
  413794:	66 48 0f 6e c0       	movq   %rax,%xmm0
  413799:	66 48 0f 6e e2       	movq   %rdx,%xmm4
  41379e:	66 0f 6c c4          	punpcklqdq %xmm4,%xmm0
  4137a2:	66 0f d4 c5          	paddq  %xmm5,%xmm0
  4137a6:	41 0f 11 87 78 06 00 	movups %xmm0,0x678(%r15)
  4137ad:	00 
  4137ae:	75 0e                	jne    4137be <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xce>
  4137b0:	41 80 bf 24 01 00 00 	cmpb   $0x0,0x124(%r15)
  4137b7:	00 
  4137b8:	0f 84 2d 0e 00 00    	je     4145eb <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xefb>
  4137be:	49 8b 87 d0 1d 00 00 	mov    0x1dd0(%r15),%rax
  4137c5:	48 85 c0             	test   %rax,%rax
  4137c8:	74 03                	je     4137cd <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdd>
  4137ca:	48 01 10             	add    %rdx,(%rax)
  4137cd:	48 83 c4 78          	add    $0x78,%rsp
  4137d1:	44 89 c0             	mov    %r8d,%eax
  4137d4:	5b                   	pop    %rbx
  4137d5:	5d                   	pop    %rbp
  4137d6:	41 5c                	pop    %r12
  4137d8:	41 5d                	pop    %r13
  4137da:	41 5e                	pop    %r14
  4137dc:	41 5f                	pop    %r15
  4137de:	c3                   	retq   
  4137df:	89 87 f0 00 00 00    	mov    %eax,0xf0(%rdi)
  4137e5:	41 89 c4             	mov    %eax,%r12d
  4137e8:	e9 35 ff ff ff       	jmpq   413722 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x32>
  4137ed:	44 39 e0             	cmp    %r12d,%eax
  4137f0:	0f 82 e7 0d 00 00    	jb     4145dd <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xeed>
  4137f6:	41 c7 87 f4 00 00 00 	movl   $0xffffffff,0xf4(%r15)
  4137fd:	ff ff ff ff 
  413801:	44 89 e5             	mov    %r12d,%ebp
  413804:	48 c7 44 24 50 00 00 	movq   $0x0,0x50(%rsp)
  41380b:	00 00 
  41380d:	44 89 64 24 5c       	mov    %r12d,0x5c(%rsp)
  413812:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  413818:	89 e8                	mov    %ebp,%eax
  41381a:	49 3b 87 e8 00 00 00 	cmp    0xe8(%r15),%rax
  413821:	0f 83 2e 0e 00 00    	jae    414655 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf65>
  413827:	49 8b 97 d0 00 00 00 	mov    0xd0(%r15),%rdx
  41382e:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  413835:	83 c5 01             	add    $0x1,%ebp
  413838:	4d 8b 87 60 01 00 00 	mov    0x160(%r15),%r8
  41383f:	49 8b 9f 30 01 00 00 	mov    0x130(%r15),%rbx
  413846:	8b 3c 82             	mov    (%rdx,%rax,4),%edi
  413849:	4d 8b b7 88 00 00 00 	mov    0x88(%r15),%r14
  413850:	48 89 5c 24 08       	mov    %rbx,0x8(%rsp)
  413855:	89 f8                	mov    %edi,%eax
  413857:	d1 ef                	shr    %edi
  413859:	83 f0 01             	xor    $0x1,%eax
  41385c:	48 c1 e7 04          	shl    $0x4,%rdi
  413860:	49 03 bf a0 00 00 00 	add    0xa0(%r15),%rdi
  413867:	89 44 24 04          	mov    %eax,0x4(%rsp)
  41386b:	48 c1 e0 04          	shl    $0x4,%rax
  41386f:	48 89 44 24 40       	mov    %rax,0x40(%rsp)
  413874:	4c 01 d0             	add    %r10,%rax
  413877:	44 8b 08             	mov    (%rax),%r9d
  41387a:	8b 50 04             	mov    0x4(%rax),%edx
  41387d:	48 89 7c 24 18       	mov    %rdi,0x18(%rsp)
  413882:	48 89 d1             	mov    %rdx,%rcx
  413885:	4c 01 ca             	add    %r9,%rdx
  413888:	4a 8d 34 8d 00 00 00 	lea    0x0(,%r9,4),%rsi
  41388f:	00 
  413890:	48 c1 e2 02          	shl    $0x2,%rdx
  413894:	4d 8d 24 30          	lea    (%r8,%rsi,1),%r12
  413898:	4d 8d 1c 10          	lea    (%r8,%rdx,1),%r11
  41389c:	48 29 f2             	sub    %rsi,%rdx
  41389f:	48 c1 fa 02          	sar    $0x2,%rdx
  4138a3:	48 8d 72 1f          	lea    0x1f(%rdx),%rsi
  4138a7:	48 c1 ee 05          	shr    $0x5,%rsi
  4138ab:	48 8d 7e 01          	lea    0x1(%rsi),%rdi
  4138af:	48 89 7c 24 10       	mov    %rdi,0x10(%rsp)
  4138b4:	4d 39 dc             	cmp    %r11,%r12
  4138b7:	0f 84 a7 0d 00 00    	je     414664 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf74>
  4138bd:	4c 89 64 24 48       	mov    %r12,0x48(%rsp)
  4138c2:	4c 89 e3             	mov    %r12,%rbx
  4138c5:	4c 89 e2             	mov    %r12,%rdx
  4138c8:	49 89 fc             	mov    %rdi,%r12
  4138cb:	89 6c 24 58          	mov    %ebp,0x58(%rsp)
  4138cf:	e9 55 01 00 00       	jmpq   413a29 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x339>
  4138d4:	0f 1f 40 00          	nopl   0x0(%rax)
  4138d8:	48 83 c2 04          	add    $0x4,%rdx
  4138dc:	48 83 c3 04          	add    $0x4,%rbx
  4138e0:	45 84 c0             	test   %r8b,%r8b
  4138e3:	0f 8f 37 01 00 00    	jg     413a20 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  4138e9:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  4138f0:	0f 85 2a 0b 00 00    	jne    414420 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xd30>
  4138f6:	89 cf                	mov    %ecx,%edi
  4138f8:	48 8b 6c 24 18       	mov    0x18(%rsp),%rbp
  4138fd:	c1 e8 02             	shr    $0x2,%eax
  413900:	83 f7 01             	xor    $0x1,%edi
  413903:	49 89 f9             	mov    %rdi,%r9
  413906:	44 8b 45 00          	mov    0x0(%rbp),%r8d
  41390a:	49 c1 e1 04          	shl    $0x4,%r9
  41390e:	4d 01 ca             	add    %r9,%r10
  413911:	41 80 bf 6d 1e 00 00 	cmpb   $0x0,0x1e6d(%r15)
  413918:	00 
  413919:	74 13                	je     41392e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x23e>
  41391b:	41 80 bf 6e 1e 00 00 	cmpb   $0x0,0x1e6e(%r15)
  413922:	00 
  413923:	74 09                	je     41392e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x23e>
  413925:	45 85 c0             	test   %r8d,%r8d
  413928:	0f 85 ea 04 00 00    	jne    413e18 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x728>
  41392e:	41 8b 6a 04          	mov    0x4(%r10),%ebp
  413932:	85 ed                	test   %ebp,%ebp
  413934:	0f 85 ae 03 00 00    	jne    413ce8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5f8>
  41393a:	4d 8b 8f 88 00 00 00 	mov    0x88(%r15),%r9
  413941:	44 8b 54 24 04       	mov    0x4(%rsp),%r10d
  413946:	66 41 0f 6e c0       	movd   %r8d,%xmm0
  41394b:	41 c6 04 31 01       	movb   $0x1,(%r9,%rsi,1)
  413950:	49 8b b7 88 00 00 00 	mov    0x88(%r15),%rsi
  413957:	44 0f b7 0d 81 ca 0f 	movzwl 0xfca81(%rip),%r9d        # 5103e0 <tactix::BinaryIdMap::EMPTY+0x18>
  41395e:	00 
  41395f:	c6 04 3e ff          	movb   $0xff,(%rsi,%rdi,1)
  413963:	49 8b bf e8 00 00 00 	mov    0xe8(%r15),%rdi
  41396a:	48 89 c6             	mov    %rax,%rsi
  41396d:	48 c1 e6 04          	shl    $0x4,%rsi
  413971:	49 03 b7 a0 00 00 00 	add    0xa0(%r15),%rsi
  413978:	66 0f 6e d7          	movd   %edi,%xmm2
  41397c:	44 89 56 08          	mov    %r10d,0x8(%rsi)
  413980:	66 0f 62 c2          	punpckldq %xmm2,%xmm0
  413984:	66 44 89 4e 0c       	mov    %r9w,0xc(%rsi)
  413989:	66 0f d6 06          	movq   %xmm0,(%rsi)
  41398d:	48 8d 77 01          	lea    0x1(%rdi),%rsi
  413991:	49 89 b7 e8 00 00 00 	mov    %rsi,0xe8(%r15)
  413998:	49 8b b7 d0 00 00 00 	mov    0xd0(%r15),%rsi
  41399f:	89 0c be             	mov    %ecx,(%rsi,%rdi,4)
  4139a2:	45 85 c0             	test   %r8d,%r8d
  4139a5:	75 10                	jne    4139b7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x2c7>
  4139a7:	49 83 87 98 06 00 00 	addq   $0x1,0x698(%r15)
  4139ae:	01 
  4139af:	41 83 87 54 1c 00 00 	addl   $0x1,0x1c54(%r15)
  4139b6:	01 
  4139b7:	49 8b b7 e0 1d 00 00 	mov    0x1de0(%r15),%rsi
  4139be:	48 85 f6             	test   %rsi,%rsi
  4139c1:	74 2d                	je     4139f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x300>
  4139c3:	41 80 bf e8 1d 00 00 	cmpb   $0x0,0x1de8(%r15)
  4139ca:	00 
  4139cb:	75 23                	jne    4139f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x300>
  4139cd:	41 80 bf 20 02 00 00 	cmpb   $0x0,0x220(%r15)
  4139d4:	00 
  4139d5:	75 19                	jne    4139f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x300>
  4139d7:	83 e1 01             	and    $0x1,%ecx
  4139da:	48 8b 36             	mov    (%rsi),%rsi
  4139dd:	80 f9 01             	cmp    $0x1,%cl
  4139e0:	19 c9                	sbb    %ecx,%ecx
  4139e2:	83 e1 02             	and    $0x2,%ecx
  4139e5:	83 e9 01             	sub    $0x1,%ecx
  4139e8:	88 0c 06             	mov    %cl,(%rsi,%rax,1)
  4139eb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  4139f0:	49 8b b7 48 1d 00 00 	mov    0x1d48(%r15),%rsi
  4139f7:	49 8b 8f 50 1d 00 00 	mov    0x1d50(%r15),%rcx
  4139fe:	41 83 af 1c 01 00 00 	subl   $0x1,0x11c(%r15)
  413a05:	01 
  413a06:	48 29 f1             	sub    %rsi,%rcx
  413a09:	48 39 c8             	cmp    %rcx,%rax
  413a0c:	73 06                	jae    413a14 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x324>
  413a0e:	80 3c 06 00          	cmpb   $0x0,(%rsi,%rax,1)
  413a12:	75 08                	jne    413a1c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x32c>
  413a14:	41 83 af 20 01 00 00 	subl   $0x1,0x120(%r15)
  413a1b:	01 
  413a1c:	49 83 c4 01          	add    $0x1,%r12
  413a20:	49 39 d3             	cmp    %rdx,%r11
  413a23:	0f 84 97 00 00 00    	je     413ac0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x3d0>
  413a29:	8b 02                	mov    (%rdx),%eax
  413a2b:	89 c1                	mov    %eax,%ecx
  413a2d:	89 03                	mov    %eax,(%rbx)
  413a2f:	d1 e9                	shr    %ecx
  413a31:	89 ce                	mov    %ecx,%esi
  413a33:	45 0f b6 04 36       	movzbl (%r14,%rsi,1),%r8d
  413a38:	a8 01                	test   $0x1,%al
  413a3a:	0f 85 98 fe ff ff    	jne    4138d8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x1e8>
  413a40:	44 8b 6a 04          	mov    0x4(%rdx),%r13d
  413a44:	4c 8d 52 08          	lea    0x8(%rdx),%r10
  413a48:	48 8d 7b 08          	lea    0x8(%rbx),%rdi
  413a4c:	44 89 6b 04          	mov    %r13d,0x4(%rbx)
  413a50:	45 84 c0             	test   %r8b,%r8b
  413a53:	0f 8f 6f 02 00 00    	jg     413cc8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5d8>
  413a59:	48 8b 44 24 08       	mov    0x8(%rsp),%rax
  413a5e:	44 89 e9             	mov    %r13d,%ecx
  413a61:	4d 8d 44 24 01       	lea    0x1(%r12),%r8
  413a66:	48 c1 e1 02          	shl    $0x2,%rcx
  413a6a:	48 8d 14 08          	lea    (%rax,%rcx,1),%rdx
  413a6e:	f6 42 04 01          	testb  $0x1,0x4(%rdx)
  413a72:	0f 85 60 02 00 00    	jne    413cd8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5e8>
  413a78:	8b 74 24 04          	mov    0x4(%rsp),%esi
  413a7c:	8b 42 14             	mov    0x14(%rdx),%eax
  413a7f:	33 42 18             	xor    0x18(%rdx),%eax
  413a82:	31 c6                	xor    %eax,%esi
  413a84:	89 74 24 10          	mov    %esi,0x10(%rsp)
  413a88:	89 f0                	mov    %esi,%eax
  413a8a:	48 89 74 24 28       	mov    %rsi,0x28(%rsp)
  413a8f:	41 0f b6 34 36       	movzbl (%r14,%rsi,1),%esi
  413a94:	40 88 74 24 20       	mov    %sil,0x20(%rsp)
  413a99:	40 84 f6             	test   %sil,%sil
  413a9c:	0f 8e 5e 02 00 00    	jle    413d00 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x610>
  413aa2:	01 c0                	add    %eax,%eax
  413aa4:	4c 89 d2             	mov    %r10,%rdx
  413aa7:	4d 89 c4             	mov    %r8,%r12
  413aaa:	89 03                	mov    %eax,(%rbx)
  413aac:	48 89 fb             	mov    %rdi,%rbx
  413aaf:	49 39 d3             	cmp    %rdx,%r11
  413ab2:	0f 85 71 ff ff ff    	jne    413a29 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x339>
  413ab8:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  413abf:	00 
  413ac0:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  413ac7:	8b 6c 24 58          	mov    0x58(%rsp),%ebp
  413acb:	4c 89 64 24 10       	mov    %r12,0x10(%rsp)
  413ad0:	45 31 ed             	xor    %r13d,%r13d
  413ad3:	48 8b 44 24 40       	mov    0x40(%rsp),%rax
  413ad8:	4c 8b 64 24 48       	mov    0x48(%rsp),%r12
  413add:	4c 01 d0             	add    %r10,%rax
  413ae0:	4c 29 e3             	sub    %r12,%rbx
  413ae3:	8b 48 04             	mov    0x4(%rax),%ecx
  413ae6:	48 89 da             	mov    %rbx,%rdx
  413ae9:	48 c1 fa 02          	sar    $0x2,%rdx
  413aed:	0f 1f 00             	nopl   (%rax)
  413af0:	8b 70 0c             	mov    0xc(%rax),%esi
  413af3:	49 8b bf 18 02 00 00 	mov    0x218(%r15),%rdi
  413afa:	29 d1                	sub    %edx,%ecx
  413afc:	49 01 8f 90 01 00 00 	add    %rcx,0x190(%r15)
  413b03:	89 50 04             	mov    %edx,0x4(%rax)
  413b06:	89 70 0c             	mov    %esi,0xc(%rax)
  413b09:	48 85 ff             	test   %rdi,%rdi
  413b0c:	0f 84 20 01 00 00    	je     413c32 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x542>
  413b12:	89 6c 24 08          	mov    %ebp,0x8(%rsp)
  413b16:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  413b1d:	45 31 f6             	xor    %r14d,%r14d
  413b20:	44 88 6c 24 18       	mov    %r13b,0x18(%rsp)
  413b25:	eb 35                	jmp    413b5c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x46c>
  413b27:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  413b2e:	00 00 
  413b30:	8b 03                	mov    (%rbx),%eax
  413b32:	66 0f 6e cd          	movd   %ebp,%xmm1
  413b36:	49 83 c6 03          	add    $0x3,%r14
  413b3a:	66 0f 62 c8          	punpckldq %xmm0,%xmm1
  413b3e:	48 01 d0             	add    %rdx,%rax
  413b41:	66 41 0f d6 0c 83    	movq   %xmm1,(%r11,%rax,4)
  413b47:	83 43 04 02          	addl   $0x2,0x4(%rbx)
  413b4b:	49 83 af 90 01 00 00 	subq   $0x2,0x190(%r15)
  413b52:	02 
  413b53:	49 39 fe             	cmp    %rdi,%r14
  413b56:	0f 83 cc 00 00 00    	jae    413c28 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x538>
  413b5c:	49 8b 87 00 02 00 00 	mov    0x200(%r15),%rax
  413b63:	4a 8d 34 b5 00 00 00 	lea    0x0(,%r14,4),%rsi
  413b6a:	00 
  413b6b:	46 8b 04 b0          	mov    (%rax,%r14,4),%r8d
  413b6f:	8b 6c 30 04          	mov    0x4(%rax,%rsi,1),%ebp
  413b73:	66 0f 6e 44 30 08    	movd   0x8(%rax,%rsi,1),%xmm0
  413b79:	49 c1 e0 04          	shl    $0x4,%r8
  413b7d:	01 ed                	add    %ebp,%ebp
  413b7f:	4b 8d 1c 02          	lea    (%r10,%r8,1),%rbx
  413b83:	44 8b 4b 08          	mov    0x8(%rbx),%r9d
  413b87:	8b 53 04             	mov    0x4(%rbx),%edx
  413b8a:	44 89 c8             	mov    %r9d,%eax
  413b8d:	29 d0                	sub    %edx,%eax
  413b8f:	83 f8 01             	cmp    $0x1,%eax
  413b92:	77 9c                	ja     413b30 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x440>
  413b94:	8b 43 0c             	mov    0xc(%rbx),%eax
  413b97:	89 d6                	mov    %edx,%esi
  413b99:	83 e6 01             	and    $0x1,%esi
  413b9c:	41 89 c4             	mov    %eax,%r12d
  413b9f:	f7 d0                	not    %eax
  413ba1:	41 83 e4 fe          	and    $0xfffffffe,%r12d
  413ba5:	83 e0 01             	and    $0x1,%eax
  413ba8:	88 44 24 04          	mov    %al,0x4(%rsp)
  413bac:	41 09 f4             	or     %esi,%r12d
  413baf:	41 39 d1             	cmp    %edx,%r9d
  413bb2:	0f 84 88 04 00 00    	je     414040 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x950>
  413bb8:	49 8b 87 90 01 00 00 	mov    0x190(%r15),%rax
  413bbf:	48 8d 78 ff          	lea    -0x1(%rax),%rdi
  413bc3:	48 89 d8             	mov    %rbx,%rax
  413bc6:	8d 72 01             	lea    0x1(%rdx),%esi
  413bc9:	03 10                	add    (%rax),%edx
  413bcb:	49 89 bf 90 01 00 00 	mov    %rdi,0x190(%r15)
  413bd2:	89 70 04             	mov    %esi,0x4(%rax)
  413bd5:	41 89 2c 93          	mov    %ebp,(%r11,%rdx,4)
  413bd9:	8b 70 04             	mov    0x4(%rax),%esi
  413bdc:	3b 70 08             	cmp    0x8(%rax),%esi
  413bdf:	0f 84 eb 02 00 00    	je     413ed0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x7e0>
  413be5:	48 83 ef 01          	sub    $0x1,%rdi
  413be9:	8d 56 01             	lea    0x1(%rsi),%edx
  413bec:	03 30                	add    (%rax),%esi
  413bee:	41 83 e4 fe          	and    $0xfffffffe,%r12d
  413bf2:	49 83 c6 03          	add    $0x3,%r14
  413bf6:	49 89 bf 90 01 00 00 	mov    %rdi,0x190(%r15)
  413bfd:	49 8b bf 18 02 00 00 	mov    0x218(%r15),%rdi
  413c04:	89 50 04             	mov    %edx,0x4(%rax)
  413c07:	0f b6 44 24 04       	movzbl 0x4(%rsp),%eax
  413c0c:	66 41 0f 7e 04 b3    	movd   %xmm0,(%r11,%rsi,4)
  413c12:	83 f0 01             	xor    $0x1,%eax
  413c15:	0f b6 c0             	movzbl %al,%eax
  413c18:	41 09 c4             	or     %eax,%r12d
  413c1b:	44 89 63 0c          	mov    %r12d,0xc(%rbx)
  413c1f:	49 39 fe             	cmp    %rdi,%r14
  413c22:	0f 82 34 ff ff ff    	jb     413b5c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x46c>
  413c28:	8b 6c 24 08          	mov    0x8(%rsp),%ebp
  413c2c:	44 0f b6 6c 24 18    	movzbl 0x18(%rsp),%r13d
  413c32:	49 c7 87 18 02 00 00 	movq   $0x0,0x218(%r15)
  413c39:	00 00 00 00 
  413c3d:	48 8b 5c 24 10       	mov    0x10(%rsp),%rbx
  413c42:	48 01 5c 24 50       	add    %rbx,0x50(%rsp)
  413c47:	45 84 ed             	test   %r13b,%r13b
  413c4a:	0f 84 c8 fb ff ff    	je     413818 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x128>
  413c50:	44 8b 64 24 5c       	mov    0x5c(%rsp),%r12d
  413c55:	89 e8                	mov    %ebp,%eax
  413c57:	45 89 e8             	mov    %r13d,%r8d
  413c5a:	41 89 87 f0 00 00 00 	mov    %eax,0xf0(%r15)
  413c61:	44 29 e0             	sub    %r12d,%eax
  413c64:	41 80 bf d8 1d 00 00 	cmpb   $0x0,0x1dd8(%r15)
  413c6b:	00 
  413c6c:	66 48 0f 6e c0       	movq   %rax,%xmm0
  413c71:	48 8b 44 24 50       	mov    0x50(%rsp),%rax
  413c76:	66 48 0f 6e f8       	movq   %rax,%xmm7
  413c7b:	66 0f 6c c7          	punpcklqdq %xmm7,%xmm0
  413c7f:	f3 41 0f 6f bf 78 06 	movdqu 0x678(%r15),%xmm7
  413c86:	00 00 
  413c88:	66 0f d4 c7          	paddq  %xmm7,%xmm0
  413c8c:	41 0f 11 87 78 06 00 	movups %xmm0,0x678(%r15)
  413c93:	00 
  413c94:	75 11                	jne    413ca7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5b7>
  413c96:	41 80 bf 24 01 00 00 	cmpb   $0x0,0x124(%r15)
  413c9d:	00 
  413c9e:	75 07                	jne    413ca7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5b7>
  413ca0:	49 01 87 88 06 00 00 	add    %rax,0x688(%r15)
  413ca7:	49 8b 87 d0 1d 00 00 	mov    0x1dd0(%r15),%rax
  413cae:	48 85 c0             	test   %rax,%rax
  413cb1:	0f 84 16 fb ff ff    	je     4137cd <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdd>
  413cb7:	48 8b 5c 24 50       	mov    0x50(%rsp),%rbx
  413cbc:	48 01 18             	add    %rbx,(%rax)
  413cbf:	e9 09 fb ff ff       	jmpq   4137cd <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdd>
  413cc4:	0f 1f 40 00          	nopl   0x0(%rax)
  413cc8:	4c 89 d2             	mov    %r10,%rdx
  413ccb:	48 89 fb             	mov    %rdi,%rbx
  413cce:	e9 4d fd ff ff       	jmpq   413a20 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  413cd3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  413cd8:	4d 89 c4             	mov    %r8,%r12
  413cdb:	4c 89 d2             	mov    %r10,%rdx
  413cde:	e9 3d fd ff ff       	jmpq   413a20 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  413ce3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  413ce8:	45 8b 12             	mov    (%r10),%r10d
  413ceb:	4d 8b 8f 60 01 00 00 	mov    0x160(%r15),%r9
  413cf2:	43 0f 18 1c 91       	prefetcht2 (%r9,%r10,4)
  413cf7:	e9 3e fc ff ff       	jmpq   41393a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x24a>
  413cfc:	0f 1f 40 00          	nopl   0x0(%rax)
  413d00:	8b 32                	mov    (%rdx),%esi
  413d02:	8b 42 08             	mov    0x8(%rdx),%eax
  413d05:	48 8d 6a 14          	lea    0x14(%rdx),%rbp
  413d09:	48 89 6c 24 30       	mov    %rbp,0x30(%rsp)
  413d0e:	48 c1 e6 02          	shl    $0x2,%rsi
  413d12:	48 c1 e0 02          	shl    $0x2,%rax
  413d16:	4c 8d 4c 35 00       	lea    0x0(%rbp,%rsi,1),%r9
  413d1b:	48 01 c5             	add    %rax,%rbp
  413d1e:	48 89 6c 24 38       	mov    %rbp,0x38(%rsp)
  413d23:	48 39 c6             	cmp    %rax,%rsi
  413d26:	0f 84 ec 04 00 00    	je     414218 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb28>
  413d2c:	48 89 e8             	mov    %rbp,%rax
  413d2f:	eb 14                	jmp    413d45 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x655>
  413d31:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  413d38:	48 83 c0 04          	add    $0x4,%rax
  413d3c:	49 39 c1             	cmp    %rax,%r9
  413d3f:	0f 84 73 04 00 00    	je     4141b8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xac8>
  413d45:	8b 30                	mov    (%rax),%esi
  413d47:	41 80 3c 36 00       	cmpb   $0x0,(%r14,%rsi,1)
  413d4c:	48 89 f5             	mov    %rsi,%rbp
  413d4f:	78 e7                	js     413d38 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x648>
  413d51:	48 8b 7c 24 30       	mov    0x30(%rsp),%rdi
  413d56:	48 89 c1             	mov    %rax,%rcx
  413d59:	49 8b b7 08 02 00 00 	mov    0x208(%r15),%rsi
  413d60:	89 6a 18             	mov    %ebp,0x18(%rdx)
  413d63:	48 29 f9             	sub    %rdi,%rcx
  413d66:	8b 7c 24 10          	mov    0x10(%rsp),%edi
  413d6a:	48 c1 f9 02          	sar    $0x2,%rcx
  413d6e:	89 7a 14             	mov    %edi,0x14(%rdx)
  413d71:	8b 7c 24 04          	mov    0x4(%rsp),%edi
  413d75:	89 4a 08             	mov    %ecx,0x8(%rdx)
  413d78:	49 8b 97 00 02 00 00 	mov    0x200(%r15),%rdx
  413d7f:	89 38                	mov    %edi,(%rax)
  413d81:	49 8b 8f 18 02 00 00 	mov    0x218(%r15),%rcx
  413d88:	48 89 f0             	mov    %rsi,%rax
  413d8b:	48 29 d0             	sub    %rdx,%rax
  413d8e:	48 8d 79 03          	lea    0x3(%rcx),%rdi
  413d92:	48 c1 f8 02          	sar    $0x2,%rax
  413d96:	48 39 f8             	cmp    %rdi,%rax
  413d99:	73 4b                	jae    413de6 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x6f6>
  413d9b:	b9 03 00 00 00       	mov    $0x3,%ecx
  413da0:	48 01 c0             	add    %rax,%rax
  413da3:	48 39 d6             	cmp    %rdx,%rsi
  413da6:	4c 89 54 24 28       	mov    %r10,0x28(%rsp)
  413dab:	48 0f 44 c1          	cmove  %rcx,%rax
  413daf:	4c 89 5c 24 20       	mov    %r11,0x20(%rsp)
  413db4:	48 39 c7             	cmp    %rax,%rdi
  413db7:	48 0f 43 c7          	cmovae %rdi,%rax
  413dbb:	49 8d bf 00 02 00 00 	lea    0x200(%r15),%rdi
  413dc2:	48 89 c6             	mov    %rax,%rsi
  413dc5:	e8 86 10 01 00       	callq  424e50 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  413dca:	4c 8b 54 24 28       	mov    0x28(%rsp),%r10
  413dcf:	4c 8b 5c 24 20       	mov    0x20(%rsp),%r11
  413dd4:	49 8b 8f 18 02 00 00 	mov    0x218(%r15),%rcx
  413ddb:	49 8b 97 00 02 00 00 	mov    0x200(%r15),%rdx
  413de2:	48 8d 79 03          	lea    0x3(%rcx),%rdi
  413de6:	89 2c 8a             	mov    %ebp,(%rdx,%rcx,4)
  413de9:	48 8d 04 8d 00 00 00 	lea    0x0(,%rcx,4),%rax
  413df0:	00 
  413df1:	8b 4c 24 10          	mov    0x10(%rsp),%ecx
  413df5:	49 83 c4 02          	add    $0x2,%r12
  413df9:	89 4c 02 04          	mov    %ecx,0x4(%rdx,%rax,1)
  413dfd:	49 89 bf 18 02 00 00 	mov    %rdi,0x218(%r15)
  413e04:	44 89 6c 02 08       	mov    %r13d,0x8(%rdx,%rax,1)
  413e09:	4c 89 d2             	mov    %r10,%rdx
  413e0c:	e9 0f fc ff ff       	jmpq   413a20 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  413e11:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  413e18:	80 7d 0c 00          	cmpb   $0x0,0xc(%rbp)
  413e1c:	0f 84 0c fb ff ff    	je     41392e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x23e>
  413e22:	4d 8b 8f b8 00 00 00 	mov    0xb8(%r15),%r9
  413e29:	8b 6c 24 04          	mov    0x4(%rsp),%ebp
  413e2d:	41 89 2c 81          	mov    %ebp,(%r9,%rax,4)
  413e31:	4c 8b 4c 24 18       	mov    0x18(%rsp),%r9
  413e36:	45 8b 6a 04          	mov    0x4(%r10),%r13d
  413e3a:	49 83 87 e8 18 00 00 	addq   $0x1,0x18e8(%r15)
  413e41:	01 
  413e42:	45 8b 49 08          	mov    0x8(%r9),%r9d
  413e46:	45 85 ed             	test   %r13d,%r13d
  413e49:	0f 85 c5 07 00 00    	jne    414614 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf24>
  413e4f:	4d 8b 97 88 00 00 00 	mov    0x88(%r15),%r10
  413e56:	66 41 0f 6e c0       	movd   %r8d,%xmm0
  413e5b:	41 c6 04 32 01       	movb   $0x1,(%r10,%rsi,1)
  413e60:	49 8b b7 88 00 00 00 	mov    0x88(%r15),%rsi
  413e67:	44 0f b7 15 cb c3 0f 	movzwl 0xfc3cb(%rip),%r10d        # 51023a <_IO_stdin_used+0x223a>
  413e6e:	00 
  413e6f:	c6 04 3e ff          	movb   $0xff,(%rsi,%rdi,1)
  413e73:	49 8b bf e8 00 00 00 	mov    0xe8(%r15),%rdi
  413e7a:	48 89 c6             	mov    %rax,%rsi
  413e7d:	48 c1 e6 04          	shl    $0x4,%rsi
  413e81:	49 03 b7 a0 00 00 00 	add    0xa0(%r15),%rsi
  413e88:	66 0f 6e f7          	movd   %edi,%xmm6
  413e8c:	44 89 4e 08          	mov    %r9d,0x8(%rsi)
  413e90:	66 0f 62 c6          	punpckldq %xmm6,%xmm0
  413e94:	66 44 89 56 0c       	mov    %r10w,0xc(%rsi)
  413e99:	66 0f d6 06          	movq   %xmm0,(%rsi)
  413e9d:	48 8d 77 01          	lea    0x1(%rdi),%rsi
  413ea1:	49 89 b7 e8 00 00 00 	mov    %rsi,0xe8(%r15)
  413ea8:	49 8b b7 d0 00 00 00 	mov    0xd0(%r15),%rsi
  413eaf:	89 0c be             	mov    %ecx,(%rsi,%rdi,4)
  413eb2:	49 8b b7 e0 1d 00 00 	mov    0x1de0(%r15),%rsi
  413eb9:	48 85 f6             	test   %rsi,%rsi
  413ebc:	0f 85 01 fb ff ff    	jne    4139c3 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x2d3>
  413ec2:	e9 29 fb ff ff       	jmpq   4139f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x300>
  413ec7:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  413ece:	00 00 
  413ed0:	4d 8b 97 70 01 00 00 	mov    0x170(%r15),%r10
  413ed7:	49 8d bf 60 01 00 00 	lea    0x160(%r15),%rdi
  413ede:	4d 39 da             	cmp    %r11,%r10
  413ee1:	0f 84 90 06 00 00    	je     414577 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe87>
  413ee7:	49 8b af 68 01 00 00 	mov    0x168(%r15),%rbp
  413eee:	4d 29 da             	sub    %r11,%r10
  413ef1:	41 89 f5             	mov    %esi,%r13d
  413ef4:	4c 29 dd             	sub    %r11,%rbp
  413ef7:	44 8b 18             	mov    (%rax),%r11d
  413efa:	49 89 e9             	mov    %rbp,%r9
  413efd:	49 c1 f9 02          	sar    $0x2,%r9
  413f01:	4d 01 eb             	add    %r13,%r11
  413f04:	4c 89 ca             	mov    %r9,%rdx
  413f07:	4d 39 cb             	cmp    %r9,%r11
  413f0a:	0f 84 b0 04 00 00    	je     4143c0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcd0>
  413f10:	85 f6                	test   %esi,%esi
  413f12:	0f 84 f0 02 00 00    	je     414208 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb18>
  413f18:	8d 2c 36             	lea    (%rsi,%rsi,1),%ebp
  413f1b:	49 89 ed             	mov    %rbp,%r13
  413f1e:	49 8d 34 29          	lea    (%r9,%rbp,1),%rsi
  413f22:	49 c1 fa 02          	sar    $0x2,%r10
  413f26:	49 39 f2             	cmp    %rsi,%r10
  413f29:	73 5f                	jae    413f8a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x89a>
  413f2b:	48 83 fe 03          	cmp    $0x3,%rsi
  413f2f:	0f 86 41 07 00 00    	jbe    414676 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf86>
  413f35:	b8 04 00 00 00       	mov    $0x4,%eax
  413f3a:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  413f40:	48 01 c0             	add    %rax,%rax
  413f43:	48 8d 50 ff          	lea    -0x1(%rax),%rdx
  413f47:	48 39 f2             	cmp    %rsi,%rdx
  413f4a:	72 f4                	jb     413f40 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x850>
  413f4c:	48 89 d6             	mov    %rdx,%rsi
  413f4f:	4c 89 44 24 30       	mov    %r8,0x30(%rsp)
  413f54:	48 89 7c 24 20       	mov    %rdi,0x20(%rsp)
  413f59:	66 0f 7e 44 24 28    	movd   %xmm0,0x28(%rsp)
  413f5f:	e8 2c 0e 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  413f64:	4c 8b 44 24 30       	mov    0x30(%rsp),%r8
  413f69:	66 0f 6e 44 24 28    	movd   0x28(%rsp),%xmm0
  413f6f:	49 8b 97 68 01 00 00 	mov    0x168(%r15),%rdx
  413f76:	49 2b 97 60 01 00 00 	sub    0x160(%r15),%rdx
  413f7d:	48 c1 fa 02          	sar    $0x2,%rdx
  413f81:	48 8b 7c 24 20       	mov    0x20(%rsp),%rdi
  413f86:	48 8d 34 2a          	lea    (%rdx,%rbp,1),%rsi
  413f8a:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  413f8f:	89 54 24 20          	mov    %edx,0x20(%rsp)
  413f93:	48 89 54 24 28       	mov    %rdx,0x28(%rsp)
  413f98:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  413f9e:	e8 ad 0e 01 00       	callq  424e50 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  413fa3:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  413fa8:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  413faf:	48 8b 54 24 28       	mov    0x28(%rsp),%rdx
  413fb4:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  413fbb:	4b 8d 0c 02          	lea    (%r10,%r8,1),%rcx
  413fbf:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  413fc5:	89 d2                	mov    %edx,%edx
  413fc7:	8b 01                	mov    (%rcx),%eax
  413fc9:	49 8d 3c 93          	lea    (%r11,%rdx,4),%rdi
  413fcd:	8b 51 04             	mov    0x4(%rcx),%edx
  413fd0:	4d 8d 0c 83          	lea    (%r11,%rax,4),%r9
  413fd4:	48 89 d6             	mov    %rdx,%rsi
  413fd7:	48 c1 e2 02          	shl    $0x2,%rdx
  413fdb:	48 83 fa 04          	cmp    $0x4,%rdx
  413fdf:	0f 86 5a 06 00 00    	jbe    41463f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf4f>
  413fe5:	4c 89 ce             	mov    %r9,%rsi
  413fe8:	48 89 4c 24 30       	mov    %rcx,0x30(%rsp)
  413fed:	66 0f 7e 44 24 28    	movd   %xmm0,0x28(%rsp)
  413ff3:	e8 b8 e3 fe ff       	callq  4023b0 <memmove@plt>
  413ff8:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  413ffd:	48 8b 4c 24 30       	mov    0x30(%rsp),%rcx
  414002:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  414009:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  414010:	66 0f 6e 44 24 28    	movd   0x28(%rsp),%xmm0
  414016:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  41401a:	8b 70 04             	mov    0x4(%rax),%esi
  41401d:	49 8b 97 90 01 00 00 	mov    0x190(%r15),%rdx
  414024:	8b 7c 24 20          	mov    0x20(%rsp),%edi
  414028:	44 89 69 08          	mov    %r13d,0x8(%rcx)
  41402c:	89 39                	mov    %edi,(%rcx)
  41402e:	48 8d 7c 15 ff       	lea    -0x1(%rbp,%rdx,1),%rdi
  414033:	e9 b1 fb ff ff       	jmpq   413be9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4f9>
  414038:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  41403f:	00 
  414040:	4d 8b 8f 70 01 00 00 	mov    0x170(%r15),%r9
  414047:	49 8d bf 60 01 00 00 	lea    0x160(%r15),%rdi
  41404e:	4d 39 d9             	cmp    %r11,%r9
  414051:	0f 84 e0 04 00 00    	je     414537 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe47>
  414057:	4d 8b 97 68 01 00 00 	mov    0x168(%r15),%r10
  41405e:	4d 29 d9             	sub    %r11,%r9
  414061:	41 89 d5             	mov    %edx,%r13d
  414064:	4d 29 da             	sub    %r11,%r10
  414067:	44 8b 1b             	mov    (%rbx),%r11d
  41406a:	4c 89 d6             	mov    %r10,%rsi
  41406d:	48 c1 fe 02          	sar    $0x2,%rsi
  414071:	4d 01 eb             	add    %r13,%r11
  414074:	48 89 f0             	mov    %rsi,%rax
  414077:	49 39 f3             	cmp    %rsi,%r11
  41407a:	0f 84 f0 02 00 00    	je     414370 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc80>
  414080:	85 d2                	test   %edx,%edx
  414082:	0f 84 68 01 00 00    	je     4141f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb00>
  414088:	44 8d 2c 12          	lea    (%rdx,%rdx,1),%r13d
  41408c:	44 89 6c 24 20       	mov    %r13d,0x20(%rsp)
  414091:	4c 01 ee             	add    %r13,%rsi
  414094:	49 c1 f9 02          	sar    $0x2,%r9
  414098:	49 39 f1             	cmp    %rsi,%r9
  41409b:	73 5d                	jae    4140fa <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa0a>
  41409d:	48 83 fe 03          	cmp    $0x3,%rsi
  4140a1:	0f 86 c5 05 00 00    	jbe    41466c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf7c>
  4140a7:	b8 04 00 00 00       	mov    $0x4,%eax
  4140ac:	0f 1f 40 00          	nopl   0x0(%rax)
  4140b0:	48 01 c0             	add    %rax,%rax
  4140b3:	48 8d 50 ff          	lea    -0x1(%rax),%rdx
  4140b7:	48 39 f2             	cmp    %rsi,%rdx
  4140ba:	72 f4                	jb     4140b0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9c0>
  4140bc:	48 89 d6             	mov    %rdx,%rsi
  4140bf:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  4140c4:	48 89 7c 24 28       	mov    %rdi,0x28(%rsp)
  4140c9:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  4140cf:	e8 bc 0c 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  4140d4:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  4140d9:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  4140df:	49 8b 87 68 01 00 00 	mov    0x168(%r15),%rax
  4140e6:	49 2b 87 60 01 00 00 	sub    0x160(%r15),%rax
  4140ed:	48 c1 f8 02          	sar    $0x2,%rax
  4140f1:	48 8b 7c 24 28       	mov    0x28(%rsp),%rdi
  4140f6:	4a 8d 34 28          	lea    (%rax,%r13,1),%rsi
  4140fa:	4c 89 44 24 40       	mov    %r8,0x40(%rsp)
  4140ff:	89 44 24 28          	mov    %eax,0x28(%rsp)
  414103:	48 89 44 24 30       	mov    %rax,0x30(%rsp)
  414108:	66 0f 7e 44 24 38    	movd   %xmm0,0x38(%rsp)
  41410e:	e8 3d 0d 01 00       	callq  424e50 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  414113:	4c 8b 44 24 40       	mov    0x40(%rsp),%r8
  414118:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  41411f:	48 8b 44 24 30       	mov    0x30(%rsp),%rax
  414124:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  41412b:	4f 8d 0c 02          	lea    (%r10,%r8,1),%r9
  41412f:	66 0f 6e 44 24 38    	movd   0x38(%rsp),%xmm0
  414135:	89 c0                	mov    %eax,%eax
  414137:	41 8b 51 04          	mov    0x4(%r9),%edx
  41413b:	49 8d 3c 83          	lea    (%r11,%rax,4),%rdi
  41413f:	41 8b 01             	mov    (%r9),%eax
  414142:	49 8d 34 83          	lea    (%r11,%rax,4),%rsi
  414146:	89 d0                	mov    %edx,%eax
  414148:	48 c1 e0 02          	shl    $0x2,%rax
  41414c:	48 83 f8 04          	cmp    $0x4,%rax
  414150:	0f 86 d3 04 00 00    	jbe    414629 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf39>
  414156:	48 89 c2             	mov    %rax,%rdx
  414159:	4c 89 4c 24 40       	mov    %r9,0x40(%rsp)
  41415e:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  414163:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  414169:	e8 42 e2 fe ff       	callq  4023b0 <memmove@plt>
  41416e:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  414173:	4c 8b 4c 24 40       	mov    0x40(%rsp),%r9
  414178:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  41417f:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  414186:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  41418c:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  414190:	8b 50 04             	mov    0x4(%rax),%edx
  414193:	8b 7c 24 28          	mov    0x28(%rsp),%edi
  414197:	49 8b b7 90 01 00 00 	mov    0x190(%r15),%rsi
  41419e:	41 89 39             	mov    %edi,(%r9)
  4141a1:	8b 7c 24 20          	mov    0x20(%rsp),%edi
  4141a5:	41 89 79 08          	mov    %edi,0x8(%r9)
  4141a9:	49 8d 7c 35 ff       	lea    -0x1(%r13,%rsi,1),%rdi
  4141ae:	e9 13 fa ff ff       	jmpq   413bc6 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4d6>
  4141b3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  4141b8:	4c 8b 4c 24 38       	mov    0x38(%rsp),%r9
  4141bd:	48 8d 42 1c          	lea    0x1c(%rdx),%rax
  4141c1:	49 39 c1             	cmp    %rax,%r9
  4141c4:	75 13                	jne    4141d9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xae9>
  4141c6:	eb 5e                	jmp    414226 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb36>
  4141c8:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  4141cf:	00 
  4141d0:	48 83 c0 04          	add    $0x4,%rax
  4141d4:	49 39 c1             	cmp    %rax,%r9
  4141d7:	74 4d                	je     414226 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb36>
  4141d9:	8b 30                	mov    (%rax),%esi
  4141db:	41 80 3c 36 00       	cmpb   $0x0,(%r14,%rsi,1)
  4141e0:	48 89 f5             	mov    %rsi,%rbp
  4141e3:	78 eb                	js     4141d0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xae0>
  4141e5:	e9 67 fb ff ff       	jmpq   413d51 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x661>
  4141ea:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  4141f0:	c7 44 24 20 01 00 00 	movl   $0x1,0x20(%rsp)
  4141f7:	00 
  4141f8:	41 bd 01 00 00 00    	mov    $0x1,%r13d
  4141fe:	e9 8e fe ff ff       	jmpq   414091 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9a1>
  414203:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  414208:	bd 01 00 00 00       	mov    $0x1,%ebp
  41420d:	41 bd 01 00 00 00    	mov    $0x1,%r13d
  414213:	e9 06 fd ff ff       	jmpq   413f1e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x82e>
  414218:	48 8d 42 1c          	lea    0x1c(%rdx),%rax
  41421c:	49 89 e9             	mov    %rbp,%r9
  41421f:	48 39 44 24 38       	cmp    %rax,0x38(%rsp)
  414224:	75 b3                	jne    4141d9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xae9>
  414226:	80 7c 24 20 00       	cmpb   $0x0,0x20(%rsp)
  41422b:	0f 85 4f 04 00 00    	jne    414680 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf90>
  414231:	49 03 8f 30 01 00 00 	add    0x130(%r15),%rcx
  414238:	8b 74 24 10          	mov    0x10(%rsp),%esi
  41423c:	8b 01                	mov    (%rcx),%eax
  41423e:	48 8d 51 14          	lea    0x14(%rcx),%rdx
  414242:	31 c9                	xor    %ecx,%ecx
  414244:	d1 ee                	shr    %esi
  414246:	48 c1 e0 02          	shl    $0x2,%rax
  41424a:	4c 8d 04 02          	lea    (%rdx,%rax,1),%r8
  41424e:	74 23                	je     414273 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb83>
  414250:	8b 02                	mov    (%rdx),%eax
  414252:	d1 e8                	shr    %eax
  414254:	39 c6                	cmp    %eax,%esi
  414256:	74 12                	je     41426a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb7a>
  414258:	48 c1 e0 04          	shl    $0x4,%rax
  41425c:	49 03 87 a0 00 00 00 	add    0xa0(%r15),%rax
  414263:	8b 00                	mov    (%rax),%eax
  414265:	39 c1                	cmp    %eax,%ecx
  414267:	0f 42 c8             	cmovb  %eax,%ecx
  41426a:	48 83 c2 04          	add    $0x4,%rdx
  41426e:	4c 39 c2             	cmp    %r8,%rdx
  414271:	75 dd                	jne    414250 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb60>
  414273:	8b 44 24 10          	mov    0x10(%rsp),%eax
  414277:	83 f0 01             	xor    $0x1,%eax
  41427a:	48 89 c2             	mov    %rax,%rdx
  41427d:	48 c1 e2 04          	shl    $0x4,%rdx
  414281:	49 03 97 78 01 00 00 	add    0x178(%r15),%rdx
  414288:	44 8b 4a 04          	mov    0x4(%rdx),%r9d
  41428c:	45 85 c9             	test   %r9d,%r9d
  41428f:	0f 85 34 03 00 00    	jne    4145c9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xed9>
  414295:	49 8b 97 88 00 00 00 	mov    0x88(%r15),%rdx
  41429c:	48 8b 5c 24 28       	mov    0x28(%rsp),%rbx
  4142a1:	45 31 c0             	xor    %r8d,%r8d
  4142a4:	c6 04 1a 01          	movb   $0x1,(%rdx,%rbx,1)
  4142a8:	49 8b 97 88 00 00 00 	mov    0x88(%r15),%rdx
  4142af:	8b 5c 24 10          	mov    0x10(%rsp),%ebx
  4142b3:	c6 04 02 ff          	movb   $0xff,(%rdx,%rax,1)
  4142b7:	48 89 f0             	mov    %rsi,%rax
  4142ba:	49 8b 97 e8 00 00 00 	mov    0xe8(%r15),%rdx
  4142c1:	48 c1 e0 04          	shl    $0x4,%rax
  4142c5:	49 03 87 a0 00 00 00 	add    0xa0(%r15),%rax
  4142cc:	89 08                	mov    %ecx,(%rax)
  4142ce:	89 50 04             	mov    %edx,0x4(%rax)
  4142d1:	44 89 68 08          	mov    %r13d,0x8(%rax)
  4142d5:	66 44 89 40 0c       	mov    %r8w,0xc(%rax)
  4142da:	48 8d 42 01          	lea    0x1(%rdx),%rax
  4142de:	49 89 87 e8 00 00 00 	mov    %rax,0xe8(%r15)
  4142e5:	49 8b 87 d0 00 00 00 	mov    0xd0(%r15),%rax
  4142ec:	89 1c 90             	mov    %ebx,(%rax,%rdx,4)
  4142ef:	85 c9                	test   %ecx,%ecx
  4142f1:	75 10                	jne    414303 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc13>
  4142f3:	49 83 87 98 06 00 00 	addq   $0x1,0x698(%r15)
  4142fa:	01 
  4142fb:	41 83 87 54 1c 00 00 	addl   $0x1,0x1c54(%r15)
  414302:	01 
  414303:	49 8b 97 e0 1d 00 00 	mov    0x1de0(%r15),%rdx
  41430a:	48 85 d2             	test   %rdx,%rdx
  41430d:	74 21                	je     414330 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc40>
  41430f:	41 80 bf e8 1d 00 00 	cmpb   $0x0,0x1de8(%r15)
  414316:	00 
  414317:	75 17                	jne    414330 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc40>
  414319:	41 80 bf 20 02 00 00 	cmpb   $0x0,0x220(%r15)
  414320:	00 
  414321:	0f 84 d0 02 00 00    	je     4145f7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf07>
  414327:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  41432e:	00 00 
  414330:	49 8b 97 48 1d 00 00 	mov    0x1d48(%r15),%rdx
  414337:	49 8b 87 50 1d 00 00 	mov    0x1d50(%r15),%rax
  41433e:	41 83 af 1c 01 00 00 	subl   $0x1,0x11c(%r15)
  414345:	01 
  414346:	48 29 d0             	sub    %rdx,%rax
  414349:	48 39 c6             	cmp    %rax,%rsi
  41434c:	73 06                	jae    414354 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc64>
  41434e:	80 3c 32 00          	cmpb   $0x0,(%rdx,%rsi,1)
  414352:	75 08                	jne    41435c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc6c>
  414354:	41 83 af 20 01 00 00 	subl   $0x1,0x120(%r15)
  41435b:	01 
  41435c:	49 83 c4 02          	add    $0x2,%r12
  414360:	4c 89 d2             	mov    %r10,%rdx
  414363:	48 89 fb             	mov    %rdi,%rbx
  414366:	e9 b5 f6 ff ff       	jmpq   413a20 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  41436b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  414370:	4d 39 ca             	cmp    %r9,%r10
  414373:	0f 83 07 fd ff ff    	jae    414080 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x990>
  414379:	49 8d 73 01          	lea    0x1(%r11),%rsi
  41437d:	4c 89 44 24 28       	mov    %r8,0x28(%rsp)
  414382:	66 0f 7e 44 24 20    	movd   %xmm0,0x20(%rsp)
  414388:	e8 c3 0a 01 00       	callq  424e50 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  41438d:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  414394:	83 43 08 01          	addl   $0x1,0x8(%rbx)
  414398:	4c 8b 44 24 28       	mov    0x28(%rsp),%r8
  41439d:	49 8b bf 90 01 00 00 	mov    0x190(%r15),%rdi
  4143a4:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  4143ab:	66 0f 6e 44 24 20    	movd   0x20(%rsp),%xmm0
  4143b1:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  4143b5:	8b 50 04             	mov    0x4(%rax),%edx
  4143b8:	e9 09 f8 ff ff       	jmpq   413bc6 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4d6>
  4143bd:	0f 1f 00             	nopl   (%rax)
  4143c0:	4c 39 d5             	cmp    %r10,%rbp
  4143c3:	0f 83 47 fb ff ff    	jae    413f10 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x820>
  4143c9:	49 8d 73 01          	lea    0x1(%r11),%rsi
  4143cd:	48 89 44 24 30       	mov    %rax,0x30(%rsp)
  4143d2:	4c 89 44 24 28       	mov    %r8,0x28(%rsp)
  4143d7:	66 0f 7e 44 24 20    	movd   %xmm0,0x20(%rsp)
  4143dd:	e8 6e 0a 01 00       	callq  424e50 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  4143e2:	48 8b 44 24 30       	mov    0x30(%rsp),%rax
  4143e7:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  4143ee:	4c 8b 44 24 28       	mov    0x28(%rsp),%r8
  4143f3:	49 8b bf 90 01 00 00 	mov    0x190(%r15),%rdi
  4143fa:	83 40 08 01          	addl   $0x1,0x8(%rax)
  4143fe:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  414405:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  414409:	66 0f 6e 44 24 20    	movd   0x20(%rsp),%xmm0
  41440f:	8b 70 04             	mov    0x4(%rax),%esi
  414412:	e9 d2 f7 ff ff       	jmpq   413be9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4f9>
  414417:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  41441e:	00 00 
  414420:	8b 44 24 04          	mov    0x4(%rsp),%eax
  414424:	8b 6c 24 58          	mov    0x58(%rsp),%ebp
  414428:	4c 89 64 24 10       	mov    %r12,0x10(%rsp)
  41442d:	4d 89 d1             	mov    %r10,%r9
  414430:	41 c6 87 d1 02 00 00 	movb   $0x1,0x2d1(%r15)
  414437:	01 
  414438:	4c 8b 64 24 48       	mov    0x48(%rsp),%r12
  41443d:	41 c7 87 d4 02 00 00 	movl   $0xffffffff,0x2d4(%r15)
  414444:	ff ff ff ff 
  414448:	41 89 87 d8 02 00 00 	mov    %eax,0x2d8(%r15)
  41444f:	41 89 8f dc 02 00 00 	mov    %ecx,0x2dc(%r15)
  414456:	48 8b 44 24 40       	mov    0x40(%rsp),%rax
  41445b:	4d 89 ca             	mov    %r9,%r10
  41445e:	4c 01 c8             	add    %r9,%rax
  414461:	49 39 d3             	cmp    %rdx,%r11
  414464:	0f 84 45 02 00 00    	je     4146af <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xfbf>
  41446a:	4c 89 de             	mov    %r11,%rsi
  41446d:	48 29 d6             	sub    %rdx,%rsi
  414470:	48 8d 7e fc          	lea    -0x4(%rsi),%rdi
  414474:	48 83 ff 08          	cmp    $0x8,%rdi
  414478:	0f 86 b2 00 00 00    	jbe    414530 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe40>
  41447e:	48 8d 4a 04          	lea    0x4(%rdx),%rcx
  414482:	49 89 d8             	mov    %rbx,%r8
  414485:	49 29 c8             	sub    %rcx,%r8
  414488:	31 c9                	xor    %ecx,%ecx
  41448a:	49 83 f8 08          	cmp    $0x8,%r8
  41448e:	77 30                	ja     4144c0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdd0>
  414490:	8b 3c 0a             	mov    (%rdx,%rcx,1),%edi
  414493:	89 3c 0b             	mov    %edi,(%rbx,%rcx,1)
  414496:	48 83 c1 04          	add    $0x4,%rcx
  41449a:	48 39 f1             	cmp    %rsi,%rcx
  41449d:	75 f1                	jne    414490 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xda0>
  41449f:	48 8d 14 33          	lea    (%rbx,%rsi,1),%rdx
  4144a3:	8b 48 04             	mov    0x4(%rax),%ecx
  4144a6:	41 bd 01 00 00 00    	mov    $0x1,%r13d
  4144ac:	4c 29 e2             	sub    %r12,%rdx
  4144af:	48 c1 fa 02          	sar    $0x2,%rdx
  4144b3:	e9 38 f6 ff ff       	jmpq   413af0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x400>
  4144b8:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  4144bf:	00 
  4144c0:	48 c1 ef 02          	shr    $0x2,%rdi
  4144c4:	48 83 c7 01          	add    $0x1,%rdi
  4144c8:	49 89 f8             	mov    %rdi,%r8
  4144cb:	49 c1 e8 02          	shr    $0x2,%r8
  4144cf:	49 c1 e0 04          	shl    $0x4,%r8
  4144d3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  4144d8:	f3 0f 6f 1c 0a       	movdqu (%rdx,%rcx,1),%xmm3
  4144dd:	0f 11 1c 0b          	movups %xmm3,(%rbx,%rcx,1)
  4144e1:	48 83 c1 10          	add    $0x10,%rcx
  4144e5:	4c 39 c1             	cmp    %r8,%rcx
  4144e8:	75 ee                	jne    4144d8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xde8>
  4144ea:	40 f6 c7 03          	test   $0x3,%dil
  4144ee:	74 af                	je     41449f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdaf>
  4144f0:	48 83 e7 fc          	and    $0xfffffffffffffffc,%rdi
  4144f4:	48 8d 0c bd 00 00 00 	lea    0x0(,%rdi,4),%rcx
  4144fb:	00 
  4144fc:	48 01 ca             	add    %rcx,%rdx
  4144ff:	48 01 d9             	add    %rbx,%rcx
  414502:	8b 3a                	mov    (%rdx),%edi
  414504:	89 39                	mov    %edi,(%rcx)
  414506:	48 8d 7a 04          	lea    0x4(%rdx),%rdi
  41450a:	49 39 fb             	cmp    %rdi,%r11
  41450d:	74 90                	je     41449f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdaf>
  41450f:	8b 7a 04             	mov    0x4(%rdx),%edi
  414512:	89 79 04             	mov    %edi,0x4(%rcx)
  414515:	48 8d 7a 08          	lea    0x8(%rdx),%rdi
  414519:	49 39 fb             	cmp    %rdi,%r11
  41451c:	74 81                	je     41449f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdaf>
  41451e:	8b 52 08             	mov    0x8(%rdx),%edx
  414521:	89 51 08             	mov    %edx,0x8(%rcx)
  414524:	e9 76 ff ff ff       	jmpq   41449f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdaf>
  414529:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  414530:	31 c9                	xor    %ecx,%ecx
  414532:	e9 59 ff ff ff       	jmpq   414490 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xda0>
  414537:	be 03 00 00 00       	mov    $0x3,%esi
  41453c:	4c 89 44 24 30       	mov    %r8,0x30(%rsp)
  414541:	48 89 7c 24 20       	mov    %rdi,0x20(%rsp)
  414546:	66 0f 7e 44 24 28    	movd   %xmm0,0x28(%rsp)
  41454c:	e8 3f 08 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  414551:	8b 53 04             	mov    0x4(%rbx),%edx
  414554:	4c 8b 44 24 30       	mov    0x30(%rsp),%r8
  414559:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  414560:	4d 8b 8f 70 01 00 00 	mov    0x170(%r15),%r9
  414567:	66 0f 6e 44 24 28    	movd   0x28(%rsp),%xmm0
  41456d:	48 8b 7c 24 20       	mov    0x20(%rsp),%rdi
  414572:	e9 e0 fa ff ff       	jmpq   414057 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x967>
  414577:	be 03 00 00 00       	mov    $0x3,%esi
  41457c:	48 89 44 24 38       	mov    %rax,0x38(%rsp)
  414581:	4c 89 44 24 30       	mov    %r8,0x30(%rsp)
  414586:	48 89 7c 24 20       	mov    %rdi,0x20(%rsp)
  41458b:	66 0f 7e 44 24 28    	movd   %xmm0,0x28(%rsp)
  414591:	e8 fa 07 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  414596:	48 8b 44 24 38       	mov    0x38(%rsp),%rax
  41459b:	4c 8b 44 24 30       	mov    0x30(%rsp),%r8
  4145a0:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  4145a7:	4d 8b 97 70 01 00 00 	mov    0x170(%r15),%r10
  4145ae:	8b 70 04             	mov    0x4(%rax),%esi
  4145b1:	66 0f 6e 44 24 28    	movd   0x28(%rsp),%xmm0
  4145b7:	48 8b 7c 24 20       	mov    0x20(%rsp),%rdi
  4145bc:	e9 26 f9 ff ff       	jmpq   413ee7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x7f7>
  4145c1:	41 89 c0             	mov    %eax,%r8d
  4145c4:	e9 a9 f1 ff ff       	jmpq   413772 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x82>
  4145c9:	44 8b 02             	mov    (%rdx),%r8d
  4145cc:	49 8b 97 60 01 00 00 	mov    0x160(%r15),%rdx
  4145d3:	42 0f 18 1c 82       	prefetcht2 (%rdx,%r8,4)
  4145d8:	e9 b8 fc ff ff       	jmpq   414295 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xba5>
  4145dd:	89 87 f0 00 00 00    	mov    %eax,0xf0(%rdi)
  4145e3:	41 89 c4             	mov    %eax,%r12d
  4145e6:	e9 0b f2 ff ff       	jmpq   4137f6 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x106>
  4145eb:	49 01 97 88 06 00 00 	add    %rdx,0x688(%r15)
  4145f2:	e9 c7 f1 ff ff       	jmpq   4137be <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xce>
  4145f7:	0f b6 44 24 10       	movzbl 0x10(%rsp),%eax
  4145fc:	48 8b 12             	mov    (%rdx),%rdx
  4145ff:	83 e0 01             	and    $0x1,%eax
  414602:	3c 01                	cmp    $0x1,%al
  414604:	19 c0                	sbb    %eax,%eax
  414606:	83 e0 02             	and    $0x2,%eax
  414609:	83 e8 01             	sub    $0x1,%eax
  41460c:	88 04 32             	mov    %al,(%rdx,%rsi,1)
  41460f:	e9 1c fd ff ff       	jmpq   414330 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc40>
  414614:	45 8b 12             	mov    (%r10),%r10d
  414617:	49 8b af 60 01 00 00 	mov    0x160(%r15),%rbp
  41461e:	42 0f 18 5c 95 00    	prefetcht2 0x0(%rbp,%r10,4)
  414624:	e9 26 f8 ff ff       	jmpq   413e4f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x75f>
  414629:	4c 89 c8             	mov    %r9,%rax
  41462c:	0f 85 61 fb ff ff    	jne    414193 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xaa3>
  414632:	8b 16                	mov    (%rsi),%edx
  414634:	89 17                	mov    %edx,(%rdi)
  414636:	41 8b 51 04          	mov    0x4(%r9),%edx
  41463a:	e9 54 fb ff ff       	jmpq   414193 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xaa3>
  41463f:	48 89 c8             	mov    %rcx,%rax
  414642:	0f 85 d5 f9 ff ff    	jne    41401d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x92d>
  414648:	41 8b 11             	mov    (%r9),%edx
  41464b:	89 17                	mov    %edx,(%rdi)
  41464d:	8b 71 04             	mov    0x4(%rcx),%esi
  414650:	e9 c8 f9 ff ff       	jmpq   41401d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x92d>
  414655:	44 8b 64 24 5c       	mov    0x5c(%rsp),%r12d
  41465a:	89 e8                	mov    %ebp,%eax
  41465c:	45 31 c0             	xor    %r8d,%r8d
  41465f:	e9 f6 f5 ff ff       	jmpq   413c5a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x56a>
  414664:	45 31 ed             	xor    %r13d,%r13d
  414667:	e9 84 f4 ff ff       	jmpq   413af0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x400>
  41466c:	ba 03 00 00 00       	mov    $0x3,%edx
  414671:	e9 46 fa ff ff       	jmpq   4140bc <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9cc>
  414676:	ba 03 00 00 00       	mov    $0x3,%edx
  41467b:	e9 cc f8 ff ff       	jmpq   413f4c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x85c>
  414680:	4c 89 44 24 10       	mov    %r8,0x10(%rsp)
  414685:	8b 6c 24 58          	mov    0x58(%rsp),%ebp
  414689:	48 89 fb             	mov    %rdi,%rbx
  41468c:	4c 89 d2             	mov    %r10,%rdx
  41468f:	41 c6 87 d1 02 00 00 	movb   $0x1,0x2d1(%r15)
  414696:	01 
  414697:	4c 8b 64 24 48       	mov    0x48(%rsp),%r12
  41469c:	45 89 af d4 02 00 00 	mov    %r13d,0x2d4(%r15)
  4146a3:	4d 8b 8f 78 01 00 00 	mov    0x178(%r15),%r9
  4146aa:	e9 a7 fd ff ff       	jmpq   414456 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xd66>
  4146af:	41 bd 01 00 00 00    	mov    $0x1,%r13d
  4146b5:	e9 26 f4 ff ff       	jmpq   413ae0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x3f0>

Disassembly of section .fini:
