
#include "clunet.h"
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>

#define RBUF_SZ		250
static char rbuf[RBUF_SZ+1];
static uint8_t rbuf_p=0;

#define F_ECHO	0x01
static uint8_t flags=0;


void hex2byte1 (char *hp, uint8_t *b) {
    *b=0;
    char *lp=hp+1;
    if ((*lp>='0')&&(*lp<='9')) {
    *b+=(*lp-'0');
    }
    if ((*lp>='a')&&(*lp<='f')) {
    *b+=(*lp-'a'+10);
    }
    if ((*lp>='A')&&(*lp<='F')) {
    *b+=(*lp-'A'+10);
    }
    if ((*hp>='0')&&(*hp<='9')) {
    *b+=((*hp-'0')*16);
    }
    if ((*hp>='a')&&(*hp<='f')) {
    *b+=((*hp-'a'+10)*16);
    }
    if ((*hp>='A')&&(*hp<='F')) {
    *b+=((*hp-'A'+10)*16);
    }
    return;
}

uint8_t hex2byte(char *hp) {
    uint8_t b;
    hex2byte1 (hp, &b);
    return b;
}

void byte2hex (uint8_t *b, char *hp) {
    uint8_t c=(*b&0xf0)>>4;
    if (c>9) c+='A'-10; else c+='0';
    *hp=c;
    c=*b&0x0f;
    if (c>9) c+='A'-10; else c+='0';
    *(hp+1)=c;
    return;
}

uint8_t byte2hex_hi(uint8_t b) {
    uint8_t h=(b&0xf0)>>4;
    if (h>9) h+='A'-10; else h+='0';
    return h;
}

uint8_t byte2hex_lo(uint8_t b) {
    uint8_t l=b&0x0f;
    if (l>9) l+='A'-10; else l+='0';
    return l;
}



void USARTInit(uint16_t ubrr_value) {
   UBRRL = ubrr_value;
   UBRRH = (ubrr_value>>8);
   //UCSRA = _BV(U2X);
   UCSRB=(1<<RXEN)|(1<<TXEN);
   UCSRC=(1<<URSEL)|(3<<UCSZ0);
   return;
}



void USART_TransmitByte( unsigned char data ) {
    while (!(UCSRA&(1<<UDRE)));
    UDR = data;
    return;
}

void USART_TransmitHex( unsigned char data ) {
    unsigned char h = data>>4;
    char ho = (h < 10) ? (h+'0') : (h+'A'-10);
    unsigned char l = data & 0xF;
    char lo = (l < 10) ? (l+'0') : (l+'A'-10);
    while ( !( UCSRA & (1<<UDRE)) );
    UDR = ho;
    while ( !( UCSRA & (1<<UDRE)) );
    UDR = lo;
}

void USART_TransmitText(char* data) {
    while (*data != 0) {
	while ( !( UCSRA & (1<<UDRE)) );
	UDR = *data;
	data++;
    }
}

void USART_Transmit(void* p, unsigned long int len) {
    unsigned char* buff = (unsigned char*)p;
    unsigned long int b;
    for (b = 0; b < len; b++) USART_TransmitByte(buff[b]);
}

char USART_ReadChar() {
   while(!(UCSRA & (1<<RXC))) {
   }
   return UDR;
}



void data_received(uint8_t src_address, uint8_t command, char* data, uint8_t size) {
    PORTB^=0x01;
    USART_TransmitText("P");
    USART_TransmitByte(byte2hex_hi(src_address));
    USART_TransmitByte(byte2hex_lo(src_address));
    USART_TransmitByte(byte2hex_hi(command));
    USART_TransmitByte(byte2hex_lo(command));
    USART_TransmitByte(byte2hex_hi(size));
    USART_TransmitByte(byte2hex_lo(size));
    for (uint8_t i=0;i<size;++i) {
	USART_TransmitByte(byte2hex_hi(data[i]));
	USART_TransmitByte(byte2hex_lo(data[i]));
    }
    USART_TransmitByte('\r');
}

uint8_t check_pack() {
    if (rbuf_p%2==1) return 0;
    for (uint8_t i=0;i<rbuf_p;++i)
	if (!(((rbuf[i]>='0')&&(rbuf[i]<='9')) ||
	      ((rbuf[i]>='A')&&(rbuf[i]<='F')) ||
	      ((rbuf[i]>='a')&&(rbuf[i]<='f'))))  return 0;
    return 1;
}


int main (void) {
    char data;
    USARTInit(51);
    _delay_ms(100);
    USART_TransmitText("\rClunet 2.0!\r");
    DDRB|=0x01;
    PORTB|=0x01;
    clunet_init();
    clunet_set_on_data_received(data_received);
	//char buffer[1];
	//buffer[0] = 1;
	//clunet_send(CLUNET_BROADCAST_ADDRESS, CLUNET_PRIORITY_MESSAGE, 50, buffer, sizeof(buffer));

/*	
    while (1) {
	if (PIND&_BV(PD2)) 
	USART_TransmitText("1\r");
	else
	USART_TransmitText("0\r");
	_delay_ms(10);
	if(UCSRA & (1<<RXC)) {
	    wdt_enable(WDTO_15MS); while (1);
        }
    }
*/
    while (1) {
	data=USART_ReadChar();
	_delay_ms(1);
	if (flags&F_ECHO) {
	    USART_TransmitByte(data);
	}
	if (rbuf_p<RBUF_SZ) { rbuf[rbuf_p++]=data; }
	if (data=='\r') {
	    uint8_t addr,prio,comm,size;
	    switch (rbuf[0]) {
		case 'F':
		    addr=hex2byte(&rbuf[1]);
		    prio=hex2byte(&rbuf[3]);
		    comm=hex2byte(&rbuf[5]);
		    size=hex2byte(&rbuf[7]);
		    for (uint8_t i=0;i<size;++i) {
			rbuf[i]=hex2byte(&rbuf[9+i*2]);
		    }
		    clunet_send(addr, prio, comm, rbuf, size);
		    break;
		case 'Z':
		    if (strncmp(rbuf+1,"ECHOON",6)==0) {flags|=F_ECHO;} else
		    if (strncmp(rbuf+1,"ECHOOFF",7)==0) {flags&=~F_ECHO;}
		    if (strncmp(rbuf+1,"REBOOT",6)==0) {wdt_enable(WDTO_15MS); while (1); }
		    if (strncmp(rbuf+1,"LOW5",3)==0) { CLUNET_SEND_1; for (uint8_t i=0;i<4*5;i++) _delay_ms(250); CLUNET_SEND_0; }
		    break;
		default:
		    /*
		    if (check_pack()) {
			USART_TransmitText("2PAK\r");
		    } else USART_TransmitText("0PAK\r");
		    */
		    break;
	    }
        rbuf_p=0;
	}
    }
    return 0;
}
