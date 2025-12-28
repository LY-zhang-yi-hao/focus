#include "Config.h"
#include "controllers/NetworkController.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <BluetoothA2DPSink.h>
#include <ArduinoJson.h>
#include <esp_bt.h>
#include <memory>

NetworkController *NetworkController::instance = nullptr;

NetworkController::NetworkController()
    : a2dp_sink(),
      btPaired(false),
      bluetoothActive(false),
      bluetoothAttempted(false),
      lastBluetoothtAttempt(0),
      bluetoothTaskHandle(nullptr),
      webhookQueue(nullptr),
      webhookTaskHandle(nullptr),
      provisioningMode(false),
      apiServer(nullptr),
      apiServerStarted(false),
      lastTaskListJson(""),
      onTaskListUpdate(nullptr)
{

    instance = this;
}

void NetworkController::begin()
{
    WiFiProvisionerSettings();

    if (isWiFiProvisioned())
    {
        Serial.println("Stored WiFi credentials found. Connecting... / 已找到已存 WiFi 凭据，开始连接");
        wifiProvisioner.connectToWiFi();
    }

    // Load bluetooth paired state from nvs / 从 NVS 读取蓝牙配对状态
    preferences.begin("network", true);
    btPaired = preferences.getBool("bt_paired", false);
    preferences.end();

    if (btPaired)
    {
        Serial.println("Previously paired with a device. Initializing Bluetooth. / 检测到已配对设备，初始化蓝牙");
        initializeBluetooth(); // Initialize Bluetooth if previously paired / 已配对则初始化蓝牙
    }
    else
    {
        Serial.println("No previous Bluetooth pairing found. Skipping Bluetooth initialization. / 未发现历史蓝牙配对，跳过初始化");
    }

    // Load Webhook URL from NVS under the "focusdial" namespace / 从 focusdial 命名空间读取 webhook URL
    preferences.begin("focusdial", true);
    webhookURL = preferences.getString("webhook_url", "");
    preferences.end();

    if (!webhookURL.isEmpty())
    {
        Serial.println("Loaded Webhook URL: " + webhookURL + " / 已加载 webhook 地址");
    }

    if (webhookQueue == nullptr)
    {
        webhookQueue = xQueueCreate(5, sizeof(char *));
    }

    if (webhookTaskHandle == nullptr)
    {
        xTaskCreatePinnedToCore(webhookTask, "Webhook Task", 4096, this, 0, &webhookTaskHandle, 1);
        Serial.println("Persistent webhook task started. / Webhook 任务已启动");
    }

    // API Server will be started lazily after WiFi is connected / 在 WiFi 连接后再延迟启动 API 服务器
}

void NetworkController::update()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();
    }

    ensureApiServer();
    if (apiServerStarted && apiServer != nullptr)
    {
        apiServer->handleClient();
    }
}

bool NetworkController::isWiFiProvisioned()
{
    // Check for stored WiFi credentials / 检查是否已有 WiFi 凭据
    preferences.begin("network", true);
    String storedSSID = preferences.getString("ssid", "");
    preferences.end();

    return !storedSSID.isEmpty(); // Return true if credentials are found / 只要存在即返回真
}

bool NetworkController::isWiFiConnected()
{
    return (WiFi.status() == WL_CONNECTED);
}

bool NetworkController::isBluetoothPaired()
{
    return btPaired;
}

void NetworkController::startProvisioning()
{
    Serial.println("Starting provisioning mode... / 进入配网模式");
    btPaired = false;        // Reset paired state for new provisioning / 新配网前重置配对状态
    bluetoothActive = true;  // Enable Bluetooth for pairing / 开启蓝牙用于配对
    provisioningMode = true; // Indicate we are in provisioning mode / 记录当前为配网模式
    initializeBluetooth();
    wifiProvisioner.setupAccessPointAndServer();
}

void NetworkController::stopProvisioning()
{
    Serial.println("Stopping provisioning mode... / 退出配网模式");
    bluetoothActive = false;  // Disable Bluetooth after provisioning / 配网后关闭蓝牙
    provisioningMode = false; // Exit provisioning mode / 退出配网模式
    stopBluetooth();
}

void NetworkController::reset()
{
    wifiProvisioner.resetCredentials();
    if (btPaired)
    {
        a2dp_sink.clean_last_connection();
        saveBluetoothPairedState(false);
    }
    Serial.println("Reset complete. WiFi credentials and paired state cleared. / 重置完成，已清除 WiFi 凭据与蓝牙配对状态");
}

