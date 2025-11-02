#include <iostream>
#include <string>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>
#include <cstdio>

// --- STB Implementation ---
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
// --------------------------

// --- Platform-specific includes ---
#ifdef _WIN32
#include <io.h>     // for _setmode, _isatty, _fileno
#include <fcntl.h>  // for _O_BINARY
#else
#include <unistd.h> // for isatty, STDIN_FILENO
#include <stdio.h>  // for fileno
#endif
// ----------------------------------


// --- STB Callbacks for Piping ---

// read callback for stbi_load (reads from stdin)
static int read_stdin(void *user, char *data, int size) {
    // user is (void*)stdin
    return static_cast<int>(fread(data, 1, size, static_cast<FILE *>(user)));
}

// skip callback for stbi_load (skips in stdin)
static void skip_stdin(void *user, int n) {
    // user is (void*)stdin
    fseek(static_cast<FILE *>(user), n, SEEK_CUR);
}

// eof callback for stbi_load (checks stdin)
static int eof_stdin(void *user) {
    // user is (void*)stdin
    return feof(static_cast<FILE *>(user));
}

// write callback for stbi_write (writes to stdout)
static void write_stdout(void *context, void *data, int size) {
    // context is (void*)stdout
    fwrite(data, 1, size, static_cast<FILE *>(context));
}

// --------------------------------


// --- Platform-specific helper for binary I/O ---

// sets stdin and stdout to binary mode
// this is critical on windows, and a no-op on posix
static void set_binary_io_mode() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    // on posix, stdin/stdout are already binary-safe
}

// --- Platform-specific helper for pipe detection ---

// checks if stdin is a pipe (and not a user terminal)
static bool is_stdin_a_pipe() {
#ifdef _WIN32
    return !_isatty(_fileno(stdin));
#else
    return !isatty(fileno(stdin)); // fileno is POSIX, STDIN_FILENO is also fine
#endif
}

// -----------------------------------------------


// --- Core Logic ---

struct StbiDeleter {
    void operator()(unsigned char *data) const {
        if (data) {
            stbi_image_free(data);
        }
    }
};

using StbiUniquePtr = std::unique_ptr<unsigned char, StbiDeleter>;

// mode 1: file-to-file conversion
// accepts a flag to enable RLE compression
int convertFileToFile(const char *input_path_c, const char *output_path_c, bool use_rle) {
    const std::string_view input_path(input_path_c);
    const std::string_view output_path(output_path_c);

    // check if input file exists
    if (!std::filesystem::exists(input_path)) {
        std::cerr << "ERROR: Input file not found: " << input_path << std::endl;
        return EXIT_FAILURE;
    }

    int width, height, channels;
    StbiUniquePtr data(stbi_load(input_path.data(), &width, &height, &channels, 0));

    if (!data) {
        std::cerr << "ERROR: Failed to load image: " << stbi_failure_reason() << std::endl;
        return EXIT_FAILURE;
    }

    // log to stderr to avoid polluting stdout
    std::cerr << "INFO: Loaded " << input_path << " ("
            << width << "x" << height << ", " << channels << " channels)" << std::endl;

    if (use_rle) {
        std::cerr << "INFO: Using RLE compression." << std::endl;
        stbi_write_tga_with_rle = 1;
    }

    int success = stbi_write_tga(output_path.data(), width, height, channels, data.get());

    // reset global just in case
    if (use_rle) {
        stbi_write_tga_with_rle = 0;
    }

    if (!success) {
        std::cerr << "ERROR: Failed to write TGA file to " << output_path << std::endl;
        return EXIT_FAILURE;
    }

    std::cerr << "INFO: Successfully converted image to " << output_path << std::endl;
    return EXIT_SUCCESS;
}

// mode 2: pipe (stdin-to-stdout) conversion
// accepts a flag to enable RLE compression
int convertPipeToPipe(bool use_rle) {
    // set stdin/stdout to binary mode (critical for windows)
    set_binary_io_mode();

    stbi_io_callbacks callbacks;
    callbacks.read = read_stdin;
    callbacks.skip = skip_stdin;
    callbacks.eof = eof_stdin;

    int width, height, channels;
    // use (void*)stdin as the user context
    StbiUniquePtr data(stbi_load_from_callbacks(&callbacks, (void *) stdin, &width, &height, &channels, 0));

    if (!data) {
        std::cerr << "ERROR: Failed to load image from stdin: " << stbi_failure_reason() << std::endl;
        return EXIT_FAILURE;
    }

    if (use_rle) {
        // log to stderr since stdout is for image data
        std::cerr << "INFO: Using RLE compression." << std::endl;
        stbi_write_tga_with_rle = 1;
    }

    // write to stdout using the callback
    // use (void*)stdout as the context
    int success = stbi_write_tga_to_func(write_stdout, (void *) stdout, width, height, channels, data.get());

    // reset global just in case
    if (use_rle) {
        stbi_write_tga_with_rle = 0;
    }

    if (!success) {
        std::cerr << "ERROR: Failed to write TGA to stdout" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// helper function to show usage
static void show_usage() {
    std::cerr << "Usage: img2tga [-r] <inputFile> <outputFile.tga>" << std::endl;
    std::cerr << "   or: cat <inputFile> | img2tga [-r] > <outputFile.tga>" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -r        Enable RLE compression for the output TGA file." << std::endl;
    std::cerr << "  -h, --help  Show this help message." << std::endl;
}


int main(const int argc, const char *argv[]) {
    bool use_rle = false;
    std::vector<std::string_view> positional_args;

    // --- simple argument parser ---
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (arg == "-r") {
            use_rle = true;
        } else if (arg == "-h" || arg == "--help") {
            show_usage();
            return EXIT_SUCCESS;
        } else if (arg.rfind('-', 0) == 0) {
            std::cerr << "ERROR: Unknown flag: " << arg << std::endl;
            show_usage();
            return EXIT_FAILURE;
        } else {
            // this is a positional argument (a file path)
            positional_args.push_back(arg);
        }
    }
    // --- end of parser ---

    // check parsed arguments to decide which mode to run
    if (positional_args.size() == 2) {
        // file mode: img2tga [-r] in.jpg out.tga
        return convertFileToFile(positional_args[0].data(), positional_args[1].data(), use_rle);
    }
    if (positional_args.empty()) {
        // no file args. check if we are in a pipe.
        if (is_stdin_a_pipe()) {
            // pipe mode: cat in.jpg | img2tga [-r] > out.tga
            return convertPipeToPipe(use_rle);
        }
        // no file args and not in a pipe: user just ran "./img2tga"
        // show help instead of hanging.
        show_usage();
        return EXIT_SUCCESS; // not an error, just showing help
    }
    // error: wrong number of positional arguments (e.g., 1 or 3+)
    std::cerr << "ERROR: Invalid number of arguments." << std::endl;
    show_usage();
    return EXIT_FAILURE;
}
