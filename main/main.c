/*
 * Kabeltester mit ESP32-S3
 *
 * Ablauf: Auftrag anlegen -> Kabel anschliessen -> Schalter wechseln ->
 * messen -> Ergebnis speichern und melden -> Kabel abziehen.
 * WLAN, LEDs und Ergebnisversand laufen neben der Hauptaufgabe.
 */

#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "flash_config.h"
#include "cJSON.h"
#include "mdns.h"


// ---- Einstellungen und gemeinsam verwendete Daten ----
#define ADER_ANZAHL 8
#define START_PIN GPIO_NUM_11
#define SCHALTER_ENTPRELLZEIT_MS 30
#define KABEL_PRUEFPAUSE_MS 200
#define KABEL_LEERSCANS 2
#define LED_GRUEN_PIN GPIO_NUM_9
#define LED_ROT_PIN GPIO_NUM_10
#define BEIDE_RICHTUNGEN 1  // 0: nur das ausgewaehlte Profil akzeptieren
#define MESSDIAGNOSE 0  // 1: alle 64 Einzelwerte im Monitor anzeigen

// -1: noch kein Test, 0: FEHLER, 1: OK
// Mehrere Aufgaben lesen diese Werte. atomic verhindert unvollstaendige Zugriffe.
// Das letzte Web-Ergebnis bleibt erhalten; die LED-Anzeige endet beim Abziehen.
static atomic_int letztes_ergebnis = -1;
static atomic_bool wlan_verbunden = false;
static atomic_int led_ergebnis = -1;
static atomic_uint kein_auftrag_signal = 0;

// Der Auftrag enthaelt Soll-Verbindungen, Zielanzahl und unveraenderte Zusatzdaten.
// Der Mutex ist eine Zugriffssperre: API, Messung und Versand teilen diese Daten.
typedef struct {
    bool aktiv;
    uint32_t id;
    int profil[ADER_ANZAHL];
    uint32_t anzahl;
    uint32_t abgeschlossen;
    cJSON *args;
} auftrag_t;

static auftrag_t auftrag = {0};
static uint32_t naechste_auftrag_id = 1;
static SemaphoreHandle_t auftrag_mutex = NULL;

#define ERGEBNIS_ANZAHL 20
#define MELDE_TIMEOUT_MS 1500

#ifndef REPORT_URL
#define REPORT_URL ""
#endif

typedef enum {
    EMPFAENGER_UNBEKANNT,
    EMPFAENGER_ERREICHBAR,
    EMPFAENGER_NICHT_ERREICHBAR,
    EMPFAENGER_FEHLER,
} empfaengerstatus_t;

static empfaengerstatus_t empfaengerstatus = EMPFAENGER_UNBEKANNT;

// Versandauftraege besitzen ihren eigenen JSON-Text. Dadurch bleiben sie gueltig,
// auch wenn der zugehoerige Auftrag endet oder ein Lauf ueberschrieben wird.
typedef struct {
    uint32_t job_id;
    uint32_t lauf;
    char *json;
} meldung_t;

static QueueHandle_t melde_queue = NULL;

// Uebersetzt den internen Empfaengerstatus in die Begriffe der JSON-API.
static const char *empfaengerstatus_text(void)
{
    switch (empfaengerstatus)
    {
        case EMPFAENGER_ERREICHBAR: return "reachable";
        case EMPFAENGER_NICHT_ERREICHBAR: return "unreachable";
        case EMPFAENGER_FEHLER: return "error";
        default: return "unknown";
    }
}

// Ein gespeicherter Lauf besitzt eine eigene Kopie von Profil und args.
// Der Ringpuffer behaelt hoechstens 20 Laeufe und ersetzt danach den aeltesten.
typedef struct {
    uint32_t job_id;
    uint32_t lauf;
    bool ok;
    int profil[ADER_ANZAHL];
    // Pro A-Anschluss ein Bit je erkanntem B-Anschluss; erfasst auch Kurzschluesse.
    uint8_t gemessen[ADER_ANZAHL];
    bool umgekehrt;
    cJSON *args;
    bool gemeldet;
} testergebnis_t;

static testergebnis_t ergebnisse[ERGEBNIS_ANZAHL];
static unsigned int ergebnis_position = 0;
static unsigned int ergebnis_anzahl = 0;


// ---- Standardprofile und Anschlussbelegung ----
typedef struct {
    const char *name;
    int verbindung[ADER_ANZAHL];
} kabelprofil_t;

// Neues Profil: eine Zeile mit Namen und allen acht Werten ergaenzen.
// Position = A-Anschluss, Wert = B-Anschluss; -1 bedeutet unbelegt.
static const kabelprofil_t profile[] = {
    {"gerade8", {0, 1, 2, 3, 4, 5, 6, 7}},
    {"gerade4", {0, 1, 2, 3, -1, -1, -1, -1}},
    {"gekreuzt01", {1, 0, 2, 3, 4, 5, 6, 7}},
    {"asymmetrisch", {1, 2, 0, 3, 4, 5, 6, -1}},
};

static const gpio_num_t pins_a[ADER_ANZAHL] = {
    GPIO_NUM_6, GPIO_NUM_16, GPIO_NUM_4, GPIO_NUM_7,
    GPIO_NUM_15, GPIO_NUM_5, GPIO_NUM_17, GPIO_NUM_18
};

static const gpio_num_t pins_b[ADER_ANZAHL] = {
    GPIO_NUM_42, GPIO_NUM_39, GPIO_NUM_1, GPIO_NUM_41,
    GPIO_NUM_40, GPIO_NUM_2, GPIO_NUM_47, GPIO_NUM_21
};

// ---- Kabelerkennung und Schalter ----

// Prueft nach einem Ergebnis, ob noch irgendeine Verbindung vorhanden ist.
// Nur von der Messaufgabe aufrufen: kein paralleles Ansteuern der A-Pins.
// Ein offenes/komplett unterbrochenes Kabel ist nicht von Abziehen unterscheidbar.
static int kabel_verbindung_vorhanden(void)
{
    for (int i = 0; i < ADER_ANZAHL; i++)
    {
        ESP_ERROR_CHECK(gpio_set_level(pins_a[i], 1));
        ESP_ERROR_CHECK(gpio_set_direction(pins_a[i], GPIO_MODE_OUTPUT));
        vTaskDelay(pdMS_TO_TICKS(10) + 1);

        int verbunden = 0;
        for (int j = 0; j < ADER_ANZAHL; j++)
            verbunden |= gpio_get_level(pins_b[j]);

        // Auch vor einem vorzeitigen Ruecksprung den Ausgang freigeben.
        ESP_ERROR_CHECK(gpio_set_direction(pins_a[i], GPIO_MODE_INPUT));
        vTaskDelay(pdMS_TO_TICKS(10) + 1);
        if (verbunden)
            return 1;
    }
    return 0;
}

