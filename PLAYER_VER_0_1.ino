#include <Arduino.h>
#include <Audio.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);

// SD
#define SD_CS      5
#define SPI_MOSI   23
#define SPI_MISO   19
#define SPI_SCK    18
#define SD_SPI_HZ  20000000UL

// I2S
#define I2S_DOUT   25
#define I2S_BCLK   27
#define I2S_LRC    26

// BUTTONS
#define BTN_PREV      32
#define BTN_NEXT      33
#define BTN_PLAY      12
#define BTN_LOOP      14

// Potentiometer
#define POT_PIN       34

#define FILTER_SAMPLES   8
#define VOLUME_DEAD_ZONE 2
#define DEBOUNCE_DELAY   40

#define MAX_SONGS    50
#define MAX_NAME_LEN 64
#define DISPLAY_MIN_INTERVAL 150

Audio audio;

SemaphoreHandle_t audioMutex;

char playlist[MAX_SONGS][MAX_NAME_LEN];
int totalSongs = 0;
int currentSong = 0;

int volume = 50;
bool paused = false;
bool loopMode = false;

unsigned long lastOLEDUpdate = 0;
unsigned long lastPotRead = 0;
unsigned long lastDisplayUpdate = 0;

int potBuffer[FILTER_SAMPLES] = {0};
int potBufferIndex = 0;
int lastVolume = 50;

volatile bool songEnded = false;

struct Button {
    uint8_t pin;
    bool    lastReading;
    bool    stableState;
    unsigned long lastDebounceTime;
};

Button btnPrev = { BTN_PREV, HIGH, HIGH, 0 };
Button btnNext = { BTN_NEXT, HIGH, HIGH, 0 };
Button btnPlay = { BTN_PLAY, HIGH, HIGH, 0 };
Button btnLoop = { BTN_LOOP, HIGH, HIGH, 0 };

