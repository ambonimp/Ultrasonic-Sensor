"""Check settings from the C header; --fit saves only the best calibration there."""
import argparse
import csv
import ctypes as ct
import itertools
from pathlib import Path
import re
import shutil
import subprocess
import sys
import numpy as np

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent.parent
# Generated only under build/. Python calls the same production C selector.
BRIDGE = r'''#include "correlation.h"
static correlation_fir_context_t fir;
int run_calibration(const float *ref, unsigned length, const float *x, unsigned count,
                    unsigned min_lag, float rate, float min_rms,
                    float min_match, unsigned radius, float *result) {
    if (!correlation_fir_init(&fir, ref, length, rate, 0)) return 0;
    correlation_echo_options_t options = {min_lag, SIZE_MAX, radius, min_match, min_rms};
    correlation_result_t echo = correlation_fir_process_first_echo(&fir, x, count, &options);
    result[0] = echo.delay_samples;
    result[1] = echo.peak_strength;
    result[2] = echo.valid;
    return 1;
}
'''
KEYS = {
    'rate': 'AUSTINS_SAMPLE_RATE_HZ',
    'frequency': 'AUSTINS_REFERENCE_FREQUENCY_HZ',
    'length': 'AUSTINS_REFERENCE_SAMPLES',
    'rms': 'AUSTINS_MIN_WINDOW_RMS_V',
    'match': 'AUSTINS_MIN_NORMALIZED_MATCH',
    'radius': 'AUSTINS_PEAK_RADIUS_SAMPLES',
    'offset': 'AUSTINS_FIXED_DELAY_SECONDS',
    'speed': 'AUSTINS_SOUND_SPEED_M_S',
}

def build_library():
    """Keep generated native bridge/build artifacts out of the capture folder."""
    build = PROJECT/'build'/'host-tests'
    archive = build/'dsp'/'Source'/'libCMSISDSP.a'
    if not archive.exists():
        cmake = shutil.which('cmake')
        if not cmake:
            installed = sorted((Path.home()/'.pico-sdk'/'cmake').glob('*/bin/cmake'))
            cmake = str(installed[-1]) if installed else None
        if not cmake:
            raise RuntimeError('CMake not found; build host tests as described in tests/README.md')
        subprocess.run([cmake,'-S',str(PROJECT/'tests'),'-B',str(build),
                        '-DCMAKE_BUILD_TYPE=Release'],check=True,stdout=subprocess.DEVNULL)
        subprocess.run([cmake,'--build',str(build),'--target','CMSISDSP','-j','4'],
                       check=True,stdout=subprocess.DEVNULL)
    bridge = build/'calibration_bridge.c'
    if not bridge.exists() or bridge.read_text() != BRIDGE:
        bridge.write_text(BRIDGE)
    library = build/('calibration.dylib' if sys.platform == 'darwin' else 'calibration.so')
    dependencies = [PROJECT/'src'/'correlation.c',PROJECT/'include'/'correlation.h',
                    PROJECT/'include'/'ultrasonic_config.h',Path(__file__),bridge,archive]
    if not library.exists() or any(p.stat().st_mtime_ns > library.stat().st_mtime_ns for p in dependencies):
        compiler = shutil.which('cc')
        if not compiler:
            raise RuntimeError('C compiler cc not found')
        subprocess.run([compiler,'-shared','-fPIC','-O3','-D__GNUC_PYTHON__',
                        '-I'+str(PROJECT/'include'),
                        '-I'+str(PROJECT/'external'/'CMSIS-DSP'/'Include'),
                        str(PROJECT/'src'/'correlation.c'),str(bridge),str(archive),
                        '-o',str(library)],check=True)
    return library

def read_c_settings(path):
    """Read numeric #defines only; never evaluate C expressions as Python."""
    text = Path(path).read_text()
    values = {}
    for key, macro in KEYS.items():
        found = re.findall(r'^\s*#define\s+' + macro + r'\s+'
                           r'([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)'
                           r'[fFuUlL]*\s*(?://[^\n]*)?$', text, re.M)
        if len(found) != 1:
            raise ValueError(f'Expected one numeric definition of {macro} in {path}')
        values[key] = float(found[0])
    for key in ('rate', 'length', 'radius'):
        if values[key] != int(values[key]) or values[key] <= 0:
            raise ValueError(f'{KEYS[key]} must be a positive integer')
        values[key] = int(values[key])
    if (not all(np.isfinite(v) for v in values.values()) or
        values['rms'] <= 0 or not 0 < values['match'] <= 1 or
        values['speed'] <= 0 or values['offset'] < 0 or
        not 0 < values['frequency'] < values['rate']/2):
        raise ValueError('Invalid calibration settings')
    return values

