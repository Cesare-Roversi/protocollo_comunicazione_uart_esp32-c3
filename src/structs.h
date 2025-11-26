
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

