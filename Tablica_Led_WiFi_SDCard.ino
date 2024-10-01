#define AP

#include "PinDefinitions.h"
#include "RequestParser.h"
#include "Settings.h"
#include "ArduinoSecrets.h"
#include "Letters.h"
#include <WiFi.h>
#include <SD.h>

enum DataEnum { Waiting, Started, Reading, Ended };

uint16_t registerPins[8] = { SER3_PIN, SER4_PIN, SER2_PIN, SER1_PIN, SER5_PIN, SER6_PIN, SER0_PIN, SER7_PIN };

uint16_t memory[Settings::MAX_VALUES];
uint16_t display[Settings::MAX_SEND_SIZE];

int currentStoredValues = 0;
int startIndex = 0;
char attempts = 0;
int currentDelay = 0;
String currentSet = "";

DataEnum readingState = DataEnum::Ended;
Settings settings;

const int startPosition = -Settings::MAX_SEND_SIZE;

WiFiServer server(80);

struct DisplaySet {
  String fileName;
  int displayTime;
};

DisplaySet displayQueue[20];
int currentDisplayIndex = 0;
int displayDuration = 0;
int displayTimeCounter = 0;

// DECLARATIONS ---------------------------------------------------------------

size_t printToMemory(const uint16_t* source, size_t arraySize, size_t offset = 0);
size_t printIPAddress(IPAddress ip);
size_t printNumberToMemory(byte number, size_t index);
void readData();
String handleUploadRequest(String request, WiFiClient client);
String handleFileRequest(String request, WiFiClient client);
String handleListFilesPlain();
String handleListFilesHtml();
String handleDataUploadRequest(String request, WiFiClient client);
String handleSetRequest(String request, WiFiClient client);
String handleSetListRequest();
void prepareData();
void clearMemory();
void clearRegisters();
void writeRegisters();
bool loadSet(String fileName);
bool loadSet(File file);
void loadNextDisplay();
void saveStartupBin(const uint16_t* data, int size);
void saveStartupSet(String data);
void loadStartupFile(File file);

// IMPLEMENTATIONS ------------------------------------------------------------

void setup() {
  pinMode(RCLK_PIN, OUTPUT);
  pinMode(SRCLK_PIN, OUTPUT);
  pinMode(OE_PIN, OUTPUT);
  pinMode(SER0_PIN, OUTPUT);
  pinMode(SER1_PIN, OUTPUT);
  pinMode(SER2_PIN, OUTPUT);
  pinMode(SER3_PIN, OUTPUT);
  pinMode(SER4_PIN, OUTPUT);
  pinMode(SER5_PIN, OUTPUT);
  pinMode(SER6_PIN, OUTPUT);
  pinMode(SER7_PIN, OUTPUT);

  Serial.begin(9600);

  clearRegisters();
  writeRegisters();

  if (WiFi.status() == WL_NO_SHIELD){
    Serial.println("WiFi shield not present");
    Settings::ENABLE_WIFI = false;
  }

  if (Settings::ENABLE_SD) {
    if (SD.begin(SD_CS)) {
      Serial.println("SD Card initialization successful!");
    }
    else {
      Settings::ENABLE_SD = false;
      Serial.println("SD Card initialization failed!");
    }
  }

  if (Settings::ENABLE_WIFI){
#ifdef AP
    WiFi.beginAP(SSID, PASSWORD);
    while (WiFi.status() != WL_AP_LISTENING) {
      delay(500);
      Serial.print("Establishing connection to AP ...");
      Serial.println(WiFi.status());
    }
#else
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.println("Establishing connection to WiFi ...");
    }
#endif
    Serial.print("Arduino IP address on: ");
    Serial.println(WiFi.localIP());
    server.begin();
    currentStoredValues = printIPAddress(WiFi.localIP());
  }
  else
  {
    currentStoredValues = printToMemory(Letters::NODHCP, 33, 2);
  }
  prepareData();
  writeRegisters();
  delay(5000);
  clearMemory();
  clearRegisters();

  if (SD.exists("/startup.bin")){
    Serial.println("Loading startup.bin ...");
    File file = SD.open("/startup.bin", FILE_READ);
    if (file) {
      loadStartupFile(file);
      file.close();
    }
  }
  /*else if (SD.exists("/startup.set")){
    Serial.println("Loading startup.set as set ...");
    File file = SD.open("/startup.set", FILE_READ);
    if (file) {
      loadSet(file, false);
      file.close();
    }
  }*/
}

