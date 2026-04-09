
#include <PDM.h>

//--------Variable for State Transition--------
String lastBehavior = "";

//-------Mean And Var Init----------
float meanAmp = 100;
float varAmp = 1000;
float alpha = 0.001;   // learning rate

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
 // -------- Recursive approach (mean & var) --------

// Step 1: compute diff using OLD mean
float diff = smoothAmp - meanAmp;

  // Step 2: update variance first
  varAmp = (1 - alpha) * varAmp + alpha * diff * diff;

  // Step 3: compute std
  float stdAmp = sqrt(varAmp);

  // clamp std
  if (stdAmp > 200) stdAmp = 200;

  // Step 4: selective mean update (NOW std is valid)
  if (smoothAmp < meanAmp + stdAmp) {
      meanAmp = (1 - alpha) * meanAmp + alpha * smoothAmp;
}
  //-------------Normalized Behaviour detection-----------
  float zScore = (smoothAmp - meanAmp) / stdAmp;

      //z-score anamoly detection
      String anomaly;

      if (zScore > 3) {
          anomaly = "EXTREME";
      }
      else if (zScore > 2) {
          anomaly = "HIGH_ANOMALY";
      }
      else if (zScore < -2) {
          anomaly = "LOW_ANOMALY";
      }
      else {
          anomaly = "NORMAL";
      }

    // -------- Energy Classification --------
    String energy;

  if (smoothAmp < meanAmp + 0.5 * stdAmp) {
    energy = "LOW";
  }
  else if (smoothAmp < meanAmp + 1.2 * stdAmp) {
      energy = "MEDIUM";
  }
  else {
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

  int longLow = 0, longHigh = 0, longBurst = 0 , longMedium = 0;

  int pCount = patternFilled ? PATTERN_SIZE : patternIndex;

  for (int i = 0; i < pCount; i++) {
      if (patternMemory[i] == "SUSTAINED_LOW") longLow++;
      else if (patternMemory[i] == "SUSTAINED_HIGH") longHigh++;
      else if (patternMemory[i] == "MEDIUM") longMedium++;
      else if (patternMemory[i] == "BURST") longBurst++;
  }

  String behavior;

  if (longLow > longHigh && longLow > longMedium) {
      behavior = "INACTIVE";
  }
  else if (longHigh  + longMedium > longLow) {
      behavior = "ACTIVE";
  }
  else if (longBurst > longHigh) {
      behavior = "IRREGULAR";
  }
  else {
      behavior = "MIXED";
  }

//--------------State Transitions----------
String transition = "NONE";

if (lastBehavior == "") {
    lastBehavior = behavior;
}
else if (behavior != lastBehavior) {

    bool wasActive = (lastBehavior == "ACTIVE" || lastBehavior == "MIXED");
    bool isActive  = (behavior == "ACTIVE"  || behavior == "MIXED");

    bool wasIrregular = (lastBehavior == "IRREGULAR");
    bool isIrregularNow = (behavior == "IRREGULAR");

    if (!wasActive && isActive) {
        transition = "ACTIVATION";
    }
    else if (wasActive && !isActive) {
        transition = "DEACTIVATION";
    }
    else if (!wasIrregular && isIrregularNow) {
        transition = "DISTURBANCE";
    }
    else if (wasIrregular && !isIrregularNow) {
        transition = "RECOVERY";
    }
    else {
        transition = "CHANGE";
    }

    lastBehavior = behavior;
}

  // -------- OUTPUT --------
Serial.print(smoothAmp);
Serial.print(",");
Serial.print(meanAmp);
Serial.print(",");
Serial.print(stdAmp);
Serial.print(",");
Serial.print(zScore);
Serial.print(",");
Serial.print(behavior);
Serial.print(",");
Serial.println(transition);   // ONLY println at end
  // Serial.print("Amp: ");
  // Serial.print(smoothAmp);
  // Serial.print(" | State: ");
  // Serial.print(finalState);

  // Serial.print(" | Duration: ");
  // Serial.print(stateDuration);
  // Serial.print(" | Stable: ");
  // Serial.print(stableState);

  // Serial.print(" | Time: ");
  // Serial.println(stateTime);

  // Serial.print(" | Confidence: ");
  // Serial.println(confidence);

  
  // Serial.print(" | anamoly: ");
  // Serial.println(anomaly);

  // Serial.print("Behavior: ");
  // Serial.println(behavior);
    samplesRead = 0;
  }
}
