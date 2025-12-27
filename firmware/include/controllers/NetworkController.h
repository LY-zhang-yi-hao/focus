#pragma once

#include <BluetoothA2DPSink.h>
#include <WiFiProvisioner.h>
#include <Preferences.h>

class NetworkController
{
public:
    NetworkController();
    void begin();
    void update();
    void startProvisioning();
    void stopProvisioning();
    void reset();
    bool isWiFiProvisioned();
    bool isWiFiConnected();
    bool isBluetoothPaired();
    void initializeBluetooth();
    void startBluetooth();
    void stopBluetooth();
    void sendWebhookAction(const String &action);

private:
    BluetoothA2DPSink a2dp_sink;
    Preferences preferences;
    WiFiProvisioner::WiFiProvisioner wifiProvisioner; // Instance of WiFiProvisioner / WiFi 配网实例

    String webhookURL;
    bool btPaired; // Paired state loaded from NVS / NVS 记录的蓝牙配对状态
    bool bluetoothActive;
    bool bluetoothAttempted;
    bool provisioningMode;
    unsigned long lastBluetoothtAttempt;

    void WiFiProvisionerSettings();
    void saveBluetoothPairedState(bool paired);
    static void btConnectionStateCallback(esp_a2d_connection_state_t state, void *obj);

    // Tasks / 任务句柄
    TaskHandle_t bluetoothTaskHandle;
    TaskHandle_t webhookTaskHandle;
    QueueHandle_t webhookQueue;

    static void bluetoothTask(void *param);
    static void webhookTask(void *param);
    bool sendWebhookRequest(const String &action);

    static NetworkController *instance;

    static bool validateInputCallback(const String &input);
    static void factoryResetCallback();

    bool validateInput(const String &input);
    void handleFactoryReset();
};
