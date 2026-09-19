#!/usr/bin/env python3
"""Run existing ReadyBASIC plans on physical Ultimate from Terminal-owned bash.

Requires PyYAML and a built D81 whose launcher starts ReadyBASIC. Assertions
run through vice_tasks_dotnet's ultimate64 backend. Disk-triggering RETURNs
are injected without the backend's post-key RAM polling. At each video gate,
inspect pending.json's PNG, then create its acknowledgement file ONLY after
visually confirming loading has completed. Delays never satisfy a gate.
"""
import argparse
import copy
import hashlib
from ftplib import FTP
import io
import json
import os
from pathlib import Path
import re
import subprocess
import time
from urllib.parse import urlencode
from urllib.request import Request, urlopen
import uuid

import yaml

ROOT = Path(__file__).resolve().parents[1]
HOST = os.environ.get('C64U_HOST', '10.0.0.79')


def prepare_runner(out):
    """Build an isolated Ultimate runner; never replace a shared VICE binary."""
    if os.environ.get('VICE_TASKS_BINARY'):
        return Path(os.environ['VICE_TASKS_BINARY'])
    source = ROOT.parent / 'agenticdevharness/tools/vice_tasks_dotnet/src/ViceTasks.Binary'
    directory = out / 'harness'
    directory.mkdir()
    original = (source / 'Program.cs').read_text(encoding='utf-8-sig')
    code = original
    # Some harness revisions implement clear_reu but omit its validator entries.
    assert '"ultimate.clear_reu" => ' in code, 'Harness lacks REU clear implementation'
    for section in ('SupportedStepTypes', 'ControlStepTypes'):
        start = code.index('private static readonly HashSet<string> ' + section)
        end = code.index('];', start)
        region = code[start:end]
        if '"ultimate.clear_reu"' not in region:
            code = code[:start] + region.replace('"ultimate.launch",',
                '"ultimate.launch",\n        "ultimate.clear_reu",') + code[end:]
    # Reuse the existing ten-byte keyboard-buffer helper for ordinary input.
    # Disk-triggering RETURNs are still separated and gated by this adapter.
    code = code.replace('await _client.WriteKeySequenceAsync(parsed, TimeSpan.FromSeconds(inter));',
                        'await _client.WriteKeyboardBufferAsync(parsed, TimeSpan.FromSeconds(inter));')
    code = code.replace('DateTime.Now.ToString("yyyyMMdd_HHmmss",',
                        'DateTime.Now.ToString("yyyyMMdd_HHmmss_fff",')
    (directory / 'Program.cs').write_text(code)
    project = (source / 'ViceTasks.Binary.csproj').read_text(encoding='utf-8-sig')
    project = project.replace('..\\ViceTasks.Ultimate\\ViceTasks.Ultimate.csproj',
                              str(source.parent / 'ViceTasks.Ultimate/ViceTasks.Ultimate.csproj'))
    project_path = directory / 'ViceTasks.Binary.csproj'
    project_path.write_text(project)
    (directory / 'source.json').write_text(json.dumps(dict(source=str(source),
        original_sha256=hashlib.sha256(original.encode()).hexdigest(),
        adapted_sha256=hashlib.sha256(code.encode()).hexdigest()), indent=2))
    subprocess.run(['dotnet', 'build', str(project_path), '-v', 'quiet'], check=True)
    return directory / 'bin/Debug/net8.0/ViceTasks.Binary.dll'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--disk', type=Path, required=True)
    parser.add_argument('--video-helper', type=Path, required=True)
    parser.add_argument('--remote-image', required=True,
                        help='Exact fresh Ultimate path already encoded in the disk apps.cfg')
    parser.add_argument('--reuse-verified-image', action='store_true',
                        help='Read back a previously uploaded owned test image without rewriting it')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--resume-from', help='Resume one plan at this step on the same idle, unchanged machine')
    parser.add_argument('--resume-command', help='Explicit BASIC fixture repair before resuming (recorded in results)')
    parser.add_argument('plans', nargs='+', type=Path)
    args = parser.parse_args()
    assert not args.resume_from or len(args.plans) == 1, 'Resume accepts one plan'
    assert not args.resume_command or args.resume_from, 'Fixture repair requires resume'
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    harness = prepare_runner(out)
    tag = uuid.uuid4().hex[:8]
    records = []
    counter = 0
    suite = ''

    def api(route, method='GET', data=None):
        body = None if data is None else json.dumps(data).encode()
        with urlopen(Request(f'http://{HOST}/v1/{route}', method=method,
                             data=body, headers={'Content-Type': 'application/json'}), timeout=30) as response:
            result = json.load(response)
        assert not result.get('errors'), result
        return result

    def write(address, data):
        api(f'machine:writemem?address={address:04X}&data={bytes(data).hex()}', 'PUT')

    def video_gate(label):
        nonlocal counter
        counter += 1
        stem = f'{counter:04d}-{suite}-{label}'
        png = out / (stem + '.png')
        ack = out / (stem + '.confirmed')
        pending = dict(label=label, suite=suite, image=str(png), acknowledge=str(ack))
        (out / 'pending.json').write_text(json.dumps(pending, indent=2))
        print('VIDEO GATE', json.dumps(pending), flush=True)
        deadline = time.monotonic() + 900
        while not ack.exists():
            if time.monotonic() > deadline:
                raise TimeoutError('No visual loading-complete confirmation: ' + label)
            subprocess.run(['dotnet', str(args.video_helper.resolve()), str(png)], check=True)
            time.sleep(3)
        records.append(dict(video_gate=stem, image=str(png), confirmation=ack.read_text()))
        (out / 'pending.json').unlink(missing_ok=True)

    defaults = dict(retry_policy=dict(max_attempts=1, backoff_ms=250, jitter=False),
                    timeouts=dict(launch_s=90, step_s=240, read_s=20),
                    artifact_policy=dict(capture_screen=True, capture_state=True, capture_dump=False),
                    ultimate=dict(host=HOST, disk8='', disk9='', drive_a_enabled=True,
                                  drive_a_bus_id=8, drive_a_type='1581', drive_b_enabled=False,
                                  capture_video_stream=True, stream_port=11005, stream_timeout_s=8))
    attach = dict(id='attach_idle_ultimate', type='ultimate.launch', params=dict(boot_mode='none'))

    def block(steps):
        nonlocal counter
        if not steps:
            return
        counter += 1
        planid = f'rbc64u_{tag}_{suite}_{counter:04d}'
        plan = dict(version=1, kind='ultimate_task_plan', plan_id=planid,
                    run_mode='ultimate64', global_defaults=defaults, steps=[attach] + steps)
        path = out / (planid + '.json')
        path.write_text(json.dumps(plan, indent=2))
        print('HARNESS', planid, [s['id'] for s in steps], flush=True)
        result = subprocess.run(['dotnet', str(harness), 'run-ultimate-plan', '--plan', str(path), '--no-tui'],
                                cwd=ROOT, env={**os.environ, 'ULTIMATE_ASSUME_MOUNTED': '1'})
        manifests = [p for p in (ROOT / 'logs').glob('ultimate_auto_*/manifest.json')
                     if p.parent.name != 'ultimate_auto_latest' and
                     json.loads(p.read_text(encoding='utf-8-sig')).get('plan_id') == planid]
        assert len(manifests) == 1, manifests
        manifest = json.loads(manifests[0].read_text(encoding='utf-8-sig'))
        archived_manifest = out / (planid + '.manifest.json')
        archived_manifest.write_text(json.dumps(manifest, indent=2))
        records.append(dict(plan_id=planid, manifest=str(archived_manifest), status=manifest['status']))
        (out / 'results.json').write_text(json.dumps(records, indent=2))
        assert result.returncode == 0 and manifest['status'] == 'success', manifest.get('failure_message')

    def trigger(line, label):
        # The final RETURN starts disk work. No RAM reads after that write.
        if line[:-1]:
            block([dict(id=label + '_prepare', type='input.sequence', params=dict(
                keys=list(line[:-1]), inter_key_delay_s=.03, post_delay_s=.2))])
        write(0xc5, [0, 0])
        write(0x277, [line[-1]])
        write(0xc6, [1])
        video_gate(label)

    def loading_line(line):
        text = line.decode('ascii', errors='replace').strip().upper()
        if re.match(r'^\d+\s', text) or text.startswith('REM '):
            return False  # Editing a stored BASIC line does not execute it.
        return (re.search(r'\b(LDMOD|LOAD|SAVE|RUN|EXIT|MUSTUNE|RSCFILE|SPRFILE|MCFILE)\b', text)
                or any(c in line for c in (2, 134, 136)))

    remote = '/' + args.remote_image.lstrip('/')
    assert remote.startswith('/USB1/automation/readybasic-suites/'), remote
    embedded_config = out / 'embedded-apps.cfg'
    subprocess.run(['c1541', str(args.disk.resolve()), '-read', 'apps.cfg,s', str(embedded_config)], check=True)
    config = dict(line.split('=', 1) for line in embedded_config.read_bytes().decode('latin1').split('\r') if '=' in line)
    assert config['DMA_LOADING'] == '1', 'Test disk must enable DMA'
    assert config['RUNAPPFIRST'] == 'READYBASIC', 'Test disk must start ReadyBASIC through launcher'
    assert config['C64U_IMAGE_PATH'].casefold() == remote.casefold(), 'apps.cfg DMA path differs from upload path'
    records.append(dict(disk=str(args.disk.resolve()), remote=remote, dma_config_verified=True))
    for suite_index, source in enumerate(args.plans):
        source = source.resolve()
        plan = yaml.safe_load(source.read_text())
        suite = plan['plan_id'].removeprefix('readybasic_')
        print('SUITE', suite, flush=True)
        if args.resume_from:
            index = next(i for i, step in enumerate(plan['steps']) if step['id'] == args.resume_from)
            plan['steps'] = plan['steps'][index:]
            video_gate('resume_idle_prompt')
            drives = api('drives')['drives']
            drive = next(d['a'] for d in drives if 'a' in d)
            mounted = drive['image_path'].rstrip('/') + '/' + drive['image_file']
            assert mounted.casefold() == remote.casefold(), 'Resume image differs from expected fixture'
            records.append(dict(suite=suite, resume_from=args.resume_from, resume_command=args.resume_command))
            if args.resume_command:
                assert not any(c in args.resume_command for c in '\r\n')
                line = args.resume_command.encode('ascii') + b'\r'
                if loading_line(line):
                    trigger(line, 'resume_fixture_repair')
                else:
                    block([dict(id='resume_fixture_repair', type='input.sequence',
                                params=dict(keys=list(line), inter_key_delay_s=.03, post_delay_s=1))])
        else:
            # One exact-path DMA image, remounted unlinked for each cold suite.
            data = args.disk.read_bytes()
            with FTP(HOST, timeout=90) as ftp:
                ftp.login()
                ftp.cwd('/USB1/automation')
                try:
                    ftp.mkd('readybasic-suites')
                except Exception:
                    ftp.cwd('readybasic-suites')
                else:
                    ftp.cwd('readybasic-suites')
                folder, filename = remote.rsplit('/', 2)[1:]
                if suite_index == 0 and not args.reuse_verified_image:
                    ftp.mkd(folder)  # Fail rather than overwrite another test image.
                ftp.cwd(folder)
                if suite_index == 0 and not args.reuse_verified_image:
                    ftp.storbinary('STOR ' + filename, io.BytesIO(data))
                readback = io.BytesIO()
                ftp.retrbinary('RETR ' + filename, readback.write)
                assert readback.getvalue() == data, 'FTP readback mismatch'
            api('machine:reset', 'PUT')
            api('configs', 'POST', {'Drive A Settings': {'Drive': 'Enabled', 'Drive Type': '1581', 'Drive Bus ID': 8},
                                  'U64 Specific Settings': {'Turbo Control': 'Off', 'CPU Speed': ' 1'}})
            api('drives/a:mount?' + urlencode(dict(image=remote, type='d81', mode='unlinked')), 'PUT')
            video_gate('stock_prompt_after_reset')
            block([dict(id='clear_stale_reu', type='ultimate.clear_reu',
                        params=dict(banks=256, clear_wait_s=20, reset_after=True))])
            video_gate('stock_prompt_after_reu_clear')
            trigger(b'LOAD "*",8,1\r', 'preboot_load_complete')
            trigger(b'RUN\r', 'readybasic_boot_complete')
        pending = []
        for original in plan['steps']:
            step = copy.deepcopy(original)
            kind, label = step['type'], step['id']
            if kind == 'vice.launch':
                continue
            if kind == 'monitor.command':
                command = step['params']['command']
                if command == 'r':
                    records.append(dict(suite=suite, omitted=label, reason='CPU register diagnostic unavailable over Ultimate REST'))
                    continue
                match = re.fullmatch(r'keybuf \\+x([0-9a-fA-F]{2})', command)
                if not match:
                    raise ValueError(f'Unmapped monitor operation: {label}: {command}')
                step = dict(id=label, type='input.sequence', params=dict(keys=[int(match[1], 16)]))
                block(pending)
                pending = []
                trigger(bytes(step['params']['keys']), label)
                continue
            if kind == 'memory.write':
                block(pending)
                pending = []
                params = step['params']
                print('MEMORY WRITE', label, params, flush=True)
                write(params.get('address', params.get('start')), bytes.fromhex(params.get('bytes_hex', params.get('hex', ''))))
                continue
            if kind == 'input.sequence':
                keys = bytes(step['params']['keys'])
                lines = re.findall(rb'[^\r]*\r|[^\r]+$', keys)
                if any(loading_line(line) for line in lines):
                    block(pending)
                    pending = []
                    for index, line in enumerate(lines):
                        part = f'{label}_{index}'
                        if loading_line(line):
                            trigger(line, part)
                        else:
                            params = dict(step['params'], keys=list(line))
                            block([dict(id=part, type='input.sequence', params=params)])
                    continue
            pending.append(step)
        block(pending)
        records.append(dict(suite=suite, status='tail_passed' if args.resume_from else 'passed',
                            resume_from=args.resume_from))
        (out / 'results.json').write_text(json.dumps(records, indent=2))
        print('SUITE TAIL PASSED' if args.resume_from else 'SUITE PASSED', suite, flush=True)
    (out / 'complete').write_text('Selected resumed range passed.\n' if args.resume_from
                                else 'All selected suites passed.\n')


if __name__ == '__main__':
    main()
