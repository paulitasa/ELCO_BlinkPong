// LIBRERÍAS UTILIZADAS
#include <Adafruit_NeoPixel.h>
#include "RF24Network.h"
#include "RF24.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//INICIALIZACIÓN DISPLAY LCD
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// Estados de nuestro autómata (MENÚ INCIO)
#define pantallaprin0 0
#define juego20 1
#define juego12 2

//Declaración de los pines de los botones
#define PIN_BTN2 2
#define PIN_BTN3 3
#define PIN_BTN4 4

//Declaración primer estado del autómata
uint8_t state = pantallaprin0;

//variable que bloquea a los botones cuando estamos jugando
bool jugando = false; 

//Botón 2 para maquinas de estados
int   boton2;
int   bit_prev2 = HIGH;        // guardamos el estado anterior.
int   bit_act2 = HIGH;        // el estado del botón.
unsigned long temporizador2;

//Botón 3 para enter
int   boton3;
int   bit_prev3 = HIGH;      // guardamos el estado anterior.
int   bit_act3 = HIGH;        // el estado del botón.
unsigned long temporizador3;

//Botón 4 para reset
int   boton4;
int   bit_prev4 = HIGH;       // guardamos el estado anterior.
int   bit_act4 = HIGH;        // el estado del botón.
unsigned long temporizador4;


//*********************** protocolo radio CCCP: CCCP consigue cogorzas potentes **************************************
typedef struct {
    unsigned short type : 1;      // 0 master, 1 posavaso
    unsigned short dev_id : 7;    // Identificador único del dispositivo
    unsigned short proc_id : 4;   // Identificador único de instrucciones
    //unsigned short data : 24;   //Comentado porque en el prototipo no se usa, pero está pensado para mandar datos de color u otra información
} CCCP_packet;      

#define DEVTYPE 0       // TYPE: Centralita (master) 
#define DEVID 4         // DEV_ID: Identificador del dispositivo

//Configuración de los pines CE y CS del nrf24l01
RF24 radio(9, 10);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

//Nodo a trabajar en la mesh
#define nodeID 2

//Envío temporizado de paquetes
uint32_t displayTimer = 0;
struct payload_t {
  unsigned long ms;
  unsigned long counter;
};
uint32_t ctr = 0;

//Declaración del paquete que se envía
CCCP_packet ejemplo;
unsigned short tipo;
unsigned short instruccion; 
unsigned short datos_mandados;
unsigned short device;

//********  Identificadores para las instrucciones (proc_id) protocolo CCCP   ********
//Slaves
const unsigned short tengoConexion = 0;
const unsigned short tengoEquipo = 3;
const unsigned short tengoVaso = 4;
const unsigned short listoParaJugar = 7;
const unsigned short derrotado = 15;
const unsigned short setUpS = 14;
const unsigned short ackV1 = 13;
const unsigned short ackV2 = 2;

//Master
const unsigned short todosConectados = 0;
const unsigned short equipo1 = 5;
const unsigned short equipo2 = 10;
const unsigned short todosTienenEquipo = 15;
const unsigned short todosConVaso = 1;
const unsigned short victoria1 = 2;
const unsigned short victoria2 = 4;
const unsigned short victoriaFinal1 = 8;
const unsigned short victoriaFinal2 = 7;
const unsigned short setUpM = 14;

//Contadores para la comprobación de todos los posavasos en un estado concreto
int contadorPosavasos = 0;
int contadorPosavasosConEquipo = 0;
int contadorPosavasosConVaso = 0;
int contadorPosavasosListosParaJugar = 0;

//Cerrojos asociados a las instrucciones
bool flagTengoConexion = true;
bool flagTengoEquipo = true;
bool flagTengoVaso = true;
bool flagDerrotado = true;
bool todosConEquipo = false;
bool flag_todosConVaso = false;
bool flag_listoParaJugar = false;
bool flag_empezando_juego = true;
bool flag_JuegoTerminado = false;

//Parámetro indicador del número de posavasos en juego, en un futuro modifcable por el MENU de INICIO (para el prototipo lo hemos fijado a 2)
const int paramPosavasos = 2;
const int paramPosavasos2 = (paramPosavasos/2); //Se usa para gestionar cada equipo por separado
int vasos1 = paramPosavasos2;
int vasos2 = paramPosavasos2;

