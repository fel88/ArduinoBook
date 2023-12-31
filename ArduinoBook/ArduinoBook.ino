#include "EPD.h"
#include "GUI_Paint.h"
#include "EPD_SDCard.h"
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

//#define USE_DEBUG 1

#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#include <INA219_WE.h>
#define I2C_ADDRESS 0x40
/*U8G2_SSD1306_128X64_NONAME_F_HW_I2C*/ 
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE);  // All Boards without Reset of the Display
INA219_WE ina219 = INA219_WE(I2C_ADDRESS);

const int max_characters = 80;
char f_name[max_characters];

const int buttonPin3 = 19;   // the number of the pushbutton pin
const int buttonPin = 18;  // the number of the pushbutton pin
//const int buttonPin2 = 35;     // the number of the pushbutton pin
File file;

bool outdoorMode = false;

String root_dir = "/";
sFONT* font = &Font12;

int fontHeight = 12;
//int cols=54;//35 for font24; 54 for font16;85 for font12
//int rows=22;//17 for font24; 27 for font16; 29 for font12(gap=3)
int cols = 80;  //35 for font24; 54 for font16;85 for font12
int rows = 25;  //17 for font24; 27 for font16; 29 for font12(gap=3)
int page = 1;
int pages = 778;
//int gap=2;
int gap = 3;
//int fontWidth=11;//11 for font16, 17 for font24, 7 for font12
int fontWidth = 7;  //11 for font16, 17 for font24, 7 for font12

int bookMenuPos = 0;  //0- back to book,1-goto page menu, 2- close book
int bootMenuPos = 0;  //0 - load bookmarks list, 1- load files list
int bookmarkMenuPos = 0;
int totalBookmarks = 0;

UWORD Image_Width_Byte;
UWORD Bmp_Width_Byte;
int width;
int heigth;
extern SdFat sd;

#define MFILE_WRITE (O_RDWR | O_CREAT | O_AT_END)
#define MFILE_READ (O_RDONLY)

const size_t LINE_DIM = 550;
char line[LINE_DIM];

int buttonState = 0;   // variable for reading the pushbutton status
int buttonState2 = 0;  // variable for reading the pushbutton status

int cbPage = 0;
String currentBook = "";
int currentBookIdx = 0;
int menuMode = 0;  //0-files, 1-text book inside, 2- CB book inside, 3-book menu(goto page,back to files), 4-goto page menu, 5-boot menu, 6 - bookmarks list, 7- bmp image
int gotoPage = 0;
int gotoPageMenu = 0;

void saveBookmark() {
  //Serial.println("saveb");
  char buffer[380];
  sprintf(buffer, "%s;%6d", (root_dir + currentBook).c_str(), cbPage - 2);
  // Serial.print(buffer);
  int p = getBookmark();
  //  Serial.println(p);

  if (p == -1) {
    File fileB = sd.open("bookmarks.txt", MFILE_WRITE);
    fileB.println(buffer);
    //Serial.print("puts line");
    fileB.close();
    //   hasBookmark=true;
  } else {
    replaceBookmark();
  }
}

int replaceBookmark() {
  //Serial.println("replaceBookmark");
  char buffer[280];
  sprintf(buffer, "%s", (root_dir + currentBook).c_str());
  // Serial.println(buffer);
  File fileB = sd.open("bookmarks.txt", O_RDWR);
  size_t n;
  // if the file opened okay, write to it:
  if (fileB) {
    while ((n = fileB.fgets(line, sizeof(line))) > 0) {
      char* pch;
      pch = strtok(line, ";");
      int cntr = 0;
      //while (pch != NULL)
      {
        //  Serial. printf ("%s\n",pch);
        //   pch = strtok(NULL, ";");
        if (!strcmp(pch, buffer)) {
          char rep[6];
          sprintf(rep, "%6d", cbPage - 2);          
          int pos = fileB.position() - n + strlen(pch);

          fileB.seek(pos);  //6+newline
          fileB.write(rep);
          break;
        }
        cntr++;
      }
    }
    fileB.close();
  } else
    // Serial.println("bookmarks.txt not found");
    return -1;
}

int printFile(const char* path) {
  Serial.print("print file: ");
  Serial.println(path);
  char buffer[333];

  File fileB = sd.open(path, MFILE_READ);
  int n;
  Serial.print(buffer);
  if (fileB) {
    while ((n = fileB.fgets(buffer, sizeof(line))) > 0) {
      Serial.println(buffer);
    }
    fileB.close();
  } else {
    Serial.println("file not found");
  }
  return -1;
}
int getBookmark() {
  // Serial.print("getb");
  char buffer[280];
  sprintf(buffer, "%s", (root_dir + currentBook).c_str());
  File fileB = sd.open("bookmarks.txt", MFILE_READ);
  size_t n;
  //Serial.print(buffer);
  if (fileB) {
    while ((n = fileB.fgets(line, sizeof(line))) > 0) {
      char* pch;
      pch = strtok(line, ";");
      int cntr = 0;

      if (!strcmp(pch, buffer)) {
        pch = strtok(NULL, ";");
        int wordToGo = atoi(pch);

        return wordToGo;
      }
      cntr++;
    }
    fileB.close();
  } else {
    //Serial.println("bookmarks.txt not found");
  }
  return -1;
}


