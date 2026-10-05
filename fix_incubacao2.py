import re

with open('main/tamagotchi_engine.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_func = '''    } else if (estado_nascimento_ == ESTADO_OVO) {
        if (rfidLido && rfidUID) {
            ESP_LOGI(TAG, "RFID LIDO NO ESTADO OVO! UID: %02X %02X %02X %02X", rfidUID[0], rfidUID[1], rfidUID[2], rfidUID[3]);
            // Se a tag do pet estiver zerada, aprende ela
            if (EUIDZerado(uid_pet_)) {
                CopiaUID(uid_pet_, rfidUID);
                ESP_LOGI(TAG, "Cartão do PET registrado na incubação!");
            }
            
            // Só choca se for o cartão do pet correto
            if (ComparaUID(rfidUID, uid_pet_)) {
                estado_nascimento_ = ESTADO_CHOCANDO;
                segundos_chocados_ = 0;
                SaveState();
                ESP_LOGI(TAG, "Ovo correto detectado! Iniciando incubação...");
            } else {
                ESP_LOGW(TAG, "Cartão Incorreto! Esperado: %02X %02X %02X %02X", uid_pet_[0], uid_pet_[1], uid_pet_[2], uid_pet_[3]);
            }
        }
    }'''

new_func = '''    } else if (estado_nascimento_ == ESTADO_OVO) {
        if (rfidLido && rfidUID) {
            ESP_LOGI(TAG, "RFID LIDO NO ESTADO OVO! UID: %02X %02X %02X %02X", rfidUID[0], rfidUID[1], rfidUID[2], rfidUID[3]);
            
            // Se ele é um ovo, qualquer cartão que o toque pela primeira vez VIRA o dono.
            // Isso previne bloqueios caso a memória flash (NVS) tenha lixo de testes antigos.
            CopiaUID(uid_pet_, rfidUID);
            ESP_LOGI(TAG, "Cartão do PET definido! O Ovo reconheceu seu dono!");
            
            estado_nascimento_ = ESTADO_CHOCANDO;
            segundos_chocados_ = 0;
            SaveState();
            ESP_LOGI(TAG, "Ovo correto detectado! Iniciando incubação...");
        }
    }'''

content = content.replace(old_func, new_func)

with open('main/tamagotchi_engine.cc', 'w', encoding='utf-8') as f:
    f.write(content)
