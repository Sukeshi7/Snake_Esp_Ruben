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
    const k = e.key.toLowerCase();
    if (['z', 'q', 's', 'd'].includes(k) && k !== lastKey) {
      ws.send(k);
      lastKey = k;
    }
  });
  document.addEventListener('keyup', (e) => {
    const k = e.key.toLowerCase();
    if (k === lastKey) lastKey = null;
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
  resetGame();
}

void resetGame() {
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

  if (gameOver) {
    resetGame();
    return;
  }

  switch (c) {
    case 'z':
      if (dir != down) nextDir = up;
      break;
    case 's':
      if (dir != up) nextDir = down;
      break;
    case 'q':
      if (dir != right) nextDir = left;
      break;
    case 'd':
      if (dir != left) nextDir = right;
      break;
  }
}

void updateGame() {
  dir = nextDir;

  Point newHead = snake[0];
  switch (dir) {
    case up:    newHead.y--; break;
    case down:  newHead.y++; break;
    case left:  newHead.x--; break;
    case right: newHead.x++; break;
  }

  if (newHead.x < 0 || newHead.x >= grid_w || newHead.y < 0 || newHead.y >= grid_h) {
    gameOver = true;
    return;
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

  display.setCursor(12, 0);
  display.println("Game Over");

  display.setTextSize(1);

  display.setCursor(40, 25);
  display.print("Score: ");
  display.setCursor(80, 25);
  display.println(score);

  if (score > best) {
    best = score;
  }

  display.setCursor(40, 35);
  display.print("Best: ");
  display.setCursor(80, 35);
  display.println(best);

  display.setCursor(17, 50);
  display.println("Restart with zqsd");

  display.display();
}

void loop() {
  server.handleClient();
  webSocket.loop();
  readInput();

  if (gameOver) {
    drawGameOver();
    delay(50);
    return;
  }

  unsigned long now = millis();
  if (now - lastTick >= tickInterval) {
    lastTick = now;
    updateGame();
    drawGame();
  }
}
