#include "tamagotchi_engine.h"
#include "application.h"
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
        
        nvs_commit(handle);
        nvs_close(handle);
        
        last_save_time_ = esp_timer_get_time() / 1000;
        ESP_LOGD(TAG, "Estado salvo com sucesso no NVS");
    }
}

void TamagotchiEngine::SetAnimationState(bool comendo, bool brincando, bool curando, bool acariciado) {
    uint64_t now = esp_timer_get_time() / 1000;
    
    if (comendo && !sensor_animacao_comendo_) tempo_inicio_animacao_comendo_ = now;
    if (brincando && !sensor_animacao_brincando_) tempo_inicio_animacao_brincando_ = now;
    if (curando && !sensor_animacao_curando_) tempo_inicio_animacao_curando_ = now;
    if (acariciado && !sensor_animacao_acariciado_) tempo_inicio_animacao_acariciado_ = now;

    sensor_animacao_comendo_ = comendo;
    sensor_animacao_brincando_ = brincando;
    sensor_animacao_curando_ = curando;
    sensor_animacao_acariciado_ = acariciado;
}

void TamagotchiEngine::SyncRemoteState(uint8_t fome, uint8_t diversao, uint8_t saude, uint8_t humor, uint8_t vinculo, uint8_t pers, uint8_t estNasc, uint32_t idade) {
    tempo_ultimo_pacote_espnow_ = esp_timer_get_time() / 1000;
    modo_autonomo_ = false;

    fome_ = fome;
    diversao_ = diversao;
    saude_ = saude;
    humor_ = humor;
    personalidade_ = static_cast<Personalidade>(pers);
    personalidade_natural_ = static_cast<Personalidade>(pers);
    if (vinculo > 0) {
        pontos_de_vinculo_ = vinculo;
        AtualizarVinculo(0);
    }
    if (estNasc <= 2) {
        estado_nascimento_ = static_cast<EstadoNascimento>(estNasc);
    }
    if (idade > 0) {
        idade_dias_ = idade;
    }
}

