# Simple HTTP Server in C

## Overview

This code was created for [sxclij.com](https://sxclij.com).
This project implements a simple HTTP server written in C. The server can handle basic `GET` requests and serves files based on the requested path. It supports multiple MIME types and uses a custom string manipulation library for efficient memory management.

## Features

- Handles basic HTTP `GET` requests.
- Serves static files from the `./routes` directory.
- Automatically assigns appropriate `Content-Type` headers based on file extensions.
- Responds with a default page (`index.html`) for requests to `/`.
- Logs errors when files cannot be found.
- Supports the following MIME types:
  - `text/html`, `text/plain`, `application/json`, and many more.
  - Image formats like `image/png`, `image/jpeg`, `image/svg+xml`, and `image/webp`.
  - Video formats like `video/mp4` and `video/webm`.
  - Font formats like `font/woff` and `font/ttf`.

## Usage

### Prerequisites

- A C compiler (e.g., GCC).
- Basic knowledge of networking and file handling in C.

### Building the Project

Compile the project using the following command:

```bash
gcc -o main main.c
```

### Running the Server

Run the server with:

```bash
./main
```

By default, the server listens on port `8080`. You can connect to it via a web browser or tools like `curl`:

```bash
curl http://localhost:8080
```

### Directory Structure

The server expects static files to be located in the `./routes` directory. For example:

- `./routes/index.html` - Default page served for `/`.
- `./routes/favicon.svg` - Favicon served for `/favicon.ico`.
- `./routes/<other-path>` - File served for other requests.

### Adding Content

To add files for the server to serve, place them in the `./routes` directory. The server automatically determines the content type based on the file extension.

## Code Highlights

### Global Struct

The `global` struct consolidates all buffers and configurations:

- `buf_recv`, `buf_send`: Buffers for receiving and sending data.
- `buf_file`, `buf_path`: Buffers for file content and paths.
- `http_server`, `http_client`: Socket descriptors.
- `http_address`: Stores the server's address information.

### String Utilities

Custom string utilities are used for:
- Creation (`string_make` and `string_make_str`).
- Copying and concatenation (`string_cpy`, `string_cat`).
- Comparison (`string_cmp`, `string_cmp_str`).

### File Handling

The `file_read` function reads the content of files into the provided buffer. If the file does not exist, an error is logged.

### Content Type Detection

The `http_contenttype` function maps file extensions to their corresponding MIME types. If an extension is unknown, `application/octet-stream` is used as the default.

### HTTP Request Handling

The server:
1. Parses the `GET` request to determine the requested path.
2. Reads the file content based on the path.
3. Generates an appropriate HTTP response with headers and content.

## Future Improvements

- **Dynamic Configuration:** Allow configuration of the server's port and root directory through command-line arguments or environment variables.
- **Enhanced Error Handling:** Add custom error pages for `404` and other HTTP status codes.
- **Support for Additional HTTP Methods:** Extend functionality to support `POST`, `PUT`, and `DELETE` methods.

## License

This project is licensed under the MIT License. See `LICENSE` for details.

## Acknowledgments

- Inspired by minimal HTTP server examples in C.
- Designed for simplicity and educational purposes.