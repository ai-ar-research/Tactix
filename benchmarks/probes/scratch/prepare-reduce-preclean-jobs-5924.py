#!/usr/bin/env python3
"""Fresh combined-candidate regression and causal gates; no existing edits."""
from pathlib import Path
import re
source = Path(__file__).with_name('prepare-reduce-root-jobs-5906.py')
code = source.read_text().replace('reduce-root', 'reduce-preclean')
mapping = {'5900':'5920','5901':'5921','5902':'5922','5906':'5924','5907':'5925'}
code = re.sub('|'.join(mapping), lambda m: mapping[m.group()], code)
code = code.replace('independent Reduce root-guidance cleanup5920', 'combined Reduce/root-preconnection cleanup5920')
code = code.replace('Removing newly root-fixed identities after Reduce preparation preserves verdicts and reduces excess search work',
    'Combining correct root guidance and incoming-root occurrence cleanup preserves verdicts and improves cumulative work')
code = code.replace('Consume newly derived roots after Reduce preparation before Reorder. Compare every decision against cached native;expect to close225987 but report any earlier or later split. No change to preclean5870 branch or fixed runtime denominator.',
    'Compose tested preconnection cleanup on5900. Compare every decision against cached native and second forward/BVE boundary271079;expect to close271085 but report earlier splits. Neither standalone5870 regression nor fixed reference is changed.')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
