using System;
using System.Collections.Generic;
using System.Linq;

namespace MinesweeperSharp
{

////////////////////////////////////////////////////////////////////////////////

	enum MinesweeperCellLabel
	{
		None,
		Bomb,
		Question
	};

	interface IMinesweeperCell {
		// whether the cell is opened (exception safe)
		bool IsOpened { get; }
		// whether the cell is bomb (only for opened cell, IsOpened() == true)
		bool IsBomb { get; }
		// returns number of neighbor bombs (only for opened and not bomb cell)
		uint NumberOfNeighborBombs { get; }

		// returns user's label of the cell (exception safe)
		// sets the label of the cell (only for closed cell, IsOpened() == false)
		MinesweeperCellLabel Label { get; set; }

		// opens the cell
		// - for a cell labeled as bomb or question does nothing
		// - for a closed cell opens it and its neighbors (if there is no bomb in all neigbors)
		// - for a opened cell opens its neighbors if all neighor bombs are labeled (even wrong labeled)
		void Open();
	};

	////////////////////////////////////////////////////////////////////////////////

	// The state of the game
	enum MinesweeperGameState
	{
		Active,
		Failure,
		Success
	};

	interface IMinesweeperGame
	{
		// returns current game status (exception safe)
		MinesweeperGameState GameState { get; }

		// access to current game parameters (exception safe)
		uint Rows { get; }
		uint Columns { get; }
		uint Bombs { get; }

		// starts a new game with passed parameters (throw an exception if failed)
		void NewGame( uint rows, uint columns, uint bombs );
		// starts a new game with current (or default) parameters (throw an exception if failed)
		void NewGame();
		// restarts current game (throw an exception if failed)
		void RestartGame();

		// access to the game cells
		IMinesweeperCell Cell( uint row, uint column );

		// returns positions of cells which have been modified since previous call
		// note: the method resets the internal cell modified flag
		// I believe such methods should be qualified as const
		Tuple<uint, uint>[] ModifiedCells();
	};

	////////////////////////////////////////////////////////////////////////////////

	interface IMinesweeperCellCallback {
		void OnOpen( CMinesweeperCell cell );
		void OnModified( CMinesweeperCell cell );
	};

	class CMinesweeperCell : IMinesweeperCell {
		private readonly IMinesweeperCellCallback callback;
		private readonly uint index;
		private bool isOpened;
		private bool isBomb;
		private MinesweeperCellLabel label;
		private uint numberOfNeighborBombs;

		public CMinesweeperCell( IMinesweeperCellCallback callback, uint index )
		{
			this.callback = callback;
			this.index = index;
			this.isOpened = false;
			this.isBomb = false;
			this.label = MinesweeperCellLabel.None;
			this.numberOfNeighborBombs = 0;
		}

		public uint Index { get { return index; } }

		public bool InternalIsBomb {
			get {
				return isBomb;
			}
			set {
				if( IsOpened ) {
					throw new InvalidOperationException();
				}
				isBomb = value;
				numberOfNeighborBombs = 0;
			}
		}

		public uint InternalNumberOfNeighborBombs {
			get {
				return numberOfNeighborBombs;
			}
			set {
				if( IsOpened ) {
					throw new InvalidOperationException();
				}
				isBomb = false;
				numberOfNeighborBombs = value;
			}
		}

		private void onModified()
		{
			callback.OnModified( this );
		}

		public bool IsOpened {
			get {
				return isOpened;
			}
			set {
				if( isOpened != value ) {
					isOpened = value;
					onModified();
				}
			}
		}

		public bool IsBomb {
			get {
				if( !IsOpened ) {
					throw new InvalidOperationException();
				}
				return isBomb;
			}
			set {
				if( IsOpened ) {
					throw new InvalidOperationException();
				}
				isBomb = value;

			}
		}

		public uint NumberOfNeighborBombs {
			get {
				if( IsBomb ) {
					throw new InvalidOperationException();
				}
				return numberOfNeighborBombs;
			}
		}

