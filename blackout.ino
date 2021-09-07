// DISPEL!
// (c) 2020 Scott Cartier

#define null 0
#define DONT_CARE 0

// Defines to enable debug code
#define RENDER_DEBUG_AND_ERRORS 0
#define USE_DATA_SPONGE         0
#define DEBUG_SETUP             0
#define DEBUG_AUTO_WIN          0
#define REDUNDANT_ROLE_CHECKS   0

#if USE_DATA_SPONGE
#warning DATA SPONGE ENABLED
byte sponge[108];
#endif
// SPONGE LOG
// 7/20: Reduced datagram size in blinklib (game doesn't use them anyway): >100
// 7/19/2021: 15 to init, 12 to complete a game
// 12/9: After new animations: 31 to init, 29 to complete a game
// 11/22: Setup & win animations: 25
// 11/21: Added an animation timer per-face instead of per-tile
// 11/20: Changed to Blackout! which removed all the tool patterns and difficulty structures: 70-75
// 11/3: 48, 39
// 11/10/2020: 25

void __attribute__((noinline)) showAnimation(byte animIndex, byte newAnimRate);
byte __attribute__((noinline)) randRange(byte min, byte max);
void __attribute__((noinline)) setSolidColorOnState(byte color, byte *state);
void __attribute__((noinline)) setYellowOnState(byte *state);
void __attribute__((noinline)) updateToolColor(byte color);
void __attribute__((noinline)) resetGame();
void __attribute__((noinline)) setupNextRainbowColor(byte value);
void __attribute__((noinline)) showAnimation_Init();
void __attribute__((noinline)) updateStateWithTools(byte *state);
void __attribute__((noinline)) showAnimation_Tool();


byte faceOffsetArray[] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 };

#define CCW_FROM_FACE(f, amt) faceOffsetArray[6 + (f) - (amt)]
#define CW_FROM_FACE(f, amt)  faceOffsetArray[(f) + (amt)]
#define OPPOSITE_FACE(f)      CW_FROM_FACE((f), 3)

#define TOGGLE_COMMAND 1
#define TOGGLE_DATA 0
struct FaceValue
{
  byte value : 4;
  byte toggle : 1;
  byte ack : 1;
};

struct FaceStateComm
{
  FaceValue faceValueOut;
  byte lastCommandIn;
  bool neighborPresent;
  bool debug;
};
FaceStateComm faceStatesComm[FACE_COUNT] =
{
  { { 0, TOGGLE_DATA, TOGGLE_DATA } },    // Initialize here so we don't need to do it in code during setup()
  { { 0, TOGGLE_DATA, TOGGLE_DATA } },
  { { 0, TOGGLE_DATA, TOGGLE_DATA } },
  { { 0, TOGGLE_DATA, TOGGLE_DATA } },
  { { 0, TOGGLE_DATA, TOGGLE_DATA } },
  { { 0, TOGGLE_DATA, TOGGLE_DATA } }
};

enum Command
{
  Command_None,         // no data

  // Setup
  Command_AssignRole,         // Working tile assigns roles to neighbors
  Command_AssignToolPattern,  // Working tile assigns patterns to Tool tiles

  // Play
  Command_RequestPattern,
  Command_RequestRotation,
  Command_RequestColor,
  Command_ToolPattern,
  Command_ToolRotation,
  Command_ToolColor,
  Command_ResetToolColor,
  
  Command_SetGameState,

  Command_PuzzleSolved,
  Command_Reset
};

void __attribute__((noinline)) enqueueCommOnFace(byte f, Command command, byte data);

struct CommandAndData
{
  Command command : 4;
  byte data : 4;
};

#define COMM_QUEUE_SIZE 8
CommandAndData commQueues[FACE_COUNT][COMM_QUEUE_SIZE];

#define COMM_INDEX_ERROR_OVERRUN 0xFF
#define COMM_INDEX_OUT_OF_SYNC   0xFE
byte commInsertionIndexes[FACE_COUNT] = { 0, 0, 0, 0, 0, 0 };

byte numNeighbors = 0;

#define ErrorOnFace(f) (commInsertionIndexes[f] > COMM_QUEUE_SIZE)

// -------------------------------------------------------------------------------------------------
// TILES
// -------------------------------------------------------------------------------------------------

enum TileRole
{
  TileRole_Unassigned,
  TileRole_Working,
  TileRole_Tool
};

TileRole tileRole = TileRole_Unassigned;

struct ToolState
{
  byte pattern;
  byte color;
  byte rotation;
};

#define TOOL_PATTERN_UNASSIGNED 0b1110    // unused pattern is our "unassigned" key value
#define COLOR_WHITE 3
ToolState assignedTool = { TOOL_PATTERN_UNASSIGNED, 0, 0 };

// Twelve possible patterns (not including all on & all off)
byte patterns[] =
{
  0b0000,   // 1
  0b0001,   // 2
  0b0010,
  0b0100,
  0b0011,   // 3
  0b0101,
  0b1010,
  0b1001,
  0b0111,   // 4
  0b1011,
  0b1101,
  0b1111    // 5
};

// -------------------------------------------------------------------------------------------------
// ANIMATION
// -------------------------------------------------------------------------------------------------