void SDCard_ReadCB(const char* Name) {


  //file = sd.open(Name,O_READ );

  //Serial.println(file.size());
  if (!file.open(Name, O_RDONLY)) {
    //  Serial.println("not found");
    DEBUG("not find : ");
    DEBUG(Name);
    DEBUG("\n");
    return;
  } else {
    DEBUG("open bmp file : ");
    DEBUG(Name);
    DEBUG("\n");
  }


  UWORD File_Type;
  File_Type = SDCard_Read16(file);  //0000h 2byte: file type

  // Serial.println(File_Type);

  if (File_Type != 0x4243) {
    // magic bytes missing
    return false;
  }

  file.read();
  file.read();
  int pages = SDCard_Read32(file);
  //  Serial.print("pages: ");
  // Serial.println(pages);
  // read file size


  width = SDCard_Read16(file);
  heigth = SDCard_Read16(file);

  //Serial.print("width: ");
  // Serial.println(width);

  //Serial.print("heigth: ");
  // Serial.println(heigth);
  UWORD X, Y;
  Image_Width_Byte = (width % 8 == 0) ? (width / 8) : (width / 8 + 1);
  Bmp_Width_Byte = (Image_Width_Byte % 4 == 0) ? Image_Width_Byte : ((Image_Width_Byte / 4 + 1) * 4);
}


void resetCB() {
  file.seek(12);
  cbPage = 0;
}

const unsigned long bytesPerPageCB = 76ul * 448ul;

void skipPagesCB(int cnt) {
  unsigned long offset = 12ul + ((unsigned long)cnt) * bytesPerPageCB;
  file.seek(offset);
  cbPage += cnt;
}
extern UBYTE _readBuff[801];
void fastNextPageCB() {
  UWORD X, Y;
  cbPage++;
  UBYTE Data_Black, Data;
  EPD_5IN83_SendCommand(0x10);
  UBYTE ReadBuff[1] = { 0 };
  for (Y = 0; Y < heigth; Y++) {  //Total display column
    file.read(_readBuff, Bmp_Width_Byte);
    for (X = 0; X < Bmp_Width_Byte; X++) {  //Show a line in the line
      //file.read(ReadBuff, 1);


      //ReadBuff[0]=reverse(ReadBuff[0]);
      if (X < Image_Width_Byte) {  //bmp
        if (Paint_Image.Image_Color == IMAGE_COLOR_POSITIVE) {

          Data_Black = _readBuff[X];
        } else {
          Data_Black = ~_readBuff[X];
        }
        for (UBYTE k = 0; k < 8; k++) {
          if (Data_Black & 0x80)
            Data = 0x00;
          else
            Data = 0x03;
          Data <<= 4;
          Data_Black <<= 1;
          k++;
          if (Data_Black & 0x80)
            Data |= 0x00;
          else
            Data |= 0x03;
          Data_Black <<= 1;
          EPD_5IN83_SendData(Data);
        }
      }
    }
    //bmpFile.read(ReadBuff, 1);
  }

  EPD_5IN83_Power(false);
}

void sendToDisplay(void) {
  UBYTE Data_Black, Data;
  UWORD Width, Height;
  Width = (EPD_5IN83_WIDTH % 8 == 0) ? (EPD_5IN83_WIDTH / 8) : (EPD_5IN83_WIDTH / 8 + 1);
  Height = EPD_5IN83_HEIGHT;

  EPD_5IN83_SendCommand(0x10);
  for (UWORD j = 0; j < Height; j++) {
    for (UWORD i = 0; i < Width; i++) {
      Data_Black = SPIRAM_RD_Byte(i + j * Width);
      for (UBYTE k = 0; k < 8; k++) {
        if (Data_Black & 0x80)
          Data = 0x00;
        else
          Data = 0x03;
        Data <<= 4;
        Data_Black <<= 1;
        k++;
        if (Data_Black & 0x80)
          Data |= 0x00;
        else
          Data |= 0x03;
        Data_Black <<= 1;
        EPD_5IN83_SendData(Data);
      }
    }
  }
  //EPD_5IN83_TurnOnDisplay();
}

