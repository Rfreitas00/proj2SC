#include <Servo.h>
#include <ArduinoBLE.h>

#define SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define CMD_UUID     "12345678-1234-1234-1234-123456789a01"
#define ACK_UUID     "12345678-1234-1234-1234-123456789a02"

#define PIN_BASE      5
#define PIN_SHOULDER  6
#define PIN_ELBOW     3

#define BASE_MIN      30
#define BASE_MAX      160
#define SHOULDER_MIN  30
#define SHOULDER_MAX  180
#define ELBOW_MIN     20
#define ELBOW_MAX     110

#define STEP_DELAY 15

#define HOME_BASE      110
#define HOME_SHOULDER   70
#define HOME_ELBOW      95

#define OFFSET_BASE      3
#define OFFSET_SHOULDER  3

Servo servoBase, servoShoulder, servoElbow;
int posBase = HOME_BASE, posShoulder = HOME_SHOULDER, posElbow = HOME_ELBOW;

BLEDevice    nicla;
BLECharacteristic cmdChar;
BLECharacteristic ackChar;
bool ble_connected = false;

struct CellPos { int base; int shoulder; int elbow; };

const CellPos cells[9] = {
  { 88, 175, 86},   // 0
  {103, 160, 73},   // 1
  {110, 135, 56},   // 2
  { 76, 161, 74},   // 3
  { 95, 148, 62},   // 4
  { 99, 118, 49},   // 5
  { 69, 145, 61},   // 6
  { 82, 125, 51},   // 7
  { 92,  96, 38},   // 8
};

int oWaypointsBase[]     = {5, 3, 0, -3, -5, -3,  0,  3, 5};
int oWaypointsShoulder[] = {0, 3, 5,  3,  0, -3, -5, -3, 0};

int clampValue(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void moveServo(Servo &servo, int &current, int target, int lo, int hi) {
  target = clampValue(target, lo, hi);
  while (current != target) {
    current += (current < target) ? 1 : -1;
    servo.write(current);
    delay(STEP_DELAY);
  }
}

void moveXY(int targetBase, int targetShoulder) {
  targetBase     = clampValue(targetBase,     BASE_MIN,     BASE_MAX);
  targetShoulder = clampValue(targetShoulder, SHOULDER_MIN, SHOULDER_MAX);
  int dBase = targetBase - posBase;
  int dShoulder = targetShoulder - posShoulder;
  int steps = max(abs(dBase), abs(dShoulder));
  if (steps == 0) return;
  for (int i = 1; i <= steps; i++) {
    servoBase.write(    posBase     + (dBase     * i) / steps);
    servoShoulder.write(posShoulder + (dShoulder * i) / steps);
    delay(STEP_DELAY);
  }
  posBase = targetBase;
  posShoulder = targetShoulder;
}

void penUp() {
  moveServo(servoElbow, posElbow, HOME_ELBOW, ELBOW_MIN, ELBOW_MAX);
  delay(80);
}

void penDown(int elbowTarget) {
  moveServo(servoElbow, posElbow, elbowTarget, ELBOW_MIN, ELBOW_MAX);
  delay(120);
}

void goHome() {
  penUp();
  moveXY(HOME_BASE, HOME_SHOULDER);
  Serial.println(F("HOME"));
}

void drawX(int cell) {
  if (cell < 0 || cell > 8) return;
  int cb = cells[cell].base;
  int cs = cells[cell].shoulder;
  int ce = cells[cell].elbow;

  Serial.print(F("X celula ")); Serial.println(cell);

  // Diagonal 1: (cb-B, cs-S) → (cb+B, cs+S)
  penUp();
  moveXY(cb - OFFSET_BASE, cs - OFFSET_SHOULDER);
  penDown(ce);
  moveXY(cb + OFFSET_BASE, cs + OFFSET_SHOULDER);
  penUp();

  delay(STEP_DELAY * 3);

  // Diagonal 2: (cb-B, cs+S) → (cb+B, cs-S)
  moveXY(cb - OFFSET_BASE, cs + OFFSET_SHOULDER);
  penDown(ce);
  moveXY(cb + OFFSET_BASE, cs - OFFSET_SHOULDER);
  penUp();

  Serial.print(F("X ok celula ")); Serial.println(cell);
}

void drawO(int cell) {
  if (cell < 0 || cell > 8) return;
  int cb = cells[cell].base;
  int cs = cells[cell].shoulder;
  int ce = cells[cell].elbow;

  Serial.print(F("O celula ")); Serial.println(cell);

  penUp();
  moveXY(cb + O_SIZE, cs);
  delay(80);
  penDown(ce);

  for (int i = 1; i < O_WAYPOINTS; i++) {
    moveXY(cb + (oWaypointsBase[i]     * O_SIZE) / 5,
           cs + (oWaypointsShoulder[i] * O_SIZE) / 5);
  }

  moveXY(cb + O_SIZE, cs);
  penUp();

  Serial.print(F("O ok celula ")); Serial.println(cell);
}

void sendACK() {
  if (ble_connected && ackChar) {
    ackChar.writeValue("OK\n");
    Serial.println(F("[BLE] ACK enviado"));
  }
}

void handleBLECommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  Serial.print(F("[BLE] Recebido: ")); Serial.println(cmd);

  if (cmd.startsWith("DRAW X ") && cmd.length() >= 8) {
    int cell = cmd.charAt(7) - '0';
    if (cell >= 0 && cell <= 8) {
      goHome();
      drawX(cell);
      goHome();
      sendACK();
    } else {
      Serial.println(F("ERRO: celula invalida"));
    }
    return;
  }

  if (cmd.startsWith("DRAW O ") && cmd.length() >= 8) {
    int cell = cmd.charAt(7) - '0';
    if (cell >= 0 && cell <= 8) {
      goHome();
      drawO(cell);
      goHome();
      sendACK();
    }
    return;
  }

  if (cmd.startsWith("WIN:")) {
    Serial.print(F("Vencedor: ")); Serial.println(cmd.charAt(4));
    sendACK();
    return;
  }

  if (cmd == "DRAW") {
    Serial.println(F("Empate!"));
    sendACK();
    return;
  }

  Serial.println(F("Comando BLE desconhecido"));
}

