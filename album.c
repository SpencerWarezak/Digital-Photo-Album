/*
 * 
 * Spencer Warezak, CS58 Winter 2022
 * 
 * album.c - this is a program to generate a digital photo album based off
 * the user input of a directory of photos
 * 
 * This program generates:
 *      * an index.html file of the images processed with
 *          - a thumbnail sized image, reoriented if wished
 *          - a medium sized image with the same orientation as the thumbnail
 *          - a caption input by the user
 *      * all of the medium sized image files
 *      * all of the thumbnail image files
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define STRING_LEN 64
#define THUMB "./thumbnail"
#define MEDIUM "./medium"
#define FILENAME "index.html"

/* function headers */
int main(int argc, char* argv[]);
int parseCmdLine(int argc, char** argv, char** directory);
int inputString(char* message, char* buffer, int len);
int reorientImage(int rc, int pid, int i, char** thumbnails, char** orientations);
int getResize(int rc, int pid, int i, char** destination, char** directory, char* size, char* name);
int generateHTML(char** thumbnails, char** captions, char** mediumImages, int len);
int cleanUp(char** directory, char** thumbnails, char** mediumImages, char** captions, char** orientations);

/*
 *
 * main - this function is the actual program itself
 * takes two arguments:
 *      - ./album
 *      - an image directory (i.e., photos/image.jpg)
 * 
 * returns the above listed documents/items
 * 
 * - if the user inputs improper values, an exit code of 1 will be returned
 * - if at any point the program fails, an exit code > 1 in ascending order
 * will be used
 * - if the program runs successfully, an exit code of 0 will be returned
 * 
 */
int main(int argc, char* argv[]) 
{
    // input checking
    if (argc == 1) {
        fprintf(stderr, "main -- incorrect syntax, please input 1 or more arguments beyond ./album: ./album dir/photo.jpg dir/photo2.jpg dir/photo3.jpg... \n");
        exit (1);
    }

    // get the number of photos and instantiate a directory to hold
    // the image paths for later use
    char** directory = (char**) malloc(sizeof(char*) * (argc - 1));
    if (directory == NULL) {
        fprintf(stderr, "parseCmdLine -- NULL directory invocation: please make sure we malloc space\n");
        exit (2);
    }

    // parse the command line to get the number of photos
    int numPhotos = parseCmdLine(argc, argv, directory);
    if (numPhotos == -1) {
        fprintf(stderr, "main -- parseCmdLine failed\n");
        exit (3);
    }

    /*
     * for each directory content
     *      * fork() and exec() to create a thumbnail
     *      * wait until the thumbnail has been generated
     *      * then display the thumbnail
     *      * while waiting for the display of one thumbnail
     *      * process the thumbnail of the next
     */
    char** thumbnails = (char**) malloc(sizeof(char*) * numPhotos);
    char** mediumImages = (char**) malloc(sizeof(char*) * numPhotos);
    char** orientations = (char**) malloc(sizeof(char*) * numPhotos);
    char** captions = (char**) malloc(sizeof(char*) * numPhotos);

    int rc, pid;

    pid = getpid();

    for (int i=0; i<numPhotos; i++) {
        rc = fork();
        getResize(rc, pid, i, thumbnails, directory, "10%", THUMB);
    }

    /*
     * now that we have displayed the thumbnail to the user,
     * let's ask the user a few questions about image orientation
     * and captioning
     * 
     * once we have done this, reorient the image for both the 
     * thumbnail, as well as create and reorient a medium-sized image
     */
    for (int i=0; i<numPhotos; i++) {
        rc = fork();

        if (rc == 0) {
            execlp("display", "display", thumbnails[i], NULL);
        }
        else if (rc > 0) {
            char* buffer;
            buffer = (char*) malloc(STRING_LEN);

            // collect the user's response
            inputString("Would you like to rotate the image either clockwise or counter-clockwise? [90/-90/n]",
            buffer, STRING_LEN);
            
            int valid = 0;
            while (valid == 0) {
                if (strcmp(buffer, "90") == 0 || strcmp(buffer, "-90") == 0 ||
                    strcmp(buffer, "n") == 0) valid = 1;
                else {
                fprintf(stderr, "Invalid input. Please input '90', '-90', or 'n'\n");
                inputString("Would you like to rotate the image either clockwise or counter-clockwise? [90/-90/n]",
                    buffer, STRING_LEN);
                }
            }
            orientations[i] = buffer;

            // kill child process
            pid = rc;
            kill(pid, SIGTERM);
        }
        else {
            fprintf(stderr, "main -- Fork failed\n");
            exit (5);
        }


        /*
         * let's reorient the image and then do the same for a 
         * medium-sized image
         */

        // reorient thumbnail images
        rc = fork();
        reorientImage(rc, pid, i, thumbnails, orientations);

        // get the medium images
        rc = fork();
        getResize(rc, pid, i, mediumImages, directory, "25%", MEDIUM);

        // reorient the medium images too
        rc = fork();
        reorientImage(rc, pid, i, mediumImages, orientations);


        // grab the caption
        char* caption;
        caption = (char*) malloc(STRING_LEN);
        inputString("What would you like to caption this photo?",
         caption, STRING_LEN);
        
        captions[i] = caption;
    }

    // write to the html file
    generateHTML(thumbnails, captions, mediumImages, numPhotos);

    // cleanup everything!
    cleanUp(directory, thumbnails, mediumImages, captions, orientations);

    // successful run!
    return 0;
}

