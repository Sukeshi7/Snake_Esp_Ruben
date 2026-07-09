#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* wifi = "decode-etudiants";
const char* mdp = "learnByDoing25!";

WebServer server(80);
WebSocketsServer webSocket(81);

// screen
#define width 128
#define height 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define I2C_SDA 18
#define I2C_SCL 17
Adafruit_SSD1306 display(width, height, &Wire, OLED_RESET);

//game
#define size 4
#define grid_w (width / size)
#define grid_h (height / size)
#define snake_max_len (grid_w * grid_h)

enum Direction { up, down, left, right };

struct Point {
  int8_t x;
  int8_t y;
};

Point snake[snake_max_len];
int snakeLength;
Direction dir;
Direction nextDir;
Point food;
bool gameOver;
unsigned long lastTick;
unsigned long tickInterval = 150;
int score;
int best = 0;

int difficulty;

enum GameState { menu, playing, gameover };
GameState state = menu;

volatile char pendingKey = 0;

const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Snake ESP32</title>
</head>
<body>
  snake
  <div id="status">connexion...</div>
<script>
  let ws = new WebSocket('ws://' + location.hostname + ':81/');
  let statusEl = document.getElementById('status');
  ws.onopen = () => { statusEl.textContent = 'connecté'; };
  ws.onclose = () => { statusEl.textContent = 'déconnecté'; };

  let lastKey = null;
  document.addEventListener('keydown', (e) => {
  const accepted = {
    KeyZ: 'z',
    KeyW: 'w',
    KeyQ: 'q',
    KeyA: 'a',
    KeyS: 's',
    KeyD: 'd',
    KeyR: 'r',
    Numpad1: '1',
    Numpad2: '2',
  };

  const k = accepted[e.code];

  if (k && k !== lastKey) {
    ws.send(k);
    lastKey = k;
  }
});
  document.addEventListener('keyup', (e) => {
  const accepted = {
    KeyZ: 'z',
    KeyW: 'w',
    KeyQ: 'q',
    KeyA: 'a',
    KeyS: 's',
    KeyD: 'd',
    KeyR: 'r',
    Numpad1: '1',
    Numpad2: '2',
  };

  if (accepted[e.code] === lastKey) {
    lastKey = null;
  }
});
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", html);
}

void webSocketEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT && length > 0) {
    pendingKey = (char)payload[0];
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Erreur : écran SSD1306 non détecté");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi, mdp);

  Serial.print("Connexion au WiFi\n");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
  Serial.print("Connecté, adresse IP : ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  randomSeed(esp_random());

  startScreen();
  state = menu;
  //resetGame();
}

void resetGame(int difficulty) {

  snakeLength = 3;
  int cx = grid_w / 2;
  int cy = grid_h / 2;
  for (int i = 0; i < snakeLength; i++) {
    snake[i].x = cx - i;
    snake[i].y = cy;
  }
  dir = right;
  nextDir = right;
  score = 0;
  gameOver = false;
  spawnFood();
  lastTick = millis();
}

void spawnFood() {
  bool onSnake;
  do {
    onSnake = false;
    food.x = random(0, grid_w);
    food.y = random(0, grid_h);
    for (int i = 0; i < snakeLength; i++) {
      if (snake[i].x == food.x && snake[i].y == food.y) {
        onSnake = true;
        break;
      }
    }
  } while (onSnake);
}

void readInput() {
  if (pendingKey == 0) return;

  char c = pendingKey;
  pendingKey = 0;

  switch (c) {
    case 'z':
      if (dir != down) nextDir = up;
      break;
    case 'w':
      if (dir != down) nextDir = up;
      break;
    case 's':
      if (dir != up) nextDir = down;
      break;
    case 'q':
      if (dir != right) nextDir = left;
      break;
    case 'a':
      if (dir != right) nextDir = left;
      break;
    case 'd':
      if (dir != left) nextDir = right;
      break;
    case 'r':
      startScreen();
      state = menu;
      break;
    case '1':
      difficulty = 1;
      resetGame(1);
      state = playing;
      break;
    case '2':
      difficulty = 2;
      resetGame(2);
      state = playing;
      break;
  }
}

void updateGame(int difficulty) {
  dir = nextDir;

  Point newHead = snake[0];
  switch (dir) {
    case up:    newHead.y--; break;
    case down:  newHead.y++; break;
    case left:  newHead.x--; break;
    case right: newHead.x++; break;
  }

  /*if (difficulty == 2) {
    if (newHead.x < 0 || newHead.x >= grid_w || newHead.y < 0 || newHead.y >= grid_h) {
    gameOver = true;
    return;
    }
  } */

  if (newHead.x < 0 || newHead.x >= grid_w || newHead.y < 0 || newHead.y >= grid_h) {
  if (difficulty == 2) {
    gameOver = true;
    return;
  } else {
    if (newHead.x < 0) newHead.x = grid_w - 1;
    else if (newHead.x >= grid_w) newHead.x = 0;

    if (newHead.y < 0) newHead.y = grid_h - 1;
    else if (newHead.y >= grid_h) newHead.y = 0;
  }
}
  
  for (int i = 0; i < snakeLength; i++) {
    if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
      gameOver = true;
      return;
    }
  }

  bool ateFood = (newHead.x == food.x && newHead.y == food.y);

  int limit = ateFood ? snakeLength : snakeLength - 1;
  for (int i = limit; i > 0; i--) {
    snake[i] = snake[i - 1];
  }
  snake[0] = newHead;

  if (ateFood) {
  snakeLength++;
  score++;
  Serial.print("Score: ");
  Serial.println(score);
  if (tickInterval > 60) tickInterval -= 3;
  spawnFood();
}
}

void drawGame() {
  display.clearDisplay();

  int foodCenterX = food.x * size + size / 2;
  int foodCenterY = food.y * size + size / 2;
  display.fillCircle(foodCenterX, foodCenterY, size / 2, SSD1306_WHITE);

  for (int i = 0; i < snakeLength; i++) {
    display.fillRect(snake[i].x * size, snake[i].y * size, size, size, SSD1306_WHITE);
  }

  display.display();
}

void drawGameOver() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(13, 0);
  display.println("Game Over");

  display.setTextSize(1);

  display.setCursor(40, 20);
  display.print("Score: ");
  display.setCursor(80, 20);
  display.println(score);

  if (score > best) {
    best = score;
  }

  display.setCursor(40, 30);
  display.print("Best: ");
  display.setCursor(80, 30);
  display.println(best);

  display.setCursor(10, 40);
  display.println("Restart with 1 or 2");

    display.setCursor(15, 50);
  display.println("Press r for menu");

  display.display();
}

void startScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.drawRect(0, 0, width, height, SSD1306_WHITE);

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(35, 2);
  display.println("SNAKE");

  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("Choose difficulty:");
  display.setCursor(20, 35);
  display.println("1. Easy (no walls)");
  display.setCursor(20, 45);
  display.println("2. Medium");

  display.display();
}

void loop() {
  server.handleClient();
  webSocket.loop();

  readInput();

  switch (state) {
    case menu:
      break;
    case playing: {
      if (gameOver) {
        drawGameOver();
        delay(50);
        return;
      }

      unsigned long now = millis();
      if (now - lastTick >= tickInterval) {
        lastTick = now;
        updateGame(difficulty);
        drawGame();
      }
      break;
    }
    case gameover: {
      delay(50);
      break;
    }
  }
}
