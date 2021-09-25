// 'Simon Game' assembled by S.Eames with help from the internet (thanks team :) )

// SETUP DEBUG MESSAGES
// #define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG
	#define DPRINT(...)		Serial.print(__VA_ARGS__)		//DPRINT is a macro, debug print
	#define DPRINTLN(...)	Serial.println(__VA_ARGS__)	//DPRINTLN is a macro, debug print with new line
#else
	#define DPRINT(...)												//now defines a blank line
	#define DPRINTLN(...)											//now defines a blank line
#endif

// Include libraries
#include "FastLED.h"
#include "RF24.h"
#include "ENUMVars.h"
#include <MD_MAX72xx.h>
#include <EEPROM.h>

///////////////////////// IO /////////////////////////
#define NUM_ADDR_PINS 3

#define BEEP_PIN 		5
#define LED_PIN 		6
#define BTN_PIN			10
const uint8_t id_addr_pin[NUM_ADDR_PINS] = {7, 8, 9};	// Pins used to assign ID of station
#define RF_CSN_PIN		18
#define RF_CE_PIN 		19
#define DISP_CS_PIN		20
#define RAND_ANALOG_PIN	21 				// Analog pin -- Leave pin floating -- used to seed random number generator


//////////////////// RF VARIABLES ////////////////////
RF24 radio(RF_CE_PIN, RF_CSN_PIN); 		// CE, CSN

#define MAX_BTNS 1<<NUM_ADDR_PINS
uint8_t addrStartCode[] = "SIMN";		// First 4 bytes of node addresses (5th byte on each is node ID - set later)
uint8_t nodeAddr[MAX_BTNS][5]; 			

#define RF_BUFF_LEN 2						// Number of bytes to transmit / receive
uint8_t radioBuf_RX[RF_BUFF_LEN];
uint8_t radioBuf_TX[RF_BUFF_LEN];
bool newRFData = false;						// True if new data over radio just in

bool master = false;							// master runs the show ;) 
uint8_t myID;									// ID of this station (set in setup() accoding to IO)
uint8_t myID_flag;							// ID in flag form (master = 0b00000001, node1 = 0c00000010, etc)
uint8_t btnsPresent_flag = 0x01; 		// Flags of detected buttons (always at least 1)
uint8_t numBtnsPresent = 1;				// Number of buttons being used

//////////////////// PIXEL SETUP ////////////////////
#define NUM_LEDS 		12

CRGB leds[NUM_LEDS]; 						// Define the array of leds

#define LED_BRIGHTNESS 255

// Colours!
#define COL_RED     	0xFF0000
#define COL_YELLOW  	0xFF8F00
#define COL_GREEN   	0x00FF08
#define COL_BLUE    	0x0000FF
#define COL_PINK		0xFF002F
#define COL_CYAN		0x00AAAA
#define COL_ORANGE	0xFF1C00

#define COL_WHITE   	0xFFFF7F
#define COL_BLACK   	0x000000

const uint32_t btnCols[] = {COL_RED, COL_YELLOW, COL_GREEN, COL_BLUE, COL_PINK, COL_CYAN, COL_ORANGE, COL_WHITE};


bool effectComplete = false;


//////////////// MATRIX DISPLAY SETUP ////////////////
const MD_MAX72XX::moduleType_t HARDWARE_TYPE = MD_MAX72XX::FC16_HW;
#define DISP_NUM_PANELS	2

MD_MAX72XX disp = MD_MAX72XX(HARDWARE_TYPE, DISP_CS_PIN, DISP_NUM_PANELS);

#define DISP_INTENSITY 2 					// Brightness of display on range 0-14

bool scrollTextComplete = false;			// Set to '1' once text completed a full cycle

///////////////////////// AUDIO /////////////////////////
								 	//	E1,	G1,	Ab1,	A1,	Bb1,	B1,	C1,	D1,	Eb2,	E2,	F2,	F#2,	G2,	A2,	C2,	C0,	D2,	E3,	G3
const uint16_t notes[] = {0, 330,	392,	415,	440,	466,	494,	262,	294,	622,	659,	698,	740,	784,	880,	523,	131,	587,	1319,	1568, 1480, 1397, 1245, 1047, 831};
								//0, 1,		2, 	3, 	4, 	5, 	6, 	7, 	8, 	9, 	10,	11, 	12,	13,	14,	15,	16,	17,	18,	19
const uint8_t lose_song[] = {15, 0, 15, 0, 15};					
const uint8_t win_song[] = {10,13,18,15,17,19};
const uint8_t highscore_song[] = {18,0,18,0,0,0,18,0,0,0,15,0,18,0,0,0,19,0,0,0,0,0,0,13,0,0,0,0,0,0,0,
											0,0,0,0,19,0,20,0,21,0,22,0,0,0,18,0,0,0,24,0,14,0,15,0,0,0,14,0,15,0,17,0,
											0,0,0,0,19,0,20,0,21,0,22,0,0,0,18,0,0,0,23,0,0,0,23,0,23};
uint8_t currentNote = 0;

#define SONG_TEMPO 			100 			// (ms) time spent on each note
#define BEEP_TIME				100 			// (ms) period beeper sounds for
#define BEEP_NOTE 			880			// (hz) frequency speaker makes when button is pressed

//////////////////// MISC VARIABLES ////////////////////

uint32_t beep_starttime = 0;