enum AnimCommand
{
  AnimCommand_SolidBase,
  AnimCommand_SolidOverlay,
  AnimCommand_SolidWithOverlay,
  AnimCommand_BaseAndOverlay,
  AnimCommand_LerpBaseToOverlay,
  AnimCommand_LerpOverlayToBase,
  AnimCommand_LerpBaseToBaseHalf,
  AnimCommand_LerpBaseHalfToBase,
  AnimCommand_LerpOverlayIfNonZeroToBase,
  AnimCommand_LerpBaseToOverlayIfNonZero,
  AnimCommand_Pause,
  AnimCommand_PauseHalf,
  AnimCommand_FadeInBase,
  AnimCommand_FadeOutBase,
  AnimCommand_FadeOutBaseIfOverlayR,
  AnimCommand_RandomRotateBaseAndOverlayR,
  AnimCommand_RandomToolOnBase,
  
  AnimCommand_Loop,
  AnimCommand_Done
};

// Animation sequences
// Each set of commands defines what colors are shown on each face and in what order
AnimCommand animSequences[] =
{
  // CONSTANT COLOR - BASE ONLY
  // Used: Show patterns on rune tiles
  AnimCommand_SolidBase,
  AnimCommand_Loop,

  // OVERLAY TO BASE - one shot
  // Used: 1. When starting the game - fade in rune tiles from solid yellow
  //       2. Rainbowpalooza (uses delay on outer half)
  AnimCommand_SolidOverlay,
  AnimCommand_PauseHalf,
  AnimCommand_LerpOverlayToBase,
  AnimCommand_SolidBase,
  AnimCommand_Done,

  // PUZZLE STATE CHANGED - one shot
  // Used: When a rune tile changes and the puzzle updates - flashes white and fades to new state
  AnimCommand_SolidWithOverlay,
  AnimCommand_LerpOverlayIfNonZeroToBase,
  AnimCommand_SolidBase,
  AnimCommand_Done,

  // INIT
  // Used: Fireworks init - random color+face fading out
  AnimCommand_RandomRotateBaseAndOverlayR,
  AnimCommand_FadeOutBaseIfOverlayR,
  AnimCommand_Loop,

  // BASE + PULSE OVERLAY
  // Used: During setup on puzzle tile to pulse the difficulty wedges
  AnimCommand_LerpBaseToOverlayIfNonZero,
  AnimCommand_LerpOverlayIfNonZeroToBase,
  AnimCommand_Loop,

  // TOOL
  // Used: During setup on rune tiles to show random tools fading in+out
  AnimCommand_RandomToolOnBase,
  AnimCommand_FadeInBase,
  AnimCommand_FadeOutBase,
  AnimCommand_Loop,

  // WORKING PULSE
  // Used: While idle, the working tile will play this anim to differentiate itself from the tool tiles
  AnimCommand_LerpBaseToBaseHalf,
  AnimCommand_LerpBaseHalfToBase,
  AnimCommand_SolidBase,
  AnimCommand_Done,
};

#define ANIM_SEQ_INDEX__BASE                    0
#define ANIM_SEQ_INDEX__OVERLAY_TO_BASE_DELAYED (ANIM_SEQ_INDEX__BASE+2)
#define ANIM_SEQ_INDEX__OVERLAY_TO_BASE         (ANIM_SEQ_INDEX__OVERLAY_TO_BASE_DELAYED+2)
#define ANIM_SEQ_INDEX__STATE_CHANGED           (ANIM_SEQ_INDEX__OVERLAY_TO_BASE_DELAYED+5)
#define ANIM_SEQ_INDEX__INIT                    (ANIM_SEQ_INDEX__STATE_CHANGED+4)
#define ANIM_SEQ_INDEX__BASE_PULSE_OVERLAY      (ANIM_SEQ_INDEX__INIT+3)
#define ANIM_SEQ_INDEX__TOOL_SETUP              (ANIM_SEQ_INDEX__BASE_PULSE_OVERLAY+3)
#define ANIM_SEQ_INDEX__WORKING_PULSE           (ANIM_SEQ_INDEX__TOOL_SETUP+4)

// Shift by 2 so it fits in a byte
#define ANIM_RATE_SLOW (1000>>2)
#define ANIM_RATE_FAST (300>>2)

#define WORKING_PULSE_RATE 1000
Timer workingPulseTimer;

// Rainbow order bitwise 1=b001=R, 3=b011=R+G, 2=b010=G, 6=b110=G+B, 4=b100=B, 5=b101=B+R
byte rainbowSequence[] = { 1, 3, 2, 6, 4, 5 };
byte rainbowIndex = 0;

// -------------------------------------------------------------------------------------------------
// GAME
// -------------------------------------------------------------------------------------------------

enum GameState
{
  GameState_Init,     // tiles were just programmed - waiting for click of Target tile
  GameState_Setup,    // let player choose number of tool tiles and difficulty
  GameState_Play,     // gameplay
  GameState_Done      // player matched the Target tile
};

GameState gameState = GameState_Init;
byte numToolTiles = 0;

#define NUM_DIFFICULTY_LEVELS 6
byte difficulty = 0;

uint32_t startingSeed = 0;

byte rootFace = 0;  // so Tool tiles to know which face is the Target tile

struct FaceStateGame
{
  byte animIndexStart;
  byte animIndexCur;
  byte animRateDiv4;            // lerp/pause rate
  Timer animTimer;              // timer used for lerps and pauses during animation
  bool animDone;

  uint16_t savedColor;          // used during pauses in animation

  // Used during setup
  ToolState neighborTool;
};
FaceStateGame faceStatesGame[FACE_COUNT];

// Store our current puzzle state
byte startingState[3];        // how the puzzle starts
byte colorState[3];           // the current state displayed
byte overlayState[3];         // used for animations or to hide things

#define RESET_TIMER_DELAY 3000
Timer resetTimer;             // prevents cyclic resets

uint32_t randState;

#if DEBUG_AUTO_WIN
bool autoWin = false;
#endif

// =================================================================================================
//
// SETUP
//
// =================================================================================================

