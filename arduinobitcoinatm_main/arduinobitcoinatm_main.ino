 #include <SoftwareSerial.h>
 #include <Wire.h>
 #include "RTClib.h"
 #include <SPI.h>
 #include <SD.h>

 File logfile; //logfile


 byte cThisChar; //for streaming from SD card
 byte cLastChar; //for streaming from SD card
 char cHexBuf[3]; //for streaming from SD card
  
 const int FIFTY_PULSE = 10; //pulse per $50 
 const int PULSE_TIMEOUT = 2000; //ms before pulse timeout
 const int MAX_BITCOINS = 10; //max btc per SD card
 const int HEADER_LEN = 25; //maximum size of bitmap header
 
 #define SET_RTCLOCK      1 // Set to true to set Bitcoin transaction log clock to program compile time.
 #define TEST_MODE        0 // Set to true to not delete private keys (prints the same private key for each dollar).
 
 #define DOUBLE_HEIGHT_MASK (1 << 4) //size of pixels
 #define DOUBLE_WIDTH_MASK  (1 << 5) //size of pixels
 
 RTC_DS1307 RTC; // define the Real Time Clock object

 char LOG_FILE[] = "btclog.txt"; //name of Bitcoin transaction log file
    
 const int chipSelect = 10; //SD module
 
 int printer_RX_Pin = 5;  // This is the green wire
 int printer_TX_Pin = 6;  // This is the yellow wire
 
 char printDensity = 14; // 15; //text darkening
 char printBreakTime = 4; //15; //text darkening

 
 // -- Initialize the printer connection

 SoftwareSerial *printer;
 #define PRINTER_WRITE(b) printer->write(b)
 

 long pulseCount = 0;
 unsigned long pulseTime, lastTime;
 volatile long pulsePerDollar = 10;
 
void setup(){
  Serial.begin(57600); //baud rate for serial monitor
  attachInterrupt(0, onPulse, RISING); //interupt for Apex bill acceptor pulse detect
  pinMode(2, INPUT); //for Apex bill acceptor pulse detect 
  pinMode(10, OUTPUT); //Slave Select Pin #10 on Uno
  pinMode(3, OUTPUT); //For Enable & Disable Acceptor


    if (!SD.begin(chipSelect)) {    
      Serial.println("card failed or not present");
      return;// error("Card failed, or not present");     
  }

   
  printer = new SoftwareSerial(printer_RX_Pin, printer_TX_Pin);
  printer->begin(9600);  //baud rate for printer

  //Modify the print speed and heat
  PRINTER_WRITE(27);
  PRINTER_WRITE(55);
  PRINTER_WRITE(7); //Default 64 dots = 8*('7'+1)
  PRINTER_WRITE(255); //Default 80 or 800us
  PRINTER_WRITE(255); //Default 2 or 20us

  //Modify the print density and timeout
  PRINTER_WRITE(18);
  PRINTER_WRITE(35);
  //int printSetting = (printDensity<<4) | printBreakTime;
  int printSetting = (printBreakTime<<5) | printDensity;
  PRINTER_WRITE(printSetting); //Combination of printDensity and printBreakTime


  Serial.println();
  Serial.println("Parameters set");
  
 #if SET_RTCLOCK
 RTC.adjust(DateTime(__DATE__, __TIME__));  // Set the RTC to the date & time for Bitcoin Transaction log
 #endif


}

void loop(){ 

    if(SD.exists("BTC_9.btc")){
      digitalWrite(3, LOW); //
     } else {
     digitalWrite(3, HIGH);//No bitcoins left disable Bill Acceptor
     }
  
    if(pulseCount == 0) {
    return;// error("No Pulses Decteced");
    }
 
    if((millis() - pulseTime) < PULSE_TIMEOUT) 
      return;
 
    if(pulseCount == FIFTY_PULSE)
       Serial.println("$50 Detected");
       getNextBitcoin(); // $50 Detected
       
     //----------------------------------------------------------
     // Add additional currency denomination logic here: $5, $10, $20      
     //----------------------------------------------------------
   
     pulseCount = 0; // reset pulse count
     pulseTime = 0;
   
     
}

/*****************************************************
onPulse
- read 50ms pulses from Apex Bill Acceptor.
- 10 pulses indicates $50 accepted

******************************************************/
void onPulse(){
  
int val = digitalRead(2);
pulseTime = millis();

if(val == HIGH)
  pulseCount++;
  
}

/*****************************************************
- Read next bitcoin QR Code from SD
******************************************************/

