

#include <avr/pgmspace.h>
#include "pins_arduino.h"
#include "wiring_private.h"
#include "Arduino_TFTLCD.h"

#define TFTWIDTH   240
#define TFTHEIGHT  320

// If there's space, we can turn on software clipping to the screen
// boundaries. This isn't essential, and the fast drawing routines
// don't use it, but it could be nice in some cases. 
#ifndef SAVE_SPACE
    #define DO_CLIP
#endif 

// The leonardo doesn't have very much space on it.
// We have to convert some of the macros to function calls, sadly.
#ifdef SAVE_SPACE
    void COMMAND(uint8_t CMD) {
        _COMMAND(CMD);
    }
    void START_PIXEL_DATA() {
        _START_PIXEL_DATA();
    }
    void SEND_PAIR(uint8_t hi,uint8_t lo) {
        _SEND_PAIR(hi,lo);
    }
    void SEND_PERMUTED_PAIR(uint8_t hi,uint8_t lo) {
        _SEND_PERMUTED_PAIR(hi,lo);
    }
    void START_READING() {
        _START_READING();
    }
    void STOP_READING() {
        _STOP_READING();
    }
    void SET_XY_RANGE(uint8_t x0,uint8_t x1,uint16_t y0,uint16_t y1) {
        _SET_XY_RANGE(x0,x1,y0,y1);
    }
    void SET_XY_LOCATION(uint8_t x,uint16_t y) {
        _SET_XY_LOCATION(x,y);
    }
    void SET_X_LOCATION(uint8_t x) {
        _SET_X_LOCATION(x);
    }
    void SET_Y_LOCATION(uint16_t y) {
        _SET_Y_LOCATION(y);
    }
    void RESET_X_RANGE() {
        _RESET_X_RANGE();
    }
    void ZERO_XY() {
        _ZERO_XY();
    }
#endif

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS AND SYSTEM ROUTINES
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// Constructor for shield (fixed LCD control lines)
Arduino_TFTLCD::Arduino_TFTLCD(void) : Arduino_GFX(TFTWIDTH, TFTHEIGHT) {
    pinMode(A3, OUTPUT);    // Enable outputs
    pinMode(A2, OUTPUT);
    pinMode(A1, OUTPUT);
    pinMode(A0, OUTPUT);
    // reset
    digitalWrite(A4, HIGH);
    pinMode(A4, OUTPUT);
    init();
}

// Initialization common to both shield & breakout configs
void Arduino_TFTLCD::init(void) {
    setWriteDir(); // Set up LCD data port(s) for WRITE operations
    rotation  = 1;
    cursor_y  = cursor_x = 0;
    textsize  = 1;
    textcolor = 0xFFFF;
    _width    = TFTWIDTH;
    _height   = TFTHEIGHT;
}
    
// Need a creative solution to compress the size here. 
// Store initialization sequence as a table? 
#define DELAY_CODE 0
#define NCOMMANDS 11*3+2*6
PROGMEM const uint8_t initialization_commands[NCOMMANDS] = {
    DELAY_CODE           , 255,
    DELAY_CODE           , 255,
    ILI9341_SOFTRESET    , 0x00, 0x00,
    DELAY_CODE           , 255,
    ILI9341_DISPLAYOFF   , 0x00, 0x00,
    ILI9341_POWERCONTROL1, 0x23, 0x00,
    ILI9341_POWERCONTROL2, 0x10, 0x00,
    ILI9341_VCOMCONTROL1 , 0x2B, 0x2B,
    ILI9341_VCOMCONTROL2 , 0xC0, 0x00,
    ILI9341_MEMCONTROL   , ILI9341_MADCTL_MY|ILI9341_MADCTL_BGR, 0x00,
    ILI9341_PIXELFORMAT  , 0x55, 0x00,
    ILI9341_FRAMECONTROL , 0x00, 0x1B,
    ILI9341_SLEEPOUT     , 0x00, 0x00,
    DELAY_CODE           , 255,
    ILI9341_DISPLAYON    , 0x00, 0x00,
    DELAY_CODE           , 255,
    DELAY_CODE           , 255};
