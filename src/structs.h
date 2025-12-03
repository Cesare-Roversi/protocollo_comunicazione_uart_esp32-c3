
typedef enum{
    type_command_01,
    type_command_02,
    type_handshake,
    type_no_msg_type,
}MsgType;

typedef struct{
    char str1[100];
    char str2[50];
}PayloadCommand01;

typedef struct{
    int num1;
    float num2;
}PayloadCommand02;

/*
l'idea è:
1.hello è solo tra 2 nodi vicini che non si conoscono
2.statement to master lo invia il nuovo nodo quello:
    int my_id != NULL
    int my_slave_id != NULL
    int my_master_id == NULL

ROOT poi si rivede il dizionario aggiunge lui, e sistema my_master_id;
*/

typedef enum{
    type_hello,
    type_report_to_root,
    type_no_handshake_type,
}HandshakeType;
typedef struct{
    HandshakeType type;

    int my_id;
    int my_slave_id;
    int my_master_id;
}PayloadHandshake;

//con le union alloca sempre i byte x il messaggio + lungo
typedef union{ 
    PayloadCommand01 payload_command_01;
    PayloadCommand02 payload_command_02;
    PayloadHandshake payload_handshake;
}Payload;

typedef struct{
    int sender_id;
    int target_id;
    MsgType type;

    Payload payload;
}Msg;


const char* enum_to_str(MsgType msg_type, HandshakeType handshake_type){
    if(msg_type != type_no_msg_type){
        switch (msg_type){
        case type_command_01:
            return "type_command_01";
        case type_command_02:
            return "type_command_02";
        case type_handshake:
            return "type_handshake";
        default:
            break;
        }

    }else if(handshake_type != type_no_handshake_type){
        switch (handshake_type){
        case type_hello:
            return "type_hello";
        case type_report_to_root:
            return "type_report_to_root";
        default:
            break;
        }
    }

    return "ERROR in enum_to_str()";
} 

