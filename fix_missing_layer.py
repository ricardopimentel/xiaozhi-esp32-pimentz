with open('main/display/lcd_display.cc', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('DrawLargeHeart(eyeLx + xOffset + tremorX, eyeLy + tremorY, (ms/400)%2);', 'DrawLargeHeart(eyeLx + xOffset + tremorX, eyeLy + tremorY, (ms/400)%2, layer);')
content = content.replace('DrawLargeHeart(eyeRx + xOffset + tremorX, eyeRy + tremorY, (ms/400)%2);', 'DrawLargeHeart(eyeRx + xOffset + tremorX, eyeRy + tremorY, (ms/400)%2, layer);')

with open('main/display/lcd_display.cc', 'w', encoding='utf-8') as f:
    f.write(content)
