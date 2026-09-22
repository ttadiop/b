#include <Arduino.h>

// 定義 GPIO，請根據你的電路連接修改
#define ZERO_CROSS_PIN 42

// 使用 volatile 保證變量在 ISR 和主循環間正確同步
volatile uint32_t last_micros = 0;
volatile uint32_t current_interval = 0;
volatile bool new_data = false;

#define FAN_SSR_PIN 41  // SSR 控制輸出

// 定時器句柄
hw_timer_t * timer = NULL;

int zero_cross_delay_us = 4000;

// --- 2. 定時器中斷：真正觸發 SSR 的時刻 ---
void IRAM_ATTR onTimer() {
    
}

// --- 1. 過零中斷：同步起點 ---
void IRAM_ATTR onZeroCross() {
    // 每次看到過零點，重置並啟動定時器
    // 延時 dim_delay_us 後觸發 onTimer

    uint32_t now = micros();
    current_interval = now - last_micros;
    last_micros = now;
    new_data = true;
    delayMicroseconds(zero_cross_delay_us); 
    digitalWrite(FAN_SSR_PIN, HIGH); 
    // SSR 導通後需要維持一小段時間確保觸發 (約 10-100us)
    // 對於 AQG22205 這類內建光耦的 SSR，脈衝不需要太長
    delayMicroseconds(10000-zero_cross_delay_us*2); 
    digitalWrite(FAN_SSR_PIN, LOW);
    
    //timerAlarmWrite(timer, zero_cross_delay_us, false);
    //timerAlarmEnable(timer);
    //timerRestart(timer);
}

void setup() {
    Serial.begin(115200);
    
    // ESP32 建議使用 PULLUP 如果外部電阻較大
    pinMode(ZERO_CROSS_PIN, INPUT_PULLUP);
    pinMode(FAN_SSR_PIN, OUTPUT);
    digitalWrite(FAN_SSR_PIN, LOW); 

    // 綁定中斷。根據你的電路：
    // 光耦通電時 Q1 截止 (1)，過零時 Q1 導通 (0)
    // 所以捕捉 FALLING 代表「進入」過零區域
    attachInterrupt(digitalPinToInterrupt(ZERO_CROSS_PIN), onZeroCross, RISING);

    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);

    Serial.println("\n--- ESP32 高精度過零檢測測試 ---");
    Serial.println("目標間隔 (50Hz): 10000 us");
}

void loop() {
    // 配置硬體定時器 (定時器 0, 預分頻 80 -> 1us 計數一次)
    if (new_data) {
        new_data = false;

        // 計算即時頻率
        float frequency = 1000000.0 / (current_interval * 2.0); 

        Serial.printf("間隔: %u us | 估計頻率: %.2f Hz", current_interval, frequency);

        // 誤差分析
        int32_t error = (int32_t)current_interval - 10000;
        if (abs(error) > 200) {
            Serial.printf(" [警告: 抖動較大 %+d us]", error);
        }
        
        Serial.println();
    }

    // 餵狗，防止長時間不回到 loop 觸發 WDT (雖然此處有中斷，但保持良好習慣)
    yield(); 
}