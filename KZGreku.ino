/* *******************************************************
** Przykład użycia biblioteki, 
** przy pierwszym starcie tworzy plik konfiguracyjny
** po każdym restarcie dodaje do pliku symbol *
** ******************************************************* */

#include <KZGwifi.h>
#include <KZGmqtt.h>
#include <KZGwiatrak.h>
#include <KZGrekuKomora.h>

#define PIN_WIATRAK_CZERPNIA D6  //12
#define PIN_TACHO_WIATRAK_CZERPNIA D2  //pin gpio4
#define PIN_WIATRAK_WYWIEW D5  //14
#define PIN_TACHO_WIATRAK_WYWIEW D1   //5
#define PIN_ONEWIRE D7  //13

#define R_PWM_NAWIEW 'N'  // pwm wiatrak 1
#define R_PWM_WYWIEW 'W'   // pwm wiatrak 2
#define R_PWM_MANUAL 'M'  //oba razem
#define R_ROZMRAZANIE_WIATRAKI 'R' //
#define R_ROZMRAZANIE_GGWC 'G' //
#define R_KOMINEK 'K' //
#define R_AUTO 'A' //tryb automatycznego dobrania predkosci
#define R_OFF 'O'
#define R_ZMIEN_TRYB 'T'
#define R_FORCE_MANUAL 'F'

#define T_AUTO 'a'
#define T_MANUAL 'm'
#define T_KOMINEK 'k'
#define T_ROZMRAZANIE_WIATRAKI 'r'
#define T_ROZMRAZANIE_GGWC 'g'
#define T_OFF 'o'
#define T_FORCE_MANUAL 'f' // wymuszenie manuala totalnego bez rozmrazania

#define KOMORA_CZERPNIA 0
#define KOMORA_WYRZUTNIA 1
#define KOMORA_NAWIEW 2
#define KOMORA_WYWIEW 3
#define KOMORA_ZEWN 4

#define WIATRAK_IN  0
#define WIATRAK_OUT 1
#define WIATRAKI_SZT 2
#define KOMORY_SZT 4


KZGwifi wifi;
KZGmqtt mqtt;
OneWire oneWire(PIN_ONEWIRE);

KZGwiatrak wiatraki[WIATRAKI_SZT]=
{
  KZGwiatrak(PIN_WIATRAK_CZERPNIA,PIN_TACHO_WIATRAK_CZERPNIA,WIATRAK_IN),
  KZGwiatrak(PIN_WIATRAK_WYWIEW,PIN_TACHO_WIATRAK_WYWIEW,WIATRAK_OUT)
};
DeviceAddress termometrAddr[]={  { 0x28, 0x52, 0x96, 0x23, 0x06, 0x00, 0x00, 0x5C },
                                        { 0x28, 0x04, 0x1E, 0x22, 0x06, 0x00, 0x00, 0xBA },
                                        { 0x28, 0xC0, 0x24, 0x23, 0x06, 0x00, 0x00, 0x4F },
                                        { 0x28, 0x67, 0xD4, 0x22, 0x06, 0x00, 0x00, 0xC7 }};
KZGrekuKomora komory[KOMORY_SZT]=
{
  KZGrekuKomora(0,termometrAddr[0]),
  KZGrekuKomora(1,termometrAddr[1]),
  KZGrekuKomora(2,termometrAddr[2]),
  KZGrekuKomora(3,termometrAddr[3])
};

String pubTopic="KZGrekuOUT/";
String subTopic="KZGrekuIN/#";

char trybPracy=T_OFF;
char trybPracyPop=T_OFF;
unsigned long kominekMillis=0;

