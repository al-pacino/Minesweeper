#include <set>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <iostream>
#include <exception>
#include <unordered_map>
#include <unordered_set>

using namespace std;

////////////////////////////////////////////////////////////////////////////////

#define internal_check( condition ) \
	do { \
		if( !( condition ) ) { \
			throw logic_error( "internal program error at " \
				+ to_string( __LINE__ ) + " line in file " + __FILE__ ); \
		} \
	} while( false )

////////////////////////////////////////////////////////////////////////////////

namespace Minesweeper {

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

////////////////////////////////////////////////////////////////////////////////
// Implementation

class CMinesweeperCell;

class IMinesweeperCellCallback {
public:
	virtual void OnOpen( CMinesweeperCell* cell ) = 0;
	virtual void OnModified( CMinesweeperCell* cell ) = 0;
};

class CMinesweeperCell : public IMinesweeperCell {
public:
	CMinesweeperCell( weak_ptr<IMinesweeperCellCallback> callback, size_t index );
	CMinesweeperCell( const CMinesweeperCell& ) = delete;
	CMinesweeperCell& operator=( const CMinesweeperCell& ) = delete;

	void Close();
	size_t Index() const { return index; }
	void SetIsBomb( bool isBomb = true );
	bool GetIsBomb() const { return isBomb; }
	void SetNumberOfNeighborBombs( size_t numberOfNeighborBombs );
	size_t GetNumberOfNeighborBombs() { return numberOfNeighborBombs; }
	void InternalOpen();

	// IMinesweeperCell
	virtual bool IsOpened() const { return isOpened; }
	virtual bool IsBomb() const;
	virtual size_t NumberOfNeighborBombs() const;
	virtual TMinesweeperCellLabel Label() const { return label; }
	virtual void SetLabel( TMinesweeperCellLabel newLabel );
	virtual void Open();

private:
	const weak_ptr<IMinesweeperCellCallback> callback;
	const size_t index;
	bool isBomb;
	bool isOpened;
	TMinesweeperCellLabel label;
	size_t numberOfNeighborBombs;