#include <avr/io.h>

void setup()
{


  // Temporary random seed is generated detiministically from our tile's serial number

  uint32_t serial_num_32 = 2463534242UL;    // We start with  Marsaglia's seed...

  // ... and then fold in bits form our serial number
  
  for( byte i=0; i< SERIAL_NUMBER_LEN; i++ ) {

    serial_num_32 ^=  getSerialNumberByte(i) << (i * 3);

  }

  randState=serial_num_32;
 
#if USE_DATA_SPONGE
  // Use our data sponge so that it isn't compiled away
  if (sponge[0])
  {
    sponge[0] = 3;
  }
#endif

/*
  FOREACH_FACE(f)
  {
    resetCommOnFace(f);
  }
*/

  //setColor(GREEN);
  showAnimation_Init();
}

// =================================================================================================
//
// LOOP
//
// =================================================================================================

void loop()
{

  // Detect button clicks
  handleUserInput();

  // Update neighbor presence and comms
  updateCommOnFaces();

  switch (gameState)
  {
    case GameState_Init: break; // do nothing - waiting for click in handleUserInput()
    case GameState_Setup:
      if (tileRole == TileRole_Working)
      {
        setupWorking();
      }
      else
      {
        setupTool();
      }
      break;
    case GameState_Play:  playWorking();  break;
    case GameState_Done:  doneWorking();  break;
  }


  render();

  // Call hasWoken() explicitly to clear the flag
  hasWoken();
}

// =================================================================================================
//
// INPUT
//
// =================================================================================================

void __attribute__((noinline)) updateDifficulty(byte newDifficulty)
{
  difficulty = newDifficulty;
  if (difficulty >= NUM_DIFFICULTY_LEVELS)
  {
    difficulty = 0;
  }

  byte difficultyOverlay = ~(0xFE << difficulty);
  overlayState[0] = overlayState[1] = difficultyOverlay; overlayState[2] = 0x0;
  
  generateToolsAndPuzzle();
}

void handleUserInput()
{
#if DEBUG_AUTO_WIN
  if (buttonMultiClicked())
  {
    byte clicks = buttonClickCount();
    if (clicks == 5)
    {
      if (tileRole == TileRole_Working)
      {
        autoWin = true;
      }
    }
  }
#endif

  if (buttonDoubleClicked())
  {
    if (gameState == GameState_Setup && tileRole == TileRole_Working && numNeighbors > 0)
    {
      // Start the game
      gameState = GameState_Play;
      resetWorkingPulseTimer();
      FOREACH_FACE(f)
      {
        if (faceStatesComm[f].neighborPresent)
        {
          enqueueCommOnFace(f, Command_SetGameState, GameState_Play);
        }
      }
      setYellowOnState(overlayState);
      showAnimation(ANIM_SEQ_INDEX__OVERLAY_TO_BASE, ANIM_RATE_FAST);

      updateWorkingState();
    }
    else if (gameState == GameState_Play && tileRole == TileRole_Working)
    {
      // Double clicking the Working tile during a game will reset all surrounding tool tiles
      FOREACH_FACE(f)
      {
        faceStatesGame[f].neighborTool.color = COLOR_WHITE;
        enqueueCommOnFace(f, Command_ResetToolColor, DONT_CARE);
      }
      updateWorkingState();
    }
    else if (gameState == GameState_Play && tileRole == TileRole_Tool)
    {
      // Double clicking a Tool tile during a game will reset it back to white
      updateToolColor(COLOR_WHITE);
    }
  }

  if (buttonSingleClicked() && !hasWoken())
  {
    switch (gameState)
    {
      case GameState_Init:
        // Clicking a tile during init changes it to the Working tile and let's the player
        // select the difficulty and add/remove tool tiles
        tileRole = TileRole_Working;
        gameState = GameState_Setup;

        setSolidColorOnState(0b100, colorState);
        showAnimation(ANIM_SEQ_INDEX__BASE_PULSE_OVERLAY, ANIM_RATE_FAST);

        updateDifficulty(difficulty); // called to update the faces to show the current difficulty

        // Clear this so the Working tile will re-detect
        numToolTiles = 0;
  
        // Button clicking provides our entropy for the initial random seed
        randState = millis();
        generateNewStartingSeed();

#if DEBUG_AUTO_WIN
        autoWin = false;
#endif
        break;

      case GameState_Setup:
        // Clicking the Working tile during setup changes the difficulty
        if (tileRole == TileRole_Working)
        {
          updateDifficulty(difficulty + 1);
        }
        break;

      case GameState_Play:
        // Clicking a Tool tile during the game cycles through Red/Green/Blue/White
        if (tileRole == TileRole_Tool)
        {
          updateToolColor((assignedTool.color == COLOR_WHITE) ? 0 : (assignedTool.color + 1));
        }
        break;

      case GameState_Done:
        // Clicking any tile after winning will reset so the player can play again
        resetGame();
        break;
    }
  }

  // No matter what state we're in, long pressing resets the game and other connected tiles
  if (buttonLongPressed())
  {
    resetGame();
  }
}

// Change the assigned color for the tool and inform the Working tile
void __attribute__((noinline)) updateToolColor(byte color)
{
  assignedTool.color = color;
  showAnimation_Tool();
  enqueueCommOnFace(rootFace, Command_ToolColor, assignedTool.color);
}

void __attribute__((noinline)) resetTileState()
{
  gameState = GameState_Init;
  tileRole = TileRole_Unassigned;
  assignedTool = { TOOL_PATTERN_UNASSIGNED, 0 };
  showAnimation_Init();
}

