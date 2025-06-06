/* Edge Impulse + Cloud Offloading for Lab 5
 * Based on Lab 4 code with cloud offloading capability
 */

/* Includes ---------------------------------------------------------------- */
#include <DianaDing-project-1_inferencing.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "MyESP32Hotspot";
const char* password = "dingyuqiao";

// Server URL
const char* serverUrl = "http://172.20.20.20:8000/predict";

// Cloud offloading threshold
#define CONFIDENCE_THRESHOLD 80.0

#define LED_PIN 5           // D3 = GPIO5
#define BUTTON_PIN 21       // D6 = GPIO21 on XIAO ESP32C3

Adafruit_MPU6050 mpu;

#define SAMPLE_RATE_MS 10
#define CAPTURE_DURATION_MS 2000
#define FEATURE_SIZE EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE

float features[FEATURE_SIZE];
bool capturing = false;
unsigned long last_sample_time = 0;
unsigned long capture_start_time = 0;
int sample_count = 0;

bool lastButtonState = HIGH;
bool wifiConnected = false;

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void setup() {
    delay(500);  // 电池供电时稳定一下供电电压

    Serial.begin(115200);
    while (!Serial && millis() < 3000);  // 最多等 3 秒，防止无串口阻塞

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_PIN, LOW);

    // 启动后 LED 快闪 2 次，确认程序在运行
    for (int i = 0; i < 2; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
        delay(200);
    }

    Serial.println("Initializing MPU6050...");
    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip");
        while (1) { delay(10); }
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // Connect to WiFi
    connectToWiFi();

    Serial.println("Magic Wand ready! Press the button to start capture.");
}

void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        // WiFi连接成功 - LED长亮2秒
        digitalWrite(LED_PIN, HIGH);
        delay(2000);
        digitalWrite(LED_PIN, LOW);
    } else {
        wifiConnected = false;
        Serial.println("\nWiFi connection failed! Will use local inference only.");
        
        // WiFi连接失败 - LED快闪5次
        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }
}

void loop() {
    int currentButtonState = digitalRead(BUTTON_PIN);

    if (lastButtonState == HIGH && currentButtonState == LOW && !capturing) {
        Serial.println("Button pressed, starting capture...");
        sample_count = 0;
        capturing = true;
        capture_start_time = millis();
        last_sample_time = millis();
    }
    lastButtonState = currentButtonState;

    if (capturing) {
        capture_accelerometer_data();
    }

    delay(10);  // 防抖
}

void capture_accelerometer_data() {
    if (millis() - last_sample_time >= SAMPLE_RATE_MS) {
        last_sample_time = millis();

        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        if (sample_count < FEATURE_SIZE / 3) {
            int idx = sample_count * 3;
            features[idx] = a.acceleration.x;
            features[idx + 1] = a.acceleration.y;
            features[idx + 2] = a.acceleration.z;
            sample_count++;
        }

        if (millis() - capture_start_time >= CAPTURE_DURATION_MS) {
            capturing = false;
            Serial.println("Capture complete.");
            run_hybrid_inference();  // 修改：使用混合推理
        }
    }
}

void run_hybrid_inference() {
    if (sample_count * 3 < FEATURE_SIZE) {
        Serial.println("ERROR: Not enough data for inference");
        return;
    }

    Serial.println("Performing local inference...");
    
    // 首先进行本地推理
    ei_impulse_result_t result = { 0 };
    signal_t features_signal;
    features_signal.total_length = FEATURE_SIZE;
    features_signal.get_data = &raw_feature_get_data;

    EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        Serial.print("ERR: Failed to run classifier (");
        Serial.print(res);
        Serial.println(")");
        return;
    }

    // 获取本地推理结果
    float max_value = 0;
    int max_index = -1;
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > max_value) {
            max_value = result.classification[i].value;
            max_index = i;
        }
    }

    float confidence = max_value * 100;
    String local_label = (max_index != -1) ? ei_classifier_inferencing_categories[max_index] : "unknown";
    
    Serial.print("Local inference: ");
    Serial.print(local_label);
    Serial.print(" (");
    Serial.print(confidence);
    Serial.println("%)");

    // 决策：是否需要云端卸载
    if (confidence < CONFIDENCE_THRESHOLD) {
        Serial.println("Low confidence - sending raw data to server...");
        if (wifiConnected) {
            sendRawDataToServer();
        } else {
            Serial.println("WiFi not connected - using local result anyway");
            actuateLED(local_label, confidence, false);
        }
    } else {
        Serial.println("High confidence - using local result");
        actuateLED(local_label, confidence, false);
    }
}

void sendRawDataToServer() {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    
    // 构建JSON数组，发送原始特征数据
    DynamicJsonDocument doc(8192);
    JsonArray dataArray = doc.createNestedArray("data");
    
    for (int i = 0; i < FEATURE_SIZE; i++) {
        dataArray.add(features[i]);
    }
    
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    
    Serial.println("Sending data to cloud server...");
    int httpResponseCode = http.POST(jsonPayload);
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Server response: " + response);
        
        // 解析JSON响应
        DynamicJsonDocument responseDoc(256);
        DeserializationError error = deserializeJson(responseDoc, response);
        if (!error) {
            const char* gesture = responseDoc["gesture"];
            float confidence = responseDoc["confidence"];
            
            Serial.println("Cloud Inference Result:");
            Serial.print("Gesture: ");
            Serial.println(gesture);
            Serial.print("Confidence: ");
            Serial.print(confidence);
            Serial.println("%");
            
            // 基于云端结果控制LED
            actuateLED(String(gesture), confidence, true);
        } else {
            Serial.print("Failed to parse server response: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.printf("Error sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    
    http.end();
}

void actuateLED(String label, float confidence, bool isCloudResult) {
    if (isCloudResult) {
        Serial.print("Cloud result - ");
    } else {
        Serial.print("Local result - ");
    }
    Serial.print("Actuating LED for gesture: ");
    Serial.print(label);
    Serial.print(" (confidence: ");
    Serial.print(confidence);
    Serial.println("%)");
    
    if (label == "O") {
        blink_led(3, 100);  // O手势：快闪 3 次
    } else if (label == "V") {
        blink_led(2, 300);  // V手势：慢闪 2 次
    } else if (label == "Z") {
        digitalWrite(LED_PIN, HIGH);  // Z手势：常亮1秒
        delay(1000);
        digitalWrite(LED_PIN, LOW);
    } else {
        digitalWrite(LED_PIN, LOW); // 其他手势：关闭LED
    }
    
    Serial.println("Ready for next gesture...");
}

void blink_led(int times, int delay_ms) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(delay_ms);
        digitalWrite(LED_PIN, LOW);
        delay(delay_ms);
    }
}