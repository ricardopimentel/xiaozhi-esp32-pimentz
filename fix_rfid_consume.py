import re

with open('main/tamagotchi_engine.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_rfid = '''    // 1.5. Processa interações por RFID se nasceu
    if (sensor_rfid_lido_) {
        if (!EUIDZerado(uid_comida_) && ComparaUID(sensor_rfid_uid_, uid_comida_)) {'''

new_rfid = '''    // 1.5. Processa interações por RFID se nasceu
    if (sensor_rfid_lido_) {
        bool consumido = true;
        if (!EUIDZerado(uid_comida_) && ComparaUID(sensor_rfid_uid_, uid_comida_)) {'''

old_rfid_end = '''                if (!ComparaUID(sensor_rfid_uid_, uid_comida_) && !ComparaUID(sensor_rfid_uid_, uid_brincar_) && !ComparaUID(sensor_rfid_uid_, uid_saude_)) {
                    CopiaUID(uid_pet_, sensor_rfid_uid_);
                    ESP_LOGI(TAG, "Cartão registrado para PET!");
                    Pet();
                }
            }
        }
    }'''

new_rfid_end = '''                if (!ComparaUID(sensor_rfid_uid_, uid_comida_) && !ComparaUID(sensor_rfid_uid_, uid_brincar_) && !ComparaUID(sensor_rfid_uid_, uid_saude_)) {
                    CopiaUID(uid_pet_, sensor_rfid_uid_);
                    ESP_LOGI(TAG, "Cartão registrado para PET!");
                    Pet();
                } else {
                    consumido = false;
                }
            }
        }
        if (consumido) sensor_rfid_lido_ = false; // Consome a leitura para evitar processamento duplo
    }'''

content = content.replace(old_rfid, new_rfid).replace(old_rfid_end, new_rfid_end)

# Também na incubação!
old_inc = '''            estado_nascimento_ = ESTADO_CHOCANDO;
            segundos_chocados_ = 0;
            SaveState();
            ESP_LOGI(TAG, "Ovo correto detectado! Iniciando incubação...");
        }
    }
}'''

new_inc = '''            estado_nascimento_ = ESTADO_CHOCANDO;
            segundos_chocados_ = 0;
            SaveState();
            ESP_LOGI(TAG, "Ovo correto detectado! Iniciando incubação...");
        }
        // Consome para garantir que o pulso não afete mais nada
        sensor_rfid_lido_ = false;
    }
}'''

content = content.replace(old_inc, new_inc)

with open('main/tamagotchi_engine.cc', 'w', encoding='utf-8') as f:
    f.write(content)
