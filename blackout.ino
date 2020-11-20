// FACTORY BLINKS
// (c) 2020 Scott Cartier

#define null 0
#define DONT_CARE 0

// Defines to enable debug code
#define USE_DATA_SPONGE         0
#define DEBUG_COMM              0
#define SHOW_TILE_ID            0
#define DEBUG_SETUP             1

#if USE_DATA_SPONGE
#warning DATA SPONGE ENABLED
byte sponge[26];
#endif
// SPONGE LOG
// 11/3: 48, 39
// 11/10: 25

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

#if DEBUG_COMM
  byte ourState;
  byte neighborState;
#endif
};
FaceStateComm faceStatesComm[FACE_COUNT];

enum Command
{
  Command_None,         // no data

#if DEBUG_COMM
  Command_UpdateState,  // data=value
#endif

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
  
  Command_SetGameState,

  Command_Reset
};

struct CommandAndData
{
  Command command : 4;
  byte data : 4;
};

#define COMM_QUEUE_SIZE 4
CommandAndData commQueues[FACE_COUNT][COMM_QUEUE_SIZE];

#define COMM_INDEX_ERROR_OVERRUN 0xFF
#define COMM_INDEX_OUT_OF_SYNC   0xFE
byte commInsertionIndexes[FACE_COUNT];

byte numNeighbors = 0;

#define ErrorOnFace(f) (commInsertionIndexes[f] > COMM_QUEUE_SIZE)

#if DEBUG_COMM
// Timer used to toggle between green & blue
Timer sendNewStateTimer;
#endif

// -------------------------------------------------------------------------------------------------
// TILES
// -------------------------------------------------------------------------------------------------

enum TileRole
{
  TileRole_Unassigned,
//  TileRole_Target,
  TileRole_Working,
  TileRole_Tool
};

TileRole tileRole = TileRole_Unassigned;

struct ToolState
{
  byte pattern  : 4;
  byte color    : 2;
  byte rotation : 3;
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
  0b1001,
  0b1010,
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
  AnimCommand_Solid,
  AnimCommand_SolidWithOverlay,
  AnimCommand_BaseAndOverlay,
  AnimCommand_LerpBaseToOverlay,
  AnimCommand_LerpOverlayToBase,
  AnimCommand_Pause,
  AnimCommand_FadeInBase,
  AnimCommand_FadeOutBase,
  AnimCommand_FadeOutOverlay,
  AnimCommand_Loop
};

// Start each face at a different command
AnimCommand animSequences[] =
{
  // SPIN
  AnimCommand_FadeInBase,
  AnimCommand_FadeOutBase,
  AnimCommand_Pause,
  AnimCommand_Pause,
  AnimCommand_Pause,
  AnimCommand_Pause,
  AnimCommand_Loop,

  // PULSE
  AnimCommand_LerpBaseToOverlay,
  AnimCommand_LerpOverlayToBase,
  AnimCommand_Loop,

  // COPY
  AnimCommand_FadeInBase,
  AnimCommand_LerpBaseToOverlay,
  AnimCommand_FadeOutOverlay,
  AnimCommand_Pause,
  AnimCommand_Loop,

  // CONSTANT COLOR - BASE ONLY
  AnimCommand_Solid,
  AnimCommand_Loop,

  // CONSTANT COLOR - BASE IF NO OVERLAY
  AnimCommand_SolidWithOverlay,     // used on Target tile during Setup phase
  AnimCommand_Loop,
};

#define ANIM_SEQ_INDEX_SPIN     0
#define ANIM_SEQ_INDEX_PULSE    7
#define ANIM_SEQ_INDEX_COPY     10
#define ANIM_SEQ_INDEX_BASE     15
#define ANIM_SEQ_INDEX_BASE_PLUS_OVERLAY  17

// Shift by 2 so it fits in a byte
#define ANIM_RATE_SLOW (1000>>2)
#define ANIM_RATE_FAST (300>>2)

// -------------------------------------------------------------------------------------------------
// GAME
// -------------------------------------------------------------------------------------------------

