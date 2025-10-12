#ifndef BSW_DRIVER_HEADLIGHT_H_
#define BSW_DRIVER_HEADLIGHT_H_

//#include "Ifx_Types.h"

#include "GPIO.h"
#include "Evadc.h"
#include "gtm_atom_pwm.h"
#include "my_stdio.h"

void HBA_ON(void);
void HBA_OFF(void);
void HBA_Init(void);

extern volatile uint8 g_headlight_brightness;

void set_headlight_brightness(uint8 brightness);
uint8 get_headlight_brightness(void);

#endif /* BSW_DRIVER_HEADLIGHT_H_ */