void fastDisplayBuffer() {
  EPD_5IN83_Power(true);
  EPD_5IN83_TurnOnDisplay();
}

void displayBuffer() {
  EPD_5IN83_Display();
  //EPD_5IN83_Sleep();
}

void clearOled() {
  u8g2.clearBuffer();  // clear the internal memory
  u8g2.sendBuffer();   // transfer internal memory to the display
}


void nextPageCB() {
  UWORD X, Y;
  cbPage++;
  UBYTE ReadBuff[1] = { 0 };
  for (Y = 0; Y < heigth; Y++) {  //Total display column

    for (X = 0; X < Bmp_Width_Byte; X++) {  //Show a line in the line
      file.read(ReadBuff, 1);
      if (X < Image_Width_Byte) {  //bmp
        if (Paint_Image.Image_Color == IMAGE_COLOR_POSITIVE) {
          SPIRAM_WR_Byte((X) + (Y)*Image_Width_Byte + Paint_Image.Image_Offset, ReadBuff[0]);
        } else {
          SPIRAM_WR_Byte((X) + (Y)*Image_Width_Byte + Paint_Image.Image_Offset, ~ReadBuff[0]);
        }
      }
    }
  }
}


void skipPage() {

  u8g2.clearBuffer();  // clear the internal memory
  u8g2.sendBuffer();   // transfer internal memory to the display

  for (int i = 0; i < rows; i++) {

    //String data = file.readString();
    int j;
    for (j = 0; j < cols; j++) {
      unsigned char ch = file.read();

      if (ch == '\n' || ch == '\r') {

        break;
      }
    }
    if (j == 0 && i == 0) {
      i--;
      continue;
    }
  }

  page++;
}

void nextPage() {
  u8g2.clearBuffer();  // clear the internal memory
  u8g2.sendBuffer();   // transfer internal memory to the display
  Paint_Clear(WHITE);
  for (int i = 0; i < rows; i++) {
    unsigned char data[cols];
    //String data = file.readString();
    int j;
    for (j = 0; j < cols; j++) {
      unsigned char ch = file.read();

      if (ch == '\n' || ch == '\r') {
        data[j] = '\0';
        break;
      }
      data[j] = ch;
    }
    if (j == 0 && i == 0) {
      i--;
      continue;
    }
    data[cols] = '\0';

    Paint_DrawString_EN(0, i * (fontHeight + gap), data, font, WHITE, BLACK);
  }
  Paint_DrawLine(5, rows * (fontHeight + gap), 595, rows * (fontHeight + gap), BLACK, LINE_STYLE_SOLID, DOT_PIXEL_1X1);
  String p = "Page: ";
  String p2 = " / ";

  String c = p + page + p2 + pages;

  Paint_DrawString_EN(0, rows * (fontHeight + gap) + 1, c.c_str(), font, WHITE, BLACK);
  EPD_5IN83_Display();
  page++;
}

bool fileExist(const char* path) {

  File fileB = sd.open(path, MFILE_READ);
  if (fileB) {
    fileB.close();
  } else {
    return false;
  }
  return true;
}
void setup() {

  // clock_prescale_set(clock_div_2);

  //ADCSRA = 0;
  ADCSRA &= ~(1 << ADEN);


  rows = (448 - 1) / (gap + fontHeight) - 1;
  cols = 600 / fontWidth;

  //rows-=5;
  //cols-=5;

  //pinMode(LED_BUILTIN, OUTPUT);
  // digitalWrite(LED_BUILTIN, LOW);
  if (!ina219.init()) {
    //Serial.println("INA219 not connected!"); b=false;
  }
  float shuntVoltage_mV = 0.0;
  float loadVoltage_V = 0.0;
  float busVoltage_V = 0.0;



  shuntVoltage_mV = ina219.getShuntVoltage_mV();
  busVoltage_V = ina219.getBusVoltage_V();

  loadVoltage_V = busVoltage_V + (shuntVoltage_mV / 1000);
  ina219.powerDown();

  u8g2.begin();
  u8g2.setContrast(0x5);
  u8g2.setFlipMode(1);
  u8g2.clearBuffer();                     // clear the internal memory
  u8g2.setFont(u8g2_font_logisoso18_tr);  // choose a suitable font
  u8g2.drawStr(0, 25, "V");

  u8g2.setCursor(55, 25);
  u8g2.print(loadVoltage_V);

  u8g2.sendBuffer();
  delay(1000);
  DEV_Module_Init();

  EPD_5IN83_Init();
  //EPD_5IN83_Clear();
  Paint_NewImage(IMAGE_BW, EPD_5IN83_WIDTH, EPD_5IN83_HEIGHT, IMAGE_ROTATE_0, IMAGE_COLOR_INVERTED);
  SDCard_Init();
  

  if (fileExist("bookmarks.txt")) {
    menuMode = 5;
    drawBootMenu();
  } else {

  menuMode = 0;
  drawFileList();
  }

  pinMode(buttonPin, INPUT);
  //pinMode(buttonPin2, INPUT);
  pinMode(buttonPin3, INPUT);

  attachInterrupt(digitalPinToInterrupt(buttonPin), myISR2, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPin3), myISR, FALLING);
  /////////////
}



