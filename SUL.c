#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>



#define SUL_port_control 5000
#define MAX_PORT 65535
#define MIN_PORT 10000
#define BUFFER_SIZE 1024
#define MAX_SOCKETS 100 // 假设我们最多有10个套接字

int offset = 0;
FILE* log_file;

int mappings_size = 0; // 当前映射的数量

typedef struct {
    int SUL_port;
    int main_socket;  
    int secondary_socket;  
} PortSocketMapping;

typedef struct {
    int param1;
    int param2;
} ThreadArgs;

PortSocketMapping mappings[MAX_SOCKETS];

// 添加一个新的映射
void add_new_mapping(int SUL_port, int main_socket) {
    if (mappings_size >= MAX_SOCKETS) {
        printf("Error: Maximum number of sockets reached.================================================================\n");
        exit(-1);
        return;
    }
    
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (mappings[i].SUL_port == -1){
            mappings[i].SUL_port = SUL_port;
            mappings[i].main_socket = main_socket;
            return;
        }
    }

    mappings_size++;
    return;
}

// 添加一个main_socket新的映射
void add_main_socket_mapping(int SUL_port, int main_socket) {
    
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (mappings[i].SUL_port == SUL_port){
            mappings[i].main_socket = main_socket;
            return;
        }
    }

    return;
}

// 添加一个secondary_socket新的映射
void add_secondary_socket_mapping(int SUL_port, int secondary_socket) {
    
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (mappings[i].SUL_port == SUL_port){
            mappings[i].secondary_socket = secondary_socket;
            return;
        }
    }

    return;
}

// 删除一个映射，通过端口号
void remove_mapping_by_port(int SUL_port) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (mappings[i].SUL_port == SUL_port) {
            mappings[i].SUL_port = -1;
            mappings[i].main_socket = -1;
            mappings[i].secondary_socket = -1;
            mappings_size--;
            return;
        }
    }
}

// 函数来获取main_socket套接字，通过端口号
int get_main_socket_by_port(int SUL_port) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (mappings[i].SUL_port == SUL_port) {
            return mappings[i].main_socket;
        }
    }
    printf("get_main_socket_by_port.==================================================================================\n");
    exit(-1);
    return -1;
}

// 函数来获取secondary_socket套接字，通过端口号
int get_secondary_socket_by_port(int SUL_port) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (mappings[i].SUL_port == SUL_port) {
            return mappings[i].secondary_socket;
        }
    }
    printf("get_secondary_socket_by_port.==================================================================================\n");
    exit(-1);
    return -1;
}


// 生成随机端口号
int generate_random_port() {
    /*
    int count = 0;
    while(1) {
        count += 1;
        if (count == 10){
            printf("Get Port failed");
            exit(EXIT_FAILURE);
        }
    
        int port = rand() % (MAX_PORT - MIN_PORT + 1) + MIN_PORT;
        for (int i = 0; i < MAX_SOCKETS; i++) {
            if (port == Port_set[i]){
                continue;
            }
        }
        return port;
    }
    */
    offset += 1;
    return offset % (MAX_PORT - MIN_PORT + 1) + MIN_PORT;
}

void process_accept(int SUL_port, int main_socket) {    
    int flags = fcntl(main_socket, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (fcntl(main_socket, F_SETFL, flags) == -1) {
        perror("fcntl(F_SETFL)");
    }
    
    int secondary_socket = -1;
    
    fprintf(log_file,"Port %d: enter process_accept", SUL_port);
    printf("Port %d: enter process_accept\n", SUL_port);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    secondary_socket = accept(main_socket, (struct sockaddr *)&client_addr, &client_len);
    
    
    fprintf(log_file, "open second %d\n", secondary_socket);
    if (secondary_socket == -1) {
        fprintf(log_file,"Port %d: accept failed", SUL_port);
        printf("Port %d: accept failed\n", SUL_port);

    }else{
        fprintf(log_file, "SUL_port %d: accept succeed, secondary_socket:%d\n", SUL_port, secondary_socket);
        printf("SUL_port %d: accept succeed, secondary_socket:%d\n", SUL_port, secondary_socket);
        
        close(get_secondary_socket_by_port(SUL_port));
        
        add_secondary_socket_mapping(SUL_port, secondary_socket);
        
        int i = 1;
        setsockopt(secondary_socket, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i));
        setsockopt(secondary_socket, IPPROTO_TCP, TCP_QUICKACK, (void *)&i, sizeof(i));
    }
}


