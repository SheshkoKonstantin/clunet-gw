
#include <stdlib.h>
#include <stdio.h>   /* Стандартные объявления ввода/вывода */
#include <string.h>  /* Объявления строковых функций */
#include <unistd.h>  /* Объявления стандартных функций UNIX */
#include <fcntl.h>   /* Объявления управления файлами */
#include <errno.h>   /* Объявления кодов ошибок */
#include <termios.h> /* Объявления управления POSIX-терминалом */
#include <sys/socket.h>
#include <arpa/inet.h>


#define MAX_FLASH_SIZE	1024*8
#define MAX_COMMAND_SIZE	256

#define uint	unsigned int
#define uint8_t	unsigned char
#define uchar	unsigned char
#define rbuf_sz	256
char rbuf[rbuf_sz+1];
unsigned int rbuf_p=0;
char wbuf[rbuf_sz+1];
char *hex_file_path=NULL;
char *tty_file_path="/dev/tty/USB0";

int fd;
int ff;

uchar device_address=0xff;
int page_size=64;

struct program_data_struct {
    char data[MAX_FLASH_SIZE];
    uint last_data;
} prog_data;


/* Записывает значение получив указатель на строку с хексом */
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

/* Возвращает байт получив указатель на строку с хексом */
uint8_t hex2byte(char *hp) {
    uint8_t b;
    hex2byte1 (hp, &b);
    return b;
}




#define LINE_BUF_SIZE	256
char line_buf[LINE_BUF_SIZE];

/* Считывает одну строчку из HEX файла в буфер line_buf */
void read_hex_file_line() {
    char c;
    uint pp=0;
    do {
	int b=read(ff, &c, 1);
	if (b>0) {
	    line_buf[pp++]=c;
	} else {
	    perror("Read File Error!\n"); 
	    exit(-1);
	}
	if (pp>=LINE_BUF_SIZE) { 
	    perror("Line is full!\n"); exit(-1); 
	}
    } while (c!='\n');
    line_buf[pp]=0;
    return;
}

/* Проверка строки - кнстрольная сумма two'c component sum */
void check_line_crc (uchar crc) {
    //// СЮДА ПИСАТЬ !!!!!!!!!!!
    return;
}


/* Разбирает строку из буфера */
int parse_hex_line() {
    if (line_buf[0]!=':') { // проверка двоеточия
        perror("Нет двоеточия в начале строки!"); 
        exit(-1);
    }
    uchar bytes=hex2byte(line_buf+1); // получаем число байт в строке
    uchar addr_hi=hex2byte(line_buf+1+2); // получаем старший байт адреса
    uchar addr_lo=hex2byte(line_buf+1+4); // получаем млажший байт адреса
    uchar type=hex2byte(line_buf+1+6); 	// получаем тип записи
    char *hex_data=line_buf+1+8; 	//указатель на начало данных
    uchar cksum=hex2byte(hex_data+bytes*2); // контрольная сумма после данных
    check_line_crc(cksum);
    char *daddr;
    switch (type) {
	case 0:
	    daddr=prog_data.data+addr_lo+addr_hi*256; 	// адрес, куда начинаем писать
	    for (int i=0;i<bytes;++i) {			// записываем нужное число байт
		daddr[i]=hex2byte(hex_data+2*i);
	    }
	    if (addr_lo+addr_hi*256+bytes>prog_data.last_data) prog_data.last_data=addr_lo+addr_hi*256+bytes; // если общее число байт меньше обновляем
	    break;
	case 1:
	    return 1;
	    break;
	default:
        perror("Неизвестный тип строки!"); 
        exit(-1);
	    break;
    }
    return 0;
}

/* Показывает записанные в память байты */
void show_prog_mem() {
    for (int i=0;i<prog_data.last_data;++i) {
	if (i%16==0) printf("\n%04x: ", i);
	printf ("%02x",(uchar)prog_data.data[i]);
    }
    printf("\n");
}

/* Считывание HEX файла */
void read_hex_file() {
    memset(prog_data.data,0xff,MAX_FLASH_SIZE);
    prog_data.last_data=0;
    do {
        read_hex_file_line();
    } while (parse_hex_line()==0);
    return;
}

