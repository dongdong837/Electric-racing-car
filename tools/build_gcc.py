"""Compile the actual Keil source list using Arm GNU; no source mocks."""
from pathlib import Path
import argparse
import os
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'OLED'

def compiler_path():
    override = os.environ.get('ARM_GCC_BIN')
    if override:
        return Path(override)
    cache = Path.home() / '.cache/25-contest-toolchain/bin'
    if (cache / 'arm-none-eabi-gcc.exe').exists():
        return cache
    paths = list((ROOT / 'tools/toolchain').glob('*/bin/arm-none-eabi-gcc.exe'))
    if not paths:
        raise SystemExit('Set ARM_GCC_BIN to the directory containing arm-none-eabi-gcc.exe')
    return paths[0].parent

def run(command, log):
    # MinGW ld cannot reliably handle Unicode absolute paths; use project-relative paths.
    command = [str(x).replace(str(ROOT), '..') for x in command]
    result = subprocess.run(command, cwd=PROJECT, capture_output=True, text=True, errors='replace')
    log.append(' '.join(str(x) for x in command) + '\n' + result.stdout + result.stderr)
    if result.returncode:
        print(result.stdout + result.stderr)
        raise RuntimeError('Command failed')
    return result.stdout + result.stderr

def build():
    bindir = compiler_path()
    gcc = bindir / 'arm-none-eabi-gcc.exe'
    out = PROJECT / 'GCC/build'
    out.mkdir(parents=True, exist_ok=True)
    tree = ET.parse(PROJECT / 'MDK-ARM/OLED.uvprojx')
    target = tree.find('./Targets/Target')
    options = target.find('./TargetOption/TargetArmAds/Cads/VariousControls')
    includes = [(PROJECT / 'MDK-ARM' / p).resolve() for p in options.findtext('IncludePath').split(';')]
    defines = options.findtext('Define').replace(',', ' ').split()
    flags = ['-mcpu=cortex-m3', '-mthumb', '-mfloat-abi=soft', '-Os', '-g3', '-std=c99',
             '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Wno-unused-parameter']
    flags += ['-D' + d for d in defines] + ['-I' + str(p) for p in includes]
    sources = [(PROJECT / 'MDK-ARM' / n.text.replace('\\', '/')).resolve()
               for n in target.findall('./Groups/Group/Files/File/FilePath') if n.text.endswith('.c')]
    sources += [PROJECT / 'GCC/runtime.c', PROJECT / 'Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s']
    log, objects = [], []
    try:
        for i, source in enumerate(sources):
            obj = out / f'{i:02d}_{source.stem}.o'
            run([gcc, *flags, '-c', source, '-o', obj], log)
            objects.append(obj)
        elf = out / 'OLED-FreeRTOS.elf'
        run([gcc, '-mcpu=cortex-m3', '-mthumb', '-mfloat-abi=soft', '-nostartfiles',
             '--specs=nano.specs', '--specs=nosys.specs', *objects, '-T', PROJECT / 'GCC/STM32F103C8.ld',
             '-Wl,--gc-sections,--print-memory-usage,-Map=' + str(out / 'OLED-FreeRTOS.map'),
             '-o', elf], log)
        run([bindir / 'arm-none-eabi-objcopy.exe', '-O', 'ihex', elf, out / 'OLED-FreeRTOS.hex'], log)
        print(run([bindir / 'arm-none-eabi-size.exe', elf], log))
        print('ARM compile and link passed:', elf)
    finally:
        (out / 'build.log').write_text('\n'.join(log), encoding='utf-8')

if __name__ == '__main__':
    build()
