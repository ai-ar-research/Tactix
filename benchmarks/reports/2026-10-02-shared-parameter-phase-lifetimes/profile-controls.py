from pathlib import Path
import importlib.util
import re

root = next(parent for parent in Path(__file__).resolve().parents if (parent / 'CLAUDE.md').is_file())
spec = importlib.util.spec_from_file_location('wiring', root / 'tools/docs/check-config-wiring.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
accepted = ['install_parameters(k);', 'install_parameters(k, completion);', 'install_parameters( k , completion );']
rejected = ['install_parameters(other);', 'install_parameters(other, completion);',
            'install_parameters(k, other);', 'install_parameters();', 'install_parameters(k, completion, other);']
for call in accepted:
    assert re.fullmatch(module.PROFILE_CALL, call)
for call in rejected:
    assert not re.fullmatch(module.PROFILE_CALL, call)
module.binding_self_test()
print('Profile owner signatures: 3 legitimate accepted; 5 incorrect rejected.')
