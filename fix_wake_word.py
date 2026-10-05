import os
import re

def update_sdkconfig_defaults(file_path):
    if not os.path.exists(file_path):
        return
        
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()
        
    # We want to make sure custom wake word is selected
    if 'CONFIG_USE_CUSTOM_WAKE_WORD=y' not in content:
        content += "\nCONFIG_USE_CUSTOM_WAKE_WORD=y\n"
        
    # Remove older wake word methods just in case they conflict
    content = re.sub(r"CONFIG_USE_AFE_WAKE_WORD=y\n?", "", content)
    content = re.sub(r"CONFIG_USE_ESP_WAKE_WORD=y\n?", "", content)
    
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(content)

update_sdkconfig_defaults("d:\\Documentos\\Projetos\\Robo\\xiaozhi-esp32-pimentz\\sdkconfig.defaults")
update_sdkconfig_defaults("d:\\Documentos\\Projetos\\Robo\\xiaozhi-esp32-pimentz\\sdkconfig.defaults.esp32s3")
