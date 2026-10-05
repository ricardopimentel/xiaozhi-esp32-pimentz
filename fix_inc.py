import re

with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_cb = '''#include <math.h>
#include "lcd_display.h"'''

new_cb = '''#include <math.h>
#include "lcd_display.h"
#include "application.h"'''

content = content.replace(old_cb, new_cb)

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)
