/*
Projekt megnevezése: - Csovago_gep
Módosítva: - 2026.01.29.
*/

#include <AccelStepper.h>
#include <MultiStepper.h>

//tombAdat segéd paraméterek
int i, n, k = 0, controllPontok = 50;
float pi = 3.14159;

/* Paraméter megadás soros kommunikációval

      char     par_1          par_2              par_3             par_4              par_5               par_6              par_7
case  'J'     D              hossz              szög              na.                 na.                  na.                na.         Jogoláshoz szükséges adatok megadása
case  'X'     na.            na.                na.               na.                 na.                  na.                na.         JOG tangely mentén                                                                                                                      
case  'A'     na.            na.                na.               na.                 na.                  na.                na.         JOG szögben
csae  'D'     D              d                  szög              falvastagság        anyagmin.            na.                na.         Vágási paraméterek megadása
case  'H'     na.            na.                na.               na.                 na.                  na.                na.         vágás homlokra --> d
case  'H'     na.            na.                na.               na.                 na.                  na.                na.         vágás palástra --> D
*/

float par_1 = 0;                     // serial olvasás parsolásához paraméterek
float par_2 = 0;
float par_3 = 0; 
float par_4 = 0; 
float par_5 = 0;  
float par_6 = 0;
//float par_7 = 0;                   // optonal




char receivedCommand;                //a letter sent from the terminal
bool newData, runallowed = false;    //booleans for new data from serial, and runallowed flag

// I/O handling
const int dirPin_1 = 2;              // Irány stepper_1
const int stepPin_1 = 3;             // Lépés stepper_1
const int dirPin_2 = 4;              // Irány stepper_2
const int stepPin_2 = 5;             // Lépés stepper_2
int enable_stepper_1 = 6;            // Enable / disabled stepper_1
int enable_stepper_2 = 7;            // Enable / disabled stepepr_2

int VESZSTOP = 21;                   // Vészstop gomb INTERRUPT bemenet
int START = 22;                      // Start
int STOP = 23;                       // Stop

int PLAZMASTART = 44;                // Plasma start, azaz a plazmavágót indít
int ZOLDLAMPA = 40;                  // Andon zöld
int PIROSLAMPA = 41;                 // Andon piros
//int STARTLED = 42;                 // Kapcsolószekrény Start gomb led (nem használjuk)

// time változók
unsigned long time;                  // time változó
unsigned long ledtime = 0;           // Led blink delay
unsigned long buttontime = 0;        // Button switch delay
unsigned long testtime = 0;          // Interrupt testing time, for a counter
int ledState = 0;                    // blink led állapot

// állapot változók
int zoldAndonLampa = 0;
int pirosAndonLampa = 0;
bool interruptHandling = 0;

const int stepsPerRevolution = 200;  //Lépés per fordulat

#define motorInterfaceType 1

AccelStepper stepper1(motorInterfaceType, stepPin_1, dirPin_1);
AccelStepper stepper2(motorInterfaceType, stepPin_2, dirPin_2);

MultiStepper steppers;

//futtatott paraméterek:
float tombAdat[51][13];
double jogAdat[3];
//----------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------------------
// par_1--> para_1, par_2--> para_2, par_3--> para_3  teszthez deffiniált paraméterek
float para_1 = 0, para_2 = 0, para_3 = 0, para_4 = 0, para_5 = 0;  //     D1                 d2              angle

void setup() {
  Serial.begin(9600);  //Define baud rate
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

  //Define default motor parameters
  stepper1.setMaxSpeed(150);  //Steps / sec
  stepper2.setMaxSpeed(150);
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
// Ando lámpa villogtatás 
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
  Serial.println("START");
  pirosAndonLampa = 0;
  zoldAndonLampa = 1;
  digitalWrite(PIROSLAMPA, LOW);
  digitalWrite(ZOLDLAMPA, HIGH);
  }}

  if(digitalRead(STOP)== LOW){
  if((time-buttontime)>250){            q // prell kezelése szoftveresen
  buttontime = time;
  Serial.println("STOP");
  zoldAndonLampa = 0;
  pirosAndonLampa = 1;
  digitalWrite(ZOLDLAMPA, LOW);
  digitalWrite(PIROSLAMPA, HIGH);
  }}
}

//------------------------------------------------------------
// A gép mindig vészállapotban indul
// Vész állapot esetén a vész állapot okát meg kell szüntetni, majd nyugtázni a vészállapot megszüntetését.
// Vész állapotból le lehet állítani a ciklus a Stop gombbal vagy lehet folytatni a Start gombbal
void veszallapot(){
Serial.println("VÉSZÁLLAPOT");
Serial.println("Szüntesse meg a vészállapotot!");
digitalWrite(enable_stepper_1, 1);
digitalWrite(enable_stepper_2, 1);
digitalWrite(ZOLDLAMPA, LOW);
digitalWrite(PIROSLAMPA, HIGH);
while(1){
   if(digitalRead(VESZSTOP) == HIGH)
   break;
}

while(1){
  if(digitalRead(STOP) == LOW){
    interruptHandling = 1;
    break;
  } 
  if(digitalRead(START) == LOW){
    interruptHandling = 0;
    break;
  }
}
Serial.println("VÉSZ MEGSZŰNT");
}

