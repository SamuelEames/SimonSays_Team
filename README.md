
# Here's the deal
It's the electronic 'Simon Says' game where a pattern is played, and then users need to repeat it back perfectly. Each time they correctly replay it the pattern is extended by one step and replayed.


## Wiring Notes

'''
            ~ PRO MICRO ~

            O          RAW
            1          GND
            GND        RST
            GND        VCC
            2 SDA       21   RAND_SEED
            3 SCL       20   DISP_CS
    SSave   4           19   RF_CE
    Piezo   5           18   RF_CSN
     LEDs   6       SCK 15   RF_SCK,  DISP_SCK
    ADDR4   7      MISO 14   RF_MISO
    ADDR2   8      MOSI 16   RF_MOSI, DISP_MOSI
    ADDR1   9           10   BTN


'''


Notes
 * In wireless mode
   * Inputs 7-9 set device address (address 0 = master, max 7 devices)
   * Button colour is set by a devices address
   * Don't set two devices to same address
   * Master station polls wireless every 5 seconds in the 'Lobby' state to see which buttons are available.
      * In this state, LEDs indicate detected buttons
      * Won't allow game to start unil 4 or more buttons are present
      *
 * Input 10 used for game button
 * Input 20 should be left floating -- it's used to seed the random number generator


Packet Format

Master --> slaves
byte 1 -- game state
byte 2 -- variable according to state 

Slaves --> Master
byte 1 -- myID
byte 2 -- not really used -- 0xFF for button press, else 0x00



## Arduino Libraries used
 * FastLED     --> Pixel LED control
 * RF24        --> NRF24L01 radio control
 * MD_MAX72XX  --> Matrix Display


