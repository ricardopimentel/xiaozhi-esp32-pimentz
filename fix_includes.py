import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

# Extract the wrappers block
wrappers_match = re.search(r'#include <esp_heap_caps\.h>.*?#endif\s*', content, re.DOTALL)
if wrappers_match:
    wrappers_code = wrappers_match.group(0)
    # Remove from top
    content = content.replace(wrappers_code, '')
    
    # Insert after #include <lvgl.h> (or #include "lcd_display.h")
    # Let's find the end of the includes
    parts = content.split('#include "lcd_display.h"')
    if len(parts) > 1:
        content = parts[0] + '#include "lcd_display.h"\n#include <lvgl.h>\n' + wrappers_code + parts[1]

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)
