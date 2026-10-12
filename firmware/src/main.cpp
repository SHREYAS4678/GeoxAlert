#define TINY_GSM_MODEM_SIM800

#include <Arduino.h>

#include <WiFi.h>

#include <WiFiClientSecure.h>

#include <HTTPClient.h>

#include <TinyGsmClient.h>

#include <DHT.h>

#include <Wire.h>

#include <I2Cdev.h>

#include <MPU6050.h>

#include <ArduinoJson.h>

#include <LittleFS.h>
#include <time.h>
#include <math.h>
#include "secrets.h"

// ============================================================

// CONFIGURATION

// ============================================================

// All credentials are supplied by firmware/include/secrets.h (ignored by Git).
const char* WIFI_SSID = GEOALERT_WIFI_SSID;
const char* WIFI_PASSWORD = GEOALERT_WIFI_PASSWORD;
const char* FIREBASE_API_KEY = GEOALERT_FIREBASE_API_KEY;
const char* FIREBASE_DB_URL = GEOALERT_FIREBASE_DB_URL;
const char* FIREBASE_EMAIL = GEOALERT_FIREBASE_EMAIL;
const char* FIREBASE_PASSWORD = GEOALERT_FIREBASE_PASSWORD;
const char* DEVICE_ID = GEOALERT_DEVICE_ID;
const char* GPRS_APN = GEOALERT_GPRS_APN;
const char* GPRS_USER = GEOALERT_GPRS_USER;
const char* GPRS_PASS = GEOALERT_GPRS_PASS;

// PINS

// ============================================================

#define PIN_SOIL 34

#define PIN_RAIN 35

#define PIN_DHT 4

#define PIN_VIB 5

#define PIN_SIM_RX 16

#define PIN_SIM_TX 17

#define I2C_SDA 21

#define I2C_SCL 22

#define DHT_TYPE DHT22

// ============================================================

// SOIL CALIBRATION

// ============================================================

#define SOIL_AIR_RAW 3500

#define SOIL_WATER_RAW 1300

// ============================================================

// TIMING

// ============================================================

#define SENSOR_INTERVAL 5000UL

#define WIFI_TIMEOUT 15000UL

#define GSM_TIMEOUT 20000UL

#define NETWORK_RETRY 30000UL

// ============================================================

// OBJECTS

// ============================================================

DHT dht(PIN_DHT, DHT_TYPE);

MPU6050 mpu;

HardwareSerial simSerial(2);

TinyGsm modem(simSerial);

TinyGsmClientSecure gsmClient(modem);

WiFiClientSecure wifiClient;

// ============================================================

// CONNECTION STATE

// ============================================================

enum ConnectionType {

    CONN_NONE,

    CONN_WIFI,

    CONN_GSM

};

ConnectionType activeConnection = CONN_NONE;

// ============================================================

// FIREBASE AUTH

// ============================================================

String idToken = "";

String refreshToken = "";

unsigned long tokenCreated = 0;

unsigned long tokenLife = 0;

// ============================================================

// SENSOR STATUS

// ============================================================

bool soilOK = false;

bool rainOK = false;

bool dhtOK = false;

bool mpuOK = false;

// ============================================================

// VIBRATION

// ============================================================

volatile uint32_t vibrationCount = 0;

void IRAM_ATTR vibrationISR() {

    vibrationCount++;

}

// ============================================================

// MPU VALUES

// ============================================================

float rollDeg = 0.0;

float pitchDeg = 0.0;

float yawDeg = 0.0;

unsigned long lastMPUTime = 0;

bool mpuStarted = false;

// ============================================================

// SYSTEM TIMING

// ============================================================

unsigned long lastSensorSend = 0;

unsigned long lastNetworkTry = 0;
unsigned long lastSmsQueuePoll = 0;
const unsigned long SMS_QUEUE_POLL_MS = 10000UL;

bool modemReadyForSMS = false;
// ============================================================

// LITTLEFS

// ============================================================

const char* SPOOL_FILE = "/spool.jsonl";

// ============================================================

// NETWORK NAME

// ============================================================

const char* networkName() {

    if (activeConnection == CONN_WIFI)

        return "WiFi";

    if (activeConnection == CONN_GSM)

        return "SIM800L";

    return "Offline";

}

// ============================================================

// WIFI

// ============================================================

void syncClockWiFi() {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    struct tm timeInfo;
    const unsigned long start = millis();
    while (millis() - start < 7000UL) {
        if (getLocalTime(&timeInfo, 500)) {
            time_t epochNow = time(nullptr);
            if (epochNow > 1700000000) {
                Serial.println("[TIME] UTC clock synchronized");
                return;
            }
        }
        delay(200);
    }
    Serial.println("[TIME] NTP unavailable; uptime timestamps will be marked");
}

bool connectWiFi() {

    Serial.println();

    Serial.println("[WiFi] Starting...");

    WiFi.mode(WIFI_STA);

    WiFi.disconnect(true);

    delay(500);

    WiFi.begin(

        WIFI_SSID,

        WIFI_PASSWORD

    );

    Serial.print("[WiFi] Connecting");

    unsigned long start = millis();

    while (

        WiFi.status() != WL_CONNECTED &&

        millis() - start < WIFI_TIMEOUT

    ) {

        delay(500);

        Serial.print(".");

    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {

        activeConnection = CONN_WIFI;
        syncClockWiFi();

        Serial.println("[WiFi] CONNECTED");

        Serial.print("[WiFi] IP: ");

        Serial.println(WiFi.localIP());

        Serial.print("[WiFi] RSSI: ");

        Serial.print(WiFi.RSSI());

        Serial.println(" dBm");

        return true;

    }

    Serial.print("[WiFi] FAILED. Status = ");

    Serial.println(WiFi.status());

    return false;

}

// ============================================================

// SIM800L

// ============================================================

bool connectGSM() {

    Serial.println();

    Serial.println("[GSM] Starting SIM800L...");

    simSerial.begin(

        9600,

        SERIAL_8N1,

        PIN_SIM_RX,

        PIN_SIM_TX

    );

    delay(1000);

    if (!modem.testAT()) {

        Serial.println(

            "[GSM] SIM800L not responding"

        );

        return false;

    }

    Serial.println("[GSM] Modem OK");

    if (!modem.init()) {

        Serial.println(

            "[GSM] Modem initialization failed"

        );

        return false;

    }

    Serial.print("[GSM] SIM status: ");

    Serial.println(modem.getSimStatus());

    if (modem.getSimStatus() != 1) {

        Serial.println(

            "[GSM] SIM is not ready"

        );

        return false;

    }

    Serial.print("[GSM] Signal: ");

    Serial.println(modem.getSignalQuality());

    Serial.println(

        "[GSM] Waiting for network..."

    );

    if (!modem.waitForNetwork(GSM_TIMEOUT)) {

        Serial.println(

            "[GSM] Network registration FAILED"

        );

        return false;

    }

    Serial.println(

        "[GSM] Network registered"

    );

    Serial.println(

        "[GSM] Connecting GPRS..."

    );

    if (!modem.gprsConnect(

            GPRS_APN,

            GPRS_USER,

            GPRS_PASS

        )) {

        Serial.println(

            "[GSM] GPRS connection FAILED"

        );

        return false;

    }

    if (!modem.isGprsConnected()) {

        Serial.println(

            "[GSM] GPRS not connected"

        );

        return false;

    }

    activeConnection = CONN_GSM;

    Serial.println(

        "[GSM] GPRS CONNECTED"

    );

    Serial.print("[GSM] IP: ");

    Serial.println(modem.getLocalIP());

    return true;

}

// ============================================================

// FIREBASE WIFI LOGIN

// ============================================================

bool firebaseLoginWiFi() {

    Serial.println(

        "[Firebase] WiFi authentication..."

    );

    HTTPClient http;

    String url =

        String(

            "https://identitytoolkit.googleapis.com/v1/"

            "accounts:signInWithPassword?key="

        )

        + FIREBASE_API_KEY;

    if (!http.begin(

            wifiClient,

            url

        )) {

        Serial.println(

            "[Firebase] HTTP begin failed"

        );

        return false;

    }

    http.addHeader(

        "Content-Type",

        "application/json"

    );

    JsonDocument request;

    request["email"] =

        FIREBASE_EMAIL;

    request["password"] =

        FIREBASE_PASSWORD;

    request["returnSecureToken"] =

        true;

    String body;

    serializeJson(

        request,

        body

    );

    int code = http.POST(body);

    if (code != 200) {

        Serial.print(

            "[Firebase] Login failed: "

        );

        Serial.println(code);

        Serial.println(

            http.getString()

        );

        http.end();

        return false;

    }

    String response =

        http.getString();

    JsonDocument json;

    DeserializationError error =

        deserializeJson(

            json,

            response

        );

    if (error) {

        Serial.println(

            "[Firebase] JSON error"

        );

        http.end();

        return false;

    }

    idToken =

        json["idToken"].as<String>();

    refreshToken =

        json["refreshToken"].as<String>();

    unsigned long expires =

        json["expiresIn"].as<unsigned long>();

    tokenLife =

        expires * 1000UL;

    tokenCreated =

        millis();

    http.end();

    Serial.println(

        "[Firebase] WiFi authentication OK"

    );

    return idToken.length() > 0;

}

// ============================================================

// SIM800L HTTPS REQUEST

// ============================================================

bool gsmHTTPSRequest(

    const String& host,

    const String& path,

    const String& payload,

    const char* method,

    String& response

) {

    Serial.println(

        "[GSM] Opening HTTPS..."

    );

    if (!gsmClient.connect(

            host.c_str(),

            443

        )) {

        Serial.println(

            "[GSM] HTTPS connection failed"

        );

        return false;

    }

    String request;

    request += method;

    request += " ";

    request += path;

    request += " HTTP/1.1\r\n";

    request += "Host: ";

    request += host;

    request += "\r\n";

    request +=

        "Content-Type: application/json\r\n";

    request +=

        "Content-Length: ";

    request +=

        String(payload.length());

    request += "\r\n";

    request +=

        "Connection: close\r\n\r\n";

    request += payload;

    gsmClient.print(request);

    unsigned long start =

        millis();

    while (!gsmClient.available()) {

        if (

            millis() - start >

            20000

        ) {

            Serial.println(

                "[GSM] HTTPS timeout"

            );

            gsmClient.stop();

            return false;

        }

        delay(10);

    }

    response = "";

    while (gsmClient.available()) {

        response +=

            gsmClient.readString();

        delay(1);

    }

    gsmClient.stop();

    Serial.println(

        "[GSM] HTTPS response received"

    );

    return (

        response.indexOf("200 OK") >= 0 ||

        response.indexOf("201 Created") >= 0

    );

}

// ============================================================

// SIM800L FIREBASE LOGIN

// ============================================================

bool firebaseLoginGSM() {

    Serial.println(

        "[Firebase] SIM800L authentication..."

    );

    String path =

        String(

            "/v1/accounts:signInWithPassword?key="

        )

        + FIREBASE_API_KEY;

    JsonDocument request;

    request["email"] =

        FIREBASE_EMAIL;

    request["password"] =

        FIREBASE_PASSWORD;

    request["returnSecureToken"] =

        true;

    String body;

    serializeJson(

        request,

        body

    );

    String response;

    if (!gsmHTTPSRequest(

            "identitytoolkit.googleapis.com",

            path,

            body,

            "POST",

            response

        )) {

        Serial.println(

            "[Firebase] GSM authentication failed"

        );

        return false;

    }

    int bodyStart =

        response.indexOf(

            "\r\n\r\n"

        );

    if (bodyStart < 0)

        return false;

    String jsonBody =

        response.substring(

            bodyStart + 4

        );

    JsonDocument json;

    if (

        deserializeJson(

            json,

            jsonBody

        )

    ) {

        Serial.println(

            "[Firebase] GSM JSON error"

        );

        return false;

    }

    idToken =

        json["idToken"].as<String>();

    refreshToken =

        json["refreshToken"].as<String>();

    unsigned long expires =

        json["expiresIn"].as<unsigned long>();

    tokenLife =

        expires * 1000UL;

    tokenCreated =

        millis();

    Serial.println(

        "[Firebase] GSM authentication OK"

    );

    return idToken.length() > 0;

}

// ============================================================

// WIFI FIREBASE UPLOAD

// ============================================================

bool firebaseWiFiUpload(

    const String& path,

    const String& payload,

    bool putRequest

) {

    if (idToken.length() == 0) {

        if (!firebaseLoginWiFi())

            return false;

    }

    HTTPClient http;

    String url =

        String(FIREBASE_DB_URL)

        + path

        + ".json?auth="

        + idToken;

    if (!http.begin(

            wifiClient,

            url

        )) {

        return false;

    }

    http.addHeader(

        "Content-Type",

        "application/json"

    );

    int code;

    if (putRequest)

        code = http.PUT(payload);

    else

        code = http.POST(payload);

    Serial.print(

        "[Firebase WiFi] HTTP: "

    );

    Serial.println(code);

    http.end();

    return (

        code >= 200 &&

        code < 300

    );

}

// ============================================================

// GSM FIREBASE UPLOAD

// ============================================================

bool firebaseGSMUpload(

    const String& path,

    const String& payload,

    bool putRequest

) {

    if (idToken.length() == 0) {

        if (!firebaseLoginGSM())

            return false;

    }

    String fullPath =

        path

        + ".json?auth="

        + idToken;

    String response;

    const char* method =

        putRequest

        ? "PUT"

        : "POST";

    bool ok =

        gsmHTTPSRequest(

            "geoalert-ai-ee1fd-default-rtdb.asia-southeast1.firebasedatabase.app",

            fullPath,

            payload,

            method,

            response

        );

    if (ok) {

        Serial.println(

            "[Firebase GSM] Upload OK"

        );

    }

    else {

        Serial.println(

            "[Firebase GSM] Upload FAILED"

        );

    }

    return ok;

}

// ============================================================
// FIREBASE QUEUE READ / WRITE HELPERS
// ============================================================

bool firebaseWiFiRead(const String& pathWithQuery, String& response) {
    const bool expired = tokenLife > 60000UL && (millis() - tokenCreated >= tokenLife - 60000UL);
    if (idToken.length() == 0 || expired) {
        idToken = "";
        if (!firebaseLoginWiFi()) return false;
    }
    String url = String(FIREBASE_DB_URL) + pathWithQuery;
    url += (url.indexOf('?') >= 0) ? "&auth=" : "?auth=";
    url += idToken;
    HTTPClient http;
    if (!http.begin(wifiClient, url)) {
        Serial.println("[Firebase WiFi] GET begin failed");
        return false;
    }
    const int code = http.GET();
    response = http.getString();
    http.end();
    Serial.print("[Firebase WiFi] GET HTTP: ");
    Serial.println(code);
    return code >= 200 && code < 300;
}

bool firebaseGSMRead(const String& pathWithQuery, String& response) {
    const bool expired = tokenLife > 60000UL && (millis() - tokenCreated >= tokenLife - 60000UL);
    if (idToken.length() == 0 || expired) {
        idToken = "";
        if (!firebaseLoginGSM()) return false;
    }
    String fullPath = pathWithQuery;
    fullPath += (fullPath.indexOf('?') >= 0) ? "&auth=" : "?auth=";
    fullPath += idToken;
    String rawResponse;
    if (!gsmHTTPSRequest(
            "geoalert-ai-ee1fd-default-rtdb.asia-southeast1.firebasedatabase.app",
            fullPath, "", "GET", rawResponse)) return false;
    const int bodyStart = rawResponse.indexOf("\r\n\r\n");
    if (bodyStart < 0) return false;
    response = rawResponse.substring(bodyStart + 4);
    return true;
}

bool firebaseReadActive(const String& pathWithQuery, String& response) {
    if (activeConnection == CONN_WIFI && WiFi.status() == WL_CONNECTED)
        return firebaseWiFiRead(pathWithQuery, response);
    if (activeConnection == CONN_GSM && modem.isGprsConnected())
        return firebaseGSMRead(pathWithQuery, response);
    return false;
}

bool firebasePutActive(const String& path, const String& payload) {
    if (activeConnection == CONN_WIFI && WiFi.status() == WL_CONNECTED)
        return firebaseWiFiUpload(path, payload, true);
    if (activeConnection == CONN_GSM && modem.isGprsConnected())
        return firebaseGSMUpload(path, payload, true);
    return false;
}


// ============================================================

bool ensureNetwork() {

    if (

        WiFi.status() ==

        WL_CONNECTED

    ) {

        activeConnection =

            CONN_WIFI;

        return true;

    }

    if (

        activeConnection ==

        CONN_GSM &&

        modem.isGprsConnected()

    ) {

        return true;

    }

    if (

        millis() -

        lastNetworkTry <

        NETWORK_RETRY

    ) {

        return false;

    }

    lastNetworkTry =

        millis();

    Serial.println(

        "[NETWORK] Trying WiFi..."

    );

    if (connectWiFi())

        return true;

    Serial.println(

        "[NETWORK] WiFi failed"

    );

    Serial.println(

        "[NETWORK] Trying SIM800L..."

    );

    if (connectGSM())

        return true;

    Serial.println(

        "[NETWORK] Both networks failed"

    );

    activeConnection =

        CONN_NONE;

    return false;

}

// ============================================================

// MPU6050

// ============================================================

void setupMPU() {

    Wire.begin(I2C_SDA, I2C_SCL);

    Wire.setClock(400000);

    Serial.println("[MPU] Starting...");

    // ElectronicCats MPU6050 library.

    // This matches the standalone test that successfully

    // returned accelerometer and gyro values from your sensor.

    mpu.initialize();

    delay(100);

    // Keep the library defaults so the conversion below matches

    // the standalone test: +/-2g and +/-250 deg/s.

    mpuStarted = true;

    mpuOK = true;

    Serial.println("[MPU] Sensor initialized");

}

// ============================================================

// READ MPU

// ============================================================

void readMPU() {

    if (!mpuStarted) {

        mpuOK = false;

        return;

    }

    int16_t ax, ay, az;

    int16_t gx, gy, gz;

    // ElectronicCats MPU6050 library.

    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    // Library defaults: +/-2g = 16384 LSB/g.

    float axg = ax / 16384.0f;

    float ayg = ay / 16384.0f;

    float azg = az / 16384.0f;

    // Library defaults: +/-250 deg/s = 131 LSB/(deg/s).

    float gyroX = gx / 131.0f;

    float gyroY = gy / 131.0f;

    float gyroZ = gz / 131.0f;

    // Accelerometer roll and pitch.

    float accelRoll =

        atan2(ayg, sqrt(axg * axg + azg * azg))

        * 180.0f / PI;

    float accelPitch =

        atan2(-axg, sqrt(ayg * ayg + azg * azg))

        * 180.0f / PI;

    unsigned long now = millis();

    float dt = 0.0f;

    if (lastMPUTime != 0) {

        dt = (now - lastMPUTime) / 1000.0f;

    }

    lastMPUTime = now;

    if (dt > 0.0f && dt < 1.0f) {

        // Complementary filter for roll/pitch.

        rollDeg =

            0.98f * (rollDeg + gyroX * dt) +

            0.02f * accelRoll;

        pitchDeg =

            0.98f * (pitchDeg + gyroY * dt) +

            0.02f * accelPitch;

        // Relative yaw from gyro Z integration.

        // No magnetometer is present, so yaw will drift.

        yawDeg += gyroZ * dt;

    } else {

        rollDeg = accelRoll;

        pitchDeg = accelPitch;

    }

    while (yawDeg > 180.0f) yawDeg -= 360.0f;

    while (yawDeg < -180.0f) yawDeg += 360.0f;

    mpuOK = true;

    Serial.println();

    Serial.println("----- MPU6050 -----");

    Serial.print("X (Roll):  ");

    Serial.print(rollDeg, 2);

    Serial.println(" deg");

    Serial.print("Y (Pitch): ");

    Serial.print(pitchDeg, 2);

    Serial.println(" deg");

    Serial.print("Z (Yaw):   ");

    Serial.print(yawDeg, 2);

    Serial.println(" deg");

    Serial.print("Raw Gyro: X=");

    Serial.print(gx);

    Serial.print(" Y=");

    Serial.print(gy);

    Serial.print(" Z=");

    Serial.println(gz);

    Serial.println("-------------------");

}

// ============================================================

// ADC AVERAGE

// ============================================================

int readAverageADC(

    int pin

) {

    long total = 0;

    for (

        int i = 0;

        i < 16;

        i++

    ) {

        total +=

            analogRead(pin);

        delay(2);

    }

    return total / 16;

}

// ============================================================

// CREATE TELEMETRY

// ============================================================

String createTelemetry() {

    int soilRaw =

        readAverageADC(

            PIN_SOIL

        );

    int rainRaw =

        readAverageADC(

            PIN_RAIN

        );

    // --------------------------------------------------------

    // Soil

    // --------------------------------------------------------

    float moisture =

        (

            (float)(

                SOIL_AIR_RAW -

                soilRaw

            )

            /

            (float)(

                SOIL_AIR_RAW -

                SOIL_WATER_RAW

            )

        )

        * 100.0;

    moisture =

        constrain(

            moisture,

            0.0,

            100.0

        );

    soilOK = true;

    // --------------------------------------------------------

    // Rain

    // --------------------------------------------------------

    float rainfall =

        (

            (float)(

                4095 -

                rainRaw

            )

            /

            4095.0

        )

        * 100.0;

    rainfall =

        constrain(

            rainfall,

            0.0,

            100.0

        );

    rainOK = true;

    // --------------------------------------------------------

    // DHT22

    // --------------------------------------------------------

    float temperature =

        dht.readTemperature();

    float humidity =

        dht.readHumidity();

    if (

        isnan(temperature) ||

        isnan(humidity)

    ) {

        dhtOK = false;

        Serial.println(

            "[DHT] Reading failed"

        );

        temperature = 25.0;

        humidity = 50.0;

    }

    else {

        dhtOK = true;

    }

    // --------------------------------------------------------

    // MPU

    // --------------------------------------------------------

    readMPU();

    // --------------------------------------------------------

    // Vibration

    // --------------------------------------------------------

    noInterrupts();

    uint32_t vibration =

        vibrationCount;

    vibrationCount = 0;

    interrupts();

    // --------------------------------------------------------

    // JSON

    // --------------------------------------------------------

    JsonDocument json;

    json["device_id"] =

        DEVICE_ID;

    time_t epochNow = time(nullptr);
    if (epochNow > 1700000000) {
        json["timestamp"] = static_cast<uint64_t>(epochNow) * 1000ULL;
        json["timestamp_format"] = "unix_ms";
    } else {
        json["timestamp"] = millis();
        json["timestamp_is_uptime"] = true;
    }

    json["network_status"]

        ["active_interface"] =

            networkName();

    JsonObject sensors =

        json["sensor_readings"]

            .to<JsonObject>();

    sensors["soil_moisture_pct"] =

        moisture;

    sensors["rainfall_pct"] =

        rainfall;

    sensors["temperature_c"] =

        temperature;

    sensors["humidity_pct"] =

        humidity;

    sensors["roll_deg"] =

        rollDeg;

    sensors["pitch_deg"] =

        pitchDeg;

    sensors["yaw_deg"] = yawDeg;

    // Combined slope inclination: use both MPU6050 pitch and roll.
    const float pitchRad = pitchDeg * PI / 180.0f;
    const float rollRad = rollDeg * PI / 180.0f;
    const float tiltCos = constrain(cosf(pitchRad) * cosf(rollRad), -1.0f, 1.0f);
    const float slopeTiltDeg = acosf(tiltCos) * 180.0f / PI;
    sensors["tilt_angle_deg"] = slopeTiltDeg;
    sensors["slope_tilt_deg"] = slopeTiltDeg;

    sensors["vibration_pulses"] =

        vibration;

    JsonObject health =

        json["health"]

            .to<JsonObject>();

    health["soil"] =

        soilOK;

    health["rain"] =

        rainOK;

    health["dht"] =

        dhtOK;

    health["mpu"] =

        mpuOK;

    String payload;

    serializeJson(

        json,

        payload

    );

    return payload;

}

// ============================================================

// LITTLEFS SAVE

// ============================================================

bool saveToLittleFS(

    const String& payload

) {

    File file =

        LittleFS.open(

            SPOOL_FILE,

            FILE_APPEND

        );

    if (!file) {

        Serial.println(

            "[LittleFS] Open failed"

        );

        return false;

    }

    file.println(

        payload

    );

    file.close();

    Serial.println(

        "[LittleFS] Payload stored"

    );

    return true;

}

// ============================================================

// LIVE UPLOAD

// ============================================================

bool uploadLive(

    const String& payload

) {

    String path =

        String("/nodes/")

        + DEVICE_ID

        + "/live";

    if (

        activeConnection ==

        CONN_WIFI

    ) {

        return firebaseWiFiUpload(

            path,

            payload,

            true

        );

    }

    if (

        activeConnection ==

        CONN_GSM

    ) {

        return firebaseGSMUpload(

            path,

            payload,

            true

        );

    }

    return false;

}

// ============================================================

// HISTORY UPLOAD

// ============================================================

bool uploadHistory(

    const String& payload

) {

    String path =

        String("/nodes/")

        + DEVICE_ID

        + "/history";

    if (

        activeConnection ==

        CONN_WIFI

    ) {

        return firebaseWiFiUpload(

            path,

            payload,

            false

        );

    }

    if (

        activeConnection ==

        CONN_GSM

    ) {

        return firebaseGSMUpload(

            path,

            payload,

            false

        );

    }

    return false;

}

// ============================================================

// LITTLEFS REPLAY

// ============================================================

void replayLittleFS() {

    if (

        !LittleFS.exists(

            SPOOL_FILE

        )

    )

        return;

    Serial.println();

    Serial.println(

        "[LittleFS] Replaying offline data..."

    );

    File input =

        LittleFS.open(

            SPOOL_FILE,

            FILE_READ

        );

    if (!input)

        return;

    File output =

        LittleFS.open(

            "/spool_tmp.jsonl",

            FILE_WRITE

        );

    if (!output) {

        input.close();

        return;

    }

    bool failed = false;

    while (

        input.available()

    ) {

        String line =

            input.readStringUntil(

                '\n'

            );

        line.trim();

        if (

            line.length() == 0

        )

            continue;

        if (!failed) {

            if (

                uploadHistory(

                    line

                )

            ) {

                Serial.println(

                    "[LittleFS] Replay OK"

                );

            }

            else {

                Serial.println(

                    "[LittleFS] Replay failed"

                );

                failed = true;

                output.println(

                    line

                );

            }

        }

        else {

            output.println(

                line

            );

        }

    }

    input.close();

    output.close();

    LittleFS.remove(

        SPOOL_FILE

    );

    if (

        LittleFS.exists(

            "/spool_tmp.jsonl"

        )

    ) {

        LittleFS.rename(

            "/spool_tmp.jsonl",

            SPOOL_FILE

        );

    }

    if (

        !LittleFS.exists(

            SPOOL_FILE

        )

    ) {

        Serial.println(

            "[LittleFS] Queue empty"

        );

    }

}

// OPTIONAL DIRECT SIM800L SMS ALERTS
bool ensureModemReadyForSMS() {
    if (modemReadyForSMS && modem.isNetworkConnected()) return true;
    Serial.println("[SMS] Preparing SIM800L for SMS...");
    simSerial.begin(9600, SERIAL_8N1, PIN_SIM_RX, PIN_SIM_TX);
    delay(300);
    // Do not reinitialize a responding modem: that could disrupt GPRS/Firebase.
    if (!modem.testAT()) {
        if (!modem.init()) {
            Serial.println("[SMS] Modem initialization failed.");
            return false;
        }
    }
    if (modem.getSimStatus() != 1 || !modem.waitForNetwork(GSM_TIMEOUT)) {
        Serial.println("[SMS] SIM or cellular network is not ready.");
        return false;
    }
    modemReadyForSMS = true;
    return true;
}


// ============================================================
// FIREBASE-QUEUED MULTI-RECIPIENT SMS
// ============================================================

void processFirebaseSmsQueue() {
    if (activeConnection == CONN_NONE) return;
    const String queuePath = String("/nodes/") + DEVICE_ID
        + "/alerts/queue.json?orderBy=%22%24key%22&limitToLast=20";
    String response;
    if (!firebaseReadActive(queuePath, response)) {
        Serial.println("[SMS Queue] Could not read Firebase queue");
        return;
    }

    JsonDocument queueDoc;
    if (deserializeJson(queueDoc, response)) {
        Serial.println("[SMS Queue] Queue JSON parse failed");
        return;
    }
    JsonObject queue = queueDoc.as<JsonObject>();
    if (queue.isNull()) return;

    for (JsonPair entry : queue) {
        JsonObject item = entry.value().as<JsonObject>();
        if (item.isNull()) continue;
        const String status = item["status"] | "";
        if (status != "queued") continue;

        const String queueId = entry.key().c_str();
        const String itemPath = String("/nodes/") + DEVICE_ID + "/alerts/queue/" + queueId;
        const String message = item["message"] | "";
        JsonArray recipients = item["recipients"].as<JsonArray>();
        if (message.length() == 0 || recipients.isNull() || recipients.size() == 0) {
            item["status"] = "failed";
            item["error"] = "Missing message or recipients; no SMS sent.";
            item["device_completed_ms"] = millis();
            String invalidPayload;
            serializeJson(item, invalidPayload);
            firebasePutActive(itemPath, invalidPayload);
            Serial.println("[SMS Queue] Invalid item marked failed");
            return;
        }

        // Leave jobs queued if SMS service is temporarily unavailable.
        if (!ensureModemReadyForSMS()) {
            Serial.println("[SMS Queue] SIM800L not ready; leaving item queued");
            return;
        }

        // Claim before sending to reduce duplicate dispatch on the next poll.
        item["status"] = "processing";
        item["device_started_ms"] = millis();
        item["device_id"] = DEVICE_ID;
        String claimedPayload;
        serializeJson(item, claimedPayload);
        if (!firebasePutActive(itemPath, claimedPayload)) {
            Serial.println("[SMS Queue] Could not claim item; no SMS sent");
            return;
        }

        uint16_t sentCount = 0;
        uint16_t failedCount = 0;
        for (JsonVariant recipientValue : recipients) {
            JsonObject recipient = recipientValue.as<JsonObject>();
            if (recipient.isNull()) {
                failedCount++;
                continue;
            }
            const String recipientStatus = recipient["status"] | "queued";
            if (recipientStatus == "sent") { sentCount++; continue; }
            if (recipientStatus == "failed") { failedCount++; continue; }

            const String phone = recipient["phone"] | "";
            const String recipientName = recipient["name"] | "recipient";
            const bool validPhone = phone.length() >= 9 && phone.startsWith("+");
            const bool sent = validPhone && modem.sendSMS(phone, message);
            recipient["status"] = sent ? "sent" : "failed";
            recipient["device_result_ms"] = millis();
            if (sent) {
                sentCount++;
                Serial.println(String("[SMS Queue] SMS accepted for ") + recipientName);
            } else {
                failedCount++;
                recipient["error"] = "SIM800L command failed or phone format invalid.";
                Serial.println(String("[SMS Queue] SMS failed for ") + recipientName);
            }

            String progressPayload;
            serializeJson(item, progressPayload);
            if (!firebasePutActive(itemPath, progressPayload)) {
                Serial.println("[SMS Queue] Recipient result write failed; item remains processing");
                return;
            }
            delay(1200);
        }

        item["sent_count"] = sentCount;
        item["failed_count"] = failedCount;
        item["device_completed_ms"] = millis();
        if (failedCount == 0 && sentCount > 0) item["status"] = "sent";
        else if (sentCount > 0) item["status"] = "partial";
        else item["status"] = "failed";

        String finalPayload;
        serializeJson(item, finalPayload);
        const bool saved = firebasePutActive(itemPath, finalPayload);
        Serial.println(saved
            ? String("[SMS Queue] Completed ") + queueId + " sent=" + sentCount + " failed=" + failedCount
            : String("[SMS Queue] Final status write failed for ") + queueId);
        return;
    }
}

// ============================================================

// SEND TELEMETRY

// ============================================================

void sendTelemetry() {

    String payload =

        createTelemetry();

    Serial.println();

    Serial.println(

        "===== TELEMETRY ====="

    );

    Serial.println(

        payload

    );

    Serial.print(

        "Network: "

    );

    Serial.println(

        networkName()

    );

    if (!ensureNetwork()) {

        Serial.println(

            "[NETWORK] BOTH OFFLINE"

        );

        saveToLittleFS(

            payload

        );

        return;

    }

    // --------------------------------------------------------

    // Live

    // --------------------------------------------------------

    if (

        uploadLive(

            payload

        )

    ) {

        Serial.println(

            "[Firebase] Live upload OK"

        );

    }

    else {

        Serial.println(

            "[Firebase] Live upload FAILED"

        );

    }

    // --------------------------------------------------------

    // History

    // --------------------------------------------------------

    if (

        uploadHistory(

            payload

        )

    ) {

        Serial.println(

            "[Firebase] History upload OK"

        );

    }

    else {

        Serial.println(

            "[Firebase] History upload FAILED"

        );

        saveToLittleFS(

            payload

        );

    }

}
// SETUP

// ============================================================

void setup() {

    Serial.begin(

        115200

    );

    delay(1500);

    Serial.println();

    Serial.println(

        "================================"

    );

    Serial.println(

        "       GEOALERT ESP32"

    );

    Serial.println(

        "================================"

    );

    // --------------------------------------------------------

    // ADC

    // --------------------------------------------------------

    analogReadResolution(12);

    analogSetPinAttenuation(

        PIN_SOIL,

        ADC_11db

    );

    analogSetPinAttenuation(

        PIN_RAIN,

        ADC_11db

    );

    // --------------------------------------------------------

    // Vibration

    // --------------------------------------------------------

    pinMode(

        PIN_VIB,

        INPUT

    );

    attachInterrupt(

        digitalPinToInterrupt(

            PIN_VIB

        ),

        vibrationISR,

        RISING

    );

    // --------------------------------------------------------

    // DHT

    // --------------------------------------------------------

    dht.begin();

    // --------------------------------------------------------

    // MPU

    // --------------------------------------------------------

    setupMPU();

    // --------------------------------------------------------

    // LittleFS

    // --------------------------------------------------------

    if (

        !LittleFS.begin(true)

    ) {

        Serial.println(

            "[LittleFS] Mount FAILED"

        );

    }

    else {

        Serial.println(

            "[LittleFS] Mounted"

        );

    }

    // --------------------------------------------------------

    // WiFi TLS

    // --------------------------------------------------------

    wifiClient.setInsecure();

    // IMPORTANT:

    // Do NOT call gsmClient.setInsecure().

    // Your TinyGSM 0.12.0 SIM800 secure client

    // does not provide that function.

    // --------------------------------------------------------

    // --------------------------------------------------------

    // PRIMARY NETWORK

    // --------------------------------------------------------

    Serial.println();

    Serial.println(

        "[NETWORK] Trying WiFi..."

    );

    if (

        connectWiFi()

    ) {

        firebaseLoginWiFi();

    }

    else {

        Serial.println(

            "[NETWORK] WiFi failed"

        );

        Serial.println(

            "[NETWORK] Trying SIM800L..."

        );

        if (

            connectGSM()

        ) {

            firebaseLoginGSM();

        }

        else {

            Serial.println(

                "[NETWORK] BOTH FAILED"

            );

            activeConnection =

                CONN_NONE;

        }

    }

    // --------------------------------------------------------

    // Offline replay

    // --------------------------------------------------------

    if (

        activeConnection !=

        CONN_NONE

    ) {

        replayLittleFS();

    }

    lastSensorSend =

        millis();

    Serial.println();

    Serial.println(

        "[SYSTEM] GeoAlert started"

    );

}

// ============================================================

// LOOP

// ============================================================

void loop() {
    if (millis() - lastSmsQueuePoll >= SMS_QUEUE_POLL_MS) {
        lastSmsQueuePoll = millis();
        if (ensureNetwork()) processFirebaseSmsQueue();
    }


    // --------------------------------------------------------

    // Send sensors every 5 seconds

    // --------------------------------------------------------

    if (

        millis() -

        lastSensorSend >=

        SENSOR_INTERVAL

    ) {

        lastSensorSend =

            millis();

        sendTelemetry();

    }

    // --------------------------------------------------------

    // WiFi restored while GSM active

    // --------------------------------------------------------

    if (

        activeConnection ==

            CONN_GSM &&

        WiFi.status() ==

            WL_CONNECTED

    ) {

        Serial.println();

        Serial.println(

            "[NETWORK] WiFi restored"

        );

        activeConnection =

            CONN_WIFI;

        syncClockWiFi();

        idToken = "";

        firebaseLoginWiFi();

        replayLittleFS();

    }

    // --------------------------------------------------------

    // WiFi lost

    // --------------------------------------------------------

    if (

        activeConnection ==

            CONN_WIFI &&

        WiFi.status() !=

            WL_CONNECTED

    ) {

        Serial.println();

        Serial.println(

            "[NETWORK] WiFi disconnected"

        );

        activeConnection =

            CONN_NONE;

        idToken = "";

        if (

            connectGSM()

        ) {

            firebaseLoginGSM();

            replayLittleFS();

        }

        else {

            Serial.println(

                "[NETWORK] GSM unavailable"

            );

        }

    }

    // --------------------------------------------------------

    // Both offline

    // --------------------------------------------------------

    if (

        activeConnection ==

        CONN_NONE

    ) {

        if (

            millis() -

            lastNetworkTry >=

            NETWORK_RETRY

        ) {

            lastNetworkTry =

                millis();

            Serial.println();

            Serial.println(

                "[NETWORK] Recovery attempt..."

            );

            if (

                connectWiFi()

            ) {

                idToken = "";

                firebaseLoginWiFi();

                replayLittleFS();

            }

            else if (

                connectGSM()

            ) {

                idToken = "";

                firebaseLoginGSM();

                replayLittleFS();

            }

        }

    }

    delay(20);

}