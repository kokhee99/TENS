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
#define FREQUENCY_POT_PIN A0    // Analog input for frequency control
#define DUTY_POT_PIN A1         // Optional analog input for PWM duty cycle control
#define PHASE_A_PIN D10         // H-bridge Phase A signal
#define PHASE_B_PIN D9          // H-bridge Phase B signal (complementary)

// Frequency constants (Hz)
#define MIN_FREQUENCY 2         // Minimum frequency
#define MAX_FREQUENCY 150       // Maximum frequency

// PWM carrier settings for phase output
#define PWM_CARRIER_FREQ 20000  // PWM carrier frequency for H-bridge outputs
#define PWM_RESOLUTION 8        // 8-bit PWM resolution
#define PWM_MAX_VALUE ((1 << PWM_RESOLUTION) - 1)
#define MIN_DUTY_PERCENT 10     // Minimum pulse width percent
#define MAX_DUTY_PERCENT 90     // Maximum pulse width percent

// ADC resolution
#define ADC_MAX 4095            // 12-bit ADC on ESP32S3

// Timing variables
unsigned long lastSwitchTime = 0;
unsigned long phaseDuration = 0;  // Duration of each phase in microseconds
bool phaseA_Active = true;        // Track which phase is active

// PWM channels
const int PWM_CHANNEL_A = 0;
const int PWM_CHANNEL_B = 1;

// Current duty control
int dutyCyclePercent = 50;

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

/**
 * Map potentiometer reading to PWM duty cycle percent
 */
int mapDutyCycle(int potValue) {
  float duty = MIN_DUTY_PERCENT + (potValue / (float)ADC_MAX) *
               (MAX_DUTY_PERCENT - MIN_DUTY_PERCENT);
  return constrain((int)duty, MIN_DUTY_PERCENT, MAX_DUTY_PERCENT);
}

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n=== TENS Biphasic Waveform Generator ===");
  Serial.println("Reading potentiometer on A0");
  Serial.println("Frequency range: 2Hz - 150Hz");
  Serial.println("Phase A: GPIO D10");
  Serial.println("Phase B: GPIO D9");
  Serial.println("PWM carrier frequency: 20 kHz");
  Serial.println("Duty cycle control pin: A1 (optional)");
  
  // Configure PWM outputs for biphasic H-bridge control
  ledcSetup(PWM_CHANNEL_A, PWM_CARRIER_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PHASE_A_PIN, PWM_CHANNEL_A);
  ledcSetup(PWM_CHANNEL_B, PWM_CARRIER_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PHASE_B_PIN, PWM_CHANNEL_B);

  // Initialize outputs to zero duty
  ledcWrite(PWM_CHANNEL_A, 0);
  ledcWrite(PWM_CHANNEL_B, 0);
  
  // Configure potentiometer pins as analog inputs
  pinMode(POT_PIN, INPUT);
  pinMode(DUTY_POT_PIN, INPUT_PULLDOWN);
  
  // Initialize timing
  lastSwitchTime = micros();
}

void loop() {
  // Read potentiometer value for frequency control
  int potValue = analogRead(POT_PIN);
  float frequency = mapFrequency(potValue);
  unsigned long newPhaseDuration = calculatePhaseDuration(frequency);
  phaseDuration = newPhaseDuration;

  // Read optional duty control potentiometer
  int dutyPotValue = analogRead(DUTY_POT_PIN);
  dutyCyclePercent = mapDutyCycle(dutyPotValue);
  int pwmDuty = map(dutyCyclePercent, 0, 100, 0, PWM_MAX_VALUE);

  // Get current time in microseconds
  unsigned long currentTime = micros();
  unsigned long elapsedTime = currentTime - lastSwitchTime;

  // Check if it's time to switch phase
  if (elapsedTime >= phaseDuration) {
    lastSwitchTime = currentTime;

    // Toggle phases - complementary PWM output
    if (phaseA_Active) {
      // Phase A active: PWM on A, B = 0
      ledcWrite(PWM_CHANNEL_A, pwmDuty);
      ledcWrite(PWM_CHANNEL_B, 0);
    } else {
      // Phase B active: PWM on B, A = 0
      ledcWrite(PWM_CHANNEL_A, 0);
      ledcWrite(PWM_CHANNEL_B, pwmDuty);
    }

    // Toggle to next phase
    phaseA_Active = !phaseA_Active;

    // Debug output every ~1 second
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime >= 1000000) {
      lastDebugTime = currentTime;
      Serial.print("Pot Freq: ");
      Serial.print(potValue);
      Serial.print(" | Freq: ");
      Serial.print(frequency, 1);
      Serial.print(" Hz | Duty: ");
      Serial.print(dutyCyclePercent);
      Serial.print("% | Period: ");
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
 *   DRV8833 H-Bridge Output:
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
