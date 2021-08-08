// 'Simon Game' assembled by S.Eames with help from the internet (thanks team :) )

#define debugMSG	1				// Uncomment to get debug / error messages in console

// Include libraries
#include "FastLED.h"
#include "RF24.h"
#include "ENUMVars.h"
#include <MD_MAX72xx.h>

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

// Button state tracking
bool btnState_last;					// NOTE: Using pull-up resistors, so 'HIGH' = off
bool btnState_now;

//////////////////// RF VARIABLES ////////////////////
RF24 radio(RF_CE_PIN, RF_CSN_PIN); 		// CE, CSN
uint8_t addresses[][6] = {"node0", "node1"}; 	//master address, slave address (same for all slaves)

#define RF_BUFF_LEN 2			// Number of bytes to transmit / receive
uint8_t radioBuf_RX[RF_BUFF_LEN];
uint8_t radioBuf_TX[RF_BUFF_LEN];
bool newRFData = false;	// True if new data over radio just in

bool master = false;		// master runs the show ;) 
uint8_t myID;				// ID of this station (set in setup() accoding to IO)
uint8_t myID_flag;			// ID in flag form (master = 0b00000001, node1 = 0c00000010, etc)
uint8_t btnsPresent = 0x01; // Flags of detected buttons (master is obviously there)
uint8_t numBtns = 1;		// Number of buttons being used

//////////////////// PIXEL SETUP ////////////////////
#define NUM_LEDS 	12

CRGB leds[NUM_LEDS]; // Define the array of leds

#define LED_BRIGHTNESS 10

// Colours!
#define COL_RED     0xFF0000
#define COL_YELLOW  0xFF8F00
#define COL_GREEN   0x00FF00
#define COL_BLUE    0x0000FF
#define COL_PINK	0xFF002F
#define COL_CYAN	0x00AAAA
#define COL_ORANGE	0xFF1C00

const uint32_t BTN_COLS[] = {COL_RED, COL_YELLOW, COL_GREEN, COL_BLUE, COL_PINK, COL_CYAN, COL_ORANGE};

#define COL_WHITE   0xFFFF7F
#define COL_BLACK   0x000000

bool effectComplete = false;


//////////////// MATRIX DISPLAY SETUP ////////////////
const MD_MAX72XX::moduleType_t HARDWARE_TYPE = MD_MAX72XX::FC16_HW;
#define DISP_NUM_PANELS	2

MD_MAX72XX disp = MD_MAX72XX(HARDWARE_TYPE, DISP_CS_PIN, DISP_NUM_PANELS);

#define DISP_INTENSITY 2 			// Brightness of display on range 0-14

bool scrollTextComplete = false;		// Set to '1' once text completed a full cycle


//////////////////// MISC VARIABLES ////////////////////

uint32_t beep_starttime = 0;


///// TIMING

#define DEBOUNCE 			5		// (ms) Button debounce time
#define LED_REFRESH 		100 	// (ms) Refresh rate of LED patterns
#define LED_EFFECT_TIME		150		// (ms) Interval of LED effects
#define LED_EFFECT_LOOP		6 		// Multiplier of LED effect time
#define TICKER_TIME			80		// (ms) Update interval of ticker text
#define SCORE_DISPLAY		3000	// (ms) Time score is displayed for after a round
#define BEEP_TIME			100 	// (ms) period beeper sounds for
#define RF_POLL_FREQ		5000		// (ms) Period on which nodes are polled 
#define RF_REPLY_DELAY		10 		// (ms) Delay between slave nodes responding to group broadcast

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
uint8_t lastBtnPressed = numBtns;


//// GAME STATE

gameStates currentState = ST_Lobby;
gameStates lastState = ST_Lobby;