void NetworkController::initializeBluetooth()
{
    if (bluetoothTaskHandle == nullptr)
    {

        // Configure the A2DP sink with empty callbacks to use it for the trigger only / 配置 A2DP sink，仅用于触发，不处理数据
        a2dp_sink.set_stream_reader(nullptr, false);
        a2dp_sink.set_raw_stream_reader(nullptr);
        a2dp_sink.set_on_volumechange(nullptr);
        a2dp_sink.set_avrc_connection_state_callback(nullptr);
        a2dp_sink.set_avrc_metadata_callback(nullptr);
        a2dp_sink.set_avrc_rn_playstatus_callback(nullptr);
        a2dp_sink.set_avrc_rn_track_change_callback(nullptr);
        a2dp_sink.set_avrc_rn_play_pos_callback(nullptr);
        a2dp_sink.set_spp_active(false);
        a2dp_sink.set_output_active(false);
        a2dp_sink.set_rssi_active(false);

        a2dp_sink.set_on_connection_state_changed(btConnectionStateCallback, this);

        Serial.println("Bluetooth A2DP Sink configured. / 蓝牙 A2DP Sink 配置完成");

        // Create task for handling Bluetooth / 创建蓝牙处理任务
        xTaskCreate(bluetoothTask, "Bluetooth Task", 4096, this, 0, &bluetoothTaskHandle);
    }
}

void NetworkController::startBluetooth()
{
    if (btPaired)
    { // Only start if paired
        bluetoothActive = true;
    }
}

void NetworkController::stopBluetooth()
{
    bluetoothActive = false; // Stop Bluetooth activity / 停止蓝牙活动
}

void NetworkController::btConnectionStateCallback(esp_a2d_connection_state_t state, void *obj)
{
    auto *self = static_cast<NetworkController *>(obj);

    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED)
    {
        Serial.println("Bluetooth device connected. / 蓝牙设备已连接");

        // Save paired state only in provisioning mode / 仅在配网模式下记录配对状态
        if (self->provisioningMode)
        {
            self->saveBluetoothPairedState(true);
            self->btPaired = true;
            Serial.println("Paired state saved during provisioning. / 配网时已保存配对状态");
        }
    }
    else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
    {
        Serial.println("Bluetooth device disconnected. / 蓝牙设备已断开");
        // No need to set flags; task loop will handle reconnection logic based on is_connected() / 标志位由任务循环处理
    }
}

void NetworkController::saveBluetoothPairedState(bool paired)
{
    preferences.begin("network", false);
    preferences.putBool("bt_paired", paired);
    preferences.end();
    btPaired = paired;
    Serial.println("Bluetooth pairing state saved in NVS. / 蓝牙配对状态已写入 NVS");
}