// Reset this tile and tell all connected tiles to reset
// This gets propagated to neighbors, using a timer to prevent infinite loops
void __attribute__((noinline)) resetGame()
{
  if (!resetTimer.isExpired())
  {
    return;
  }

  resetTimer.set(RESET_TIMER_DELAY);

  resetTileState();
  
  // Propagate the reset out to the cluster
  FOREACH_FACE(f)
  {
    enqueueCommOnFace(f, Command_Reset, DONT_CARE);
  }
}

byte __attribute__((noinline)) rotateSixBits(byte inByte, byte rotateAmount)
{
  return ((inByte << rotateAmount) | (inByte >> (6 - rotateAmount))) & 0x3F;
}

// =================================================================================================
//
// COMMUNICATIONS
//
// =================================================================================================

void __attribute__((noinline)) sendValueOnFace(byte f, FaceValue faceValue)
{
  byte outVal = *((byte*)&faceValue);
  setValueSentOnFace(outVal, f);
}

void resetCommOnFace(byte f)
{
  // Clear the queue
  commInsertionIndexes[f] = 0;

  // Put the current output into its reset state.
  // In this case, all zeroes works for us.
  // Assuming the neighbor is also reset, it means our ACK == their TOGGLE.
  // This allows the next pair to be sent immediately.
  // Also, since the toggle bit is set to TOGGLE_DATA, it will toggle into TOGGLE_COMMAND,
  // which is what we need to start sending a new pair.
  faceStatesComm[f].faceValueOut.value = 0;
  faceStatesComm[f].faceValueOut.toggle = TOGGLE_DATA;
  faceStatesComm[f].faceValueOut.ack = TOGGLE_DATA;
  sendValueOnFace(f, faceStatesComm[f].faceValueOut);
}

// Called by the main program when this tile needs to tell something to
// a neighbor tile.
void __attribute__((noinline)) enqueueCommOnFace(byte f, Command command, byte data)
{
  // Check here so callers don't all need to check
  if (!faceStatesComm[f].neighborPresent)
  {
    return;
  }

  if (commInsertionIndexes[f] >= COMM_QUEUE_SIZE)
  {
    // Buffer overrun - might need to increase queue size to accommodate
//    commInsertionIndexes[f] = COMM_INDEX_ERROR_OVERRUN;
    return;
  }

  byte index = commInsertionIndexes[f];
  commQueues[f][index].command = command;
  commQueues[f][index].data = data;
  commInsertionIndexes[f]++;
}

// Called every iteration of loop(), preferably before any main processing
// so that we can act on any new data being sent.
void updateCommOnFaces()
{
  numNeighbors = 0;
  FOREACH_FACE(f)
  {
    FaceStateComm *faceStateComm = &faceStatesComm[f];

    // Is the neighbor still there?
    if (isValueReceivedOnFaceExpired(f))
    {
      // Lost the neighbor - go back into reset on this face
      faceStateComm->neighborPresent = false;
      resetCommOnFace(f);
      continue;
    }

    faceStateComm->neighborPresent = true;
    numNeighbors++;

    // If there is any kind of error on the face then do nothing
    // The error can be reset by removing the neighbor
    if (ErrorOnFace(f))
    {
      continue;
    }

    // Read the neighbor's face value it is sending to us
    byte val = getLastValueReceivedOnFace(f);
    FaceValue faceValueIn = *((FaceValue*)&val);

    //
    // RECEIVE
    //

    // Did the neighbor send a new comm?
    // Recognize this when their TOGGLE bit changed from the last value we got.
    if (faceStateComm->faceValueOut.ack != faceValueIn.toggle)
    {
      // Got a new comm - process it
      byte value = faceValueIn.value;
      if (faceValueIn.toggle == TOGGLE_COMMAND)
      {
        // This is the first part of a comm (COMMAND)
        // Save the command value until we get the data
        faceStateComm->lastCommandIn = (Command) value;
      }
      else
      {
        // This is the second part of a comm (DATA)
        // Use the saved command value to determine what to do with the data
        processCommForFace((Command) faceStateComm->lastCommandIn, value, f);
      }

      // Acknowledge that we processed this value so the neighbor can send the next one
      faceStateComm->faceValueOut.ack = faceValueIn.toggle;
    }

    //
    // SEND
    //

    // Did the neighbor acknowledge our last comm?
    // Recognize this when their ACK bit equals our current TOGGLE bit.
    if (faceValueIn.ack == faceStateComm->faceValueOut.toggle)
    {
      // If we just sent the DATA half of the previous comm, check if there
      // are any more commands to send.
      if (faceStateComm->faceValueOut.toggle == TOGGLE_DATA)
      {
        if (commInsertionIndexes[f] == 0)
        {
          // Nope, no more comms to send - bail and wait
          continue;
        }
      }

      // Send the next value, either COMMAND or DATA depending on the toggle bit

      // Toggle between command and data
      faceStateComm->faceValueOut.toggle = ~faceStateComm->faceValueOut.toggle;

      // Grab the first element in the queue - we'll need it either way
      CommandAndData commandAndData = commQueues[f][0];

      // Send either the command or data depending on the toggle bit
      if (faceStateComm->faceValueOut.toggle == TOGGLE_COMMAND)
      {
        faceStateComm->faceValueOut.value = commandAndData.command;
      }
      else
      {
        faceStateComm->faceValueOut.value = commandAndData.data;

        // No longer need this comm - shift everything towards the front of the queue
        for (byte commIndex = 1; commIndex < COMM_QUEUE_SIZE; commIndex++)
        {
          commQueues[f][commIndex - 1] = commQueues[f][commIndex];
        }

        // Adjust the insertion index since we just shifted the queue
        /*
        if (commInsertionIndexes[f] == 0)
        {
          // Shouldn't get here - if so something is funky
          commInsertionIndexes[f] = COMM_INDEX_OUT_OF_SYNC;
          continue;
        }
        else
        */
        {
          commInsertionIndexes[f]--;
        }
      }
    }
  }

  FOREACH_FACE(f)
  {
    // Update the value sent in case anything changed
    sendValueOnFace(f, faceStatesComm[f].faceValueOut);
  }
}

