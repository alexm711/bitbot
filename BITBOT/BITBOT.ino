#include <QTRSensors.h>
#include <Timers.h>
#include <Servo.h>

#define TIME_BACKWARDS 500
#define TURN_TIME 250
#define PUSHER_TIME 500
#define RIGHT 1
#define LEFT 0
#define MAX_TAPE_SENSOR_VAL 1023
#define MAX_SPEED 255
#define DELAY 500

/*---------------- Module Level Variables ---------------------------*/
//analog
#define rightSensorInput A0
#define leftSensorInput A4
#define centerSensorInput A2
#define serverSensorInput A5
#define frontTapeSensorInput 15
#define backTapeSensorInput 17

//digital
#define rightWheelToggle 2
#define leftWheelToggle 4
#define rightMotorPWM 3
#define leftMotorPWM 5
#define backRightBumperInput 6
#define backLeftBumperInput 7
#define pusherToggle 8
#define threeCoinDumpOut 9
#define fiveCoinDumpOut 10
#define leftBumperInput 12
#define rightBumperInput 13

//states
int state;
#define FIND_SERVER 0 //going around looking for light
#define GO_TO_SERVER 1 //going straight until server is hit
#define REVERSE 2 //align 
#define FORWARD 4 //reach tape
#define ALIGN_WITH_TAPE 5 //turn forward
#define GET_3_COINS 6
#define GET_5_COINS 7
#define TURNING 8
#define TURN_BACK 9
#define GOING_TO_CENTER 10
#define TURNING_TO_3 11
#define TURNING_TO_5 12
#define GOING_TO_3 13
#define GOING_TO_5 14
#define GOING_TO_8 15
#define GO_BACK 16
#define DUMPING 17
#define REALIGN 18

//variables
int bumpedRightOrLeft; //right or left one is three
int coinsGotten = 0;
int coinsWanted = 0;
int pushes = 0;
boolean hasDumped = false;
int linesSensed = 0;
int curr_tape_sensor_values[2];
char *sequence_of_tape_sensor_changes; //A front on, B front off, C back on, D back off
byte byteRead;
int sideToAlign;

//for collision logic 
int nextLeft;
int nextRight;

Servo threeCoinDump;  // Creates a servo object
Servo fiveCoinDump;  // Creates a servo object

//sensors
QTRSensorsAnalog tapeSensors((unsigned char[]) {frontTapeSensorInput, backTapeSensorInput}, 2);

void setup() { 
  for (int i = 0; i < 250; i++) // make the calibration take about 5 seconds
  {
    tapeSensors.calibrate();
    delay(20);
  }

  
  TMRArd_ClearTimerExpired(0);
  
  state = FIND_SERVER;
  threeCoinDump.attach(threeCoinDumpOut);
  fiveCoinDump.attach(fiveCoinDumpOut);

  Serial.begin(9600);
  Serial.println("Starting MEbot...");
  //pins
  pinMode(rightSensorInput,INPUT);
  pinMode(leftSensorInput, INPUT);
  pinMode(centerSensorInput, INPUT);
  pinMode(serverSensorInput, INPUT);
  pinMode(frontTapeSensorInput, INPUT);
  pinMode(backTapeSensorInput, INPUT);
  pinMode(rightBumperInput, INPUT);
  pinMode(leftBumperInput, INPUT);
  pinMode(pusherToggle, OUTPUT);
  pinMode(rightWheelToggle, OUTPUT);
  pinMode(leftWheelToggle, OUTPUT);
  pinMode(rightMotorPWM, OUTPUT);
  pinMode(leftMotorPWM, OUTPUT);
  pinMode(threeCoinDumpOut, OUTPUT);
  pinMode(fiveCoinDumpOut, OUTPUT);
  pinMode(pusherToggle,OUTPUT);
  
//collision logic
  nextLeft=digitalRead(leftBumperInput);
  nextRight=digitalRead(rightBumperInput);

//testing
  digitalWrite(rightWheelToggle,HIGH);
  digitalWrite(leftWheelToggle,LOW);
  analogWrite(rightMotorPWM,255);
  analogWrite(leftMotorPWM,255);

//  Testparts
  push();
}

