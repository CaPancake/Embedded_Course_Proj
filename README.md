# Embedded_Course_Proj
* A project depicting vault management. STM32L552ZE + LCD (includes GPIO &amp; UART &amp; Timers)
* Overall idea: On startup or reset, with UART via TeraTerm, the user is prompted to decide on the safe's code. After successful setup, the safe's status is displayed via the connected LCD. Nucleo's ADC buttons are decoded into digits in range of 0-4, with the user prompted to maximum 3 attempts. Reset can be done through a long press on user button.
* NOTE: LCD driver code was provided by the course instructor and thus not uploaded here. 