void processCommForFace(Command command, byte value, byte f)
{
  //FaceStateComm *faceStateComm = &faceStatesComm[f];

  //byte oppositeFace = OPPOSITE_FACE(f);

  //faceStateComm->debug = true;
  switch (command)
  {
    case Command_AssignRole:
      // Grab our new role
      tileRole = (TileRole) value;
      rootFace = f;
      gameState = GameState_Setup;
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Tool)
#endif
      {
        showAnimation(ANIM_SEQ_INDEX__TOOL_SETUP, ANIM_RATE_SLOW);
      }
      break;

    case Command_AssignToolPattern:
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Tool)
#endif
      {
        assignedTool.pattern = value;
        assignedTool.color = COLOR_WHITE;
#if DEBUG_SETUP
        showAnimation_Tool();
#endif
        // Send our rotation in case Working tile doesn't have it
        enqueueCommOnFace(f, Command_ToolRotation, f);
      }
      break;
      
    case Command_SetGameState:
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Tool)
#endif
      {
        gameState = (GameState) value;
        if (gameState == GameState_Play)
        {
          showAnimation_Tool();
          
          setYellowOnState(overlayState);
          showAnimation_BurstOutward(f);
        }
      }
      break;

    case Command_RequestPattern:
      // Only the working tile can request Tool info - update the root face
      rootFace = f;
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Tool)
#endif
      {
        enqueueCommOnFace(f, Command_ToolPattern, assignedTool.pattern);
      }
      break;
      
    case Command_RequestRotation:
      // Only the working tile can request Tool info - update the root face
      rootFace = f;
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Tool)
#endif
      {
        // We send the face that received this comm so the requester can compute our relative rotation
        enqueueCommOnFace(f, Command_ToolRotation, f);
      }
      break;

    case Command_RequestColor:
      // Only the working tile can request Tool info - update the root face
      rootFace = f;
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Tool)
#endif
      {
        enqueueCommOnFace(f, Command_ToolColor, assignedTool.color);
      }
      break;

    case Command_ToolPattern:
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Working)
#endif
      {
        faceStatesGame[f].neighborTool.pattern = value;
        // Got the pattern - now request the rotation
        enqueueCommOnFace(f, Command_RequestRotation, DONT_CARE);
      }
      break;

    case Command_ToolRotation:
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Working)
#endif
      {
        // Figure out how the Tool tile is rotated relative to the Working tile
        byte oppositeFace = OPPOSITE_FACE(value);
        faceStatesGame[f].neighborTool.rotation = CCW_FROM_FACE(f, oppositeFace);
        if (gameState == GameState_Play)
        {
          // Got the rotation - now request the color
          enqueueCommOnFace(f, Command_RequestColor, DONT_CARE);
        }
      }
      break;
      
    case Command_ToolColor:
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Working)
#endif
      {
        faceStatesGame[f].neighborTool.color = value;
        updateWorkingState();
      }
      break;

    case Command_ResetToolColor:
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Tool)
#endif
      {
        updateToolColor(COLOR_WHITE);
      }
      break;

    case Command_PuzzleSolved:
#if REDUNDANT_ROLE_CHECKS
      if (tileRole == TileRole_Tool)
#endif
      {
        // BUGFIX : While doing the winning rainbow burst, resetting the tiles by clicking a rune
        //          tile had a race condition where the hex tile would send out one last 'PuzzleSolved'
        //          message before resetting, causing the tile you just clicked to go back to
        //          rainbow. Now, once you reset the game on a tile, it cannot be considered
        //          solved again.
        if (gameState != GameState_Init)
        {
          gameState = GameState_Done;
          setupNextRainbowColor(value);
          showAnimation_BurstOutward(f);
        }
      }
      break;
      
    case Command_Reset:
      resetGame();
      break;

    case Command_None:
      /* Empty */
      break;      
  }
}

// =================================================================================================
//
// GAME STATE: SETUP
// Let the player remove/attach Tool tiles, select difficulty, set a random seed
//
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// WORKING
// -------------------------------------------------------------------------------------------------

void setupWorking()
{
  // Check if a neighbor tile was added or removed
  if (numNeighbors != numToolTiles)
  {
    numToolTiles = numNeighbors;
    
    // Generate the tools and the puzzle based on them
    if (numNeighbors > 1)
    {
      generateToolsAndPuzzle();
    }
  }
}

