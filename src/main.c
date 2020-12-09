#include <u8g2_esp32_hal.h>
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include <wifi.h>
#include <utils.h>
#include "esp_log.h"

#define PIN_SDA 5
#define PIN_SCL 4
#define PIN_BUTTON_POS_RIGHT 14
#define PIN_BUTTON_VAR 2
#define DRAW_COLOR_WHITE 1
#define DRAW_COLOR_BLACK 0
#define GRID_MAX_HEIGHT 20
#define GRID_MAX_WIDTH 10
#define SQUARE_SIZE 6
#define SQUARE_STATUS_EMPTY 0
#define SQUARE_STATUS_DRAW 1
#define SQUARE_STATUS_ERASE 2
#define SQUARE_STATUS_STATIC 3

u8g2_t u8g2;
int pieceXPos = 0;
int pieceYPos;
int pieceVariation = 0;
int pieceType = 0;
int score = 0;
bool buttonPosBlocked = false;
bool buttonVarBlocked = false;
int grid[GRID_MAX_HEIGHT][GRID_MAX_WIDTH];
static const char *TAG = "main";

static void IRAM_ATTR buttonPosPressedHandler(void* arg);
static void IRAM_ATTR buttonVarPressedHandler(void* arg);
void buttonPosPressedTask(void *pvParameters);
void buttonVarPressedTask(void *pvParameters);
void taskDisplay(void *pvParameters);
void startDisplay();
void changePiece();
void drawPiece(int pos, int xPos, int status);
void drawPiece1(int pos, int xPos, int status);
void drawPiece2(int pos, int xPos, int status);
void drawPiece3(int pos, int xPos, int status);
void drawPiece4(int pos, int xPos, int status);
void drawGrid();
void destroyLines();
bool verifyCollisions();
bool verifyEnd();
void clearGrid();

void app_main() {
    initNvs();
    initWifi();
    xTaskCreate(sendGamingScoreTask, "task_send_score", 2000, NULL, 2, NULL);
    startDisplay();
}

void startDisplay() {
    gpio_install_isr_service(0);

    gpio_set_direction(PIN_BUTTON_POS_RIGHT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_POS_RIGHT, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_POS_RIGHT, GPIO_INTR_NEGEDGE); 
    gpio_intr_enable(PIN_BUTTON_POS_RIGHT);
    gpio_isr_handler_add(PIN_BUTTON_POS_RIGHT, buttonPosPressedHandler, &pieceXPos);

    gpio_set_direction(PIN_BUTTON_VAR, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_VAR, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_BUTTON_VAR, GPIO_INTR_NEGEDGE); 
    gpio_intr_enable(PIN_BUTTON_VAR);
    gpio_isr_handler_add(PIN_BUTTON_VAR, buttonVarPressedHandler, &pieceXPos);

    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
	u8g2_esp32_hal.sda = PIN_SDA;
	u8g2_esp32_hal.scl = PIN_SCL;
	u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f (
		&u8g2,
		U8G2_R0,
		u8g2_esp32_i2c_byte_cb,
		u8g2_esp32_gpio_and_delay_cb
    );

    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78); 
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);

    u8g2_ClearBuffer(&u8g2);
    u8g2_ClearDisplay(&u8g2);

    xTaskCreate(taskDisplay, "task_display", 3000, NULL, 5, NULL);
}

void taskDisplay(void *pvParameters) {
    u8g2_SetFontDirection(&u8g2, 1);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_mf);
    char scoreView[20];
    loop {
        ESP_LOGI(TAG, "Novo Jogo!");
        score = 0;
        loop {
            pieceXPos = 0;
            int initFor = GRID_MAX_HEIGHT - 4;
            for (pieceYPos = initFor; pieceYPos >= 0; pieceYPos--) {
                drawPiece(pieceYPos, pieceXPos, SQUARE_STATUS_DRAW);
                drawGrid();

                if (pieceYPos == 0) {
                    drawPiece(pieceYPos, pieceXPos, SQUARE_STATUS_STATIC);
                } else {
                    drawPiece(pieceYPos, pieceXPos, SQUARE_STATUS_ERASE);
                }

                sprintf(scoreView, "Pts: %d     ", score);
                u8g2_DrawUTF8(&u8g2, 120, 1, scoreView);
                u8g2_SendBuffer(&u8g2);
                delay(100);

                if (verifyCollisions()) {
                    drawPiece(pieceYPos, pieceXPos, SQUARE_STATUS_STATIC);
                    break;
                }
            }
            destroyLines();
            if (initFor == pieceYPos) {
                clearGrid();
                u8g2_ClearBuffer(&u8g2);
                u8g2_ClearDisplay(&u8g2);
                xQueueSend(bufferEndGameScore, &score, pdMS_TO_TICKS(0));
                break;
            }
            score += 4;
            xQueueSend(bufferGamingScore, &score, pdMS_TO_TICKS(0));
            changePiece();
        }
    }
}

void drawPiece(int pos, int xPos, int status) {
    switch (pieceType)
    {
        case 0:
            drawPiece1(pos, xPos, status);
            break;
        case 1:
            drawPiece2(pos, xPos, status);
            break;
        case 2:
            drawPiece3(pos, xPos, status);
            break;
        case 3:
            drawPiece4(pos, xPos, status);
            break;
        default:
            break;
    }
}

void changePiece() {
    pieceType++;
    if (pieceType > 3) {
        pieceType = 0;
    }
}