// Eigene Aufgabe: auch waehrend einer Messung beide Schaltflanken erfassen.
static void schalter_ueberwachen(void *parameter)
{
    TaskHandle_t mess_task = (TaskHandle_t)parameter;
    int stabil = gpio_get_level(START_PIN);
    int kandidat = stabil;
    TickType_t seit = xTaskGetTickCount();
    // Ein zusaetzlicher Tick verhindert eine zu kurze Wartezeit durch Rundung.
    const TickType_t entprellzeit = pdMS_TO_TICKS(SCHALTER_ENTPRELLZEIT_MS) + 1;

    while (1)
    {
        int pegel = gpio_get_level(START_PIN);
        TickType_t jetzt = xTaskGetTickCount();
        if (pegel != kandidat)
        {
            kandidat = pegel;
            seit = jetzt;
        }
        if (kandidat != stabil && (TickType_t)(jetzt - seit) >= entprellzeit)
        {
            stabil = kandidat;
            // Zaehlt jeden entprellten Wechsel; Messungen laufen nacheinander.
            xTaskNotifyGive(mess_task);
        }
        vTaskDelay(1);
    }
}
// ---- WLAN-Verbindung und lokaler Geraetename ----

/*
 * Reagiert auf WLAN-Ereignisse: verbinden, nach Abbruch erneut versuchen
 * und nach Erhalt einer IP-Adresse den Verbindungsstatus fuer die LEDs setzen.
 */
static void wlan_ereignis(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START)
    {
        printf("WLAN gestartet - Verbindung wird aufgebaut.\n");
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        atomic_store(&wlan_verbunden, false);
        // Der Grundcode hilft bei der Diagnose, ohne Zugangsdaten auszugeben.
        wifi_event_sta_disconnected_t *info = event_data;
        printf("WLAN getrennt (Grund %u) - neuer Verbindungsversuch.\n",
               (unsigned int)info->reason);
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        atomic_store(&wlan_verbunden, true);
        ip_event_got_ip_t *info = event_data;

        printf("WLAN verbunden! IP-Adresse: " IPSTR "\n",
               IP2STR(&info->ip_info.ip));
    }
}

/*
 * Bereitet den WLAN-Treiber vor und uebernimmt die private Flash-Konfiguration.
 * Die Verbindung entsteht danach ueber wlan_ereignis im Hintergrund.
 */
static void wlan_starten(void)
{
    esp_err_t status = nvs_flash_init();

    // NVS wird vom WLAN-Treiber benoetigt.
    if (status == ESP_ERR_NVS_NO_FREE_PAGES ||
        status == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        status = nvs_flash_init();
    }
    ESP_ERROR_CHECK(status);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Station: Der ESP verbindet sich mit dem vorhandenen Router.
    esp_netif_t *netz = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(netz != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_netif_set_hostname(netz, DEVICE_HOSTNAME));

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    // Die Ereignisfunktion startet/repariert die Verbindung und zeigt die IP.
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wlan_ereignis, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wlan_ereignis, NULL));

    wifi_config_t config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    // Einstellungen aus flash_config.h nur im RAM des WLAN-Treibers halten.
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/*
 * Macht den ESP unter DEVICE_HOSTNAME.local und die Webseite auf Port 80 bekannt.
 * Bei einem mDNS-Fehler bleibt der Zugriff ueber die IP-Adresse moeglich.
 */
static void mdns_starten(void)
{
    esp_err_t status = mdns_init();
    if (status == ESP_OK)
    {
        status = mdns_hostname_set(DEVICE_HOSTNAME);
        if (status == ESP_OK)
            status = mdns_instance_name_set("Kabeltester");
        if (status == ESP_OK)
        {
            // Die Webseite als HTTP-Dienst auf Port 80 bekannt machen.
            mdns_txt_item_t txt[] = {{"path", "/"}};
            status = mdns_service_add("Kabeltester", "_http", "_tcp", 80, txt, 1);
        }
        if (status != ESP_OK)
            mdns_free();
    }
    if (status == ESP_OK)
        printf("mDNS gestartet - Webseite: http://%s.local\n", DEVICE_HOSTNAME);
    else
        printf("mDNS konnte nicht starten (%s) - Zugriff ueber IP bleibt verfuegbar.\n",
               esp_err_to_name(status));
}

// ---- Webseite und HTTP-API ----

// GET /: Liefert die beim Build eingebettete Webseite an den Browser.
static esp_err_t startseite_senden(httpd_req_t *anfrage)
{
    // Die HTML-Datei wird beim Build in die Firmware eingebettet.
    extern const char seite[] asm("_binary_index_html_start");

    esp_err_t status = httpd_resp_set_type(anfrage, "text/html; charset=utf-8");
    if (status != ESP_OK)
        return status;
    status = httpd_resp_set_hdr(anfrage, "Cache-Control", "no-store");
    if (status != ESP_OK)
        return status;
    return httpd_resp_sendstr(anfrage, seite);
}

// GET /api/result: Liefert das letzte Gesamtergebnis, auch nach dem Abziehen.
static esp_err_t ergebnis_senden(httpd_req_t *anfrage)
{
    // Letztes Messergebnis als JSON bereitstellen.
    int ergebnis = atomic_load(&letztes_ergebnis);
    const char *antwort;

    if (ergebnis == 1)
        antwort = "{\"result\":\"OK\"}";
    else if (ergebnis == 0)
        antwort = "{\"result\":\"FEHLER\"}";
    else
        antwort = "{\"result\":\"Noch kein Test\"}";

    esp_err_t status = httpd_resp_set_type(anfrage, "application/json");
    if (status != ESP_OK)
        return status;
    status = httpd_resp_set_hdr(anfrage, "Cache-Control", "no-store");
    if (status != ESP_OK)
        return status;
    return httpd_resp_sendstr(anfrage, antwort);
}

// Gemeinsamer Antwortweg: HTTP-Status, JSON-Typ und keine Zwischenspeicherung.
static esp_err_t json_senden(httpd_req_t *anfrage,
                            const char *status,
                            const char *inhalt)
{
    esp_err_t fehler = httpd_resp_set_status(anfrage, status);
    if (fehler != ESP_OK)
        return fehler;
    fehler = httpd_resp_set_type(anfrage, "application/json");
    if (fehler != ESP_OK)
        return fehler;
    fehler = httpd_resp_set_hdr(anfrage, "Cache-Control", "no-store");
    if (fehler != ESP_OK)
        return fehler;
    return httpd_resp_sendstr(anfrage, inhalt);
}

/*
 * POST /api/jobs: Daten empfangen und pruefen, args kopieren, Auftrag anlegen.
 * Ein vorhandener Auftrag bleibt bei einer ungueltigen Anfrage oder 409 erhalten.
 */