bool buttonPressed(Button &btn)
{
    bool reading = digitalRead(btn.pin);

    if(reading != btn.lastReading)
    {
        btn.lastDebounceTime = millis();
    }

    bool pressedEdge = false;

    if((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY)
    {
        if(reading != btn.stableState)
        {
            btn.stableState = reading;
            if(btn.stableState == LOW)
                pressedEdge = true;
        }
    }

    btn.lastReading = reading;
    return pressedEdge;
}

void audioTask(void *parameter)
{
    for(;;)
    {
        if(xSemaphoreTakeRecursive(audioMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            audio.loop();
            xSemaphoreGiveRecursive(audioMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void showSong(bool force = false)
{
    if(!force && (millis() - lastDisplayUpdate < DISPLAY_MIN_INTERVAL))
        return;

    lastDisplayUpdate = millis();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Now Playing");
    display.drawLine(0, 10, 127, 10, WHITE);

    if(totalSongs == 0)
    {
        display.setCursor(0, 25);
        display.println("No songs loaded");
        display.display();
        return;
    }

    char name[MAX_NAME_LEN];
    strncpy(name, playlist[currentSong], MAX_NAME_LEN - 1);
    name[MAX_NAME_LEN - 1] = '\0';

    char *nameStart = name;
    if(nameStart[0] == '/')
        nameStart++;

    int len = strlen(nameStart);
    if(len > 4 && strcasecmp(nameStart + len - 4, ".mp3") == 0)
        nameStart[len - 4] = '\0';

    if(strlen(nameStart) > 20)
        nameStart[20] = '\0';

    display.setCursor(0, 15);
    display.println(nameStart);

    int current = 0;
    int total   = 0;

    if(xSemaphoreTakeRecursive(audioMutex, pdMS_TO_TICKS(20)) == pdTRUE)
    {
        current = audio.getAudioCurrentTime();
        total   = audio.getAudioFileDuration();
        xSemaphoreGiveRecursive(audioMutex);
    }

    if(current < 0) current = 0;
    if(total < 0)   total = 0;

    display.setCursor(0, 30);
    display.printf("%d:%02d/%d:%02d", current / 60, current % 60, total / 60, total % 60);

    int progressWidth = 0;
    if(total > 0)
    {
        progressWidth = ((long)current * 120) / total;
        progressWidth = constrain(progressWidth, 0, 120);
    }

    display.drawRect(4, 45, 120, 10, WHITE);
    if(progressWidth > 0)
    {
        display.fillRect(5, 46, progressWidth - 1, 8, WHITE);
    }

    display.setCursor(0, 57);
    display.print("V:");
    display.print(volume);
    display.print("%");

    display.setCursor(45, 57);
    display.print(paused ? "PAUSE" : "PLAY");

    display.setCursor(80, 57);
    display.print(loopMode ? "LOOP" : "    ");

    display.setCursor(115, 57);
    display.print(currentSong + 1);
    display.print("/");
    display.print(totalSongs);

    display.display();
}

void sortPlaylist()
{
    for(int i = 1; i < totalSongs; i++)
    {
        char temp[MAX_NAME_LEN];
        strncpy(temp, playlist[i], MAX_NAME_LEN - 1);
        temp[MAX_NAME_LEN - 1] = '\0';

        int j = i - 1;
        while(j >= 0 && strcasecmp(playlist[j], temp) > 0)
        {
            strncpy(playlist[j + 1], playlist[j], MAX_NAME_LEN - 1);
            playlist[j + 1][MAX_NAME_LEN - 1] = '\0';
            j--;
        }
        strncpy(playlist[j + 1], temp, MAX_NAME_LEN - 1);
        playlist[j + 1][MAX_NAME_LEN - 1] = '\0';
    }
}

void scanMusic()
{
    File root = SD.open("/");
    if(!root)
        return;

    while(totalSongs < MAX_SONGS)
    {
        File file = root.openNextFile();
        if(!file)
            break;

        if(!file.isDirectory())
        {
            const char *name = file.name();
            String lower = name;
            lower.toLowerCase();

            if(lower.endsWith(".mp3"))
            {
                if(name[0] == '/')
                {
                    strncpy(playlist[totalSongs], name, MAX_NAME_LEN - 1);
                }
                else
                {
                    playlist[totalSongs][0] = '/';
                    strncpy(playlist[totalSongs] + 1, name, MAX_NAME_LEN - 2);
                }
                playlist[totalSongs][MAX_NAME_LEN - 1] = '\0';
                totalSongs++;
            }
        }
        file.close();
    }

    root.close();

    if(totalSongs >= MAX_SONGS)
    {
        // Warning: playlist limit reached
    }

    sortPlaylist();
}

void playSong(int index)
{
    if(totalSongs == 0)
        return;

    int attempts = 0;

    while(attempts < totalSongs)
    {
        if(index >= totalSongs) index = 0;
        if(index < 0) index = totalSongs - 1;

        bool ok = false;

        if(xSemaphoreTakeRecursive(audioMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            audio.stopSong();
            ok = audio.connecttoFS(SD, playlist[index]);
            xSemaphoreGiveRecursive(audioMutex);
        }

        if(ok)
        {
            currentSong = index;
            paused = false;
            songEnded = false;
            showSong(true);
            return;
        }

        index++;
        attempts++;
    }
}

void restartCurrentSong()
{
    if(xSemaphoreTakeRecursive(audioMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        audio.stopSong();
        xSemaphoreGiveRecursive(audioMutex);
    }
    playSong(currentSong);
}

void setup()
{
    Serial.begin(115200);

    Wire.begin(21, 22);
    Wire.setClock(400000);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        while(1);
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("MP3");
    display.setCursor(0, 25);
    display.println("PLAYER");
    display.display();

    delay(1500);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    
    if(!SD.begin(SD_CS, SPI, 10000000))
    {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("SD FAILED");
        display.display();
        while(1);
    }

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    
    audioMutex = xSemaphoreCreateRecursiveMutex();
    if(audioMutex == NULL)
    {
        while(1);
    }

    audio.setVolume(map(volume, 0, 100, 0, 21));

    pinMode(BTN_PREV, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);
    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_LOOP, INPUT_PULLUP);

    analogReadResolution(12);

    xTaskCreatePinnedToCore(
        audioTask,
        "audioTask",
        16384,
        NULL,
        2,
        NULL,
        0
    );

    delay(100);

    scanMusic();

    if(totalSongs > 0)
    {
        playSong(0);
    }
    else
    {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("NO MP3 FILES");
        display.display();
    }
}

void loop()
{
    if(songEnded)
    {
        songEnded = false;
        if(loopMode)
            restartCurrentSong();
        else
            playSong(currentSong + 1);
        showSong(true);
    }

    if(millis() - lastOLEDUpdate >= 1000)
    {
        lastOLEDUpdate = millis();
        showSong(true);
    }

    if(millis() - lastPotRead >= 50)
    {
        lastPotRead = millis();

        potBuffer[potBufferIndex] = analogRead(POT_PIN);
        potBufferIndex++;
        if(potBufferIndex >= FILTER_SAMPLES)
            potBufferIndex = 0;

        long sum = 0;
        for(int i = 0; i < FILTER_SAMPLES; i++)
        {
            sum += potBuffer[i];
        }
        int filteredValue = sum / FILTER_SAMPLES;

        int newVolume = map(filteredValue, 0, 4095, 0, 100);
        newVolume = constrain(newVolume, 0, 100);

        if(abs(newVolume - lastVolume) >= VOLUME_DEAD_ZONE)
        {
            volume = newVolume;

            if(xSemaphoreTakeRecursive(audioMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                audio.setVolume(map(volume, 0, 100, 0, 21));
                xSemaphoreGiveRecursive(audioMutex);
            }

            lastVolume = volume;
            showSong();
        }
    }

    if(buttonPressed(btnPrev))
    {
        if(loopMode)
            restartCurrentSong();
        else
            playSong(currentSong - 1);
    }

    if(buttonPressed(btnNext))
    {
        if(loopMode)
            restartCurrentSong();
        else
            playSong(currentSong + 1);
    }

    if(buttonPressed(btnPlay))
    {
        paused = !paused;

        if(xSemaphoreTakeRecursive(audioMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            audio.pauseResume();
            xSemaphoreGiveRecursive(audioMutex);
        }

        showSong(true);
    }

    if(buttonPressed(btnLoop))
    {
        loopMode = !loopMode;
        showSong(true);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
}

void audio_eof_mp3(const char *info)
{
    songEnded = true;
}