uint8_t get_init_command(uint8_t i) {
    return (uint8_t)pgm_read_byte(&initialization_commands[i]);
}
void send_byte(uint8_t byte) {
    WRITE_BUS(byte);
    CLOCK_DATA;
}
void Arduino_TFTLCD::begin() {
    ALL_IDLE;
    RS_LOW;
    delay(200);
    RS_HIGH;
    for(uint8_t i=0; i<4; i++) COMMAND(0);
    uint8_t hi,lo,code;
    for (uint16_t i=0; i<NCOMMANDS;) {
        code = get_init_command(i++);
        hi   = get_init_command(i++);
        if (code==DELAY_CODE) {
            delay(hi);
        } else {
            COMMAND(code);
            send_byte(hi);
            if (lo = get_init_command(i++)) {
                send_byte(lo);
                CLOCK_DATA;
            }
        }
    }
}

void Arduino_TFTLCD::setRotation(uint8_t x) {

	rotation = (x & 3);
	switch (rotation) {
	case 0:
	case 2:
		_width = WIDTH;
		_height = HEIGHT;
		break;
	case 1:
	case 3:
		_width = HEIGHT;
		_height = WIDTH;
		break;
	}

	// perform hardware-specific rotation operations...

	CS_ACTIVE;

	uint16_t t = 0;

	switch (rotation) {
		case 2:
		t = ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR;
		break;
    case 3:
		t = ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR;
		break;
    case 0:
		t = ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR;
		break;
    case 1:
		t = ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_MV |
          ILI9341_MADCTL_BGR;
      break;
    }
	
	CD_COMMAND;                                                                
    write8(ILI9341_MADCTL);                                                                 
    CD_DATA;                                                                   
    write8(t); 
	SET_WINDOW(0,0,_width - 1, _height - 1);

}

// Control the low color mode
void Arduino_TFTLCD::set_low_color_mode(uint8_t ison) {
    COMMAND(ison?LOW_COLOR_MODE_ON:LOW_COLOR_MODE_OFF);
}


////////////////////////////////////////////////////////////////////////////
// BASIC DRAWING ROUTINES
////////////////////////////////////////////////////////////////////////////

void Arduino_TFTLCD::fastFlood(uint8_t c, uint16_t l) {
    flood( 0x0101*c, l);
}

// Very fast flood routine. 
void Arduino_TFTLCD::flood(uint16_t color, uint32_t len) {
    uint8_t  i;
    uint8_t hi = IDENTITY(color>>8);
    uint8_t lo = IDENTITY(color);
    START_PIXEL_DATA();
    #ifdef SAVE_SPACE
        /*
        hi = BIT_TO_PORT_PERMUTATION(hi);
        lo = BIT_TO_PORT_PERMUTATION(lo);
        do {
            SEND_PERMUTED_PAIR(hi,lo);
            len--;
        } while (len>0);
        */
        hi = BIT_TO_PORT_PERMUTATION(hi);
        WRITE_PERMUTED_BUS(hi);
        while (len>=8) {
            CLOCK_8;
            len-=8;
        } 
        if (len &0b00000100) { CLOCK_4; }
        if (len &0b00000010) { CLOCK_2; }
        if (len &0b00000001) { CLOCK_1; }
    #else
        if(hi == lo) {
            WRITE_BUS(color);
            while (len>=128) {
                CLOCK_128;
                len-=128;
            } 
            if (len &0b01000000) { CLOCK_64; }
            if (len &0b00100000) { CLOCK_32; }
            if (len &0b00010000) { CLOCK_16; }
            if (len &0b00001000) { CLOCK_8; }
            if (len &0b00000100) { CLOCK_4; }
            if (len &0b00000010) { CLOCK_2; }
            if (len &0b00000001) { CLOCK_1; }
        } else {
            uint16_t blocks = (uint16_t)(len/32); 
            while(blocks--) {
              i = 4;
              do {
                SEND_PAIR(hi,lo); SEND_PAIR(hi,lo); 
                SEND_PAIR(hi,lo); SEND_PAIR(hi,lo);
                SEND_PAIR(hi,lo); SEND_PAIR(hi,lo); 
                SEND_PAIR(hi,lo); SEND_PAIR(hi,lo);
              } while(--i);
            }
            for(i = (uint8_t)len&31; i--; ) SEND_PAIR(hi,lo);
        }
    #endif
}

