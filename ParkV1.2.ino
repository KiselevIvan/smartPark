#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <time.h>
#include <ESP8266WiFi.h> 
#include <RFID.h>

#define RST_PIN D9
#define SS_PIN D10
//DHT
#define DHTTYPE DHT11
#define DHTPIN D2
//74HC595
#define dataPin D7
#define latch D8
#define CLK D5 //общий

#define intervalUpdateTime 1000
#define intervalUpdateUrn 50
#define intervalUpdatelight 50
#define timerWaitingTrash 20000
#define timeLightOn 4000
#define intervalUpdateRfid 300
const char* ssid = "trololo"; 
const char* password = "9.81zantiAD54331653"; 
int timezone = 3; 
int dst = 0; 
byte reg1=0;
byte reg2=0;
RFID rfid(SS_PIN, RST_PIN);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

static unsigned long timers[7]={0,0,0,0,0,0,0};
void showTH()
{
  static unsigned long lastUpdate=0;
  if ((millis()-lastUpdate)>2100)
  {
    byte t,h;
    t=dht.readTemperature();
    h=dht.readHumidity();
    lcd.setCursor(7,1);
    if ((t!=0xff)&&(h!=0xff))
    {  
      lcd.printf("T:%2d|H:%2d",t,h);
    }
    else
      lcd.print("dht error");
    lastUpdate=millis();
  }
}


byte regGet(byte n)
{  
  byte data;
  //reg2 ce,pl,000000  
  reg2 &= B10111111; //pl low
  regSet();  
  delayMicroseconds(5);
  reg2 |= B01000000; //pl high
  regSet();
  delayMicroseconds(5);
  reg2 &= B01111111; //ce low
  regSet(); 
  delayMicroseconds(5); 
  data=SPI.transfer (0);
  delayMicroseconds(5); 
  byte data2=SPI.transfer (0);  
  delayMicroseconds(5);
  reg2 |= B10000000; //ce high
  regSet();
  switch (n)
  {
   case 1:
    {return data;
    break;}
   case 2: 
   { return data2;
    break;}         
  }
}

void regSet()
{ 
  digitalWrite(latch,0);
  delayMicroseconds(5); 
  SPI.transfer(reg2); 
  digitalWrite(latch,1);
  delayMicroseconds(5);
  digitalWrite(latch,0);
  delayMicroseconds(5);
  SPI.transfer(reg1); 
  digitalWrite(latch,1); 
  delayMicroseconds(5);
}

void rfidMonitor()
{ 
  static unsigned long lastUpdateRfid=0;
  if ((millis()-lastUpdateRfid)>intervalUpdateRfid)
  {  
    digitalWrite(SS_PIN,0);
    if (rfid.isCard()) 
    {
      if (rfid.readCardSerial()) 
      {        
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Card detected");
        lcd.setCursor(0,1);
        lcd.print("UID:");
        lcdPrintHex(rfid.serNum, 5);
        yield();
        delay(1000);
        lcd.clear();
        if (trashMonitor())
        {
          lcd.setCursor(0,0);
          lcd.clear();
          lcd.print("reward received");
          delay(1000);
          lcd.clear();
        }
      } 
    } 
  rfid.halt();
  lastUpdateRfid=millis();
  } 
}

bool trashMonitor()
{
  Serial.println("trashmonitor");
  byte mask=B00000111;  
  static byte oldState=0;
  static unsigned long lastDisplayTaimer=0;
  unsigned long timeStop= millis()+timerWaitingTrash;
  while(millis()<timeStop)
  {
    yield();
    if(millis()-lastDisplayTaimer>1000)
    {
      lcdPrintTimer((timeStop-millis())/1000);
      lastDisplayTaimer=millis();
    }
    byte data = (~regGet(1))&mask;
    if (data>oldState)
      return true;  
    oldState=data;  
  }
  return false;
}

void lcdPrintTimer(byte n)
{
  lcd.setCursor(3,0);
  lcd.print("wait trash");
  lcd.setCursor(7,1);
  lcd.print(n,DEC);
  lcd.print("   ");
}

void lcdPrintHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    lcd.print(buffer[i] < 0x10 ? " 0" : " ");
    lcd.print(buffer[i], HEX);
  }
}

void lcdPrintTime()
{
 static unsigned long lastUpdateTime;
 if ((millis()-lastUpdateTime)>=intervalUpdateTime)
 {
  time_t t = time(nullptr);
  struct tm * tm;
  tm = localtime(&t);  
  lcd.setCursor(0,0);
  lcd.printf("%d/%d/%i %d:%d",tm->tm_mday,tm->tm_mon+1,tm->tm_year+1900,tm->tm_hour,tm->tm_min);
  lastUpdateTime=millis(); 
 }
}

void controlUrn()
{
  byte mask=B00000111;
  static unsigned long lastUpdateUrn=0;
  if ((millis()-lastUpdateUrn)>intervalUpdateUrn)
  {
  byte data = (~regGet(1))&mask;
  reg2=data|(reg2&~mask);
  regSet();  
  lastUpdateUrn=millis();
  }
}

void lightControl()
{  
  byte mask =B11111110;
  
  static unsigned long lastUpdatelight=0;
  static byte oldData=0;
  if ((millis()-lastUpdatelight)>intervalUpdatelight)
  {
    byte data = (~regGet(2))&mask; //новые данные    
    for(byte i=1;i<=7;i++) //нумерация бит с 0
    {      
      if ((data&(1<<i)) > (oldData&(1<<i))) //если появился объект
      {        
        if (i==6)
        {          
          reg2|=(1<<3);
        }
        
        else
        reg1= reg1|(1<<i);                
        timers[i-1]=millis();                                             
      }
      
      if ((timers[i-1] + timeLightOn)< millis())
      {
        if (i==6)
        reg2&=(~(1<<3));
        reg1=reg1&(~(1<<i)); //!!!!!
      }
      regSet();
    }       
    lastUpdatelight= millis();
    oldData=data;
  }  
}

void setup() 
{ 
  Serial.begin(9600);  
  pinMode(dataPin,OUTPUT);
  pinMode(CLK,OUTPUT);
  pinMode(latch,OUTPUT); 
  pinMode(SS_PIN,OUTPUT); 
  digitalWrite(latch,1);
  
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);
  dht.begin();
  SPI.begin();
  
  //reg2 ce,pl,000000
  reg2 |= B11000000;
  regSet();
  rfid.init();
  lcd.begin(D14,D15);  // sda=0, scl=2  
  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.setCursor(6,0);
  lcd.print("Wait");
  lcd.setCursor(1,1);
  lcd.print("Connection");
  while (WiFi.status() != WL_CONNECTED) 
  {     
    lcd.setCursor(11,1);
    lcd.print(".  ");
    delay(400);
    lcd.setCursor(11,1);
    lcd.print(".. ");
    delay(400);
    lcd.setCursor(11,1);
    lcd.print("..."); 
    delay(400);
    lcd.setCursor(11,1);
    lcd.print("   ");
  } 
   configTime(timezone * 3600, dst * 0, "pool.ntp.org", "time.nist.gov"); 
   while (!time(nullptr)) 
   { 
    delay(1000); 
   } 
  lcd.clear();  
}
void loop() 
{ 
  lightControl();
  controlUrn();
  showTH();
  lcdPrintTime();   
  byte i=0;
  char str[4]; 
    for(;(Serial.available()>0)&&(i<4);i++)
    { 
      str[i]=Serial.read();      
    }
    if (i==3)
    {
      int data=atoi(str);
      if(data==999)
      { 
        while (true)
        {
          delay(500);
          Serial.println("byte1       byte2"); 
          Serial.print(regGet(1),BIN);
          Serial.print("  ");
          Serial.println(regGet(2),BIN);
        }
      }
      else
      {
      Serial.printf("получено %3d\n",data);
      reg2=data;                
      regSet();
      }
    }
    else 
    rfidMonitor();
}
