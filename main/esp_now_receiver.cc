#include "esp_now_receiver.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <string.h>
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
};

static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(RobotState)) {
        RobotState state;
        memcpy(&state, data, sizeof(RobotState));
        ESP_LOGI(TAG, "Recebido RobotState via ESP-NOW: Fome=%d, Diversao=%d, Humor=%d", state.fome, state.diversao, state.humor);
        
        // Executa a atualização da interface de forma thread-safe na fila principal da aplicação
        Application::GetInstance().Schedule([state]() {
            auto display = Board::GetInstance().GetDisplay();
            if (display == nullptr) return;
            
            // Atualiza o motor do Tamagotchi com as leituras físicas do corpo
            // Temporariamente passamos falso/nullptr para o RFID até atualizarmos o protocolo de envio do corpo
            auto& engine = TamagotchiEngine::GetInstance();
            engine.Update(state.temperatura, state.umidade, false, nullptr);
            
            // Define a expressão facial dinamicamente gerada pelo motor Tamagotchi
            std::string emotion = engine.GetCurrentEmotion(state.temperatura, state.choqueDetectado, state.obstaculoDetectado);
            display->SetEmotion(emotion.c_str());
            
            // Exibe balão de texto se o robô estiver com uma fala ativa
            if (strlen(state.fala) > 0) {
                display->SetChatMessage("assistant", state.fala);
            }
        });
    } else {
        ESP_LOGW(TAG, "Tamanho de pacote ESP-NOW incorreto: recebido %d, esperado %d", len, (int)sizeof(RobotState));
    }
}

void InitializeEspNowReceiver() {
    esp_err_t err = esp_now_init();
    if (err == ESP_OK) {
        esp_now_register_recv_cb(esp_now_recv_cb);
        ESP_LOGI(TAG, "Receptor ESP-NOW registrado com sucesso!");
    } else {
        ESP_LOGE(TAG, "Falha ao inicializar ESP-NOW: %s", esp_err_to_name(err));
    }
}
