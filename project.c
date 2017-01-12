#ifdef DEBUG
#define TRACE(...) fprintf(stderr,__VA_ARGS__)
#else
#define TRACE(...)
#endif

#include "type.h"

MINODE minode[NMINODES];
MINODE *root;
PROC proc[NPROC], *running;
MOUNT mounttab[5];
OFT oftable[NOFT];

char names[64][128], *name[64];
//ELLIOT USED DEV FOR SOME OF THE LEVEL ONE STUFF
int fd, dev, n;
int nblocks, ninodes, bmap, imap, inode_start, iblock;
char pathname[256], parameter[256];

char line[128], cname[64];
void (*p[34]) (void);

int get_block (int dev, int blk, char *buf);
int put_block (int dev, int blk, char *buf);

char *cmd[] = { "menu", "mkdir", "cd", "pwd", "ls", "rmdir", "creat", "link", "unlink", "symlink", "rm", "chmod", "chown", 
	"stat", "touch", "open", "close", "pfd", "lseek", "access", "read", "write", "cat", "cp", "mv", "mount", "umount", "cs",  "fork", 
	"ps", "sync", "quit",  0 };

char *t1 = "xwrxwrxwr-------";
char *t2 = "----------------";
char *d = "0123456789";
//char *disk = "mydisk";

mount_root (char *disk)
{
	TRACE ("IN MOUNT_ROOT\n");
	char super[BLKSIZE];

	dev = open (disk, O_RDWR);
	TRACE ("Dev: %d\n", dev);
	
	//Get super block
	get_block (dev, 1, super);
	sp = (SUPER *) super;

	//Check for EXT2 file system
	if (sp->s_magic != 0xEF53)
	{
		printf ("File system is not EXT2\n");
		exit (1);
	}

	root =  (MINODE *) iget (dev, 2);	
	proc[0].cwd = (MINODE *) iget (dev, 2);
	proc[1].cwd = (MINODE *) iget (dev, 2);
}

void menu(){
    printf("K.C. Fan Club FileSystem\n"
           "Please enter one of the following options:\n"
                 "Level One:\n"
                 "menu mkdir chdir pwd ldir rmdir creat link unlink symlink rm chmod chown stat touch\n"
                 "Level Two:\n"
                 "open close pfd lseek access read write cat cp mv\n"
                 "Level Three:\n"
                 "mount umount cs fork ps sync\n"
                 "quit\n"
             );
}

int findCmd()
{
    int i = 0;

	TRACE ("IN findCmd\n");

    while (cmd[i]){

        if (strcmp(cname, cmd[i]) == 0)
            return i; //return index of command.
        i++;
    }

    return i; //invalid command. return "default"
}

int enter_name(MINODE *pip, int myino, char *myname)
{
	TRACE ("IN enter_name\n");

    int x = 0;
	char buf[BLKSIZE];

	int need_len = 4*((8+strlen(myname)+3)/4);
	int remain;

	DIR *dp;
	char *cp;
	int bno;

	for (x = 0; x < 12; x++) //go through all direct blocks.
	{
		if (pip->dINODE.i_block[x] == 0) //create new block.
		{
			bno = balloc(dev);
			pip->dINODE.i_block[x] = bno;
			pip->dINODE.i_size += BLKSIZE;

			//TRACE ("enter_name 1 ");
			memset (buf, 0, BLKSIZE);
			dp = (DIR *)&(buf[0]);
			dp->inode = myino;
			dp->rec_len = BLKSIZE;
			dp->name_len = strlen(myname);
			strncpy(dp->name, myname, strlen(myname)); //copy the name into dp->name minus the null termination.
			put_block(dev, pip->dINODE.i_block[x],buf);
			pip->dirty = 1;
			return 1;
		}

		//TRACE ("enter_name 2 ");
		get_block(dev, pip->dINODE.i_block[x], buf);
		cp = buf;
		dp = (DIR *)cp;

		while (cp + dp->rec_len < buf+BLKSIZE) //go to last entry in block.
		{	
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}

		remain = dp->rec_len - (4*((8 + dp->name_len + 3)/4)); //rec_len - ideal_len

		if (remain > need_len)
		{
			dp->rec_len = (4*((8 + dp->name_len + 3)/4)); //change len to IDEAL length
			cp+= dp->rec_len; //go to remaining space and add new dp
			dp = (DIR*) cp;
			dp-> inode = myino;
			dp-> rec_len = remain;
			dp-> name_len = strlen(myname);
			strncpy(dp->name, myname, strlen(myname)); //copy the name into dp->name minus the null termination.
			put_block(dev, pip->dINODE.i_block[x], buf);
			pip->dirty = 1;
			return 1;
		}
	}
	return -1;
}

void mymkdir(MINODE *pip, char name[256]){
	TRACE ("IN mymkdir\n");

    int ino, bno, x;
    MINODE *mip;
    char buf[BLKSIZE];
	memset (buf, 0, BLKSIZE);
    char *cp;
    DIR *mydir;

    ino = ialloc(dev);
    bno = balloc(dev);

    mip = (MINODE *)iget(dev, ino);

    mip->dINODE.i_mode = DIR_MODE;                        //DIR type and permissions
    mip->dINODE.i_uid = running->uid;                //Owner uid
    mip->dINODE.i_gid = running->gid;       //Group Id
    mip->dINODE.i_size = BLKSIZE;                        //Size in bytes
    mip->dINODE.i_links_count = 2;                    //Links count = 2 because of . and ..
    mip->dINODE.i_atime = mip->dINODE.i_ctime = mip->dINODE.i_mtime = time(0L); //set to current time
    mip->dINODE.i_blocks = 2;                                //LINUX: Blocks count in 512-byte chunks
    mip->dINODE.i_block[0] = bno;                    //new DIR has one data block

    for (x = 1; x<15; x++)
    {
        mip->dINODE.i_block[x] = 0;
    }

    mip->dirty = 1;                         //flag as dirty

    mydir = (DIR *)buf; //write .
    mydir->inode = ino;
    mydir->rec_len = 12;
    mydir->name_len = 1;
    mydir->name[0] = '.';
    cp = buf + mydir->rec_len;
    mydir = (DIR *)cp; //write ..
    mydir->inode = pip->ino;
    mydir->rec_len = 1012;
    mydir->name_len = 2;
    mydir->name[0] = '.';
    mydir->name[1] = '.';

	put_block(dev, mip->dINODE.i_block[0], buf);

    enter_name(pip, ino, name);
	iput(mip);
}

int SearchForChild(INODE *dinode, char child[256]) //Takes the parent inode, and the name of the child searching for.
{
	TRACE ("IN SearchForChild: %s\n", child);

    int x;
    char BLK[BLKSIZE];
    DIR *dir;
    char *tempCount = BLK;
	char tempname[256];

    for (x = 0; x<12; x++) //check direct blocks
    {
        get_block( dev, dinode->i_block[x], BLK); //get block

        while (tempCount < BLK + BLKSIZE){ //check whole block for inodes

            if (*tempCount == 0) //check to make sure data block is not null.
                return 0;

            dir = (DIR *)tempCount; //set the dir equal to the char* pointer
            strncpy(tempname, dir->name, dir->name_len); //copy the name from dir and add a '0' to the end.
            tempname[dir->name_len] = 0;

            if (strcmp(tempname, child) == 0) //check to see if the tempname is the same as the child's name. If so, return 1 (child found)
            {
				TRACE ("SearchForChild: %s\n", child);
                return 1;
            }

            tempCount+= dir->rec_len;
        }
    }
    return 0;
}