void setup() 
{
	// Initialise Display
	disp.begin();
	disp.control(2, DISP_INTENSITY);
	disp.clear();

	// Initialise Button
	pinMode(BTN_PIN, INPUT_PULLUP);
	btnState_last = digitalRead(BTN_PIN);

	// Initialise beeper
	pinMode(BEEP_PIN, OUTPUT);
	digitalWrite(BEEP_PIN, HIGH);

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
	radio.begin();  
	radio.setChannel(111);					// Keep out of way of common wifi frequencies
	radio.setPALevel(RF24_PA_MIN);			// Let's make this powerful... later
	radio.setDataRate( RF24_2MBPS );		// Let's make this quick

	// Opening Listening pipes
	if (master)
	{
		radio.openReadingPipe(1, addresses[0]);
		radio.openWritingPipe(addresses[1]);
	}
	else // slave --> sends all messages to master
	{
		radio.openReadingPipe(1, addresses[1]);
		radio.openWritingPipe(addresses[0]);
	}

	radio.startListening();




	// Initialise Serial debug
	#ifdef debugMSG
		Serial.begin(115200);				// Open comms line
		while (!Serial) ; 					// Wait for serial port to be available

		Serial.println(F("Wassup?"));
		Serial.print(F("myID = "));
		Serial.println(myID);

		if (master)
			Serial.println(F("THE MASTER HAS ARRIVED!"));
	#endif

	// Let the games begin!


	

}

void loop() 
{
	static uint32_t lasttime;				// Used for timining intervals
	bool readSuccess = false;

	updateBeepState();

	if (master)
	{



		switch (currentState) 
		{
			case ST_Lobby: // Waiting for someone to initiate a game by pressing any button
				updateLEDs();
				
				
				if (lastState != ST_Lobby)			// Clears any residual text in scrollText buffer
				{
					scrollText(" ");
					btnsPresent = 1;				// Reset list of connected buttons when entering Lobby state
					lastState = currentState;
				}

				checkNumBtns();

				scrollText("PRESS TO PLAY ");

				if (numBtns >= 4)						// Don't start new game until 4 or more buttons connected
					checkStartNewGame();				// Start new game if any button pressed
				
				break;


			case ST_Intro:
				updateLEDs();
				// Refer to LED function for this part
				break;


			case ST_SeqPlay:
				
				// Reset variables
				// Generate new sequence if we're starting a new game
				if ((lastState != ST_Correct) && (lastState != currentState))			
				{
					generateSequence();
					lasttime = millis(); 			// Initiate timer
					seq_StepTimeStage = 0;
					lastState = currentState;
				}
				
				if (lastState != currentState)		
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
					if(lastBtnPressed != numBtns)			// If any button pressed...
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

				updateLEDs();										// Play success animation

				break;


			case ST_Incorrect:

				lastState = currentState;

				// Play fail animation
				if (effectComplete)
				{
					OffAllButtons();
					effectComplete = false;
				}
				else
				{
					disp.clear();
					updateLEDs();		// Fail LED sequence
					break;
				}

				// Check for high score

				// Handle case where 0 levels achieved
				if (seq_level == 0)
					seq_level++;

				// check for high score
				if (--seq_level > highscore)
				{
					currentState = ST_HighScore;
					highscore = seq_level;
					seq_level = 0;					// Reset seq level after using
				}
				else
					currentState = ST_ShowScore;		

				break;		


			case ST_HighScore:
				checkStartNewGame();				// Start new game if any button pressed
				updateLEDs();

				// Scroll 'high score' text, then show number
				// TODO: make it only run 'dispNumber' once --> I don't like the flickering screen when it's refreshing unnecessarily
				if (scrollTextComplete)
					dispNumber(highscore);
				else
				{
					scrollText("HIGH SCORE   ");
					break;
				}


				// Show high score value for a bit
				if (lastState != ST_HighScore)
				{
					lasttime = millis(); 			// Initiate timer
					lastState = currentState;
				}

				// Timing
				if (millis() < lasttime) 			// Timer wrapped --> reset (should never be an issue here)
					lasttime = millis();

				if ((lasttime + SCORE_DISPLAY) > millis())		
					break; 
				
				currentState = ST_Lobby;
				break;


			case ST_ShowScore:

				checkStartNewGame();
				if (lastState != currentState)
				{
					dispNumber(seq_level);
					seq_level = 0;					// Reset seq level after using
					lastState = currentState;
					lasttime = millis();
				}


				OffAllButtons();

				// Timing
				if (millis() < lasttime) 			// Timer wrapped --> reset (should never be an issue here)
					lasttime = millis();

				if ((lasttime + SCORE_DISPLAY) > millis())		
					break; 

				currentState = ST_Lobby;


			default:
			// statements
				break;
		}

	}
	else // Slave device
	{
		// Listen to radio
		// Respond to broadcast messages


		while ( radio.available() )
		{
			// Read in 
			radio.read( radioBuf_RX, RF_BUFF_LEN);
			newRFData = true;
			Serial.println(F("New RF Message!"));
		}

		if (newRFData)
		{
			radioBuf_TX[0] = myID_flag;

			// Transmit message
			radio.stopListening();
			radio.write(radioBuf_TX, RF_BUFF_LEN); // Transmit message & get ack
			radio.startListening();

			newRFData = false;
		}


	}
}

