#include "cacheSim.h"

// In this question, we will assume DRAM will take a 4-byte values starting from 0 to 
void init_DRAM()
{
	unsigned int i=0;
	DRAM = malloc(sizeof(char) * DRAM_SIZE);
	for(i=0;i<DRAM_SIZE/4;i++)
	{
		*((unsigned int*)DRAM+i) = i;
	}
}

void printCache()
{
	int i,j,k;
	printf("===== L1 Cache Content =====\n");
	for(i=0;i<2;i++)
	{
		printf("Set %d :", i);
		for(j=0;j<2;j++)
		{
			printf(" {(TAG: 0x%x)", (unsigned int)(L1_cache[i][j].tag));
			for(k=0;k<16;k++)
				printf(" 0x%x,", (unsigned int)(L1_cache[i][j].data[k]));
			printf(" |");
		}
		printf("\n");
	}
	printf("===== L2 Cache Content =====\n");
	for(i=0;i<4;i++)
	{
		printf("Set %d :", i);
		for(j=0;j<4;j++)
		{
			printf(" {(TAG: 0x%x)", (unsigned int)(L2_cache[i][j].tag));
			for(k=0;k<16;k++)
				printf(" 0x%x,", (unsigned int)(L2_cache[i][j].data[k]));
			printf(" |");
		}
		printf("\n");
	}
}
// ------------P2-------------
// Note: we will assume that every request is for a 4-byte data and address can start at any byte; use FIFO lowest timeStamp
static u_int32_t fifoTime = 1;

// helper functions
static int l1_hit(unsigned int l1Set, unsigned int l1Tag) {
	for (int way = 0; way < 2; way++) {
        if (L1_cache[l1Set][way].timeStamp != 0 && L1_cache[l1Set][way].tag == l1Tag) {
            return way;
        }
    }
    return -1;
}
static int l2_hit(unsigned int l2Set, unsigned int l2Tag) {
    for (int way = 0; way < 4; way++) {
        if (L2_cache[l2Set][way].timeStamp != 0 && L2_cache[l2Set][way].tag == l2Tag) {
            return way;
        }
    }
    return -1;
}
	// found empty space or if not found use FIFO
static int l1_fifo(unsigned int l1Set, unsigned int l1Tag) {
    int kickout = -1;
    // find empty space
    for (int way = 0; way < 2; way++) {
        if (L1_cache[l1Set][way].timeStamp == 0) {
            kickout = way;
            break;
        }
    }
    // fifo: find 1st in
    if (kickout == -1) {
		kickout=0;
        if (L1_cache[l1Set][1].timeStamp < L1_cache[l1Set][0].timeStamp) {
                    kickout = 1;
                }
    }
    // update l1
    L1_cache[l1Set][kickout].tag = l1Tag;
    L1_cache[l1Set][kickout].timeStamp = fifoTime++;
    return kickout;
}
// l2 miss as well -> load data from DRAM
static int l2_miss(unsigned int l2Set, unsigned int l2Tag, u_int32_t currentAddress) {
    int kickout = -1;
    u_int32_t int blockStart = currentAddress & ~0xF;
    
    // find empty space
    for (int way = 0; way < 4; way++) {
        if (L2_cache[l2Set][way].timeStamp == 0) {
            kickout = way;
            break;
        }
    }
    // set is full
    if (kickout == -1) {
        kickout = 0;
		// use fifo
        for (int way = 1; way < 4; way++) {
            if (L2_cache[l2Set][way].timeStamp < L2_cache[l2Set][kickout].timeStamp) {
                kickout = way; 
            }
        }
		// if remove a block in l2, this block in l1 is removed as well
    	u_int32_t oldBlockAddress =(L2_cache[l2Set][kickout].tag << 6) |(l2Set << 4);
    	unsigned int oldL1Set = getL1SetID(oldBlockAddress);
    	unsigned int oldL1Tag = getL1Tag(oldBlockAddress);

    	for (int way = 0; way < 2; way++){
        	if (L1_cache[oldL1Set][way].timeStamp != 0 &&
            	L1_cache[oldL1Set][way].tag == oldL1Tag){
            	L1_cache[oldL1Set][way].timeStamp = 0;}
    	}
	}

    // Load the block from DRAM into L2.
    L2_cache[l2Set][kickout].tag = l2Tag;
    L2_cache[l2Set][kickout].timeStamp = fifoTime++;

    for (int i = 0; i < 16; i++)
    {
        L2_cache[l2Set][kickout].data[i] =
            DRAM[blockStart + i];
    }

    return kickout;
}