int getNextBitcoin(){
    
  int BTCNumber = 0, i = 0;
 // long counter = 0;
 char cBuf, cPrev;
       
    Serial.println("card initialized.");
 
    while(BTCNumber<MAX_BITCOINS){
      
         //prepend file name
         String temp = "BTC_";
         //add file number
         temp.concat(BTCNumber);
         //append extension
         temp.concat(".btc"); 
         
         //char array
         char filename[temp.length()+1];   
         temp.toCharArray(filename, sizeof(filename));
        
         //check if the bitcoin QR code exist on the SD card
         if(SD.exists(filename)){
             Serial.print("file exists: ");
             Serial.println(filename);
             
             //print logo at top of paper
             if(SD.exists("logo.oba")){
               printBitmap("logo.oba"); 
             }  
             
                         
               //print QR code off the SD card
               printBitmap(filename); 

               //printer->println("Download Breadwallet for iPhone");
               //printer->println("or Android and scan the");
               //printer->println("QR Code to use your Bitcoins.");
               //printer->println("Keep this QR code secure until");
               //printer->println("you have scanned it into");
               //printer->println("Breadwallet. For more info visit");
               //printer->println("www.test.com.au");
               
               printer->println(" ");
               printer->println(" ");
               printer->println(" ");
               printer->println(" ");

   #if !TEST_MODE
  //delete the QR code file after it is printed
     SD.remove(filename);
   #endif      

               
         break; //stop looking, bitcoin file found
         }                 
    
          else{
            if (BTCNumber >= MAX_BITCOINS -1){
              
                digitalWrite(3, HIGH); //Stop accepting notes
                Serial.println("Bill Acceptor Disabled"); 
              
            }  
             Serial.print("file does not exist: ");
             Serial.println(filename);
             
             
        }
    //increment bitcoin number
    BTCNumber++;
    }

}  

/*****************************************************
printBitmap(char *filename)
- open QR code bitmap from SD card. Bitmap file consists of 
byte array output by OpenBitcoinQRConvert.pde
width of bitmap should be byte aligned -- evenly divisable by 8
******************************************************/
void printBitmap(char *filename){
  int nBytes = 0;
  int iBitmapWidth = 0 ;
  int iBitmapHeight = 0 ;
  File tempFile = SD.open(filename);

        for(int h = 0; h < HEADER_LEN; h++){
        
          cLastChar = cThisChar;
          if(tempFile.available()) cThisChar = tempFile.read(); 
    
              //read width of bitmap
              if(cLastChar == '0' && cThisChar == 'w'){   
                if(tempFile.available()) cHexBuf[0] = tempFile.read(); 
                if(tempFile.available()) cHexBuf[1] = tempFile.read(); 
                  cHexBuf[2] = '\0';
                  
                  iBitmapWidth = (byte)strtol(cHexBuf, NULL, 16); 
                  Serial.println("bitmap width");
                  Serial.println(iBitmapWidth);           
              }
    
              //read height of bitmap
              if(cLastChar == '0' && cThisChar == 'h'){
               
                 if(tempFile.available()) cHexBuf[0] = tempFile.read(); 
                 if(tempFile.available()) cHexBuf[1] = tempFile.read(); 
                  cHexBuf[2] = '\0';
                  
                  iBitmapHeight = (byte)strtol(cHexBuf, NULL, 16);
                  Serial.println("bitmap height");
                  Serial.println(iBitmapHeight); 
              }
      }
      
  
      PRINTER_WRITE(0x0a); //line feed

      
      Serial.println("Print bitmap image");
      //set Bitmap mode
      PRINTER_WRITE(18); //DC2 -- Bitmap mode
      PRINTER_WRITE(42); //* -- Bitmap mode
      PRINTER_WRITE(iBitmapHeight); //r
      PRINTER_WRITE((iBitmapWidth+7)/8); //n (round up to next byte boundary
  
  
      //print 
      while(nBytes < (iBitmapHeight * ((iBitmapWidth+7)/8))){ 
        if(tempFile.available()){
            cLastChar = cThisChar;
            cThisChar = tempFile.read(); 
        
                if(cLastChar == '0' && cThisChar == 'x'){
      
                    cHexBuf[0] = tempFile.read(); 
                    cHexBuf[1] = tempFile.read(); 
                    cHexBuf[2] = '\0';
                    Serial.println(cHexBuf);
                    
                    PRINTER_WRITE((byte)strtol(cHexBuf, NULL, 16)); 
                    nBytes++;
                }
        }  
          
      }

       
      PRINTER_WRITE(10); //Paper feed
      Serial.println("Print bitmap done");


  tempFile.close();
    Serial.println("file closed");

     
    
     // update transaction log file
   //if (! SD.exists(LOG_FILE)) {
      //updateLog();
      // only open a new file if it doesn't exist
       
   
    //}
    
  return;
  
}

/*****************************************************
updateLog()
Updates Bitcoin transaction log stored on SD Card
Logfile name = LOG_FILE

******************************************************/
void updateLog(){
  
      DateTime now;
      
      now=RTC.now();
      
      logfile = SD.open(LOG_FILE, FILE_WRITE); 
      logfile.print("Bitcoin Transaction ");
      logfile.print(now.unixtime()); // seconds since 1/1/1970
      logfile.print(",");
      logfile.print(now.year(), DEC);
      logfile.print("/");
      logfile.print(now.month(), DEC);
      logfile.print("/");
      logfile.print(now.day(), DEC);
      logfile.print(" ");
      logfile.print(now.hour(), DEC);
      logfile.print(":");
      logfile.print(now.minute(), DEC);
      logfile.print(":");
      logfile.println(now.second(), DEC);
      logfile.close();
}
