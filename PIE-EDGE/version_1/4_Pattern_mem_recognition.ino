
#include <PDM.h>

//------State Variables---------
int stateDuration = 0;
String lastState = "";
String stableState = "";

//------Pattern Memory--------
#define PATTERN_SIZE 50

String patternMemory[PATTERN_SIZE];
int patternIndex = 0;
bool patternFilled = false;

int slowCounter = 0;

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

    // -------- States Stability --------
    
    if(finalState == lastState)
    {
      stateDuration++;
    }
    else{
      stateDuration = 1; //reset
      lastState = finalState;
    }

    float confidence = 0;

  if (finalState == "LOW_STATE") {
  confidence = (float)countLow / memCount;
  }
  else if (finalState == "HIGH_STATE") {
  confidence = (float)countHigh / memCount;
  }
  else {
  confidence = 0.5;
  }

    // -------- Convert to Time --------
  float stateTime = stateDuration * 0.016;

  // -------- Burst vs Sustained --------
  if (finalState == "HIGH_STATE") {
    if (stateTime < 0.3) {
      finalState = "BURST";
    } else if (stateTime > 0.8) {
      finalState = "SUSTAINED_HIGH";
    }
  }
  if (finalState == "LOW_STATE") {
    if (stateTime < 0.3) {
        finalState = "SHORT_LOW";
    }
    else if (stateTime > 1.0) {
        finalState = "SUSTAINED_LOW";
    }
}
   // -------- Inertia (Stability Filter) --------
  if (stateDuration > 5) {
    stableState = finalState;
  }

  //-----------Patten Memory (over long time)-----------
  slowCounter++;

if (slowCounter >= 10) {   // every ~160 ms therefor 50*160ms = 8 sec. therefore here we have values till 8sec
    slowCounter = 0;

    patternMemory[patternIndex] = stableState;
    patternIndex = (patternIndex + 1) % PATTERN_SIZE;

    if (patternIndex == 0) patternFilled = true;
}

int longLow = 0, longHigh = 0, longBurst = 0;

int pCount = patternFilled ? PATTERN_SIZE : patternIndex;

for (int i = 0; i < pCount; i++) {
    if (patternMemory[i] == "SUSTAINED_LOW") longLow++;
    else if (patternMemory[i] == "SUSTAINED_HIGH") longHigh++;
    else if (patternMemory[i] == "BURST") longBurst++;
}

String behavior;

if (longLow > longHigh && longLow > longBurst) {
    behavior = "INACTIVE";
}
else if (longHigh > longLow) {
    behavior = "ACTIVE";
}
else if (longBurst > longHigh) {
    behavior = "IRREGULAR";
}
else {
    behavior = "MIXED";
}

  // -------- OUTPUT --------
  Serial.print("Amp: ");
  Serial.print(smoothAmp);
  Serial.print(" | State: ");
  Serial.print(finalState);

  Serial.print(" | Duration: ");
  Serial.print(stateDuration);
  Serial.print(" | Stable: ");
  Serial.print(stableState);

  Serial.print(" | Time: ");
  Serial.println(stateTime);

  Serial.print(" | Confidence: ");
  Serial.println(confidence);

  Serial.print("Behavior: ");
  Serial.println(behavior);
    samplesRead = 0;
  }
}