void loop() {
  digitalWrite(OE_PIN, LOW);
  readData();
  prepareData();

  if (currentSet != "" && displayDuration > 0) {
    displayTimeCounter++;
    if (displayTimeCounter >= displayDuration) {
      loadNextDisplay();
      displayTimeCounter = 0;
    }
  }
  else if (displayDuration < 0 && displayDuration >= displayTimeCounter) {
    loadNextDisplay();
    displayTimeCounter = 0;
  }
}

size_t printToMemory(const uint16_t* source, size_t arraySize, size_t offset) {
  size_t array1Length = arraySize;
  size_t i = 0;
  for (; i < array1Length; i++) {
    memory[i + offset] = source[i];
  }
  return offset + i;
}

size_t printIPAddress(IPAddress ip)
{
  size_t index = printToMemory(Letters::DHCP, 11, 2);
  index = printNumberToMemory(ip[0], index);
  index = printToMemory(Letters::_DOT, 2, index);
  index = printNumberToMemory(ip[1], index);
  index = printToMemory(Letters::_DOT, 2, index);
  index = printNumberToMemory(ip[2], index);
  index = printToMemory(Letters::_DOT, 2, index);
  index = printNumberToMemory(ip[3], index);
  return index;
}

size_t printNumberToMemory(uint8_t number, size_t index)
{
  if (number < 10) {
    index = printToMemory(Letters::getNumber(number), 4, index);
  }
  else if (number < 100) {
    index = printToMemory(Letters::getNumber(number / 10), 4, index);
    index = printToMemory(Letters::getNumber(number % 10), 4, index);
  }
  else {
    index = printToMemory(Letters::getNumber(number / 100), 4, index);
    index = printToMemory(Letters::getNumber(number % 100 / 10), 4, index);
    index = printToMemory(Letters::getNumber(number % 10), 4, index);
  }
  return index;
}

void readData()
{
  WiFiClient client = server.available();
  if (client)
  {
    Serial.println("New client connected");

    unsigned long connectionTimeout = millis();
    const unsigned long timeoutDuration = 2500;  // Timeout set to 5 seconds
    String requestString = "";

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        requestString += c;

        // Reset the timeout each time new data is received
        connectionTimeout = millis();

        if (requestString.endsWith("\r\n\r\n"))
        {
          break;
        }
      }

      // Check if timeout has been exceeded
      if (millis() - connectionTimeout > timeoutDuration)
      {
        Serial.println("Client connection timeout");
        break;
      }
    }

    if (requestString.length() > 0)
    {
      Serial.println("Request:");
      Serial.println(requestString);
      RequestParser parser;
      Request request = parser.parse(requestString);
      String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

      if (request.method == Method::GET)
      {
        if (request.path == "/index.html" || request.path == "/")
        {
          response += "<html><body><h1>Hello</h1></body></html>";
        }
        else if (request.path == "/getFiles.html")
        {
          response += handleListFilesPlain();
        }
        else if (request.path == "/files.html")
        {
          response += handleListFilesHtml();
        }
        else if (request.path == "/sets.html") {
          response += handleSetListRequest();
        }
        else
        {
          response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
          response += "<html><body><h1>404 Not Found</h1></body></html>";
        }
      }
      else if (request.method == Method::POST)
      {
        if (request.path == "/upload.html")
        {
          response = handleUploadRequest(requestString, client);
        }
        else if (request.path == "/file.html")
        {
          response = handleFileRequest(requestString, client);
        }
        else if (request.path == "/saveFile.html")
        {
          response = handleDataUploadRequest(requestString, client);
        }
        else if (request.path == "/set.html"){
          response = handleSetRequest(requestString, client);
        }
      }

      client.print(response);
    }
    delay(1);
    client.stop();
    Serial.println("Client disconnected");
  }
}

