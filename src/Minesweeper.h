#pragma once

#include <memory>
#include <vector>

namespace Minesweeper {

////////////////////////////////////////////////////////////////////////////////

using namespace std; // only is in Mineseeper namespace

////////////////////////////////////////////////////////////////////////////////

enum TMinesweeperCellLabel {
	MCL_None,
	MCL_Bomb,
	MCL_Question
};

class IMinesweeperCell {
public:
	// destructor
	virtual ~IMinesweeperCell() = 0 {}

	// whether the cell is opened (exception safe)
	virtual bool IsOpened() const = 0;
	// whether the cell is bomb (only for opened cell, IsOpened() == true)
	virtual bool IsBomb() const = 0;
	// returns number of neighbor bombs (only for opened and not bomb cell)
	virtual size_t NumberOfNeighborBombs() const = 0;

	// returns user's label of the cell (exception safe)
	virtual TMinesweeperCellLabel Label() const = 0;
	// sets the label of the cell (only for closed cell, IsOpened() == false)
	virtual void SetLabel( TMinesweeperCellLabel newLabel ) = 0;

	// opens the cell
	// - for a cell labeled as bomb or question does nothing
	// - for a closed cell opens it and its neighbors (if there is no bomb in all neigbors)
	// - for a opened cell opens its neighbors if all neighor bombs are labeled (even wrong labeled)
	virtual void Open() = 0;
};

////////////////////////////////////////////////////////////////////////////////

// The state of the game
enum TMinesweeperGameState {
	MGS_Active,
	MGS_Failure,
	MGS_Success
};

class IMinesweeperGame {
public:
	// destructor
	virtual ~IMinesweeperGame() = 0 {}

	// returns current game status (exception safe)
	virtual TMinesweeperGameState GameState() const = 0;

	// access to current game parameters (exception safe)
	virtual size_t Rows() const = 0;
	virtual size_t Columns() const = 0;
	virtual size_t Bombs() const = 0;

	// starts a new game with passed parameters (throw an exception if failed)
	virtual void NewGame( size_t rows, size_t columns, size_t bombs ) = 0;
	// starts a new game with current (or default) parameters (throw an exception if failed)
	virtual void NewGame() = 0;
	// restarts current game (throw an exception if failed)
	virtual void RestartGame() = 0;

	// access to the game cells
	virtual IMinesweeperCell* Cell( size_t row, size_t column ) = 0;
	virtual const IMinesweeperCell* Cell( size_t row, size_t column ) const = 0;

	// returns positions of cells which have been modified since previous call
	// note: the method resets the internal cell modified flag
	// I believe such methods should be qualified as const
	virtual vector<pair<size_t, size_t>> ModifiedCells() const = 0; // move semantic
};

shared_ptr<IMinesweeperGame> CreateGame( size_t rows = 9, size_t columns = 9,
	size_t bombs = 10 );

////////////////////////////////////////////////////////////////////////////////

} // end of Minesweeper namespace

////////////////////////////////////////////////////////////////////////////////
