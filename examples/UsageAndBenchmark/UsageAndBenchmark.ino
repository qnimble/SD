#include "SdFat.h"
#include "sdios.h"

#define RBUFFSIZE 4096 // Read and Write in 4k chunks at a time
#define BUFFSTOREAD 2048   // 8MByte file size for random read
#define SEQBUFFSTOREAD 8192   // 32MByte file size for sequential read

SdFs sd; 
FsFile file;
FsFile root;

IntervalTimer checkSerialReady;

void setup() {
  checkSerialReady.begin(checkSerial,100); //check every 100us for serial being ready
}

void checkSerial() {
  if (Serial) {
    Serial.println("Insert SD card and type any character to start");
    checkSerialReady.end(); //Serial ready, so stop checking
  }
}

void error(const char* message) {
  sd.errorPrint(&Serial);
  Serial.println(message);
  Serial.println("There was an error reading the SD card. Please check that SD card is properly formatted and inserted. Press enter to try again");
}

void loop() {
  static unsigned long lastrun = 0;    
  char fname[] = "speedtest.dat";

  if (millis() > lastrun) { 
    lastrun = millis() + 500;
    toggleLEDGreen();    
  }

  //If key pressed, start the test
  if (Serial.available()) {
    if ( Serial.read() != '\n') {
      //do nothing if not enter
      return;
    };

    while (Serial.available()) {
      Serial.read(); // clear input characters after enter, so we do not queue up runs on top of each other
    }

    if (!sd.begin(SdioConfig(FIFO_SDIO))) {
      error("Error initializing SD card");
      return;
    }
  
    ReadWriteSDCard(); //Do List files, Read/Write files, create directories, etc
  
    WriteBenchFile(fname); //sequential write benchmark
    SeqReadBenchFile(fname); //sequential read benchmakr
    RandReadBenchFile(fname); //random read benchmakr
    
    if (!sd.remove(fname)) { //delete benchmark file when done
      error("Error deleting test benchmark file");
      return;
    }
    Serial.println("SD Tests Complete. Press enter to run them again")  ;
  }
}


void ReadWriteSDCard(void) {
  if (sd.exists("Folder1")
    || sd.exists("Folder1/file1.txt")
    || sd.exists("Folder1/File2.txt")) {
    error("Please remove existing Folder1, file1.txt, and File2.txt");
  }

  int rootFileCount = 0;
  if (!root.open("/")) {
    error("open root");
  }
  while (file.openNext(&root, O_RDONLY)) {
    if (!file.isHidden()) {
      rootFileCount++;
    }
    file.close();
  
  }
  Serial.println("*****************");
  if (rootFileCount > 15) {
      Serial.printf("...  %d files in root, not listing them\n", rootFileCount);
    } else {
      Serial.printf("%d files found. Listing:\n", rootFileCount);
      sd.ls(LS_R);  //list recursive 
    }
  
  // Create a new folder.
  if (!sd.mkdir("Folder1")) {
    error("Create Folder1 failed");
    return;
  }
  Serial.println("Created Folder1");

  // Create a file in Folder1 using a path.
  if (!file.open("Folder1/file1.txt", O_WRONLY | O_CREAT)) {
    error("create Folder1/file1.txt failed");
  }
  file.close();
  Serial.println("Created Folder1/file1.txt");

  // Change volume working directory to Folder1.
  if (!sd.chdir("Folder1")) {
    error("chdir failed for Folder1.\n");
  }
  Serial.println("chdir to Folder1");

  // Create File2.txt in current directory.
  if (!file.open("File2.txt", O_WRONLY | O_CREAT)) {
    error("create File2.txt failed");
  }
  file.close();
  Serial.println("Created File2.txt in current directory");

  Serial.println("\nList of files now on the SD.");
  sd.ls("/", LS_R); //list recursive 

  // Remove files from current directory.
  if (!sd.remove("file1.txt") || !sd.remove("File2.txt")) {
    error("remove failed");
  }
  Serial.println("\nfile1.txt and File2.txt removed.");

  // Change current directory to root.
  if (!sd.chdir()) {
    error("chdir to root failed.\n");
  }

  // Remove Folder1.
  if (!sd.rmdir("Folder1")) {
    error("rmdir for Folder1 failed\n");
  }
  Serial.println("\nFolder1 removed.");
}

