// FACTORY BLINKS
// (c) 2020 Scott Cartier

#define null 0
#define DONT_CARE 0

// Defines to enable debug code
#define USE_DATA_SPONGE         0
#define DEBUG_COMM              0
#define SHOW_TILE_ID            0
#define DEBUG_SETUP             1
#define SHOW_COMM_QUEUE_LENGTH  0
#define SHOW_TOOL_TYPE_BITWISE  0

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
  Command_AssignRole,
  Command_RequestToolType,
  Command_AssignToolType,
  Command_RequestToolPattern,
  Command_AssignToolPattern,
  Command_SetGameState,

  Command_AnimSync,
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
  TileRole_Target,
  TileRole_Working,
  TileRole_Tool
};

TileRole tileRole = TileRole_Unassigned;

enum ToolType
{
  ToolType_Unassigned,
  ToolType_Color1,
  ToolType_Color2,
  ToolType_Color3,
  ToolType_Copy,
  ToolType_Mask,
  ToolType_Wedge,
  ToolType_Swap,
  ToolType_Erase,

  ToolType_Color1Start,
  ToolType_ColorR = ToolType_Color1Start,
  ToolType_ColorG,
  ToolType_ColorB,
  ToolType_Color1End = ToolType_ColorB,

  ToolType_Color2Start,
  ToolType_ColorRG = ToolType_Color2Start,
  ToolType_ColorRB,
  ToolType_ColorGB,
  ToolType_Color2End = ToolType_ColorGB,
};

struct ToolTypeAndPattern
{
  ToolType type : 4;
  byte pattern : 4;
};

ToolTypeAndPattern assignedTool = { ToolType_Unassigned, 0 };

