"""Strict self-PC attribution across differing diagnostic C++ demanglers."""


def bind_self_pc(pc, symbol, inventory, selected):
    """Return an inventory name, or None only for an unselected symbol.

    Symbol spelling is diagnostic metadata; the binary's disjoint code interval
    and exact instruction inventory own identity. Only the sampled leaf PC may
    be used here, never an unwound caller's return-address-minus-one.
    """
    if not selected(symbol):
        return None
    if type(pc) is not int or pc < 0:
        raise ValueError('invalid self PC')
    matches = []
    for name, record in inventory.items():
        start, size = record['address'], record['size']
        if type(start) is not int or type(size) is not int or start < 0 or size <= 0:
            raise ValueError('invalid code interval')
        if start <= pc < start + size:
            matches.append((name, record))
    if len(matches) != 1:
        raise ValueError('selected sample needs one exact code interval')
    name, record = matches[0]
    if not selected(name) or pc not in record['instruction_addresses']:
        raise ValueError('selected sample not on a selected instruction')
    return name
