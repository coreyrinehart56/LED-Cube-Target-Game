// other ideas:
  //add multiple boss lives (changes color)
  //add button for changing weapons (firework) or for last shot
    //or add charge shot (hold button down)
  //add scoreboard
  //add 3x3 boss
  //health or penalty for missing too many times

#include <FastLED.h>

// pins
#define LED_PIN  2    // Data pin for cube LEDs
#define BARRAGE_BUTTON_PIN  4 // first button pin
#define MISSILE_BUTTON_PIN 5 // second button pin
#define GREEN_LED_PIN  6 // Laser barrage ready indicator
#define YELLOW_LED_PIN 7 // Missile ready indicator

// Joystick pin setup
#define JOY_X_PIN   A0  // X-axis analog input
#define JOY_Y_PIN   A1  // Y-axis analog input
#define BUTTON_PIN  3   // Button pin for laser shooting

#define LASER_BARRAGE_COOLDOWN 15000  // 15 seconds
#define MISSILE_COOLDOWN 20000        // 20 seconds

unsigned long lastLaserBarrageTime = 0;
unsigned long lastMissileTime = 0;

#define NUM_LEDS    125   // 5x5x5 LED cube (5 * 5 * 5 = 125)
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

bool bigTargetMode = true;  // Set to true to enable 2x2 target, false for smaller targets
int bigTargetX = 1;  // starting x (1, so block fits from 1 to 2)
int bigTargetZ = 1;  // starting z (1, so block fits from 1 to 2)
unsigned long bigTargetTimer = 0;
int bigTargetDelay = 300;  // move every 300 ms

int bigTargetHealth = 4; //health (lives) of big target
int totalHits = 0;

// add target storage
const int MAX_TARGETS = 10; // number of small targets
int targetX[MAX_TARGETS];
int targetZ[MAX_TARGETS];
bool targetAlive[MAX_TARGETS];
bool bigTargetAlive;

// makes sure big target can only be hit once by each laserbarrage or missile
bool bigTargetHitThisBarrage = false;
bool bigTargetHitThisMissile = false;

unsigned long targetMoveTimers[MAX_TARGETS];  // individual timers
int targetMoveDelay = 200;  // smaller delay for smoother movement

// Light position (on front face: x = 0, y = vertical, z = front-back)
int lightX = 0;
int lightY = 0;
int lightZ = 0;

bool laserActive = false;

// setup /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BARRAGE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MISSILE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  Serial.begin(9600);

  // Init targets
  randomSeed(analogRead(A2));  // Use an unused analog pin for random seed
  for (int i = 0; i < MAX_TARGETS; i++) {
    targetX[i] = random(0, 5);
    targetZ[i] = random(0, 5);
    targetAlive[i] = true;
  }

  // starts with small targets
  bigTargetMode = false;
  bigTargetAlive = true;
  bigTargetHealth = 4;
  totalHits = 0;

  for (int i = 0; i < MAX_TARGETS; i++) {
    targetMoveTimers[i] = millis();
  }
  
}

