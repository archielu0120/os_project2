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
#define slave_IOCTL_CREATESOCK 0x12345677
#define slave_IOCTL_MMAP 0x12345678
#define slave_IOCTL_EXIT 0x12345679

int  N;
int device_fd, file_fd[128], file_sz[128];
char method;
char master_method;
char master_ip[16];
char buf[BUFFER_SIZE];
size_t file_size, ret, device_offset = 0;
struct timeval start_time, end_time;
char *from_device, *to_file;
const char special_char = 'E';
int total_file_size = 0, file_index = 0;
int PAGE_SIZE = 4096;

void open_device();
void establish_connect();
void close_connect();
void clean_all();
int get_size_from_read();
void get_size_for_mmap();

int main(int argc, char *argv[]){
    PAGE_SIZE = getpagesize();
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
    
    int begin_index = 0, init = 0;
    if(method == 'f'){
        begin_index = get_size_from_read();
        if(begin_index != -1) init = 1;
        printf("init = %d\n",init);
    }
    else{
        get_size_for_mmap();
        device_offset += PAGE_SIZE;
    }
    gettimeofday(&start_time,NULL);
    
    switch(method){
        case 'f' :
            while(file_index < N){
                int len;
                if(init == 1) {
                    init = 0;
                    len = strlen(buf);

                }
                else{
                    begin_index = 0;
                    len = read(device_fd, buf, BUFFER_SIZE);
                    if(len == 0) continue;
                }
                printf("%d\n",len);
                while(begin_index < len){
                    if(file_sz[file_index] >= len-begin_index){
                        write(file_fd[file_index], &buf[begin_index], len-begin_index);
                        file_sz[file_index] -= (len-begin_index);
                        if(file_sz[file_index] == 0) file_index ++;
                        break;
                    }
                    else{
                        write(file_fd[file_index], &buf[begin_index], file_sz[file_index]);
                        begin_index += file_sz[file_index];
                        file_sz[file_index] = 0;
                        file_index ++;
                        if(file_index >= N) break;
                    }
                    
                }
            }
            break;
        case 'm' :
            for(int i = 0; i<N; i++){
                printf("%d\n",file_sz[i]);
             }
            for(int i = 0 ; i < N ; i++){

                int offset = 0;
                int num_of_page = file_sz[i] / PAGE_SIZE;
                if(file_sz[i] % PAGE_SIZE != 0) num_of_page ++;
                printf("file %d, num_of_page %d\n",i,num_of_page);

                int rec;
                if(num_of_page <= 100){
                    for(int j = 0; j<num_of_page; j++){
                        rec = ioctl(device_fd, 0x12345678);
                        printf("rec: %d\n",rec);
                    }
                    printf("kernel ok\n");
                    printf("device offset %d\n",device_offset);
                    if(from_device = mmap(NULL, file_sz[i] , PROT_READ , MAP_SHARED , device_fd , device_offset) == (void *) - 1){
                        printf("%s\n",strerror(errno));
                    }
                    printf("device mmap\n");
                    printf("offset %d\n",offset);
                    if(to_file = mmap(NULL, file_sz[i] , PROT_WRITE , MAP_SHARED ,  file_fd[i], offset) == (void *) - 1){
                        printf("%s\n",strerror(errno));
                    }
                    printf("file mmap\n");
                    memcpy(to_file, from_device, file_sz[i]);

                    device_offset += num_of_page * PAGE_SIZE;

                }
                else{
                    
                    while(file_sz[i] > 0){
                        int len = (file_sz[i] >= 409600) ? 409600 : file_sz[i];
                        from_device = mmap(NULL, len , PROT_READ , MAP_SHARED , device_fd , device_offset);
                        to_file = mmap(NULL, len , PROT_WRITE , MAP_SHARED , file_fd[i] , offset);
                        memcpy(to_file, from_device, len);
                        offset += len;
                        file_sz[i] -= len;
                        if(file_sz[i] != 0) device_offset += PAGE_SIZE * 100;
                        else {
                            device_offset += (len / PAGE_SIZE) * PAGE_SIZE;
                            if(len % PAGE_SIZE != 0) device_offset += PAGE_SIZE;
                        }
                    }
                }
            }
            break;
        default :
            perror("Method error\n");
            return -1;
    }
    
    
    close_connect();
    gettimeofday(&end_time, NULL);
    double trans_time = (end_time.tv_sec - start_time.tv_sec)*1000 + (end_time.tv_usec - start_time.tv_usec)*0.0001;
    
    
    printf("Transmission time: %lf ms, File size: %d bytes\n", trans_time, total_file_size);
    
    
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
    if(ioctl(device_fd , slave_IOCTL_CREATESOCK , master_ip) == -1){
        perror("Connect master error\n");
        exit(2);
    }
}


void close_connect(){
    if(ioctl(device_fd , slave_IOCTL_EXIT) == -1){
        perror("Close connect error\n");
        exit(3);
    }
}


void clean_all(){
    close(device_fd);
    for (int i = 0 ; i < N ; i ++) close(file_fd[i]);
}
 
    
int get_size_from_read(){
    int f_index = 0, END = 0;
    int index = 0, ret = 0;
    while(!END){
        ret = read(device_fd, buf, BUFFER_SIZE);
        printf("ret = %d\n",ret);
        if(ret == 0) continue;
        index = 0;
        while(index < ret && f_index < N){
            device_offset ++;
            if(buf[index] == special_char){
                total_file_size += file_sz[f_index];
                f_index ++;
                if(f_index >= N) END = 1;
                index ++;
                continue;
            }
            file_sz[f_index] = file_sz[f_index] * 10 + buf[index] - '0';
            index ++;
        }
    }
    printf("index = %d\n",index);
    if(index < ret) return index;
    else return -1;
}

void get_size_for_mmap(){
    char size_buf[4100];
    read(device_fd, size_buf, BUFFER_SIZE);
    printf("%s\n",size_buf);
    int file_cnt = 0,  size = 0;
    for(int i = 0 ; file_cnt < N ; i++){
        if(size_buf[i] != special_char){
            size = size * 10 + size_buf[i] - '0';
        }
        
        else{
            file_sz[file_cnt] = size;
            size = 0;
            file_cnt ++;
        }
    }
    for(int i = 0; i<7; i++){
        read(device_fd, size_buf, BUFFER_SIZE);
    }
}
