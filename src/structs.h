
typedef enum{
    type_command_01,
    type_command_02,
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
    hello,
    statement_to_root,
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
}Payload;

typedef struct{
    int sender_id;
    int target_id;
    MsgType type;

    Payload payload;
}Msg;