//------------------------------------------------------------
void palyaParameterek() {
  for (; k <= controllPontok; k++) {
    if (k < 1) {
      //Serial.println("paraméter no.     FOK   RAD       Xp      Yp      Zp    Szigma      Ypk      r(fi)    p (fi)     Zp-tr    Ypk-tr  r(fi)-tr    p(fi)-tr "), Serial.println(" ");
    }
    tombAdat[k][0] = { k * 360 / controllPontok };                                                                                                                                                                             // paraméter fokban ( °)
    tombAdat[k][1] = { (pi / (controllPontok / 2)) * k };                                                                                                                                                                      // paraméter radiánban (RAD)
    tombAdat[k][2] = { sqrt(pow(para_1, 2) - pow(para_2, 2) * pow(cos(tombAdat[k][1]), 2)) };                                                                                                                                  // Xp (mm)
    tombAdat[k][3] = { para_2 * cos(tombAdat[k][1]) };                                                                                                                                                                         // Yp (mm)
    tombAdat[k][4] = { (para_2 / (cos(para_3 / 180 * pi))) * sin(tombAdat[k][1]) + (tan(para_3 / 180 * pi)) * sqrt(pow(para_1, 2) - pow(para_2, 2) * pow(cos(tombAdat[k][1]), 2)) };                                           // Zp (mm)
    //tombAdat[k][5] = { atan(tombAdat[k][3] / tombAdat[k][2]) };                                                                                                                                                              // Szigma ( °)
    if(tombAdat[k][2]==0){
      tombAdat[k][5] = 0;
      }else{
      atan(tombAdat[k][3] / tombAdat[k][2]);
      }
    //tombAdat[k][6] = { 2 * para_1 * pi * (tombAdat[k][5] / (2 * pi)) };                                                                                                                                                      // Ypk pont kiszámítva ívhosszra (mm)
    tombAdat[k][6] = 2*para_1*pi*(tombAdat[k][5]/360);
    tombAdat[k][7] = { tombAdat[k][0] / 360 * 2 * pi * para_2 };                                                                                                                                                               // r(fi)
    tombAdat[k][8] = { (para_1 + para_2 * sin(para_3 / 180 * pi) - (para_2 * sin(para_3 / 180 * pi) * sin(tombAdat[k][1]) - sqrt(pow(para_1, 2) - pow(para_2, 2) * pow(cos(tombAdat[k][1]), 2)))) / cos(para_3 / 180 * pi) };  // p (fi)
    tombAdat[k][9] = { cos(-pi / 4) * tombAdat[k][4] - (sin(-pi / 4) * tombAdat[k][6]) };                                                                                                                                      // Zp forgatott (-45°) (palást)
    tombAdat[k][10] = { sin(-pi / 4) * tombAdat[k][4] + (cos(-pi / 4) * tombAdat[k][6]) };                                                                                                                                     // Ypk forgatott (-45°) (palást)
    tombAdat[k][11] = { cos(-pi / 4) * tombAdat[k][7] - (sin(-pi / 4) * tombAdat[k][8]) };                                                                                                                                     // r(fi) -45° transzformált (homlok)
    tombAdat[k][12] = { sin(-pi / 4) * tombAdat[k][7] + (cos(-pi / 4) * tombAdat[k][8]) };                                                                                                                                     // p (fi) -45° transzformált (homlok)

  }
  Serial.println("paraméter no.     FOK   RAD       Xp      Yp      Zp    Béta        y'      i        P       Zp forg    y' forg   i forg    P forg "), Serial.println(" ");

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
}
//------------------------------------------------------------

void jogX(){
  long positions[2];
    positions[0] = 0;
    positions[1] = 0;

  digitalWrite(ZOLDLAMPA, HIGH);
  digitalWrite(enable_stepper_1, LOW); //Stepper_mozgás_engedélyezés
  digitalWrite(enable_stepper_2, LOW); //Stepper_mozgás_engedélyezés
  Serial.println("X-jog indul");
    positions[0] = jogAdat[1] * 4.9;
    positions[1] = - jogAdat[1] * 4.9;
    steppers.moveTo(positions);
    steppers.runSpeedToPosition();

  digitalWrite(PLAZMASTART, LOW);
  
  digitalWrite(enable_stepper_1, HIGH); //Stepper_mozgás_tiltás
  digitalWrite(enable_stepper_2, HIGH); //Stepper_mozgás_tiltás

  Serial.println(jogAdat[0]);
  Serial.println(jogAdat[1]);
  Serial.println(jogAdat[2]);

}
//}