static esp_err_t auftrag_anlegen(httpd_req_t *anfrage)
{
    // Anfrage auf 2048 Bytes begrenzen.
    if (anfrage->content_len == 0 || anfrage->content_len > 2048)
    {
        return json_senden(
            anfrage, "400 Bad Request",
            "{\"error\":\"invalid_request\","
            "\"message\":\"Anfrage leer oder zu gross.\"}");
    }

    char *text = malloc(anfrage->content_len + 1);
    if (text == NULL)
    {
        return json_senden(
            anfrage, "500 Internal Server Error",
            "{\"error\":\"out_of_memory\"}");
    }

    // HTTP-Daten koennen in mehreren Teilen eintreffen.
    size_t empfangen = 0;
    while (empfangen < anfrage->content_len)
    {
        int gelesen = httpd_req_recv(
            anfrage, text + empfangen, anfrage->content_len - empfangen);
        if (gelesen <= 0)
        {
            free(text);
            if (gelesen == HTTPD_SOCK_ERR_TIMEOUT)
                return httpd_resp_send_408(anfrage);
            return ESP_FAIL;
        }
        empfangen += gelesen;
    }
    text[empfangen] = '\0';

    cJSON *daten = cJSON_ParseWithLengthOpts(text, empfangen + 1, NULL, true);
    free(text);
    if (!cJSON_IsObject(daten))
    {
        cJSON_Delete(daten);
        return json_senden(
            anfrage, "400 Bad Request",
            "{\"error\":\"invalid_request\","
            "\"message\":\"Ungueltiges JSON-Objekt.\"}");
    }

    const cJSON *profil = cJSON_GetObjectItemCaseSensitive(daten, "profile");
    const cJSON *anzahl = cJSON_GetObjectItemCaseSensitive(daten, "count");
    const cJSON *args = cJSON_GetObjectItemCaseSensitive(daten, "args");
    auftrag_t neu = {0};

    bool gueltig = cJSON_IsArray(profil) &&
        cJSON_GetArraySize(profil) == ADER_ANZAHL &&
        cJSON_IsNumber(anzahl) && (args == NULL || cJSON_IsObject(args));

    if (gueltig)
    {
        double wert = anzahl->valuedouble;
        gueltig = wert >= 1 && wert <= (double)UINT32_MAX;
        if (gueltig)
        {
            neu.anzahl = (uint32_t)wert;
            gueltig = wert == (double)neu.anzahl;
        }
    }

    for (int i = 0; gueltig && i < ADER_ANZAHL; i++)
    {
        const cJSON *ader = cJSON_GetArrayItem(profil, i);
        if (!cJSON_IsNumber(ader) ||
            !(ader->valuedouble >= -1 && ader->valuedouble < ADER_ANZAHL))
        {
            gueltig = false;
            break;
        }
        neu.profil[i] = (int)ader->valuedouble;
        gueltig = ader->valuedouble == neu.profil[i];
    }

    if (!gueltig)
    {
        cJSON_Delete(daten);
        return json_senden(
            anfrage, "400 Bad Request",
            "{\"error\":\"invalid_request\","
            "\"message\":\"Profil, count oder args ungueltig.\"}");
    }

    // Zusatzdaten unabhaengig von der Anfrage behalten.
    neu.args = args != NULL ? cJSON_Duplicate(args, true) : cJSON_CreateObject();
    cJSON_Delete(daten);
    if (neu.args == NULL)
    {
        return json_senden(
            anfrage, "500 Internal Server Error",
            "{\"error\":\"out_of_memory\"}");
    }

    if (xSemaphoreTake(auftrag_mutex, portMAX_DELAY) != pdTRUE)
    {
        cJSON_Delete(neu.args);
        return json_senden(
            anfrage, "500 Internal Server Error",
            "{\"error\":\"internal_error\"}");
    }
    if (auftrag.aktiv)
    {
        xSemaphoreGive(auftrag_mutex);
        cJSON_Delete(neu.args);
        return json_senden(
            anfrage, "409 Conflict",
            "{\"error\":\"job_already_active\","
            "\"message\":\"Es ist bereits ein Auftrag aktiv.\"}");
    }
    if (naechste_auftrag_id == 0)
    {
        xSemaphoreGive(auftrag_mutex);
        cJSON_Delete(neu.args);
        return json_senden(
            anfrage, "503 Service Unavailable",
            "{\"error\":\"job_ids_exhausted\"}");
    }

    neu.aktiv = true;
    neu.id = naechste_auftrag_id++;
    cJSON_Delete(auftrag.args);
    auftrag = neu;
    xSemaphoreGive(auftrag_mutex);

    char antwort[160];
    snprintf(antwort, sizeof(antwort),
             "{\"job_id\":%lu,\"status\":\"active\",\"completed\":0,\"count\":%lu}",
             (unsigned long)neu.id, (unsigned long)neu.anzahl);
    return json_senden(anfrage, "201 Created", antwort);
}

// GET /api/jobs/current: Erstellt unter der Zugriffssperre eine konsistente Antwort.
static esp_err_t aktueller_auftrag_senden(httpd_req_t *anfrage)
{
    cJSON *antwort = cJSON_CreateObject();
    if (antwort == NULL)
    {
        return json_senden(
            anfrage, "500 Internal Server Error",
            "{\"error\":\"out_of_memory\"}");
    }

    if (xSemaphoreTake(auftrag_mutex, portMAX_DELAY) != pdTRUE)
    {
        cJSON_Delete(antwort);
        return json_senden(
            anfrage, "500 Internal Server Error",
            "{\"error\":\"internal_error\"}");
    }

    bool gueltig;
    if (auftrag.aktiv)
    {
        cJSON *job = cJSON_AddObjectToObject(antwort, "job");
        gueltig = job != NULL &&
            cJSON_AddNumberToObject(job, "job_id", auftrag.id) != NULL &&
            cJSON_AddStringToObject(job, "status", "active") != NULL &&
            cJSON_AddNumberToObject(job, "completed", auftrag.abgeschlossen) != NULL &&
            cJSON_AddNumberToObject(job, "count", auftrag.anzahl) != NULL;

        cJSON *profil = gueltig
            ? cJSON_CreateIntArray(auftrag.profil, ADER_ANZAHL)
            : NULL;
        if (profil != NULL)
        {
            gueltig = cJSON_AddItemToObject(job, "profile", profil);
            if (!gueltig)
                cJSON_Delete(profil);
        }
        else
        {
            gueltig = false;
        }

        if (gueltig)
            gueltig = cJSON_AddItemReferenceToObject(job, "args", auftrag.args);
    }
    else
    {
        gueltig = cJSON_AddNullToObject(antwort, "job") != NULL;
    }

    if (gueltig)
    {
        gueltig = cJSON_AddStringToObject(
            antwort, "receiver_status", empfaengerstatus_text()) != NULL;
    }

    // JSON fertigstellen, solange der Auftrag gesperrt ist.
    char *text = gueltig ? cJSON_PrintUnformatted(antwort) : NULL;
    cJSON_Delete(antwort);
    xSemaphoreGive(auftrag_mutex);

    if (text == NULL)
    {
        return json_senden(
            anfrage, "500 Internal Server Error",
            "{\"error\":\"out_of_memory\"}");
    }

    esp_err_t status = json_senden(anfrage, "200 OK", text);
    cJSON_free(text);
    return status;
}

