// The MIT License (MIT)
// 
// Copyright (c) 2016, 2017 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

#define BLOCK_NUM 4226

#define BLOCK_SIZE 8192

#define NUM_FILE 128

#define FILENAME_LEN 32

#define BLOCK_START_INDEX 132

#define BLOCK_FOR_A_FILE 1250

uint8_t blocks[BLOCK_NUM][BLOCK_SIZE];

FILE * file_d;

typedef struct Directory_entry
{
	uint8_t valid;
	char name[FILENAME_LEN];
	char timestamp[30];
	uint32_t inode;
}Directory_Entry;

typedef struct Inode
{
	uint8_t attributes;
	uint32_t size;
	uint32_t blocks[1250];
}Inode;


Directory_Entry * dir;
struct Inode * inodes_list;
uint8_t * free_block_list;
uint8_t * free_inode_list;
uint8_t file_count;
time_t timestamp;
	

void FreeINodeList_Init()
{
	int i;
	for(i =0 ; i<128;i++)
	{
		free_inode_list[i] = 1;
	}
}

void FreeBlockList_Init()
{
	int i;
	for(i =0 ; i<BLOCK_NUM;i++)
	{
		free_block_list[i] = 1;
	}
}

void Dir_Init()
{
	int i;
	for(i =0 ; i<128;i++)
	{
		dir[i].valid = 0;
		memset(dir[i].name,0,32);
		memset(dir[i].timestamp,0,30);
		dir[i].inode = -1;
	}
}

void Inodes_Init()
{
	int i, j ;
	for(i =0 ; i<128;i++)
	{
		inodes_list[i].attributes = 0;
		inodes_list[i].size = 0;
		
		for(j=0 ; j<1250;j++)
		{
			inodes_list[i].blocks[j] = -1;
		}
	}
}

void initialized()

{
	Dir_Init();
	FreeBlockList_Init();
	FreeINodeList_Init();
	file_count = 0;
}

int disk_size()
{
	int size = 0;
	int i = 0;
	for(i=0;i<128;i++)
	{

		if(dir[i].valid != 0)
		{
			size += inodes_list[dir[i].inode].size ; 
		}
	}

return size;
}


int find_free_dir()
{
	int i;
	int val =-1;
	for(i =0 ; i<128;i++)
	{
		if(dir[i].valid == 0)
		{			
			val = i;
			dir[i].valid = 1;
			break;
		}		
	}
	return val;
}

int find_free_inode()
{
	int i;
	int val =-1;
	for(i =0 ; i<128;i++)
	{
		if(free_inode_list[i] == 1)
		{
			val = i;
			free_inode_list[i] = 0;
			break;
		}		
	}

	return val;
}

int find_free_block()
{
	int i;
	int val =-1;
	for(i = 132 ; i<BLOCK_NUM;i++)
	{
		if(free_block_list[i] == 1)
		{
			val = i;
			free_block_list[i] = 0;
			break;
		}		
	}
	return val;
}

void create_fs(char* fsname) 
{
	if(fsname==NULL) 
	{
		printf("mfs> createfs: File not found\n");
		return;
	}
	memset(blocks, 0 , BLOCK_NUM* BLOCK_SIZE);
	file_d = fopen(fsname,"w");
	initialized();
	fwrite(blocks, BLOCK_SIZE,BLOCK_NUM,file_d);
	fclose(file_d);
	file_d = NULL;

}

void open(char* fsname) 
{
	file_d = fopen(fsname,"r+");
	if(file_d==NULL) 
	{
		printf("mfs> open: File not found\n");
		return;
	}
	fread(blocks, BLOCK_SIZE,BLOCK_NUM,file_d);
}

void fs_close( ) 
{
	if(file_d == NULL)
	{
		printf("mfs> close error: No open fs to close.");
	}
	rewind(file_d);
	fwrite(blocks, BLOCK_SIZE,BLOCK_NUM,file_d);
	fclose(file_d);
	file_d = NULL;
}