///// TIMING

#define DEBOUNCE 				100			// (ms) Button debounce time
#define LED_REFRESH 			100 			// (ms) Refresh rate of LED patterns
#define LED_EFFECT_TIME		150			// (ms) Interval of LED effects
#define LED_EFFECT_LOOP		6 				// Multiplier of LED effect time
#define TICKER_TIME			80				// (ms) Update interval of ticker text
#define SCORE_DISPLAY		3000			// (ms) Time score is displayed for after a round
#define RF_POLL_FREQ			5000			// (ms) Period on which nodes are polled 
#define RF_REPLY_DELAY		10 			// (ms) Delay between slave nodes responding to group broadcast

// StepTime (ms) is the on & off time intervals of steps played back in a sequence. 
// SEQ_STEP_PER_BLOCK indicates when speed increases - i.e. after level = SEQ_STEP_PER_BLOCK, seq_StepTime[1] is used for interval
// For levels higher than sizeof(seq_StepTime) * SEQ_STEP_PER_BLOCK, seq_StepTime[sizeof(seq_StepTime) -1] is used
// When in ST_SeqRec, 2*seq_StepTime is the allowed space for players to press button
uint16_t seq_StepTime[] = {600, 550, 500, 450, 400, 350, 300, 250, 200};
uint8_t seq_StepTimeStage = 0;
#define SEQ_STEP_PER_BLOCK	4

#define SEQ_INPUTTIME_MULT	3 		// SEQ_INPUTTIME_MULT * seq_StepTime[x] = max allowed time to press button


///// GAME PATTERN
#define SEQ_MAX_LEN			255			// Maximum sequence length (maximum value uint8_t can store)
uint8_t sequence[SEQ_MAX_LEN];			// Holds sequence used for game
uint8_t seq_level = 0;						// Current level achieved of sequence - increments with each success
uint8_t seq_RecPlayStep = 0;				// Current step being either played back or recorded
uint8_t highscore = 0;						// Holds highest achieved level (since power on);
uint8_t seq_LightOn = 0;					// (bool) holds whether in on or off state of playing sequence
uint8_t lastBtnPressed = numBtnsPresent;

#define HS_EEPROM_LOC		2 				// Memory location in EEPROM to use for savinf high scores

//// GAME STATE
gameStates currentState = ST_Lobby;
gameStates lastState = ST_Lobby;


void setup() 
{
	// Initialise Display
	disp.begin();
	disp.control(2, DISP_INTENSITY);
	disp.clear();

	// Initialise Inputs
	pinMode(BTN_PIN, INPUT_PULLUP);
	pinMode(RAND_ANALOG_PIN, INPUT);
	// btnState_last = digitalRead(BTN_PIN);

	// Initialise beeper
	pinMode(BEEP_PIN, OUTPUT);
	digitalWrite(BEEP_PIN, LOW);

	// Initialise LEDs
	FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
	fill_solid( leds, NUM_LEDS, COL_BLACK);
	FastLED.setBrightness(LED_BRIGHTNESS);
	FastLED.show();


	// Initialise address ID inputs & calculate my ID
	for (uint8_t i = 0; i < NUM_ADDR_PINS; ++i)
	{
		pinMode(id_addr_pin[i], INPUT_PULLUP);

		myID = myID << 1;	// Shift left 1 digit
		myID += !digitalRead(id_addr_pin[i]);
	}

	myID_flag = 1 << myID;



	if (myID == 0)
		master = true;
	else
		master = false;

	// Initialise Radio

	// Generate node addresses
	for (uint8_t i = 0; i < MAX_BTNS; ++i)
	{
		for (uint8_t j = 0; j < 4; ++j)	// Set first four bytes of address to same code
			nodeAddr[i][j] = addrStartCode[j];

		nodeAddr[i][4] = i;					// Unique 5th byte according to node address
	}

	radio.begin();  
	radio.setAutoAck(1);						// Ensure autoACK is enabled
	radio.enableAckPayload();				// Allow optional ack payloads
	radio.setRetries(0,15);					// Smallest time between retries, (max no. of retries is 15)
 	radio.setPayloadSize(RF_BUFF_LEN);	// Here we are sending 1-byte payloads to test the call-response speed
 
	radio.setChannel(111);					// Keep out of way of common wifi frequencies
	radio.setPALevel(RF24_PA_HIGH);		// Let's make this powerful... later
	radio.setDataRate( RF24_2MBPS );		// Let's make this quick

	// Opening Listening pipe
	radio.openReadingPipe(1, nodeAddr[myID]);

	// Setup for slave RF nodes --> only need to listen/write to master
	if (!master)
	{
		radio.openWritingPipe(nodeAddr[0]);
		radioBuf_TX[0] = myID_flag;				// Always respond with myID to master
		radio.startListening();
	}
	else
	{
		highscore = EEPROM.read(HS_EEPROM_LOC);// Get saved highscore if master
	} 													

	radio.writeAckPayload(1, radioBuf_TX, RF_BUFF_LEN); 	// Setup AckPayload
	
	// Initialise Serial debug
	#ifdef DEBUG
		Serial.begin(115200);						// Open comms line
		while (!Serial) ; 							// Wait for serial port to be available

		DPRINT(F("myID = "));
		DPRINT(myID);

		DPRINT(F(",\tmyID_flag = "));
		DPRINTLN(myID_flag, BIN);

		if (master)
		{
			DPRINTLN(F("THE MASTER HAS ARRIVED!"));
			DPRINT(F("Saved highscore = "));
			DPRINTLN(highscore);
		}
	#endif

	// Let the games begin!
}