// DELETE /api/jobs/current: Beendet den Auftrag; gespeicherte Laeufe bleiben erhalten.
static esp_err_t auftrag_abbrechen(httpd_req_t *anfrage)
{
    if (xSemaphoreTake(auftrag_mutex, portMAX_DELAY) != pdTRUE)
    {
        return json_senden(
            anfrage, "500 Internal Server Error",
            "{\"error\":\"internal_error\"}");
    }

    if (!auftrag.aktiv)
    {
        xSemaphoreGive(auftrag_mutex);
        return json_senden(
            anfrage, "404 Not Found",
            "{\"error\":\"no_active_job\","
            "\"message\":\"Es ist kein Auftrag aktiv.\"}");
    }

    uint32_t id = auftrag.id;
    auftrag.aktiv = false;
    cJSON_Delete(auftrag.args);
    auftrag.args = NULL;
    xSemaphoreGive(auftrag_mutex);

    char antwort[96];
    snprintf(antwort, sizeof(antwort),
             "{\"job_id\":%lu,\"status\":\"cancelled\"}",
             (unsigned long)id);
    return json_senden(anfrage, "200 OK", antwort);
}

// Eine Bitmaske als Liste erkannter B-Anschluesse ausgeben; [] bedeutet offen.
static cJSON *verbindungen_json(uint8_t maske)
{
    cJSON *liste = cJSON_CreateArray();
    if (liste == NULL)
        return NULL;
    for (int b = 0; b < ADER_ANZAHL; b++)
    {
        if ((maske & (1u << b)) == 0)
            continue;
        cJSON *wert = cJSON_CreateNumber(b);
        if (wert == NULL || !cJSON_AddItemToArray(liste, wert))
        {
            cJSON_Delete(wert);
            cJSON_Delete(liste);
            return NULL;
        }
    }
    return liste;
}

// Messprofil: -1 fuer offen, eine Zahl fuer ein Ziel, eine Liste fuer mehrere.
static cJSON *messprofil_json(const testergebnis_t *quelle)
{
    cJSON *profil = cJSON_CreateArray();
    if (profil == NULL)
        return NULL;
    for (int a = 0; a < ADER_ANZAHL; a++)
    {
        uint8_t maske = quelle->gemessen[a];
        int ziel = -1;
        for (int b = 0; b < ADER_ANZAHL; b++)
            if (maske & (1u << b))
                ziel = b;
        bool mehrere = maske != 0 && (maske & (maske - 1)) != 0;
        cJSON *wert = mehrere ? verbindungen_json(maske) : cJSON_CreateNumber(ziel);
        if (wert == NULL || !cJSON_AddItemToArray(profil, wert))
        {
            cJSON_Delete(wert);
            cJSON_Delete(profil);
            return NULL;
        }
    }
    return profil;
}

// Verbindungen kurz und lesbar beschreiben, z. B. "B0, B2" oder "keine".
static void verbindungen_text(uint8_t maske, char text[40])
{
    size_t position = 0;
    text[0] = '\0';
    for (int b = 0; b < ADER_ANZAHL; b++)
    {
        if (maske & (1u << b))
            position += (size_t)snprintf(text + position, 40 - position,
                                       "%sB%d", position == 0 ? "" : ", ", b);
    }
    if (position == 0)
        snprintf(text, 40, "keine");
}

/*
 * Ergaenzt bei FEHLER die tatsaechlichen Verbindungen und Abweichungen.
 * Pro A-Anschluss gibt es eine Fehlerbeschreibung; die Anzahl zaehlt weiterhin
 * jede fehlende oder zusaetzliche Verbindung wie die Diagnose im Monitor.
 */
static bool diagnose_json_ergaenzen(cJSON *lauf, const testergebnis_t *quelle)
{
    cJSON *fehler = cJSON_AddArrayToObject(lauf, "errors");
    if (fehler == NULL ||
        cJSON_AddStringToObject(lauf, "comparison_direction",
                               quelle->umgekehrt ? "reversed" : "normal") == NULL)
        return false;

    int fehleranzahl = 0;
    for (int a = 0; a < ADER_ANZAHL; a++)
    {
        uint8_t erwartet = 0;
        for (int b = 0; b < ADER_ANZAHL; b++)
        {
            bool soll = quelle->umgekehrt ? quelle->profil[b] == a
                                         : quelle->profil[a] == b;
            if (soll)
                erwartet |= (uint8_t)(1u << b);
        }
        uint8_t unterschied = erwartet ^ quelle->gemessen[a];
        if (unterschied == 0)
            continue;
        for (int b = 0; b < ADER_ANZAHL; b++)
            fehleranzahl += (unterschied >> b) & 1u;

        char soll_text[40];
        char ist_text[40];
        char beschreibung[128];
        verbindungen_text(erwartet, soll_text);
        verbindungen_text(quelle->gemessen[a], ist_text);
        snprintf(beschreibung, sizeof(beschreibung),
                 "A%d: erwartet %s; gemessen %s", a, soll_text, ist_text);
        cJSON *eintrag = cJSON_CreateString(beschreibung);
        if (eintrag == NULL || !cJSON_AddItemToArray(fehler, eintrag))
        {
            cJSON_Delete(eintrag);
            return false;
        }
    }
    return cJSON_AddNumberToObject(lauf, "error_count", fehleranzahl) != NULL;
}

/*
 * Erstellt ein unabhaengiges JSON-Objekt fuer einen Lauf, bei FEHLER mit Diagnose.
 * Nur die Laufliste bekommt gemeldet; beim Versand steht die Annahme noch nicht fest.
 */
static cJSON *lauf_json_erstellen(const testergebnis_t *quelle, bool mit_meldestatus)
{
    cJSON *lauf = cJSON_CreateObject();
    bool gueltig = lauf != NULL &&
        cJSON_AddNumberToObject(lauf, "job_id", quelle->job_id) != NULL &&
        cJSON_AddNumberToObject(lauf, "lauf", quelle->lauf) != NULL &&
        cJSON_AddStringToObject(lauf, "result", quelle->ok ? "OK" : "FEHLER") != NULL;
    if (gueltig && mit_meldestatus)
        gueltig = cJSON_AddBoolToObject(lauf, "gemeldet", quelle->gemeldet) != NULL;
    if (gueltig)
    {
        cJSON *profil = cJSON_CreateIntArray(quelle->profil, ADER_ANZAHL);
        gueltig = profil != NULL && cJSON_AddItemToObject(lauf, "profile", profil);
        if (!gueltig)
            cJSON_Delete(profil);
    }
    if (gueltig)
    {
        cJSON *args = cJSON_Duplicate(quelle->args, true);
        gueltig = args != NULL && cJSON_AddItemToObject(lauf, "args", args);
        if (!gueltig)
            cJSON_Delete(args);
    }
    // Messprofil fuer die eigene Spalte und innerhalb der Zusatzdaten versenden.
    if (gueltig)
    {
        cJSON *gemessen = messprofil_json(quelle);
        gueltig = gemessen != NULL && cJSON_AddItemToObject(lauf, "measured_profile", gemessen);
        if (!gueltig)
            cJSON_Delete(gemessen);
        if (gueltig)
        {
            cJSON *args = cJSON_GetObjectItemCaseSensitive(lauf, "args");
            cJSON *kopie = cJSON_Duplicate(gemessen, true);
            // Der Name gemessen ist fuer die vom Tester erzeugten Messwerte reserviert.
            cJSON_DeleteItemFromObjectCaseSensitive(args, "gemessen");
            gueltig = kopie != NULL && cJSON_AddItemToObject(args, "gemessen", kopie);
            if (!gueltig)
                cJSON_Delete(kopie);
        }
    }
    // Dieselbe Diagnose geht an die Webseite und an den Meldeempfaenger.
    if (gueltig && !quelle->ok)
        gueltig = diagnose_json_ergaenzen(lauf, quelle);
    if (!gueltig)
    {
        cJSON_Delete(lauf);
        return NULL;
    }
    return lauf;
}

