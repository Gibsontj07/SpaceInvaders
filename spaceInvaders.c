/* Author: Thomas Gibson (Based on Pong by Steve Gunn)
 * Licence: This work is licensed under the Creative Commons Attribution License.
 *           View this license at http://creativecommons.org/about/licenses/
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include "lcd.h"
#include "rotary.h"
#include "led.h"


#define LAZER_SIZE	10
#define SHIP_WIDTH	40
#define SHIP_HEIGHT	10
#define SHIP_INC	7
#define NO_OF_INVADERS 12
#define HIT_BOX_SIZE 23

typedef struct {
	uint16_t x, y;
} coord;

const rectangle start_ship = {(LCDHEIGHT-SHIP_WIDTH)/2, (LCDHEIGHT+SHIP_WIDTH)/2, LCDWIDTH-SHIP_HEIGHT-1, LCDWIDTH};
const rectangle original_lazer = {(LCDHEIGHT-1)/2, (LCDHEIGHT+1)/2, LCDWIDTH-LAZER_SIZE-1, LCDWIDTH};
const rectangle invader = {150,LCDHEIGHT-50, 180, LCDWIDTH-50};
const uint8_t zapSequence[24] = {1,8,2,11,5,10,3,6,0,9,4,7,2,11,5,8,1,3,9,6,0,7,10,4};

volatile rectangle start_lazer = {(LCDHEIGHT-1)/2, (LCDHEIGHT+1)/2, LCDWIDTH-LAZER_SIZE-1, LCDWIDTH};
volatile rectangle ship, lazer, last_ship, last_lazer;
volatile uint16_t score;
volatile uint8_t lives;
volatile uint8_t fps = 0;
volatile uint8_t lazer_fired = 0; 
volatile uint8_t zap_fired = 0; 
volatile uint8_t squadOffset = 0;
volatile coord invadersCoords[NO_OF_INVADERS] = {{70, 100}, {70, 70}, {100, 100}, {100, 70}, {130, 100}, {130, 70}, {160, 100}, {160, 70}, {190, 100}, {190, 70}, {220, 100}, {220, 70}};
volatile coord invadersPrevCoords[NO_OF_INVADERS] = {{130, 100}, {100, 100}, {100, 70}, {130, 70}, {160, 100}, {160, 70}, {190, 100}, {190, 70}, {220, 100}, {220, 70}, {250, 100}, {250, 70}};
volatile uint8_t invadersVisible[NO_OF_INVADERS] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
volatile coord zap, last_zap;
volatile coord start_zap = {0,0};
volatile int move = 0;
volatile int zap_timer = 0;
volatile int squadDist = 20;
volatile int zapSequenceVal = 0;

ISR(INT6_vect)
{
	//draw ship.
	fill_rectangle(last_ship, display.background);
	fill_rectangle(ship, GREEN);

	//draw invaders.
	int i;
	for (i = 0; i < NO_OF_INVADERS; i++) {
		display_invader(invadersPrevCoords[i].x, invadersPrevCoords[i].y, display.background);
		if (invadersVisible[i] == 0) {
			display_invader(invadersCoords[i].x, invadersCoords[i].y, display.background);
		} else {
			display_invader(invadersCoords[i].x, invadersCoords[i].y, WHITE);
		}

		invadersPrevCoords[i].x = invadersCoords[i].x;
		invadersPrevCoords[i].y = invadersCoords[i].y;
	}

	//draw lazer and zap.
	fill_rectangle(last_lazer, display.background);
	fill_rectangle(lazer, GREEN);
	display_zap(last_zap.x, last_zap.y, display.background);

	if (zap_fired == 1) {
		display_zap(zap.x, zap.y, WHITE);
	}
	
	//update previous positions before objects are moved again.
	last_lazer = lazer;
	last_ship = ship;
	last_zap.x = zap.x;
	last_zap.y = zap.y;

	//Display lives left and score.
	char buffer[4];
	sprintf(buffer, "%03d", lives);
	display_string_xy(buffer, 280, 20);
	sprintf(buffer, "%03d", score);
	display_string_xy(buffer, 25, 20);

	//Increase timer values
	move++;
	zap_timer++;
	fps++;
}

ISR(TIMER1_COMPA_vect)
{
	//Move lazer if it's been fired.
	static int8_t xinc = 2;
	if (lazer_fired == 1) {
		lazer.top   -= xinc;
		lazer.bottom  -= xinc;
	}
	//Move lazer on centre button press
	if(!(PINE & _BV(SWC))){
		lazer_fired = 1;
	}
	//If the lazer goes past the top of the display move it back to a starting position.
	if (lazer.top > display.height) {
		lazer_fired = 0;
		lazer = start_lazer;
	}
	//Move the ship left when the rotary encoder is used.
	if (rotary>0 && ship.left >= SHIP_INC) {
		ship.left  -= SHIP_INC;
		ship.right -= SHIP_INC;
		start_lazer.left -= SHIP_INC;
		start_lazer.right -= SHIP_INC;
		if (lazer_fired == 0) {
			lazer.left  -= SHIP_INC;
			lazer.right -= SHIP_INC;	
		}
	}
	//Move the ship right when the rotary encoder is used.
	if (rotary<0 && ship.right < display.width-SHIP_INC) {
		ship.left  += SHIP_INC;
		ship.right += SHIP_INC;
		start_lazer.left += SHIP_INC;
		start_lazer.right += SHIP_INC;
		if (lazer_fired == 0) {
			lazer.left  += SHIP_INC;
			lazer.right += SHIP_INC;
		}
	}
	rotary = 0;

	//Check to see if the lazer hit any invaders
	int i;
	for (i = 0; i < NO_OF_INVADERS; i++) {
		if (lazer.left > (invadersCoords[i].x - 2) && 
			lazer.right < (invadersCoords[i].x + HIT_BOX_SIZE) && 
			lazer.top > (invadersCoords[i].y) && 
			lazer.bottom < (invadersCoords[i].y + HIT_BOX_SIZE) && 
			invadersVisible[i] != 0) 
		{
			invadersVisible[i] = 0;
			lazer_fired = 0;
			lazer = start_lazer;
			score++;
		}
	}

	//If the space invaders reach the end of the display make them move the other way.
	if (invadersCoords[NO_OF_INVADERS - 1].x > display.width - 45) {
		squadDist = -20;
	}

	if (invadersCoords[0].x < 40) {
		squadDist = 20;
	}

	//Change the position of all the space invaders
	if (move >= 75) {
		move = 0;
		for (i = 0; i < NO_OF_INVADERS; i++) {
			invadersCoords[i].x += squadDist;
		}
	}

	//Trigger invaders to launch projectiles
	if (zap_timer >= 100) {
		zap_timer = 0;
		zap_fired = 1;
		for (i = 0; i < 23; i++) {
			zapSequenceVal++;
			if (zapSequenceVal > 23) {
				zapSequenceVal = 0;
			}
			if (invadersVisible[zapSequence[zapSequenceVal]] != 0) {
				zap.x = invadersCoords[zapSequence[zapSequenceVal]].x + 8;
				zap.y = invadersCoords[zapSequence[zapSequenceVal]].y + 15;
				break;
			}
		}
		
	}

	if (zap_fired == 1) {
		zap.y += 2;
	}

 	//If zap reaches top of display, make it disappear.
	if (zap.y + 15 > display.height) {
		zap_fired = 0;
		zap.x = 0;
		zap.y = 0;
	}

	//If an invaders projectile makes contact with the ship subtract one life.
	if (zap.x < ship.right && zap.x + 3 > ship.left && zap.y + 14 > LCDWIDTH-SHIP_HEIGHT) {
		lives--;
		zap_fired = 0;
		zap.x = 0;
		zap.y = 0;
	}

	//If the lazer and invader projectile make contact they should both disappear and be reset.
	if (lazer_fired == 1 && lazer.top > zap.y - 3 && lazer.bottom < zap.y + 15 && lazer.left > zap.x-3 && lazer.right < zap.x + 8) {
		zap_fired = 0;
		zap.x = 0;
		zap.y = 0;
		lazer_fired = 0;
		lazer = start_lazer;
	}
}

ISR(TIMER3_COMPA_vect)
{
	fps = 0;
}

int main()
{
	/* Clear DIV8 to get 8MHz clock */
	CLKPR = (1 << CLKPCE);
	CLKPR = 0;
	init_rotary();
	init_led();
	init_lcd();
	set_frame_rate_hz(61); /* > 60 Hz  (KPZ 30.01.2015) */
	/* Enable tearing interrupt to get flicker free display */
	EIMSK |= _BV(INT6);
	/* Enable rotary interrupt to respond to input */
	EIMSK |= _BV(INT4) | _BV(INT5);
	/* Enable game timer interrupt (Timer 1 CTC Mode 4) */
	TCCR1A = 0;
	TCCR1B = _BV(WGM12);
	TCCR1B |= _BV(CS10);
	TIMSK1 |= _BV(OCIE1A);
	/* Enable performance counter (Timer 3 CTC Mode 4) */
	TCCR3A = 0;
	TCCR3B = _BV(WGM32);
	TCCR3B |= _BV(CS32);
	TIMSK3 |= _BV(OCIE3A);
	OCR3A = 31250;
	/* Play the game */
	do {
		lazer_fired = 0;
		last_ship = ship = start_ship;
		last_lazer = lazer = start_lazer = original_lazer;
		lives = 3;
		score = 0;
		int i;
		for (i = 0; i < NO_OF_INVADERS; i++) {
			invadersVisible[i] = 1;
		}
		OCR1A = 65535;
		led_on();
		sei();
		while(lives > 0 && score < NO_OF_INVADERS);
		cli();
		led_off();
		clear_screen();
		if (lives == 0) {
			display_string_xy("You Lost, Try again", (display.width/2) - 53, display.height/2);
		} else {
			display_string_xy("You Won", (display.width/2) - 20, display.height/2);
		}
		display_string_xy("Press the centre button to play again", 50, 230);
		PORTB |= _BV(PB6);
		while(PINE & _BV(SWC))
		{
			if (PINB & _BV(PB6))
				led_on();
			else
				led_off();
		}
		clear_screen();
	} while(1);
}
