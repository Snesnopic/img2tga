# **img2tga**

A simple command-line utility to convert various image formats (JPG, PNG, etc.) to the TGA format, powered by stb_image and stb_image_write.  
Overengineered for fun.

## **Usage**

The utility supports both file-to-file and pipe (stdin/stdout) modes.

### **File-to-File**

Provide an input and output path.

```bash
# Basic conversion
./img2tga input.jpg output.tga

# Convert with RLE compression
./img2tga -r input.png output_compressed.tga
```

### **Pipe Mode**

Pipe data via stdin and redirect the output from stdout.

```bash
# Basic conversion
cat input.bmp | ./img2tga > output.tga

# Convert with RLE compression
cat input.jpg | ./img2tga -r > output_compressed.tga
```

### **Options**

* `-r` : Enable RLE compression for the output TGA file.
* `-h`, `--help` : Show the help message.