// GET /api/runs: Liest den Ringpuffer vom aeltesten zum neuesten gespeicherten Lauf.
static esp_err_t testergebnisse_senden(httpd_req_t *anfrage)
{
    if (xSemaphoreTake(auftrag_mutex, portMAX_DELAY) != pdTRUE)
        return json_senden(anfrage, "500 Internal Server Error",
                           "{\"error\":\"internal_error\"}");

    // Eigene JSON-Kopien erstellen, bevor ein Lauf ueberschrieben werden kann.
    unsigned int anzahl = ergebnis_anzahl;
    unsigned int start = (ergebnis_position + ERGEBNIS_ANZAHL - anzahl)
                         % ERGEBNIS_ANZAHL;
    cJSON *antwort = cJSON_CreateObject();
    cJSON *runs = antwort != NULL
        ? cJSON_AddArrayToObject(antwort, "runs") : NULL;
    bool gueltig = runs != NULL;
    for (unsigned int i = 0; gueltig && i < anzahl; i++)
    {
        const testergebnis_t *quelle = &ergebnisse[(start + i) % ERGEBNIS_ANZAHL];
        cJSON *lauf = lauf_json_erstellen(quelle, true);
        gueltig = lauf != NULL && cJSON_AddItemToArray(runs, lauf);
        if (!gueltig)
            cJSON_Delete(lauf);
    }
    xSemaphoreGive(auftrag_mutex);

    char *text = gueltig ? cJSON_PrintUnformatted(antwort) : NULL;
    cJSON_Delete(antwort);
    if (text == NULL)
        return json_senden(anfrage, "500 Internal Server Error",
                           "{\"error\":\"out_of_memory\"}");

    esp_err_t status = json_senden(anfrage, "200 OK", text);
    cJSON_free(text);
    return status;
}

// GET /api/profiles: Liest Namen und Verbindungen direkt aus der Profiltabelle.
static esp_err_t standardprofile_senden(httpd_req_t *anfrage)
{
    cJSON *antwort = cJSON_CreateObject();
    cJSON *liste = antwort != NULL ? cJSON_AddArrayToObject(antwort, "profiles") : NULL;
    bool gueltig = liste != NULL;
    for (unsigned int i = 0; gueltig && i < sizeof(profile) / sizeof(profile[0]); i++)
    {
        cJSON *eintrag = cJSON_CreateObject();
        gueltig = eintrag != NULL && cJSON_AddStringToObject(eintrag, "name", profile[i].name) != NULL;
        if (gueltig)
        {
            cJSON *profil = cJSON_CreateIntArray(profile[i].verbindung, ADER_ANZAHL);
            gueltig = profil != NULL && cJSON_AddItemToObject(eintrag, "profile", profil);
            if (!gueltig)
                cJSON_Delete(profil);
        }
        if (gueltig)
            gueltig = cJSON_AddItemToArray(liste, eintrag);
        if (!gueltig)
            cJSON_Delete(eintrag);
    }
    char *text = gueltig ? cJSON_PrintUnformatted(antwort) : NULL;
    cJSON_Delete(antwort);
    if (text == NULL)
        return json_senden(anfrage, "500 Internal Server Error", "{\"error\":\"out_of_memory\"}");
    esp_err_t status = json_senden(anfrage, "200 OK", text);
    cJSON_free(text);
    return status;
}

// Startet HTTP-Port 80 und verbindet jeden API-Pfad mit seiner Funktion.
static void webserver_starten(void)
{
    // Eigene Server-Aufgabe: Die Kabelmessung kann parallel weiterlaufen.
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    const httpd_uri_t startseite = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = startseite_senden,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &startseite));

    const httpd_uri_t ergebnis = {
        .uri = "/api/result",
        .method = HTTP_GET,
        .handler = ergebnis_senden,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ergebnis));

    const httpd_uri_t neuer_auftrag = {
        .uri = "/api/jobs",
        .method = HTTP_POST,
        .handler = auftrag_anlegen,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &neuer_auftrag));

    const httpd_uri_t aktueller_auftrag = {
        .uri = "/api/jobs/current",
        .method = HTTP_GET,
        .handler = aktueller_auftrag_senden,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &aktueller_auftrag));

    const httpd_uri_t abbrechen = {
        .uri = "/api/jobs/current",
        .method = HTTP_DELETE,
        .handler = auftrag_abbrechen,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &abbrechen));

    const httpd_uri_t testergebnisse = {
        .uri = "/api/runs",
        .method = HTTP_GET,
        .handler = testergebnisse_senden,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &testergebnisse));

    const httpd_uri_t standardprofile = {
        .uri = "/api/profiles",
        .method = HTTP_GET,
        .handler = standardprofile_senden,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &standardprofile));
    printf("Webserver gestartet - HTTP-Port 80.\n");
}

// ---- Ergebnisversand im Hintergrund ----

/*
 * Wartet auf eingereihte Meldungen und sendet sie nacheinander an REPORT_URL.
 * Nur HTTP 2xx bedeutet angenommen. Ein Versandfehler aendert das Kabelergebnis nicht.
 * Ein Lauf wird ueber job_id und lauf gesucht, damit kein anderer Eintrag markiert wird.
 */
