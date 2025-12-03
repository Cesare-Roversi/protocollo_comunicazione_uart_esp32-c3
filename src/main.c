#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include <string.h>

#include "structs.h"

//* _______________________________________ CONSTS e STRUCTS

#define U_SLAVE_TX_PIN 10
#define U_SLAVE_RX_PIN 9
#define U_MASTER_TX_PIN 12
#define U_MASTER_RX_PIN 11
#define U_BUF_SIZE 1024 //i bit dei messaggi che si accodano prima di essere fisicamente trasmessi

#define LED_GPIO 8

//ids
#define ROOT_ID 0 //perchè sì
#define UNKNOWN_ID -1
#define INVALID_ID -2 

uint8_t MASTER_ID = 0; //li ho hardcodati nel mockup c'è un protocollo di hello che forse funziona
uint8_t SELF_ID = 1;
uint8_t SLAVE_ID = 2;

//handles:
TaskHandle_t h_task_led;

//code x tutti i tipi di comandi diversi
QueueHandle_t h_queue_command_01;
QueueHandle_t h_queue_command_02;
QueueHandle_t h_queue_handshake;

//code x inviare messaggi
QueueHandle_t h_queue_send_to_slave;
QueueHandle_t h_queue_send_to_master;

typedef struct{
  enum{
    U_MASTER = UART_NUM_0,
    U_SLAVE = UART_NUM_1,
  }select_uart;
  QueueHandle_t select_queue;
} InfoUART;

void print_info_uart_struct(InfoUART* info){
  printf("\nselect_uart: %d\n", info->select_uart);
  printf("select_queue: %p\n\n", (void*)info->select_queue);
  fflush(stdout);
}

const char* get_role_name(int role) {
    if (role == 0) {
        return "MASTER";
    } else if (role == 1) {
        return "SLAVE";
    } else {
        return "UNKNOWN";
    }
}


//* _______________________________________UART RECEIVE
void print_msg_struct(Msg* msg){
  printf("HEAP PT: %p \n", msg);
  printf("Sender: %d\n", msg->sender_id);
  printf("Target: %d\n", msg->target_id);
  printf("Type: %d\n", msg->type);

  if (msg->type == type_command_01) {
      printf("str1: %s\n", msg->payload.payload_command_01.str1);
      printf("str2: %s\n", msg->payload.payload_command_01.str2);
  } else if (msg->type == type_command_02) {
      printf("num1: %d\n", msg->payload.payload_command_02.num1);
      printf("num2: %f\n\n", msg->payload.payload_command_02.num2);
  } else{
    printf("type non riconosciuto\n");
  }
  printf("\n\n");
}


void sort_new_msg(Msg *msg){
  if(msg->type == type_command_01){
    xQueueSend(h_queue_command_01, &msg, portMAX_DELAY);
  }else if (msg->type == type_command_02){
    xQueueSend(h_queue_command_02, &msg, portMAX_DELAY);
  }else if (msg->type == type_handshake){
    xQueueSend(h_queue_handshake, &msg, portMAX_DELAY);
  }else{
    printf("ERRORE: type: %i , non esiste\n", msg->type);
  }
  //....
}


void task_receive_uart(void *arg){
  InfoUART* info_uart = (InfoUART*) arg;

  while (1){
    Msg *msg = malloc(sizeof(Msg)); //!RICEVE DEI BYTE, DEVE ALLOCARLI LUI NELL'HEAP
    int bytes_received = 0;

    while(bytes_received < sizeof(Msg)){
      int n = uart_read_bytes(info_uart->select_uart, ((uint8_t*)msg)+bytes_received, sizeof(*msg)-bytes_received, portMAX_DELAY);
      bytes_received += n;
    }
    bytes_received = 0;

    // printf("puntatore messaggio: %p\n", msg);


    char* role = get_role_name(info_uart->select_uart);
    if(msg->target_id == SELF_ID){
      printf("SONO: %d, HO RICEVUTO DA: %s, il messggio E' PER ME (non verra' ritrasmesso):\n", SELF_ID, role);
      print_msg_struct(msg);
      sort_new_msg(msg);
    }else{
      int tp = (int)!(bool)info_uart->select_uart;
      char* tpr = get_role_name(tp);
      printf("SONO: %d, HO RICEVUTO DA: %s, il messaggio NON E' PER ME, verra' ritrasmesso a: %s :\n", SELF_ID, role, tpr);
      print_msg_struct(msg);
      xQueueSend(info_uart->select_queue, &msg, portMAX_DELAY);
    }
  }

  printf("186 FR DI: %p \n", info_uart);
  free(info_uart);

  //-// non faccio il free del messagio qui, lo fa il consumer
}



