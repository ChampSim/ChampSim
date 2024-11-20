import numpy as np
import sys
import os

# Constants defining matrix dimensions
N_ROWS = 128  # Number of rows
M_COLS = 64   # Number of columns

# Available file types
file_types = ['q', 'k', 'v']

def initialize_matrix_file(base_file_name: str, file_type: str, rows: int, cols: int):
    """
    Initializes a binary file with a matrix of random float values in the current directory.
    
    Parameters:
    - base_file_name (str): The base name for the output files.
    - file_type (str): The type identifier ('q', 'k', or 'v') to append to the file name.
    - rows (int): The number of rows in the matrix.
    - cols (int): The number of columns in the matrix.
    """
    # Generate a random matrix of shape (rows, cols) with float32 values
    matrix = np.random.rand(rows, cols).astype(np.float32)
    
    # Define the output file path in the current directory
    output_file = f"{base_file_name}_{file_type}.bin"
    
    # Write the matrix to the binary file
    with open(output_file, 'wb') as f:
        f.write(matrix.tobytes())
    
    print(f"Initialized file '{output_file}' with a {rows}x{cols} matrix.")

def main():
    if len(sys.argv) != 2:
        print("Usage: python initialize_matrices.py <base_file_name>")
        sys.exit(1)
    
    # Get the base file name from the command-line argument
    base_file_name = sys.argv[1]
    
    # Initialize binary files for each file type in the current directory
    for file_type in file_types:
        initialize_matrix_file(base_file_name, file_type, N_ROWS, M_COLS)

if __name__ == "__main__":
    main()
