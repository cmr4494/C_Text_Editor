#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
struct termios original;  //terminal before and after starting editor program
struct termios raw; //terminal during the editor program
char * globalFilename = NULL;
int cursorx;
int cursory;
int rows; //rows of terminal size
int cols; //cols of terminal size
int currentLine; //line that the user is currently on
typedef struct line{
  int length; //length of one line
  char *text; //actual text in the line
}line;
struct line *editorLines= NULL; //all lines in the editor
int numLines; //the number of lines in the editor
int numCols; //numbers
void resetTerminal(){
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
  //possibly add error handling
}
void terminalHandler(){
  tcgetattr(STDIN_FILENO, &original); //getting original terminal settings 
  raw = original;
  cfmakeraw(&raw); //sets the terminal to read input byte by byte
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); //TCSA flush discards any input reading, including queued input

  atexit(resetTerminal); //once the program terminates the terminal will be reset so the user can go back to what they were doing
}

enum arrows_and_specials{
  BACKSPACE = 127,
  UP = 1000,
  DOWN,  
  RIGHT,
  LEFT
};
void putChar(line *line, int index, int c){
  if (index <0 || index> line->length) index = line->length;
  line->text = realloc(line->text, line->length+2);
  memmove(&line->text[index+1], &line->text[index], line->length -index+1);
  line->length++;
  line->text[index] =c;
}
void writeSingleChar(int c){ //index where char will be inserted, char to be inserted
  if(cursory == numLines){
    //append line
    editorLines = realloc(editorLines, sizeof(editorLines[currentLine]) *(numLines+1));
    editorLines[numLines].length = 0;
    editorLines[numLines].text = malloc(1);
    memcpy(editorLines[numLines].text, &c, 0);
    editorLines[numLines].text[0] = '\0' ;
    numLines++;
	   }
  //insert char
  putChar(&editorLines[cursory], cursorx, c);
  cursorx++;
}
void addLine(int index, char *string, size_t len){
  if (index <0 || index >numLines) return;
  editorLines = realloc(editorLines, sizeof(line)*(numLines+1));
  memmove(&editorLines[index+1], &editorLines[index], sizeof(line) *(numLines -index));
  editorLines[index].length = len;
  editorLines[index].text = malloc(len+1);
  memcpy(editorLines[index].text, string, len);
  editorLines[index].text[len] = '\0';
  numLines++;
}
void newLine(){
  if(cursorx==0){//at start of line
    addLine(cursory, "", 0);
  }
  else{
    line *line = &editorLines[cursory];
    addLine(cursory+1, &line->text[cursorx], line->length - cursorx);
    line = &editorLines[cursory];
    line->length = cursorx;
    line->text[line->length];'\0';
  }
  cursory++;
  cursorx=0;
}
void freeLine(line *line){
    free(line->text);
  }
void deleteLine(int index){
  if (index <0 || index >=numLines)return;
  freeLine(&editorLines[index]);
  memmove(&editorLines[index], &editorLines[index+1], sizeof(line)*(numLines-index-1));
  numLines--;
}
void addToEnd(line *line, char *toAdd, size_t len){
  line->text = realloc(line->text, line->length +len+1);
  memcpy(&line->text[line->length], toAdd, len);
  line->length += len;
  line->text[line->length] ='\0';
}
  
void deleteChar(line *line, int index){
  if (index< 0 || index >= line->length) return ;
  memmove(&line->text[index], &line->text[index+1], line->length -index);
  line->length--;
}
void deleteSingleChar(){
  if(cursory == numLines) return;
  if(cursorx ==0 && cursory ==0) return;
  line *line = &editorLines[cursory];
  if(cursorx>0){
    deleteChar(line, cursorx-1);
   cursorx--;
  } else {
    cursorx = editorLines[cursory-1].length;
    addToEnd(&editorLines[cursory-1], line->text, line->length);
    deleteLine(cursory);
    cursory--;
  }
}
int readUserInput(){
  int newChar; //whether there is a char to be read or not
  char c; //char to be read
  while(1){
    newChar=read(STDIN_FILENO, &c, 1);
    if(newChar!=1){
      break;
    }
    if (c =='\x1b') {
      char seq[3];
      if(read(STDIN_FILENO, &seq[0], 1)!=1){
	  return '\x1b';
	}
      if(read(STDIN_FILENO, &seq[1], 1)!=1){
	    return '\x1b';
	  }
	  if(seq[0] =='['){
	    switch(seq[1]){
	    case 'A': return UP;
	    case 'B': return DOWN;
	    case 'C': return RIGHT;
	    case 'D': return LEFT;
	    }
	  }
	  
	  return '\x1b';
	  }
	else{
  return c;
	}
  }
}
char *toString(int *stringlen){
  int len=0;
  int i;
  for(i=0; i <numLines; i++){
    len += editorLines[i].length+1;
  }
  *stringlen = len;
  char *string = malloc(len);
  char *a = string;
  for( i=0; i<numLines; i++){
    memcpy(a, editorLines[i].text, editorLines[i].length);
    a+= editorLines[i].length;
    *a = '\n'; //add newline
    a++;
  }
  return string;
}
void save(){
  if(globalFilename == NULL) return;

  int len;
  char *string = toString(&len);
  int file = open(globalFilename, O_RDWR | O_CREAT, 0644);
  ftruncate(file, len);
  write(file, string, len);
  close(file);
  free(string);
}
void useInput(){
    int c = readUserInput();
    switch(c){
    case ((('s') & 0x1f)):
      save();
      break;
    case ((('q') & 0x1f)):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    case '\r': //return key
      newLine();
      break;
    case BACKSPACE:
      deleteSingleChar();
      break;
    case UP:
      if(cursory !=0){
      cursory--;
      }
      break;
    case DOWN:
      if(cursory<numLines){
      cursory++;
      }
      break;
    case LEFT:
      if(cursorx!=0){
      cursorx--;
      }
      break;
    case RIGHT:
      cursorx++;
      break;
    default:
      writeSingleChar(c);
    }
}