bool isIntChars(char * ctab) 
{
  
  bool decPt = false;
  uint8_t startInd=0;
  if(ctab[0] == '+' || ctab[0] == '-') startInd=1;

  for(uint8_t x=startInd;x<strlen(ctab);x++)
  {
   if(!isDigit(ctab[x])) return false;
  }
  return true;
}
void isrIN()
{
  wiatraki[WIATRAK_IN].obslugaTachoISR();
}
void isrOUT()
{
  wiatraki[WIATRAK_OUT].obslugaTachoISR();
}
void callback(char* topic, byte* payload, unsigned int length) 
{
  char* p = (char*)malloc(length+1);
  memcpy(p,payload,length);
  p[length]='\0';
  /*if(strstr(topic,"watchdog"))
  {
    DPRINT("Watchdog msg=");
    DPRINT(p);
    DPRINT(" teraz=");
   
    if(isNumber(p))
      wifi.setWDmillis(strtoul (p, NULL, 0));
    DPRINTLN(wifi.getWDmillis());
    

  }*/
    if(topic[strlen(topic)-1]=='/')
        {topic[strlen(topic)-1] = '\0';}
    Serial.print("@@@@ Debug: MQTTcallback topic=");
    Serial.print(topic);
    Serial.print(" msg=");
    Serial.println(p);
    parsujRozkaz(topic,p);
  
  free(p);
}

void setup()
{
    Serial.begin(115200);
    //////////////////////////////// wifi /////////////////////////////
    wifi.begin();
    // ustawienie domyślnych AP
    Serial.print("Przygotuj domyslny config");
    wifi.dodajAP("InstalujWirusa","BlaBlaBla123");
  //  wifi.dodajAP("DOrangeFreeDom","KZagaw01_ruter_key"); 
   // wifi.dodajAP("KZG276BE76","s6z8rdsTmtrpff");
    wifi.initAP("TestWifiAP","qwerty");

    
    //wczytanie konfiguracji i ew zastapienie domyślnych ustawien
    //wifi.importFromFile();
    ////////////////////////////////////////////////////////////////////

    //////////////////////////// mqtt /////////////////////////////////
    mqtt.begin();
    mqtt.setCallback(callback);
    // domyślna konfiguracja
    Serial.print("Przygotuj domyslny config MQTT");
    mqtt.setMqtt("192.168.1.3",1883,"KZGreku1","","","KZGrekuDebug");

    mqtt.addSubscribeTopic(subTopic);
    //mqtt.importFromFile();
    ////////////////////////////////////////////////////////////////////
  //////////// komory //////////////////
   

    for(int i=0;i<KOMORY_SZT;i++)
    {
      komory[i].begin(&oneWire);
    }
    
    ////////////// wiatraki ///////////////////////////
     wiatraki[WIATRAK_IN].begin();
     wiatraki[WIATRAK_OUT].begin();
  
     attachInterrupt(digitalPinToInterrupt( wiatraki[WIATRAK_IN].dajISR()), isrIN, RISING );
     attachInterrupt(digitalPinToInterrupt( wiatraki[WIATRAK_OUT].dajISR()), isrOUT, RISING );
    //////////////////////////////////////
  
    ////////////////////////////
    Serial.println("Koniec Setup"); 
    delay(2000);
    Serial.println(".............................."); 
}

void setTrybPracy(char t)
{
  if(trybPracy==t) return;
  trybPracyPop=trybPracy;
  trybPracy=t;
  if(trybPracy==T_KOMINEK)
  {
    kominekMillis=millis();
  }
  String topic=pubTopic+"TrybPracy/";
  String msg=String(t);
  DPRINT("--- Zmiana TrybPracy:");DPRINTLN(msg);
  mqtt.mqttPub(topic,msg);
}

