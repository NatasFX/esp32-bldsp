"""Use the measured board response, rather than fabricated defaults, as C fixture."""
import json
from pathlib import Path
root = Path(__file__).resolve().parent
raw = bytes.fromhex(json.loads((root / 'data/rec_pre_eq_frame.json').read_text())['frame'])
(Path(__file__).parent / 'fixture.h').write_text(
    'static const unsigned char captured[] = {' + ','.join(hex(x) for x in raw) + '};\n')
