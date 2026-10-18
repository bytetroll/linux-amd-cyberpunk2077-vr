#!/usr/bin/env python3
"""Reversible workaround for this exact Proton Experimental OpenXR module.

Only Win64 projection-first layer arrays are covered. The accompanying source
patch fixes per-element allocation generally. Steam updates may replace this
module; this script refuses versions it has not verified.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import sys
import tempfile

DEFAULT_BACKUP = Path(os.environ.get('XDG_STATE_HOME', Path.home() / '.local/state')) / 'linux-amd-cyberpunk2077-vr/proton-openxr/wineopenxr.so'
ORIGINAL = '2e8bf9bd70483dd7c5887902e707dbd878fb5b8c5ed17e50ec1f774ccdcbda4a'
PATCHED = '07bc4e927eefc84c604ffff0cf70641c61bab97e1241585b2a53a60623fd0087'
INSTRUCTION_OFFSET = 0x50e25
ORIGINAL_INSTRUCTION = bytes.fromhex('c7 45 cc 30 00 00 00')
BYTE_OFFSET = INSTRUCTION_OFFSET + 3


def digest(data):
    return hashlib.sha256(data).hexdigest()


def active_clients(module):
    """Do not replace files while a game or client using this module is running."""
    clients = []
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit() or int(proc.name) == os.getpid():
            continue
        try:
            args = (proc / 'cmdline').read_bytes().split(b'\0')
            game = any(arg.replace(b'\\', b'/').rsplit(b'/', 1)[-1].lower()
                       == b'cyberpunk2077.exe' for arg in args)
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            game = False
        try:
            mapped = str(module) in (proc / 'maps').read_text(errors='replace')
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            mapped = False
        if game or mapped:
            clients.append(proc.name)
    return clients


def preserve_original(module, backup, data):
    if backup.exists():
        if digest(backup.read_bytes()) != ORIGINAL:
            raise RuntimeError('Existing backup has an unexpected checksum; refusing to overwrite it.')
        return
    backup.parent.mkdir(parents=True, exist_ok=True)
    with backup.open('xb') as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    shutil.copystat(module, backup)
    if digest(backup.read_bytes()) != ORIGINAL:
        raise RuntimeError('Backup verification failed; installed file was not changed.')


def replace_module(module, previous, replacement):
    metadata = module.stat()
    if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid != os.geteuid():
        raise RuntimeError('Module must be a regular file owned by the current user.')
    fd, name = tempfile.mkstemp(prefix='.wineopenxr-hotfix-', dir=module.parent)
    temporary = Path(name)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(replacement)
            stream.flush()
            os.fsync(stream.fileno())
        shutil.copystat(module, temporary)
        if digest(temporary.read_bytes()) != digest(replacement):
            raise RuntimeError('Temporary module verification failed.')
        clients = active_clients(module)
        if clients:
            raise RuntimeError('Close Cyberpunk and clients using this module first. PIDs: ' + ', '.join(clients))
        if module.read_bytes() != previous:
            raise RuntimeError('Module changed during preparation; refusing replacement.')
        os.replace(temporary, module)
        directory_fd = os.open(module.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
        if module.read_bytes() != replacement:
            raise RuntimeError('Installed module verification failed; backup is preserved.')
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', nargs='?', choices=['status', 'apply', 'restore'], default='status')
    parser.add_argument('--module', type=Path, required=True, help='Exact wineopenxr.so to inspect or patch; no automatic runtime selection.')
    parser.add_argument('--backup', type=Path, default=DEFAULT_BACKUP)
    args = parser.parse_args()
    module, backup = args.module.resolve(strict=True), args.backup.resolve()
    if module == backup:
        raise RuntimeError('Backup and installed module must be different files.')
    data = module.read_bytes()
    checksum = digest(data)
    state = {ORIGINAL: 'original', PATCHED: 'hotfix installed'}.get(checksum, 'unrecognized version')
    if args.action == 'status':
        print(json.dumps({'module': str(module), 'state': state, 'sha256': checksum,
                          'backup': str(backup), 'backup_exists': backup.exists()}, indent=2))
        return
    if checksum not in (ORIGINAL, PATCHED):
        raise RuntimeError('Unrecognized module version; no file was changed. Do not reuse this patch after a Steam update without re-verification.')
    clients = active_clients(module)
    if clients:
        raise RuntimeError('Close Cyberpunk and clients using this module first. PIDs: ' + ', '.join(clients))
    if args.action == 'apply':
        if checksum == PATCHED:
            print('Hotfix already installed; no change.')
            return
        if data[INSTRUCTION_OFFSET:INSTRUCTION_OFFSET + 7] != ORIGINAL_INSTRUCTION:
            raise RuntimeError('Instruction guard failed; no file was changed.')
        replacement = bytearray(data)
        replacement[BYTE_OFFSET] = 0x78
        replacement = bytes(replacement)
        if digest(replacement) != PATCHED:
            raise RuntimeError('Patched checksum guard failed; no file was changed.')
        preserve_original(module, backup, data)
    else:
        if checksum == ORIGINAL:
            print('Original module already installed; no change.')
            return
        replacement = backup.read_bytes()
        if digest(replacement) != ORIGINAL:
            raise RuntimeError('Original backup verification failed; no file was changed.')
    replace_module(module, data, replacement)
    print(json.dumps({'action': args.action, 'module': str(module), 'sha256': digest(replacement),
                      'backup': str(backup), 'verified': True}, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError) as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        sys.exit(1)
