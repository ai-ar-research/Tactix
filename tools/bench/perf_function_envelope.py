"""Count a complete sampled function envelope independently of phase ancestry."""
from collections import Counter
from perf_callstack import phase


def function_envelope(stream, binary, prefix, entries, caller, sites):
    """Count each target-containing sample once, including external descendants.

    Phase attribution is descriptive: a missing phase frame must not discard
    a present target frame. Unmatched samples remain explicitly outside this
    envelope. This measures sampled inclusive work, not exact invocation time.
    """
    if not binary or not prefix.endswith('('):
        raise ValueError('exact binary and a function-name prefix are required')
    total = selected = count = selected_count = 0
    by_phase, by_leaf, by_entry = Counter(), Counter(), Counter()
    for sample in stream:
        if sample.period <= 0 or not sample.frames:
            raise ValueError('invalid sample')
        total += sample.period
        count += 1
        matches = [f for f in sample.frames
                   if f.dso == binary and f.symbol.startswith(prefix)]
        if not matches:
            continue
        selected += sample.period
        selected_count += 1
        name, _ = phase(sample, binary, entries, caller, sites)
        by_phase[name] += sample.period
        leaf = sample.frames[0]
        by_leaf[leaf.symbol + ' [' + leaf.dso + ']'] += sample.period
        # Recursion/clone aliases are one envelope, not additive frames.
        by_entry[matches[-1].symbol] += sample.period
    assert sum(by_phase.values()) == sum(by_leaf.values()) == sum(by_entry.values()) == selected
    return dict(samples=count, total_period=total, selected_samples=selected_count,
        selected_period=selected, outside_period=total - selected,
        by_phase=dict(by_phase), by_leaf=dict(by_leaf), by_entry=dict(by_entry))