String handleUploadRequest(String request, WiFiClient client)
{
  int contentLengthIndex = request.indexOf("Content-Length:");
  if (contentLengthIndex == -1)
  {
    return "HTTP/1.1 411 Length Required\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Length Required: Content-Length header is missing</h1></body></html>";
  }

  int sIndex = contentLengthIndex + 16;
  int endIndex = request.indexOf("\r\n", sIndex);
  int contentLength = request.substring(sIndex, endIndex).toInt();

  if (contentLength == 0)
  {
    return "HTTP/1.1 411 Length Required\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Length Required: Content-Length is zero</h1></body></html>";
  }

  byte* postData = new byte[contentLength];
  if (client.read(postData, contentLength) != contentLength)
  {
    delete[] postData;
    return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Error reading data</h1></body></html>";
  }
  
  size_t i = 0;
  for (; i < contentLength / sizeof(uint16_t) && i < Settings::MAX_VALUES; i++) {
    memory[i] = *((uint16_t*)(postData + i * sizeof(uint16_t)));
  }
  currentStoredValues = i;
  saveStartupBin(memory, currentStoredValues);
  Serial.println(memory[0], HEX);
  startIndex = -Settings::MAX_SEND_SIZE;
  delete[] postData;
  return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
    "<html><body><h1>Received and saved ushort array</h1></body></html>";
}

String handleFileRequest(String request, WiFiClient client)
{
  int contentLengthIndex = request.indexOf("Content-Length:");
  if (contentLengthIndex == -1)
  {
    return "HTTP/1.1 411 Length Required\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Length Required: Content-Length header is missing</h1></body></html>";
  }

  int sIndex = contentLengthIndex + 16;
  int endIndex = request.indexOf("\r\n", sIndex);
  int contentLength = request.substring(sIndex, endIndex).toInt();
  
  if (contentLength == 0)
  {
    return "HTTP/1.1 411 Length Required\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Length Required: Content-Length is zero</h1></body></html>";
  }

  char* postData = new char[contentLength + 1];
  if (client.readBytes(postData, contentLength) != contentLength)
  {
    delete[] postData;
    return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Error reading data</h1></body></html>";
  }
  postData[contentLength] = '\0';  // Null-terminate the string

  String postDataStr = String(postData);
  delete[] postData;  // Free memory

  // Debugging: Print raw POST data
  Serial.print("Raw POST data: ");
  Serial.println(postDataStr);

  String fileName;

  // Check if the POST data contains the "fileName=" format
  int fileNameIndex = postDataStr.indexOf("fileName=");
  if (fileNameIndex != -1) {
    // Extract the filename from the "fileName=" part
    fileName = postDataStr.substring(fileNameIndex + 9);  // Skip "fileName="
    fileName.trim();  // Remove any leading/trailing whitespace
  } else {
    // If not in "fileName=" format, assume raw filename
    fileName = postDataStr;
    fileName.trim();  // Remove any leading/trailing whitespace
  }
  String filePath = String("/displays/") + fileName;
  Serial.print("File ");
  Serial.print(filePath);
  Serial.print(" exist? ");
  Serial.println(SD.exists(filePath) ? "True" : "False");
  File file = SD.open(filePath, FILE_READ);
  delete[] postData;

  if (!file)
  {
    return "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>File not found on SD card</h1></body></html>";
  }

  size_t i = 0;
  uint16_t value;
  while (file.available() && i < Settings::MAX_VALUES) {
    // Read two bytes from the file
    int lowByte = file.read();  // First byte (low byte)
    int highByte = file.read();   // Second byte (high byte)

    if (highByte == -1 || lowByte == -1) {
      break; // Stop if we can't read a complete uint16_t (two bytes)
    }

    // Construct uint16_t by combining highByte and lowByte
    memory[i++] = (uint16_t)((highByte << 8) | lowByte);
  }
  
  delete[] postData;
  
  if (i == 0)
  {
    return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Error reading file from SD card</h1></body></html>";
  }

  currentStoredValues = i;
  saveStartupBin(memory, currentStoredValues);
  Serial.print("Readed ");
  Serial.print(currentStoredValues);
  Serial.println(" values");
  file.close();
  
  startIndex = -Settings::MAX_SEND_SIZE;
  String output = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  output += "<html><body><h1>Readed and displayed ushort array ";
  output += fileName;
  output += "</h1></body></html>";
  return output;
}

