#include "Messages.h"

#define MASTER_TX_PIN 17
#define MASTER_RX_PIN 16
#define SLAVE_TX_PIN 15
#define SLAVE_RX_PIN 14
#define BAUD 115200

/*
- Ho deciso che lo slave si presenta sempre con un MSG_HELLO e il master gli manda un ack con un MSG_HELLO
- Un blocco manda un messaggio (MY_MASTER_ID, MY_ID, MY_SLAVE_ID) al ROOT quando tutti e 3 i campi sono != -1
- ROOT ha l'ID=0 perchè si
*/

int MASTER_ID = -1; //son relativi
int SELF_ID = 1;
int SLAVE_ID = -1;

bool root_info_already_sent = 0;

HardwareSerial COM_DEBUG(0); //Il mio master
HardwareSerial COM_WITH_MASTER(1); 
HardwareSerial COM_WITH_SLAVE(2); //Il mio slave

void setup() {
  COM_DEBUG.begin(115200);
  COM_WITH_MASTER.begin(BAUD, SERIAL_8N1, MASTER_RX_PIN, MASTER_TX_PIN);
  COM_WITH_SLAVE.begin(BAUD, SERIAL_8N1, SLAVE_RX_PIN, SLAVE_TX_PIN);

  Message hello_msg;
  hello_msg.dest_id = NEAREST_CUBE; //non so chi sia il mio master
  hello_msg.type = MSG_HELLO;
  hello_msg.payload.hello_payload.id = SELF_ID;
  COM_WITH_MASTER.begin(BAUD, SERIAL_8N1, MASTER_RX_PIN, MASTER_TX_PIN);
}

void loop() {
    if (COM_WITH_MASTER.available() >= sizeof(Message)) {
        Message msg_from_master;
        COM_WITH_MASTER.readBytes((uint8_t*)&msg_from_master, sizeof(Message));

        if(msg_from_master.dest_id == SELF_ID || msg_from_master.dest_id == NEAREST_CUBE) {
            if (msg_from_master.type == MSG_HELLO){
                MASTER_ID = msg_from_master.payload.hello_payload.id;

            }else if (msg_from_master.type == MSG_SERVO){
                int servo_n = msg_from_master.payload.servo_payload.servo_n;
                float servo_rad = msg_from_master.payload.servo_payload.radian;
                //E CHIAMI LA FUNZIONE DI CONTROLLO SERVO
            }

        }else{ //I send to my SLAVE
            COM_WITH_SLAVE.write((uint8_t*)&msg_from_master, sizeof(Message));
        }
    }

    if (COM_WITH_SLAVE.available() >= sizeof(Message)) {
        Message msg_from_slave;
        COM_WITH_SLAVE.readBytes((uint8_t*)&msg_from_slave, sizeof(Message));

        if(msg_from_slave.dest_id == SELF_ID || msg_from_slave.dest_id == NEAREST_CUBE) {
            if (msg_from_slave.type == MSG_HELLO){
                SLAVE_ID = msg_from_slave.payload.hello_payload.id;
                Message ack;
                ack.dest_id = SLAVE_ID;
                ack.type = MSG_HELLO;
                ack.payload.hello_payload.id = SELF_ID; 
                COM_WITH_SLAVE.write((uint8_t*)&ack, sizeof(Message)); //send ack 
            }

        }else{ //I send to my MASTER
            COM_WITH_MASTER.write((uint8_t*)&msg_from_slave, sizeof(Message));
        }
    }

    if(!root_info_already_sent && MASTER_ID != -1 && SLAVE_ID != -1){ //aspetto di avere tutte le info
        root_info_already_sent = 1;
        Message for_root;
        for_root.dest_id = ROOT_ID;
        for_root.type = MSG_ROOT_INFO;
        for_root.payload.root_info_payload.my_id = SELF_ID;
        for_root.payload.root_info_payload.my_master_id = MASTER_ID;
        for_root.payload.root_info_payload.my_slave_id = SLAVE_ID;
        COM_WITH_MASTER.write((uint8_t*)&for_root, sizeof(Message));
    }
}


