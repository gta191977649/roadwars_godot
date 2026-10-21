"""Check that a stub prints trace identity and terminates even with NDEBUG."""
import os
import subprocess
import sys

if os.name == 'nt':
    import ctypes
    ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
result = subprocess.run([sys.argv[1]], capture_output=True, text=True, timeout=10)
assert result.returncode != 0, 'Unimplemented function returned successfully'
assert 'NOT IMPLEMENTED' in result.stderr, result.stderr
assert '00401000' in result.stderr and 'TestClass::missing_method' in result.stderr
assert 'not_implemented_test.cpp' in result.stderr
print('Release-mode fail-fast trace and nonzero termination verified')
