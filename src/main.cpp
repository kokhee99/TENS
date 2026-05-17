/**
 * TENS Device - Biphasic Square Wave Generator with H-Bridge Control
 * 
 * ESP32S3 based TENS stimulator
 * - Reads potentiometer on A0 for frequency control (2Hz - 150Hz)
 * - Generates complementary pulse signals for H-bridge driver
 * - Creates biphasic square wave output
 */

#include <Arduino.h>

// Pin definitions (Seeed XIAO ESP32S3)
#define POT_PIN A0              // Analog input for frequency control
#define PHASE_A_PIN 10           // H-bridge Phase A signal
#define PHASE_B_PIN 9           // H-bridge Phase B signal (complementary)

// Frequency constants (Hz)
#define MIN_FREQUENCY 2         // Minimum frequency
#define MAX_FREQUENCY 150       // Maximum frequency

// ADC resolution
#define ADC_MAX 4095            // 12-bit ADC on ESP32S3

// Timing variables
unsigned long lastSwitchTime = 0;
unsigned long phaseDuration = 0;  // Duration of each phase in microseconds
bool phaseA_Active = true;        // Track which phase is active

/**
 * Map potentiometer reading to frequency
 * @param potValue ADC reading (0-4095)
 * @return Frequency in Hz (2-150)
 */
float mapFrequency(int potValue) {
  // Linear mapping from pot reading to frequency
  float frequency = MIN_FREQUENCY + (potValue / (float)ADC_MAX) * 
                    (MAX_FREQUENCY - MIN_FREQUENCY);
  return frequency;
}

/**
 * Calculate phase duration in microseconds
 * @param frequency Desired frequency in Hz
 * @return Duration of half period in microseconds
 */
unsigned long calculatePhaseDuration(float frequency) {
  // Period = 1/frequency (in seconds)
  // Half period = 1/(2*frequency) (in seconds) 
  // Convert to microseconds: multiply by 1,000,000
  unsigned long halfPeriod = (unsigned long)(500000.0 / frequency);
  return halfPeriod;
}

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n=== TENS Biphasic Waveform Generator ===");
  Serial.println("Reading potentiometer on A0");
  Serial.println("Frequency range: 2Hz - 150Hz");
  Serial.println("Phase A: GPIO 8");
  Serial.println("Phase B: GPIO 9");
  
  // Configure GPIO pins as outputs
  pinMode(PHASE_A_PIN, OUTPUT);
  pinMode(PHASE_B_PIN, OUTPUT);
  
  // Initialize outputs to LOW
  digitalWrite(PHASE_A_PIN, LOW);
  digitalWrite(PHASE_B_PIN, LOW);
  
  // Configure potentiometer pin as analog input
  pinMode(POT_PIN, INPUT);
  
  // Initialize timing
  lastSwitchTime = micros();
}

void loop() {
  // Read potentiometer value
  int potValue = analogRead(POT_PIN);
  
  // Convert to frequency (2Hz - 150Hz)
  float frequency = mapFrequency(potValue);
  
  // Calculate required phase duration
  unsigned long newPhaseDuration = calculatePhaseDuration(frequency);
  
  // Update phase duration for frequency changes
  phaseDuration = newPhaseDuration;
  
  // Get current time in microseconds
  unsigned long currentTime = micros();
  unsigned long elapsedTime = currentTime - lastSwitchTime;
  
  // Check if it's time to switch phase
  if (elapsedTime >= phaseDuration) {
    lastSwitchTime = currentTime;
    
    // Toggle phases - complementary output
    if (phaseA_Active) {
      // Phase A active: A = HIGH, B = LOW (forward bias)
      digitalWrite(PHASE_A_PIN, HIGH);
      digitalWrite(PHASE_B_PIN, LOW);
    } else {
      // Phase B active: A = LOW, B = HIGH (reverse bias - biphasic)
      digitalWrite(PHASE_A_PIN, LOW);
      digitalWrite(PHASE_B_PIN, HIGH);
    }
    
    // Toggle to next phase
    phaseA_Active = !phaseA_Active;
    
    // Debug output every ~1 second
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime >= 1000000) {
      lastDebugTime = currentTime;
      Serial.print("Pot: ");
      Serial.print(potValue);
      Serial.print(" | Freq: ");
      Serial.print(frequency, 1);
      Serial.print(" Hz | Period: ");
      Serial.print(phaseDuration * 2 / 1000.0, 2);
      Serial.println(" ms");
    }
  }
}

/**
 * CIRCUIT CONNECTION GUIDE:
 * 
 * H-BRIDGE DRIVER (e.g., L298N or DRV8833):
 * 
 *   PHASE_A_PIN (GPIO 10) -----> H-Bridge Input A (IN1)
 *   PHASE_B_PIN (GPIO 9) -----> H-Bridge Input B (IN2)
 *   
 *   H-Bridge Output:
 *   OUT1 (+) -----> Load+
 *   OUT2 (-) -----> Load-
 * 
 * POTENTIOMETER:
 *   VCC (3.3V) -----> One end of potentiometer
 *   GND        -----> Other end of potentiometer
 *   Wiper      -----> A0 (GPIO 3)
 * 
 * SIGNAL BEHAVIOR:
 * - Phase 1: A=HIGH, B=LOW  -> Load forward bias
 * - Phase 2: A=LOW, B=HIGH  -> Load reverse bias (biphasic)
 * - Frequency controlled by potentiometer (2Hz-150Hz)
 * - Each phase lasts 1/(2*frequency) seconds
 */