void jogA(){
  long positions[2];

  positions[0] = 0;
  positions[1] = 0;

  digitalWrite(ZOLDLAMPA, HIGH);
  digitalWrite(enable_stepper_1, LOW); //Stepper_mozgás_engedélyezés
  digitalWrite(enable_stepper_2, LOW); //Stepper_mozgás_engedélyezés
  Serial.println("A-jog indul"); 
  positions[0] = ((jogAdat[0]*pi)/360)*jogAdat[2] * 4.9;
  positions[1] = ((jogAdat[0]*pi)/360)*jogAdat[2] * 4.9;
  steppers.moveTo(positions);
  steppers.runSpeedToPosition();

  digitalWrite(PLAZMASTART, LOW);
  
  digitalWrite(enable_stepper_1, HIGH); //Stepper_mozgás_tiltás
  digitalWrite(enable_stepper_2, HIGH); //Stepper_mozgás_tiltás

  Serial.println(jogAdat[0]);
  Serial.println(jogAdat[1]);
  Serial.println(jogAdat[2]);
}
//}



void palyaPalast() {
  digitalWrite(ZOLDLAMPA, HIGH);
  stepper1.setMaxSpeed(para_4);  //Steps / sec
  stepper2.setMaxSpeed(para_4);

  Serial.println("Gorbe kovetes teszt indul PALAST!");
  delay(1000);

  digitalWrite(enable_stepper_1, LOW); //Stepper_mozgás_engedélyezés
  digitalWrite(enable_stepper_2, LOW); //Stepper_mozgás_engedélyezés

  digitalWrite(PLAZMASTART, HIGH);
  delay(1000);

  long positions[2];

    // [0, 0] -ból pályára állás
    if(interruptHandling == 0){    
    positions[0] = 0;
    positions[1] = 0;
    steppers.moveTo(positions);
    steppers.runSpeedToPosition();
    Serial.print(positions[0]);
    //plotteres megjelenítéshez
    Serial.print("; ");
    Serial.print(positions[1]);
    Serial.print("; ");
    Serial.print(i);
    Serial.println(";");}


  // pályapontok követése  
  for (; i < controllPontok+1; i++) {
    //long positions[2];
    if(interruptHandling == 0){ 
    positions[0] = -tombAdat[i][9] * 4.9;
    positions[1] = -tombAdat[i][10] * 4.9;
    steppers.moveTo(positions);
    steppers.runSpeedToPosition();
    //plotteres megjelenítéshez
    Serial.print(positions[0]);
    Serial.print("; ");
    Serial.print(positions[1]);
    Serial.print("; ");
    Serial.print(i);
    Serial.println(";");
  }}

    // pályapontokról leállás [0,0] -ra.
    if(interruptHandling == 0){    
    positions[0] = 0;
    positions[1] = 0;
    steppers.moveTo(positions);
    steppers.runSpeedToPosition();
    Serial.print(positions[0]);
    //plotteres megjelenítéshez
    Serial.print("; ");
    Serial.print(positions[1]);
    Serial.print("; ");
    Serial.print(i);
    Serial.println(";");
    }

    interruptHandling = 0;
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
//------------------------------------------------------------

void palyaHomlok() {
  digitalWrite(ZOLDLAMPA, HIGH);
  stepper1.setMaxSpeed(para_4);  //Steps / sec
  stepper2.setMaxSpeed(para_4);

  Serial.println("Gorbe kovetes teszt indul HOMLOK!");
  delay(1000);

  digitalWrite(enable_stepper_1, LOW); //Stepper_mozgás_engedélyezés
  digitalWrite(enable_stepper_2, LOW); //Stepper_mozgás_engedélyezés
,,
  digitalWrite(PLAZMASTART, HIGH);
  delay(1000);
    
    long positions[2];

  // [0, 0] -ból pályára állás
    positions[0] = 0;
    positions[1] = 0;
    steppers.moveTo(positions);
    steppers.runSpeedToPosition();
    Serial.print(positions[0]);
    Serial.print("; ");
    Serial.print(positions[1]);
    Serial.print("; ");
    Serial.print(i);
    Serial.println(";");

  for (; i < controllPontok + 1; i++) {
    positions[0] = -tombAdat[i][11] * 4.9;
    positions[1] = -tombAdat[i][12] * 4.9;
    steppers.moveTo(positions);
    steppers.runSpeedToPosition();
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
    steppers.moveTo(positions);
    steppers.runSpeedToPosition();
    Serial.print(positions[0]);
    //plotteres megjelenítéshez
    Serial.print("; ");
    Serial.print(positions[1]);
    Serial.print("; ");
    Serial.print(i);
    Serial.println(";");

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

          Serial.println(para_1);
          Serial.println(para_2);
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

          Serial.println(jogAdat[0]);
          Serial.println(jogAdat[1]);
          Serial.println(jogAdat[2]);
          break;

        case 'X':                               // JOG hossz mentén
          jogX();
          break;

        case 'A':                               // JOG szögben
          jogA();
          break;

        case 'P':                               // vágás palástra
          palyaPalast();
          i = 0;
          k = 0;
          break;

        case 'H':                               // vágás homlokra
          palyaHomlok();
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
