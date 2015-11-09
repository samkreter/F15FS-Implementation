#include "../include/FonefiveFS_basic.h"

#define BLOCK_SIZE 1024



int fs_format(const char *const fname){
	//param check
	if(fname && fname != NULL && strcmp(fname,"") != 0){
		//create the blockstore
		block_store_t* bs = block_store_create();
		if(bs){
			//use util funt o allocate all the spaces needed in it.
			//really just to make sure we set the front blocks to used
			//in the bitmap for the inode table so they can't be allocated as data blcoks
			if(allocateInodeTableInBS(bs) >= 0){

                //write the block store to the file
                block_store_link(bs, fname);

                return 0;
            }

		}
	}
	return -1;
}


block_ptr_t setUpDirBlock(block_store_t* bs){
	//param check
	if(bs){
		//declare the new dir
	    dir_block_t newDir;

	    //set the size of its contents to zero
	    newDir.metaData.size = 0;

	    //declare it pos
	    block_ptr_t dirBlockPos;

	    //get the next free block in the blockstore
	    if((dirBlockPos = block_store_allocate(bs))){
	    	//write the data of the dir to the block and get teh pos to put in the inode table
	    	if(block_store_write(bs, dirBlockPos, &newDir, BLOCK_SIZE, 0) == BLOCK_SIZE){
	    		return dirBlockPos;
	    	}

	    }
	}
    return -1;
}

int allocateInodeTableInBS(block_store_t* bs){
	//param check,
    if(bs){

		//declare i because c just isn't as cool as c++ with declaring in the loop
		size_t i = 0;
        //allocate the room for the inode table
		for(i=8; i<41; i++){
			if(!block_store_request(bs,i)){
				return -1;
			}
		}

		//create a root inode
		iNode_t rootNode;

        //set root filename as '/'
		rootNode.fname[0] = '/';

        //set the file type to a directory
        rootNode.metaData.filetype = DIRECTORY;
        rootNode.metadata.inUse = 1;

		//allocate the root directory node and check it worked
		if((rootNode.data_ptrs[0] = setUpDirBlock(bs)) < 0){
            return -1;
        }

        //write root inode to the blockstore
        if(block_store_write(bs, 8, &rootNode, 48, 0) == 48){
			return 1;
		}
		return -11;

	}
	return -1;
}


int flushiNodeTableToBS(F15FS_t* fs){
	//I'm really lovin all these param checks
	if(fs && fs->bs != NULL){
		//I think size_t is some cool stuff, you know causes it looks special
		size_t i = 8;
		//another kilobyte of fun
		char buffer[BLOCK_SIZE];
		//index mapping var to go from i to the correct index of the inodetable
		int startingPos = 0;
		for(i = 8; i < 41; i++){
			//increment the mapping var
			startingPos = (i-8)*8;
			//take the memory to a buffer before we write to the table in casue of
			// some funny bussiness in the blockstore
			if(memcpy(&buffer,(fs->inodeTable+startingPos),BLOCK_SIZE) != NULL){
				//write our stuff to the block store
				if(block_store_write(fs->bs,i,&buffer,BLOCK_SIZE,0) != BLOCK_SIZE){
					return 0;
				}
			}
			else{
				return 0;
			}
		}
		return 1;
	}
	return 0;

}

int getiNodeTableFromBS(F15FS_t* fs){
	//usual param checks
	if(fs){
		//starting at the 8th block in the blockstore
		size_t i = 8;
		//set up the buffer to be 1 kilobyte
		uint8_t buffer[BLOCK_SIZE];
		//create a maping var to get the index of the inodetable
		int startingPos = 0;
		for(i = 8; i < 40; i++){
			//increment the index mapper
			startingPos = (i-8)*8;
			//reak a block from the bs
			if(block_store_read(fs->bs,i,&buffer,BLOCK_SIZE,0) == BLOCK_SIZE){
				//put the contents into the fs inode table
				if(memcpy((fs->inodeTable+startingPos),&buffer,BLOCK_SIZE) == NULL){
					return 0;
				}
			}
			else{
				return 0;
			}
		}
		return 1;
	}
	return 0;
}


F15FS_t *fs_mount(const char *const fname){
	//check params, having to check for weird things
	if(fname && strcmp(fname,"") != 0){
		//create the filesytem struct
		F15FS_t* fs = (F15FS_t*)malloc(sizeof(F15FS_t));
		if(fs){
			//import the blockstore from the file
			if((fs->bs = block_store_import(fname)) != NULL){
				//pull the inode table into memory
				if(getiNodeTableFromBS(fs)){
					return fs;
				}
			}
		}
	}
	return NULL;
}



int fs_unmount(F15FS_t *fs){
	//check params
	if(fs && fs->inodeTable && fs->bs){
		//flush the inode table into the blockstore
		if(flushiNodeTableToBS(fs)){
			//flush the blockstore to the file
			block_store_flush(fs->bs);
			if(block_store_errno() == BS_OK){
				//free the blockstore
				block_store_destroy(fs->bs,BS_NO_FLUSH);
				//free the file system pointer
				free(fs);
				return 0;
			}

		}
	}
	return -1;
}



