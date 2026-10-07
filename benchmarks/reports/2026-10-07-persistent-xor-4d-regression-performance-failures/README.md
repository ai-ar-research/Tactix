# XOR-enabled 4d regression performance failures — 2026-10-07

Candidate `4d7069a4c3ca8342a96ad93b1409093a83291ba1`, Release SHA256 `64da8d3e0702c3d4a8da338bac90d627dcaacd992f60653073638741a897078d`, arguments `--auto-proof --default-affine-search=on`.

Terminal complete B28+C5, Rank1–50 and WV100 campaigns were scored with the maintained controller-regression tool. All three are rejected. Raw score documents and campaign manifests are frozen here; these results supersede submission-only status, not the earlier different-candidate baseline.

B28+C5: aggregate solve time 1,169.626 seconds exceeds 999.87615; 32 independently verified results and one allowed proof censor. Rank1–50: aggregate 11,556.994 exceeds 8,521.52175 and task 38 loses a baseline retained solve. WV100: task 94 loses a retained solve. Bounded proof censors do not independently close instances.

Terminal Rank task 38 diagnostics identified 96,669 refreshes with zero admitted sources, zero services and zero yield; affine overhead was 684.554838145 seconds with 6,704,763,066 initialization-work units. The next policy increment addresses repeated recognition/maintenance of an empty unchanged session. This is a grounded regression cause, not evidence of the next fix succeeding.

WV2–89 was still running at report creation and is not scored here. Rank 569 remains separately running; there is no current verified speedup or new closure. Do not promote this 4d candidate on these failed gates.
