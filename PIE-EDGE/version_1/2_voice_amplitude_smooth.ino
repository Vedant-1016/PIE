#include <PDM.h>

#define WINDOW_SIZE 5

int ampBuffer[WINDOW_SIZE];
int buffer_index = 0;
bool bufferFilled = false;

short sampleBuffer[256];
volatile int samplesRead;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

void setup() {
  Serial.begin(9600);
  PDM.onReceive(onPDMdata);

  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }
}

void loop() {
  if (samplesRead) {
    long sum = 0;

    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }

    int amplitude = sum / samplesRead;

    ampBuffer[buffer_index] = amplitude;
    buffer_index = (buffer_index + 1) % WINDOW_SIZE;

    if (buffer_index == 0) bufferFilled = true;

    int count = bufferFilled ? WINDOW_SIZE : buffer_index;
    long avgSum = 0;

    for (int i = 0; i < count; i++) {
      avgSum += ampBuffer[i];
    }

    int smoothAmp = avgSum / count;

    String energy;
     if (smoothAmp < 300 ) {
      energy = "LOW";
    } else if (smoothAmp < 500) {
      energy = "MEDIUM";
    } else {
      energy = "HIGH";
    }
    Serial.print("Raw: ");
    Serial.print(amplitude);
    Serial.print(" | Smooth: ");
    Serial.print(smoothAmp);
    Serial.print(" → Energy: ");
    Serial.println(energy);
    

    samplesRead = 0;
  }
}