String handleListFilesPlain() {
  // Open the "displays" directory
  File dir = SD.open("/displays");
  if (!dir || !dir.isDirectory()) {
    return "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
           "<html><body><h1>Displays directory not found</h1></body></html>";
  }

  // Prepare the HTTP response header
  String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  response += "<html><body><h1>List of .bin files in /displays directory</h1><ul>";

  // Iterate over all files in the directory
  File file = dir.openNextFile();
  while (file) {
    String fileName = file.name();

    Serial.print("Found file: ");
    Serial.println(fileName);

    // Check if the file has the ".bin" extension
    if (fileName.endsWith(".BIN")) {
      response += "<li>" + fileName + "</li>";
    }

    file.close();  // Close the current file
    file = dir.openNextFile();  // Move to the next file
  }

  response += "</ul></body></html>";

  dir.close();  // Close the directory

  return response;  // Return the HTML response
}

String handleListFilesHtml() {
  // Open the "displays" directory
  File dir = SD.open("/displays");
  if (!dir || !dir.isDirectory()) {
    return "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
           "<html><body><h1>Displays directory not found</h1></body></html>";
  }

  // Prepare the HTTP response header
  String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  response += "<html><body><h1>Select a .bin file to send as POST request</h1><ul>";

  // Iterate over all files in the directory
  File file = dir.openNextFile();
  while (file) {
    String fileName = file.name();

    // Check if the file has the ".bin" extension
    if (fileName.endsWith(".BIN")) {
      // Create a form with a POST request and the file name as data
      response += "<li>";
      response += "<form action=\"/file.html\" method=\"POST\" style=\"display:inline;\">";
      response += "<input type=\"hidden\" name=\"fileName\" value=\"" + fileName + "\">";
      response += "<button type=\"submit\">" + fileName + "</button>";
      response += "</form>";
      response += "</li>";
    }

    file.close();  // Close the current file
    file = dir.openNextFile();  // Move to the next file
  }

  response += "</ul></body></html>";

  dir.close();  // Close the directory

  return response;  // Return the HTML response
}

String handleDataUploadRequest(String request, WiFiClient client) {
  // Check for Content-Length header
  int contentLengthIndex = request.indexOf("Content-Length:");
  if (contentLengthIndex == -1) {
    return "HTTP/1.1 411 Length Required\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Length Required: Content-Length header is missing</h1></body></html>";
  }

  int sIndex = contentLengthIndex + 16;
  int endIndex = request.indexOf("\r\n", sIndex);
  int contentLength = request.substring(sIndex, endIndex).toInt();

  if (contentLength <= 0) {
    return "HTTP/1.1 411 Length Required\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Length Required: Content-Length is zero</h1></body></html>";
  }

  // Extract filename from the headers
  String fileName;
  int fileNameIndex = request.indexOf("fileName:");
  if (fileNameIndex != -1) {
    int startFIndex = fileNameIndex + 9;  // Skip "fileName="
    int endIndex = request.indexOf("\r\n", startFIndex);  // Look for end of header
    if (endIndex != -1) {
      fileName = request.substring(startFIndex, endIndex);
      fileName.trim();  // Clean up whitespace
    }
  }
  else {
    return "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Bad Request: Missing fileName in header</h1></body></html>";
  }

  // Debugging: Print the detected filename
  Serial.print("Detected filename: ");
  Serial.println(fileName);

  String filePath = String("/displays/") + fileName;
  Serial.print("File ");
  Serial.print(filePath);
  Serial.print(" exist? ");
  Serial.println(SD.exists(filePath) ? "True" : "False");

  File file = SD.open(filePath, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open/create file: " + fileName);
    return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Error opening file on SD card</h1></body></html>";
  }

  // Allocate buffer for reading POST data
  byte* postData = new byte[contentLength];
  if (client.read(postData, contentLength) != contentLength) {
    delete[] postData;
    file.close();
    Serial.println("Error reading data - mismatch length");
    return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Error reading data</h1></body></html>";
  }

  // Write the received binary data to the file
  file.write(postData, contentLength);  // Write the raw binary data
  file.close();  // Close the file
  delete[] postData;  // Free allocated memory

  return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
    "<html><body><h1>Data saved successfully</h1></body></html>";
}