static void meldungen_senden(void *parameter)
{
    meldung_t meldung;
    while (xQueueReceive(melde_queue, &meldung, portMAX_DELAY) == pdTRUE)
    {
        esp_err_t fehler = ESP_ERR_INVALID_ARG;
        int http_status = 0;
        if (REPORT_URL[0] != '\0')
        {
            esp_http_client_config_t config = {
                .url = REPORT_URL,
                .method = HTTP_METHOD_POST,
                .timeout_ms = MELDE_TIMEOUT_MS,
                .disable_auto_redirect = true,
                .max_authorization_retries = -1,
            };
            esp_http_client_handle_t client = esp_http_client_init(&config);
            if (client != NULL)
            {
                fehler = esp_http_client_set_header(client, "Content-Type", "application/json");
                if (fehler == ESP_OK)
                    fehler = esp_http_client_set_post_field(client, meldung.json, strlen(meldung.json));
                if (fehler == ESP_OK)
                    fehler = esp_http_client_perform(client);
                if (fehler == ESP_OK)
                    http_status = esp_http_client_get_status_code(client);
                esp_http_client_cleanup(client);
            }
            else
                fehler = ESP_ERR_NO_MEM;
        }

        bool gemeldet = fehler == ESP_OK && http_status >= 200 && http_status < 300;
        if (xSemaphoreTake(auftrag_mutex, portMAX_DELAY) == pdTRUE)
        {
            empfaengerstatus = gemeldet ? EMPFAENGER_ERREICHBAR
                : fehler == ESP_OK ? EMPFAENGER_FEHLER : EMPFAENGER_NICHT_ERREICHBAR;
            // Ein inzwischen ueberschriebener Lauf darf nicht veraendert werden.
            for (unsigned int i = 0; i < ERGEBNIS_ANZAHL; i++)
            {
                if (ergebnisse[i].job_id == meldung.job_id && ergebnisse[i].lauf == meldung.lauf)
                {
                    ergebnisse[i].gemeldet = gemeldet;
                    break;
                }
            }
            xSemaphoreGive(auftrag_mutex);
        }
        if (gemeldet)
            printf("Auftrag %lu, Lauf %lu: Meldung angenommen (HTTP %d).\n",
                   (unsigned long)meldung.job_id, (unsigned long)meldung.lauf, http_status);
        else if (fehler == ESP_OK)
            printf("Auftrag %lu, Lauf %lu: Meldung abgelehnt (HTTP %d).\n",
                   (unsigned long)meldung.job_id, (unsigned long)meldung.lauf, http_status);
        else
            printf("Auftrag %lu, Lauf %lu: Meldeempfaenger nicht erreichbar (%s).\n",
                   (unsigned long)meldung.job_id, (unsigned long)meldung.lauf, esp_err_to_name(fehler));
        cJSON_free(meldung.json);
    }
    vTaskDelete(NULL);
}

