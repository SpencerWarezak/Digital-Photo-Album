# README.md - CS58 Programming Project #1
### Author - Spencer Warezak
### Date - January 17, 2022

---

### Description:
Though this project does not differ from the instructions or constraints, I felt it would be good to do a brief write up on the use and functionality of the program.

### Compilation
In running this program, use the `Makefile` to `make clean`, cleaning the directory space before using the command line command `make album` to compile the program.

### Usage
When running the program from the command line, use the command: `./album arg1 arg2 ....`
This will prompt you with each image to select a rotation change (or lackthereof) with the values '90' (clockwise), '-90' (counter-clockwise), and 'n' (no rotation). After this, the user will be prompted to give a caption for the image before moving to the next one.

When inputting photo directories or single images as arguments, if non-existent files are run into the program will skip them, continuing to execute all of the other image files that do exist. If no existent files are found, it will generate an `HTML` page with no images or captions.

