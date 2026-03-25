/*
Projekt megnevezése: - Csovago_gep
Módosítva: - 2026.03.19.
*/

#include <AccelStepper.h>       //Version 1.64.0
#include <MultiStepper.h>       //Version 1.64.0
#include <LiquidCrystal_I2C.h> //Library version:1.1

//tombAdat segéd paraméterek
int i, n, k = 0, controllPontok = 40;
float pi = 3.14159;
float korrekciosFaktor = 6.94;      // a nagy gép esetén ez 4.9   - tesztpad esetén 21.1

/* Paraméter megadás soros kommunikációval, tagolás " " karakterekkel.

      char     par_1          par_2              par_3             par_4              par_5               par_6              par_7
case  'J'     D              hossz              szög              na.                 na.                  na.                na.         Jogoláshoz szükséges adatok megadása
case  'X'     na.            na.                na.               na.                 na.                  na.                na.         JOG tangely mentén                                                                                                                      
case  'A'     na.            na.                na.               na.                 na.                  na.                na.         JOG szögben
csae  'D'     D              d                  szög              falvastagság        anyagmin.            na.                na.         Vágási paraméterek megadása
case  'H'     na.            na.                na.               na.                 na.                  na.                na.         vágás homlokra --> d
case  'P'     na.            na.                na.               na.                 na.                  na.                na.         vágás palástra --> D
*/

float par_1 = 0;                     // serial olvasás parseolásához paraméterek
float par_2 = 0;
float par_3 = 0; 
float par_4 = 0; 
float par_5 = 0;  
float par_6 = 0;
//float par_7 = 0;                   // optional


char receivedCommand;                //a letter sent from the terminal
bool newData, runallowed = false;    //booleans for new data from serial, and runallowed flag

// I/O handling
const int dirPin_1 = 2;              // Irány stepper_1
const int stepPin_1 = 3;             // Lépés stepper_1
const int dirPin_2 = 4;              // Irány stepper_2
const int stepPin_2 = 5;             // Lépés stepper_2
int enable_stepper_1 = 6;            // Enable / disabled stepper_1
int enable_stepper_2 = 7;            // Enable / disabled stepepr_2

int VESZSTOP = 19;                   // Vészstop gomb INTERRUPT bemenet
volatile bool veszstop_aktiv = true; // vész állapot változó (gép indításakor mindig vészben van) 
int START = 22;                      // Start gomb
int STOP = 23;                       // Stop gomb

int PLAZMASTART = 44;                // Plasma start, azaz a plazmavágót indít
int ZOLDLAMPA = 40;                  // Andon zöld
int PIROSLAMPA = 41;                 // Andon piros
//int STARTLED = 42;                 // Kapcsolószekrény Start gomb led (nem használjuk)

// time változók
unsigned long time;                  // time változó
unsigned long ledtime = 0;           // Led blink delay
unsigned long buttontime = 0;        // Button switch delay
int ledState = 0;                    // blink led állapot

// állapot változók
bool zoldAndonLampa = 0;
bool pirosAndonLampa = 0;
bool interruptHandling = 0;
bool startGombAllapot = 0;
bool stopGombAllapot = 0;

bool startRisingEdge = 0;
bool stopRisingEdge = 0;
int pozAdatFogadva = 0;
int allapotkod = 0;

// JOG offset segéd paraméter
float offsetX = 0;
float offsetA = 0;


LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

const int stepsPerRevolution = 200;  //Lépés per fordulat

#define motorInterfaceType 1

AccelStepper stepper1(motorInterfaceType, stepPin_1, dirPin_1);
AccelStepper stepper2(motorInterfaceType, stepPin_2, dirPin_2);

MultiStepper steppers;

//futtatott paraméterek:
float tombAdat[41][13];
double jogAdat[3];
//----------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------------
// par_1--> para_1, par_2--> para_2, par_3--> para_3  teszthez deffiniált paraméterek
float para_1 = 0, para_2 = 0, para_3 = 0, para_4 = 0, para_5 = 0;  //     D1                 d2              angle