void Arduino_TFTLCD::fillRect(int16_t x1, int16_t y1, int16_t w, int16_t h, 
  uint16_t fillcolor) {
    int16_t  x2=x1+w-1, y2=y1+h-1;
    #ifdef DO_CLIP
        if(w<=0||h<=0||x1>=_width||y1>=_height||x2<0||y2<0) return;
        if(x1<0) {w+=x1;x1=0;}
        if(y1<0) {h+=y1;y1=0;}
        if(x2>=_width) {x2=_width-1;w=x2-x1+1;}
        if(y2>=_height){y2=_height-1;h=y2-y1+1;}
    #endif
    SET_XY_RANGE(x1,x2,y1);
    flood(fillcolor, (uint32_t)w * (uint32_t)h);
    RESET_X_RANGE();
}

void Arduino_TFTLCD::fillScreen(uint16_t color) {
    ZERO_XY();
    flood(color, (long)TFTWIDTH * (long)TFTHEIGHT);
}

void Arduino_TFTLCD::colorPixel(uint16_t y, uint16_t permuted_color) {
    uint8_t permuted_line_flag = PERMUTED_FRAME_ID_FLAG8*(y&1);
    uint8_t permuted_mask_test = BIT_TO_PORT_PERMUTATION(mask_flag)^permuted_line_flag;
    if (do_masking) {
        START_READING();
        DELAY1
        uint8_t R = PERMUTED_QUICK_READ;
        STOP_READING();
        if ((R&PERMUTED_FRAME_ID_FLAG8)==permuted_mask_test) return;
    } else {
        permuted_color &= PERMUTED_FRAME_ID_MASK16;
        permuted_color |= (uint16_t)(permuted_mask_test)<< 8;
    }
    START_PIXEL_DATA();
    SEND_PERMUTED_PIXEL(permuted_color);
}

void Arduino_TFTLCD::drawPixel(int16_t x, int16_t y, uint16_t color) {
    #ifdef DO_CLIP
        if((x<0)||(y<0)||(x>=_width)||(y>=_height)) return;
    #endif
    SET_XY_LOCATION(x,y);
    BIT_TO_PORT_PERMUTATION_16(color);
    colorPixel(y,color);
}

void Arduino_TFTLCD::drawFastVLine(int16_t x, int16_t y, int16_t length, uint16_t color)
{
    #ifdef DO_CLIP
        int16_t y2=y+length-1;
        if(length<=0||x<0||x>=_width||y>=_height||y2<0) return;
        if(y<0) {length+=y;y=0;}
        if(y2>=_height) {y2=_height-1;length=y2-y+1;}
    #endif
    SET_XY_RANGE(x,x,y);
    flood(color, length);
    RESET_X_RANGE();
}