void generateToolsAndPuzzle()
{
  // Generate a puzzle based on the difficulty, number of tool tiles, and random seed

  // Nibble 4 (19:16) represents sixteen different puzzles using the same tools
  // It needs to be part of the starting seed so it can be shown to the player,
  // but we don't want it to factor into the seed for tool selection because tools
  // shouldn't change based on it.
  // FUTURE SCOTT NOTES: This is for the non-existing feature of sharing starting seeds

/*
  uint32_t puzzleSeed = startingSeed;
  puzzleSeed |= (uint32_t) numToolTiles << 24;
  puzzleSeed |= (uint32_t) difficulty << 28;
*/

  byte toolSlotIndex = randRange(0, 6 - numToolTiles + 2);
  FOREACH_FACE(f)
  {
    if (!faceStatesComm[f].neighborPresent)
    {
      continue;
    }

    // Choose the tools sequentially, but start from a random tool if less than six neighbors
    faceStatesGame[f].neighborTool.pattern = patterns[difficulty+toolSlotIndex];

    // Make it a Tool tile and assign its pattern
    enqueueCommOnFace(f, Command_AssignRole, TileRole_Tool);
    enqueueCommOnFace(f, Command_AssignToolPattern, faceStatesGame[f].neighborTool.pattern);

    toolSlotIndex++;
  }

  // Now that we're generating the actual puzzle, use a seed that includes [19:16]
//  randState = puzzleSeed;

  // Fill up the starting state with our tools
  startingState[0] = startingState[1] = startingState[2] = 0;
  byte randColor = randRange(0, 3);   // randomly start at R G or B
  FOREACH_FACE(f)
  {
    if (faceStatesComm[f].neighborPresent)
    {
      faceStatesGame[f].neighborTool.color = randColor;
      faceStatesGame[f].neighborTool.rotation = randRange(0, FACE_COUNT);

      // Go sequentially through RGB
      // Could just select a random color every time, but that might result in some puzzles being all one color
      randColor = (randColor == 2) ? 0 : (randColor + 1);
    }
  }
  updateStateWithTools(startingState);

  // Clear Tool colors at start
  FOREACH_FACE(f)
  {
    if (faceStatesComm[f].neighborPresent)
    {
      faceStatesGame[f].neighborTool.color = COLOR_WHITE;
    }
  }
  
#if DEBUG_SETUP
  colorState[0] = startingState[0];
  colorState[1] = startingState[1];
  colorState[2] = startingState[2];
  showAnimation(ANIM_SEQ_INDEX__BASE, DONT_CARE);
#endif
}

void updateWorkingState()
{
  byte prevStateR = colorState[0];
  byte prevStateG = colorState[1];
  byte prevStateB = colorState[2];

  colorState[0] = startingState[0];
  colorState[1] = startingState[1];
  colorState[2] = startingState[2];

  updateStateWithTools(colorState);

  // Determine which faces changed
  byte changedFaces = (prevStateR ^ colorState[0]) | (prevStateG ^ colorState[1]) | (prevStateB ^ colorState[2]);

  // Pulse the changed faces
  overlayState[0] = overlayState[1] = overlayState[2] = changedFaces;
  showAnimation(ANIM_SEQ_INDEX__STATE_CHANGED, ANIM_RATE_SLOW);

  // Reset the pulse timer so it doesn't try to interrupt this anim
  resetWorkingPulseTimer();
}

void __attribute__((noinline)) updateStateWithTools(byte *state)
{
  // Apply all the attached tools
  FOREACH_FACE(f)
  {
    if (faceStatesComm[f].neighborPresent)
    {
      ToolState tool = faceStatesGame[f].neighborTool;
      if (tool.pattern != TOOL_PATTERN_UNASSIGNED && tool.color != COLOR_WHITE)
      {
        byte toolMask = 0x1 | (tool.pattern << 1);
        
        // Apply rotation
        toolMask = rotateSixBits(toolMask, tool.rotation);
      
        state[tool.color] ^= toolMask;
      }
    }
  }
}

// -------------------------------------------------------------------------------------------------
// TOOL & TARGET
// -------------------------------------------------------------------------------------------------

void setupTool()
{
  // If we disconnected from the Target tile then clear our role
  if (!faceStatesComm[rootFace].neighborPresent)
  {
    resetTileState();
  }
}

// =================================================================================================
//
// GAME STATE: PLAY
//
// =================================================================================================

void playWorking()
{
  if (tileRole != TileRole_Working)
  {
    return;
  }
  
  // Check for a Tool added or removed
  if (numNeighbors != numToolTiles)
  {
    numToolTiles = numNeighbors;

    FOREACH_FACE(f)
    {
      if (faceStatesComm[f].neighborPresent)
      {
        if (faceStatesGame[f].neighborTool.pattern == TOOL_PATTERN_UNASSIGNED)
        {
          // Give the tool a pattern so that it doesn't fall in here again, but make it WHITE so it doesn't get drawn yet
          faceStatesGame[f].neighborTool.pattern = 0;
          faceStatesGame[f].neighborTool.color = COLOR_WHITE;

          // Start requesting the Tool info - pattern, rotation, color
          enqueueCommOnFace(f, Command_RequestPattern, DONT_CARE);
        }
      }
      else
      {
        // No neighbor on this face - clear its pattern so that it doesn't influence the puzzle
        faceStatesGame[f].neighborTool.pattern = TOOL_PATTERN_UNASSIGNED;
      }
    }

    // Update the color state
    updateWorkingState();
  }

  // Check if the faces should pulse
  if (workingPulseTimer.isExpired())
  {
    setSolidColorOnState(0b111, overlayState);
    showAnimation(ANIM_SEQ_INDEX__WORKING_PULSE, ANIM_RATE_FAST);
    resetWorkingPulseTimer();
  }

#if DEBUG_AUTO_WIN
  if (autoWin)
  {
    autoWin = false;
    setSolidColorOnState(0b000, colorState);
  }
#endif
  
  // Check for win state
  if (colorState[0] == 0 && colorState[1] == 0 && colorState[2] == 0)
  {
    // TOTAL BLACKOUT - PUZZLE SOLVED
    gameState = GameState_Done;
    rainbowIndex = 0;
  }
}

void resetWorkingPulseTimer()
{
  workingPulseTimer.set(WORKING_PULSE_RATE);
}

// =================================================================================================
//
// GAME STATE: DONE
//
// =================================================================================================

