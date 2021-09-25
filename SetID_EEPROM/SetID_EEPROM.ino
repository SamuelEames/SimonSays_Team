#include <EEPROM.h>
#include <avr/sleep.h>	

// change this to be the ID of your node in the mesh network
uint8_t myID = 0;

/* Node IDs

	52			Master station
	21-?
*/

void setup() 
{
	Serial.begin(115200);
		while (!Serial) ; // Wait for serial port to be available

	Serial.println("setting myID...");

	EEPROM.write(2, myID);
	Serial.print(F("set myID = "));
	Serial.println(myID);

	// Check it saved properly
	uint8_t readID = EEPROM.read(2);

	Serial.print(F("read myID: "));
	Serial.println(readID);

	if (myID != readID) 
		Serial.println(F("*** FAIL ***"));
	else 
		Serial.println(F("SUCCESS"));

	delay(1000);

	// We're done here - go to sleep
	set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
	sleep_enable();
	sleep_cpu ();  
}

void loop() 
{
	//Nothing to see here
}