void RandReadBenchFile(const char *filename) {
  //Benchmark reading from random location in benchmark file
  uint16_t idx;
  FsFile benchFile;
  uint8_t rbuffer[RBUFFSIZE];
  uint32_t startmicro, dmicro, maxmicro, minmicro, fpos;
  float microsum;

  if (!benchFile.open(filename, O_READ)) {
    Serial.printf("\nCould not open <%s> for reading.", filename);
    return;
  }
  uint32_t fileSize = benchFile.size();
  uint32_t maxChunks = fileSize / RBUFFSIZE;
  Serial.printf("fileSize = %d and maxChunks = %d\n",fileSize,maxChunks);
  startmicro = micros();  // save starting time
  Serial.printf("\nReading %lu random buffers of size %u bytes\n",BUFFSTOREAD, RBUFFSIZE);
  maxmicro = 0;
  minmicro = 4000000;//set to large value
  microsum = 0.0;

  for(idx = 0; idx<BUFFSTOREAD; idx++){
    startmicro = micros();
    //fpos = random(0,5000)*RBUFFSIZE;
    fpos = random(0,maxChunks)*RBUFFSIZE;
    benchFile.seek(fpos);
    benchFile.read(&rbuffer, RBUFFSIZE);
    dmicro = micros()-startmicro;
    if(dmicro > maxmicro) maxmicro = dmicro;
    if(dmicro < minmicro) minmicro = dmicro;    
    microsum += dmicro;
  }
  benchFile.close();
  Serial.printf("Random Seek Buffer read times: average = %4.1f usec   Maximum = %lu usec    Min = %lu usec\n",
                              microsum/BUFFSTOREAD, maxmicro, minmicro);
  Serial.printf("Random Seek average Read Rate = %6.3f MBytes/second\n", (float)(BUFFSTOREAD *RBUFFSIZE)/(float)microsum);
}

void WriteBenchFile(char *filename) {  
  //Benchmark writing a large file
  uint32_t i;
  FsFile benchFile;
  float seconds;
  uint32_t startmilli, dmilli, blockmax, mbytes;
  unsigned char benchbuff[RBUFFSIZE];

  Serial.printf("\n\nWriting 32MB of random data to file.\n");
  // Open the file
  if (!benchFile.open(filename,  O_RDWR | O_CREAT | O_TRUNC)) {
    Serial.printf("Unable to open <%s> for writing.", filename);
    return;
  }

  // now write the data in blocks of RBUFFSIZE --   8192 blocks of size 4k = 32MB
  // send out a message every 10MB or 256 blocks
  blockmax = 8192;
  mbytes = 0;
  startmilli = millis();
  for(i=0; i<blockmax; i++){
    benchFile.write(&benchbuff, RBUFFSIZE);
    if ( (i%(1280) == 0) && (i != 0)){    
      Serial.printf("%lu MB... ",mbytes);
      mbytes+= 5; //1280*4kblocks = 5 MB
    }

  }
  Serial.println("done.");
  dmilli = millis()-startmilli;

  benchFile.close();
  seconds = (float)dmilli/1000.0;
  Serial.printf("\nRandom Write Benchmark: 32MBytes in %4.2f seconds", (float)dmilli/1000.0);
  Serial.printf(" (%3.2f Mbytes/second)\n", (32)/seconds);
}

void SeqReadBenchFile(const char *filename) {
  //benchmark reading data sequentially
  uint16_t idx;
  FsFile benchFile;
  uint8_t rbuffer[RBUFFSIZE];
  uint32_t startmicro, dmicro, maxmicro, minmicro;
  float microsum;

  if (!benchFile.open(filename, O_READ)) {
    Serial.printf("\nCould not open <%s> for reading.", filename);
    return;
  }
  startmicro = micros();  // save starting time
  Serial.printf("\nReading %lu sequential buffers of %u bytes each\n",SEQBUFFSTOREAD, RBUFFSIZE);
  maxmicro = 0;
  minmicro = 4000000;
  microsum = 0.0;
  
  for(idx = 0; idx<SEQBUFFSTOREAD; idx++){
    startmicro = micros();
    benchFile.read(&rbuffer, RBUFFSIZE);
    dmicro = micros()-startmicro;
    if(dmicro > maxmicro) maxmicro = dmicro;
    if(dmicro < minmicro) minmicro = dmicro;
    microsum += dmicro;

  }

  Serial.printf("Sequential read times: average = %4.1f usec   Maximum = %u usec    Min = %u usec\n",
                              microsum/SEQBUFFSTOREAD, maxmicro, minmicro);
  Serial.printf("Sequential Read Rate = %6.3f MBytes/second\n", (float)(SEQBUFFSTOREAD *RBUFFSIZE)/(float)microsum);
  
  benchFile.close();
}