void doneWorking()
{
  if (tileRole != TileRole_Working)
  {
    return;
  }
  
  // CELEBRATORY ANIMATION!
  FOREACH_FACE(f)
  {
    // Wait for all faces to finish their anims
    if (!faceStatesGame[f].animDone)
    {
      return;
    }
  }

  // Start our next color and tell neighbors to start theirs
  setupNextRainbowColor(rainbowSequence[rainbowIndex]);
  showAnimation(ANIM_SEQ_INDEX__OVERLAY_TO_BASE, ANIM_RATE_FAST);
  FOREACH_FACE(f)
  {
    enqueueCommOnFace(f, Command_PuzzleSolved, rainbowSequence[rainbowIndex]);
  }
  rainbowIndex++;
  if (rainbowIndex > 5)
  {
    rainbowIndex = 0;
  }
}

void __attribute__((noinline)) setupNextRainbowColor(byte value)
{
  overlayState[0] = colorState[0];
  overlayState[1] = colorState[1];
  overlayState[2] = colorState[2];
  
  setSolidColorOnState(value, colorState);
}

// =================================================================================================
//
// RANDOM
//
// =================================================================================================

// Random code partially copied from blinklib because there's no function
// to set an explicit seed, which we need so we can share/replay puzzles.
byte __attribute__((noinline)) randGetByte()
{
  // Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
  uint32_t x = randState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  randState = x;
  return x & 0xFF;
}

byte __attribute__((noinline)) randRange(byte min, byte max)
{
  uint32_t val = randGetByte();
  uint32_t range = max - min;
  val = (val * range) >> 8;
  return val + min;
}

void generateNewStartingSeed()
{
  randGetByte();  // make the randomizer take a step

  // We encode the starting seed as color patterns on 5 of the 6 faces
  // That way the player can copy the seed and post it to share or save it to try again
  // NOTE: Seed sharing is not implemented due to space limitations
  startingSeed = randState & 0x000FFFFF;
}

// =================================================================================================
//
// ANIMATIONS
//
// =================================================================================================

void __attribute__((noinline)) showAnimation_Init()
{
  showAnimation(ANIM_SEQ_INDEX__INIT, ANIM_RATE_FAST * 2 + randRange(0, 100));
}

void showAnimation_BurstOutward(byte f)
{
  showAnimation(ANIM_SEQ_INDEX__OVERLAY_TO_BASE, ANIM_RATE_FAST);
  
  FOREACH_FACE(offset)
  {
    if (offset >= 2 && offset <= 4)
    {
      byte face = CW_FROM_FACE(f, offset);
      faceStatesGame[face].animIndexCur -= 2;
    }
  }
}

void __attribute__((noinline)) showAnimation_Tool()
{
  // Create the mask corresponding to the pattern
  byte mask = 0x1 | (assignedTool.pattern << 1);

  if (assignedTool.color == COLOR_WHITE)
  {
    colorState[0] = colorState[1] = colorState[2] = mask;
  }
  else
  {
    setSolidColorOnState(0b000, colorState);
    colorState[assignedTool.color] = mask;
  }
  showAnimation(ANIM_SEQ_INDEX__BASE, DONT_CARE);
}

void __attribute__((noinline)) showAnimation(byte animIndex, byte newAnimRate)
{
  FOREACH_FACE(f)
  {
    faceStatesGame[f].animIndexStart = faceStatesGame[f].animIndexCur = animIndex;
    faceStatesGame[f].animRateDiv4 = newAnimRate;
    faceStatesGame[f].animTimer.set(faceStatesGame[f].animRateDiv4 << 2);
    faceStatesGame[f].animDone = false;
  }
}

// =================================================================================================
//
// RENDER
//
// =================================================================================================

byte __attribute__((noinline)) lerpColor(byte in1, byte in2, byte t)
{
  uint32_t temp;
  temp = in1 * (128 - t) + in2 * t;
  return temp >> 7;
}

byte __attribute__((noinline)) getColorFromState(byte state, byte bitOffset)
{
  byte colorBit = state >> bitOffset;
  char colorByte = colorBit << 7;
  colorByte >>= 7;  // use sign extension to fill with 0s or 1s
  return colorByte;
}

void __attribute__((noinline)) setSolidColorOnState(byte color, byte *state)
{
  state[0] = (color & 0x1) ? 0x3F : 0;
  state[1] = (color & 0x2) ? 0x3F : 0;
  state[2] = (color & 0x4) ? 0x3F : 0;
}

void __attribute__((noinline)) setYellowOnState(byte *state)
{
  setSolidColorOnState(0b011, state);
}

