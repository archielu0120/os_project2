// slave.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define BUFFER_SIZE 512

int  N;
int device_fd, file_fd[128];
char method;
char master_ip[16];
char buf[BUFFER_SIZE];
size_t file_size, ret, offset = 0, device_offset = 0;
struct timeval start_time, end_time;
char *from_device, *to_file;
const char special_char = 'E';
int total_file_size = 0, file_index = 0;

void open_device();
void establish_connect();
void close_connect();
void clean_all();

int main(int argc, char *argv[]){
    N = atoi(argv[1]);
    for (int i = 0 ; i < N ; i++) {
        if((file_fd[i] = open(argv[i+2] , O_RDWR | O_CREAT | O_TRUNC )) < 0){
            perror("Create file error\n");
            exit(4);
        }
    }
    method = argv[N+2][0];
    strcpy(master_ip , argv[N+3]);
    
    open_device();
    establish_connect();
    
    gettimeofday(&start_time,NULL);
    
    switch(method){
        case 'f' :
            for(int i = 0 ; i < N ; i++){
                char c = '0';
                int size = 0;
                while (c != special_char) {
                    size = 10 * size + c - '0';
                    if (read(device_fd , &c , sizeof(c)) != 1){
                        printf("READ FAIL\n");
                        
                    }
                    printf("%c\n",c);
                }
                total_file_size += size;
                while(size > 0){
                    size_t ret = (sizeof(buf) > size) ? size : sizeof(buf);
                    ret = read(device_fd , buf , ret);
                    write(file_fd[i] , buf , ret);
                    size -= ret;
                }
            }
            break;
        case 'm' :
            while(file_index < N){
                offset = 0;
                char c[] = {'0'};
                int size = 0;
                while (c[0] != special_char) {
                    size = 10 * size + c[0] - '0';
                    printf("%c\n",c[0]);
                    read(device_fd , &c , 1);
                    device_offset ++;
                }
                while(size > 0){
                    int len = (size >= 409600) ? 409600 : size;
                    posix_fallocate(file_fd[file_index],offset,len);
                    from_device = mmap(NULL, len , PROT_READ , MAP_SHARED , device_fd , device_offset);
                    to_file = mmap(NULL, len , PROT_WRITE , MAP_SHARED , file_fd[file_index] , offset);
                    memcpy(to_file, from_device , len);
                    offset += len;
                    size -= len;
                    device_offset += len;
                    ioctl(device_fd , 0x12345678,len);
                }
                file_index ++;
            }
            break;
        default :
            perror("Method error\n");
            return -1;
    }
    
    
    close_connect();
    gettimeofday(&end_time, NULL);
    double trans_time = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)*0.0001;
    
    
    printf("Transmission time: %lf ms, File size: %d bytes\n", trans_time, total_file_size / 8);
    
    
    clean_all();
    return 0;
}



void open_device(){
    if((device_fd = open("/dev/slave_device" , O_RDWR)) < 0){
        perror("Open device error\n");
        exit(1);
    }
}


void establish_connect(){
    if(ioctl(device_fd , 0x12345677 , master_ip) == -1){
        perror("Connect master error\n");
        exit(2);
    }
}

void close_connect(){
    if(ioctl(device_fd , 0x12345679) == -1){
        perror("Close connect error\n");
        exit(3);
    }
}


void clean_all(){
    close(device_fd);
    for (int i = 0 ; i < N ; i ++) close(file_fd[i]);
}
