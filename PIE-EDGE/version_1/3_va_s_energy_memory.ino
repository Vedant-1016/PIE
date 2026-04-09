// #include <PDM.h>

// #define WINDOW_SIZE 5
// #define MEMORY_SIZE 10

// String energyMemory[MEMORY_SIZE];
// int memoryIndex = 0;
// bool memoryFilled = false;


// int ampBuffer[WINDOW_SIZE];
// int buffer_index = 0;
// bool bufferFilled = false;

// short sampleBuffer[256];
// volatile int samplesRead;

// void onPDMdata() {
//   int bytesAvailable = PDM.available();
//   PDM.read(sampleBuffer, bytesAvailable);
//   samplesRead = bytesAvailable / 2;
// }

// void setup() {
//   Serial.begin(9600);
//   PDM.onReceive(onPDMdata);

//   if (!PDM.begin(1, 16000)) {
//     Serial.println("Failed to start PDM!");
//     while (1);
//   }
// }

// void loop() {
//   if (samplesRead) {
//     long sum = 0;

//     for (int i = 0; i < samplesRead; i++) {
//       sum += abs(sampleBuffer[i]);
//     }

//     int amplitude = sum / samplesRead;

//     ampBuffer[buffer_index] = amplitude;
//     buffer_index = (buffer_index + 1) % WINDOW_SIZE;

//     if (buffer_index == 0) bufferFilled = true;

//     int count = bufferFilled ? WINDOW_SIZE : buffer_index;
//     long avgSum = 0;

//     for (int i = 0; i < count; i++) {
//       avgSum += ampBuffer[i];
//     }

//     int smoothAmp = avgSum / count;

//     String energy;
//      if (smoothAmp < 300 ) {
//       energy = "LOW";
//     } else if (smoothAmp < 500) {
//       energy = "MEDIUM";
//     } else {
//       energy = "HIGH";
//     }
//     Serial.print("Raw: ");
//     Serial.print(amplitude);
//     Serial.print(" | Smooth: ");
//     Serial.print(smoothAmp);
//     Serial.print(" → Energy: ");
//     Serial.println(energy);
    

//     samplesRead = 0;
//   }
// }
#include <PDM.h>

// ===== Mic Buffer =====
short sampleBuffer[256];
volatile int samplesRead;

// ===== Smoothing (Moving Average) =====
#define WINDOW_SIZE 5
int ampBuffer[WINDOW_SIZE];
int bufferIndex = 0;
bool bufferFilled = false;

// ===== Memory (State over time) =====
#define MEMORY_SIZE 40
String energyMemory[MEMORY_SIZE];
int memoryIndex = 0;
bool memoryFilled = false;

// ===== PDM Callback =====
void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

// ===== Setup =====
void setup() {
  Serial.begin(9600);
  PDM.onReceive(onPDMdata);

  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }
}

// ===== Main Loop =====
void loop() {
  if (samplesRead) {

    // -------- Raw Amplitude --------
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }
    int amplitude = sum / samplesRead;

    // -------- Moving Average --------
    ampBuffer[bufferIndex] = amplitude;
    bufferIndex = (bufferIndex + 1) % WINDOW_SIZE;

    if (bufferIndex == 0) bufferFilled = true;

    int count = bufferFilled ? WINDOW_SIZE : bufferIndex;

    long avgSum = 0;
    for (int i = 0; i < count; i++) {
      avgSum += ampBuffer[i];
    }

    int smoothAmp = avgSum / count;

    // -------- Energy Classification --------
    String energy;

    if (smoothAmp < 150) {
      energy = "LOW";
    } else if (smoothAmp < 350) {
      energy = "MEDIUM";
    } else {
      energy = "HIGH";
    }

    // -------- Memory Storage --------
    energyMemory[memoryIndex] = energy;
    memoryIndex = (memoryIndex + 1) % MEMORY_SIZE;

    if (memoryIndex == 0) memoryFilled = true;

    // -------- Memory Analysis --------
    int countLow = 0, countMedium = 0, countHigh = 0;

    int memCount = memoryFilled ? MEMORY_SIZE : memoryIndex;

    for (int i = 0; i < memCount; i++) {
      if (energyMemory[i] == "LOW") countLow++;
      else if (energyMemory[i] == "MEDIUM") countMedium++;
      else if (energyMemory[i] == "HIGH") countHigh++;
    }

    // -------- Final State --------
    String finalState;

    if (countLow > countMedium && countLow > countHigh) {
      finalState = "LOW_STATE";
    } else if (countHigh > countLow && countHigh > countMedium) {
      finalState = "HIGH_STATE";
    } else {
      finalState = "TRANSITION";
    }

    // -------- Output (IMPORTANT for Python) --------
    Serial.println(smoothAmp);
    // If you want to debug:
    Serial.println(finalState);

    samplesRead = 0;
  }
}