void put(char* filename)
{
	
	if(strlen(filename)>32)
	{
		printf("mfs> put error: File name too long.\n");
		return;
	}
	int status;
	struct stat buffer;
	status = stat(filename, &buffer);
	if(status == -1 )
	{
		printf("mfs> put error: File not found.\n");
	}
	/* condition for checking disk min req */
	else if (buffer.st_size> (BLOCK_SIZE* (BLOCK_NUM-8) - disk_size()))
	{
		printf("mfs> put error: Not enough disk space.\n");
	}
	else if (buffer.st_size> BLOCK_FOR_A_FILE*BLOCK_SIZE)
	{
		printf("mfs> put error: File too big.\n");
	}
	
	else
	{ 
		 // Open the input file read-only 
    FILE *ifp = fopen ( filename, "r" ); 
    // Save off the size of the input file since we'll use it in a couple of places and 
    // also initialize our index variables to zero. 
    int filenum = find_free_dir();
    dir[filenum].inode = find_free_inode();
    int copy_size   = buffer.st_size;
    strcpy(dir[filenum].name,filename);
    timestamp = time(NULL);
	strcpy(dir[filenum].timestamp,asctime( localtime(&timestamp) ));
    dir[filenum].timestamp[strlen(dir[filenum].timestamp)-1] = '\0';


   	inodes_list[dir[filenum].inode].size = copy_size;
	
  	//inodes_list[dir[filenum].inode].attributes = 3;
	
    int block_count = 0;
    // We want to copy and write in chunks of BLOCK_SIZE. So to do this 
    // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
    // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
    int offset      = 0;               

    // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
    // memory pool. Why? We are simulating the way the file system stores file data in
    // blocks of space on the disk. block_index will keep us pointing to the area of
    // the area that we will read from or write to.
    
    // copy_size is initialized to the size of the input file so each loop iteration we
    // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
    // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
    // we have copied all the data from the input file.
    while( copy_size > 0 )
    {
    	int block_index = find_free_block();
    
    	inodes_list[filenum].blocks[block_count]= block_index;
      // Index into the input file by offset number of bytes.  Initially offset is set to
      // zero so we copy BLOCK_SIZE number of bytes from the front of the file.  We 
      // then increase the offset by BLOCK_SIZE and continue the process.  This will
      // make us copy from offsets 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
      fseek( ifp, offset, SEEK_SET );
 
      // Read BLOCK_SIZE number of bytes from the input file and store them in our
      // data array. 
      int bytes  = fread( blocks[block_index], BLOCK_SIZE, 1, ifp );

      // If bytes == 0 and we haven't reached the end of the file then something is 
      // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
      // It means we've reached the end of our input file.
      if( bytes == 0 && !feof( ifp ) )
      {
        printf("mfs> An error occured reading from the input file.\n");
        return;
      }

      // Clear the EOF file flag.
      clearerr( ifp );

      // Reduce copy_size by the BLOCK_SIZE bytes.
      copy_size -= BLOCK_SIZE;
      
      // Increase the offset into our input file by BLOCK_SIZE.  This will allow
      // the fseek at the top of the loop to position us to the correct spot.
      offset    += BLOCK_SIZE;
    
      // Increment the index into the block array 
      block_count ++;
    }

    // We are done copying from the input file so close it out.
    fclose( ifp );
 
	}

return;
}


void list(char* data)
{

	int i ;
	int count =0;

	for(i=0;i<128;i++)
	{
		if(dir[i].valid != 0)
	{
		if((inodes_list[dir[i].inode].attributes == (int)'h') ||
		 (inodes_list[dir[i].inode].attributes == ((int)'h')+((int)'r')) )
		{
			if(data[1]=='h'||data[1]=='H')
			{
				printf("Hidden Filename is %s\n",dir[i].name );
				count++;
			}
		}
		else
		{
		printf("%8d%27s%15s\n", inodes_list[dir[i].inode].size , dir[i].timestamp ,dir[i].name);
		count++;
		}
	}
}
if(count ==0)
{
	printf("mfs> list: No files found\n");
}

}

void del(char* filename)
{


}

void df()
{
	printf("%d bytes free.\n", BLOCK_SIZE* (BLOCK_NUM - 8) - disk_size() );
}


int file_searcher(char* filename)
{
	int i;
	for(i=0;i<128; i++)
	{
		if(dir[i].valid!=0)
		{
			if(strcmp(filename,dir[i].name)==0)
			{
				return i;
			}
		}
	}
return -1;
}

