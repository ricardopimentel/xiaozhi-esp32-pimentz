#include "tamagotchi_engine.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <algorithm>

#define TAG "TamagotchiEngine"

// Constantes de tempo em milissegundos
#define INTERVALO_FOME 60000
#define INTERVALO_BRINCAR 90000
#define INTERVALO_SAUDE 120000
#define INTERVALO_ALERTA 15000
#define INTERVALO_ALERTA_MUITO_BAIXO 5000

static const uint16_t tempoIncubacaoSegundos = 15; // 15 segundos para fins de teste/nascimento rápido

TamagotchiEngine& TamagotchiEngine::GetInstance() {
    static TamagotchiEngine instance;
    return instance;
}

TamagotchiEngine::TamagotchiEngine() {
    last_tick_time_ = esp_timer_get_time() / 1000;
    last_save_time_ = last_tick_time_;
    last_vinculo_check_ = last_tick_time_;
}

void TamagotchiEngine::Initialize() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("tamagotchi", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        uint8_t fome = 100, diversao = 100, saude = 100, estNasc = 0, persNat = 0, vinculo = 0;
        uint8_t doente = 0;
        uint16_t segChoc = 0;
        uint32_t segVida = 0;

        if (nvs_get_u8(handle, "fome", &fome) == ESP_OK) fome_ = fome;
        if (nvs_get_u8(handle, "diversao", &diversao) == ESP_OK) diversao_ = diversao;
        if (nvs_get_u8(handle, "saude", &saude) == ESP_OK) saude_ = saude;
        if (nvs_get_u8(handle, "estaDoente", &doente) == ESP_OK) esta_doente_ = (doente != 0);
        if (nvs_get_u8(handle, "estNasc", &estNasc) == ESP_OK) estado_nascimento_ = static_cast<EstadoNascimento>(estNasc);
        if (nvs_get_u16(handle, "segChoc", &segChoc) == ESP_OK) segundos_chocados_ = segChoc;
        if (nvs_get_u8(handle, "personalidade", &persNat) == ESP_OK) personalidade_natural_ = static_cast<Personalidade>(persNat);
        if (nvs_get_u8(handle, "vinculo", &vinculo) == ESP_OK) pontos_de_vinculo_ = vinculo;
        if (nvs_get_u32(handle, "segVida", &segVida) == ESP_OK) segundos_de_vida_ = segVida;

        // Carrega os cartões RFID auto-aprendidos
        size_t len = 4;
        nvs_get_blob(handle, "uidComida", uid_comida_, &len);
        len = 4;
        nvs_get_blob(handle, "uidBrincar", uid_brincar_, &len);
        len = 4;
        nvs_get_blob(handle, "uidSaude", uid_saude_, &len);
        len = 4;
        nvs_get_blob(handle, "uidPet", uid_pet_, &len);

        nvs_close(handle);
        
        // Inicializa a personalidade baseada nos pontos salvos
        AtualizarVinculo(0);
        
        ESP_LOGI(TAG, "Estado carregado com sucesso: Nascimento=%d, Fome=%d, Diversao=%d, Saude=%d, Vinculo=%d, Doente=%d", 
                 estado_nascimento_, fome_, diversao_, saude_, pontos_de_vinculo_, esta_doente_);
    } else {
        ESP_LOGW(TAG, "NVS não inicializado ou vazio, usando padrões de fábrica");
        SaveState();
    }
}

void TamagotchiEngine::SaveState() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("tamagotchi", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "fome", fome_);
        nvs_set_u8(handle, "diversao", diversao_);
        nvs_set_u8(handle, "saude", saude_);
        nvs_set_u8(handle, "estaDoente", esta_doente_ ? 1 : 0);
        nvs_set_u8(handle, "estNasc", static_cast<uint8_t>(estado_nascimento_));
        nvs_set_u16(handle, "segChoc", segundos_chocados_);
        nvs_set_u8(handle, "personalidade", static_cast<uint8_t>(personalidade_natural_));
        nvs_set_u8(handle, "vinculo", pontos_de_vinculo_);
        nvs_set_u32(handle, "segVida", segundos_de_vida_);
        
        // Salva os cartões RFID auto-aprendidos
        nvs_set_blob(handle, "uidComida", uid_comida_, 4);
        nvs_set_blob(handle, "uidBrincar", uid_brincar_, 4);
        nvs_set_blob(handle, "uidSaude", uid_saude_, 4);
        nvs_set_blob(handle, "uidPet", uid_pet_, 4);
        
        nvs_commit(handle);
        nvs_close(handle);
        
        last_save_time_ = esp_timer_get_time() / 1000;
        ESP_LOGD(TAG, "Estado salvo com sucesso no NVS");
    }
}

