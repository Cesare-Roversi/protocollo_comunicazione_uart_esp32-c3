# INTRODUZIONE:

## File:
- bridge.cpp
- GLOBAL_VARS.cpp
- init_uart_comms.cpp
- commands.cpp
- handle_handshake.cpp
- led.cpp
- debug_print.cpp
- handle_report.cpp
- uart.cpp
- msg_structs.h
- utils_uart_comms.h

## Scopo e responsabilità:
Implementa comunicazione UART tra le schede:
- flessibile a diversi tipi di messaggi,
- implementa protezione a messaggi mal formati e rumore elettrico che potrebbe essere interpretato come messaggio,
- permette la creazione dinamica della catena di schede, permette di scollegare e ricollegare schede a runtime, la root ricostruisce la catena,
- riceve comandi dal codice del wifi,
- porta messaggi al codice del servo.

## Strutture dati principali:
```cpp
typedef union{
    PayloadCommand01 payload_command_01; //just for mockup
    PayloadCommand02 payload_command_02; // just for mockup
    ........
}Payload;

typedef enum{
    type_command_01, // just for mockup
    type_command_02, //just for mockup
    .......
}MsgType;

typedef struct __attribute__((packed)){
    uint8_t header;
    int sender_id;
    int target_id;
    MsgType type;

    Payload payload;

    uint32_t footer;
}Msg;
```

#### Descrizione:
1 byte di header all'inizio del messaggio e 4 byte di footer alla fine assicurano che il messaggio non sia corrotto.
```
#define HEADER_BYTE 0xAA
#define FOOTER_4_BYTES 0xCAFEBABE
```

`sender_id` e `target_id` sono necessari per il routing tra schede.
Quando il messaggio è arrivato al destinatario, l'enum `MsgType` permette al codice (`sort_new_msg(Msg *msg)`) di appendere il puntatore al messaggio nella coda corretta, in attesa di essere consumato dalla task apposita.
Se si aggiungono nuovi payload bisogna aggiornare le funzioni:
- `sort_new_msg(Msg *msg)`
- `print_msg_struct(Msg* msg)` // utile per il debug

Questa struttura assume che per ogni tipologia di messaggio, un nuovo messaggio finisca (il puntatore) in una coda dedicata a quel tipo, in attesa di essere consumato da una task dedicata a quel tipo:
`xQueueReceive(h_queue_command_01, &msg, portMAX_DELAY);` blocca la task indefinitamente in attesa dell'arrivo di un nuovo messaggio (la task è in sleep, non consuma CPU).

ES:
```cpp
void task_execute_command_01(void *arg){
  while(1){
    Msg *msg = nullptr;
    xQueueReceive(h_queue_command_01, &msg, portMAX_DELAY);
    printf("START: execute_command_01\n");
    vTaskDelay(pdMS_TO_TICKS(4000));
    printf("END: execute_command_01\n");
    delete msg;
  }
}
```

Ovviamente si può variare la struttura.


## Struttura UART:
Abbiamo usato 2 UART per ogni ESP32, entrambe bidirezionali:
```
#define U_WITH_MASTER 0
#define U_WITH_SLAVE 1

#define FROM_MASTER_RX 10
#define TO_MASTER_TX 9
#define FROM_SLAVE_RX 2
#define TO_SLAVE_TX 3
```

Ogni scheda è master di quella successiva nella catena e slave di quella precedente.
La prima scheda è la root: non ha un master, gestisce la daisy chain e la comunicazione wifi.

Queste sono variabili globali:
```
extern int MASTER_ID;
extern int SELF_ID;
extern int SLAVE_ID;
```

Valori di default:
```
#define ROOT_ID 0
#define UNKNOWN_ID -1
```

## Costruzione dinamica della catena:
I messaggi di handshake sono incapsulati in `Msg`.
Payload handshake:

```cpp
typedef enum{
    type_StM, // slave to master
    type_StM_ack, // slave to master ack
    type_MtS, // master to slave
    type_MtS_ack, // master to slave ack
}HandshakeType;
typedef struct{
    HandshakeType handshake_type;
} PayloadHandshake;
```

**`handle_handshake.cpp`:**
Queste 2 task mandano rispettivamente `type_StM` e `type_MtS` periodicamente:
- `task_ping_master(void* info)`
- `task_ping_slave(void* info)`

Poi vanno in sleep per un intervallo di ms:
`ulTaskNotifyTake(...);`
Si svegliano se l'intervallo finisce o se `task_handle_handshakes(void* info)` le sveglia quando riceve una risposta.
Se master/slave non ha risposto, o ha risposto una scheda con ID differente, notificano la root.