String handleSetRequest(String request, WiFiClient client) {
  int contentLengthIndex = request.indexOf("Content-Length:");
  if (contentLengthIndex == -1)
  {
    return "HTTP/1.1 411 Length Required\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Length Required: Content-Length header is missing</h1></body></html>";
  }

  int sIndex = contentLengthIndex + 16;
  int endIndex = request.indexOf("\r\n", sIndex);
  int contentLength = request.substring(sIndex, endIndex).toInt();
  
  if (contentLength == 0)
  {
    return "HTTP/1.1 411 Length Required\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Length Required: Content-Length is zero</h1></body></html>";
  }

  char* postData = new char[contentLength + 1];
  if (client.readBytes(postData, contentLength) != contentLength)
  {
    delete[] postData;
    return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Error reading data</h1></body></html>";
  }
  postData[contentLength] = '\0';  // Null-terminate the string

  String postDataStr = String(postData);
  delete[] postData;  // Free memory

  // Debugging: Print raw POST data
  Serial.print("Raw POST data: ");
  Serial.println(postDataStr);

  // Check if the POST data contains the "setName=" format
  int fileNameIndex = postDataStr.indexOf("setName=");
  if (fileNameIndex != -1) {
    // Extract the filename from the "setName=" part
    currentSet = postDataStr.substring(fileNameIndex + 8);  // Skip "setName="
    currentSet.trim();
  }
  else {
    currentSet = postDataStr;
    currentSet.trim();  // Remove any leading/trailing whitespace
  }
  if (loadSet(currentSet)) {
    return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
      "<html><body><h1>Set loaded: " + currentSet + "</h1></body></html>";
  }
}

String handleSetListRequest() {
  // Open the "displays" directory
  File dir = SD.open("/sets");
  if (!dir || !dir.isDirectory()) {
    return "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
           "<html><body><h1>Sets directory not found</h1></body></html>";
  }

  // Prepare the HTTP response header
  String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  response += "<html><body><h1>Select a set</h1><ul>";

  // Iterate over all files in the directory
  File file = dir.openNextFile();
  while (file) {
    String fileName = file.name();

    // Check if the file has the ".bin" extension
    if (fileName.endsWith(".SET")) {
      // Create a form with a POST request and the file name as data
      response += "<li>";
      response += "<form action=\"/set.html\" method=\"POST\" style=\"display:inline;\">";
      response += "<input type=\"hidden\" name=\"setName\" value=\"" + fileName + "\">";
      response += "<button type=\"submit\">" + fileName + "</button>";
      response += "</form>";
      response += "</li>";
    }

    file.close();  // Close the current file
    file = dir.openNextFile();  // Move to the next file
  }

  response += "</ul></body></html>";

  dir.close();  // Close the directory

  return response;  // Return the HTML response
}

void prepareData() {
  if (currentStoredValues == 0) return;
  display[0] = 0;
  display[1] = 0;
  display[112] = 0;
  if (currentStoredValues <= Settings::MAX_SEND_SIZE && !settings.ALWAYS_SCROLL) {
    for (int i = 0; i < Settings::MAX_SEND_SIZE - 3; i++)
    {
      if (i + 2 < currentStoredValues)
      {
        display[i + 2] = memory[i];
      }
      else
      {
        display[i + 2] = 0x0000;
      }
    }
  }
  else {
    for (int i = 0; i < Settings::MAX_SEND_SIZE - 3; i++) {
      int calcIndex = (startIndex + i);
      if (calcIndex < 0 || calcIndex >= currentStoredValues) {
        display[i + 2] = 0;
      }
      else {
        display[i + 2] = memory[calcIndex];
      }
    }
    if (currentDelay >= settings.SCROLL_DELAY) {
      startIndex++;
      Serial.println(startIndex++);
      currentDelay = 0;
      if (startIndex > currentStoredValues) {
        startIndex = -Settings::MAX_SEND_SIZE;//startPosition;
        displayTimeCounter--;
      }
    }
    else {
      currentDelay++;
    }
  }
  writeRegisters();
  delay(settings.DISPLAY_TIME);
}

void clearMemory()
{
  for (int i = 0; i < Settings::MAX_VALUES; i++) {
    memory[i] = 0x0000;
  }
}

void clearRegisters()
{
  noInterrupts();
  digitalWrite(OE_PIN, HIGH);
  digitalWrite(RCLK_PIN, LOW);

  for (int i = 14 * 16 + 15; i >= 0; i--)
  {
    digitalWrite(SRCLK_PIN, HIGH);
    for (int reg = 0; reg < 8; reg++) {
      digitalWrite(registerPins[reg], LOW);
    }
    digitalWrite(SRCLK_PIN, LOW);
  }

  digitalWrite(RCLK_PIN, HIGH);
  digitalWrite(OE_PIN, LOW);
  interrupts();
}