#define NUM_DIFFICULTY_LEVELS 6
/*
ToolTypeAndPattern tools[NUM_DIFFICULTY_LEVELS][3*(NUM_DIFFICULTY_LEVELS+1)] =
{
  // SLOT 1 (color tools only)
  {
    { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 },    // Difficulty 1
    { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 },    // Difficulty 1-2
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },    // Difficulty 2-3
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },    // Difficulty 3-4
    { ToolType_Color1, 0b0011 }, { ToolType_Color1, 0b1010 }, { ToolType_Color1, 0b0101 },    // Difficulty 4-5
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 }, { ToolType_Color2, 0b0100 },    // Difficulty 5-6
    { ToolType_Color2, 0b0011 }, { ToolType_Color2, 0b1010 }, { ToolType_Color2, 0b1001 },    // Difficulty 6
  },
  // SLOT 2 (color tools only)
  {
    { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 },    // Difficulty 1
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },    // Difficulty 1-2
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },    // Difficulty 2-3
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },    // Difficulty 3-4
    { ToolType_Color1, 0b0011 }, { ToolType_Color1, 0b1010 }, { ToolType_Color1, 0b0101 },    // Difficulty 4-5
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 }, { ToolType_Color2, 0b0100 },    // Difficulty 5-6
    { ToolType_Color2, 0b0011 }, { ToolType_Color2, 0b1010 }, { ToolType_Color2, 0b1001 },    // Difficulty 6
  },
  // SLOT 3 (widget tools only)
  {
    { ToolType_Copy,   0b0001 }, { ToolType_Copy,   0b0010 }, { ToolType_Copy,   0b0100 },
    { ToolType_Copy,   0b1000 }, { ToolType_Copy,   0b1000 }, { ToolType_Copy,   0b0001 },
    { ToolType_Mask,   0b0001 }, { ToolType_Mask,   0b0001 }, { ToolType_Mask,   0b0001 },
    { ToolType_Copy,   0b0010 }, { ToolType_Copy,   0b0100 }, { ToolType_Copy,   0b1000 },
    { ToolType_Mask,   0b0010 }, { ToolType_Mask,   0b0100 }, { ToolType_Mask,   0b0011 },
    { ToolType_Copy,   0b0110 }, { ToolType_Copy,   0b1100 }, { ToolType_Copy,   0b1010 },
    { ToolType_Mask,   0b1010 }, { ToolType_Mask,   0b0101 }, { ToolType_Mask,   0b1001 },
  },
  // SLOT 4 (color tools only)
  {
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 }, { ToolType_Color1, 0b0100 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 }, { ToolType_Color2, 0b0100 },
    { ToolType_Color2, 0b0101 }, { ToolType_Color2, 0b1100 }, { ToolType_Color2, 0b0111 },
  },
  // SLOT 5 (color or widget tools)
  {
    { ToolType_Color1, 0b0011 }, { ToolType_Color1, 0b0110 }, { ToolType_Color1, 0b1010 },
    { ToolType_Swap,   0b0001 }, { ToolType_Swap,   0b0010 }, { ToolType_Swap,   0b0100 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 }, { ToolType_Color2, 0b0100 },
    { ToolType_Swap,   0b0001 }, { ToolType_Swap,   0b0010 }, { ToolType_Swap,   0b0100 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 }, { ToolType_Color2, 0b0100 },
    { ToolType_Swap,   0b0001 }, { ToolType_Swap,   0b0010 }, { ToolType_Swap,   0b0100 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 }, { ToolType_Color2, 0b0100 },
  },
  // SLOT 6 (widget tools only)
  {
    { ToolType_Copy,   0b0001 }, { ToolType_Copy,   0b0010 }, { ToolType_Copy,   0b0100 },
    { ToolType_Copy,   0b1000 }, { ToolType_Copy,   0b1000 }, { ToolType_Copy,   0b0001 },
    { ToolType_Mask,   0b0001 }, { ToolType_Mask,   0b0001 }, { ToolType_Mask,   0b0001 },
    { ToolType_Copy,   0b0010 }, { ToolType_Copy,   0b0100 }, { ToolType_Copy,   0b1000 },
    { ToolType_Mask,   0b0010 }, { ToolType_Mask,   0b0100 }, { ToolType_Mask,   0b0011 },
    { ToolType_Copy,   0b0110 }, { ToolType_Copy,   0b1100 }, { ToolType_Copy,   0b1010 },
    { ToolType_Mask,   0b1010 }, { ToolType_Mask,   0b0101 }, { ToolType_Mask,   0b1001 },
  },
};
*/
ToolTypeAndPattern tools[NUM_DIFFICULTY_LEVELS][2*(NUM_DIFFICULTY_LEVELS+1)] =
{
  // SLOT 1 (color tools only)
  {
    { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 },
    { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color1, 0b0011 }, { ToolType_Color1, 0b1010 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 },
    { ToolType_Color2, 0b0011 }, { ToolType_Color2, 0b1010 },
  },
  // SLOT 2 (color tools only)
  {
    { ToolType_Color1, 0b0000 }, { ToolType_Color1, 0b0000 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color1, 0b0011 }, { ToolType_Color1, 0b1010 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 },
    { ToolType_Color2, 0b0011 }, { ToolType_Color2, 0b1010 },
  },
  // SLOT 3 (widget tools only)
  {
    { ToolType_Copy,   0b0001 }, { ToolType_Copy,   0b0010 },
    { ToolType_Copy,   0b1000 }, { ToolType_Copy,   0b1000 },
    { ToolType_Mask,   0b0001 }, { ToolType_Mask,   0b0001 },
    { ToolType_Copy,   0b0010 }, { ToolType_Copy,   0b0100 },
    { ToolType_Mask,   0b0010 }, { ToolType_Mask,   0b0100 },
    { ToolType_Copy,   0b0110 }, { ToolType_Copy,   0b1100 },
    { ToolType_Mask,   0b1010 }, { ToolType_Mask,   0b0101 },
  },
  // SLOT 4 (color tools only)
  {
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color1, 0b0001 }, { ToolType_Color1, 0b0010 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 },
    { ToolType_Color2, 0b0101 }, { ToolType_Color2, 0b1100 },
  },
  // SLOT 5 (color or widget tools)
  {
    { ToolType_Color1, 0b0011 }, { ToolType_Color1, 0b0110 },
    { ToolType_Swap,   0b0001 }, { ToolType_Swap,   0b0010 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 },
    { ToolType_Swap,   0b0001 }, { ToolType_Swap,   0b0010 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 },
    { ToolType_Swap,   0b0001 }, { ToolType_Swap,   0b0010 },
    { ToolType_Color2, 0b0001 }, { ToolType_Color2, 0b0010 },
  },
  // SLOT 6 (widget tools only)
  {
    { ToolType_Copy,   0b0001 }, { ToolType_Copy,   0b0010 },
    { ToolType_Copy,   0b1000 }, { ToolType_Copy,   0b1000 },
    { ToolType_Mask,   0b0001 }, { ToolType_Mask,   0b0001 },
    { ToolType_Copy,   0b0010 }, { ToolType_Copy,   0b0100 },
    { ToolType_Mask,   0b0010 }, { ToolType_Mask,   0b0100 },
    { ToolType_Copy,   0b0110 }, { ToolType_Copy,   0b1100 },
    { ToolType_Mask,   0b1010 }, { ToolType_Mask,   0b0101 },
  },
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
  GameState_Detach,   // player started game - wait for tool tile to detach from Target tile
  GameState_Play,     // gameplay
  GameState_Done      // player matched the Target tile
};

