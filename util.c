#ifdef DEBUG
#define TRACE(...) fprintf(stderr,__VA_ARGS__)
#else
#define TRACE(...)
#endif

#include "type.h"
#include "sys/stat.h"

char names[64][64];
extern char *name[64];
extern MINODE minode[NMINODES], *root;
extern PROC *running;

//Read a block of data from the file system
int get_block (int fd, int blk, char buf[BLKSIZE])
{
	//TRACE ("getting block %d\n", blk);
	lseek (fd, (long) (blk * BLKSIZE), 0);
	read (fd, buf, BLKSIZE);
}

//Write a block of data back to the file system
int put_block (int fd, int blk, char buf[BLKSIZE])
{
	//TRACE ("putting block %d\n", blk);
	lseek (fd, (long) (blk * BLKSIZE), 0);
	write (fd, buf, BLKSIZE);
}

//Take the path from the user and split it into each directory and the file
//Does NOT check if the user is starting from root
//Also does not mess with the original string passed in
//Returns the number of times the pathname was tokenize, which is the number of directories
int tokenize (char *pathname)
{
	char copy[strlen(pathname) + 1];
	int i = 0;

	strcpy (copy, pathname);

	char *tok = strtok(copy, "/");
	strcpy (names[i], tok);

	while (tok = strtok (0, "/"))
		strcpy(names[++i], tok);

	names[++i][0] = 0;

	return i;
}

//Look Through the data blocks of a directory inode to search for 
//A given filename. Takes a file descriptor to the open file system,
//the index of the directory block to look through, and the 
//index of the directory name in the paths array
int search_dir_block (int fd, int dir_block, int dir_name)
{
	TRACE ("IN search_dir_block: %s\n", names[dir_name]);
	char buf[BLKSIZE];
	DIR *dp = (DIR *)buf;
	char *cp = buf;
	char *np;

	//Grab the directory block from the file system
	get_block (fd, dir_block, buf);

	//Search through the directory block (buffer)
	while (cp < buf + BLKSIZE)
	{
		//First, take the directory name recorded in the directory entry
		// and create a null-terminated copy of it
		np = (char *) malloc (sizeof(char) * (dp->name_len + 1));
		strncpy(np, dp->name, dp->name_len);
		np[dp->name_len] = 0;

		//Check to see if the entry name matches the directory name we're looking for
		if (strcmp (np, names[dir_name]) == 0)
		{
			TRACE ("Found entry: %d %s\n", dp->inode, np);
			//If it does, free the created directory name and return the inode number
			free (np);
			return dp->inode;
		}

		//Otherwise, continue looking through the directory block
		cp += dp->rec_len;
		dp = (DIR *)cp;
		free (np);
	}	

	//Return an error if the code gets here because it never found the name it was looking for.
	return -1;
}

//Grabs an Inumber from a pathname
int getino (int *dev, char* pathname)
{
	TRACE ("In getino\t");
	TRACE ("%d\t", *dev);
	TRACE ("%s\n", pathname);

	int i, j, cur_block, next_block;
	char buf[BLKSIZE];

	int num_names = tokenize (pathname); //Tokenize the pathname
	
	//TRACE ("getino GD ");
	get_block(*dev, 2, buf);
	gp = (GD *)buf;
	int inodeBeginBlock = gp->bg_inode_table; //Grab the inode table location

	INODE *cip;
	
	if (pathname[0] == '/')
	{
		*dev = root->dev;
		cip = &(root->dINODE);
	}
	else
	{
		*dev = running->cwd->dev;
		cip = &(running->cwd->dINODE);
	}

	//Search for each directory name in order
	for (i = 0; i < num_names; i++)
	{
		//Look through each directory entry in the directory
		for (j = 0; j < 12; j++)
		{
			//Grab the block number of the directory entry
			cur_block = cip->i_block[j];

			//If it is zero, there are no more entries to look through so stop iterating
			if (cur_block == 0)
			{
				j = 12;
				continue;
			}

			//Search through the directory block for the current path name
			next_block = search_dir_block (*dev, cur_block, i);

			//If the next directory was found, grab its inode and start searching through it
			if (next_block > -1)
			{
				cur_block = next_block;
				
				//TRACE ("getino searching ");
				get_block (*dev, ((cur_block - 1) / 8) + inodeBeginBlock, buf);
				cip = (INODE *) buf + ((cur_block - 1) % 8);
				j = 12;
			}
		}
		
		//If a file was found and it should be a path, return an error and let the user know
		if (i < num_names - 1 && (cip->i_mode & 0x4000) == 0) //Check if directory or not
		{
			printf ("File found where directory expected.\n");
			return -1;
		}

		//If the directory was not found, return an error
		if (next_block < 0)
		{
			printf ("Invalid path\n");
			return -1;
		}
	}

	//The specified file was found: return its inode number.
	return cur_block;
}