// When drawing triangles, large contiguous areas will 
// be masked out. So instead we store a list of offsets and
// lengths that are /not/ masked out, and just draw those
// To do this, we start reading the color data. If it is masked, 
// we continue until it is not masked, and mark that position.
// We keep reading unmasked data until we come to a masked pixel,
// or are at the end of the line. We then draw the pixel data.
// We use the continue read data to pick up where we left off.
void Arduino_TFTLCD::drawFastHLine(int16_t x, int16_t y, int16_t length, uint16_t color){
    // if (length<1) return;
    // #ifdef DO_CLIP
        // int16_t x2 = x+length-1;
        // if(length<=0||y<0||y>=_height||x>=_width||x2<0) return;
        // if(x<0) {length+=x; x=0;}
        // if(x2>=_width) {x2=_width-1; length=x2-x+1;}
    // #endif
    // uint8_t line_flag = FRAME_ID_FLAG8*(y&1);
    // uint8_t permuted_mask_test = BIT_TO_PORT_PERMUTATION(mask_flag^line_flag);
    // uint8_t permuted_background_mask = BIT_TO_PORT_PERMUTATION( (background_color>>8) & QUICK_COLOR_MASK );
    // if (!do_masking) {
        // color &= FRAME_ID_MASK16;
        // color |= (uint16_t)(mask_flag^line_flag)<< 8;
    // }
    // color >>= 8;
    // SET_Y_LOCATION(y);
    // if (do_masking || do_overdraw) {
        // uint8_t in_segment=0;
        // uint8_t start=x;
        // uint8_t stop =x+length;
        // uint8_t i=x;
        // while (i<stop) {
            // SET_X_LOCATION(i);
            // START_READING();
            // while (i<stop) {
                // uint8_t read = PERMUTED_QUICK_READ;
                // uint8_t is_masked = (read&PERMUTED_QUICK_COLOR_MASK)!=permuted_background_mask
                                 // && (read&PERMUTED_FRAME_ID_FLAG8)==permuted_mask_test;
                // SEND_DATA; 
                // READY_READ;
                // SEND_DATA; 
                // if (is_masked) {
                    // if (in_segment) {
                        // STOP_READING();
                        // SET_X_LOCATION(start);
                        // fastFlood(color,i-start);
                        // in_segment=0;
                        // start=i;
                        // i++;
                        // break;
                    // }
                // }
                // else if (!in_segment) {
                    // start = i;
                    // in_segment = 1;
                // }     
                // READY_READ;
                // i++;
            // }
        // }
        // STOP_READING();
        // if (in_segment) {
            // SET_X_LOCATION(start);
            // fastFlood(color,i-start);
        // }
    // } else {
        // SET_X_LOCATION(x);
        // fastFlood(color,length);
    // }
	drawLine(x, y, x+length-1, y, color);
}


void Arduino_TFTLCD::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)   { swap(x0, y0); swap(x1, y1); }
    if (x0 > x1) { swap(x0, x1); swap(y0, y1); }
    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep;
    BIT_TO_PORT_PERMUTATION_16(color);
    ystep = y0<y1?1:-1;
    if (steep) {  
        SET_X_LOCATION(y0);
        for (; x0<=x1; x0++) {
            SET_Y_LOCATION(x0);
            colorPixel(x0,color);
            err -= dy;
            if (err < 0) {
                y0 += ystep;
                SET_X_LOCATION(y0);
                err += dx;
            }
        }
    } else {
        SET_Y_LOCATION(y0);
        for (; x0<=x1; x0++) {
            SET_X_LOCATION(x0);
            colorPixel(y0,color);
            err -= dy;
            if (err < 0) {
                y0 += ystep;
                SET_Y_LOCATION(y0);
                err += dx;
            }
        }
    }
}  


////////////////////////////////////////////////////////////////////////////
// ROUTINES FOR MASKING AND OVERDRAW
////////////////////////////////////////////////////////////////////////////

// Functions for controlling masked and overdrawn rendering
void     Arduino_TFTLCD::overdraw_on()  {do_overdraw = 1;}
void     Arduino_TFTLCD::overdraw_off() {do_overdraw = 0;}
void     Arduino_TFTLCD::masking_on()   {do_masking  = 1;}
void     Arduino_TFTLCD::masking_off()  {do_masking  = 0;}
void     Arduino_TFTLCD::flip_mask()    {mask_flag  ^= FRAME_ID_FLAG8;}

