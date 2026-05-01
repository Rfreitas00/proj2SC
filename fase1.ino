// ─── NOTES ────────────────────────
//  delay() = para o programa por X segundos (blocking)
//  millis() = espera X segundos mas continua o loop (non-blocking)
//
//  problemas com o jitter -> GND comum entre arduino e servos
// ──────────────────────────────────


#include <Servo.h>

// ─── Pin assignments (from project datasheet) ───────────────────────────────
#define PIN_BASE      5
#define PIN_SHOULDER  6
#define PIN_ELBOW     3

// ─── Safe movement limits (adjust after physical testing) ───────────────────
// WARNING: test with small ranges first before expanding these values
#define BASE_MIN      20
#define BASE_MAX      160
#define SHOULDER_MIN  30
#define SHOULDER_MAX  150
#define ELBOW_MIN     30
#define ELBOW_MAX     160

// ─── Homing positions (arm in safe upright resting pose) ────────────────────
// Shoulder up, elbow folded back — keeps arm away from board on startup
#define BASE_HOME     90
#define SHOULDER_HOME 90
#define ELBOW_HOME    90

// ─── Movement speed ─────────────────────────────────────────────────────────
#define STEP_INTERVAL_MS  15   // ms between each 1° step (~67°/s)

// ─── Servo objects ──────────────────────────────────────────────────────────
Servo servoBase;
Servo servoShoulder;
Servo servoElbow;

// ─── Current and target positions ───────────────────────────────────────────
int posBase     = BASE_HOME;
int posShoulder = SHOULDER_HOME;
int posElbow    = ELBOW_HOME;

int targetBase     = BASE_HOME;
int targetShoulder = SHOULDER_HOME;
int targetElbow    = ELBOW_HOME;

unsigned long lastStepTime = 0;
bool isMoving = false;

// ─── Helpers ────────────────────────────────────────────────────────────────
int clamp(int val, int minVal, int maxVal) {
  return max(minVal, min(maxVal, val));
}

int stepToward(int current, int target) {
  if (current < target) return current + 1;
  if (current > target) return current - 1;
  return current;
}

void setTargets(int base, int shoulder, int elbow) {
  targetBase     = clamp(base,     BASE_MIN,     BASE_MAX);
  targetShoulder = clamp(shoulder, SHOULDER_MIN, SHOULDER_MAX);
  targetElbow    = clamp(elbow,    ELBOW_MIN,    ELBOW_MAX);
  isMoving = true;
}

bool allAtTarget() {
  return posBase     == targetBase &&
         posShoulder == targetShoulder &&
         posElbow    == targetElbow;
}

// ─── Non-blocking servo update (call every loop) ────────────────────────────
void updateServos() {
  if (!isMoving) return;
  if (millis() - lastStepTime < STEP_INTERVAL_MS) return;

  lastStepTime = millis();

  posBase     = stepToward(posBase,     targetBase);
  posShoulder = stepToward(posShoulder, targetShoulder);
  posElbow    = stepToward(posElbow,    targetElbow);

  servoBase.write(posBase);
  servoShoulder.write(posShoulder);
  servoElbow.write(posElbow);

  if (allAtTarget()) {
    isMoving = false;
    Serial.println(">> Movimento concluido.");
    printStatus();
    printPrompt();
  }
}

// ─── Homing ─────────────────────────────────────────────────────────────────
void doHoming() {
  Serial.println(">> A executar homing...");
  setTargets(BASE_HOME, SHOULDER_HOME, ELBOW_HOME);
}

// ─── Status ─────────────────────────────────────────────────────────────────
void printStatus() {
  Serial.print("   Base: ");     Serial.print(posBase);
  Serial.print("  Shoulder: "); Serial.print(posShoulder);
  Serial.print("  Elbow: ");    Serial.println(posElbow);
}

void printPrompt() {
  Serial.println();
  Serial.println("Comandos:");
  Serial.println("  h          -> homing");
  Serial.println("  b <angulo> -> mover base");
  Serial.println("  s <angulo> -> mover shoulder");
  Serial.println("  e <angulo> -> mover elbow");
  Serial.println("  p          -> mostrar posicao atual");
  Serial.println("  t <b> <s> <e> -> mover os 3 servos em simultaneo");
  Serial.print("> ");
}

// ─── Serial command parser ───────────────────────────────────────────────────
void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  char type = cmd.charAt(0);

  if (type == 'h') {
    doHoming();

  } else if (type == 'p') {
    printStatus();
    printPrompt();

  } else if (type == 'b' && cmd.length() > 2) {
    int angle = clamp(cmd.substring(2).toInt(), BASE_MIN, BASE_MAX);
    Serial.print(">> Base -> "); Serial.println(angle);
    setTargets(angle, posShoulder, posElbow);

  } else if (type == 's' && cmd.length() > 2) {
    int angle = clamp(cmd.substring(2).toInt(), SHOULDER_MIN, SHOULDER_MAX);
    Serial.print(">> Shoulder -> "); Serial.println(angle);
    setTargets(posBase, angle, posElbow);

  } else if (type == 'e' && cmd.length() > 2) {
    int angle = clamp(cmd.substring(2).toInt(), ELBOW_MIN, ELBOW_MAX);
    Serial.print(">> Elbow -> "); Serial.println(angle);
    setTargets(posBase, posShoulder, angle);

  } else if (type == 't' && cmd.length() > 2) {
    // format: t <base> <shoulder> <elbow>
    int i1 = cmd.indexOf(' ', 2);
    int i2 = cmd.indexOf(' ', i1 + 1);
    if (i1 > 0 && i2 > 0) {
      int b = cmd.substring(2, i1).toInt();
      int s = cmd.substring(i1 + 1, i2).toInt();
      int e = cmd.substring(i2 + 1).toInt();
      Serial.print(">> Mover para: b="); Serial.print(b);
      Serial.print(" s="); Serial.print(s);
      Serial.print(" e="); Serial.println(e);
      setTargets(b, s, e);
    } else {
      Serial.println("!! Formato: t <base> <shoulder> <elbow>");
      printPrompt();
    }

  } else {
    Serial.println("!! Comando desconhecido.");
    printPrompt();
  }
}

// ─── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  while (!Serial);

  servoBase.attach(PIN_BASE);
  servoShoulder.attach(PIN_SHOULDER);
  servoElbow.attach(PIN_ELBOW);

  // Write home positions immediately to avoid violent jumps on attach
  servoBase.write(BASE_HOME);
  servoShoulder.write(SHOULDER_HOME);
  servoElbow.write(ELBOW_HOME);

  Serial.println("========================================");
  Serial.println("  Robo Tic-Tac-Toe — Fase 1: Servos");
  Serial.println("========================================");
  Serial.println("AVISO: limites atuais sao conservadores.");
  Serial.println("Testa cada servo antes de expandir o range.");
  Serial.println();
  Serial.print("Posicao inicial: ");
  printStatus();
  Serial.println();
  Serial.println("Pressiona [Enter] para executar homing...");
}

// ─── Loop ───────────────────────────────────────────────────────────────────
String inputBuffer = "";
bool waitingForFirstEnter = true;

void loop() {
  updateServos();

  if (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (waitingForFirstEnter) {
        waitingForFirstEnter = false;
        doHoming();
        // printPrompt called automatically when homing finishes
      } else {
        if (inputBuffer.length() > 0) {
          handleCommand(inputBuffer);
          inputBuffer = "";
        }
      }
    } else {
      inputBuffer += c;
    }
  }
}