`task_handle_handshakes(void* info)` si occupa di ricevere i 4 tipi di messaggi di handshake:
- svegliare `task_ping_master` e `task_ping_slave`,
- mandare gli ack a master/slave,
- mandare report alla root se arrivano messaggi `type_StM` e `type_MtS` inattesi.

**`payload_report`:**
La scheda manda un report alla root quando cambia il suo stato di connessione.
```cpp
typedef struct{
    int my_slave_id;
    int my_id;
    int my_master_id;
}PayloadReport;
```

**`handle_report.cpp`:**
La root, man mano che riceve le struct:

Le salva in un array (funge da dizionario) indicizzato in base a `my_id`, poi cerca di ricostruire la catena controllando che `my_slave_id` dell'N-esimo nodo corrisponda a `my_master_id` dell'N+1-esimo nodo.

Comunica con il codice wifi tramite:
`ProtocolManager::set_num_servos((uint8_t)get_ids_array_len());`

**`bridge.cpp`:**
Il codice UART prende comandi dal codice WiFi esponendo:
```cpp
void convert_servo_instructions(const std::vector<float>& angles)
```


## Comunicazione UART:
**`uart.cpp`**
Gestisce ricezione e invio UART.
#### Ricezione:
Dati problemi di ricezione (di cui parlerò dopo), ci sono delle guardie extra:

1. Per evitare falsi positivi: rimane in loop finché non trova il byte di header.
2. Per evitare messaggi incompleti: se riceve meno byte di `sizeof(Msg)` ricomincia da capo.
3. Per evitare messaggi corrotti: se il footer (4 byte) è sbagliato ricomincia da capo.

Se va tutto bene, estrae il messaggio in base al tipo: il puntatore al messaggio finisce in una coda in attesa che una task lo gestisca.

#### Invio:
Si pone il problema di messaggi inviati a master o slave che non sono ancora collegati: quindi, tranne `type_handshake` (che deve sondare), i messaggi si accumulano e vengono inviati quando il nodo master o slave viene collegato.

Si pone il problema di N task che vogliono accedere al buffer contemporaneamente, risolto con 2 mutex.

Abbiamo:
- `void send_buffered_messages_to_master()`
- `void send_msg_to_master(Msg* msg)`
- `void send_buffered_messages_to_slave()`
- `void send_msg_to_slave(Msg* msg)`

**Problema senza mutex:**
```cpp
void send_msg_to_master(Msg* msg){
    if(MASTER_ID == UNKNOWN_ID && msg->type != type_handshake){
    //! ISTANTE_0: MSG1 SOSPESO QUI
        master_pre_init_buffer.push(msg);
    } else {
        xQueueSend(h_queue_send_to_master, &msg, portMAX_DELAY);
    }
}

// Questa funzione viene chiamata:
void send_buffered_messages_to_master(){...} //! ESEGUE TUTTO

void send_msg_to_master(Msg* msg){
    if(MASTER_ID == UNKNOWN_ID && msg->type != type_handshake){
        master_pre_init_buffer.push(msg); //! ISTANTE_1: MSG1 NEL BUFFER (EFFETTIVAMENTE PERSO)
    } else {
        xQueueSend(h_queue_send_to_master, &msg, portMAX_DELAY);
    }
}
```

## Testing:
**`commands.cpp`**
Contiene dei mockup di comandi usati durante il testing.
Durante il testing le schede erano collegate a una breadboard e i collegamenti (jumper) tra le schede erano numerati.
Il testing è avvenuto tramite monitor seriale e osservando il LED.

**`led.cpp`**
Permette di far lampeggiare la scheda:
- sempre,
- quando riceve un messaggio,
- quando invia un messaggio.

Utile per testare l'UART.

## Problemi:
#### Problemi lato software:
1. Impostazioni UART USB settate male:
   Essendo settate male, mandava messaggi destinati al serial monitor (anche) su UART,
   si mischiavano con le struct `Msg` facendo fallire la ricezione.
   Risolto stampando in esadecimale i byte ricevuti e verificando che i messaggi avevano lunghezza diversa (impossibile: dovrebbero tutti essere `sizeof(Msg)`).

2. Messaggi inviati in ordine errato:
   `send_buffered_messages_to_slave()` / `send_buffered_messages_to_master()` chiamato dopo `send_msg_to_slave()` / `send_msg_to_master()`: poteva capitare che i messaggi bufferati (più vecchi) contraddicessero il messaggio più nuovo.

#### Problemi lato hardware:
Perché l'UART funzioni correttamente:
- le schede devono essere collegate in parallelo alla stessa fonte di energia,
- il ground delle schede deve essere collegato tra di loro.

In un'occasione abbiamo anche invertito i cavi di ricezione e trasmissione, quando ce ne siamo accorti abbiamo cambiato i valori nel codice per farli coincidere.