/*
 * 
 * parseCmdLine - this function parses the arguments, creating a directory with
 * all of the image filenames, as well as the number of photos
 * 
 * returns the count if completed successfully,
 * otherwise this function will return -1.
 * 
 */
int parseCmdLine(int argc, char** argv, char** directory)
{
    // argument checking
    if (argv == NULL || directory == NULL) {
        fprintf(stderr, "parseCmdLine -- NULL input value: make sure only non-NULL variables are passed in\n");
        return -1;
    }

    /* 
     * counter for the number of photos
     * now we count the number of photos passed in so
     * we can properly instantiate the directory
     * start a 1 so that we ignore the command line invocation
     */
    int count = 0;
    for (int i=1; i<argc; i++) {
        /*
         * grab the current string and see if:
         *      * it contains all of the files under a certain format
         *      * validate the image file/files to ensure they are existent
         *      * add the curr to the directory if it is an openable file
         */
        char* curr = argv[i];
        // single image
        FILE* fp;
        if ((fp = fopen(curr, "r")) != NULL) {
            directory[count] = curr;
            count++;
            fclose(fp);
        }
        else fprintf(stderr, "parseCmdLine -- File %s not found!\n", curr);
    }

    // successful run!
    return count;
}

/*
 * 
 * inputString - this function requests an input
 * from the user and adds it to a buffer
 * 
 * returns the 0 if completed successfully,
 * otherwise this function will return -1.
 * 
 */
int inputString(char* message, char* buffer, int len)
{
    // arg checking
    if (message == NULL || buffer == NULL || len < 0) {
        fprintf(stderr, "inputString -- invalid input: please enter non-NULL or non-negative inputs");
        return -1;
    }

    // prompt the user and collect their response
    if (message) {
        fprintf(stdout, "%s: ", message);
        fgets(buffer, len, stdin);
    } else return -1;

    // set the buffer's last char to the null-terminating char
    int buffLen;
    buffLen = strlen(buffer);
    if (buffer[buffLen-1] == '\n') {
        buffer[buffLen-1] = '\0';
    }

    // return successfully
    return 0;
}

/*
 * 
 * reorientImage - this function reorients an image
 * based off of user input
 * 
 * returns the 0 if completed successfully,
 * otherwise this function will exit with a value > 1
 * 
 */