extern unsigned int __heap_start, *__brkval;
int freeMemory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup() {
  Serial.begin(9600);                             //Define baud rate
  Serial.print("Free RAM at startup: ");
  Serial.println(freeMemory());
  pinMode(stepPin_1, OUTPUT);
  pinMode(dirPin_1, OUTPUT);
  pinMode(stepPin_2, OUTPUT);
  pinMode(dirPin_2, OUTPUT);
  pinMode(enable_stepper_1, OUTPUT);
  pinMode(enable_stepper_2, OUTPUT);
  pinMode(START, INPUT_PULLUP);
  pinMode(STOP, INPUT_PULLUP);
  pinMode(VESZSTOP, INPUT_PULLUP);
  pinMode(PLAZMASTART, OUTPUT);
  pinMode(ZOLDLAMPA, OUTPUT);
  pinMode(PIROSLAMPA, OUTPUT);

  // Stepper disabled
  digitalWrite(PLAZMASTART, LOW);
  digitalWrite(ZOLDLAMPA, LOW);
  digitalWrite(PIROSLAMPA, HIGH);                 // Initial red andon light
  digitalWrite(enable_stepper_1, HIGH);           // Disabled stepepr state
  digitalWrite(enable_stepper_2, HIGH);           // Disabled stepepr state

  // Interrupt handling
  attachInterrupt(digitalPinToInterrupt(VESZSTOP), veszallapot, CHANGE);
  
  // initialize the lcd 
  lcd.init();
  lcd.backlight();

  //Define default motor parameters
  stepper1.setMaxSpeed(50);  //Steps / sec
  stepper2.setMaxSpeed(50);
  // Configure max. accelration
  stepper1.setAcceleration(800);  //Steps/ sec^2
  stepper2.setAcceleration(800);

  // Then give them to MultiStepper to manage
  steppers.addStepper(stepper1);
  steppers.addStepper(stepper2);

  //Serial.println(cos(pi));
}