void parsujRozkaz(char* topic,char * msg)
{
    if(strstr(topic,"Wiatraki")>0)
    {
     if(isIntChars(msg))
     {
        realizujRozkaz(R_PWM_MANUAL,atoi(msg));       
     }else
     {
         DPRINT("ERR msg Wiatraki nie int, linia:");DPRINTLN(__LINE__);
     }
     return;
    }
   if(strstr(topic,"WiatrakN")>0)
   {
     if(isIntChars(msg))
     {
        realizujRozkaz(R_PWM_NAWIEW,atoi(msg));       
     }else
     {
         DPRINT("ERR msg WiatrakN nie int, linia:");DPRINTLN(__LINE__);
     }
     return;
    }
    if(strstr(topic,"WiatrakW")>0)
    {
      if(isIntChars(msg))
      {
        realizujRozkaz(R_PWM_WYWIEW,atoi(msg));
      }else
      {
        DPRINT("ERR msg WiatrakW nie int, linia:");DPRINTLN(__LINE__);
      }
     return; 
    }
    if(strstr(topic,"TrybPracy")>0)
    {
      //if(isIntChars(msg))
     // {
        realizujRozkaz(R_ZMIEN_TRYB,atoi(msg));
      //}else
     // {
      //  DPRINT("ERR msg WiatrakW nie int, linia:");DPRINTLN(__LINE__);
      //}
     return; 
    }
 /*   -------- nie inTopic a subTopic przemyslec 
    if(strlen(topic)==strlen(inTopic)+2)  //Reku/X
    {
       if(isIntChars(msg))
      {
        realizujRozkaz(topic[strlen(topic)-1],atoi(msg));
      }else
      {
        DPRINT("ERR msg ");DPRINT(msg);DPRINT(" nie int, linia:");DPRINTLN(__LINE__);
      }
      
     return; 
    }*/
}
void realizujRozkaz(uint16_t paramName,uint16_t paramValue) 
{
  switch(paramName)
  {
    case R_PWM_NAWIEW:
      if(trybPracy!=T_FORCE_MANUAL)setTrybPracy(T_MANUAL);
    //  wiatraki[WIATRAK_IN].ustawPredkosc(paramValue);
     ustawPredkoscPublikuj(WIATRAK_IN,paramValue);
    break;
    case R_PWM_WYWIEW:
      if(trybPracy!=T_FORCE_MANUAL)setTrybPracy(T_MANUAL);
      //wiatraki[WIATRAK_OUT].ustawPredkosc(paramValue);
      ustawPredkoscPublikuj(WIATRAK_OUT,paramValue);
    break;
    case R_PWM_MANUAL:
      if(trybPracy!=T_FORCE_MANUAL)setTrybPracy(T_MANUAL);
      if(paramValue==0){realizujRozkaz(R_OFF,0);break;}
      ustawPredkoscPublikuj(WIATRAK_IN,paramValue);
      ustawPredkoscPublikuj(WIATRAK_OUT,paramValue);
    break;
    case R_ROZMRAZANIE_WIATRAKI:
      if(trybPracy!=T_FORCE_MANUAL)setTrybPracy(T_ROZMRAZANIE_WIATRAKI) ;
    break;
    case R_ROZMRAZANIE_GGWC:
      if(trybPracy!=T_FORCE_MANUAL)setTrybPracy(T_ROZMRAZANIE_GGWC);
    break;
    case R_KOMINEK:
     if(trybPracy!=T_FORCE_MANUAL) setTrybPracy(T_KOMINEK);   
    break;
    case R_AUTO:
      if(trybPracy!=T_FORCE_MANUAL)setTrybPracy(T_AUTO);
    break;
    case R_OFF:
      setTrybPracy(T_OFF);
      ustawPredkoscPublikuj(WIATRAK_IN,0);
      ustawPredkoscPublikuj(WIATRAK_OUT,0);
    break;   
    case R_ZMIEN_TRYB:
      setTrybPracy(paramValue);
    break;     
  }
  
}


