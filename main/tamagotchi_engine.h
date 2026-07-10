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
    void Update(float temperatura, float umidade, bool rfidLido, const uint8_t* rfidUID);
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
    
    std::string GetCurrentEmotion(float temperatura, bool choque, bool obstaculo) const;

private:
    TamagotchiEngine();
    ~TamagotchiEngine() = default;
    
    void NascerPet();
    void ProcessarCicloIncubacao(bool rfidLido, const uint8_t* rfidUID);
    void AtualizarVinculo(int8_t pontos);

    // Estados e atributos
    EstadoNascimento estado_nascimento_ = ESTADO_OVO;
    uint8_t fome_ = 100;
    uint8_t diversao_ = 100;
    uint8_t saude_ = 100;
    bool esta_doente_ = false;
    uint32_t segundos_de_vida_ = 0;
    uint32_t idade_dias_ = 0;
    
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
