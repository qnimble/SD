#include "SdFat.h"

SdFs sd;
char filename[] = "Data.dat";


void setup() {
  while (sd.begin() == false) {
    //Sd card failed, wait and try again
    delay(1000); //wait 1000ms or 1 sec
    Serial.println("Unable to connect to SD Card, was it inserted?");
  }

  Serial.println("Successfully connected to SD card.");
  eraseFile(); //erase file
  readFile(); //try to read when no file exists
  writeFile(); //write to file for first time
  readFile(); //read file
  delay(2000); //wait 2 seconds
  writeFile(); // write to file a second time
  readFile(); //read file
  delay(2000); //wait 2 seconds
  writeFile(); //write to file a third time
  readFile(); //read file
 
}

void readFile(void) {
  char filedata[2000]; //temporary place to store contents of the file
  if (sd.exists(filename)) {
    FsFile file = sd.open(filename,O_RDONLY); //open the file read only
    uint64_t fileSize = file.fileSize(); //get the file size
    Serial.printf("File %s exists with size %u\n",filename,file.fileSize());
    uint64_t openSize = min(fileSize-1,sizeof(filedata)); //prevent overflow if file larger than buffer to store it
    file.read(filedata,openSize); //read the entire file into filedata array
    Serial.println(" ** Data Contents Start **");
    filedata[openSize] = '\0'; //Terminate data so we can print as string
    Serial.print(filedata);
    Serial.println("\n ** Data Contents End **");
    file.close();
  } else {
    Serial.printf("File %s is missing!\n",filename);
  }
}

void writeFile(void) {
  char message[500];
  FsFile file;
  static uint count = 1;
  file = sd.open(filename,O_RDWR |  O_CREAT | O_APPEND);
  if (file)  {
    uint nowTime =  rtc_get();
    uint charsWritten = sprintf(message,"%u Write number %u\n",nowTime,count); //Prepare string to write to file
    count++; //keep track of how many times we have written to the file
    file.write(message,charsWritten); //write to file
    file.close(); // close file now that write is done
  } else {
    Serial.println("Error opening Data.txt file.");
    sd.errorPrint(&Serial);
    return;
  }
}

void eraseFile(void) {
  if (sd.exists(filename)) {
    sd.remove(filename);
  }
}

void loop() {
  static unsigned long lastrun = 0;    
  if (millis() > lastrun) { //Run once every 1000ms
    lastrun = millis() + 1000;
    toggleLEDBlue();
  }
}