// loop /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  int xRaw = analogRead(JOY_X_PIN);
  int yRaw = analogRead(JOY_Y_PIN);

  //joystick debug
  Serial.print(xRaw);
  Serial.print("  ");
  Serial.println(yRaw);

  int deadZone = 50;

  if (abs(xRaw - 512) > deadZone) {
    lightZ = map(xRaw, 0, 900, 0, 4);
  } else {
    lightZ = 2;
  }

  if (abs(yRaw - 512) > deadZone) {
    lightX = map(yRaw, 0, 900, 4, 0);
  } else {
    lightX = 2;
  }

  lightY = 0;

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  
  // Center LED
  leds[XYZtoLEDnum(lightX, lightY, lightZ)] = CRGB::Red;
  
  // Crosshair arms
  if (lightX > 0) leds[XYZtoLEDnum(lightX - 1, lightY, lightZ)] = CRGB::White;
  if (lightX < 4) leds[XYZtoLEDnum(lightX + 1, lightY, lightZ)] = CRGB::White;
  if (lightZ > 0) leds[XYZtoLEDnum(lightX, lightY, lightZ - 1)] = CRGB::White;
  if (lightZ < 4) leds[XYZtoLEDnum(lightX, lightY, lightZ + 1)] = CRGB::White;

  

  if (digitalRead(BUTTON_PIN) == LOW) {
    shootLaser();
  }
  if (digitalRead(BARRAGE_BUTTON_PIN) == LOW) {
    if (millis() - lastLaserBarrageTime >= LASER_BARRAGE_COOLDOWN) {
      shootLaserBarrage();
      lastLaserBarrageTime = millis();  // Reset cooldown timer
    }
  }
  if (digitalRead(MISSILE_BUTTON_PIN) == LOW) {
  if (millis() - lastMissileTime >= MISSILE_COOLDOWN) {
      shootMissile();
      lastMissileTime = millis();  // Reset cooldown timer
    }
  }

  // Show targets
  if (bigTargetMode) {
    // Draw the 2x2 target block
    for (int dx = 0; dx < 2; dx++) {
      for (int dz = 0; dz < 2; dz++) {
        leds[XYZtoLEDnum(bigTargetX + dx, 4, bigTargetZ + dz)] = CRGB::Red;
      }
    }

    // Move the big target every few ms
    if (millis() - bigTargetTimer > bigTargetDelay) {
      bigTargetTimer = millis();
  
      // Try to move by -1, 0, or +1 on x and z, staying in bounds [0,3]
      int moveX = random(-1, 2);
      int moveZ = random(-1, 2);
      
      bigTargetX = constrain(bigTargetX + moveX, 0, 3);  // max 3 to stay within 2x2 range
      bigTargetZ = constrain(bigTargetZ + moveZ, 0, 3);
    }
  } else {
    for (int i = 0; i < MAX_TARGETS; i++) {
      if (targetAlive[i]) {
        leds[XYZtoLEDnum(targetX[i], 4, targetZ[i])] = CRGB::Red;
      }
    }
  }
  
  // Smooth per-target movement
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (!targetAlive[i]) continue;
  
    if (millis() - targetMoveTimers[i] > targetMoveDelay) {
      targetMoveTimers[i] = millis();
  
      // 30% chance to move on any axis
      if (random(100) < 30) {
        targetX[i] = constrain(targetX[i] + random(-1, 2), 0, 4);
      }
      if (random(100) < 30) {
        targetZ[i] = constrain(targetZ[i] + random(-1, 2), 0, 4);
      }
    }
  }

  // Check if all targets are destroyed, spawn big target
  
  bool allDestroyed = true;
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (targetAlive[i]) {
      allDestroyed = false;
      break;
    }
  }

  if (allDestroyed) {
    delay(1000);  // Optional pause before reset
    bigTargetMode = true;
    bigTargetAlive = true;
    bigTargetHealth = 4;
    for (int i = 0; i < MAX_TARGETS; i++) {
      targetX[i] = random(0, 5);
      targetZ[i] = random(0, 5);
      targetAlive[i] = true;
    }
  }

  // Cooldown LED indicators
    if (millis() - lastLaserBarrageTime >= LASER_BARRAGE_COOLDOWN) {
      digitalWrite(GREEN_LED_PIN, HIGH);  // Ready
    } else {
      digitalWrite(GREEN_LED_PIN, LOW);   // Still cooling
    }
    
    if (millis() - lastMissileTime >= MISSILE_COOLDOWN) {
      digitalWrite(YELLOW_LED_PIN, HIGH);
    } else {
      digitalWrite(YELLOW_LED_PIN, LOW);
    }

    //check if big target is alive
    if (bigTargetAlive == false) {
      celebrateWin();
      celebrateWin();
      celebrateWin();
      bigTargetMode = false;
      bigTargetAlive == true;
    }

  FastLED.show();
}