def save_best(path, settings, original):
    """Replace the four fitted constants, preserving all other C settings."""
    if Path(path).read_text() != original:
        raise RuntimeError('C header changed while fitting; refusing to overwrite it')
    text = original
    for key in ('rms', 'match', 'radius', 'offset'):
        value = settings[key]
        literal = str(int(value)) + 'u' if key == 'radius' else f'{value:.9g}f'
        # A C floating literal needs a decimal point or exponent before 'f'.
        if key != 'radius' and '.' not in literal and 'e' not in literal.lower():
            literal = literal[:-1] + '.0f'
        text, count = re.subn(r'(^\s*#define\s+' + KEYS[key] + r'\s+)\S+',
                             lambda m: m[1] + literal, text, flags=re.M)
        if count != 1:
            raise ValueError(f'Missing or duplicate definition: {KEYS[key]}')
    Path(path).write_text(text)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--fit', action='store_true', help='fit trials 1/2 and save only the best C settings')
    mode.add_argument('--check', action='store_true', help='test saved C settings without changing them (default)')
    parser.add_argument('--header', type=Path, default=PROJECT/'include'/'ultrasonic_config.h')
    parser.add_argument('--captures', type=Path, default=HERE.parent)
    parser.add_argument('--library', type=Path, help='optional prebuilt native bridge; otherwise build automatically')
    parser.add_argument('--output', type=Path, default=HERE/'results.csv')
    args = parser.parse_args()
    settings = read_c_settings(args.header)
    original = args.header.read_text()
    library_path = args.library or build_library()
    library = ct.CDLL(str(library_path.resolve()))
    array = np.ctypeslib.ndpointer(dtype=np.float32, ndim=1, flags='C_CONTIGUOUS')
    library.run_calibration.argtypes = [array,ct.c_uint,array,ct.c_uint,ct.c_uint,
                                       ct.c_float,ct.c_float,ct.c_float,ct.c_uint,array]
    library.run_calibration.restype = ct.c_int
    captures = []
    for file in sorted(args.captures.glob('chirp_*cm_*.csv')):
        data = np.loadtxt(file, delimiter=',', skiprows=2)
        if (data.ndim != 2 or data.shape[1] != 3 or len(data) < settings['length'] or
            not np.all(np.isfinite(data)) or
            not np.allclose(np.diff(data[:,0]),1/settings['rate'],rtol=1e-5,atol=1e-12)):
            raise ValueError(f'Invalid data or sample rate: {file}')
        # CSV scope only: assume channel 2 marks TX, channel 3 is the receiver.
        rise = np.where(np.diff((data[:,1] > 1.65).astype(int)) == 1)[0] + 1
        if len(rise) != 1:
            raise ValueError(f'Expected one channel-2 rising edge: {file}')
        captures.append(dict(file=file.name, distance=int(file.stem.split('_')[1][:-2]),
                             trial=int(file.stem.split('_')[2]), rise=int(rise[0]),
                             x=(data[:,2]-data[:,2].mean()).astype(np.float32)))
    if not captures:
        raise ValueError('No chirp captures found')
    truth = np.array([c['distance'] for c in captures], dtype=float)
    train = np.array([i for i,c in enumerate(captures) if c['trial'] in (1,2)])
    check = np.array([i for i,c in enumerate(captures) if c['trial'] == 3])
    if not len(train) or not len(check):
        raise ValueError('Need fitting trials 1/2 and checking trial 3')
    ref = np.sin(2*np.pi*settings['frequency']*np.arange(settings['length'])/settings['rate']).astype(np.float32)

    def evaluate(params):
        delays = []
        for capture in captures:
            result = np.zeros(3, np.float32)
            ok = library.run_calibration(ref,len(ref),capture['x'],len(capture['x']),
                                         capture['rise'],settings['rate'],*params,result)
            if not ok:
                raise ValueError('C FIR initialization failed; check reference limits')
            delays.append((int(result[0])-capture['rise'])/settings['rate'] if result[2] else np.nan)
        return np.array(delays)

    scale = settings['speed']*50  # Seconds of round-trip time to centimeters.
    if args.fit:
        best = None
        candidates = itertools.chain([(settings['rms'],settings['match'],settings['radius'])],
                                     itertools.product((.04,.06,.08,.10,.12,.14,.16,.18),(.5,.6,.7,.8),(10,20)))
        for params in candidates:
            delays = evaluate(params)
            valid = train[np.isfinite(delays[train])]
            if not len(valid):
                continue
            offset = float(np.float32(np.median(delays[valid]-truth[valid]/scale)))
            if offset < 0:
                continue
            predicted = (delays-offset)*scale
            error = np.where(np.isfinite(predicted[train]) & (predicted[train]>=0),
                             np.abs(predicted[train]-truth[train]),100.)
            score = float(np.mean(error))
            if best is None or score < best[0]:
                best = (score,params,offset)
        if best is None:
            raise ValueError('No valid calibration found; C header was not changed')
        _, params, settings['offset'] = best
        settings['rms'],settings['match'],settings['radius'] = params
        save_best(args.header,settings,original)
        settings = read_c_settings(args.header)  # Check the values actually saved.
        print(f'Saved best settings to {args.header}')

    delays = evaluate((settings['rms'],settings['match'],settings['radius']))
    predicted = (delays-settings['offset'])*scale
    predicted[predicted < 0] = np.nan
    rows = []
    for i,capture in enumerate(captures):
        valid = bool(np.isfinite(predicted[i]))
        rows.append(dict(file=capture['file'],split='fit' if i in train else 'check',
                         distance_label_cm=capture['distance'],estimated_cm=float(predicted[i]) if valid else '',
                         absolute_error_cm=float(abs(predicted[i]-truth[i])) if valid else '',
                         selected_delay_ms=float(delays[i]*1000) if np.isfinite(delays[i]) else ''))
    with args.output.open('w') as handle:
        writer = csv.DictWriter(handle,fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    valid_check = check[np.isfinite(predicted[check])]
    if len(valid_check):
        error = np.abs(predicted[valid_check]-truth[valid_check])
        print(f'Check trial 3: {len(valid_check)}/{len(check)} detected; '
              f'mean error {np.mean(error):.2f} cm; worst error {np.max(error):.2f} cm')
    else:
        print(f'Check trial 3: 0/{len(check)} detected')
    print(f'Final per-file results: {args.output}')

if __name__ == '__main__':
    main()
