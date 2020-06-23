// master.c
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
#define master_IOCTL_CREATESOCK 0x12345677
#define master_IOCTL_MMAP 0x12345678
#define master_IOCTL_EXIT 0x12345679
#define MAP_SIZE 409600
int N;
int device_fd, file_fd[128], file_sz[128];
char filename[128][128];
char method; // 'f' if file IO , 'm' if mmap
char buf[BUFFER_SIZE];
char write_buf[410000];
size_t file_size, ret, device_offset = 0;
struct timeval start_time, end_time;
char *from_file, *to_device;
const char special_char = 'E';
int total_file_size = 0;
int size_len = 0;
int PAGE_SIZE = 4096;
int write_size = 0, file_index = 0;
int start_point;
int file_idx = 0;
int device_mmap_offset = 0;
int file_mmap_offset = 0;
int file_offset = 0;
char *dst;
char *src;
char *sf;
int det;
int sf_fd;
int get_size();
void open_device();
void open_connect();
void close_connect();
void clean_all();

int main(int argc, char *argv[]){
    char *check;
    PAGE_SIZE = getpagesize();
    N = atoi(argv[1]);
    for (int i = 0 ; i < N ; i++) {
        strcpy(filename[i],argv[i+2]);
        if((file_fd[i] = open(argv[i+2] , O_RDWR)) < 0){
            perror("Open file error\n");
            exit(4);
        }
    }
    method = argv[N+2][0];
    
    open_device();
    open_connect();
    if(method == 'm'){
        sf_fd=open("size_file" , O_CREAT|O_RDWR);
    }
    
    size_len = get_size();
    start_point = size_len;
    gettimeofday(&start_time,NULL);
    
    switch(method){
        case 'f' :
            for(int i = 0 ; i < N ; i++){
                while(file_sz[i] > 0){
                    size_t ret = (sizeof(buf) > file_sz[i]) ? file_sz[i] : sizeof(buf);
                    ret = read(file_fd[i] , buf , ret);
                    write(device_fd , buf , ret);
                    file_sz[i] -= ret;
                }
            }
            break;
        case 'm' :
            sf = mmap(NULL,start_point,PROT_WRITE|PROT_READ, MAP_SHARED, sf_fd, 0);
            dst = mmap(NULL,MAP_SIZE,PROT_WRITE|PROT_READ, MAP_SHARED, device_fd, device_offset);
            memcpy(dst,sf,start_point);
            //printf("dedvice_mmap_offset %d\n",device_mmap_offset);
            device_mmap_offset = start_point;
            while(file_idx < N){
                file_offset = 0;
                while(file_sz[file_idx] > 0){
                    int ret;
                    if(file_sz[file_idx] <= MAP_SIZE){
                        src=mmap(NULL,file_sz[file_idx],PROT_WRITE|PROT_READ, MAP_SHARED, file_fd[file_idx], file_offset);
                        ret = file_sz[file_idx];
                        file_sz[file_idx] = 0;
                        file_idx++;
                    }
                    else{
                        src=mmap(NULL,MAP_SIZE,PROT_WRITE|PROT_READ, MAP_SHARED, file_fd[file_idx], file_offset);
                        file_sz[file_idx] -= MAP_SIZE;
                        ret = MAP_SIZE;
                        file_offset += MAP_SIZE;
                    }
                    //printf("%d\n",ret);
                    if(device_mmap_offset + ret > MAP_SIZE){
                        memcpy(&dst[device_mmap_offset],src,MAP_SIZE - device_mmap_offset);
                        ioctl(device_fd,0x12345678,MAP_SIZE);
                        //printf("%d\n",det);
                        file_mmap_offset = MAP_SIZE - device_mmap_offset;
                        device_offset += MAP_SIZE;
                        munmap(dst,MAP_SIZE);
                        dst = mmap(NULL,MAP_SIZE,PROT_WRITE|PROT_READ, MAP_SHARED, device_fd, device_offset);
                        memcpy(dst,&src[file_mmap_offset],device_mmap_offset+ret-MAP_SIZE);
                        device_mmap_offset = device_mmap_offset + ret- MAP_SIZE;
                    }
                    else{
                        memcpy(&dst[device_mmap_offset],src,ret);
                        //printf("------------------\n");
                        //printf("%s\n",dst);
                        device_mmap_offset += ret;
                        if(file_idx == N){
                            ioctl(device_fd,0x12345678,device_mmap_offset);
                            //printf("det: %d\n",det);
                        }

                    }
                    //printf("device_mmap_offset: %d\n",device_mmap_offset);
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


int get_size(){
    int total = 0;
    char size_buf[20];
    for(int i = 0 ; i < N ; i++){
        struct stat st_buf;
        stat(filename[i], &st_buf);
        sprintf(size_buf , "%lld%c\0" , st_buf.st_size , special_char);
        int len = strlen(size_buf);
        if(method == 'f'){
            write(device_fd, size_buf, len);
        }
        else{
            write(sf_fd, size_buf, len);
        }
        total += len;
        total_file_size += st_buf.st_size;
        file_sz[i] = st_buf.st_size;
    }

    return total;
}


void open_device(){
    if((device_fd = open("/dev/master_device" , O_RDWR)) < 0){
        perror("Open device error\n");
        exit(1);
    }
}


void open_connect(){
    if(ioctl(device_fd , master_IOCTL_CREATESOCK) == -1){
        perror("Open connect error\n");
        exit(2);
    }
}

void close_connect(){
    if(ioctl(device_fd , master_IOCTL_EXIT) == -1){
        perror("Close connect error\n");
        exit(3);
    }
}

void clean_all(){
    close(device_fd);
    for (int i = 0 ; i < N ; i ++) close(file_fd[i]);
}