	void onModified();
};

CMinesweeperCell::CMinesweeperCell( weak_ptr<IMinesweeperCellCallback> _callback,
	size_t _index ) :
	callback( _callback ),
	index( _index ),
	isBomb( false ),
	isOpened( false ),
	label( MCL_None ),
	numberOfNeighborBombs( 0 )
{
}

void CMinesweeperCell::Close()
{
	isOpened = false;
	label = MCL_None;
	onModified();
}

void CMinesweeperCell::SetIsBomb( bool _isBomb )
{
	internal_check( !IsOpened() );
	isBomb = _isBomb;
	numberOfNeighborBombs = 0;
}

void CMinesweeperCell::SetNumberOfNeighborBombs( size_t _numberOfNeighborBombs )
{
	internal_check( !IsOpened() );
	isBomb = false;
	numberOfNeighborBombs = _numberOfNeighborBombs;
}

void CMinesweeperCell::InternalOpen()
{
	if( !isOpened ) {
		isOpened = true;
		onModified();
	}
}

bool CMinesweeperCell::IsBomb() const
{
	internal_check( IsOpened() );
	return isBomb;
}

size_t CMinesweeperCell::NumberOfNeighborBombs() const
{
	internal_check( !IsBomb() );
	return numberOfNeighborBombs;
}

void CMinesweeperCell::SetLabel( TMinesweeperCellLabel newLabel )
{
	internal_check( !IsOpened() );
	if( label != newLabel ) {
		label = newLabel;
		onModified();
	}
}

void CMinesweeperCell::Open()
{
	callback.lock()->OnOpen( this );
}

void CMinesweeperCell::onModified()
{
	callback.lock()->OnModified( this );
}

////////////////////////////////////////////////////////////////////////////////

class CMinesweeperGame :
	public IMinesweeperGame,
	public IMinesweeperCellCallback,
	public enable_shared_from_this<CMinesweeperGame> {
public:
	CMinesweeperGame( const CMinesweeperGame& ) = delete;
	CMinesweeperGame& operator=( const CMinesweeperGame& ) = delete;

	static shared_ptr<CMinesweeperGame> CreateGame( size_t rows = 9,
		size_t columns = 9, size_t bombs = 10 );

	// IMinesweeperGame
	virtual TMinesweeperGameState GameState() const { return state; }
	virtual size_t Rows() const { return rows; }
	virtual size_t Columns() const { return columns; }
	virtual size_t Bombs() const { return bombs; }
	virtual void NewGame( size_t rows, size_t columns, size_t bombs );
	virtual void NewGame();
	virtual void RestartGame();
	virtual CMinesweeperCell* Cell( size_t row, size_t column );
	virtual const CMinesweeperCell* Cell( size_t row, size_t column ) const;
	virtual vector<pair<size_t, size_t>> ModifiedCells() const;

	// IMinesweeperCellCallback
	virtual void OnOpen( CMinesweeperCell* cell );
	virtual void OnModified( CMinesweeperCell* cell );

private:
	TMinesweeperGameState state;
	size_t rows;
	size_t columns;
	size_t bombs;
	vector<unique_ptr<CMinesweeperCell>> cells;
	size_t numberOfOpenedCells;
	mutable unordered_set<size_t> modifiedCellIndices;

	void reset();
	vector<CMinesweeperCell*> findNeighbors( size_t index ) const;
	void plantBombs();
	void plantBomb( size_t index );
	bool open( size_t index );
	void openBombs();
	void openNeighbors( size_t index );
	size_t CalculateNumberOfNeighborCellsLabeledAsBombs( size_t index ) const;
	bool hasSuccess();

	CMinesweeperGame();
};

CMinesweeperGame::CMinesweeperGame() :
	state( MGS_Failure ),
	rows( 0 ),
	columns( 0 ),
	bombs( 0 ),
	numberOfOpenedCells( 0 )
{
}

shared_ptr<CMinesweeperGame> CMinesweeperGame::CreateGame( size_t rows,
	size_t columns, size_t bombs )
{
	shared_ptr<CMinesweeperGame> game( new CMinesweeperGame );
	game->NewGame( rows, columns, bombs );
	return game;
}

void CMinesweeperGame::NewGame( size_t _rows, size_t _columns, size_t _bombs )
{
	internal_check( _rows >= 9 && _rows <= 24 );
	internal_check( _columns >= 9 && _columns <= 30 );
	internal_check( _bombs >= 10 && _bombs <= static_cast<size_t>( 0.93 * _rows * _columns ) );

	rows = _rows;
	columns = _columns;
	bombs = _bombs;

	// starts the game
	NewGame();
}

void CMinesweeperGame::NewGame()
{
	reset();

	//shared_ptr<CMinesweeperGame> ptr( this );

	cells.resize( rows * columns );
	for( size_t index = 0; index < cells.size(); index++ ) {
		cells[index].reset( new CMinesweeperCell( shared_from_this(), index ) );
	}

	plantBombs();
}

void CMinesweeperGame::RestartGame()
{
	reset();

	for( size_t index = 0; index < cells.size(); index++ ) {
		cells[index]->Close();
	}
}

CMinesweeperCell* CMinesweeperGame::Cell( size_t row, size_t column )
{
	internal_check( row < rows );
	internal_check( column < columns );
	return cells[row * columns + column].get();
}

const CMinesweeperCell* CMinesweeperGame::Cell( size_t row, size_t column ) const
{
	return const_cast<CMinesweeperGame&>( *this ).Cell( row, column );
}

vector<pair<size_t, size_t>> CMinesweeperGame::ModifiedCells() const
{
	vector<pair<size_t, size_t>> result;
	result.reserve( modifiedCellIndices.size() );
	for( auto i = modifiedCellIndices.cbegin(); i != modifiedCellIndices.cend(); ++i ) {
		result.push_back( make_pair( ( *i ) / columns, ( *i ) % columns ) );
	}
	modifiedCellIndices.clear();
	return move( result );
}

void CMinesweeperGame::OnOpen( CMinesweeperCell* cell )
{
	internal_check( cell != nullptr );
	internal_check( state == MGS_Active );

	const size_t index = cell->Index();
	internal_check( index < cells.size() );
	internal_check( cells[index].get() == cell );

	if( cell->IsOpened() ) {
		if( cell->GetNumberOfNeighborBombs() == CalculateNumberOfNeighborCellsLabeledAsBombs( index ) ) {
			openNeighbors( index );
		}
	} else if( open( index ) ) {
		if( !hasSuccess() && cell->GetNumberOfNeighborBombs() == 0 ) {
			openNeighbors( index );
		}
	}
}

void CMinesweeperGame::OnModified( CMinesweeperCell* cell )
{
	internal_check( cell != nullptr );
	internal_check( state == MGS_Active );

	const size_t index = cell->Index();
	internal_check( index < cells.size() );
	internal_check( cells[index].get() == cell );

	modifiedCellIndices.insert( index );
}

void CMinesweeperGame::reset()
{
	state = MGS_Active;
	modifiedCellIndices.clear();
	numberOfOpenedCells = 0;
}

vector<CMinesweeperCell*> CMinesweeperGame::findNeighbors( size_t index ) const
{
	internal_check( index < cells.size() );

	const size_t row = index / columns;
	const size_t column = index % columns;
	const bool top = row > 0;
	const bool bottom = row < ( rows - 1 );
	const bool left = column > 0;
	const bool right = column < ( columns - 1 );

	vector<CMinesweeperCell*> neighbors;
	if( top ) {
		if( left ) {
			neighbors.push_back( cells[index - columns - 1].get() );
		}
		neighbors.push_back( cells[index - columns].get() );
		if( right ) {
			neighbors.push_back( cells[index - columns + 1].get() );
		}
	}

	if( left ) {
		neighbors.push_back( cells[index - 1].get() );
	}
	if( right ) {
		neighbors.push_back( cells[index + 1].get() );
	}

	if( bottom ) {
		if( left ) {
			neighbors.push_back( cells[index + columns - 1].get() );
		}
		neighbors.push_back( cells[index + columns].get() );
		if( right ) {
			neighbors.push_back( cells[index + columns + 1].get() );
		}
	}

	return move( neighbors );
}

void CMinesweeperGame::plantBombs()
{
	random_device randomDevice;
	uniform_int_distribution<size_t> randomGenerator( 0, cells.size() - 1 );

	for( size_t numberOfPlantedBombs = 0; numberOfPlantedBombs < bombs; ) {
		const size_t index = randomGenerator( randomDevice );
		if( !cells[index]->GetIsBomb() ) {
			plantBomb( index );
			numberOfPlantedBombs++;
		}
	}
}

void CMinesweeperGame::plantBomb( size_t index )
{
	internal_check( !cells[index]->GetIsBomb() );
	cells[index]->SetIsBomb();

	vector<CMinesweeperCell*> neighbors = findNeighbors( index );
	for( auto i = neighbors.begin(); i != neighbors.end(); ++i ) {
		CMinesweeperCell* cell = *i;
		cell->SetNumberOfNeighborBombs( cell->GetNumberOfNeighborBombs() + 1 );
	}
}

bool CMinesweeperGame::open( size_t index )
{
	CMinesweeperCell* cell = cells[index].get();
	if( !cell->IsOpened() && cell->Label() == MCL_None ) {
		cell->InternalOpen();

		if( cell->GetIsBomb() ) {
			openBombs();
			return false;
		}

		numberOfOpenedCells++;
	}
	return true;
}

void CMinesweeperGame::openBombs()
{
	for( auto i = cells.begin(); i != cells.end(); ++i ) {
		CMinesweeperCell* cell = i->get();
		if( cell->GetIsBomb() ) {
			cell->InternalOpen();
		}
	}
	state = MGS_Failure;
}

// OnOpen
// I. The cell is not opened:
//    1. The cell is bomb:
//       Boom!
//    2. The cell has N > 0 bombs in its neighbors:
//       Just open the cell!
//    3. The cell has no bombs in its neighbors:
//       Open the cell and its neighbors (recursively)
// II. The cell is already opened:
//     2. Number of labeled neighbor bombs equal to number of neighbor bombs:
//        Open neighbors

void CMinesweeperGame::openNeighbors( size_t index )
{
	unordered_set<size_t> unprocessed;
	unprocessed.insert( index );

	while( !unprocessed.empty() ) {
		const size_t current = *unprocessed.begin();
		unprocessed.erase( unprocessed.begin() );

		vector<CMinesweeperCell*> neighbors = findNeighbors( current );
		for( auto i = neighbors.begin(); i != neighbors.end(); ++i ) {
			const size_t neighborIndex = ( *i )->Index();
			if( !cells[neighborIndex]->IsOpened() ) {
				if( !open( neighborIndex ) ) {
					return;
				}
				if( cells[neighborIndex]->GetNumberOfNeighborBombs() == 0 ) {
					unprocessed.insert( neighborIndex );
				}
			}
		}
	}
}

size_t CMinesweeperGame::CalculateNumberOfNeighborCellsLabeledAsBombs( size_t index ) const
{
	vector<CMinesweeperCell*> neighbors = findNeighbors( index );
	size_t numberOfNeighborCellsLabeledAsBombs = 0;
	for( auto i = neighbors.cbegin(); i != neighbors.cend(); ++i ) {
		CMinesweeperCell* cell = *i;
		if( !cell->IsOpened() && cell->Label() == MCL_Bomb ) {
			numberOfNeighborCellsLabeledAsBombs++;
		}
	}
	return numberOfNeighborCellsLabeledAsBombs;
}

bool CMinesweeperGame::hasSuccess()
{
	internal_check( state == MGS_Active );
	const size_t numberOfSafeCells = cells.size() - bombs;
	internal_check( numberOfOpenedCells <= numberOfSafeCells );
	if( numberOfOpenedCells == numberOfSafeCells ) {
		state = MGS_Success;
	}
	return ( state == MGS_Success );
}

} // end of Minesweeper namespace