void loop() { 

	if (state == FIND_SERVER) { 
		if(serverLightSensed()) {
				goForward();
				state = GO_TO_SERVER;
			} 
	}
	if (state == GO_TO_SERVER) { 
		if (rightBumperHit()) { 
			reverseFromRight();
			state = REVERSE;
		}
		if (leftBumperHit()) {
			reverseFromLeft();
			state = REVERSE;
		}
	}
	if (state == REVERSE) { 
		if (TestTimerExpired()) {
			goForward();
			state = FORWARD;
		}
	}
	if (state == FORWARD) { 
		if (TestTimerExpired()) {
			alignWithTape();
			state = ALIGN_WITH_TAPE;
		}
	}
	if (state == ALIGN_WITH_TAPE) { 
		if (alignedWithTape()) { 
			getThreeCoins();
			state = GET_3_COINS;
		}
	}
	if (state == GET_3_COINS) { 
		if (TestTimerExpired()) { 
			collect();
		}
		if (doneCollecting()) { 
			turn();
			state = TURNING;
		}

	}
	if (state == TURNING) { 
		if (TestTimerExpired()) { 
			getFiveCoins();
			state = GET_5_COINS;
		}
	}
	if (state == GET_5_COINS) { 
		if (TestTimerExpired()) { 
			collect();
		}
		if (doneCollecting()) {
			bumpedRightOrLeft = RIGHT;
			turnBack();
			state = TURN_BACK;
		}

	}
	if (state == TURN_BACK) { 
		if(alignedWithTape()) { 
			goBackwards();
			state = GOING_TO_CENTER;
		}
	}
	if (state == GOING_TO_CENTER) {
		if(missaligned()) { 
			alignWithTape();
			state = REALIGN;
		}
		if(tapeUnseen()) { 
			if (threeIsAvailable()) { 
				turnTo3();
				state = TURNING_TO_3;
			}
			else if (fiveIsAvailable()) {
				turnTo5();
				state = TURNING_TO_5;
			}	
			else if (eightIsAvailable()) { 
				goBackwards();
				state = GOING_TO_8;
			}
		}
	}
	if (state == REALIGN) { 
		if (alignedWithTape()) { 
			goBackwards();
			state = GOING_TO_CENTER;
		}
	}
	if (state == TURNING_TO_3) { 
		if (TestTimerExpired()) {
			goBackwards();
			state = GOING_TO_3;
		}
	}
	if (state == TURNING_TO_5) {
		if (TestTimerExpired()) {
			goBackwards();
			state = GOING_TO_5;
		}
	}
	if (state == GOING_TO_5) {
		if (twoLinesSensed()) { 
			dumpFive();
			state = DUMPING;
		}
	}
	if (state == GOING_TO_3) {
		if (threeLinesSensed()) { 
			dumpThree();
			state = DUMPING;
		}
	}
	if (state == GOING_TO_8) {
		if (oneLineSensed()) {
			dumpEight();
			state = DUMPING;
		}
	}
	if (state == DUMPING) {
		hasDumped = true;
		goForward();
		state = GO_BACK;
	}
	if (state == GO_BACK) {
		if (lineTapeIsSensed()) {
			alignWithTape();
			state = ALIGN_WITH_TAPE;
		}
	}

}

boolean serverLightSensed() { 
	return (digitalRead(serverSensorInput) == HIGH);
}

boolean rightBumperHit() { 
	if (!(digitalRead(rightBumperInput) == nextRight)) {
        nextRight = digitalRead(rightBumperInput);
        return true;
    }
}

void reverseFromRight() {
	TMRArd_InitTimer(0, TIME_BACKWARDS); 
	goBackwards();
	bumpedRightOrLeft = RIGHT;
	sideToAlign = RIGHT;
}

boolean leftBumperHit() {
	if (!(digitalRead(leftBumperInput) == nextLeft)) {
        nextLeft = digitalRead(leftBumperInput);
        return true;
	}
}

void reverseFromLeft() {
	TMRArd_InitTimer(0, TIME_BACKWARDS); 
	goBackwards();
	bumpedRightOrLeft = LEFT;
	sideToAlign = LEFT;
}

void goBackwards() { 
	adjustMotorSpeed(-1 * MAX_SPEED, -1 * MAX_SPEED);
}

void goForward() { 
	adjustMotorSpeed(MAX_SPEED, MAX_SPEED);
}