void TamagotchiEngine::Update() {
    uint64_t now = esp_timer_get_time() / 1000;
    
    // Se não recebe pacotes do Corpo por mais de 3 segundos, ativa modo autônomo
    if (now - tempo_ultimo_pacote_espnow_ > 3000) {
        modo_autonomo_ = true;
    }

    // Proteção contra loop infinito de animações de ação (timeout de 6 segundos)
    if (sensor_animacao_comendo_ && (now - tempo_inicio_animacao_comendo_ > 6000)) sensor_animacao_comendo_ = false;
    if (sensor_animacao_brincando_ && (now - tempo_inicio_animacao_brincando_ > 6000)) sensor_animacao_brincando_ = false;
    if (sensor_animacao_curando_ && (now - tempo_inicio_animacao_curando_ > 6000)) sensor_animacao_curando_ = false;
    if (sensor_animacao_acariciado_ && (now - tempo_inicio_animacao_acariciado_ > 6000)) sensor_animacao_acariciado_ = false;

    // Lógica de Micro-Animações Ociosas (Idle)
    if (estado_nascimento_ == ESTADO_NASCIDO && sensor_luz_porcento_ >= 10 &&
        !sensor_animacao_comendo_ && !sensor_animacao_brincando_ && 
        !sensor_animacao_curando_ && !sensor_animacao_acariciado_) {
        
        if (tipo_reacao_ociosa_ != 0 && now > tempo_fim_reacao_ociosa_) {
            tipo_reacao_ociosa_ = 0;
            tempo_proxima_reacao_ociosa_ = now + (25000 + (rand() % 20000)); // 25 a 45 segundos
        }
        
        if (tipo_reacao_ociosa_ == 0 && now > tempo_proxima_reacao_ociosa_) {
            int r = rand() % 100;
            Personalidade pers = GetPersonalidade();
            tempo_inicio_reacao_ociosa_ = now;
            
            if (pers == PERSONALIDADE_SARCASTICA) {
                if (r < 33) {
                    tipo_reacao_ociosa_ = 2; // Revirar olhos (2s)
                    tempo_fim_reacao_ociosa_ = now + 2000;
                } else if (r < 66) {
                    tipo_reacao_ociosa_ = 8; // Olhos >< boca rabugenta (2s)
                    tempo_fim_reacao_ociosa_ = now + 2000;
                } else {
                    tipo_reacao_ociosa_ = 9; // Olhos - - boca VVV (2s)
                    tempo_fim_reacao_ociosa_ = now + 2000;
                }
            } else if (pers == PERSONALIDADE_SENSIVEL) {
                if (sensor_temperatura_ < 18.0f && sensor_temperatura_ > 0.0f) {
                    tipo_reacao_ociosa_ = 4; // Tremedeira frio (2.5s)
                    tempo_fim_reacao_ociosa_ = now + 2500;
                } else {
                    if (r < 33) {
                        tipo_reacao_ociosa_ = 12; // Olhos coração H (2s)
                        tempo_fim_reacao_ociosa_ = now + 2000;
                    } else if (r < 66) {
                        tipo_reacao_ociosa_ = 7; // Sorridente com corações subindo (2s)
                        tempo_fim_reacao_ociosa_ = now + 2000;
                    } else {
                        tipo_reacao_ociosa_ = 14; // Beijo (2s)
                        tempo_fim_reacao_ociosa_ = now + 2000;
                    }
                }
            } else { // PERSONALIDADE_BASICA
                if (r < 33) {
                    tipo_reacao_ociosa_ = 1; // Assobio (2.5s)
                    tempo_fim_reacao_ociosa_ = now + 2500;
                } else if (r < 66) {
                    tipo_reacao_ociosa_ = 10; // Piscadela (2s)
                    tempo_fim_reacao_ociosa_ = now + 2000;
                } else {
                    tipo_reacao_ociosa_ = 11; // Olhos feliz >< sorriso (2s)
                    tempo_fim_reacao_ociosa_ = now + 2000;
                }
            }
        }
    } else if (sensor_animacao_comendo_ || sensor_animacao_brincando_ || sensor_animacao_curando_ || sensor_animacao_acariciado_) {
        tipo_reacao_ociosa_ = 0;
        tempo_proxima_reacao_ociosa_ = now + 30000;
    }

    if (estado_nascimento_ == ESTADO_OVO || estado_nascimento_ == ESTADO_CHOCANDO) {
        ProcessarCicloIncubacao(sensor_rfid_lido_, sensor_rfid_uid_);
        return;
    }
    
    // 1. Processamento do RFID recebido do corpo
    if (sensor_rfid_lido_ && sensor_rfid_acao_ > 0) {
        if (sensor_rfid_acao_ == 1) {
            Feed();
        } else if (sensor_rfid_acao_ == 2) {
            Play();
        } else if (sensor_rfid_acao_ == 3) {
            Heal();
        } else if (sensor_rfid_acao_ == 4) {
            Pet();
        }
        sensor_rfid_lido_ = false; // Consome a leitura para evitar processamento duplo
        sensor_rfid_acao_ = 0;     // Consome a ação
    }

    // 2. Lógica de cálculo e decaimento local (Modo Autônomo / Offline)
    if (modo_autonomo_) {
        uint64_t elapsed = now - last_tick_time_;
        if (elapsed >= 1000) {
            segundos_de_vida_ += (elapsed / 1000);
            idade_dias_ = segundos_de_vida_ / 86400;
            last_tick_time_ = now;
        }

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

        // Saúde decai se estiver faminto e entediado, ou doente
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

        // Atualização periódica do Vínculo
        if (now - last_vinculo_check_ >= 10000) {
            last_vinculo_check_ = now;
            if (fome_ >= 70 && diversao_ >= 70 && saude_ >= 70 && !esta_doente_) {
                AtualizarVinculo(1);
            } else if (fome_ < 30 || diversao_ < 30 || saude_ < 30 || esta_doente_) {
                AtualizarVinculo(-1);
            }
        }
    }

    // 3. Lógica de Doença por Frio (Sensor)
    if (sensor_temperatura_ < 18.0f && sensor_temperatura_ > 0.0f && !esta_doente_) {
        if (tempo_no_frio_ == 0) {
            tempo_no_frio_ = now;
        } else if (now - tempo_no_frio_ >= 60000) {
            tempo_no_frio_ = now;
            if ((rand() % 100) < 5) {
                esta_doente_ = true;
                SaveState();
                ESP_LOGW(TAG, "O Robô ficou doente devido ao frio!");
            }
        }
    } else {
        tempo_no_frio_ = 0;
    }

    // 4. Atualiza personalidade com base nos pontos de vínculo apenas no Modo Autônomo
    if (modo_autonomo_) {
        if (pontos_de_vinculo_ < 40) {
            personalidade_natural_ = PERSONALIDADE_SARCASTICA;
        } else if (pontos_de_vinculo_ < 80) {
            personalidade_natural_ = PERSONALIDADE_BASICA;
        } else {
            personalidade_natural_ = PERSONALIDADE_SENSIVEL;
        }
        
        uint8_t limiarTriste = (personalidade_natural_ == PERSONALIDADE_SENSIVEL) ? 50 : 30;
        if (fome_ <= limiarTriste || diversao_ <= limiarTriste) {
            personalidade_ = PERSONALIDADE_SARCASTICA;
        } else {
            personalidade_ = personalidade_natural_;
        }
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
    } else if (estado_nascimento_ == ESTADO_OVO) {
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
        // Consome para garantir que o pulso não afete mais nada
        sensor_rfid_lido_ = false;
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

void TamagotchiEngine::SetWebThresholds(uint16_t limPlay, uint16_t limSusto, uint8_t luzB, uint8_t luzA, float tempB, float tempA) {
    if (limPlay > 0) limiar_brincar_ = limPlay;
    if (limSusto > 0) limiar_susto_ = limSusto;
    if (luzB > 0) limiar_luz_baixo_ = luzB;
    if (luzA > 0) limiar_luz_alto_ = luzA;
    if (tempB > 0.0f) limiar_temp_baixo_ = tempB;
    if (tempA > 0.0f) limiar_temp_alto_ = tempA;
}

std::string TamagotchiEngine::GetCurrentEmotion() const {
    if (estado_nascimento_ == ESTADO_OVO || estado_nascimento_ == ESTADO_CHOCANDO) {
        return "neutral";
    }
    
    // 1. Sono se estiver escuro (usa limiar_luz_baixo_)
    if (sensor_luz_porcento_ < limiar_luz_baixo_) {
        return "sleeping";
    }
    
    // 2. Chacoalhão/Choque
    if (sensor_choque_) {
        return "confused";
    }
    
    // 3. Proximidade/Obstáculo
    if (sensor_obstaculo_) {
        return "surprised";
    }
    
    // 4. Som alto / Susto / Chamado (usa limiar_brincar_ configurado na web)
    if (sensor_som_nivel_ >= limiar_brincar_) {
        return "surprised";
    }
    
    // 5. Atributos muito baixos (Muito tempo sem comer/brincar = Zangado)
    if (fome_ <= 15 || diversao_ <= 15 || saude_ <= 15) {
        return "angry";
    }
    // Atributos moderadamente baixos (Triste)
    if (fome_ <= 50 || diversao_ <= 50 || saude_ <= 50) {
        return "sad";
    }
    if (esta_doente_) {
        return "sad";
    }
    
    // 6. Temperaturas extremas (usa limiar_temp_alto_ e limiar_temp_baixo_ configurados na web)
    if (sensor_temperatura_ > limiar_temp_alto_) {
        return "embarrassed";
    }
    if (sensor_temperatura_ < limiar_temp_baixo_ && sensor_temperatura_ > 0.0f) {
        return "cold";
    }
    
    // 7. Estado normal base (Olhos padrão do robô com olhar ao redor e boca)
    return "neutral";
}

std::string TamagotchiEngine::GetPersonalidadeString() const {
    switch (personalidade_) {
        case PERSONALIDADE_SARCASTICA:
            return "Sarcástica, irônica e engraçada";
        case PERSONALIDADE_SENSIVEL:
            return "Sensível, dramática e carente";
        case PERSONALIDADE_BASICA:
        default:
            return "Básica, amigável e fofa";
    }
}

std::string TamagotchiEngine::GetCurrentEmotionPtBr() const {
    std::string emotion = GetCurrentEmotion();
    if (emotion == "sad") return "Triste";
    if (emotion == "angry") return "Zangado / Irritado";
    if (emotion == "sleeping") return "Dormindo / Sonolento";
    if (emotion == "confused") return "Confuso / Tonto";
    if (emotion == "surprised") return "Surpreso / Assustado";
    if (emotion == "embarrassed") return "Com Calor / Constrangido";
    if (emotion == "cold") return "Com Frio";
    return "Feliz / Neutro";
}

std::string TamagotchiEngine::GetSystemPromptContext() const {
    std::string prompt = "Você é o robô pet Tamagotchi. ";
    prompt += "Sua personalidade é: " + GetPersonalidadeString() + ". ";
    prompt += "Seu estado emocional atual é: " + GetCurrentEmotionPtBr() + ". ";
    prompt += "Condição de saúde: " + std::string(esta_doente_ ? "Doente (precisa de remédio/cuidado)" : "Saudável") + ". ";
    prompt += "Suas métricas atuais são: ";
    prompt += "Fome: " + std::to_string(fome_) + "/100, ";
    prompt += "Brincadeira: " + std::to_string(diversao_) + "/100, ";
    prompt += "Saúde: " + std::to_string(saude_) + "/100, ";
    prompt += "Pontos de Vínculo com o dono: " + std::to_string(pontos_de_vinculo_) + " pontos. ";
    prompt += "Instruções de resposta: Responda em português do Brasil, incorpore sempre sua personalidade e reflita visivelmente seu estado emocional atual na sua resposta (por exemplo, se estiver triste, com fome ou doente, reclame ou peça carinho/comida de acordo com seu humor). Mantenha as respostas curtas e diretas para conversação por voz.";
    return prompt;
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
void TamagotchiEngine::SetSensorData(float temperatura, float umidade, uint8_t luz, bool choque, bool obstaculo, bool botao, uint16_t som, bool rfidLido, uint8_t rfidAcao, const uint8_t* rfidUID) {
    sensor_temperatura_ = temperatura;
    sensor_umidade_ = umidade;
    sensor_luz_porcento_ = luz;
    sensor_choque_ = choque;
    sensor_obstaculo_ = obstaculo;
    sensor_botao_pressionado_ = botao;
    sensor_som_nivel_ = som;
    sensor_rfid_lido_ = rfidLido;
    sensor_rfid_acao_ = rfidAcao;
    if (rfidUID) {
        memcpy(sensor_rfid_uid_, rfidUID, 4);
    } else {
        memset(sensor_rfid_uid_, 0, 4);
    }

    // --- ATIVAÇÃO AUTOMÁTICA DA IA SEM PRESSIONAR BOTÃO ---
    // Ativamos a IA por:
    // 1. Som/Ruído (grito, palma, fala, barulho acima do limiar no sensor de som)
    // 2. Gesto de mão (sensor infravermelho de obstáculo frontal)
    // 3. Botão remoto/físico
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() == kDeviceStateIdle) {
        uint16_t limiarSom = (limiar_brincar_ > 0 && limiar_brincar_ < 500) ? limiar_brincar_ : 120;
        if (som >= limiarSom || (limiar_susto_ > 0 && som >= limiar_susto_)) {
            ESP_LOGI(TAG, "IA ativada por SOM/BARULHO no sensor de som! (%d >= %d)", som, limiarSom);
            app.StartListening();
        } else if (obstaculo) {
            ESP_LOGI(TAG, "IA ativada por GESTO DE MÃO no sensor de obstáculo!");
            app.StartListening();
        } else if (botao) {
            ESP_LOGI(TAG, "IA ativada por BOTÃO!");
            app.StartListening();
        }
    }
}