//Arrays para la comprobación de todos los posavasos en un estado concreto
boolean id_boolean[paramPosavasos];
boolean id_boolean_equipo1[paramPosavasos];
boolean id_boolean_equipo2[paramPosavasos];
boolean id_boolean_conVaso[paramPosavasos];



void setup() {
  Serial.begin(115200);

  //Inicialización del paquete CCCP de la centralita
  ejemplo.type = DEVTYPE;
  ejemplo.dev_id = DEVID;
  ejemplo.proc_id = setUpM;
  //ejemplo.data = 0;
  //readCCCPPacket(ejemplo);  //instrucción para debuggear código
    
  // Conexión a la mesh
  mesh.setNodeID(0);
  Serial.println(mesh.getNodeID());
  mesh.begin(); 

  //Inicialización de los arrays que comprueban los estados de los posavasos
  for (int i=0; i< paramPosavasos; i++){    
    id_boolean[i]= false;
  }
  for (int i=0; i< paramPosavasos2; i++){  
    id_boolean_equipo1[i]= false;
  }
  for (int i=0; i< paramPosavasos2; i++){  
    id_boolean_equipo2[i]= false;
  }
  for (int i=0; i< paramPosavasos; i++){  
    id_boolean_conVaso[i]= false;
  }

  //Declaración de botones
  pinMode(PIN_BTN3, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_BTN4, INPUT_PULLUP);

  //Pantalla
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("*BLINK PONG*");
  lcd.setCursor(1, 1);
  lcd.print("Proyecto ELCO!");
}