// Legt die Versandwarteschlange und ihre eigene Aufgabe an.
static void meldeversand_starten(void)
{
    melde_queue = xQueueCreate(ERGEBNIS_ANZAHL, sizeof(meldung_t));
    ESP_ERROR_CHECK(melde_queue != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    BaseType_t gestartet = xTaskCreate(meldungen_senden, "meldungen", 6144, NULL, 4, NULL);
    ESP_ERROR_CHECK(gestartet == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

// Bereitet den JSON-Text vor und reiht ihn ohne Wartezeit ein.
// Ist die Queue voll, bleibt der Lauf gespeichert, aber nicht gemeldet.
// Die Queue erhaelt eine eigene Kopie, unabhaengig von Auftrag und Laufliste.
static void meldung_einreihen(const testergebnis_t *quelle)
{
    cJSON *daten = lauf_json_erstellen(quelle, false);
    char *text = daten != NULL ? cJSON_PrintUnformatted(daten) : NULL;
    cJSON_Delete(daten);
    if (text == NULL)
    {
        printf("Meldung nicht vorgemerkt - zu wenig Speicher.\n");
        return;
    }
    meldung_t meldung = {.job_id = quelle->job_id, .lauf = quelle->lauf, .json = text};
    if (xQueueSend(melde_queue, &meldung, 0) != pdTRUE)
    {
        cJSON_free(text);
        printf("Meldung nicht vorgemerkt - Versandwarteschlange voll.\n");
    }
}

// ---- Auftragszugriff und Ergebnisspeicher ----

// Erstellt die gemeinsame Zugriffssperre, bevor andere Aufgaben gestartet werden.
static void auftrag_speicher_starten(void)
{
    // Gemeinsamen Zugriff von API und Messung absichern.
    auftrag_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(auftrag_mutex != NULL ? ESP_OK : ESP_ERR_NO_MEM);
}

// Eine Messung braucht nur ID und Profil als festen Schnappschuss.
typedef struct {
    uint32_t id;
    int profil[ADER_ANZAHL];
} messauftrag_t;

// Kopiert den aktiven Auftrag fuer die Messung und gibt die Sperre sofort wieder frei.
static bool messauftrag_lesen(messauftrag_t *ziel)
{
    if (xSemaphoreTake(auftrag_mutex, portMAX_DELAY) != pdTRUE)
        return false;

    bool aktiv = auftrag.aktiv;
    if (aktiv)
    {
        ziel->id = auftrag.id;
        for (int i = 0; i < ADER_ANZAHL; i++)
            ziel->profil[i] = auftrag.profil[i];
    }
    xSemaphoreGive(auftrag_mutex);
    return aktiv;
}

// Speichert Profil, args und Ergebnis im naechsten Ringplatz und merkt die Meldung vor.
// Aufrufen, solange auftrag_mutex gesperrt ist.
static bool ergebnis_speichern(uint32_t job_id, uint32_t lauf, bool ok,
                              const int messung[ADER_ANZAHL][ADER_ANZAHL],
                              bool umgekehrt)
{
    cJSON *args = cJSON_Duplicate(auftrag.args, true);
    if (args == NULL)
        return false;

    testergebnis_t neu = {
        .job_id = job_id,
        .lauf = lauf,
        .ok = ok,
        .umgekehrt = umgekehrt,
        .args = args,
        .gemeldet = false,
    };
    for (int i = 0; i < ADER_ANZAHL; i++)
    {
        neu.profil[i] = auftrag.profil[i];
        // Eigene Messkopie behalten, bevor die naechste Messung beginnt.
        for (int j = 0; j < ADER_ANZAHL; j++)
            if (messung[i][j])
                neu.gemessen[i] |= (uint8_t)(1u << j);
    }

    // Nur die Kopie des ueberschriebenen Laufs freigeben.
    cJSON_Delete(ergebnisse[ergebnis_position].args);
    ergebnisse[ergebnis_position] = neu;
    meldung_einreihen(&ergebnisse[ergebnis_position]);
    ergebnis_position = (ergebnis_position + 1) % ERGEBNIS_ANZAHL;
    if (ergebnis_anzahl < ERGEBNIS_ANZAHL)
        ergebnis_anzahl++;
    return true;
}

/*
 * Prueft vor dem Speichern nochmals die Auftrags-ID: Ein inzwischen abgebrochener
 * Auftrag darf keinen neuen Lauf erhalten. Erst nach dem Speichern wird gezaehlt.
 */
static esp_err_t messung_abschliessen(uint32_t id, bool ok,
                                    const int messung[ADER_ANZAHL][ADER_ANZAHL],
                                    bool umgekehrt)
{
    if (xSemaphoreTake(auftrag_mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    // Abgebrochene oder ersetzte Auftraege erhalten keinen weiteren Lauf.
    if (!auftrag.aktiv || auftrag.id != id)
    {
        xSemaphoreGive(auftrag_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    if (!ergebnis_speichern(id, auftrag.abgeschlossen + 1, ok, messung, umgekehrt))
    {
        xSemaphoreGive(auftrag_mutex);
        return ESP_ERR_NO_MEM;
    }
    auftrag.abgeschlossen++;
    printf("Auftrag %lu: %lu von %lu Kabeln geprueft.\n",
           (unsigned long)id, (unsigned long)auftrag.abgeschlossen,
           (unsigned long)auftrag.anzahl);
    if (auftrag.abgeschlossen >= auftrag.anzahl)
    {
        auftrag.aktiv = false;
        cJSON_Delete(auftrag.args);
        auftrag.args = NULL;
        printf("Auftrag beendet.\n");
    }
    xSemaphoreGive(auftrag_mutex);
    return ESP_OK;
}

// ---- LED-Anzeige ----

/*
 * Diese Aufgabe steuert beide LEDs. Reihenfolge: Ergebnis, fehlendes WLAN,
 * Schalter ohne Auftrag, danach Auftrag aktiv oder Bereitschaft.
 * Die Zeitvergleiche erzeugen Blinkmuster, ohne die Messung anzuhalten.
 */
static void leds_steuern(void *parameter)
{
    unsigned int letztes_signal = 0;
    uint32_t warnstart = 0;
    bool warnung_aktiv = false;

    while (1)
    {
        uint32_t jetzt = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
        unsigned int signal = atomic_load(&kein_auftrag_signal);
        if (signal != letztes_signal)
        {
            letztes_signal = signal;
            warnstart = jetzt;
            warnung_aktiv = true;
        }
        if (warnung_aktiv && (uint32_t)(jetzt - warnstart) >= 3000)
            warnung_aktiv = false;

        int gruen = 0;
        int rot = 0;
        int ergebnis = atomic_load(&led_ergebnis);
        // Ein Messergebnis hat immer Vorrang, auch bei WLAN-Ausfall.
        if (ergebnis >= 0)
        {
            gruen = (ergebnis == 1);
            rot = (ergebnis == 0);
        }
        else if (!atomic_load(&wlan_verbunden))
            rot = (jetzt % 1000) < 500;
        else if (warnung_aktiv)
            rot = ((uint32_t)(jetzt - warnstart) % 200) < 100;
        else
        {
            bool aktiv = false;
            if (xSemaphoreTake(auftrag_mutex, portMAX_DELAY) == pdTRUE)
            {
                aktiv = auftrag.aktiv;
                xSemaphoreGive(auftrag_mutex);
            }
            gruen = aktiv ? (jetzt % 1000) < 500 : (jetzt % 3000) < 100;
        }

        ESP_ERROR_CHECK(gpio_set_level(LED_GRUEN_PIN, gruen));
        ESP_ERROR_CHECK(gpio_set_level(LED_ROT_PIN, rot));
        vTaskDelay(pdMS_TO_TICKS(20) + 1);
    }
}

// ---- Hardware und einzelne Schritte einer Messung ----

/*
 * Richtet LEDs, Kabelanschluesse und Schalter ein.
 * A-Pins bleiben anfangs hochohmig; B-Pins lesen mit Pull-down.
 * Die Schalteraufgabe benachrichtigt weiterhin die aufrufende Hauptaufgabe.
 */
static void hardware_starten(void)
{
    // Externe LEDs: GPIO -> eigener 1-kOhm-Widerstand -> LED -> GND.
    // LOW vor dem Einschalten der Ausgaenge vorbereiten: beide LEDs aus.
    const gpio_num_t led_pins[] = {LED_GRUEN_PIN, LED_ROT_PIN};
    for (unsigned int i = 0; i < sizeof(led_pins) / sizeof(led_pins[0]); i++)
    {
        ESP_ERROR_CHECK(gpio_reset_pin(led_pins[i]));
        ESP_ERROR_CHECK(gpio_set_pull_mode(led_pins[i], GPIO_FLOATING));
        ESP_ERROR_CHECK(gpio_set_level(led_pins[i], 0));
        ESP_ERROR_CHECK(gpio_set_direction(led_pins[i], GPIO_MODE_OUTPUT));
    }

    BaseType_t led_task = xTaskCreate(leds_steuern, "leds", 2048, NULL, 3, NULL);
    ESP_ERROR_CHECK(led_task == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    // B: Eingaenge mit Pull-down.
    for (int i = 0; i < ADER_ANZAHL; i++)
    {
        ESP_ERROR_CHECK(gpio_reset_pin(pins_b[i]));
        ESP_ERROR_CHECK(gpio_set_direction(pins_b[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_set_pull_mode(pins_b[i], GPIO_PULLDOWN_ONLY));
    }

    // A: Zu Beginn sind alle Pins hochohmig.
    for (int i = 0; i < ADER_ANZAHL; i++)
    {
        ESP_ERROR_CHECK(gpio_reset_pin(pins_a[i]));
        ESP_ERROR_CHECK(gpio_set_direction(pins_a[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_set_pull_mode(pins_a[i], GPIO_FLOATING));
    }

    // Externer Kippschalter: GPIO11 gegen GND, geschlossen entspricht 0.
    ESP_ERROR_CHECK(gpio_reset_pin(START_PIN));
    ESP_ERROR_CHECK(gpio_set_direction(START_PIN, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(START_PIN, GPIO_PULLUP_ONLY));

    // Diagnose: Zeigt, welche GPIOs die laufende Firmware den Buchsen zuordnet.
    printf("GPIO-Diagnose aktiv - Messanschluesse der laufenden Firmware:\n");
    for (int i = 0; i < ADER_ANZAHL; i++)
    {
        printf("A%d = GPIO%d; B%d = GPIO%d\n",
               i, (int)pins_a[i], i, (int)pins_b[i]);
    }

    BaseType_t gestartet = xTaskCreate(schalter_ueberwachen, "schalter", 2048,
                                      xTaskGetCurrentTaskHandle(), 5, NULL);
    ESP_ERROR_CHECK(gestartet == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    printf("Kabeltester bereit - Schalterwechsel an GPIO%d startet bei aktivem Auftrag eine Messung.\n",
           (int)START_PIN);
}

/*
 * Baut die Verbindungsmatrix auf: Zeile = A-Anschluss, Spalte = B-Anschluss.
 * Immer nur einen A-Pin aktiv treiben und danach wieder freigeben.
 */
static void kabel_messen(int messung[ADER_ANZAHL][ADER_ANZAHL])
{
    // Nacheinander A0 bis A7 aktivieren.
    for (int i = 0; i < ADER_ANZAHL; i++)
    {
        // HIGH vorbereiten, bevor der Ausgang eingeschaltet wird.
        ESP_ERROR_CHECK(gpio_set_level(pins_a[i], 1));
        ESP_ERROR_CHECK(gpio_set_direction(pins_a[i], GPIO_MODE_OUTPUT));
        vTaskDelay(pdMS_TO_TICKS(10));

        // Fuer den aktiven A-Pin alle acht B-Eingaenge lesen.
        for (int j = 0; j < ADER_ANZAHL; j++)
        {
            int pegel = gpio_get_level(pins_b[j]);
            messung[i][j] = pegel;
#if MESSDIAGNOSE
            // Einzelwerte nur zur Fehlersuche ausgeben; die Messung bleibt gleich.
            printf("A%d (GPIO%d) -> B%d (GPIO%d): %d\n",
                   i, (int)pins_a[i], j, (int)pins_b[j], pegel);
#endif
        }

        // Vor dem naechsten A-Pin den aktuellen freigeben.
        ESP_ERROR_CHECK(gpio_set_direction(pins_a[i], GPIO_MODE_INPUT));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*
 * Vergleicht alle Verbindungen mit dem Profil, auch in umgekehrter Richtung.
 * Die Fehlerzahlen werden fuer die spaetere Diagnose mit zurueckgegeben.
 */
static bool profil_pruefen(const int messung[ADER_ANZAHL][ADER_ANZAHL],
                          const int profil[ADER_ANZAHL],
                          int *normal, int *umgekehrt)
{
    // Kabelenden tauschen entspricht dem Vertauschen von Zeile
    // und Spalte der erwarteten Verbindungsmatrix, nicht dem
    // einfachen Rueckwaertslesen des Profil-Arrays.
    int fehler_normal = 0;
    int fehler_umgekehrt = 0;
    for (int i = 0; i < ADER_ANZAHL; i++)
    {
        for (int j = 0; j < ADER_ANZAHL; j++)
        {
            fehler_normal += (messung[i][j] != (profil[i] == j));
            fehler_umgekehrt += (messung[i][j] != (profil[j] == i));
        }
    }

    bool kabel_ok = (fehler_normal == 0) ||
                    (BEIDE_RICHTUNGEN && fehler_umgekehrt == 0);
    *normal = fehler_normal;
    *umgekehrt = fehler_umgekehrt;
    return kabel_ok;
}

/*
 * Schreibt das Gesamtergebnis und bei FEHLER die Abweichungen in den Monitor.
 * Dieser Schritt erfolgt erst, nachdem der Lauf erfolgreich gespeichert wurde.
 */
static void diagnose_ausgeben(const int messung[ADER_ANZAHL][ADER_ANZAHL],
                             const int profil[ADER_ANZAHL],
                             int fehler_normal, int fehler_umgekehrt)
{
    if (fehler_normal == 0)
    {
        if (BEIDE_RICHTUNGEN && fehler_umgekehrt == 0)
            printf("Ergebnis: OK - beide Richtungen identisch\n");
        else
            printf("Ergebnis: OK - normale Richtung\n");
    }
    else if (BEIDE_RICHTUNGEN && fehler_umgekehrt == 0)
    {
        printf("Ergebnis: OK - Kabelenden vertauscht\n");
    }
    else
    {
        printf("Ergebnis: FEHLER - normal: %d Abweichungen\n", fehler_normal);
        if (BEIDE_RICHTUNGEN)
            printf("Umgekehrte Richtung: %d Abweichungen\n", fehler_umgekehrt);

        // Diagnose fuer die passendere Richtung; bei Gleichstand normal.
        int umgekehrt = BEIDE_RICHTUNGEN && fehler_umgekehrt < fehler_normal;
        printf("Details gegen Profilrichtung: %s\n",
               umgekehrt ? "umgekehrt" : "normal");
        for (int i = 0; i < ADER_ANZAHL; i++)
        {
            for (int j = 0; j < ADER_ANZAHL; j++)
            {
                int erwartet = umgekehrt ? (profil[j] == i) : (profil[i] == j);
                if (messung[i][j] != erwartet)
                    printf("FEHLER A%d -> B%d: erwartet %d, gemessen %d\n",
                           i, j, erwartet, messung[i][j]);
            }
        }
    }
}

// ---- Programmstart und wiederkehrender Messablauf ----

/*
 * Startet zuerst die Dienste und Hardware. Danach wartet diese Hauptaufgabe
 * auf Schalterwechsel und fuehrt die einzelnen Messschritte nacheinander aus.
 */
void app_main(void)
{
    // 1. Gemeinsamen Speicher und die Netzwerkdienste starten.
    auftrag_speicher_starten();
    // WLAN beim Einschalten starten; die Verbindung entsteht im Hintergrund.
    wlan_starten();
    meldeversand_starten();
    webserver_starten();
    mdns_starten();

    // 2. GPIOs vorbereiten und die Schalter- und LED-Aufgaben starten.
    hardware_starten();

    int ergebnis_angezeigt = 0;
    int leerscans = 0;
    while (1)
    {
        // 3. Auf den Schalter warten; nach einem Ergebnis das Abziehen erkennen.
        // Ohne Ergebnis nur auf den Schalter warten. Nach einem Test werden
        // Anwesenheitspruefungen zwischen den Schalterauftraegen ausgefuehrt.
        TickType_t wartezeit = ergebnis_angezeigt
            ? pdMS_TO_TICKS(KABEL_PRUEFPAUSE_MS) + 1 : portMAX_DELAY;
        if (ulTaskNotifyTake(pdFALSE, wartezeit) == 0)
        {
            if (kabel_verbindung_vorhanden())
                leerscans = 0;
            else
                leerscans++;

            if (leerscans >= KABEL_LEERSCANS)
            {
                atomic_store(&led_ergebnis, -1);
                ergebnis_angezeigt = 0;
                leerscans = 0;
                printf("Keine Verbindung mehr erkannt - Ergebnisanzeige beendet, bereit fuer neuen Test.\n");
            }
            continue;
        }
        if (ergebnis_angezeigt)
        {
            printf("Zuerst das Kabel abziehen.\n");
            continue;
        }

        // 4. Den Auftrag kopieren, damit die Messung mit einem festen Profil laeuft.
        messauftrag_t messauftrag;
        if (!messauftrag_lesen(&messauftrag))
        {
            atomic_fetch_add(&kein_auftrag_signal, 1);
            printf("Kein Auftrag aktiv - keine Messung.\n");
            continue;
        }
        const int *profil = messauftrag.profil;
        leerscans = 0;
        ergebnis_angezeigt = 0;
        printf("Schalterwechsel erkannt - Messung startet\n");

        // 5. Verbindungen messen und mit dem Auftragsprofil vergleichen.
        int messung[ADER_ANZAHL][ADER_ANZAHL] = {0};
        kabel_messen(messung);

        int fehler_normal;
        int fehler_umgekehrt;
        bool kabel_ok = profil_pruefen(messung, profil,
                                      &fehler_normal, &fehler_umgekehrt);

        // 6. Lauf speichern, Zaehler erhoehen und bei Bedarf den Auftrag beenden.
        // Nur ein noch aktiver Auftrag darf das Ergebnis erhalten.
        bool umgekehrt = BEIDE_RICHTUNGEN && fehler_umgekehrt < fehler_normal;
        esp_err_t gespeichert = messung_abschliessen(messauftrag.id, kabel_ok,
                                                    messung, umgekehrt);
        if (gespeichert != ESP_OK)
        {
            if (gespeichert == ESP_ERR_INVALID_STATE)
                printf("Messung verworfen - Auftrag nicht mehr aktiv.\n");
            else
                printf("Messung verworfen - Ergebnis konnte nicht gespeichert werden (%s).\n",
                       esp_err_to_name(gespeichert));
            continue;
        }

        // 7. Diagnose ausgeben und Ergebnis fuer Webseite und LEDs festhalten.
        diagnose_ausgeben(messung, profil, fehler_normal, fehler_umgekehrt);

        // Dasselbe Gesamtergebnis wie im Monitor anzeigen und halten.
        atomic_store(&letztes_ergebnis, kabel_ok);
        atomic_store(&led_ergebnis, kabel_ok);
        ergebnis_angezeigt = 1;

    }
}
