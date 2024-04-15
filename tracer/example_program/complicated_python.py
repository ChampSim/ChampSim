import time

# Function to generate Fibonacci sequence
def fibonacci(n):
    a, b = 0, 1
    fib_sequence = []
    while a < n:
        fib_sequence.append(a)
        a, b = b, a + b
    return fib_sequence

# Function to calculate factorial
def factorial(n):
    if n == 0:
        return 1
    else:
        return n * factorial(n - 1)

# Function to find prime numbers
def is_prime(num):
    if num < 2:
        return False
    for i in range(2, int(num ** 0.5) + 1):
        if num % i == 0:
            return False
    return True

# Main function
def main():
    fib_sequence = fibonacci(1000)
    print("Fibonacci Sequence:", fib_sequence)

    fact_result = factorial(5000)
    print("Factorial of 5000:", fact_result)

    prime_numbers = [num for num in range(1000) if is_prime(num)]
    print("Prime Numbers up to 1000:", prime_numbers)
    with open(pipe_path, "w") as pipe:
        pipe.write("stop\n")
    time.sleep(2)

if __name__ == "__main__":
    pipe_path = "/tmp/pinToolPipe"
    with open(pipe_path, "w") as pipe:
        pipe.write("start\n")
    time.sleep(2)
    main()
