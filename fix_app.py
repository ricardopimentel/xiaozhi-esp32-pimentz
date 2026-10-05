import re

with open('main/application.cc', 'r', encoding='utf-8') as f:
    content = f.read()

old_cb = '''            // Se ainda não nasceu, exibe o status de ovo/incubação na tela
            if (engine.GetEstadoNascimento() != ESTADO_NASCIDO) {
                if (engine.GetEstadoNascimento() == ESTADO_OVO) {
                    display->SetStatus("OVO DE TAMAGOTCHI");
                    display->SetChatMessage("system", "Aproxime o cartão RFID do corpo do robô para começar a chocar!");
                    display->SetEmotion("neutral");
                } else if (engine.GetEstadoNascimento() == ESTADO_CHOCANDO) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Chocando o ovo... (%d/15s)", engine.GetSegundosChocados());
                    display->SetStatus("CHOCANDO...");
                    display->SetChatMessage("system", buf);
                    display->SetEmotion("neutral");
                }
            } else {'''

new_cb = '''            // Se ainda não nasceu, exibe o status de ovo/incubação na tela
            if (engine.GetEstadoNascimento() != ESTADO_NASCIDO) {
                if (!Board::GetInstance().GetNetwork()->IsConnected()) {
                    display->SetStatus("AGUARDANDO WIFI");
                    display->SetChatMessage("system", "Configure o WiFi pelo celular (ou aguarde conectar) antes de chocar o ovo.");
                    display->SetEmotion("neutral");
                } else {
                    if (engine.GetEstadoNascimento() == ESTADO_OVO) {
                        display->SetStatus("OVO DE TAMAGOTCHI");
                        display->SetChatMessage("system", "Aproxime o cartão RFID do corpo do robô para começar a chocar!");
                        display->SetEmotion("neutral");
                    } else if (engine.GetEstadoNascimento() == ESTADO_CHOCANDO) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "Chocando o ovo... (%d/15s)", engine.GetSegundosChocados());
                        display->SetStatus("CHOCANDO...");
                        display->SetChatMessage("system", buf);
                        display->SetEmotion("neutral");
                    }
                }
            } else {'''

content = content.replace(old_cb, new_cb)

with open('main/application.cc', 'w', encoding='utf-8') as f:
    f.write(content)
