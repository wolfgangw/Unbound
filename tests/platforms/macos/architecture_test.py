#!/usr/bin/env python3
"""Audit real Mach-O slices without loading them or touching an installation."""
import pathlib
import shutil
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[3]
stage = pathlib.Path(sys.argv[1]).resolve()
audit = root / 'infrastructure/macos/lua/audit.sh'
libraries = ('UnboundLuaLoader.dylib', 'UnboundHelper.dylib')
architectures = subprocess.check_output(['lipo', '-archs', str(stage/'runtime'/libraries[0])], text=True).split()

def check(candidate, variant, success):
    result = subprocess.run([str(audit), str(candidate), variant], capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stdout + result.stderr

with tempfile.TemporaryDirectory(prefix='unbound-architecture-') as directory:
    candidate = pathlib.Path(directory)/'candidate'
    shutil.copytree(stage, candidate)
    check(candidate, 'universal' if len(architectures) == 2 else architectures[0], True)
    if len(architectures) == 2:
        for architecture in architectures:
            for library in libraries:
                subprocess.run(['lipo', str(stage/'runtime'/library), '-thin', architecture,
                                '-output', str(candidate/'runtime'/library)], check=True)
            check(candidate, architecture, True)
            check(candidate, 'universal', False)
        # A universal loader must not conceal a helper missing one slice.
        shutil.copy2(stage/'runtime'/libraries[0], candidate/'runtime'/libraries[0])
        check(candidate, 'universal', False)
    check(candidate, 'invalid', False)
print('PASS: architecture audits accept matching slices and reject incomplete/mixed payloads')
