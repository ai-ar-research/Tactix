
/home/hwu/SAT-solving/build/pin-5672-assignment-level-release/tactix:     file format elf64-x86-64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004136d0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)>:
  4136d0:	41 57                	push   %r15
  4136d2:	49 89 ff             	mov    %rdi,%r15
  4136d5:	41 56                	push   %r14
  4136d7:	41 55                	push   %r13
  4136d9:	41 54                	push   %r12
  4136db:	55                   	push   %rbp
  4136dc:	53                   	push   %rbx
  4136dd:	48 81 ec 88 00 00 00 	sub    $0x88,%rsp
  4136e4:	8b 87 f4 00 00 00    	mov    0xf4(%rdi),%eax
  4136ea:	44 8b a7 f0 00 00 00 	mov    0xf0(%rdi),%r12d
  4136f1:	83 fe ff             	cmp    $0xffffffff,%esi
  4136f4:	0f 84 d6 00 00 00    	je     4137d0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x100>
  4136fa:	89 f5                	mov    %esi,%ebp
  4136fc:	44 39 e0             	cmp    %r12d,%eax
  4136ff:	0f 82 bd 00 00 00    	jb     4137c2 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf2>
  413705:	41 c7 87 f4 00 00 00 	movl   $0xffffffff,0xf4(%r15)
  41370c:	ff ff ff ff 
  413710:	44 89 e3             	mov    %r12d,%ebx
  413713:	48 c7 44 24 78 00 00 	movq   $0x0,0x78(%rsp)
  41371a:	00 00 
  41371c:	eb 26                	jmp    413744 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x74>
  41371e:	66 90                	xchg   %ax,%ax
  413720:	49 8b 97 d0 00 00 00 	mov    0xd0(%r15),%rdx
  413727:	48 8d 4c 24 78       	lea    0x78(%rsp),%rcx
  41372c:	4c 89 ff             	mov    %r15,%rdi
  41372f:	83 c3 01             	add    $0x1,%ebx
  413732:	8b 34 82             	mov    (%rdx,%rax,4),%esi
  413735:	89 ea                	mov    %ebp,%edx
  413737:	e8 74 f0 ff ff       	callq  4127b0 <bool tactix::propagate_literal<false, false, true>(tactix::Kernel&, unsigned int, unsigned int, unsigned long&)>
  41373c:	84 c0                	test   %al,%al
  41373e:	0f 85 bf 0e 00 00    	jne    414603 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf33>
  413744:	89 d8                	mov    %ebx,%eax
  413746:	49 3b 87 e8 00 00 00 	cmp    0xe8(%r15),%rax
  41374d:	72 d1                	jb     413720 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x50>
  41374f:	45 31 c0             	xor    %r8d,%r8d
  413752:	48 8b 54 24 78       	mov    0x78(%rsp),%rdx
  413757:	89 d8                	mov    %ebx,%eax
  413759:	41 89 9f f0 00 00 00 	mov    %ebx,0xf0(%r15)
  413760:	f3 41 0f 6f af 78 06 	movdqu 0x678(%r15),%xmm5
  413767:	00 00 
  413769:	44 29 e0             	sub    %r12d,%eax
  41376c:	41 80 bf d8 1d 00 00 	cmpb   $0x0,0x1dd8(%r15)
  413773:	00 
  413774:	66 48 0f 6e c0       	movq   %rax,%xmm0
  413779:	66 48 0f 6e e2       	movq   %rdx,%xmm4
  41377e:	66 0f 6c c4          	punpcklqdq %xmm4,%xmm0
  413782:	66 0f d4 c5          	paddq  %xmm5,%xmm0
  413786:	41 0f 11 87 78 06 00 	movups %xmm0,0x678(%r15)
  41378d:	00 
  41378e:	75 0e                	jne    41379e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xce>
  413790:	41 80 bf 24 01 00 00 	cmpb   $0x0,0x124(%r15)
  413797:	00 
  413798:	0f 84 99 0e 00 00    	je     414637 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf67>
  41379e:	49 8b 87 d0 1d 00 00 	mov    0x1dd0(%r15),%rax
  4137a5:	48 85 c0             	test   %rax,%rax
  4137a8:	74 03                	je     4137ad <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdd>
  4137aa:	48 01 10             	add    %rdx,(%rax)
  4137ad:	48 81 c4 88 00 00 00 	add    $0x88,%rsp
  4137b4:	44 89 c0             	mov    %r8d,%eax
  4137b7:	5b                   	pop    %rbx
  4137b8:	5d                   	pop    %rbp
  4137b9:	41 5c                	pop    %r12
  4137bb:	41 5d                	pop    %r13
  4137bd:	41 5e                	pop    %r14
  4137bf:	41 5f                	pop    %r15
  4137c1:	c3                   	retq   
  4137c2:	89 87 f0 00 00 00    	mov    %eax,0xf0(%rdi)
  4137c8:	41 89 c4             	mov    %eax,%r12d
  4137cb:	e9 35 ff ff ff       	jmpq   413705 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x35>
  4137d0:	44 39 e0             	cmp    %r12d,%eax
  4137d3:	0f 82 50 0e 00 00    	jb     414629 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf59>
  4137d9:	41 c7 87 f4 00 00 00 	movl   $0xffffffff,0xf4(%r15)
  4137e0:	ff ff ff ff 
  4137e4:	44 89 e5             	mov    %r12d,%ebp
  4137e7:	48 c7 44 24 60 00 00 	movq   $0x0,0x60(%rsp)
  4137ee:	00 00 
  4137f0:	44 89 64 24 6c       	mov    %r12d,0x6c(%rsp)
  4137f5:	0f 1f 00             	nopl   (%rax)
  4137f8:	89 e8                	mov    %ebp,%eax
  4137fa:	49 3b 87 e8 00 00 00 	cmp    0xe8(%r15),%rax
  413801:	0f 83 7d 0e 00 00    	jae    414684 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xfb4>
  413807:	49 8b 97 d0 00 00 00 	mov    0xd0(%r15),%rdx
  41380e:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  413815:	83 c5 01             	add    $0x1,%ebp
  413818:	4d 8b 87 60 01 00 00 	mov    0x160(%r15),%r8
  41381f:	49 8b 9f 30 01 00 00 	mov    0x130(%r15),%rbx
  413826:	8b 3c 82             	mov    (%rdx,%rax,4),%edi
  413829:	4d 8b b7 88 00 00 00 	mov    0x88(%r15),%r14
  413830:	48 89 5c 24 18       	mov    %rbx,0x18(%rsp)
  413835:	89 f8                	mov    %edi,%eax
  413837:	d1 ef                	shr    %edi
  413839:	83 f0 01             	xor    $0x1,%eax
  41383c:	48 c1 e7 04          	shl    $0x4,%rdi
  413840:	49 03 bf a0 00 00 00 	add    0xa0(%r15),%rdi
  413847:	89 44 24 0c          	mov    %eax,0xc(%rsp)
  41384b:	48 c1 e0 04          	shl    $0x4,%rax
  41384f:	48 89 44 24 48       	mov    %rax,0x48(%rsp)
  413854:	4c 01 d0             	add    %r10,%rax
  413857:	44 8b 08             	mov    (%rax),%r9d
  41385a:	8b 50 04             	mov    0x4(%rax),%edx
  41385d:	48 89 7c 24 20       	mov    %rdi,0x20(%rsp)
  413862:	48 89 d1             	mov    %rdx,%rcx
  413865:	4c 01 ca             	add    %r9,%rdx
  413868:	4a 8d 34 8d 00 00 00 	lea    0x0(,%r9,4),%rsi
  41386f:	00 
  413870:	48 c1 e2 02          	shl    $0x2,%rdx
  413874:	4d 8d 24 30          	lea    (%r8,%rsi,1),%r12
  413878:	4d 8d 1c 10          	lea    (%r8,%rdx,1),%r11
  41387c:	48 29 f2             	sub    %rsi,%rdx
  41387f:	48 c1 fa 02          	sar    $0x2,%rdx
  413883:	48 8d 72 1f          	lea    0x1f(%rdx),%rsi
  413887:	48 c1 ee 05          	shr    $0x5,%rsi
  41388b:	48 8d 7e 01          	lea    0x1(%rsi),%rdi
  41388f:	48 89 7c 24 10       	mov    %rdi,0x10(%rsp)
  413894:	4d 39 dc             	cmp    %r11,%r12
  413897:	0f 84 f6 0d 00 00    	je     414693 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xfc3>
  41389d:	4c 89 64 24 50       	mov    %r12,0x50(%rsp)
  4138a2:	4c 89 e3             	mov    %r12,%rbx
  4138a5:	4c 89 e2             	mov    %r12,%rdx
  4138a8:	49 89 fc             	mov    %rdi,%r12
  4138ab:	89 6c 24 68          	mov    %ebp,0x68(%rsp)
  4138af:	e9 55 01 00 00       	jmpq   413a09 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x339>
  4138b4:	0f 1f 40 00          	nopl   0x0(%rax)
  4138b8:	48 83 c2 04          	add    $0x4,%rdx
  4138bc:	48 83 c3 04          	add    $0x4,%rbx
  4138c0:	45 84 c0             	test   %r8b,%r8b
  4138c3:	0f 8f 37 01 00 00    	jg     413a00 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  4138c9:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  4138d0:	0f 85 7a 0b 00 00    	jne    414450 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xd80>
  4138d6:	89 cf                	mov    %ecx,%edi
  4138d8:	48 8b 6c 24 20       	mov    0x20(%rsp),%rbp
  4138dd:	c1 e8 02             	shr    $0x2,%eax
  4138e0:	83 f7 01             	xor    $0x1,%edi
  4138e3:	49 89 f9             	mov    %rdi,%r9
  4138e6:	44 8b 45 00          	mov    0x0(%rbp),%r8d
  4138ea:	49 c1 e1 04          	shl    $0x4,%r9
  4138ee:	4d 01 ca             	add    %r9,%r10
  4138f1:	41 80 bf 6d 1e 00 00 	cmpb   $0x0,0x1e6d(%r15)
  4138f8:	00 
  4138f9:	74 13                	je     41390e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x23e>
  4138fb:	41 80 bf 6e 1e 00 00 	cmpb   $0x0,0x1e6e(%r15)
  413902:	00 
  413903:	74 09                	je     41390e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x23e>
  413905:	45 85 c0             	test   %r8d,%r8d
  413908:	0f 85 fa 04 00 00    	jne    413e08 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x738>
  41390e:	41 8b 6a 04          	mov    0x4(%r10),%ebp
  413912:	85 ed                	test   %ebp,%ebp
  413914:	0f 85 ae 03 00 00    	jne    413cc8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5f8>
  41391a:	4d 8b 8f 88 00 00 00 	mov    0x88(%r15),%r9
  413921:	66 41 0f 6e c0       	movd   %r8d,%xmm0
  413926:	41 c6 04 31 01       	movb   $0x1,(%r9,%rsi,1)
  41392b:	49 8b b7 88 00 00 00 	mov    0x88(%r15),%rsi
  413932:	44 8b 4c 24 0c       	mov    0xc(%rsp),%r9d
  413937:	c6 04 3e ff          	movb   $0xff,(%rsi,%rdi,1)
  41393b:	48 89 c6             	mov    %rax,%rsi
  41393e:	49 8b bf e8 00 00 00 	mov    0xe8(%r15),%rdi
  413945:	48 c1 e6 04          	shl    $0x4,%rsi
  413949:	49 03 b7 a0 00 00 00 	add    0xa0(%r15),%rsi
  413950:	44 89 4e 08          	mov    %r9d,0x8(%rsi)
  413954:	66 0f 6e d7          	movd   %edi,%xmm2
  413958:	44 0f b7 0d 80 ca 0f 	movzwl 0xfca80(%rip),%r9d        # 5103e0 <tactix::BinaryIdMap::EMPTY+0x18>
  41395f:	00 
  413960:	66 0f 62 c2          	punpckldq %xmm2,%xmm0
  413964:	66 44 89 4e 0c       	mov    %r9w,0xc(%rsi)
  413969:	66 0f d6 06          	movq   %xmm0,(%rsi)
  41396d:	48 8d 77 01          	lea    0x1(%rdi),%rsi
  413971:	49 89 b7 e8 00 00 00 	mov    %rsi,0xe8(%r15)
  413978:	49 8b b7 d0 00 00 00 	mov    0xd0(%r15),%rsi
  41397f:	89 0c be             	mov    %ecx,(%rsi,%rdi,4)
  413982:	45 85 c0             	test   %r8d,%r8d
  413985:	75 10                	jne    413997 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x2c7>
  413987:	49 83 87 98 06 00 00 	addq   $0x1,0x698(%r15)
  41398e:	01 
  41398f:	41 83 87 54 1c 00 00 	addl   $0x1,0x1c54(%r15)
  413996:	01 
  413997:	49 8b b7 e0 1d 00 00 	mov    0x1de0(%r15),%rsi
  41399e:	48 85 f6             	test   %rsi,%rsi
  4139a1:	74 2d                	je     4139d0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x300>
  4139a3:	41 80 bf e8 1d 00 00 	cmpb   $0x0,0x1de8(%r15)
  4139aa:	00 
  4139ab:	75 23                	jne    4139d0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x300>
  4139ad:	41 80 bf 20 02 00 00 	cmpb   $0x0,0x220(%r15)
  4139b4:	00 
  4139b5:	75 19                	jne    4139d0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x300>
  4139b7:	83 e1 01             	and    $0x1,%ecx
  4139ba:	48 8b 36             	mov    (%rsi),%rsi
  4139bd:	80 f9 01             	cmp    $0x1,%cl
  4139c0:	19 c9                	sbb    %ecx,%ecx
  4139c2:	83 e1 02             	and    $0x2,%ecx
  4139c5:	83 e9 01             	sub    $0x1,%ecx
  4139c8:	88 0c 06             	mov    %cl,(%rsi,%rax,1)
  4139cb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  4139d0:	49 8b b7 48 1d 00 00 	mov    0x1d48(%r15),%rsi
  4139d7:	49 8b 8f 50 1d 00 00 	mov    0x1d50(%r15),%rcx
  4139de:	41 83 af 1c 01 00 00 	subl   $0x1,0x11c(%r15)
  4139e5:	01 
  4139e6:	48 29 f1             	sub    %rsi,%rcx
  4139e9:	48 39 c8             	cmp    %rcx,%rax
  4139ec:	73 06                	jae    4139f4 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x324>
  4139ee:	80 3c 06 00          	cmpb   $0x0,(%rsi,%rax,1)
  4139f2:	75 08                	jne    4139fc <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x32c>
  4139f4:	41 83 af 20 01 00 00 	subl   $0x1,0x120(%r15)
  4139fb:	01 
  4139fc:	49 83 c4 01          	add    $0x1,%r12
  413a00:	49 39 d3             	cmp    %rdx,%r11
  413a03:	0f 84 97 00 00 00    	je     413aa0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x3d0>
  413a09:	8b 02                	mov    (%rdx),%eax
  413a0b:	89 c1                	mov    %eax,%ecx
  413a0d:	89 03                	mov    %eax,(%rbx)
  413a0f:	d1 e9                	shr    %ecx
  413a11:	89 ce                	mov    %ecx,%esi
  413a13:	45 0f b6 04 36       	movzbl (%r14,%rsi,1),%r8d
  413a18:	a8 01                	test   $0x1,%al
  413a1a:	0f 85 98 fe ff ff    	jne    4138b8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x1e8>
  413a20:	44 8b 6a 04          	mov    0x4(%rdx),%r13d
  413a24:	48 8d 4a 08          	lea    0x8(%rdx),%rcx
  413a28:	48 8d 7b 08          	lea    0x8(%rbx),%rdi
  413a2c:	44 89 6b 04          	mov    %r13d,0x4(%rbx)
  413a30:	45 84 c0             	test   %r8b,%r8b
  413a33:	0f 8f 6f 02 00 00    	jg     413ca8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5d8>
  413a39:	48 8b 44 24 18       	mov    0x18(%rsp),%rax
  413a3e:	44 89 ee             	mov    %r13d,%esi
  413a41:	4d 8d 44 24 01       	lea    0x1(%r12),%r8
  413a46:	48 c1 e6 02          	shl    $0x2,%rsi
  413a4a:	48 8d 14 30          	lea    (%rax,%rsi,1),%rdx
  413a4e:	f6 42 04 01          	testb  $0x1,0x4(%rdx)
  413a52:	0f 85 60 02 00 00    	jne    413cb8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5e8>
  413a58:	44 8b 54 24 0c       	mov    0xc(%rsp),%r10d
  413a5d:	8b 42 14             	mov    0x14(%rdx),%eax
  413a60:	33 42 18             	xor    0x18(%rdx),%eax
  413a63:	41 31 c2             	xor    %eax,%r10d
  413a66:	44 89 54 24 10       	mov    %r10d,0x10(%rsp)
  413a6b:	44 89 d0             	mov    %r10d,%eax
  413a6e:	4c 89 54 24 28       	mov    %r10,0x28(%rsp)
  413a73:	47 0f b6 14 16       	movzbl (%r14,%r10,1),%r10d
  413a78:	45 84 d2             	test   %r10b,%r10b
  413a7b:	0f 8e 5f 02 00 00    	jle    413ce0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x610>
  413a81:	01 c0                	add    %eax,%eax
  413a83:	48 89 ca             	mov    %rcx,%rdx
  413a86:	4d 89 c4             	mov    %r8,%r12
  413a89:	89 03                	mov    %eax,(%rbx)
  413a8b:	48 89 fb             	mov    %rdi,%rbx
  413a8e:	49 39 d3             	cmp    %rdx,%r11
  413a91:	0f 85 72 ff ff ff    	jne    413a09 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x339>
  413a97:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  413a9e:	00 00 
  413aa0:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  413aa7:	8b 6c 24 68          	mov    0x68(%rsp),%ebp
  413aab:	4c 89 64 24 10       	mov    %r12,0x10(%rsp)
  413ab0:	45 31 ed             	xor    %r13d,%r13d
  413ab3:	48 8b 44 24 48       	mov    0x48(%rsp),%rax
  413ab8:	4c 8b 64 24 50       	mov    0x50(%rsp),%r12
  413abd:	4c 01 d0             	add    %r10,%rax
  413ac0:	4c 29 e3             	sub    %r12,%rbx
  413ac3:	8b 48 04             	mov    0x4(%rax),%ecx
  413ac6:	48 89 da             	mov    %rbx,%rdx
  413ac9:	48 c1 fa 02          	sar    $0x2,%rdx
  413acd:	0f 1f 00             	nopl   (%rax)
  413ad0:	8b 70 0c             	mov    0xc(%rax),%esi
  413ad3:	49 8b bf 18 02 00 00 	mov    0x218(%r15),%rdi
  413ada:	29 d1                	sub    %edx,%ecx
  413adc:	49 01 8f 90 01 00 00 	add    %rcx,0x190(%r15)
  413ae3:	89 50 04             	mov    %edx,0x4(%rax)
  413ae6:	89 70 0c             	mov    %esi,0xc(%rax)
  413ae9:	48 85 ff             	test   %rdi,%rdi
  413aec:	0f 84 20 01 00 00    	je     413c12 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x542>
  413af2:	89 6c 24 18          	mov    %ebp,0x18(%rsp)
  413af6:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  413afd:	45 31 f6             	xor    %r14d,%r14d
  413b00:	44 88 6c 24 20       	mov    %r13b,0x20(%rsp)
  413b05:	eb 35                	jmp    413b3c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x46c>
  413b07:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  413b0e:	00 00 
  413b10:	8b 03                	mov    (%rbx),%eax
  413b12:	66 0f 6e cd          	movd   %ebp,%xmm1
  413b16:	49 83 c6 03          	add    $0x3,%r14
  413b1a:	66 0f 62 c8          	punpckldq %xmm0,%xmm1
  413b1e:	48 01 d0             	add    %rdx,%rax
  413b21:	66 41 0f d6 0c 83    	movq   %xmm1,(%r11,%rax,4)
  413b27:	83 43 04 02          	addl   $0x2,0x4(%rbx)
  413b2b:	49 83 af 90 01 00 00 	subq   $0x2,0x190(%r15)
  413b32:	02 
  413b33:	49 39 fe             	cmp    %rdi,%r14
  413b36:	0f 83 cc 00 00 00    	jae    413c08 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x538>
  413b3c:	49 8b 87 00 02 00 00 	mov    0x200(%r15),%rax
  413b43:	4a 8d 34 b5 00 00 00 	lea    0x0(,%r14,4),%rsi
  413b4a:	00 
  413b4b:	46 8b 04 b0          	mov    (%rax,%r14,4),%r8d
  413b4f:	8b 6c 30 04          	mov    0x4(%rax,%rsi,1),%ebp
  413b53:	66 0f 6e 44 30 08    	movd   0x8(%rax,%rsi,1),%xmm0
  413b59:	49 c1 e0 04          	shl    $0x4,%r8
  413b5d:	01 ed                	add    %ebp,%ebp
  413b5f:	4b 8d 1c 02          	lea    (%r10,%r8,1),%rbx
  413b63:	44 8b 4b 08          	mov    0x8(%rbx),%r9d
  413b67:	8b 53 04             	mov    0x4(%rbx),%edx
  413b6a:	44 89 c8             	mov    %r9d,%eax
  413b6d:	29 d0                	sub    %edx,%eax
  413b6f:	83 f8 01             	cmp    $0x1,%eax
  413b72:	77 9c                	ja     413b10 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x440>
  413b74:	8b 43 0c             	mov    0xc(%rbx),%eax
  413b77:	89 d6                	mov    %edx,%esi
  413b79:	83 e6 01             	and    $0x1,%esi
  413b7c:	41 89 c4             	mov    %eax,%r12d
  413b7f:	f7 d0                	not    %eax
  413b81:	41 83 e4 fe          	and    $0xfffffffe,%r12d
  413b85:	83 e0 01             	and    $0x1,%eax
  413b88:	88 44 24 0c          	mov    %al,0xc(%rsp)
  413b8c:	41 09 f4             	or     %esi,%r12d
  413b8f:	41 39 d1             	cmp    %edx,%r9d
  413b92:	0f 84 98 04 00 00    	je     414030 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x960>
  413b98:	49 8b 87 90 01 00 00 	mov    0x190(%r15),%rax
  413b9f:	48 8d 78 ff          	lea    -0x1(%rax),%rdi
  413ba3:	48 89 d8             	mov    %rbx,%rax
  413ba6:	8d 72 01             	lea    0x1(%rdx),%esi
  413ba9:	03 10                	add    (%rax),%edx
  413bab:	49 89 bf 90 01 00 00 	mov    %rdi,0x190(%r15)
  413bb2:	89 70 04             	mov    %esi,0x4(%rax)
  413bb5:	41 89 2c 93          	mov    %ebp,(%r11,%rdx,4)
  413bb9:	8b 70 04             	mov    0x4(%rax),%esi
  413bbc:	3b 70 08             	cmp    0x8(%rax),%esi
  413bbf:	0f 84 fb 02 00 00    	je     413ec0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x7f0>
  413bc5:	48 83 ef 01          	sub    $0x1,%rdi
  413bc9:	8d 56 01             	lea    0x1(%rsi),%edx
  413bcc:	03 30                	add    (%rax),%esi
  413bce:	41 83 e4 fe          	and    $0xfffffffe,%r12d
  413bd2:	49 83 c6 03          	add    $0x3,%r14
  413bd6:	49 89 bf 90 01 00 00 	mov    %rdi,0x190(%r15)
  413bdd:	49 8b bf 18 02 00 00 	mov    0x218(%r15),%rdi
  413be4:	89 50 04             	mov    %edx,0x4(%rax)
  413be7:	0f b6 44 24 0c       	movzbl 0xc(%rsp),%eax
  413bec:	66 41 0f 7e 04 b3    	movd   %xmm0,(%r11,%rsi,4)
  413bf2:	83 f0 01             	xor    $0x1,%eax
  413bf5:	0f b6 c0             	movzbl %al,%eax
  413bf8:	41 09 c4             	or     %eax,%r12d
  413bfb:	44 89 63 0c          	mov    %r12d,0xc(%rbx)
  413bff:	49 39 fe             	cmp    %rdi,%r14
  413c02:	0f 82 34 ff ff ff    	jb     413b3c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x46c>
  413c08:	8b 6c 24 18          	mov    0x18(%rsp),%ebp
  413c0c:	44 0f b6 6c 24 20    	movzbl 0x20(%rsp),%r13d
  413c12:	49 c7 87 18 02 00 00 	movq   $0x0,0x218(%r15)
  413c19:	00 00 00 00 
  413c1d:	48 8b 5c 24 10       	mov    0x10(%rsp),%rbx
  413c22:	48 01 5c 24 60       	add    %rbx,0x60(%rsp)
  413c27:	45 84 ed             	test   %r13b,%r13b
  413c2a:	0f 84 c8 fb ff ff    	je     4137f8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x128>
  413c30:	44 8b 64 24 6c       	mov    0x6c(%rsp),%r12d
  413c35:	89 e8                	mov    %ebp,%eax
  413c37:	45 89 e8             	mov    %r13d,%r8d
  413c3a:	41 89 87 f0 00 00 00 	mov    %eax,0xf0(%r15)
  413c41:	44 29 e0             	sub    %r12d,%eax
  413c44:	41 80 bf d8 1d 00 00 	cmpb   $0x0,0x1dd8(%r15)
  413c4b:	00 
  413c4c:	66 48 0f 6e c0       	movq   %rax,%xmm0
  413c51:	48 8b 44 24 60       	mov    0x60(%rsp),%rax
  413c56:	66 48 0f 6e f8       	movq   %rax,%xmm7
  413c5b:	66 0f 6c c7          	punpcklqdq %xmm7,%xmm0
  413c5f:	f3 41 0f 6f bf 78 06 	movdqu 0x678(%r15),%xmm7
  413c66:	00 00 
  413c68:	66 0f d4 c7          	paddq  %xmm7,%xmm0
  413c6c:	41 0f 11 87 78 06 00 	movups %xmm0,0x678(%r15)
  413c73:	00 
  413c74:	75 11                	jne    413c87 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5b7>
  413c76:	41 80 bf 24 01 00 00 	cmpb   $0x0,0x124(%r15)
  413c7d:	00 
  413c7e:	75 07                	jne    413c87 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x5b7>
  413c80:	49 01 87 88 06 00 00 	add    %rax,0x688(%r15)
  413c87:	49 8b 87 d0 1d 00 00 	mov    0x1dd0(%r15),%rax
  413c8e:	48 85 c0             	test   %rax,%rax
  413c91:	0f 84 16 fb ff ff    	je     4137ad <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdd>
  413c97:	48 8b 5c 24 60       	mov    0x60(%rsp),%rbx
  413c9c:	48 01 18             	add    %rbx,(%rax)
  413c9f:	e9 09 fb ff ff       	jmpq   4137ad <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdd>
  413ca4:	0f 1f 40 00          	nopl   0x0(%rax)
  413ca8:	48 89 ca             	mov    %rcx,%rdx
  413cab:	48 89 fb             	mov    %rdi,%rbx
  413cae:	e9 4d fd ff ff       	jmpq   413a00 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  413cb3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  413cb8:	4d 89 c4             	mov    %r8,%r12
  413cbb:	48 89 ca             	mov    %rcx,%rdx
  413cbe:	e9 3d fd ff ff       	jmpq   413a00 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  413cc3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  413cc8:	45 8b 12             	mov    (%r10),%r10d
  413ccb:	4d 8b 8f 60 01 00 00 	mov    0x160(%r15),%r9
  413cd2:	43 0f 18 1c 91       	prefetcht2 (%r9,%r10,4)
  413cd7:	e9 3e fc ff ff       	jmpq   41391a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x24a>
  413cdc:	0f 1f 40 00          	nopl   0x0(%rax)
  413ce0:	8b 2a                	mov    (%rdx),%ebp
  413ce2:	48 8d 42 14          	lea    0x14(%rdx),%rax
  413ce6:	49 89 c1             	mov    %rax,%r9
  413ce9:	48 89 44 24 38       	mov    %rax,0x38(%rsp)
  413cee:	48 c1 e5 02          	shl    $0x2,%rbp
  413cf2:	48 01 e8             	add    %rbp,%rax
  413cf5:	48 89 44 24 30       	mov    %rax,0x30(%rsp)
  413cfa:	8b 42 08             	mov    0x8(%rdx),%eax
  413cfd:	48 c1 e0 02          	shl    $0x2,%rax
  413d01:	49 01 c1             	add    %rax,%r9
  413d04:	4c 89 4c 24 40       	mov    %r9,0x40(%rsp)
  413d09:	48 39 c5             	cmp    %rax,%rbp
  413d0c:	0f 84 f9 08 00 00    	je     41460b <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf3b>
  413d12:	4c 89 c8             	mov    %r9,%rax
  413d15:	48 89 7c 24 58       	mov    %rdi,0x58(%rsp)
  413d1a:	4c 8b 4c 24 30       	mov    0x30(%rsp),%r9
  413d1f:	eb 14                	jmp    413d35 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x665>
  413d21:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  413d28:	48 83 c0 04          	add    $0x4,%rax
  413d2c:	49 39 c1             	cmp    %rax,%r9
  413d2f:	0f 84 73 04 00 00    	je     4141a8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xad8>
  413d35:	8b 38                	mov    (%rax),%edi
  413d37:	41 80 3c 3e 00       	cmpb   $0x0,(%r14,%rdi,1)
  413d3c:	48 89 fd             	mov    %rdi,%rbp
  413d3f:	78 e7                	js     413d28 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x658>
  413d41:	48 8b 7c 24 38       	mov    0x38(%rsp),%rdi
  413d46:	48 89 c6             	mov    %rax,%rsi
  413d49:	4d 8b 87 08 02 00 00 	mov    0x208(%r15),%r8
  413d50:	89 6a 18             	mov    %ebp,0x18(%rdx)
  413d53:	48 29 fe             	sub    %rdi,%rsi
  413d56:	8b 7c 24 10          	mov    0x10(%rsp),%edi
  413d5a:	48 c1 fe 02          	sar    $0x2,%rsi
  413d5e:	89 7a 14             	mov    %edi,0x14(%rdx)
  413d61:	8b 7c 24 0c          	mov    0xc(%rsp),%edi
  413d65:	89 72 08             	mov    %esi,0x8(%rdx)
  413d68:	49 8b 97 00 02 00 00 	mov    0x200(%r15),%rdx
  413d6f:	89 38                	mov    %edi,(%rax)
  413d71:	49 8b b7 18 02 00 00 	mov    0x218(%r15),%rsi
  413d78:	4c 89 c0             	mov    %r8,%rax
  413d7b:	48 29 d0             	sub    %rdx,%rax
  413d7e:	48 8d 7e 03          	lea    0x3(%rsi),%rdi
  413d82:	48 c1 f8 02          	sar    $0x2,%rax
  413d86:	48 39 f8             	cmp    %rdi,%rax
  413d89:	73 4b                	jae    413dd6 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x706>
  413d8b:	48 01 c0             	add    %rax,%rax
  413d8e:	49 39 d0             	cmp    %rdx,%r8
  413d91:	ba 03 00 00 00       	mov    $0x3,%edx
  413d96:	48 89 4c 24 30       	mov    %rcx,0x30(%rsp)
  413d9b:	48 0f 44 c2          	cmove  %rdx,%rax
  413d9f:	4c 89 5c 24 28       	mov    %r11,0x28(%rsp)
  413da4:	48 39 c7             	cmp    %rax,%rdi
  413da7:	48 0f 43 c7          	cmovae %rdi,%rax
  413dab:	49 8d bf 00 02 00 00 	lea    0x200(%r15),%rdi
  413db2:	48 89 c6             	mov    %rax,%rsi
  413db5:	e8 d6 0f 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  413dba:	48 8b 4c 24 30       	mov    0x30(%rsp),%rcx
  413dbf:	4c 8b 5c 24 28       	mov    0x28(%rsp),%r11
  413dc4:	49 8b b7 18 02 00 00 	mov    0x218(%r15),%rsi
  413dcb:	49 8b 97 00 02 00 00 	mov    0x200(%r15),%rdx
  413dd2:	48 8d 7e 03          	lea    0x3(%rsi),%rdi
  413dd6:	89 2c b2             	mov    %ebp,(%rdx,%rsi,4)
  413dd9:	48 8d 04 b5 00 00 00 	lea    0x0(,%rsi,4),%rax
  413de0:	00 
  413de1:	8b 74 24 10          	mov    0x10(%rsp),%esi
  413de5:	49 83 c4 02          	add    $0x2,%r12
  413de9:	89 74 02 04          	mov    %esi,0x4(%rdx,%rax,1)
  413ded:	49 89 bf 18 02 00 00 	mov    %rdi,0x218(%r15)
  413df4:	44 89 6c 02 08       	mov    %r13d,0x8(%rdx,%rax,1)
  413df9:	48 89 ca             	mov    %rcx,%rdx
  413dfc:	e9 ff fb ff ff       	jmpq   413a00 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  413e01:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  413e08:	80 7d 0c 00          	cmpb   $0x0,0xc(%rbp)
  413e0c:	0f 84 fc fa ff ff    	je     41390e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x23e>
  413e12:	4d 8b 8f b8 00 00 00 	mov    0xb8(%r15),%r9
  413e19:	8b 6c 24 0c          	mov    0xc(%rsp),%ebp
  413e1d:	41 89 2c 81          	mov    %ebp,(%r9,%rax,4)
  413e21:	4c 8b 4c 24 20       	mov    0x20(%rsp),%r9
  413e26:	45 8b 6a 04          	mov    0x4(%r10),%r13d
  413e2a:	49 83 87 e8 18 00 00 	addq   $0x1,0x18e8(%r15)
  413e31:	01 
  413e32:	45 8b 49 08          	mov    0x8(%r9),%r9d
  413e36:	45 85 ed             	test   %r13d,%r13d
  413e39:	0f 85 04 08 00 00    	jne    414643 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf73>
  413e3f:	4d 8b 97 88 00 00 00 	mov    0x88(%r15),%r10
  413e46:	66 41 0f 6e c0       	movd   %r8d,%xmm0
  413e4b:	41 c6 04 32 01       	movb   $0x1,(%r10,%rsi,1)
  413e50:	49 8b b7 88 00 00 00 	mov    0x88(%r15),%rsi
  413e57:	44 0f b7 15 db c3 0f 	movzwl 0xfc3db(%rip),%r10d        # 51023a <_IO_stdin_used+0x223a>
  413e5e:	00 
  413e5f:	c6 04 3e ff          	movb   $0xff,(%rsi,%rdi,1)
  413e63:	49 8b bf e8 00 00 00 	mov    0xe8(%r15),%rdi
  413e6a:	48 89 c6             	mov    %rax,%rsi
  413e6d:	48 c1 e6 04          	shl    $0x4,%rsi
  413e71:	49 03 b7 a0 00 00 00 	add    0xa0(%r15),%rsi
  413e78:	66 0f 6e f7          	movd   %edi,%xmm6
  413e7c:	44 89 4e 08          	mov    %r9d,0x8(%rsi)
  413e80:	66 0f 62 c6          	punpckldq %xmm6,%xmm0
  413e84:	66 44 89 56 0c       	mov    %r10w,0xc(%rsi)
  413e89:	66 0f d6 06          	movq   %xmm0,(%rsi)
  413e8d:	48 8d 77 01          	lea    0x1(%rdi),%rsi
  413e91:	49 89 b7 e8 00 00 00 	mov    %rsi,0xe8(%r15)
  413e98:	49 8b b7 d0 00 00 00 	mov    0xd0(%r15),%rsi
  413e9f:	89 0c be             	mov    %ecx,(%rsi,%rdi,4)
  413ea2:	49 8b b7 e0 1d 00 00 	mov    0x1de0(%r15),%rsi
  413ea9:	48 85 f6             	test   %rsi,%rsi
  413eac:	0f 85 f1 fa ff ff    	jne    4139a3 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x2d3>
  413eb2:	e9 19 fb ff ff       	jmpq   4139d0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x300>
  413eb7:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  413ebe:	00 00 
  413ec0:	4d 8b 97 70 01 00 00 	mov    0x170(%r15),%r10
  413ec7:	49 8d bf 60 01 00 00 	lea    0x160(%r15),%rdi
  413ece:	4d 39 da             	cmp    %r11,%r10
  413ed1:	0f 84 e2 06 00 00    	je     4145b9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xee9>
  413ed7:	49 8b af 68 01 00 00 	mov    0x168(%r15),%rbp
  413ede:	4d 29 da             	sub    %r11,%r10
  413ee1:	41 89 f5             	mov    %esi,%r13d
  413ee4:	4c 29 dd             	sub    %r11,%rbp
  413ee7:	44 8b 18             	mov    (%rax),%r11d
  413eea:	49 89 e9             	mov    %rbp,%r9
  413eed:	49 c1 f9 02          	sar    $0x2,%r9
  413ef1:	4d 01 eb             	add    %r13,%r11
  413ef4:	4c 89 ca             	mov    %r9,%rdx
  413ef7:	4d 39 cb             	cmp    %r9,%r11
  413efa:	0f 84 f0 04 00 00    	je     4143f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xd20>
  413f00:	85 f6                	test   %esi,%esi
  413f02:	0f 84 80 04 00 00    	je     414388 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcb8>
  413f08:	8d 2c 36             	lea    (%rsi,%rsi,1),%ebp
  413f0b:	49 89 ed             	mov    %rbp,%r13
  413f0e:	49 8d 34 29          	lea    (%r9,%rbp,1),%rsi
  413f12:	49 c1 fa 02          	sar    $0x2,%r10
  413f16:	49 39 f2             	cmp    %rsi,%r10
  413f19:	73 5f                	jae    413f7a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x8aa>
  413f1b:	48 83 fe 03          	cmp    $0x3,%rsi
  413f1f:	0f 86 76 07 00 00    	jbe    41469b <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xfcb>
  413f25:	b8 04 00 00 00       	mov    $0x4,%eax
  413f2a:	66 0f 1f 44 00 00    	nopw   0x0(%rax,%rax,1)
  413f30:	48 01 c0             	add    %rax,%rax
  413f33:	48 8d 50 ff          	lea    -0x1(%rax),%rdx
  413f37:	48 39 f2             	cmp    %rsi,%rdx
  413f3a:	72 f4                	jb     413f30 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x860>
  413f3c:	48 89 d6             	mov    %rdx,%rsi
  413f3f:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  413f44:	48 89 7c 24 28       	mov    %rdi,0x28(%rsp)
  413f49:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  413f4f:	e8 7c 0d 01 00       	callq  424cd0 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  413f54:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  413f59:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  413f5f:	49 8b 97 68 01 00 00 	mov    0x168(%r15),%rdx
  413f66:	49 2b 97 60 01 00 00 	sub    0x160(%r15),%rdx
  413f6d:	48 c1 fa 02          	sar    $0x2,%rdx
  413f71:	48 8b 7c 24 28       	mov    0x28(%rsp),%rdi
  413f76:	48 8d 34 2a          	lea    (%rdx,%rbp,1),%rsi
  413f7a:	4c 89 44 24 40       	mov    %r8,0x40(%rsp)
  413f7f:	89 54 24 28          	mov    %edx,0x28(%rsp)
  413f83:	48 89 54 24 30       	mov    %rdx,0x30(%rsp)
  413f88:	66 0f 7e 44 24 38    	movd   %xmm0,0x38(%rsp)
  413f8e:	e8 fd 0d 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  413f93:	4c 8b 44 24 40       	mov    0x40(%rsp),%r8
  413f98:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  413f9f:	48 8b 54 24 30       	mov    0x30(%rsp),%rdx
  413fa4:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  413fab:	4b 8d 0c 02          	lea    (%r10,%r8,1),%rcx
  413faf:	66 0f 6e 44 24 38    	movd   0x38(%rsp),%xmm0
  413fb5:	89 d2                	mov    %edx,%edx
  413fb7:	8b 01                	mov    (%rcx),%eax
  413fb9:	49 8d 3c 93          	lea    (%r11,%rdx,4),%rdi
  413fbd:	8b 51 04             	mov    0x4(%rcx),%edx
  413fc0:	4d 8d 0c 83          	lea    (%r11,%rax,4),%r9
  413fc4:	48 89 d6             	mov    %rdx,%rsi
  413fc7:	48 c1 e2 02          	shl    $0x2,%rdx
  413fcb:	48 83 fa 04          	cmp    $0x4,%rdx
  413fcf:	0f 86 99 06 00 00    	jbe    41466e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf9e>
  413fd5:	4c 89 ce             	mov    %r9,%rsi
  413fd8:	48 89 4c 24 38       	mov    %rcx,0x38(%rsp)
  413fdd:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  413fe3:	e8 c8 e3 fe ff       	callq  4023b0 <memmove@plt>
  413fe8:	4c 8b 44 24 40       	mov    0x40(%rsp),%r8
  413fed:	48 8b 4c 24 38       	mov    0x38(%rsp),%rcx
  413ff2:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  413ff9:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  414000:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  414006:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  41400a:	8b 70 04             	mov    0x4(%rax),%esi
  41400d:	49 8b 97 90 01 00 00 	mov    0x190(%r15),%rdx
  414014:	8b 7c 24 28          	mov    0x28(%rsp),%edi
  414018:	44 89 69 08          	mov    %r13d,0x8(%rcx)
  41401c:	89 39                	mov    %edi,(%rcx)
  41401e:	48 8d 7c 15 ff       	lea    -0x1(%rbp,%rdx,1),%rdi
  414023:	e9 a1 fb ff ff       	jmpq   413bc9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4f9>
  414028:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  41402f:	00 
  414030:	4d 8b 8f 70 01 00 00 	mov    0x170(%r15),%r9
  414037:	49 8d bf 60 01 00 00 	lea    0x160(%r15),%rdi
  41403e:	4d 39 d9             	cmp    %r11,%r9
  414041:	0f 84 32 05 00 00    	je     414579 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xea9>
  414047:	4d 8b 97 68 01 00 00 	mov    0x168(%r15),%r10
  41404e:	4d 29 d9             	sub    %r11,%r9
  414051:	41 89 d5             	mov    %edx,%r13d
  414054:	4d 29 da             	sub    %r11,%r10
  414057:	44 8b 1b             	mov    (%rbx),%r11d
  41405a:	4c 89 d6             	mov    %r10,%rsi
  41405d:	48 c1 fe 02          	sar    $0x2,%rsi
  414061:	4d 01 eb             	add    %r13,%r11
  414064:	48 89 f0             	mov    %rsi,%rax
  414067:	49 39 f3             	cmp    %rsi,%r11
  41406a:	0f 84 30 03 00 00    	je     4143a0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xcd0>
  414070:	85 d2                	test   %edx,%edx
  414072:	0f 84 f8 02 00 00    	je     414370 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xca0>
  414078:	44 8d 2c 12          	lea    (%rdx,%rdx,1),%r13d
  41407c:	44 89 6c 24 48       	mov    %r13d,0x48(%rsp)
  414081:	4c 01 ee             	add    %r13,%rsi
  414084:	49 c1 f9 02          	sar    $0x2,%r9
  414088:	49 39 f1             	cmp    %rsi,%r9
  41408b:	73 5d                	jae    4140ea <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xa1a>
  41408d:	48 83 fe 03          	cmp    $0x3,%rsi
  414091:	0f 86 0e 06 00 00    	jbe    4146a5 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xfd5>
  414097:	b8 04 00 00 00       	mov    $0x4,%eax
  41409c:	0f 1f 40 00          	nopl   0x0(%rax)
  4140a0:	48 01 c0             	add    %rax,%rax
  4140a3:	48 8d 50 ff          	lea    -0x1(%rax),%rdx
  4140a7:	48 39 f2             	cmp    %rsi,%rdx
  4140aa:	72 f4                	jb     4140a0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9d0>
  4140ac:	48 89 d6             	mov    %rdx,%rsi
  4140af:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  4140b4:	48 89 7c 24 28       	mov    %rdi,0x28(%rsp)
  4140b9:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  4140bf:	e8 0c 0c 01 00       	callq  424cd0 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  4140c4:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  4140c9:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  4140cf:	49 8b 87 68 01 00 00 	mov    0x168(%r15),%rax
  4140d6:	49 2b 87 60 01 00 00 	sub    0x160(%r15),%rax
  4140dd:	48 c1 f8 02          	sar    $0x2,%rax
  4140e1:	48 8b 7c 24 28       	mov    0x28(%rsp),%rdi
  4140e6:	4a 8d 34 28          	lea    (%rax,%r13,1),%rsi
  4140ea:	4c 89 44 24 40       	mov    %r8,0x40(%rsp)
  4140ef:	89 44 24 28          	mov    %eax,0x28(%rsp)
  4140f3:	48 89 44 24 30       	mov    %rax,0x30(%rsp)
  4140f8:	66 0f 7e 44 24 38    	movd   %xmm0,0x38(%rsp)
  4140fe:	e8 8d 0c 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  414103:	4c 8b 44 24 40       	mov    0x40(%rsp),%r8
  414108:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  41410f:	48 8b 44 24 30       	mov    0x30(%rsp),%rax
  414114:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  41411b:	4f 8d 0c 02          	lea    (%r10,%r8,1),%r9
  41411f:	66 0f 6e 44 24 38    	movd   0x38(%rsp),%xmm0
  414125:	89 c0                	mov    %eax,%eax
  414127:	41 8b 51 04          	mov    0x4(%r9),%edx
  41412b:	49 8d 3c 83          	lea    (%r11,%rax,4),%rdi
  41412f:	41 8b 01             	mov    (%r9),%eax
  414132:	49 8d 34 83          	lea    (%r11,%rax,4),%rsi
  414136:	89 d0                	mov    %edx,%eax
  414138:	48 c1 e0 02          	shl    $0x2,%rax
  41413c:	48 83 f8 04          	cmp    $0x4,%rax
  414140:	0f 86 12 05 00 00    	jbe    414658 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xf88>
  414146:	48 89 c2             	mov    %rax,%rdx
  414149:	4c 89 4c 24 40       	mov    %r9,0x40(%rsp)
  41414e:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  414153:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  414159:	e8 52 e2 fe ff       	callq  4023b0 <memmove@plt>
  41415e:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  414163:	4c 8b 4c 24 40       	mov    0x40(%rsp),%r9
  414168:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  41416f:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  414176:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  41417c:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  414180:	8b 50 04             	mov    0x4(%rax),%edx
  414183:	8b 7c 24 28          	mov    0x28(%rsp),%edi
  414187:	49 8b b7 90 01 00 00 	mov    0x190(%r15),%rsi
  41418e:	41 89 39             	mov    %edi,(%r9)
  414191:	8b 7c 24 48          	mov    0x48(%rsp),%edi
  414195:	41 89 79 08          	mov    %edi,0x8(%r9)
  414199:	49 8d 7c 35 ff       	lea    -0x1(%r13,%rsi,1),%rdi
  41419e:	e9 03 fa ff ff       	jmpq   413ba6 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4d6>
  4141a3:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  4141a8:	4c 8b 4c 24 40       	mov    0x40(%rsp),%r9
  4141ad:	48 8d 42 1c          	lea    0x1c(%rdx),%rax
  4141b1:	48 8b 7c 24 58       	mov    0x58(%rsp),%rdi
  4141b6:	49 39 c1             	cmp    %rax,%r9
  4141b9:	74 2d                	je     4141e8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb18>
  4141bb:	48 89 54 24 30       	mov    %rdx,0x30(%rsp)
  4141c0:	4c 89 ca             	mov    %r9,%rdx
  4141c3:	eb 0c                	jmp    4141d1 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb01>
  4141c5:	0f 1f 00             	nopl   (%rax)
  4141c8:	48 83 c0 04          	add    $0x4,%rax
  4141cc:	48 39 c2             	cmp    %rax,%rdx
  4141cf:	74 17                	je     4141e8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb18>
  4141d1:	44 8b 08             	mov    (%rax),%r9d
  4141d4:	43 80 3c 0e 00       	cmpb   $0x0,(%r14,%r9,1)
  4141d9:	4c 89 cd             	mov    %r9,%rbp
  4141dc:	78 ea                	js     4141c8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xaf8>
  4141de:	48 8b 54 24 30       	mov    0x30(%rsp),%rdx
  4141e3:	e9 59 fb ff ff       	jmpq   413d41 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x671>
  4141e8:	45 84 d2             	test   %r10b,%r10b
  4141eb:	0f 85 be 04 00 00    	jne    4146af <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xfdf>
  4141f1:	44 8b 44 24 10       	mov    0x10(%rsp),%r8d
  4141f6:	45 8b 8f 18 01 00 00 	mov    0x118(%r15),%r9d
  4141fd:	49 8b 87 30 01 00 00 	mov    0x130(%r15),%rax
  414204:	41 d1 e8             	shr    %r8d
  414207:	45 85 c9             	test   %r9d,%r9d
  41420a:	74 53                	je     41425f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb8f>
  41420c:	48 01 f0             	add    %rsi,%rax
  41420f:	48 8d 50 14          	lea    0x14(%rax),%rdx
  414213:	8b 00                	mov    (%rax),%eax
  414215:	48 c1 e0 02          	shl    $0x2,%rax
  414219:	48 8d 34 02          	lea    (%rdx,%rax,1),%rsi
  41421d:	0f 84 bb 04 00 00    	je     4146de <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x100e>
  414223:	45 31 d2             	xor    %r10d,%r10d
  414226:	66 2e 0f 1f 84 00 00 	nopw   %cs:0x0(%rax,%rax,1)
  41422d:	00 00 00 
  414230:	8b 02                	mov    (%rdx),%eax
  414232:	d1 e8                	shr    %eax
  414234:	41 39 c0             	cmp    %eax,%r8d
  414237:	74 1a                	je     414253 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb83>
  414239:	48 c1 e0 04          	shl    $0x4,%rax
  41423d:	49 03 87 a0 00 00 00 	add    0xa0(%r15),%rax
  414244:	8b 00                	mov    (%rax),%eax
  414246:	41 39 c2             	cmp    %eax,%r10d
  414249:	73 08                	jae    414253 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb83>
  41424b:	41 39 c1             	cmp    %eax,%r9d
  41424e:	74 0f                	je     41425f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb8f>
  414250:	41 89 c2             	mov    %eax,%r10d
  414253:	48 83 c2 04          	add    $0x4,%rdx
  414257:	48 39 d6             	cmp    %rdx,%rsi
  41425a:	75 d4                	jne    414230 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb60>
  41425c:	45 89 d1             	mov    %r10d,%r9d
  41425f:	8b 44 24 10          	mov    0x10(%rsp),%eax
  414263:	83 f0 01             	xor    $0x1,%eax
  414266:	48 89 c2             	mov    %rax,%rdx
  414269:	48 c1 e2 04          	shl    $0x4,%rdx
  41426d:	49 03 97 78 01 00 00 	add    0x178(%r15),%rdx
  414274:	44 8b 52 04          	mov    0x4(%rdx),%r10d
  414278:	45 85 d2             	test   %r10d,%r10d
  41427b:	0f 85 e6 02 00 00    	jne    414567 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe97>
  414281:	49 8b 97 88 00 00 00 	mov    0x88(%r15),%rdx
  414288:	48 8b 5c 24 28       	mov    0x28(%rsp),%rbx
  41428d:	31 f6                	xor    %esi,%esi
  41428f:	c6 04 1a 01          	movb   $0x1,(%rdx,%rbx,1)
  414293:	49 8b 97 88 00 00 00 	mov    0x88(%r15),%rdx
  41429a:	8b 5c 24 10          	mov    0x10(%rsp),%ebx
  41429e:	c6 04 02 ff          	movb   $0xff,(%rdx,%rax,1)
  4142a2:	4c 89 c0             	mov    %r8,%rax
  4142a5:	49 8b 97 e8 00 00 00 	mov    0xe8(%r15),%rdx
  4142ac:	48 c1 e0 04          	shl    $0x4,%rax
  4142b0:	49 03 87 a0 00 00 00 	add    0xa0(%r15),%rax
  4142b7:	44 89 08             	mov    %r9d,(%rax)
  4142ba:	89 50 04             	mov    %edx,0x4(%rax)
  4142bd:	44 89 68 08          	mov    %r13d,0x8(%rax)
  4142c1:	66 89 70 0c          	mov    %si,0xc(%rax)
  4142c5:	48 8d 42 01          	lea    0x1(%rdx),%rax
  4142c9:	49 89 87 e8 00 00 00 	mov    %rax,0xe8(%r15)
  4142d0:	49 8b 87 d0 00 00 00 	mov    0xd0(%r15),%rax
  4142d7:	89 1c 90             	mov    %ebx,(%rax,%rdx,4)
  4142da:	45 85 c9             	test   %r9d,%r9d
  4142dd:	75 10                	jne    4142ef <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc1f>
  4142df:	49 83 87 98 06 00 00 	addq   $0x1,0x698(%r15)
  4142e6:	01 
  4142e7:	41 83 87 54 1c 00 00 	addl   $0x1,0x1c54(%r15)
  4142ee:	01 
  4142ef:	49 8b 97 e0 1d 00 00 	mov    0x1de0(%r15),%rdx
  4142f6:	48 85 d2             	test   %rdx,%rdx
  4142f9:	74 35                	je     414330 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc60>
  4142fb:	41 80 bf e8 1d 00 00 	cmpb   $0x0,0x1de8(%r15)
  414302:	00 
  414303:	75 2b                	jne    414330 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc60>
  414305:	41 80 bf 20 02 00 00 	cmpb   $0x0,0x220(%r15)
  41430c:	00 
  41430d:	75 21                	jne    414330 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc60>
  41430f:	0f b6 44 24 10       	movzbl 0x10(%rsp),%eax
  414314:	48 8b 12             	mov    (%rdx),%rdx
  414317:	83 e0 01             	and    $0x1,%eax
  41431a:	3c 01                	cmp    $0x1,%al
  41431c:	19 c0                	sbb    %eax,%eax
  41431e:	83 e0 02             	and    $0x2,%eax
  414321:	83 e8 01             	sub    $0x1,%eax
  414324:	42 88 04 02          	mov    %al,(%rdx,%r8,1)
  414328:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  41432f:	00 
  414330:	49 8b 97 48 1d 00 00 	mov    0x1d48(%r15),%rdx
  414337:	49 8b 87 50 1d 00 00 	mov    0x1d50(%r15),%rax
  41433e:	41 83 af 1c 01 00 00 	subl   $0x1,0x11c(%r15)
  414345:	01 
  414346:	48 29 d0             	sub    %rdx,%rax
  414349:	49 39 c0             	cmp    %rax,%r8
  41434c:	73 07                	jae    414355 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc85>
  41434e:	42 80 3c 02 00       	cmpb   $0x0,(%rdx,%r8,1)
  414353:	75 08                	jne    41435d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xc8d>
  414355:	41 83 af 20 01 00 00 	subl   $0x1,0x120(%r15)
  41435c:	01 
  41435d:	49 83 c4 02          	add    $0x2,%r12
  414361:	48 89 ca             	mov    %rcx,%rdx
  414364:	48 89 fb             	mov    %rdi,%rbx
  414367:	e9 94 f6 ff ff       	jmpq   413a00 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x330>
  41436c:	0f 1f 40 00          	nopl   0x0(%rax)
  414370:	c7 44 24 48 01 00 00 	movl   $0x1,0x48(%rsp)
  414377:	00 
  414378:	41 bd 01 00 00 00    	mov    $0x1,%r13d
  41437e:	e9 fe fc ff ff       	jmpq   414081 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9b1>
  414383:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  414388:	bd 01 00 00 00       	mov    $0x1,%ebp
  41438d:	41 bd 01 00 00 00    	mov    $0x1,%r13d
  414393:	e9 76 fb ff ff       	jmpq   413f0e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x83e>
  414398:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  41439f:	00 
  4143a0:	4d 39 ca             	cmp    %r9,%r10
  4143a3:	0f 83 c7 fc ff ff    	jae    414070 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9a0>
  4143a9:	49 8d 73 01          	lea    0x1(%r11),%rsi
  4143ad:	4c 89 44 24 30       	mov    %r8,0x30(%rsp)
  4143b2:	66 0f 7e 44 24 28    	movd   %xmm0,0x28(%rsp)
  4143b8:	e8 d3 09 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  4143bd:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  4143c4:	83 43 08 01          	addl   $0x1,0x8(%rbx)
  4143c8:	4c 8b 44 24 30       	mov    0x30(%rsp),%r8
  4143cd:	49 8b bf 90 01 00 00 	mov    0x190(%r15),%rdi
  4143d4:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  4143db:	66 0f 6e 44 24 28    	movd   0x28(%rsp),%xmm0
  4143e1:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  4143e5:	8b 50 04             	mov    0x4(%rax),%edx
  4143e8:	e9 b9 f7 ff ff       	jmpq   413ba6 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4d6>
  4143ed:	0f 1f 00             	nopl   (%rax)
  4143f0:	4c 39 d5             	cmp    %r10,%rbp
  4143f3:	0f 83 07 fb ff ff    	jae    413f00 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x830>
  4143f9:	49 8d 73 01          	lea    0x1(%r11),%rsi
  4143fd:	48 89 44 24 38       	mov    %rax,0x38(%rsp)
  414402:	4c 89 44 24 30       	mov    %r8,0x30(%rsp)
  414407:	66 0f 7e 44 24 28    	movd   %xmm0,0x28(%rsp)
  41440d:	e8 7e 09 01 00       	callq  424d90 <std::vector<unsigned int, std::allocator<unsigned int> >::resize(unsigned long)>
  414412:	48 8b 44 24 38       	mov    0x38(%rsp),%rax
  414417:	4d 8b 97 78 01 00 00 	mov    0x178(%r15),%r10
  41441e:	4c 8b 44 24 30       	mov    0x30(%rsp),%r8
  414423:	49 8b bf 90 01 00 00 	mov    0x190(%r15),%rdi
  41442a:	83 40 08 01          	addl   $0x1,0x8(%rax)
  41442e:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  414435:	4b 8d 04 02          	lea    (%r10,%r8,1),%rax
  414439:	66 0f 6e 44 24 28    	movd   0x28(%rsp),%xmm0
  41443f:	8b 70 04             	mov    0x4(%rax),%esi
  414442:	e9 82 f7 ff ff       	jmpq   413bc9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x4f9>
  414447:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
  41444e:	00 00 
  414450:	8b 44 24 0c          	mov    0xc(%rsp),%eax
  414454:	8b 6c 24 68          	mov    0x68(%rsp),%ebp
  414458:	4c 89 64 24 10       	mov    %r12,0x10(%rsp)
  41445d:	4d 89 d1             	mov    %r10,%r9
  414460:	41 c6 87 d1 02 00 00 	movb   $0x1,0x2d1(%r15)
  414467:	01 
  414468:	4c 8b 64 24 50       	mov    0x50(%rsp),%r12
  41446d:	41 c7 87 d4 02 00 00 	movl   $0xffffffff,0x2d4(%r15)
  414474:	ff ff ff ff 
  414478:	41 89 87 d8 02 00 00 	mov    %eax,0x2d8(%r15)
  41447f:	41 89 8f dc 02 00 00 	mov    %ecx,0x2dc(%r15)
  414486:	48 8b 44 24 48       	mov    0x48(%rsp),%rax
  41448b:	4d 89 ca             	mov    %r9,%r10
  41448e:	4c 01 c8             	add    %r9,%rax
  414491:	49 39 d3             	cmp    %rdx,%r11
  414494:	0f 84 4c 02 00 00    	je     4146e6 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x1016>
  41449a:	4c 89 de             	mov    %r11,%rsi
  41449d:	48 29 d6             	sub    %rdx,%rsi
  4144a0:	48 8d 7e fc          	lea    -0x4(%rsi),%rdi
  4144a4:	48 83 ff 08          	cmp    $0x8,%rdi
  4144a8:	0f 86 b2 00 00 00    	jbe    414560 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe90>
  4144ae:	48 8d 4a 04          	lea    0x4(%rdx),%rcx
  4144b2:	49 89 d8             	mov    %rbx,%r8
  4144b5:	49 29 c8             	sub    %rcx,%r8
  4144b8:	31 c9                	xor    %ecx,%ecx
  4144ba:	49 83 f8 08          	cmp    $0x8,%r8
  4144be:	77 30                	ja     4144f0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe20>
  4144c0:	8b 3c 0a             	mov    (%rdx,%rcx,1),%edi
  4144c3:	89 3c 0b             	mov    %edi,(%rbx,%rcx,1)
  4144c6:	48 83 c1 04          	add    $0x4,%rcx
  4144ca:	48 39 f1             	cmp    %rsi,%rcx
  4144cd:	75 f1                	jne    4144c0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdf0>
  4144cf:	48 8d 14 33          	lea    (%rbx,%rsi,1),%rdx
  4144d3:	8b 48 04             	mov    0x4(%rax),%ecx
  4144d6:	41 bd 01 00 00 00    	mov    $0x1,%r13d
  4144dc:	4c 29 e2             	sub    %r12,%rdx
  4144df:	48 c1 fa 02          	sar    $0x2,%rdx
  4144e3:	e9 e8 f5 ff ff       	jmpq   413ad0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x400>
  4144e8:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
  4144ef:	00 
  4144f0:	48 c1 ef 02          	shr    $0x2,%rdi
  4144f4:	48 83 c7 01          	add    $0x1,%rdi
  4144f8:	49 89 f8             	mov    %rdi,%r8
  4144fb:	49 c1 e8 02          	shr    $0x2,%r8
  4144ff:	49 c1 e0 04          	shl    $0x4,%r8
  414503:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)
  414508:	f3 0f 6f 1c 0a       	movdqu (%rdx,%rcx,1),%xmm3
  41450d:	0f 11 1c 0b          	movups %xmm3,(%rbx,%rcx,1)
  414511:	48 83 c1 10          	add    $0x10,%rcx
  414515:	4c 39 c1             	cmp    %r8,%rcx
  414518:	75 ee                	jne    414508 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xe38>
  41451a:	40 f6 c7 03          	test   $0x3,%dil
  41451e:	74 af                	je     4144cf <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdff>
  414520:	48 83 e7 fc          	and    $0xfffffffffffffffc,%rdi
  414524:	48 8d 0c bd 00 00 00 	lea    0x0(,%rdi,4),%rcx
  41452b:	00 
  41452c:	48 01 ca             	add    %rcx,%rdx
  41452f:	48 01 d9             	add    %rbx,%rcx
  414532:	8b 3a                	mov    (%rdx),%edi
  414534:	89 39                	mov    %edi,(%rcx)
  414536:	48 8d 7a 04          	lea    0x4(%rdx),%rdi
  41453a:	49 39 fb             	cmp    %rdi,%r11
  41453d:	74 90                	je     4144cf <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdff>
  41453f:	8b 7a 04             	mov    0x4(%rdx),%edi
  414542:	89 79 04             	mov    %edi,0x4(%rcx)
  414545:	48 8d 7a 08          	lea    0x8(%rdx),%rdi
  414549:	49 39 fb             	cmp    %rdi,%r11
  41454c:	74 81                	je     4144cf <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdff>
  41454e:	8b 52 08             	mov    0x8(%rdx),%edx
  414551:	89 51 08             	mov    %edx,0x8(%rcx)
  414554:	e9 76 ff ff ff       	jmpq   4144cf <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdff>
  414559:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
  414560:	31 c9                	xor    %ecx,%ecx
  414562:	e9 59 ff ff ff       	jmpq   4144c0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdf0>
  414567:	8b 32                	mov    (%rdx),%esi
  414569:	49 8b 97 60 01 00 00 	mov    0x160(%r15),%rdx
  414570:	0f 18 1c b2          	prefetcht2 (%rdx,%rsi,4)
  414574:	e9 08 fd ff ff       	jmpq   414281 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xbb1>
  414579:	be 03 00 00 00       	mov    $0x3,%esi
  41457e:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  414583:	48 89 7c 24 28       	mov    %rdi,0x28(%rsp)
  414588:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  41458e:	e8 3d 07 01 00       	callq  424cd0 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  414593:	8b 53 04             	mov    0x4(%rbx),%edx
  414596:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  41459b:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  4145a2:	4d 8b 8f 70 01 00 00 	mov    0x170(%r15),%r9
  4145a9:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  4145af:	48 8b 7c 24 28       	mov    0x28(%rsp),%rdi
  4145b4:	e9 8e fa ff ff       	jmpq   414047 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x977>
  4145b9:	be 03 00 00 00       	mov    $0x3,%esi
  4145be:	48 89 44 24 40       	mov    %rax,0x40(%rsp)
  4145c3:	4c 89 44 24 38       	mov    %r8,0x38(%rsp)
  4145c8:	48 89 7c 24 28       	mov    %rdi,0x28(%rsp)
  4145cd:	66 0f 7e 44 24 30    	movd   %xmm0,0x30(%rsp)
  4145d3:	e8 f8 06 01 00       	callq  424cd0 <std::vector<unsigned int, std::allocator<unsigned int> >::reserve(unsigned long)>
  4145d8:	48 8b 44 24 40       	mov    0x40(%rsp),%rax
  4145dd:	4c 8b 44 24 38       	mov    0x38(%rsp),%r8
  4145e2:	4d 8b 9f 60 01 00 00 	mov    0x160(%r15),%r11
  4145e9:	4d 8b 97 70 01 00 00 	mov    0x170(%r15),%r10
  4145f0:	8b 70 04             	mov    0x4(%rax),%esi
  4145f3:	66 0f 6e 44 24 30    	movd   0x30(%rsp),%xmm0
  4145f9:	48 8b 7c 24 28       	mov    0x28(%rsp),%rdi
  4145fe:	e9 d4 f8 ff ff       	jmpq   413ed7 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x807>
  414603:	41 89 c0             	mov    %eax,%r8d
  414606:	e9 47 f1 ff ff       	jmpq   413752 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x82>
  41460b:	48 8d 42 1c          	lea    0x1c(%rdx),%rax
  41460f:	48 39 44 24 40       	cmp    %rax,0x40(%rsp)
  414614:	0f 84 ce fb ff ff    	je     4141e8 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb18>
  41461a:	48 89 54 24 30       	mov    %rdx,0x30(%rsp)
  41461f:	48 8b 54 24 40       	mov    0x40(%rsp),%rdx
  414624:	e9 a8 fb ff ff       	jmpq   4141d1 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb01>
  414629:	89 87 f0 00 00 00    	mov    %eax,0xf0(%rdi)
  41462f:	41 89 c4             	mov    %eax,%r12d
  414632:	e9 a2 f1 ff ff       	jmpq   4137d9 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x109>
  414637:	49 01 97 88 06 00 00 	add    %rdx,0x688(%r15)
  41463e:	e9 5b f1 ff ff       	jmpq   41379e <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xce>
  414643:	45 8b 12             	mov    (%r10),%r10d
  414646:	49 8b af 60 01 00 00 	mov    0x160(%r15),%rbp
  41464d:	42 0f 18 5c 95 00    	prefetcht2 0x0(%rbp,%r10,4)
  414653:	e9 e7 f7 ff ff       	jmpq   413e3f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x76f>
  414658:	4c 89 c8             	mov    %r9,%rax
  41465b:	0f 85 22 fb ff ff    	jne    414183 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xab3>
  414661:	8b 16                	mov    (%rsi),%edx
  414663:	89 17                	mov    %edx,(%rdi)
  414665:	41 8b 51 04          	mov    0x4(%r9),%edx
  414669:	e9 15 fb ff ff       	jmpq   414183 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xab3>
  41466e:	48 89 c8             	mov    %rcx,%rax
  414671:	0f 85 96 f9 ff ff    	jne    41400d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x93d>
  414677:	41 8b 11             	mov    (%r9),%edx
  41467a:	89 17                	mov    %edx,(%rdi)
  41467c:	8b 71 04             	mov    0x4(%rcx),%esi
  41467f:	e9 89 f9 ff ff       	jmpq   41400d <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x93d>
  414684:	44 8b 64 24 6c       	mov    0x6c(%rsp),%r12d
  414689:	89 e8                	mov    %ebp,%eax
  41468b:	45 31 c0             	xor    %r8d,%r8d
  41468e:	e9 a7 f5 ff ff       	jmpq   413c3a <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x56a>
  414693:	45 31 ed             	xor    %r13d,%r13d
  414696:	e9 35 f4 ff ff       	jmpq   413ad0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x400>
  41469b:	ba 03 00 00 00       	mov    $0x3,%edx
  4146a0:	e9 97 f8 ff ff       	jmpq   413f3c <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x86c>
  4146a5:	ba 03 00 00 00       	mov    $0x3,%edx
  4146aa:	e9 fd f9 ff ff       	jmpq   4140ac <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x9dc>
  4146af:	4c 89 44 24 10       	mov    %r8,0x10(%rsp)
  4146b4:	8b 6c 24 68          	mov    0x68(%rsp),%ebp
  4146b8:	48 89 fb             	mov    %rdi,%rbx
  4146bb:	48 89 ca             	mov    %rcx,%rdx
  4146be:	41 c6 87 d1 02 00 00 	movb   $0x1,0x2d1(%r15)
  4146c5:	01 
  4146c6:	4c 8b 64 24 50       	mov    0x50(%rsp),%r12
  4146cb:	45 89 af d4 02 00 00 	mov    %r13d,0x2d4(%r15)
  4146d2:	4d 8b 8f 78 01 00 00 	mov    0x178(%r15),%r9
  4146d9:	e9 a8 fd ff ff       	jmpq   414486 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xdb6>
  4146de:	45 31 c9             	xor    %r9d,%r9d
  4146e1:	e9 79 fb ff ff       	jmpq   41425f <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0xb8f>
  4146e6:	41 bd 01 00 00 00    	mov    $0x1,%r13d
  4146ec:	e9 cf f3 ff ff       	jmpq   413ac0 <tactix::kernel_propagate(tactix::Kernel&, unsigned int)+0x3f0>

Disassembly of section .fini:
