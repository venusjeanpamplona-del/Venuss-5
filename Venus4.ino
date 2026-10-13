#define ENABLE_USER_AUTH
#define ENABLE_DATABASE


#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include <DHT.h>
#include <time.h>


/* =====================================================
   DHT11
   ===================================================== */

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(
    DHTPIN,
    DHTTYPE
);


/* =====================================================
   FIREBASE
   VENUS
   ===================================================== */

#define API_KEY \
"AIzaSyD69UthfpO3blXr-bkYJ4f-vK9CiZDSsR8"

#define DATABASE_URL \
"https://venusjean-253b5-default-rtdb.europe-west1.firebasedatabase.app"


/*
   IMPORTANT:

   Use the Firebase Authentication
   email belonging to Venus.

   Do NOT send the password in chat.
*/

#define USER_EMAIL \
"venusjeanpamplona@gmail.com"

#define USER_PASSWORD \
"Benus0987"


/* =====================================================
   FIREBASE OBJECTS
   ===================================================== */

UserAuth user_auth(
    API_KEY,
    USER_EMAIL,
    USER_PASSWORD,
    3000
);


FirebaseApp app;

WiFiClientSecure ssl_client;

AsyncClientClass aClient(
    ssl_client
);

RealtimeDatabase Database;


/* =====================================================
   WEB SERVER
   ===================================================== */

AsyncWebServer server(80);


/* =====================================================
   WIFI MANAGER
   ===================================================== */

const char *AP_SSID =
    "VENUS-ESP32-MANAGER";

bool wifiManagerMode =
    false;


/* =====================================================
   SENSOR TIMER
   ===================================================== */

unsigned long lastSensorRead =
    0;

const unsigned long SENSOR_INTERVAL =
    10000;


/* =====================================================
   FUNCTION DECLARATIONS
   ===================================================== */

void processFirebase(
    AsyncResult &aResult
);

bool connectToSavedWiFi();

void startWiFiManager();

void startMainWebServer();

void setupFirebase();

void sendSensorData();

String readFile(
    const char *path
);

bool writeFile(
    const char *path,
    const String &data
);

void deleteWiFiFiles();

String getDateString();

String getTimeString();


/* =====================================================
   VENUS WIFI MANAGER HTML
   ===================================================== */

const char WIFI_MANAGER_HTML[] PROGMEM =
R"rawliteral(

<!DOCTYPE html>

<html lang="en">

<head>

<meta charset="UTF-8">

<meta
    name="viewport"
    content="width=device-width, initial-scale=1.0"
>

<title>
Venus | WiFi Manager
</title>


<style>

* {
    box-sizing: border-box;
}


body {

    margin: 0;

    min-height: 100vh;

    font-family:
        Arial,
        Helvetica,
        sans-serif;

    background:
        linear-gradient(
            135deg,
            #0b1020,
            #171a35,
            #24134a
        );

    color: white;

    display: flex;

    align-items: center;

    justify-content: center;

    padding: 25px;

}


.card {

    width: 100%;

    max-width: 500px;

    background:
        rgba(255,255,255,0.08);

    border:
        1px solid
        rgba(255,255,255,0.15);

    border-radius: 24px;

    padding: 35px;

    box-shadow:
        0 25px 70px
        rgba(0,0,0,0.4);

    backdrop-filter:
        blur(15px);

}


.brand {

    font-size: 16px;

    font-weight: bold;

    letter-spacing: 3px;

    color: #7de7ff;

    margin-bottom: 10px;

}


h1 {

    margin: 0 0 10px;

    font-size: 34px;

}


.subtitle {

    color: #b9bfd7;

    line-height: 1.6;

    margin-bottom: 30px;

}


label {

    display: block;

    margin:
        18px 0 8px;

    font-weight: bold;

    color: #e8eaff;

}


input {

    width: 100%;

    padding:
        14px 15px;

    border-radius: 12px;

    border:
        1px solid
        rgba(255,255,255,0.18);

    background:
        rgba(0,0,0,0.25);

    color: white;

    outline: none;

    font-size: 15px;

}


input:focus {

    border-color:
        #7de7ff;

}


.note {

    margin-top: 18px;

    padding: 14px;

    border-radius: 12px;

    background:
        rgba(125,231,255,0.08);

    color: #cbd2ea;

    font-size: 13px;

    line-height: 1.5;

}


button {

    width: 100%;

    margin-top: 25px;

    padding: 15px;

    border: 0;

    border-radius: 12px;

    background:
        linear-gradient(
            90deg,
            #5ee7ff,
            #7774ff
        );

    color: white;

    font-size: 16px;

    font-weight: bold;

    cursor: pointer;

}


button:hover {

    opacity: 0.9;

}

</style>

</head>


<body>


<div class="card">


<div class="brand">
VENUS
</div>


<h1>
WiFi Manager
</h1>


<div class="subtitle">

Connect your ESP32 to the WiFi
network used by the Venus
DHT11 realtime monitor.

</div>


<form
    method="POST"
    action="/save"
>


<label for="ssid">
WiFi SSID
</label>

<input
    type="text"
    id="ssid"
    name="ssid"
    placeholder="Enter WiFi name"
    required
>


<label for="password">
WiFi Password
</label>

<input
    type="password"
    id="password"
    name="password"
    placeholder="Enter WiFi password"
>


<label for="ip">
Static IP
</label>

<input
    type="text"
    id="ip"
    name="ip"
    placeholder="Optional"
>


<label for="gateway">
Gateway
</label>

<input
    type="text"
    id="gateway"
    name="gateway"
    placeholder="Optional"
>


<div class="note">

Leave <b>Static IP</b> and
<b>Gateway</b> blank if you want
the ESP32 to use automatic DHCP.

<br><br>

ESP32 WiFi normally requires
a <b>2.4 GHz</b> network.

</div>


<button type="submit">
SAVE & CONNECT
</button>


</form>


</div>


</body>

</html>

)rawliteral";


/* =====================================================
   READ FILE
   ===================================================== */

String readFile(
    const char *path
)
{

    if (!LittleFS.exists(path))
    {
        return "";
    }


    File file =
        LittleFS.open(
            path,
            "r"
        );


    if (!file)
    {
        return "";
    }


    String data =
        file.readString();


    file.close();


    data.trim();


    return data;
}


/* =====================================================
   WRITE FILE
   ===================================================== */

bool writeFile(
    const char *path,
    const String &data
)
{

    File file =
        LittleFS.open(
            path,
            "w"
        );


    if (!file)
    {

        Serial.print(
            "Failed to open: "
        );

        Serial.println(path);

        return false;
    }


    file.print(data);

    file.close();


    return true;
}


/* =====================================================
   DELETE WIFI FILES
   ===================================================== */

void deleteWiFiFiles()
{

    LittleFS.remove(
        "/ssid.txt"
    );

    LittleFS.remove(
        "/pass.txt"
    );

    LittleFS.remove(
        "/ip.txt"
    );

    LittleFS.remove(
        "/gateway.txt"
    );


    Serial.println(
        "WiFi settings deleted."
    );
}


/* =====================================================
   CONNECT SAVED WIFI
   ===================================================== */

bool connectToSavedWiFi()
{

    String ssid =
        readFile("/ssid.txt");

    String pass =
        readFile("/pass.txt");

    String ip =
        readFile("/ip.txt");

    String gateway =
        readFile("/gateway.txt");


    if (ssid.length() == 0)
    {

        Serial.println(
            "No saved WiFi."
        );

        return false;
    }


    WiFi.mode(
        WIFI_STA
    );


    delay(500);


    /* STATIC IP */

    if (
        ip.length() > 0 &&
        gateway.length() > 0
    )
    {

        IPAddress localIP;

        IPAddress gatewayIP;


        if (
            localIP.fromString(ip) &&
            gatewayIP.fromString(gateway)
        )
        {

            IPAddress subnet(
                255,
                255,
                255,
                0
            );


            WiFi.config(
                localIP,
                gatewayIP,
                subnet
            );

        }

    }


    WiFi.begin(
        ssid.c_str(),
        pass.c_str()
    );


    Serial.print(
        "Connecting to WiFi"
    );


    unsigned long startTime =
        millis();


    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < 20000
    )
    {

        Serial.print(".");

        delay(500);

    }


    Serial.println();


    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {

        Serial.println();
        Serial.println(
            "WIFI CONNECTED"
        );


        Serial.print(
            "IP: "
        );

        Serial.println(
            WiFi.localIP()
        );


        return true;

    }


    Serial.println(
        "WiFi connection failed."
    );


    WiFi.disconnect(
        true
    );


    delay(1000);


    return false;
}


/* =====================================================
   START WIFI MANAGER
   ===================================================== */

void startWiFiManager()
{

    wifiManagerMode =
        true;


    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "VENUS WIFI MANAGER"
    );

    Serial.println(
        "================================="
    );


    WiFi.mode(
        WIFI_AP
    );


    delay(500);


    WiFi.softAP(
        AP_SSID
    );


    delay(1000);


    Serial.print(
        "AP SSID: "
    );

    Serial.println(
        AP_SSID
    );


    Serial.print(
        "AP IP: "
    );

    Serial.println(
        WiFi.softAPIP()
    );


    /* =========================================
       WIFI MANAGER PAGE
    ========================================= */

    server.on(
        "/",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {

            request->send(
                200,
                "text/html",
                WIFI_MANAGER_HTML
            );

        }
    );


    /* =========================================
       SAVE WIFI
    ========================================= */

    server.on(
        "/save",
        HTTP_POST,
        [](AsyncWebServerRequest *request)
        {

            String ssid =
                "";

            String password =
                "";

            String ip =
                "";

            String gateway =
                "";


            if (
                request->hasParam(
                    "ssid",
                    true
                )
            )
            {

                ssid =
                    request
                    ->getParam(
                        "ssid",
                        true
                    )
                    ->value();

            }


            if (
                request->hasParam(
                    "password",
                    true
                )
            )
            {

                password =
                    request
                    ->getParam(
                        "password",
                        true
                    )
                    ->value();

            }


            if (
                request->hasParam(
                    "ip",
                    true
                )
            )
            {

                ip =
                    request
                    ->getParam(
                        "ip",
                        true
                    )
                    ->value();

            }


            if (
                request->hasParam(
                    "gateway",
                    true
                )
            )
            {

                gateway =
                    request
                    ->getParam(
                        "gateway",
                        true
                    )
                    ->value();

            }


            ssid.trim();
            password.trim();
            ip.trim();
            gateway.trim();


            if (
                ssid.length() == 0
            )
            {

                request->send(
                    400,
                    "text/plain",
                    "WiFi SSID is required."
                );

                return;

            }


            writeFile(
                "/ssid.txt",
                ssid
            );


            writeFile(
                "/pass.txt",
                password
            );


            writeFile(
                "/ip.txt",
                ip
            );


            writeFile(
                "/gateway.txt",
                gateway
            );


            request->send(
                200,
                "text/html",

                "<html>"
                "<head>"
                "<meta name='viewport' "
                "content='width=device-width,initial-scale=1'>"
                "</head>"

                "<body style='"
                "margin:0;"
                "min-height:100vh;"
                "display:flex;"
                "align-items:center;"
                "justify-content:center;"
                "font-family:Arial;"
                "background:linear-gradient(135deg,#0b1020,#171a35,#24134a);"
                "color:white;"
                "'>"

                "<div style='"
                "max-width:500px;"
                "width:90%;"
                "padding:35px;"
                "border-radius:24px;"
                "background:rgba(255,255,255,.08);"
                "border:1px solid rgba(255,255,255,.15);"
                "text-align:center;"
                "'>"

                "<div style='"
                "color:#7de7ff;"
                "letter-spacing:3px;"
                "font-weight:bold;"
                "'>VENUS</div>"

                "<h1>WiFi Saved!</h1>"

                "<p style='color:#b9bfd7;'>"
                "The ESP32 will restart and connect "
                "to the saved WiFi network."
                "</p>"

                "<p style='color:#b9bfd7;'>"
                "Please wait..."
                "</p>"

                "</div>"

                "</body>"
                "</html>"
            );


            delay(1500);


            ESP.restart();

        }
    );


    server.begin();


    Serial.println(
        "WiFi Manager ready."
    );

}


/* =====================================================
   MAIN WEB SERVER
   ===================================================== */

void startMainWebServer()
{

    wifiManagerMode =
        false;


    /* =========================================
       MAIN WEBSITE
    ========================================= */

    server.on(
        "/",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {

            if (
                LittleFS.exists(
                    "/index.html"
                )
            )
            {

                request->send(
                    LittleFS,
                    "/index.html",
                    "text/html"
                );

            }

            else
            {

                request->send(
                    404,
                    "text/plain",
                    "index.html not found."
                );

            }

        }
    );


    /* =========================================
       CHANGE WIFI
    ========================================= */

    server.on(
        "/change-wifi",
        HTTP_GET,
        [](AsyncWebServerRequest *request)
        {

            request->send(
                200,
                "text/html",

                "<html>"
                "<head>"
                "<meta name='viewport' "
                "content='width=device-width,initial-scale=1'>"
                "</head>"

                "<body style='"
                "margin:0;"
                "min-height:100vh;"
                "display:flex;"
                "align-items:center;"
                "justify-content:center;"
                "font-family:Arial;"
                "background:linear-gradient(135deg,#0b1020,#171a35,#24134a);"
                "color:white;"
                "'>"

                "<div style='"
                "max-width:500px;"
                "width:90%;"
                "padding:35px;"
                "border-radius:24px;"
                "background:rgba(255,255,255,.08);"
                "border:1px solid rgba(255,255,255,.15);"
                "text-align:center;"
                "'>"

                "<div style='"
                "color:#7de7ff;"
                "letter-spacing:3px;"
                "font-weight:bold;"
                "'>VENUS</div>"

                "<h1>Changing WiFi...</h1>"

                "<p style='color:#b9bfd7;'>"
                "Saved WiFi settings will be cleared."
                "</p>"

                "<p style='color:#b9bfd7;'>"
                "The ESP32 will restart."
                "</p>"

                "</div>"

                "</body>"
                "</html>"
            );


            delay(1000);


            deleteWiFiFiles();


            ESP.restart();

        }
    );


    /* =========================================
       STATIC FILES
    ========================================= */

    server.serveStatic(
        "/",
        LittleFS,
        "/"
    );


    server.begin();


    Serial.println(
        "Main web server ready."
    );


    Serial.print(
        "Website: http://"
    );

    Serial.println(
        WiFi.localIP()
    );

}


/* =====================================================
   FIREBASE CALLBACK
   ===================================================== */

void processFirebase(
    AsyncResult &aResult
)
{

    if (
        !aResult.isResult()
    )
    {

        return;

    }


    if (
        aResult.isEvent()
    )
    {

        Firebase.printf(
            "Firebase Event - task: %s, msg: %s, code: %d\n",
            aResult.uid().c_str(),
            aResult.eventLog().message().c_str(),
            aResult.eventLog().code()
        );

    }


    if (
        aResult.isDebug()
    )
    {

        Firebase.printf(
            "Firebase Debug - task: %s, msg: %s\n",
            aResult.uid().c_str(),
            aResult.debug().c_str()
        );

    }


    if (
        aResult.isError()
    )
    {

        Firebase.printf(
            "Firebase Error - task: %s, msg: %s, code: %d\n",
            aResult.uid().c_str(),
            aResult.error().message().c_str(),
            aResult.error().code()
        );

    }


    if (
        aResult.available()
    )
    {

        Firebase.printf(
            "Firebase Payload - task: %s, payload: %s\n",
            aResult.uid().c_str(),
            aResult.c_str()
        );

    }

}


/* =====================================================
   FIREBASE SETUP
   ===================================================== */

void setupFirebase()
{

    Serial.println(
        "Initializing Firebase..."
    );


    ssl_client.setInsecure();


    initializeApp(
        aClient,
        app,
        getAuth(user_auth),
        processFirebase,
        "authTask"
    );


    app.getApp<RealtimeDatabase>(
        Database
    );


    Database.url(
        DATABASE_URL
    );


    Serial.println(
        "Firebase initialization started."
    );

}


/* =====================================================
   GET DATE
   ===================================================== */

String getDateString()
{

    struct tm timeinfo;


    if (
        !getLocalTime(
            &timeinfo
        )
    )
    {

        return "1970-01-01";

    }


    char buffer[20];


    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d",
        &timeinfo
    );


    return String(
        buffer
    );

}


/* =====================================================
   GET TIME
   ===================================================== */

String getTimeString()
{

    struct tm timeinfo;


    if (
        !getLocalTime(
            &timeinfo
        )
    )
    {

        return "00:00:00";

    }


    char buffer[20];


    strftime(
        buffer,
        sizeof(buffer),
        "%H:%M:%S",
        &timeinfo
    );


    return String(
        buffer
    );

}


/* =====================================================
   SEND SENSOR DATA
   ===================================================== */

