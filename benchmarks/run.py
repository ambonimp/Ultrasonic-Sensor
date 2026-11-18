"""Run Pico timing benchmarks or host correctness tests; print results, save no reports."""
import argparse
import csv
import glob
import io
import math
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent

def find_tool(name, pattern):
    found = shutil.which(name)
    if found:
        return found
    installed = sorted((Path.home()/'.pico-sdk').glob(pattern))
    if installed:
        return str(installed[-1])
    raise RuntimeError(f'{name} not found; install the Pico SDK tools or add it to PATH')

def quiet_run(command):
    result = subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
    if result.returncode:
        raise RuntimeError((result.stdout+result.stderr).strip() or f'{command[0]} failed')

def run_host():
    cmake = find_tool('cmake','cmake/*/bin/cmake')
    build = ROOT/'build'/'host-tests'
    print('Building and running C correctness tests...',flush=True)
    quiet_run([cmake,'-S',str(ROOT/'tests'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release'])
    quiet_run([cmake,'--build',str(build),'--target','correlation_test','first_echo_test','-j','4'])
    for name in ('correlation_test','first_echo_test'):
        subprocess.run([str(build/name)],cwd=ROOT,check=True)

def flash_benchmark():
    cmake = find_tool('cmake','cmake/*/bin/cmake')
    ninja = find_tool('ninja','ninja/*/ninja')
    picotool = find_tool('picotool','picotool/*/picotool/picotool')
    build = ROOT/'build'
    print('Building Pico benchmark...',flush=True)
    command = [cmake,'-S',str(ROOT),'-B',str(build),'-G','Ninja',
               '-DCMAKE_BUILD_TYPE=Release','-DPICO_BOARD=pico2_w',f'-DCMAKE_MAKE_PROGRAM={ninja}']
    if not os.environ.get('PICO_SDK_PATH'):
        sdks = sorted((Path.home()/'.pico-sdk'/'sdk').glob('*/pico_sdk_init.cmake'))
        if sdks:
            command.append(f'-DPICO_SDK_PATH={sdks[-1].parent}')
    quiet_run(command)
    quiet_run([cmake,'--build',str(build),'--target','ultrasonic_benchmark','-j','4'])
    print('Flashing Pico benchmark...',flush=True)
    quiet_run([picotool,'load',str(build/'ultrasonic_benchmark.uf2'),'-fx'])

def find_port(requested, timeout=15):
    deadline = time.monotonic()+timeout
    while True:
        ports = [requested] if requested and Path(requested).exists() else []
        if not requested:
            ports = sorted(set(glob.glob('/dev/cu.usbmodem*')+glob.glob('/dev/ttyACM*')))
        if len(ports) == 1:
            return ports[0]
        if len(ports) > 1:
            raise RuntimeError('Multiple Pico serial ports found; select one with --port PORT')
        if time.monotonic() >= deadline:
            raise RuntimeError('Pico serial port not found; connect the board or use --port PORT')
        time.sleep(.1)

def capture(port, timeout):
    # Imported here so --host also works without POSIX serial support.
    import fcntl
    import struct
    import termios
    fd = os.open(port,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[1] = attrs[3] = 0
        attrs[2] = termios.CS8|termios.CREAD|termios.CLOCAL
        attrs[4] = attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd,termios.TCSANOW,attrs)
        termios.tcflush(fd,termios.TCIFLUSH)
        try:
            fcntl.ioctl(fd,termios.TIOCMBIS,struct.pack('I',termios.TIOCM_DTR))
        except (OSError,AttributeError):
            pass
        os.write(fd,b'r')
        deadline = time.monotonic()+timeout
        data = bytearray()
        while time.monotonic() < deadline:
            if not select.select([fd],[],[],min(1,max(0,deadline-time.monotonic())))[0]:
                continue
            chunk = os.read(fd,65536)
            if not chunk:
                raise RuntimeError('Pico disconnected before benchmark completed')
            data.extend(chunk)
            if len(data) > 2_000_000:
                raise RuntimeError('Unexpected serial output; is ultrasonic_benchmark flashed?')
            if re.search(rb'^# END [^\n]*\n',data,re.M):
                return data.decode(errors='replace')
        raise RuntimeError('Benchmark timed out; close other serial monitors and check the flashed program')
    finally:
        os.close(fd)

def format_summary(raw):
    lines = raw.splitlines()
    if lines.count('# BEGIN ultrasonic-benchmark-v2') != 1:
        raise ValueError('Missing or duplicate benchmark start marker')
    ends = [line for line in lines if line.startswith('# END ')]
    if ends != ['# END rows=144 failures=0']:
        raise ValueError('Incomplete or failed benchmark: '+(' / '.join(ends) or 'no end marker'))
    begin = lines.index('# BEGIN ultrasonic-benchmark-v2')
    end = lines.index(ends[0])
    if begin >= end:
        raise ValueError('Benchmark end precedes its start')
    lines = lines[begin+1:end]
    agreements = [line for line in lines if line.startswith('# AGREEMENT ')]
    if len(agreements) != 72 or any(not line.endswith('pass=1') for line in agreements):
        raise ValueError('Missing or failed FIR/FFT agreement checks')
    rows = list(csv.DictReader(io.StringIO('\n'.join(line for line in lines if line and not line.startswith('#')))))
    if len(rows) != 144:
        raise ValueError('Expected 144 measurement rows')
    scenarios = {'near_2cm','one_m','four_m','fft_boundary','weak_noise','no_echo'}
    seen = set()
    grouped = {}
    for row in rows:
        key = tuple(row[k] for k in ('method','sample_rate_hz','taps','signal','scenario'))
        if key in seen or row['method'] not in ('FIR','FFT1024') or row['scenario'] not in scenarios:
            raise ValueError('Duplicate or unexpected measurement')
        seen.add(key)
        if row['pass'] != '1':
            raise ValueError('A correctness check failed: '+str(key))
        detection = row['scenario'] != 'no_echo'
        if row['detected'] not in ('0','1') or (row['detected']=='1') != detection:
            raise ValueError('Unexpected detection result: '+str(key))
        if detection and int(row['expected_lag']) != int(row['measured_lag']):
            raise ValueError('Wrong simulated echo delay: '+str(key))
        timing = {k:float(row[k]) for k in ('mean_us','worst_us','capture_us','ping_slack_us')}
        if (not all(math.isfinite(x) for x in timing.values()) or timing['mean_us'] < 0 or
            timing['worst_us'] < timing['mean_us']-.001 or timing['capture_us'] <= 0):
            raise ValueError('Invalid timing measurement')
        if detection and (not math.isfinite(float(row['error_mm'])) or float(row['error_mm']) > .01):
            raise ValueError('Invalid simulated distance error')
        group = (int(row['sample_rate_hz']),int(row['taps']),row['signal'])
        if group[2] not in ('burst40k','chirp30to50k'):
            raise ValueError('Unexpected signal')
        grouped.setdefault(group,[]).append(row)
    if len(grouped) != 12:
        raise ValueError('Expected 12 benchmark configurations')
    output = ['Pico 2 W: 144/144 simulated checks passed; 72/72 FIR/FFT agreement checks passed.',
              'Worst processing time across six cases, three repetitions (milliseconds).',
              '',f'{"Rate":>7}  {"Signal":<13} {"Taps":>5} {"FIR ms":>9} {"FFT ms":>9}  {"Faster":<7} {"Fits 65 ms*":<12}']
    for (rate,taps,signal),selected in grouped.items():
        if len(selected) != 12:
            raise ValueError('Missing method/scenario combination')
        worst = {method:max(float(row['worst_us']) for row in selected if row['method']==method)/1000
                 for method in ('FIR','FFT1024')}
        fits = {method:min(float(row['ping_slack_us']) for row in selected if row['method']==method)>=0
                for method in ('FIR','FFT1024')}
        faster = 'FIR' if worst['FIR'] < worst['FFT1024'] else 'FFT' if worst['FIR'] > worst['FFT1024'] else 'tie'
        fit = '/'.join('yes' if fits[m] else 'no' for m in ('FIR','FFT1024'))
        label = '40k burst' if signal=='burst40k' else '30-50k chirp'
        output.append(f'{rate//1000:>4}kHz  {label:<13} {taps:>5} {worst["FIR"]:>9.3f} {worst["FFT1024"]:>9.3f}  {faster:<7} {fit:<12}')
    output += ['', '* FIR/FFT: complete 4 m capture plus processing within a provisional 65 ms cycle.',
               'These timings compare strongest-peak FIR and FFT, not the newer first-echo selector.',
               'Synthetic signals only; this does not establish real-sensor distance accuracy.']
    return '\n'.join(output)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host',action='store_true',help='run C correctness tests on this computer; no Pico needed')
    parser.add_argument('--port',help='Pico serial port; auto-detected when omitted')
    parser.add_argument('--no-flash',action='store_true',help='run the benchmark already flashed on the Pico')
    parser.add_argument('--timeout',type=float,default=90,help='serial run timeout in seconds')
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error('--timeout must be positive and finite')
    try:
        if args.host:
            run_host()
            return
        if not args.no_flash:
            flash_benchmark()
        port = find_port(args.port,timeout=2 if args.no_flash else 15)
        print(f'Running benchmark on {port}...',flush=True)
        print(format_summary(capture(port,args.timeout)))
    except (OSError,ValueError,KeyError,RuntimeError,subprocess.CalledProcessError) as error:
        raise SystemExit(f'Benchmark failed: {error}')
    except KeyboardInterrupt:
        raise SystemExit('Benchmark stopped.')

if __name__ == '__main__':
    main()