void alignWithTape() {
	if (sideToAlign == LEFT) { 
		adjustMotorSpeed(MAX_SPEED/2, -1 * MAX_SPEED/2);
	}
	if (sideToAlign == RIGHT) { 
		adjustMotorSpeed(-1 * MAX_SPEED/2, MAX_SPEED/2);
	}
}

boolean alignedWithTape() { 
	updateTapeSensorStatus();
	return (curr_tape_sensor_values[0] == HIGH and curr_tape_sensor_values[1] == HIGH);
}

void updateTapeSensorStatus() {

	unsigned int sensor_values[2];
	tapeSensors.read(sensor_values);
	if (sensor_values[0] > 3/4 * MAX_TAPE_SENSOR_VAL) { 
		curr_tape_sensor_values[0] = HIGH;
		sequence_of_tape_sensor_changes += 'A';
	}
	if (sensor_values[0] < 1/4 * MAX_TAPE_SENSOR_VAL) { 
		curr_tape_sensor_values[0] = LOW;
		sequence_of_tape_sensor_changes = sequence_of_tape_sensor_changes + 'B';
	}
	if (sensor_values[1] > 3/4 * MAX_TAPE_SENSOR_VAL) { 
		curr_tape_sensor_values[1] = HIGH;
		sequence_of_tape_sensor_changes = sequence_of_tape_sensor_changes + 'C';
	}
	if (sensor_values[1] < 1/4 * MAX_TAPE_SENSOR_VAL) { 
		curr_tape_sensor_values[1] = LOW;
		sequence_of_tape_sensor_changes = sequence_of_tape_sensor_changes + 'D';
	}

}

void getThreeCoins() { 
	if (hasDumped) coinsWanted = 11;
	else coinsWanted = 3;
	TMRArd_InitTimer(0, PUSHER_TIME); 
}

void getFiveCoins() { 
	if (hasDumped) coinsWanted = 16;
	else coinsWanted = 8;
	TMRArd_InitTimer(0, PUSHER_TIME); 
}


void pushAlgorithmButton() { 
	push();
	pushes += 1;
	if (pushes == coinsGotten) { 
		coinsGotten +=1;
	}

}

void collect() { 
	if (coinsGotten < coinsWanted) { 
		pushAlgorithmButton();
		TMRArd_InitTimer(0, PUSHER_TIME);
	}
}

boolean doneCollecting() { 
	return coinsGotten == coinsWanted;
}

void turn() {
	adjustMotorSpeed(-1 * MAX_SPEED/2, -1 * MAX_SPEED/2);
	delay(250);
	adjustMotorSpeed(MAX_SPEED/2, -1 * MAX_SPEED);
	delay(125);
	adjustMotorSpeed(MAX_SPEED/2, MAX_SPEED/2);
	delay(250);
	adjustMotorSpeed(-1 * MAX_SPEED/2, MAX_SPEED/2);
	delay(125);
}

void turnBack() { 
	adjustMotorSpeed(-1 * MAX_SPEED/2, -1 * MAX_SPEED/2);
	delay(250);
	adjustMotorSpeed(-1 * MAX_SPEED/2, MAX_SPEED/2);
	delay(125);
	adjustMotorSpeed(MAX_SPEED/2, MAX_SPEED/2);
	delay(250);
	adjustMotorSpeed(MAX_SPEED/2, -1 * MAX_SPEED);
	delay(125);
	sideToAlign = LEFT;

}

boolean missaligned() { 
	updateTapeSensorStatus();
	if (curr_tape_sensor_values[0] == LOW or curr_tape_sensor_values[1] == LOW) { 
		if (sideToAlign == RIGHT) sideToAlign = LEFT;
		else sideToAlign = RIGHT;
		return true;	
	}
}

boolean tapeUnseen() {
	updateTapeSensorStatus();
	return curr_tape_sensor_values[0] == LOW and curr_tape_sensor_values[1] == LOW;
}


boolean threeIsAvailable() {
	int lightInput;
	if (bumpedRightOrLeft == RIGHT) { 
		lightInput = digitalRead(rightSensorInput);
	} else { 
		lightInput = digitalRead(leftSensorInput);
	}
	return (coinsGotten == 8 and lightInput == HIGH);
}

boolean fiveIsAvailable() {
	int lightInput;
	if (bumpedRightOrLeft == RIGHT) { 
		lightInput = digitalRead(leftSensorInput);
	} else { 
		lightInput = digitalRead(rightSensorInput);
	}
	return (coinsGotten == 8 and lightInput == HIGH);
}

