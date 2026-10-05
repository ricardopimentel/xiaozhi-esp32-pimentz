import re

# 1. Update tamagotchi_engine.h
with open('main/tamagotchi_engine.h', 'r', encoding='utf-8') as f:
    header = f.read()

# Remove SyncState, change Update signature, add SetSensorData and member vars
header = header.replace('void Update(float temperatura, float umidade, bool rfidLido, const uint8_t* rfidUID);', 'void Update();')
header = header.replace('void SyncState(uint8_t estadoNasc, uint8_t fome, uint8_t diversao, uint8_t saude);', 'void SetSensorData(float temperatura, float umidade, bool rfidLido, const uint8_t* rfidUID);')

if 'float sensor_temperatura_' not in header:
    header = header.replace('// UIDs de RFID para interação', '''// Sensor cache
    float sensor_temperatura_ = 25.0f;
    float sensor_umidade_ = 50.0f;
    bool sensor_rfid_lido_ = false;
    uint8_t sensor_rfid_uid_[4] = {0};

    // UIDs de RFID para interacao''')

with open('main/tamagotchi_engine.h', 'w', encoding='utf-8') as f:
    f.write(header)

# 2. Update tamagotchi_engine.cc
with open('main/tamagotchi_engine.cc', 'r', encoding='utf-8') as f:
    impl = f.read()

# Replace Update signature
impl = impl.replace('void TamagotchiEngine::Update(float temperatura, float umidade, bool rfidLido, const uint8_t* rfidUID)', 'void TamagotchiEngine::Update()')
# Inside Update, we need to pass the member vars to ProcessarCicloIncubacao
impl = impl.replace('ProcessarCicloIncubacao(rfidLido, rfidUID);', 'ProcessarCicloIncubacao(sensor_rfid_lido_, sensor_rfid_uid_);')
impl = impl.replace('if (rfidLido && rfidUID)', 'if (sensor_rfid_lido_ && sensor_rfid_uid_)')
impl = impl.replace('ComparaUID(rfidUID,', 'ComparaUID(sensor_rfid_uid_,')
impl = impl.replace('CopiaUID(uid_comida_, rfidUID);', 'CopiaUID(uid_comida_, sensor_rfid_uid_);')
impl = impl.replace('CopiaUID(uid_brincar_, rfidUID);', 'CopiaUID(uid_brincar_, sensor_rfid_uid_);')
impl = impl.replace('CopiaUID(uid_saude_, rfidUID);', 'CopiaUID(uid_saude_, sensor_rfid_uid_);')
impl = impl.replace('CopiaUID(uid_pet_, rfidUID);', 'CopiaUID(uid_pet_, sensor_rfid_uid_);')

# Remove SyncState
impl = re.sub(r'void TamagotchiEngine::SyncState.*?}\n', '', impl, flags=re.DOTALL)

# Add SetSensorData
if 'void TamagotchiEngine::SetSensorData' not in impl:
    sensor_code = '''
void TamagotchiEngine::SetSensorData(float temperatura, float umidade, bool rfidLido, const uint8_t* rfidUID) {
    sensor_temperatura_ = temperatura;
    sensor_umidade_ = umidade;
    sensor_rfid_lido_ = rfidLido;
    if (rfidUID) {
        memcpy(sensor_rfid_uid_, rfidUID, 4);
    } else {
        memset(sensor_rfid_uid_, 0, 4);
    }
}
'''
    impl = impl + '\n' + sensor_code

with open('main/tamagotchi_engine.cc', 'w', encoding='utf-8') as f:
    f.write(impl)

# 3. Update application.cc
with open('main/application.cc', 'r', encoding='utf-8') as f:
    app = f.read()

app = app.replace('engine.Update(0.0f, 0.0f, false, nullptr);', 'engine.Update();')

with open('main/application.cc', 'w', encoding='utf-8') as f:
    f.write(app)

# 4. Update esp_now_receiver.cc
with open('main/esp_now_receiver.cc', 'r', encoding='utf-8') as f:
    recv = f.read()

recv = re.sub(r'engine\.SyncState\(.*?\);\n\s*engine\.Update\(.*?\);', 'engine.SetSensorData(state.temperatura, state.umidade, state.rfidLido, state.rfidUID);\n            engine.Update();', recv)

with open('main/esp_now_receiver.cc', 'w', encoding='utf-8') as f:
    f.write(recv)