void loop() 
{
	static uint32_t lasttime;						// Used for timining intervals
	static bool messageSent = false;

	updateBeepState();

	if (millis() < lasttime) 						// Timer wrapped -- reset it
		lasttime = millis();

	if (master)											// (Master) Transmit updates on state change
	{
		if (lastState != currentState)
		{
			updateSlaves();

			DPRINT(F("Transmitted State = "));
			DPRINTLN(currentState);
		}
	}
	else 													// (Slave) Check for new RF messages
		slaveCheckRF();

	switch (currentState) 
	{
		case ST_Lobby: 								// Waiting for someone to initiate a game by pressing any button
			updateLEDs();
			
			if (lastState != ST_Lobby)				// Clears any residual text in scrollText buffer
			{
				scrollText(" ");
				btnsPresent_flag = 1;				// Reset list of connected buttons when entering Lobby state
				lastState = currentState;
			}

			if (numBtnsPresent >= 4) 				// Game can't start until 4 or more btns present
				scrollText("PRESS TO PLAY ");
			else
				scrollText("-");


			if (master)
			{
				checkNumBtnsPresent();
				if (numBtnsPresent >= 4)			// Don't start new game until 4 or more buttons connected
					checkStartNewGame();				// Start new game if any button pressed
			}
			else
				checkButtons();						// This transmits to master if button pressed
			
			break;


		case ST_Intro:
			if (lastState != currentState)
			{
				lastState = currentState;
			}
			disp.clear();
			updateLEDs();
			// Refer to LED function for this part
			break;


		case ST_SeqPlay:
			// RESET VARIABLES
			// Generate new sequence if we're starting a new game
			if ((lastState != ST_Correct) && (lastState != currentState))			
			{
				generateSequence();
				seq_StepTimeStage = 0;
			}
			
			if (lastState != currentState)		
			{
				lasttime = millis(); 				// Initiate timer
				BlackMyLEDs();
				seq_RecPlayStep = 0;
				lastState = currentState;
			}

			// Timing
			if ((lasttime + seq_StepTime[seq_StepTimeStage]) > millis())		
				break; 
			
			lasttime = millis();

			// Show SeqStep
			if (seq_LightOn)							// Currently on --> turn off
			{
				BlackMyLEDs();
				updateSlaveLeds(numBtnsPresent, 0x00);	// All off
				seq_LightOn = !seq_LightOn;
			}
			else 											// Currently off --> turn on
			{
				LightButton(sequence[seq_RecPlayStep++]);
				seq_LightOn = !seq_LightOn;

				// Calculate StepTimeStage
				seq_StepTimeStage = floor(seq_RecPlayStep/SEQ_STEP_PER_BLOCK);
				if (seq_StepTimeStage >= sizeof(seq_StepTime) / sizeof(seq_StepTime[0]))		// NOTE: sizeof returns number of bytes, so divide by number of bytes per variable
					seq_StepTimeStage = (sizeof(seq_StepTime) / sizeof(seq_StepTime[0])) -1;	// -1 because index starts at 0
			}


			if (seq_RecPlayStep >= SEQ_MAX_LEN)	// Winner winner chicken dinner!
			{
				seq_RecPlayStep = 0;
				// TODO: handle this case better - all levels achieved!
			}


			// GOTO Record state if we've finished playing sequence
			if (!seq_LightOn && (seq_RecPlayStep > seq_level))
			{
				currentState = ST_PreRec;
				seq_level++;

				BlackMyLEDs();
				updateSlaveLeds(numBtnsPresent, 0x00);	// All off
			}
			
			break;


		case ST_SeqPlay_Slave:
			disp.clear();
			seq_LightOn = false; 				// Used in next state
			break;


		case ST_PreRec:
			// PHASE 1 - Stay black for a bit --> update state, stay black until timer elapses

			if (lastState != currentState)
			{
				lastState = currentState;
				lasttime = millis();
				disp.clear();
			}		
			
			// Timing
			if ((lasttime + seq_StepTime[seq_StepTimeStage]) > millis())		
				break; 

			lasttime = millis();

			// PHASE 2 - All white LEDs
			if (!seq_LightOn) 
			{
				fill_solid( leds, NUM_LEDS, COL_WHITE);	 // Light all buttons white to indicate start of record sequence
				FastLED.show();
				seq_LightOn = !seq_LightOn;					// Confused yet? - I am
			}	
			// PHASE 3 - 'GO' and change to next state
			else
			{
				BlackMyLEDs();					// Start rec with all black again
				staticText("GO!");

				if (master)
				{
					currentState = ST_SeqRec;
					seq_RecPlayStep = 0;				// Reset step variable when starting record sequence
				}
			}

			break;


		case ST_SeqRec:
			if (lastState != currentState)	// Update state
				lastState = currentState;

			// Record player inputs!
			if ((lasttime + (seq_StepTime[seq_StepTimeStage] * SEQ_INPUTTIME_MULT)) > millis())	
			{
				// Turn off previously lit buttons
				if (((lasttime + seq_StepTime[seq_StepTimeStage]) < millis()) && !messageSent)
				{
					// GRRRRR - this line is not my friend right now
					BlackMyLEDs();
					updateSlaveLeds(numBtnsPresent, 0x00);	// All off
					messageSent = true;		
				}

				lastBtnPressed = checkButtons();			// check for input
				if(lastBtnPressed != numBtnsPresent)	// If any button pressed...
				{
					LightButton(lastBtnPressed);			// Light up button colour that was pressed
					lasttime = millis();						// reset timer on button press
					messageSent = false;
				
					// Check correct button was pressed
					if (lastBtnPressed == sequence[seq_RecPlayStep])
					{
						DPRINT(F("CORRECT - Step Number = "));
						DPRINTLN(seq_RecPlayStep);

						if (seq_RecPlayStep >= seq_level-1)	// Go to next state if finished recording sequence
							currentState = ST_Correct;
						else
							seq_RecPlayStep++;							
					}
					else
					{
						// Incorrect
						DPRINT(F("INCORRECT - Step Number = "));
						DPRINT(seq_RecPlayStep);
						DPRINT(F(", Expected input = "));
						DPRINT(sequence[seq_RecPlayStep]);
						DPRINT(F(", Button Pressed = "));
						DPRINTLN(lastBtnPressed);

						currentState = ST_Incorrect;
					}
				}
			}
			else 										// Failed to press button in allowed time frame --> end game
			{
				DPRINTLN(F("Input timeout"));

				lasttime = millis();
				currentState = ST_Incorrect;
			}

			break;


		case ST_SeqRec_Slave:
			checkButtons();
			break;


		case ST_Correct:
			disp.clear();
			// PHASE 1
			if (lastState != ST_Correct)				// Keep buttons lit from last State, then play animation
			{
				lastState = currentState;
				lasttime = millis();
			}

			// Timing
			if ((lasttime + LED_EFFECT_TIME) > millis())		
				break; 
		
			lasttime = millis();


			if (seq_LightOn) 
			{
				BlackMyLEDs();								// Start with all black again
				seq_LightOn = !seq_LightOn;
			}
			else 												// NOTE: this is run first
				seq_LightOn = !seq_LightOn;		

			if (effectComplete)
			{
				if (master)									// Master commands state changes
					currentState = ST_SeqPlay;
			}

			updateLEDs();									// Play success animation
			break;


		case ST_Incorrect:
			disp.clear();

			if (lastState != currentState)
			{
				lastState = currentState;
				effectComplete = false;
			}

			// Play fail animation
			if (effectComplete)
				BlackMyLEDs();
			else
			{
				updateLEDs();						// Fail LED sequence
				break;
			}

			if (master)
			{
				// Check for high score
				if (seq_level == 0)					// Handle case where 0 levels achieved
					seq_level++;

				if (--seq_level > highscore)		// check for high score
				{
					currentState = ST_HighScore;
					highscore = seq_level;
					seq_level = 0;						// Reset seq level after using

					// Save new high score to EEPROM
					EEPROM.write(HS_EEPROM_LOC, highscore);

					// Check it saved properly
					#ifdef DEBUG
						if (EEPROM.read(HS_EEPROM_LOC) == highscore)
							DPRINTLN("Highscore saved successfully!");
						else
							DPRINTLN("Highscore didn't save");
					#endif
				}
				else 										// Note: slaves are commanded by master to change state
					currentState = ST_ShowScore;		
			}

			break;		


		case ST_HighScore:
			// Scroll 'high score' text, then show number
			if (lastState != currentState)
			{
				lastState = currentState;
				scrollTextComplete = false;		// Reset on first run
				seq_LightOn = false; 				// Used in next state
				scrollText(" ");						// Flush scrollText of previous text
			}

			checkStartNewGame();						// Start new game if any button pressed
			updateLEDs();

			// PHASE 1 - Show scrolling 'high score' text for a bit
			if (!scrollTextComplete)
				scrollText("HIGH SCORE   ");
			else
			{
				if ((lasttime + SCORE_DISPLAY) > millis())		// Timing - not ready to continue yet
					break; 
				
				lasttime = millis(); 				// Initiate timer
				
				// PHASE 2 - Show Score
				if (!effectComplete)
				{
					dispNumber(highscore);
					effectComplete = true;
				}
				// PHASE 3 - Goto next state
				else if (master)
					currentState = ST_Lobby;
			}

			break;


		case ST_ShowScore:
			if (lastState != currentState)
			{
				dispNumber(seq_level);
				seq_level = 0;							// Reset seq level after using
				seq_LightOn = false; 				// Used in next state
				lasttime = millis();
				lastState = currentState;
				BlackMyLEDs();
			}

			checkStartNewGame();
			
			if ((lasttime + SCORE_DISPLAY) > millis())		// Delay a bit
				break; 

			if (master)
				currentState = ST_Lobby;

			break;


		default:
			// statements
			break;
	}


}

void checkNumBtnsPresent() // Note: Only called by Master
{
	// Broadcasts message asking which buttons are around
	// Then records present buttons

	static uint32_t lasttime = 0; 						// I want it to run the loop TX part of the code first
	uint8_t temp = 0;	

	// Timing
	if (millis() < lasttime) 								// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + RF_POLL_FREQ) > millis() && lasttime != 0)		
		return;

	else 															// Check for radio responses
	{
		DPRINTLN(F("Polling Nodes"));
		radio.stopListening();

		radioBuf_TX[0] = ST_TheVoid;						// Blank message - just used to scan present devices

		for (uint8_t i = 1; i < MAX_BTNS; ++i)			// Poll each possible slave node & record those present
		{
			radio.openWritingPipe(nodeAddr[i]);
			radio.write(radioBuf_TX, RF_BUFF_LEN, 0); // Transmit message & wait for ack (blocking function)
		
		
			if (radio.available())
			{
				radio.read(radioBuf_RX, RF_BUFF_LEN);	// Get received message
				btnsPresent_flag |= radioBuf_RX[0];		// Update current list of devices

				// Tally up number of present buttons
				temp = btnsPresent_flag;
				numBtnsPresent = 0;

				while (temp) 
				{
					numBtnsPresent += temp & 1;
					temp >>= 1;
				}

				DPRINT(F("Received ack from: "));
				DPRINTLN(btnFlag2Num(radioBuf_RX[0]));
			}
			else
			{
				DPRINT(F("No response from: "));
				DPRINTLN(i);
			}

			DPRINT(F("numBtnsPresent = "));
			DPRINT(numBtnsPresent);
			DPRINT(F(", btnsPresent_flag = "));
			DPRINTLN(btnsPresent_flag, BIN);
		}

		updateSlaves();
		radio.startListening();
		lasttime = millis();						// Reset polling timer
	}

	return;
}


void checkStartNewGame()
{
	// Initiates new game if any button is pressed
	if(checkButtons() != numBtnsPresent)
	{
		if (master)
			currentState = ST_Intro;
	}

	return;
}


void generateSequence()
{
	// Seed random number generator
	randomSeed(analogRead(RAND_ANALOG_PIN));

	DPRINT(F("Sequence ="));

	// fill sequence with random numbers according to number of buttons used in game
	for (uint8_t i = 0; i < SEQ_MAX_LEN; ++i)
	{
		sequence[i] = random(0, MAX_BTNS);

		// drop value if it isn't a present button
		while (!((1U << sequence[i]) & btnsPresent_flag ))
			sequence[i] = random(0, MAX_BTNS);

		DPRINT(F(" "));
		DPRINT(sequence[i]);
	}

	DPRINTLN();

	return;
}


uint8_t btnFlag2Num(uint8_t flags)
{
	// Returns position of first set flag in number
	for (uint8_t i = 0; i < MAX_BTNS; ++i)
	{
		if ((1U << i ) & flags)
			return i;
	}

	return MAX_BTNS;
}


uint8_t btnFlag2QTY(uint8_t flags)
{
	// Returns number of set flags 
	uint8_t temp = 0;

	for (uint8_t i = 0; i < MAX_BTNS; ++i)
	{
		if ((1U << i ) & flags)
			temp++;
	}

	return temp;
}


uint8_t checkButtons()
{
	// Checks if any button has been pressed
	// Returns ID number of any button just pressed, else returns NUM_BTNS

	static uint32_t lasttime;
	static bool btnState_now;
	static bool btnState_last = digitalRead(BTN_PIN);

	// Check for button press input from radios if master
	if (master)
	{
		while (radio.available())
		{
			radio.read(radioBuf_RX, RF_BUFF_LEN);	// Get received message

			if (radioBuf_RX[1] == 0xFF)			// Return number if it was a button press
			{
				DPRINT(F("Button pressed received from = "));
				DPRINTLN(btnFlag2Num(radioBuf_RX[0]));
				return btnFlag2Num(radioBuf_RX[0]);
			}
			else
			{
				DPRINT(F("BLANK Button pressed received from = "));
				DPRINTLN(btnFlag2Num(radioBuf_RX[0]));
			}
		}
	}

	// Debounce buttons
	if (millis() < lasttime) 						// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + DEBOUNCE) > millis())		// Debounce timer hasn't elapsed
		return numBtnsPresent; 
	
	// Record button state
	btnState_now = digitalRead(BTN_PIN);		// Read input

	if (btnState_now != btnState_last) 			// If button state changed 
	{
		btnState_last = btnState_now;				// Record button state

		if (!btnState_now)							// If button is pressed
		{
			if (currentState != ST_SeqRec_Slave) 	// 'Beep' is triggered by master for this case
				beepNow();

			lasttime = millis();						// Debouncing complete; record new time & continue
			
			if (master)
			{
				return myID;							// Return number of button that was just pressed
			}
			else 											// if slave node, transmit button press to master
			{
				radioBuf_TX[1] = 0xFF;				// Signal button press
				radio.stopListening();
				if (radio.write(radioBuf_TX, RF_BUFF_LEN, 0)) // Transmit message & wait for ack (blocking function)
					DPRINTLN(F("Message Sent"));
				else
				{
					DPRINTLN(F("FAILED TO SEND"));
					if (radio.write(radioBuf_TX, RF_BUFF_LEN, 0))
						DPRINTLN(F("Message Sent 2nd time"));
					else
						DPRINTLN(F("FAILED TO SEND AGAIN"));
				}
				radio.startListening();
				radioBuf_TX[1] = 0x00;				// Clear button press
			}				
		}
	}

	return numBtnsPresent;							// Return NUM_BTNS if no buttons pressed
}


void updateSlaves()
{
	// Updates state of detected slave buttons

	radioBuf_TX[0] = currentState;				// Send currentState of game in first byte

	switch (currentState) 							// Send other data according to state
	{
		case ST_Lobby:
			radioBuf_TX[1] = btnsPresent_flag;
			break;

		case ST_PreRec:
			radioBuf_TX[1] = seq_StepTimeStage;
			break;

		case ST_HighScore:
			radioBuf_TX[1] = highscore;
			break;

		case ST_ShowScore:
			radioBuf_TX[1] = seq_level;
			break;

		default:
			radioBuf_TX[1] = 0x00;
			break;
	}

	DPRINT(F("Updated slaves with state - "));
	DPRINT(currentState);
	DPRINT(F(" and data "));
	DPRINTLN(radioBuf_TX[1]);
	
	radioTX2Slaves(numBtnsPresent);				// Transmit the things to present buttons

	return;
}


void radioTX2Slaves(uint8_t ID)
{
	// Transmits to either all slaves or given ID of slave
	radio.stopListening();

	if (ID >= numBtnsPresent)										// Transmit to all slaves!
	{
		for (uint8_t i = 1; i < MAX_BTNS; ++i)					// Note: '0' is master (me) so skip that one
		{
			// Skip absent buttons
			if ((1U << i) & btnsPresent_flag)
			{
				radio.openWritingPipe(nodeAddr[i]);
				radio.write(radioBuf_TX, RF_BUFF_LEN, 0); 	// Transmit message & wait for ack (blocking function)
				
				while (radio.available())
					radio.read(radioBuf_RX, RF_BUFF_LEN); 		// flush ack responses
			}
		}
	}
	else 																	// Only signal one slave
	{
		radio.openWritingPipe(nodeAddr[ID]);
		radio.write(radioBuf_TX, RF_BUFF_LEN, 0); 
		while (radio.available())
			radio.read(radioBuf_RX, RF_BUFF_LEN); 				// flush ack response
	}

	radio.startListening();

	return;
}

void slaveCheckRF()
{
	// Checks for updates from master & updates state accordingly
	bool newMessage = false;

	while (radio.available())
	{
		// Read in message
		radio.read(radioBuf_RX, RF_BUFF_LEN);
		radio.writeAckPayload(1, radioBuf_TX, RF_BUFF_LEN ); 	//Note: need to re-write ack payload after each use
		newMessage = true;
	}

	if (newMessage)
	{
		// Update my state based off received message
		switch (radioBuf_RX[0])
		{
			case ST_TheVoid: 	// Do nothing - used to scan present devices
				DPRINT(F("Void State - "));
				break;

			case ST_Lobby:
				DPRINT(F("Lobby State - "));
				currentState = ST_Lobby;

				// Record present devices to updated LEDs with
				btnsPresent_flag = radioBuf_RX[1];
				numBtnsPresent = (btnFlag2QTY(radioBuf_RX[1]));
				break;

			case ST_SeqPlay:
				DPRINT(F("PLAY! - "));
				currentState = ST_SeqPlay_Slave;
				if (1U & (radioBuf_RX[1] >> myID))		// Light up our LED if we got signalled to, otherwise blackout
					LightButton(myID);
				else
					BlackMyLEDs();
				break;

			case ST_PreRec:
				DPRINT(F("PreRec - "));
				currentState = ST_PreRec;
				seq_StepTimeStage = radioBuf_RX[1];
				break;

			case ST_SeqRec:
				DPRINT(F("REC! - "));
				currentState = ST_SeqRec_Slave;
				if (1U & (radioBuf_RX[1] >> myID))		// Light up our LED if we got signalled to, otherwise blackout
				{
					LightButton(myID);
					beepNow();
				}
				else
					BlackMyLEDs();
				break;

			case ST_HighScore:
				DPRINT(F("Highscore State - "));
				currentState = ST_HighScore;
				highscore = radioBuf_RX[1];				// High score value from master
				break;

			case ST_ShowScore:
				DPRINT(F("Show score state - "));
				currentState = ST_ShowScore;
				seq_level = radioBuf_RX[1];
				break;

			default:
				DPRINT(F("Default Case = "));
				DPRINT(radioBuf_RX[0]);
				DPRINT(F(" - "));
				currentState = radioBuf_RX[0];
				break;
		
		}

		DPRINTLN(radioBuf_RX[1], BIN);
	}
	return;
}