void loop() {
  checkSerial();


/*
Andon lámpa kezelése:
Piros folyamatos: Vész, vészstop megnyomva, nyugtázás szükséges.
Piros villogás 1 Hz: Vészgombos nyugtázás és kapcsolószekrény nyugta gomb megnyomása után.
Zöld villogás 1 Hz: Megmunkálásra kész állapot.
Zöld folyamatos: Megmunkálási állapot.
*/
  time = millis();

  if((time-ledtime)>1000){
    ledtime = time;
    ledState = !ledState;
    if(ledState && zoldAndonLampa){
      digitalWrite(PIROSLAMPA,LOW);
      digitalWrite(ZOLDLAMPA,HIGH);
    }
    if(!ledState && zoldAndonLampa){
      digitalWrite(PIROSLAMPA,LOW);
      digitalWrite(ZOLDLAMPA,LOW);
    }
    if(ledState && pirosAndonLampa){ 
      digitalWrite(ZOLDLAMPA,LOW);
      digitalWrite(PIROSLAMPA,HIGH);
    }
    if(!ledState && pirosAndonLampa){ 
      digitalWrite(ZOLDLAMPA,LOW);
      digitalWrite(PIROSLAMPA,LOW);
    }
  }

// Start és Stop gombok állapot olvasása és prell kezelése
 if(digitalRead(START)== LOW){
  if((time-buttontime)>250){            // prell kezelése szoftveresen
  buttontime = time;
  if(allapotkod == 1 || allapotkod == 2 || allapotkod == 3 || allapotkod == 4){
    startRisingEdge = 1;
  }
  Serial.println("START");
  pirosAndonLampa = 0;
  zoldAndonLampa = 1;
  digitalWrite(PIROSLAMPA, LOW);
  digitalWrite(ZOLDLAMPA, HIGH);
  stopGombAllapot = 0;
  }}

  if(digitalRead(STOP)== LOW){
  if((time-buttontime)>250){            // prell kezelése szoftveresen
  buttontime = time;
  Serial.println("STOP");
  zoldAndonLampa = 0;
  pirosAndonLampa = 1;
  digitalWrite(ZOLDLAMPA, LOW);
  digitalWrite(PIROSLAMPA, HIGH);
  stopGombAllapot = 1;
  }}

// Funkció indítás nyomógombbal

if (pozAdatFogadva == 1){                               //Palya pontok kiszamolva
  if(allapotkod == 3 && startRisingEdge == 1){          //Palást megmunkálás
    startRisingEdge = 0;
    pozAdatFogadva = 0;
    allapotkod = 0;
    palyaPalast();

  }
  if(allapotkod == 4 && startRisingEdge == 1){           //Homlok megmunkálás
    startRisingEdge = 0;
    pozAdatFogadva = 0;
    allapotkod = 0;
    palyaHomlok();
  }
}

if (pozAdatFogadva == 2){                               //JOG adatok beérkeztek
  if(allapotkod == 1 && startRisingEdge == 1){           //JOG hossz
    startRisingEdge = 0;
    pozAdatFogadva = 0;
    allapotkod = 0;
    jogX();
  }
  if(allapotkod == 2 && startRisingEdge == 1){           //JOG tengely körül
    startRisingEdge = 0;
    pozAdatFogadva = 0;
    allapotkod = 0;
    jogA();
  }
}

//------------------------------------------------------------
// A gép mindig vészállapotban indul
// Vész állapot esetén a vész állapot okát meg kell szüntetni, majd nyugtázni a vészállapot megszüntetését Start vagy Stop gomb megnyomásával.
// Vész állapotból le lehet állítani a ciklus a Stop gombbal vagy lehet folytatni a Start gombbal
  if(veszstop_aktiv){
    Serial.println("VÉSZÁLLAPOT");
    lcd.setCursor(0,0);
    lcd.print("VESZALLAPOT!    ");
    Serial.println("Szüntesse meg a vészállapotot!");
    lcd.setCursor(0,1);
    lcd.print("VESZ STOP AKTIV");
    digitalWrite(enable_stepper_1, 1);
    digitalWrite(enable_stepper_2, 1);
    digitalWrite(ZOLDLAMPA, LOW);
    digitalWrite(PIROSLAMPA, HIGH);
    while(1){
      if(digitalRead(VESZSTOP) == HIGH)
      if((time-buttontime)>250){   
      veszstop_aktiv = false;
        lcd.setCursor(0,0);
        lcd.print("VESZ MEGSZUNT");
        lcd.setCursor(0,1);
        lcd.print("STOP / START ?  ");
        Serial.println("VÉSZ MEGSZŰNT");
      break;
    }}

    // Vész állípot után eldönthető, hogy folytatjuk a megmunkálást vagy befejezzük azt
    while(1){
      if(digitalRead(STOP) == LOW){             // Megmunkálás befejezése
        interruptHandling = 1;
        lcd.setCursor(0,0);
        lcd.print("FELADAT MEGSZAK-");
        lcd.setCursor(0,1);
        lcd.print("ITVA / UZEMKESZ ");
        break;
      } 
      if(digitalRead(START) == LOW){            // Megmunkálás folytatása
        interruptHandling = 2;
        lcd.setCursor(0,0);
        lcd.print("FELADAT FOLYTAT-");
        lcd.setCursor(0,1);
        lcd.print("ODIK / UZEMKESZ ");
        break;
      }
    }
  }
}

void veszallapot() {
  veszstop_aktiv = true;
}

