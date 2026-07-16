#ifndef TAMAGOTCHI_ENGINE_H
#define TAMAGOTCHI_ENGINE_H

#include <cstdint>
#include <string>

enum EstadoNascimento : uint8_t {
    ESTADO_OVO = 0,
    ESTADO_CHOCANDO = 1,
    ESTADO_NASCIDO = 2
};

enum Personalidade : uint8_t {
    PERSONALIDADE_BASICA = 0,
    PERSONALIDADE_SARCASTICA = 1,
    PERSONALIDADE_SENSIVEL = 2
};

class TamagotchiEngine {
public:
    static TamagotchiEngine& GetInstance();

    void Initialize();
    void Update();
    void SetSensorData(float temperatura, float umidade, uint8_t luz, bool choque, bool obstaculo, bool botao, uint16_t som, bool rfidLido, uint8_t rfidAcao, const uint8_t* rfidUID);
    void SetAnimationState(bool comendo, bool brincando, bool curando, bool acariciado) {
        sensor_animacao_comendo_ = comendo;
        sensor_animacao_brincando_ = brincando;
        sensor_animacao_curando_ = curando;
        sensor_animacao_acariciado_ = acariciado;
    }
    void SaveState();
    
    // Ações de cuidado
    void Feed();
    void Play();
    void Heal();
    void Pet();

    // Getters
    EstadoNascimento GetEstadoNascimento() const { return estado_nascimento_; }
    uint8_t GetFome() const { return fome_; }
    uint8_t GetDiversao() const { return diversao_; }
    uint8_t GetSaude() const { return saude_; }
    bool EstaDoente() const { return esta_doente_; }
    uint32_t GetIdadeDias() const { return idade_dias_; }
    Personalidade GetPersonalidade() const { return personalidade_; }
    uint16_t GetSegundosChocados() const { return segundos_chocados_; }
    
    float GetSensorTemperatura() const { return sensor_temperatura_; }
    float GetSensorUmidade() const { return sensor_umidade_; }
    uint8_t GetSensorLuz() const { return sensor_luz_porcento_; }
    bool GetSensorChoque() const { return sensor_choque_; }
    bool GetSensorObstaculo() const { return sensor_obstaculo_; }
    bool GetSensorBotao() const { return sensor_botao_pressionado_; }
    uint16_t GetSensorSom() const { return sensor_som_nivel_; }
    bool GetAnimacaoComendo() const { return sensor_animacao_comendo_; }
    bool GetAnimacaoBrincando() const { return sensor_animacao_brincando_; }
    bool GetAnimacaoCurando() const { return sensor_animacao_curando_; }
    bool GetAnimacaoAcariciado() const { return sensor_animacao_acariciado_; }
    
    std::string GetCurrentEmotion() const;

private:
    TamagotchiEngine();
    ~TamagotchiEngine() = default;
    
    void NascerPet();
    void ProcessarCicloIncubacao(bool rfidLido, const uint8_t* rfidUID);
    void AtualizarVinculo(int8_t pontos);
    
    bool ComparaUID(const uint8_t* a, const uint8_t* b) const;
    bool EUIDZerado(const uint8_t* a) const;
    void CopiaUID(uint8_t* dest, const uint8_t* src);

    // Estados e atributos
    EstadoNascimento estado_nascimento_ = ESTADO_OVO;
    uint8_t fome_ = 100;
    uint8_t diversao_ = 100;
    uint8_t saude_ = 100;
    bool esta_doente_ = false;
    uint32_t segundos_de_vida_ = 0;
    uint32_t idade_dias_ = 0;
    
    // Sensor cache
    float sensor_temperatura_ = 25.0f;
    float sensor_umidade_ = 50.0f;
    uint8_t sensor_luz_porcento_ = 100;
    bool sensor_choque_ = false;
    bool sensor_obstaculo_ = false;
    bool sensor_botao_pressionado_ = false;
    uint16_t sensor_som_nivel_ = 0;
    bool sensor_rfid_lido_ = false;
    uint8_t sensor_rfid_acao_ = 0;
    uint8_t sensor_rfid_uid_[4] = {0};

    // Animation flags from ESP-NOW
    bool sensor_animacao_comendo_ = false;
    bool sensor_animacao_brincando_ = false;
    bool sensor_animacao_curando_ = false;
    bool sensor_animacao_acariciado_ = false;

    // UIDs de RFID para interacao
    uint8_t uid_comida_[4] = {0};
    uint8_t uid_brincar_[4] = {0};
    uint8_t uid_saude_[4] = {0};
    uint8_t uid_pet_[4] = {0};
    
    Personalidade personalidade_natural_ = PERSONALIDADE_BASICA;
    Personalidade personalidade_ = PERSONALIDADE_BASICA;
    int8_t pontos_de_vinculo_ = 0;
    uint16_t segundos_chocados_ = 0;
    
    // Timers
    uint64_t last_tick_time_ = 0;
    uint64_t last_save_time_ = 0;
    uint64_t tempo_no_frio_ = 0;
    uint64_t last_vinculo_check_ = 0;
};

#endif // TAMAGOTCHI_ENGINE_H