void loop() {
  
  //Comprobación de boton reset pulsado
  boton4pulsado();           
  if(jugando == false){
    boton3pulsado();
    boton2pulsado();
  } else {     
    
    //Refresco de la mesh
    mesh.update();
    mesh.DHCP();
  
    //Comprueba estar dentro de la red
    if (network.available()) {     
      RF24NetworkHeader header;
      CCCP_packet packet;
      network.peek(header);
  
      uint32_t dat = 0;
      
      switch (header.type) {
        case 'M':
          network.read(header, &packet, sizeof(packet));
  
          //Lectura de paquetes
          //readCCCPPacket(packet);  //instrucción para debuggear código
          tipo = packet.type ;
          datos_mandados = packet.data;
          instruccion = packet.proc_id;
          device = packet.dev_id;
          
          //Si recibe un paquete con la instrucción tengoConexion se añade ese dispositivo al array de dispositivos 
          //conectados a la mesh y se incrementa un contador del número de posavasos conectados. Cuando este número 
          //coincida con el número de posavasos en juego, la centralita se dispondrá a asignar equipos.
          switch (instruccion){
            case tengoConexion:
              if(flagTengoConexion){
                for (int i=0; i< paramPosavasos; i++){
                  if(!(id_boolean[device])){
                    id_boolean[device]= true;
                    contadorPosavasos++;
                  }
                }
                if(contadorPosavasos==paramPosavasos){
                  ejemplo.proc_id = todosConectados;
                  flagTengoConexion = 0;
                  Serial.println("estan todos conectados");
                } 
              } 
            break;
          }
          
          Serial.print(" from RF24Network address 0");
          Serial.println(header.from_node, OCT);
          break;
          
          default:
          network.read(header, 0, 0);
          Serial.println(header.type);
          break;
      }
  
      //Cada vez que un posavasos recibe la instrucción de asignación de un equipo y la cumple, manda
      //un ack "tengoEquipo" y la centralita aumenta el número de posavasos que han asignado correctamente
      //un equipo
      if(packet.proc_id==tengoEquipo){
        contadorPosavasosConEquipo++;
      }
  
      //Mientras el número de posavasos con equipo asignado sea menor al número total de posavasos, se 
      //van asignando ambos equipos. Además el cerrojo de flagTengoConexion asegura de que esto solo se
      //realizará cuando todos los posavasos se encuentren en la mesh.
      if((!flagTengoConexion)&&(!todosConEquipo)) {
        
        //Asignación del equipo naranja a la primera mitad del número de posavasos
        if(contadorPosavasosConEquipo< paramPosavasos2){
          asignarEquipo1(packet);
  
          //Impresión en el display
          lcd.clear();
          lcd.setCursor(4, 0);
          lcd.print("ASIGNANDO");
          lcd.setCursor(5, 1);
          lcd.print("NARANJA");
          //readCCCPPacket(ejemplo);  //instrucción para debuggear código
        }
        
        //Asignación del equipo azul a la segunda mitad del número de posavasos
        if((contadorPosavasosConEquipo>= paramPosavasos2)&&(contadorPosavasosConEquipo < paramPosavasos)){
          asignarEquipo2(packet);
          
          //Impresión en el display
          lcd.clear();
          lcd.setCursor(4, 0);
          lcd.print("ASIGNANDO");
          lcd.setCursor(5, 1);
          lcd.print("AZULES");
          //readCCCPPacket(ejemplo);  //instrucción para debuggear código
        }
  
        //Comprobación de qye todos los posavasos tienen equipo
        if(contadorPosavasosConEquipo== paramPosavasos){
          todosConEquipo=true;
        }
      } 
  
      //Una vez todos los posavasos tienen equipo, se espera a que todos tengan un vaso encima para
      //comenzar el juego 
      if((todosConEquipo)&&(!flag_todosConVaso)){
  
        //Se manda en broadcast (dev_id=4) el aviso de que todos tienen equipo para que así los posavasos
        //comiencen a avisar del estado del sensor infrarrojo que indica la presencia de un vaso encima
        ejemplo.dev_id = 4;
        ejemplo.proc_id=todosTienenEquipo;
  
        //Cuando los posavasos informan de que tienen un vaso encima se incrementa un contador que cuando 
        //llegue al númeor total de posavasos en juego hará comenzar el juego      
        if(instruccion == tengoVaso){
          if(checkPrimerEnvio(packet)){ //Solo entra cuando recibe la instrucción de tengoVaso por primera vez por parte de cada posavasos y así que no haya problemas con el contador 
            contadorPosavasosConVaso++;
            //readCCCPPacket(packet);  //instrucción para debuggear código
          }
        }
        if(contadorPosavasosConVaso==paramPosavasos){
          flag_todosConVaso=true;
        } else {
          //Impresión en el display
          lcd.clear();
          lcd.setCursor(3, 0);
          lcd.print("ESPERANDO");
          lcd.setCursor(3, 1);
          lcd.print("VASOS:");
          lcd.setCursor(10, 1);
          lcd.print(contadorPosavasosConVaso);
        }
      }
  
      //Comienza el juego
      if(flag_todosConVaso){
        Serial.println("ESTAMOS JUGANDO");
        //readCCCPPacket(packet);  //instrucción para debuggear código
        if(flag_empezando_juego==true){
  
          //Broadcast de que comienza el juego
          ejemplo.dev_id = 4;
          ejemplo.proc_id=todosConVaso;
        }
  
        //Cuando se celebra un punto de cualquiera de los dos equipos, tras realizar la celebración
        //los posavasos mandan un ack para que la centralita vuelva a colocar a todos los posavasos
        //en el juego normal
        if(instruccion == ackV1 || instruccion == ackV2){
          ejemplo.dev_id = 4; //broadcast
          ejemplo.proc_id=todosConVaso; 
        }
                    
        juego(packet);
  
        //Victorias finales del equipo naranja o azul, cuando uno de los dos equipos se queda con 0 posavasos jugando
        if(vasos1==0){
          ejemplo.dev_id=4;
          ejemplo.proc_id=victoriaFinal2;
          flag_JuegoTerminado= true;
          lcd.clear();
          lcd.setCursor(4, 0);
          lcd.print("WINNER");
          lcd.setCursor(4, 1);
          lcd.print("AZULES");
        }
  
        if(vasos2==0){
          ejemplo.dev_id=4;
          ejemplo.proc_id=victoriaFinal1;
          flag_JuegoTerminado= true;
          lcd.clear();
          lcd.setCursor(4, 0);
          lcd.print("WINNER");
          lcd.setCursor(4, 1);
          lcd.print("NARANJAS");
        }
      }      
    }
  
    //Refresco temporal para el envío de paquetes
    if (millis() - displayTimer > 500) { 
      ctr++;
      for (int i = 0; i < mesh.addrListTop; i++) {
        payload_t payload = {millis(), ctr};
        if (mesh.addrList[i].nodeID == 1) {  //Búsqueda del nodo de la lista de direcciones
          payload = {ctr % 3, ctr};
        }
        RF24NetworkHeader header(mesh.addrList[i].address, OCT); //Construcción de cabecera
        
        Serial.println( network.write(header, &ejemplo, sizeof(ejemplo)) == 1 ? F("Send OK") : F("Send Fail")); //Mandando mensaje
      }
      displayTimer = millis();
    }
  } 
}