boolean eightIsAvailable() {
	int lightInput = digitalRead(centerSensorInput);
	return (coinsGotten == 8 and lightInput == HIGH);
}

void turnTo3() {
	if (bumpedRightOrLeft == LEFT) {
		adjustMotorSpeed(-1 * MAX_SPEED/2, MAX_SPEED/2);
	}
	if (bumpedRightOrLeft == RIGHT) { 
		adjustMotorSpeed(MAX_SPEED/2, -1 * MAX_SPEED/2);
	}
	TMRArd_InitTimer(0, TURN_TIME);
}

void turnTo5() {
	if (bumpedRightOrLeft == RIGHT) {
		adjustMotorSpeed(-1 * MAX_SPEED/2, MAX_SPEED/2);
	}
	if (bumpedRightOrLeft == LEFT) { 
		adjustMotorSpeed(MAX_SPEED/2, -1 * MAX_SPEED/2);
	}
	TMRArd_InitTimer(0, TURN_TIME);
}

//A front on, B front off, C back on, D back off
boolean twoLinesSensed()  {
	updateTapeSensorStatus();
	if (sequence_of_tape_sensor_changes == "ABCDABCD") {
		sequence_of_tape_sensor_changes = "";
		return true;
	}
	return false;
}

void dumpFive() {
	unloadFiveDumpServo();
}

boolean threeLinesSensed() { 
	updateTapeSensorStatus();
	if (sequence_of_tape_sensor_changes == "ABCDABCDABCD") {
		sequence_of_tape_sensor_changes = "";
		return true;
	}
	return false;
}

void dumpThree() {
	unloadThreeDumpServo();
}

boolean oneLineSensed() { 
	updateTapeSensorStatus();
	if (sequence_of_tape_sensor_changes == "ABCD") {
		sequence_of_tape_sensor_changes = "";
		return true;
	}
	return false;
}

void dumpEight() { 
	unloadThreeDumpServo();
	unloadFiveDumpServo();
}

void goBack() { 

}

boolean lineTapeIsSensed() { 
	updateTapeSensorStatus();
	if (sequence_of_tape_sensor_changes == "CDAB") {
		sequence_of_tape_sensor_changes = "";
		return true;
	}
	return false;
}

unsigned char TestTimerExpired(void) {
  unsigned char value = (unsigned char)TMRArd_IsTimerExpired(0);
  if (value == TMRArd_EXPIRED) {
    TMRArd_ClearTimerExpired(0);
  }
  return value;
}
//This function sweeps the servo searching the area for an object within range
void unloadThreeDumpServo() {

	Serial.println("Unloading...");
	for(int pos = 180; pos>=90; pos -= 1)     // goes from 180 degrees to 0 degrees 
	{                                
	  threeCoinDump.write(pos);              // tell servo to go to position in variable 'pos' 
	  delayMicroseconds(DELAY);
    }
	for(int pos = 90; pos < 180; pos += 1)  // goes from 0 degrees to 180 degrees 
	{                                  // in steps of 1 degree 
	  threeCoinDump.write(pos);              // tell servo to go to position in variable 'pos'
	  delayMicroseconds(DELAY);
	}    
}

void unloadFiveDumpServo() {

	Serial.println("Unloading...");
	for(int pos = 180; pos >= 90; pos -= 1)     // goes from 180 degrees to 0 degrees 
	{                                
	  fiveCoinDump.write(pos);              // tell servo to go to position in variable 'pos' 
	  delayMicroseconds(DELAY);
    }
	for(int pos = 90; pos < 180; pos += 1)  // goes from 0 degrees to 180 degrees 
	{                                  // in steps of 1 degree 
	  fiveCoinDump.write(pos);              // tell servo to go to position in variable 'pos'
	  delayMicroseconds(DELAY);
	}    
}

void push() {
  digitalWrite(pusherToggle,HIGH);
  delay(PUSHER_TIME);
  digitalWrite(pusherToggle,LOW);
}

void toggleMotorDirection()
{
  PORTD= PORTD ^ B00010100;
}

void adjustMotorSpeed(int rightSpeed, int leftSpeed)
{
  analogWrite(rightMotorPWM,rightSpeed);
  analogWrite(leftMotorPWM,leftSpeed);
}