void TamagotchiEngine::Update() {
    uint64_t now = esp_timer_get_time() / 1000;
    
    // 1. Processa ciclo de incubação se for ovo ou chocando
    if (estado_nascimento_ == ESTADO_OVO || estado_nascimento_ == ESTADO_CHOCANDO) {
        ProcessarCicloIncubacao(sensor_rfid_lido_, sensor_rfid_uid_);
        return;
    }

    // 1.5. Processa interações por RFID se nasceu
    if (sensor_rfid_lido_ && sensor_rfid_uid_) {
        if (!EUIDZerado(uid_comida_) && ComparaUID(sensor_rfid_uid_, uid_comida_)) {
            Feed();
        } else if (!EUIDZerado(uid_brincar_) && ComparaUID(sensor_rfid_uid_, uid_brincar_)) {
            Play();
        } else if (!EUIDZerado(uid_saude_) && ComparaUID(sensor_rfid_uid_, uid_saude_)) {
            Heal();
        } else if (!EUIDZerado(uid_pet_) && ComparaUID(sensor_rfid_uid_, uid_pet_)) {
            Pet();
        } else {
            // Auto-aprendizado na ordem
            if (EUIDZerado(uid_comida_)) {
                CopiaUID(uid_comida_, sensor_rfid_uid_);
                ESP_LOGI(TAG, "Cartão registrado para COMIDA!");
                Feed();
            } else if (EUIDZerado(uid_brincar_)) {
                if (!ComparaUID(sensor_rfid_uid_, uid_comida_)) {
                    CopiaUID(uid_brincar_, sensor_rfid_uid_);
                    ESP_LOGI(TAG, "Cartão registrado para BRINCAR!");
                    Play();
                }
            } else if (EUIDZerado(uid_saude_)) {
                if (!ComparaUID(sensor_rfid_uid_, uid_comida_) && !ComparaUID(sensor_rfid_uid_, uid_brincar_)) {
                    CopiaUID(uid_saude_, sensor_rfid_uid_);
                    ESP_LOGI(TAG, "Cartão registrado para SAUDE!");
                    Heal();
                }
            } else if (EUIDZerado(uid_pet_)) {
                if (!ComparaUID(sensor_rfid_uid_, uid_comida_) && !ComparaUID(sensor_rfid_uid_, uid_brincar_) && !ComparaUID(sensor_rfid_uid_, uid_saude_)) {
                    CopiaUID(uid_pet_, sensor_rfid_uid_);
                    ESP_LOGI(TAG, "Cartão registrado para PET!");
                    Pet();
                }
            }
        }
    }

    // 2. Ticks de vida do Robô Nascido
    uint64_t elapsed = now - last_tick_time_;
    if (elapsed >= 1000) {
        segundos_de_vida_ += (elapsed / 1000);
        idade_dias_ = segundos_de_vida_ / 86400;
        last_tick_time_ = now;
    }

    // 3. Decaimento de Atributos dependendo da Personalidade
    // Fome decai periodicamente
    uint32_t intervalo_fome = (personalidade_natural_ == PERSONALIDADE_SENSIVEL) ? (INTERVALO_FOME * 2) / 3 : INTERVALO_FOME;
    static uint64_t last_fome_tick = 0;
    if (now - last_fome_tick >= intervalo_fome) {
        fome_ = std::max(0, fome_ - 1);
        last_fome_tick = now;
        SaveState();
    }

    // Diversão decai periodicamente
    uint32_t intervalo_brincar = (personalidade_natural_ == PERSONALIDADE_SENSIVEL) ? (INTERVALO_BRINCAR * 2) / 3 : 
                                 (personalidade_natural_ == PERSONALIDADE_SARCASTICA) ? (INTERVALO_BRINCAR * 5) / 3 : INTERVALO_BRINCAR;
    static uint64_t last_brincar_tick = 0;
    if (now - last_brincar_tick >= intervalo_brincar) {
        diversao_ = std::max(0, diversao_ - 1);
        last_brincar_tick = now;
        SaveState();
    }

    // Saúde decai se estiver faminto e entediado, ou se estiver doente
    uint32_t intervalo_saude = (personalidade_natural_ == PERSONALIDADE_SENSIVEL) ? INTERVALO_SAUDE / 2 : INTERVALO_SAUDE;
    static uint64_t last_saude_tick = 0;
    if (now - last_saude_tick >= intervalo_saude) {
        uint8_t limiar = (personalidade_natural_ == PERSONALIDADE_SENSIVEL) ? 50 : 30;
        if ((fome_ <= limiar && diversao_ <= limiar) || esta_doente_) {
            saude_ = std::max(0, saude_ - 1);
            SaveState();
        }
        last_saude_tick = now;
    }

    // 4. Lógica de Doença por Frio
    if (temperatura < 18.0f && temperatura > 0.0f && !esta_doente_) {
        if (tempo_no_frio_ == 0) {
            tempo_no_frio_ = now;
        } else if (now - tempo_no_frio_ >= 60000) { // 1 minuto no frio
            tempo_no_frio_ = now;
            // 5% de chance de adoecer por minuto no frio
            if ((rand() % 100) < 5) {
                esta_doente_ = true;
                SaveState();
                ESP_LOGW(TAG, "O Robô ficou doente devido ao frio!");
            }
        }
    } else {
        tempo_no_frio_ = 0;
    }

    // 5. Atualização periódica do Vínculo
    if (now - last_vinculo_check_ >= 10000) {
        last_vinculo_check_ = now;
        if (fome_ >= 70 && diversao_ >= 70 && saude_ >= 70 && !esta_doente_) {
            AtualizarVinculo(1); // Ganha afeto
        } else if (fome_ < 30 || diversao_ < 30 || saude_ < 30 || esta_doente_) {
            AtualizarVinculo(-1); // Perde afeto
        }
    }

    // 6. Atualiza personalidade atual (dinâmica com base no humor)
    uint8_t limiarTriste = (personalidade_natural_ == PERSONALIDADE_SENSIVEL) ? 50 : 30;
    if (fome_ <= limiarTriste || diversao_ <= limiarTriste) {
        personalidade_ = PERSONALIDADE_SARCASTICA; // Fica irritado/sarcástico sob negligência
    } else {
        personalidade_ = personalidade_natural_;
    }
}