int reorientImage(int rc, int pid, int i, char** thumbnails, char** orientations)
{
    if (i < 0 || thumbnails == NULL | orientations == NULL) {
        fprintf(stderr, "reorientImage -- invalid inputs passed in: please enter non-NULL values\n");
        exit (6);
    }
    char* orientation = orientations[i];
    char* path = thumbnails[i];

    if (rc == 0) {
        if (strcmp(orientation, "n") != 0) execlp("convert", "convert", "-rotate", orientation, path, path, NULL);
        else exit (0);
    }
    else if (rc > 0) {
        pid = rc;
        wait(&pid);
    }
    else {
        fprintf(stderr, "main -- Fork failed\n");
        exit (5);
    }

    return 0;
}

/*
 * 
 * getMedium - this function gets a medium-sized image
 * 
 * returns the 0 if completed successfully,
 * otherwise this function will exit with a value > 1
 * 
 */
int getResize(int rc, int pid, int i, char** destination, char** directory, char* size, char* name)
{
    if (i < 0 || destination == NULL | directory == NULL) {
        fprintf(stderr, "getResize -- invalid inputs passed in: please enter non-NULL values\n");
        exit (6);
    }

    char* src = directory[i];
    char* dest = (char*) malloc(strlen(name) + 6); // i.jpg is 5 chars + 1 for null terminating (\0)
    sprintf(dest, "%s%d%s", name, i+1, ".jpg");
    destination[i] = dest;

    if (rc == 0) {
        execlp("convert", "convert", "-resize", size, directory[i], dest, NULL);
    }
    else if (rc > 0) {
        pid = rc;
        wait(&pid);
    }
    else {
        fprintf(stderr, "main -- Fork failed\n");
        exit (5);
    }

    return 0;
}

/*
 * 
 * generateHTML - this function generates an HTML
 * file with the thumbnails, captions, and medium-sized
 * images
 * 
 * returns the 0 if completed successfully,
 * otherwise this function will exit with a value > 1
 * 
 */
int generateHTML(char** thumbnails, char** captions, char** mediumImages, int len)
{
    FILE* fp = fopen(FILENAME, "w");
    if (fp == NULL || thumbnails == NULL || captions == NULL || 
        mediumImages == NULL || len < 0) {
            fprintf(stderr, "generateHTML -- operation failed: NULL inputs\n");
            exit (6);
    }

    char* html = "<!DOCTYPE html><html><title>Your Digital Photo Album</title><body><h1>Here is your Digital Photo Album!</h1>";
    fprintf(fp, "%s\n", html);

    char* html2 = "<br/><h2>Click on a thumbnail to view a larger image</h2><br/>";
    fprintf(fp, "%s\n", html2);

    for (int i=0; i<len; i++) {
        // 57 is the number of chars in the basic html code 
        // plus the length of caption, img filename, thumbnail name, and one null terminating char
        char fileline[(57 + strlen(captions[i]) + strlen(mediumImages[i]) + strlen(thumbnails[i]) + 1)]; //= (char*) malloc(sizeof(char) * (57 + strlen(captions[i]) + strlen(mediumImages[i]) + strlen(thumbnails[i]) + 1));
        sprintf(fileline, "<h2>%s</h2><a href=\"%s\"><img src=\"%s\" border=\"1\"></a><br/>", captions[i], mediumImages[i], thumbnails[i]);

        // write to file and free
        fprintf(fp, "%s\n", fileline);
        //free(fileline);
        free(captions[i]);
        free(mediumImages[i]);
        free(thumbnails[i]);
    }

    char* htmlClose = "</body></html>";
    fprintf(fp, "%s\n", htmlClose);

    fclose(fp);
    return 0;
}

/*
 *
 * cleanUp - this function cleans up all of the previously
 * malloc'd variables so we do not exit with any memory leaks
 * 
 * returns 0 if successful
 * exits with a value > 1 otherwise
 * 
 */
int cleanUp(char** directory, char** thumbnails, char** mediumImages, char** captions, char** orientations)
{
    if (directory == NULL || thumbnails == NULL || mediumImages == NULL || 
        captions == NULL || orientations == NULL) {
            fprintf(stderr, "cleanUp -- NULL inputs are not accepted\n");
            exit (7);
        }

    free(directory);
    free(thumbnails);
    free(mediumImages);
    free(captions);
    free(orientations);

    return 0;
}