GameState gameState = GameState_Init;
byte numToolTiles = 0;
byte difficulty = 0;
uint32_t startingSeed = 0;

byte rootFace = 0;  // used during setup for Tool tiles to know which face is the Target tile

#define MAX_TOOL_TILES 6

struct FaceStateGame
{
  byte animIndexStart;
  byte animIndexCur;

  // Used during setup
  ToolTypeAndPattern neighborTool;
};
FaceStateGame faceStatesGame[FACE_COUNT];
uint32_t animRate;            // lerp/pause rate
Timer animTimer;              // timer used for lerps and pauses during animation

// Store our current puzzle state in a 32-bit int for easy comparison
// Each RGB of the 6 faces corresponds to a nibble - so we only use 24 bits
uint32_t colorState;
uint32_t overlayState;
uint32_t maskState;

#define COLOR_STATE_OFF   0x00000000
#define COLOR_STATE_WHITE 0x00777777

#if SHOW_TOOL_TYPE_BITWISE
bool showToolTypeBitwise;
bool showToolPatternBitwise;
#endif

// -------------------------------------------------------------------------------------------------
// SOLUTION
// -------------------------------------------------------------------------------------------------

struct SolutionStep
{
  byte neighborFace : 3;
  byte rotation : 3;
};
#define MAX_SOLUTION_STEPS 10
SolutionStep solutionSteps[MAX_SOLUTION_STEPS];
int numSolutionSteps;
uint32_t solutionState;


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

    case GameState_Setup:
      loopSetup();
      break;

    case GameState_Detach:
      loopDetach();
      break;

    case GameState_Play:
      break;

    case GameState_Done:
      break;
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
    if (gameState == GameState_Init || gameState == GameState_Setup)
    {
      if (isAlone())
      {
        tileRole = TileRole_Working;
        gameState = GameState_Setup;

        // During setup, Working tiles are green
        colorState = 0x00222222;
        showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
      }
    }
    
    if (gameState == GameState_Setup && tileRole == TileRole_Target)
    {
    }
  }

  if (buttonSingleClicked() && !hasWoken())
  {
    if (gameState == GameState_Init)
    {
      tileRole = TileRole_Target;
      gameState = GameState_Setup;

      // Default to all blue - will light up faces with neighbors
      colorState = 0x00444444;
      showAnimation(ANIM_SEQ_INDEX_BASE_PLUS_OVERLAY, DONT_CARE);

      // Reset our perception of each neighbor's state
      FOREACH_FACE(f)
      {
        faceStatesGame[f].neighborTool.type = ToolType_Unassigned;
      }

      // Button clicking provides our entropy for the initial random seed
      randSetSeed(millis());
      generateNewStartingSeed();
    }
    else if (tileRole == TileRole_Target && gameState == GameState_Setup)
    {
      // Tell the player to detach all the Tool tiles
      // Once all of the Tool tiles are detached, the game will start
      gameState = GameState_Detach;

      // Tell the player to detach all the Tools from the Target
      FOREACH_FACE(f)
      {
        enqueueCommOnFace(f, Command_SetGameState, GameState_Detach);
      }
    }
#if DEBUG_SETUP
    else if (tileRole == TileRole_Tool && (gameState == GameState_Setup || gameState == GameState_Detach))
    {
#if SHOW_TOOL_TYPE_BITWISE
      if (showToolTypeBitwise)
      {
        showToolTypeBitwise = false;
        showToolPatternBitwise = true;
      }
      else if (showToolPatternBitwise)
      {
        showToolPatternBitwise = false;
      }
      else
      {
        showToolTypeBitwise = true;
      }
#endif
      showAnimation_Tool();
    }
#endif
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
      if (tileRole == TileRole_Tool)
      {
        rootFace = f;
        
        // Request the tool type
        enqueueCommOnFace(f, Command_RequestToolType, DONT_CARE);
        
        // During setup, Tool tiles are white
        colorState = COLOR_STATE_WHITE;
        showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
        
        gameState = GameState_Setup;
      }
      break;

    case Command_RequestToolType:
      if (tileRole == TileRole_Target)
      {
        enqueueCommOnFace(f, Command_AssignToolType, faceStatesGame[f].neighborTool.type);
      }
      break;

    case Command_AssignToolType:
      if (tileRole == TileRole_Tool)
      {
        assignedTool.type = (ToolType) value;
        enqueueCommOnFace(f, Command_RequestToolPattern, DONT_CARE);
      }
      break;
      
    case Command_RequestToolPattern:
      if (tileRole == TileRole_Target)
      {
        enqueueCommOnFace(f, Command_AssignToolPattern, faceStatesGame[f].neighborTool.pattern);
      }
      break;

    case Command_AssignToolPattern:
      if (tileRole == TileRole_Tool)
      {
        assignedTool.pattern = value;
#if DEBUG_SETUP
        showAnimation_Tool();
#endif
      }
      break;

    case Command_SetGameState:
      if (tileRole == TileRole_Tool)
      {
        gameState = (GameState) value;
        if (gameState == GameState_Detach)
        {
          // Start pulsing so the player knows to detach everything
          showAnimation(ANIM_SEQ_INDEX_PULSE, ANIM_RATE_FAST);
        }
      }
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
    case TileRole_Target: setupTarget(); break;
    case TileRole_Tool:   setupTool(); break;
  }
}

