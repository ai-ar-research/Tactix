
/home/hwu/SAT-solving/build/pin-congruence-index-repair-5623-release/tactix:     file format elf64-x86-64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004136f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)>:
  4136f0:	41 57                	push   %r15
  4136f2:	49 89 fb             	mov    %rdi,%r11
  4136f5:	89 f7                	mov    %esi,%edi
  4136f7:	41 56                	push   %r14
  4136f9:	41 55                	push   %r13
  4136fb:	41 54                	push   %r12
  4136fd:	55                   	push   %rbp
  4136fe:	53                   	push   %rbx
  4136ff:	48 83 ec 78          	sub    $0x78,%rsp
  413703:	41 8b 83 f4 00 00 00 	mov    0xf4(%r11),%eax
  41370a:	41 8b 9b f0 00 00 00 	mov    0xf0(%r11),%ebx
  413711:	39 d8                	cmp    %ebx,%eax
  413713:	73 09                	jae    41371e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x2e>
  413715:	41 89 83 f0 00 00 00 	mov    %eax,0xf0(%r11)
  41371c:	89 c3                	mov    %eax,%ebx
  41371e:	89 5c 24 5c          	mov    %ebx,0x5c(%rsp)
  413722:	89 d8                	mov    %ebx,%eax
  413724:	41 89 fc             	mov    %edi,%r12d
  413727:	4c 89 db             	mov    %r11,%rbx
  41372a:	41 c7 83 f4 00 00 00 	movl   $0xffffffff,0xf4(%r11)
  413731:	ff ff ff ff 
  413735:	48 c7 44 24 68 00 00 	movq   $0x0,0x68(%rsp)
  41373c:	00 00 
  41373e:	eb 22                	jmp    413762 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x72>
  413740:	48 8d 4c 24 68       	lea    0x68(%rsp),%rcx
  413745:	44 89 e2             	mov    %r12d,%edx
  413748:	48 89 df             	mov    %rbx,%rdi
  41374b:	e8 e0 f0 ff ff       	callq  412830 <bool tactix::propagate_literal<false, false, true>(tactix::Kernel&, unsigned int, unsigned int, unsigned long&)>
  413750:	41 89 c6             	mov    %eax,%r14d
  413753:	8b 83 f0 00 00 00    	mov    0xf0(%rbx),%eax
  413759:	45 84 f6             	test   %r14b,%r14b
  41375c:	0f 85 57 04 00 00    	jne    413bb9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4c9>
  413762:	89 c2                	mov    %eax,%edx
  413764:	48 3b 93 e8 00 00 00 	cmp    0xe8(%rbx),%rdx
  41376b:	0f 83 de 0c 00 00    	jae    41444f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xd5f>
  413771:	83 c0 01             	add    $0x1,%eax
  413774:	89 83 f0 00 00 00    	mov    %eax,0xf0(%rbx)
  41377a:	48 8b 83 d0 00 00 00 	mov    0xd0(%rbx),%rax
  413781:	8b 34 90             	mov    (%rax,%rdx,4),%esi
  413784:	41 83 fc ff          	cmp    $0xffffffff,%r12d
  413788:	75 b6                	jne    413740 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x50>
  41378a:	89 f0                	mov    %esi,%eax
  41378c:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  413793:	48 8b bb 30 01 00 00 	mov    0x130(%rbx),%rdi
  41379a:	d1 ee                	shr    %esi
  41379c:	83 f0 01             	xor    $0x1,%eax
  41379f:	4c 8b 83 60 01 00 00 	mov    0x160(%rbx),%r8
  4137a6:	48 c1 e6 04          	shl    $0x4,%rsi
  4137aa:	48 03 b3 a0 00 00 00 	add    0xa0(%rbx),%rsi
  4137b1:	89 04 24             	mov    %eax,(%rsp)
  4137b4:	48 c1 e0 04          	shl    $0x4,%rax
  4137b8:	4c 8b 9b 88 00 00 00 	mov    0x88(%rbx),%r11
  4137bf:	49 89 c7             	mov    %rax,%r15
  4137c2:	49 8d 04 02          	lea    (%r10,%rax,1),%rax
  4137c6:	48 89 7c 24 10       	mov    %rdi,0x10(%rsp)
  4137cb:	44 8b 08             	mov    (%rax),%r9d
  4137ce:	8b 50 04             	mov    0x4(%rax),%edx
  4137d1:	48 89 74 24 18       	mov    %rsi,0x18(%rsp)
  4137d6:	48 89 d1             	mov    %rdx,%rcx
  4137d9:	4c 01 ca             	add    %r9,%rdx
  4137dc:	4a 8d 3c 8d 00 00 00 	lea    0x0(,%r9,4),%rdi
  4137e3:	00 
  4137e4:	48 c1 e2 02          	shl    $0x2,%rdx
  4137e8:	4d 8d 2c 38          	lea    (%r8,%rdi,1),%r13
  4137ec:	49 01 d0             	add    %rdx,%r8
  4137ef:	48 29 fa             	sub    %rdi,%rdx
  4137f2:	48 c1 fa 02          	sar    $0x2,%rdx
  4137f6:	4c 89 44 24 30       	mov    %r8,0x30(%rsp)
  4137fb:	48 8d 72 1f          	lea    0x1f(%rdx),%rsi
  4137ff:	48 c1 ee 05          	shr    $0x5,%rsi
  413803:	48 8d 7e 01          	lea    0x1(%rsi),%rdi
  413807:	48 89 7c 24 08       	mov    %rdi,0x8(%rsp)
  41380c:	4d 39 c5             	cmp    %r8,%r13
  41380f:	0f 84 a8 0d 00 00    	je     4145bd <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xecd>
  413815:	4c 89 7c 24 48       	mov    %r15,0x48(%rsp)
  41381a:	4c 89 ed             	mov    %r13,%rbp
  41381d:	4c 89 ea             	mov    %r13,%rdx
  413820:	49 89 ff             	mov    %rdi,%r15
  413823:	44 89 64 24 58       	mov    %r12d,0x58(%rsp)
  413828:	4d 89 dc             	mov    %r11,%r12
  41382b:	4d 89 c3             	mov    %r8,%r11
  41382e:	4c 89 6c 24 50       	mov    %r13,0x50(%rsp)
  413833:	e9 4f 01 00 00       	jmpq   413987 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x297>
  413838:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  41383f:	00 
  413840:	48 83 c2 04          	add    $0x4,%rdx
  413844:	48 83 c5 04          	add    $0x4,%rbp
  413848:	40 84 ff             	test   %dil,%dil
  41384b:	0f 8f 2d 01 00 00    	jg     41397e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x28e>
  413851:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  413858:	0f 85 ca 0a 00 00    	jne    414328 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc38>
  41385e:	89 cf                	mov    %ecx,%edi
  413860:	4c 8b 74 24 18       	mov    0x18(%rsp),%r14
  413865:	c1 e8 02             	shr    $0x2,%eax
  413868:	83 f7 01             	xor    $0x1,%edi
  41386b:	49 89 f9             	mov    %rdi,%r9
  41386e:	45 8b 06             	mov    (%r14),%r8d
  413871:	49 c1 e1 04          	shl    $0x4,%r9
  413875:	4d 01 ca             	add    %r9,%r10
  413878:	80 bb 6d 1e 00 00 00 	cmpb   $0x0,0x1e6d(%rbx)
  41387f:	74 12                	je     413893 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x1a3>
  413881:	80 bb 6e 1e 00 00 00 	cmpb   $0x0,0x1e6e(%rbx)
  413888:	74 09                	je     413893 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x1a3>
  41388a:	45 85 c0             	test   %r8d,%r8d
  41388d:	0f 85 8d 04 00 00    	jne    413d20 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x630>
  413893:	45 8b 6a 04          	mov    0x4(%r10),%r13d
  413897:	45 85 ed             	test   %r13d,%r13d
  41389a:	0f 85 50 03 00 00    	jne    413bf0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x500>
  4138a0:	4c 8b 8b 88 00 00 00 	mov    0x88(%rbx),%r9
  4138a7:	44 8b 14 24          	mov    (%rsp),%r10d
  4138ab:	66 41 0f 6e c0       	movd   %r8d,%xmm0
  4138b0:	41 c6 04 31 01       	movb   $0x1,(%r9,%rsi,1)
  4138b5:	48 8b b3 88 00 00 00 	mov    0x88(%rbx),%rsi
  4138bc:	c6 04 3e ff          	movb   $0xff,(%rsi,%rdi,1)
  4138c0:	48 89 c6             	mov    %rax,%rsi
  4138c3:	48 8b bb e8 00 00 00 	mov    0xe8(%rbx),%rdi
  4138ca:	48 c1 e6 04          	shl    $0x4,%rsi
  4138ce:	48 03 b3 a0 00 00 00 	add    0xa0(%rbx),%rsi
  4138d5:	44 89 56 08          	mov    %r10d,0x8(%rsi)
  4138d9:	66 0f 6e d7          	movd   %edi,%xmm2
  4138dd:	44 0f b7 15 fb ca 0f 	movzwl 0xfcafb(%rip),%r10d        # 5103e0 <tactix::BinaryIdMap::EMPTY+0x18>
  4138e4:	00 
  4138e5:	66 0f 62 c2          	punpckldq %xmm2,%xmm0
  4138e9:	66 44 89 56 0c       	mov    %r10w,0xc(%rsi)
  4138ee:	66 0f d6 06          	movq   %xmm0,(%rsi)
  4138f2:	48 8d 77 01          	lea    0x1(%rdi),%rsi
  4138f6:	48 89 b3 e8 00 00 00 	mov    %rsi,0xe8(%rbx)
  4138fd:	48 8b b3 d0 00 00 00 	mov    0xd0(%rbx),%rsi
  413904:	89 0c be             	mov    %ecx,(%rsi,%rdi,4)
  413907:	45 85 c0             	test   %r8d,%r8d
  41390a:	75 0f                	jne    41391b <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x22b>
  41390c:	48 83 83 98 06 00 00 	addq   $0x1,0x698(%rbx)
  413913:	01 
  413914:	83 83 54 1c 00 00 01 	addl   $0x1,0x1c54(%rbx)
  41391b:	48 8b b3 e0 1d 00 00 	mov    0x1de0(%rbx),%rsi
  413922:	48 85 f6             	test   %rsi,%rsi
  413925:	74 29                	je     413950 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x260>
  413927:	80 bb e8 1d 00 00 00 	cmpb   $0x0,0x1de8(%rbx)
  41392e:	75 20                	jne    413950 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x260>
  413930:	80 bb 20 02 00 00 00 	cmpb   $0x0,0x220(%rbx)
  413937:	75 17                	jne    413950 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x260>
  413939:	83 e1 01             	and    $0x1,%ecx
  41393c:	48 8b 36             	mov    (%rsi),%rsi
  41393f:	80 f9 01             	cmp    $0x1,%cl
  413942:	19 c9                	sbb    %ecx,%ecx
  413944:	83 e1 02             	and    $0x2,%ecx
  413947:	83 e9 01             	sub    $0x1,%ecx
  41394a:	88 0c 06             	mov    %cl,(%rsi,%rax,1)
  41394d:	0f 1f 00             	nopl   (%rax)
  413950:	48 8b b3 48 1d 00 00 	mov    0x1d48(%rbx),%rsi
  413957:	48 8b 8b 50 1d 00 00 	mov    0x1d50(%rbx),%rcx
  41395e:	83 ab 1c 01 00 00 01 	subl   $0x1,0x11c(%rbx)
  413965:	48 29 f1             	sub    %rsi,%rcx
  413968:	48 39 c8             	cmp    %rcx,%rax
  41396b:	73 06                	jae    413973 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x283>
  41396d:	80 3c 06 00          	cmpb   $0x0,(%rsi,%rax,1)
  413971:	75 07                	jne    41397a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x28a>
  413973:	83 ab 20 01 00 00 01 	subl   $0x1,0x120(%rbx)
  41397a:	49 83 c7 01          	add    $0x1,%r15
  41397e:	49 39 d3             	cmp    %rdx,%r11
  413981:	0f 84 99 00 00 00    	je     413a20 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  413987:	8b 02                	mov    (%rdx),%eax
  413989:	89 c1                	mov    %eax,%ecx
  41398b:	89 45 00             	mov    %eax,0x0(%rbp)
  41398e:	d1 e9                	shr    %ecx
  413990:	89 ce                	mov    %ecx,%esi
  413992:	41 0f b6 3c 34       	movzbl (%r12,%rsi,1),%edi
  413997:	a8 01                	test   $0x1,%al
  413999:	0f 85 a1 fe ff ff    	jne    413840 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x150>
  41399f:	44 8b 72 04          	mov    0x4(%rdx),%r14d
  4139a3:	4c 8d 4a 08          	lea    0x8(%rdx),%r9
  4139a7:	4c 8d 55 08          	lea    0x8(%rbp),%r10
  4139ab:	44 89 75 04          	mov    %r14d,0x4(%rbp)
  4139af:	40 84 ff             	test   %dil,%dil
  4139b2:	0f 8f 18 02 00 00    	jg     413bd0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4e0>
  4139b8:	48 8b 44 24 10       	mov    0x10(%rsp),%rax
  4139bd:	44 89 f1             	mov    %r14d,%ecx
  4139c0:	49 8d 7f 01          	lea    0x1(%r15),%rdi
  4139c4:	48 c1 e1 02          	shl    $0x2,%rcx
  4139c8:	48 89 7c 24 08       	mov    %rdi,0x8(%rsp)
  4139cd:	48 8d 14 08          	lea    (%rax,%rcx,1),%rdx
  4139d1:	f6 42 04 01          	testb  $0x1,0x4(%rdx)
  4139d5:	0f 85 05 02 00 00    	jne    413be0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4f0>
  4139db:	44 8b 04 24          	mov    (%rsp),%r8d
  4139df:	8b 42 14             	mov    0x14(%rdx),%eax
  4139e2:	33 42 18             	xor    0x18(%rdx),%eax
  4139e5:	41 31 c0             	xor    %eax,%r8d
  4139e8:	44 89 c0             	mov    %r8d,%eax
  4139eb:	48 89 44 24 28       	mov    %rax,0x28(%rsp)
  4139f0:	41 0f b6 04 04       	movzbl (%r12,%rax,1),%eax
  4139f5:	88 44 24 20          	mov    %al,0x20(%rsp)
  4139f9:	84 c0                	test   %al,%al
  4139fb:	0f 8e 07 02 00 00    	jle    413c08 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x518>
  413a01:	43 8d 04 00          	lea    (%r8,%r8,1),%eax
  413a05:	4c 89 ca             	mov    %r9,%rdx
  413a08:	49 89 ff             	mov    %rdi,%r15
  413a0b:	89 45 00             	mov    %eax,0x0(%rbp)
  413a0e:	4c 89 d5             	mov    %r10,%rbp
  413a11:	49 39 d3             	cmp    %rdx,%r11
  413a14:	0f 85 6d ff ff ff    	jne    413987 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x297>
  413a1a:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  413a20:	4c 89 7c 24 08       	mov    %r15,0x8(%rsp)
  413a25:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  413a2c:	45 31 f6             	xor    %r14d,%r14d
  413a2f:	4c 8b 7c 24 48       	mov    0x48(%rsp),%r15
  413a34:	4c 8b 6c 24 50       	mov    0x50(%rsp),%r13
  413a39:	44 8b 64 24 58       	mov    0x58(%rsp),%r12d
  413a3e:	4b 8d 04 3a          	lea    (%r10,%r15,1),%rax
  413a42:	48 89 ea             	mov    %rbp,%rdx
  413a45:	8b 48 04             	mov    0x4(%rax),%ecx
  413a48:	4c 29 ea             	sub    %r13,%rdx
  413a4b:	48 c1 fa 02          	sar    $0x2,%rdx
  413a4f:	90                   	nop
  413a50:	8b 70 0c             	mov    0xc(%rax),%esi
  413a53:	48 8b bb 18 02 00 00 	mov    0x218(%rbx),%rdi
  413a5a:	29 d1                	sub    %edx,%ecx
  413a5c:	48 01 8b 90 01 00 00 	add    %rcx,0x190(%rbx)
  413a63:	89 50 04             	mov    %edx,0x4(%rax)
  413a66:	89 70 0c             	mov    %esi,0xc(%rax)
  413a69:	48 85 ff             	test   %rdi,%rdi
  413a6c:	0f 84 23 01 00 00    	je     413b95 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4a5>
  413a72:	44 88 74 24 10       	mov    %r14b,0x10(%rsp)
  413a77:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  413a7e:	45 31 ed             	xor    %r13d,%r13d
  413a81:	44 89 64 24 18       	mov    %r12d,0x18(%rsp)
  413a86:	eb 36                	jmp    413abe <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x3ce>
  413a88:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  413a8f:	00 
  413a90:	8b 45 00             	mov    0x0(%rbp),%eax
  413a93:	66 41 0f 6e cc       	movd   %r12d,%xmm1
  413a98:	49 83 c5 03          	add    $0x3,%r13
  413a9c:	66 0f 62 c8          	punpckldq %xmm0,%xmm1
  413aa0:	48 01 d0             	add    %rdx,%rax
  413aa3:	66 41 0f d6 0c 83    	movq   %xmm1,(%r11,%rax,4)
  413aa9:	83 45 04 02          	addl   $0x2,0x4(%rbp)
  413aad:	48 83 ab 90 01 00 00 	subq   $0x2,0x190(%rbx)
  413ab4:	02 
  413ab5:	49 39 fd             	cmp    %rdi,%r13
  413ab8:	0f 83 cc 00 00 00    	jae    413b8a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x49a>
  413abe:	48 8b 83 00 02 00 00 	mov    0x200(%rbx),%rax
  413ac5:	4a 8d 34 ad 00 00 00 	lea    0x0(,%r13,4),%rsi
  413acc:	00 
  413acd:	46 8b 04 a8          	mov    (%rax,%r13,4),%r8d
  413ad1:	44 8b 64 30 04       	mov    0x4(%rax,%rsi,1),%r12d
  413ad6:	66 0f 6e 44 30 08    	movd   0x8(%rax,%rsi,1),%xmm0
  413adc:	49 c1 e0 04          	shl    $0x4,%r8
  413ae0:	45 01 e4             	add    %r12d,%r12d
  413ae3:	4b 8d 2c 02          	lea    (%r10,%r8,1),%rbp
  413ae7:	44 8b 4d 08          	mov    0x8(%rbp),%r9d
  413aeb:	8b 55 04             	mov    0x4(%rbp),%edx
  413aee:	44 89 c8             	mov    %r9d,%eax
  413af1:	29 d0                	sub    %edx,%eax
  413af3:	83 f8 01             	cmp    $0x1,%eax
  413af6:	77 98                	ja     413a90 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x3a0>
  413af8:	8b 45 0c             	mov    0xc(%rbp),%eax
  413afb:	89 d6                	mov    %edx,%esi
  413afd:	83 e6 01             	and    $0x1,%esi
  413b00:	41 89 c6             	mov    %eax,%r14d
  413b03:	f7 d0                	not    %eax
  413b05:	41 83 e6 fe          	and    $0xfffffffe,%r14d
  413b09:	41 89 c7             	mov    %eax,%r15d
  413b0c:	41 09 f6             	or     %esi,%r14d
  413b0f:	41 83 e7 01          	and    $0x1,%r15d
  413b13:	41 39 d1             	cmp    %edx,%r9d
  413b16:	0f 84 24 04 00 00    	je     413f40 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x850>
  413b1c:	48 8b 83 90 01 00 00 	mov    0x190(%rbx),%rax
  413b23:	48 8d 78 ff          	lea    -0x1(%rax),%rdi
  413b27:	48 89 e8             	mov    %rbp,%rax
  413b2a:	8d 72 01             	lea    0x1(%rdx),%esi
  413b2d:	03 10                	add    (%rax),%edx
  413b2f:	48 89 bb 90 01 00 00 	mov    %rdi,0x190(%rbx)
  413b36:	89 70 04             	mov    %esi,0x4(%rax)
  413b39:	45 89 24 93          	mov    %r12d,(%r11,%rdx,4)
  413b3d:	8b 70 04             	mov    0x4(%rax),%esi
  413b40:	3b 70 08             	cmp    0x8(%rax),%esi
  413b43:	0f 84 87 02 00 00    	je     413dd0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x6e0>
  413b49:	48 83 ef 01          	sub    $0x1,%rdi
  413b4d:	8d 56 01             	lea    0x1(%rsi),%edx
  413b50:	03 30                	add    (%rax),%esi
  413b52:	41 83 e6 fe          	and    $0xfffffffe,%r14d
  413b56:	49 83 c5 03          	add    $0x3,%r13
  413b5a:	48 89 bb 90 01 00 00 	mov    %rdi,0x190(%rbx)
  413b61:	48 8b bb 18 02 00 00 	mov    0x218(%rbx),%rdi
  413b68:	89 50 04             	mov    %edx,0x4(%rax)
  413b6b:	44 89 f8             	mov    %r15d,%eax
  413b6e:	83 f0 01             	xor    $0x1,%eax
  413b71:	66 41 0f 7e 04 b3    	movd   %xmm0,(%r11,%rsi,4)
  413b77:	0f b6 c0             	movzbl %al,%eax
  413b7a:	41 09 c6             	or     %eax,%r14d
  413b7d:	44 89 75 0c          	mov    %r14d,0xc(%rbp)
  413b81:	49 39 fd             	cmp    %rdi,%r13
  413b84:	0f 82 34 ff ff ff    	jb     413abe <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x3ce>
  413b8a:	44 0f b6 74 24 10    	movzbl 0x10(%rsp),%r14d
  413b90:	44 8b 64 24 18       	mov    0x18(%rsp),%r12d
  413b95:	48 c7 83 18 02 00 00 	movq   $0x0,0x218(%rbx)
  413b9c:	00 00 00 00 
  413ba0:	48 8b 44 24 08       	mov    0x8(%rsp),%rax
  413ba5:	48 01 44 24 68       	add    %rax,0x68(%rsp)
  413baa:	8b 83 f0 00 00 00    	mov    0xf0(%rbx),%eax
  413bb0:	45 84 f6             	test   %r14b,%r14b
  413bb3:	0f 84 a9 fb ff ff    	je     413762 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x72>
  413bb9:	49 89 db             	mov    %rbx,%r11
  413bbc:	44 89 f5             	mov    %r14d,%ebp
  413bbf:	8b 5c 24 5c          	mov    0x5c(%rsp),%ebx
  413bc3:	e9 90 08 00 00       	jmpq   414458 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xd68>
  413bc8:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  413bcf:	00 
  413bd0:	4c 89 ca             	mov    %r9,%rdx
  413bd3:	4c 89 d5             	mov    %r10,%rbp
  413bd6:	e9 a3 fd ff ff       	jmpq   41397e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x28e>
  413bdb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  413be0:	49 89 ff             	mov    %rdi,%r15
  413be3:	4c 89 ca             	mov    %r9,%rdx
  413be6:	e9 93 fd ff ff       	jmpq   41397e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x28e>
  413beb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  413bf0:	45 8b 12             	mov    (%r10),%r10d
  413bf3:	4c 8b 8b 60 01 00 00 	mov    0x160(%rbx),%r9
  413bfa:	43 0f 18 1c 91       	prefetcht2 (%r9,%r10,4)
  413bff:	e9 9c fc ff ff       	jmpq   4138a0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x1b0>
  413c04:	0f 1f 40 00          	nopl   0x0(%rax)
  413c08:	8b 32                	mov    (%rdx),%esi
  413c0a:	8b 42 08             	mov    0x8(%rdx),%eax
  413c0d:	4c 8d 6a 14          	lea    0x14(%rdx),%r13
  413c11:	4c 89 6c 24 38       	mov    %r13,0x38(%rsp)
  413c16:	48 c1 e6 02          	shl    $0x2,%rsi
  413c1a:	48 c1 e0 02          	shl    $0x2,%rax
  413c1e:	49 8d 7c 35 00       	lea    0x0(%r13,%rsi,1),%rdi
  413c23:	49 01 c5             	add    %rax,%r13
  413c26:	4c 89 6c 24 40       	mov    %r13,0x40(%rsp)
  413c2b:	48 39 c6             	cmp    %rax,%rsi
  413c2e:	0f 84 f6 04 00 00    	je     41412a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa3a>
  413c34:	4c 89 e8             	mov    %r13,%rax
  413c37:	eb 14                	jmp    413c4d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x55d>
  413c39:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  413c40:	48 83 c0 04          	add    $0x4,%rax
  413c44:	48 39 c7             	cmp    %rax,%rdi
  413c47:	0f 84 7b 04 00 00    	je     4140c8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9d8>
  413c4d:	8b 30                	mov    (%rax),%esi
  413c4f:	41 80 3c 34 00       	cmpb   $0x0,(%r12,%rsi,1)
  413c54:	49 89 f5             	mov    %rsi,%r13
  413c57:	78 e7                	js     413c40 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x550>
  413c59:	48 8b 7c 24 38       	mov    0x38(%rsp),%rdi
  413c5e:	48 89 c1             	mov    %rax,%rcx
  413c61:	48 8b b3 08 02 00 00 	mov    0x208(%rbx),%rsi
  413c68:	44 89 42 14          	mov    %r8d,0x14(%rdx)
  413c6c:	44 89 6a 18          	mov    %r13d,0x18(%rdx)
  413c70:	48 29 f9             	sub    %rdi,%rcx
  413c73:	8b 3c 24             	mov    (%rsp),%edi
  413c76:	48 c1 f9 02          	sar    $0x2,%rcx
  413c7a:	89 4a 08             	mov    %ecx,0x8(%rdx)
  413c7d:	48 8b 93 00 02 00 00 	mov    0x200(%rbx),%rdx
  413c84:	89 38                	mov    %edi,(%rax)
  413c86:	48 8b 8b 18 02 00 00 	mov    0x218(%rbx),%rcx
  413c8d:	48 89 f0             	mov    %rsi,%rax
  413c90:	48 29 d0             	sub    %rdx,%rax
  413c93:	48 8d 79 03          	lea    0x3(%rcx),%rdi
  413c97:	48 c1 f8 02          	sar    $0x2,%rax
  413c9b:	48 39 f8             	cmp    %rdi,%rax
  413c9e:	73 55                	jae    413cf5 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x605>
  413ca0:	48 01 c0             	add    %rax,%rax
  413ca3:	48 39 d6             	cmp    %rdx,%rsi
  413ca6:	ba 03 00 00 00       	mov    $0x3,%edx
  413cab:	4c 89 4c 24 28       	mov    %r9,0x28(%rsp)
  413cb0:	48 0f 44 c2          	cmove  %rdx,%rax
  413cb4:	4c 89 5c 24 20       	mov    %r11,0x20(%rsp)
  413cb9:	44 89 44 24 08       	mov    %r8d,0x8(%rsp)
  413cbe:	48 39 c7             	cmp    %rax,%rdi
  413cc1:	48 0f 43 c7          	cmovae %rdi,%rax
  413cc5:	48 8d bb 00 02 00 00 	lea    0x200(%rbx),%rdi
  413ccc:	48 89 c6             	mov    %rax,%rsi
  413ccf:	e8 ec 10 01 00       	callq  424dc0 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  413cd4:	4c 8b 4c 24 28       	mov    0x28(%rsp),%r9
  413cd9:	4c 8b 5c 24 20       	mov    0x20(%rsp),%r11
  413cde:	48 8b 8b 18 02 00 00 	mov    0x218(%rbx),%rcx
  413ce5:	48 8b 93 00 02 00 00 	mov    0x200(%rbx),%rdx
  413cec:	44 8b 44 24 08       	mov    0x8(%rsp),%r8d
  413cf1:	48 8d 79 03          	lea    0x3(%rcx),%rdi
  413cf5:	48 8d 04 8d 00 00 00 	lea    0x0(,%rcx,4),%rax
  413cfc:	00 
  413cfd:	44 89 2c 8a          	mov    %r13d,(%rdx,%rcx,4)
  413d01:	49 83 c7 02          	add    $0x2,%r15
  413d05:	44 89 44 02 04       	mov    %r8d,0x4(%rdx,%rax,1)
  413d0a:	48 89 bb 18 02 00 00 	mov    %rdi,0x218(%rbx)
  413d11:	44 89 74 02 08       	mov    %r14d,0x8(%rdx,%rax,1)
  413d16:	4c 89 ca             	mov    %r9,%rdx
  413d19:	e9 60 fc ff ff       	jmpq   41397e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x28e>
  413d1e:	66 90                	xchg   %ax,%ax
  413d20:	41 80 7e 0c 00       	cmpb   $0x0,0xc(%r14)
  413d25:	0f 84 68 fb ff ff    	je     413893 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x1a3>
  413d2b:	4c 8b 8b b8 00 00 00 	mov    0xb8(%rbx),%r9
  413d32:	44 8b 34 24          	mov    (%rsp),%r14d
  413d36:	45 89 34 81          	mov    %r14d,(%r9,%rax,4)
  413d3a:	4c 8b 74 24 18       	mov    0x18(%rsp),%r14
  413d3f:	45 8b 6a 04          	mov    0x4(%r10),%r13d
  413d43:	48 83 83 e8 18 00 00 	addq   $0x1,0x18e8(%rbx)
  413d4a:	01 
  413d4b:	45 8b 4e 08          	mov    0x8(%r14),%r9d
  413d4f:	45 85 ed             	test   %r13d,%r13d
  413d52:	0f 85 24 08 00 00    	jne    41457c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe8c>
  413d58:	4c 8b 93 88 00 00 00 	mov    0x88(%rbx),%r10
  413d5f:	66 41 0f 6e c0       	movd   %r8d,%xmm0
  413d64:	41 be 01 01 00 00    	mov    $0x101,%r14d
  413d6a:	41 c6 04 32 01       	movb   $0x1,(%r10,%rsi,1)
  413d6f:	48 8b b3 88 00 00 00 	mov    0x88(%rbx),%rsi
  413d76:	c6 04 3e ff          	movb   $0xff,(%rsi,%rdi,1)
  413d7a:	48 8b bb e8 00 00 00 	mov    0xe8(%rbx),%rdi
  413d81:	48 89 c6             	mov    %rax,%rsi
  413d84:	48 c1 e6 04          	shl    $0x4,%rsi
  413d88:	48 03 b3 a0 00 00 00 	add    0xa0(%rbx),%rsi
  413d8f:	66 0f 6e f7          	movd   %edi,%xmm6
  413d93:	44 89 4e 08          	mov    %r9d,0x8(%rsi)
  413d97:	66 0f 62 c6          	punpckldq %xmm6,%xmm0
  413d9b:	66 44 89 76 0c       	mov    %r14w,0xc(%rsi)
  413da0:	66 0f d6 06          	movq   %xmm0,(%rsi)
  413da4:	48 8d 77 01          	lea    0x1(%rdi),%rsi
  413da8:	48 89 b3 e8 00 00 00 	mov    %rsi,0xe8(%rbx)
  413daf:	48 8b b3 d0 00 00 00 	mov    0xd0(%rbx),%rsi
  413db6:	89 0c be             	mov    %ecx,(%rsi,%rdi,4)
  413db9:	48 8b b3 e0 1d 00 00 	mov    0x1de0(%rbx),%rsi
  413dc0:	48 85 f6             	test   %rsi,%rsi
  413dc3:	0f 85 5e fb ff ff    	jne    413927 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x237>
  413dc9:	e9 82 fb ff ff       	jmpq   413950 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x260>
  413dce:	66 90                	xchg   %ax,%ax
  413dd0:	4c 8b 93 70 01 00 00 	mov    0x170(%rbx),%r10
  413dd7:	48 8d bb 60 01 00 00 	lea    0x160(%rbx),%rdi
  413dde:	4d 39 da             	cmp    %r11,%r10
  413de1:	0f 84 3b 07 00 00    	je     414522 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe32>
  413de7:	4c 8b a3 68 01 00 00 	mov    0x168(%rbx),%r12
  413dee:	4d 29 da             	sub    %r11,%r10
  413df1:	89 f1                	mov    %esi,%ecx
  413df3:	4d 29 dc             	sub    %r11,%r12
  413df6:	44 8b 18             	mov    (%rax),%r11d
  413df9:	4d 89 e1             	mov    %r12,%r9
  413dfc:	49 c1 f9 02          	sar    $0x2,%r9
  413e00:	49 01 cb             	add    %rcx,%r11
  413e03:	4c 89 ca             	mov    %r9,%rdx
  413e06:	4d 39 cb             	cmp    %r9,%r11
  413e09:	0f 84 71 04 00 00    	je     414280 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb90>
  413e0f:	85 f6                	test   %esi,%esi
  413e11:	0f 84 01 03 00 00    	je     414118 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa28>
  413e17:	44 8d 24 36          	lea    (%rsi,%rsi,1),%r12d
  413e1b:	44 89 24 24          	mov    %r12d,(%rsp)
  413e1f:	4b 8d 34 21          	lea    (%r9,%r12,1),%rsi
  413e23:	49 c1 fa 02          	sar    $0x2,%r10
  413e27:	49 39 f2             	cmp    %rsi,%r10
  413e2a:	73 5e                	jae    413e8a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x79a>
  413e2c:	48 83 fe 03          	cmp    $0x3,%rsi
  413e30:	0f 86 99 07 00 00    	jbe    4145cf <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xedf>
  413e36:	b8 04 00 00 00       	mov    $0x4,%eax
  413e3b:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  413e40:	48 01 c0             	add    %rax,%rax
  413e43:	48 8d 50 ff          	lea    -0x1(%rax),%rdx
  413e47:	48 39 f2             	cmp    %rsi,%rdx
  413e4a:	72 f4                	jb     413e40 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x750>
  413e4c:	48 89 d6             	mov    %rdx,%rsi
  413e4f:	4c 89 44 24 30       	mov    %r8,0x30(%rsp)
  413e54:	48 89 7c 24 20       	mov    %rdi,0x20(%rsp)
  413e59:	66 0f 7e 44 24 28    	movd   %xmm0,0x28(%rsp)
  413e5f:	e8 9c 0e 01 00       	callq  424d00 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  413e64:	4c 8b 44 24 30       	mov    0x30(%rsp),%r8
  413e69:	66 0f 6e 44 24 28    	movd   0x28(%rsp),%xmm0
  413e6f:	48 8b 93 68 01 00 00 	mov    0x168(%rbx),%rdx
  413e76:	48 2b 93 60 01 00 00 	sub    0x160(%rbx),%rdx
  413e7d:	48 c1 fa 02          	sar    $0x2,%rdx
  413e81:	48 8b 7c 24 20       	mov    0x20(%rsp),%rdi
  413e86:	4a 8d 34 22          	lea    (%rdx,%r12,1),%rsi
  413e8a:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  413e8f:	89 54 24 20          	mov    %edx,0x20(%rsp)
  413e93:	48 89 54 24 28       	mov    %rdx,0x28(%rsp)
  413e98:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  413e9e:	e8 1d 0f 01 00       	callq  424dc0 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  413ea3:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  413ea8:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  413eaf:	48 8b 54 24 28       	mov    0x28(%rsp),%rdx
  413eb4:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  413ebb:	4b 8d 0c 02          	lea    (%r10,%r8,1),%rcx
  413ebf:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  413ec5:	89 d2                	mov    %edx,%edx
  413ec7:	8b 01                	mov    (%rcx),%eax
  413ec9:	49 8d 3c 93          	lea    (%r11,%rdx,4),%rdi
  413ecd:	8b 51 04             	mov    0x4(%rcx),%edx
  413ed0:	4d 8d 0c 83          	lea    (%r11,%rax,4),%r9
  413ed4:	48 89 d6             	mov    %rdx,%rsi
  413ed7:	48 c1 e2 02          	shl    $0x2,%rdx
  413edb:	48 83 fa 04          	cmp    $0x4,%rdx
  413edf:	0f 86 c2 06 00 00    	jbe    4145a7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xeb7>
  413ee5:	4c 89 ce             	mov    %r9,%rsi
  413ee8:	48 89 4c 24 30       	mov    %rcx,0x30(%rsp)
  413eed:	66 0f 7e 44 24 28    	movd   %xmm0,0x28(%rsp)
  413ef3:	e8 b8 e4 fe ff       	callq  4023b0 <memmove@plt>
  413ef8:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  413efd:	48 8b 4c 24 30       	mov    0x30(%rsp),%rcx
  413f02:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  413f09:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  413f10:	66 0f 6e 44 24 28    	movd   0x28(%rsp),%xmm0
  413f16:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  413f1a:	8b 70 04             	mov    0x4(%rax),%esi
  413f1d:	8b 7c 24 20          	mov    0x20(%rsp),%edi
  413f21:	48 8b 93 90 01 00 00 	mov    0x190(%rbx),%rdx
  413f28:	89 39                	mov    %edi,(%rcx)
  413f2a:	8b 3c 24             	mov    (%rsp),%edi
  413f2d:	89 79 08             	mov    %edi,0x8(%rcx)
  413f30:	49 8d 7c 14 ff       	lea    -0x1(%r12,%rdx,1),%rdi
  413f35:	e9 13 fc ff ff       	jmpq   413b4d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x45d>
  413f3a:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  413f40:	4c 8b 8b 70 01 00 00 	mov    0x170(%rbx),%r9
  413f47:	48 8d bb 60 01 00 00 	lea    0x160(%rbx),%rdi
  413f4e:	4d 39 d9             	cmp    %r11,%r9
  413f51:	0f 84 8d 05 00 00    	je     4144e4 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdf4>
  413f57:	4c 8b 93 68 01 00 00 	mov    0x168(%rbx),%r10
  413f5e:	4d 29 d9             	sub    %r11,%r9
  413f61:	89 d1                	mov    %edx,%ecx
  413f63:	4d 29 da             	sub    %r11,%r10
  413f66:	44 8b 5d 00          	mov    0x0(%rbp),%r11d
  413f6a:	4c 89 d6             	mov    %r10,%rsi
  413f6d:	48 c1 fe 02          	sar    $0x2,%rsi
  413f71:	49 01 cb             	add    %rcx,%r11
  413f74:	48 89 f0             	mov    %rsi,%rax
  413f77:	49 39 f3             	cmp    %rsi,%r11
  413f7a:	0f 84 58 03 00 00    	je     4142d8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xbe8>
  413f80:	85 d2                	test   %edx,%edx
  413f82:	0f 84 78 01 00 00    	je     414100 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa10>
  413f88:	8d 0c 12             	lea    (%rdx,%rdx,1),%ecx
  413f8b:	89 4c 24 20          	mov    %ecx,0x20(%rsp)
  413f8f:	48 89 0c 24          	mov    %rcx,(%rsp)
  413f93:	48 8b 0c 24          	mov    (%rsp),%rcx
  413f97:	49 c1 f9 02          	sar    $0x2,%r9
  413f9b:	48 01 ce             	add    %rcx,%rsi
  413f9e:	49 39 f1             	cmp    %rsi,%r9
  413fa1:	73 63                	jae    414006 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x916>
  413fa3:	48 83 fe 03          	cmp    $0x3,%rsi
  413fa7:	0f 86 18 06 00 00    	jbe    4145c5 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xed5>
  413fad:	b8 04 00 00 00       	mov    $0x4,%eax
  413fb2:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  413fb8:	48 01 c0             	add    %rax,%rax
  413fbb:	48 8d 50 ff          	lea    -0x1(%rax),%rdx
  413fbf:	48 39 f2             	cmp    %rsi,%rdx
  413fc2:	72 f4                	jb     413fb8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x8c8>
  413fc4:	48 89 d6             	mov    %rdx,%rsi
  413fc7:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  413fcc:	48 89 7c 24 28       	mov    %rdi,0x28(%rsp)
  413fd1:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  413fd7:	e8 24 0d 01 00       	callq  424d00 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  413fdc:	48 8b 83 68 01 00 00 	mov    0x168(%rbx),%rax
  413fe3:	48 8b 0c 24          	mov    (%rsp),%rcx
  413fe7:	48 2b 83 60 01 00 00 	sub    0x160(%rbx),%rax
  413fee:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  413ff3:	48 c1 f8 02          	sar    $0x2,%rax
  413ff7:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  413ffd:	48 8b 7c 24 28       	mov    0x28(%rsp),%rdi
  414002:	48 8d 34 08          	lea    (%rax,%rcx,1),%rsi
  414006:	4c 89 44 24 40       	mov    %r8,0x40(%rsp)
  41400b:	89 44 24 28          	mov    %eax,0x28(%rsp)
  41400f:	48 89 44 24 30       	mov    %rax,0x30(%rsp)
  414014:	66 0f 7e 44 24 38    	movd   %xmm0,0x38(%rsp)
  41401a:	e8 a1 0d 01 00       	callq  424dc0 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  41401f:	4c 8b 44 24 40       	mov    0x40(%rsp),%r8
  414024:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  41402b:	48 8b 44 24 30       	mov    0x30(%rsp),%rax
  414030:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  414037:	4f 8d 0c 02          	lea    (%r10,%r8,1),%r9
  41403b:	66 0f 6e 44 24 38    	movd   0x38(%rsp),%xmm0
  414041:	89 c0                	mov    %eax,%eax
  414043:	41 8b 51 04          	mov    0x4(%r9),%edx
  414047:	49 8d 3c 83          	lea    (%r11,%rax,4),%rdi
  41404b:	41 8b 01             	mov    (%r9),%eax
  41404e:	49 8d 34 83          	lea    (%r11,%rax,4),%rsi
  414052:	89 d0                	mov    %edx,%eax
  414054:	48 c1 e0 02          	shl    $0x2,%rax
  414058:	48 83 f8 04          	cmp    $0x4,%rax
  41405c:	0f 86 2f 05 00 00    	jbe    414591 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xea1>
  414062:	48 89 c2             	mov    %rax,%rdx
  414065:	4c 89 4c 24 40       	mov    %r9,0x40(%rsp)
  41406a:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  41406f:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  414075:	e8 36 e3 fe ff       	callq  4023b0 <memmove@plt>
  41407a:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  41407f:	4c 8b 4c 24 40       	mov    0x40(%rsp),%r9
  414084:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  41408b:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  414092:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  414098:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  41409c:	8b 50 04             	mov    0x4(%rax),%edx
  41409f:	8b 7c 24 28          	mov    0x28(%rsp),%edi
  4140a3:	48 8b b3 90 01 00 00 	mov    0x190(%rbx),%rsi
  4140aa:	41 89 39             	mov    %edi,(%r9)
  4140ad:	8b 7c 24 20          	mov    0x20(%rsp),%edi
  4140b1:	41 89 79 08          	mov    %edi,0x8(%r9)
  4140b5:	48 8b 3c 24          	mov    (%rsp),%rdi
  4140b9:	48 8d 7c 37 ff       	lea    -0x1(%rdi,%rsi,1),%rdi
  4140be:	e9 67 fa ff ff       	jmpq   413b2a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x43a>
  4140c3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  4140c8:	48 8b 7c 24 40       	mov    0x40(%rsp),%rdi
  4140cd:	48 8d 42 1c          	lea    0x1c(%rdx),%rax
  4140d1:	48 39 c7             	cmp    %rax,%rdi
  4140d4:	75 13                	jne    4140e9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9f9>
  4140d6:	eb 60                	jmp    414138 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa48>
  4140d8:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  4140df:	00 
  4140e0:	48 83 c0 04          	add    $0x4,%rax
  4140e4:	48 39 c7             	cmp    %rax,%rdi
  4140e7:	74 4f                	je     414138 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa48>
  4140e9:	8b 30                	mov    (%rax),%esi
  4140eb:	41 80 3c 34 00       	cmpb   $0x0,(%r12,%rsi,1)
  4140f0:	49 89 f5             	mov    %rsi,%r13
  4140f3:	78 eb                	js     4140e0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9f0>
  4140f5:	e9 5f fb ff ff       	jmpq   413c59 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x569>
  4140fa:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  414100:	48 c7 04 24 01 00 00 	movq   $0x1,(%rsp)
  414107:	00 
  414108:	c7 44 24 20 01 00 00 	movl   $0x1,0x20(%rsp)
  41410f:	00 
  414110:	e9 7e fe ff ff       	jmpq   413f93 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x8a3>
  414115:	0f 1f 00             	nopl   (%rax)
  414118:	c7 04 24 01 00 00 00 	movl   $0x1,(%rsp)
  41411f:	41 bc 01 00 00 00    	mov    $0x1,%r12d
  414125:	e9 f5 fc ff ff       	jmpq   413e1f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x72f>
  41412a:	48 8d 42 1c          	lea    0x1c(%rdx),%rax
  41412e:	4c 89 ef             	mov    %r13,%rdi
  414131:	48 39 44 24 40       	cmp    %rax,0x40(%rsp)
  414136:	75 b1                	jne    4140e9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9f9>
  414138:	80 7c 24 20 00       	cmpb   $0x0,0x20(%rsp)
  41413d:	0f 85 96 04 00 00    	jne    4145d9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xee9>
  414143:	48 03 8b 30 01 00 00 	add    0x130(%rbx),%rcx
  41414a:	44 89 c6             	mov    %r8d,%esi
  41414d:	8b 01                	mov    (%rcx),%eax
  41414f:	48 8d 51 14          	lea    0x14(%rcx),%rdx
  414153:	d1 ee                	shr    %esi
  414155:	31 c9                	xor    %ecx,%ecx
  414157:	48 c1 e0 02          	shl    $0x2,%rax
  41415b:	48 8d 3c 02          	lea    (%rdx,%rax,1),%rdi
  41415f:	74 2a                	je     41418b <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa9b>
  414161:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  414168:	8b 02                	mov    (%rdx),%eax
  41416a:	d1 e8                	shr    %eax
  41416c:	39 c6                	cmp    %eax,%esi
  41416e:	74 12                	je     414182 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa92>
  414170:	48 c1 e0 04          	shl    $0x4,%rax
  414174:	48 03 83 a0 00 00 00 	add    0xa0(%rbx),%rax
  41417b:	8b 00                	mov    (%rax),%eax
  41417d:	39 c1                	cmp    %eax,%ecx
  41417f:	0f 42 c8             	cmovb  %eax,%ecx
  414182:	48 83 c2 04          	add    $0x4,%rdx
  414186:	48 39 fa             	cmp    %rdi,%rdx
  414189:	75 dd                	jne    414168 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa78>
  41418b:	44 89 c0             	mov    %r8d,%eax
  41418e:	83 f0 01             	xor    $0x1,%eax
  414191:	48 89 c2             	mov    %rax,%rdx
  414194:	48 c1 e2 04          	shl    $0x4,%rdx
  414198:	48 03 93 78 01 00 00 	add    0x178(%rbx),%rdx
  41419f:	8b 6a 04             	mov    0x4(%rdx),%ebp
  4141a2:	85 ed                	test   %ebp,%ebp
  4141a4:	0f 85 c0 03 00 00    	jne    41456a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe7a>
  4141aa:	48 8b 93 88 00 00 00 	mov    0x88(%rbx),%rdx
  4141b1:	48 8b 7c 24 28       	mov    0x28(%rsp),%rdi
  4141b6:	c6 04 3a 01          	movb   $0x1,(%rdx,%rdi,1)
  4141ba:	48 8b 93 88 00 00 00 	mov    0x88(%rbx),%rdx
  4141c1:	31 ff                	xor    %edi,%edi
  4141c3:	c6 04 02 ff          	movb   $0xff,(%rdx,%rax,1)
  4141c7:	48 89 f0             	mov    %rsi,%rax
  4141ca:	48 8b 93 e8 00 00 00 	mov    0xe8(%rbx),%rdx
  4141d1:	48 c1 e0 04          	shl    $0x4,%rax
  4141d5:	48 03 83 a0 00 00 00 	add    0xa0(%rbx),%rax
  4141dc:	89 08                	mov    %ecx,(%rax)
  4141de:	89 50 04             	mov    %edx,0x4(%rax)
  4141e1:	44 89 70 08          	mov    %r14d,0x8(%rax)
  4141e5:	66 89 78 0c          	mov    %di,0xc(%rax)
  4141e9:	48 8d 42 01          	lea    0x1(%rdx),%rax
  4141ed:	48 89 83 e8 00 00 00 	mov    %rax,0xe8(%rbx)
  4141f4:	48 8b 83 d0 00 00 00 	mov    0xd0(%rbx),%rax
  4141fb:	44 89 04 90          	mov    %r8d,(%rax,%rdx,4)
  4141ff:	85 c9                	test   %ecx,%ecx
  414201:	0f 84 c0 02 00 00    	je     4144c7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdd7>
  414207:	48 8b 83 e0 1d 00 00 	mov    0x1de0(%rbx),%rax
  41420e:	48 85 c0             	test   %rax,%rax
  414211:	74 2d                	je     414240 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb50>
  414213:	80 bb e8 1d 00 00 00 	cmpb   $0x0,0x1de8(%rbx)
  41421a:	75 24                	jne    414240 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb50>
  41421c:	80 bb 20 02 00 00 00 	cmpb   $0x0,0x220(%rbx)
  414223:	75 1b                	jne    414240 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb50>
  414225:	44 89 c2             	mov    %r8d,%edx
  414228:	48 8b 00             	mov    (%rax),%rax
  41422b:	83 e2 01             	and    $0x1,%edx
  41422e:	80 fa 01             	cmp    $0x1,%dl
  414231:	19 d2                	sbb    %edx,%edx
  414233:	83 e2 02             	and    $0x2,%edx
  414236:	83 ea 01             	sub    $0x1,%edx
  414239:	88 14 30             	mov    %dl,(%rax,%rsi,1)
  41423c:	0f 1f 40 00          	nopl   0x0(%rax)
  414240:	48 8b 93 48 1d 00 00 	mov    0x1d48(%rbx),%rdx
  414247:	48 8b 83 50 1d 00 00 	mov    0x1d50(%rbx),%rax
  41424e:	83 ab 1c 01 00 00 01 	subl   $0x1,0x11c(%rbx)
  414255:	48 29 d0             	sub    %rdx,%rax
  414258:	48 39 c6             	cmp    %rax,%rsi
  41425b:	0f 82 57 02 00 00    	jb     4144b8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdc8>
  414261:	83 ab 20 01 00 00 01 	subl   $0x1,0x120(%rbx)
  414268:	49 83 c7 02          	add    $0x2,%r15
  41426c:	4c 89 ca             	mov    %r9,%rdx
  41426f:	4c 89 d5             	mov    %r10,%rbp
  414272:	e9 07 f7 ff ff       	jmpq   41397e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x28e>
  414277:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  41427e:	00 00 
  414280:	4d 39 d4             	cmp    %r10,%r12
  414283:	0f 83 86 fb ff ff    	jae    413e0f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x71f>
  414289:	49 8d 73 01          	lea    0x1(%r11),%rsi
  41428d:	48 89 44 24 28       	mov    %rax,0x28(%rsp)
  414292:	4c 89 44 24 20       	mov    %r8,0x20(%rsp)
  414297:	66 0f 7e 04 24       	movd   %xmm0,(%rsp)
  41429c:	e8 1f 0b 01 00       	callq  424dc0 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  4142a1:	48 8b 44 24 28       	mov    0x28(%rsp),%rax
  4142a6:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  4142ad:	4c 8b 44 24 20       	mov    0x20(%rsp),%r8
  4142b2:	48 8b bb 90 01 00 00 	mov    0x190(%rbx),%rdi
  4142b9:	83 40 08 01          	addl   $0x1,0x8(%rax)
  4142bd:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  4142c4:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  4142c8:	66 0f 6e 04 24       	movd   (%rsp),%xmm0
  4142cd:	8b 70 04             	mov    0x4(%rax),%esi
  4142d0:	e9 78 f8 ff ff       	jmpq   413b4d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x45d>
  4142d5:	0f 1f 00             	nopl   (%rax)
  4142d8:	4d 39 ca             	cmp    %r9,%r10
  4142db:	0f 83 9f fc ff ff    	jae    413f80 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x890>
  4142e1:	49 8d 73 01          	lea    0x1(%r11),%rsi
  4142e5:	4c 89 44 24 20       	mov    %r8,0x20(%rsp)
  4142ea:	66 0f 7e 04 24       	movd   %xmm0,(%rsp)
  4142ef:	e8 cc 0a 01 00       	callq  424dc0 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  4142f4:	4c 8b 93 78 01 00 00 	mov    0x178(%rbx),%r10
  4142fb:	83 45 08 01          	addl   $0x1,0x8(%rbp)
  4142ff:	4c 8b 44 24 20       	mov    0x20(%rsp),%r8
  414304:	48 8b bb 90 01 00 00 	mov    0x190(%rbx),%rdi
  41430b:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  414312:	66 0f 6e 04 24       	movd   (%rsp),%xmm0
  414317:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  41431b:	8b 50 04             	mov    0x4(%rax),%edx
  41431e:	e9 07 f8 ff ff       	jmpq   413b2a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x43a>
  414323:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  414328:	8b 04 24             	mov    (%rsp),%eax
  41432b:	4c 8b 6c 24 50       	mov    0x50(%rsp),%r13
  414330:	4c 89 7c 24 08       	mov    %r15,0x8(%rsp)
  414335:	4d 89 d1             	mov    %r10,%r9
  414338:	4c 8b 7c 24 48       	mov    0x48(%rsp),%r15
  41433d:	44 8b 64 24 58       	mov    0x58(%rsp),%r12d
  414342:	89 8b dc 02 00 00    	mov    %ecx,0x2dc(%rbx)
  414348:	c6 83 d1 02 00 00 01 	movb   $0x1,0x2d1(%rbx)
  41434f:	c7 83 d4 02 00 00 ff 	movl   $0xffffffff,0x2d4(%rbx)
  414356:	ff ff ff 
  414359:	89 83 d8 02 00 00    	mov    %eax,0x2d8(%rbx)
  41435f:	4d 89 ca             	mov    %r9,%r10
  414362:	4b 8d 04 39          	lea    (%r9,%r15,1),%rax
  414366:	48 39 54 24 30       	cmp    %rdx,0x30(%rsp)
  41436b:	0f 84 97 02 00 00    	je     414608 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf18>
  414371:	48 8b 74 24 30       	mov    0x30(%rsp),%rsi
  414376:	48 29 d6             	sub    %rdx,%rsi
  414379:	48 8d 7e fc          	lea    -0x4(%rsi),%rdi
  41437d:	48 83 ff 08          	cmp    $0x8,%rdi
  414381:	0f 86 c1 00 00 00    	jbe    414448 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xd58>
  414387:	48 8d 4a 04          	lea    0x4(%rdx),%rcx
  41438b:	49 89 e8             	mov    %rbp,%r8
  41438e:	49 29 c8             	sub    %rcx,%r8
  414391:	31 c9                	xor    %ecx,%ecx
  414393:	49 83 f8 08          	cmp    $0x8,%r8
  414397:	77 37                	ja     4143d0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xce0>
  414399:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  4143a0:	8b 3c 0a             	mov    (%rdx,%rcx,1),%edi
  4143a3:	89 7c 0d 00          	mov    %edi,0x0(%rbp,%rcx,1)
  4143a7:	48 83 c1 04          	add    $0x4,%rcx
  4143ab:	48 39 f1             	cmp    %rsi,%rcx
  4143ae:	75 f0                	jne    4143a0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcb0>
  4143b0:	48 8d 54 35 00       	lea    0x0(%rbp,%rsi,1),%rdx
  4143b5:	8b 48 04             	mov    0x4(%rax),%ecx
  4143b8:	41 be 01 00 00 00    	mov    $0x1,%r14d
  4143be:	4c 29 ea             	sub    %r13,%rdx
  4143c1:	48 c1 fa 02          	sar    $0x2,%rdx
  4143c5:	e9 86 f6 ff ff       	jmpq   413a50 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x360>
  4143ca:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  4143d0:	48 c1 ef 02          	shr    $0x2,%rdi
  4143d4:	48 83 c7 01          	add    $0x1,%rdi
  4143d8:	49 89 f8             	mov    %rdi,%r8
  4143db:	49 c1 e8 02          	shr    $0x2,%r8
  4143df:	49 c1 e0 04          	shl    $0x4,%r8
  4143e3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  4143e8:	f3 0f 6f 1c 0a       	movdqu (%rdx,%rcx,1),%xmm3
  4143ed:	0f 11 5c 0d 00       	movups %xmm3,0x0(%rbp,%rcx,1)
  4143f2:	48 83 c1 10          	add    $0x10,%rcx
  4143f6:	4c 39 c1             	cmp    %r8,%rcx
  4143f9:	75 ed                	jne    4143e8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcf8>
  4143fb:	40 f6 c7 03          	test   $0x3,%dil
  4143ff:	74 af                	je     4143b0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcc0>
  414401:	48 83 e7 fc          	and    $0xfffffffffffffffc,%rdi
  414405:	4c 8b 7c 24 30       	mov    0x30(%rsp),%r15
  41440a:	48 8d 0c bd 00 00 00 	lea    0x0(,%rdi,4),%rcx
  414411:	00 
  414412:	48 01 ca             	add    %rcx,%rdx
  414415:	48 01 e9             	add    %rbp,%rcx
  414418:	8b 3a                	mov    (%rdx),%edi
  41441a:	89 39                	mov    %edi,(%rcx)
  41441c:	48 8d 7a 04          	lea    0x4(%rdx),%rdi
  414420:	49 39 ff             	cmp    %rdi,%r15
  414423:	74 8b                	je     4143b0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcc0>
  414425:	8b 7a 04             	mov    0x4(%rdx),%edi
  414428:	89 79 04             	mov    %edi,0x4(%rcx)
  41442b:	48 8d 7a 08          	lea    0x8(%rdx),%rdi
  41442f:	49 39 ff             	cmp    %rdi,%r15
  414432:	0f 84 78 ff ff ff    	je     4143b0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcc0>
  414438:	8b 52 08             	mov    0x8(%rdx),%edx
  41443b:	89 51 08             	mov    %edx,0x8(%rcx)
  41443e:	e9 6d ff ff ff       	jmpq   4143b0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcc0>
  414443:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  414448:	31 c9                	xor    %ecx,%ecx
  41444a:	e9 51 ff ff ff       	jmpq   4143a0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcb0>
  41444f:	49 89 db             	mov    %rbx,%r11
  414452:	8b 5c 24 5c          	mov    0x5c(%rsp),%ebx
  414456:	31 ed                	xor    %ebp,%ebp
  414458:	f3 41 0f 6f ab 78 06 	movdqu 0x678(%r11),%xmm5
  41445f:	00 00 
  414461:	48 8b 54 24 68       	mov    0x68(%rsp),%rdx
  414466:	89 c7                	mov    %eax,%edi
  414468:	29 df                	sub    %ebx,%edi
  41446a:	41 80 bb d8 1d 00 00 	cmpb   $0x0,0x1dd8(%r11)
  414471:	00 
  414472:	66 48 0f 6e c7       	movq   %rdi,%xmm0
  414477:	66 48 0f 6e e2       	movq   %rdx,%xmm4
  41447c:	66 0f 6c c4          	punpcklqdq %xmm4,%xmm0
  414480:	66 0f d4 c5          	paddq  %xmm5,%xmm0
  414484:	41 0f 11 83 78 06 00 	movups %xmm0,0x678(%r11)
  41448b:	00 
  41448c:	75 0a                	jne    414498 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xda8>
  41448e:	41 80 bb 24 01 00 00 	cmpb   $0x0,0x124(%r11)
  414495:	00 
  414496:	74 43                	je     4144db <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdeb>
  414498:	49 8b 83 d0 1d 00 00 	mov    0x1dd0(%r11),%rax
  41449f:	48 85 c0             	test   %rax,%rax
  4144a2:	74 03                	je     4144a7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdb7>
  4144a4:	48 01 10             	add    %rdx,(%rax)
  4144a7:	48 83 c4 78          	add    $0x78,%rsp
  4144ab:	89 e8                	mov    %ebp,%eax
  4144ad:	5b                   	pop    %rbx
  4144ae:	5d                   	pop    %rbp
  4144af:	41 5c                	pop    %r12
  4144b1:	41 5d                	pop    %r13
  4144b3:	41 5e                	pop    %r14
  4144b5:	41 5f                	pop    %r15
  4144b7:	c3                   	retq   
  4144b8:	80 3c 32 00          	cmpb   $0x0,(%rdx,%rsi,1)
  4144bc:	0f 85 a6 fd ff ff    	jne    414268 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb78>
  4144c2:	e9 9a fd ff ff       	jmpq   414261 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb71>
  4144c7:	48 83 83 98 06 00 00 	addq   $0x1,0x698(%rbx)
  4144ce:	01 
  4144cf:	83 83 54 1c 00 00 01 	addl   $0x1,0x1c54(%rbx)
  4144d6:	e9 2c fd ff ff       	jmpq   414207 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb17>
  4144db:	49 01 93 88 06 00 00 	add    %rdx,0x688(%r11)
  4144e2:	eb b4                	jmp    414498 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xda8>
  4144e4:	be 03 00 00 00       	mov    $0x3,%esi
  4144e9:	4c 89 44 24 28       	mov    %r8,0x28(%rsp)
  4144ee:	48 89 3c 24          	mov    %rdi,(%rsp)
  4144f2:	66 0f 7e 44 24 20    	movd   %xmm0,0x20(%rsp)
  4144f8:	e8 03 08 01 00       	callq  424d00 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  4144fd:	8b 55 04             	mov    0x4(%rbp),%edx
  414500:	4c 8b 44 24 28       	mov    0x28(%rsp),%r8
  414505:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  41450c:	4c 8b 8b 70 01 00 00 	mov    0x170(%rbx),%r9
  414513:	66 0f 6e 44 24 20    	movd   0x20(%rsp),%xmm0
  414519:	48 8b 3c 24          	mov    (%rsp),%rdi
  41451d:	e9 35 fa ff ff       	jmpq   413f57 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x867>
  414522:	be 03 00 00 00       	mov    $0x3,%esi
  414527:	48 89 44 24 30       	mov    %rax,0x30(%rsp)
  41452c:	4c 89 44 24 28       	mov    %r8,0x28(%rsp)
  414531:	48 89 3c 24          	mov    %rdi,(%rsp)
  414535:	66 0f 7e 44 24 20    	movd   %xmm0,0x20(%rsp)
  41453b:	e8 c0 07 01 00       	callq  424d00 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  414540:	48 8b 44 24 30       	mov    0x30(%rsp),%rax
  414545:	4c 8b 44 24 28       	mov    0x28(%rsp),%r8
  41454a:	4c 8b 9b 60 01 00 00 	mov    0x160(%rbx),%r11
  414551:	4c 8b 93 70 01 00 00 	mov    0x170(%rbx),%r10
  414558:	8b 70 04             	mov    0x4(%rax),%esi
  41455b:	66 0f 6e 44 24 20    	movd   0x20(%rsp),%xmm0
  414561:	48 8b 3c 24          	mov    (%rsp),%rdi
  414565:	e9 7d f8 ff ff       	jmpq   413de7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x6f7>
  41456a:	8b 3a                	mov    (%rdx),%edi
  41456c:	48 8b 93 60 01 00 00 	mov    0x160(%rbx),%rdx
  414573:	0f 18 1c ba          	prefetcht2 (%rdx,%rdi,4)
  414577:	e9 2e fc ff ff       	jmpq   4141aa <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xaba>
  41457c:	45 8b 12             	mov    (%r10),%r10d
  41457f:	4c 8b ab 60 01 00 00 	mov    0x160(%rbx),%r13
  414586:	43 0f 18 5c 95 00    	prefetcht2 0x0(%r13,%r10,4)
  41458c:	e9 c7 f7 ff ff       	jmpq   413d58 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x668>
  414591:	4c 89 c8             	mov    %r9,%rax
  414594:	0f 85 05 fb ff ff    	jne    41409f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9af>
  41459a:	8b 16                	mov    (%rsi),%edx
  41459c:	89 17                	mov    %edx,(%rdi)
  41459e:	41 8b 51 04          	mov    0x4(%r9),%edx
  4145a2:	e9 f8 fa ff ff       	jmpq   41409f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9af>
  4145a7:	48 89 c8             	mov    %rcx,%rax
  4145aa:	0f 85 6d f9 ff ff    	jne    413f1d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x82d>
  4145b0:	41 8b 11             	mov    (%r9),%edx
  4145b3:	89 17                	mov    %edx,(%rdi)
  4145b5:	8b 71 04             	mov    0x4(%rcx),%esi
  4145b8:	e9 60 f9 ff ff       	jmpq   413f1d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x82d>
  4145bd:	45 31 f6             	xor    %r14d,%r14d
  4145c0:	e9 8b f4 ff ff       	jmpq   413a50 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x360>
  4145c5:	ba 03 00 00 00       	mov    $0x3,%edx
  4145ca:	e9 f5 f9 ff ff       	jmpq   413fc4 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x8d4>
  4145cf:	ba 03 00 00 00       	mov    $0x3,%edx
  4145d4:	e9 73 f8 ff ff       	jmpq   413e4c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x75c>
  4145d9:	4c 89 ca             	mov    %r9,%rdx
  4145dc:	4c 8b 7c 24 48       	mov    0x48(%rsp),%r15
  4145e1:	4c 8b 6c 24 50       	mov    0x50(%rsp),%r13
  4145e6:	4c 89 d5             	mov    %r10,%rbp
  4145e9:	c6 83 d1 02 00 00 01 	movb   $0x1,0x2d1(%rbx)
  4145f0:	44 8b 64 24 58       	mov    0x58(%rsp),%r12d
  4145f5:	44 89 b3 d4 02 00 00 	mov    %r14d,0x2d4(%rbx)
  4145fc:	4c 8b 8b 78 01 00 00 	mov    0x178(%rbx),%r9
  414603:	e9 57 fd ff ff       	jmpq   41435f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc6f>
  414608:	41 be 01 00 00 00    	mov    $0x1,%r14d
  41460e:	e9 2f f4 ff ff       	jmpq   413a42 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x352>

Disassembly of section .fini:
