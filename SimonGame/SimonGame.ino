// 'Simon Game' assembled by S.Eames with help from the internet (thanks team :) )

#define debugMSG	1				// Uncomment to get debug / error messages in console

// Include libraries
#include "FastLED.h"
#include "RF24.h"
#include "ENUMVars.h"
// #include <MD_MAXPanel.h>
// #include "Font5x3.h"
#include <MD_MAX72xx.h>


///////////////////////// IO /////////////////////////
#define NUM_BTNS 4


#define BUZZ_PIN 		5
#define LED_PIN 		6
const uint8_t Button[NUM_BTNS] = {7, 8, 9, 10};	// Pins for buttons
#define RF_CSN_PIN		18
#define RF_CE_PIN 		19
#define DISP_CS_PIN		20
#define RAND_ANALOG_PIN	21 				// Analog pin -- Leave pin floating -- used to seed random number generator

// Button state arrays
uint8_t btnState_last[NUM_BTNS];		// State of buttons on previous iteration
uint8_t btnState_now[NUM_BTNS];			// Last read state of buttons

//////////////////// RF VARIABLES ////////////////////
RF24 myRadio(RF_CE_PIN, RF_CSN_PIN); // CE, CSN
byte addresses[][6] = {"973126"};

const byte numChars = 32;
char receivedChars[numChars];

//////////////////// PIXEL SETUP ////////////////////
#define NUM_LEDS 	12

CRGB leds[NUM_LEDS]; // Define the array of leds

#define LED_BRIGHTNESS 10

// Colours!
#define COL_RED     0xFF0000
#define COL_YELLOW  0xFF8F00
#define COL_GREEN   0x00FF00
#define COL_BLUE    0x0000FF

const uint32_t BTN_COLS[NUM_BTNS] = {COL_BLUE, COL_GREEN, COL_YELLOW, COL_RED};

#define COL_WHITE   0xFFFF7F
#define COL_BLACK   0x000000


//////////////// MATRIX DISPLAY SETUP ////////////////
const MD_MAX72XX::moduleType_t HARDWARE_TYPE = MD_MAX72XX::FC16_HW;
// const uint8_t X_DEVICES = 2;
// const uint8_t Y_DEVICES = 1;
#define DISP_NUM_PANELS	2

// MD_MAXPanel disp = MD_MAXPanel(HARDWARE_TYPE, DISP_CS_PIN, X_DEVICES, Y_DEVICES);
MD_MAX72XX disp = MD_MAX72XX(HARDWARE_TYPE, DISP_CS_PIN, DISP_NUM_PANELS);

#define DISP_INTENSITY 2 			// Brightness of display on range 0-14


//////////////////// MISC VARIABLES ////////////////////

///// TIMING

#define DEBOUNCE 			5		// (ms) Button debounce time
#define LED_REFRESH 		100 	// (ms) Refresh rate of LED patterns
#define LED_EFFECT_TIME		150		// (ms) Interval of LED effects
#define LED_EFFECT_LOOP		10 		// Multiplier of LED effect time
#define TICKER_TIME			80		// (ms) Update interval of ticker text

// StepTime (ms) is the on & off time intervals of steps played back in a sequence. 
// SEQ_STEP_PER_BLOCK indicates when speed increases - i.e. after level = SEQ_STEP_PER_BLOCK, seq_StepTime[1] is used for interval
// For levels higher than sizeof(seq_StepTime) * SEQ_STEP_PER_BLOCK, seq_StepTime[sizeof(seq_StepTime) -1] is used
// When in ST_SeqRec, 2*seq_StepTime is the allowed space for players to press button
uint16_t seq_StepTime[] = {600, 550, 500, 450, 400, 350, 300, 250, 200};
uint8_t seq_StepTimeStage = 0;
#define SEQ_STEP_PER_BLOCK	4

#define SEQ_INPUTTIME_MULT	3 		// SEQ_INPUTTIME_MULT * seq_StepTime[x] = max allowed time to press button


///// PATTERN
#define SEQ_MAX_LEN	255				// Maximum sequence length (maximum value uint8_t can store)
uint8_t sequence[SEQ_MAX_LEN];		// Holds sequence used for game
uint8_t seq_level = 0;				// Current level achieved of sequence - increments with each success
uint8_t seq_RecPlayStep = 0;		// Current step being either played back or recorded
uint8_t highscore = 0;				// Holds highest achieved level (since power on);
uint8_t seq_LightOn = 0;			// (bool) holds whether in on or off state of playing sequence
uint8_t lastBtnPressed = NUM_BTNS;


//// GAME STATE

gameStates currentState = ST_Lobby;
gameStates lastState = ST_Lobby;


