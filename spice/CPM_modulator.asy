Version 4
SymbolType CELL
RECTANGLE Normal 79 48 -64 -80
TEXT 7 -64 Center 2 CPM
WINDOW 0 2 -80 Bottom 2
WINDOW 39 2 64 Center 2
SYMATTR SpiceLine fs=100kHz, Va=0
SYMATTR Prefix X
SYMATTR Value CPM_modulator
SYMATTR SpiceLine2 Vcmax=5, Dmin=0, Dmax=0.9, Voffset=0
SYMATTR Description Current programmed mode (peak current mode) modulator
PIN -64 -32 LEFT 8
PINATTR PinName vc
PINATTR SpiceOrder 1
PIN -64 16 LEFT 8
PINATTR PinName vs
PINATTR SpiceOrder 2
PIN 80 -32 RIGHT 8
PINATTR PinName PWM
PINATTR SpiceOrder 3
PIN 80 16 RIGHT 8
PINATTR PinName ramp
PINATTR SpiceOrder 4