String getBookmarkName(int idx) {

  File fileB = sd.open("bookmarks.txt", MFILE_READ);
  size_t n;
  int cntr = 0;
  //Serial.print(buffer);
  if (fileB) {
    while ((n = fileB.fgets(line, sizeof(line))) > 0) {
      char* pch;
      pch = strtok(line, ";");

      if (cntr == idx) {
        String str = String(pch);
        fileB.close();
        return str;
      }
      cntr++;
    }
    fileB.close();
  } else {
    //Serial.println("bookmarks.txt not found");
  }
}

void drawBookmarksList() {


  u8g2.clearBuffer();  // clear the internal memory
  //u8g2.setFont(u8g2_font_4x6_t_cyrillic); // choose a suitable font
  u8g2.setFont(u8g2_font_9x15_t_cyrillic);  // choose a suitable font
  //u8g2_font_cu12_t_cyrillic
  u8g2.setContrast(0x5);
  // Serial.print("getb");
  EPD_5IN83_Power(true);
  Paint_Clear(WHITE);

  File fileB = sd.open("bookmarks.txt", MFILE_READ);
  size_t n;
  int cntr = 0;
  //Serial.print(buffer);
  if (fileB) {
    while ((n = fileB.fgets(line, sizeof(line))) > 0) {
      char* pch;
      pch = strtok(line, ";");
      Paint_DrawString_EN(0, cntr * fontHeight, pch, &Font12, WHITE, BLACK);
      if (cntr == 0)
        u8g2.drawStr(0, 16, pch);  // write something to the internal memory

      pch = strtok(NULL, ";");
      int page = atoi(pch);
      //Serial.println(filename);
      Paint_DrawString_EN(400, cntr * fontHeight, String(page).c_str(), &Font12, WHITE, BLACK);
      cntr++;
      if (cntr > 14)
        break;
    }
    fileB.close();
  } else {
    //Serial.println("bookmarks.txt not found");
  }


  totalBookmarks = cntr;
  //display.display();
  u8g2.sendBuffer();  // transfer internal memory to the display
  EPD_5IN83_Display();
  EPD_5IN83_Power(false);
}
void drawFileList() {


  u8g2.clearBuffer();  // clear the internal memory
  //u8g2.setFont(u8g2_font_4x6_t_cyrillic); // choose a suitable font
  u8g2.setFont(u8g2_font_4x6_t_cyrillic);  // choose a suitable font
  //u8g2_font_cu12_t_cyrillic
  u8g2.setContrast(0x5);
  EPD_5IN83_Power(true);
  File root = sd.open(root_dir.c_str());
  Paint_Clear(WHITE);
  int cntr = 0;
  if (root_dir != "/") {

    Paint_DrawString_EN(0, cntr * fontHeight, "..", &Font12, WHITE, BLACK);
    u8g2.drawStr(0, cntr * fontHeight, "..");
    cntr++;
  }
  
  while (true) {

    File entry = root.openNextFile();
    if (!entry) {

      // no more files

      break;
    }

    entry.getName(f_name, max_characters);
    String filename = String(f_name);

    Paint_DrawString_EN(0, cntr * fontHeight, filename.c_str(), &Font12, WHITE, BLACK);
    //u8g2.drawStr(0, cntr * fontHeight, filename.c_str());  // write something to the internal memory

    cntr++;


    if (entry.isDirectory()) {
      //Serial.println("/");
      //printDirectory(entry, numTabs + 1);

    } else {
      // files have sizes, directories do not
      // Serial.print("\t\t");
      //Serial.println(entry.size(), DEC);
    }

    entry.close();
  }

  root.close();

  u8g2.clearBuffer();                       // clear the internal memory
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);  // choose a suitable font
  currentBook = getFileN(0);

  u8g2.drawStr(0, 16, currentBook.c_str());  // write something to the internal memory

  //display.display();
  u8g2.sendBuffer();  // transfer internal memory to the display
  EPD_5IN83_Display();
  EPD_5IN83_Power(false);
}

int getTotalFiles() {

  File root = sd.open(root_dir.c_str());

  int cntr = 0;
  if (root_dir != "/") {
    cntr++;
  }
  while (true) {

    File entry = root.openNextFile();
    if (!entry) {
      break;
    }

    cntr++;
    if (entry.isDirectory()) {

    } else {
    }

    entry.close();
  }
  root.close();
  return cntr;
}

