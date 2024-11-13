

Create a demo virtual board.
* Virtual board written in C compiled to WebAssembly.
* It uses ccvm to emulate the CPU, hardware is emulated with WebAssembly/HTML
* ccvm calls callback on out of boundaries access, this gives possibility to implement registers.
* User editable "host.c" code embeds another ccvm instance that runs "guest.c" file.


Virtual HW emulation:
```
PORTA/PORTB (like AVR)
    DIR
    OUTPUT
    INTEN
    INTSET
    ...
    - some ports LCD panel emulated
    - LEDs
    - buttons
    - heater
    - cooler
    - motor with encoder ???

UART0 - sending ccvm bytecode
UART1 - terminal
    CFG
    DATA
    INT
    ...

TIMER0+PWM - timers and LED controlling
TIMER1+PWN
...
    ...

ADC - channels connected to temperature sensor and potentiometer



```