// -------------------------------------------------------------------------------------------------
// TARGET
// -------------------------------------------------------------------------------------------------

void setupTarget()
{
  bool neighborsChanged = false;

  overlayState = 0;
  uint32_t neighborColor = 0b110;
  FOREACH_FACE(f)
  {
    if (faceStatesComm[f].neighborPresent)
    {
      overlayState |= neighborColor;
      if (faceStatesGame[f].neighborTool.type == ToolType_Unassigned)
      {
        // Found a new neighbor
        neighborsChanged = true;
      }
    }
    else
    {
      if (faceStatesGame[f].neighborTool.type != ToolType_Unassigned)
      {
        // Lost a neighbor
        neighborsChanged = true;
        faceStatesGame[f].neighborTool.type = ToolType_Unassigned;
      }
    }
    neighborColor <<= 4;
  }

  if (neighborsChanged)
  {
    // Find out how many tool tiles there are and assign Tool indexes
    numToolTiles = 0;
    FOREACH_FACE(f)
    {
      if (faceStatesComm[f].neighborPresent)
      {
        numToolTiles++;
      }
    }

    // Generate the tools and the puzzle based on them
    generateToolsAndPuzzle();
  }
}

void generateToolsAndPuzzle()
{
  // Generate a puzzle based on the difficulty, number of tool tiles, and random seed

  // Nibble 4 (19:16) represents sixteen different puzzles using the same tools
  // It needs to be part of the starting seed so it can be shown to the player,
  // but we don't want it to factor into the seed for tool selection because tools
  // shouldn't change based on it.

  uint32_t puzzleSeed = startingSeed;
  puzzleSeed |= (uint32_t) numToolTiles << 24;
  puzzleSeed |= (uint32_t) difficulty << 28;

  // Mask it out [19:16] while we select tools
  uint32_t toolSeed = puzzleSeed & 0xFFF0FFFF;
  randSetSeed(toolSeed);

  // This is the base index into Tools[] array from which to randomly select tools for each slot
  byte toolDifficultyBase = difficulty * 2;

  ToolType color1Type = (ToolType)((byte) ToolType_Color1Start + randRange(0, 3));
  ToolType color2Type = (ToolType)((byte) ToolType_Color2Start + randRange(0, 3));
  
  byte toolSlotIndex = 0;
  FOREACH_FACE(f)
  {
    if (!faceStatesComm[f].neighborPresent)
    {
      continue;
    }
    
    // Randomly choose among the tools for each slot
    byte randomToolIndex = toolDifficultyBase + randRange(0, 4);
    ToolTypeAndPattern tool = tools[toolSlotIndex][randomToolIndex];

    // Swap out ToolType_Color# for the one with an actual color
    if (tool.type == ToolType_Color1)
    {
      tool.type = color1Type;
      color1Type = (ToolType)(((byte) color1Type) + 1);
      if (color1Type > ToolType_Color1End)
      {
        color1Type = ToolType_Color1Start;
      }
    }

    if (tool.type == ToolType_Color2)
    {
      tool.type = color2Type;
      color2Type = (ToolType)(((byte) color2Type) + 1);
      if (color2Type > ToolType_Color2End)
      {
        color2Type = ToolType_Color2Start;
      }
    }

    faceStatesGame[f].neighborTool = tool;

    // Make it a Tool tile
    // The neighbor will then request the tool type & pattern
    enqueueCommOnFace(f, Command_AssignRole, TileRole_Tool);

    // Quit when we're done
    toolSlotIndex++;
    if (toolSlotIndex >= numToolTiles || toolSlotIndex >= MAX_TOOL_TILES)
    {
      break;
    }
  }

  // Now that we're generating the actual puzzle, use a seed that includes [19:16]
  randSetSeed(puzzleSeed);

  // Fill up the solution with our tools
  numSolutionSteps = 4;
  for (byte i = 0, f = 0; i < MAX_SOLUTION_STEPS; i++, f++)
  {
    if (faceStatesComm[f].neighborPresent && faceStatesGame[f].neighborTool.type != ToolType_Unassigned)
    {
      solutionSteps[i].neighborFace = f;
      solutionSteps[i].rotation = randRange(0, FACE_COUNT);
    }
    
    if (f >= FACE_COUNT)
    {
      f = 0;
    }
  }

  // Solution starts blank
  solutionState = 0;
  maskState = 0;
  for (byte i = 0; i < numSolutionSteps; i++)
  {
    applyTool(faceStatesGame[solutionSteps[i].neighborFace].neighborTool, solutionSteps[i].rotation, &solutionState);
  }

#if DEBUG_SETUP
  colorState = solutionState;
  showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
#endif
}