enum GameState
{
  GameState_Init,     // tiles were just programmed - waiting for click of Target tile
  GameState_Setup,    // let player choose number of tool tiles and difficulty
//  GameState_Detach,   // detach the tool tiles from working tile before the pattern is revealed
  GameState_Play,     // gameplay
  GameState_Done      // player matched the Target tile
};

GameState gameState = GameState_Init;
byte numTargetAndToolTiles = 0;

#define NUM_DIFFICULTY_LEVELS 6
byte difficulty = 0;

uint32_t startingSeed = 0;

byte rootFace = 0;  // used during setup for Tool tiles to know which face is the Target tile

#define MAX_TOOL_TILES 6

struct FaceStateGame
{
  byte animIndexStart;
  byte animIndexCur;

  // Used during setup
  ToolState neighborTool;
};
FaceStateGame faceStatesGame[FACE_COUNT];
uint32_t animRate;            // lerp/pause rate
Timer animTimer;              // timer used for lerps and pauses during animation

// Store our current puzzle state
byte startingState[3];        // how the puzzle starts
byte colorState[3];           // the current state displayed
byte overlayState[3];         // used for animations or to hide things

#define RESET_TIMER_DELAY 3000
Timer resetTimer;             // prevents cyclic resets

// =================================================================================================
//
// SETUP
//
// =================================================================================================

void setup()
{
#if USE_DATA_SPONGE
  // Use our data sponge so that it isn't compiled away
  if (sponge[0])
  {
    sponge[0] = 3;
  }
#endif

  FOREACH_FACE(f)
  {
    resetCommOnFace(f);
  }

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
    case GameState_Init:  break; // do nothing - waiting for click in handleUserInput()
    case GameState_Setup: loopSetup();  break;
    case GameState_Play:  loopPlay();   break;
    case GameState_Done:  break;
  }

#if DEBUG_COMM
  if (sendNewStateTimer.isExpired())
  {
    FOREACH_FACE(f)
    {
      byte nextVal = faceStatesComm[f].neighborState == 2 ? 3 : 2;
      faceStatesComm[f].neighborState = nextVal;
      enqueueCommOnFace(f, Command_UpdateState, nextVal);
    }
    sendNewStateTimer.set(500);
  }
#endif

  render();

  // Call hasWoken() explicitly to clear the flag
  hasWoken();
}

// =================================================================================================
//
// INPUT
//
// =================================================================================================

void handleUserInput()
{
  if (buttonMultiClicked())
  {
    byte clicks = buttonClickCount();
    if (clicks == 4)
    {
    }
  }

  if (buttonDoubleClicked())
  {
    if (gameState == GameState_Setup && tileRole == TileRole_Working)
    {
      difficulty++;
      if (difficulty >= NUM_DIFFICULTY_LEVELS)
      {
        difficulty = 0;
      }
      generateToolsAndPuzzle();
    }
  }

  if (buttonSingleClicked() && !hasWoken())
  {
    switch (gameState)
    {
      case GameState_Init:
        tileRole = TileRole_Working;
        gameState = GameState_Setup;
  
        colorState[0] = 0; colorState[1] = 0; colorState[2] = 0x3F;
        showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);

        numTargetAndToolTiles = 0;
  
        // Button clicking provides our entropy for the initial random seed
        randSetSeed(millis());
        generateNewStartingSeed();
        break;

      case GameState_Setup:
        if (tileRole == TileRole_Working && numNeighbors > 0)
        {
          gameState = GameState_Play;
          
          // Start the game
          FOREACH_FACE(f)
          {
            if (faceStatesComm[f].neighborPresent)
            {
              enqueueCommOnFace(f, Command_SetGameState, GameState_Play);
            }
          }

          updateWorkingState();
        }
        else if (tileRole == TileRole_Tool)
        {
#if DEBUG_SETUP
          showAnimation_Tool();
#endif
        }
        break;

      case GameState_Play:
        if (tileRole == TileRole_Tool)
        {
          // Cycle through the primary colors and white
          assignedTool.color = (assignedTool.color == COLOR_WHITE) ? 0 : (assignedTool.color + 1);
          showAnimation_Tool();
          enqueueCommOnFace(rootFace, Command_ToolColor, assignedTool.color);
        }
        break;
    }
  }

  // No matter what state we're in, long pressing resets the game and other connected tiles
  if (buttonLongPressed())
  {
    resetGame();
  }
}

