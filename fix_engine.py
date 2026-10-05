
h_path = "main/tamagotchi_engine.h"
with open(h_path, "r", encoding="utf-8") as f:
    h_content = f.read()

if "GetTimerDurationMs" not in h_content:
    h_content = h_content.replace("uint32_t GetTimerRemainingMs() const;", "uint32_t GetTimerRemainingMs() const;\n    uint32_t GetTimerDurationMs() const { return timer_duration_ms_; }")
    with open(h_path, "w", encoding="utf-8") as f:
        f.write(h_content)
    print("Added GetTimerDurationMs to tamagotchi_engine.h")

