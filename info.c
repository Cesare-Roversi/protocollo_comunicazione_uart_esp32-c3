/*
- INFO: PER CREARE TASK
xTaskCreate(
    TaskFunction_t pvTaskCode,  / pointer to the task function
    const char * const pcName,  / task name 
    uint32_t usStackDepth,      / stack size in words, 1 word = 4 bytes
    void *pvParameters,         / pointer to data passed to task (can be NULL)
    UBaseType_t uxPriority,     / task priority (higher = better, [0-24])
    TaskHandle_t *pxCreatedTask / optional pointer to store task handle (can be NULL), 
                                / esume/stop/delete/notify/checkstate
);

NON puoi creare task vuote, se raggiungono l'ultima graffa crashano, fai così:
void task_send_uart(void *arg){
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}


- INFO: READ UART

int len = uart_read_bytes(U_SLAVE, data, BUF_SIZE - 1, pdMS_TO_TICKS(1000));
IL DELAY SERVE PERCHÈ QUELLA PARTICOLARE TASK PUÒ LEGGERE MESSAGGI SOLO METRE È BLOCCATA
SU uart_read_bytes(), OGNI VOLTA CHE RUNNA LA FUNZIONE SI BLOCCA E LEGGE MESSAGGI,
SE ARRIVANO MESSAGGI MENTRE LA TASK FACEVA ALTRO SI METTONO IN UNA CODA INTERNA DELL'ESP.

portMAX_DELAY = ASPETTA X SEMPRE

La ESP32 ha 2 buffer:
1.RX buffer, mantenuti fino a lettura da  uart_read_bytes()
2.TX buffer, mantenuti fino a trasmissione effettiva

per le struct sei costretto a leggere i byte nel mentre che arrivano, se non li leggi
si accumulano nel buffer della esp e magari lo riempiono;

anche mandare struct è un problema, sotto manda a ogni botta tutti i byte che sono disponibili
nel buffer di invio dell'esp (ptrebbero essere < size(msg))


- INFO: CODE
QueueHandle_t q = xQueueCreate(1, sizeof(int));

xQueueSend(q, &v, portMAX_DELAY);
xQueueReceive(q, &v, portMAX_DELAY);

il terzo parametro indica il tempo max che la task resta a aspettare sulla funzione se coda troppo piena/vuota,
se timeout != portMAX_DELAY allora restituisce pdPASS o pdFAIL, controllalo;

xQueueSend(info_uart->select_queue, &msg, portMAX_DELAY);
!Send e Receve vogliono il puntatore a cio che è da copiare nella/dalla coda, 
!ES: se vuoi copiare Msg*, vuole Msg**
ES:
Q = xQueueCreate(10, sizeof(Msg*));

Msg* v = ...;
xQueueSend(Q, (Msg**)v, portMAX_DELAY);

///Passo il riferimento all'istanza di v
///Internamente alla funzione:
Msg* v = *(Msg**)RIF ///lo stesso v


- DEBUGGARE 
vTaskDelay(pdMS_TO_TICKS(1000)); 
!1. FAI UPLOAD E MONITOR, 
!2. DAGLI IL TEMPO AL MONTOR DI PARTIRE IMPOSTANDO UN DELAY NEL MAIN
!(LA SCHEDA NON ASPETTA CHE PALTFORMIO ABBIA APERTO IL MONITOR SERIALE)
*/