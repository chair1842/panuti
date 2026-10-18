# Conway's Game of Life

`conway` is an implementation of Conway's Game of Life on the Panuti OS.

The board starts with a random state if a starting point isnt provided

The simulation is displayed at 60FPS by default.
This can be adjusted within a range of 1-60FPS.

## Usage

```
conway
```

The `-n` argument is used to make the simulation last for N steps.
Default: 300

The `-f` argument determines how many FPS the sim runs at.
Default: 60

The `-s` argument is used to determine where to save the resulting board state.
If this argument is not provided, then nothing will ever be saved.

The `-p` argument will read from a starting point if present.


A starting position can also be supplied to `conway` with this:

```
in startingpos.txt > conway -p
```

## Board format

The board is 80x24 and wraps around at the edges. A starting position is a
plain text grid, one character per cell and one line per row. `#`, `*`, `O`,
`o`, `X`, `x` and `1` are live cells, anything else is dead. Rows or columns
past the board size are ignored, and missing cells are dead. `-s` writes the
final board out in the same format.