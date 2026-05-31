Analisi dettagliata del sottosistema "cinematica" (servo)

Scopo
------
Questo documento è un'analisi di dettaglio del codice presente in lib/cinematics e degli header in include/cinematics_header.
Non sostituisce il README.md già presente (che spiega l'idea generale) ma scende nel codice e nelle scelte implementative, con osservazioni pratiche per sistemi embedded.

File principali (mappa rapida)
--------------------------------
- include/cinematics_header/servo_types.h  -> tipi, costanti, dichiarazioni globali
- include/cinematics_header/servo_hal.h    -> HAL: LEDC, conversione posizione -> duty
- include/cinematics_header/servo_motion.h -> helper per la distanza di arresto (decel)
- include/cinematics_header/servo_task.h   -> interfaccia della task e api pubbliche
- lib/cinematics/servo_types.cpp           -> definizione globale di servo_data, inizializzazioni
- lib/cinematics/servo_hal.cpp             -> configurazione LEDC e set_servo_pos()
- lib/cinematics/servo_motion.cpp          -> decel_distance_sim(...) e wrapper
- lib/cinematics/servo_task.cpp            -> state‑machine S‑curve, queue, preemption, backlash
- lib/cinematics/tests.cpp                 -> test funzionali (sweep, precision, reactivity...)

Panoramica esecutiva (runtime flow)
-----------------------------------
1) Durante l'inizializzazione (servo_init) viene creato il timer PWM (servo_timer_init), la coda xServoQueue e la task
   move_servo_speed_task_state_machine. Viene effettuato un movimento iniziale verso 0 rad.
2) L'API move_servo_speed(rad, speed, acc, jerk) impacchetta i parametri e li mette in xServoQueue (se è piena, scarta il comando più vecchio).
3) La task è bloccata su xQueueReceive(..., portMAX_DELAY). Quando riceve un comando entra in una do/while che esegue il profilo di movimento
   S‑curve (7 fasi). All'interno del ciclo principale la task esegue update ogni 20 ms (vTaskDelayUntil con xFrequency = 20 ms).
4) Se arriva un nuovo comando mentre si esegue il profilo, la task lo legge subito (xQueueReceive(..., 0)) e ri‑pianifica (preemption): salva la posizione logica corrente e riavvia la FSM con il nuovo target.

Tipi e costanti (aspetti rilevanti)
----------------------------------
- ServoTaskParams: { target_rad, speed, acc, jerk } — payload messo in coda.
- ServoData: struttura globale (servo_data) che contiene limiti, current_pos/current_speed/current_acc come std::atomic<float>.
  - Usare std::atomic<float> rende esplicito il comportamento thread‑safe ma va considerato l'overhead (potrebbe non essere lock_free su tutte le ABI).
