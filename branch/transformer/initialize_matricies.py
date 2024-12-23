import numpy as np
import sys
import json

def generate_matrix(rows: int, cols: int):
    """
    Generates a matrix of random float values of shape (rows, cols).

    Parameters:
    - rows (int): Number of rows in the matrix.
    - cols (int): Number of columns in the matrix.

    Returns:
    - list[list[float]]: The generated matrix as a nested list.
    """
    matrix = np.random.rand(rows, cols).astype(np.float32)
    return matrix.tolist()  # Convert numpy array to a Python nested list for JSON serialization

def generate_vector(size: int):
    """
    Generates a vector of random float values.

    Parameters:
    - size (int): Number of elements in the vector.

    Returns:
    - list[float]: The generated vector as a list.
    """
    vector = np.random.rand(size).astype(np.float32)
    return vector.tolist()  # Convert numpy array to a Python list for JSON serialization

def create_json_file(file_name: str, d_model: int, num_heads: int, d_ff: int):
    """
    Creates a JSON file containing matrices and biases for a transformer model.

    Parameters:
    - file_name (str): The name of the JSON file to be created.
    - d_model (int): Dimensionality of each vector (number of features per vector).
    - num_heads (int): Number of attention heads.
    - d_ff (int): Dimensionality of the feed-forward layer.
    """
    # Ensure d_model is divisible by num_heads
    if d_model % num_heads != 0:
        raise ValueError(f"d_model ({d_model}) must be divisible by num_heads ({num_heads})")

    data = {
        "queries": generate_matrix(d_model, d_model),  # w_q
        "keys": generate_matrix(d_model, d_model),     # w_k
        "values": generate_matrix(d_model, d_model),   # w_v
        "output": generate_matrix(d_model, d_model),   # w_o
        "w_ff1": generate_matrix(d_model, d_ff),       # Feed-forward weight matrix 1
        "w_ff2": generate_matrix(d_ff, d_model),       # Feed-forward weight matrix 2
        "b_ff1": generate_vector(d_ff),                # Feed-forward bias 1
        "b_ff2": generate_vector(d_model),             # Feed-forward bias 2
        "w_out": generate_vector(d_model),             # Output weight
        "b_out": generate_vector(1)                    # Output bias
    }
    
    # Write the data to a JSON file
    with open(file_name, 'w') as f:
        json.dump(data, f, indent=4)
    
    print(f"JSON file '{file_name}' created with matrices and biases.")

def main():
    if len(sys.argv) != 5:
        print("Usage: python create_json_matrices.py <file_name.json> <d_model> <num_heads> <d_ff>")
        sys.exit(1)
    
    # Get the input arguments
    file_name = sys.argv[1]
    d_model = int(sys.argv[2])
    num_heads = int(sys.argv[3])
    d_ff = int(sys.argv[4])
    
    # Create the JSON file
    create_json_file(file_name, d_model, num_heads, d_ff)

if __name__ == "__main__":
    main()