//Método de asignación del equipo naranja
void asignarEquipo1(CCCP_packet packetCCCP){
  for(int i= 0; i< paramPosavasos2; i++){
      ejemplo.dev_id=contadorPosavasosConEquipo; 
      ejemplo.proc_id=equipo1;     
  }
}

//Método de asignación del equipo azul
void asignarEquipo2(CCCP_packet packetCCCP){
  for(int j= paramPosavasos2; j< paramPosavasos; j++){
        ejemplo.dev_id=contadorPosavasosConEquipo; 
        ejemplo.proc_id=equipo2;
  }
}

//Método que comprueba si es la primera vez que se recibe un paquete con cierta instrucción de un dispositivo
bool checkPrimerEnvio(CCCP_packet packetCCCP){
  if(id_boolean_conVaso[packetCCCP.dev_id]==false){
    id_boolean_conVaso[packetCCCP.dev_id]= true;
    return true;
  } else{
    return false;
  }
}

//Impresión en el display del marcador durante el juego
void juego(CCCP_packet packetCCCP){
  if(packetCCCP.proc_id == derrotado){
    flag_empezando_juego = false;
    if(packetCCCP.dev_id < paramPosavasos2){
      vasos1--;
      ejemplo.dev_id=4;
      ejemplo.proc_id= victoria2;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("EQUIPO N");
      lcd.setCursor(11, 0);
      lcd.print(vasos1);
      lcd.setCursor(0, 1);
      lcd.print("EQUIPO A");
      lcd.setCursor(11, 1);
      lcd.print(vasos2);
    } else {
      vasos2--;
      ejemplo.dev_id=4;
      ejemplo.proc_id= victoria1;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("EQUIPO N");
      lcd.setCursor(11, 0);
      lcd.print(vasos1);
      lcd.setCursor(0, 1);
      lcd.print("EQUIPO A");
      lcd.setCursor(11, 1);
      lcd.print(vasos2);
    }
  } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("EQUIPO N");
      lcd.setCursor(11, 0);
      lcd.print(vasos1);
      lcd.setCursor(0, 1);
      lcd.print("EQUIPO A");
      lcd.setCursor(11, 1);
      lcd.print(vasos2);
   }
}

//Método para debuggear
void readCCCPPacket(CCCP_packet packetCCCP) {
  Serial.print("TIPO DE DEVICE 0(CENTRALITA) 1(POSAVASO): ");
  Serial.println(packetCCCP.type);

  Serial.print("IDENTIFICADOR DE DEVICE_ID: ");
  Serial.println(packetCCCP.dev_id);

  Serial.print("ID DE INSTRUCCION: ");
  Serial.println(packetCCCP.proc_id);

  Serial.print("DATOS: ");
  Serial.println(packetCCCP.data);
}




//********************************* Máquina de estados para el MENU DE INICIO ************************************+

//Método de activación del botón 2 (nota: los estados de lectura de los botones 3 y 4 funcionan de la misma manera)
void boton2pulsado() {        
  
  unsigned long tiemporebote2 = 50;  // tiempo para evitar problemas de rebote
  int lectura2 = digitalRead(PIN_BTN2);

  if ( bit_act2 == lectura2 ) {
    temporizador2 = 0;
  } else if ( temporizador2 == 0 ) {
    temporizador2 = millis();
  } else if ( millis() - temporizador2 > tiemporebote2 ) {
    bit_act2 = !bit_act2;
  }
  // Ya hemos leido el botón, podemos trabajar con él.
  if ( bit_prev2 == HIGH && bit_act2 == LOW ) { //Esto es el botón

    if(boton2 == 2){   //controlar el número para la máquina de estados
      boton2 = 0;
    }
    boton2 += 1;  //cuenta de cada estado
    switchestados();
  }
    bit_prev2 = bit_act2;  //guardar el estado anterior.
}

