#include <ICMPPing.h>

#include <SD.h>

#include <Time.h>
#include <TimeLib.h>

#include <Dhcp.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#define SD_CS 4
#define SD_ETHER 10
 
byte mac[] = { 0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01 };  

// Manually added prototype because parameter is a reference
time_t ntpUnixTime();

char fileName[12];
unsigned long outage;

IPAddress pingAddr(8,8,8,8); // ip address to ping for internet connectivity
SOCKET pingSocket = 0;
ICMPPing ping(pingSocket, (uint16_t)random(0, 255));

// A UDP instance to let us send and receive packets over UDP
EthernetUDP udp;

File dataFile;

EthernetClient client;

unsigned long webUnixTime (Client &client);

void setup() {
  Serial.begin(115200);

  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD card failed, or not present"));
    // don't do anything more:
    for(;;)
      ;
  }
  Serial.println(F("SD card initialized."));  

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));
    // try to congifure using IP address instead of DHCP:
    //Ethernet.begin(mac, ip);
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }

  Serial.print(F("Obtained IP: "));
  Serial.println(Ethernet.localIP());


  Serial.println(F("Setting Time Function"));
  unsigned long unixTime = webUnixTime(client);
  setTime(unixTime);
  // Set time
//  setSyncProvider(webUnixTime(client));
  
  while ( timeStatus() != timeSet ) {    
    Serial.println(F("Waiting for time to be set..."));
    now();
    delay(5000);    
  }
  char dateTime[19];
  sprintf(dateTime, "%d/%d/%d %d:%d:%d ", month(),day(),year(),hour(),minute(),second());

  sprintf(fileName, "16-%d-%d.log",month(),day());
  Serial.print(F("Log name: "));
  Serial.println(fileName);
  dataFile = SD.open(fileName, FILE_WRITE);

  if (dataFile) {
    dataFile.print(dateTime);
    dataFile.print("Obtained IP: ");
    dataFile.println(Ethernet.localIP());
    dataFile.flush();
  }   

}

void loop() {
  
  Ethernet.maintain();

  char dateTime[19];
  sprintf(dateTime, "%d/%d/%d %d:%d:%d ", month(),day(),year(),hour(),minute(),second());

//  SD.begin(SD_CS);
//  Serial.print(F("Log name: "));
//  Serial.println(fileName);
//  File dataFile = SD.open(fileName, FILE_WRITE);
  unsigned long now = millis();    
  ICMPEchoReply echoReply = ping(pingAddr, 4);
  if (echoReply.status == SUCCESS)
  {

    Serial.print(dateTime);
    Serial.println(F("Got Ping"));
    if ( outage ) {
      unsigned int duration = now - outage;
      Serial.print(dateTime);
      Serial.print(F("Internet was out for "));
      Serial.print(duration / 1000);
      Serial.println(F(" sec"));             
      if (dataFile) {
        dataFile.print(dateTime);
        dataFile.print(F("Internet was out for "));
        dataFile.print(duration / 1000);
        dataFile.println(F(" sec"));
        dataFile.flush();
      }    
      // reset outage
      outage = 0;   
    }
  }
  else
  {
    Serial.print(dateTime);
    Serial.println(F(" Failed Ping"));
    // Only set outage if we aren't already in outage.
    if ( ! outage ) {
      outage = now;    
      if (dataFile) {
        dataFile.print(dateTime);
        dataFile.println(F("Connectivity failing starts"));
        dataFile.flush();        
      }
    }
  }
  
  delay(1000);
}


/*
 * © Francesco Potortì 2013 - GPLv3
 *
 * Send an HTTP packet and wait for the response, return the Unix time
 */

unsigned long webUnixTime (Client &client)
{
  unsigned long time = 0;

  // Just choose any reasonably busy web server, the load is really low
  if (client.connect("google.com", 80))
    {
      // Make an HTTP 1.1 request which is missing a Host: header
      // compliant servers are required to answer with an error that includes
      // a Date: header.
      client.print(F("GET / HTTP/1.1 \r\n\r\n"));

      char buf[5];      // temporary buffer for characters
      client.setTimeout(5000);
      if (client.find((char *)"\r\nDate: ") // look for Date: header
    && client.readBytes(buf, 5) == 5) // discard
  {
    unsigned day = client.parseInt();    // day
    client.readBytes(buf, 1);    // discard
    client.readBytes(buf, 3);    // month
    int year = client.parseInt();    // year
    byte hour = client.parseInt();   // hour
    byte minute = client.parseInt(); // minute
    byte second = client.parseInt(); // second

    int daysInPrevMonths;
    switch (buf[0])
      {
      case 'F': daysInPrevMonths =  31; break; // Feb
      case 'S': daysInPrevMonths = 243; break; // Sep
      case 'O': daysInPrevMonths = 273; break; // Oct
      case 'N': daysInPrevMonths = 304; break; // Nov
      case 'D': daysInPrevMonths = 334; break; // Dec
      default:
        if (buf[0] == 'J' && buf[1] == 'a')
    daysInPrevMonths = 0;   // Jan
        else if (buf[0] == 'A' && buf[1] == 'p')
    daysInPrevMonths = 90;    // Apr
        else switch (buf[2])
         {
         case 'r': daysInPrevMonths =  59; break; // Mar
         case 'y': daysInPrevMonths = 120; break; // May
         case 'n': daysInPrevMonths = 151; break; // Jun
         case 'l': daysInPrevMonths = 181; break; // Jul
         default: // add a default label here to avoid compiler warning
         case 'g': daysInPrevMonths = 212; break; // Aug
         }
      }

    // This code will not work after February 2100
    // because it does not account for 2100 not being a leap year and because
    // we use the day variable as accumulator, which would overflow in 2149
    day += (year - 1970) * 365; // days from 1970 to the whole past year
    day += (year - 1969) >> 2;  // plus one day per leap year 
    day += daysInPrevMonths;  // plus days for previous months this year
    if (daysInPrevMonths >= 59  // if we are past February
        && ((year & 3) == 0)) // and this is a leap year
      day += 1;     // add one day
    // Remove today, add hours, minutes and seconds this month
    time = (((day-1ul) * 24 + hour) * 60 + minute) * 60 + second;
  }
    }
  delay(10);
  client.flush();
  client.stop();

  return time - 28800ul;  // -8h for PST
}