// shootlaser /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void shootLaser() {
  laserActive = true;
  CHSV laserColor = CHSV(100, 255, 255);  // Green

  // Extend the beam upward (Y direction) with gradient + fading trail
  for (int y = 1; y < 5; y++) {
    // Fade previous frame
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].fadeToBlackBy(80);
    }
  
    // Draw laser beam
    leds[XYZtoLEDnum(lightX, y, lightZ)] = CHSV(100, 255, 255);
  
    
    // Redraw targets to stay lit ////////////////////
    // For big target mode
    if (bigTargetMode) {
      for (int dx = 0; dx < 2; dx++) {
          for (int dz = 0; dz < 2; dz++) {
            leds[XYZtoLEDnum(bigTargetX + dx, 4, bigTargetZ + dz)] = CRGB::Red;
          }
       }
    } else {
      // Redraw targets so they stay lit
      for (int i = 0; i < MAX_TARGETS; i++) {
        if (targetAlive[i]) {
          leds[XYZtoLEDnum(targetX[i], 4, targetZ[i])] = CRGB::Red;
        }
      }
    }
    
    CHSV color = CHSV(100, 255, 255);  // Blue to green
    leds[XYZtoLEDnum(lightX, y, lightZ)] = color;
  
    FastLED.show();
    delay(75);
  }

  //updated hit detection
  if (bigTargetMode) {
    // Check if laser hits any part of the 2x2 block
    if (lightX >= bigTargetX && lightX <= bigTargetX + 1 &&
        lightZ >= bigTargetZ && lightZ <= bigTargetZ + 1) {
        bigTargetHealth--;
        explodeBigTarget();
        if (bigTargetHealth <= 0) {
          explodeBigTarget();
          bigTargetAlive = false;
          bigTargetMode = false;
        }
    }
  } else {
    for (int i = 0; i < MAX_TARGETS; i++) {
      if (targetAlive[i] && targetX[i] == lightX && targetZ[i] == lightZ) {
        explodeTarget(i);
      }
    }
  }
  
  // Fade the tail
  for (int step = 0; step < 5; step++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].fadeToBlackBy(80);
    }
    FastLED.show();
    delay(50);
  }

  laserActive = false;
}

// shootLaserBarrage /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void shootLaserBarrage() {
  bigTargetHitThisBarrage = false;  // Reset flag at start of barrage
  
  for (int x = 0; x < 5; x++) {
    // Shoot one vertical laser upward at (x, lightZ)
    for (int y = 1; y < 5; y++) {
      // Fade previous frame
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].fadeToBlackBy(80);
      }

      // Draw current beam
      leds[XYZtoLEDnum(x, y, lightZ)] = CHSV(100, 255, 255);

      // Redraw targets to stay lit
      if (bigTargetMode) {
        for (int dx = 0; dx < 2; dx++) {
          for (int dz = 0; dz < 2; dz++) {
            leds[XYZtoLEDnum(bigTargetX + dx, 4, bigTargetZ + dz)] = CRGB::Red;
          }
        }
      } else {
        for (int i = 0; i < MAX_TARGETS; i++) {
          if (targetAlive[i]) {
            leds[XYZtoLEDnum(targetX[i], 4, targetZ[i])] = CRGB::Red;
          }
        }
      }

      FastLED.show();
      delay(30);
    }

    // Check for hit after vertical shot
    // Handle big target hit
    if (bigTargetMode && !bigTargetHitThisBarrage) {
      for (int dx = 0; dx < 2; dx++) {
        for (int dz = 0; dz < 2; dz++) {
          if (x == bigTargetX + dx && lightZ == bigTargetZ + dz) {
            bigTargetHealth--;
            totalHits++;
            explodeBigTarget();
            if (bigTargetHealth <= 0) {
              explodeBigTarget();
              bigTargetAlive = false;
              bigTargetMode = false;
            }
            bigTargetHitThisBarrage = true;
          }
        }
      }
    } else {
      for (int i = 0; i < MAX_TARGETS; i++) {
        if (targetAlive[i] && targetX[i] == x && targetZ[i] == lightZ) {
          explodeTarget(i);
        }
      }
    }

    // Optional mini fade after each column
    for (int step = 0; step < 2; step++) {
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].fadeToBlackBy(80);
      }
      FastLED.show();
      delay(30);
    }
  }
}
// shootMissile /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void shootMissile() {
  bigTargetHitThisMissile = false;
  int missileX = lightX;
  int missileZ = lightZ;
  // Launch: vertical trail
  for (int y = 0; y <= 4; y++) {
    // Fade previous frame
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].fadeToBlackBy(200);
    }

    // Draw missile
    leds[XYZtoLEDnum(missileX, y, missileZ)] = CRGB::White;

    // Redraw targets to stay visible
    if (bigTargetMode) {
      for (int dx = 0; dx < 2; dx++) {
        for (int dz = 0; dz < 2; dz++) {
          leds[XYZtoLEDnum(bigTargetX + dx, 4, bigTargetZ + dz)] = CRGB::Red;
        }
      }
    } else {
      for (int i = 0; i < MAX_TARGETS; i++) {
        if (targetAlive[i]) {
          leds[XYZtoLEDnum(targetX[i], 4, targetZ[i])] = CRGB::Red;
        }
      }
    }

    FastLED.show();
    delay(100);
  }

  // Explosion at impact
  fireworkExplosion(missileX, 4, missileZ);
}