void palyaParameterek() {
  for (; k <= controllPontok; k++) {
    tombAdat[k][0] = { k * 360 / controllPontok };                                                                                                                                                                             // fi paraméter fokban ( °)
    tombAdat[k][1] = { (pi / (controllPontok / 2)) * k };                                                                                                                                                                      // paraméter radiánban (RAD)
    tombAdat[k][2] = { sqrt(pow(para_1, 2) - pow(para_2, 2) * pow(cos(tombAdat[k][1]), 2)) };                                                                                                                                  // Xp (mm)
    tombAdat[k][3] = { para_2 * cos(tombAdat[k][1]) };                                                                                                                                                                         // Yp (mm)
    tombAdat[k][4] = { (para_2 / (cos(para_3 / 180 * pi))) * sin(tombAdat[k][1]) + (tan(para_3 / 180 * pi)) * sqrt(pow(para_1, 2) - pow(para_2, 2) * pow(cos(tombAdat[k][1]), 2)) };                                           // Zp (mm)
    tombAdat[k][5] = { atan(tombAdat[k][3] / tombAdat[k][2]) };                                                                                                                                                                // Szigma ( °)
    tombAdat[k][6] = 2*para_1*pi*(tombAdat[k][5]/(2 * pi));                                                                                                                                                                    // Ypk pont kiszámítva ívhosszra (mm)
    tombAdat[k][7] = { tombAdat[k][0] / 360 * 2 * pi * para_2 };                                                                                                                                                               // r(fi)
    tombAdat[k][8] = { (para_1 + para_2 * sin(para_3 / 180 * pi) - (para_2 * sin(para_3 / 180 * pi) * sin(tombAdat[k][1]) - sqrt(pow(para_1, 2) - pow(para_2, 2) * pow(cos(tombAdat[k][1]), 2)))) / cos(para_3 / 180 * pi) };  // p (fi)
    tombAdat[k][9] = { cos(-pi / 4) * tombAdat[k][4] - (sin(-pi / 4) * tombAdat[k][6]) };                                                                                                                                      // Zp forgatott (-45°) (palást)
    tombAdat[k][10] = { sin(-pi / 4) * tombAdat[k][4] + (cos(-pi / 4) * tombAdat[k][6]) };                                                                                                                                     // Ypk forgatott (-45°) (palást)
    tombAdat[k][11] = { cos(-pi / 4 * 3) * tombAdat[k][7] - (sin(-pi / 4 * 3) * tombAdat[k][8]) };                                                                                                                             // r(fi) -135° transzformált (homlok)
    tombAdat[k][12] = { sin(-pi / 4 * 3) * tombAdat[k][7] + (cos(-pi / 4 *3) * tombAdat[k][8]) };                                                                                                                              // p (fi) -135° transzformált (homlok)
  }

  Serial.println("Pontok számítása befejeződött");

/*
  // Pontok kiíratása debug céllal
  Serial.println("paraméter no.     FOK   RAD       Xp      Yp      Zp   Szigma(°)   Ypk      r(fi)      p(fi)     Zp(fi)    Ypk(fi)     r(fi)     P(fi) "), Serial.println(" ");

  for (; n < controllPontok+1; n++) {
    Serial.print(n + 1);
    Serial.print(".               ");
    Serial.print(int(tombAdat[n][0]));
    Serial.print("    ");
    Serial.print(tombAdat[n][1]);
    Serial.print("    ");
    Serial.print(tombAdat[n][2]);
    Serial.print("    ");
    Serial.print(tombAdat[n][3]);
    Serial.print("    ");
    Serial.print(tombAdat[n][4]);
    Serial.print("    ");
    Serial.print(tombAdat[n][5]);
    Serial.print("    ");
    Serial.print(tombAdat[n][6]);
    Serial.print("    ");
    Serial.print(tombAdat[n][7]);
    Serial.print("    ");
    Serial.print(tombAdat[n][8]);
    Serial.print("    ");
    Serial.print(tombAdat[n][9]);
    Serial.print("    ");
    Serial.print(tombAdat[n][10]);
    Serial.print("    ");
    Serial.print(tombAdat[n][11]);
    Serial.print("    ");
    Serial.println(tombAdat[n][12]);
  }
*/

}
//------------------------------------------------------------