void process_rcv(int secondary_socket) {
    if (secondary_socket == -1){
        return;
    }
    char read_buffer[1025];
    recv(secondary_socket, read_buffer, sizeof(read_buffer), MSG_DONTWAIT);
}


void handle_close(int main_socket, int SUL_port) {

    int res;
    if (main_socket == -1){
        return;
    }
    res = close(main_socket);
    if (res == 0) {
        fprintf(log_file,"close main_socket succeded %d\n", main_socket);
        printf("close main_socket succeded\n");
        add_main_socket_mapping(SUL_port, -1);
    } else {
        fprintf(log_file, "close main_socket failed");
        printf("close main_socket failed");
    }
    
}

void handle_closeconnection(int secondary_socket, int SUL_port) {

    int res;
    if (secondary_socket == -1){
        return;
    }
    
    res = close(secondary_socket);
    if (res == 0) {
        fprintf(log_file, "close secondary_socket succeded %d\n", secondary_socket);
        printf("close secondary_socket succeded\n");
        
        add_secondary_socket_mapping(SUL_port, -1);
    } else {
        fprintf(log_file, "close secondary_socket failed");
        printf("close secondary_socket failed");
    }

}


void handle_reset(int port, int main_socket, int secondary_socket) {
    if (main_socket != -1){
        close(main_socket);
    }

    if (secondary_socket != -1){
        close(secondary_socket);
    }
    
    remove_mapping_by_port(port);
}

// 按字节读取消息，直到遇到\n
char* receive_message(int control_socket){
    char buffer[BUFFER_SIZE]; // 使用静态分配的缓冲区
    ssize_t bytes_received;
    size_t message_length = 0;

    // 循环读取数据直到 '\n' 或缓冲区满
    while (message_length < (BUFFER_SIZE - 1)) { // 留出一个字符的空间给 '\0'
        bytes_received = recv(control_socket, &buffer[message_length], 1, 0);

        // 检查是否接收到 '\n'
        if (buffer[message_length] == '\n') {
            message_length++;
            break;
        }

        message_length++;
    }
    
    buffer[message_length] = '\0';

    // 使用动态内存分配来存储接收到的消息
    char *dynamic_buffer = (char *)malloc((message_length + 1) * sizeof(char));
    strcpy(dynamic_buffer, buffer);

    // 返回接收到的消息
    return dynamic_buffer;

}

