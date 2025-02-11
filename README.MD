# Order Book System

# Overview

The Order Book System is a high-performance C implementation for managing and matching buy and sell orders in a financial trading system. This project efficiently handles order matching, insertion, and cancellation while maintaining an organized structure to optimize lookup and execution.

# Features

Efficiently manages buy and sell orders.

Implements an order-matching algorithm.

Supports order insertion, modification, and cancellation.

Uses data structures optimized for fast order retrieval.

# Installation

To build and run the project, follow these steps:

# Clone the repository
git clone <repository-url>
cd order-book

# Compile the program
gcc -o order_book order_book.c -Wall -Wextra -O2

# Run the program
./order_book

# Usage

The system operates by processing commands such as adding, canceling, and matching orders.

# Example Commands:

Add Order: ADD BUY 100 10.50  (Adds a buy order of 100 units at $10.50)

Cancel Order: CANCEL 12345 (Cancels order with ID 12345)

Match Orders: The system automatically matches compatible buy and sell orders.

Code Structure

order_book.c – Core implementation of the order book.

order_book.h – Header file defining the data structures and function prototypes.

main.c – Entry point for the program.

Makefile – Automates the build process.

Future Improvements

Implement persistent storage for orders.

Optimize data structures for lower latency.

Add support for different order types (limit, market, stop-loss).

Enhance logging and debugging features.

License

This project is licensed under the MIT License.

Contributors

Shane Janney

Contributions welcome! Open a pull request if you'd like to contribute.

C
