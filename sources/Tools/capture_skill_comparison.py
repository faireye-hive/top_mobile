#!/usr/bin/env python3
"""Capture fixed-step skill previews in a disposable copy of user/offline data.

Run under Xvfb or a real DISPLAY. No external Python dependency. The engine
must support SK2.19's fixed preview clock; record that in each run's log.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from run_binding_previews import hashes

CASES = [(84, 'cast', .35), (84, 'cast', .7), (84, 'cast', 1.2),
         (84, 'hit', .35), (84, 'hit', .7), (84, 'hit', 1.2),
         (242, 'cast', .35), (81, 'flight', .35), (81, 'hit', .25),
         (93, 'flight', .35), (93, 'flight', 1.5), (90, 'flight', .35)]


def capture(binary, client, out):
    binary, client, out = (Path(p).resolve() for p in (binary, client, out))
    if client == out or client in out.parents:
        raise ValueError('output must be outside Client')
    out.mkdir(parents=True, exist_ok=False)
    before = hashes(client / 'user')
    offline = client.parent / 'offline'
    offline_before = hashes(offline)
    report = dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  character='1', scope='isolated_channel_preview', step_seconds=1/60,
                  original_user_before=before, runs=[])
    environment = {k: v for k, v in os.environ.items() if not k.startswith('TOP_')}
    environment.update(LIBGL_ALWAYS_SOFTWARE='1', SDL_AUDIODRIVER='dummy',
                       TOP_HISTORICAL_BINDING_TRACE='1')
    with tempfile.TemporaryDirectory(prefix='top-skill-comparison-') as session:
        work = Path(session)
        isolated = work / 'Client'
        isolated.mkdir()
        for source in client.iterdir():
            dest = isolated / source.name
            if source.name == 'user':
                shutil.copytree(source, dest)
            else:
                dest.symlink_to(source, target_is_directory=source.is_dir())
        (isolated / 'user').mkdir(exist_ok=True)
        if offline.is_dir():
            shutil.copytree(offline, work / 'offline')
        archive = client.parent / 'arquivo'
        if archive.is_dir():
            (work / 'arquivo').symlink_to(archive, target_is_directory=True)
        for skill, channel, seconds in CASES:
            # Same unoccluded map position for both revisions. This changes
            # only the disposable session, never the player's original save.
            (isolated / 'user/offline-location-1.txt').write_text(
                'TOPLOC1 garner 2222.5 2770.5 0.0 0\n')
            stem = f'{skill}-{channel}-{seconds}'
            shot = out / (stem + '.ppm')
            command = [str(binary), '--character', '1', '--skill-preview', str(skill),
                       '--skill-preview-channel', channel, '--skill-preview-time', str(seconds),
                       '--shot', str(shot)]
            with (out / (stem + '.log')).open('w') as log:
                try:
                    result = subprocess.run(command, cwd=work, env=environment,
                                            stdout=log, stderr=subprocess.STDOUT, timeout=60)
                    code = result.returncode
                except subprocess.TimeoutExpired:
                    code = 124
            report['runs'].append(dict(skill=skill, channel=channel, seconds=seconds,
                                       exit_code=code, shot_exists=shot.is_file(),
                                       location='garner 2222.5 2770.5',
                                       clock_logged='[skill-preview-clock]' in
                                       (out / (stem + '.log')).read_text(errors='replace')))
            report['original_user_unchanged'] = hashes(client / 'user') == before
            report['original_offline_unchanged'] = hashes(offline) == offline_before
            (out / 'runs.json').write_text(json.dumps(report, indent=2) + '\n')
            print(stem, code, shot.is_file(), flush=True)
            if code or not shot.is_file() or not all((report['original_user_unchanged'],
                                                       report['original_offline_unchanged'])):
                raise RuntimeError('capture failed or original save changed; see runs.json')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--client', type=Path, default=Path('Client'))
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    capture(args.binary, args.client, args.out)