//* _______________________________________UART SEND
//! PROBLEMA QUI:
void task_send_uart(void *arg){

  InfoUART* info_uart = (InfoUART*) arg;
  print_info_uart_struct(info_uart);

  while (1) {
      
    Msg *msg = NULL;
    xQueueReceive(info_uart->select_queue, &msg, portMAX_DELAY);

    char* role = get_role_name(info_uart->select_uart);
    printf("SONO: %d, INVIO A: %s, IL SEGUENTE MESSAGGIO:\n", SELF_ID, role);
    print_msg_struct(msg);

    size_t to_send = sizeof(*msg);
    size_t sent = 0;
    while (sent < to_send) {
        int n = uart_write_bytes(info_uart->select_uart, (const char *)msg + sent, to_send - sent);
        if (n > 0) {
            sent += (size_t)n;
        } else {
            // piccolo delay se il driver non scrive nulla
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    printf("221 FR DI: %p \n", msg);
    free(msg);
  }
  printf("224 FR DI: %p \n", info_uart);
  free(info_uart);
}


//* _______________________________________ EXECUTE COMMANDS
void task_execute_command_01(void *arg){
  while(1){
    Msg *msg = NULL;
    xQueueReceive(h_queue_send_to_slave, &msg, portMAX_DELAY);
    printf("execute_command_01");
    vTaskDelay(pdMS_TO_TICKS(10000));

    printf("237 FR DI: %p \n", msg);
    free(msg);
  }
}


void task_execute_command_02(void *arg){
  while(1){
    Msg *msg = NULL;
    xQueueReceive(h_queue_send_to_slave, &msg, portMAX_DELAY);
    printf("execute_command_02");
    vTaskDelay(pdMS_TO_TICKS(9000));

    printf("250 FR DI: %p \n", msg);
    free(msg);
  }
}

//* _______________________________________ ON START (LED + INIT UART)

int L_DELAY = 200;
void task_led(void *info){
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
  
  while(1){
    gpio_set_level(LED_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(L_DELAY));
    gpio_set_level(LED_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(L_DELAY));
  }
}



void init_uart(uart_port_t uart_num, int rx_pin, int tx_pin) {
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    // Installa il driver (Buffer RX, no Buffer TX, no coda eventi)
    uart_driver_install(uart_num, U_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(uart_num, &uart_config);
    // Assegna i pin tramite la Matrix
    uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}


//* _______________________________________ GESTIONE DI HANDSHAKE
void send_handshake_msg(int to_whom, HandshakeType handshake_type){
  Msg* hello_msg = malloc(sizeof(Msg));
  hello_msg->sender_id = SELF_ID;
  hello_msg->target_id = to_whom; //-1 = IDK
  hello_msg->type = type_handshake;
  hello_msg->payload.payload_handshake.type = handshake_type;
  hello_msg->payload.payload_handshake.my_id = SELF_ID;
  hello_msg->payload.payload_handshake.my_master_id = MASTER_ID;
  hello_msg->payload.payload_handshake.my_master_id = SLAVE_ID;

  if(handshake_type == type_hello){
    if(to_whom == -1 || to_whom == MASTER_ID){
      xQueueSend(h_queue_send_to_master, &hello_msg, portMAX_DELAY);
    }else if(to_whom == SLAVE_ID){
      xQueueSend(h_queue_send_to_slave, &hello_msg, portMAX_DELAY);
    }else{
      printf("ERRORE: type_hello è solo valido tra nodi adiacenti\n");
    }

  }else if(handshake_type == type_report_to_root){
    if(to_whom == 0){
      xQueueSend(h_queue_send_to_master, &hello_msg, portMAX_DELAY);
    }else{
      printf("ERRORE: type_report_to_root è mandato unicamente a ROOT\n");
    }

  }else{
    printf("ERRORE: handshake_type: %i , non esiste\n", handshake_type);
  }
  
}


void handle_hello(){
  Msg *msg = NULL;
  xQueueReceive(h_queue_handshake, &msg, portMAX_DELAY);
  if(msg->target_id == -1){
    if(SLAVE_ID == -1){
      SLAVE_ID = msg->payload.payload_handshake.my_id;
      send_hello(SLAVE_ID);
    }else{
      printf("ERRORE: IO ho già uno slave\n");
    }
  }
}


//TODO _______________________________________ SOLO MASTER - MAPPA NODI
#define NODES_ARR_SIZE 10 

typedef struct{
  int id;
  int master_id;
  int slave_id;
}node;
node nodes_arr[NODES_ARR_SIZE];
int nodes_arr_len =0;

void print_nodes_arr_len(){

}

void init_nodes_arr(){
  nodes_arr[0] = (node){.id=0, .master_id=-2, .slave_id=-1};
  nodes_arr_len = 1;
}

int find_and_update(int id, int master_id, int slave_id){

  for(int i=0; i<nodes_arr_len; i++){
    if(nodes_arr[i].id == id){
      if(master_id >= 0){ //se non è -1 (INFO VECCHIA? assumo sia aggiornato)
        nodes_arr[i].master_id = master_id;
      } 
      if(slave_id >= 0){
        nodes_arr[i].slave_id = slave_id;
      }
      return true;
    }
  }

  return false;
}

void task_handle_report(void *arg){ //!NON TESTATA
  while(1){
    Msg* msg;
    xQueueReceive(h_queue_handshake, &msg, portMAX_DELAY);
    if(!(msg->type == type_handshake && msg->payload.payload_handshake.type == type_report_to_root)){
      printf("ERRORE: type o handshake_type\n");
      return;
    }

    int id = msg->payload.payload_handshake.my_id;
    int master_id = msg->payload.payload_handshake.my_master_id;
    int slave_id = msg->payload.payload_handshake.my_slave_id;

    bool found = find_and_update(id, master_id, slave_id);
    if(found == true && nodes_arr_len < NODES_ARR_SIZE){
      nodes_arr[nodes_arr_len] = (node){.id=id, .master_id=master_id, .slave_id=slave_id};
      find_and_update(master_id, -2, id);
      nodes_arr_len +=1;
    }
    free(msg);
  }
}

//* _______________________________________ MAIN e TEST

void test(){
  //messaggio fa il giro 1 -> 2 -> 1
  //sono reciprocamente sia master che slave di se stessi
  
  Msg* prova = malloc(sizeof(Msg));
  prova->sender_id = 1;
  prova->target_id = 1;
  prova->type = type_command_01;
  char* s1 = prova->payload.payload_command_01.str1;
  char* s2 = prova->payload.payload_command_01.str2;
  strcpy(s1, "ciao1");
  strcpy(s2, "ciao2");

  printf("\nciao: %p\n", prova);

  xQueueSend(h_queue_send_to_slave, &prova, portMAX_DELAY); 
}

void app_main(void){
  vTaskDelay(pdMS_TO_TICKS(1500)); //!1. FAI UPLOAD E MONITOR, 2.DAGLI IL TEMPO AL MONTOR DI PARTIRE

  //*inizializzo le code
  //le code contengono punatori all'heap
  h_queue_command_01 = xQueueCreate(10, sizeof(Msg*));
  h_queue_command_02 = xQueueCreate(10, sizeof(Msg*));
  h_queue_handshake = xQueueCreate(10, sizeof(Msg*));
  h_queue_send_to_slave = xQueueCreate(10, sizeof(Msg*));
  h_queue_send_to_master = xQueueCreate(10, sizeof(Msg*));

  //*inizializzo le UART
  init_uart(U_MASTER, U_MASTER_RX_PIN, U_MASTER_TX_PIN);
  init_uart(U_SLAVE, U_SLAVE_RX_PIN, U_SLAVE_TX_PIN);

  //*creo le task
  xTaskCreate(task_led, "task_led", 2048, NULL, 1, &h_task_led);

  InfoUART* info_receive_master = malloc(sizeof(InfoUART)); 
  info_receive_master->select_uart = U_MASTER;
  info_receive_master->select_queue = h_queue_send_to_slave;//accoda a coda slave
  xTaskCreate(task_receive_uart, "task_receive_uart_master", 5000, (void*)info_receive_master, 1, NULL);

  InfoUART* info_receive_slave = malloc(sizeof(InfoUART));
  info_receive_slave->select_uart = U_SLAVE;
  info_receive_slave->select_queue = h_queue_send_to_master;//accoda a coda master
  xTaskCreate(task_receive_uart, "task_receive_uart_slave", 5000, (void*)info_receive_slave, 1, NULL);

  InfoUART* info_send_master = malloc(sizeof(InfoUART)); 
  info_send_master->select_uart = U_MASTER;
  info_send_master->select_queue = h_queue_send_to_master;//predo da coda master e invio a master
  //!problema qui
  xTaskCreate(task_send_uart, "task_send_uart_master", 5000, (void*)info_send_master, 1, NULL);

  InfoUART* info_send_slave = malloc(sizeof(InfoUART)); 
  info_send_slave->select_uart = U_SLAVE;
  info_send_slave->select_queue = h_queue_send_to_slave;//predo da coda slave e invio a slave
  xTaskCreate(task_send_uart, "task_send_uart_slave", 5000, (void*)info_send_slave, 1, NULL);

  xTaskCreate(task_handle_report, "task_handle_report", 5000, NULL, 1, NULL); //!UNTESTED!!!


  //*test
  SELF_ID = 2;

  if(SELF_ID == 1){ 
    L_DELAY = 2000;
    MASTER_ID = 2;
    SLAVE_ID = 2;

  }else if(SELF_ID == 2){
    L_DELAY = 200;
    MASTER_ID = 1;
    SLAVE_ID = 1;
  }

  while(SELF_ID == 1){
    test();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