// fireworkExplosion /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void fireworkExplosion(int cx, int cy, int cz) {
  // Explosion effect with hit detection
  for (int i = 0; i < 3; i++) {
    for (int dx = -1; dx <= 1; dx++) {
      for (int dy = -1; dy <= 1; dy++) {
        for (int dz = -1; dz <= 1; dz++) {
          int x = constrain(cx + dx, 0, 4);
          int y = constrain(cy + dy, 0, 4);
          int z = constrain(cz + dz, 0, 4);

          leds[XYZtoLEDnum(x, y, z)] = CRGB::Yellow;

          // Check regular targets
          if (!bigTargetMode) {
            for (int i = 0; i < MAX_TARGETS; i++) {
              if (targetAlive[i] && targetX[i] == x && targetZ[i] == z && y == 4) {
                explodeTarget(i);
              }
            }
          }

          // Check if big target is hit (only once per missile), decrease health by 2

          if (bigTargetMode && !bigTargetHitThisMissile && bigTargetAlive) {
            for (int dxT = 0; dxT < 2; dxT++) {
              for (int dzT = 0; dzT < 2; dzT++) {
                if (x == bigTargetX + dxT && z == bigTargetZ + dzT && y == 4) {
                  bigTargetHealth--;
                  totalHits++;
                  explodeBigTarget();
                  if (bigTargetHealth <= 0) {
                    explodeBigTarget();
                    bigTargetAlive = false;
                    bigTargetMode = false;
                  }
                  bigTargetHitThisMissile = true;
                }
              }
            }
          }
        }
      }
    }
    FastLED.show();
    delay(75);

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(50);
  }
}

// explodeTarget /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void explodeTarget(int index) {
  int cx = targetX[index];
  int cy = 4;  // Target wall
  int cz = targetZ[index];

  // Initial explosion - light 3x3x3 area
  for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
      for (int dz = -1; dz <= 1; dz++) {
        int x = cx + dx;
        int y = cy + dy;
        int z = cz + dz;

        if (x >= 0 && x < 5 && y >= 0 && y < 5 && z >= 0 && z < 5) {
          leds[XYZtoLEDnum(x, y, z)] = CRGB::Orange;
        }
      }
    }
  }

  FastLED.show();

  // Gradually fade out explosion
  for (int fadeStep = 0; fadeStep < 10; fadeStep++) {
    delay(100);  // Slower = longer lasting explosion
    for (int dx = -1; dx <= 1; dx++) {
      for (int dy = -1; dy <= 1; dy++) {
        for (int dz = -1; dz <= 1; dz++) {
          int x = cx + dx;
          int y = cy + dy;
          int z = cz + dz;

          if (x >= 0 && x < 5 && y >= 0 && y < 5 && z >= 0 && z < 5) {
            leds[XYZtoLEDnum(x, y, z)].fadeToBlackBy(40);  // Slower fade
          }
        }
      }
    }
    FastLED.show();
  }

  targetAlive[index] = false;

  // Respawn target after delay
  delay(200);
  
}

