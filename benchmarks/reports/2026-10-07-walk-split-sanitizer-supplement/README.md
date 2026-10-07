# Split-Walk sanitizer supplement

Committed runtime sourceb56b1b0ce. Fresh Debug GCC13.1 build enables ASan/UBSan, proof shadow and leak detection; event trace/rule journal remain off. All9 focused checks pass in12.14seconds outside ptrace isolation: Walk, split Walk, API N1, shared guidance, old/new replay, restart controller and three inventories. Exact CTest output and configuration are preserved.

Initial isolated execution passes three inventory checks but six executables stop at LeakSanitizer fatal ptrace inspection; that exact output is retained. The identical tests rerun outside isolation with detect_leaks=1 and UBSan halt_on_error, without weakening checks, and pass. Effective Claude-owned fuzz/new-profile benchmark acceptance remain pending.
