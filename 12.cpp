#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdint>
#include <cinttypes>
#include <ctime>
#include <atomic>
#include <mutex>
#include <condition_variable>

using namespace std;

struct async_op {
    struct aiocb control;
    char *data_buf;
    int is_write;
    void* next_op;
};

int input_fd = -1;
int output_fd = -1;
off_t total_bytes = 0;
atomic<int> finished_count(0);
atomic<int> total_ops(0);
atomic<int> next_block_idx(0);
int total_blocks_count = 0;
int current_block_size = 4096;
atomic<bool> copy_done(false);
mutex sync_mtx;
condition_variable sync_cv;

void op_complete_callback(sigval_t sigval);
void launch_next_read();

async_op* build_read_op(int fd, off_t offset, size_t size) {
    async_op *op = new async_op();
    
    op->data_buf = new char[size]();
    
    memset(&op->control, 0, sizeof(op->control));
    op->control.aio_fildes = fd;
    op->control.aio_buf = op->data_buf;
    op->control.aio_nbytes = size;
    op->control.aio_offset = offset;
    
    op->control.aio_sigevent.sigev_notify = SIGEV_THREAD;
    op->control.aio_sigevent.sigev_notify_function = op_complete_callback;
    op->control.aio_sigevent.sigev_notify_attributes = NULL;
    op->control.aio_sigevent.sigev_value.sival_ptr = op;
    
    op->is_write = 0;
    op->next_op = nullptr;
    
    return op;
}

async_op* build_write_op(int fd, off_t offset, size_t size, char *buffer) {
    async_op *op = new async_op();
    
    memset(&op->control, 0, sizeof(op->control));
    op->data_buf = buffer;
    
    op->control.aio_fildes = fd;
    op->control.aio_buf = op->data_buf;
    op->control.aio_nbytes = size;
    op->control.aio_offset = offset;
    
    op->control.aio_sigevent.sigev_notify = SIGEV_THREAD;
    op->control.aio_sigevent.sigev_notify_function = op_complete_callback;
    op->control.aio_sigevent.sigev_notify_attributes = NULL;
    op->control.aio_sigevent.sigev_value.sival_ptr = op;
    
    op->is_write = 1;
    op->next_op = nullptr;
    
    return op;
}

bool prepare_files(const char* src_name, const char* dst_name) {
    struct stat file_stat;

    input_fd = open(src_name, O_RDONLY | O_NONBLOCK, 0666);
    if (input_fd == -1) {
        perror("Source file open error");
        return false;
    }

    output_fd = open(dst_name, O_CREAT | O_WRONLY | O_TRUNC | O_NONBLOCK, 0666);
    if (output_fd == -1) {
        perror("Destination file open error");
        close(input_fd);
        return false;
    }
    
    if (fstat(input_fd, &file_stat) == -1) {
        perror("File stat error");
        close(input_fd);
        close(output_fd);
        return false;
    }
    
    total_bytes = file_stat.st_size;
    cout << "Source file size: " << total_bytes << " bytes" << endl;
    return true;
}

void launch_next_read() {
    int block_idx = next_block_idx.fetch_add(1);
    
    if (block_idx < total_blocks_count) {
        off_t offset = block_idx * current_block_size;
        size_t size = (block_idx == total_blocks_count - 1) ? (total_bytes - offset) : current_block_size;
        
        cout << "Starting read block " << block_idx << " (offset=" << offset << ", size=" << size << ")" << endl;
        
        async_op *read_op = build_read_op(input_fd, offset, size);
        
        if (aio_read(&read_op->control) == -1) {
            perror("Read start error");
            delete[] read_op->data_buf;
            delete read_op;
        } else {
            total_ops++;
        }
    }
}

void op_complete_callback(sigval_t sigval) {
    async_op *completed_op = static_cast<async_op*>(sigval.sival_ptr);
    int result = aio_return(&completed_op->control);

    if (result < 0) {
        cerr << "Operation error: " << strerror(errno) << endl;
        if (completed_op->is_write) {
            delete[] completed_op->data_buf;
        }
        delete completed_op;
        finished_count++;
        sync_cv.notify_one();
        return;
    }

    if (completed_op->is_write) {
        cout << "Write complete: " << result << " bytes (offset=" << completed_op->control.aio_offset << ")" << endl;
        
        delete[] completed_op->data_buf;
        delete completed_op;
        finished_count++;
        launch_next_read();
        
    } else {
        cout << "Read complete: " << result << " bytes (offset=" << completed_op->control.aio_offset << ")" << endl;
        async_op *write_op = build_write_op(output_fd, completed_op->control.aio_offset, result, completed_op->data_buf);
        if (aio_write(&write_op->control) == -1) {
            perror("Write start error");
            delete[] completed_op->data_buf;
            delete write_op;
        } else {
            total_ops++;
        }
        
        delete completed_op;
        finished_count++;
    }
    if (finished_count >= total_ops && 
        next_block_idx >= total_blocks_count) {
        copy_done = true;
        sync_cv.notify_one();
    }
}

