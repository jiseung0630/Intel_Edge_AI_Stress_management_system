#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <mysql/mysql.h>
#include <math.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 10
#define HBEAT_SIZE 120
#define HRV_HOUR 1
#define HOST "localhost"
#define USER "iot"
#define PASS "pwiot"
#define DB "lulldb"
#define STMUSER [JYJ_UBT]
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5000
#define PASSWD "PASSWD"

void send_stress_message(const char* status);

void* send_msg(void* arg);
void* recv_msg(void* arg);
void* hrv_msg(void* arg);
void error_handling(char* msg);
char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];
int hbeat[HBEAT_SIZE * HRV_HOUR];
int timer_set_flag;
int temp;
int humi;
volatile sig_atomic_t timer_expired = 0;
volatile int stress_level =-1;
typedef struct {
    float sdnn;
    float rmssd;
    float pnn50;
} HRVData;

HRVData hrv;

void finish_with_error(MYSQL* con) {
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);
}

void calculateHRVFromHeartRateNSendData(int heart_rates[], int count,int temp,int humi ,HRVData* hrv) {
    double sum = 0.0;
    double sum_sq_diff = 0.0;
    int nn50_count = 0;

    for (int i = 0; i < count; i++) {
        sum += heart_rates[i];
    }

    double mean = sum / count;

    for (int i = 0; i < count; i++) {
        sum_sq_diff += (heart_rates[i] - mean) * (heart_rates[i] - mean);
    }
    hrv->sdnn = sqrt(sum_sq_diff/(count-1));
    hrv->rmssd = sqrt(sum_sq_diff / (count - 1));
    hrv->pnn50 = (double)nn50_count / (count - 1) * 100.0;
    stress_level = (int)(hrv->sdnn < 50 ? 1 : 0) + (int)(hrv->rmssd < 42 ? 1 : 0) + (int)(hrv->pnn50 < 3 ? 1 : 0);
    printf("stress_level : %d, temp: %d, humi: %d\n",stress_level,temp,humi);
    const char* status;   
    if (stress_level == 0) {
        status = "GREEN@ON";
	const char *music_file = "green.mp3";
    	char command[256];
    	snprintf(command, sizeof(command), "mpg123 %s", music_file);
    	// system() 함수를 사용하여 명령어 실행
    	int result = system(command);
    	// 실행 결과 확인
    	if (result == -1) {
        	perror("system() failed");
        	return 1;
   	 }
    } else if (stress_level == 1) {
        status = "YELLOW@ON";
 	const char *music_file = "yellow.mp3";
    	char command[256];
    	snprintf(command, sizeof(command), "mpg123 %s", music_file);
    	// system() 함수를 사용하여 명령어 실행
    	int result = system(command);
    	// 실행 결과 확인
    	if (result == -1) {
        	perror("system() failed");
        	return 1;
   	 }
    } else if (stress_level >= 2) {
        status = "RED@ON";
 	const char *music_file = "red.mp3";
    	char command[256];
    	snprintf(command, sizeof(command), "mpg123 %s", music_file);
    	// system() 함수를 사용하여 명령어 실행
    	int result = system(command);
    	// 실행 결과 확인
    	if (result == -1) {
        	perror("system() failed");
        	return 1;
   	 }
    } else {
        status = "UNKNOWN";
    }

    send_stress_message(status);
}

void send_stress_message(const char* status) {
    int sock;
    struct sockaddr_in serv_addr;
    char name_msg[BUF_SIZE];
    char cmd_msg[BUF_SIZE];

    signal(SIGPIPE, SIG_IGN);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket() error");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect() error");
        close(sock);
        return;
    }

    snprintf(name_msg, sizeof(name_msg), "[JYJ_LIN:PASSWD]\n");

    if (write(sock, name_msg, strlen(name_msg)) <= 0) {
        perror("write() error");
    }

    snprintf(cmd_msg, sizeof(cmd_msg), "[JYJ_BLT]%s\n",status);

    if (write(sock, cmd_msg, strlen(cmd_msg)) <= 0) {
        perror("write() error");
    }
    close(sock);
}

int main(int argc, char* argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread, mysql_thread, hrv_thread;
    void* thread_return;

    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "%s", argv[3]);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    sprintf(msg, "[%s:PASSWD]", name);
    write(sock, msg, strlen(msg));
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
    pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);
    pthread_create(&hrv_thread, NULL, hrv_msg, (void*)&sock);

    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);
    pthread_join(hrv_thread, &thread_return);

    close(sock);
    return 0;
}

