#include <stdio.h>
#include <string.h>

//definiti da me:
#include "msg_structs.h"
#include "utils_uart_comms.h"


//* _______________________________________ GESTIONE DI HANDSHAKE



//todo STO RISCRIVENDO COMPLETAMENTE QUESTO FILE QUA SOTTO, ANCHE HANDLER REPORT é DA RIVEDERE


void send_report_to_root(){
  Payload p;
  p.payload_report.my_id = SELF_ID;
  p.payload_report.my_master_id = MASTER_ID;
  p.payload_report.my_slave_id = SLAVE_ID;

  // If I'm the root, handle the report locally
  if(SELF_ID == ROOT_ID){
    receive_new_report(p.payload_report); //chiami direttamente il modulo
    return;
  }

  // If I don't yet know my master, defer sending a report to avoid
  // producing stale reports that would later overwrite a fresher state
  // on the root (see issue with buffered reports).
  if(MASTER_ID == UNKNOWN_ID){
    printf("[HANDSHAKE] MASTER unknown for SELF %d, deferring report\n", SELF_ID);
    return;
  }

  Msg* m = create_msg(SELF_ID, ROOT_ID, type_report, p);
  send_msg_to_master(m);
}

const int PING_SLAVE_WAIT_FOR_ACK_MAX_DELAY = 500; //! fix: do alle schedine 5s per rispondere ad un hanshake invece che 2s
const int PING_MASTER_WAIT_FOR_ACK_MAX_DELAY = 500;

const int PING_SLAVE_SEND_NEW_HANDSHAKE_DELAY = 500;
const int PING_MASTER_SEND_NEW_HANDSHAKE_DELAY = 500;

volatile int last_MtS_ack_sender_id = -1;
volatile bool received_MtS_ack = false;

volatile int last_StM_ack_sender_id = -1;
volatile bool received_StM_ack = false;

TaskHandle_t handle_task_ping_slave = nullptr;
TaskHandle_t handle_task_ping_master = nullptr;

void task_ping_slave(void* info){ // mando MtS a slave
  handle_task_ping_slave = xTaskGetCurrentTaskHandle();

  while(1){
    Payload p;
    p.payload_handshake.handshake_type = type_MtS;
    Msg* msg = create_msg(SELF_ID, UNKNOWN_ID, type_handshake, p); 

    received_MtS_ack = false; 
    /*
    type_MtS ->
    type_StM <-
    TIMEOUT - NO RESPONSE
    Even if it doesnt have a response it knows not to delete the new slave
    */
    int SLAVE_ID_WHEN_I_SENT_THE_MESSAGE = SLAVE_ID; 
    send_msg_to_slave(msg);
    
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(PING_SLAVE_WAIT_FOR_ACK_MAX_DELAY)); //!NOTIFY

    if(received_MtS_ack){ // slave esiste
      if(last_MtS_ack_sender_id != SLAVE_ID){ // è diverso da slave ID
        printf(">>> SLAVE CHANGED FROM %d TO %d\n", SLAVE_ID, last_MtS_ack_sender_id);

        SLAVE_ID = last_MtS_ack_sender_id; 
        send_buffered_messages_to_slave();
        send_report_to_root();
      }
    } else if(SLAVE_ID_WHEN_I_SENT_THE_MESSAGE != UNKNOWN_ID) { // slave non esiste
      printf(">>> SLAVE %d DOESNT RESPOND, I ASSUME HE ISNT THERE (SLAVE_ID = -1)\n", SLAVE_ID);
      SLAVE_ID = UNKNOWN_ID; 
      send_report_to_root();
    }

    vTaskDelay(pdMS_TO_TICKS(PING_SLAVE_SEND_NEW_HANDSHAKE_DELAY));
  }
}


