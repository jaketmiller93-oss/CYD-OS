"""Enable OTA rollback in an existing sdkconfig without changing credentials."""
from pathlib import Path
FLAGS = {
    'CONFIG_LV_FONT_MONTSERRAT_20': 'y',
    'CONFIG_LV_FONT_MONTSERRAT_28': 'y',
    'CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE': 'y',
    'CONFIG_BOOTLOADER_LOG_LEVEL_WARN': 'y',
    'CONFIG_BOOTLOADER_LOG_LEVEL_INFO': 'n',
    'CONFIG_BOOTLOADER_LOG_LEVEL_DEBUG': 'n',
    'CONFIG_BOOTLOADER_LOG_LEVEL_VERBOSE': 'n',
    'CONFIG_BOOTLOADER_LOG_LEVEL_ERROR': 'n',
    'CONFIG_BOOTLOADER_LOG_LEVEL_NONE': 'n',
}

def configured_text(text):
    if 'CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=y' in text:
        raise ValueError('eFuse anti-rollback configurations are not supported by this updater')
    lines = []
    found = set()
    for line in text.splitlines():
        key = line.split('=',1)[0] if '=' in line else line[2:-11] if line.startswith('# ') and line.endswith(' is not set') else ''
        if key in FLAGS:
            if key not in found:
                lines.append(key+'=y' if FLAGS[key]=='y' else '# '+key+' is not set')
                found.add(key)
        else:
            lines.append(line)
    for key,value in FLAGS.items():
        if key not in found:
            lines.append(key+'=y' if value=='y' else '# '+key+' is not set')
    return '\n'.join(lines)+'\n'

def enable(path):
    path = Path(path)
    path.write_text(configured_text(path.read_text()))
