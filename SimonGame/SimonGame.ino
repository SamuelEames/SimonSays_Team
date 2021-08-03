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

const uint32_t BTN_COLS[NUM_BTNS] = {COL_RED, COL_YELLOW, COL_GREEN, COL_BLUE};

#define COL_WHITE   0xFFFF7F
#define COL_BLACK   0x000000


//////////////////// MISC VARIABLES ////////////////////

///// TIMING

#define DEBOUNCE 			5		// (ms) Button debounce time
#define LED_REFRESH 		200 	// (ms) Refresh rate of LED patterns
#define LED_EFFECT_TIME		300		// (ms) Interval of LED effects
#define LED_EFFECT_LOOP		6 		// Multiplier of LED effect time

// StepTime (ms) is the on & off time intervals of steps played back in a sequence. 
// SEQ_STEP_PER_BLOCK indicates when speed increases - i.e. after level = SEQ_STEP_PER_BLOCK, seq_StepTime[1] is used for interval
// For levels higher than sizeof(seq_StepTime) * SEQ_STEP_PER_BLOCK, seq_StepTime[sizeof(seq_StepTime) -1] is used
// When in ST_SeqRec, 2*seq_StepTime is the allowed space for players to press button
uint16_t seq_StepTime[] = {1000, 900, 800, 700, 600, 500, 400, 300};
uint8_t seq_StepTimeStage = 0;
#define SEQ_STEP_PER_BLOCK	5


///// PATTERN
#define SEQ_MAX_LEN	255				// Maximum sequence length (maximum value uint8_t can store)
uint8_t sequence[SEQ_MAX_LEN];		// Holds sequence used for game
uint8_t seq_level = 0;				// Current level achieved of sequence - increments with each success
uint8_t seq_RecPlayStep = 0;		// Current step being either played back or recorded
uint8_t highscore = 0;				// Holds highest achieved level (since power on);
uint8_t seq_LightOn = 0;			// (bool) holds whether in on or off state of playing sequence



//// GAME STATE

gameStates currentState = ST_Lobby;
gameStates lastState = ST_Lobby;


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
	static long lasttime;


	

	switch (currentState) 
	{
		case ST_Lobby: // Waiting for someone to initiate a game by pressing any button
			updateLEDs();
			if(checkButtons() != NUM_BTNS) 		// start game if any button is pressed
			{
				lastState = currentState;
				currentState = ST_Intro;
			}
			break;
		case ST_Intro:
			updateLEDs();
			// Refer to LED function for this part
			break;
		case ST_SeqPlay:
			if (lastState == ST_Intro)
			{
				generateSequence();
				lastState = ST_SeqPlay;

				// Reset variables
				seq_RecPlayStep = 0;
				seq_StepTimeStage = 0;
				lasttime = millis(); 			// Initiate timer

				OffAllButtons();
			}


			// Timing
			if (millis() < lasttime) 			// Timer wrapped -- reset (should never be an issue here)
				lasttime = millis();



			if ((lasttime + seq_StepTime[seq_StepTimeStage]) > millis())		
				break; 
			
			lasttime = millis();

			// Show SeqStep
			if (seq_LightOn)					// Currently on --> turn off
			{
				OffAllButtons();
				seq_LightOn = !seq_LightOn;
			}
			else 								// Currently off --> turn on
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
				// TODO: handle this case better - all levels achieved
			}
			
			







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

	#ifdef debugMSG
		Serial.print(F("Sequence ="));
	#endif

	// fill sequence with random numbers according to number of buttons used in game
	for (uint8_t i = 0; i < SEQ_MAX_LEN; ++i)
		{
			sequence[i] = random(0, NUM_BTNS);

			#ifdef debugMSG
				Serial.print(F(" "));
				Serial.print(sequence[i]);
			#endif
		}

		#ifdef debugMSG
			Serial.println();
		#endif

	return;
}


uint8_t checkButtons()
{
	// Checks button state. Returns true if there was a change


	// Debounce buttons
	static long lasttime;

	if (millis() < lasttime) 						// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + DEBOUNCE) > millis())			// Debounce timer hasn't elapsed
		return NUM_BTNS; 
	
	lasttime = millis();							// Debouncing complete; record new time & continue

	// Record button state
	for (uint8_t i = 0; i < NUM_BTNS; ++i)
	{
		btnState_now[i] = digitalRead(Button[i]);	// Read input

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
				fill_solid( leds, NUM_LEDS, COL_WHITE);
			else
				fill_solid( leds, NUM_LEDS, COL_BLACK);


			if (effect_step > LED_EFFECT_LOOP)
			{
				effect_step = 0;
				lastState = currentState;
				currentState = ST_SeqPlay;
			}
		
			break;
		case ST_SeqPlay:
			if (lastState != ST_SeqPlay)
				


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

void LightButton(uint8_t button)
{
	// Lights up according to given button number


	fill_solid( leds, NUM_LEDS, BTN_COLS[button]);
	FastLED.show();

	return;
}

void OffAllButtons()
{
	// Turns off lights on buttons

	fill_solid( leds, NUM_LEDS, COL_BLACK);
	FastLED.show();

	return;
}