void make_dir(){
	TRACE ("IN make_dir\n");

	char temp[256];
	char *parent, *child;
	int pino;
	MINODE *pip;

	if (pathname[0] ==  0) //no pathname.
	{
		fprintf(stderr, "Error: No pathname specified.\n");
		return;
	}

	if (pathname[0] == '/') //absolute
	{
		dev = root->dev;
	}
	else //relative
	{
		dev = running->cwd->dev;
	}
	
	strcpy(temp, pathname);
	parent = dirname(pathname);
	//strcpy (parent, dirname(pathname));
	child = basename(temp);
	//strcpy (child, basename(pathname));
	
	pino = getino(&dev, parent);
	if (pino < 2)
	{
		fprintf (stderr, "Error: Bad path specified\n");
		return;
	}

	pip = (MINODE *)iget(dev, pino);

	if ((pip->dINODE.i_mode & 0x4000)>1 && !SearchForChild(&(pip->dINODE), child)){ //check to see if it is a directory or not AND check to see
		//if the child exists in this directory.

		mymkdir(pip, child);

        pip->dINODE.i_links_count++;
        pip->dirty = 1;
	}
    iput(pip);
}

void change_dir(){
	TRACE ("IN change_dir\n");

	int ino;
	MINODE *mp;

	ino = getino(&dev, pathname); //grab the inode of the directory you want to switch to.

	if (ino < 2) //if invalid, return.
	{
		fprintf (stderr, "Error: Directory does not exist\n");
		return;
	}

	mp = (MINODE *) iget(dev, ino);
	iput (running->cwd);

	TRACE ("Test: %d\n", mp->dINODE.i_mode & 0x4000);

	if ((mp->dINODE.i_mode & 0x4000) == 0)
	{
		fprintf (stderr, "Error: Not a directory\n");
		return;
	}

	running->cwd = mp;

	/*int i;
	TRACE ("Datablocks of %d\n", running->cwd->ino);
	for (i = 0; i < 12; i++)
		TRACE ("%d ", running->cwd->dINODE.i_block[i]);
		TRACE ("\n");*/

	return;
}

void rpwd(MINODE *cwd){
	if (cwd->ino == 2)
		return;

	TRACE ("IN rpwd: %x %d\n", cwd, cwd->ino);

	char tname[256];
	char buf[BLKSIZE];
	int pino;
	int cino;

	get_block(dev, cwd->dINODE.i_block[0], buf);

	char *cp = buf;
	DIR *dp = (DIR *)cp;

	cino = dp->inode;

	cp += dp->rec_len;
	dp = (DIR *)cp;

	pino = dp->inode;

	TRACE ("%d %d\n", cino, pino);

	MINODE *mp = (MINODE *) iget (dev, pino);

	TRACE ("%x\n", mp);

	rpwd(mp);
	findmyname (mp, cino, tname); //get the name of this entry
	printf("/%s", tname); //print "/name"

	iput(mp); //put the mp back just in case.
}

void pwd(){
	TRACE ("IN pwd\n");

	MINODE *temp = running->cwd;
	rpwd(temp);
	putchar('\n');
}

void print_inode(MINODE *entry)
{
	char timehold[64];
	strcpy(timehold, (char *)ctime(&(entry->dINODE.i_mtime))); //Grabs the modify time as a string
	timehold[strlen(timehold) - 1] = 0;

	if ((entry->dINODE.i_mode & 0xF000) == 0x8000) //Print a - if it's a regular file
		printf("%c",'-');
	if ((entry->dINODE.i_mode & 0xF000) == 0x4000) //Print a d if it's a directory
		printf("%c",'d');
  	if ((entry->dINODE.i_mode & 0xF000) == 0xA000) //Print an l if it's a link
		printf("%c",'l');

	int i;
	//Print out the permission bits
  	for (i=8; i >= 0; i--){
		if (entry->dINODE.i_mode & (1 << i))
			printf("%c", t1[i]);
    	else
			printf("%c", t2[i]);
  	}
	
	printf("%4d ",entry->dINODE.i_links_count); //Print number of links
  	printf("%4d ",entry->dINODE.i_gid); //Print group ID
 	printf("%4d ",entry->dINODE.i_uid); //Print user ID
  	printf("%8d ",entry->dINODE.i_size); //Print size

	printf ("%s ", timehold); //Print out the modify time
}

