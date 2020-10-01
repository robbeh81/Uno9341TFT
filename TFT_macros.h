
//////////////////////////////////////////////////////////////////////////
// Platform-independent IO macros

#define RS_FLAG ((uint8_t)1<<RS_PIN)
#define CS_FLAG ((uint8_t)1<<CS_PIN)
#define CD_FLAG ((uint8_t)1<<CD_PIN)
#define WR_FLAG ((uint8_t)1<<WR_PIN)
#define RD_FLAG ((uint8_t)1<<RD_PIN)

#define CONTROL_MASK (RS_FLAG|CS_FLAG|CD_FLAG|WR_FLAG|RD_FLAG)
#define TFTDEFAULT (~CONTROL_MASK|RS_FLAG)

#define READY_COMMAND     CONTROLPORT=(TFTDEFAULT|RD_FLAG)
#define SEND_COMMAND      CONTROLPORT=(TFTDEFAULT|RD_FLAG|WR_FLAG)
#define READY_DATA        CONTROLPORT=(TFTDEFAULT|CD_FLAG|RD_FLAG)
#define SEND_DATA         CONTROLPORT=(TFTDEFAULT|CD_FLAG|WR_FLAG|RD_FLAG)
#define READY_READ        CONTROLPORT=(TFTDEFAULT|CD_FLAG|WR_FLAG)
#define REQUEST_READ      CONTROLPORT=(TFTDEFAULT|WR_FLAG)

#define ALL_IDLE   CONTROLPORT = TFTDEFAULT|CS_FLAG

#define CS_IDLE    CONTROLPORT |=  CS_FLAG
#define CS_ACTIVE  CONTROLPORT &= ~CS_FLAG
#define RD_ACTIVE  CONTROLPORT &= ~RD_FLAG
#define RD_IDLE    CONTROLPORT |=  RD_FLAG
#define WR_ACTIVE  CONTROLPORT &= ~WR_FLAG
#define WR_IDLE    CONTROLPORT |=  WR_FLAG
#define RS_LOW     CONTROLPORT &= ~WR_FLAG
#define RS_HIGH    CONTROLPORT |=  WR_FLAG
#define CD_COMMAND CONTROLPORT &= ~CD_FLAG
#define CD_DATA    CONTROLPORT |=  CD_FLAG
   
// Macros for masked rendering -- set FRAME_ID_BIT based on platform
#define FRAME_ID_FLAG16   ((uint16_t)0x100<<FRAME_ID_BIT)
#define FRAME_ID_MASK16  ~FRAME_ID_FLAG16
#define FRAME_ID_FLAG8    ((uint8_t)1<<FRAME_ID_BIT)
#define FRAME_ID_MASK8   ~FRAME_ID_FLAG8
#define QUICK_COLOR_MASK  (QUICK_READ_MASK^FRAME_ID_FLAG8)

   
// Data write strobe, ~2 instructions and always inline
#define WR_STROBE {\
    WR_ACTIVE;\
    WR_IDLE;\
}

#define write8(d) {\
    WRITE_BUS(d);\
    WR_STROBE;\
}

#define read8(result) {\
    RD_ACTIVE;\
    DELAY7;\
    result = READ_BYTE;\
    RD_IDLE;\
}

#define CLOCK_DATA {\
    READY_DATA;\
    SEND_DATA;\
}\

#define _COMMAND(CMD) {\
    WRITE_BUS(CMD);\
    READY_COMMAND;\
    SEND_COMMAND;\
}

#define _START_PIXEL_DATA() {\
    COMMAND(BEGIN_PIXEL_DATA);\
}

#define _SEND_PAIR(hi,lo) {\
    WRITE_BUS(hi);\
    CLOCK_DATA;\
    WRITE_BUS(lo);\
    CLOCK_DATA;\
}

///////////////////////////////////////////////////////////////////////////
// Permuted reads and writes -- arrange color bits to match pins and ports
// nicely in advance. Important for good performance on Leonardo.

#define PERMUTED_FRAME_ID_FLAG8  BIT_TO_PORT_PERMUTATION(FRAME_ID_FLAG8)
#define PERMUTED_FRAME_ID_FLAG16 (((uint16_t)PERMUTED_FRAME_ID_FLAG8)<<8)
#define PERMUTED_FRAME_ID_MASK16 (~PERMUTED_FRAME_ID_FLAG16)
#define PERMUTED_FRAME_ID_MASK8  (~PERMUTED_FRAME_ID_FLAG8)
#define PERMUTED_QUICK_READ_MASK  BIT_TO_PORT_PERMUTATION(QUICK_READ_MASK)
#define PERMUTED_QUICK_COLOR_MASK BIT_TO_PORT_PERMUTATION(QUICK_COLOR_MASK)

#define BIT_TO_PORT_PERMUTATION_16(c) {\
    uint8_t hi = (c)>>8;\
    uint8_t lo = (c);\
    hi = BIT_TO_PORT_PERMUTATION(hi);\
    lo = BIT_TO_PORT_PERMUTATION(lo);\
    c = lo | hi<<8;\
}

// Color-data can be permuted in advance for faster IO operations.
// This is important on the Leonardo, where the pin locations don't
// map to the bus lines in an orderly way. 
#define _SEND_PERMUTED_PAIR(hi,lo) {\
    uint8_t _hi = (hi);\
    uint8_t _lo = (lo);\
    WRITE_PERMUTED_BUS(_hi);\
    CLOCK_DATA;\
    WRITE_PERMUTED_BUS(_lo);\
    CLOCK_DATA;\
}