void pozicioFelvetel(long pos0, long pos1){
  interruptHandling = 0;
  digitalWrite(ZOLDLAMPA, HIGH);
  digitalWrite(enable_stepper_1, LOW); //Stepper_mozgás_engedélyezés
  digitalWrite(enable_stepper_2, LOW); //Stepper_mozgás_engedélyezés

  Serial.println("Mozgás indul!");

  stepper1.setMaxSpeed(100);
  stepper2.setMaxSpeed(100);
  stepper1.moveTo(pos0);
  stepper2.moveTo(pos1);

  /*
Pozíció felvételekor a vészállapot kezelése csak bonyolult módon oldható meg, mert a pozíció felvétel blokkoló folyamat multistepper esetén. 
Ezért a multistepper mozgatás helyett motoronkénti pozíció felvételt alkalmazok, mely során az interrupt olvasható.
*/

  while(stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0){
    if(veszstop_aktiv){
      digitalWrite(enable_stepper_1, HIGH); //Stepper_mozgás_tiltás
      digitalWrite(enable_stepper_2, HIGH); //Stepper_mozgás_tiltás

      stepper1.stop();
      stepper2.stop();

      Serial.println("VÉSZSTOP AKTÍV – Mozgás megállítva");

      while(veszstop_aktiv){
        if (digitalRead(VESZSTOP) == HIGH) {
        veszstop_aktiv = false;
        }
      }

      Serial.println("VÉSZSTOP MEGSZŰNT – Döntés szükséges");

      bool dontesKeszen = false;

      while(!dontesKeszen){
        time = millis();
        if(digitalRead(START)== LOW){
          if((time-buttontime)>250){ 
            Serial.println("Mozgás folytatása");
            digitalWrite(enable_stepper_1, LOW); //Stepper_mozgás_engedélyezés
            digitalWrite(enable_stepper_2, LOW);
            stepper1.moveTo(pos0);
            stepper2.moveTo(pos1);
            dontesKeszen = true;
          }}
          if(digitalRead(STOP)== LOW){
            if((time-buttontime)>250){ 
              Serial.println("Mozgás leállítva");
              dontesKeszen = true;
            }
          }  
      }
    }
    stepper1.run();
    stepper2.run();
  }
  
  digitalWrite(PLAZMASTART, LOW);
  
  digitalWrite(enable_stepper_1, HIGH); //Stepper_mozgás_tiltás
  digitalWrite(enable_stepper_2, HIGH); //Stepper_mozgás_tiltás

  //Serial.println(jogAdat[0]);
  //Serial.println(jogAdat[1]);
  //Serial.println(jogAdat[2]);
  Serial.println(String("X offset: ") + offsetX);
  Serial.println(String("A offset: ") + offsetA);

  stepper1.setCurrentPosition(0);       // Abszolút elmozdulás számláló nullázása
  stepper2.setCurrentPosition(0);

}

void jogX(){
  long positions[2];
  offsetX += jogAdat[1];
  positions[0] = jogAdat[1] * korrekciosFaktor;
  positions[1] = - jogAdat[1] * korrekciosFaktor;
  pozicioFelvetel(positions[0], positions[1]);
  
}

void jogA(){
  long positions[2];
  offsetA += jogAdat[2];
  positions[0] = ((jogAdat[0]*pi)/360)*jogAdat[2] * korrekciosFaktor;
  positions[1] = ((jogAdat[0]*pi)/360)*jogAdat[2] * korrekciosFaktor;
  pozicioFelvetel(positions[0], positions[1]);
}