bool isDir = false;

String getFileN(int n) {
  isDir = false;
  File root = sd.open(root_dir.c_str());

  int cntr = 0;
  if (root_dir != "/") {
    if (n == 0) {
      isDir = true;
      return "..";
    }
    cntr++;
  }
  while (true) {

    File entry = root.openNextFile();
    if (!entry) {

      // no more files

      break;
    }


    /////////////
    if (cntr == n) {
      if (entry.isDirectory()) {
        isDir = true;
      }

      entry.getName(f_name, max_characters);
      String filename = String(f_name);

      entry.close();
      //return entry.name();
      return filename;
    }
    cntr++;

    if (entry.isDirectory()) {

      //Serial.println("/");
      //printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      //Serial.print("\t\t");
      //Serial.println(entry.size(), DEC);
    }

    entry.close();
  }
  root.close();
  return "";
}



void drawGotoPageMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_10x20_t_cyrillic);
  if (gotoPageMenu == 0)
    u8g2.drawStr(0, 16, "up +1");
  if (gotoPageMenu == 1)
    u8g2.drawStr(0, 16, "up +5");
  if (gotoPageMenu == 2)
    u8g2.drawStr(0, 16, "up +10");
  if (gotoPageMenu == 3)
    u8g2.drawStr(0, 16, "up +100");
  if (gotoPageMenu == 4)
    u8g2.drawStr(0, 16, "up -1");
  if (gotoPageMenu == 5)
    u8g2.drawStr(0, 16, "goto");
  if (gotoPageMenu == 6)
    u8g2.drawStr(0, 16, "back");


  u8g2.drawStr(0, 32, "Page: ");
  u8g2.setCursor(64, 32);
  u8g2.print(gotoPage + 1);
  u8g2.sendBuffer();
}
void processBookMenu() {
  if (bookMenuPos == 2) {
    u8g2.clearBuffer();                       // clear the internal memory
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);  // choose a suitable font

    u8g2.drawStr(0, 16, "Wait..");  // write something to the internal memory
    u8g2.sendBuffer();              // transfer internal memory to the display

    menuMode = 0;
    file.close();
    if (outdoorMode) {
      EPD_5IN83_Reset();
      EPD_5IN83_Init();
      Paint_NewImage(IMAGE_BW, EPD_5IN83_WIDTH, EPD_5IN83_HEIGHT, IMAGE_ROTATE_0, IMAGE_COLOR_INVERTED);
      SDCard_Init();
    }

    drawFileList();
  } else if (bookMenuPos == 1) {
    menuMode = 4;
    gotoPage = 0;
    gotoPageMenu = 0;
    drawGotoPageMenu();
    
  } else if (bookMenuPos == 0) {
    menuMode = 2;
    u8g2.clearBuffer();  // clear the internal memory

    u8g2.sendBuffer();
  } else if (bookMenuPos == 3) {  //print page info
    menuMode = 2;
    u8g2.clearBuffer();  // clear the internal memory
    u8g2.sendBuffer();
    int temp = cbPage - 2;
    cbPage = 0;
    skipPagesCB(temp);
    nextPageCB();
    Paint_DrawString_EN(10, 310, currentBook.c_str(), &Font24, BLACK, WHITE);

    Paint_DrawNum(10, 360, cbPage, &Font24, BLACK, WHITE);

    //3.Refresh the picture in RAM to e-Paper
    //  DEBUG("EPD_5IN83_Display\r\n");
    EPD_5IN83_Display();

  } else if (bookMenuPos == 4) {  //one page back
    menuMode = 2;
    u8g2.clearBuffer();  // clear the internal memory
    u8g2.sendBuffer();
    int temp = cbPage - 3;
    cbPage = 0;
    skipPagesCB(temp);
    fastNextPageCB();
    fastDisplayBuffer();
    fastNextPageCB();

  } else if (bookMenuPos == 5) {  //goto bookmark
    int p = getBookmark();
    if (p != -1) {
      menuMode = 2;
      u8g2.clearBuffer();  // clear the internal memory
      u8g2.sendBuffer();

      int temp = p;
      cbPage = 0;
      skipPagesCB(temp);
      fastNextPageCB();
      fastDisplayBuffer();
      fastNextPageCB();
    }

  } else if (bookMenuPos == 6) {  //save bookmark

    saveBookmark();
    menuMode = 2;
    u8g2.clearBuffer();  // clear the internal memory

    u8g2.sendBuffer();
  } else if (bookMenuPos == 7) {  //remove all bookmarks
    File fileB = sd.open("bookmarks.txt", MFILE_WRITE);
    if (fileB) {
      fileB.remove();
    }

    menuMode = 2;
    u8g2.clearBuffer();  // clear the internal memory

    u8g2.sendBuffer();
  }
}