void startScan() {
  ble_connected = false;
  BLE.scanForName("TTT-Nicla");
  Serial.println(F("[BLE] A procurar TTT-Nicla..."));
}

bool connectToNicla() {
  nicla = BLE.available();
  if (!nicla) return false;

  BLE.stopScan();
  Serial.print(F("[BLE] Nicla encontrada: "));
  Serial.println(nicla.address());

  if (!nicla.connect()) {
    Serial.println(F("[BLE] Falha ao ligar."));
    startScan();
    return false;
  }

  if (!nicla.discoverAttributes()) {
    Serial.println(F("[BLE] Falha ao descobrir atributos."));
    nicla.disconnect();
    startScan();
    return false;
  }

  cmdChar = nicla.characteristic(CMD_UUID);
  ackChar = nicla.characteristic(ACK_UUID);

  if (!cmdChar || !ackChar) {
    Serial.println(F("[BLE] Caracteristicas nao encontradas."));
    nicla.disconnect();
    startScan();
    return false;
  }

  if (cmdChar.canSubscribe()) {
    cmdChar.subscribe();
    Serial.println(F("[BLE] Subscrito a CMD."));
  }

  ble_connected = true;
  Serial.println(F("[BLE] Nicla ligada!"));
  return true;
}

void parseSerial(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "HOME") { goHome(); return; }

  if (cmd.startsWith("DRAW ") && cmd.length() >= 8) {
    char sym  = cmd.charAt(5);
    int  cell = cmd.charAt(7) - '0';
    if (cell < 0 || cell > 8) { Serial.println(F("ERRO: celula 0-8")); return; }
    goHome();
    if      (sym == 'X') drawX(cell);
    else if (sym == 'O') drawO(cell);
    goHome();
    return;
  }

  if (cmd.startsWith("POINT ")) {
    int cell = cmd.substring(6).toInt();
    if (cell >= 0 && cell <= 8) {
      penUp();
      moveXY(cells[cell].base, cells[cell].shoulder);
      Serial.print(F("Centro celula ")); Serial.println(cell);
    }
    return;
  }

  Serial.println(F("Comandos: DRAW X/O 0-8 | HOME | POINT 0-8"));
}

void setup() {
  Serial.begin(9600);

  // ── Servos ──
  servoBase.attach(PIN_BASE);
  servoShoulder.attach(PIN_SHOULDER);
  servoElbow.attach(PIN_ELBOW);

  servoElbow.write(HOME_ELBOW);       posElbow    = HOME_ELBOW;    delay(500);
  servoShoulder.write(HOME_SHOULDER); posShoulder = HOME_SHOULDER; delay(500);
  servoBase.write(HOME_BASE);         posBase     = HOME_BASE;     delay(500);

  Serial.println(F("=========================================="));
  Serial.println(F("SISTEMA PRONTO - TIC-TAC-TOE ROBOT"));
  Serial.println(F("=========================================="));

  // ── BLE ──
  if (!BLE.begin()) {
    Serial.println(F("BLE falhou! Continua sem BLE."));
  } else {
    startScan();
  }
}

void loop() {

  if (!ble_connected) {
    // Tenta ligar à Nicla
    connectToNicla();
  } else {
    if (!nicla.connected()) {
      // Nicla desligou — reinicia scan
      ble_connected = false;
      Serial.println(F("[BLE] Nicla desligada. A reconectar..."));
      startScan();
    } else if (cmdChar.valueUpdated()) {
      // Novo comando recebido
      int  len = cmdChar.valueLength();
      char buf[32] = {0};
      cmdChar.readValue(buf, min(len, 31));
      handleBLECommand(String(buf));
    }
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    parseSerial(cmd);
  }
}
