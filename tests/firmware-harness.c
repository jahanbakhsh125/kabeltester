/*
 * Softwaretest im Emulator: reale Profil-, Speicher- und JSON-Funktionen.
 * FreeRTOS-Sperren und Versandwarteschlange werden nachgebildet.
 * HTTP-Empfang/Antworten werden nachgebildet, die API-Handler sind echter Code.
 * GPIOs, WLAN, HTTP-Transport und echtes Task-Timing werden nicht getestet.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <reent.h>
#include <sys/stat.h>
#include "cJSON.h"

#include "firmware-types.inc"
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_NO_MEM 3
#define pdTRUE 1
#define portMAX_DELAY 0
typedef int esp_err_t;
static auftrag_t auftrag;
static testergebnis_t ergebnisse[ERGEBNIS_ANZAHL];
static unsigned int ergebnis_position, ergebnis_anzahl;
static int auftrag_mutex;
static bool sperre_ok = true;
static char *versand;
static unsigned int pruefungen;
static uint32_t naechste_auftrag_id=1;
typedef struct { size_t content_len; const char *body; size_t offset; bool timeout; } httpd_req_t;
#define HTTPD_SOCK_ERR_TIMEOUT -2
static cJSON *antwort;
static int http_status;
static const char *empfaengerstatus_text(void) { return "unknown"; }
static esp_err_t json_senden(httpd_req_t *req, const char *status, const char *body)
{
    (void)req; http_status=atoi(status);
    cJSON_Delete(antwort); antwort=cJSON_Parse(body); return ESP_OK;
}
static int httpd_req_recv(httpd_req_t *req, char *buffer, size_t size)
{
    if(req->timeout) return HTTPD_SOCK_ERR_TIMEOUT;
    // Absichtlich kleine Pakete: der echte Handler muss mehrfach empfangen.
    if(size>7) size=7;
    memcpy(buffer,req->body+req->offset,size); req->offset+=size; return (int)size;
}
static esp_err_t httpd_resp_send_408(httpd_req_t *req)
{
    return json_senden(req,"408 Request Timeout","{\"error\":\"timeout\"}");
}
static int xSemaphoreTake(int mutex, int wait) { (void)mutex; (void)wait; return sperre_ok; }
static void xSemaphoreGive(int mutex) { (void)mutex; }
static void meldung_einreihen(const testergebnis_t *quelle);

#include "firmware-functions.inc"

// Fuer die Versandpruefung denselben Serializer verwenden wie die Firmware.
static void meldung_einreihen(const testergebnis_t *quelle)
{
    cJSON *json = lauf_json_erstellen(quelle, false);
    free(versand);
    versand = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
}

// Minimale Laufzeit fuer QEMUs RISC-V-Board (UART und Semihosting-Exit).
struct _reent *__getreent(void) { return _impure_ptr; }
int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, struct stat *status) { (void)fd; status->st_mode=S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int offset, int direction) { (void)fd; (void)offset; (void)direction; return 0; }
int _read(int fd, void *buffer, size_t size) { (void)fd; (void)buffer; (void)size; return 0; }
int _getpid(void) { return 1; }
int _kill(int pid, int signal) { (void)pid; (void)signal; return -1; }
extern char _end;
void *_sbrk(int amount)
{
    static char *next;
    if (!next) next = &_end;
    if (next + amount >= (char *)0x80f00000) { errno = ENOMEM; return (void *)-1; }
    char *old = next; next += amount; return old;
}
int _write(int fd, const void *data, size_t size)
{
    (void)fd;
    const char *text = data;
    for (size_t i = 0; i < size; i++) *(volatile char *)0x10000000 = text[i];
    return (int)size;
}
__attribute__((noreturn)) void _exit(int code)
{
    uint32_t args[2] = {0x20026, (uint32_t)code};
    register uint32_t operation __asm__("a0") = 0x20;
    register void *argument __asm__("a1") = args;
    __asm__ volatile(".option push\n.option norvc\nslli zero,zero,31\nebreak\nsrai zero,zero,7\n.option pop"
                     : : "r"(operation), "r"(argument) : "memory");
    for (;;) {}
}
__attribute__((naked, section(".text.start"))) void _start(void)
{
    __asm__ volatile(".option push\n.option norelax\nla gp,__global_pointer$\n.option pop\n"
                     "la sp,__stack_top\nli t0,0x6000\ncsrs mstatus,t0\ncall test_entry\n");
}
static void pruefen(bool bedingung, const char *text)
{
    if (!bedingung) { printf("FEHLER: %s\n", text); fflush(stdout); _exit(1); }
    pruefungen++;
}
static void auftrag_starten(uint32_t id, uint32_t anzahl, const int profil[8])
{
    cJSON_Delete(auftrag.args);
    auftrag = (auftrag_t){.aktiv=true, .id=id, .anzahl=anzahl};
    memcpy(auftrag.profil, profil, sizeof(auftrag.profil));
    auftrag.args = cJSON_Parse("{\"charge\":\"A17\",\"nested\":{\"x\":2},\"gemessen\":\"alt\"}");
}
static cJSON *feld(cJSON *objekt, const char *name) { return cJSON_GetObjectItemCaseSensitive(objekt, name); }
static void api_testen(void)
{
    const char *valid="{\"profile\":[0,1,2,3,-1,-1,-1,-1],\"count\":3,\"args\":{\"charge\":\"A17\"}}";
    const char *invalid[]={"", "{}", "[]", "broken", "{} trailing",
        "{\"profile\":[0,1,2,3,-1,-1,-1,-1],\"count\":0}",
        "{\"profile\":[0,1,2,3,-1,-1,-1,-1],\"count\":1.5}",
        "{\"profile\":[0,1,2,3,-1,-1,-1,-1],\"count\":4294967296}",
        "{\"profile\":[0,1,2,3,-1,-1,-1,-1],\"count\":\"3\"}",
        "{\"profile\":[0,1,2],\"count\":1}",
        "{\"profile\":[8,1,2,3,-1,-1,-1,-1],\"count\":1}",
        "{\"profile\":[-2,1,2,3,-1,-1,-1,-1],\"count\":1}",
        "{\"profile\":[0.5,1,2,3,-1,-1,-1,-1],\"count\":1}",
        "{\"profile\":[true,1,2,3,-1,-1,-1,-1],\"count\":1}",
        "{\"profile\":[0,1,2,3,-1,-1,-1,-1],\"count\":1,\"args\":null}",
        "{\"profile\":[0,1,2,3,-1,-1,-1,-1],\"count\":1,\"args\":[]}"};
    for(unsigned int i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        httpd_req_t req={.body=invalid[i],.content_len=strlen(invalid[i])};
        auftrag_anlegen(&req);
        pruefen(http_status==400 && !auftrag.aktiv,"Ungueltiger Auftrag: 400, keine Zustandsaenderung");
    }
    httpd_req_t req={.body=valid,.content_len=2049};
    auftrag_anlegen(&req); pruefen(http_status==400,"Body-Grenze 2048");
    req=(httpd_req_t){.body=valid,.content_len=strlen(valid),.timeout=true};
    auftrag_anlegen(&req); pruefen(http_status==408 && !auftrag.aktiv,"Empfangstimeout: 408");
    req=(httpd_req_t){.body=valid,.content_len=strlen(valid)};
    auftrag_anlegen(&req);
    pruefen(http_status==201 && auftrag.aktiv && auftrag.anzahl==3,"Gueltiger Auftrag: 201");
    uint32_t id=auftrag.id;
    req.offset=0; auftrag_anlegen(&req);
    pruefen(http_status==409 && auftrag.id==id,"Aktiver Auftrag: 409 ohne Ersetzen");
    aktueller_auftrag_senden(&req);
    pruefen(http_status==200 && feld(feld(antwort,"job"),"job_id")->valueint==(int)id,"GET aktueller Auftrag");
    pruefen(!feld(feld(feld(antwort,"job"),"args"),"gemessen"),"GET Auftrag behaelt Original-args");
    auftrag_abbrechen(&req);
    pruefen(http_status==200 && !auftrag.aktiv,"DELETE: 200 cancelled");
    auftrag_abbrechen(&req); pruefen(http_status==404,"DELETE ohne Auftrag: 404");
    aktueller_auftrag_senden(&req);
    pruefen(http_status==200 && cJSON_IsNull(feld(antwort,"job")),"GET ohne Auftrag: job null");
    naechste_auftrag_id=0; req.offset=0;
    auftrag_anlegen(&req); pruefen(http_status==503 && !auftrag.aktiv,"Erschoepfte IDs: 503");
    naechste_auftrag_id=2;
    sperre_ok=false; req.offset=0;
    auftrag_anlegen(&req); pruefen(http_status==500 && !auftrag.aktiv,"API-Sperrfehler: 500");
    sperre_ok=true;
    req=(httpd_req_t){.body="{\"profile\":[0,1,2,3,-1,-1,-1,-1],\"count\":4294967295}"};
    req.content_len=strlen(req.body); auftrag_anlegen(&req);
    pruefen(http_status==201 && auftrag.anzahl==UINT32_MAX && cJSON_IsObject(auftrag.args),"Maximaler count und optionale args");
    auftrag_abbrechen(&req);
}
static void test_entry(void) __attribute__((used));
static void test_entry(void)
{
    api_testen();
    const int gerade4[8] = {0,1,2,3,-1,-1,-1,-1};
    int matrix[8][8] = {{0}};
    for (int i=0; i<4; i++) matrix[i][i]=1;
    int normal, rueckwaerts;
    pruefen(profil_pruefen(matrix, gerade4, &normal, &rueckwaerts) && normal==0, "gerade4 OK");
    matrix[0][0]=0;
    pruefen(!profil_pruefen(matrix, gerade4, &normal, &rueckwaerts) && normal==1, "Unterbrechung FEHLER");
    matrix[0][1]=1;
    pruefen(!profil_pruefen(matrix, gerade4, &normal, &rueckwaerts) && normal==2, "Falsches Ziel: zwei Abweichungen");
    matrix[0][0]=1;
    pruefen(!profil_pruefen(matrix, gerade4, &normal, &rueckwaerts) && normal==1, "Mehrfachverbindung FEHLER");
    const int asymmetrisch[8]={1,2,-1,-1,-1,-1,-1,-1};
    memset(matrix,0,sizeof(matrix)); matrix[1][0]=1; matrix[2][1]=1;
    pruefen(profil_pruefen(matrix,asymmetrisch,&normal,&rueckwaerts) && normal>0 && rueckwaerts==0, "Umgekehrte Richtung");

    auftrag_starten(17,3,gerade4);
    memset(matrix,0,sizeof(matrix)); for(int i=0;i<4;i++) matrix[i][i]=1;
    matrix[0][1]=1;
    pruefen(messung_abschliessen(17,false,matrix,false)==ESP_OK, "FEHLER-Lauf speicherbar");
    cJSON *run=lauf_json_erstellen(&ergebnisse[0],true);
    cJSON *args=feld(run,"args"), *gemessen=feld(run,"measured_profile");
    pruefen(cJSON_Compare(gemessen,feld(args,"gemessen"),true), "Messprofil und args.gemessen identisch");
    pruefen(cJSON_GetArraySize(cJSON_GetArrayItem(gemessen,0))==2, "Mehrere Ziele als Liste");
    pruefen(cJSON_GetArrayItem(gemessen,4)->valueint==-1, "Offen als -1");
    pruefen(feld(args,"charge") && strcmp(feld(args,"charge")->valuestring,"A17")==0, "Charge erhalten");
    pruefen(feld(feld(args,"nested"),"x")->valueint==2, "Verschachtelte Zusatzdaten erhalten");
    pruefen(cJSON_IsString(feld(auftrag.args,"gemessen")), "Originalauftrag unveraendert");
    pruefen(feld(run,"error_count")->valueint==1 && cJSON_GetArraySize(feld(run,"errors"))==1, "Fehlerdetails zaehlen");
    cJSON *report=cJSON_Parse(versand);
    pruefen(!feld(report,"gemeldet") && cJSON_Compare(feld(report,"args"),args,true), "Servermeldung enthaelt dieselben Zusatzdaten");
    cJSON_Delete(report); cJSON_Delete(run);

    // Alle Kombinationen eines Anschlusses: offen, Einzelziel und mehrere Ziele.
    testergebnis_t kombination={.ok=true};
    kombination.args=cJSON_CreateObject();
    for(int mask=0;mask<256;mask++) {
        kombination.gemessen[0]=(uint8_t)mask;
        cJSON *profil=messprofil_json(&kombination);
        cJSON *erster=cJSON_GetArrayItem(profil,0);
        int bits=0, letztes=-1;
        for(int b=0;b<8;b++) if(mask&(1<<b)){bits++;letztes=b;}
        bool korrekt=cJSON_GetArraySize(profil)==8;
        if(bits<2) korrekt=korrekt && cJSON_IsNumber(erster) && erster->valueint==letztes;
        else {
            korrekt=korrekt && cJSON_IsArray(erster) && cJSON_GetArraySize(erster)==bits;
            int index=0;
            for(int b=0;b<8;b++) if(mask&(1<<b))
                korrekt=korrekt && cJSON_GetArrayItem(erster,index++)->valueint==b;
        }
        pruefen(korrekt,"Messprofil fuer jede Kombination von B-Zielen");
        cJSON_Delete(profil);
    }
    kombination.ok=false; kombination.umgekehrt=true;
    memcpy(kombination.profil,asymmetrisch,sizeof(kombination.profil));
    memset(kombination.gemessen,0,sizeof(kombination.gemessen));
    kombination.gemessen[0]=1; kombination.gemessen[1]=1; kombination.gemessen[2]=2;
    run=lauf_json_erstellen(&kombination,false);
    pruefen(feld(run,"error_count")->valueint==1 &&
            strcmp(feld(run,"comparison_direction")->valuestring,"reversed")==0,
            "Fehlerdiagnose gegen umgekehrtes Sollprofil");
    cJSON_Delete(run); cJSON_Delete(kombination.args);

    auftrag.aktiv=false;
    pruefen(messung_abschliessen(17,true,matrix,false)==ESP_ERR_INVALID_STATE && ergebnis_anzahl==1, "Abgebrochene Messung verworfen");
    auftrag_starten(18,21,gerade4);
    pruefen(messung_abschliessen(17,true,matrix,false)==ESP_ERR_INVALID_STATE && auftrag.abgeschlossen==0, "Alter Auftrag veraendert neuen Auftrag nicht");
    sperre_ok=false;
    pruefen(messung_abschliessen(18,true,matrix,false)==ESP_FAIL && auftrag.abgeschlossen==0, "Gesperrter Zugriff veraendert nichts");
    sperre_ok=true; matrix[0][1]=0;
    for(int i=0;i<21;i++) pruefen(messung_abschliessen(18,true,matrix,false)==ESP_OK,"Lauf gespeichert");
    pruefen(ergebnis_anzahl==20 && !auftrag.aktiv && auftrag.abgeschlossen==21, "Speicher begrenzt und Auftrag beendet");
    unsigned int start=(ergebnis_position+20-ergebnis_anzahl)%20;
    for(unsigned int i=0;i<20;i++) {
        testergebnis_t *lauf=&ergebnisse[(start+i)%20];
        pruefen(lauf->job_id==18 && lauf->lauf==i+2,"Laeufe 2 bis 21 in Reihenfolge");
    }
    httpd_req_t req={0}; testergebnisse_senden(&req);
    cJSON *runs=feld(antwort,"runs");
    pruefen(http_status==200 && cJSON_GetArraySize(runs)==20,"GET runs enthaelt 20 Laeufe");
    pruefen(feld(cJSON_GetArrayItem(runs,0),"lauf")->valueint==2 &&
            feld(cJSON_GetArrayItem(runs,19),"lauf")->valueint==21,"API-Reihenfolge nach Ueberschreiben");
    run=lauf_json_erstellen(&ergebnisse[(ergebnis_position+19)%20],true);
    pruefen(!feld(run,"errors") && cJSON_IsNumber(cJSON_GetArrayItem(feld(run,"measured_profile"),0)),"OK ohne Fehlerfelder, Einzelziel als Zahl");
    cJSON_Delete(run);
    printf("Firmware-C-Tests bestanden: %u Pruefungen\n",pruefungen);
    fflush(stdout); _exit(0);
}
