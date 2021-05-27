//Librerías utilizadas
#include <Adafruit_NeoPixel.h>
#include "RF24Network.h"
#include "RF24.h"
#include "RF24Mesh.h"
#include <SPI.h>

//*********************** protocolo radio CCCP: CCCP consigue cogorzas potentes **************************************
typedef struct {
    unsigned short type : 1;      // 0 master, 1 posavaso
    unsigned short dev_id : 7;    // Unique identifier for the device
    unsigned short proc_id : 4;   // Uniquely identify the individual sending or receiving application process
    unsigned short data : 24;
} CCCP_packet;      // CCCP -> CCCP Consegue Cogorzas Potentes


#define DEVTYPE 1       // TYPE: Posavasos (slave)
#define DEVID 2         // DEV_ID: Identificador del dispositivo

//Configuración de los pines CE y CS del nrf24l01
RF24 radio(9, 10);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

//Nodo a trabajar en la mesh
#define nodeID 2

//Declaración de los colores de cada equipo
uint32_t naranja= Adafruit_NeoPixel::Color(255, 50, 0);
uint32_t azul= Adafruit_NeoPixel::Color(0, 0, 255);
uint32_t rojo= Adafruit_NeoPixel::Color(255, 0, 0);
uint32_t ColorEquipo1 = naranja
uint32_t ColorEquipo2 = azul
uint32_t color_equipo;
int equipo = 0;

//envío temporizado de paquetes
uint32_t displayTimer = 0;
struct payload_t {
  unsigned long ms;
  unsigned long counter;
};

//Inicialización del sensor infrarrojo
const int D0=7; //digital input 

//Declaración de la tira de leds NeoPixel
#define LEDPIN 4
#define LED_COUNT 9
Adafruit_NeoPixel strip(LED_COUNT, LEDPIN, NEO_GRB + NEO_KHZ800);

//Declaración del paquete que se envía
CCCP_packet ejemplo;
unsigned short tipo;
unsigned short instruccion; 
unsigned short datos_mandados;

//********  Identificadores para las instrucciones (proc_id) protocolo CCCP   ********
//Slaves
unsigned short tengoConexion = 0;
unsigned short tengoEquipo = 3;
unsigned short tengoVaso = 4;
unsigned short listoParaJugar = 7;
unsigned short derrotado = 15;
unsigned short setUpS = 14;
unsigned short ackV1 = 13;
unsigned short ackV2 = 2;

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

//Cerrojos asociados a las instrucciones
bool flagTodosConectados = 1;
bool flagEquipo1 = 1;
bool flagEquipo2 = 1;
bool flagTodosTienenEquipo = 1;
bool flagTodosConVaso = 1;
bool flagVictoria1 = 1;
bool flagVictoria2 = 1;
bool flagVictoriaFinal1 = 1;
bool flagVictoriaFinal2 = 1;
bool flagSetUpM = 1;

//flags del juego
bool juegoEmpezado = 1; //Flag "El juego ha comenzado"
bool flagOn = 1; //Flag "Estoy participando en el juego"


void setup() {
  Serial.begin(115200);
  pinMode(D0, INPUT);

  //Inicialización del paquete CCCP del posavasos
  ejemplo.type = DEVTYPE;
  ejemplo.dev_id = DEVID;
  ejemplo.proc_id = setUpS;
  //ejemplo.data = 0;
  //readCCCPPacket(ejemplo); //Instrucción para debuggear código

  // Conexión a la mesh
  mesh.setNodeID(nodeID);
  mesh.begin(); //Conectar a la mesh
  
  //Inicialización de la tira de LEDs
  strip.begin();
  strip.show();
}