void checkNumBtns()
{
	// Broadcasts message asking which buttons are around
	// Then records present buttons


	static uint32_t lasttime = 10000; 				// I want it to run the loop TX part of the code first
	bool readSuccess = false;
	uint8_t temp = 0;	

	// Timing
	if (millis() < lasttime) 						// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + RF_POLL_FREQ) > millis())		// Check for radio responses
	{
		while ( radio.available() )
		{
			// Read in 
			radio.read( radioBuf_RX, RF_BUFF_LEN);
			newRFData = true;
		}

		if (newRFData)
		{
			btnsPresent |= radioBuf_RX[0];

			// Tally up number of present buttons
			temp = btnsPresent;
			numBtns = 0;

			while (temp) 
			{
				numBtns += temp & 1;
				temp >>= 1;
			}

			newRFData = false;
		}		
	} 
	else
	{
		Serial.println(F("Polling Nodes"));
		radio.stopListening();
		radio.write(radioBuf_TX, RF_BUFF_LEN); // Transmit message without checking for ack
		radio.startListening();
		lasttime = millis();						// It's time! Record new start time & continue
	}

	return;
}


void checkStartNewGame()
{
	// Initiates new game if any button is pressed
	if(checkButtons() != numBtns)
	{
		lastState = currentState;
		currentState = ST_Intro;
		disp.clear();
	}

	return;
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
			sequence[i] = random(0, numBtns);

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

	// Check for button press input from radios


	// Debounce buttons
	static uint32_t lasttime;

	if (millis() < lasttime) 						// Millis() wrapped around - restart timer
		lasttime = millis();

	if ((lasttime + DEBOUNCE) > millis())			// Debounce timer hasn't elapsed
		return numBtns; 
	
	lasttime = millis();							// Debouncing complete; record new time & continue

	// Record button state
	btnState_now = digitalRead(BTN_PIN);	// Read input

	if (btnState_now != btnState_last) 	// If button state changed 
	{
		btnState_last = btnState_now;		// Record button state

		if (!btnState_now)
		{
			beepNow();
			return myID;							// Return number of button that was just pressed
		}
	}

	return numBtns;								// Return NUM_BTNS if no buttons pressed
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
			if (lastState != ST_Intro)
			{
				lasttime = millis();
				lastState = currentState;
				effect_step = 0;
			}

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

			effect_2Step(effect_step++, COL_GREEN, COL_WHITE);

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

			effect_2Step(effect_step++, COL_BLACK, COL_RED);

			if (effect_step > LED_EFFECT_LOOP)
			{
				effect_step = 0;
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
		// statements
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

	// Shift display left one column
	disp.transform(MD_MAX72XX::TSL);	

	// Display one column of current character
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



void updateBeepState()
{
	// Turns beeper off / on according to whether timer has elapsed

	if (millis() < beep_starttime) 			// Timer wrapped -- reset (From memory this state takes about 3 days to get to)
		beep_starttime = millis();

	if ((beep_starttime + BEEP_TIME) > millis())		
		digitalWrite(BEEP_PIN, LOW);		// Beep
	else
		digitalWrite(BEEP_PIN, HIGH);		// !Beep

	return;
}

void beepNow()
{
	// Resets start time of beep timer
	beep_starttime = millis();
	return;
}