void updateLEDs()
{
	static uint32_t lasttime;
	static uint8_t currentState_last;
	static uint8_t effect_step = 0;

	// Variables for HighScore effect
	static uint8_t hue = 0;

	// Variables for Lobby effect
	static uint8_t step = 0;
	const uint8_t width = floor(NUM_LEDS / numBtnsPresent);
	uint8_t pixelNum = 0;												// Current pixel being updated

	if (millis() < lasttime) 											// Millis() wrapped around - restart timer
		lasttime = millis();

	// Reset variables
	if (currentState != currentState_last)
	{
		effect_step = 0;
		effectComplete = false;
		currentState_last = currentState;
	}


	// Does patterns on LEDs according to state
	switch (currentState) 
	{
		case ST_Lobby: // Waiting for someone to initiate a game by pressing any button

			effect_step = 0;												// Reset in prep for next stage

			if ((lasttime + LED_REFRESH) > millis())
				return; 
			

			if (numBtnsPresent <= 1)										// Light the colour of this station if only us here
				fill_solid(leds, NUM_LEDS, btnCols[myID]); 			
			else
			{
				fill_solid(leds, NUM_LEDS, COL_BLACK); 				// Fill in any unused gaps with black
				for (uint8_t i = 0; i < MAX_BTNS; ++i)					// Step through btnsPresent_flags
				{
					if ((btnsPresent_flag >> i) & 1U)					// If flag is set (present)
					{
						for (uint8_t j = 0; j < width; ++j)				// Light LEDs for present flags
						{
							if ( (step + pixelNum) >= NUM_LEDS)			// Loop back to start of pixel chain if we're over
								leds[step + pixelNum++ - NUM_LEDS] = btnCols[i];
							else
								leds[step + pixelNum++] = btnCols[i];	
						}
					}
				}
			}

			// Update step count
			if (++step >= NUM_LEDS)
				step = 0;

			break;


		case ST_Intro:
			if ((lasttime + LED_EFFECT_TIME) > millis())
				return; 
			
			if (effect_step++ % 2)
				fill_solid(leds, NUM_LEDS, COL_WHITE);
			else
				fill_solid(leds, NUM_LEDS, COL_BLACK);


			if (effect_step > LED_EFFECT_LOOP)
			{
				effect_step = 0;
				lastState = currentState;

				if (master)
					currentState = ST_SeqPlay;
				else
					currentState = ST_SeqPlay_Slave;
				disp.clear();
			}
		
			break;


		case ST_Correct:
			if ((lasttime + LED_EFFECT_TIME) > millis()) // Hold previous state of LEDs for a bit
				return; 

			effect_2Step(effect_step++, COL_GREEN, COL_WHITE);

			if (effect_step > LED_EFFECT_LOOP)
			{
				if (master)
					effectComplete = true;
			}
			break;


		case ST_Incorrect:
			if ((lasttime + LED_EFFECT_TIME) > millis())
				return; 

			effect_2Step(effect_step++, COL_BLACK, COL_RED);

			if (effect_step > LED_EFFECT_LOOP)
			{
				if (master)
					effectComplete = true;
			}
			break;


		case ST_HighScore:
			if ((lasttime + LED_EFFECT_TIME/2) > millis())
				return; 

			fill_solid( leds, NUM_LEDS, COL_BLACK);

			if (effect_step >= NUM_LEDS)
				effect_step = 0;

			leds[effect_step++] = CHSV(hue, 255, 255);
			hue += 255/NUM_LEDS;

			break;


		default:
			break;
	}

	lasttime = millis();

	FastLED.show(); 
}


void effect_2Step(uint8_t step, uint32_t col_1, uint32_t col_2)
{
	// Flashes alternate colours on LEDs
	fill_solid( leds, NUM_LEDS, col_1);

	for (uint8_t i = step % 2; i < NUM_LEDS; i += 2)
		leds[i] = col_2;

	return;
}


void LightButton(uint8_t button)
{
	// Lights up according to given button number
	if (button == myID)
	{
		fill_solid( leds, NUM_LEDS, btnCols[button]);
		FastLED.show();
	}
	else if (master)
		updateSlaveLeds(button, 1U << button);

	return;
}


void BlackMyLEDs()
{
	// Turns off my lights
	fill_solid( leds, NUM_LEDS, COL_BLACK);
	FastLED.show();

	return;
}

void updateSlaveLeds(uint8_t ID, uint8_t flags) // Only called by master
{
	// Turns button LEDs on/off

	// Load data to transmit
	radioBuf_TX[0] = currentState;
	radioBuf_TX[1] = flags;

	radioTX2Slaves(ID);
	
	return;
}


void scrollText(const uint8_t *p)
{
	// Scrolling text
	// Displays the next frame of given pointer each time it's called

	static uint8_t *last_p = p;
	static uint8_t cBuf[8];  						// Column buffer - this width should be ok for all built-in fonts
	static uint8_t charCol = 0;
	static uint8_t letterNum = 0;
	static uint8_t charWidth = 0;
	static uint32_t lasttime = millis();

	if (millis() < lasttime) 						// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + TICKER_TIME) > millis())		// Return --> it's not time yet
		return; 

	lasttime = millis();							// It's time! Record new start time & continue


	// Reset the things if new string to display
	if ((last_p != p) || (*p != *last_p)) // compares ptr and 1st character change... I think - I'm not great with pointers :( 
	{
		// NOTE: This assumes either different ptr value for each string, OR different start charater
		// That will cover most cases anyway
		// New string! (probably?) - Reset the things!
		last_p = p;
		letterNum = 0;
		charCol = 0;
		scrollTextComplete = false;
		disp.clear();		
	}

	if (charCol == 0)
	{
		// Loop back to start of string
		if (*(p + letterNum) == '\0')
		{
			letterNum = 0;
			scrollTextComplete = true;
		}

		// For some reason 'spaces' aren't displaying properly - handle this case
		if (*(p + letterNum) == ' ')
		{
			for (uint8_t i = 0; i < sizeof(cBuf); ++i)
				cBuf[i] = 0x00;
		}
		else // Load new char
			charWidth = disp.getChar(*(p + letterNum), sizeof(cBuf) / sizeof(cBuf[0]), cBuf); 

		letterNum++;
	}

	disp.transform(MD_MAX72XX::TSL);						// Shift display left one column
	disp.setColumn(0, cBuf[charCol++]);					// Display one column of current character

	if (charCol > charWidth)
		charCol = 0;

	return;
}