void applyTool(ToolTypeAndPattern tool, byte rotation, uint32_t *state)
{
  uint32_t mask = 0xF;
  for (byte f = 0; f <= 3; f++)
  {
    if (tool.pattern & (0x1 << f))
    {
      maskState |= 0xF << ((f+1)<<2);
    }
  }

  uint32_t newState = 0;
  switch (tool.type)
  {
    case ToolType_ColorR: newState = 0x00111111 & mask; break;
    case ToolType_ColorG: newState = 0x00222222 & mask; break;
    case ToolType_ColorB: newState = 0x00444444 & mask; break;
  }

  // Apply rotation
  rotation <<= 2;
  newState = ((newState << rotation) | (newState >> (24 - rotation))) & 0x00FFFFFF;

  *state |= newState & maskState;
}

// -------------------------------------------------------------------------------------------------
// TOOL
// -------------------------------------------------------------------------------------------------

void setupTool()
{
  // If we disconnected from the Target tile then clear our role
  if (!faceStatesComm[rootFace].neighborPresent)
  {
    gameState = GameState_Init;
    tileRole = TileRole_Unassigned;
    assignedTool = { ToolType_Unassigned, 0 };

    showAnimation_Init();
  }
}

// =================================================================================================
//
// GAME STATE: DETACH
// Wait for the player to detach all Tool tiles from the Target tile and all Tool tiles from
// one another.
//
// =================================================================================================