// 处理客户端连接
void handle_control_cmd(int control_socket) {
    fprintf(log_file, "---------------------------------------------------------------------------------\n");
    printf("---------------------------------------------------------------------------------\n");
    //fflush(log_file);
    
    char* message;
    message = receive_message(control_socket);
    
    //fprintf(log_file, "message:%s", message);
    printf("message:%s\n", message);
    
    if (strcmp(message, "port\n") == 0) {
        fprintf(log_file, "process port: %s\n", message);
        printf("process port\n");
    
        int count = 0;
    	while(1){
    	    count += 1;
    	    if (count == 100){
    	        printf("No available port========================================================================================\n");
    	        exit(EXIT_FAILURE);
    	    }
            // 如果收到 "port"，生成一个随机端口并创建新的socket
            int SUL_port = generate_random_port();
            int main_socket = socket(AF_INET, SOCK_STREAM, 0);
            fprintf(log_file,"open main %d\n", main_socket);

            if (main_socket < 0) {
                fprintf(log_file, "Creat main_socket failed, Error: %s\n", strerror(errno));
                printf("Creat main_socket failed, Error: %s\n", strerror(errno));
                close(main_socket);
                continue;
            }

            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(SUL_port);

            int i = 1;
            setsockopt(main_socket, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i));
            setsockopt(main_socket, IPPROTO_TCP, TCP_QUICKACK, (void *)&i, sizeof(i));
            setsockopt(main_socket, IPPROTO_TCP, SO_REUSEADDR, (void *)&i, sizeof(i));


            if (bind(main_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                fprintf(log_file, "Failed to bind port: %d, Error: %s, main_socket: %d\n", SUL_port, strerror(errno), main_socket);
                printf("Failed to bind port: %d, Error: %s, main_socket: %d\n", SUL_port, strerror(errno), main_socket);
                fprintf(log_file, "close main %d\n", main_socket);
                close(main_socket);
                continue;
            }

            //fprintf(log_file, "socket fd: %d\n", main_socket);
            printf("socket fd: %d\n", main_socket);
            
            /*
            for (int i = 0; i < MAX_SOCKETS; i++) {
                fprintf(log_file, "ALL socket fd: %d\n", mappings[i].main_socket);
                printf("ALL socket fd: %d\n", mappings[i].main_socket);
            }
            */

            //fprintf(log_file, "...\n");
            printf("...\n");
            
            add_new_mapping(SUL_port, main_socket);
            /*
            for (int i = 0; i < MAX_SOCKETS; i++) {
                fprintf(log_file, "ALL socket fd: %d\n", mappings[i].main_socket);
                printf("ALL socket fd: %d\n", mappings[i].main_socket);
            }
            */

            char port_str[6];
            sprintf(port_str, "%d\n", SUL_port);
            send(control_socket, port_str, strlen(port_str), 0);
            //fprintf(log_file, "Sent new port number: %d\n", SUL_port);
            printf("Sent new port number: %d\n", SUL_port);
            break;
        }
        free(message);
        return;
    }    

    int SUL_port = -1;
    char* str_port;
    str_port = receive_message(control_socket);
    str_port[strlen(str_port) - 1] = '\0';
    SUL_port = atoi(str_port);
    int main_socket = get_main_socket_by_port(SUL_port);
    int secondary_socket = get_secondary_socket_by_port(SUL_port);
    fprintf(log_file, "SUL_port,main_socket,secondary_socket: %d,%d,%d\n", SUL_port,main_socket,secondary_socket);
    printf("SUL_port,main_socket,secondary_socket: %d,%d,%d\n", SUL_port,main_socket,secondary_socket);
    
    if (strcmp(message, "listen\n") == 0){
        //fprintf(log_file, "process listen\n");
        printf("process listen\n");
    	int res;
    	res = listen(main_socket, 1);
    	if (res==0){
    	    //fprintf(log_file, "SUL listening on port %d...\n", SUL_port);
    	    printf("SUL listening on port %d...\n", SUL_port);
    	} else{
    	    perror("listen failed\n");
    	}
    	
    } else if (strcmp(message, "accept\n") == 0){
        fprintf(log_file, "process accept for port: %d\n", SUL_port);
        printf("Accept via port %d...\n", SUL_port);
        process_accept(SUL_port, main_socket);
    	
    } else if (strcmp(message, "rcv\n") == 0){
        //fprintf(log_file, "process rcv\n");
        printf("process rcv\n");
        process_rcv(secondary_socket);
        printf("Recv via port %d...\n", SUL_port);
    	
    } else if (strcmp(message, "close\n") == 0){
        //fprintf(log_file, "process close\n");
        printf("process close\n");
        handle_close(main_socket, SUL_port);
        
    } else if (strcmp(message, "closeconnection\n") == 0){
        //fprintf(log_file, "process closeconnection\n");
        printf("process closeconnection\n");
        handle_closeconnection(secondary_socket, SUL_port);
        
    } else if (strcmp(message, "reset\n") == 0){
        //fprintf(log_file, "process reset\n");
        printf("process reset\n");
        handle_reset(SUL_port, main_socket, secondary_socket);
    }
    free(message);
    free(str_port);
}



int main() {
    int server_socket, control_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    srand(time(NULL)); // 初始化随机数生成器
    
    log_file = fopen("log.txt", "a+");
    if (log_file != NULL) {
        // 文件存在
        fclose(log_file); // 关闭文件
        // 删除文件
        remove("log.txt");
        printf("File existed and has been deleted.\n");
    } else {
        // 文件不存在
        printf("File did not exist.\n");
    }
    log_file = fopen("log.txt", "a+");

    // 初始化port和socket的mapping
    for (int i = 0; i < MAX_SOCKETS; i++) {
        mappings[i].SUL_port = -1;
        mappings[i].main_socket = -1;
        mappings[i].secondary_socket = -1;
    }

    // 创建服务器socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int flag = 1; // 非零值，启用 TCP_NODELAY 选项
    setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    // 绑定socket到端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SUL_port_control);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 监听端口
    if (listen(server_socket, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //fprintf(log_file, "Server listening on port %d...\n", SUL_port_control);
    printf("Server listening on port %d...\n", SUL_port_control);
    


    // 接受客户端连接
    control_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (control_socket < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    
    //fprintf(log_file, "Connected to learner\n");
    printf("Connected to learner\n");


    while (1) {
        // 处理客户端请求
        handle_control_cmd(control_socket);
    }
    
    return 0;
}
