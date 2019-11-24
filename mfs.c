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

#define BLOCK_NUM 4226          // Number of blocks available in file system

#define BLOCK_SIZE 8192         //Number of bytes for each block

#define NUM_FILE 128            //Number of files that can exist in the files system

#define FILENAME_LEN 32         //maximum length of the file name

#define BLOCK_START_INDEX 132   //index in the file systrem where
                                //the data block of a file starts.

#define BLOCK_FOR_A_FILE 1250   //maximum blocks of a file.

uint8_t blocks[BLOCK_NUM][BLOCK_SIZE];  //a 2d dimensional array that creates blocks.

FILE * file_d = NULL;                   //creating a file pointer

typedef struct Directory_entry          //A structure is created which holds the information for file
{                                       //such as is the directory valid, name of the file
	uint8_t valid;                        //time the file is created and inode index of that file.
	char name[FILENAME_LEN];
	char timestamp[30];
	uint32_t inode;
}Directory_Entry;

typedef struct Inode                    //A structure is created which holds the information for inode
{                                       //for a particular file such as hidden, read only, size of file
	uint8_t attributes_h;                 // maximum size of a file and an array of blocks where file takes up         
	uint8_t attributes_r;                 //space of the system.
	uint32_t size;                        //attribute_h holds the attribute for hidden file and
                                        //attribute_r holds the attribute for read file.
	uint32_t blocks[1250];
}Inode;


Directory_Entry * dir;                  //A pointer of array to the directory structure
struct Inode * inodes_list;             //A pointer of array to the  inodes list
uint8_t * free_block_list;              //a pointer of array to the free blocks
uint8_t * free_inode_list;              //a pointer of array to the free inodes

time_t timestamp;                       //declaration of time stamp


void FreeINodeList_Init()               //function that initializes the free inode list.
{
	int i;
	for(i =0 ; i<128;i++)
	{
		free_inode_list[i] = 1;
	}
}

void FreeBlockList_Init()               //function that initializes the free block list.
{
	int i;
	for(i =BLOCK_START_INDEX ; i<BLOCK_NUM;i++)   //variable i starts from block_start_index(132) because 
                                                //all the blocks above that are for directory entry, inodes,
                                                //free block map and inode map.
	{
		free_block_list[i] = 1;
	}
}

void Dir_Init()                                 //function that initializes the directory_entry
                                                //block for all the files
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

void Inodes_Init()                              //function that initializes the Inode
                                                //block for all the files
{
	int i, j ;
	for(i =0 ; i<128;i++)
	{
		inodes_list[i].attributes_r = 0;           //setting the attributes for read and hidden files to default i.e 0 
		inodes_list[i].attributes_h = 0;           //if the file is hidden or read, attribute is set to 1
		inodes_list[i].size = 0;                    
		
		for(j=0 ; j<1250;j++)
		{
			inodes_list[i].blocks[j] = -1;
		}
	}
}

void initialized()                            //initializing the functions below for the file system.
{
	Dir_Init();
	FreeBlockList_Init();
	FreeINodeList_Init();
}

int disk_size()                               //finding the size occupied by files in the file system.
{
	int size = 0;
	int i = 0;
	for(i=0;i<128;i++)
	{

		if(dir[i].valid != 0)                     //if the directory is occupied then the size of file is taken from inode
                                              //structure and added to the variable size.
		{
			size += inodes_list[dir[i].inode].size ; 
		}
	}

	return size;
}


int find_free_dir()                           //this function finds and returns free directory index if valid == 0.
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

int find_free_inode()                        //this function finds and returns free inode index.             
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