void palyaPalast() {
  stepper1.setCurrentPosition(0);       // Abszolút elmozdulás számláló nullázása
  stepper2.setCurrentPosition(0);

  digitalWrite(ZOLDLAMPA, HIGH);

  Serial.println("Palást megmunkálás indul!");
  unsigned long t0 = millis();
  while (millis() - t0 < 1000) {
  }

  digitalWrite(PLAZMASTART, HIGH);
  while (millis() - t0 < 1000) {
  }

  long positions[2];


  // Pályapontok követése során a multistepper funkciót kell használni a pálya szabályos lekövetéséhez. 
  // A motoroknak egyszerre kell indulniuk és megállniuk.
  stepper1.setMaxSpeed(para_4);           //Steps / sec
  stepper2.setMaxSpeed(para_4);
  digitalWrite(enable_stepper_1, LOW);    //Stepper_mozgás_engedélyezés
  digitalWrite(enable_stepper_2, LOW);    //Stepper_mozgás_engedélyezés
  
  for (; i < controllPontok+1; i++) {
    if(veszstop_aktiv){
      digitalWrite(enable_stepper_1, HIGH); //Stepper_mozgás_tiltás
      digitalWrite(enable_stepper_2, HIGH); //Stepper_mozgás_tiltás

      stepper1.stop();
      stepper2.stop();

      Serial.println("VÉSZSTOP AKTÍV – Mozgás megállítva");

      while(veszstop_aktiv){
        if (digitalRead(VESZSTOP) == HIGH) {
        veszstop_aktiv = false;
        }
      }

      Serial.println("VÉSZSTOP MEGSZŰNT – Döntés szükséges");

      while(1){
        time = millis();
        if(digitalRead(START)== LOW){
          if((time-buttontime)>250){
            goto folytatas;
          }
        }
        if(digitalRead(STOP)== LOW){
          if((time-buttontime)>250){
            goto megszakit;
          }
        }
      }
    }
    

    folytatas:
    digitalWrite(enable_stepper_1, LOW);    //Stepper_mozgás_engedélyezés
    digitalWrite(enable_stepper_2, LOW);    //Stepper_mozgás_engedélyezés
    positions[0] = -tombAdat[i][9] * korrekciosFaktor;
    positions[1] = -tombAdat[i][10] * korrekciosFaktor;
    steppers.moveTo(positions);
    steppers.runSpeedToPosition();
    //plotteres megjelenítéshez
    Serial.print(positions[0]);
    Serial.print("; ");
    Serial.print(positions[1]);
    Serial.print("; ");
    Serial.print(i);
    Serial.println(";");
  }

  // pályapontokról leállás [0,0] -ra.
  positions[0] = 0;
  positions[1] = 0;
  pozicioFelvetel(positions[0], positions[1]);   

  megszakit:
  digitalWrite(PLAZMASTART, LOW);

  digitalWrite(enable_stepper_1, HIGH); //Stepper_mozgás_tiltás
  digitalWrite(enable_stepper_2, HIGH); //Stepper_mozgás_tiltás
  
}

/*
void bluetoothTest(){
  Serial.println("Bluetoth OK");
  digitalWrite(ZOLDLAMPA, HIGH);
  
}
*/