- trim (inline constexpr) e backlash (inline constexpr) — costanti di calibrazione hardware.
- servo_deadzone è calcolato in lib/cinematics/servo_types.cpp con la formula
  servo_deadzone = (270° in rad)/(sgnl_max_duty - sgnl_min_duty) * servo_deadzone_ms
  (nome un po' fuorviante: servo_deadzone_ms non è un tempo ma un fattore numerico)

HAL e conversione posizione -> PWM (lib/cinematics/servo_hal.cpp)
-----------------------------------------------------------------
- servo_timer_init() usa l'API LEDC (ESP32): trova una risoluzione di duty con ledc_find_suitable_duty_resolution(80MHz, 50Hz), configura timer e canale.
- set_servo_pos(float rad): la funzione che traduce la posizione rad in duty da passare a ledc_set_duty.
  - Calcolo principale (semplificato):
      mid_point = sgnl_min_duty + (sgnl_max_duty - sgnl_min_duty)/2
      time_us   = mid_point + (rad + trim)/(1.5 * M_PI) * (sgnl_max_duty - sgnl_min_duty)
      max_duty  = (1 << servo_data.duty_res)
      duty      = time_us / 20000.0 * max_duty   // 20 ms = 20000 us periodo
  - Osservazioni:
    * "time_us" è espresso in microsecondi (nome variabile "time" un po' ambiguo).
    * La mappatura usa 1.5 * M_PI come fattore: è un'approssimazione pratica della range meccanico (~279.8°).
      Per maggiore robustezza è preferibile usare i valori min_pos/max_pos di servo_data per normalizzare (evita assunzioni magiche).
    * set_servo_pos esegue anche servo_data.current_pos.store(rad): la posizione logica viene aggiornata dall'HAL.

Profilo S‑curve e motore numerico (lib/cinematics/servo_motion.cpp)
-----------------------------------------------------------------
- decel_distance_sim(v_init, acc_init, a_max, j_max, v_max): funzione numerica che simula la decelerazione con step dt = 0.002 s (2 ms)
  per stimare la distanza necessaria per fermarsi sotto limiti di accelerazione e jerk. È una soluzione pragmatica quando la formula analitica
  è inaccurata a causa di quantizzazione temporale.
- decel_distance(...) e decel_distance_with_acc(...) sono wrapper.

Uso nella FSM: la task calcola ogni intervallo
  d_stop = (acc > 0) ? decel_distance_with_acc(vel, acc, a, j, cmd.speed) : decel_distance(vel, a, j, cmd.speed)
  e confronta rem (distanza residua) con d_stop per decidere il passaggio alla fase di decelerazione.

Note di performance e limiti della simulazione
- decel_distance_sim è chiamata frequentemente (ogni ciclo di controllo) e compie una simulazione iterativa; nel caso peggiore può iterare molte volte.
  Nella pratica i parametri rendono il numero di iterazioni contenuto, ma bisogna monitorare il costo CPU: eseguire questa simulazione nella stessa task che aggiorna la PWM può essere costoso.
- Possibili ottimizzazioni: usare formule analitiche (se si dimostra accurate), memoizzazione dei risultati per (v,a,j) ripetuti, oppure ridurre frequenza di chiamata (non ogni 20 ms se non necessario).

State machine: move_servo_speed_task_state_machine (lib/cinematics/servo_task.cpp)
--------------------------------------------------------------------------------
Comportamento generale:
- Il task legge un comando da xServoQueue (bloccando indefinitamente se vuota).
- Dopo aver clamped i parametri (speed/acc/jerk) e aver eventualmente applicato la backlash compensation
  (se il target è a sinistra della posizione corrente il comando viene prima portato oltre il target e poi, alla fine, ri‑richiamata la posizione reale),
  entra nella FSM che implementa le fasi S‑curve:
    PH_ACCEL_JUP -> PH_ACCEL_CONST -> PH_ACCEL_JDN -> PH_CRUISE -> PH_DECEL_JUP -> PH_DECEL_CONST -> PH_DECEL_JDN

Punti chiave di implementazione (osservazioni puntuali)
- Preemption: dentro la loop principale si prova a leggere la coda con timeout 0; se arriva un nuovo comando la variabile restart viene settata a true,
  la posizione logica viene salvata (servo_data.current_pos.store(pos)) e si esce dal loop per riavviare la pianificazione con il nuovo comando.
  Questo garantisce reattività: l'ultimo comando prende sempre la precedenza.
- Calcolo del dt: dt = (now - xPrevTick) * (portTICK_PERIOD_MS / 1000.0f). La task poi usa vTaskDelayUntil(&xLastWake, xFrequency) con xFrequency = 20 ms.
- Applicazione delle protezioni: vel è saturata a [0, v] e, se il passo porta a superare target, si forza pos = target e vel/acc = 0.
- Comandi al servo: set_servo_pos viene chiamato quando |pos - target| > servo_deadzone, altrimenti si invia il target finale direttamente (minimizza jitter vicino al target).
- Alla fine del movimento viene fatto un set_servo_pos(cmd.target_rad) per correggere errori numerici e si invia l'ack (o si pianifica il backlash_cmd se richiesto).


Test e validazione
-------------------
I test in lib/cinematics/tests.cpp coprono: sweep, precisione, reattività (preemption), crescita di speed/acc/jerk. Buone pratiche:
- Eseguire i test con logging disabilitato o su file per non influenzare il timing.
- Misurare tempo di esecuzione di decel_distance_sim in condizioni estreme.
- Validare il comportamento di backlash su hardware reale con differenti coppie e alimentazioni.

Sequenza passo‑passo (dettagliata)
---------------------------------
Di seguito una descrizione passo‑passo di cosa succede nel codice, con riferimenti espliciti alle funzioni e ai meccanismi FreeRTOS utilizzati.

All'avvio
- init_wifi(): se viene avviata la root
- init_uart_comms().
- Funzione coinvolta: servo_init() (lib/cinematics/servo_task.cpp)
  1. Chiamata iniziale: servo_init() invoca servo_timer_init() (lib/cinematics/servo_hal.cpp) per inizializzare la periferica PWM LEDC.
     - servo_timer_init() configura la risoluzione di duty con ledc_find_suitable_duty_resolution(...) e chiama ledc_timer_config / ledc_channel_config.
  2. Viene impostato un valore noto per la posizione logica: servo_data.current_pos.store(-0.1f).
  3. Viene creata la coda dei comandi: xServoQueue = xQueueCreate(SERVO_QUEUE_LEN, sizeof(ServoTaskParams)).
     - FreeRTOS feature: code per la comunicazione inter‑task (xQueueCreate).
    la coda non viene direttamente esposta, i comandi vengono messi in questa coda dalla funzione move_servo_speed()
  4. Viene creata la task persistente con xTaskCreate(move_servo_speed_task_state_machine, "ServoMotorTask", 3072, NULL, 1, &xTaskHandle).
     - FreeRTOS feature: creazione di task con priorità e dimensione di stack specificata.
     - Motivo: desincronizzare l'avvio dei più servos per ridurre i picchi di corrente sull'alimentazione.
  5. Viene fatta una stampa del deadzone e quindi un ritardo casuale vTaskDelay(pdMS_TO_TICKS(rand()%3000));
  6. Infine viene inviato un comando iniziale move_servo_speed(0.0f, 1.0f, ... ) e si attende con vTaskDelay(1000) per consentire l'inizializzazione fisica.
  7. viene creata la task principale che fa da interfaccia tra il layer uart e il layer di controllo del servo:
      - xTaskCreate(task_execute_servo, "ExecServoTask", 3072, NULL, 2, NULL);
      - lo scopo di questa task è leggere se ci sono messaggi in arrivo nella coda della task di comunicazione Uart:
       - se ci sono chiama la funzione move_servo_speed() con i dati del comando, che si occuperà di sanificare i dati del comando e di fare spazio nella coda per il nuovo comando se è piena.
       - se non ci sono blocca la task (niente busy waiting)
  
Quando si riceve un comando (API call)
- Funzione coinvolta: move_servo_speed(float rad, float speed, float acc, float jerk) (lib/cinematics/servo_task.cpp)
  1. L'API effettua dei log diagnostici e controlla che xServoQueue sia inizializzata; ritorna ESP_ERR_INVALID_STATE se non lo è.
  2. I parametri sono "sanitizzati" rispetto ai limiti in servo_data (clamping di speed/acc/jerk).
  3. Tentativo di inserimento non‑bloccante in coda: if (xQueueSend(xServoQueue, &params, 0) != pdPASS) { ... }
     - Politica applicata: se la coda è piena si elimina l'elemento più vecchio con xQueueReceive(xServoQueue, &dropped, 0) e si ritenta la xQueueSend.
     - FreeRTOS feature: uso di xQueueSend con timeout 0 per operazioni non bloccanti e xQueueReceive per rimuovere l'elemento più vecchio.
  4. Ritorno: la funzione ritorna ESP_OK quando il comando è stato accodato correttamente.

Esecuzione del comando (state machine)
- Funzione coinvolta: move_servo_speed_task_state_machine(void *pvParameters) (lib/cinematics/servo_task.cpp)
  1. Blocco iniziale in attesa di comando: la task è generalmente bloccata su xQueueReceive(xServoQueue, &cmd, portMAX_DELAY).
     - FreeRTOS feature: portMAX_DELAY blocca la task fino all'arrivo di un elemento (modalità tipica per worker tasks event driven).
  2. All'arrivo di un comando la task imposta servo_data.moving.store(true) e prepara variabili locali (pos, target, vel, acc). (salvataggio atomico dello stato di movimento nella struttura dati del servo)
  3. Backlash compensation: se target < pos, il codice modifica il target iniziale target = max(target - backlash, servo_data.min_pos)
     e imposta la flag backlash_compensation=true. Questo causa che il primo movimento arrivi "da un lato" rispetto al target,
     successivamente verrà inviato un comando inverso per la compensazione.
  4. Clamp dei parametri locali j, a, v dai valori del comando o dai limiti di servo_data.
  5. Preparazione del controllo periodico:
     - MotionPhase phase = PH_ACCEL_JUP;
     - TickType_t xLastWake = xTaskGetTickCount(); // per vTaskDelayUntil
     - TickType_t xPrevTick = xLastWake;         // per il calcolo dt basato sui ticks
     - const TickType_t xFrequency = pdMS_TO_TICKS(20); // periodo di controllo = 20 ms
  6. Ciclo principale: while (!done) { ... }
     - Preemption (reazione a nuovi comandi): in testa al loop viene fatto un tentativo non‑bloccante di leggere la coda:
         ServoTaskParams next; if (xQueueReceive(xServoQueue, &next, 0) == pdTRUE) { servo_data.current_pos.store(pos); cmd = next; restart = true; break; }
       Questo è un pattern avanzato: controllare la coda senza aspettare permette alla task di essere altamente reattiva e di "prendere" subito nuovi target.
       - FreeRTOS feature: xQueueReceive con timeout 0 per polling non‑bloccante / preemption.
     - Calcolo di dt: TickType_t now = xTaskGetTickCount(); dt = (now - xPrevTick) * (portTICK_PERIOD_MS / 1000.0f);
       xPrevTick = now;
       Nota: dt è basato sul tick di FreeRTOS (risoluzione dipendente da configTICK_RATE_HZ). 
     - Calcolo remaining distance: rem = fabsf(target - pos) e d_stop = decel_distance... (chiamata a decel_distance_with_acc o decel_distance)
       - decel_distance_* invocano la simulazione numerica decel_distance_sim(...) (lib/cinematics/servo_motion.cpp) che valuta la distanza di arresto sotto limiti di acc e jerk.
     - State machine S‑curve: switch(phase) { ... } implementa le sette fasi (PH_ACCEL_JUP, PH_ACCEL_CONST, PH_ACCEL_JDN, PH_CRUISE, PH_DECEL_JUP, PH_DECEL_CONST, PH_DECEL_JDN).
       Le transizioni sono determinate da rem rispetto a d_trig e da condizioni analitiche su vel/acc.
     - Aggiornamento dinamico: vel += acc * dt; clamp vel; pos += dir * vel * dt; correzione overshoot (se pos oltre il target si forza pos=target e done=true).
     - Stato condiviso: servo_data.current_speed.store(vel); servo_data.current_acc.store(acc); servo_data.current_pos.store(pos);
       - Nota: si usano std::atomic<float> per evitare mutex, scelta praticabile ma con considerazioni di portabilità/overhead.
     - Comando hardware: se (fabsf(pos - target) > servo_deadzone) set_servo_pos(pos); else set_servo_pos(target);
       - set_servo_pos è la funzione HAL che calcola il duty e chiama ledc_set_duty / ledc_update_duty (interazione diretta con periferica).
     - Sincronizzazione periodica: if (!done) vTaskDelayUntil(&xLastWake, xFrequency);
       - FreeRTOS feature avanzata: vTaskDelayUntil mantiene l'offset temporale e previene drift, garantendo un ciclo di controllo a ~20 ms.

Terminazione del comando
- Dopo che la FSM imposta done = true o se il comando è stato preemp­tato (restart = true), si esce dal ciclo di esecuzione.
  1. Se restart è true il do/while esterno riavvia la pianificazione con il nuovo comando (pattern di ri‑pianificazione atomica).
  2. Se il movimento è realmente terminato:
     - set_servo_pos(cmd.target_rad); // correzione finale per errori numerici
     - servo_data.current_speed.store(0.0f); servo_data.current_acc.store(0.0f); servo_data.moving.store(false);
  3. Backlash compensation: se backlash_compensation era true, la task attende 500 ms (vTaskDelay(pdMS_TO_TICKS(500))) e quindi prepara
     un backlash_cmd (piccola velocità, acc e jerk) e lo invia con xQueueSend(xServoQueue, &backlash_cmd, 0). Questo garantisce che il target finale venga raggiunto sempre nello stesso verso.
     - Nota pratica: qui xQueueSend è non‑bloccante e l'invio del backlash_cmd non viene verificato; in scenari estremi il comando potrebbe essere scartato se la coda fosse piena.
  4. Se non è necessaria la backlash_compensation la task invoca send_movement_ack() (lib/cinematics/servo_task.cpp)
     - send_movement_ack() costruisce un Msg* tramite create_msg e lo passa a send_msg_to_master(msg).
     - Importantissimo: il commento nel codice specifica che l'ownership del Msg* viene trasferita al sottosistema di invio e quindi
       non bisogna liberarlo nel chiamante. Questo è un pattern comune in sistemi embedded con code e handoff di puntatori tra task.

Funzionalità FreeRTOS "avanzate" usate (sintesi)
- xQueueCreate / xQueueSend / xQueueReceive: code per comunicazione sicura inter‑task; uso di xQueueReceive(..., portMAX_DELAY) per blocco indefinito e di xQueueReceive(..., 0)
  per polling non‑bloccante (preemption).
- vTaskDelayUntil: implementazione corretta di un loop periodico a 20 ms evitando il drift temporale (migliore di ripetuti vTaskDelay).
- xTaskCreate: creazione e configurazione della task con stack/piorità dedicate.
- pdMS_TO_TICKS / portTICK_PERIOD_MS / xTaskGetTickCount: conversioni e uso dei tick di sistema per calcolare dt e sincronizzare la task.
- Strategia di gestione della coda piena: pattern "drop oldest" implementato in move_servo_speed per garantire reattività verso l'ultimo comando.
- Handoff di memoria/ownership: passaggio di Msg* tra task (send_movement_ack e send subsystem) con regole di ownership documentate nel codice.

Distance simulation: come funziona decel_distance_sim
-------------------------------------------------
Descrizione generale
- Funzione: decel_distance_sim(float v_init, float acc_init, float a_max, float j_max, float v_max)
- File: lib/cinematics/servo_motion.cpp
- Scopo: stimare la distanza necessaria a portare la velocità corrente v_init a zero rispettando i limiti di accelerazione (a_max) e jerk (j_max).
  Questa stima è usata nella FSM per calcolare d_stop (distanza di arresto) e decidere quando iniziare la decelerazione.

Algoritmo passo‑passo (mappato sul codice)
1) Controllo iniziale: se v_init <= 0 la funzione ritorna 0.0f immediatamente: nulla da fermare.
2) Parametri di simulazione:
   - dt = 0.002f (2 ms): passo di integrazione scelto per bilanciare accuratezza e costo CPU.
   - v = v_init, a = acc_init, x = 0.0f (distanza accumulata).
   - max_iters = 20000: limite superiore delle iterazioni per evitare loop infiniti.