void resetGame()
{
  // Can only reset once every so often to prevent infinite loops
  if (!resetTimer.isExpired())
  {
    return;
  }

  resetTimer.set(RESET_TIMER_DELAY);
  
  gameState = GameState_Init;
  tileRole = TileRole_Unassigned;
  assignedTool = { TOOL_PATTERN_UNASSIGNED, 0 };
  showAnimation_Init();

  // Propagate the reset out to the rest of the cluster
  FOREACH_FACE(f)
  {
    enqueueCommOnFace(f, Command_Reset, DONT_CARE);
  }
}

// =================================================================================================
//
// COMMUNICATIONS
//
// =================================================================================================

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

void resetCommQueueOnFace(byte f)
{
  // Kill all commands except the active one at index 0 since it might be in progress
  if (commInsertionIndexes[f] > 0)
  {
    commInsertionIndexes[f] = 1;
  }
}

void sendValueOnFace(byte f, FaceValue faceValue)
{
  byte outVal = *((byte*)&faceValue);
  setValueSentOnFace(outVal, f);
}

// Called by the main program when this tile needs to tell something to
// a neighbor tile.
void enqueueCommOnFace(byte f, Command command, byte data)
{
  // Check here so callers don't all need to check
  if (!faceStatesComm[f].neighborPresent)
  {
    return;
  }

  if (commInsertionIndexes[f] >= COMM_QUEUE_SIZE)
  {
    // Buffer overrun - might need to increase queue size to accommodate
    commInsertionIndexes[f] = COMM_INDEX_ERROR_OVERRUN;
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
        if (commInsertionIndexes[f] == 0)
        {
          // Shouldn't get here - if so something is funky
          commInsertionIndexes[f] = COMM_INDEX_OUT_OF_SYNC;
          continue;
        }
        else
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
  FaceStateComm *faceStateComm = &faceStatesComm[f];

  byte oppositeFace = OPPOSITE_FACE(f);

  //faceStateComm->debug = true;
  switch (command)
  {
#if DEBUG_COMM
    // YOUR STUFF GOES HERE
    // For now, just save the value that was sent
    case Command_UpdateState:
      faceStateComm->ourState = value;
      break;
#endif

    case Command_AssignRole:
      // Grab our new role
      tileRole = (TileRole) value;
      rootFace = f;
      gameState = GameState_Setup;
      if (tileRole == TileRole_Tool)
      {
        colorState[0] = colorState[1] = colorState[2] = 0x3F;
        showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
      }
      break;

    case Command_AssignToolPattern:
      if (tileRole == TileRole_Tool)
      {
        assignedTool.pattern = value;
        assignedTool.color = COLOR_WHITE;
#if DEBUG_SETUP
        showAnimation_Tool();
#endif
      }
      break;
      
    case Command_SetGameState:
      if (tileRole == TileRole_Tool)
      {
        gameState = (GameState) value;
        if (gameState == GameState_Play)
        {
          showAnimation_Tool();
        }
      }
      break;

    case Command_RequestPattern:
      // Only the working tile can request Tool info - update the root face
      rootFace = f;
      if (tileRole == TileRole_Tool)
      {
        enqueueCommOnFace(f, Command_ToolPattern, assignedTool.pattern);
      }
      break;
      
    case Command_RequestRotation:
      // Only the working tile can request Tool info - update the root face
      rootFace = f;
      if (tileRole == TileRole_Tool)
      {
        // We send the face that received this comm so the requester
        // can compute our relative rotation
        enqueueCommOnFace(f, Command_ToolRotation, f);
      }
      break;

    case Command_RequestColor:
      // Only the working tile can request Tool info - update the root face
      rootFace = f;
      if (tileRole == TileRole_Tool)
      {
        enqueueCommOnFace(f, Command_ToolColor, assignedTool.color);
      }
      break;

    case Command_ToolPattern:
      if (tileRole == TileRole_Working)
      {
        faceStatesGame[f].neighborTool.pattern = value;
        enqueueCommOnFace(f, Command_RequestRotation, DONT_CARE);
      }
      break;

    case Command_ToolRotation:
      if (tileRole == TileRole_Working)
      {
        // Figure out how the Tool tile is rotated relative to the Working tile
        byte oppositeFace = OPPOSITE_FACE(value);
        char offset = f - oppositeFace;
        faceStatesGame[f].neighborTool.rotation = (offset < 0) ? (offset + 6) : offset;
        enqueueCommOnFace(f, Command_RequestColor, DONT_CARE);
      }
      break;
      
    case Command_ToolColor:
      if (tileRole == TileRole_Working)
      {
        faceStatesGame[f].neighborTool.color = value;
        updateWorkingState();
      }
      break;

    case Command_Reset:
      resetGame();
      break;
  }
}

// =================================================================================================
//
// GAME STATE: SETUP
// Let the player remove/attach Tool tiles, select difficulty, set a random seed
//
// =================================================================================================

void loopSetup()
{
  switch (tileRole)
  {
    case TileRole_Working:  setupWorking(); break;
    case TileRole_Tool:     setupTool(); break;
  }
}

// -------------------------------------------------------------------------------------------------
// WORKING
// -------------------------------------------------------------------------------------------------

void setupWorking()
{
  if (numNeighbors != numTargetAndToolTiles)
  {
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

  numTargetAndToolTiles = numNeighbors;

  // Nibble 4 (19:16) represents sixteen different puzzles using the same tools
  // It needs to be part of the starting seed so it can be shown to the player,
  // but we don't want it to factor into the seed for tool selection because tools
  // shouldn't change based on it.

  uint32_t puzzleSeed = startingSeed;
  puzzleSeed |= (uint32_t) numTargetAndToolTiles << 24;
  puzzleSeed |= (uint32_t) difficulty << 28;

  byte toolSlotIndex = 0;
  FOREACH_FACE(f)
  {
    if (!faceStatesComm[f].neighborPresent)
    {
      continue;
    }

    // Choose the tools sequentially
    faceStatesGame[f].neighborTool.pattern = patterns[difficulty+toolSlotIndex];

    // Make it a Tool tile
    // The neighbor will then request the tool type & pattern
    enqueueCommOnFace(f, Command_AssignRole, TileRole_Tool);
    enqueueCommOnFace(f, Command_AssignToolPattern, faceStatesGame[f].neighborTool.pattern);

    toolSlotIndex++;
  }

  // Now that we're generating the actual puzzle, use a seed that includes [19:16]
  randSetSeed(puzzleSeed);

  // Fill up the starting state with our tools
  startingState[0] = startingState[1] = startingState[2] = 0;
  byte randColor = randRange(0, 3);
  FOREACH_FACE(f)
  {
    if (faceStatesComm[f].neighborPresent)
    {
      faceStatesGame[f].neighborTool.color = randColor;
      faceStatesGame[f].neighborTool.rotation = randRange(0, FACE_COUNT);

      // Go sequentially through RGB
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
  showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
#endif
}

void updateWorkingState()
{
  colorState[0] = startingState[0];
  colorState[1] = startingState[1];
  colorState[2] = startingState[2];

  updateStateWithTools(colorState);
}

void updateStateWithTools(byte *state)
{
  // Apply all the attached tools
  FOREACH_FACE(f)
  {
    if (faceStatesComm[f].neighborPresent)
    {
      applyTool(faceStatesGame[f].neighborTool, state);
    }
  }
}

void applyTool(ToolState tool, byte *state)
{
  if (tool.pattern == TOOL_PATTERN_UNASSIGNED)
  {
    return;
  }
  if (tool.color == COLOR_WHITE)
  {
    return;
  }
        
  // Save old state to know if we changed anything by applying this tool
  byte oldState[3];
  oldState[0] = state[0];
  oldState[1] = state[1];
  oldState[2] = state[2];
  
  byte toolMask = 0x1 | (tool.pattern << 1);
  
  // Apply rotation
  toolMask = ((toolMask << tool.rotation) | (toolMask >> (6 - tool.rotation))) & 0x3F;

  state[tool.color] ^= toolMask;
}

// -------------------------------------------------------------------------------------------------
// TOOL & TARGET
// -------------------------------------------------------------------------------------------------

void setupTool()
{
  // If we disconnected from the Target tile then clear our role
  if (!faceStatesComm[rootFace].neighborPresent)
  {
    gameState = GameState_Init;
    tileRole = TileRole_Unassigned;
    assignedTool = { TOOL_PATTERN_UNASSIGNED, 0 };

    showAnimation_Init();
  }
}

// =================================================================================================
//
// GAME STATE: PLAY
//
// =================================================================================================

void loopPlay()
{
  switch (tileRole)
  {
    case TileRole_Working:  playWorking(); break;
    case TileRole_Tool:     playTool(); break;
  }
}

void playWorking()
{
  // Check for a Tool added or removed
  if (numNeighbors != numTargetAndToolTiles)
  {
    numTargetAndToolTiles = numNeighbors;

    FOREACH_FACE(f)
    {
      if (faceStatesComm[f].neighborPresent)
      {
        if (faceStatesGame[f].neighborTool.pattern == TOOL_PATTERN_UNASSIGNED)
        {
          // Give the tool a pattern so that it doesn't fall in here again, but make it WHITE so it doesn't get drawn yet
          faceStatesGame[f].neighborTool.pattern = 0;
          faceStatesGame[f].neighborTool.color = COLOR_WHITE;
          
          enqueueCommOnFace(f, Command_RequestPattern, DONT_CARE);
        }
      }
      else
      {
        faceStatesGame[f].neighborTool.pattern = TOOL_PATTERN_UNASSIGNED;
      }
    }

    // Update the color state
    updateWorkingState();
  }

  // Check for win state
  if (colorState[0] == 0 && colorState[1] == 0 && colorState[2] == 0)
  {
    // TOTAL BLACKOUT - PUZZLE SOLVED
  }
}

void playTool()
{
  
}

// =================================================================================================
//
// RANDOM
//
// =================================================================================================

// Random code partially copied from blinklib because there's no function
// to set an explicit seed, which we need so we can share/replay puzzles.
uint32_t randState = 0;

void randSetSeed(uint32_t newSeed)
{
  randState = newSeed;
}

byte randGetByte()
{
  // Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
  uint32_t x = randState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  randState = x;
  return x & 0xFF;
}

byte randRange(byte min, byte max)
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
  startingSeed = randState & 0x000FFFFF;
}

// =================================================================================================
//
// ANIMATIONS
//
// =================================================================================================

void showAnimation_Init()
{
  colorState[0] = colorState[1] = colorState[2] = 0x3F;
  showAnimation(ANIM_SEQ_INDEX_SPIN, ANIM_RATE_SLOW);
  
  FOREACH_FACE(f)
  {
    faceStatesGame[f].animIndexCur = ANIM_SEQ_INDEX_SPIN + f;
  }
}

void showAnimation_Tool()
{
  // Create the mask corresponding to the pattern
  byte mask = 0x1 | (assignedTool.pattern << 1);

  if (assignedTool.color == COLOR_WHITE)
  {
    colorState[0] = colorState[1] = colorState[2] = mask;
  }
  else
  {
    colorState[0] = 0; colorState[1] = 0; colorState[2] = 0;
    colorState[assignedTool.color] = mask;
  }
  showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
}

void showAnimation(byte animIndex, byte newAnimRate)
{
  FOREACH_FACE(f)
  {
    faceStatesGame[f].animIndexStart = faceStatesGame[f].animIndexCur = animIndex;
  }
  animRate = newAnimRate<<2;
  animTimer.set(animRate);
}

// =================================================================================================
//
// RENDER
//
// =================================================================================================

void lerpColors(byte inR1, byte inG1, byte inB1, byte inR2, byte inG2, byte inB2, byte *outRGB, byte t)
{
  uint32_t temp;

  temp = inR1 * (128 - t) + inR2 * t;
  outRGB[0] = temp >> 7;
  temp = inG1 * (128 - t) + inG2 * t;
  outRGB[1] = temp >> 7;
  temp = inB1 * (128 - t) + inB2 * t;
  outRGB[2] = temp >> 7;
}

byte getColorFromState(byte state, byte bitOffset)
{
  byte colorBit = state >> bitOffset;
  char colorByte = colorBit << 7;
  colorByte >>= 7;  // use sign extension to fill with 0s or 1s
  return colorByte;
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
  
  byte colorRGB[3];
  bool startNextCommand = false;
  bool paused = false;
  uint32_t t = (128 * (animRate - animTimer.getRemaining())) / animRate;  // 128 = 1.0
  switch (animSequences[faceStateGame->animIndexCur])
  {
    case AnimCommand_Solid:
      colorRGB[0] = r;
      colorRGB[1] = g;
      colorRGB[2] = b;
      startNextCommand = true;
      break;

    case AnimCommand_SolidWithOverlay:
    {
      bool hasOverlay = (overlayState[0] | overlayState[1] | overlayState[2]) & (1<<f);
      colorRGB[0] = hasOverlay ? overlayR : r;
      colorRGB[1] = hasOverlay ? overlayG : g;
      colorRGB[2] = hasOverlay ? overlayB : b;
      startNextCommand = true;
    }
      break;

    case AnimCommand_LerpOverlayToBase:
      t = 128 - t;
    case AnimCommand_LerpBaseToOverlay:
      lerpColors(r, g, b, overlayR, overlayG, overlayB, colorRGB, t);
      break;

    case AnimCommand_Pause:
      paused = true;
      break;

    case AnimCommand_FadeInBase:
      lerpColors(0, 0, 0, r, g, b, colorRGB, t);
      break;
      
    case AnimCommand_FadeOutBase:
      lerpColors(r, g, b, 0, 0, 0, colorRGB, t);
      break;
      
    case AnimCommand_FadeOutOverlay:
      lerpColors(overlayR, overlayG, overlayB, 0, 0, 0, colorRGB, t);
      break;
  }

  if (animTimer.isExpired())
  {
    startNextCommand = true;
  }
  
  if (!paused)
  {
    Color color = makeColorRGB(colorRGB[0], colorRGB[1], colorRGB[2]);
    setColorOnFace(color, f);
  }

  if (startNextCommand)
  {
    faceStateGame->animIndexCur++;

    // If we finished the sequence, loop back to the beginning
    if (animSequences[faceStateGame->animIndexCur] == AnimCommand_Loop)
    {
      faceStateGame->animIndexCur = faceStateGame->animIndexStart;
    }

    // Start timer for next command, but only after done processing all faces
    if (f == 5)
    {
      animTimer.set(animRate);
    }
  }

}

void render()
{
  setColor(OFF);

  FOREACH_FACE(f)
  {
    renderAnimationStateOnFace(f);
  }
  
#if SHOW_TILE_ID
  if (tileRole == TileRole_Tool)
  {
    setColorOnFace(RED, 0);
    setColorOnFace(GREEN, 1);
    setColorOnFace(tileId & 0x1 ? WHITE : OFF, 2);
    setColorOnFace(tileId & 0x2 ? WHITE : OFF, 3);
    setColorOnFace(tileId & 0x4 ? WHITE : OFF, 4);
    setColorOnFace(tileId & 0x8 ? WHITE : OFF, 5);
  }
#endif

#if DEBUG_COMM
  FOREACH_FACE(f)
  {
    FaceStateComm *faceStateComm = &faceStatesComm[f];

    if (!isValueReceivedOnFaceExpired(f))
    {
      if (faceStateComm->ourState == 2)
      {
        setColorOnFace(GREEN, f);
      }
      else if (faceStateComm->ourState == 3)
      {
        setColorOnFace(BLUE, f);
      }
    }
  }
#endif

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
}