void task_ping_master(void* info){ 
  if(SELF_ID == ROOT_ID){ //it shouldn't be the case.
    vTaskDelete(nullptr);
  }
  handle_task_ping_master = xTaskGetCurrentTaskHandle();

  while(1){
    Payload p;
    p.payload_handshake.handshake_type = type_StM;
    Msg* msg = create_msg(SELF_ID, UNKNOWN_ID, type_handshake, p); 

    received_StM_ack = false; 
    int MASTER_ID_WHEN_I_SENT_THE_MESSAGE = MASTER_ID;
    send_msg_to_master(msg);

    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(PING_MASTER_WAIT_FOR_ACK_MAX_DELAY)); //!NOTIFY
    
    if(received_StM_ack){ // master esiste
      if(last_StM_ack_sender_id != MASTER_ID){ // è diverso da master ID
        printf(">>> MASTER CHANGED FROM %d TO %d\n", MASTER_ID, last_StM_ack_sender_id);

        MASTER_ID = last_StM_ack_sender_id; 
        send_buffered_messages_to_master();
        send_report_to_root();
      }
    } else if(MASTER_ID_WHEN_I_SENT_THE_MESSAGE != UNKNOWN_ID) { // master non esiste
      printf(">>> MASTER %d DOESNT RESPOND, I ASSUME HE ISNT THERE (MASTER_ID = -1)\n", MASTER_ID);
      MASTER_ID = UNKNOWN_ID; 
      // todo NON MANDARE IL REPORT NON PUO GESTIRLO
    }

    vTaskDelay(pdMS_TO_TICKS(PING_MASTER_SEND_NEW_HANDSHAKE_DELAY));
  }
}


void task_handle_handshakes(void* info){
  while(1){
    Msg *msg = nullptr;
    xQueueReceive(h_queue_handshake, &msg, portMAX_DELAY);

    if(msg->payload.payload_handshake.handshake_type == type_MtS){ //* il master fa ciao rispondigli
      Payload p;
      p.payload_handshake.handshake_type = type_MtS_ack;
      Msg* nm = create_msg(SELF_ID, UNKNOWN_ID, type_handshake, p);
      send_msg_to_master(nm);

      if(msg->sender_id != MASTER_ID){
        if(SHOW_UART_COMMS_LOGS){
          printf(">>> MASTER CHANGED FROM %d TO %d\n", MASTER_ID, msg->sender_id);}
        MASTER_ID = msg->sender_id; 
        send_buffered_messages_to_master(); //! fix: ho invertito questa righa e quella dopo
        send_report_to_root(); //so che non è -1 in quanto ho ricevuto un messaggio da qualcuno; 
      }

    } else if(msg->payload.payload_handshake.handshake_type == type_MtS_ack){
      last_MtS_ack_sender_id = msg->sender_id;
      received_MtS_ack = true; // FIX: consistency (true instead of 1)

      if(SHOW_UART_COMMS_LOGS)
        printf("DOVREI SVEGLIARMI\n");
      xTaskNotifyGive(handle_task_ping_slave); //ping_slave sends type_MtS
      if(SHOW_UART_COMMS_LOGS)
        printf("MI SONO SVEGLIATO\n");

    } else if(msg->payload.payload_handshake.handshake_type == type_StM){  //* lo slave fa ciao rispondigli
      Payload p;
      p.payload_handshake.handshake_type = type_StM_ack;
      Msg* nm = create_msg(SELF_ID, UNKNOWN_ID, type_handshake, p);
      send_msg_to_slave(nm);

      if(msg->sender_id != SLAVE_ID){
        if(SHOW_UART_COMMS_LOGS)
          printf(">>> SLAVE CHANGED FROM %d TO %d\n", SLAVE_ID, msg->sender_id);
        SLAVE_ID = msg->sender_id; 
        send_buffered_messages_to_slave(); //! fix: ho invertito questa righa e quella dopo
        send_report_to_root();
      }

    } else if(msg->payload.payload_handshake.handshake_type == type_StM_ack){
      last_StM_ack_sender_id = msg->sender_id;
      received_StM_ack = true; 

      if(SHOW_UART_COMMS_LOGS)
        printf("DOVREI SVEGLIARMI\n");
      xTaskNotifyGive(handle_task_ping_master); //ping_master sends type_StM
      if(SHOW_UART_COMMS_LOGS)
        printf("MI SONO SVEGLIATO\n");
    }

    // msg was allocated with `new` in task_receive_uart/create_msg ->
    // must use `delete` to release it. `free` corrupts the C++ heap.
    delete msg;
  }
}

/*
X invia type_MtS / type_StM ciclicamente:
Y capisce chi è X, potrebbe essere cambiato, in caso aggiorna MY_MASTER e MY_SLAVE e manda un report a ROOT;
Y iniva type_MtS_ack / type_StM_ack a X (SOLO IN RISPOSTA AD UN type_MtS / type_StM);
X capisce chi è X, potrebbe essere cambiato, in caso aggiorna MY_MASTER e MY_SLAVE e manda un report a ROOT;
*/