void setup() 
{
	// Initialise Display
	disp.begin();
	// disp.setIntensity(DISP_INTENSITY);
	disp.control(2, DISP_INTENSITY);
	disp.clear();

	// Initialise IO
	for (uint8_t i = 0; i < NUM_BTNS; ++i)
	{
		pinMode(Button[i], INPUT_PULLUP);
		btnState_last[i] = 1;				// Set initial assumed button state
	}

	// Initialise Buzzer
	pinMode(BUZZ_PIN, OUTPUT);
	digitalWrite(BUZZ_PIN, HIGH);



	// Initialise LEDs
	FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
	fill_solid( leds, NUM_LEDS, COL_BLACK);
	FastLED.setBrightness(LED_BRIGHTNESS);
	FastLED.show();



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
	static uint32_t lasttime;


	

	switch (currentState) 
	{
		case ST_Lobby: // Waiting for someone to initiate a game by pressing any button
			updateLEDs();
			scrollText("PRESS TO PLAY ");
			if(checkButtons() != NUM_BTNS) 		// start game if any button is pressed
			{
				lastState = currentState;
				currentState = ST_Intro;
				disp.clear();
			}
			break;
		case ST_Intro:
			updateLEDs();
			// Refer to LED function for this part
			break;
		case ST_SeqPlay:
			
			// Reset variables
			if (lastState == ST_Intro)
			{
				generateSequence();
				seq_StepTimeStage = 0;

			}
			if (lastState != ST_SeqPlay)
			{
				lasttime = millis(); 			// Initiate timer
				OffAllButtons();
				seq_RecPlayStep = 0;
				lastState = currentState;
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
				// TODO: handle this case better - all levels achieved!
			}


			// GOTO Record state if we've finished playing sequence
			if (!seq_LightOn && (seq_RecPlayStep > seq_level))
			{
				currentState = ST_SeqRec;
				seq_level++;

				OffAllButtons();
			}
			


			break;


		case ST_SeqRec:

			// Timing							// Note: timer still running from last stage & StepTime same as Play Stage
			if (millis() < lasttime) 			// Timer wrapped -- reset (should never be an issue here)
				lasttime = millis();

			// Start indicator
			if (lastState == ST_SeqPlay)						// Light all buttons white to indicate start of record sequence
			{
				// Note: need to measure button presses once we're out of this section so can't use break then
				if ((lasttime + seq_StepTime[seq_StepTimeStage]) > millis())		
					break; 
			
				lasttime = millis();


				if (seq_LightOn) 
				{
					OffAllButtons();							// Start rec with all black again
					lastState = currentState;
					seq_LightOn = !seq_LightOn;
					seq_RecPlayStep = 0;						// Reset step variable when starting record sequence
					// disp.clear();
					staticText("GO!");
				}
				else 											// NOTE: this is run first
				{
					fill_solid( leds, NUM_LEDS, COL_WHITE);	 	// All White
					FastLED.show();
					seq_LightOn = !seq_LightOn;	

					solidDisplay();
				}

				break;
			}


			// Record player inputs!
			if ((lasttime + (seq_StepTime[seq_StepTimeStage] * SEQ_INPUTTIME_MULT)) > millis())	
			{
				// Turn off previously lit buttons
				if ((lasttime + seq_StepTime[seq_StepTimeStage]) < millis())
					OffAllButtons();

				lastBtnPressed = checkButtons();		// check for input
				if(lastBtnPressed != NUM_BTNS)			// If any button pressed...
				{
					disp.clear();
					LightButton(lastBtnPressed);		// Light up button colour that was pressed
					lasttime = millis();				// reset timer on button press
				
					// Check correct button was pressed
					if (lastBtnPressed == sequence[seq_RecPlayStep])
					{
						#ifdef debugMSG
							Serial.println(F("Correct"));
							Serial.print(F("Step Number = "));
							Serial.println(seq_RecPlayStep);
						#endif

						if (seq_RecPlayStep >= seq_level-1)	// Go to next state if finished recording sequence
							currentState = ST_Correct;
						else
							seq_RecPlayStep++;							
					}
					else
					{
						// Incorrect
						#ifdef debugMSG
							Serial.println(F("Incorrect"));
							Serial.print(F("Step Number = "));
							Serial.print(seq_RecPlayStep);
							Serial.print(F(", Expected input = "));
							Serial.print(sequence[seq_RecPlayStep]);
							Serial.print(F(", Button Pressed = "));
							Serial.println(lastBtnPressed);
						#endif


						currentState = ST_Incorrect;
					}
				}

			}
			else 										// Failed to press button in allowed time frame --> end game
			{
				// Timeout
				#ifdef debugMSG
					Serial.println(F("Input timeout"));
				#endif

				lasttime = millis();
				currentState = ST_Incorrect;
			}
		



		
			break;


		case ST_Correct:

			// Start indicator
			if (lastState != ST_Correct)						// Keep buttons lit from last State, then play animation
			{
				// Note: need to measure button presses once we're out of this section so can't use break then
				if ((lasttime + LED_EFFECT_TIME) > millis())		
					break; 
			
				lasttime = millis();


				if (seq_LightOn) 
				{
					OffAllButtons();							// Start with all black again
					lastState = currentState;
					seq_LightOn = !seq_LightOn;
				}
				else 											// NOTE: this is run first
				{
					seq_LightOn = !seq_LightOn;		
				}

				break;
			}


			// Play success animation
			updateLEDs();

			break;


		case ST_Incorrect:

			// Play fail animation
			updateLEDs();

			// check for high score
			if (--seq_level > highscore)
			{
				currentState = ST_HighScore;
				highscore = seq_level;
			}
			else
				currentState = ST_Lobby;

			// reset variables
			seq_level = 0;

			break;		

		case ST_HighScore:
			scrollText("HIGH SCORE ");
			// currentState = ST_Lobby;
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
	// Checks button state
	// If button just pressed returns that button number, else returns NUM_BTNS


	// Debounce buttons
	static uint32_t lasttime;

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
	// static uint8_t ticker_step = disp.getXMax();
	// Debounce buttons
	static uint32_t lasttime;

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
			{
				leds[i] = CHSV(hue, 255, 255);
				hue += 255/NUM_LEDS;
			}


			break;


		case ST_Intro:

			if ((lasttime + LED_EFFECT_TIME) > millis())
				return; 

			solidDisplay();
			
			if (effect_step++ % 2)
				fill_solid( leds, NUM_LEDS, COL_WHITE);
			else
				fill_solid( leds, NUM_LEDS, COL_BLACK);


			if (effect_step > LED_EFFECT_LOOP)
			{
				effect_step = 0;
				lastState = currentState;
				currentState = ST_SeqPlay;
				disp.clear();
			}
		
			break;
		case ST_SeqPlay:
			// Done in main loop
			break;

		case ST_SeqRec:
			// Done in main loop
			break;

		case ST_Correct:
			if ((lasttime + LED_EFFECT_TIME) > millis())
				return; 

			fill_solid( leds, NUM_LEDS, COL_WHITE);

			for (uint8_t i = effect_step++ % 2; i < NUM_LEDS; i += 2)
				leds[i] = COL_GREEN;

			if (effect_step > LED_EFFECT_LOOP)
			{
				effect_step = 0;
				lastState = currentState;
				currentState = ST_SeqPlay;
			}
			break;

		case ST_Incorrect:
			if ((lasttime + LED_EFFECT_TIME) > millis())
				return; 

			fill_solid( leds, NUM_LEDS, COL_BLACK);

			for (uint8_t i = effect_step++ % 2; i < NUM_LEDS; i += 2)
				leds[i] = COL_RED;

			if (effect_step > LED_EFFECT_LOOP)
			{
				effect_step = 0;
				lastState = currentState;
				currentState = ST_Lobby;
			}
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


void scrollText(const char *p)
{
	// Scrolling text - starts from where it left off... not sure how to fix that yet
	// Displays the next frame of given pointer each time it's called

	// static uint8_t *last_p = *p;
	static uint8_t cBuf[8];  // Column buffer - this should be ok for all built-in fonts
	static uint8_t charCol = 0;
	static uint8_t letterNum = 0;
	static uint8_t charWidth = 0;
	static uint32_t lasttime = millis();

	if (millis() < lasttime) 						// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + TICKER_TIME) > millis())			// Debounce timer hasn't elapsed
		return; 

	lasttime = millis();							// It's time! Record new time & continue


	// // Reset the things if new string to display
	// if ((last_p != p) || (*p != *last_p)) // compares ptr and 1st character change... I think - I'm not great with pointers :( 
	// {
	// 	// NOTE: This assumes either different ptr value for each string, OR different start charater
	// 	// That will cover most cases anyway
	// 	// New string! (probably?) - Reset the things!
	// 	*last_p = *p;
	// 	letterNum = 0;
	// 	disp.clear();		
	// }

	if (charCol == 0)
	{
		// Loop back to start of string
		if (*(p + letterNum) == '\0')
			letterNum = 0;

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

	// Shift display left one column
	disp.transform(MD_MAX72XX::TSL);	

	// Display first column of that character
	disp.setColumn(0, cBuf[charCol++]);

	if (charCol > charWidth)
		charCol = 0;


	return;
}

void staticText(const char *p)
{
	// Prints given text in the center of the display

	uint8_t cBuf[(DISP_NUM_PANELS+1) * 8];	// Screen buffer
	uint8_t width_full = 0;					// Cumulative width of text


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

	if (Number < 10)							// 1 Digit numbers
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

	staticText(charBuf);						// Print to screen!


	return;
}