/* Запись байта в формате hex в строку */
void byte2hex (uint8_t *b, char *hp) {
    uint8_t c=(*b&0xf0)>>4;
    if (c>9) c+=0x61-10; else c+=0x30;
    *hp=c;
    c=*b&0x0f;
    if (c>9) c+=0x61-10; else c+=0x30;
    *(hp+1)=c;
    return;
}

/* считывает строку из COM порта в rbuf */
void com_read_line () {
    char c;
    rbuf_p=0;
    while (1) {
	int b=read(fd, &c, 1);
	if (b>0) {
	    if ((c!='\r')&&(rbuf_p<rbuf_sz)) rbuf[rbuf_p++]=c;
	    if (c=='\r') {
		rbuf[rbuf_p]=0;
		break;
	    }
	} else {
		perror("Ошибка чтения порта!");
		exit(-1);
	}
    }
    return;
}

/* Отправка строки ком-порт */
void write_str (char *str) {
    for (int i=0;*(str+i)!=0;++i) {
	usleep(500);
	write (fd,str+i,1);
    }
    usleep(500);
    return;
}

/* Открытие порта для чтения/записи */
int open_port(void) {
  int fd;
  fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd == -1)   {
	perror("Не открывается порт! ");
	exit(-1);
  }
  else
    fcntl(fd, F_SETFL, 0);
  return (fd);
}

/* Открытия hex файла для чтения */
int open_file(void) {
  int file;
  if (hex_file_path==NULL) {
	perror("Нулевой указатель на файл! ");
	exit(-1);
  }
  file = open(hex_file_path,O_RDONLY);
  if (fd == -1)   {
	perror("Не открывается hex-файл! ");
	exit(-1);
  }
  else
  return (file);
}


/* Вывод на экран строки в hex-ах */
void print_hex1 (char * p, unsigned int c) {
    for (int i=0;i<c;++i)
	printf ("%02x ",p[i]);
	printf("\n");
    return;
}

/* Ожидает готовности бутлодера */
void wait_bootbegin () {
	while (1) {
    	    com_read_line();
    	    //printf("> %s\n",rbuf);
    	    if ((rbuf_p==1+4*2)&&(rbuf[0]=='P')) { // если размер как у ожидаемого пакета
    		uchar addr=hex2byte(rbuf+1);
    		uchar comm=hex2byte(rbuf+1+2);
    		uchar size=hex2byte(rbuf+1+4);
    		uchar data=hex2byte(rbuf+1+6);
    		if ((comm==2)&&(size==1)&&(data==0)) { // если нужные команда, размер, данные
    		    if (addr==device_address||device_address==0xff) { // и если адрес подходящий
    			device_address=addr;				// запоминаем адрес
    			break;						// и выходим
    		    }
    		}
    	    }
	}
    return;
}

/* Ожидает что бутлодер согласится программироваться */
void wait_progready () {
	while (1) {
    	    com_read_line();
    	    //printf(">: %s\n",rbuf);
    	    if ((rbuf_p==1+5*2)&&(rbuf[0]=='P')) { // если размер как у ожидаемого пакета
    		uchar addr=hex2byte(rbuf+1);
    		uchar comm=hex2byte(rbuf+1+2);
    		uchar size=hex2byte(rbuf+1+4);
    		uchar data1=hex2byte(rbuf+1+6);
    		uchar data2=hex2byte(rbuf+1+8);
    		if ((comm==2)&&(size==2)&&(data1==2)) { // если нужные команда, размер, данные
    		    if (addr==device_address) { // и если адрес подходящий
    			page_size=data2;	// запоминаем размер  страницы
    			break;			// и выходим
    		    }
    		}
    	    }
	}
    return;
}

/* Ожидает что бутлодер схавал порцию информации */
void wait_progok () {
	while (1) {
    	    com_read_line();
    	    //printf(">: %s\n",rbuf);
    	    if ((rbuf_p==1+4*2)&&(rbuf[0]=='P')) { // если размер как у ожидаемого пакета
    		uchar addr=hex2byte(rbuf+1);
    		uchar comm=hex2byte(rbuf+1+2);
    		uchar size=hex2byte(rbuf+1+4);
    		uchar data1=hex2byte(rbuf+1+6);
    		if ((comm==2)&&(size==1)&&(data1==4)) { // если нужные команда, размер, данные
    		    if (addr==device_address) { // и если адрес подходящий
    			break;			// выходим
    		    }
    		}
    	    }
	}
    return;
}