//Gets an inode into memory and returns a pointer to it
MINODE *iget (int dev, int ino)
{
	TRACE ("IN iget: %d\n", ino);

	char buf[BLKSIZE];

	//TRACE ("iget GD ");
	get_block(dev, 2, buf);
	gp = (GD *)buf;

	int inodeBeginBlock = gp->bg_inode_table;
	
	int i;

	//Iterate all of the minodes. NMINODES is a declared constant
	for (i = 0; i < NMINODES; i++)
	{
		if (minode[i].ino == ino) //Check if the inode has already been loaded into memory
		{
			TRACE ("Found inode in spot %d\n", i);
			minode[i].refCount++;
			return &(minode[i]);
		}			
	}
	
	for (i = 0; i < NMINODES; i++) //Find a FREE MINODE (0 references + 0 references) 
	{
		if (minode[i].refCount == 0 && minode[i].ino == 0)
		{
			TRACE ("Empty inode in spot %d\n", i);
			break;
		}
	}

	get_block (dev, ((ino - 1) / 8) + inodeBeginBlock, buf); //Mailman's algorithm
	INODE *temp = (INODE *)buf + ((ino - 1) % 8); //Point inode at the right inode in the block
	
	minode[i].ino = ino;
	minode[i].dev = dev;
	minode[i].dINODE = *temp;
	minode[i].refCount = 1;
	minode[i].dirty = 0;
	minode[i].mounted = 0;
	minode[i].mountptr = 0;
	
	return &(minode[i]); //return that minode
}

MINODE *iput (MINODE *mip) //Puts an MINODE back onto the disk
{
	TRACE ("IN iput: %d\n", mip->ino);

	mip->refCount--;

	if (mip->refCount > 0) //If the file is still in use, just decrement number of references to it
	{
		TRACE ("refCount = %d\n", mip->refCount);
		return 0;
	}

	if (mip->dirty == 0) //If it's not dirty, just label it as empty.
	{
		TRACE ("Minode not dirty\n");
		mip->ino = 0;
		return 0;
	}

	TRACE ("Writing inode %d back to disk\n", mip->ino);

	char gb[BLKSIZE]; //Group descriptor buffer

	TRACE ("dev: %d\n", mip->dev);
	get_block(mip->dev, 2, gb); //Grab the group descriptor
	gp = (GD *)gb;

	int inodeBeginBlock = gp->bg_inode_table; //Find where the inodes begin

	char buf[BLKSIZE];

	//TRACE ("iput ");
	get_block (mip->dev, ((mip->ino - 1) / 8) + inodeBeginBlock, buf); //Mailman's Algorithm
	INODE *temp = (INODE *) buf + ((mip->ino - 1) % 8); //Point it at correct idone
	
	*temp = mip->dINODE;

	//TRACE ("iput ");
	put_block (mip->dev, ((mip->ino - 1) / 8) + inodeBeginBlock, buf); //Put the block back

	mip->ino = 0;

	return mip;
}

int findmyname (MINODE *parent, int myino, char *myname) //Populate myname with the name of the inode from myino
{
	TRACE ("IN findmyname\n");
	char buf[BLKSIZE];
	DIR *dp = (DIR *)buf;
	char *cp = buf;
	int i;

	for (i = 0; i < 12; i++)
	{
		//TRACE ("findmyname ");
		get_block (parent->dev, parent->dINODE.i_block[i], buf);

		while (cp < buf + BLKSIZE)
		{
			if (dp->inode == myino)
			{
				strncpy (myname, dp->name, dp->name_len);
				myname[dp->name_len] = 0;

				TRACE ("FINDING NAME: %s, %d\n", myname, dp->name_len);

				return myino;
			}	
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
	}	
	
	return -1;
}

int findino (MINODE *mip, int *myino, int *parentino) //Returns parent ino and current ino
{
	TRACE ("IN findino\n");
	char buf[BLKSIZE];

	//TRACE ("findino ");
	get_block (mip->dev, mip->dINODE.i_block[0], buf);

	DIR *dp = (DIR *)buf;
	char *cp = buf;

	*myino = dp->inode;

	cp += dp->rec_len;
	dp = (DIR *)cp;

	*parentino = dp->inode;

	return 1;	
}

int tst_bit(char *buf, int bit) //Test a bit from a buffer
{
  int i, j;
  i = bit/8; j=bit%8; //Mailman's algorithm
  if (buf[i] & (1 << j))
     return 1;
  return 0;
}

int set_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] |= (1 << j);
}