void loop() {

  //refresco de la mesh
  mesh.update();

  //Comprueba estar dentro de la red
  while (network.available()) {

    //Inicialización del paquete recibido y creación de variables de trabajo
    RF24NetworkHeader header;
    CCCP_packet packet;
    network.read(header, &packet, sizeof(CCCP_packet));
    tipo = packet.type ;
    datos_mandados = packet.data;
    instruccion = packet.proc_id;

     //Comprobación de que el paquete recibido o bien es broadcast o bien es para mí
    if((int(packet.type) == 0)&&((packet.dev_id == DEVID) || (packet.dev_id==4))){
      //readCCCPPacket(packet); //Instrucción para debuggear
      switch (instruccion){
        
        case setUpM: //estado de espera. Es aquí donde en futuro se reseteará todo
          esperaEquipos();
          if(flagSetUpM){
          ejemplo.proc_id = tengoConexion;
          sendPacket(&ejemplo);
          flagSetUpM = 0;
          }
        break;

        case equipo1: //Se ha asignado correctamente el equipo naranja
          if(flagEquipo1){
            ejemplo.proc_id = tengoEquipo;
            color_equipo = naranja;
            sendPacket(&ejemplo);
            flagEquipo1 = 0;
            setFullColor(color_equipo);
            equipo = 1;
          }
        break;

        case equipo2: //Se ha asignado correctamente el equipo 2
          if(flagEquipo2){            
            ejemplo.proc_id = tengoEquipo;
            color_equipo = azul;
            sendPacket(&ejemplo);
            flagEquipo2 = 0;
            setFullColor(color_equipo);
            equipo = 2;
          }
        break;

        case todosTienenEquipo: //Se recibe cuando se han asignado correctamente todos los equipos
          if (flagTodosTienenEquipo){            
            boolean vaso = sensorVaso() ; 
              if(vaso){
              ejemplo.proc_id = tengoVaso; //Se comunica a la centralita la presencia de vaso
              sendPacket(&ejemplo);
              flagTodosTienenEquipo = 0;
              }
          }
        break;

        case todosConVaso: //Juego comenzado
        if(flagOn){
          setFullColor(color_equipo); //Se "pintan" los posavasos del color de cada equipo
        }
          flagVictoria1=1;
          flagVictoria2=1;
          if (flagTodosConVaso){ 
          flagTodosConVaso = 0;
          ejemplo.proc_id = listoParaJugar; //Se envía un paquete indicando que puede dar comienzo la partida
          sendPacket(&ejemplo);
          LEDEmpiezaJuego(color_equipo);
          }
          if(!sensorVaso()){ //Se deja de detectar presencia en el vaso
            if (flagOn){
            crossFade(color_equipo, Adafruit_NeoPixel::Color(0, 0, 0), 20);
            ejemplo.proc_id = derrotado; //Se envía un paquete indicando que no hay vaso
            sendPacket(&ejemplo);
            flagOn = 0;
            }
          }
        break;
    

        case victoria1:
        if(flagVictoria1 && flagOn){
          if (equipo == 1){
            vasoBebido(color_equipo);
            ejemplo.proc_id = ackV1; //Se envía a la centralita la información de que el equipo 1 ha acertado un vaso
            sendPacket(&ejemplo);
            flagVictoria1=0;
          }
        }
        break;

        case victoria2:
        if(flagVictoria2 && flagOn){
          if (equipo == 2){
            vasoBebido(color_equipo);
            ejemplo.proc_id = ackV2; //Se envía a la centralita la información de que el equipo 2 ha acertado un vaso
            sendPacket(&ejemplo);
            flagVictoria2=0;
          }
        }
        break;

        case victoriaFinal1:
            if (flagVictoriaFinal1 && equipo == 1){
            victoriaLuz(color_equipo);//No hace falta mandar la información de que un equipo ha ganado, puesto que la centralita está contando los vasos restantes
            flagVictoriaFinal1=0;
          }          
        break;

        case victoriaFinal2:
          if (flagVictoriaFinal2 && equipo == 2){
            victoriaLuz(color_equipo);
            flagVictoriaFinal2 = 0;
          }
        break;
      }
    } 

// FUNCIONES DE INFRARROJO--------------------------------------------------------------------------------------------------------------------------------------------
boolean sensorVaso(){
  int Dvalue=0;
  Dvalue=digitalRead(D0);
  if(Dvalue==LOW){ 
    return true;
  } else{
    return false;
  }

  
}
// FUNCIONES DE NRF--------------------------------------------------------------------------------------------------------------------------------------------

void readCCCPPacket(CCCP_packet packetCCCP) { //Este método se usa para debuggear
  Serial.print("TIPO DE DATOS: ");
  Serial.println(packetCCCP.type);

  Serial.print("TIPO DE DEVICE_ID: ");
  Serial.println(packetCCCP.dev_id);

  Serial.print("ID DE INSTRUCCION: ");
  Serial.println(packetCCCP.proc_id);

  Serial.print("DATOS: ");
  Serial.println(packetCCCP.data);
}
  
//Efectos de luz para las tiras de LEDs--------------------------------------------------------------------------------------------------------------------------------------------------
void esperaEquipos(){
//  setFullColor2(0, 255, 0);
  crossFade(Adafruit_NeoPixel::Color(0, 255, 0), Adafruit_NeoPixel::Color(0, 0, 0), 4);
  delay(100);
  crossFade(Adafruit_NeoPixel::Color(0, 0, 0), Adafruit_NeoPixel::Color(0, 255, 0), 4);
  delay(75);
}

void vasoBebido(uint32_t color){
  Strobe(color, 40, 30, 40);
  RunningLights(color, 30, 10);
  Strobe(color, 40, 25, 40);
}

void victoriaLuz(uint32_t color){
  Strobe(color, 60, 30, 40);
  Strobe(color, 9, 200, 40);
  RunningLights(color, 30, 20);
  Strobe(color, 8, 250, 40);
}

void LEDEmpiezaJuego(uint32_t color){
  Sparkle(color, 20, 75);
  crossFade(Adafruit_NeoPixel::Color(0, 0, 0), color, 15);
}

// FUNCIONES DE LUCES CRUDAS (esto es el puro encendido y apagado de LEDs--------------------------------------------------------------------------------------------
void Strobe(uint32_t color, int StrobeCount, int FlashDelay, int EndPause){
    byte red = (color >> 16) & 0xff;
    byte green = (color >> 8) & 0xff;
    byte blue = color & 0xff;  
  for(int j = 0; j < StrobeCount; j++) {
    for (int i=0; i<LED_COUNT; i++){
    strip.setPixelColor(i, red, green, blue);
    }
    strip.show();
    delay(FlashDelay);
    for (int i=0; i<LED_COUNT; i++){
    strip.setPixelColor(i, 0, 0, 0);
    }
    strip.show();
    delay(FlashDelay);
  }
 
 delay(EndPause);
}

void RunningLights(uint32_t color, int WaveDelay, int vueltas) {
  int Position=0;
  byte red = (color >> 16) & 0xff;
  byte green = (color >> 8) & 0xff;
  byte blue = color & 0xff;
  for(int j=0; j<LED_COUNT*vueltas; j++)
  {
      Position++; // = 0; //Position + Rate;
      for(int i=0; i<LED_COUNT; i++) {
        // sine wave, 3 offset waves make a rainbow!
        //float level = sin(i+Position) * 127 + 128;
        //setPixel(i,level,0,0);
        //float level = sin(i+Position) * 127 + 128;
        strip.setPixelColor(i,((sin(i+Position) * 127 + 128)/255)*red,
                   ((sin(i+Position) * 127 + 128)/255)*green,
                   ((sin(i+Position) * 127 + 128)/255)*blue);
      }
     
      strip.show();
      delay(WaveDelay);
  }
}

void crossFade(const uint32_t startColor, const uint32_t endColor, unsigned long speed_led) {
    byte startRed= (startColor >> 16) & 0xff;
    byte startGreen= (startColor >> 8) & 0xff;
    byte startBlue= startColor & 0xff;

    byte endRed= (endColor >> 16) & 0xff;
    byte endGreen= (endColor >> 8) & 0xff;
    byte endBlue= endColor & 0xff;    

    for (int step = 0; step < 256; step++){
      byte red = map (step, 0, 255, startRed, endRed);
      byte green = map (step, 0, 255, startGreen, endGreen);
      byte blue = map (step, 0, 255, startBlue, endBlue);
      setFullColor2(red, green, blue);
      delay(speed_led);

    } 
}

void setFullColor2(int r, int g, int b){
  for (int i=0; i<LED_COUNT; i++){
  strip.setPixelColor(i, r, g, b);
  }
  strip.show();
}

void Sparkle(uint32_t color, int SpeedDelay, int count) {
  
  
  byte red = (color >> 16) & 0xff;
  byte green = (color >> 8) & 0xff;
  byte blue = color & 0xff;
  for (int i=0; i<count; i++){
      int Pixel = random(LED_COUNT);
      strip.setPixelColor(Pixel,red,green,blue);
      strip.show();
      delay(SpeedDelay);
      strip.setPixelColor(Pixel,0,0,0);
      strip.show();
  }
}

void setFullColor(const uint32_t color) {
    byte red = (color >> 16) & 0xff;
    byte green = (color >> 8) & 0xff;
    byte blue = color & 0xff;
    setFullColor2(red, green, blue);
}
// Método de envío de paquetes. Envía el paquete 5 veces cada 500 ms, a no ser que haya error en los 5, en cuyo caso vuelve a mandar una ristra de 5 paquetes
boolean sendPacket(CCCP_packet* packet){
    uint8_t retry = 0;    // Count retries, if send fail
    boolean a = mesh.write(packet, 'M', sizeof(packet));
    while(!a && retry < 5){
      retry++;  // Count retry
      // delay 500 millis
      displayTimer = millis();
      if (millis() - displayTimer >= 500) {
        displayTimer = millis();
      }
    }

    if(retry == 5){
      if ( ! mesh.checkConnection() ) {
          //refresh the network address
          if(!mesh.renewAddress()){
            //If address renewal fails, reconfigure the radio and restart the mesh
            //This allows recovery from most if not all radio errors
            mesh.begin();
            sendPacket(packet);
            Serial.println("SOCORRO");
          }
        } 
    }
    readCCCPPacket(*packet);
    return a;
}
