// Ultrasonic drums: two HC-SR04 sensors play drums over USB MIDI.
// Move a hand closer to a sensor to go from hi-hat to snare to kick.
// Needs a board with native USB (Pro Micro, Leonardo, Micro), which shows up as a MIDI device.

#include "MIDIUSB.h"

const byte MIDI_CHANNEL = 9;  // zero-based, so MIDI channel 10: the General MIDI drum channel
const byte VELOCITY = 127;
const int NO_NOTE = -1;

// General MIDI drum notes, nearest zone first
const int KICK = 36;
const int SNARE = 38;
const int CLOSED_HAT = 42;

// Zone edges in cm
const int ZONE_KICK = 10;
const int ZONE_SNARE = 20;
const int ZONE_HAT = 30;

// An echo takes about 1.8 ms from 30 cm. Stop listening after 6 ms (about 1 m) so a missed echo
// can't stall the loop; pulseIn's default wait is a whole second.
const unsigned long ECHO_TIMEOUT_US = 6000;
// HC-SR04s drop the odd reading. Only let go of a note after this many misses in a row,
// or a held hand would keep retriggering the drum.
const byte MAX_MISSES = 3;

struct Sensor {
  byte trigPin;
  byte echoPin;
  int note;
  byte misses;
};

Sensor sensors[] = {
  {2, 3, NO_NOTE, 0},  // left
  {4, 5, NO_NOTE, 0},  // right
};

void sendNote(byte status, byte header, int pitch, byte velocity) {
  midiEventPacket_t packet = {header, (byte)(status | MIDI_CHANNEL), (byte)pitch, velocity};
  MidiUSB.sendMIDI(packet);
  MidiUSB.flush();  // send now, not when the USB buffer next empties
}

// Distance in cm, or -1 if no echo came back
int readDistance(const Sensor &sensor) {
  digitalWrite(sensor.trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(sensor.trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(sensor.trigPin, LOW);
  unsigned long duration = pulseIn(sensor.echoPin, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

int noteForDistance(int distance) {
  if (distance < ZONE_KICK) return KICK;
  if (distance < ZONE_SNARE) return SNARE;
  if (distance < ZONE_HAT) return CLOSED_HAT;
  return NO_NOTE;
}

void update(Sensor &sensor) {
  int distance = readDistance(sensor);
  if (distance < 0 && ++sensor.misses < MAX_MISSES) return;

  int note = NO_NOTE;
  if (distance >= 0) {
    sensor.misses = 0;
    note = noteForDistance(distance);
  }
  if (note == sensor.note) return;

  if (sensor.note != NO_NOTE) sendNote(0x80, 0x08, sensor.note, 0);
  if (note != NO_NOTE) sendNote(0x90, 0x09, note, VELOCITY);
  sensor.note = note;
}

void setup() {
  for (Sensor &sensor : sensors) {
    pinMode(sensor.trigPin, OUTPUT);
    pinMode(sensor.echoPin, INPUT);
  }
}

void loop() {
  for (Sensor &sensor : sensors) {
    update(sensor);
    delay(10);  // let the last ping die out so the sensors don't hear each other
  }
}
