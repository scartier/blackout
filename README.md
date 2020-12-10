# Blackout!
### Summary
Blackout! is a single player puzzle game for 2-7 Blinks. Realistically, you'll want at least 4 Blinks to have a minimum of challenge.

The goal is to turn off all the colors on your Puzzle Tile using the surrounding Color Tiles.

### Setup
Be sure to grab the .ino and the blinklib.cpp. I modified the stock blinklib slightly to gain a bit of needed code space. If you don't grab that then you'll get errors when compiling.

After programming your tiles, they will display a slowly spinning half-face on each tile.

#### Puzzle Tile
Click one tile to choose it as your Puzzle Tile. It will turn blue with a single yellow wedge. That yellow wedge indicates your selected puzzle difficulty.

Difficulty ranges from 1-6 and affects the patterns on the Color Tiles you are given. Higher difficulties give more complicated Color Tiles. Double click the Puzzle Tile to cycle your difficulty level.

#### Color Tiles
During setup, all other tiles touching your Puzzle Tile automatically become Color Tiles and turn solid white. Add or remove Color Tiles as you wish before starting the game. More Color Tiles also increases difficulty. [**Hint: Start with two or three Color Tiles on difficulty 1 to learn the basics**]

### Play
Once you have selected your difficulty level and number of Color Tiles, click the Puzzle Tile again to start the game. Your Puzzle Tile will reveal the puzzle and your Color Tiles will show their assigned patterns in white.

Your goal is to turn off all of the colors on your Puzzle Tile!

#### Manipulating Tool Tiles
Clicking a Color Tile will cycle its color to Red, then Green, then Blue, and back to White. When not white, the Color Tile will influence your Puzzle Tile by toggling that color on all of the corresponding faces.

For instance, if your Color Tile is red and has its top face lit, then attaching it to your Puzzle Tile will toggle the red color on *its* top face.

Remember: Magenta is a combination of red and blue, Cyan combines blue and green, and Yellow combines red and green. Of course white combines all three.

Detach and rotate tiles so that they influence the faces you want on the Puzzle Tile.

Double click a Color tile during play to reset it to white. Or double click the Puzzle tile during play to reset *all* attached Color tiles to white.

### Winning

Can you deduce the correct combinations of colors and rotations for your Color Tiles that will result in a Blackout on the Puzzle Tile?

There is at least one guaranteed solution for every puzzle. Some have more than one solution or may even be solvable without using all of the Color Tiles.

Once you win, click any tile to reset all tiles to their starting state. You can then select a new difficulty or number of Color Tiles for your next puzzle.

Good luck!

### Reset

Long press any tile at any time to reset all tiles back to their initial state.