void* send_msg(void* arg) {
    int* sock = (int*)arg;
    int str_len;
    int ret;
    fd_set initset, newset;
    struct timeval tv;
    char name_msg[NAME_SIZE + BUF_SIZE + 2];

    FD_ZERO(&initset);
    FD_SET(STDIN_FILENO, &initset);

    fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n", stdout);
    while (1) {
        memset(msg, 0, sizeof(msg));
        name_msg[0] = '\0';
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        newset = initset;
        ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
        if (FD_ISSET(STDIN_FILENO, &newset)) {
            fgets(msg, BUF_SIZE, stdin);
            if (!strncmp(msg, "quit\n", 5)) {
                *sock = -1;
                return NULL;
            }
            else if (msg[0] != '[') {
                strcat(name_msg, "[ALLMSG]");
                strcat(name_msg, msg);
            }
            else
                strcpy(name_msg, msg);
            if (write(*sock, name_msg, strlen(name_msg)) <= 0) {
                *sock = -1;
                return NULL;
            }
        }
        if (ret == 0) {
            if (*sock == -1)
                return NULL;
        }
    }
}

void timer_handler(int signum) {
    timer_expired = 1;
}

void* recv_msg(void* arg) {
    struct sigaction sa;
    struct itimerval timer;

    sa.sa_handler = &timer_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    int* sock = (int*)arg;
    int i;
    char* pToken;
    char* pArray[ARR_CNT] = { 0 };

    char name_msg[NAME_SIZE + BUF_SIZE + 1];
    int str_len;

    while (1) {
        memset(name_msg, 0x0, sizeof(name_msg));
        str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
        if (str_len <= 0) {
            *sock = -1;
            return NULL;
        }
        name_msg[str_len] = 0;
        fputs(name_msg, stdout);

        pToken = strtok(name_msg, "[:@]");
        i = 0;
        while (pToken != NULL) {
            pArray[i] = pToken;
            if (++i >= ARR_CNT)
                break;
            pToken = strtok(NULL, "[:@]");
        }
        timer_set_flag = 0;
        if (!strcmp(pArray[1], "UPDATE") && (i == 3)) {
            timer.it_value.tv_sec = 30;
            timer.it_value.tv_usec = 0;

            timer.it_interval.tv_sec = 30;
            timer.it_interval.tv_usec = 0;
	    timer_set_flag = 1;
            setitimer(ITIMER_REAL, &timer, NULL);

        }
        else
            continue;
        if (timer_set_flag)
            printf("timer_set_complete");
        else
            fprintf(stderr, "ERROR: Timer\n");
    }
}

void* hrv_msg(void* arg) {
    while (1) {
        if (timer_expired) {
            MYSQL* conn = mysql_init(NULL);
            if (conn == NULL) {
                fprintf(stderr, "mysql_init() failed\n");
                pthread_exit(NULL);
            }
            if (mysql_real_connect(conn, HOST, USER, PASS, DB, 0, NULL, 0) == NULL) {
                finish_with_error(conn);
            }

            // Heartbeat query
            char query[256];
            snprintf(query, sizeof(query), "SELECT hbeat FROM lull_sensor WHERE time >= NOW() - INTERVAL 6 HOUR;");
            if (mysql_query(conn, query)) {
                finish_with_error(conn);
            }

            MYSQL_RES* result = mysql_store_result(conn);
            if (result == NULL) {
                finish_with_error(conn);
            }

            int num_fields = mysql_num_fields(result);
            MYSQL_ROW sqlrow;
            int i = 0;
            while ((sqlrow = mysql_fetch_row(result))) {
                hbeat[i] = atoi(sqlrow[0]);
                i++;
            }
            mysql_free_result(result);
            mysql_close(conn);


            // Temp and humidity query
            conn = mysql_init(NULL);
            if (conn == NULL) {
                fprintf(stderr, "mysql_init() failed\n");
                pthread_exit(NULL);
            }
            if (mysql_real_connect(conn, HOST, USER, PASS, DB, 0, NULL, 0) == NULL) {
                finish_with_error(conn);
            }
            snprintf(query, sizeof(query), "SELECT temp, humi FROM lull_sensor ORDER BY date ASC, time ASC LIMIT 1");
            if (mysql_query(conn, query)) {
                finish_with_error(conn);
            }

            result = mysql_store_result(conn);
            if (result == NULL) {
                finish_with_error(conn);
            }

            while ((sqlrow = mysql_fetch_row(result))) {
                temp = atoi(sqlrow[0]);
                humi = atoi(sqlrow[1]);
            }

            calculateHRVFromHeartRateNSendData(hbeat,num_fields,temp,humi,&hrv);
            mysql_free_result(result);
            mysql_close(conn);

            timer_expired = 0;
        }
        usleep(100000);
    }
}

void error_handling(char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

