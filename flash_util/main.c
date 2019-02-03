//#define _BSD_SOURCE

#include <time.h>
#include <sys/time.h>
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
char *tty_file_path="/dev/ttyUSB0";

int fd;
int ff;

uchar device_address=0xff;
int page_size=64;
int flags=0;
#define F_SELFPROG 0x01

struct program_data_struct {
    char data[MAX_FLASH_SIZE];
    uint last_data;
} prog_data;


int time_diff_ms(struct timeval t1, struct timeval t2) {
    return (((t1.tv_sec - t2.tv_sec) * 1000000) + (t1.tv_usec - t2.tv_usec))/1000;
}


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

/* считывает строку из COM/TCP порта в rbuf */
int com_read_line () {
    char c;
    if (rbuf[rbuf_p]==0) rbuf_p=0;
    time_t time1, time2;
    time(&time1);
    time(&time2);
    while (1) {
	int b=read(fd, &c, 1);
	if (b>0) {
	    time(&time1);
	    if ((c!='\r')&&(c!='\n')&&(rbuf_p<rbuf_sz)) rbuf[rbuf_p++]=c;
	    if (c=='\r') {
		rbuf[rbuf_p]=0;
		//printf(">%s",rbuf);
		return 1;
	    }
	} else {
	    time(&time2);
	    if (difftime(time2,time1)>2) {
		return 0;
	    }
	    //perror("Ошибка чтения порта!");
	    //exit(-1);
	}
    }
    return 0;
}


/* считывает строку из COM/TCP порта в rbuf */
int com_read_line1 (unsigned int delay) {
    char c;
    if (rbuf[rbuf_p]==0) rbuf_p=0;
    struct timeval time1, time2;
    gettimeofday(&time1, NULL);
    gettimeofday(&time2, NULL);
    while (1) {
	int b=read(fd, &c, 1);
	if (b>0) {
	    gettimeofday(&time1, NULL);
	    if ((c!='\r')&&(c!='\n')&&(rbuf_p<rbuf_sz)) rbuf[rbuf_p++]=c;
	    if (c=='\r') {
		rbuf[rbuf_p]=0;
		return 1;
	    }
	} else {
	    gettimeofday(&time2, NULL);
	    if (time_diff_ms(time2,time1)>delay) {
		return 0;
	    }
	}
    }
    return 0;
}



char fd_type=0;

/* Отправка строки ком-порт */
void write_str (char *str) {
    if (fd_type==0) {
	//printf(">>");
	for (int i=0;*(str+i)!=0;++i) {
	    usleep(1000);
	    write (fd,str+i,1);
	    //printf("(%c)[%02x]\n",str[i],str[i]);
	}
	if (fd_type==1) write (fd,"\n",1);
	//printf ("\n");
	usleep(1000);
    } else if (fd_type==1) {
	write (fd,str,strlen(str));
	write (fd,"\n",1);
    }

    return;
}