void automat()
{
  ///////////////////// wyznacz obroty wiatraka ////////////
            // gdy manual to tylko sprawdz czy nie zamarza
            //jesli temp wyrzutni < 2C ustaw rozmrazanie
            if((trybPracy!=T_OFF)&&(komory[KOMORA_WYRZUTNIA].dajTemp()<1.0f)&&(trybPracy!=T_FORCE_MANUAL))
            {
    //--- tymczasowo poku robie bez podlaczonych termometrów           
  //  setTrybPracy(T_ROZMRAZANIE_WIATRAKI);
            }
            switch(trybPracy)
            {
              case T_OFF:
                   wiatraki[WIATRAK_IN].ustawPredkosc(0);
                   wiatraki[WIATRAK_OUT].ustawPredkosc(0);
               //   ustawPredkoscPublikuj(WIATRAK_IN,0);
               //   ustawPredkoscPublikuj(WIATRAK_OUT,0);
                 break;
              case T_MANUAL:
                break;
              case T_AUTO:
                //pora roku czy chlodzic czy ziebic
                //temp zewn? czy z temp wewn da sie oszacować ile os
                //liczba osob = czy pusto/domownicy/impreza
                //pora dnia
                  wiatraki[WIATRAK_IN].ustawPredkosc(15);
                  wiatraki[WIATRAK_OUT].ustawPredkosc(15);
                  //ustawPredkoscPublikuj(WIATRAK_IN,15);
                  //ustawPredkoscPublikuj(WIATRAK_OUT,15);
                break;
              case T_KOMINEK:
              //todo czas 5min?
                if(millis()-kominekMillis>300000)//5min
                {
                    setTrybPracy(trybPracyPop);
                }
                  wiatraki[WIATRAK_IN].ustawPredkosc(50);
                  wiatraki[WIATRAK_OUT].ustawPredkosc(15);
                 //ustawPredkoscPublikuj(WIATRAK_IN,50);
                  //ustawPredkoscPublikuj(WIATRAK_OUT,15);
                break;
              case T_ROZMRAZANIE_WIATRAKI:
                if(komory[KOMORA_WYRZUTNIA].dajTemp()>3.0f)
                {
                  setTrybPracy(T_AUTO);
                } 
                wiatraki[WIATRAK_IN].ustawPredkosc(35);
                wiatraki[WIATRAK_OUT].ustawPredkosc(60);
               //  ustawPredkoscPublikuj(WIATRAK_IN,15);
               //  ustawPredkoscPublikuj(WIATRAK_OUT,30);
                break;
              case T_ROZMRAZANIE_GGWC:
                break;
            
            }
            
            // gdy auto to ???
            }

unsigned long status_ms=0;
unsigned long publish_ms=0;
uint8_t idPublish=0;
String topic="";
String msg="";
////////////
/// po zadanej zmianie predkosci od razu opublikuj na mqtt zmianę
void publikujWiatrak(uint8_t id)
{
   topic=pubTopic+"WIATRAK/"+String(id)+"/pub/";     
   msg=wiatraki[id].getStatusString();
   mqtt.mqttPub(topic,msg);
}
//// nakladka na ustaw predkosc wiatraka by jednoczenie po zadanej zmianie publikowal zadany rozkaz
void ustawPredkoscPublikuj(uint8_t id, uint8_t procent)
{
  wiatraki[id].ustawPredkosc(procent);
  publikujWiatrak(id);
}
void loop()
{
    wifi.loop();
    mqtt.loop();
    wiatraki[WIATRAK_IN].loop();
    wiatraki[WIATRAK_OUT].loop();
    delay(5);
    for(int i=0;i<KOMORY_SZT;i++)
    {
      komory[i].loop();
    }
  //  automat();        
    ////////////////// publish ///////////////
    if(millis()-publish_ms>2000)
    {
      if(idPublish<KOMORY_SZT)
      {
        topic=pubTopic+"KOMORA/"+idPublish+"/pub/";
        msg=komory[idPublish].getStatusString();
        mqtt.mqttPub(topic,msg);
      } else
      {
        publikujWiatrak(idPublish-KOMORY_SZT);
        ///topic=pubTopic+"WIATRAK/"+String(idPublish-KOMORY_SZT)+"/pub/";     
       /// msg=wiatraki[idPublish-KOMORY_SZT].getStatusString();
      }
     
      if(++idPublish >= 6) idPublish=0;
      publish_ms=millis();
    }
    /////////////// end publish ////////////
    if(millis()-status_ms>15000)
    {
        Serial.print("## ");Serial.print(wifi.getWifiStatusString());
        Serial.print(" #### ");Serial.print(wifi.getTimeString());Serial.println(" ####");

        String tmp=pubTopic+String("czas/");
        mqtt.mqttPub(tmp,wifi.getTimeString());        
        status_ms=millis();
    }
}
