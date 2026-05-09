#include <reent.h>
#include <errno.h>
#include <ff_stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <UART_16550.h>
#include <fcntl.h>
// Simple mapping table: Newlib FD -> FreeRTOS+FAT File Handle
FF_FILE *fd_table[10]; 

int _open_r(struct _reent *r, const char *path, int flags, int mode) {
    // Note: You may need to translate 'flags' (O_RDONLY, etc.) to FF_MODE strings
    const char *ff_mode = "r"; 
    if (flags & O_WRONLY) ff_mode = "w";
    
    FF_FILE *pxFile = ff_fopen(path, ff_mode);
    
    if (pxFile == NULL) {
        r->_errno = ENOENT;
        return -1;
    }

    // Find an empty slot in your table
    for (int i = 3; i < 10; i++) { // Start at 3 to skip stdin/out/err
        if (fd_table[i] == NULL) {
            fd_table[i] = pxFile;
            return i;
        }
    }

    r->_errno = EMFILE;
    return -1;
}

_ssize_t _read_r(struct _reent *r, int fd, void *ptr, size_t len) {
    if (fd < 3 || fd_table[fd] == NULL) {
        r->_errno = EBADF;
        return -1;
    }

    size_t bytesRead = ff_fread(ptr, 1, len, fd_table[fd]);
    return (_ssize_t)bytesRead;
}

_off_t _lseek_r(struct _reent *r, int fd, _off_t pos, int whence) {
    if (fd < 3 || fd_table[fd] == NULL) {
        r->_errno = EBADF;
        return -1;
    }

    // Translate Newlib whence to FreeRTOS+FAT whence
    int ff_whence;
    switch (whence) {
        case SEEK_SET: ff_whence = FF_SEEK_SET; break;
        case SEEK_CUR: ff_whence = FF_SEEK_CUR; break;
        case SEEK_END: ff_whence = FF_SEEK_END; break;
        default: r->_errno = EINVAL; return -1;
    }

    int result = ff_fseek(fd_table[fd], pos, ff_whence);
    if (result == 0) {
        return ff_ftell(fd_table[fd]);
    }

    r->_errno = EINVAL;
    return -1;
}

int _close_r(struct _reent *r, int fd) {
    if (fd < 3 || fd_table[fd] == NULL) {
        r->_errno = EBADF;
        return -1;
    }

    ff_fclose(fd_table[fd]);
    fd_table[fd] = NULL;
    return 0;
}


int _fstat_r(struct _reent *r, int fd, struct stat *st) {
    if (fd < 3 || fd_table[fd] == NULL) {
        r->_errno = EBADF;
        return -1;
    }

    st->st_mode = S_IFREG; // Claim it's a regular file
    st->st_size = ff_filelength(fd_table[fd]);
    return 0;
}


// Replace this with your actual UART transmission function

_ssize_t _write_r(struct _reent *r, int fd, const void *ptr, size_t len) {
    // fd 1 is stdout, fd 2 is stderr
    if (fd == 1 || fd == 2) {
        const char *buf = (const char *)ptr;
        for (size_t i = 0; i < len; i++) {
            // Standard Doom/Unix uses \n, but many Serial Terminals need \r\n
            if (buf[i] == '\n') {
                UART_16550_put_char(UART0, '\r', portMAX_DELAY);
            }
            UART_16550_put_char(UART0, buf[i], portMAX_DELAY);
        }
        return len;
    }

    // If you implemented the filesystem _write_r from the previous step,
    // you would handle file descriptors > 2 here for file writing.
    return -1; 
}