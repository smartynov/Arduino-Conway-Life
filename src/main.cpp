#include <TFT_eSPI.h>
#include <Button2.h>

// TODO
// - 2-step oscillator detection & fade
// - hsv colors
// - oscillate color during iterations
// - speed control (slow/fast)
// - randomize seed density?

#define SCREENX 240
#define SCREENY 135

TFT_eSPI tft = TFT_eSPI(SCREENY, SCREENX);

Button2 buttonStep = Button2(0);
Button2 buttonPlay = Button2(35);

bool stepMode = false;
bool nextStep = false;

int msDelay = 100;
int pixPerCell = 1;
uint32_t drawColor = 0xFFFF;
uint32_t blankColor = 0x0;

// Conway's Life logic

int sizeX, sizeY;
bool *curField, *newField;

void initField(void)
{
	curField = new bool [sizeX * sizeY];
	newField = new bool [sizeX * sizeY];

	for (int x = 0; x < sizeX; ++x) {
		int x0 = x*sizeY;
		for (int y = 0; y < sizeY; ++y) {
			curField[x0+y] = 0;
			newField[x0+y] = !random(5);
		}
	}
}

void destroyField(void)
{
	delete newField;
	delete curField;
}

int countNeighbors(int x, int y)
{
	int xm0 = ((x <= 0) ? (sizeX - 1) : (x - 1)) * sizeY;
	int x0  = x * sizeY;
	int xp0 = ((x >= sizeX - 1) ? (0) : (x + 1)) * sizeY;
	int ym = (y <= 0) ? (sizeY - 1) : (y - 1);
	int yp = (y >= sizeY - 1) ? (0) : (y + 1);

	return   curField[xm0+ym] + curField[xm0+y] + curField[xm0+yp]
	       + curField[x0 +ym]                   + curField[x0 +yp]
	       + curField[xp0+ym] + curField[xp0+y] + curField[xp0+yp];
}

int nextGeneration() {
	int countChanged = 0;
	for (int x = 0; x < sizeX; ++x) {
		int x0 = x*sizeY;
		for (int y = 0; y < sizeY; ++y) {
			int neighbors = countNeighbors(x, y);
			bool newCell = (neighbors == 3 || (curField[x*sizeY+y] && (neighbors == 2)));
			newField[x0+y] = newCell;
			if (curField[x0+y] != newCell) {
				countChanged++;
			}
		}
	}
	return countChanged;
}

// display field

int redrawField(bool force = false) {
	int countCells = 0;
	for (int x = 0; x < sizeX; ++x) {
		int x0 = x*sizeY;
		for (int y = 0; y < sizeY; ++y) {
			if (newField[x0+y]) {
				countCells++;
			}
			if ((force && newField[x0+y]) || newField[x0+y] != curField[x0+y]) {
				tft.fillRect(x*pixPerCell, y*pixPerCell,
				             pixPerCell, pixPerCell,
				             newField[x0+y] ? drawColor : blankColor);
			}
		}
	}
	return countCells;
}

// buttons & sleep

void buttonHandler(Button2& btn) {
    if (btn == buttonStep) {
        stepMode = true;
        nextStep = true;
    } else if (btn == buttonPlay) {
        stepMode = false;
    }
}

void sleep(int ms)
{
	esp_sleep_enable_timer_wakeup(ms * 1000);
	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
	esp_light_sleep_start();
}

void sleepLoop(int ms)
{
    while (true) {
        buttonStep.loop();
        buttonPlay.loop();

        if (ms <= 0) {
            break;
        }

        int step = ms > 50 ? 50 : ms;
        sleep(step);
        ms -= step;
    }
}

// setup & loop

void setup()
{
	tft.init();
	tft.setRotation(3);
	tft.fillScreen(blankColor);
	tft.setCursor(0, 0);

    buttonStep.setTapHandler(buttonHandler);
    buttonPlay.setTapHandler(buttonHandler);
}

void loop()
{
	drawColor = random(0xFFFF);
	pixPerCell = 1+random(15);
	msDelay = sqrt(pixPerCell - 1) * 50;
	int generations = 2500 / pixPerCell;

	sizeX = SCREENX / pixPerCell;
	sizeY = SCREENY / pixPerCell;

	initField();

	bool fading = false;
	do {
        // draw field
		if (redrawField(fading) == 0) {
			break;
		}

        // delay or wait for button
        if (stepMode) {
            while (stepMode && !nextStep) {
                sleepLoop(10);
            }
            nextStep = false;
        }
		else {
			sleepLoop(msDelay);
		}

        // swap fields
		bool * tmp = curField;
		curField = newField;
		newField = tmp;

        // calculate next generation
		if (nextGeneration() == 0 || --generations < 0) {
            // fade to black
			fading = true;
			uint16_t r = drawColor >> 11;
			uint16_t g = (drawColor >> 6) & 0x1F;
			uint16_t b = drawColor & 0x1F;
			drawColor = ((r ? r-1 : 0) << 11) | ((g ? g-1 : 0) << 6) | (b ? b-1 : 0);
		}
	} while (drawColor);

	destroyField();

	tft.fillScreen(blankColor);
	sleep(500);
}
