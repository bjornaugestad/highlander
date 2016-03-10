#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <meta_common.h>

enum io_method {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

struct buffer {
    void *start;
    size_t length;
};

static enum io_method io = IO_METHOD_MMAP;
struct buffer *buffers;
static size_t n_buffers;
static int force_format;
static int frame_count = 70;


static int xioctl(int fh, unsigned long request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (r == -1 && EINTR == errno);

    return r;
}

static void process_image(const void *p, int size)
{
    (void)p; (void)size;
    fflush(stderr);
    fprintf(stderr, ".");
    fflush(stdout);
}

static status_t read_frame(int fd)
{
    struct v4l2_buffer buf;
    size_t i;

    switch (io) {
        case IO_METHOD_READ:
            if (read(fd, buffers[0].start, buffers[0].length) == -1)
                return failure;

            process_image(buffers[0].start, buffers[0].length);
            break;

        case IO_METHOD_MMAP:
            memset(&buf, 0, sizeof buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1)
                return failure;

            assert(buf.index < n_buffers);

            process_image(buffers[buf.index].start, buf.bytesused);

            if (xioctl(fd, VIDIOC_QBUF, &buf) == -1)
                return failure;

            break;

        case IO_METHOD_USERPTR:
            memset(&buf, 0, sizeof buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1)
                return failure;

            for (i = 0; i < n_buffers; ++i)
                if (buf.m.userptr == (unsigned long)buffers[i].start && buf.length == buffers[i].length)
                    break;

            assert(i < n_buffers);

            process_image((void *)buf.m.userptr, buf.bytesused);

            if (xioctl(fd, VIDIOC_QBUF, &buf) == -1)
                return failure;

            break;
    }

    return success;
}

void mainloop(int fd)
{
    unsigned int count;

    count = frame_count;

    while (count-- > 0) {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select(fd + 1, &fds, NULL, NULL, &tv);

            if (r == -1) {
                if (EINTR == errno)
                    continue;
                die("select");
            }

            if (0 == r) {
                fprintf(stderr, "select timeout\n");
                exit(EXIT_FAILURE);
            }

            if (read_frame(fd))
                break;
            /* EAGAIN - continue select loop. */
        }
    }
}

void stop_capturing(int fd)
{
    enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
                die("VIDIOC_STREAMOFF");
            break;
    }
}

void start_capturing(int fd)
{
    size_t i;
    enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                memset(&buf, 0, sizeof buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (xioctl(fd, VIDIOC_QBUF, &buf) == -1)
                    die("VIDIOC_QBUF");
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(fd, VIDIOC_STREAMON, &type) == -1)
                die("VIDIOC_STREAMON");
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                memset(&buf, 0, sizeof buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                buf.index = i;
                buf.m.userptr = (unsigned long)buffers[i].start;
                buf.length = buffers[i].length;

                if (xioctl(fd, VIDIOC_QBUF, &buf) == -1)
                    die("VIDIOC_QBUF");
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(fd, VIDIOC_STREAMON, &type) == -1)
                die("VIDIOC_STREAMON");
            break;
    }
}

void uninit_device(void)
{
    size_t i;

    switch (io) {
        case IO_METHOD_READ:
            free(buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
                if (munmap(buffers[i].start, buffers[i].length) == -1)
                    die("munmap");
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i)
                free(buffers[i].start);
            break;
    }

    free(buffers);
    buffers = NULL;
}

static status_t init_read(size_t bufsize)
{
    if ((buffers = calloc(1, sizeof *buffers)) == NULL)
        return failure;

    buffers[0].length = bufsize;
    if ((buffers[0].start = malloc(bufsize)) == NULL) {
        free(buffers);
        buffers = NULL;
        return failure;
    }

    return success;
}

static status_t init_mmap(int fd)
{
    struct v4l2_requestbuffers req;

    memset(&req, 0, sizeof req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1)
        return failure;

    if (req.count < 2)
        return fail(ENOSPC);

    if ((buffers = calloc(req.count, sizeof *buffers)) == NULL)
        return failure;

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
            die("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, buf.m.offset);

        if (buffers[n_buffers].start == MAP_FAILED)
            return failure;
    }

    return success;
}

static status_t init_userp(int fd, size_t bufsize)
{
    struct v4l2_requestbuffers req;

    memset(&req, 0, sizeof req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1)
        return failure;

    if ((buffers = calloc(4, sizeof *buffers)) == NULL)
        return failure;

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
        buffers[n_buffers].length = bufsize;
        buffers[n_buffers].start = malloc(bufsize);

        if (buffers[n_buffers].start == NULL) {
            while (n_buffers--)
                free(buffers[n_buffers].start);

            free(buffers);
            return failure;
        }
    }

    return success;
}

status_t init_device(int fd)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
        return failure;

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        return failure;

    switch (io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE))
                return failure;
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING))
                return failure;
            break;
    }

    /* Select video input, video standard and tune here. */
    memset(&cropcap, 0, sizeof cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_CROPCAP, &cropcap) == 0) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;   /* reset to default */

        if (xioctl(fd, VIDIOC_S_CROP, &crop) == -1) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    }
    else {
        /* Errors ignored. */
    }

    memset(&fmt, 0, sizeof fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (force_format) {
        fmt.fmt.pix.width = 640;
        fmt.fmt.pix.height = 480;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
            return failure;

        /* Note VIDIOC_S_FMT may change width and height. */
    }
    else {
        /* Preserve original settings as set by v4l2-ctl for example */
        if (xioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
            return failure;
    }

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;

    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    if (io == IO_METHOD_READ)
        return init_read(fmt.fmt.pix.sizeimage);

    if (io == IO_METHOD_MMAP)
        return init_mmap(fd);

    if (io == IO_METHOD_USERPTR)
        return init_userp(fd, fmt.fmt.pix.sizeimage);

    assert(0 && "Invalid value for io");
    return failure;
}

status_t close_device(int fd)
{
    if (close(fd) == -1)
        return failure;

    return success;
}

status_t open_device(const char *name, int *pfd)
{
    struct stat st;

    if (stat(name, &st) == -1)
        return failure;

    if (!S_ISCHR(st.st_mode))
        return failure;

    if ((*pfd = open(name, O_RDWR | O_NONBLOCK, 0)) == -1)
        return failure;

    return success;
}

#ifdef WEBCAMTASK_CHECK

int main(void)
{
    const char *devname = "/dev/video0";
    int fd;

    if (!open_device(devname, &fd))
        die_perror("open_device\n");

    if (!init_device(fd))
        die_perror("init device\n");

    if (!close_device(fd))
        die_perror("close_device()");
    return 0;
}

#endif