////////////////////////////////////////////////////////////////////////////////

using namespace Minesweeper;

void Draw( const IMinesweeperGame* game )
{
	for( size_t i = 0; i < game->Rows(); i++ ) {
		for( size_t j = 0; j < game->Columns(); j++ ) {
			const IMinesweeperCell* cell = game->Cell( i, j );
			if( cell->IsOpened() ) {
				if( cell->IsBomb() ) {
					cout << "*";
				} else {
					size_t bombs = cell->NumberOfNeighborBombs();
					if( bombs == 0 ) {
						cout << "O";
					} else {
						cout << bombs;
					}
				}
			} else {
				cout << "-";
			}
		}
		cout << endl;
	}
	cout << endl;
}

int main( int argc, const char* argv[] )
{
	try {
		shared_ptr<IMinesweeperGame> game = CMinesweeperGame::CreateGame( 9, 9 );
		Draw( game.get() );

		random_device randomDevice;
		size_t max = game->Rows() * game->Columns() - 1;
		uniform_int_distribution<size_t> randomGenerator( 0, max );

		while( game->GameState() == MGS_Active ) {
			const size_t index = randomGenerator( randomDevice );
			const size_t row = index / game->Columns();
			const size_t column = index % game->Columns();
			game->Cell( row, column )->Open();
			Draw( game.get() );
		}
	} catch( exception& e ) {
		cerr << e.what() << endl;
		return 1;
	}

	return 0;
}
