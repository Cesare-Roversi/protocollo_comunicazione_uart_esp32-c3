#include "Messages.h"

#define MASTER_TX_PIN 17
#define MASTER_RX_PIN 16
#define SLAVE_TX_PIN 15
#define SLAVE_RX_PIN 14
#define BAUD 115200


int SLAVE_ID = -1;

//ho fatto sta roba perchè le info poterbbero arriavre in ordine sbagliato, 
//ogni volta che ne arriva uno prova a ricostruire la catena
//non gestisce il caso in cui io stacco un cubo
RootInfoPayload connections_dump[100];
int cd_ix = 0;
int connections[100];
int c_ix = 0;
bool CATENA_COMPLETA = 0;

//non saprei come far arrivare i comandi alla root
bool SEND_COMMAND = 0;
int dest_id = -1;
int servo_n = -1;
float radian = -1;

HardwareSerial COM_DEBUG(0); //Il mio master
HardwareSerial COM_WITH_SLAVE(2); //Il mio slave


bool make_connections(){
    for(int i; i<100; i++){//clear connections
        connections[i] =-1;
    }
    c_ix = 0;

    int curr_master_id = SLAVE_ID;
    while(c_ix < cd_ix){
        bool found = 0;
        for(int j=0; j<cd_ix; j++){
            if(connections_dump->my_id == curr_master_id){
                connections[c_ix] = connections_dump->my_slave_id;
                curr_master_id = connections_dump->my_slave_id;
                c_ix++;
                found = 1;
            }
        }
        if(found == 0){ //la catena è rotta da qualche parte
            return 0;
        }
    }
    return 1;
}


void setup() {
  COM_DEBUG.begin(115200);
  COM_WITH_SLAVE.begin(BAUD, SERIAL_8N1, SLAVE_RX_PIN, SLAVE_TX_PIN);
}

void loop() {
    if (COM_WITH_SLAVE.available() >= sizeof(Message)) {
        Message msg_from_slave;
        COM_WITH_SLAVE.readBytes((uint8_t*)&msg_from_slave, sizeof(Message));

        if(msg_from_slave.dest_id == ROOT_ID || msg_from_slave.dest_id == NEAREST_CUBE) {
            if (msg_from_slave.type == MSG_HELLO){
                SLAVE_ID = msg_from_slave.payload.hello_payload.id;
                Message ack;
                ack.dest_id = SLAVE_ID;
                ack.type = MSG_HELLO;
                ack.payload.hello_payload.id = ROOT_ID; 
                COM_WITH_SLAVE.write((uint8_t*)&ack, sizeof(Message)); //send ack 
            }

            if(msg_from_slave.type == MSG_ROOT_INFO){
                connections_dump[cd_ix] = msg_from_slave.payload.root_info_payload;
                cd_ix++;
                CATENA_COMPLETA = make_connections();
            }
        }
    }

    if(SEND_COMMAND){
        SEND_COMMAND = 0;
        Message command;
        command.type = MSG_SERVO;
        command.dest_id = dest_id;
        command.payload.servo_payload.servo_n = servo_n;
        command.payload.servo_payload.radian = radian;
        COM_WITH_SLAVE.write((uint8_t*)&command, sizeof(Message));
    }
}


