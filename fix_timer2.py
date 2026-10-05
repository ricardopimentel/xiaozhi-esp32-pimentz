
# -*- coding: utf-8 -*-
with open("main/tamagotchi_engine.cc", "r", encoding="latin1") as f:
    content = f.read()

# Fix Update
target_update = """void TamagotchiEngine::Update() {
    uint64_t now = esp_timer_get_time() / 1000;
    if (timer_active_ && GetTimerRemainingMs() == 0) {
        ESP_LOGI("TamagotchiEngine", "Timer finalizado: %s", timer_label_.c_str());
        timer_active_ = false;
        tipo_reacao_ociosa_ = 10;
        tempo_fim_reacao_ociosa_ = now + 5000;
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
    }
    if (timer_active_ && GetTimerRemainingMs() == 0) {
        ESP_LOGI("TamagotchiEngine", "Timer finalizado: %s", timer_label_.c_str());
        timer_active_ = false;
        tipo_reacao_ociosa_ = 10;
        tempo_fim_reacao_ociosa_ = now + 5000;
        Application::GetInstance().PlaySound("custom_alarm");
    }"""
replacement_update = """void TamagotchiEngine::Update() {
    uint64_t now = esp_timer_get_time() / 1000;
    
    if (timer_active_ && GetTimerRemainingMs() == 0) {
        ESP_LOGI("TamagotchiEngine", "Timer finalizado: %s", timer_label_.c_str());
        timer_active_ = false;
        tipo_reacao_ociosa_ = 10;
        tempo_fim_reacao_ociosa_ = now + 5000;
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
    }"""
content = content.replace(target_update, replacement_update)

# Fix GetTimerRemainingMs
target_timer = """uint32_t TamagotchiEngine::GetTimerRemainingMs() const {
    if (!timer_active_) return 0;
    uint64_t now = esp_timer_get_time() / 1000;
    if (timer_active_ && GetTimerRemainingMs() == 0) {
        ESP_LOGI("TamagotchiEngine", "Timer finalizado: %s", timer_label_.c_str());
        timer_active_ = false;
        tipo_reacao_ociosa_ = 10;
        tempo_fim_reacao_ociosa_ = now + 5000;
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
    }
    if (timer_active_ && GetTimerRemainingMs() == 0) {
        ESP_LOGI("TamagotchiEngine", "Timer finalizado: %s", timer_label_.c_str());
        timer_active_ = false;
        tipo_reacao_ociosa_ = 10;
        tempo_fim_reacao_ociosa_ = now + 5000;
        Application::GetInstance().PlaySound("custom_alarm");
    }
    uint64_t elapsed = now - timer_start_time_;"""
replacement_timer = """uint32_t TamagotchiEngine::GetTimerRemainingMs() const {
    if (!timer_active_) return 0;
    uint64_t now = esp_timer_get_time() / 1000;
    uint64_t elapsed = now - timer_start_time_;"""
content = content.replace(target_timer, replacement_timer)

with open("main/tamagotchi_engine.cc", "w", encoding="latin1") as f:
    f.write(content)
print("Fix applied")