void FastReadBMP(const char* BmpName, UWORD Xstart, UWORD Ystart) {
  File bmpFile;
  //bmpFile = sd.open(BmpName,FILE_READ);

  if (!bmpFile.open(BmpName, O_RDONLY)) {
    DEBUG("not find : ");
    DEBUG(BmpName);
    DEBUG("\n");
    return;
  } else {
    DEBUG("open bmp file : ");
    DEBUG(BmpName);
    DEBUG("\n");
  }

  if (!SDCard_ReadBmpHeader(bmpFile)) {
    DEBUG("read bmp file error\n");
    return;
  }
  DEBUG("out\n");
  DEBUG("BMP_Header index\n");
  DEBUG(BMP_Header.Index);
  bmpFile.seek(BMP_Header.Index);

  UWORD X, Y;
  UWORD Image_Width_Byte = (BMP_Header.BMP_Width % 8 == 0) ? (BMP_Header.BMP_Width / 8) : (BMP_Header.BMP_Width / 8 + 1);
  UWORD Bmp_Width_Byte = (Image_Width_Byte % 4 == 0) ? Image_Width_Byte : ((Image_Width_Byte / 4 + 1) * 4);
  DEBUG(Image_Width_Byte);
  DEBUG(Bmp_Width_Byte);

  UBYTE Data_Black, Data;
  UWORD Width, Height;
  Width = (EPD_5IN83_WIDTH % 8 == 0) ? (EPD_5IN83_WIDTH / 8) : (EPD_5IN83_WIDTH / 8 + 1);
  Height = EPD_5IN83_HEIGHT;

  //UBYTE ReadBuff[1] = {0};
  EPD_5IN83_SendCommand(0x10);
  for (Y = Ystart; Y < BMP_Header.BMP_Height; Y++) {  //Total display column
    bmpFile.read(_readBuff, Bmp_Width_Byte);
    for (X = Xstart / 8; X < Bmp_Width_Byte; X++) {  //Show a line in the line
      //bmpFile.read(ReadBuff, 1);
      if (X < Image_Width_Byte) {  //bmp

        if (Paint_Image.Image_Color == IMAGE_COLOR_POSITIVE) {
          Data_Black = _readBuff[X];
          //SPIRAM_WR_Byte(X + ( BMP_Header.BMP_Height - Y - 1 ) * Image_Width_Byte , _readBuff[X]);
        } else {
          Data_Black = ~_readBuff[X];
          //  SPIRAM_WR_Byte(X + ( BMP_Header.BMP_Height - Y - 1 ) * Image_Width_Byte , ~_readBuff[X]);
        }
        for (UBYTE k = 0; k < 8; k++) {
          if (Data_Black & 0x80)
            Data = 0x00;
          else
            Data = 0x03;
          Data <<= 4;
          Data_Black <<= 1;
          k++;
          if (Data_Black & 0x80)
            Data |= 0x00;
          else
            Data |= 0x03;
          Data_Black <<= 1;
          EPD_5IN83_SendData(Data);
        }
      }
    }
  }
  DEBUG("cp3\n");
  bmpFile.close();
  DEBUG("cp4\n");
  EPD_5IN83_TurnOnDisplay();
}

