#!/usr/bin/env python3
"""Reference-constant cooling arithmetic, not an annealing/solver experiment."""
import json
rows = []
for width in (2, 21, 42, 50, 64, 85, 128):
    initial = max(1., width * width / 3.)
    temperature, cooling, after = initial, 0, None
    while temperature > .05:
        temperature = max(.05, temperature * 999970 / 1000000)
        cooling += 1
        if cooling == 100000:
            after = temperature
    rows.append(dict(separation=width, initial_temperature=initial,
                     cooling_updates_to_floor=cooling,
                     temperature_after_100000_updates=after,
                     maximum_attempts_in_experimental_stall=100000))
print(json.dumps(rows, indent=2))