/* Отпрака пакета */
void send_pack(uchar addr, uchar prio, uchar comm, uchar size, uchar *data) {
    uchar buf[MAX_COMMAND_SIZE];
    buf[0]='F';			// команда F
    byte2hex(&addr,buf+1);	// адрес
    byte2hex(&prio,buf+1+2);	// приоритет
    byte2hex(&comm,buf+1+4);	// команда
    byte2hex(&size,buf+1+6);	// размер данных
    for (uint i=0;i<size;++i) { // данные
	byte2hex(data+i,buf+1+8+i*2);
    }
    buf[1+8+size*2]='\r';	// концы
    buf[1+8+size*2+1]=0;
    //printf (">>> %s\n", buf);
    write_str(buf);
    return;
}

/* Отправляет устройству запрос на перезапуск */
void send_reset() {
    send_pack(device_address,8,3,0,NULL);
    return;
}

/* Отправляет устройству - закончить програмирование */
void send_endprog() {
    uchar byte=5; // код конца программирования
    send_pack(device_address,8,2,1,&byte);
    return;
}

/* Отправляет устройству - начать програмирование */
void send_beginprog() {
    uchar byte=1; // код начала програмирования
    send_pack(device_address,8,2,1,&byte);
    return;
}


/* Настройка порта */
void port_setup() {
    struct termios oldtio, newtio;
    tcgetattr(fd,&oldtio); // save current port settings 
    newtio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VMIN]=1;
    newtio.c_cc[VTIME]=0;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd,TCSANOW,&newtio);
    return;
}

/* Отправляет устройству страницу по номеру */
int prog_page(uint page_num) {
    uchar buf[256];
    uint addr=page_num*page_size;
    uchar size=page_size+5;
    buf[0]=3;				// команда записи страницы 3
    buf[1]=(uchar)(addr>>0);		// запись младшего байта 1
    buf[2]=(uchar)(addr>>8);		// запись старшего байта 2
    buf[3]=0;				// запись старшего байта 3
    buf[4]=0;				// запись старшего байта 4
    memcpy(buf+5,prog_data.data+addr,page_size);
    send_pack(device_address,8,2,size,buf);
    if (prog_data.last_data<=addr+page_size) return 1;
    return 0;
}



void flash_mode() {
    printf("Flash mode>\n");

    // Обработка файла
    ff=open_file();
    read_hex_file();
    close(ff);
    
    // Работа с устройством
    fd=open_port();
    port_setup();
    
    if (device_address != 0xff) { // Если выбран конкретный адрес, можно отправить команду на перезагрузку
	send_endprog();
	send_reset();
    }
    wait_bootbegin();
    send_beginprog();
    wait_progready();
    
    printf ("Прогаю...\n");
    uchar page=0;
        int res=0;
    do {
	res=prog_page(page++);
	wait_progok();
    } while (res==0);
    send_endprog();
    close(fd);
    printf ("Готово!\n");

    return;
}


int main (int argc, char **argv) {

    if (argc<2) { // если не указан HEX файл
	fprintf(stderr,"Надо указывать параметры!\nНапример:\n%s ./main.hex [99] [/dev/ttyUSB0]\n", argv[0]);
	exit(-1);
    }
    
    
    
    hex_file_path=argv[1];
    if (argc>2) { // если указан адрес Clunet
	int addr=atoi(argv[2]);
	if ((addr<0)||(addr>255)) {
	    fprintf(stderr,"Адрес должен быть от 0 до 255\n");
	    exit(-1);
	}
	device_address=addr;
	printf ("Clunet адрес: %d\n",device_address);
    }
    if (argc>3) {
	tty_file_path=argv[3];
	printf ("Порт: %s\n",tty_file_path);
    }

    flash_mode();



return 0;
}