void palyaHomlok() {
  stepper1.setCurrentPosition(0);       // Abszolút elmozdulás számláló nullázása
  stepper2.setCurrentPosition(0);

  digitalWrite(ZOLDLAMPA, HIGH);

  Serial.println("Gorbe kovetes teszt indul HOMLOK!");
  unsigned long t0 = millis();
  while (millis() - t0 < 1000) {
  }

  digitalWrite(PLAZMASTART, HIGH);
  while (millis() - t0 < 1000) {
  }

  long positions[2];


  // Pályapontok követése során a multistepper funkciót kell használni a pálya szabályos lekövetéséhez. 
  // A motoroknak egyszerre kell indulniuk és megállniuk.
  stepper1.setMaxSpeed(para_4);           //Steps / sec
  stepper2.setMaxSpeed(para_4);
  digitalWrite(enable_stepper_1, LOW);    //Stepper_mozgás_engedélyezés
  digitalWrite(enable_stepper_2, LOW);    //Stepper_mozgás_engedélyezés

  for (; i < controllPontok+1; i++) {
    if(veszstop_aktiv){
      digitalWrite(enable_stepper_1, HIGH); //Stepper_mozgás_tiltás
      digitalWrite(enable_stepper_2, HIGH); //Stepper_mozgás_tiltás

      stepper1.stop();
      stepper2.stop();

      Serial.println("VÉSZSTOP AKTÍV – Mozgás megállítva");

      while(veszstop_aktiv){
        if (digitalRead(VESZSTOP) == HIGH) {
        veszstop_aktiv = false;
        }
      }

      Serial.println("VÉSZSTOP MEGSZŰNT – Döntés szükséges");

      while(1){
        time = millis();
        if(digitalRead(START)== LOW){
          if((time-buttontime)>250){
            goto folytat;
          }
        }
        if(digitalRead(STOP)== LOW){
          if((time-buttontime)>250){
            goto megszak;
          }
        }
      }
    }

      folytat:
      digitalWrite(enable_stepper_1, LOW);    //Stepper_mozgás_engedélyezés
      digitalWrite(enable_stepper_2, LOW);    //Stepper_mozgás_engedélyezés
      positions[0] = -tombAdat[i][11] * korrekciosFaktor;
      positions[1] = -tombAdat[i][12] * korrekciosFaktor;
      steppers.moveTo(positions);
      steppers.runSpeedToPosition();
      Serial.print(positions[0]);
      Serial.print("; ");
      Serial.print(positions[1]);
      Serial.print("; ");
      Serial.print(i);
      Serial.println(";");
  }

/* Nem kell a leállás, mert ott kell befejeznie, ahol elkezdte.
  // pályapontokról leállás [0,0] -ra.
  positions[0] = 0;
  positions[1] = 0;
  pozicioFelvetel(positions[0], positions[1]);   
*/

  megszak:
  digitalWrite(PLAZMASTART, LOW);

  digitalWrite(enable_stepper_1, HIGH); //Stepper_mozgás_tiltás
  digitalWrite(enable_stepper_2, HIGH); //Stepper_mozgás_tiltás
}

//------------------------------------------------------------
void checkSerial() {
  if (Serial.available() > 0) {
    receivedCommand = Serial.read();
    newData = true;
    if (newData == true) {
      switch (receivedCommand) {
        case 'D':
          para_1 = Serial.parseFloat();         // csőátmérő megmunkált D
          para_2 = Serial.parseFloat();         // csőátmérő csatlakozó d
          para_3 = Serial.parseFloat();         // csatlakozási szög alfa
          para_4 = Serial.parseFloat();         // anyagvastagság
          para_5 = Serial.parseFloat();         // anyagminődség

          para_1 = para_1/2;                  // Mert sugárral számolunk, a UI pedig átmérőt kér!
          para_2 = para_2/2;                  // Mert sugárral számolunk, a UI pedig átmérőt kér!

          pozAdatFogadva = 1;

          Serial.println(para_1 * 2);
          Serial.println(para_2 * 2);
          Serial.println(para_3);
          Serial.println(para_4);
          Serial.println(para_5);

          k = 0;                                                                                            
          n = 0;
          palyaParameterek();
          break;

        case 'J':                               // JOG hosszanti és szög
          jogAdat[0] = Serial.parseFloat();         // átmérő
          jogAdat[1] = Serial.parseFloat();         // hossz
          jogAdat[2] = Serial.parseFloat();         // szög

          pozAdatFogadva = 2;  

          Serial.println(jogAdat[0]);
          Serial.println(jogAdat[1]);
          Serial.println(jogAdat[2]);
          break;

        case 'X':                               // JOG hossz mentén
          allapotkod = 1;
          Serial.println("X parancs jött");
          //jogX();
          break;

        case 'A':                               // JOG szögben
          allapotkod = 2;
          Serial.println("A parancs jött");
          //jogA();
          break;

        case 'P':                               // vágás palástra
          allapotkod = 3;
          //palyaPalast();
          i = 0;
          k = 0;
          break;

        case 'H':                               // vágás homlokra
          allapotkod = 4;
          //palyaHomlok();
          i = 0;
          k = 0;
          break;
/*
        case 'P':
          bluetoothTest();
          break;  
*/

        default:
          //skip
          break;
      }
    }
    newData = false;
  }
}
