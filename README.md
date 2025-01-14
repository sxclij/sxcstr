# README

## Overview

This project implements a minimal HTTP server in C with the following constraints:
- **No Global Variables:** Ensures all state is handled locally, improving modularity and testability.
- **Single Thread:** Operates in a single-threaded environment, avoiding complexities of concurrency.
- **Standard Library Only:** Relies solely on the C standard library for portability and simplicity.
- **No Dynamic Memory Allocation:** Avoids runtime memory allocation to ensure predictability and reduce potential memory management issues.

## Features
- Serves static files from a predefined directory.
- Supports common MIME types (e.g., HTML, CSS, JavaScript, images).
- Handles HTTP `GET` requests.
- Customizable stack size using `setrlimit`.

## Dependencies
- A C99-compatible compiler.
- Linux environment for sockets and file system interactions.

## Build Instructions
1. Clone this repository:
   ```bash
   git clone <repository_url>
   cd <repository_name>/src
   ```
2. Compile the code using `gcc`:
   ```bash
   gcc -o main main.c
   ```

## Usage
1. Run the server:
   ```bash
   ./main
   ```
2. Access the server at `http://localhost:8080` using a web browser or a tool like `curl`.

## Code Structure
### Main Components
1. **`init` Function:**
   - Sets up the server socket and stack size.
   - Binds the server to the specified port.

2. **`loop` Function:**
   - Listens for incoming client connections.
   - Processes HTTP requests and sends appropriate responses.

3. **`handle_get` Function:**
   - Parses the HTTP `GET` request.
   - Determines the requested file and MIME type.
   - Reads the file and constructs the HTTP response.

4. **`deinit` Function:**
   - Closes the server socket and performs cleanup.

### Helper Structures
- **`struct string`:** Represents an immutable string.
- **`struct vec`:** Represents a mutable string buffer for constructing responses.

### Memory Management
- All buffers are stack-allocated with predefined sizes.
- No calls to `malloc` or `free` are used.

### Constraints and Limitations
- The server does not support HTTP methods other than `GET`.
- File paths and responses are limited to a maximum size of 16 MB (configurable via `BUFFER_SIZE`).
- Designed for local development and testing; not suitable for production use.

## Extending the Server
1. **Adding MIME Types:**
   - Update the `handle_get` function with additional file extensions and MIME types.

2. **Supporting More Methods:**
   - Add functions to handle methods like `POST` or `PUT`.

3. **Improving Performance:**
   - Introduce multi-threading or asynchronous I/O (outside the current constraints).

## License
This project is licensed under the MIT License. See `LICENSE` for details.

## Acknowledgments
- Inspired by the simplicity and performance of lightweight HTTP servers.

Feel free to contribute by submitting issues or pull requests!