void TamagotchiEngine::ProcessarCicloIncubacao(bool rfidLido, const uint8_t* rfidUID) {
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
}

void TamagotchiEngine::NascerPet() {
    estado_nascimento_ = ESTADO_NASCIDO;
    personalidade_ = PERSONALIDADE_SARCASTICA;
    personalidade_natural_ = PERSONALIDADE_SARCASTICA;
    pontos_de_vinculo_ = 0;
    
    fome_ = 80;
    diversao_ = 75;
    saude_ = 90;
    esta_doente_ = false;
    segundos_de_vida_ = 0;
    idade_dias_ = 0;
    
    SaveState();
    ESP_LOGI(TAG, "Parabéns! O Robô nasceu com a personalidade SARCASTICA.");
}

void TamagotchiEngine::AtualizarVinculo(int8_t pontos) {
    int antigo = pontos_de_vinculo_;
    pontos_de_vinculo_ = std::max(0, std::min(120, pontos_de_vinculo_ + pontos));
    
    if (pontos_de_vinculo_ < 40) {
        personalidade_natural_ = PERSONALIDADE_SARCASTICA;
    } else if (pontos_de_vinculo_ < 80) {
        personalidade_natural_ = PERSONALIDADE_BASICA;
    } else {
        personalidade_natural_ = PERSONALIDADE_SENSIVEL;
    }
    
    if (pontos_de_vinculo_ != antigo) {
        SaveState();
    }
}

void TamagotchiEngine::Feed() {
    if (estado_nascimento_ != ESTADO_NASCIDO) return;
    fome_ = std::min(100, fome_ + 20);
    AtualizarVinculo(2);
    SaveState();
}

void TamagotchiEngine::Play() {
    if (estado_nascimento_ != ESTADO_NASCIDO) return;
    diversao_ = std::min(100, diversao_ + 15);
    AtualizarVinculo(2);
    SaveState();
}

void TamagotchiEngine::Heal() {
    if (estado_nascimento_ != ESTADO_NASCIDO) return;
    if (esta_doente_) {
        esta_doente_ = false;
        saude_ = std::min(100, saude_ + 30);
        AtualizarVinculo(5);
    } else {
        saude_ = std::min(100, saude_ + 10);
    }
    SaveState();
}

void TamagotchiEngine::Pet() {
    if (estado_nascimento_ != ESTADO_NASCIDO) return;
    AtualizarVinculo(3);
    SaveState();
}

std::string TamagotchiEngine::GetCurrentEmotion(float temperatura, bool choque, bool obstaculo) const {
    if (estado_nascimento_ == ESTADO_OVO || estado_nascimento_ == ESTADO_CHOCANDO) {
        return "neutral";
    }
    
    if (choque) {
        return "confused";
    }
    if (obstaculo) {
        return "surprised";
    }
    if (fome_ > 75) {
        return "crying";
    }
    if (fome_ > 50) {
        return "sad";
    }
    if (temperatura > 28.0f) {
        return "embarrassed";
    }
    if (temperatura < 18.0f && temperatura > 0.0f) {
        return "confused";
    }
    if (esta_doente_) {
        return "confused";
    }
    if (personalidade_ == PERSONALIDADE_SARCASTICA) {
        return "angry";
    }
    if (personalidade_ == PERSONALIDADE_SENSIVEL) {
        return "loving";
    }
    
    return "happy";
}

bool TamagotchiEngine::ComparaUID(const uint8_t* a, const uint8_t* b) const {
    if (!a || !b) return false;
    return (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
}

bool TamagotchiEngine::EUIDZerado(const uint8_t* a) const {
    if (!a) return true;
    return (a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 0);
}

void TamagotchiEngine::CopiaUID(uint8_t* dest, const uint8_t* src) {
    if (!dest || !src) return;
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
    SaveState();
}


    fome_ = fome;
    diversao_ = diversao;
    saude_ = saude;
}


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