void makeLines(){
  //clears the screen
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  //writes the welcome message on the screen
  for(int i=0; i<rows; i++){
    int lineNum = i + currentLine;
    if(lineNum>= numLines){
    if( i>= numLines){
    if(rows/3!=i){
    write(STDOUT_FILENO, "~", 3);
    }else{
      write(STDOUT_FILENO, "~", 3);
      if(numLines==0){
      for(int j=0; j<cols/2; j++){
	write(STDOUT_FILENO, " ", 1);
      }
      write(STDOUT_FILENO, "C Text Editor", 13);
      }
      
    }
    }
    }else{
      int len = editorLines[lineNum].length - numCols;
      if(len <0) len=0;
      if (len> cols) len = cols;
      write(STDOUT_FILENO, &editorLines[lineNum].text[numCols], len);
    }
    write(STDOUT_FILENO, "\x1b[K", 3);
    if(i<rows-1){
      write(STDOUT_FILENO, "\r\n", 2);
    }
  } 
}
void updateDisplayedLines(){
  if(cursory<currentLine){
    currentLine = cursory;
  }
  if(cursory>= currentLine+ rows){
    currentLine = cursory - rows+1;
  }
  if(cursorx<numCols){
    numCols = cursorx;
  }
  if(cursorx >= numCols + cols){
    numCols = cursorx - cols+1;
}
}
void refresh(){
  updateDisplayedLines();
  write(STDOUT_FILENO, "\x1b[?25l", 6);
  write(STDOUT_FILENO, "\x1b[H", 3);
  makeLines();

  //displaying cursor
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (cursory-currentLine)+1, cursorx+1);
  write(STDOUT_FILENO, buf, strlen(buf));
  write(STDOUT_FILENO, "\x1b[?25h", 6);
}
void openFile(char *filename){
  free(globalFilename);
    globalFilename = strdup(filename);
  FILE *file = fopen(filename, "r"); //open file in read only
  char *line; 
  ssize_t linelen;
  size_t lineBuffSize=0;
  while((linelen = getline(&line, &lineBuffSize, file)) != -1){ //while there is a line
    while (linelen > 0 && (line[linelen -1] =='\n' || line[linelen-1] == '\r')) linelen--;

    editorLines = realloc( editorLines, sizeof(struct line) *(numLines +1)); //frees memory for a new line

    editorLines[numLines].length = linelen; //defining the length of a line
    editorLines[numLines].text = malloc(linelen+1); //freeing memory for text
    memcpy(editorLines[numLines].text, line, linelen);
    editorLines[numLines].text[linelen]='\0'; //appending end of string char
    numLines++; //incrementing row
  }
  free(line);
  fclose(file);
  
}
int getCursorPosition() {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  
  
  return -1;
}
int main(int numArgs, char *args[]) {
  terminalHandler();
  struct winsize window;
  cursorx = 0; //set origin position of cursor on x
  cursory = 0; //set origin position of cursor on y
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);//gets the rows and columns of the window
  rows = window.ws_row; //sets rows from window
  cols = window.ws_col; //sets cols from window
  numLines = 0;
  currentLine = 0;
  if(numArgs >= 2){
  openFile(args[1]);
  }
  while(1){
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
  rows = window.ws_row;
  cols = window.ws_col;
  refresh();
  useInput();
  }
}