void sendSensorData()
{

    if (
        !app.ready()
    )
    {

        Serial.println(
            "Firebase not ready yet..."
        );

        return;

    }


    /* READ DHT11 */

    float humidity =
        dht.readHumidity();

    float temperature =
        dht.readTemperature();


    /* CHECK */

    if (
        isnan(humidity) ||
        isnan(temperature)
    )
    {

        Serial.println(
            "ERROR: Failed to read DHT11."
        );

        return;

    }


    /* DATE */

    String date =
        getDateString();


    /* TIME */

    String time =
        getTimeString();


    /* BASE PATH */

    String basePath =
        "/ESP32_Data/" +
        date +
        "/" +
        time;


    /* TEMPERATURE */

    String temperaturePath =
        basePath +
        "/temperature";


    /* HUMIDITY */

    String humidityPath =
        basePath +
        "/humidity";


    /* SERIAL MONITOR */

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "VENUS DHT11 READING"
    );

    Serial.println(
        "================================="
    );


    Serial.print(
        "Temperature: "
    );

    Serial.print(
        temperature,
        1
    );

    Serial.println(
        " °C"
    );


    Serial.print(
        "Humidity: "
    );

    Serial.print(
        humidity,
        1
    );

    Serial.println(
        " %"
    );


    Serial.print(
        "Date: "
    );

    Serial.println(
        date
    );


    Serial.print(
        "Time: "
    );

    Serial.println(
        time
    );


    Serial.print(
        "Firebase: "
    );

    Serial.println(
        basePath
    );


    /* WRITE TEMPERATURE */

    Database.set<float>(
        aClient,
        temperaturePath,
        temperature,
        processFirebase,
        "temperatureTask"
    );


    /* WRITE HUMIDITY */

    Database.set<float>(
        aClient,
        humidityPath,
        humidity,
        processFirebase,
        "humidityTask"
    );


    Serial.println(
        "Sensor data sent."
    );

}


/* =====================================================
   SETUP
   ===================================================== */

void setup()
{

    Serial.begin(
        115200
    );


    delay(1000);


    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "VENUS ACT4"
    );

    Serial.println(
        "DHT11 FIREBASE MONITOR"
    );

    Serial.println(
        "================================="
    );


    /* =========================================
       LITTLEFS
    ========================================= */

    if (
        !LittleFS.begin(true)
    )
    {

        Serial.println(
            "LittleFS mount failed!"
        );


        while (true)
        {

            delay(1000);

        }

    }


    Serial.println(
        "LittleFS ready."
    );


    /* =========================================
       DHT11
    ========================================= */

    dht.begin();


    Serial.println(
        "DHT11 initialized."
    );


    /* =========================================
       WIFI
    ========================================= */

    bool connected =
        connectToSavedWiFi();


    if (!connected)
    {

        startWiFiManager();

        return;

    }


    /* =========================================
       NTP
    ========================================= */

    configTime(
        8 * 3600,
        0,
        "pool.ntp.org",
        "time.nist.gov",
        "time.google.com"
    );


    Serial.print(
        "Synchronizing time"
    );


    struct tm timeinfo;


    int retry = 0;


    while (
        !getLocalTime(
            &timeinfo
        ) &&
        retry < 20
    )
    {

        Serial.print(".");

        delay(500);

        retry++;

    }


    Serial.println();


    if (
        getLocalTime(
            &timeinfo
        )
    )
    {

        Serial.println(
            "Time synchronized."
        );

    }

    else
    {

        Serial.println(
            "Time synchronization failed."
        );

    }


    /* =========================================
       FIREBASE
    ========================================= */

    setupFirebase();


    /* =========================================
       WEB SERVER
    ========================================= */

    startMainWebServer();


    /* =========================================
       READY
    ========================================= */

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "VENUS ACT4 SYSTEM READY"
    );

    Serial.println(
        "================================="
    );


    Serial.print(
        "Website: http://"
    );

    Serial.println(
        WiFi.localIP()
    );

}


/* =====================================================
   LOOP
   ===================================================== */

void loop()
{

    if (
        !wifiManagerMode
    )
    {

        app.loop();

    }


    if (
        !wifiManagerMode &&
        millis() -
        lastSensorRead >=
        SENSOR_INTERVAL
    )
    {

        lastSensorRead =
            millis();


        sendSensorData();

    }


    delay(10);

}