void writeRegisters()
{
  noInterrupts();
  digitalWrite(RCLK_PIN, LOW);
  digitalWrite(OE_PIN, HIGH);

  for (int i = 14 * 16 + 15; i >= 0; i--)
  {
    int index = i / 16;
    int bit = i % 16;
    digitalWrite(SRCLK_PIN, HIGH);
    for (int reg = 0; reg < 8; reg++) {
      digitalWrite(registerPins[reg], (display[index * 8 + reg] & (1 << (index % 2 == 1 ? bit : 15 - bit))) != 0);
    }
    digitalWrite(SRCLK_PIN, LOW);
  }

  digitalWrite(RCLK_PIN, HIGH);
  digitalWrite(OE_PIN, LOW);
  interrupts();
}

bool loadSet(File file) {
  return loadSet(file, true);
}

bool loadSet(File file, bool save) 
{
  Serial.println("loadSet() ...");
  String fileContent = "";
  int index = 0;
  while (file.available()) {
    if (index > 20) {
      break;
    }
    String line = file.readStringUntil('\n');
    line.trim();
    Serial.println(line);
    int separatorIndex = line.indexOf(';');
    if (separatorIndex != -1) {
      displayQueue[index].fileName = line.substring(0, separatorIndex);
      displayQueue[index].displayTime = line.substring(separatorIndex + 1).toInt() * 100;
      index++;
    }
    fileContent += line + '\n';
    
  }
  Serial.print("Loaded sets: ");
  Serial.println(index);
  Serial.println(fileContent);
  file.close();
  if (save)
  {
    saveStartupSet(fileContent);
  }
  currentDisplayIndex - 0;
  loadNextDisplay();
  Serial.println("exiting loadSet() ...");
  return true;
}

bool loadSet(String setFile){
  String setPath = "/sets/" + setFile;
  File file = SD.open(setPath);
  if (!file) {
    Serial.println("Failed to open set file: " + setPath);
    return false;
  }
  return loadSet(file);
}

void loadNextDisplay() {
  if (currentDisplayIndex >= 20) {
    currentDisplayIndex = 0;
  }
  String displayFile = "/displays/" + displayQueue[currentDisplayIndex].fileName;
  if (SD.exists(displayFile)) {
    File file = SD.open(displayFile, FILE_READ);
    if (file) {
      size_t i = 0;
      uint16_t value;
      while (file.available() && i < Settings::MAX_VALUES) {
        int lowByte = file.read();
        int highByte = file.read();
        if (highByte == -1 || lowByte == -1) {
          break;
        }
        memory[i] = (uint16_t)((highByte << 8) | lowByte);
      }
      currentStoredValues = i;
      file.close();
      startIndex = -Settings::MAX_SEND_SIZE;
      displayDuration = displayQueue[currentDisplayIndex].displayTime;
      currentDisplayIndex++;
    }
  }
}

void saveStartupBin(const uint16_t* data, int size) {
  File file = SD.open("/startup.bin", FILE_WRITE);
  if (file) {
    for (int i = 0; i < size; i++) {
      uint16_t value = data[i];
      byte lowByte = value & 0xFF;
      byte highByte = (value >> 8) & 0xff;
      file.write(highByte);
      file.write(lowByte);
    }
    file.close();
    if (SD.exists("/startup.set")) {
      SD.remove("/startup.set");
    }
  }
  else {
    Serial.println("Failed to open startup.bin for writing");
  }
}

void saveStartupSet(String data) {
  File file = SD.open("/startup.set", FILE_WRITE);
  if (file) {
    file.println(data);
    file.close();
    if (SD.exists("/startup.bin")){
      SD.remove("/startup.bin");
    }
  }
  else {
    Serial.println("Failed to open startup.set for writing");
  }
}

void loadStartupFile(File file) {
  int i = 0;
  while (file.available() && i < Settings::MAX_VALUES) {
    int highByte = file.read();
    int lowByte = file.read();
    if (highByte == -1 || lowByte == -1) {
      break;
    }
    memory[i++] = (uint16_t)((highByte << 8) | lowByte);
  }
  currentStoredValues = i;
  startIndex = -Settings::MAX_SEND_SIZE;
  writeRegisters();
}