/* Открытие порта для чтения/записи */
int open_port(void) {
  int fd;
  fd = open(tty_file_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd == -1)   {
	//perror("Не открывается порт! ");
	//exit(-1);
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
int wait_bootbegin () {
	time_t time1, time2;
	time(&time1);
	while (1) {
	    time(&time2);
	    if (difftime(time2,time1)>2) return 0;
    	    if (com_read_line()==0) continue;
    	    //printf("> %s\n",rbuf);
    	    
    	    if ((flags&F_SELFPROG)&&(strncmp(rbuf,"R00",3)==0)) { // если шлюз прогает сам себя
    		return 2;
    	    } else
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
    return 1;
}

/* Ожидает что бутлодер согласится программироваться */
int wait_progready () {
	time_t time1, time2;
	time(&time1);
	while (1) {
	    time(&time2);
	    if (difftime(time2,time1)>3) return 0;
    	    if (com_read_line()==0) continue;
    	    //printf(">: %s\n",rbuf);
    	    if ((flags&F_SELFPROG)&&(rbuf_p==7)&&(rbuf[0]=='R')&&(rbuf[1]=='0')&&(rbuf[2]=='2')&&(rbuf[3]=='0')&&(rbuf[4]=='0')) { // если программирование шлюза
    	    	uchar data1=hex2byte(rbuf+3);
    		uchar data2=hex2byte(rbuf+5);
		page_size=data2;
		printf ("Размер страницы: %d\n", page_size);
		break;
    	    } else // иначе
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
    return 1;
}

/* Ожидает что бутлодер схавал порцию информации */
int wait_progok () {
	time_t time1, time2;
	time(&time1);
	while (1) {
	    time(&time2);
	    if (difftime(time2,time1)>2) return 0;
    	    if (com_read_line()==0) continue;
    	    //printf(">: %s\n",rbuf);
    	    if ((flags&F_SELFPROG)&&(strncmp(rbuf,"R04",3)==0)) { // если программирование самого шлюза
    		break;
    	    } else // иначе устройство в сети
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
    return 1;
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
    if (flags&F_SELFPROG) {
	write_str("ZREBOOT\r");
    } else {
	send_pack(device_address,8,3,0,NULL);
    }
    return;
}

/* Отправляет устройству - закончить програмирование */
void send_endprog() {
    if (flags&F_SELFPROG) {
	write_str("S05\r");
    } else {
	uchar byte=5; // код конца программирования
	send_pack(device_address,8,2,1,&byte);
    }
    return;
}

/* Отправляет устройству - начать програмирование */
void send_beginprog() {
    if (flags&F_SELFPROG) {
	write_str("S01\r");
    } else {
	uchar byte=1; // код начала програмирования
	send_pack(device_address,8,2,1,&byte);
    }
    return;
}


/* Настройка порта */
void port_setup() {
    struct termios oldtio, newtio;
    tcgetattr(fd,&oldtio); // save current port settings 
    newtio.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
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
    if (flags&F_SELFPROG) { // если програамирование самого шлюза
	buf[0]='S';
	buf[1]='0';
	buf[2]='3';
	byte2hex(((uchar *)&addr),buf+3);    // сначала младший байт
	byte2hex(((uchar *)&addr)+1,buf+5);  // второй байт
	buf[7]='0';
	buf[8]='0';
	buf[9]='0';
	buf[10]='0';
	for (int i=0;i<page_size;++i) {
	    byte2hex(prog_data.data+addr+i,buf+11+2*i);
	}
	buf[11+2*page_size]='\r';
	buf[11+2*page_size+1]='\0';
	write_str(buf);
    } else { // иначе программирование устройства в сети
	buf[0]=3;				// команда записи страницы 3
	buf[1]=(uchar)(addr>>0);		// запись младшего байта 1
	buf[2]=(uchar)(addr>>8);		// запись старшего байта 2
	buf[3]=0;				// запись старшего байта 3
	buf[4]=0;				// запись старшего байта 4
	memcpy(buf+5,prog_data.data+addr,page_size);
	send_pack(device_address,8,2,size,buf);
    }
    if (prog_data.last_data<=addr+page_size) return 1;
    return 0;
}


int open_rport(char* addr, int port) {
    int socket1;
    struct sockaddr_in server;
    socket1 = socket(AF_INET , SOCK_STREAM , 0);
    if (socket1 == -1) {
        fprintf(stderr, "Не удается создать сокет!\n");
        return -1;
    }
    server.sin_addr.s_addr = inet_addr(addr);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (connect(socket1 , (struct sockaddr *)&server , sizeof(server)) < 0) {
	fprintf(stderr, "Нет соединения!\n");
        return -1;
    }
    fcntl(socket1, F_SETFL, O_NONBLOCK);
    return socket1;
}


/* Режим прошивки */
void flash_mode(int argc, char **argv) {
    printf("Flash mode>\n");
    hex_file_path=argv[1];
    if (argc>2) { // если указан адрес Clunet
	int addr=atoi(argv[2]);
	if (strcmp(argv[2],"self")==0) {
	    flags|=F_SELFPROG;
	    printf ("Режим программирование шлюза\n");
	} else
	if ((addr<0)||(addr>255)) {
	    fprintf(stderr,"Адрес должен быть от 0 до 255\n");
	    exit(-1);
	}
	device_address=addr;
	printf ("Clunet адрес: %d\n",device_address);
    }
    if (argc>3) { // пусть к последовательному порту
	tty_file_path=argv[3];
	printf ("Порт: %s\n",tty_file_path);
    }
    // Обработка файла
    ff=open_file();
    read_hex_file();
    close(ff);
    

    // Если надо подключаться через удаленный порт
    char *dots=strstr(tty_file_path,":");
    if (dots!=NULL) {
	char buf[128];
	strcpy(buf,dots+1);
	int rport=atoi(buf);
	if ((rport<=0)||(rport>0xffff)) {
	    fprintf(stderr,"Плохой порт: %s\n",buf);
	}
	*dots=0;
	strcpy(buf,tty_file_path);
	printf("Подключение на %s:%d...\n",buf,rport);
	fd=open_rport(buf,rport);
	if (fd < 0)   {
	    perror("Не открывается tcp порт!");
	    exit(-1);
	}
	fd_type=1;
    } else {
	fd=open_port();
	if (fd < 0)   {
	    perror("Не открывается порт!");
	    exit(-1);
	}
	port_setup();
    }
    
    
    // Работа с устройством    
    printf ("Подключение к устройству");
    do {
    if (device_address != 0xff) { // Если выбран конкретный адрес, можно отправить команду на перезагрузку
	printf(".");fflush(stdout);
	send_endprog();
	send_reset();
    }
    } while (wait_bootbegin()==0);
    printf("\n");

    do {
	send_beginprog();
    } while (wait_progready()==0);
    // Запись данных
    printf ("Прогаю");
    uchar page=0;
    int res=0;
    
    time_t time1, time2;
    time(&time1);
    do {
	printf(".");fflush(stdout);
	do {
	    res=prog_page(page++);
	} while (wait_progok()==0);
    } while (res==0);
    send_endprog();
    send_endprog();
    printf ("Готово!\n");
    time(&time2);
    printf ("Время загрузки: %.1f\n",difftime(time2,time1));
    sleep(1);
    close(fd);
    return;
}


int socket0;
void init_socket0(int port) {
    struct sockaddr_in server;
    socket0 = socket(AF_INET , SOCK_STREAM , 0);
    fcntl(socket0, F_SETFL, O_NONBLOCK);
    if (socket0 == -1) {
        printf("Could not create socket");
        exit(-1);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    if( bind(socket0,(struct sockaddr *)&server , sizeof(server)) < 0) {
        perror("bind failed. Error");
        exit(-1);
    }
    listen(socket0,8);
    return;
}

struct sock_cont {
    int sock;
    struct sock_cont *next;
    struct sock_cont *prev;
    char *buf;
    uint buf_p;
    struct sockaddr_in addr;
} *socks=NULL;

void sock_add (int sock, struct sockaddr_in addr) {
    struct sock_cont *new = malloc(sizeof(struct sock_cont));
    new->buf = malloc(sizeof(char)*(MAX_COMMAND_SIZE+1));
    new->buf_p=0;
    new->next=NULL;
    new->sock=sock;
    new->addr=addr;
    if (socks==NULL) {
	socks=new;
	new->prev=NULL;
    } else {
	struct sock_cont *ptr=socks;
	while (ptr->next!=NULL) {
	    ptr=ptr->next;
	}
	ptr->next=new;
	new->prev=ptr;
    }
    //printf("+%d\n",new->sock);
    return;
}

void sock_remove (struct sock_cont *ptr) {
    close(ptr->sock);
    if (ptr->next!=NULL)
	ptr->next->prev=ptr->prev;
    if (ptr->prev!=NULL)
	ptr->prev->next=ptr->next;
	else
	socks=ptr->next;
    free(ptr->buf);
    free(ptr);
    return;
}

void sock_remove_all () {
    while (socks!=NULL) {
	sock_remove(socks);
    }
}

int try_accept () {
    int client_sock;
    struct sockaddr_in client;
    int c = sizeof(struct sockaddr_in);
    client_sock = accept(socket0, (struct sockaddr *)&client, (socklen_t*)&c);
    if (client_sock < 0) {
	return 0;
    }
    fcntl(client_sock, F_SETFL, O_NONBLOCK);
    puts("Connection accepted");
    sock_add(client_sock,client);
    return 1;
}


void send_all_sockets(char *msg, int len) {
    struct sock_cont *ptr=socks;
    while (ptr!=NULL) {
    	write(ptr->sock, msg , len);
	ptr=ptr->next;
    }
    return;
}

void process_sockets() {
    char c;
    char buf[64];
    struct sock_cont *ptr=socks;
    while (ptr!=NULL) {
    	    int rs = recv(ptr->sock, &c , 1 , 0);
    	    if (rs==0) {
    		printf ("Соединение закрыто\n");
    		sock_remove(ptr);
    	    } else if (rs>0) {
    		if (ptr->buf_p<MAX_COMMAND_SIZE)
    		    ptr->buf[ptr->buf_p++]=c;
    		if (c=='\n') {
    		    int n = sprintf(buf,"%d.%d.%d.%d:%d > ",(uchar)(ptr->addr.sin_addr.s_addr<<0),
    							    (uchar)(ptr->addr.sin_addr.s_addr>>8),
    							    (uchar)(ptr->addr.sin_addr.s_addr>>16),
    							    (uchar)(ptr->addr.sin_addr.s_addr>>24),
    							    ptr->addr.sin_port);
    		    send_all_sockets(buf,n);
    		    send_all_sockets(ptr->buf,ptr->buf_p);
    		    ptr->buf[ptr->buf_p-1]='\0';
    		    write_str(ptr->buf);
    		    ptr->buf_p=0;
    		}
	    }
	ptr=ptr->next;
    }
    return;
}


struct termios oldt;
int oldf;
void init_stdin () {
    tcgetattr(STDIN_FILENO, &oldt);
    struct termios newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    return;
}


void return_stdin () {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    return;
}

int mustExit() {
    char c;
    if ((read(STDIN_FILENO,&c,1)==1)&&(c=='q')) return 1;
    return 0;
}

static int get_device_status(int fd)
{
  struct termios t;
  if (fd < 0)
    return 0;
  //if (portfd_is_socket && portfd_is_connected)
    //return 1;
  return !tcgetattr(fd, &t);
}



void process_serial() {
    int res;
    read1:
	if(!get_device_status(fd)) {
	    fprintf(stderr,"FAIL TERM...\n");
	    int reopen = fd == -1;
	    close(fd);
	    fd = -1;
	    reopen:
	    fd = open_port();
	    if (fd < 0)   {
		goto reopen;
	    }
	    port_setup();
	    fcntl(fd, F_SETFL, O_NONBLOCK|O_NDELAY|O_NOCTTY|O_RDONLY);
	    fprintf(stderr,"TERM OK!\n");
	}
	res = read(fd,rbuf+rbuf_p,1);
	if (res==1) {
	    if (rbuf[rbuf_p]=='\r') {
		++rbuf_p;
		rbuf[rbuf_p]='\n';
		++rbuf_p;
		//send_all_sockets("Serial > ",9);
		send_all_sockets(rbuf,rbuf_p);
		rbuf_p=0;
	    } else {
		++rbuf_p;
	    }
	    
	    goto read1;
	} else if (res<-1) {
		printf("%d\n", res);
	}
    return;
}

#define DEFAULT_TCP_PORT	8888
void server_mode(int argc, char **argv) {
    printf("Server mode\n");
    int tcp_port=DEFAULT_TCP_PORT;
    if (argc>2) {
	tcp_port=atoi(argv[2]);
	if ((tcp_port<1)||tcp_port>0xffff) {
	    fprintf(stderr,"Неверный порт %d!\n",tcp_port);
	    exit(-1);
	} else {
	    printf("Выбран порт %d\n",tcp_port);
	}
    }
    if (argc>3) { // пусть к последовательному порту
	tty_file_path=argv[3];
	printf ("Порт: %s\n",tty_file_path);
    }

    init_socket0(tcp_port);
    fd=open_port();
        if (fd < 0)   {
	perror("Не открывается порт! ");
	exit(-1);
    }
    port_setup();
    fcntl(fd, F_SETFL, O_NONBLOCK|O_NDELAY|O_NOCTTY|O_RDONLY);
    init_stdin();
    do {
	try_accept();
	process_sockets();
	process_serial();
    } while (!mustExit());
    sock_remove_all();
    close (socket0);
    return_stdin();
    close(fd);
    return;
}


/* Отправляет устройству ping */
void send_ping(uchar addr) {
    if (flags&F_SELFPROG) {
    } else {
	send_pack(addr,8,0xfe,0,NULL);
    }
    return;
}


/* Ожидает pong */
uchar wait_pong () {
	while (1) {
    	    if (com_read_line1(100)==0) return 0xff;
    	    if ((rbuf_p==7)&&(rbuf[0]=='P')) { // если размер как у ожидаемого пакета
    		uchar addr=hex2byte(rbuf+1);
    		uchar comm=hex2byte(rbuf+1+2);
    		uchar size=hex2byte(rbuf+1+4);
    		if ((comm==0xff)&&(size==0)) { // если нужные команда, размер
    			return addr;
    		}
    	    }
	}
    return 0xff;
}

uchar wait_pong1 (unsigned int wait_ms, uchar wait_addr) {
	struct timeval time1, time2;
	gettimeofday(&time1, NULL);
	while (1) {
	    gettimeofday(&time2, NULL);
	    if (time_diff_ms(time2,time1)>wait_ms) {
		return 0xff;
	    }
    	    if (com_read_line1(100)==0) continue;
    	    if ((rbuf_p==7)&&(rbuf[0]=='P')) { // если размер как у ожидаемого пакета
    		uchar addr=hex2byte(rbuf+1);
    		uchar comm=hex2byte(rbuf+1+2);
    		uchar size=hex2byte(rbuf+1+4);
    		if ((comm==0xff)&&(size==0)&&(addr=wait_addr)) { // если нужные команда, размер
    			return addr;
    		}
    	    }
	}
    return 0xff;
}


/* Попытка Discovery (имя устройства) */
char *try_discover (uchar address) {
	send_pack(address,8,0x00,0,NULL);
	while (1) {
    	    if (com_read_line1(2000)==0) return NULL;
    	    if ((rbuf_p>=7)&&(rbuf[0]=='P')) {
    		uchar addr=hex2byte(rbuf+1);
    		uchar comm=hex2byte(rbuf+1+2);
    		uchar size=hex2byte(rbuf+1+4);
    		if ((comm==0x01)&&(addr==address)) { // если нужные команда, размер
    			char *buffer = (char*) malloc (size+1);
    			for (int i=0;i<size;++i) {
    			    buffer[i]=hex2byte(rbuf+1+6+i*2);
    			}
    			buffer[size]=0x00;
    			return buffer;
    		}
    	    }
	}
    return NULL;
}


void open_terminal (char *port) {
    if (port) { // пусть к последовательному порту
	tty_file_path=port;
	printf ("Порт: %s\n",tty_file_path);
    }
    // Если надо подключаться через удаленный порт
    char *dots=strstr(tty_file_path,":");
    if (dots!=NULL) {
	char buf[128];
	strcpy(buf,dots+1);
	int rport=atoi(buf);
	if ((rport<=0)||(rport>0xffff)) {
	    fprintf(stderr,"Плохой порт: %s\n",buf);
	}
	*dots=0;
	strcpy(buf,tty_file_path);
	printf("Подключение на %s:%d...\n",buf,rport);
	fd=open_rport(buf,rport);
	if (fd < 0)   {
	    perror("Не открывается tcp порт!");
	    exit(-1);
	}
	fd_type=1;
    } else {
	fd=open_port();
	if (fd < 0)   {
	    perror("Не открывается порт!");
	    exit(-1);
	}
	port_setup();
    }
    return;
}


/* убираем 0xff */
void chop_str(uchar *str) {
    int size=strlen(str);
    for (int i=0;i<size;++i) {
	if (str[i]==0xff) {
	    str[i]=0;
	}
    }
    return;
}


/* Режим обнаружения */
void discover_mode(int argc, char **argv) {
    printf("Discover mode.\n");
    uchar a_from=0;
    uchar a_to=254;
    if (argc==3) {
	a_from=atoi(argv[2]);
	a_to=a_from;
    }
    if (argc==4) {
	a_from=atoi(argv[2]);
	a_to=atoi(argv[3]);
    }
    open_terminal(tty_file_path);
    printf ("Поиск устройств...\n");
    for (int i=a_from;i<=a_to;++i) {
	send_ping(i);
	uchar pong;
	if (a_from==a_to) {
	    pong = wait_pong1(3000,i);
	} else {
	    pong = wait_pong();
	}
	if (pong !=0xff) {
	    char *name = try_discover(pong);
	    if (name != NULL) {
		chop_str(name);
		printf ("Надено устройство. Адрес: %03d (0x%02x) Имя: %s\n",pong,pong,name);
		free(name);
	    } else {
		printf ("Надено устройство. Адрес: %03d (0x%02x) Имя: не определяется\n",pong,pong);
	    }
	}
    }
    close(fd);
    return;
}


/* Попытка установить имя устройства */
char *device_try_set_name (uchar address, char *name) {
	uchar size=strlen(name);
	send_pack(address,8,0xf8,size,name);
	time_t time1, time2;
	time(&time1);
	while (1) {
	    time(&time2);
	    if (difftime(time2,time1)>2) {
		return NULL;
	    }
    	    if (com_read_line1(200)==0) continue;
    	    if ((rbuf_p>=7)&&(rbuf[0]=='P')) {
    		uchar addr=hex2byte(rbuf+1);
    		uchar comm=hex2byte(rbuf+1+2);
    		uchar size=hex2byte(rbuf+1+4);
    		if ((comm==0xf9)&&(addr==address)) { // если нужные команда, размер
    			char *buffer = (char*) malloc (size+1);
    			for (int i=0;i<size;++i) {
    			    buffer[i]=hex2byte(rbuf+1+6+i*2);
    			}
    			buffer[size]=0x00;
    			return buffer;
    		}
    	    }
	}
    return NULL;
}

/* Попытка установить адрес устроства */
uchar device_try_set_addr (uchar address, uchar new_address, uint8_t param) {
	uint8_t COMMAND=0xfc;
	uint8_t COMMAND_REPLY=0xfd;
	uint8_t ADDRESS_REPLY=new_address;
	if (param==0x01) {
	    COMMAND=0xfa;
	    COMMAND_REPLY=0xfb;
	    ADDRESS_REPLY=address;
	}
	send_pack(address,8,COMMAND,1,&new_address);
	time_t time1, time2;
	time(&time1);
	while (1) {
	    time(&time2);
	    if (difftime(time2,time1)>2) {
		return 0xff;
	    }
    	    if (com_read_line1(200)==0) continue;
    	    if ((rbuf_p>=7)&&(rbuf[0]=='P')) {
    		uchar addr=hex2byte(rbuf+1);
    		uchar comm=hex2byte(rbuf+1+2);
    		uchar size=hex2byte(rbuf+1+4);
    		if ((comm==COMMAND_REPLY)&&(addr==ADDRESS_REPLY)&&(size==1)) { // если нужные команда, размер
    			uchar newa=hex2byte(rbuf+1+6);
    			return newa;
    		}
    	    }
	}
    return 0xff;
}


/* Установить имя устройства */
void setname_mode(int argc, char **argv) {
    printf("Установка имени устройства.\n");
    if (argc<3) {
	printf ("Не хватает параметров. Укажите setname адрес имя\n");
	exit(-1);
    }
    open_terminal(tty_file_path);
    uchar address=atoi(argv[2]);
    char *name=argv[3];
    printf("Установка имени устройства...\n");
    char *newname = device_try_set_name(address,name);
    if (newname==NULL) {
	printf ("Ошибка установки имени '%s' для устройства с адресом %03d (0x%02x).\n",name,address,address);
    } else {
	chop_str(newname);
	printf ("Новое имя устройства (%s).\n",newname);
    }
    close(fd);
    return;
}

/* Установить адрес устройства */
void setaddr_mode(int argc, char **argv, uint8_t param) {
    printf("Установка адреса устройства.\n");
    if (argc<3) {
	printf ("Не хватает параметров. Укажите setaddr адрес новый_адрес\n");
	exit(-1);
    }
    open_terminal(tty_file_path);
    uchar address=atoi(argv[2]);
    uchar new_address=atoi(argv[3]);
    if (address!=new_address) {
	printf("Проверка нового адреса...\n");
	send_ping(new_address);
	if (wait_pong1(1000,new_address)!=0xff) {
	    printf ("Адрес %03d (0x%02x) уже занят.\n",new_address,new_address);
	    char *name = try_discover(new_address);
	    if (name != NULL) {
		chop_str(name);
		printf ("Надено устройство. Адрес: %03d (0x%02x) Имя: %s\n",new_address,new_address,name);
		free(name);
	    } else {
		printf ("Надено устройство. Адрес: %03d (0x%02x) Имя: не определяется\n",new_address,new_address);
	    }
	    exit(-1);
	}
    }
    printf("Установка адреса устройства...\n");
    uchar newa = device_try_set_addr(address,new_address,param);
    if (newa!=new_address) {
	printf ("Ошибка установки адреса %03d (0x%02x) для устройства с адресом %03d (0x%02x).\n",new_address,new_address,address,address);
    } else {
	printf ("Новый адрес устройства %03d (0x%02x).\n",newa,newa);
    }
    close(fd);
    return;
}

/* Отправка команды */
void sendcommand_mode(int argc, char **argv) {
    printf("Отправка команды на устройство.\n");
    if (argc<3) {
	printf ("Не хватает параметров. Укажите sndcmd адрес команда\n");
	exit(-1);
    }
    open_terminal(tty_file_path);
    uchar address=atoi(argv[2]);
    uchar command=atoi(argv[3]);
    
    printf("Отправка команды устройства...\n");
    send_pack(address,3,command,0,NULL);
    
    close(fd);
    return;
}


void read_config () {
    FILE * f = fopen("util.conf","r");
    if (f) {
	char str[256];
	while(!feof(f)) {
            fscanf(f,"%s\n",str);
            if (strstr(str,"port=")==str) {
        	char *dot=strstr(str,"=");
        	++dot;
        	tty_file_path=malloc(strlen(dot));
        	strcpy(tty_file_path,dot);
            }
        }
	fclose(f);
    }
    return;
}


int main (int argc, char **argv) {
    read_config();
    if (argc<2) { // если не указаны параметры
	fprintf(stderr,"Надо указывать параметры!\nНапример:\n%s ./main.hex [99] [/dev/ttyUSB0]\n", argv[0]);
	exit(-1);
    }
    if (strcmp(argv[1],"srv")==0) {
	server_mode(argc,argv);
    } else if (strcmp(argv[1],"discover")==0) {
	discover_mode(argc,argv);
    } else if (strcmp(argv[1],"setname")==0) {
	setname_mode(argc,argv);
    } else if (strcmp(argv[1],"setaddr")==0) {
	setaddr_mode(argc,argv,0x00);
    } else if (strcmp(argv[1],"setaddreeprom")==0) {
	setaddr_mode(argc,argv,0x01);
    } else if (strcmp(argv[1],"sndcmd")==0) {
	sendcommand_mode(argc,argv);
    } else {
	flash_mode(argc,argv);
    }
return 0;
}