int findEmptyInode(F15FS *const fs){
    if(fs){
        int i = 0;
        for(i = 0; i < 256; i++){
            if(fs->inodeTable[i].metadata.inUse != 1){
                return i;
            }
        }
    }
    return -1;
}

//0 for not there -1 for actual error
int searchDir(F15FS_t *const fs, char* fname, block_ptr_t blockNum, inode_ptr_t* inodeIndex){
    dir_block_t dir
    if(block_store_read(fs->bs,i,&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
        int i = 0;
        for(i = 0; i < dir.metadata.size; i++){
            if(strcmp(dir.entries[i].filename,fname) == 0){
                *inodeIndex = dir.entries[i].inode;
                return 1;
            }
        }
        return 0;
    }
    return -1;
}

int freeFilePath(char** pathList){
    int listSize = *pathList[0] - '0';
    int i = 0;
    for(i = 0; i < listSize + 1; i++){
        free(pathList[i]);
    }
    free(pathList);
}

inode_ptr_t getInodeFromPath(F15FS_t *const fs, char* fname){
    if(fs && fname && strcmp(fname,"") != 0){
        char** pathList = parseFilePath(fname);
        int listSize = *pathList[0] - '0';
        int i = 1;

        fs.inodetable[0].data_ptrs[0]

        freeFilePath(pathList);
    }
}

char** parseFilePath(char* filePath){
    if(filePath && strcmp(filePath,"") != 0){
        char* temp = filePath;
        char** pathList;
        int count = 0;
        int i = 1;
        const char delim[2] = "/";
        char* token;

        while(*temp){
            if(*temp == '/'){
                count++;
            }
            temp++;
        }

        if(count == 0){
            //theres only a file or directory name so just add the count
            //and whats in there to one
            if((pathList = (char**)malloc(sizeof(char*)*2)) != NULL){
                if((pathList[0] = (char*)malloc(sizeof(char))) != NULL){
                    *pathList[0] = 1;
                    pathList[1] = &filePath;
                    return pathList;
                }
            }
            return NULL;
        }else{
            //create string array with right size plus one to add the size in
            pathList = (char**)malloc(sizeof(char*)*(count+1));

            if(pathList == NULL){
                return NULL;
            }

            pathList[0] = malloc(sizeof(char));
            if(pathList[0] == NULL){
                return NULL;
            }

            //put the length at the begging
            *pathList[0] = count;

            token = strtok(filePath,delim);

            while(token != NULL){

                pathList[i] = (char*)malloc(strlen(token));
                if(pathList[i] == NULL){
                    return NULL;
                }
                strcpy(pathList[i],token);

                token = strtok(NULL,delim);
                i++;
            }

            return pathList

        }
    }
    return NULL;
}

///
/// Creates a new file in the given F15FS object
/// \param fs the F15FS file
/// \param fname the file to create
/// \param ftype the type of file to create
/// \return 0 on success, < 0 on error
///
int fs_create_file(F15FS_t *const fs, const char *const fname, const ftype_t ftype){
    //param check
    if(fs && fname && strcmp(fname,"") != 0 && ftype){
        int emptyiNodeIndex = findEmptyInode(fs);

        //set the use byte
        fs->inodeTable[emptyiNodeIndex].metadata.inUse = 1;

        //add the fname to the inode
        strcpy(fs->inodeTable[emptyiNodeIndex].fname,fname)

        if(ftype == DIRECTORY){
            if((fs->inodeTable[emptyiNodeIndex].data_ptrs[0] = setUpDirBlock(fs->bs)) > 0){
                return 1;
            }
        }
    }
    return -1;
}

///
/// Returns the contents of a directory
/// \param fs the F15FS file
/// \param fname the file to query
/// \param records the record object to fill
/// \return 0 on success, < 0 on error
///
int fs_get_dir(const F15FS_t *const fs, const char *const fname, dir_rec_t *const records){

}

///
/// Writes nbytes from the given buffer to the specified file and offset
/// Increments the read/write position of the descriptor by the ammount written
/// \param fs the F15FS file
/// \param fname the name of the file
/// \param data the buffer to read from
/// \param nbyte the number of bytes to write
/// \param offset the offset in the file to begin writing to
/// \return ammount written, < 0 on error
///
ssize_t fs_write_file(F15FS_t *const fs, const char *const fname, const void *data, size_t nbyte, size_t offset);

///
/// Reads nbytes from the specified file and offset to the given data pointer
/// Increments the read/write position of the descriptor by the ammount read
/// \param fs the F15FS file
/// \param fname the name of the file to read from
/// \param data the buffer to write to
/// \param nbyte the number of bytes to read
/// \param offset the offset in the file to begin reading from
/// \return ammount read, < 0 on error
///
ssize_t fs_read_file(F15FS_t *const fs, const char *const fname, void *data, size_t nbyte, size_t offset);

///
/// Removes a file. (Note: Directories cannot be deleted unless empty)
/// This closes any open descriptors to this file
/// \param fs the F15FS file
/// \param fname the file to remove
/// \return 0 on sucess, < 0 on error
///