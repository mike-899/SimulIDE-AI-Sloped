#include <avr/io.h>
#include <util/delay.h>
/* Blink PB3 (pin 2) and PB4 (pin 3) in antiphase */
int main(void){
  DDRB = (1<<PB3)|(1<<PB4);
  PORTB = (1<<PB3);
  for(;;){ _delay_ms(10); PORTB ^= (1<<PB3)|(1<<PB4); }
}