////////////////////////////////////////////////////////////////////////////
// LOW-LEVEL DATA IO ROUTINES
////////////////////////////////////////////////////////////////////////////

uint16_t Arduino_TFTLCD::readPixel(int16_t x, int16_t y) {
    return 0;
}




////////////////////////////////////////////////////////////////////////////
// Fast drawing extensions.
// Support only a limited color pallet
////////////////////////////////////////////////////////////////////////////


/** X-ORs Pixel data with mask
 *  Fast mode: only top 6 bits of first byte of color data is considered
 *  The High and Low bytes of color data are duplicated.  
 */
void Arduino_TFTLCD::fastXORFlood(uint8_t mask, uint8_t length) {
    // bless avr-gcc for supporting variable length arrays on the stack
    uint8_t colors[length];
    mask = BIT_TO_PORT_PERMUTATION(mask);
    // First read the pixels
    START_READING();       
    for(uint16_t i=0; i<length; i++) {
        uint8_t read = PERMUTED_QUICK_READ;
        SEND_DATA;
        READY_READ;
        SEND_DATA;
        colors[i] = read^mask;
        READY_READ;
    }
    STOP_READING();
    START_PIXEL_DATA();
    for(uint16_t i=0; i<length; i++) {
        WRITE_PERMUTED_BUS(colors[i]);
        CLOCK_1;
    }
}

void Arduino_TFTLCD::fastFillScreen(uint8_t color) {
#ifdef SAVE_SPACE
    fillScreen(color*0x0101);
#else
    ZERO_XY();
    START_PIXEL_DATA();
    WRITE_BUS_FAST(color);
    for (uint16_t i=0; i<300; i++) CLOCK_256; 
#endif
}

void Arduino_TFTLCD::fastPixel(uint8_t x, uint16_t y, uint8_t color) {
#ifdef SAVE_SPACE
    drawPixel(x,y,color*0x0101);
#else
    SET_XY_LOCATION(x,y);
    START_PIXEL_DATA();
    WRITE_BUS(color);
    CLOCK_1;
#endif
}

void Arduino_TFTLCD::fastXORRect(uint8_t x, uint16_t y, uint8_t w, uint16_t h, uint8_t mask) {
    uint8_t  x2=x+w-1;
    SET_X_RANGE(x,x2);
    for (int i=0; i<h; i++) {
        SET_Y_LOCATION(i+y);
        fastXORFlood(mask,w);
    }
    RESET_X_RANGE();
}

void Arduino_TFTLCD::fastFillRect(uint8_t x, uint16_t y, uint8_t w, uint16_t h, uint8_t c) {
#ifdef SAVE_SPACE
    fillRect(x,y,w,h,c*0x0101);
#else
    uint8_t  x2=x+w-1;
    SET_XY_RANGE(x,x2,y);
    fastFlood(c,w*h);
    RESET_X_RANGE();
#endif
}

void Arduino_TFTLCD::fastDrawRect(uint8_t x, uint16_t y, uint8_t w, uint16_t h, uint8_t c) {
#ifdef SAVE_SPACE
    drawRect(x,y,w,h,c*0x0101);
#else
    fastestHLine(x, y, w, c);
    fastestHLine(x, y+h-1, w, c);
    fastestVLine(x, y, h, c);
    fastestVLine(x+w-1, y, h, c);
#endif
}

void Arduino_TFTLCD::fastestVLine(uint8_t x, uint16_t y, uint16_t h, uint8_t color) {
    SET_XY_RANGE(x,x,y);
    fastFlood(color,h);
    RESET_X_RANGE();
}

void Arduino_TFTLCD::fastestHLine(uint8_t x, uint16_t y, uint16_t w, uint8_t color) {
    if (!do_masking) {
        uint8_t line_flag = FRAME_ID_FLAG8*(y&1);
        color &= FRAME_ID_MASK8;
        color |= mask_flag^line_flag;
    }
    SET_XY_LOCATION(x,y);
    fastFlood(color,w);
}