u_int32_t read_fifo(u_int32_t address)
{
    u_int32_t result = 0;
    for (int byteNumber = 0; byteNumber < 4; byteNumber++) {
        u_int32_t currentAddress = address + byteNumber;

        unsigned int l1Set = getL1SetID(currentAddress);
        unsigned int l1Tag = getL1Tag(currentAddress);
        unsigned int l2Set = getL2SetID(currentAddress);
        unsigned int l2Tag = getL2Tag(currentAddress);
        unsigned int offset = currentAddress & 0xF;

        // find the block in l1
        int l1Way =l1_hit(l1Set, l1Tag);
		// l1 miss
        if (l1Way == -1) {
            // find in l2
            int l2Way =l2_hit(l2Set, l2Tag);
            
            //l2 miss
            if (l2Way == -1) {
                l2Way =l2_miss(l2Set, l2Tag, currentAddress);
            }
            
            // move the block from l2 intol1
            l1Way = l1_fifo(l1Set, l1Tag);
            for (int i = 0; i < 16; i++) {
                L1_cache[l1Set][l1Way].data[i] = L2_cache[l2Set][l2Way].data[i];
            }
        }
        result |= ((u_int32_t)L1_cache[l1Set][l1Way].data[offset]) << (8 * byteNumber);
    }

    return result;
}


//------------P1------------

// in the cache -> valid =1 and correct tag
int L1lookup(u_int32_t address) // 2 ways
{
unsigned tag=getL1Tag(address);
unsigned setID=getL1SetID(address);
for(int way=0; way<2; way++ ){
	if(L1_cache[setID][way].timeStamp!=0 && L1_cache[setID][way].tag==tag){
		return 1;
	}
}
return 0;
}

int L2lookup(u_int32_t address) //4 ways
{
unsigned tag=getL2Tag(address);
unsigned setID=getL2SetID(address);
for(int way=0; way<4; way++ ){
	if(L2_cache[setID][way].timeStamp!=0 && L2_cache[setID][way].tag==tag){
		return 1;
	}
}
return 0;
}

unsigned int getL1SetID(u_int32_t address)
{
	u_int32_t movedright;
	unsigned compare;
	movedright=address>>4; //move 4 most right bits to replace offset bits; make set digit to be the most right
	// #set digit =log2(2sets)=1 digit
	compare=movedright & 0x1; // 0x1=0001; comparing the most right digit to see whether it's set 0 or 1; 1&1=1 1&0 =0
	return compare;
}

unsigned int getL2SetID(u_int32_t address)
{
u_int32_t movedright;
	unsigned compare;
	movedright=address>>4;
	// #set digit =log2(4sets)=2 digits 
	compare=movedright & 0x3; // 0x3 =0011
	return compare;
}

unsigned int getL1Tag(u_int32_t address)
{
return address>>5; // L1 tag bit starts at 5th digit in the address (offset 0-3, set 4, tag 5-31)
}

unsigned int getL2Tag(u_int32_t address)
{
return address >>6; // L2 tag bit start at 6th digit (offset 0-3, set 4-5, tag 6-31)
}

// --------P3---------
void write(u_int32_t address, u_int32_t data)
{
///// REPLACE THIS /////
	read_fifo(address);
return;
}


int main()
{
	init_DRAM();
	cacheAccess buffer;
	int timeTaken=0;
	FILE *trace = fopen("input.trace","r");
	int L1hit = 0;
	int L2hit = 0;
	cycles = 0;
	while(!feof(trace))
	{
		fscanf(trace,"%d %x %x", &buffer.readWrite, &buffer.address, &buffer.data);
		printf("Processing the request for [R/W] = %d, Address = %x, data = %x\n", buffer.readWrite, buffer.address, buffer.data);

		// Checking whether the current access is a hit or miss so that we can advance time correctly
		if(L1lookup(buffer.address))// Cache hit
		{
			timeTaken = 1;
			L1hit++;
		}
		else if(L2lookup(buffer.address))// L2 Cache Hit
		{
			L2hit++;
			timeTaken = 5;
		}
		else timeTaken = 50;
		if (buffer.readWrite) write(buffer.address, buffer.data);
		else read_fifo(buffer.address);
		cycles+=timeTaken;
	}
	printCache();
	printf("Total cycles used = %ld\nL1 hits = %d, L2 hits = %d", cycles, L1hit, L2hit);
	fclose(trace);
	free(DRAM);
	return 0;
}




