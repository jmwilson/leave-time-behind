Version 4
SymbolType CELL
RECTANGLE Normal 144 64 -112 -48
TEXT -80 51 Center 2 vc
TEXT -15 51 Center 2 vs
TEXT 48 52 Center 2 vm1
TEXT 112 51 Center 2 vm2
TEXT 15 -35 Center 2 d
WINDOW 39 154 -15 Left 2
WINDOW 40 155 8 Left 2
WINDOW 3 17 4 Center 2
SYMATTR SpiceLine Rf=1 Va=0.6
SYMATTR SpiceLine2 L=35u fs=200k
SYMATTR Value CPM-CCM
SYMATTR Prefix X
SYMATTR Description CPM model
PIN -80 64 NONE 8
PINATTR PinName control
PINATTR SpiceOrder 1
PIN -16 64 NONE 8
PINATTR PinName current
PINATTR SpiceOrder 2
PIN 48 64 NONE 8
PINATTR PinName 1
PINATTR SpiceOrder 3
PIN 112 64 NONE 8
PINATTR PinName 2
PINATTR SpiceOrder 4
PIN 16 -48 NONE 8
PINATTR PinName d
PINATTR SpiceOrder 5