void Arduino_TFTLCD::fastDrawTriangle(
    uint8_t x0, uint16_t y0, 
    uint8_t x1, uint16_t y1, 
    uint8_t x2, uint16_t y2, uint8_t color) {
  fastLine(x0, y0, x1, y1, color);
  fastLine(x1, y1, x2, y2, color);
  fastLine(x2, y2, x0, y0, color);
}

void Arduino_TFTLCD::fastFillTriangle(
    uint8_t _x0, uint16_t _y0, 
    uint8_t _x1, uint16_t _y1, 
    uint8_t _x2, uint16_t _y2, uint8_t color) {
    // There's sort of a numerical problem I'm still pinning down with
    // using the short unsigned types. Just convert them for now
    int x0 = _x0;
    int x1 = _x1;
    int x2 = _x2;
    int y0 = _y0;
    int y1 = _y1;
    int y2 = _y2;
#ifdef SAVE_SPACE
    fillTriangle(x0,y0,x1,y1,x2,y2,color*0x0101);
#else
    int16_t a, b, y;
    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1) { swap(y0, y1); swap(x0, x1); }
    if (y1 > y2) { swap(y2, y1); swap(x2, x1); }
    if (y0 > y1) { swap(y0, y1); swap(x0, x1); }
    if(y0 == y2)  return;
    int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;
    int16_t last = y1==y2? y1 : y1-1; 
    sa += dx01;
    sb += dx02;
    for(y=y0+1; y<=last; y++) {
        a   = x0 + sa / dy01;
        b   = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        if(a > b) swap(a,b);
        fastestHLine(a, y, b-a, color);
    }
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for(; y<y2; y++) {
        a   = x1 + sa / dy12;
        b   = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        if(a > b) swap(a,b);
        fastestHLine(a, y, b-a, color);
    }
#endif
}

uint16_t Arduino_TFTLCD::color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


// fastest way to draw a line. 
// Sacrifices color accuracy for speed.
// Does not support masking.
void Arduino_TFTLCD::fastLine(
    uint8_t x0, uint16_t y0, 
    uint8_t x1, uint16_t y1, 
    uint8_t color) {
    uint16_t dx = x1>x0?x1-x0:x0-x1;
    uint16_t dy = y1>y0?y1-y0:y0-y1;
    
    color &= FRAME_ID_MASK8;
    
    if (dy>dx) {
        if (y0 > y1) { 
            swapU16(y0, y1); 
            swapU8(x0, x1); 
        }
        int16_t err = dy/2;
        int16_t xstep = x0<x1?1:-1;
        SET_XY_RANGE(x0,x0,y0);
        START_PIXEL_DATA();
        while (y0<=y1) {
            WRITE_BUS(color|mask_flag^(FRAME_ID_FLAG8*(y0&1)));
            CLOCK_1; 
            y0++;
            err -= dx;
            if (err < 0) {
              x0 += xstep;
              SET_XY_RANGE(x0,x0,y0);
              START_PIXEL_DATA();
              err += dy;
            }
        }
        RESET_X_RANGE();
    } else {
        if (x0 > x1) { 
            swapU16(y0, y1); 
            swapU8(x0, x1); 
        }
        int16_t err = dx/2;
        int16_t ystep = y0<y1?1:-1;
        SET_XY_LOCATION(x0,y0);
        START_PIXEL_DATA();
        WRITE_BUS(color|mask_flag^(FRAME_ID_FLAG8*(y0&1)));
        while (x0<=x1) {
            CLOCK_1;
            err -= dy;
            x0++;
            if (err < 0) {
                y0 += ystep;
                SET_XY_LOCATION(x0,y0);
                START_PIXEL_DATA();
                WRITE_BUS(color|mask_flag^(FRAME_ID_FLAG8*(y0&1)));
                err += dx;
            }
        }
    }
}  