bool b = true;
bool btn1 = false;
bool btn2 = false;
void myISR() {
  btn1 = true;
}
void applyButton() {
  //b=!b;
  //digitalWrite(LED_BUILTIN, b?HIGH:LOW);
  u8g2.clearBuffer();  // clear the internal memory

  //u8g2_font_9x15_t_cyrillic
  //u8g2_font_unifont_t_cyrillic
  //u8g2_font_10x20_t_cyrillic
  //u8g2_font_inr24_t_cyrillic
  //u8g2_font_inr27_t_cyrillic


  if (menuMode == 2) {
    u8g2.setFont(u8g2_font_10x20_t_cyrillic);  // choose a suitable font
    u8g2.drawStr(0, 24, "PAGE: ");             // write something to the internal memory
    u8g2.setCursor(64, 24);
    u8g2.print(cbPage + 1);
  } else if (menuMode == 4) {

  } else {
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);  // choose a suitable font
    u8g2.drawStr(0, 16, "Wait..");            // write something to the internal memory
  }
  u8g2.sendBuffer();  // transfer internal memory to the display

  if (menuMode == 6) {

    String str = getBookmarkName(bookmarkMenuPos);
    root_dir = str;

    for (int j = root_dir.length() - 2; j >= 0; j--) {
      if (root_dir[j] == '/') {
        root_dir = root_dir.substring(0, j + 1);
        currentBook = str.substring(j + 1);
        break;
      }
    }
    //set currentBook
    SDCard_ReadCB((root_dir + currentBook).c_str());
    menuMode = 2;
    int p = getBookmark();

    u8g2.clearBuffer();  // clear the internal memory
    u8g2.sendBuffer();

    int temp = p;
    cbPage = 0;
    skipPagesCB(temp);
    fastNextPageCB();
    fastDisplayBuffer();
    fastNextPageCB();

  } else if (menuMode == 5) {
    if (bootMenuPos == 0) {  //draw bookmarks
      menuMode = 6;
      drawBookmarksList();

    } else if (bootMenuPos == 1) {  //files
      menuMode = 0;
      drawFileList();
    }
  } else if (menuMode == 1) {
    nextPage();
  } else if (menuMode == 3) {
    processBookMenu();
  } else if (menuMode == 2) {
    //EPD_5IN83_Init();
    clearOled();
    //displayBuffer();

    if (outdoorMode) {
      EPD_5IN83_Reset();
      EPD_5IN83_Init();
      Paint_NewImage(IMAGE_BW, EPD_5IN83_WIDTH, EPD_5IN83_HEIGHT, IMAGE_ROTATE_0, IMAGE_COLOR_INVERTED);
      SDCard_Init();
      SDCard_ReadCB((root_dir + currentBook).c_str());
      fastNextPageCB();
      fastDisplayBuffer();
      EPD_5IN83_Sleep();
    } else {


      fastDisplayBuffer();
      //nextPageCB();
      //sendToDisplay();
      fastNextPageCB();
    }


  } else if (menuMode == 0) {
    if (isDir) {
      if (currentBook == "..") {
        for (int j = root_dir.length() - 2; j >= 0; j--) {
          if (root_dir[j] == '/') {
            root_dir = root_dir.substring(0, j + 1);
            break;
          }
        }
      } else {
        root_dir += currentBook + "/";
      }

      currentBookIdx = 0;
      drawFileList();
    } else {


      if (currentBook.endsWith(".CB") || currentBook.endsWith(".cb")) {
        menuMode = 2;
        SDCard_ReadCB((root_dir + currentBook).c_str());
        cbPage = 0;
        clearOled();
        fastNextPageCB();
        // nextPageCB();
        fastDisplayBuffer();
        // nextPageCB();
        //sendToDisplay();
        fastNextPageCB();
      } else if (currentBook.endsWith(".TXT")) {
        file = sd.open((root_dir + currentBook), O_READ);
        pages = file.size() / (rows * cols);
        menuMode = 1;
        nextPage();
      } else if (currentBook.endsWith(".bmp") || currentBook.endsWith(".BMP")) {
        menuMode = 7;

        clearOled();

        Paint_Clear(WHITE);
        //FastReadBMP((root_dir + currentBook).c_str(), 0, 0);

        EPD_5IN83_Reset();
        EPD_5IN83_Init(1);
        Paint_NewImage(IMAGE_BW, EPD_5IN83_WIDTH, EPD_5IN83_HEIGHT, IMAGE_ROTATE_0, IMAGE_COLOR_INVERTED);
        SDCard_Init();

        SDCard_ReadBMP((root_dir + currentBook).c_str(), 0, 0);
        EPD_5IN83_Display();

        EPD_5IN83_Sleep();
      }
    }

  } else if (menuMode == 4) {
    if (gotoPageMenu == 0) {
      gotoPage++;
      drawGotoPageMenu();
    } else if (gotoPageMenu == 1) {
      gotoPage += 5;
      drawGotoPageMenu();
    } else if (gotoPageMenu == 2) {
      gotoPage += 10;
      drawGotoPageMenu();
    } else if (gotoPageMenu == 3) {
      gotoPage += 100;
      drawGotoPageMenu();
    } else if (gotoPageMenu == 4) {
      gotoPage -= 1;
      drawGotoPageMenu();
    } else if (gotoPageMenu == 5) {
      u8g2.clearBuffer();                       // clear the internal memory
      u8g2.setFont(u8g2_font_cu12_t_cyrillic);  // choose a suitable font

      u8g2.drawStr(0, 16, "Wait..");  // write something to the internal memory
      u8g2.sendBuffer();              // transfer internal memory to the display
      if (currentBook.endsWith(".CB") || currentBook.endsWith(".cb")) {
        //resetCB();
        cbPage = 0;
        clearOled();
        skipPagesCB(gotoPage);
        fastNextPageCB();
        fastDisplayBuffer();
        fastNextPageCB();
        menuMode = 2;
      } else if (currentBook.endsWith(".TXT")) {
        page = gotoPage - 1;
        //file.seek(0);
        unsigned long offset = ((unsigned long)rows * (unsigned long)cols) * (gotoPage - 1);
        file.seek(offset);
        //for(int k=0;k<gotoPage-1;k++)
        //    skipPage();
        nextPage();
        menuMode = 1;
      }
    } else if (gotoPageMenu == 6) {
      menuMode = 3;
      bookMenuPos = 0;
      drawBookMenu();
    }
  }
}


void drawBootMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_t_cyrillic);
  if (bootMenuPos == 0)
    u8g2.drawStr(0, 16, "bookmarks");
  if (bootMenuPos == 1) {
    u8g2.drawStr(0, 16, "files");
  }

  u8g2.sendBuffer();
}

void drawBookmarkMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_t_cyrillic);

  String name = getBookmarkName(bookmarkMenuPos);

  u8g2.drawStr(0, 16, name.c_str());

  u8g2.sendBuffer();
}

void drawBookMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_cu12_t_cyrillic);  // choose a suitable font
  if (bookMenuPos == 0)
    u8g2.drawStr(0, 16, "back to book");
  if (bookMenuPos == 1) {
    u8g2.drawStr(0, 16, "goto page");

    u8g2.drawStr(0, 32, "current: ");
    u8g2.setCursor(64, 32);
    u8g2.print(cbPage + 1);
  } else if (bookMenuPos == 2)
    u8g2.drawStr(0, 16, "close book");
  else if (bookMenuPos == 3)
    u8g2.drawStr(0, 16, "print page info");
  else if (bookMenuPos == 4)
    u8g2.drawStr(0, 16, "one page back");
  else if (bookMenuPos == 5)
    u8g2.drawStr(0, 16, "goto bookmark");
  else if (bookMenuPos == 6)
    u8g2.drawStr(0, 16, "save bookmark");
  else if (bookMenuPos == 7)
    u8g2.drawStr(0, 16, "remove bookmarks");

  u8g2.sendBuffer();
}

void myISR2() {
  btn2 = true;
}

void menuButton() {
  //b=!b;
  //digitalWrite(LED_BUILTIN, b?HIGH:LOW);


  if (menuMode == 1) {
    u8g2.clearBuffer();                       // clear the internal memory
    u8g2.setFont(u8g2_font_cu12_t_cyrillic);  // choose a suitable font

    u8g2.drawStr(0, 16, "Wait..");  // write something to the internal memory
    u8g2.sendBuffer();              // transfer internal memory to the display
    menuMode = 3;
    bookMenuPos = 0;

    drawBookMenu();
  } else if (menuMode == 7) {  //bmp

    menuMode = 0;
    EPD_5IN83_Reset();
    EPD_5IN83_Init();
    Paint_NewImage(IMAGE_BW, EPD_5IN83_WIDTH, EPD_5IN83_HEIGHT, IMAGE_ROTATE_0, IMAGE_COLOR_INVERTED);
    SDCard_Init();
    drawFileList();
  } else if (menuMode == 6) {
    bookmarkMenuPos++;
    if (bookmarkMenuPos == totalBookmarks) bookmarkMenuPos = 0;
    drawBookmarkMenu();

  } else if (menuMode == 5) {
    bootMenuPos++;
    if (bootMenuPos == 2) bootMenuPos = 0;
    drawBootMenu();
  } else if (menuMode == 3) {
    bookMenuPos++;
    if (bookMenuPos == 8) bookMenuPos = 0;
    drawBookMenu();
  } else if (menuMode == 2) {

    menuMode = 3;
    bookMenuPos = 0;

    drawBookMenu();
    //file.close();
    //drawFileList();
  } else if (menuMode == 0) {
    currentBookIdx++;
    if (currentBookIdx >= getTotalFiles()) {
      currentBookIdx = 0;
    }
    currentBook = getFileN(currentBookIdx);
    //currentBook.toLowerCase();

    u8g2.clearBuffer();  // clear the internal memory
    //u8g2_font_9x15_t_cyrillic
    //u8g2_font_unifont_t_cyrillic
    //u8g2_font_10x20_t_cyrillic
    //u8g2_font_inr24_t_cyrillic
    //u8g2_font_inr27_t_cyrillic
    u8g2.setFont(u8g2_font_10x20_t_cyrillic);  // choose a suitable font
    u8g2.drawStr(0, 16, currentBook.c_str());  // write something to the internal memory
    u8g2.sendBuffer();
  } else if (menuMode == 4) {
    gotoPageMenu++;
    if (gotoPageMenu >= 7) {
      gotoPageMenu = 0;
    }
    drawGotoPageMenu();
  }
}

void loop() {
  interrupts();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();
  
  if (btn1) {
    applyButton();
    btn1 = false;
  } else if (btn2) {
    btn2 = false;
    menuButton();
  }

  noInterrupts(); 
}
