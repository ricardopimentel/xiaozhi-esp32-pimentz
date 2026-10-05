import re

with open('main/tamagotchi_engine.cc', 'r', encoding='utf-8') as f:
    content = f.read()

# Substituir ProcessarCicloIncubacao para adicionar logs detalhados
old_func = '''void TamagotchiEngine::ProcessarCicloIncubacao(bool rfidLido, const uint8_t* rfidUID) {
    uint64_t now = esp_timer_get_time() / 1000;
    
    if (estado_nascimento_ == ESTADO_CHOCANDO) {
        static uint64_t last_chocando_tick = 0;
        uint64_t diff = now - last_chocando_tick;
        if (diff >= 1000) {
            segundos_chocados_++;
            last_chocando_tick = now;
            ESP_LOGI(TAG, "Incubação: %d segundos chocados", segundos_chocados_);
            if (segundos_chocados_ % 10 == 0) {
                SaveState();
            }
            if (segundos_chocados_ >= tempoIncubacaoSegundos) {
                NascerPet();
            }
        }
    } else if (estado_nascimento_ == ESTADO_OVO && rfidLido && rfidUID) {
        // Se a tag do pet estiver zerada, aprende ela
        if (EUIDZerado(uid_pet_)) {
            CopiaUID(uid_pet_, sensor_rfid_uid_);
            ESP_LOGI(TAG, "Cartão do PET registrado na incubação!");
        }
        
        // Só choca se for o cartão do pet correto
        if (ComparaUID(sensor_rfid_uid_, uid_pet_)) {
            estado_nascimento_ = ESTADO_CHOCANDO;
            segundos_chocados_ = 0;
            SaveState();
            ESP_LOGI(TAG, "Ovo correto detectado! Iniciando incubação...");
        }
    }
}'''

new_func = '''void TamagotchiEngine::ProcessarCicloIncubacao(bool rfidLido, const uint8_t* rfidUID) {
    uint64_t now = esp_timer_get_time() / 1000;
    
    if (estado_nascimento_ == ESTADO_CHOCANDO) {
        static uint64_t last_chocando_tick = 0;
        uint64_t diff = now - last_chocando_tick;
        if (diff >= 1000) {
            segundos_chocados_++;
            last_chocando_tick = now;
            ESP_LOGI(TAG, "Incubação: %d segundos chocados", segundos_chocados_);
            if (segundos_chocados_ % 10 == 0) {
                SaveState();
            }
            if (segundos_chocados_ >= tempoIncubacaoSegundos) {
                NascerPet();
            }
        }
    } else if (estado_nascimento_ == ESTADO_OVO) {
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
    }
}'''

content = content.replace(old_func, new_func)

with open('main/tamagotchi_engine.cc', 'w', encoding='utf-8') as f:
    f.write(content)