void renderAnimationStateOnFace(byte f)
{
  FaceStateGame *faceStateGame = &faceStatesGame[f];

  byte r = getColorFromState(colorState[0], f);
  byte g = getColorFromState(colorState[1], f);
  byte b = getColorFromState(colorState[2], f);
  
  byte overlayR = getColorFromState(overlayState[0], f);
  byte overlayG = getColorFromState(overlayState[1], f);
  byte overlayB = getColorFromState(overlayState[2], f);
  bool overlayNonZero = overlayR | overlayG | overlayB;
  bool hasOverlay = (overlayState[0] | overlayState[1] | overlayState[2]) & (1<<f);
  
  byte colorRGB[3];
  byte paused = false;  
  bool startNextCommand = faceStateGame->animTimer.isExpired();
  uint32_t animRate32 = (uint32_t) faceStateGame->animRateDiv4 << 2;
  uint32_t t = (128 * (animRate32 - faceStateGame->animTimer.getRemaining())) / animRate32;  // 128 = 1.0
  
  AnimCommand animCommand = animSequences[faceStateGame->animIndexCur];

  if (animCommand == AnimCommand_LerpBaseHalfToBase ||
      animCommand == AnimCommand_LerpBaseToBaseHalf)
  {
    overlayR = (r >> 1) + (r >> 2);
    overlayG = (g >> 1) + (g >> 2);
    overlayB = (b >> 1) + (b >> 2);
  }

  switch (animCommand)
  {
    case AnimCommand_SolidBase:
      colorRGB[0] = r;
      colorRGB[1] = g;
      colorRGB[2] = b;
      startNextCommand = true;
      break;

    case AnimCommand_SolidOverlay:
      colorRGB[0] = overlayR;
      colorRGB[1] = overlayG;
      colorRGB[2] = overlayB;
      startNextCommand = true;
      break;

    case AnimCommand_SolidWithOverlay:
      colorRGB[0] = hasOverlay ? overlayR : r;
      colorRGB[1] = hasOverlay ? overlayG : g;
      colorRGB[2] = hasOverlay ? overlayB : b;
      startNextCommand = true;
      break;

    case AnimCommand_LerpOverlayIfNonZeroToBase:
      /* fall through */    
    case AnimCommand_LerpOverlayToBase:
      /* fall through */
    case AnimCommand_LerpBaseHalfToBase:
      t = 128 - t;
      /* fall through */
    case AnimCommand_LerpBaseToOverlayIfNonZero:
      /* fall through */
    case AnimCommand_LerpBaseToOverlay:
      /* fall through */
    case AnimCommand_LerpBaseToBaseHalf:
      colorRGB[0] = lerpColor(r, overlayR, t);
      colorRGB[1] = lerpColor(g, overlayG, t);
      colorRGB[2] = lerpColor(b, overlayB, t);

      if (animCommand == AnimCommand_LerpOverlayIfNonZeroToBase ||
          animCommand == AnimCommand_LerpBaseToOverlayIfNonZero)
      {
        if (!overlayNonZero)
        {
          colorRGB[0] = r;
          colorRGB[1] = g;
          colorRGB[2] = b;
        }
      }
      break;

    case AnimCommand_Pause:
    case AnimCommand_PauseHalf:
      paused = true;
      break;

    case AnimCommand_FadeInBase:
      t = 128 - t;
      /* fall through */
    case AnimCommand_FadeOutBase:
      // Force the code below to do the lerp
      overlayR = 1;
      /* fall through */
    case AnimCommand_FadeOutBaseIfOverlayR:
      colorRGB[0] = colorRGB[1] = colorRGB[2] = 0;
      if (overlayR)
      {
        colorRGB[0] = lerpColor(r, 0, t);
        colorRGB[1] = lerpColor(g, 0, t);
        colorRGB[2] = lerpColor(b, 0, t);
      }
      break;
      
    case AnimCommand_RandomRotateBaseAndOverlayR:
      colorState[0] = randGetByte();
      colorState[1] = randGetByte();
      colorState[2] = randGetByte();
      overlayState[0] = 1 << randRange(0, 6);
      
      paused = true;
      startNextCommand = true;
      break;

    case AnimCommand_RandomToolOnBase: {
      // TODO : Change to only show tools appropriate to the selected difficulty
      byte randByte = randGetByte() | 0x2;
      byte toolPattern = (randByte & 0x3E) >> (randByte & 0x1);
      colorState[0] = colorState[1] = colorState[2] = toolPattern;
      
      paused = true;
      startNextCommand = true;
      }
      break;

    case AnimCommand_BaseAndOverlay:
      /* Empty */
      break;

    case AnimCommand_Loop:
      /* Empty */
      break;

    case AnimCommand_Done:
      /* Empty */
      break;
      
  }

  Color color = makeColorRGB(colorRGB[0], colorRGB[1], colorRGB[2]);
  if (paused)
  {
    color.as_uint16 = faceStateGame->savedColor;
  }
  faceStateGame->savedColor = color.as_uint16;

  
  setColorOnFace(color, f);

  if (startNextCommand)
  {
    faceStateGame->animIndexCur++;

    // If we finished the sequence, either loop back to the beginning or repeat the last command
    if (animSequences[faceStateGame->animIndexCur] == AnimCommand_Loop)
    {
      faceStateGame->animIndexCur = faceStateGame->animIndexStart;
    }
    else if (animSequences[faceStateGame->animIndexCur] == AnimCommand_Done)
    {
      faceStateGame->animIndexCur--;
      faceStateGame->animDone = true;
    }    

    // Start timer for next command
    if (animSequences[faceStateGame->animIndexCur] == AnimCommand_PauseHalf)
    {
      animRate32 >>= 1;
    }
    faceStateGame->animTimer.set(animRate32);
  }

}

void render()
{
  setColor(OFF);

  FOREACH_FACE(f)
  {
    renderAnimationStateOnFace(f);
  }

#if RENDER_DEBUG_AND_ERRORS
  FOREACH_FACE(f)
  {
    if (faceStatesComm[f].debug)
    {
      setColorOnFace(RED, f);
    }
  }

  FOREACH_FACE(f)
  {
    FaceStateComm *faceStateComm = &faceStatesComm[f];

    if (ErrorOnFace(f))
    {
      if (commInsertionIndexes[f] == COMM_INDEX_ERROR_OVERRUN)
      {
        setColorOnFace(MAGENTA, f);
      }
      else if (commInsertionIndexes[f] == COMM_INDEX_OUT_OF_SYNC)
      {
        setColorOnFace(GREEN, f);
      }
      else
      {
        setColorOnFace(RED, f);
      }
    }
  }
#endif
}
