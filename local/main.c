#ifndef ARDUINO

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include "keyboard.h"
#include "disk.h"

char term_in() {
  if (!kbhit()) return 0;
  unsigned char ch = fgetc(stdin);    //printf("char: %02xh\n",ch);
  if (ch==0x0a) ch=0x0d;  //replace line-feed by carriage-return
  return ch;
}

#include "i8080.h"
#include "i8080_hal.h"

int cloadStarted = 0;
FILE *cloadFile;

int hal_io_input(int port) {
  static uint8_t character = 0;

  switch (port) {
    case 0x00:
      return 0;
    case 0x01: //serial read
      return term_in();
	case 0x06: // character ready flag for program loading from cassette
		return 0x00; // 0X00 appears to be the read flag meaning port is ready for reading or writing
	case 0x07:
		// assume the file is "A.bin"
		if (cloadStarted == 0)
		{
			cloadStarted = 1;
			// open file
			if ((cloadFile = fopen("A.bin", "rb")) == NULL)  // r for read, b for binary
				printf("File couldn't be opened.\n");
			else
				printf("File opened.\n");
			fflush(stdout);
		}

		fread(&character, 1, 1, cloadFile);
		// TODO at EOF or after reading the three '\0' that end a program, close file
		return character;
    case 0x08: 
      return disk_status();
    case 0x09:
      return disk_sector();
    case 0x0a: 
      return disk_read();
    case 0x10: //2SIO port 1 status
      if (!character) {
        // printf("2SIO port 1 status\n");
        character = term_in();
      }
      return (character ? 0b11 : 0b10); 
    case 0x11: //2SIO port 1, read
      if (character) {
        int tmp = character; 
        character = 0; 
        return tmp; 
      } else {
        return term_in();
      }
    case 0xff: //sense switches
      return 0;
    default:
      printf("%04x: in 0x%02x\n",i8080_pc(),port);
      exit(1);
      return 1;
  }
  return 1;
}

// port 8 buffer
char port8buffer[1024];
int port8bufferIndex = 0;
int port8zeroaccumulator = 0;

#define SHOULD_PRINT_BIN 0

void hal_io_output(int port, int value) {
	int i;
  switch (port) {
    case 0x01: 
      printf("%c",value & 0x7f);
      fflush(stdout);
      break;
	case 0x07:
		if (port8bufferIndex > sizeof port8buffer)
		{
			printf("buffer overrun on port 0x07 writing.");
			exit(1);
		}
		port8buffer[port8bufferIndex++] = (char)value;
		if (value == '\0')
			port8zeroaccumulator++;
		else
			port8zeroaccumulator = 0;

		if (port8zeroaccumulator == 3 && port8bufferIndex >= 7) // end of CSAVE reached when 3 '\0' in a row, flush to a file
		{
			// "program name" will be 4th character in stream
			char filename[7];
			if (SHOULD_PRINT_BIN)
				sprintf(filename, "%c.bin", (char)port8buffer[3]);
			else
				sprintf(filename, "%c.txt", (char)port8buffer[3]);
			FILE *f = fopen(filename, "w");
			if (f == NULL)
			{
				printf("Error opening file!\n");
				exit(1);
			}

			for (i = 0; i < port8bufferIndex; i++)
			{
				if (SHOULD_PRINT_BIN)
					fprintf(f, "%c", port8buffer[i] & 0x7f);
				else
				{
					fprintf(f, "%#04x, ", port8buffer[i] & 0x7f);
					if ((i+1) % 10 == 0)
						fprintf(f, "\n");
				}
			}
			fprintf(f, "\n");
			fclose(f);
		}
    case 0x08:
      disk_select(value);
      break;
    case 0x09:
      disk_function(value);
      break;
    case 0x0a:
      disk_write(value);
      break;
    case 0x10: // 2SIO port 1 control
      //nothing
      break;
    case 0x11: // 2SIO port 1 write  
      printf("%c",value & 0x7f);
      fflush(stdout);
      break;
    case 0x12: // ????
      break;
    default:
      printf("%04x: out 0x%02x\n",i8080_pc(),port);
      exit(1);
      break;
  }
}  



void load4KRom() {
  const uint8_t rom[] = {
    #include "data/4kbas32.h"
  };
  unsigned char* mem = i8080_hal_memory();
  memset(mem, 0, 0x4000);
  memcpy(mem, rom, sizeof rom);
  i8080_jump(0x0000);
}

void load8KRom() {
	const uint8_t rom[] = {
#include "data/8krom.h"
	};
	unsigned char* mem = i8080_hal_memory();
	memset(mem, 0, 0x4000);
	memcpy(mem, rom, sizeof rom);
	i8080_jump(0x0000);
}

#define loadRom() load8KRom()

void load_mem_file(const char* filename, size_t offset) {
  size_t size;
  FILE* fp = fopen(filename, "rb");
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  unsigned char* mem = i8080_hal_memory();
  fread(mem+offset, 1, size, fp);
  fclose(fp);
}

int main() {
  printf("[altair8800]\n");

  i8080_init();

  // load_mem_file("data/4kbas32.bin", 0);
  // i8080_jump(0);

  // load_mem_file("data/88dskrom.bin", 0xff00);
  // disk_drive.disk1.fp = fopen("data/cpm63k.dsk", "r+b");
  // disk_drive.disk2.fp = fopen("data/zork.dsk", "r+b");
  // disk_drive.nodisk.status = 0xff;
  // i8080_jump(0xff00);

  loadRom();

  nonblock(NB_ENABLE); //keyboard

  while (1) {
    i8080_instruction();
    //printf("%04x\n",i8080_pc());
    //usleep(1000);
  }
}
#endif