int clr_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] &= ~(1 << j);
}

int decFreeInodes(int dev) //Decrement free inodes
{
  char buf[BLKSIZE];

  // dec free inodes count in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count--;
  put_block(dev, 2, buf);
}

int decFreeBlocks(int dev) //Decrement free blocks
{
	char buf[BLKSIZE];
	
	get_block(dev, 1, buf);
	sp = (SUPER *)buf;
	sp->s_free_blocks_count--;
	put_block(dev, 1, buf);

	get_block(dev, 2, buf);
	gp = (GD *)buf;
	gp->bg_free_blocks_count--;
	put_block(dev, 2, buf);
}

int ialloc(int dev) //Allocate new inode
{
	TRACE ("IN ialloc %d\n", dev);
	int  i;
	char buf[BLKSIZE];

	//TRACE ("ialloc GD ");	
	get_block(dev, 2, buf);
	gp = (GD *)buf;
	int imap = gp->bg_inode_bitmap; //Grab inode bitmap

	get_block(dev, 1, buf);
	sp = (SUPER *)buf;
	int ninodes = sp->s_inodes_count; //Grab max number of inodes
	
	// read inode_bitmap block
	//TRACE ("ialloc i_bitmap ");
	get_block(dev, imap, buf);

	for (i=0; i < ninodes; i++){ //Find an empty inode and allocate it
		if (tst_bit(buf, i)==0){
			set_bit(buf,i);
			decFreeInodes(dev);

			put_block(dev, imap, buf);
			
			TRACE ("Allocated inode %d\n", i+1);

			return i+1;
		}	
	}

	fprintf(stderr, "Error: no more free inodes\n");
	return 0;
}

int balloc(int dev) //Allocate new block
{
	TRACE ("IN balloc\n");
	int  i = 0;
	char buf[BLKSIZE];

	//TRACE ("balloc GD ");
	get_block(dev, 2, buf);
	gp = (GD *)buf;
	int bmap = gp->bg_block_bitmap;//Get the block bitmap

	get_block(dev, 1, buf);
	sp = (SUPER *)buf;
	int nblocks = sp->s_blocks_count;//Get the number of blocks in total

	//TRACE ("balloc b_bitmap ");
	get_block(dev, bmap, buf);

	for (i=0; i < nblocks; i++) //Search for the first open block and allocate it
	{
		if (tst_bit(buf, i)==0){
			set_bit(buf,i);
			decFreeBlocks(dev);

			put_block(dev, bmap, buf);
			memset(buf, 0, BLKSIZE);

			put_block(dev, i + 1, buf);
			
			TRACE ("Allocated block %d\n", i+1);
			return i+1;
		}
	}

	fprintf(stderr, "Error: no more free blocks\n");
	return 0;
}

int incFreeInodes(int dev) //Increment the number of free inodes
{
  char buf[BLKSIZE];

  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buf);
}

int incFreeBlocks(int dev) //Increment the number of free blocks
{
	char buf[BLKSIZE];
	
	get_block(dev, 1, buf);
	sp = (SUPER *)buf;
	sp->s_free_blocks_count++;
	put_block(dev, 1, buf);

	get_block(dev, 2, buf);
	gp = (GD *)buf;
	gp->bg_free_blocks_count++;
	put_block(dev, 2, buf);
}

int idealloc(int dev, int ino) //Deallocate a inode
{
	TRACE ("IN idealloc %d\n", ino);

	char buf[BLKSIZE];

	get_block(dev, 2, buf);
	gp = (GD *)buf;

	int imap = gp->bg_inode_bitmap;

	ino--;

	get_block(dev, imap, buf);

	clr_bit(buf,ino); //Set it to open in the bit map
	incFreeInodes(dev);

    put_block(dev, imap, buf);

	return 1;
}

int bdealloc(int dev, int bno) //Deallocate a block
{
	TRACE ("IN bdealloc %d\n", bno);

	if (bno == 0)
		return 0;
	
	char buf[BLKSIZE];

	get_block(dev, 2, buf);
	gp = (GD *)buf;

	int bmap = gp->bg_block_bitmap;

	get_block(dev, bmap, buf);

	clr_bit(buf,bno); //Set it to open in the bit map
	incFreeBlocks(dev);

    put_block(dev, bmap, buf);

	return 1;
}