3) Loop principale: for (int i = 0; i < max_iters && v > 1e-6f; ++i) { ... }
   - target_a = -a_max: l'obiettivo di accelerazione è decelerare fino a -a_max (accelerazione negativa costante in regime di decelerazione).
   - j = (a > target_a) ? -j_max : 0.0f; si applica jerk negativo solo se l'accelerazione corrente a è maggiore del target (si vuole scendere fino a -a_max).
   - a_next = a + j * dt; se a_next < target_a allora a_next = target_a; questo clampa l'accelerazione al valore target.
   - a_avg = 0.5f * (a + a_next); si usa l'accelerazione media sul passo dt per integrare in modo più accurato.
   - v_next = v + a_avg * dt; poi viene eseguito un clamp v_next = min(v_next, v_max) (v_max è passato alla funzione: nella simulazione di arresto esso impedisce aumenti indesiderati sopra un limite di velocità).
   - Se v_next <= 0.0f: il codice calcola il tempo esatto t_stop necessario per arrivare a zero usando t_stop = (a_avg == 0.0f) ? dt : (-v / a_avg);
       * Sanitizza t_stop (se negativo lo imposta a dt) e aggiunge la porzione finale di distanza: x += v * t_stop + 0.5f * a_avg * t_stop * t_stop; quindi ritorna x.
   - Altrimenti si aggiorna la distanza con l'integrazione del moto: x += v * dt + 0.5f * a_avg * dt * dt; si assegnano v = v_next; a = a_next e si continua.
4) Se il loop termina senza che v sia arrivato a zero (esauriti gli iters o v non scende sotto la soglia), la funzione ritorna la x accumulata.

Motivazioni numeriche e osservazioni
- Integrazione: l'uso di a_avg e delle formule per x è equivalente a una regola trapezoidale su acc → velocità → spazio, stabile per piccoli dt.
- Jerk handling: la scelta j = -j_max (solo riduzione dell'accelerazione) assume che per la decelerazione si applicherà sempre un jerk negativo fino a raggiungere -a_max.
  Questo modello è corretto quando si vuole una decelerazione rapida; copre anche il caso in cui acc_init sia positivo (prima si porta l'acc verso valori negativi).
- Clamping v_next a v_max protegge contro casi in cui l'acc iniziale sia positiva e la velocità aumenti oltre il limite operativo.
- Early exit su v_next <= 0: la funzione calcola esattamente l'ultimo tratto fino a fermarsi invece di approssimarlo con un passo intero.