bool execute_copy(const char* source, const char* dest, int block_sz, int parallel_ops) {
    cout << "\n=== COPY START ===" << endl;
    cout << "Source: " << source << endl;
    cout << "Destination: " << dest << endl;
    cout << "Block size: " << block_sz << " bytes" << endl;
    cout << "Parallel ops: " << parallel_ops << endl;
    
    if (!prepare_files(source, dest)) {
        return false;
    }

    struct timespec time_start, time_end;
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    
    finished_count = 0;
    total_ops = 0;
    next_block_idx = 0;
    copy_done = false;
    current_block_size = block_sz;
    
    total_blocks_count = (total_bytes + block_sz - 1) / block_sz;
    cout << "Total blocks: " << total_blocks_count << endl;
    
    int start_ops = min(parallel_ops, total_blocks_count);
    for (int i = 0; i < start_ops; i++) {
        launch_next_read();
    }
    
    cout << "\nWaiting for operations... (total: " << total_ops << ")" << endl;
    
    {
        unique_lock<mutex> lock(sync_mtx);
        while (!copy_done) {
            sync_cv.wait_for(lock, chrono::seconds(1));
            cout << "\rProgress: " << finished_count << "/" << total_ops << " ops" << flush;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    
    close(input_fd);
    close(output_fd);
    
    double elapsed = (time_end.tv_sec - time_start.tv_sec) + (time_end.tv_nsec - time_start.tv_nsec) / 1e9;
    double speed = total_bytes / elapsed;
    double speed_mb = speed / (1024 * 1024);
    
    cout << "\nCopy completed successfully!" << endl;
    cout << "  Total operations: " << total_ops << endl;
    cout << "  Time: " << elapsed << " seconds" << endl;
    cout << "  Speed: " << speed_mb << " MB/s" << endl;
    
    return true;
}

bool validate_copy(const char* src, const char* dst) {
    cout << "\nVerifying copy..." << endl;
    
    FILE *f_src = fopen(src, "rb");
    FILE *f_dst = fopen(dst, "rb");
    
    if (!f_src || !f_dst) {
        cout << "Cannot open files for verification!" << endl;
        if (f_src) fclose(f_src);
        if (f_dst) fclose(f_dst);
        return false;
    }
    
    fseek(f_src, 0, SEEK_END);
    fseek(f_dst, 0, SEEK_END);
    long sz_src = ftell(f_src);
    long sz_dst = ftell(f_dst);
    
    if (sz_src != sz_dst) {
        cout << "Size mismatch! (" << sz_src << " vs " << sz_dst << ")" << endl;
        fclose(f_src);
        fclose(f_dst);
        return false;
    }

    rewind(f_src);
    rewind(f_dst);
    
    const int BUF_SZ = 4096;
    char buf_src[BUF_SZ];
    char buf_dst[BUF_SZ];
    
    size_t bytes_read;
    bool match = true;
    
    while ((bytes_read = fread(buf_src, 1, BUF_SZ, f_src)) > 0) {
        fread(buf_dst, 1, bytes_read, f_dst);
        if (memcmp(buf_src, buf_dst, bytes_read) != 0) {
            match = false;
            break;
        }
    }
    
    fclose(f_src);
    fclose(f_dst);
    
    if (match) {
        cout << "Files are identical!" << endl;
        return true;
    } else {
        cout << "Files differ!" << endl;
        return false;
    }
}

void show_menu(char src_path[256], char dst_path[256], int blk_sz, int ops_cnt) {
    cout << "\n========================================" << endl;
    cout << "  ASYNC FILE COPY (POSIX AIO)" << endl;
    cout << "========================================" << endl;
    cout << "1. Set source file (current: " << (strlen(src_path) > 0 ? src_path : "Not set") << ")" << endl;    
    cout << "2. Set destination file (current: " << (strlen(dst_path) > 0 ? dst_path : "Not set") << ")" << endl;    
    cout << "3. Set block size (current: " << blk_sz << ")" << endl;
    cout << "4. Set parallel operations (current: " << ops_cnt << ")" << endl;
    cout << "5. Execute copy" << endl;
    cout << "6. Verify result" << endl;
    cout << "0. Exit" << endl;
    cout << "========================================" << endl;
    cout << "Choice: ";
}

int main() {
    int option;
    char source_file[256] = "";
    char dest_file[256] = "";
    int block_sz = 4096;
    int parallel_ops = 4;
    
    cout << "POSIX AIO Async File Copy Program" << endl;
    cout << "Recommended values:" << endl;
    cout << "- Block size: 1024, 2048, 4096, 8192, 16384, 32768, 65536" << endl;
    cout << "- Parallel ops: 1, 2, 4, 8, 12, 16" << endl;
    
    do {
        show_menu(source_file, dest_file, block_sz, parallel_ops);
        cin >> option;
        
        switch(option) {
            case 1:
                cout << "Enter source file path: ";
                cin >> source_file;
                break;
                
            case 2:
                cout << "Enter destination file path: ";
                cin >> dest_file;
                break;
                
            case 3:
                cout << "Recommended block sizes:" << endl;
                cout << "- 1024, 2048, 4096, 8192, 16384, 32768, 65536" << endl;
                cout << "Enter block size (bytes): ";
                cin >> block_sz;
                if (block_sz < 512) block_sz = 512;
                break;
                
            case 4:
                cout << "Recommended parallel ops:" << endl;
                cout << "- 1, 2, 4, 8, 12, 16" << endl;
                cout << "Enter operations count: ";
                cin >> parallel_ops;
                if (parallel_ops < 1) parallel_ops = 1;
                if (parallel_ops > 16) parallel_ops = 16;
                break;
                
            case 5:
                if (strlen(source_file) == 0 || strlen(dest_file) == 0) {
                    cout << "Please set both source and destination files first!" << endl;
                } else {
                    execute_copy(source_file, dest_file, block_sz, parallel_ops);
                }
                break;
                
            case 6:
                if (strlen(source_file) == 0 || strlen(dest_file) == 0) {
                    cout << "Please set both source and destination files first!" << endl;
                } else {
                    validate_copy(source_file, dest_file);
                }
                break;
                
            case 0:
                cout << "Exiting..." << endl;
                break;
                
            default:
                cout << "Invalid choice!" << endl;
        }
    } while(option != 0);
    
    return 0;
}
