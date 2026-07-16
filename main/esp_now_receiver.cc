#include "esp_now_receiver.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "application.h"
#include "board.h"
#include "display.h"
#include "tamagotchi_engine.h"

static const char* TAG = "ESP_NOW_RX";

struct __attribute__((packed)) RobotState {
  uint8_t fome;
  uint8_t diversao;
  uint8_t saude;
  uint8_t humor;           // 0=FELIZ, 1=ENTEDIADO, 2=CARENTE, 3=DOENTE
  uint8_t estadoNascimento; // 0=OVO, 1=CHOCANDO, 2=NASCIDO
  uint32_t idadeDias;
  float temperatura;
  float umidade;
  uint8_t luzPorcento;
  bool petDormindo;
  bool choqueDetectado;
  bool obstaculoDetectado;
  bool animacaoComendo;
  bool animacaoBrincando;
  bool animacaoCurando;
  bool animacaoAcariciado;
  bool animacaoHighFive;
  bool animacaoDado;
  bool animacaoDance;
  uint8_t valorDado;
  char fala[64];
  
  // Novos campos para a Fase 2
  bool rfidLido;
  uint8_t rfidUID[4];
  
  // Novos campos para a arquitetura Cérebro-Corpo
  uint8_t rfidAcao;
  uint16_t somNivel;
  bool botaoPressionado;
};

static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    size_t max_size = sizeof(RobotState);
    
    if (len >= 90 && len <= max_size) {
        RobotState state;
        memset(&state, 0, sizeof(RobotState));
        memcpy(&state, data, len); // Copia apenas o que foi enviado
        
        ESP_LOGI(TAG, "Recebido RobotState via ESP-NOW: Fome=%d, Diversao=%d, Humor=%d, RFID_Lido=%d, RFID_Acao=%d", 
                 state.fome, state.diversao, state.humor, state.rfidLido, state.rfidAcao);
        
        // Garantia absoluta de terminação nula para evitar estouro de memória no std::string (Watchdog/OOM)
        state.fala[sizeof(state.fala) - 1] = '\0';
        
        // Executa a atualização da interface de forma thread-safe na fila principal da aplicação
        Application::GetInstance().Schedule([state]() {
            auto display = Board::GetInstance().GetDisplay();
            if (display == nullptr) return;
            
            // Atualiza o motor do Tamagotchi com as leituras físicas do corpo
            auto& engine = TamagotchiEngine::GetInstance();
            engine.SetSensorData(state.temperatura, state.umidade, state.luzPorcento,
                                 state.choqueDetectado, state.obstaculoDetectado,
                                 state.botaoPressionado, state.somNivel,
                                 state.rfidLido, state.rfidAcao, state.rfidUID);
            engine.SetAnimationState(state.animacaoComendo, state.animacaoBrincando,
                                     state.animacaoCurando, state.animacaoAcariciado);
            engine.Update();
            
            // Define a expressão facial dinamicamente gerada pelo motor Tamagotchi
            std::string emotion = engine.GetCurrentEmotion();
            display->SetEmotion(emotion.c_str());
            
            // Exibe balão de texto se o robô estiver com uma fala ativa
            if (strlen(state.fala) > 0) {
                display->SetChatMessage("assistant", state.fala);
            }
        });
    } else {
        ESP_LOGW(TAG, "Tamanho de pacote ESP-NOW incorreto: recebido %d, esperado entre 90 e %d", len, (int)max_size);
    }
}

void InitializeEspNowReceiver() {
    int retries = 0;
    esp_err_t err = ESP_OK;
    while (retries < 10) {
        err = esp_now_init();
        if (err == ESP_OK) {
            esp_now_register_recv_cb(esp_now_recv_cb);
            ESP_LOGI(TAG, "Receptor ESP-NOW registrado com sucesso!");
            return;
        }
        ESP_LOGW(TAG, "Falha ao inicializar ESP-NOW (tentativa %d): %s. Retentando em 1s...", retries + 1, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(1000));
        retries++;
    }
    ESP_LOGE(TAG, "Falha definitiva ao inicializar ESP-NOW após 10s: %s", esp_err_to_name(err));
}
