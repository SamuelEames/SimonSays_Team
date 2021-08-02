// Extra variables to go with Simon Game because apparently Arduino doesn't accept enum types in .ino files


// Game State
enum gameStates {ST_Lobby, ST_Intro, ST_SeqPlay, ST_SeqRec, ST_Correct, ST_Incorrect, ST_HighScore, ST_ShowScore};

/* GAME STATES

	* ST_Lobby: Idle state while waiting for players to initiate a game
	* ST_Intro: Light/Sound effect played when someone initiates a game by pressing a button
	* ST_SeqPlay: Plays 'Simon sequence' for players to remember lighting up buttons
	* ST_SeqRec: Players use buttons to input same sequence just shown in ST_SeqPlay
	* ST_Correct: Light/Sound effect played if players correctly entered current SeqPlay pattern
	* ST_Incorrect: Light/Sound effect played if players incorretly entered current SeqPlay pattern
	* ST_HighScore: If after ST_Incorrect players achieved a higher level than before, plays this effect 
	* ST_ShowScore: Shows score for a few seconds between ST_HighScore OR ST_Incorrect and ST_Lobby


*/