// celebrateWin /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void celebrateWin() {
  int centerX = 2;
  int centerY = 2;

  // Launch: vertical trail
  for (int z = 0; z < 5; z++) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    leds[XYZtoLEDnum(centerX, centerY, z)] = CRGB::White;
    FastLED.show();
    delay(100);
  }

  // Explosion colors
  CHSV explosionColor = CHSV(random(0, 255), 255, 255);

  // Firework explosion: 3D sphere
  for (int r = 0; r <= 2; r++) {
    for (int x = 0; x < 5; x++) {
      for (int y = 0; y < 5; y++) {  // Focus explosion on top of cube
        for (int z = 3; z < 5; z++) {
          int dx = abs(x - centerX);
          int dy = abs(y - centerY);  // top layer
          int dz = abs(z - 4); // top layer
          if (dx + dy + dz == r) {
            leds[XYZtoLEDnum(x, y, z)] = explosionColor;
          }
        }
      }
    }
    FastLED.show();
    delay(100);
  }

  // Fade the explosion gradually
  for (int fadeStep = 0; fadeStep < 6; fadeStep++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].fadeToBlackBy(40);
    }
    FastLED.show();
    delay(70);
  }

  // reset hit counter
  totalHits = 0;
  bigTargetMode = false;
  bigTargetAlive = true;

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

// explodeBigTarget /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void explodeBigTarget() {
  // Center of the 2x2 target for the explosion (approx midpoint)
  int centerX = bigTargetX + 0.5;
  int centerY = 4;
  int centerZ = bigTargetZ + 0.5;

  // Store which LEDs are part of the explosion (so we only fade those)
  bool isExplosionLED[NUM_LEDS] = { false };

  // Flash explosion burst with circular shape (blinking)
  for (int burst = 0; burst < 3; burst++) {
    for (int x = 0; x < 5; x++) {
      for (int y = 3; y < 5; y++) {
        for (int z = 0; z < 5; z++) {
          float dx = x - centerX;
          float dy = y - centerY;
          float dz = z - centerZ;
          float dist = sqrt(dx * dx + dy * dy + dz * dz);

          if (dist <= 1.5) {
            int idx = XYZtoLEDnum(x, y, z);
            leds[idx] = CRGB::OrangeRed;
            isExplosionLED[idx] = true;
          }
        }
      }
    }
    FastLED.show();
    delay(100);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(80);
  }

  // Restore explosion area to red for fade-out
  for (int i = 0; i < NUM_LEDS; i++) {
    if (isExplosionLED[i]) {
      leds[i] = CRGB::OrangeRed;
    }
  }
  FastLED.show();

  // Fade out explosion slowly
  for (int step = 0; step < 10; step++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      if (isExplosionLED[i]) {
        leds[i].fadeToBlackBy(30);  // slower, smoother fade
      }
    }
    FastLED.show();
    delay(60);
  }

  //bigTargetMode = false;
}

// Converts XYZ to LED index based on 5x5x5 cube wiring
// Convert XYZ coordinates to LED index in the 5x5x5 grid
int XYZtoLEDnum(int x, int y, int z) {
  //if ((y == 1) || (y == 3)) 
  //  x = -x + 4;  // Reverse the x direction for every other row
  
  return(x + 5 * y + 25 * z); // Return the LED index based on XYZ coordinates
}
