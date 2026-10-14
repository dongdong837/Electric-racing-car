"""Verify project references, vendor integrity and actual linked IRQ vectors."""
from pathlib import Path
from io import BytesIO
import hashlib
import json
import struct
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path.home() / '.cache/25-contest-python'))
sys.path.insert(0, str(ROOT / 'tools/python-libs'))
from elftools.elf.elffile import ELFFile

project = ROOT / 'OLED'
report = []
tree = ET.parse(project / 'MDK-ARM/OLED.uvprojx')
paths = [n.text.replace('\\', '/') for n in tree.findall('.//FilePath')]
assert all((project / 'MDK-ARM' / p).is_file() for p in paths)
assert len(paths) == len(set(paths))
assert not (project / 'Core/Inc/task.h').exists()
assert not any(p.endswith('/task.c') for p in paths)
assert len([p for p in paths if '/heap_' in p]) == 1
assert tree.findtext('.//uAC6') == '1'
report.append('PASS: Keil file references, unique sources, AC6 selection, task.h isolation and single heap implementation.')

kernel = project / 'Middlewares/Third_Party/FreeRTOS'
manifest = json.loads((kernel / 'SOURCE.json').read_text())
for name, expected in manifest['sha256'].items():
    assert hashlib.sha256((kernel / name).read_bytes()).hexdigest() == expected, name
report.append(f"PASS: {len(manifest['sha256'])} FreeRTOS V11.1.0 vendor files match recorded SHA-256.")

for name, irqs in [('dma.c', ['DMA1_Channel4_IRQn', 'DMA1_Channel5_IRQn']), ('usart.c', ['USART1_IRQn'])]:
    text = (project / 'Core/Src' / name).read_text()
    for irq in irqs:
        assert f'HAL_NVIC_SetPriority({irq}, 5, 0)' in text
text = (project / 'Core/Src/main.c').read_text()
assert 'MX_TIM4_Init();' not in text and 'HAL_TIM_Base_Start_IT' not in text
assert 'App_Start();' in text
irq_text = (project / 'Core/Src/stm32f1xx_it.c').read_text()
assert 'void SVC_Handler(void)' not in irq_text and 'void PendSV_Handler(void)' not in irq_text
assert 'HAL_IncTick();' in irq_text and 'xPortSysTickHandler();' in irq_text
assert 'HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4)' in (project / 'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c').read_text()
report.append('PASS: RTOS-compatible IRQ priorities, HAL priority grouping, shared SysTick and retired TIM4 polling.')

elf = ELFFile(BytesIO((project / 'GCC/build/OLED-FreeRTOS.elf').read_bytes()))
symbols = {s.name: s for s in elf.get_section_by_name('.symtab').iter_symbols()}
vectors = elf.get_section_by_name('.isr_vector').data()
assert struct.unpack_from('<I', vectors, 0)[0] == 0x20005000
for index, name in [(11, 'SVC_Handler'), (14, 'PendSV_Handler'), (15, 'SysTick_Handler'),
                    (30, 'DMA1_Channel4_IRQHandler'), (31, 'DMA1_Channel5_IRQHandler'),
                    (53, 'USART1_IRQHandler')]:
    address = struct.unpack_from('<I', vectors, index * 4)[0]
    assert address == symbols[name]['st_value'], (name, hex(address))
    assert symbols[name]['st_info']['bind'] == 'STB_GLOBAL', name
    report.append(f'PASS: vector {index} -> {name} at 0x{address:08x}, strong definition.')
assert symbols['ucHeap']['st_size'] == 10 * 1024
assert symbols['__heap_limit']['st_value'] <= 0x20005000 - 1024
report.append('PASS: 10 KiB RTOS heap, 64 KiB/20 KiB link limits and at least 1 KiB reserved MSP headroom.')
log = (project / 'GCC/build/build.log').read_text(encoding='utf-8')
assert 'warning:' not in log.lower() and 'error:' not in log.lower()
report.append('PASS: latest full ARM build log has no warnings or errors.')

result = '\n'.join(report) + '\n'
(ROOT / 'tests/build').mkdir(exist_ok=True)
(ROOT / 'tests/build/project_checks.txt').write_text(result, encoding='utf-8')
print(result)