void boton3pulsado() {        //botón de enter
  boton3 = 0; //reiniciamos el valor a 0 para evitar perder el control del botón
  unsigned long tiemporebote3 = 50; // tiempo para evitar problemas de rebote
  int lectura3 = digitalRead(PIN_BTN3);

  if ( bit_act3 == lectura3 ) {
    temporizador3 = 0;
  } else if ( temporizador3 == 0 ) {
    temporizador3 = millis();
  } else if ( millis() - temporizador3 > tiemporebote3 ) {
    bit_act3 = !bit_act3;
  }
  // Ya hemos leido el botón, podemos trabajar con él.
  if ( bit_prev3 == HIGH && bit_act3 == LOW ) { //Esto es el cambio de estados dependiente del botón
    boton3 += 1;  //cuenta de cada estado
    switchestados();
  }
    bit_prev3 = bit_act3;  //guardar el estado anterior.
}


void boton4pulsado() {        //botón de reset (por implementar)

  boton4 = 0; //reiniciamos el valor a 0 para evitar perder el control del botón
  unsigned long tiemporebote4 = 50; // tiempo para evitar problemas de rebote
  int lectura4 = digitalRead(PIN_BTN4);

  if ( bit_act4 == lectura4 ) {
    temporizador4 = 0;
  } else if ( temporizador4 == 0 ) {
    temporizador4 = millis();
  } else if ( millis() - temporizador4 > tiemporebote4 ) {
    bit_act4 = !bit_act4;
  }
  // Ya hemos leido el botón, podemos trabajar con él.
  if ( bit_prev4 == HIGH && bit_act4 == LOW ) { //Esto es el cambio de estados dependiente del botón
    boton4 += 1;  //cuenta de cada estado
    //jugando=false;  //Reset por implementar
    boton2=0;
    switchestados();
  }
    bit_prev4 = bit_act4;  //guardar el estado anterior.
}


void switchestados(){ //Máquina de estados de la pantalla de menú principal
  switch (boton2) {
    case 0:
      state = pantallaprin0;
      break;
      case 1:
      state = juego20;
      break;
      case 2:
      state = juego12;
        break; 
      default:
        boton2=0;     
  }
  menuprin();
}

void menuprin(){ //Impresión de pantalla del menú principal
  switch (state) {
    case pantallaprin0:
      pantallaprin();
    break;
    case juego20:
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print("4 VASOS");
      if(boton3==1){
        boton2 = 0;
        jugando = true;
        //
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print("ENCIENDE");
      lcd.setCursor(3, 1);
      lcd.print("POSAVASOS");
      //paramPosavasos = 20;    //Escalabilidad por implementar
      }
    break;
    case juego12:
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print("12 VASOS");
      if(boton3==1){
        boton2= 0;
        jugando = true;
      lcd.clear();
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("ENCIENDE");
      lcd.setCursor(1, 1);
      lcd.print("POSAVASOS");
      //paramPosavasos = 12;    //Escalabilidad por implementar
      }
    break;
  }
}

void pantallaprin(){
  ejemplo.type = DEVTYPE;
  ejemplo.dev_id = DEVID;
  ejemplo.proc_id = setUpM;
  ejemplo.data = 0;
  //readCCCPPacket(ejemplo);  //instrucción para debuggear código
    
  //Conectar a la mesh y setup
  mesh.setNodeID(0);
  Serial.println(mesh.getNodeID());
  mesh.begin(); 

  for (int i=0; i< paramPosavasos; i++){    
    id_boolean[i]= false;
  }
  for (int i=0; i< paramPosavasos2; i++){  
    id_boolean_equipo1[i]= false;
  }
  for (int i=0; i< paramPosavasos2; i++){  
    id_boolean_equipo2[i]= false;
  }
  for (int i=0; i< paramPosavasos; i++){  
    id_boolean_conVaso[i]= false;
  }
  jugando = false; 
  
  //Display pantalla inicio
  lcd.clear();  
  lcd.setCursor(2, 0);
  lcd.print("*BLINK PONG*");
  lcd.setCursor(1, 1);
  lcd.print("Proyecto ELCO!");
}
