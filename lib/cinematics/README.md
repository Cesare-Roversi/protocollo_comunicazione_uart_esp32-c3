# Sottosistema Cinematica — Servo

Ambito: gestione del movimento del servomotore (profilazione S‑curve jerk‑limited, conversione posizione→PWM, compensazioni meccaniche). Responsabilità: Mattia Pistollato (just-normal-username).

## File principali
- lib/cinematics/servo_types.*
- lib/cinematics/servo_motion.*
- lib/cinematics/servo_task.*
- lib/cinematics/servo_hal.*
- lib/cinematics/tests.cpp

## Scopo e responsabilità
- Calcolare e applicare profili di movimento jerk‑limited (S‑curve) per i servomotori.
- Fornire l'API runtime `move_servo_speed(...)` e la task che esegue lo state‑machine.
- Convertire la posizione logica in duty PWM (LEDC) e applicare compensazioni: trim, backlash, deadzone.

## Panoramica HW ↔ SW
- HW: servomotore pilotato via periferica LEDC dell'ESP32 (PWM). Gestione temporale basata su FreeRTOS tasks e tick.
- SW: API pubblica `move_servo_speed` → enqueue comando in `xServoQueue`. La task `move_servo_speed_task_state_machine` legge la coda e applica il profilo S‑curve ogni 20 ms, aggiornando periodicamente la posizione con `set_servo_pos()`.
- Interfaccia superiore: `task_execute_servo` (src/esp32-c3/main.cpp) estrae i parametri da `Msg*` e chiama `move_servo_speed`.
- A fine movimento viene inviato ack verso il master (send_movement_ack) usando `create_msg`/`send_msg_to_master`

## Strutture dati principali
- `ServoTaskParams` { target_rad, speed, acc, jerk } — elementi messi in `xServoQueue`.
- `ServoData` — stato globale: pos/speed/acc sono `std::atomic<float>`; limiti max e parametri HW.
- `MotionPhase` — 7 fasi S‑curve (ACCEL_JUP, ACCEL_CONST, ACCEL_JDN, CRUISE, DECEL_JUP, DECEL_CONST, DECEL_JDN).

## Algoritmo chiave
- Profilo S‑curve jerk‑limited con transizioni di fase controllate dalla distanza residua `rem` e dalla distanza di arresto `d_stop`.
- `d_stop` è calcolata con `decel_distance_sim(...)`, una simulazione numerica a passo `dt` (scelta per ridurre errori dovuti alla quantizzazione temporale).
- La FSM è preemptive: se arriva un nuovo comando in coda durante l'esecuzione, la routine cattura il nuovo comando e ri‑pianifica immediatamente.
- Controllo dei limiti: speed/acc/jerk sono clampati ai massimi configurati in servo_data.
- Ogni iterazione calcola dt usando FreeRTOS ticks: dt = (now - xPrevTick) * (portTICK_PERIOD_MS / 1000.0f)
- Applica protezioni: se rem (distanza residua) <= d_trig (distanza di arresto) si passa alla fase di decelerazione.

## Compensazioni e protezioni
- Backlash compensation: Se il target è minore della posizione corrente il movimento eseguito viene aumentato di un valore di backlash (target = max(target - backlash, min_pos)), si esegue il movimento e, a fine, si effettua un movimento di ritorno (backlash_cmd) per assicurarsi che una certa posizione venga raggiunta sempre nello stesso verso rendendo prevedibile il gioco meccanico.
- Deadzone: Per evitare che il servo scarti comandi molto vicini tra loro, viene applicata una soglia servo_deadzone: se |pos - target| > deadzone si invia pos; altrimenti si imposta direttamente target.
- Trim: Viene aggiunto un offset trim (trim = 0.07 * π) usato nella conversione posizione→duty per calibrare la meccanica.
- Clamp su speed/acc/jerk ai massimi definiti in `servo_data`.

## Interfaccia verso altri moduli
- Ingresso: `move_servo_speed(rad, speed, acc, jerk)`
- Messaging: `task_execute_servo` riceve `Msg*` da `h_queue_servo`, estrae parametri e chiama `move_servo_speed`.
- Acknowledgement: a fine movimento viene inviato un `type_servo_ack` al master tramite `send_msg_to_master(create_msg(...))` (ownership del `Msg*` è trasferita al layer di invio).

## Codice rappresentativo (algoritmo avanzato)
Il seguente estratto mostra la funzione `decel_distance_sim(...)`, il cuore numerico utilizzato per stimare la distanza di arresto con limiti di accelerazione e jerk. Questa simulazione guida le transizioni della FSM S‑curve.