void list_dir(){
	TRACE ("IN list_dir\n");

	int ino, dev, i;
	MINODE *mip;
	char buf[BLKSIZE];
	DIR *dp = (DIR *)buf;
	char *cp;
	char *np;

	//If no pathname was specified, grab the current working directory
	if (pathname == 0 || strlen(pathname) == 0)
	{
		ino = running->cwd->ino;
		dev = running->cwd->dev;
	}
	else //Grab the inode number of the specified path
	{
		if (pathname[0] == '/')
			dev = root->dev;
		ino = getino (&dev, pathname);	
	}	

	if (ino < 2)
	{
		fprintf (stderr, "Error: Bad path specified\n");
		return;
	}
	mip = (MINODE *) iget (dev, ino);

	//Iterate through the direct blocks
	for (i = 0; i < 12; i++)
	{
		if (mip->dINODE.i_block[i] == 0)
			break;

		//TRACE ("list_dir ");
		get_block (dev, mip->dINODE.i_block[i], buf);
		cp = buf;
		dp = (DIR *)buf;

		while (cp < buf + BLKSIZE)
		{
			np = (char *) malloc (sizeof(char) * (dp->name_len + 1)); 
			strncpy (np, dp->name, dp->name_len);

			np[dp->name_len] = 0;

			MINODE *entry = (MINODE *) iget (dev, dp->inode);

			TRACE ("%d %d ", dp->inode, entry->refCount);

			print_inode(entry);
			printf ("%s\n", np);
			
			iput(entry);
			free(np);
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
		putchar('\n');
	}

	iput (mip);
}

int rm_child (MINODE *parent, char *name)
{
	TRACE ("IN rm_child %s\n", name);

    int x, i;
    char BLK[BLKSIZE];
    char *tempCount = BLK;
	char tempname[256];
    DIR *dir = (DIR *)tempCount;

	TRACE ("Start: %d\n", tempCount);

    for (x = 0; x<12; x++) //check direct blocks
    {
		memset (BLK, 0, BLKSIZE);
        get_block( dev, parent->dINODE.i_block[x], BLK); //get block

		tempCount = BLK;
		dir = (DIR *) tempCount;

        while (tempCount < BLK + BLKSIZE)
		{ //check whole block for inodes

            if (*tempCount == 0) //check to make sure data block is not null.
			{
				TRACE ("BLARGH\n");
                return 0;
			}

            strncpy(tempname, dir->name, dir->name_len); //copy the name from dir and add a '0' to the end.
            tempname[dir->name_len] = 0;

            if (strcmp(tempname, name) == 0) //check to see if the tempname is the same as the child's name. If so, return 1 (child found)
            {
				TRACE ("Found child in block %d\n", x);
				i = x;
				x = 12;
				break; //Found the child
            }

            tempCount += dir->rec_len;
			dir = (DIR *) tempCount;
			TRACE ("Iterating: %d\n", tempCount);
        }
    }

	TRACE ("Out of search\n");
	if (i == 11 && tempCount == BLK + BLKSIZE)
		return 0; //Child not found

	if (tempCount == BLK && dir->rec_len == BLKSIZE) //If the it is the first and only entry in the block
	{
		TRACE ("First entry\n");
		//Decrement the size of the directory
		parent->dINODE.i_size -= BLKSIZE;
	
		//Deallocate the block and shuffle the other blocks down
		bdealloc (dev, parent->dINODE.i_block[i]);
		parent->dINODE.i_block[i] = 0;
		while (x + 1 < 12)
		{
			if (parent->dINODE.i_block[i + 1] == 0)
				break;
			parent->dINODE.i_block[i] = parent->dINODE.i_block[i+1];
			i++;
		}
		parent->dINODE.i_block[i+1] = 0;
		return 1; //Don't need to do any more
	}
	
	int len = dir->rec_len;
	TRACE ("len: %d\n", len);

	if (tempCount + dir->rec_len >= BLK + BLKSIZE) //If it's the last entry
	{
		TRACE ("Last entry\n");
		char *cp = BLK;
		dir = (DIR *) cp;

		while (cp + dir->rec_len < tempCount)
		{
			cp += dir->rec_len;
			dir = (DIR *)cp;
		}
		dir->rec_len += len;
	}
	else //It's somewhere in the middle
	{
		TRACE ("Somewhere in middle %d %d %d\n", tempCount, tempCount + len, BLK + BLKSIZE - (tempCount + len));

		char *cp = BLK;
		dir = (DIR *) cp;		
		while (cp + dir->rec_len < BLK + BLKSIZE)
		{
			cp += dir->rec_len;
			dir = (DIR *) cp;
		}
		dir->rec_len += len;

		memcpy(tempCount, tempCount + len, BLK + BLKSIZE - (tempCount + len));
	}
	
	TRACE ("Final rec_len: %d\n", dir->rec_len);
	put_block (dev, parent->dINODE.i_block[i], BLK);
	parent->dirty = 1;
	return 1;
}

//Remove a directory from the file system
void rmdir(){
	TRACE ("IN rmdir\n");

	int ino;
	MINODE *mip;

	if (pathname == 0 || strlen(pathname) == 0)
	{
		fprintf (stderr, "Error: No directory specified. (Don't remove root, please)\n");
		return;
	}

	if (pathname[0] == '/')
		dev = root->dev;
	ino = getino (&dev, pathname);

	if (ino < 3)
	{
		fprintf (stderr, "Error: Invalid directory specified\n");
		return;
	}
	
	//Grab the inode of the directory to remove
	mip = (MINODE *) iget (dev, ino);

	//Should check for permissions

	if ((mip->dINODE.i_mode & 0x4000) == 0)	//Check if path is directory or not
	{
		fprintf (stderr, "Error: Specified file is not a directory\n");
		return;
	}

	if (mip->refCount > 1) //Make sure no one else is using it
	{
		fprintf (stderr, "Error: Directory is in use by other processes\n");
		return;
	}

	if (mip->dINODE.i_links_count > 2) //Check if directory could be empty
	{
		fprintf (stderr, "Error: Directory is not empty (Don't even try -r)\n");
		return;
	}

	char buf[BLKSIZE];
	char *cp = buf;
	DIR *dp = (DIR *)cp;

	//Grab the first directory block
	get_block(mip->dev, mip->dINODE.i_block[0], buf);
	
	cp += dp->rec_len;
	dp = (DIR *)cp;
	
	//If there are any entries other than . and ..
	if (dp->rec_len < 1012)
	{
		fprintf (stderr, "Error: Directory is not empty (-r is not supported)\n");
		return;
	}

	//Clear the dir's i_block and inode.
	bdealloc(mip->dev, mip->dINODE.i_block[0]);
	mip->dINODE.i_block[0] = 0;
	idealloc(mip->dev, mip->ino); //Free the inode number for use again
	
	char temp[256];
	
	strcpy(temp, pathname);

	//DON'T LET IT MANGLE PATHNAME
	//Grab the inode number of the parent
	int pi = getino(&dev, dirname(temp));
	
	if (pi < 2)
	{
		fprintf (stderr, "Error: Um\n");
		return;
	}

	//Get the parent inode into ram
	MINODE *pip = (MINODE *) iget(dev, pi);

	//DON'T LET THE PATHNAME GET MANGLED AGAIN
	//Remove the entry from the parent's directory entries
	rm_child (pip, basename(pathname));

	//Decrement the link count to the parent directory
	pip->dINODE.i_links_count--;
	//TOUCH TIME STUFF for the directory that was deleted
	mip->dINODE.i_mtime = time(0L);
	mip->dINODE.i_dtime = time(0L);

	pip->dirty = 1;
	iput (pip);

	mip->dirty = 1;
	iput(mip); //Put the minode back
}

int mycreat(MINODE *pip, char *child)
{
    TRACE ("IN mycreat: %s\n", child);

    int ino, bno, x;
    MINODE *mip;
    DIR *mydir;

    ino = ialloc(dev);

    mip = (MINODE *)iget(dev, ino); //get minode of this inode.

    mip->dINODE.i_mode = FILE_MODE;                        //File type and permissions
    mip->dINODE.i_uid = running->uid;                //Owner uid
    mip->dINODE.i_gid = running->gid;       //Group Id
    mip->dINODE.i_size = 0;                        //Size in bytes
    mip->dINODE.i_links_count = 1;                    //Links count = 2 because of . and ..
    mip->dINODE.i_atime = mip->dINODE.i_ctime = mip->dINODE.i_mtime = time(0L); //set to current time
    mip->dINODE.i_blocks = 0;                            //No iblocks

    for (x = 0; x<15; x++)
    {
        mip->dINODE.i_block[x] = 0;
    }

    mip->dirty = 1;                         //flag as dirty
    iput(mip);                                  //write INODE to disk
                        //What is name supposed to be?
    enter_name(pip, ino, child); //enter this inode into parents data blocks.

    return 1;
}

int icreat(char path[])
{
    TRACE ("IN icreat %s\n", pathname);

    char *parent, *child, temp[256];
    int pino, rval = -1;
    MINODE *pip;

    if (path[0] ==  0) //no pathname.
    {
        printf("Error: No file name specified.\n");
        return rval;
    }

    if (path[0] == '/') //absolute
    {
        dev = root->dev;
    }
    else //relative
    {
        dev = running->cwd->dev;
    }

    strcpy(temp, path); //copy so it doesn't get mangled
    parent = dirname(temp);

    pino = getino(&dev, parent);
	if (pino == -1) {return -1;}

    pip = (MINODE *) iget(dev, pino);

    strcpy(temp, path); //copy so it doesn't get mangled
    child = basename(temp);

    if ((pip->dINODE.i_mode & 0x4000)>1 && !SearchForChild(&(pip->dINODE), child)){ //check to see if it is a directory or not AND check to see
                                                                            //if the child exists in this directory.
        mycreat(pip, child); //Call mymkdir

        rval = 1;
        pip->dirty = 1;
    }

    iput(pip); //put parent minode back into filesystem
    return rval;
}

void creat_file(){ //Create a directory and then use file surgery to transform it to a file
	TRACE ("IN creat_file %s\n", pathname);

	char temp[256];
    int rval;

    strcpy(temp, pathname);
    rval = icreat(temp); //see if creat works or not

    if (rval == 1) {printf("Creat Successful\n");}
    else {printf("Creat failed\n");}
}

void link(){

	int ino, npino;
	char *newp, *newc, dest[256];
	MINODE *mpip, *mip;

	strcpy (dest, parameter);	

	ino = getino(&dev, pathname); //File we want to make a link to
	npino = getino(&dev, dirname(dest)); //Parent directory of destination

	if (ino < 1 || npino < 1)  //make sure the inodes are valid.
	{
		fprintf (stderr, "Error: Either the source file or the destination directory don't exist\n");
		return;
	}

//	if (SearchForChild(npino, newc)) //make sure the file doesn't already exist in target destination
//		return;

	newc = basename(parameter); //set new child name to basename
	newp = pathname;//dirname(pathname); //set new parent to dirname

	mpip = (MINODE *) iget(dev, npino);
	mip = (MINODE *) iget(dev, ino);

	TRACE ("Bit Tests:%x %d %d\n", mip->dINODE.i_mode, (mip->dINODE.i_mode & 0x8000), (mip->dINODE.i_mode & 0xA000));
	if ((mip->dINODE.i_mode & 0x8000) == 0 && (mip->dINODE.i_mode & 0xA000) == 0)
	{
		fprintf (stderr, "Error: File specified is not a regular file or link\n");
		return;
	}

	if (enter_name(mpip, ino, newc) < 1) //insert the new dir into parent of link.
	{
		fprintf (stderr, "Error: File could not be added (Does it already exist?)\n");
		return;
	}

	mip->dINODE.i_links_count++;
	mip->dirty = 1;
	iput(mip);
	mpip->dirty = 1;
	iput(mpip);
}

void symlink(){
	TRACE ("IN symlink\n");	

    int ino, nino, pino; //inode and new inode numbers
    char temp[256];
    MINODE *mp, *parent;
        int success;

    ino = getino(&dev, pathname); //grab old file
    if (ino < 2) //verify that ino is ok.
        return;

    strcpy(temp, pathname); //copy pathname into temp
    strcpy(pathname, parameter); //copy parameter into pathname

    success = icreat(parameter); //create file with parameter (new file)

        if (success != 1) {
            printf("Symlink Failed\n");
            return;
        }

        TRACE ("%s\n", parameter);

        nino = getino(&dev, parameter); //grab the new file you just created.

    mp = (MINODE *)iget(dev, nino); //grab an MINODE of the new file you created.

    mp->dINODE.i_mode = 0120000; //change type to LNK
        mp->dINODE.i_size = strlen(parameter);

        strncpy(mp->dINODE.i_block, temp, strlen(temp)); //this will write the old filename into the i_block[] of the new file.

    mp->dirty = 1;

    iput (mp);

}

void rm_file(){

	int ino;
	MINODE *mip;

	if (pathname == 0 || strlen(pathname) == 0)
	{
		fprintf (stderr, "Error: No file specified\n");
		return;
	}

	if (pathname[0] == '/')
		dev = root->dev;
	ino = getino (&dev, pathname);

	if (ino < 3) 
	{
		fprintf (stderr, "Error: Bad file specified\n");
		return;
	}
	
	mip = (MINODE *) iget (dev, ino);

	//SOMETHING SOMETHING SOMETHING PROCESS
	//SOMETHING SOMETHING SOMETHING CHECK PERMISSIONS
	//SOMETHING SOMETHING SOMETHING RETURN IF NOT ALLOWED
	//WHO CARES?!

	if ((mip->dINODE.i_mode & 0x4000) > 0)	
	{
		fprintf (stderr, "Error: Specified file is a directory (use rmdir)\n");
		return;
	}

	if (mip->refCount > 1)
	{
		fprintf (stderr, "Error: File is in use by other processes %d\n", mip->refCount);
		return;
	}

	if (mip->dINODE.i_links_count > 1)
	{
		fprintf (stderr, "Error: File has multiple links (Good luck finding them)\n");
		return;
	}

	char buf[BLKSIZE];

	//TRACE ("rmdir ");
	int i;
	for (i = 0; i < 12; i++)
	{
		if (mip->dINODE.i_block ==0)
			break;
		bdealloc(mip->dev, mip->dINODE.i_block[i]);
	}

	idealloc(mip->dev, mip->ino);
	
	iput(mip);

	char temp[256];
	
	strcpy(temp, pathname);

	//DON'T LET IT MANGLE PATHNAME
	int pi = getino(&dev, dirname(temp));
	if (pi < 2)
	{
		fprintf (stderr, "Error: Parent directory does not exist, somehow\n");
		return;
	}

	MINODE *pip = (MINODE *) iget(dev, pi);

	//DON'T LET IT HAPPEN AGAIN
	rm_child (pip, basename(pathname));

	pip->dINODE.i_links_count--;
	//TOUCH TIME STUFF
	mip->dINODE.i_mtime = time(0L);
	mip->dINODE.i_dtime = time(0L);
	pip->dirty = 1;
	iput (pip);
}

void unlink(){
	int ino, pino;
	MINODE *mp, *parent;
	char temp[256];

	strcpy (temp, pathname);

	ino = getino(&dev, pathname); //grab ino
	if (ino<3) 
	{
		fprintf (stderr, "Error: Invalid file path specified\n");
		return;
	} //check for valid ino
 
	mp = (MINODE *)iget(dev, ino);

	if (mp->dINODE.i_links_count == 1) //delete file if there would be no more links to the file
	{
		iput (mp);
		rm_file();
		return;
	}

	pino = getino (&dev, dirname(temp));
	if (pino < 2) 
	{
		fprintf (stderr, "Error: I'm actually impressed you triggered this one\n");
		return;
	}
	
	parent = (MINODE *)iget(dev, pino);

	if ((mp->dINODE.i_mode & 0x8000) == 0) 
	{
		fprintf (stderr, "Error: Can't unlink a directory\n");
		return;
	} //Check to make sure its a file.

	mp->dINODE.i_links_count--; //decrement link counter
	
	rm_child (parent, basename(pathname));

	iput (mp);
	iput (parent);
}

void chmod_file(){
	TRACE ("IN chmod_file\n");

	int ino;
	MINODE *mip;
	int temp;

	ino = getino(&dev, parameter);
	if (ino<2) 
	{
		fprintf (stderr, "Error: File does not exist\n");
		return;
	} //make sure valid ino

	mip = (MINODE *) iget(dev, ino);

	sscanf(pathname, "%o", &temp); //grab the octal value from the string.
	mip->dINODE.i_mode &= 0xFE00; //Preserve leading bits
	mip->dINODE.i_mode |= temp; //change the priveledges of the file.

	mip->dirty = 1;
	iput(mip);
	return;
}

void chown_file(){
	TRACE ("IN chown_file\n");

	int ino;
	MINODE *mip;
	int temp;

	ino = getino(&dev,parameter);
	if (ino<2) 
	{
		fprintf (stderr, "Error: File does not exist\n");
		return;
	} //make sure valid ino

	mip = (MINODE *) iget(dev, ino);

	sscanf(pathname, "%d", &temp); //grab the octal value from the string.

	mip->dINODE.i_uid = temp; //change owner id to parameter YOU NEED TO CONVERT PARAMETER TO A DIGIT!!!!!!!!!!!!!!!!!!!1

	mip->dirty = 1;
	iput(mip);
	return;
}

void stat_file(){
	TRACE ("IN stat_file\n");

	int ino;
	MINODE *mip;

	ino = getino(&dev,pathname);

	if (ino<2) 
	{
		fprintf (stderr, "Error: File does not exist\n");
		return;
	} //make sure valid ino

	mip = (MINODE *) iget(dev, ino);

	printf("Inode: %d\tDevice: %d\n", ino, dev);
	printf("Mode : %o\n", mip->dINODE.i_mode);
	printf("User : %d\t", mip->dINODE.i_uid);
	printf("Group: %d\n", mip->dINODE.i_gid);
	printf("Links: %d\t", mip->dINODE.i_links_count);
	printf("Size: %d\n", mip->dINODE.i_size);
	printf("Access Time: %s", ctime(&(mip->dINODE.i_atime)));
	printf("Create Time: %s", ctime(&(mip->dINODE.i_ctime)));
	printf("Modify Time: %s", ctime(&(mip->dINODE.i_mtime)));
	//printf("i_dtime = %d\n", mip->dINODE.i_dtime);
	iput (mip);
}

void touch_file(){
	TRACE ("IN touch_file\n");

	int ino;
	MINODE *mip;

	ino = getino(&dev,pathname);

	if (ino<2) 
	{
		fprintf (stderr, "Error: File does not exist\n");
		return;
	} //make sure valid ino

	mip = (MINODE *) iget(dev, ino);

	mip->dINODE.i_mtime = time(0L);
	mip->dirty = 1;
	iput(mip);
	return;
}

/*Beginning of Level 2*/
int truncate(MINODE *mip) //deallocates every block for an inode.
{
    int x = 0;
    int *bp;
    int *bp2;
    char ibuf[BLKSIZE];
	char dbuf[BLKSIZE];

    for (x = 0; x < 12; x++) //direct blocks
    {
        if (mip->dINODE.i_block[x] != 0) //if it has data.
        {
            bdealloc(dev, mip->dINODE.i_block[x]); //deallocate it.
        }
        else
            break;
    }
    x = 12;//indirect
    if (mip->dINODE.i_block[x] != 0)
    {
		get_block (dev, mip->dINODE.i_block[x], ibuf);
        bp = (int *)ibuf;
        while (bp < (int *)(ibuf + BLKSIZE)) //loop through indirect block and get new blocks
        {
            if (*bp == 0) //make sure that part of the block is written into.
            {
                break;
            }

            bdealloc(dev, *bp);
			*bp = 0;
            bp++;
        }
        bdealloc(dev, mip->dINODE.i_block[x]);
		mip->dINODE.i_block[x] = 0;
    }

    x = 13; //double indirect
    if (mip->dINODE.i_block[x] != 0)
    {
		get_block (dev, mip->dINODE.i_block[x], ibuf);
        bp = (int *)ibuf;
        while (bp < (int *)(ibuf + BLKSIZE))
        {
            if (*bp == 0) {break; } //make sure valid block

            get_block(dev, *bp, dbuf); //grab the block at bp location.
            bp2 = (int *)dbuf; //grab that block as an int*

            while (bp2 < (int *)(dbuf + BLKSIZE)) //loop this block and de-allocate all blocks that are pointed to inside.
            {
                if (*bp2 == 0) {break;} //make sure that part of the block is written into.
                bdealloc(dev, *bp2); //dealocate the block that bp2 is pointing to.
				*bp2 = 0;
                bp2++; //increment bp2
            }
            bdealloc(dev,*bp);
			*bp = 0;
            bp++;
        }
        bdealloc(dev, mip->dINODE.i_block[x]);
		mip->dINODE.i_block[x] = 0;
    }

    //all blocks dealocated.
    mip->dINODE.i_mtime = time(0L); //update inode's time.
    mip->dINODE.i_size = 0;
    mip->dirty = 1;
    return 1;
}

int my_open(char path[], int opentype ){ //called from anywhere
    TRACE ("IN my_open\n");

    int ino;
    MINODE *mp;
    int x = 0, i = 0;

    if (strlen(path) == 0)
    {
        fprintf (stderr, "Error: No file specified\n");
        return -1;
    }

    if (path[0] == '/') //if root, set dev to root dev.
        dev = root->dev;
    else
        dev = running->cwd->dev; //if else, use cwd dev

    ino = getino(&dev, path);
	if (ino < 3)
	{
		fprintf (stderr, "Error: Bad file specified\n");
		return -1;
	}
    mp = (MINODE *)iget(dev, ino);

    if ((mp->dINODE.i_mode & 0x8000) == 0)
    {
        fprintf (stderr, "Error: File was not specified\n");
        return -1;
    } //make sure this is a normal file.

    for (x = 0; x < NOFT; x++) //double check logic, loop through all openfiletable structs in oftable[]
    {
        if (oftable[x].mode == -1) {break;} //if hit end, or empty member of table, end.
        if (oftable[x].inodeptr == mp && oftable[x].mode > 0) {return;} //check to see if there is another open version of this file that's not read.
    }
    //If you hit here, x will be the index of a free member of oftable OR the end of it. check to make sure you're not past the limit....
    if (x >= NOFT)
    {
        fprintf (stderr, "Error: No more open spots in OFT\n");
        return -1;
    }

    //Hit here, means you're ready to put new open file pointer in here

    sscanf(parameter, "%d", &opentype); //grab the open type
	
	if (opentype < 0 || opentype > 3)
	{
		fprintf (stderr, "Error: Invalid open type specified (0 read, 1 write, 2 rdwr, 3 append)\n");
		return -1;
	}
    oftable[x].mode = opentype;
    oftable[x].refCount = 1;
    oftable[x].inodeptr = mp;

    switch(oftable[x].mode){
        case 0:
            oftable[x].offset = 0;
        break;

        case 1:
			truncate(oftable[x].inodeptr);
            oftable[x].offset = 0;
        break;

        case 2:
            oftable[x].offset = 0;
        break;

        case 3:
            oftable[x].offset = mp->dINODE.i_size; //append
        break;

        default: fprintf(stderr, "Error: Invalid open type\n");
            return -1;
            break; //pointless line...
    }

    for (i = 0; i < NFD; i++)
    {
        if (running->fd[i] == 0){ //if it's null,
            running->fd[i] = &(oftable[x]); //point the first null fd[i] to the open file table entry
            break; //so you don't increment.
        }
    }

    if (i >= NFD ) //if this hits, the current running process fd[] was full.
    {
        fprintf (stderr, "Error: Process has too many files open\n");
        return -1;
    }

    mp->dINODE.i_mtime = time(0L);
    if (opentype > 0) {mp->dirty = 1;} //not read only mode, make dirty

    return i; //return index of running->fd[] where the file stuff is.
}

void open_file(){
	TRACE ("IN open_file\n");

    int opentype, lfd;
    sscanf(parameter,"%d", &opentype);
    lfd = my_open(pathname, opentype);

    if (lfd == -1) { fprintf(stderr, "Error: Failed to open file\n"); } //If unsuccessful
    else { fprintf(stdout, "File Descriptor: %d\n", lfd); } //If successful}
}

void close_file(){
	TRACE ("IN close_file %s\n", pathname); //Pathname should be file descriptor

    int lfd;
    OFT *oftp;
    MINODE *mp;

    sscanf(pathname, "%d", &lfd); //grab local file descriptor

    if (lfd < 0 || lfd > NFD) 
	{
		fprintf (stderr, "Error: Invalid file descriptor specified\n");
		return -1;
	} //return failure if fd out of range.
    
	if (!(running->fd[lfd])) 
	{
		fprintf (stderr, "Error: File descriptor is not open\n");
		return -1;
	} //make sure running->fd[lfd] is pointing to an oft entry

    oftp = running->fd[lfd];
    running->fd[lfd] = 0;
    oftp->refCount--;

    if (oftp->refCount > 0) 
	{
		fprintf (stderr, "Error: File is in use by other processes\n");
		return 0;
	} //if there are still references to the file descriptor, leave it.

	mp = oftp->inodeptr; //set the mp equal to the inodepointer before disposing of this file descriptor
	oftp->mode = -1;
	oftp->refCount = 0;
	oftp->inodeptr = 0;
	oftp->offset = 0;
	//this will basically 'dispose' of the inodeptr. Our code checks the mode to see if -1 for a free inodeptr (in our code under open_file())

    iput(mp); //put back the mip, if it's been written to, it will be dirty.
    return;
}

void lseek_file(){ //returns -1 if invalid position
	TRACE ("IN lseek_file\n");

    int lfd, position, originalposition;
    OFT *op;

    sscanf(pathname,"%d",&lfd); //grab local file descriptor
    sscanf(parameter,"%d", &position); //grab position
	
    op = running->fd[lfd];

    if (position > op->inodeptr->dINODE.i_size || position < 0) 
	{
		fprintf (stderr, "Error: Seeking out of file boundaries\n");
		return;
	} //make sure you're not changing the position past boundary.

    originalposition = op->offset;
    op->offset = position; //set the new position

    return;// originalposition; //return old position

}

void pfd(){
	TRACE ("IN pfd\n");
	
	int i;

	fprintf (stdout, "fd\tMode\toffset\tDev,Inode\n");

	for (i = 0; i < NOFT; i++)
	{
		if (running->fd[i] == 0)
			continue;
		fprintf (stdout, "%d\t", i);
		
		switch (running->fd[i]->mode)
		{
			case 0:
				fprintf (stdout, "Read\t");
			break;
			case 1:
				fprintf (stdout, "Write\t");
			break;
			case 2:
				fprintf (stdout, "Rd/Wr\t");
			break;
			case 3:
				fprintf (stdout, "Append\t");
			break;
			default:
				fprintf (stdout, "ERROR\t");
			break;
		}
		fprintf (stdout, "%d\t", running->fd[i]->offset);
		fprintf (stdout, "[%d, %d]\n", running->fd[i]->inodeptr->dev, running->fd[i]->inodeptr->ino);
	}
}

void access_file(){
	TRACE ("IN access_file\n");
	fprintf (stderr, "Error: This function is not implemented\n");	
}

int myread(int lfd, char *buf, int nbytes)
{
	TRACE ("IN myread %d %d\n", lfd, nbytes);

    int count = 0;
    int avil; //bytes still left in file.
    int fileSize, startByte;
    int lblk; //logical block,
    int blk; //actual block address.
    char readbuf[BLKSIZE], *cp;
	memset (readbuf, 0, BLKSIZE);

    OFT *op; //temp pointer to current filedescriptor openfiletable

    op = running->fd[lfd];
    fileSize = op->inodeptr->dINODE.i_size; //get file size

    avil = fileSize - op->offset; //set the available size to the size of the file, minus the offset.

    while (nbytes && avil) //while you still have more bytes available AND there are more bytes to read.
    {
        lblk = op->offset/BLKSIZE; //populate the logical block with the offset / blksize.
        startByte = op->offset % BLKSIZE; //populate the startByte with offset mod BLKSIZE


        if (lblk < 12){ //direct block
            blk = op->inodeptr->dINODE.i_block[lblk]; //grab actual block.
        }
        else if (lblk >= 12 && lblk < 256+12){//indirect blocks
			char ibuf[BLKSIZE];
			int *ip1;
			memset (ibuf, 0, BLKSIZE);
			//int *ip;
			get_block (running->cwd->dev, op->inodeptr->dINODE.i_block[12], ibuf);
			ip1 = ibuf + ((lblk - 12) * 4);
			blk = *ip1;
        }
        else{ //double indirect blocks
			char ibuf[BLKSIZE];
			char dbuf[BLKSIZE];
			memset (ibuf, 0, BLKSIZE);
			memset (dbuf, 0, BLKSIZE);
			int *ip2;
			get_block (running->cwd->dev, op->inodeptr->dINODE.i_block[13], ibuf);
			int *bi = ibuf + (((lblk - 268) / 256)* 4);//Subtract 268 to account for the fact that 2x indirect blocks start at 268, not 0
			get_block (running->cwd->dev, *bi, dbuf);
			ip2 = dbuf + (((lblk - 268) % 256) * 4);
			blk = *ip2;
        }

		TRACE ("readblk: %d\n", blk);
        get_block(running->cwd->dev, blk, readbuf); //get block into readbuf

        cp = readbuf + startByte;
        int remain = BLKSIZE - startByte;

		if (nbytes < avil)
			avil = nbytes;
		//If there's less of the file left to read than the user wants
		if (avil < remain)
			remain = avil; //Don't read past the end of the file.

        if (remain > 0){
			memcpy (buf, cp, remain); //Copy a chunk at a time
			op->offset += remain; //Adjust values as necessary
			count += remain;
			avil -= remain;
			nbytes -= remain;
			remain = 0;
        }
    }

	return count;
}

void read_file(){ //assumes pathname is filedescriptor, and second is number of bytes to read
	TRACE ("IN read_file\n");

    int lfd, nbytes;
    OFT* op;
    op = running->fd[lfd];
	char buf[BLKSIZE];

    sscanf(pathname, "%d", &lfd); //grab the file descriptor and put into lfd, local file descriptor.
    sscanf(parameter, "%d", &nbytes); //grab the file descriptor and put into lfd, local file descriptor.

    if (!op) 
	{	
		fprintf (stderr, "Error: File is not open\n");
		return -1; 
	} //return -1 , no bytes read.
    if (op->mode == 0 || op->mode == 2) //myread(lfd,buf, nbytes);
    {
		//Where buf from and why is it returning a value?
        myread(lfd, buf, nbytes);
    }
	
	fprintf (stdout, "%s\n", buf);

    return;
}

int mywrite(int lfd, char buf[], int nbytes)
{
	TRACE ("IN mywrite: %d \n", nbytes);

    int lblk; //block to start writing.
    int startByte; //byte to start writing.
    OFT *op = running->fd[lfd]; //pointer to oft entry inside file descriptor.
    int blk; //the actual block in use.
    char wbuf[BLKSIZE], *cp, *cq = buf;  //cp will point to wbuf, cq will point to buf writing from,
															//wbuf will be the buff writing to.
    int tempnbyte; //used to calculate how many bytes written.
    int *ipOne, *ipTwo;

    tempnbyte = nbytes;
    
    while (nbytes > 0){
        lblk = op->offset / BLKSIZE;
        startByte = op->offset % BLKSIZE;

		TRACE ("Write lblk: %d\n", lblk);

        if (lblk < 12){ //direct blocks
			if (op->inodeptr->dINODE.i_block[lblk] == 0)
			{
				op->inodeptr->dINODE.i_block[lblk] = balloc(op->inodeptr->dev); //allocate block
				op->inodeptr->dirty = 1;
			}
            blk = op->inodeptr->dINODE.i_block[lblk]; //point block to the actual block.
		}    
        else if (lblk >= 12 && lblk < 256 + 12) //indirect blocks.
        {
			TRACE ("Indirect#: %d\n", lblk - 12);
			char tempbuf[BLKSIZE];
			memset(tempbuf, 0, BLKSIZE); //fill in with 0s
            if (op->inodeptr->dINODE.i_block[12] == 0){ //if no indirect block, allocate.
                op->inodeptr->dINODE.i_block[12] = balloc(op->inodeptr->dev);
                op->inodeptr->dirty = 1; //set dirty
            }
            get_block(op->inodeptr->dev, op->inodeptr->dINODE.i_block[12], tempbuf); //grab the indrect block and put in tempbuf
            ipOne = tempbuf + ((lblk - 12) * 4);

			if (!*ipOne) //if *ipOne is null, allocate it.
			{
				*ipOne = balloc(op->inodeptr->dev); 
				op->inodeptr->dirty = 1;
				put_block(op->inodeptr->dev, op->inodeptr->dINODE.i_block[12], tempbuf); 
			}
            blk = *ipOne;
			//blk = *(tempbuf + (lblk -12));
        }
        else //(lblk >= 256+12 && lblk < 256*256 + 256 + 12) //double indirect blocks
        {
			char tempbuf[BLKSIZE], tempbuf2[BLKSIZE];
			memset(tempbuf,0, BLKSIZE);// Flush
			memset(tempbuf2,0,BLKSIZE);// Flush

			if (op->inodeptr->dINODE.i_block[13] == 0){ //if double indirect doesn't exist, allocate.
				op->inodeptr->dINODE.i_block[13] = balloc(op->inodeptr->dev);
				op->inodeptr->dirty = 1;
				//TRACE ("i_block: %d\n", op->inodeptr->dINODE.i_block[13]);
			}
			
			//Grab the double-indirect block
			get_block(op->inodeptr->dev, op->inodeptr->dINODE.i_block[13], tempbuf);
			ipOne = tempbuf + (((lblk - 268) / 256) * 4); //point ipOne to double indrect block
			//TRACE ("Pre ipOne: %d\n", *ipOne);
			//This loop will allocate all of the blocks up to the block that contains the start point, if they need to be
			if (!*ipOne) //if *ipOne is null, allocate it.
			{
				*ipOne = balloc(op->inodeptr->dev);
				op->inodeptr->dirty = 1;
				put_block(op->inodeptr->dev, op->inodeptr->dINODE.i_block[13], tempbuf);
			}
			//TRACE ("Post ipOne: %d\n", *ipOne);
			//Grab the doubly-indirect block
			get_block(op->inodeptr->dev, *ipOne, tempbuf2);
			ipTwo = tempbuf2 + (((lblk - 268) % 256) * 4);
			//TRACE ("Pre ipTwo: %d\n", *ipTwo);

			if (!*ipTwo) //If the data block is not allocated, then allocate one and blank out the data
			{
				*ipTwo = balloc(op->inodeptr->dev);
				op->inodeptr->dirty = 1;
				put_block(op->inodeptr->dev, *ipOne, tempbuf2);
			}
			blk = *ipTwo;
			//TRACE ("Post ipTwo: %d %d\n", *ipTwo, blk);		
			//Don't forget to write back the doubly-indirect block
			//put_block(op->inodeptr->dev, *ipOne, tempbuf2);
			//put_block(op->inodeptr->dev, op->inodeptr->dINODE.i_block[13], tempbuf);
		}

        get_block(op->inodeptr->dev, blk, wbuf);
        cp = wbuf + startByte;
        int remain = BLKSIZE - startByte; //never read over the ammount of bytes inside the buf
		
		//TRACE ("Remaining: %d\t nbytes: %d\n", remain, nbytes);

        //while (remain > 0)
        //{
            if (remain < nbytes) 
            {
                memcpy(cp, buf, remain); //grab remain bytes from buf to wbuf.
                
                op->offset += remain; //add to nbytes
                if (op->offset > op->inodeptr->dINODE.i_size) //if the the file increased in size, update size.
                    op->inodeptr->dINODE.i_size += remain; 

                nbytes -= remain; //decrement by remain
            }
            else //Have less than a block left to fill, likely at the end
            {
                memcpy(cp, buf, nbytes); //grab nbytes from buf to wbuf.

                op->offset += nbytes; //add nbytes to offset
                if (op->offset > op->inodeptr->dINODE.i_size) //if the the file increased in size, update size.
                    op->inodeptr->dINODE.i_size += nbytes;

                nbytes -= nbytes; //decrement by nbytes
            }
		put_block (op->inodeptr->dev, blk, wbuf);
        //}
        // grab new block, or return.
    }

    op->inodeptr->dirty = 1;
    //printf("wrote %d char into file descriptor fd=%d\n", tempnbyte - nbytes, fd);

    return tempnbyte - nbytes; //return how many bytes written.
}

int iwrite(int lfd, char buf[], int nbytes)
{
	TRACE ("IN iwrite\n");
    OFT *op;

    op = running->fd[lfd];

    if (!(op->mode == 1 || op->mode == 2 || op->mode == 3)) 
	{ 
		fprintf (stderr, "Error: File opened for wrong type %d\n", op->mode);
		return -1; 
	}//if it's not write, readwrite, or append, return -1
	
    if (strlen(buf) < 1) 
	{ 
		fprintf (stderr, "Error: Nothing to write\n");
		return -1; 
	} //make sure buf has things to write.

    return mywrite(lfd, buf, nbytes);
}

void write_file() //if calling from main, pathname will contain file descriptor, parameter will contain buffer to be written. nbytes assumed length of buffer
{
	TRACE ("IN write_file\n");

    //char *buf;
    int lfd;
    int nbytes = strlen(parameter);

    sscanf(pathname, "%d", &lfd); //grab the file descriptor and put into lfd, local file descriptor.

    //buf = (char*)malloc(nbytes); //because it's char* array size of char is 1.
	//strcpy(buf, parameter);
    iwrite(lfd, parameter, nbytes); //call write.
	
	//free (buf); 
}

void cat_file(){
	TRACE ("IN cat_file\n");
	/*
		Open the file for read
		read a chunk from it
		print it to the screen
	*/
	char buf[BLKSIZE];
    int nbytes;
    int lfd;
    int bytesread = 0;

    lfd = my_open(pathname, 0); //open file for read.
    if (lfd < 0) { //check if valid
        fprintf(stderr, "Failed to open file at pathname\n");
        return;
    }

    while (nbytes = myread(lfd, buf, BLKSIZE)){ //read file and print to main.
		bytesread += nbytes;
        fprintf(stdout, "%s",buf);
    }

    fprintf(stdout, "\n%d Bytes read.\n", bytesread);
    memset(pathname, 0, 256);

    pathname[0] = d[lfd];

    close_file(); //close file with Local File Descriptor in Pathname
}

void cp_file(){
	TRACE ("IN cp_file\n");

	int sfd, dfd;
	char dest[256];
	char source[256];	

	strcpy (source, pathname);
	strcpy (dest, parameter);

	if (strlen(source) == 0)
	{
		fprintf (stderr, "Error: No files specified\n");
		return;
	}

	if (strlen(dest) == 0)
	{
		fprintf (stderr, "Error: No destination file specified\n");
		return;
	}
	
	int sino = getino (&dev, source);
	int dino = getino (&dev, dest);

	if (sino == dino)
	{
		fprintf (stderr, "Error: Trying to copy a file to itself (Use link?)\n");
		return;
	}

	sfd = my_open(source, 0);
	strcpy (pathname, dest);
	creat_file();
	dfd = my_open(dest, 1);

	char buf[BLKSIZE];
	int nbytes;

	while (nbytes = myread (sfd, buf, BLKSIZE))
	{
		TRACE ("%s", buf);
		pfd();
		mywrite (dfd, buf, nbytes);
		memset(buf, 0, BLKSIZE);
		getchar();
	}

	memset (pathname, 0, 256);
	
	pathname[0] = d[sfd];
	close_file();
	pathname[0] = d[dfd];
	close_file();
}

void mv_file(){
	TRACE ("IN mv_file\n");
	/*
		creat link in dest
		remove entry in source	
	*/
	char save[256];

	strcpy (save, pathname);
	link();
	
	strcpy (pathname, save);
	unlink();
}

void mount(){
	TRACE ("IN mount\n");
	fprintf (stderr, "Error: This function is not implemented\n");
}

void unmount(){
	TRACE ("IN unmount\n");
	fprintf (stderr, "Error: This function is not implemented\n");
}

void cs(){
	TRACE ("IN cs\n");
	fprintf (stderr, "Error: This function is not implemented\n");
}

void do_fork(){
	TRACE ("IN do_fork\n");
	fprintf (stderr, "Error: This function is not implemented\n");
}

void do_ps(){
	TRACE ("IN do_ps\n");
	fprintf (stderr, "Error: This function is not implemented\n");
}

void sync(){
	TRACE ("IN sync\n");
	fprintf (stderr, "Error: This function is not implemented\n");
}

void quit(){
	TRACE ("QUITTING\n");

	int i; 

	for (i = 0; i < NMINODES; i++)
	{
		if (minode[i].refCount == 0 && minode[i].ino == 0)
			continue;
		minode[i].refCount = 1;
		iput(&(minode[i]));	
	}

	close (dev);

	exit(0);
}

void defa(){
    fprintf(stderr, "Error: Invalid command. Please enter 'menu' for help.\n");
}

init ()
{
	TRACE ("IN INIT\n");
	proc[0].uid = 0;
	proc[0].pid = 0;
	proc[0].gid = 0;
	proc[0].cwd = 0;

	proc[1].uid = 1;
	proc[1].pid = 0;
	proc[1].gid = 0;
	proc[1].cwd = 0;

	running = &(proc[1]);

	int i; 

	for (i = 0; i < NMINODES; i++)
	{
		minode[i].refCount = 0;
		minode[i].ino = 0;
	}

	for (i = 0; i < NOFT; i++)
	{
		oftable[i].mode = -1;
		oftable[i].refCount = 0;
		oftable[i].inodeptr = 0;
		oftable[i].offset = 0;
	}

	root = 0;

    p[0] = menu;
    p[1] = make_dir;
    p[2] = change_dir;
    p[3] = pwd;
    p[4] = list_dir;
    p[5] = rmdir;
    p[6] = creat_file;
    p[7] = link;
    p[8] = unlink;
    p[9] = symlink;
    p[10] = rm_file;
    p[11] = chmod_file;
    p[12] = chown_file;
    p[13] = stat_file;
    p[14] = touch_file;
    p[15] = open_file;
    p[16] = close_file;
    p[17] = pfd;
    p[18] = lseek_file;
    p[19] = access_file;
    p[20] = read_file;
    p[21] = write_file;
    p[22] = cat_file;
    p[23] = cp_file;
    p[24] = mv_file;
    p[25] = mount;
    p[26] = unmount;
    p[27] = cs;
    p[28] = do_fork;
    p[29] = do_ps;
    p[30] = sync;
    p[31] = quit;
    p[32] = defa;
    p[33] = 0;
}

int main (int argc, char *argv[], char *env[])
{
	if (argc < 2)
	{
		fprintf (stderr, "Enter the name of a file system to use\n");
		exit(1);
	}

    init();
    mount_root(argv[1]);
    menu();
	
    while (1)
    {
        //printf("P%d running: ", running->pid);
        printf("input command : ");
        fgets(line, 128, stdin);
        line[strlen(line)-1] = 0;  // kill the \r char at end
        if (line[0]==0) continue;

        sscanf(line, "%s %s %64c", cname, pathname, parameter);

        int a = findCmd(cname); // map cname to an index

        (*p[a]) (); //call the function from function pinter table 

		memset(line, 0, 128);
		memset(cname, 0, 64);		
		memset(pathname, 0, 256);
		memset(parameter, 0, 256);
    }

    return 0;
}