void drawPiece1(int pos, int xPos, int status) {
    if (pieceVariation == 0) {
        grid[pos][xPos] = status;
        grid[pos + 1][xPos] = status;
        grid[pos + 1][xPos + 1] = status;
        grid[pos + 2][xPos] = status;
        return;
    }
    if (pieceVariation == 1) {
        grid[pos][xPos] = status;
        grid[pos][xPos + 1] = status;
        grid[pos + 1][xPos + 1] = status;
        grid[pos][xPos + 2] = status;
        return;
    }
    if (pieceVariation == 2) {
        grid[pos][xPos + 1] = status;
        grid[pos + 1][xPos + 1] = status;
        grid[pos + 1][xPos] = status;
        grid[pos + 2][xPos + 1] = status;
        return;
    }
    if (pieceVariation == 3) {
        grid[pos + 1][xPos] = status;
        grid[pos + 1][xPos + 1] = status;
        grid[pos][xPos + 1] = status;
        grid[pos + 1][xPos + 2] = status;
        return;
    }
}

void drawPiece2(int pos, int xPos, int status) {
    if (pieceVariation % 2 == 0) {
        grid[pos][xPos] = status;
        grid[pos][xPos + 1] = status;
        grid[pos][xPos + 2] = status;
        grid[pos][xPos + 3] = status;
    } else {
        grid[pos][xPos] = status;
        grid[pos + 1][xPos] = status;
        grid[pos + 2][xPos] = status;
        grid[pos + 3][xPos] = status;
    }
}

void drawPiece3(int pos, int xPos, int status) {
    grid[pos][xPos] = status;
    grid[pos][xPos + 1] = status;
    grid[pos + 1][xPos] = status;
    grid[pos + 1][xPos + 1] = status;
}

void drawPiece4(int pos, int xPos, int status) {
    if (pieceVariation % 2 == 0) {
        grid[pos][xPos] = status;
        grid[pos][xPos + 1] = status;
        grid[pos + 1][xPos + 1] = status;
        grid[pos + 1][xPos + 2] = status;
    } else {
        grid[pos][xPos] = status;
        grid[pos + 1][xPos] = status;
        grid[pos + 1][xPos + 1] = status;
        grid[pos + 2][xPos + 1] = status;
    }
}

void drawGrid() {
    int i, j;
    for (i = 0; i < GRID_MAX_HEIGHT; i++) {
        for (j = 0; j < GRID_MAX_WIDTH; j++) {
            int square = grid[i][j];
            if (square == SQUARE_STATUS_STATIC) {
                u8g2_SetDrawColor(&u8g2, DRAW_COLOR_WHITE);
                u8g2_DrawBox(&u8g2, i * SQUARE_SIZE, j * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE);
            } else if (square == SQUARE_STATUS_EMPTY) {
                u8g2_SetDrawColor(&u8g2, DRAW_COLOR_BLACK);
                u8g2_DrawBox(&u8g2, i * SQUARE_SIZE, j * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE);
            } else if (square == SQUARE_STATUS_DRAW) {
                u8g2_SetDrawColor(&u8g2, DRAW_COLOR_WHITE);
                u8g2_DrawFrame(&u8g2, i * SQUARE_SIZE, j * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE);
            } else if (square == SQUARE_STATUS_ERASE) {
                u8g2_SetDrawColor(&u8g2, DRAW_COLOR_BLACK);
                u8g2_DrawBox(&u8g2, i * SQUARE_SIZE, j * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE);
                grid[i][j] = SQUARE_STATUS_EMPTY;
            }
        }
    }
}

void clearGrid() {
    int i, j;
    for (i = 0; i < GRID_MAX_HEIGHT; i++) {
        for (j = 0; j < GRID_MAX_WIDTH; j++)  {
            grid[i][j] = SQUARE_STATUS_ERASE;
        }
    }
    drawGrid();
}

void destroyLines() {
    int i, j, k, l;
    for (i = GRID_MAX_HEIGHT - 2; i >= 0; i--) {
        for (j = 0; j < GRID_MAX_WIDTH; j++) {
            if (grid[i][j] != SQUARE_STATUS_STATIC) {
                goto jumpFor;
            }
        }

        for (k = i; k < GRID_MAX_HEIGHT; k++) {
            for (l = 0; l < GRID_MAX_WIDTH; l++) {
                grid[k][l] = grid[k + 1][l];
            }
        }

        jumpFor: continue;
    }
}

bool verifyCollisions() {
    int i, j;
    for (i = 0; i < GRID_MAX_HEIGHT; i++) {
        for (j = 0; j < GRID_MAX_WIDTH; j++) {
            int square = grid[i][j];
            if (square == SQUARE_STATUS_DRAW || square == SQUARE_STATUS_ERASE) {
                if (grid[i - 1][j] == SQUARE_STATUS_STATIC) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool verifyEnd() {
    int i;
    for (i = 0; i < GRID_MAX_WIDTH; i++) {
        if (grid[GRID_MAX_HEIGHT - 1][i] == SQUARE_STATUS_STATIC) {
            return true;
        }
    }
    return false;
}

static void IRAM_ATTR buttonPosPressedHandler(void *arg) {
    xTaskCreate(buttonPosPressedTask, "task_button_pos", 2000, NULL, 1, NULL);
}

void buttonPosPressedTask(void *pvParameters) {
    if (!buttonPosBlocked) {
        buttonPosBlocked = true;
        pieceXPos++;
        delay(500);
        buttonPosBlocked = false;
    }
    vTaskDelete(NULL);
}

static void IRAM_ATTR buttonVarPressedHandler(void *arg) {
    xTaskCreate(buttonVarPressedTask, "task_button_var", 2000, NULL, 1, NULL);
}

void buttonVarPressedTask(void *pvParameters) {
    if (!buttonVarBlocked) {
        buttonVarBlocked = true;
        pieceVariation++;
        if (pieceVariation > 3) {
            pieceVariation = 0;
        }
        delay(500);
        buttonVarBlocked = false;
    }
    vTaskDelete(NULL);
}