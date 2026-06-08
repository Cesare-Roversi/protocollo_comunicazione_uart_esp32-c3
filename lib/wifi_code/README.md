# Architettura Wi-Fi

Il sistema Wi-Fi si compone di tre blocchi principali:

1. **Access Point Manager** `wifi_manager` – Configura ESP32 in modalità Access Point con autenticazione WPA2-PSK, inizializza stack TCP/IP, registra eventi di associazione/disassociazione client tramite event handler FreeRTOS.
2. **Server TCP** `tcp_server` – Task FreeRTOS dedicato: crea socket, lo lega alla porta 3333 su IP statico 192.168.4.1, ascolta un singolo client, esegue chiamate bloccanti `accept` e `recv`. Dati in ingresso bufferizzati e divisi in linee (delimitatore `\n`).
3. **Protocol Manager** `protocol_manager` – Parsing di stringhe ASCII separate da spazi in comandi di movimento. Ogni comando contiene quattro valori float per servo: angolo, velocità, accelerazione, jerk. Controlli di validità applicati (limiti fisici, conteggio parametri, corrispondenza numero servo effettivi). Stringa di risposta (es. `OK` o messaggio errore) inviata al computer.

All'avvio, `init_wifi()` lancia l'Access Point e poi il task del server TCP. Il task si blocca su `accept`: nessuna ISR esplicita necessaria; stack lwIP e driver Wi-Fi gestiscono interrupt hardware internamente. Quando computer remoto si associa e apre connessione TCP, callback `on_connect` invia immediatamente numero servo correnti (`SERVOS <n>`). Da quel punto, ogni linea completa ricevuta dal computer attiva il parser. Comando valido → quattro vettori (angoli, velocità, accelerazioni, jerk) passati al bridge UART; altrimenti errore restituito.

## Codice rappresentativo

```c
// tcp_server.c

auto port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(arg));

// Crea socket streaming con protocollo TCP/IPv4
int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
if (listen_sock < 0) {
    ESP_LOGE(TAG, "Errore creazione socket: errno %d", errno);
    vTaskDelete(nullptr);
    return;
}

// Mette socket in ascolto per accettare richieste di connessione
listen(listen_sock, 1);
ESP_LOGI(TAG, "Listening on 192.168.4.1:%d ...", port);
```

Queste righe costituiscono il nucleo del layer TCP. Mostrano come ESP32 root crea socket streaming, lo lega all'indirizzo noto dell'Access Point, entra in stato di ascolto. Insieme al loop `accept` successivo (omesso per brevità), stabiliscono il server single-client che permette scambio affidabile di comandi testuali con la catena molecubes.

---

## Test

Test eseguiti manualmente: firmware su ESP32-C3, istruzioni `ESP_LOGI` per tracciare ogni passo di inizializzazione (avvio Wi-Fi, assegnazione IP, ascolto). Lato computer, script Python inviava comandi di lunghezza e contenuto variabili.

Problema principale riscontrato: messaggio con più entry servo del numero fisicamente presente causava misinterpretazione dei parametri rimanenti → dati di movimento errati inviati all'hardware. Causa: parser iniziale non verificava conteggio gruppi servo rispetto a configurazione reale. Soluzione: aggiunta validazione tra numero angoli e servo connessi durante verifica del messaggio ricevuto.

---
## Conclusioni e sviluppi futuri

Sistema attuale richiede computer in portata Wi-Fi diretta dell'ESP32 root. Miglioramento principale: connettere ESP32 a rete Wi-Fi esistente come station → trasmissione comandi via internet. Altra feature importante: applicazione migliore lato computer per gestione connessione, che consenta controllo più semplice e rapido del movimento della catena.