		public MinesweeperCellLabel Label {
			get {
				if( IsOpened ) {
					throw new InvalidOperationException();
				}
				return label;
			}
			set {
				if( IsOpened ) {
					throw new InvalidOperationException();
				}
				if( label != value ) {
					label = value;
					onModified();
				}
			}
		}

		public void Open()
		{
			callback.OnOpen( this );
		}
	};

	////////////////////////////////////////////////////////////////////////////////

	class CMinesweeperGame : IMinesweeperGame, IMinesweeperCellCallback {
		private MinesweeperGameState state;
		private uint rows;
		private uint columns;
		private uint bombs;
		private CMinesweeperCell[] cells;
		private uint numberOfOpenedCells;
		private HashSet<uint> modifiedCellIndices;

		public static IMinesweeperGame Game()
		{
			CMinesweeperGame game = new CMinesweeperGame();
			game.NewGame( 9, 9, 10 );
			return game;
		}

		private CMinesweeperGame()
		{
			state = MinesweeperGameState.Failure;
			rows = 0;
			columns = 0;
			bombs = 0;
			cells = null;
			numberOfOpenedCells = 0;
			modifiedCellIndices = new HashSet<uint>();
		}

		private void reset()
		{
			state = MinesweeperGameState.Active;
			numberOfOpenedCells = 0;
			modifiedCellIndices.Clear();
		}
		private List<CMinesweeperCell> findNeighbors( uint index )
		{
			if( !( index < cells.GetLength( 0 ) ) ) {
				throw new InvalidProgramException();
			}

			uint row = index / columns;
			uint column = index % columns;
			bool top = row > 0;
			bool bottom = row < ( rows - 1 );
			bool left = column > 0;
			bool right = column < ( columns - 1 );

			List<CMinesweeperCell> neighbors = new List<CMinesweeperCell>();
			if( top ) {
				if( left ) {
					neighbors.Add( cells[index - columns - 1] );
				}
				neighbors.Add( cells[index - columns] );
				if( right ) {
					neighbors.Add( cells[index - columns + 1] );
				}
			}

			if( left ) {
				neighbors.Add( cells[index - 1] );
			}
			if( right ) {
				neighbors.Add( cells[index + 1] );
			}

			if( bottom ) {
				if( left ) {
					neighbors.Add( cells[index + columns - 1] );
				}
				neighbors.Add( cells[index + columns] );
				if( right ) {
					neighbors.Add( cells[index + columns + 1] );
				}
			}

			return neighbors;
		}
		private void plantBombs()
		{
			Random random = new Random();
			for( uint numberOfPlantedBombs = 0; numberOfPlantedBombs < bombs; ) {
				int index = random.Next( cells.GetLength( 0 ) );
				if( !cells[index].InternalIsBomb ) {
					plantBomb( (uint)index );
					numberOfPlantedBombs++;
				}
			}
		}
		private void plantBomb( uint index )
		{
			if( cells[index].InternalIsBomb ) {
				throw new InvalidProgramException();
			}
			cells[index].InternalIsBomb = true;
			var neighbors = findNeighbors( index );
			foreach( CMinesweeperCell cell in neighbors ) {
				if( !cell.InternalIsBomb ) {
					cell.InternalNumberOfNeighborBombs = cell.InternalNumberOfNeighborBombs + 1;
				}
			}
		}
		private bool open( uint index )
		{
			CMinesweeperCell cell = cells[index];
			if( !cell.IsOpened && cell.Label == MinesweeperCellLabel.None ) {
				cell.IsOpened = true;

				if( cell.InternalIsBomb ) {
					openBombs();
					return false;
				}

				numberOfOpenedCells++;
			}
			return true;
		}
		private void openBombs()
		{
			foreach( CMinesweeperCell cell in cells ) {
				if( cell.InternalIsBomb ) {
					cell.IsOpened = true;
				}
			}
			state = MinesweeperGameState.Failure;
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
		private void openNeighbors( uint index )
		{
			HashSet<uint> unprocessed = new HashSet<uint>();
			unprocessed.Add( index );

			while( unprocessed.Count() > 0 ) {
				uint current = unprocessed.First();
				unprocessed.Remove( current );

				var neighbors = findNeighbors( index );
				foreach( CMinesweeperCell cell in neighbors ) {
					if( !cell.IsOpened ) {
						if( !open( cell.Index ) ) {
							return;
						}
						if( cell.InternalNumberOfNeighborBombs == 0 ) {
							unprocessed.Add( cell.Index );
						}
					}
				}
			}

		}
		private uint CalculateNumberOfNeighborCellsLabeledAsBombs( uint index )
		{
			uint numberOfNeighborCellsLabeledAsBombs = 0;
			var neighbors = findNeighbors( index );
			foreach( CMinesweeperCell cell in neighbors ) {
				if( !cell.IsOpened && cell.Label == MinesweeperCellLabel.Bomb ) {
					numberOfNeighborCellsLabeledAsBombs++;
				}
			}
			return numberOfNeighborCellsLabeledAsBombs;
		}
		private bool hasSuccess()
		{
			if( state != MinesweeperGameState.Active ) {
				throw new InvalidProgramException();
			}
			uint numberOfSafeCells = (uint)cells.GetLength( 0 ) - bombs;
			if( numberOfOpenedCells > numberOfSafeCells ) {
				throw new InvalidProgramException();
			}
			if( numberOfOpenedCells == numberOfSafeCells ) {
				state = MinesweeperGameState.Success;
				return true;
			}
			return false;
		}

		// IMinesweeperGame
		public MinesweeperGameState GameState { get { return state; } }

		public uint Rows { get { return rows; } }
		public uint Columns { get { return columns; } }
		public uint Bombs { get { return bombs; } }

		public void NewGame( uint rows, uint columns, uint bombs )
		{
			if( rows < 9 || rows > 24 || columns < 9 || columns > 30 || bombs < 10
				|| bombs > (uint)( 0.93 * rows * columns ) )
			{
				throw new InvalidOperationException();
			}

			this.rows = rows;
			this.columns = columns;
			this.bombs = bombs;

			// starts the game
			NewGame();
		}
		public void NewGame()
		{
			reset();

			uint size = rows * columns;
			cells = new CMinesweeperCell[size];
			for( uint index = 0; index < size; index++ ) {
				cells[index] = new CMinesweeperCell( this, index );
			}

			plantBombs();
		}
		public void RestartGame()
		{
			reset();

			foreach( CMinesweeperCell cell in cells ) {
				cell.IsOpened = false;
			}
		}

		// access to the game cells
		public IMinesweeperCell Cell( uint row, uint column )
		{
			return cells[row * columns + column];
		}

		public Tuple<uint, uint>[] ModifiedCells()
		{
			Tuple<uint, uint>[] result = new Tuple<uint, uint>[modifiedCellIndices.Count];
			int i = 0;
			foreach( var index in modifiedCellIndices ) {
				result[i] = new Tuple<uint, uint>( index / columns, index % columns );
				i++;
			}
			modifiedCellIndices.Clear();
			return result;
		}

		// IMinesweeperCellCallback
		public void OnOpen( CMinesweeperCell cell )
		{
			if( state != MinesweeperGameState.Active || cells[cell.Index] != cell ) {
				throw new InvalidProgramException();
			}

			if( cell.IsOpened ) {
				if( cell.InternalNumberOfNeighborBombs
					== CalculateNumberOfNeighborCellsLabeledAsBombs( cell.Index ) )
				{
					openNeighbors( cell.Index );
				}
			} else if( open( cell.Index ) ) {
				if( !hasSuccess() && cell.InternalNumberOfNeighborBombs == 0 ) {
					openNeighbors( cell.Index );
				}
			}
		}
		public void OnModified( CMinesweeperCell cell )
		{
			if( state != MinesweeperGameState.Active || cells[cell.Index] != cell ) {
				throw new InvalidProgramException();
			}
			modifiedCellIndices.Add( cell.Index );
		}
	};

////////////////////////////////////////////////////////////////////////////////

}