void get(char* filename, char* newfilename)
{

	if(strlen(newfilename)>32)
	{
		printf("mfs> get error: New file name too long.\n");
		return;
	}
	
    int block_count = 0;
    int filenum = file_searcher(filename);
    if(filenum ==-1){
    	printf("mfs> get error: File not found\n");
    	return;
    	
    }
	FILE *ofp;
	if(newfilename == NULL)
	{
		ofp = fopen(filename, "w");
	}
	else
	{
		ofp = fopen(newfilename, "w");
	}
    if( ofp == NULL )
    {
      printf("mfs> Could not open output file: %s\n", filename );
      return ;
    }

    // Initialize our offsets and pointers just we did above when reading from the file.

    int copy_size   = inodes_list[dir[filenum].inode].size;
    int offset      = 0;

    // Using copy_size as a count to determine when we've copied enough bytes to the output file.
    // Each time through the loop, except the last time, we will copy BLOCK_SIZE number of bytes from
    // our stored data to the file fp, then we will increment the offset into the file we are writing to.
    // On the last iteration of the loop, instead of copying BLOCK_SIZE number of bytes we just copy
    // how ever much is remaining ( copy_size % BLOCK_SIZE ).  If we just copied BLOCK_SIZE on the
    // last iteration we'd end up with gibberish at the end of our file. 
    while( copy_size > 0 )
    { 
      int block_index = inodes_list[filenum].blocks[block_count];
      int num_bytes;

      // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
      // end up with garbage at the end of the file.
      if( copy_size < BLOCK_SIZE )
      {
        num_bytes = copy_size;
      }
      else 
      {
        num_bytes = BLOCK_SIZE;
      }

      // Write num_bytes number of bytes from our data array into our output file.
      fwrite( blocks[block_index], num_bytes, 1, ofp ); 

      // Reduce the amount of bytes remaining to copy, increase the offset into the file
      // and increment the block_index to move us to the next data block.
      copy_size -= BLOCK_SIZE;
      offset    += BLOCK_SIZE;
      block_count ++;

      // Since we've copied from the point pointed to by our current file pointer, increment
      // offset number of bytes so we will be ready to copy to the next area of our output file.
      fseek( ofp, offset, SEEK_SET );
    }

    // Close the output file, we're done. 
    fclose( ofp );

}


void attrib(char* attributes, char* filename)
{
	  int filenum = file_searcher(filename);
    if(filenum ==-1){
    	printf("mfs> attrib: File not found\n");
    	return;
  	}
  	if(attributes[0] =='+')
  	{


  		  	inodes_list[dir[filenum].inode].attributes += (int) attributes[1]; 
  	}
  	else if(attributes[0]=='-')
  	{
  		inodes_list[dir[filenum].inode].attributes -= (int) attributes[1]; 
  	}
  	else
  	{
  		printf("mfs> attrib: Unrecognized operation.\n");
  		return;
  	}


}

int main()
{
	
	dir = (Directory_Entry*) &blocks[0];
	
	inodes_list = (Inode *) &blocks[9];
	free_block_list = (uint8_t*) &blocks[7];
	free_inode_list = (uint8_t*) &blocks[8];
 
	initialized();

	int i;

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your shell functionality

    if(strcmp(token[0],"quit")==0 || strcmp(token[0],"exit")==0 )
    {
   	    free( working_root );
   	    if(file_d!= NULL) fs_close();
    	exit(0);
    }
    else if(strcmp(token[0],"put")==0)
    {
    	put(token[1]);
    }
    else if(strcmp(token[0],"get")==0)
    {
    	get(token[1],token[2]);
    }
    else if(strcmp(token[0],"del")==0)
    {
    	del(token[1]);
    }
    else if(strcmp(token[0],"list")==0)
    {
    	list(token[1]);
    }
    else if(strcmp(token[0],"df")==0)
    {
    	df();
    }
    else if(strcmp(token[0],"open")==0)
    {
    	open(token[1]);
    }
    else if(strcmp(token[0],"close")==0)	
    {
    	fs_close();
    }
    else if(strcmp(token[0],"createfs")==0)	
    {
    	create_fs(token[1]);
    }
    else if(strcmp(token[0],"attrib")==0)
    {
    	attrib(token[1], token[2]);
    }
    else
    {
   		printf("mfs> Command not found. Try Again!!!\n");
    }	
    free( working_root );
  }
  return 0;
}
