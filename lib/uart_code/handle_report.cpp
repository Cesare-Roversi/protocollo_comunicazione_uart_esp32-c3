#include "msg_structs.h"
#include "utils_uart_comms.h"
#include "protocol_manager.h"

#include <iostream>
#include <string>
#include <array>
#include <cstdio>
#include <algorithm>

using namespace std;

#define MAX_NODES 10

int ids_array[MAX_NODES]; //ordinati
int ids_array_len = 0;

PayloadReport dict[MAX_NODES]; //disordinati
bool is_dict_ix_empty[MAX_NODES];
//dict[ix] ix = my_id del nodo contenuto



const char* get_node_info(const PayloadReport& n){
    static char tmp[64];
    snprintf(tmp, sizeof(tmp), " {MASTER: %d, SELF: %d, SLAVE: %d}",
             n.my_master_id,
             n.my_id,
             n.my_slave_id);
    return tmp;
}

void print_node_info(PayloadReport& p){
    cout << get_node_info(p) << endl;
}


void print_ids_array(){
    cout << "PRINTING: IDS_ARRAY\n";
    for(int i = 0; i < ids_array_len; i++){
        cout << ids_array[i] << ' ';
    }
    cout << endl;
}


void compute_ids_array(){
    ids_array[0] = ROOT_ID; // ROOT_ID == 0
    ids_array_len = 1; // reset the array

    // avoid loops
    bool is_node_already_in_ids_array[MAX_NODES];
    for (int i = 0; i < MAX_NODES; ++i){
        is_node_already_in_ids_array[i] = false;
    }
    is_node_already_in_ids_array[ROOT_ID] = true;

    int curr_node_id = ROOT_ID;
    for (int step = 0; step < MAX_NODES - 1; step++) {
        int next_node_id = -1;
        //find dict[j].my_master_id == curr_node_id
        for (int j = 0; j < MAX_NODES; j++) {
            if (is_node_already_in_ids_array[j]){
                continue;
            } 
            if (is_dict_ix_empty[j]){ //non è mai arrivato un report con un SELF_ID == j
                continue;
            }
            if (dict[j].my_master_id == curr_node_id) {
                next_node_id = j;
                break;
            }
        }

        if (next_node_id == -1){
            break; //chain ends
        }
        
        ids_array[ids_array_len] = next_node_id;
        ids_array_len +=1;
        is_node_already_in_ids_array[next_node_id] = true;
        curr_node_id = next_node_id;
    }
}


void remove_subtree_recursive(int node_id) {
    for (int j = 0; j < MAX_NODES; ++j) {
        if (is_dict_ix_empty[j]){
            continue;
        }
        if (dict[j].my_master_id == node_id) {
            remove_subtree_recursive(j);
            
            // rimuovi j
            is_dict_ix_empty[j] = true;
            dict[j].my_master_id = UNKNOWN_ID;
            dict[j].my_slave_id = UNKNOWN_ID;
            dict[j].my_id = j;
        }
    }
}


void receive_new_report(PayloadReport p){
    dict[p.my_id] = p;
    is_dict_ix_empty[p.my_id] = false;

    
    if (p.my_slave_id == UNKNOWN_ID) {
        remove_subtree_recursive(p.my_id);
    }

    compute_ids_array();
    if(SHOW_UART_COMMS_LOGS){
        cout << "RECEIVED NEW REPORT:" << endl;
        print_node_info(p);
        print_ids_array();
        cout << "___RECEIVED NEW REPORT:" << endl;
    }
    ProtocolManager::set_num_servos((uint8_t)get_ids_array_len());
}


int get_ids_array_len(){
    return ids_array_len;
}

void get_ids_array(int* arr, int len){ //copia ids_array in arr
    for(int i=0; i<min(len, ids_array_len); i++){
        arr[i] = ids_array[i];
    }
}


void init_report_handler(int* default_ids, int default_ids_len, bool use_default_ids){
    for(int i = 0; i < MAX_NODES; i++) {
        is_dict_ix_empty[i] = true;
    }

    if(use_default_ids){
        for(int i=0; i<default_ids_len; i++){
            ids_array[i] = default_ids[i];
        }
        ids_array_len=default_ids_len;

    }else{
        //*INIT IDS_ARRAY[] (ROOT IS ALONE)
        ids_array[0] = ROOT_ID; //ROOT_ID == 0
        ids_array_len = 1; 

        //*INIT DICT[] e IS_DICT_EMPTY[] (ROOT IS ALONE)
        PayloadReport pr;
        pr.my_id = ROOT_ID;
        pr.my_master_id = UNKNOWN_ID;
        pr.my_slave_id = UNKNOWN_ID;
        dict[0] = pr;
        is_dict_ix_empty[0] = false;
    }
}

void task_handle_report(void* arg){
  if(SELF_ID != ROOT_ID){ //it shouldn't be the case.
    vTaskDelete(nullptr);
  }

  while(1){
  Msg* msg = nullptr;
  xQueueReceive(h_queue_report, &msg, portMAX_DELAY);
    
    // printf("REPORT: SLAVE=%d, MY=%d, MASTER=%d\n", msg->payload.payload_report.my_slave_id, msg->payload.payload_report.my_id, msg->payload.payload_report.my_master_id);
    receive_new_report(msg->payload.payload_report);
    // free the message allocated by the UART layer
    delete msg;
  }
}

