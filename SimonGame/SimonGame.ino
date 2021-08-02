// 'Simon Game' assembled by S.Eames with help from the internet (thanks team :) )

#define debugMSG	1				// Uncomment to get debug / error messages in console

// Include libraries
#include "FastLED.h"
#include "RF24.h"
#include "ENUMVars.h"

///////////////////////// IO /////////////////////////
#define NUM_BTNS 4
const uint8_t Button[NUM_BTNS] = {18, 8, 20, 21};

uint8_t btnState_last[NUM_BTNS];		// State of buttons on previous iteration
uint8_t btnState_now[NUM_BTNS];			// Last read state of buttons
uint8_t btnState_flag[NUM_BTNS];		// 

#define BUZZ_PIN 5
#define RAND_ANALOG_PIN	10 				// Leave pin floating -- used to seed random number generator




//////////////////// RF Variables ////////////////////
RF24 myRadio (19, 18); // CE, CSN
byte addresses[][6] = {"973126"};

const byte numChars = 32;
char receivedChars[numChars];

//////////////////// Pixel Setup ////////////////////
#define NUM_LEDS 12
#define DATA_PIN 6
CRGB leds[NUM_LEDS]; // Define the array of leds

#define LED_BRIGHTNESS 10

// Colours!
#define COL_RED     0xFF0000
#define COL_YELLOW  0xFF8F00
#define COL_GREEN   0x00FF00
#define COL_BLUE    0x0000FF

#define COL_WHITE   0xFFFF7F
#define COL_BLACK   0x000000


//////////////////// MISC VARIABLES ////////////////////

///// TIMING

#define DEBOUNCE 			5		// Button debounce time
#define LED_REFRESH 		500 	// Refresh rate of LED patterns
#define LED_EFFECT_TIME		400		// Interval of LED effects
#define LED_EFFECT_LOOP		5 		// Multiplier of LED effect time


///// PATTERN
#define SEQ_MAX_LEN	100 			// Maximum sequence length
uint8_t sequence[SEQ_MAX_LEN];		// Holds sequence used for game
uint8_t seq_stage = 0;				// Current level achieved of sequence - increments with each success




//// GAME STATE

gameStates currentState = ST_Lobby;


void setup() 
{
	// Initialise IO
	for (uint8_t i = 0; i < NUM_BTNS; ++i)
	{
		pinMode(Button[i], INPUT_PULLUP);
		btnState_last[i] = 1;				// Set initial assumed button state
	}

	pinMode(BUZZ_PIN, OUTPUT);
	digitalWrite(BUZZ_PIN, HIGH);


	// Initialise LEDs
	FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
	fill_solid( leds, NUM_LEDS, COL_BLACK);
	FastLED.setBrightness(LED_BRIGHTNESS);
	FastLED.show();

	// Initialise Buzzer

	// Initialise Radio
	myRadio.begin();  
	myRadio.setChannel(115);
	myRadio.setPALevel(RF24_PA_MAX);
	myRadio.setDataRate( RF24_250KBPS );
	myRadio.openWritingPipe( addresses[0]);

	// Initialise Serial debug
	Serial.begin(115200);

	#ifdef debugMSG
		while (!Serial) ; 								// Wait for serial port to be available

		Serial.println(F("Wassup?"));
	#endif


	// Start the game!
	

}

void loop() 
{
	updateLEDs();

	switch (currentState) 
	{
		case ST_Lobby: // Waiting for someone to initiate a game by pressing any button
			if(checkButtons() != NUM_BTNS)
				currentState = ST_Intro;
			break;
		case ST_Intro:
			// Refer to LED function for this part
			break;
		case ST_SeqPlay:
			break;
		case ST_SeqRec:
		// statements
			break;
		case ST_Correct:
		// statements
			break;
		case ST_Incorrect:
		// statements
			break;			
		default:
		// statements
			break;
	}
}

void generateSequence()
{
	// Seed random number generator
	randomSeed(analogRead(RAND_ANALOG_PIN));

	// fill sequence with random numbers according to number of buttons used in game
	for (uint8_t i = 0; i < SEQ_MAX_LEN; ++i)
		{
			sequence[i] = random(0, NUM_BTNS);
			Serial.println(sequence[i]);
		}

	return;
}


uint8_t checkButtons()
{
	// Checks button state. Returns true if there was a change


	// Debounce buttons
	static long lasttime;

	if (millis() < lasttime) 						// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + LED_REFRESH) > millis())			// Debounce timer hasn't elapsed
		return NUM_BTNS; 
	
	lasttime = millis();							// Debouncing complete; record new time & continue

	// Record button state
	for (uint8_t i = 0; i < NUM_BTNS; ++i)
	{
		btnState_now[i] = digitalRead(Button[i]);	// Read input

		// Serial.print("Button number = ");
		// Serial.print(i);
		// Serial.print("\t State = ");
		// Serial.print(btnState_now[i]);
		// Serial.print("\t PrevState = ");
		// Serial.println(btnState_last[i]);


		if (btnState_now[i] != btnState_last[i]) 	// If button state changed 
		{
			btnState_last[i] = btnState_now[i];		// Record button state

			if (!btnState_now[i])
				return i;							// Return number of button that was just pressed
		}
	}

	return NUM_BTNS;								// Return NUM_BTNS if no buttons pressed
}



void updateLEDs()
{

	static uint8_t effect_step = 0;
	// Debounce buttons
	static long lasttime;

	if (millis() < lasttime) 						// Millis() wrapped around - restart timer
		lasttime = millis();


	
	


	static uint8_t hue = 0;


	// Does patterns on LEDs according to state
	switch (currentState) 
	{
		case ST_Lobby: // Waiting for someone to initiate a game by pressing any button

			if ((lasttime + LED_REFRESH) > millis())
				return; 
			
			for(uint8_t i = 0; i < NUM_LEDS; i++) 
				leds[i] = CHSV(hue++, 255, 255);

			break;


		case ST_Intro:

			if ((lasttime + LED_EFFECT_TIME) > millis())
				return; 
			
			if (effect_step++ % 2)
				fill_solid( leds, NUM_LEDS, COL_BLACK);
			else
				fill_solid( leds, NUM_LEDS, COL_WHITE);


			if (effect_step > LED_EFFECT_LOOP)
			{
				effect_step = 0;
				generateSequence();
				currentState = ST_SeqPlay;
			}
		
			break;
		case ST_SeqPlay:
		// statements
			break;
		case ST_SeqRec:
		// statements
			break;
		case ST_Correct:
		// statements
			break;
		case ST_Incorrect:
		// statements
			break;			
		default:
		// statements
			break;
	}

	lasttime = millis();

	FastLED.show(); 
}