void NetworkController::bluetoothTask(void *param)
{
    NetworkController *self = static_cast<NetworkController *>(param);

    while (true)
    {
        // If in provisioning mode, start Bluetooth only once / 配网模式仅启动一次蓝牙
        if (self->provisioningMode)
        {
            if (!self->bluetoothAttempted)
            {
                Serial.println("Starting Bluetooth for provisioning... / 配网模式启动蓝牙");
                self->a2dp_sink.start("Focus Dial", true);
                self->bluetoothAttempted = true; // Mark as attempted to prevent repeated starts / 避免重复启动
            }
        }
        else
        {
            // Normal operation mode / 正常工作模式
            if (self->bluetoothActive && !self->bluetoothAttempted)
            {
                Serial.println("Starting Bluetooth... / 启动蓝牙");
                self->a2dp_sink.start("Focus Dial", true); // Auto-reconnect enabled / 启用自动重连
                self->bluetoothAttempted = true;
                self->lastBluetoothtAttempt = millis(); // Record the time of the start attempt / 记录启动时间
            }

            // If Bluetooth is active but not connected, attempt reconnect every 2 seconds / 蓝牙应当激活但未连接时，每 2 秒重连
            if (self->bluetoothActive && !self->a2dp_sink.is_connected() && (millis() - self->lastBluetoothtAttempt >= 2000))
            {
                Serial.println("Attempting Bluetooth reconnect... / 尝试重新连接蓝牙");
                self->a2dp_sink.start("Focus Dial", true);
                self->lastBluetoothtAttempt = millis(); // Update last attempt time / 更新尝试时间
            }

            // If Bluetooth is not supposed to be active but is connected, disconnect / 蓝牙不应激活但已连接时断开
            if (!self->bluetoothActive && self->a2dp_sink.is_connected())
            {
                Serial.println("Stopping Bluetooth... / 停止蓝牙");
                self->a2dp_sink.disconnect();
                self->bluetoothAttempted = false; // Allow re-attempt later / 允许后续重新启动
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void NetworkController::sendWebhookAction(const String &action)
{
    DynamicJsonDocument doc(128);
    doc["action"] = action;

    String payload;
    serializeJson(doc, payload);
    sendWebhookPayload(payload);
}

void NetworkController::sendWebhookPayload(const String &payload)
{
    if (webhookQueue == nullptr)
    {
        webhookQueue = xQueueCreate(5, sizeof(char *));
    }

    char *payloadCopy = strdup(payload.c_str());
    if (payloadCopy == nullptr)
    {
        Serial.println("Failed to allocate memory for webhook payload. / 分配 webhook payload 内存失败");
        return;
    }

    if (xQueueSend(webhookQueue, &payloadCopy, 0) == pdPASS)
    {
        Serial.println("Webhook payload enqueued. / Webhook payload 已入队");
    }
    else
    {
        Serial.println("Failed to enqueue webhook payload: Queue is full. / 入队失败，队列已满");
        free(payloadCopy); // Free the memory if not enqueued / 入队失败时释放内存
    }
}

void NetworkController::webhookTask(void *param)
{
    NetworkController *self = static_cast<NetworkController *>(param);
    char *payload;

    while (true)
    {
        // Wait for a webhook action to arrive in the queue
        // 等待队列中出现新的 webhook 动作
        if (xQueueReceive(self->webhookQueue, &payload, portMAX_DELAY) == pdPASS)
        {
            Serial.println("Processing webhook payload... / 处理 webhook payload");

            // Send the webhook request and check the response
            // 发送 webhook 请求并检查结果
            bool success = self->sendWebhookRequest(String(payload));
            if (success)
            {
                Serial.println("Webhook payload sent successfully. / webhook 发送成功");
            }
            else
            {
                Serial.println("Failed to send webhook payload. / webhook 发送失败");
            }

            free(payload); // Free the allocated memory for payload / 释放 payload 文本内存

            Serial.println("Finished processing webhook payload. / 处理 webhook payload 完毕");
        }

        // Small delay to yield / 小延迟，释放 CPU
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

bool NetworkController::sendWebhookRequest(const String &payload)
{
    if (webhookURL.isEmpty())
    {
        Serial.println("Webhook URL is not set. Cannot send payload. / 未配置 webhook URL，无法发送");
        return false;
    }

    std::unique_ptr<WiFiClient> client;
    if (webhookURL.startsWith("https://"))
    {
        client.reset(new WiFiClientSecure());
        if (!client)
        {
            Serial.println("Memory allocation for WiFiClientSecure failed. / 分配 WiFiClientSecure 失败");
            return false;
        }
        static_cast<WiFiClientSecure *>(client.get())->setInsecure(); // Not verifying server certificate / 不校验证书
    }
    else
    {
        client.reset(new WiFiClient());
        if (!client)
        {
            Serial.println("Memory allocation for WiFiClient failed. / 分配 WiFiClient 失败");
            return false;
        }
    }

    HTTPClient http;
    bool result = false;

    if (http.begin(*client, webhookURL))
    {
        http.addHeader("Content-Type", "application/json");

        // Send the POST request / 发送 POST 请求
        int httpResponseCode = http.POST(payload);

        if (httpResponseCode > 0)
        {
            String response = http.getString();
            Serial.println("HTTP Response code: " + String(httpResponseCode) + " / HTTP 响应码");
            Serial.println("Response: " + response + " / 响应体");
            result = true;
        }
        else
        {
            Serial.println("Error in sending POST: " + String(httpResponseCode) + " / POST 发送失败");
        }

        http.end(); // Close the connection / 关闭连接
    }
    else
    {
        Serial.println("Unable to connect to server. / 无法连接服务器");
    }

    return result;
}

void NetworkController::WiFiProvisionerSettings()
{
    wifiProvisioner.enableSerialDebug(true);
    wifiProvisioner.AP_NAME = "Focus Dial";
    wifiProvisioner.SVG_LOGO =
        R"rawliteral(
        <svg width="297" height="135" viewBox="0 0 99 45" xmlns="http://www.w3.org/2000/svg" style="margin:1rem auto;">
            <g fill="currentColor">
                <path d="m54 15h3v3h-3z"/>
                <path d="m54 3h3v3h-3z"/>
                <path d="m60 9v3h-6v3h-3v6h-3v-6h-3v-3h-6v-3h6v-3h3v-6h3v6h3v3z"/>
                <path d="m42 3h3v3h-3z"/><path d="m42 15h3v3h-3z"/>
                <path d="m21 30v12h-3v-9h-3v-3z"/><path d="m18 42v3h-6v-12h3v9z"/>
                <path d="m84 33h3v12h-3z"/><path d="m48 33h3v3h6v6h-3v-3h-6z"/>
                <path d="m99 42v3h-9v-15h3v12z"/><path d="m27 42h6v3h-6z"/><path d="m36 30h3v12h-3z"/>
                <path d="m48 42h6v3h-6z"/><path d="m81 30h3v3h-3z"/><path d="m24 33h3v9h-3z"/><path d="m51 30h6v3h-6z"/>
                <path d="m39 42h3v3h-3z"/><path d="m0 33h3v3h6v3h-6v6h-3z"/><path d="m3 30h6v3h-6z"/><path d="m72 30h3v15h-3z"/>
                <path d="m42 30h3v12h-3z"/><path d="m66 33h3v9h-3z"/><path d="m78 33h3v12h-3z"/><path d="m63 42h3v3h-6v-15h6v3h-3z"/>
                <path d="m27 30h6v3h-6z"/>
            </g>
        </svg>
        <style> /* Override lib defaults */
            :root {
                --theme-color: #4caf50;    
                --font-color: #fff;
                --card-background: #171717;
                --black: #080808;
            }
            body {
                background-color: var(--black);
            }
            input {
                background-color: #2b2b2b;
            }
            .error input[type="text"],
            .error input[type="password"] {
                background-color: #3e0707;
            }
            input[type="text"]:disabled ,input[type="password"]:disabled ,input[type="radio"]:disabled {
                color:var(--black);
            }
        </style>)rawliteral";

    wifiProvisioner.HTML_TITLE = "Focus Dial - Provisioning / 配网";
    wifiProvisioner.PROJECT_TITLE = " Focus Dial — Setup / 安装配置";
    wifiProvisioner.PROJECT_INFO = R"rawliteral(
            1. Connect to Bluetooth if you want to use the phone automation trigger. / 如需使用手机端自动化触发，请先连接蓝牙。
            2. Select a WiFi network to save and allow Focus Dial to trigger webhook automations. / 选择并保存 WiFi，让 Focus Dial 能触发 webhook 自动化。
            3. Enter the webhook URL below to trigger it when a focus session starts. / 在下方输入 webhook URL，专注开始时将调用该地址。)rawliteral";

    wifiProvisioner.FOOTER_INFO = R"rawliteral(
        Focus Dial - Made by <a href="https://youtube.com/@salimbenbouz" target="_blank">Salim Benbouziyane</a><br/>专注计时器 - 作者：Salim Benbouziyane)rawliteral";

    wifiProvisioner.CONNECTION_SUCCESSFUL =
        "Provision Complete. Focus Dial will now start and status led will turn to blue. / 配网完成，设备将启动，状态灯将变为蓝色。";

    wifiProvisioner.RESET_CONFIRMATION_TEXT =
        "This will erase all settings and require re-provisioning. Confirm on the device. / 这将清除所有设置并需要重新配网，请在设备上确认。";

    wifiProvisioner.setShowInputField(true);
    wifiProvisioner.INPUT_TEXT = "Webhook URL to Trigger Automation: / 自动化的 Webhook URL";
    wifiProvisioner.INPUT_PLACEHOLDER = "e.g., https://example.com/webhook / 例如：https://example.com/webhook";
    wifiProvisioner.INPUT_INVALID_LENGTH = "The URL appears incomplete. Please enter the valid URL to trigger the automation. / URL 不完整，请输入有效的自动化地址。";
    wifiProvisioner.INPUT_NOT_VALID = "The URL entered is not valid. Please verify it and try again. / 输入的 URL 无效，请检查后重试。";

    // Set the static methods as callbacks / 设置静态回调
    wifiProvisioner.setInputCheckCallback(validateInputCallback);
    wifiProvisioner.setFactoryResetCallback(factoryResetCallback);
}

// Static method for input validation callback / 输入校验回调（静态包装）
bool NetworkController::validateInputCallback(const String &input)
{
    if (instance)
    {
        return instance->validateInput(input);
    }
    return false;
}

// Static method for factory reset callback / 恢复出厂设置回调（静态包装）
void NetworkController::factoryResetCallback()
{
    if (instance)
    {
        instance->handleFactoryReset();
    }
}

bool NetworkController::validateInput(const String &input)
{
    // Empty input is valid (webhook is optional) / 空输入视为有效（webhook 为可选项）
    if (input.isEmpty() || input.length() == 0)
    {
        Serial.println("Webhook URL is empty, skipping validation. / Webhook URL 为空，跳过验证");
        return true;
    }

    String modifiedInput = input;

    // Check if URL starts with "http://" or "https://" / 检查是否包含协议头
    if (!(modifiedInput.startsWith("http://") || modifiedInput.startsWith("https://")))
    {
        // If none supplied assume "http://"
        modifiedInput = "http://" + modifiedInput;
        Serial.println("Protocol missing, defaulting to http:// / 未填写协议，默认补为 http://");
    }

    // Basic validation / 基础校验
    int protocolEnd = modifiedInput.indexOf("://") + 3;
    int dotPosition = modifiedInput.indexOf('.', protocolEnd);

    bool isValid = (dotPosition != -1);

    Serial.print("Validating input: "); // 正在校验输入
    Serial.println(modifiedInput);

    // Save URL to NVS here if valid / 校验通过则写入 NVS
    if (isValid)
    {
        Serial.println("URL is valid. Saving to NVS... / URL 有效，写入 NVS");

        if (preferences.begin("focusdial", false))
        { // false means open for writing / false 表示写入模式
            preferences.putString("webhook_url", modifiedInput);
            preferences.end();
            webhookURL = modifiedInput;
            Serial.println("Webhook URL saved: " + webhookURL + " / 已保存 webhook 地址");
        }
        else
        {
            Serial.println("Failed to open NVS for writing. / 打开 NVS 进行写入失败");
        }
    }
    else
    {
        Serial.println("Invalid URL. Not saving to NVS. / URL 无效，未写入");
    }

    return isValid;
}

void NetworkController::handleFactoryReset()
{
    Serial.println("Factory reset initiated. / 已触发恢复出厂设置");
    reset();
}

// ========== HTTP API Server / HTTP API 服务器 ==========

void NetworkController::setTaskListUpdateCallback(std::function<void(const String&)> callback)
{
    onTaskListUpdate = callback;
}

void NetworkController::ensureApiServer()
{
    // 不在配网模式启用，避免与 WiFiProvisioner 的 Web 服务冲突 / Avoid conflicts with WiFiProvisioner server
    if (provisioningMode)
    {
        return;
    }

    if (!isWiFiProvisioned())
    {
        return;
    }

    if (!isWiFiConnected())
    {
        return;
    }

    if (apiServerStarted)
    {
        return;
    }

    setupApiServer();
}

void NetworkController::setupApiServer()
{
    if (apiServer != nullptr)
    {
        delete apiServer;
        apiServer = nullptr;
    }

    apiServer = new WebServer(80);

    apiServer->on("/api/tasklist", HTTP_POST, [this]() { handleAPITaskList(); });
    apiServer->on("/api/status", HTTP_GET, [this]() { handleAPIStatus(); });

    apiServer->begin();
    apiServerStarted = true;
    Serial.printf("API Server started on http://%s:80 / API 服务器已启动\n", WiFi.localIP().toString().c_str());
}

void NetworkController::handleAPITaskList()
{
    if (apiServer == nullptr)
    {
        return;
    }

    String body = apiServer->arg("plain");
    if (body.isEmpty())
    {
        apiServer->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    // 基础 JSON 校验（避免明显错误）/ Basic JSON validation
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, body);
    if (error)
    {
        apiServer->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }

    if (!doc["tasks"].is<JsonArray>())
    {
        apiServer->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing tasks\"}");
        return;
    }

    lastTaskListJson = body;

    if (onTaskListUpdate)
    {
        onTaskListUpdate(body);
    }

    apiServer->send(200, "application/json", "{\"status\":\"ok\"}");
}

void NetworkController::handleAPIStatus()
{
    if (apiServer == nullptr)
    {
        return;
    }

    DynamicJsonDocument doc(256);
    doc["wifi_connected"] = isWiFiConnected();
    doc["tasklist_loaded"] = !lastTaskListJson.isEmpty();

    String out;
    serializeJson(doc, out);
    apiServer->send(200, "application/json", out);
}
