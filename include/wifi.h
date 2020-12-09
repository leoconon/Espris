#ifndef WIFI
#define WIFI

QueueHandle_t bufferGamingScore;
QueueHandle_t bufferEndGameScore;

void initNvs();
void initWifi();
void sendGamingScoreTask(void *pvParameters);

#endif