```c
float decel_distance_sim(float v,float a,float A,float J,float Vmax){
  if(v<=0) return 0;
  const float dt=0.002f; float x=0;
  for(int i=0;i<20000 && v>1e-6f;++i){
    float targ = -A;
    float j = (a>targ) ? -J : 0.0f;
    float a_next = a + j*dt; if(a_next<targ) a_next=targ;
    float a_avg = 0.5f*(a + a_next);
    float v_next = v + a_avg*dt; if(v_next>Vmax) v_next=Vmax;
    if(v_next<=0.0f){ float t=(a_avg==0)?dt:(-v/a_avg); x+=v*t+0.5f*a_avg*t*t; return x; }
    x += v*dt + 0.5f*a_avg*dt*dt; v=v_next; a=a_next;
  }
  return x;
}
```

FSM
```c
switch (phase) {
  case PH_ACCEL_JUP:
      //this means that we have just enough space to stop
      if (rem <= d_trig) {
          phase = PH_DECEL_JUP;
          break; 
      }
      acc += j * dt; //updating acceleration
      if (acc >= a) { //capping acceleration to a
          acc = a;
          // speed after ajerk-up phase
          // if the speed after the jerk-up phase is higher than the target speed, we have to go directly to jerk-down (should not happen)
          // otherwise we can enter the constant acceleration phase
          float vp = vel + (a * a) / (2.0f * j);
          if (vp >= v) {
              phase = PH_ACCEL_JDN;
          } else {
              phase = PH_ACCEL_CONST;
          }
      } else {
          // checking for overshoot
          float vp = vel + (acc * acc) / (2.0f * j);
          if (vp >= v) {
              phase = PH_ACCEL_JDN;
          }
      }
      break;

  case PH_ACCEL_CONST:
      // if we have just enough space to stop, we have to start decelerating
      if (rem <= d_trig) { 
          phase = PH_DECEL_JUP;
          break;
      }
      // if with the deceleration the speed would be too high, we have to start reducing acceleration
      if (vel + (a * a) / (2.0f * j) >= v) {
          phase = PH_ACCEL_JDN;
      }
      break;
    case PH_ACCEL_JDN:
      // if we have just enough space to stop, we have to start decelerating
      if (rem <= d_trig) { 
          phase = PH_DECEL_JUP; 
          break; 
      }
      acc -= j * dt;
      if (acc <= 0.0f) {
          acc = 0.0f;
          vel = v;
          phase = PH_CRUISE; //linear motion phase
      }
      break;

  case PH_CRUISE:
      // if we have just enough space to stop, we have to start decelerating
      if (rem <= d_trig) {
          phase = PH_DECEL_JUP;
      }
      break;

  case PH_DECEL_JUP:
      // starting deceleration ramp: acc goes from 0 to -a
      acc -= j * dt;
      if (acc < -a) acc = -a;
      // in this case we have just enough space to stop, so we can skip the constant deceleration phase and go directly to jerk-down
      if (vel <= (acc * acc) / (2.0f * j)) {
          phase = PH_DECEL_JDN;       // triangular profile
      } else if (acc <= -a) {
          phase = PH_DECEL_CONST;     // trapezoidal profile
      }
      break;

  case PH_DECEL_CONST:
      // if we have just enough space to stop, we have to start reducing deceleration
      if (vel <= (a * a) / (2.0f * j)) {
          phase = PH_DECEL_JDN;
      }
      break;

  case PH_DECEL_JDN:
      // deceleration ramp: acc goes from -a to 0
      acc += j * dt;
      if (acc >= 0.0f) {
          acc = 0.0f;
          vel = 0.0f;
          done = true;    // now we can stop, we have reached the target
      }
      break;
  }
```

## Testing
- `lib/cinematics/tests.cpp` fornisce test funzionali: sweep, precision, reactivity (preemption), speed/acc/jerk coverage. I test sono stati eseguiti utilizzando un apposito environment di PlatformIO (`esp32-c3-test`) per caricare la funzione corretta.
- Questi test usano funzioni di logging e controlli su servo_data.current_pos / current_speed / current_acc per rilevare uscita dai limiti e mismatch finale. Quindi le funzioni hardware independent sono state testate in software per quanto possibile.
- Il testing sull'harware ha però rivelato problemi che erano sfuggiti nella fase di test software.

