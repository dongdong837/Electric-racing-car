"""Execute real ARM C control/parser code in Unicorn; no MCU peripherals simulated."""
from pathlib import Path
import sys
from io import BytesIO
from build_gcc import ROOT, PROJECT, compiler_path, run
sys.path.insert(0, str(Path.home() / '.cache/25-contest-python'))
sys.path.insert(0, str(ROOT / 'tools/python-libs'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC
from elftools.elf.elffile import ELFFile

out = ROOT / 'tests/build'
out.mkdir(exist_ok=True)
log = []
elfpath = out / 'test_aim.elf'
run([compiler_path() / 'arm-none-eabi-gcc.exe', '-mcpu=cortex-m3', '-mthumb', '-Os', '-g',
     '-std=c99', '-Wall', '-Wextra', '-Werror', '-nostartfiles', '--specs=nosys.specs',
     '-I' + str(PROJECT / 'Core/Inc'), ROOT / 'tests/test_aim.c',
     PROJECT / 'Core/Src/aim_control.c', PROJECT / 'Core/Src/aim_protocol.c',
     '-Wl,-e,RunTests,-Ttext=0x08000000,-Tdata=0x20000000', '-o', elfpath], log)
elf = ELFFile(BytesIO(elfpath.read_bytes()))
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x08000000, 0x10000)
uc.mem_map(0x20000000, 0x10000)
for section in elf.iter_sections():
    if section['sh_flags'] & 2 and section['sh_type'] != 'SHT_NOBITS' and section['sh_size']:
        uc.mem_write(section['sh_addr'], section.data())
symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
uc.reg_write(UC_ARM_REG_SP, 0x2000F000)
uc.reg_write(UC_ARM_REG_LR, 0x0800F001)
uc.emu_start(symbols['RunTests'] | 1, 0x0800F000, count=20000000)
assert uc.reg_read(UC_ARM_REG_PC) == 0x0800F000, 'Instruction budget exhausted'
failure = uc.reg_read(UC_ARM_REG_R0)
checks = int.from_bytes(uc.mem_read(symbols['g_checks'], 4), 'little')
report = f'ARM C tests: {checks} assertions; failure line = {failure}\n'
print(report)
(out / 'result.txt').write_text(report, encoding='utf-8')
assert failure == 0, f'test_aim.c:{failure}'