void staticText(const char *p)
{
	// Prints given text in the center of the display

	uint8_t cBuf[(DISP_NUM_PANELS+1) * 8];		// Screen buffer
	uint8_t width_full = 0;							// Cumulative width of text

	// Work out width of text while loading characters into buffer
	// While still in string && buffer has space
	while ( (*p != '\0') && (width_full <= (sizeof(cBuf) / sizeof(cBuf[0]))) )
	{
		width_full += disp.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf + width_full); 
		cBuf[width_full++] = 0x00; // add 1 col space between characters
	}

	// Clear screen
	disp.clear();

	// Display buffer on screen
	for (uint8_t i = 0; i < width_full; ++i)
		disp.setColumn((DISP_NUM_PANELS*4 + width_full/2) - i - 1, cBuf[i]); // -1 to shift it to right one col to be more centered

	return;
}


void solidDisplay()
{
	// Turns on every pixel on display
	for (uint8_t i = 0; i < DISP_NUM_PANELS*8; ++i)
		disp.setColumn(i, 0xFF);

	return;
}


void dispNumber(uint8_t Number)
{
	// Prints given number on display
	// Doesn't print numbers >= 4 digits

	uint8_t charBuf[4]; // Buffer to store text digits into
	const uint8_t numOffset = 48; // Value of '0' in ascii table

	// Fill char buff with null characters (so staticText() knows where end of number is)
	for (uint8_t i = 0; i < sizeof(charBuf) / sizeof(charBuf[0]); ++i)
		charBuf[i] = '\0';

	if (Number < 10)								// 1 Digit numbers
		charBuf[0] = Number + numOffset;
	else if (Number < 100)						// 2 Digit numbers
	{
		charBuf[0] = floor(Number / 10) + numOffset;
		charBuf[1] = Number % 10 + numOffset;
	}
	else if (Number < 1000)						// 3 Digit numbers!
	{
		charBuf[0] = floor(Number / 100) + numOffset;
		charBuf[1] = floor((Number % 100) / 10) + numOffset;
		charBuf[2] = Number % 10 + numOffset;
	}

	staticText(charBuf);							// Print to screen!

	return;
}


void updateBeepState()
{
	// Turns beeper off / on according to whether timer has elapsed && plays music for correct/incorrect/highscore

	static uint32_t note_starttime;
	static uint8_t currentState_last = currentState;
	static bool beeping = false;
	uint8_t tempoDivide = 1;



	// BEEP for button presses
	if (millis() < beep_starttime) 			// Timer wrapped -- reset (From memory this state takes about 3 days to get to)
		beep_starttime = millis();

	// Piezo buzzers I got make annoying noise when 'not on'
	// Setting port to input stops this annoying noise
	if ((beep_starttime + BEEP_TIME) > millis())		// Beep
	{
		beeping = true;
		pinMode(BEEP_PIN, OUTPUT);
		tone(BEEP_PIN, BEEP_NOTE);		
	}	
	else 															// !Beep	
	{
		beeping = false;
		// noTone(BEEP_PIN);		
		// pinMode(BEEP_PIN, INPUT);
	}

	if (currentState_last != currentState)
	{
		currentNote = 0;
		currentState_last = currentState;
	}

	



	// Play music!
	if (millis() < note_starttime) 			// Timer wrapped -- reset (From memory this state takes about 3 days to get to)
		beep_starttime = millis();

	if (currentState == ST_HighScore)
		tempoDivide = 2;
	else
		tempoDivide = 1;

	if ((note_starttime + SONG_TEMPO / tempoDivide) > millis())
		return;

	switch (currentState)
	{
		case ST_Correct:
			if (currentNote >= sizeof(win_song) / sizeof(win_song[0]))
			{
				noTone(BEEP_PIN);
				pinMode(BEEP_PIN, INPUT);
			}
			else
			{
				pinMode(BEEP_PIN, OUTPUT);
				tone(BEEP_PIN, notes[win_song[currentNote++]]) ;
			}
			break;

		case ST_Incorrect:
			if (currentNote >= sizeof(lose_song) / sizeof(lose_song[0]))
			{
				noTone(BEEP_PIN);
				pinMode(BEEP_PIN, INPUT);
			}
			else
			{
				pinMode(BEEP_PIN, OUTPUT);

				if (lose_song[currentNote++] > 0)
					tone(BEEP_PIN, notes[lose_song[currentNote]]);
				else
					noTone(BEEP_PIN);
			}
			break;

		case ST_HighScore:
			if (currentNote >= sizeof(highscore_song) / sizeof(highscore_song[0]))
			{
				noTone(BEEP_PIN);						// Turn off speaker once song finished
				pinMode(BEEP_PIN, INPUT);
			}
			else
			{
				pinMode(BEEP_PIN, OUTPUT);
				tone(BEEP_PIN, notes[highscore_song[currentNote++]]);
			}
			break;


		default:
			if (!beeping)
			{
				noTone(BEEP_PIN);		
				pinMode(BEEP_PIN, INPUT);
			}
			break;
	}


	note_starttime = millis();


	return;
}


void beepNow()
{
	// Resets start time of beep timer
	beep_starttime = millis();

	return;
}