## Problemi e risoluzioni osservate:
- Inaccuratezza delle formule analitiche per distanza di arresto → risolto con decel_distance_sim (simulazione numerica con dt piccolo).
- Problemi di ownership Messaggi → centralizzazione allocate_msg / free_msg e commenti per non cancellare msg prima che il task di invio li distrugga.
- Oscillazioni / errori di posizionamento a causa di deadzone / backlash → introdotti trim, deadzone e sequenza di backlash compensation, ciò ha fatto si che senza grandi coppie applicate ai servo i movimenti siano ripetibili con buona precisione (considerando la natura economica dei servo, dal fatto che una un potenziometro analogico e che non fornisce nessun output sulla posizione).
- Per evitare grandi picchi di corrente allo start che causavano il reset delle schede, è stato introdotto un delay casuale in servo_init per desincronizzare l’avvio dei servos.
- nella prima implementazione il servo arrivava ai limiti del potenziometro e c'erano dei casi in cui, impostando una posizion, l'esp32 inviava costantemente aggiornamenti sulla posizione al servo in modo alternato in un intorno della posizione desiderata senza mai arrivare a quella corretta, tutto ciò faceva si che il servo consumasse molto e fosse meno stabile e rispondendo peggio a coppie alte, rischiando di spostarsi molto o compiere una rotazione completa (nel caso in cui superasse il limite del potenziometro). È stato risolto correggendo l'implementazione in modo che impostasse l'angolo corretto e evitando che il servo arrivasse al limite del potenziometro.
- Molti problemi sono stati riscontrati durante la fase di assemblaggio e testing derivanti dall'elettronica/alimentazione e dalla natura poco ripetibile della comunicazione uart in un ambiente distribuito: problemi come cavi/connessioni interrotte durante la fase di assemblaggio uniti a poche conoscenze di circuiti elettronici hanno portato a comportamenti anomali come comunicazioni interrotte, reboot anomali delle schede che sono stati difficili da diagnosticare. questi problemi sono stati risolti collegando ogni scheda ad un monitor seriale e controllando ogni output di debug inserito in zone chiave del codice, ad esempio stampando il motivo del riavvio ogni volta ha permesso di individuare il motivo del riavvio improvviso delle schede (brownout).
- Un problema serio si è verificato a causa della power supply utilizzata, nonostante da specifiche doveva dare in output 5v 2a questo output si è rivelato essere in verità 12v cosa che ha causato la distruzione della scheda root che avevamo intenzione di utilizzare (`esp32-s3`). Poteva essere evitato controllando in anticipo l'output con un multimetro ma è una situazione che non sembrava possibile. Questo problema è stato risolto sostituendo la scheda `esp32-s3` con una `esp32-c3` che avevamo in più.

## Note operative e suggerimenti
- molti miglioramenti possono essere fatti sull'elettronica, ad esempio aggiungendo un condensatore per ogni scheda per aiutare a mitigare drop improvvisi nel voltaggio. Ciò non dovrebbe essere un grande problema data la natura modulare e un design pensato appositamente per poter facilmente smontare e rimontare i vari cubi.
- una delle carenze principali attualmente è il chip usato dal servo per gestire la posizione, data la natura economica e l'utilizzo di un potenziometro, nonostante la compensazione del backlash software, non si potrà mai raggiungere una precisione assoluta. Semplice miglioramento potrebbe essere aggiungere un encoder relativo, come un photointerrupter e una con un anello tacchettato all'interno del cubo o un sensore ottico, per compensare questo problema a livello software. La scelta migliore resta comunque utilizzare dei servo digitali come i **Feetech STS3215** oppure rimuovere la scheda di controllo dei servo attuali e utilizzare un encoder magnetico con un driver di qualità più alta.
- un altro maggior problema è causato dalla geometria poco convenzionale di questi moduli che rende difficile controllare in modo preciso il robot senza un implementazione ad hoc di un inverse kinematics, averla migliorerebbe di gran lunga l'utilizzabilità e la user experience.
- un altro miglioramento può essere fatto aggiungendo i collegamenti in ogni faccia di un modulo. Attalmente i collegamenti sono presenti solo su 2 facce per ogni cubo, questo limita le funzionalità pensate originariamente ma è stato fatto per mantenere i costi contenuti.
- Ovviamente avendo un design modulare, questo concept può essere aumentato all'infinito creando moduli per ogni cosa, a partire da sensori fino a gripper di ogni forma e dimensione.
- un ultimo miglioramento potrebbe essere fatto nella comunicazione, noi abbiamo optato per uart perchè è bidirezionale e richiede solo 2 cavi ma non consente un throughput molto elevato, per controllare solo dei servo va bene ma se serve inviare dati di sensori potrebbero esserci limitazioni. 

---