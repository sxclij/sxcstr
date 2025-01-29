# Minimal HTTP Server in C

This project implements a basic HTTP server in C, designed for educational purposes and local testing. It adheres to several constraints to demonstrate core concepts without the complexities of production-level servers.

## Overview

The server is built with the following principles:

-   **No Global Variables:** All state is managed within functions, promoting modularity and testability.
-   **Single-Threaded:** Operates on a single thread to avoid concurrency issues.
-   **Standard Library Only:** Uses only the C standard library for maximum portability and simplicity.
-   **No Dynamic Memory Allocation:** Avoids `malloc` and `free` to ensure predictable memory usage and prevent memory leaks.

## Features

-   Serves static files from the `./routes` directory.
-   Supports common MIME types based on file extensions (configured in `routes/setting.json`).
-   Handles HTTP `GET` requests.
-   Customizable stack size using `setrlimit`.
-   Uses a treap data structure for efficient JSON parsing.

## Dependencies

-   A C99-compatible compiler (like GCC).
-   A Linux-like environment for socket and file system interactions.

## Build Instructions

1.  Clone the repository:

    ```bash
    git clone https://github.com/sxclij/sxclijcom
    cd sxclijcom/src
    ```
2.  Compile the code:

    ```bash
    gcc -o main main.c
    ```

## Usage

1.  Run the server:

    ```bash
    ./main
    ```
2.  Access the server at `http://localhost:8080` using a web browser or `curl`.

## Code Structure

### Main Components

1.  **`init_limit` Function:**
    -   Sets the stack size using `setrlimit`.

2.  **`init_socket` Function:**
    -   Creates and configures the server socket.
    -   Binds the socket to the specified port (8080).

3.  **`init_setting` Function:**
    -   Reads and parses the `routes/setting.json` file to configure MIME types.

4.  **`loop` Function:**
    -   Accepts incoming client connections.
    -   Calls the `handle` function to process requests.

5.  **`handle` Function:**
    -   Receives HTTP requests from clients.
    -   Calls `handle_get` for `GET` requests.

6.  **`handle_get` Function:**
    -   Parses the HTTP `GET` request to determine the requested file.
    -   Reads the file from the `./routes` directory.
    -   Constructs and sends the HTTP response with the correct MIME type.

### Helper Structures

-   **`struct string`:** Represents an immutable string.
-   **`struct vec`:** Represents a mutable string buffer for constructing responses.
-   **`struct json`:** Represents a JSON node, used for parsing the `setting.json` file.

### Memory Management

-   All buffers are stack-allocated with predefined sizes (defined by `BUFFER_SIZE`).
-   No dynamic memory allocation (`malloc` or `free`) is used.

### JSON Parsing

-   The server uses a custom JSON parser that tokenizes the input and builds a treap data structure for efficient lookups.
-   The `json_parse`, `json_get`, and `json_tovec` functions handle parsing, accessing, and serializing JSON data.

### Constraints and Limitations

-   Only supports the HTTP `GET` method.
-   File paths and responses are limited to a maximum size of 16 MB (configurable via `BUFFER_SIZE`).
-   Designed for local development and testing; not suitable for production use.
-   The server does not handle malformed requests or errors gracefully.

## Extending the Server

1.  **Adding MIME Types:**
    -   Modify the `routes/setting.json` file to include additional file extensions and their corresponding MIME types.

2.  **Supporting More Methods:**
    -   Implement functions to handle methods like `POST`, `PUT`, etc.

3.  **Improving Performance:**
    -   Introduce multi-threading or asynchronous I/O (outside the current constraints).

## License

This project is licensed under the MIT License. See `LICENSE` for details.

## Acknowledgments

-   Inspired by the simplicity and performance of lightweight HTTP servers.

Feel free to contribute by submitting issues or pull requests!
