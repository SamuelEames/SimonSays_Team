
# Here's the deal
It's the electronic 'Simon Says' game where a pattern is played, and then users need to repeat it back perfectly. Each time they correctly replay it the pattern is extended by one step and replayed.


## Wiring Notes

Stage 1 Setup (for testing game logic)
* 12x WS2812B LEDs on pin 9
* 4x buttons (red, yel, grn, blu) on pins 18-21
* Buzzer - TODO

Stage 2 (added NRF24L01 wireless comms)
* TODO

'''
            ~ PRO MICRO ~

            O          RAW
            1          GND
            GND        RST
            GND        VCC
            2 SDA       21   RAND_SEED
            3 SCL       20   DISP_CS
            4           19   RF_CE
    Piezo   5           18   RF_CSN
     LEDs   6       SCK 15   RF_SCK,  DISP_SCK
    BTN_B   7      MISO 14   RF_MISO
    BTN_G   8      MOSI 16   RF_MOSI, DISP_MOSI
    BTN_Y   9           10   BTN_R


'''


Notes
 * In wireless mode
   * Inputs 7-9 set device address (address 0 = master, max 7 devices)
   * Input 10 used for main button



## Arduino Libraries used
 * FastLED     --> Pixel LED control
 * RF24        --> NRF24L01 radio control
 * MD_MAX72XX  --> Matrix Display






#Stuff git added for me

usage: git [--version] [--help] [-C <path>] [-c <name>=<value>]
           [--exec-path[=<path>]] [--html-path] [--man-path] [--info-path]
           [-p | --paginate | -P | --no-pager] [--no-replace-objects] [--bare]
           [--git-dir=<path>] [--work-tree=<path>] [--namespace=<name>]
           <command> [<args>]

These are common Git commands used in various situations:

start a working area (see also: git help tutorial)
   clone     Clone a repository into a new directory
   init      Create an empty Git repository or reinitialize an existing one

work on the current change (see also: git help everyday)
   add       Add file contents to the index
   mv        Move or rename a file, a directory, or a symlink
   restore   Restore working tree files
   rm        Remove files from the working tree and from the index

examine the history and state (see also: git help revisions)
   bisect    Use binary search to find the commit that introduced a bug
   diff      Show changes between commits, commit and working tree, etc
   grep      Print lines matching a pattern
   log       Show commit logs
   show      Show various types of objects
   status    Show the working tree status

grow, mark and tweak your common history
   branch    List, create, or delete branches
   commit    Record changes to the repository
   merge     Join two or more development histories together
   rebase    Reapply commits on top of another base tip
   reset     Reset current HEAD to the specified state
   switch    Switch branches
   tag       Create, list, delete or verify a tag object signed with GPG

collaborate (see also: git help workflows)
   fetch     Download objects and refs from another repository
   pull      Fetch from and integrate with another repository or a local branch
   push      Update remote refs along with associated objects

'git help -a' and 'git help -g' list available subcommands and some
concept guides. See 'git help <command>' or 'git help <concept>'
to read about a specific subcommand or concept.
See 'git help git' for an overview of the system.
