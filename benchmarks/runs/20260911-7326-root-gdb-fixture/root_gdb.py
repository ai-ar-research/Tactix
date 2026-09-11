"""GDB-host script: one external snapshot before old witness storage is freed.

Loaded only by pinned GDB. Registers and memory are read; no inferior calls,
allocation hooks, evaluator function expressions, or solver-source edits.
"""
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import gdb

config_path = Path(os.environ['TACTIX_GDB_CONFIG']).resolve()
config = json.loads(config_path.read_text())
sys.path.insert(0, str(config_path.parent))
from root_memory import snapshot

binary = Path(gdb.current_progspace().filename).resolve()
assert hashlib.sha256(binary.read_bytes()).hexdigest() == config['binary_sha256']
output = Path(os.environ['TACTIX_GDB_OUTPUT'])
assert not output.exists()
stream = output.open('x')
seen, errors, exits = [], [], []

def emit(row):
    stream.write(json.dumps(row) + '\n')
    stream.flush()

def reg(name):
    return int(gdb.selected_frame().read_register(name))

class Capture(gdb.Breakpoint):
    def stop(self):
        try:
            new_start, new_end = reg('r13'), reg('r8')
            if new_end-new_start < config['minimum_new_capacity_bytes']:
                return False
            self.enabled = False
            inferior = gdb.selected_inferior()
            at = reg('pc')
            assert bytes(inferior.read_memory(at, 3)).hex() == config['instruction_hex']
            proc = Path('/proc') / str(inferior.pid)
            assert (proc / 'exe').resolve() == binary
            started = time.time_ns()
            status = (proc / 'status').read_text()
            smaps = (proc / 'smaps').read_text()
            state = snapshot(inferior.read_memory, reg('r14'), config['layout'])
            old = state['vectors']['reconstruction']
            assert old['address'] == reg('rbp') and old['end'] == old['limit']
            assert new_start != old['address'] and new_end-new_start == 2*old['capacity_bytes']
            emit(dict(kind='snapshot', pid=inferior.pid, binary=str(binary),
                before_ns=started, after_ns=time.time_ns(), pc=at, kernel=reg('r14'),
                status_before=status, smaps=smaps, status_after=(proc / 'status').read_text(),
                old_reconstruction=old, new_reconstruction_address=new_start,
                new_reconstruction_capacity_bytes=new_end-new_start, state=state))
            seen.append(at)
            return False
        except Exception as error:
            errors.append(repr(error))
            emit(dict(kind='error', error=repr(error)))
            return True

gdb.events.exited.connect(lambda event: exits.append(getattr(event, 'exit_code', None)))
Capture('*'+hex(config['breakpoint_address']), internal=True)
emit(dict(kind='start', binary=str(binary), config_sha256=hashlib.sha256(config_path.read_bytes()).hexdigest()))
gdb.execute('run')
emit(dict(kind='done', captures=len(seen), errors=errors, exits=exits))
stream.close()
if errors or len(seen) != 1 or not exits or exits[-1] not in config['expected_exit_codes']:
    raise gdb.GdbError('external root snapshot incomplete or failed')