void loopDetach()
{
  // Just waiting for all neighbors to detach before starting the game
  if (isAlone())
  {
    gameState = GameState_Play;

    // Show the Target and Tool patterns
    if (tileRole == TileRole_Target)
    {
      // Show the target pattern
    }
    else if (tileRole == TileRole_Tool)
    {
      showAnimation_Tool();
    }
  }
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
  colorState = 0x00777777;
  showAnimation(ANIM_SEQ_INDEX_SPIN, ANIM_RATE_SLOW);
  
  FOREACH_FACE(f)
  {
    faceStatesGame[f].animIndexCur = ANIM_SEQ_INDEX_SPIN + f;
  }
}

void showAnimation_Tool()
{
  // Create the mask corresponding to the pattern
  maskState = 0xF;
  for (byte f = 0; f <= 3; f++)
  {
    if (assignedTool.pattern & (0x1 << f))
    {
      maskState |= 0xF << ((f+1)<<2);
    }
  }

  switch (assignedTool.type)
  {
    case ToolType_ColorR:
      colorState = 0x00111111 & maskState;
      showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
      break;
      
    case ToolType_ColorG:
      colorState = 0x00222222 & maskState;
      showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
      break;
      
    case ToolType_ColorB:
      colorState = 0x00444444 & maskState;
      showAnimation(ANIM_SEQ_INDEX_BASE, DONT_CARE);
      break;

    case ToolType_Copy:
    {
      // Fill in face 0 already since it is assumed
      colorState = 0x00000007;
      overlayState = 0x00777770 & maskState;
      showAnimation(ANIM_SEQ_INDEX_COPY, ANIM_RATE_SLOW);
    }
      break;
  }
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

byte getColorFromState(uint32_t state, byte bitOffset)
{
  byte colorBit = (state >> bitOffset) & 0x1;
  char colorByte = colorBit << 7;
  colorByte >>= 7;  // use sign extension to fill with 0s or 1s
  return colorByte;
}

void renderAnimationStateOnFace(byte f)
{
  FaceStateGame *faceStateGame = &faceStatesGame[f];

  byte faceOffset = f<<2;
  byte r = getColorFromState(colorState, faceOffset+0);
  byte g = getColorFromState(colorState, faceOffset+1);
  byte b = getColorFromState(colorState, faceOffset+2);
  
  byte overlayRGB = (overlayState >> faceOffset) & 0xF;
  byte overlayR = getColorFromState(overlayState, faceOffset+0);
  byte overlayG = getColorFromState(overlayState, faceOffset+1);
  byte overlayB = getColorFromState(overlayState, faceOffset+2);
  
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
      colorRGB[0] = overlayRGB == 0 ? r : overlayR;
      colorRGB[1] = overlayRGB == 0 ? g : overlayG;
      colorRGB[2] = overlayRGB == 0 ? b : overlayB;
      startNextCommand = true;
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

#if SHOW_COMM_QUEUE_LENGTH
  if (tileRole == TileRole_Target)
  {
    FOREACH_FACE(f)
    {
      switch (commInsertionIndexes[f])
      {
        case 0: setColorOnFace(OFF, f); break;
        case 1: setColorOnFace(RED, f); break;
        case 2: setColorOnFace(ORANGE, f); break;
        case 3: setColorOnFace(YELLOW, f); break;
      }
    }
  }
#endif

#if SHOW_TOOL_TYPE_BITWISE
  if (tileRole == TileRole_Tool)
  {
    FOREACH_FACE(f)
    {
      if (showToolTypeBitwise)
      {
        setColorOnFace(OFF, f);
        if (assignedTool.type & (1<<f))
        {
          setColorOnFace(GREEN, f);
        }
      }
      else if (showToolPatternBitwise)
      {
        setColorOnFace(OFF, f);
        if (assignedTool.pattern & (1<<f))
        {
          setColorOnFace(YELLOW, f);
        }
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
