import re

# 1. Update tamagotchi_engine.h
with open('main/tamagotchi_engine.h', 'r', encoding='utf-8') as f:
    header = f.read()
if 'void SyncState' not in header:
    header = header.replace('void Update(float temperatura, float umidade, bool rfidLido, const uint8_t* rfidUID);',
                            'void Update(float temperatura, float umidade, bool rfidLido, const uint8_t* rfidUID);\n    void SyncState(uint8_t estadoNasc, uint8_t fome, uint8_t diversao, uint8_t saude);')
    with open('main/tamagotchi_engine.h', 'w', encoding='utf-8') as f:
        f.write(header)

# 2. Update tamagotchi_engine.cc
with open('main/tamagotchi_engine.cc', 'r', encoding='utf-8') as f:
    impl = f.read()
if 'void TamagotchiEngine::SyncState' not in impl:
    sync_code = '''
void TamagotchiEngine::SyncState(uint8_t estadoNasc, uint8_t fome, uint8_t diversao, uint8_t saude) {
    if (estadoNasc == 2 && estado_nascimento_ != ESTADO_NASCIDO) {
        estado_nascimento_ = ESTADO_NASCIDO;
    } else if (estadoNasc == 1) {
        estado_nascimento_ = ESTADO_CHOCANDO;
        ultima_leitura_rfid_ = esp_timer_get_time() / 1000000; // Impede o timeout
    } else if (estadoNasc == 0 && estado_nascimento_ != ESTADO_OVO && estado_nascimento_ != ESTADO_NASCIDO) {
        estado_nascimento_ = ESTADO_OVO;
    }
    fome_ = fome;
    diversao_ = diversao;
    saude_ = saude;
}
'''
    impl = impl + '\n' + sync_code
    with open('main/tamagotchi_engine.cc', 'w', encoding='utf-8') as f:
        f.write(impl)

# 3. Update esp_now_receiver.cc
with open('main/esp_now_receiver.cc', 'r', encoding='utf-8') as f:
    recv = f.read()
if 'engine.SyncState' not in recv:
    recv = recv.replace('engine.Update(state.temperatura, state.umidade, state.rfidLido, state.rfidUID);',
                        'engine.SyncState(state.estadoNascimento, state.fome, state.diversao, state.saude);\n            engine.Update(state.temperatura, state.umidade, state.rfidLido, state.rfidUID);')
    with open('main/esp_now_receiver.cc', 'w', encoding='utf-8') as f:
        f.write(recv)