#define SEND_PIXEL(color) {\
    SEND_PAIR(color>>8,color);\
}

#define SEND_PERMUTED_PIXEL(color) {\
    SEND_PERMUTED_PAIR(color>>8,color);\
}

#define SEND_FAST(c) {\
    WRITE_BUS(c);\
    CLOCK_DATA;\
    CLOCK_DATA;\
}

// for loop unrolling
#define CLOCK_1   {CLOCK_DATA; CLOCK_DATA;};
#define CLOCK_2   {CLOCK_1   CLOCK_1};
#define CLOCK_4   {CLOCK_2   CLOCK_2};
#define CLOCK_8   {CLOCK_4   CLOCK_4};
#define CLOCK_16  {CLOCK_8   CLOCK_8};
#define CLOCK_32  {CLOCK_16  CLOCK_16};
#define CLOCK_64  {CLOCK_32  CLOCK_32};
#define CLOCK_128 {CLOCK_64  CLOCK_64};
#define CLOCK_256 {CLOCK_128 CLOCK_128};


//////////////////////////////////////////////////////////////////////////
// Configure drawing region


#define _ZERO_XY() {\
    COMMAND(SET_COLUMN_ADDRESS_WINDOW);\
    WRITE_BUS(0);\
    CLOCK_DATA;\
    CLOCK_DATA;\
    COMMAND(SET_ROW_ADDRESS_WINDOW);\
    WRITE_BUS(0);\
    CLOCK_DATA;\
    CLOCK_DATA;\
}

// Set the X location 
// Because x in 0..239, top byte is always 0
#define _SET_X_LOCATION(x) {\
    COMMAND(SET_COLUMN_ADDRESS_WINDOW);\
    SEND_PAIR((x)>>8,x);\
}

// Set the Y location
#define _SET_Y_LOCATION(y) {\
    COMMAND(SET_ROW_ADDRESS_WINDOW);\
    SEND_PAIR((y)>>8,(y));\
}

// Set both X and Y location
#define _SET_XY_LOCATION(x,y) {\
    SET_X_LOCATION(x);\
    SET_Y_LOCATION(y);\
}

// Set X range
#define SET_X_RANGE(x1,x2) {\
    COMMAND(SET_COLUMN_ADDRESS_WINDOW);\
    SEND_PAIR((x1)>>8,(x1));\
    SEND_PAIR((x2)>>8,(x2));\
}

// Set Y range
#define SET_Y_RANGE(y1,y2) {\
    COMMAND(SET_ROW_ADDRESS_WINDOW);\
    SEND_PAIR((y1)>>8,(y1));\
    SEND_PAIR((y2)>>8,(y2));\
}

// Set X range and Y location
#define _SET_XY_RANGE(x1,x2,y) {\
    SET_X_RANGE(x1,x2);\
    SET_Y_LOCATION(y);\
}

// Sets X range to (0,_width)
#define _RESET_X_RANGE() {\
	SET_X_RANGE(0,_width);\
}

/*
Sets the LCD address window (and address counter, on 932X).
Relevant to rect/screen fills and H/V lines.  Input coordinates are
assumed pre-sorted (e.g. x2 >= x1).
This does not set the upper bounds for the row information. This is left
at the initialization value of 319. There is no reason to ever change this
register. 
*/
#define SET_WINDOW(x1,y1,x2,y2) {\
	SET_X_RANGE(x1,x2);\
	SET_Y_RANGE(y1,y2);\
}

/*
In order to save a few register writes on each pixel drawn, the lower-right
corner of the address window is reset after most fill operations, so that 
drawPixel only needs to change the upper left each time.
The row range is set to 0..239
The col range is set to 0..Current
Transmission of the end of the column range command is ommitted, as this
should be set to 319 during initialization and is never changed by the
driver code. As long as no other code sends data afterwards, this register
will remain set to its original value. This driver does not send data
without issuing a command first, so this works. However, it could interact
poorly with user code if the chip select line for the touch screen is not
disabled prior to performing IO operations on PORTC. 
*/
#define SET_LR(void) {\
    RESET_X_RANGE();\
    ZERO_Y();\
}


//////////////////////////////////////////////////////////////////////////
// Start reading pixel data
#define _START_READING() {\
    COMMAND(BEGIN_READ_DATA);\
    setReadDir();\
    READY_READ;\
    SEND_DATA;\
    DELAY1;\
    READY_READ;\
    DELAY1;\
}

// Finish reading pixel data
#define _STOP_READING() {\
    setWriteDir();\
}

// Leonardo can't use macros because there isn't enough space
// Other Arduino version can though
#ifndef SAVE_SPACE
    #define ZERO_XY() _ZERO_XY()
    #define RESET_X_RANGE() _RESET_X_RANGE()
    #define SET_X_LOCATION(x) _SET_X_LOCATION(x)
    #define SET_Y_LOCATION(y) _SET_Y_LOCATION(y)
    #define SET_XY_LOCATION(x,y) _SET_XY_LOCATION(x,y)
    #define SET_XY_RANGE(x1,x2,y) _SET_XY_RANGE(x1,x2,y)
    #define STOP_READING() _STOP_READING()
    #define START_READING() _START_READING()
    #define COMMAND(CMD) _COMMAND(CMD)
    #define START_PIXEL_DATA() _START_PIXEL_DATA()
    #define SEND_PAIR(hi,lo) _SEND_PAIR(hi,lo)
    #define SEND_PERMUTED_PAIR(hi,lo) _SEND_PAIR(hi,lo)
#endif




