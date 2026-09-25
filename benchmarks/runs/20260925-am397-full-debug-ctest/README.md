# AM397 current-source full Debug suite

Source is the immutable AM397 archive SHA-256
`105b886f44cbbdb0f82280329fe5a513efd618e4ea0ffabbbe3baaa8a43ff421`
from commit `4039674b`. The runner stages only exact frozen fixture links and
AM369 checker binaries, builds with the rule journal and proof shadow enabled,
then runs the full Debug CTest suite on hwulab. Resources: one job, 12 CPUs,
32 GiB, eight-hour wall time; no array throttle or node constraint. This is a
correctness suite, not a Tier D benchmark or N1--N4 acceptance gate. Terminal
results are pending; submitted as job `1720299`.