int find_free_block()                            //this function finds and returns free block index.       
{
	int i;
	int val =-1;
	for(i = BLOCK_START_INDEX ; i<BLOCK_NUM;i++)  //variable i starts from block_start_index(132) because 
                                                //all the blocks above that are for directory entry, inodes,
                                                //free block map and inode map.
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

/*create_fs function takes filename as a parameter. If the filename is null then file
system image won't be created. If the filename is not null, all the memory blocks are
set to zero. initialized funtion is called to set directory, free blocks and free inodes
for a file. */

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


/*file_searcher function searches for a file in the directory and if the valid is 1
then it returns the directory index for that file. */
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

//open function takes the name of the image of the file in the argument
//if the name of the file system is null, it returns file is not found.
//if the name of the file system exists, it reads every data of file_d and
//copies into 2d array blocks.
void open(char* fsname) 
{
	file_d = fopen(fsname,"r+");
	if(file_d==NULL) 
	{
		printf("mfs> open: File not found\n");
		return;
	}
	else
	{

	fread(blocks, BLOCK_SIZE,BLOCK_NUM,file_d);
}
}

/*
  fs_close is a void function that  doesn't have any parameters.
  This function closes any opened file system properly.
  It basically copies the all the blocks into the file and closes it
*/
void fs_close( ) 
{
  if(file_d == NULL)
  { 
    // checks if a file system is opeened or not
    printf("mfs> close error: No open fs to close.\n");
    return;
  }
  // rewinds the file pointer to the top of the file.
  rewind(file_d);
  // writes all the data from the block array into the opened file
  fwrite(blocks, BLOCK_SIZE,BLOCK_NUM,file_d);
  fclose(file_d);

  // sets the file_d to null to prevent overwriting
  file_d = NULL;
}


/*
  put is a void function that accepts one char pointer as parameter.
  This function copies the file if present from the working directory into
  the file system. it validates the file and checks the condition for copy
  and later adds it to the opened file system.
*/
void put(char* filename)
{
  
  if(strlen(filename)>32)
  {
    // checks the length of the filename
    printf("mfs> put error: File name too long.\n");
    return;
  }
  int status;
  struct stat buffer;
  status = stat(filename, &buffer);
  if(status == -1 )
  {
    // checks if file exist or not
    printf("mfs> put error: File not found.\n");
  }
  /* condition for checking disk min req */
  else if (buffer.st_size> (BLOCK_SIZE* (BLOCK_NUM-8) - disk_size()))
  {
    // checks t h availability of the file system
    printf("mfs> put error: Not enough disk space.\n");
  }
  else if (buffer.st_size> BLOCK_FOR_A_FILE*BLOCK_SIZE)
  {
    ///checks the length of the upcoming file
    printf("mfs> put error: File too big.\n");
  }
  
  else
  { 
     // Open the input file read-only 
    FILE *ifp = fopen ( filename, "r" ); 
    // Save off the size of the input file since we'll use it in a couple of places and 
    // also initialize our index variables to zero. 

    // finds the index of a free dir and free onode
    int filenum = find_free_dir();
    dir[filenum].inode = find_free_inode();
    int copy_size   = buffer.st_size;

    // copies the file name in the dir list
    strcpy(dir[filenum].name,filename);
    timestamp = time(NULL);

    // stores the timestamp for the file put in the filesystem.
    strcpy(dir[filenum].timestamp,asctime( localtime(&timestamp) ));
    dir[filenum].timestamp[strlen(dir[filenum].timestamp)-1] = '\0';

    //copys the file size into the inode
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
      // finds a free block from tbhe free block list

      // stores the index of the data bloc k in the inode
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

/*
  list is a void function that accepts one char pointer as parameter.
  This function lists all the files that are present in the file system
  along with showing their timestamp and size.
  It doesn't show hidden files in default but shows hidden file if -h is 
  passed as a parameter. 
*/
void list(char* data)
{
  int i ;
  int count =0; // counter for number of files in the system.
  // runs through the loop ih the dir list for to check for the files 
  for(i=0;i<128;i++)
  {
    // checks for the validity of the file
    if(dir[i].valid != 0)
    {
      //checks for the hidden file 
      if(inodes_list[dir[i].inode].attributes_h == 1)
      {
        if(data!= NULL)
        {
          // DISPLAYS THE HIDDEN FILE IF USER HAS PROMPTED TO DO SO.
          if(data[1]=='h'||data[1]=='H')
          {
            printf("%8d%27s%15s\n", inodes_list[dir[i].inode].size, 
              dir[i].timestamp,dir[i].name);
            count++;
          }
        }
      }
      // shes the regular files that are not hidden
      else
      {
        printf("%8d%27s%15s\n", inodes_list[dir[i].inode].size , 
          dir[i].timestamp ,dir[i].name);
        count++;
      }
    }
  }
  // if no files are found in the system shoes no file found.
  if(count ==0)
  {
    printf("mfs> list: No files found\n");
  }

}

/*
  del is a void function that accepts one char pointer as parameter.
  This function deletes the file if present in the file system. 
  It can't delete a read only file and throws error if you try to do it.
*/
void del(char* filename)
{

  int filenum = file_searcher(filename);
  // checks if a file is found or not
  if(filenum ==-1){
    printf("mfs> del error: File not found\n");
    return;
  }
  //checks if a file is read only or not
  if(inodes_list[dir[filenum].inode].attributes_r == 1 )
  {
    printf("mfs> del error: That file is marked read-only.\n");
    return;
  }
  int i;

  // clears the attributes from the fle
  inodes_list[dir[filenum].inode].attributes_r = 0;
  inodes_list[dir[filenum].inode].attributes_h = 0;
  inodes_list[dir[filenum].inode].size = 0;
  // clears the size of the inode of the file
  // frees all the pointed data blocks from the inode
  for(i=0;i<1250;i++)
  {
    if(inodes_list[dir[filenum].inode].blocks[i] != -1)
    {
      // frees the data block for the free block list which can be later used by other files.
      free_block_list[inodes_list[dir[filenum].inode].blocks[i]] = 1;
      inodes_list[dir[filenum].inode].blocks[i] = -1;
    }

  }
  // frees the inode adn adds it to the free inode list.
  free_inode_list[dir[filenum].inode] = 1;
  // makes the dir available for future reuse.
    dir[filenum].valid = 0;
    // sets name, timestamp to 0 and removes the inode for m the dir
    memset(dir[filenum].name,0,32);
    memset(dir[filenum].timestamp,0,30);
    dir[filenum].inode = -1;
}

/*
  df is a void function that doesn't have any parameters.
  This function just prints the total free space avaialble in the filesystem.
*/
void df()
{
  printf("%d bytes free.\n", BLOCK_SIZE* (BLOCK_NUM - 8) - disk_size() );
}

/*
  get is a void function that accepts two char pointer as parameter.
  This function copies the file if present into the working directory 
  It also renames the file to a new file name if provided by the user.
*/
void get(char* filename, char* newfilename)
{
  //checks if a newfile name is provided or not
  if(newfilename!= NULL)

  {
    if(strlen(newfilename)>32)
    {
      // checks for the length of the file
      printf("mfs> get error: New file name too long.\n");
      return;
    }
  }

  int block_count = 0;
  // searches the index of the file that is requested form the file sys.
  int filenum = file_searcher(filename);
  if(filenum ==-1){
    printf("mfs> get error: File not found\n");
    return;
  }

  FILE *ofp;
  // if newfilename is given it opens it otherwise it uses the deafult filename 
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

  // assign the copy size of the file
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
    // reads the specific block from the inode to copy into the file
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

 /* attrib is a void function has two parameters wwhere both of them are char pointer.
  This function basically sets the asssigned atributes to a requested file if
  it is present in the file system.
 */
void attrib(char* attributes, char* filename)
{
  int filenum = file_searcher(filename);
  if(filenum ==-1){
    //checks if filename is in the file sys or not
    printf("mfs> attrib: File not found\n");
    return;
  }
  if(attributes[0] =='+')
  {
    // sets the respective attributes according to the user input.
    if(attributes[1]=='h'|| attributes[1]=='H')
    {
      // if +h is typed sets hidden attribute
      inodes_list[dir[filenum].inode].attributes_h = 1; 
    }
    else if(attributes[1]=='r'|| attributes[1]=='R')
    {
      // if +r is typed sets read only attribute
      inodes_list[dir[filenum].inode].attributes_r = 1; 
    }
    else
    {
      printf("mfs> attrib: Unrecognized attribute.\n"); 
    }     
  }
  else if(attributes[0]=='-')
  {
    // sets the respective attributes according to the user input.
    if(attributes[1]=='h'|| attributes[1]=='H')
    {
      // if -h is typed removes hidden attribute
      inodes_list[dir[filenum].inode].attributes_h = 0; 
    }
    else if(attributes[1]=='r'|| attributes[1]=='R')
    {
      // if -r is typed removes read only attribute
      inodes_list[dir[filenum].inode].attributes_r = 0; 
    }
    else
    {
      printf("mfs> attrib: Unrecognized attribute.\n"); 
    }
  }
  else
  {
    printf("mfs> attrib: Unrecognized operation.\n");
    return;
  }

}

int main()
{
  // declares the directory list to the first block  
  dir = (Directory_Entry*) &blocks[0];
  
  // declares the inode list to the eighth block 
  inodes_list = (Inode *) &blocks[7];
  
  // declares the list of free blocks to the sixth block 
  free_block_list = (uint8_t*) &blocks[5];

  // declares the list of free inodes to the seventh block
  free_inode_list = (uint8_t*) &blocks[6];

  
  // initializes all the inode lists, dir lists and free list for inodes and blocks.
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
    // After tokenization of the command input the token is compared to check for their 
    // respective functionality in the program.

    if(strcmp(token[0],"quit")==0 || strcmp(token[0],"exit")==0 )
    {
      // quits the program
      free( working_root );
      //checks if file is closed or not.
      // if fs is not closed it closes it first before exiting...
      if(file_d!= NULL) fs_close();
      exit(0);
    }
    else if(strcmp(token[0],"put")==0)
    {
      // puts a file form the current working directory into the file sys.
      put(token[1]);
    }
    else if(strcmp(token[0],"get")==0)
    {
      // gets the file from the file sys and adds it to the working directory.
      get(token[1],token[2]);
    }
    else if(strcmp(token[0],"del")==0)
    {
      // deletes a file from the file system
      del(token[1]);
    }
    else if(strcmp(token[0],"list")==0)
    {
      // list all the files in the file system.
      list(token[1]);
    }
    else if(strcmp(token[0],"df")==0)
    { 
      // dislplays the available free space in the file system.
      df();
    }
    else if(strcmp(token[0],"open")==0)
    {
      // opens a requested file system if possible
      open(token[1]);
    }
    else if(strcmp(token[0],"close")==0)  
    {
      // closes a open file system
      fs_close();
    }
    else if(strcmp(token[0],"createfs")==0) 
    {
      // creates a empty file system with the given name
      create_fs(token[1]);
    }
    else if(strcmp(token[0],"attrib")==0)
    {
      // sets the attribute of a file present in the file system
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
// And this ends the semester FALL 2019.. Hurray!!!!!!!!!




