#!/usr/bin/env perl

#
#Copyright (c) 2010-2016 NEC Corporation
#All rights reserved.
#
#This program and the accompanying materials are made available under the
#terms of the Eclipse Public License v1.0 which accompanies this
#distribution, and is available at http://www.eclipse.org/legal/epl-v10.html
#

##
## Configure PFC build environment.
##

use strict;
use vars qw( % COMMAND_SPEC % OSTYPE % OS_CONF % GCC_OSTYPE % ARCH % GCC_ARCH @PATH
$GCC_MULTIARCH $C_OPT $CXX_OPT $C_DEBUG $CXX_DEBUG $DEBUG_BUILD
$PFC_VERSION $PFC_VERSION_MAJOR $PFC_VERSION_MINOR
$PFC_VERSION_REVISION $PFC_VERSION_PATCHLEVEL $PFC_BUILD_ID
$PFC_VERSION_STRING % OSDEP @CC_WARN @CXX_WARN $CC_MODE $CXX_MODE
        % ARCH_CC_MODEFLAGS % ARCH_CXX_MODEFLAGS $SHELL_PATH $PATH_SCRIPT
        % DEFAULT_OSARCH_LIBPATH @DEFAULT_LIBPATH @LIBDIR_NAMES
        $PFC_PRODUCT_NAME % MAKEVAR_RESV @LINUX_DIST $PFC_SCM_REVISION
        @OPENSSL_TESTS @CONFIG_TESTS @CFLAGS_TESTS % REQUIRED_CFLAGS
        @UNLINK_ON_ERROR $YEAR_RANGE @JAVA_LIBRARY_PATH % MAKEVAR_OP
        @CC_INCDIRS_FIRST @LD_LIBDIRS_FIRST $CXX_STD
        % INCDIRS_FIRST_MAP % LIBDIRS_FIRST_MAP);

use Cwd qw(abs_path);
use DirHandle;
use FileHandle;
use File::Basename;
use File::Find;
use File::Path;
use File::stat;
use Getopt::Long;
use POSIX qw( : DEFAULT : sys_wait_h : errno_h : fcntl_h);

use vars qw( % Config);

#Define product name.
$PFC_PRODUCT_NAME = 'PF6800';

#Define PFC version.
$PFC_VERSION_MAJOR = 1;
$PFC_VERSION_MINOR = 0;
$PFC_VERSION_REVISION = 0;
$PFC_VERSION_PATCHLEVEL = 0;

$PFC_VERSION = join('.', $PFC_VERSION_MAJOR, $PFC_VERSION_MINOR,
        $PFC_VERSION_REVISION, $PFC_VERSION_PATCHLEVEL);

#Required Apache Ant version.
use constant ANT_VERSION_MAJOR = > 1;
use constant ANT_VERSION_MINOR = > 7;
use constant ANT_VERSION_REVISION = > 1;

#Required JDK version.
use constant JDK_VERSION_MAJOR = > 1;
use constant JDK_VERSION_MINOR = > 6;

#Required boost version.
use constant BOOST_VERSION = > 103600;

#Required OpenSSL version. (0.9.8 or later)
use constant OPENSSL_VERSION = > 0x00908000;

#Construct OpenSSL test programs.
{
    my $ver = OPENSSL_VERSION;
    my $code = << OUT;
#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

    int
    main(int argc, char **argv) {
#ifdef OPENSSL_VERSION_NUMBER
        if ((unsigned long) OPENSSL_VERSION_NUMBER < $
            {
                ver
            }
            UL) {
        printf("ERROR:Insecure OpenSSL version: %s\\n",
                OPENSSL_VERSION_TEXT);
        return 0;
    }
        if (SSLeay() != OPENSSL_VERSION_NUMBER) {
            printf("ERROR:OpenSSL header does not match library.\\n");
            return 0;
        }

        printf("OK:0x%08lx (%s)\\n", (unsigned long) OPENSSL_VERSION_NUMBER,
                OPENSSL_VERSION_TEXT);

        return 0;
#else /* !OPENSSL_VERSION_NUMBER */
        printf("ERROR:Mission OpenSSL version.\\n");
        return 0;
#endif /* OPENSSL_VERSION_NUMBER */
    }
    OUT
    push(@OPENSSL_TESTS,{CHECK = > 'for OpenSSL version',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <openssl/evp.h>

    int
    main(int argc, char **argv) {
        const EVP_MD *md = EVP_sha256();
        int size;

        if (md == NULL) {
            printf("ERROR:SHA-256 is not supported.\n");
        } else if ((size = EVP_MD_size(md)) != 32) {
            printf("ERROR:Unexpected digest size: %d\n", size);
        } else {
            printf("OK:yes\n");
        }

        return 0;
    }
    OUT
    push(@OPENSSL_TESTS,{CHECK = > 'whether OpenSSL supports SHA-256',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <openssl/evp.h>

    int
    main(int argc, char **argv) {
        const EVP_CIPHER *cipher = EVP_aes_256_ofb();
        unsigned long mode;
        int ksize, bsize;

        if (cipher == NULL) {
            printf("ERROR:AES-256 (OFB mode) is not supported.\n");
        } else if ((mode = EVP_CIPHER_mode(cipher)) != EVP_CIPH_OFB_MODE) {
            printf("ERROR:Unexpected cipher mode: %lu\n", mode);
        } else if ((ksize = EVP_CIPHER_key_length(cipher)) != 32) {
            printf("ERROR:Unexpected key length: %d\n", ksize);
        } else if ((bsize = EVP_CIPHER_block_size(cipher)) != 1) {
            printf("ERROR:Unexpected block size: %d\n", bsize);
        } else {
            printf("OK:yes\n");
        }

        return 0;
    }
    OUT
    push(@OPENSSL_TESTS,{CHECK = > 'whether OpenSSL supports AES-256 ' .
        '(OFB mode)',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <openssl/buffer.h>

    typedef struct {
        int length;
        char *data;
        int max;
    } my_bufmem_t;

#define LAYOUT_CHECK(member)      \
        do {        \
                if (offsetof(BUF_MEM, member) !=   \
                    offsetof(my_bufmem_t, member)) {   \
                        printf("ERROR:Unexpected BUF_MEM layout: %s, %u\n", \
                               #member, (uint32_t)offsetof(BUF_MEM, member)); \
                        exit(0);     \
            }       \
        } while (0)

    int main(int argc, char **argv) {
        if (sizeof (BUF_MEM) != sizeof (my_bufmem_t)) {
            printf("ERROR:Unexpected size of BUF_MEM: %u instead of %u\n",
                    (uint32_t)sizeof (BUF_MEM), (uint32_t)sizeof (my_bufmem_t));
            return 0;
        }

        LAYOUT_CHECK(length);
        LAYOUT_CHECK(data);
        LAYOUT_CHECK(max);

        printf("OK:yes\n");

        return 0;
    }
    OUT
    push(@OPENSSL_TESTS,{CHECK = > 'for BUF_MEM layout defined by OpenSSL',
        CODE = > $code});
}

#Construct tests which determines required CFLAGS.
{
    my $code = << 'OUT';
#include <stdio.h>
#include <fcntl.h>

    int
    main(int argc, char **argv) {
        return O_CLOEXEC;
    }
    OUT
    push(@CFLAGS_TESTS,{NAME = > 'O_CLOEXEC', IGNORE_ERROR = > 1,
        CANDIDATES = > [
        {NAME = > '_GNU_SOURCE'}
        ],
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <fcntl.h>

    int
    main(int argc, char **argv) {
        return F_DUPFD_CLOEXEC;
    }
    OUT
    push(@CFLAGS_TESTS,{NAME = > 'F_DUPFD_CLOEXEC', IGNORE_ERROR = > 1,
        CANDIDATES = > [
        {NAME = > '_GNU_SOURCE'}
        ],
        CODE = > $code});
}

#Construct configuration tests.
{
    my $code = << 'OUT';
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef MSG_NOSIGNAL
    static volatile sig_atomic_t signaled;

    static void
    handler(int sig) {
        signaled = 1;
    }
#endif /* MSG_NOSIGNAL */

    int
    main(int argc, char **argv) {
#ifdef MSG_NOSIGNAL
        struct sigaction sact;
        char c;
        int fds[2], err;

        sact.sa_handler = handler;
        sact.sa_flags = 0;
        sigemptyset(&sact.sa_mask);
        if (sigaction(SIGPIPE, &sact, NULL) != 0) {
            printf("ERROR:sigaction() failed.\n");
            return 0;
        }
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
            printf("ERROR:socketpair() failed.\n");
            return 0;
        }
        close(fds[1]);
        c = 0;
        if (send(fds[0], &c, sizeof (c), MSG_NOSIGNAL) != (ssize_t) - 1) {
            printf("ERROR:sendto() succeeded unexpectedly.\n");
            return 0;
        }
        err = errno;
        if (err != EPIPE) {
            printf("ERROR:Unexpected errno of sendto(): %d\n", err);
            return 0;
        }
        if (signaled != 0) {
            printf("NO:MSG_NOSIGNAL does not work.\n");
            return 0;
        }

        printf("OK:yes\n");
#else /* !MSG_NOSIGNAL */
        printf("NO:MSG_NOSIGNAL is not defined.\n");
#endif /* MSG_NOSIGNAL */

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether MSG_NOSIGNAL is supported',
        DESC = > 'Define 1 if MSG_NOSIGNAL is supported.',
        DEFINE = > 'PFC_HAVE_MSG_NOSIGNAL',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

    int
    main(int argc, char **argv) {
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
        int sock, flag;

        sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (sock == -1) {
            printf("NO:SOCK_CLOEXEC or SOCK_NONBLOCK is not supported.\n");
            return 0;
        }

        flag = fcntl(sock, F_GETFL);
        if (flag == -1) {
            printf("ERROR:fcntl(F_GETFL) failed.\n");
            return 0;
        }
        if ((flag & O_NONBLOCK) == 0) {
            printf("NO:SOCK_NONBLOCK does not work.\n");
            return 0;
        }

        flag = fcntl(sock, F_GETFD);
        if (flag == -1) {
            printf("ERROR:fcntl(F_GETFD) failed.\n");
            return 0;
        }
        if ((flag & FD_CLOEXEC) == 0) {
            printf("NO:SOCK_CLOEXEC does not work.\n");
            return 0;
        }

        printf("OK:yes\n");
#else /* !SOCK_CLOEXEC || !SOCK_NONBLOCK */
        printf("NO:SOCK_CLOEXEC or SOCK_NONBLOCK is not defined.\n");
#endif /* SOCK_CLOEXEC && SOCK_NONBLOCK */

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether both SOCK_CLOEXEC and ' .
        'SOCK_NONBLOCK are supported',
        DESC = > 'Define 1 if both SOCK_CLOEXEC and ' .
        'SOCK_NONBLOCK are supported.',
        DEFINE = > 'PFC_HAVE_SOCK_CLOEXEC_NONBLOCK',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SERVER_PORT "conftest.sock"

    static void *
    worker(void *arg) {
        int sock, loop;
        struct sockaddr_un saddr;

        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock == -1) {
            printf("ERROR:socket() failed.\n");
            goto error;
        }

        saddr.sun_family = AF_UNIX;
        snprintf(saddr.sun_path, sizeof (saddr.sun_path), "%s",
                SERVER_PORT);
        for (;;) {
            struct timespec ts;
            if (connect(sock, (const struct sockaddr *) &saddr,
                    sizeof (saddr)) == 0) {
                break;
            }
            loop++;
            if (loop >= 10) {
                printf("ERROR:connect() failed.\n");
                goto error;
            }

            ts.tv_sec = 0;
            ts.tv_nsec = 1000000;
            nanosleep(&ts, NULL);
        }

        return NULL;

error:
        (void) unlink(SERVER_PORT);
        exit(0);
    }

    int
    main(int argc, char **argv) {
        int sock, client, flag, err;
        pthread_t pth;
        struct sockaddr_un saddr;

        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock == -1) {
            printf("ERROR:socket() failed.\n");
            return 0;
        }

        saddr.sun_family = AF_UNIX;
        snprintf(saddr.sun_path, sizeof (saddr.sun_path), "%s",
                SERVER_PORT);
        if (bind(sock, (const struct sockaddr *) &saddr, sizeof (saddr)) == -1) {
            printf("ERROR:bind() failed.\n");
            return 0;
        }

        if (listen(sock, 1) == -1) {
            printf("ERROR:listen() failed.\n");
            goto out;
        }

        err = pthread_create(&pth, NULL, worker, NULL);
        if (err != 0) {
            printf("ERROR:pthread_create() failed: %s\n", strerror(err));
            goto out;
        }

        client = accept4(sock, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (client == -1) {
            printf("NO:accept4() failed.\n");
            goto out;
        }

        flag = fcntl(client, F_GETFL);
        if (flag == -1) {
            printf("ERROR:fcntl(F_GETFL) failed.\n");
            goto out;
        }
        if ((flag & O_NONBLOCK) == 0) {
            printf("NO:SOCK_NONBLOCK does not work.\n");
            goto out;
        }

        flag = fcntl(client, F_GETFD);
        if (flag == -1) {
            printf("ERROR:fcntl(F_GETFD) failed.\n");
            goto out;
        }
        if ((flag & FD_CLOEXEC) == 0) {
            printf("NO:SOCK_CLOEXEC does not work.\n");
            goto out;
        }

        err = pthread_join(pth, NULL);
        if (err != 0) {
            printf("ERROR:pthread_join() failed: %s\n", strerror(err));
            goto out;
        }

        printf("OK:yes\n");

out:
        (void) unlink(SERVER_PORT);

        exit(0);
    }

    OUT

    push(@CONFIG_TESTS,{CHECK = > 'whether accept4(2) works',
        DESC = > 'Define 1 if accept4(2) is supported.',
        DEFINE = > 'PFC_HAVE_ACCEPT4',
        REQUIRED = > ['_GNU_SOURCE'],
        CFLAGS = > ['-pthread'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

    int
    main(int argc, char **argv) {
#ifdef MSG_CMSG_CLOEXEC
        struct msghdr msg;
        struct cmsghdr *cmsg;
        int stream[2], fd, fd1, *fdp, flag;
        long buf[CMSG_SPACE(sizeof (fd)) / sizeof (long)];
        char dummy = 0;
        ssize_t nbytes;
        struct iovec iov;
        struct stat sbuf, sbuf1;

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, stream) != 0) {
            printf("ERROR:socketpair() failed.\n");
            return 0;
        }

        fd = open("/dev/null", O_RDONLY);
        if (fd == -1) {
            printf("ERROR:open(/dev/null) failed.");
            return 0;
        }

        iov.iov_base = &dummy;
        iov.iov_len = sizeof (dummy);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_flags = 0;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_control = buf;
        msg.msg_controllen = sizeof (buf);
        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof (fd));
        fdp = (int *) CMSG_DATA(cmsg);
        *fdp = fd;
        msg.msg_controllen = cmsg->cmsg_len;

        nbytes = sendmsg(stream[0], &msg, 0);
        if (nbytes == (ssize_t) - 1) {
            printf("ERROR:sendmsg() failed.\n");
            return 0;
        }

        msg.msg_control = buf;
        msg.msg_controllen = sizeof (buf);
        msg.msg_flags = 0;

        nbytes = recvmsg(stream[1], &msg, MSG_CMSG_CLOEXEC);
        if (nbytes == (ssize_t) - 1) {
            printf("ERROR:recvmsg() failed.\n");
            return 0;
        }

        cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg == NULL) {
            printf("ERROR:No control message.\n");
            return 0;
        }
        if (cmsg->cmsg_level != SOL_SOCKET) {
            printf("ERROR:Unexpected control level: %d\n",
                    cmsg->cmsg_level);
            return 0;
        }
        if (cmsg->cmsg_type != SCM_RIGHTS) {
            printf("ERROR:Unexpected control type: %d\n",
                    cmsg->cmsg_type);
            return 0;
        }

        fdp = (int *) CMSG_DATA(cmsg);
        fd1 = *fdp;
        if (fd1 == fd) {
            printf("ERROR:Unexpected file descriptor: %d, %d\n",
                    fd1, fd);
            return 0;
        }

        if (fstat(fd, &sbuf) != 0) {
            printf("ERROR:fstat() failed.\n");
            return 0;
        }
        if (fstat(fd1, &sbuf1) != 0) {
            printf("ERROR:fstat() failed.\n");
            return 0;
        }
        if ((sbuf.st_dev != sbuf1.st_dev) || (sbuf.st_ino != sbuf1.st_ino)) {
            printf("ERROR:Unexpected file.\n");
            return 0;
        }

        flag = fcntl(fd1, F_GETFD);
        if (flag == -1) {
            printf("ERROR:fcntl(F_GETFD) failed.\n");
            return 0;
        }
        if ((flag & FD_CLOEXEC) == 0) {
            printf("NO:MSG_CMSG_CLOEXEC does not work.\n");
            return 0;
        }

        printf("OK:yes\n");
#else /* !MSG_CMSG_CLOEXEC */
        printf("NO:MSG_CMSG_CLOEXEC is not defined.\n");
#endif /* MSG_CMSG_CLOEXEC */

        exit(0);
    }

    OUT

    push(@CONFIG_TESTS,{CHECK = > 'whether MSG_CMSG_CLOEXEC works',
        DESC = > 'Define 1 if MSG_CMSG_CLOEXEC is ' .
        'supported.',
        DEFINE = > 'PFC_HAVE_CMSG_CLOEXEC',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

    int
    main(int argc, char **argv) {
#ifdef O_CLOEXEC
        int fd, flag;

        fd = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (fd == -1) {
            printf("ERROR:open(/dev/null) failed: %s\n", strerror(errno));
            return 0;
        }
        flag = fcntl(fd, F_GETFD);
        if (flag == -1) {
            printf("ERROR:fcntl(F_GETFD) failed: %s\n", strerror(errno));
            return 0;
        }
        if ((flag & FD_CLOEXEC) == 0) {
            printf("NO:O_CLOEXEC does not work.\n");
            return 0;
        }

        printf("OK:yes\n");
#else /* !O_CLOEXEC */
        printf("NO:O_CLOEXEC is not defined.\n");
#endif /* O_CLOEXEC */

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether O_CLOEXEC is supported',
        DESC = > 'Define 1 if O_CLOEXEC is supported.',
        DEFINE = > 'PFC_HAVE_O_CLOEXEC',
        REQUIRED = > sub
        {
            my $opt = $REQUIRED_CFLAGS
            {'O_CLOEXEC'};
            return undef unless(ref($opt) eq 'HASH');

            my $name = $opt-> {
                NAME
            };
            return undef unless($name);

            return [$opt-> {
                NAME
            }
            ];
        },
        SKIP = > sub
        {
            my $opt = $REQUIRED_CFLAGS
            {'O_CLOEXEC'};
            return 1 unless(defined($opt));
            return undef;
        },
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

    int
    main(int argc, char **argv) {
#ifdef F_DUPFD_CLOEXEC
        struct stat sbuf, newsbuf;
        int fd, newfd, flag;

        fd = open("/dev/null", O_RDWR);
        if (fd == -1) {
            printf("ERROR:open(/dev/null) failed: %s\n", strerror(errno));
            return 0;
        }
        if (fstat(fd, &sbuf) != 0) {
            printf("ERROR:fstat() failed: %s\n", strerror(errno));
            return 0;
        }
        newfd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
        if (newfd == -1) {
            printf("ERROR:fcntl(F_DUPFD_CLOEXEC) failed: %s\n",
                    strerror(errno));
            return 0;
        }
        if (fd == newfd) {
            printf("ERROR:fcntl(F_DUPFD_CLOEXEC) returned unexpected "
                    "value.\n");
            return 0;
        }
        if (fstat(newfd, &newsbuf) != 0) {
            printf("ERROR:fstat() failed: %s\n", strerror(errno));
            return 0;
        }
        if (sbuf.st_dev != newsbuf.st_dev || sbuf.st_ino != newsbuf.st_ino) {
            printf("ERROR:fcntl(F_DUPFD_CLOEXEC) did not duplicate FD.\n");
            return 0;
        }
        flag = fcntl(newfd, F_GETFD);
        if (flag == -1) {
            printf("ERROR:fcntl(F_GETFD) failed: %s\n", strerror(errno));
            return 0;
        }
        if ((flag & FD_CLOEXEC) == 0) {
            printf("NO:F_DUPFD_CLOEXEC does not work.\n");
            return 0;
        }

        printf("OK:yes\n");
#else /* !F_DUPFD_CLOEXEC */
        printf("NO:F_DUPFD_CLOEXEC is not defined.\n");
#endif /* F_DUPFD_CLOEXEC */

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether F_DUPFD_CLOEXEC is supported',
        DESC = > 'Define 1 if F_DUPFD_CLOEXEC is ' .
        'supported.',
        DEFINE = > 'PFC_HAVE_F_DUPFD_CLOEXEC',
        REQUIRED = > sub
        {
            my $opt = $REQUIRED_CFLAGS
            {'F_DUPFD_CLOEXEC'};
            return undef unless(ref($opt) eq 'HASH');

            my $name = $opt-> {
                NAME
            };
            return undef unless($name);

            return [$opt-> {
                NAME
            }
            ];
        },
        SKIP = > sub
        {
            my $opt = $REQUIRED_CFLAGS
            {'F_DUPFD_CLOEXEC'};
            return 1 unless(defined($opt));
            return undef;
        },
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

    int
    main(int argc, char **argv) {
        struct stat sbuf, newsbuf;
        int fd, newfd, flag;

        fd = open("/dev/null", O_RDWR);
        if (fd == -1) {
            printf("ERROR:open(/dev/null) failed: %s\n", strerror(errno));
            return 0;
        }
        if (fstat(fd, &sbuf) != 0) {
            printf("ERROR:fstat() failed: %s\n", strerror(errno));
            return 0;
        }
        newfd = dup3(fd, 50, O_CLOEXEC);
        if (newfd == -1) {
            printf("ERROR:dup3() failed: %s\n", strerror(errno));
            return 0;
        }
        if (newfd != 50) {
            printf("ERROR:dup3() ignored 2nd. argument.\n");
            return 0;
        }
        if (fstat(newfd, &newsbuf) != 0) {
            printf("ERROR:fstat() failed: %s\n", strerror(errno));
            return 0;
        }
        if (sbuf.st_dev != newsbuf.st_dev || sbuf.st_ino != newsbuf.st_ino) {
            printf("ERROR:dup3() did not duplicate FD.\n");
            return 0;
        }
        flag = fcntl(newfd, F_GETFD);
        if (flag == -1) {
            printf("ERROR:fcntl(F_GETFD) failed: %s\n", strerror(errno));
            return 0;
        }
        if ((flag & FD_CLOEXEC) == 0) {
            printf("NO:dup3() does not work.\n");
            return 0;
        }

        printf("OK:yes\n");

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether dup3(2) works',
        DESC = > 'Define 1 if dup3(2) is supported.',
        DEFINE = > 'PFC_HAVE_DUP3',
        REQUIRED = > ['_GNU_SOURCE'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

    int
    main(int argc, char **argv) {
        int i, fds[2];

        if (pipe2(fds, O_NONBLOCK | O_CLOEXEC) == -1) {
            printf("NO:pipe2() failed.\n");
            return 0;
        }

        for (i = 0; i < 2; i++) {
            int flag = fcntl(fds[i], F_GETFD);

            if (flag == -1) {
                printf("ERROR:fcntl(F_GETFD) failed: %s\n",
                        strerror(errno));
                return 0;
            }
            if ((flag & FD_CLOEXEC) == 0) {
                printf("NO:pipe2(O_CLOEXEC) does not work.\n");
                return 0;
            }

            flag = fcntl(fds[i], F_GETFL);
            if (flag == -1) {
                printf("ERROR:fcntl(F_GETFL) failed: %s\n",
                        strerror(errno));
                return 0;
            }
            if ((flag & O_NONBLOCK) == 0) {
                printf("NO:pipe2(O_NONBLOCK) does not work.\n");
                return 0;
            }
        }

        printf("OK:yes\n");

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether pipe2(2) works',
        DESC = > 'Define 1 if pipe2(2) is supported.',
        DEFINE = > 'PFC_HAVE_PIPE2',
        REQUIRED = > ['_GNU_SOURCE'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define TEST_PATH_MAX 256
#define TEST_FILE_NAME "test"
#define TEST_DIR_NAME "testdir"

    int
    main(int argc, char **argv) {
        char dirpath[TEST_PATH_MAX];
        char path1[TEST_PATH_MAX], path2[TEST_PATH_MAX];
        int dirfd, fd;
        pid_t pid = getpid();
        struct stat sbuf;

        snprintf(dirpath, sizeof (dirpath), "confdir.%u", pid);
        snprintf(path1, sizeof (path1), "%s/" TEST_FILE_NAME, dirpath);
        snprintf(path2, sizeof (path2), "%s/" TEST_DIR_NAME, dirpath);

        if (mkdir(dirpath, 0755) != 0) {
            printf("ERROR:mkdir() failed: %s\n", strerror(errno));
            return 0;
        }

        dirfd = openat(AT_FDCWD, dirpath, O_RDONLY);
        if (dirfd == -1) {
            printf("NO:openat(FT_FDCWD) failed: %s\n", strerror(errno));
            goto out;
        }
        fd = openat(dirfd, TEST_FILE_NAME,
                O_RDWR | O_CREAT | O_EXCL | O_TRUNC, 0644);
        if (fd == -1) {
            printf("NO:openat() failed: %s\n", strerror(errno));
            goto out;
        }
        (void) close(fd);
        if (mkdirat(dirfd, TEST_DIR_NAME, 0755) != 0) {
            printf("NO:mkdirat() failed: %s\n", strerror(errno));
            goto out;
        }

        if (stat(path1, &sbuf) != 0) {
            printf("NO:stat(%s) failed: %s\n", path1, strerror(errno));
            goto out;
        }
        if (!S_ISREG(sbuf.st_mode)) {
            printf("NO:Unexpected file type: %s: 0x%x\n",
                    path1, sbuf.st_mode);
            goto out;
        }

        if (stat(path2, &sbuf) != 0) {
            printf("NO:stat(%s) failed: %s\n", path2, strerror(errno));
            goto out;
        }
        if (!S_ISDIR(sbuf.st_mode)) {
            printf("NO:Unexpected file type: %s: 0x%x\n",
                    path2, sbuf.st_mode);
            goto out;
        }

        if (unlinkat(dirfd, TEST_FILE_NAME, 0) != 0) {
            printf("NO:unlinkat(%s) failed: %s\n", TEST_FILE_NAME,
                    strerror(errno));
            goto out;
        }
        if (unlinkat(dirfd, TEST_DIR_NAME, AT_REMOVEDIR) != 0) {
            printf("NO:unlinkat(%s) failed: %s\n", TEST_DIR_NAME,
                    strerror(errno));
            goto out;
        }
        (void) close(dirfd);

        if (rmdir(dirpath) != 0) {
            printf("NO:rmdir(%s) failed: %s\n", dirpath, strerror(errno));
            goto out;
        }

        printf("OK:yes\n");

        return 0;

out:
        (void) unlink(path1);
        (void) rmdir(path2);
        (void) rmdir(dirpath);

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether ATFILE syscalls work',
        DESC = > 'Define 1 if ATFILE syscalls, ' .
        'such as openat(), are supported.',
        DEFINE = > 'PFC_HAVE_ATFILE_SYSCALL',
        REQUIRED = > ['_ATFILE_SOURCE'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define TEST_PATH_MAX 256
#define TEST_FILE_NAME "test"
#define TEST_DIR_NAME "testdir"

    int
    main(int argc, char **argv) {
        FILE *fp;
        int flag;

        fp = fopen("/dev/null", "re");
        if (fp == NULL) {
            printf("NO:fopen(/dev/null, \"re\") failed: %s\n",
                    strerror(errno));
            return 0;
        }

        flag = fcntl(fileno(fp), F_GETFD);
        if (flag == -1) {
            printf("NO:fcntl(F_GETFD) failed: %s\n", strerror(errno));
            return 0;
        }
        if ((flag & FD_CLOEXEC) == 0) {
            printf("NO:fopen() ignores \"e\" in mode string.\n");
            return 0;
        }

        printf("OK:yes\n");

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether fopen() recognizes "e" in ' .
        'mode string',
        DESC = > 'Define 1 if fopen() recognizes "e" in ' .
        'mode string.',
        DEFINE = > 'PFC_FOPEN_SUPPORTS_E',
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

    typedef struct {
        uint8_t u8;
        uint64_t u64;
    } test_data_t;

    int
    main(int argc, char **argv) {
        printf("VALUE:%u\n", (uint32_t) offsetof(test_data_t, u64));

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'for alignment required by 64-bit ' .
        'integer',
        DESC = > 'Required alignment for 64-bit integer.',
        DEFINE = > 'PFC_ALIGNOF_INT64',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/wait.h>

#ifdef __GNUC__
#define __NOINLINE __attribute__((noinline))
#else /* !__GNUC__ */
#define __NOINLINE
#endif /* __GNUC__ */

    uint16_t __NOINLINE
    uint16_read(uint16_t * ptr) {
        return *ptr;
    }

    uint32_t __NOINLINE
    uint32_read(uint32_t * ptr) {
        return *ptr;
    }

    uint64_t __NOINLINE
    uint64_read(uint64_t * ptr) {
        return *ptr;
    }

    void __NOINLINE
    uint16_write(uint16_t *ptr, uint16_t value) {
        *ptr = value;
    }

    void __NOINLINE
    uint32_write(uint32_t *ptr, uint32_t value) {
        *ptr = value;
    }

    void __NOINLINE
    uint64_write(uint64_t *ptr, uint64_t value) {
        *ptr = value;
    }

    int
    main(int argc, char **argv) {
        struct rlimit rlim;
        pid_t pid, cpid;
        int status;

        rlim.rlim_cur = 0;
        rlim.rlim_max = 0;
        if (setrlimit(RLIMIT_CORE, &rlim) != 0) {
            printf("ERROR:setrlimit(RLIMIT_CORE) failed: %s\n",
                    strerror(errno));
            return 0;
        }

        pid = fork();
        if (pid == (pid_t) - 1) {
            printf("ERROR:setrlimit(RLIMIT_CORE) failed: %s\n",
                    strerror(errno));
            return 0;
        }

        if (pid == 0) {

            union {
                char c[2];
                uint64_t u[2];
            } buffer;
            char *unaligned = &buffer.c[1];
            uint64_t *u64 = (uint64_t *) unaligned;
            uint32_t *u32 = (uint32_t *) unaligned;
            uint16_t *u16 = (uint16_t *) unaligned;

            uint16_write(u16, 0x1234);
            if (uint16_read(u16) != 0x1234) {
                _exit(1);
            }

            uint32_write(u32, 0x11223344);
            if (uint32_read(u32) != 0x11223344) {
                _exit(1);
            }

            uint64_write(u64, 0xaabbccddeeff0011ULL);
            if (uint64_read(u64) != 0xaabbccddeeff0011ULL) {
                _exit(1);
            }

            _exit(0);
        }

        cpid = waitpid(pid, &status, 0);
        if (cpid == (pid_t) - 1) {
            printf("ERROR:waitpid() failed: %s\n", strerror(errno));
            return 0;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            printf("NO\n");
            return 0;
        }

        printf("OK:yes\n");

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether unaligned access is allowed',
        DESC = > 'Define 1 if unaligned access is allowed.',
        DEFINE = > 'PFC_UNALIGNED_ACCESS',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef POLLRDHUP
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    static volatile int do_write = 0;
    static volatile int data_ready = 0;
    static volatile int do_close = 0;

    static void *
    worker(void *arg) {
        int fd = (int) (uintptr_t) arg;
        ssize_t nbytes;
        char c = 0;

        pthread_mutex_lock(&mutex);
        while (do_write == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        nbytes = write(fd, &c, 1);
        if (nbytes == (ssize_t) - 1) {
            printf("ERROR:write(2) failed: %s\n", strerror(errno));
            exit(0);
        }
        if (nbytes != 1) {
            printf("ERROR:write(2) returned unexpected error: %s\n",
                    strerror(errno));
            exit(0);
        }

        pthread_mutex_lock(&mutex);
        data_ready = 1;
        pthread_cond_signal(&cond);

        while (do_close == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        (void) close(fd);

        return NULL;
    }

    int
    main(int argc, char **argv) {
        int efd, sfd[2], nfds, err;
        pthread_t pth;
        void *cookie = (void *) (uintptr_t) time(NULL);
        struct pollfd pfd;

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sfd) != 0) {
            printf("ERROR:socketpair(2) failed: %s\n", strerror(errno));
            return 0;
        }

        err = pthread_create(&pth, NULL, worker, (void *) (uintptr_t) sfd[1]);
        if (err != 0) {
            printf("ERROR:pthread_create() failed: %s\n", strerror(err));
            return 0;
        }

        pfd.fd = sfd[0];
        pfd.events = POLLRDHUP;
        pfd.revents = 0;
        nfds = poll(&pfd, 1, 0);
        if (nfds == -1) {
            printf("NO:poll(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:poll(POLLRDHUP) returned unexpected value: %d\n",
                    nfds);
            return 0;
        }

        pthread_mutex_lock(&mutex);
        do_write = 1;
        pthread_cond_signal(&cond);

        while (data_ready == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        pfd.fd = sfd[0];
        pfd.events = POLLRDHUP;
        pfd.revents = 0;
        nfds = poll(&pfd, 1, 0);
        if (nfds == -1) {
            printf("NO:poll(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:poll(POLLRDHUP) returned unexpected value: %d\n",
                    nfds);
            return 0;
        }

        pthread_mutex_lock(&mutex);
        do_close = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        nfds = poll(&pfd, 1, -1);
        if (nfds == -1) {
            printf("NO:poll(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 1) {
            printf("NO:poll(POLLRDHUP) returned unexpected value: %d\n",
                    nfds);
            return 0;
        }

        if (pfd.revents != (POLLRDHUP | POLLHUP)) {
            printf("NO:Unexpected event bits: 0x%x\n", pfd.revents);
            return 0;
        }

        err = pthread_join(pth, NULL);
        if (err != 0) {
            printf("ERROR:pthread_join() failed: %s\n", strerror(err));
            return 0;
        }

        printf("OK:yes\n");

        return 0;
    }
#else /* !POLLRDHUP */

int
    main(int argc, char **argv) {
        printf("NO:POLLRDHUP is not defined.\n");
        return 0;
    }
#endif /* POLLRDHUP */
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether poll(2) recognizes POLLRDHUP',
        DESC = > 'Define 1 if POLLRDHUP is supported.',
        DEFINE = > 'PFC_HAVE_POLLRDHUP',
        CFLAGS = > ['-pthread'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    static volatile int do_write = 0;
    static volatile int do_close = 0;

    static void *
    worker(void *arg) {
        int fd = (int) (uintptr_t) arg;
        ssize_t nbytes;
        char c = 0;

        pthread_mutex_lock(&mutex);
        while (do_write == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        nbytes = write(fd, &c, 1);
        if (nbytes == (ssize_t) - 1) {
            printf("ERROR:write(2) failed: %s\n", strerror(errno));
            exit(0);
        }
        if (nbytes != 1) {
            printf("ERROR:write(2) returned unexpected value: %d\n",
                    nbytes);
            exit(0);
        }

        pthread_mutex_lock(&mutex);
        while (do_close == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        (void) close(fd);

        return NULL;
    }

    int
    main(int argc, char **argv) {
        int efd, pfd[2], nfds, err, i, timeout;
        char c;
        ssize_t nbytes;
        pthread_t pth;
        void *cookie = (void *) (uintptr_t) time(NULL);
        struct epoll_event event, ev[4];

        efd = epoll_create(1);
        if (efd == -1) {
            printf("NO:epoll_create(1) failed: %s\n", strerror(errno));
            return 0;
        }

        if (pipe(pfd) != 0) {
            printf("ERROR:pipe(2) failed: %s\n", strerror(errno));
            return 0;
        }

        event.events = EPOLLIN;
        event.data.ptr = cookie;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, pfd[0], &event) != 0) {
            printf("NO:epoll_ctl(2) failed: %s\n", strerror(errno));
            return 0;
        }

        err = pthread_create(&pth, NULL, worker, (void *) (uintptr_t) pfd[1]);
        if (err != 0) {
            printf("ERROR:pthread_create() failed: %s\n", strerror(err));
            return 0;
        }

        nfds = epoll_wait(efd, ev, 4, 0);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:epoll_wait(EPOLLIN) returned unexpected value: "
                    "%d\n", nfds);
            return 0;
        }

        pthread_mutex_lock(&mutex);
        do_write = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        timeout = -1;
        for (i = 0; i < 10; i++) {
            ev[0].events = 0;
            ev[0].data.ptr = NULL;
            nfds = epoll_wait(efd, ev, 4, timeout);
            if (nfds == -1) {
                printf("NO:epoll_wait(2) failed: %s\n",
                        strerror(errno));
                return 0;
            } else if (nfds != 1) {
                printf("NO:epoll_wait(EPOLLIN) returned unexpected "
                        "value: %d\n", nfds);
                return 0;
            }

            if (ev[0].events != EPOLLIN) {
                printf("NO:Unexpected event bits: 0x%x\n",
                        ev[0].events);
                return 0;
            }
            if (ev[0].data.ptr != cookie) {
                printf("NO:Unexpected user data: %p\n",
                        ev[0].data.ptr);
                return 0;
            }
            timeout = 0;
        }

        nfds = epoll_wait(efd, ev, 4, 0);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 1) {
            printf("NO:epoll_wait(EPOLLIN) returned unexpected value: "
                    "%d\n", nfds);
            return 0;
        }

        nbytes = read(pfd[0], &c, 1);
        if (nbytes == (ssize_t) - 1) {
            printf("ERROR:read(2) failed: %s\n", strerror(errno));
            return 0;
        }
        if (nbytes != 1) {
            printf("ERROR:read(2) returned unexpected value: %d\n",
                    nbytes);
            return 0;
        }

        nfds = epoll_wait(efd, ev, 4, 0);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:epoll_wait(EPOLLIN) returned unexpected value: "
                    "%d\n", nfds);
            return 0;
        }

        pthread_mutex_lock(&mutex);
        do_close = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        timeout = -1;
        for (i = 0; i < 10; i++) {
            ev[0].events = 0;
            ev[0].data.ptr = NULL;
            nfds = epoll_wait(efd, ev, 4, timeout);
            if (nfds == -1) {
                printf("NO:epoll_wait(2) failed: %s\n",
                        strerror(errno));
                return 0;
            } else if (nfds != 1) {
                printf("NO:epoll_wait(EPOLLIN) returned unexpected "
                        "value: %d\n", nfds);
                return 0;
            }

            if ((ev[0].events & EPOLLHUP) == 0) {
                printf("NO:Unexpected event bits: 0x%x\n",
                        ev[0].events);
                return 0;
            }
            if (ev[0].data.ptr != cookie) {
                printf("NO:Unexpected user data: %p\n",
                        ev[0].data.ptr);
                return 0;
            }
            timeout = 0;
        }

        err = pthread_join(pth, NULL);
        if (err != 0) {
            printf("ERROR:pthread_join() failed: %s\n", strerror(err));
            return 0;
        }

        printf("OK:yes\n");

        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether epoll(7) works',
        DESC = > 'Define 1 if epoll(7) is supported.',
        DEFINE = > 'PFC_HAVE_EPOLL',
        CFLAGS = > ['-pthread'],
        MANDATORY = > 'epoll(7) is mandatory.',
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#ifdef EPOLLRDHUP
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    static volatile int do_write = 0;
    static volatile int data_ready = 0;
    static volatile int do_close = 0;

    static void *
    worker(void *arg) {
        int fd = (int) (uintptr_t) arg;
        ssize_t nbytes;
        char c = 0;

        pthread_mutex_lock(&mutex);
        while (do_write == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        nbytes = write(fd, &c, 1);
        if (nbytes == (ssize_t) - 1) {
            printf("ERROR:write(2) failed: %s\n", strerror(errno));
            exit(0);
        }
        if (nbytes != 1) {
            printf("ERROR:write(2) returned unexpected error: %s\n",
                    strerror(errno));
            exit(0);
        }

        pthread_mutex_lock(&mutex);
        data_ready = 1;
        pthread_cond_signal(&cond);

        while (do_close == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        (void) close(fd);

        return NULL;
    }

    int
    main(int argc, char **argv) {
        int efd, sfd[2], nfds, err;
        pthread_t pth;
        void *cookie = (void *) (uintptr_t) time(NULL);
        struct epoll_event event, ev[4];

        efd = epoll_create(1);
        if (efd == -1) {
            printf("NO:epoll_create(1) failed: %s\n", strerror(errno));
            return 0;
        }

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sfd) != 0) {
            printf("ERROR:socketpair(2) failed: %s\n", strerror(errno));
            return 0;
        }

        event.events = EPOLLRDHUP;
        event.data.ptr = cookie;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd[0], &event) != 0) {
            printf("NO:epoll_ctl(2) failed: %s\n", strerror(errno));
            return 0;
        }

        err = pthread_create(&pth, NULL, worker, (void *) (uintptr_t) sfd[1]);
        if (err != 0) {
            printf("ERROR:pthread_create() failed: %s\n", strerror(err));
            return 0;
        }

        nfds = epoll_wait(efd, ev, 4, 0);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:epoll_wait(EPOLLRDHUP) returned unexpected value: "
                    "%d\n", nfds);
            return 0;
        }

        pthread_mutex_lock(&mutex);
        do_write = 1;
        pthread_cond_signal(&cond);

        while (data_ready == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        nfds = epoll_wait(efd, ev, 4, 0);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:epoll_wait(EPOLLRDHUP) returned unexpected value: "
                    "%d\n", nfds);
            return 0;
        }

        pthread_mutex_lock(&mutex);
        do_close = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        nfds = epoll_wait(efd, ev, 4, -1);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 1) {
            printf("NO:epoll_wait(EPOLLRDHUP) returned unexpected value: "
                    "%d\n", nfds);
            return 0;
        }

        if (ev[0].events != (EPOLLRDHUP | EPOLLHUP)) {
            printf("NO:Unexpected event bits: 0x%x\n", ev[0].events);
            return 0;
        }
        if (ev[0].data.ptr != cookie) {
            printf("NO:Unexpected user data: %p\n", ev[0].data.ptr);
            return 0;
        }

        err = pthread_join(pth, NULL);
        if (err != 0) {
            printf("ERROR:pthread_join() failed: %s\n", strerror(err));
            return 0;
        }

        printf("OK:yes\n");

        return 0;
    }
#else /* !EPOLLRDHUP */

int
    main(int argc, char **argv) {
        printf("NO:EPOLLRDHUP is not defined.\n");
        return 0;
    }
#endif /* EPOLLRDHUP */
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether epoll(7) recognizes EPOLLRDHUP',
        DESC = > 'Define 1 if EPOLLRDHUP is supported.',
        DEFINE = > 'PFC_HAVE_EPOLLRDHUP',
        CFLAGS = > ['-pthread'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>

#ifdef EPOLL_CLOEXEC

int
    main(int argc, char **argv) {
        int efd, flag;

        efd = epoll_create1(EPOLL_CLOEXEC);
        if (efd == -1) {
            printf("NO:epoll_create1(EPOLL_CLOEXEC) failed: %s\n",
                    strerror(errno));
            return 0;
        }

        flag = fcntl(efd, F_GETFD);
        if (flag == -1) {
            printf("NO:fcntl(F_GETFD) failed: %s\n", strerror(errno));
            return 0;
        }
        if ((flag & FD_CLOEXEC) == 0) {
            printf("NO:epoll_create1() ignores EPOLL_CLOEXEC.\n");
            return 0;
        }

        printf("OK:yes\n");

        return 0;
    }
#else /* !EPOLL_CLOEXEC */

int
    main(int argc, char **argv) {
        printf("NO:EPOLL_CLOEXEC is not defined.\n");
        return 0;
    }
#endif /* EPOLL_CLOEXEC */
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether EPOLL_CLOEXEC works',
        DESC = > 'Define 1 if EPOLL_CLOEXEC is supported.',
        DEFINE = > 'PFC_HAVE_EPOLL_CLOEXEC',
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#ifdef EPOLLONESHOT
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    static volatile int do_write = 0;
    static volatile int do_close = 0;

    static void *
    worker(void *arg) {
        int fd = (int) (uintptr_t) arg;
        ssize_t nbytes;
        char c = 0;

        pthread_mutex_lock(&mutex);
        while (do_write == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        nbytes = write(fd, &c, 1);
        if (nbytes == (ssize_t) - 1) {
            printf("ERROR:write(2) failed: %s\n", strerror(errno));
            exit(0);
        }
        if (nbytes != 1) {
            printf("ERROR:write(2) returned unexpected value: %d\n",
                    nbytes);
            exit(0);
        }

        pthread_mutex_lock(&mutex);
        while (do_close == 0) {
            pthread_cond_wait(&cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);

        (void) close(fd);

        return NULL;
    }

    int
    main(int argc, char **argv) {
        int efd, pfd[2], nfds, err;
        char c;
        ssize_t nbytes;
        pthread_t pth;
        void *cookie = (void *) (uintptr_t) time(NULL);
        struct epoll_event event, ev[4];

        efd = epoll_create(1);
        if (efd == -1) {
            printf("NO:epoll_create(1) failed: %s\n", strerror(errno));
            return 0;
        }

        if (pipe(pfd) != 0) {
            printf("ERROR:pipe(2) failed: %s\n", strerror(errno));
            return 0;
        }

        event.events = EPOLLIN | EPOLLONESHOT;
        event.data.ptr = cookie;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, pfd[0], &event) != 0) {
            printf("NO:epoll_ctl(2) failed: %s\n", strerror(errno));
            return 0;
        }

        err = pthread_create(&pth, NULL, worker, (void *) (uintptr_t) pfd[1]);
        if (err != 0) {
            printf("ERROR:pthread_create() failed: %s\n", strerror(err));
            return 0;
        }

        nfds = epoll_wait(efd, ev, 4, 0);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:epoll_wait(EPOLLIN | EPOLLONESHOT) returned "
                    "unexpected value: %d\n", nfds);
            return 0;
        }

        pthread_mutex_lock(&mutex);
        do_write = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        nfds = epoll_wait(efd, ev, 4, -1);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 1) {
            printf("NO:epoll_wait(EPOLLIN | EPOLLONESHOT) returned "
                    "unexpected value: %d\n", nfds);
            return 0;
        }

        if (ev[0].events != EPOLLIN) {
            printf("NO:Unexpected event bits: 0x%x\n", ev[0].events);
            return 0;
        }
        if (ev[0].data.ptr != cookie) {
            printf("NO:Unexpected user data: %p\n", ev[0].data.ptr);
            return 0;
        }

        nfds = epoll_wait(efd, ev, 4, 5);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:epoll_wait(EPOLLONESHOT) does not work: %d\n",
                    nfds);
            return 0;
        }

        nbytes = read(pfd[0], &c, 1);
        if (nbytes == (ssize_t) - 1) {
            printf("ERROR:read(2) failed: %s\n", strerror(errno));
            return 0;
        }
        if (nbytes != 1) {
            printf("ERROR:read(2) returned unexpected value: %d\n",
                    nbytes);
            return 0;
        }

        nfds = epoll_wait(efd, ev, 4, 0);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:epoll_wait(EPOLLIN | EPOLLONESHOT) returned "
                    "unexpected value: %d\n", nfds);
            return 0;
        }

        event.events = EPOLLIN | EPOLLONESHOT;
        event.data.ptr = cookie;
        if (epoll_ctl(efd, EPOLL_CTL_MOD, pfd[0], &event) != 0) {
            printf("NO:epoll_ctl(2) failed: %s\n", strerror(errno));
            return 0;
        }

        pthread_mutex_lock(&mutex);
        do_close = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        nfds = epoll_wait(efd, ev, 4, -1);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 1) {
            printf("NO:epoll_wait(EPOLLIN) returned unexpected value: "
                    "%d\n", nfds);
            return 0;
        }

        if ((ev[0].events & EPOLLHUP) == 0) {
            printf("NO:Unexpected event bits: 0x%x\n", ev[0].events);
            return 0;
        }
        if (ev[0].data.ptr != cookie) {
            printf("NO:Unexpected user data: %p\n", ev[0].data.ptr);
            return 0;
        }

        nfds = epoll_wait(efd, ev, 4, 5);
        if (nfds == -1) {
            printf("NO:epoll_wait(2) failed: %s\n", strerror(errno));
            return 0;
        } else if (nfds != 0) {
            printf("NO:epoll_wait(EPOLLONESHOT) does not work: %d\n",
                    nfds);
            return 0;
        }

        err = pthread_join(pth, NULL);
        if (err != 0) {
            printf("ERROR:pthread_join() failed: %s\n", strerror(err));
            return 0;
        }

        printf("OK:yes\n");

        return 0;
    }
#else /* !EPOLLONESHOT */

int
    main(int argc, char **argv) {
        printf("NO:EPOLLONESHOT is not defined.\n");
        return 0;
    }
#endif /* EPOLLONESHOT */
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether epoll(7) recognizes ' .
        'EPOLLONESHOT',
        DESC = > 'Define 1 if EPOLLONESHOT is supported.',
        DEFINE = > 'PFC_HAVE_EPOLLONESHOT',
        CFLAGS = > ['-pthread'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

    int
    main(int argc, char **argv) {
        uid_t ruid, euid, suid, uid = getuid();

        if (getresuid(&ruid, &euid, &suid) != 0) {
            printf("NO:getresuid() does not work: errno=%d\n", errno);
            return 0;
        }

        if (ruid != uid) {
            printf("NO:getresuid() returned unexpected ruid: %u, %u\n",
                    ruid, uid);
            return 0;
        }

        printf("OK:yes\n");
        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether getresuid() works',
        DESC = > 'Define 1 if getresuid() is supported.',
        DEFINE = > 'PFC_HAVE_GETRESUID',
        REQUIRED = > ['_GNU_SOURCE'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

    int
    main(int argc, char **argv) {
        gid_t rgid, egid, sgid, gid = getgid();

        if (getresgid(&rgid, &egid, &sgid) != 0) {
            printf("NO:getresgid() does not work: errno=%d\n", errno);
            return 0;
        }

        if (rgid != gid) {
            printf("NO:getresgid() returned unexpected rgid: %u, %u\n",
                    rgid, gid);
            return 0;
        }

        printf("OK:yes\n");
        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether getresgid() works',
        DESC = > 'Define 1 if getresgid() is supported.',
        DEFINE = > 'PFC_HAVE_GETRESGID',
        REQUIRED = > ['_GNU_SOURCE'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

    int
    main(int argc, char **argv) {
        uid_t uid = getuid();

        if (setresuid(uid, uid, uid) != 0) {
            printf("NO:setresuid() does not work: errno=%d\n", errno);
            return 0;
        }

        if (uid != 0) {
            int err;

            if (setresuid(0, 0, 0) == 0) {
                printf("NO:setresuid() succeeded unexpectedly.\n");
                return 0;
            }

            if ((err = errno) != EPERM) {
                printf("NO:setresuid() set unexpected errno: %d\n",
                        err);
                return 0;
            }
        }

        printf("OK:yes\n");
        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether setresuid() works',
        DESC = > 'Define 1 if setresuid() is supported.',
        DEFINE = > 'PFC_HAVE_SETRESUID',
        REQUIRED = > ['_GNU_SOURCE'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

    int
    main(int argc, char **argv) {
        gid_t gid = getgid();

        if (setresgid(gid, gid, gid) != 0) {
            printf("NO:setresgid() does not work: errno=%d\n", errno);
            return 0;
        }

        if (gid != 0) {
            int err;

            if (setresgid(0, 0, 0) == 0) {
                printf("NO:setresgid() succeeded unexpectedly.\n");
                return 0;
            }

            if ((err = errno) != EPERM) {
                printf("NO:setresgid() set unexpected errno: %d\n",
                        err);
                return 0;
            }
        }

        printf("OK:yes\n");
        return 0;
    }
    OUT
    push(@CONFIG_TESTS,{CHECK = > 'whether setresgid() works',
        DESC = > 'Define 1 if setresgid() is supported.',
        DEFINE = > 'PFC_HAVE_SETRESGID',
        REQUIRED = > ['_GNU_SOURCE'],
        IGNORE_ERROR = > 1,
        CODE = > $code});

    $code = << 'OUT';
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/prctl.h>

    static int
    change_dumpable(int value) {
        int ret;

        ret = prctl(PR_SET_DUMPABLE, value);
        if (ret == -1) {
            printf("NO:prctl(PR_SET_DUMPABLE, %d) failed: %s\n", value,
                    strerror(errno));
            return 0;
        } else if (ret != 0) {
            printf("NO:prctl(PR_SET_DUMPABLE, %d) returned unexpected "
                    "value: %d, %d\n", value, ret);
            return 0;
        }

        ret = prctl(PR_GET_DUMPABLE);
        if (ret != value) {
            printf("NO:prctl(PR_GET_DUMPABLE) returned unexpected value: "
                    "%d, %d:\n", value, ret);
            return 0;
        }

        return 1;
    }

    int
    main(int argc, char **argv) {
#if !defined(PR_GET_DUMPABLE)
        printf("NO:PR_GET_DUMPABLE is not defined.\n");
#elif !defined(PR_SET_DUMPABLE)
        printf("NO:PR_SET_DUMPABLE is not defined.\n");
#else /* PR_GET_DUMPABLE && PR_GET_DUMPABLE */

        /* Disable core dump. */
        if (change_dumpable(0) == 0) {
            return 0;
        }

        /* Enable core dump. */
        if (change_dumpable(1) == 0) {
            return 0;
        }

        printf("OK:yes\n");
#endif /* !PR_GET_DUMPABLE */

        return 0;
    }
    OUT

    push(@CONFIG_TESTS,{CHECK = > 'whether prctl(PR_SET_DUMPABLE) works',
        DESC = > 'Define 1 if prctl(PR_SET_DUMPABLE) is ' .
        'supported.',
        DEFINE = > 'PFC_HAVE_PRCTL_DUMPABLE',
        IGNORE_ERROR = > 1,
        CODE = > $code});
}

#Required Google Test version. (1.0 or later)
use constant GTEST_VERSION = > 1.0;

#Default installation directories.
use constant DEFAULT_PREFIX = > '/usr/local/pfc';

use constant OBJDIR_NAME = > 'objs';
use constant BUILD_DIR = > 'build';
use constant TOOLS_DIR = > 'tools';
use constant MODULES_DIR = > 'modules';

use constant CONFIG_MK = > BUILD_DIR . '/config.mk';
use constant CONFIG_PL = > BUILD_DIR . '/config.pl';
use constant CONFIG_H = > 'include/pfc/config.h';
use constant OPENSSL_CONST_H = > 'include/pfc/openssl_const.h';
use constant JAVA_CONFIG_MK = > 'java_config.mk';

use constant DEV_NULL = > '/dev/null';
use constant DEFAULT_PATH = > qw( / usr / bin / bin / usr / sbin / sbin);
use constant DEFAULT_INCDIR = > '/usr/include';

use constant TOOLBIN = > TOOLS_DIR . '/bin';
use constant DEFS_MK = > BUILD_DIR . '/defs.mk';
use constant CONFIG_STATUS = > 'config.status';
use constant STUB_GTEST_CONFIG = > TOOLS_DIR . '/src/gtest-config';

use constant VALUE_NONE = > 'none';

use constant CSTATUS_ARGV = > 'ARGV';

#Reserved Makefile variable names.
% MAKEVAR_RESV = (CC_FEATURE_DEFS = > 1,
        CFDEF_MODE = > 1,
        CONFIG_MK_INCLUDED = > 1,
        DEBUG_BUILD = > 1,
        EXTERNAL_RUNPATH = > 1,
        INST_BINDIR = > 1,
        INST_DATADIR = > 1,
        INST_DOCDIR = > 1,
        INST_INCLUDEDIR = > 1,
        INST_LIBDIR = > 1,
        INST_LIBEXECDIR = > 1,
        INST_LOCALSTATEDIR = > 1,
        INST_MODULEDIR = > 1,
        INST_MODCONFDIR = > 1,
        INST_SBINDIR = > 1,
        INST_SYSCONFDIR = > 1,
        INST_SYSSCRIPTDIR = > 1,
        INST_IPCWORKDIR = > 1,
        JAVA_OBJDIR = > 1,
        PFC_VERSION = > 1,
        PFC_VERSION_MAJOR = > 1,
        PFC_VERSION_MINOR = > 1,
        PFC_VERSION_REVISION = > 1,
        PFC_VERSION_PATCHLEVEL = > 1,
        PFC_VERSION_STRING = > 1,
        SUBARCH = > 1,
        SYSLOG_TYPE = > 1);

@PATH = ((DEFAULT_PATH), split( / : /, $ENV{PATH}));

$C_OPT = '-O3';
$CXX_OPT = '-O3';
$C_DEBUG = '-g';
$CXX_DEBUG = '-g';

#Use C++98 with GNU extensions.
$CXX_STD = '-std=gnu++98';

$DEBUG_BUILD = 1;
$SHELL_PATH = '/bin/sh';
$PATH_SCRIPT = '/bin:/usr/bin';

#Compiler option for strict checking.
use constant GCC_WARN_OPTS = >
        qw(-Wall - Wextra - Wno - clobbered - Wno - unused - parameter
        - Wno - unused - local - typedefs - Wno - unused - result - Wno - format - security);

use constant GCC_NO_STRICT_ALIAS = > '-fno-strict-aliasing';

#Supported OS types.
use constant OSTYPE_LINUX = > 'linux';

% OSTYPE = (OSTYPE_LINUX() = > 1);
% GCC_OSTYPE = (__linux = > OSTYPE_LINUX());

#OS specific make macros.
% OSDEP = (OSTYPE_LINUX() = >{
    CC_FEATURE_DEFS = > ['-D_GNU_SOURCE'],
    SYSLOG_TYPE = > 'syslog',
});

% OS_CONF = (OSTYPE_LINUX() = > \&check_linux);
@LINUX_DIST = ({FILE = > '/etc/fedora-release', CODE = > \&fedora_version},
{
    FILE = > '/etc/redhat-release', CODE = > \&redhat_version
},
{
    FILE = > '/etc/SuSE-release', CODE = > \&suse_version
},
{
    FILE = > '/etc/os-release', CODE = > \&os_release_version
},
{
    FILE = > '/etc/lsb-release', CODE = > \&lsb_version
});

#Supported architectures.
use constant ARCH_I386 = > 'i386';
use constant ARCH_I686 = > 'i686';
use constant ARCH_X86 = > 'x86';
use constant ARCH_X86_64 = > 'x86_64';

use constant COMMON_ARCH = >
{
    ARCH_I386() = > ARCH_X86(), ARCH_X86_64() = > ARCH_X86(),
};

use constant JAVA_ARCHFLAG = >
{
    ARCH_I386() = > '-d32', ARCH_I686() = > '-d32', ARCH_X86_64() = > '-d64',
};

#ILP model.
use constant MODEL_LP64 = > 'LP64';
use constant MODEL_ILP32 = > 'ILP32';

% ARCH = (ARCH_I386() = > MODEL_ILP32(), ARCH_X86_64() = > MODEL_LP64());
% GCC_ARCH = (__i386 = > ARCH_I386(), __x86_64 = > ARCH_X86_64());
% ARCH_CC_MODEFLAGS = (ARCH_I386() = > ['-m32', '-march=i686'],
        ARCH_X86_64() = > ['-m64']);
% ARCH_CXX_MODEFLAGS = (ARCH_I386() = > ['-m32', '-march=i686'],
        ARCH_X86_64() = > ['-m64']);

#Default library search path.
% DEFAULT_OSARCH_LIBPATH =
        (OSTYPE_LINUX() = >{
    MODEL_LP64() = >
    {
        PATH = > ['/lib64', '/usr/lib64'],
        NAME = > ['lib64'],
    },
    MODEL_ILP32() = >
    {
        PATH = > ['/lib', '/usr/lib'],
        NAME = > ['lib'],
    },
});

#Mandatory commands.
% COMMAND_SPEC = (make = >{NAMES = > ['make', 'gmake'],
    ARG = > ['--version'],
    PATTERN = > ['GNU Make'], DESC = > 'GNU make',
    VERIFY = > \&check_make},
gcc = >{NAMES = > ['gcc', 'cc'],
    ARG = > ['--version'],
    PATTERN = > ['gcc(-\S+)?\s+\('],
    DESC = > 'GNU cc',
    VERIFY = > \&check_gcc},
gxx = >{NAMES = > ['g++', 'c++'],
    ARG = > ['--version'],
    PATTERN = > ['g\+\+(-\S+)?\s+\('],
    DESC = > 'GNU c++',
    VERIFY = > \&check_gxx},
tar = >{NAMES = > ['tar', 'gtar'],
    ARG = > ['--version'],
    PATTERN = > ['GNU tar'],
    DESC = > 'GNU tar',
    EXPORT = > 1},
gtest_config = >{NAMES = > ['gtest-config'],
    ARG = > ['--usage'],
    PATTERN = > ['Usage: gtest-config'],
    DESC = > 'gtest-config',
    IGNORE_ERROR = > 1,
    EXPORT = > 1},
lsb_release = >{ARG = > ['-h'],
    PATTERN = > ['Usage: lsb_release '],
    IGNORE_ERROR = > 1},
ar = >{EXPORT = > 1},
cat = >{EXPORT = > 1},
mkdir = >{EXPORT = > 1},
touch = >{EXPORT = > 1},
install = >{EXPORT = > 1},
ln = >{EXPORT = > 1},
cp = >{EXPORT = > 1},
mv = >{EXPORT = > 1},
diff = >{EXPORT = > 1},
sed = >{EXPORT = > 1});

use constant OLDEST_YEAR = > 2012;

sub fatal(@);
sub error(@);
sub warning(@);
sub dupfd($$);
sub checking(@);
sub setup_java_libdir($);
sub exec_command(\ % @);
sub verify_command($\ %);
sub canonicalize_path($);
sub stringify($);
sub isLP64($);
sub create_year_range();
sub create_dynamic_header($$);
sub check_runpath($);
sub check_additional_vars($\ %);
sub check_extcmd_dirs($\@);
sub check_pfc_version($);
sub check_srcroot();
sub check_make($$);
sub check_gcc($$);
sub check_gxx($$);
sub check_gtest($);
sub check_gtest_config($);
sub compile_and_exec($ %);
sub check_perl($);
sub check_boost($);
sub check_openssl($\@);
sub check_openssl_const( %);
sub check_cflags_tests();
sub check_config_tests(\@$\ %);
sub format_boost_version($);
sub check_java(\ % $);
sub scan_jdk($$);
sub get_required_jdk_version($);
sub get_java_home($ %);
sub search_java_home($$ %);
sub check_java_home($ %);
sub check_java_version($$ %);
sub check_jarfile($$$$);
sub check_junit(\ % $);
sub check_junit_jar($);
sub check_ant(\ % $);
sub check_file($\@ %);
sub check_header($\@ %);
sub check_library($\ %);
sub check_library_impl($$\ %);
sub is_default_header_path($);
sub is_default_library_path($);
sub check_gcc_arch($@);
sub check_gcc_multiarch($);
sub check_gcc_option($$$$$);
sub check_strict_aliasing($$$\@);
sub check_gcc_ld($);
sub check_cc_search_path($);
sub check_os_arch($$$$);
sub write_makevar($$$);
sub create_config_mk($\ %);
sub create_config_pl(\ %);
sub create_java_config_mk($\ %);
sub create_config_h($$\@);
sub create_openssl_const_h($\ %);
sub create_config_status(\@\ %);
sub create_gtest_config();
sub parse_config_status($);
sub prepare(\ % @);
sub setup($);
sub make_symbol($$);
sub cpp_feature_key($);
sub make_feature_key($);
sub feature_setup(\ % \@$);
sub check_linux($\ %);
sub check_linux_dist();
sub check_lsb_release();
sub fedora_version($);
sub redhat_version($);
sub suse_version($);
sub os_release_version($);
sub lsb_version($);
sub read_command_output(\ % @);
sub read_command_oneline(\ % @);

#Configuration key for additional features.
use constant FEATURE_KEYS = > qw(refptr - debug tidlog);

use constant FEATURE_DESC = >
{
    'refptr-debug' = > 'refptr debugging',
};

END{
    if (@UNLINK_ON_ERROR) {
        unlink(@UNLINK_ON_ERROR) unless($ ? == 0);
    }}

#Class which represents additional definition in config.h.
{
    package ConfDef;

    sub new{
        my $this = shift;
        my $class = ref($this) || $this;
        my($name, $value, $desc, $required) = @_;

        my $me =
        {NAME = > $name, VALUE = > $value, DESC = > $desc,
            REQUIRED = > $required};

        return bless($me, $class);
    }

    sub print
            {
        my $me = shift;
        my($fh) = @_;

        my($name, $value, $desc) =
        ($me-> {
            NAME
        }, $me-> {
            VALUE
        }, $me-> {
            DESC
        });
        if (defined($value)) {
            my($required, $comm) = $me->getRequired();
            if ($required) {
                $fh->print( << OUT);
                /* $desc */
#if $required
#define $name $value
#endif /* $comm */
#define __$name $value

                OUT
            } else {
                $fh->print( << OUT);
                /* $desc */
#define $name $value

                OUT
            }
        } else {
            $fh->print( << OUT);
            /* $desc */
#undef $name

            OUT
        }}

    sub getRequired
            {
        my $me = shift;

        my $required = $me-> {
            REQUIRED
        };
        $required = &$required() if (ref($required) eq 'CODE');
        return (undef, undef) unless($required);

        if (ref($required) eq 'ARRAY') {
            return (undef, undef) unless(@$required);
        } else {
            $required = [$required];
        }

        my(@defs, @comm);
        my $pat = qr, ^!,;
        foreach my $r(@$required)
        {
            my $invert = '';
            $invert = '!' if ($r = ~s, % pat,,);
            push(@defs, sprintf("%sdefined(%s)", $invert, $r));
            push(@comm, $invert . $r);
        }

        my $and = ' && ';
        return (join($and, @defs), join($and, @comm));}
}

#Class which represents file handle associated with temporary file.
#A created file will be removed by destructor.
{
    package TempFile;

    use vars qw($AUTOLOAD);
    use FileHandle;

    sub new{
        my $this = shift;
        my $class = ref($this) || $this;
        my($fname, $flags, $mode) = @_;

        my $fh = FileHandle->new($fname, $flags, $mode);
        return undef unless($fh);

        my $me =
        {HANDLE = > $fh, NAME = > $fname};

        return bless($me, $class);
    }

    sub AUTOLOAD
            {
        my $me = shift;

        my $method = $AUTOLOAD;
        $method = ~s, ^.*::([^ : ]+)$, $1, o;
        return if ($method eq 'DESTROY');

        my $fh = $me-> {
            HANDLE
        };

        return $fh->$method(@_);}

    sub DESTROY
            {
        my $me = shift;

        undef $me-> {
            HANDLE
        };
        unlink($me-> {
            NAME
        });}
}

#Utility class to print message with line breaking.
{
    package LineBreak;

#Maximum column width of screen.
    use constant MAX_LINE_WIDTH = > 78;

    sub new{
        my $this = shift;
        my $class = ref($this) || $this;
        my($fh) = @_;

        my $me =
        {_HANDLE = > $fh, _CURSOR = > 0};

        return bless($me, $class);
    }

#Insert newline character.
    sub newLine
            {
        my $me = shift;
        my($indent, $num) = @_;

        my $fh = $me-> {
            _HANDLE
        };
        if (defined($num)) {
            $fh->print("\n" x $num);
        } else {
            $fh->print("\n");
        }
        if ($indent > 0) {
            $fh->print(" " x $indent);
            $me-> {
                _CURSOR
            }
            = $indent;
        } else {
            $me-> {
                _CURSOR
            }
            = 0;
        }}

#Move cursor to the specified position.
    sub moveCursor
            {
        my $me = shift;
        my($pos) = @_;

#Insert newline if needed.
        my $cursor = $me-> {
            _CURSOR
        };
        my $fh = $me-> {
            _HANDLE
        };
        if ($pos < $cursor) {
            $fh->print("\n");
            $cursor = 0;
        }

        while ($cursor < $pos) {
            $fh->print(" ");
            $cursor++;
        }
        $me-> {
            _CURSOR
        }
        = $pos;}

#Print message without line breaking.
#Specified message must not contain any newline character.
    sub printRaw
            {
        my $me = shift;
        my($startpos, $msg) = @_;

        $me->moveCursor($startpos);
        $me-> {
            _CURSOR
        }
        += length($msg);
        my $fh = $me-> {
            _HANDLE
        };
        $fh->print($msg);}

#Print message without line breaking.
#A newline character is inserted if the specified message exceeds
#the max width.
#Specified message must not contain any newline character.
    sub printNoWrap
            {
        my $me = shift;
        my($indent, $msg) = @_;

        my $len = length($msg);
        my $fh = $me-> {
            _HANDLE
        };
        my $cursor = $me-> {
            _CURSOR
        };
        my $nextcur = $cursor + $len + 1;
        if ($nextcur > MAX_LINE_WIDTH) {
#Need to break line.
            $me->newLine($indent);
            $cursor = $indent;
        } else {
#Insert whitespace.
            $fh->print(' ');
            $cursor++;
        }
        $fh->print($msg);
        $me-> {
            _CURSOR
        }
        = $cursor + $len;}

#Print message with line breaking.
    sub printMessage
            {
        my $me = shift;
        my($startpos, $indent, $msg) = @_;

        $me->moveCursor($startpos);

        my $fh = $me-> {
            _HANDLE
        };
        my $cursor = $me-> {
            _CURSOR
        };

#Eliminate leading whitespaces.
        $msg = ~s, ^\s +,,;
        my(@array) = split(//, $msg);
        while (my $word = $me->getWord(\@array)) {
            my $wlen = length($word);
                    my $nextcur = $cursor + $wlen;
            if ($nextcur > MAX_LINE_WIDTH) {
#Need to break line.

                $me->newLine($indent);
                        $cursor = $indent;
            }
            elsif($cursor > $indent) {
                $fh->print(" ");
                        $cursor++;
            }

            $fh->print($word);
                    $cursor += $wlen;

#Skip sequence of whitespaces.
                    while (@array and $array[0] = ~m, ^\s +, o) {
                my $c = shift(@array);
                if ($c eq "\n") {
                    $me->newLine($indent);
                            $cursor = $indent;
                }
            }
        }

        $me-> {
            _CURSOR
        }
        = $cursor;}

#Get a word from the specified character array.
    sub getWord{
        my $me = shift;
        my($array) = @_;

        my(@ret);
        while (@$array) {
            my $c = shift(@$array);
            if ($c = ~m, ^\s$, o) {
                unshift(@$array, $c);
                        last;
            }
            push(@ret, $c);
        }

        return (@ret) ? join('', @ret) : undef;
    }
}

#Class which represents simple boolean option spec.
{
    package OptSpec;

            use constant INDENT_OPTION = > 4;
            use constant INDENT_DESC = > 8;

            use constant OPT_PREFIX = > '--';

            use constant OPTARG_DIR = > '<dir>';
            use constant OPTARG_FILE = > '<file>';
            use constant OPTARG_PATH = > '<path>';
            use constant OPTARG_OSTYPE = > '<ostype>';
            use constant OPTARG_ARCH = > '<arch>';
            use constant OPTARG_CPP_OPT = > '<cpp-option>';
            use constant OPTARG_CC_OPT = > '<cc-option>';
            use constant OPTARG_CXX_OPT = > '<cxx-option>';
            use constant OPTARG_MAKEVAR = > '<name>=<value>';
            use constant OPTARG_GROUP_LIST = > '<group-or-gid>';
            use constant OPTARG_BUILD_ID = > '<build-id>';
            use constant OPTARG_SCM_REV = > '<scm-revision>';
            use constant OPTARG_DEFAULT = > '<arg>';

            use constant OPTDEF_AUTO = > 'auto-detect';

            sub new{
        my $this = shift;
        my $class = ref($this) || $this;
        my($key, $desc, % attr) = @_;

        my $me =
        {_KEY = > $key, _DESC = > $desc};
        my $default = $attr
        {default};
        $me-> {
            _DEFAULT
        }
        = $default if (defined($default));
                $me-> {
                    _ATTR
                }
                = \ % attr;
        $me = bless($me, $class);

        my $def = $me->getDefault();
        $me->value($def) if (defined($def));

            return $me;
        }

    sub getKey{
        my $me = shift;

        return $me-> {
            _KEY
        };
    }

    sub getOption{
        my $me = shift;

        return OPT_PREFIX . $me->getKey();
    }

    sub attribute{
        my $me = shift;
        my $key = shift;

        my $attr = $me-> {
            _ATTR
        };
        my $value = $attr-> {
            $key
        };
        $attr-> {
            $key
        }
        = $_[0] if (@_ > 0);

            return $value;
        }

    sub value{
        my $me = shift;

        my $value = $me-> {
            _VALUE
        };
        if (@_ > 0) {
            my $v = $_[0];
                    $me-> {
                        _VALUE
                    }
                    = $v;

                    my $d = $me-> {
                        _DEFAULT
                    };
            if (ref($d) eq 'SCALAR') {
#Overwrite default value.
                $$d = $v;
            }
        }

        return $value;
    }

#Set value specified by configure option.
    sub setValue{
        my $me = shift;
        my($value) = @_;

#Invoke check method if needed.
        my $check = $me->attribute('check');
        $value = $me->$check($value) if ($check);

#Set new value.
                $me->value($value);
        }

    sub getDescription{
        my $me = shift;

        return $me-> {
            _DESC
        };
    }

    sub setSpec{
        my $me = shift;
        my($spec) = @_;

        my $func = $me->getSpecFunc();
        foreach my $k(@
        {$me->getSpecKeys()})
        {
            &main::fatal("Option conflicts: $k")
            if ($spec-> {
                    $k
                }
                or $k eq 'help');
            $spec-> {
                $k
            }
            = $func;
        }
    }

    sub getSpecKeys{
        my $me = shift;

        return [$me->getKey()];
    }

    sub getSpecFunc{
        my $me = shift;

        my $key = $me->getKey();
        my $func = sub
        {
            my($k, $v) = @_;

            $me->setValue($v);
        };

        return $func;
    }

    sub getDefault{
        my $me = shift;

        my $default = $me-> {
            _DEFAULT
        };

        return (ref($default) eq 'SCALAR') ? $$default : $default;
    }

    sub getDefaultDesc{
        my $me = shift;

        return $me->OPTDEF_AUTO if ($me->attribute('auto'));
            return $me->getDefault();
        }

    sub printHelp{
        my $me = shift;
        my($lbrk) = @_;

        my $option = $me->getOption();
        my $arg = $me->attribute('arg');
        $option . = ' ' . $arg if ($arg);

                $lbrk->printRaw(INDENT_OPTION, $option);
                $lbrk->printMessage(INDENT_DESC, INDENT_DESC,
                $me->getDescription(1));
                my $default = $me->getDefaultDesc();
            if ($default) {
                my $def = "(Default: $default)";
                        $lbrk->printNoWrap(INDENT_DESC, $def);
            }
    }
}

#Option spec class to toggle boolean switch by "--enable-XXX" or
#"--disable-XXX".
{
    package BoolOptSpec;

            use base qw(OptSpec);

            use constant PREFIX_ENABLE = > 'enable';
            use constant PREFIX_DISABLE = > 'disable';

            use constant DESC_ENABLED = > 'enabled';
            use constant DESC_DISABLED = > 'disabled';

            sub value{
        my $me = shift;

        my $value = $me-> {
            _VALUE
        };
        if (@_ > 0) {
            my $d = $me-> {
                _DEFAULT
            };
            if ($_[0]) {
                $me-> {
                    _VALUE
                }
                = 1;
                if (ref($d) eq 'SCALAR') {
#Overwrite default value.
                    $$d = 1;
                }
            } else {
                delete($me-> {
                    _VALUE
                });
                if (ref($d) eq 'SCALAR') {
#Overwrite default value.
                    undef $$d;
                }
            }
        }

        return $value;
    }

    sub getDefaultDesc{
        my $me = shift;

        return $me->OPTDEF_AUTO if ($me->attribute('auto'));
            return ($me->getDefault()) ? DESC_ENABLED : DESC_DISABLED;
        }

    sub getSpecKeys{
        my $me = shift;

        my $key = $me->getKey();
        my(@prefix) = (PREFIX_ENABLE(), PREFIX_DISABLE());
        my(@spec) = map
        {$_ . '-' . $key}
        @prefix;

        return \@spec;
    }

    sub getSpecFunc{
        my $me = shift;

        my $key = $me->getKey();
        my $en = PREFIX_ENABLE;
        my $dis = PREFIX_DISABLE;
        my $pat = qr, ^($en | $dis) - $key$,;
        my $func = sub
        {
            my($k) = @_;

            if ($k = ~$pat) {
                my $v = ($1 eq PREFIX_ENABLE) ? 1 : 0;
                        $me->setValue($v);
            }
        };

        return $func;
    }

    sub printHelp{
        my $me = shift;
        my($lbrk) = @_;

        my $key = $me->getKey();
        foreach my $opt(PREFIX_ENABLE(), PREFIX_DISABLE())
        {
            my $option = $me->OPT_PREFIX . $opt . '-' . $key;
            $lbrk->printRaw($me->INDENT_OPTION, $option);
        }

        $lbrk->printMessage($me->INDENT_DESC, $me->INDENT_DESC,
        $me->getDescription(1));
        my $default = $me->getDefaultDesc();
        if ($default) {
            my $def = "(Default: $default)";
                    $lbrk->printNoWrap($me->INDENT_DESC, $def);
        }
    }
}

#Option spec class which takes a string argument.
{
    package StrOptSpec;

            use base qw(OptSpec);
            use Cwd qw(abs_path);
            use POSIX qw( : errno_h : fcntl_h);
            use File::stat;

            use constant KEY_PREFIX = > 'with-';

            sub new{
        my $this = shift;
        my $class = ref($this) || $this;
        my($key, $desc, % attr) = @_;

        my $arg = $attr
        {arg};
        $attr
        {arg}
        = OptSpec::OPTARG_DEFAULT unless($arg);

        return $class->SUPER::new($key, $desc, % attr);
    }

    sub getOption{
        my $me = shift;

        return $me->OPT_PREFIX . $me->getKeyPrefix() . $me->getKey();
    }

    sub getKeyPrefix{
        my $me = shift;

        return $me->attribute('prefix') || KEY_PREFIX;
    }

    sub getSpecKeys{
        my $me = shift;

        my $key = $me->getKey();

        return [$me->getKeyPrefix() . $key . '=s'];
    }

#Set empty string as option value if 'none' is specified as new value.
    sub checkNone{
        my $me = shift;
        my($value) = @_;

        $value = '' if ($value eq main::VALUE_NONE());

            return $value;
        }

#Ensure that the specified value is valid absolute path on the build
#host.
    sub checkAbsPath{
        my $me = shift;
        my($value) = @_;

        &main::error($me->getOption(),
        ": Value must be an absolute path.")
        unless(substr($value, 0, 1) eq '/');

        my $path = abs_path($value);
        &main::error($me->getOption(),
        ": Failed to determine absolute path: ", $value)
        unless($path);

        my $st = stat($path);
        unless($st)
        {
            &main::error($me->getOption(),
            ": File not found: ", $value)
            if ($! == ENOENT);
                    &main::error($me->getOption(),
                    ": stat() failed: $value: $!");
            }

        my $arg = $me->attribute('arg');
        if ($arg eq $me->OPTARG_FILE) {
            &main::error($me->getOption(),
                    ": Regular file must be specified: ",
                    $value) unless(S_ISREG($st->mode()));
        }
        elsif($arg eq $me->OPTARG_DIR)
        {
            &main::error($me->getOption(),
            ": Directory must be specified: ",
            $value) unless(S_ISDIR($st->mode()));
        }

        return $path;
    }

#Same as checkAbsPath, but this method sets absolute file path to
#%main::COMMAND_SPEC.
    sub checkAbsCommandPath{
        my $me = shift;
        my($value) = @_;

        my $path = $me->checkAbsPath($value);
        my $key = $me->getKey();
        $main::COMMAND_SPEC
        {$key}
        -> {
            ABSPATH
        }
        = $path;

        return $path;
    }

#Ensure that the specified value is valid absolute file path on the
#target system.
    sub checkTargetAbsPath{
        my $me = shift;
        my($value) = @_;

        &main::error($me->getOption(),
        ": Value must be an absolute path.")
        unless(substr($value, 0, 1) eq '/');

        my $arg = $me->attribute('arg');
        if ($arg eq $me->OPTARG_FILE) {
            &main::error($me->getOption(),
                    ": Value must not be directory path.")
            if ($value = ~m, / $, o);
            }

        return &main::canonicalize_path($value);
    }

#Ensure that the specified value is suitable for PATH environment
#variable on the target system.
    sub checkTargetPathEnv{
        my $me = shift;
        my($value) = @_;

        foreach my $path(split( / : /, $value, -1))
        {
            &main::error($me->getOption(),
            ": PATH environment variable must not ",
            "contain an empty string.")
            if ($path eq '');
                    &main::error($me->getOption(),
                    ": Command path must be an absolute ",
                    "path: $path")
                    unless(substr($path, 0, 1) eq '/');

                    foreach my $comp(split( / \//, $path, -1)) {
                    &main::error($me->getOption(),
                    ": Command search path must not ",
                    "contain \"$comp\": $path")
                if ($comp eq '.' or $comp eq '..');
                }
    }

    return $value;
}
}

#Option spec class which takes a list of string arguments.
#Each value will be separated with whitespace character.
{
    package StrListOptSpec;

            use base qw(StrOptSpec);

            sub new{
        my $this = shift;
        my $class = ref($this) || $this;

        my $me = $class->SUPER::new(@_);
        $me-> {
            _VALUE
        }
        = [];

        return bless($me, $class);
    }

    sub getSpecKeys{
        my $me = shift;

        my $key = $me->getKey();

        return [$me->getKeyPrefix() . $key . '=s@'];
    }

    sub value{
        my $me = shift;

        my $value = $me-> {
            _VALUE
        };
        if (@_ > 0) {
            my $v = $_[0];
            if (ref($v) eq 'ARRAY') {
                $me-> {
                    _VALUE
                }
                = $v;
            } else {
                push(@$value, $v);
            }

            my $d = $me-> {
                _DEFAULT
            };
            if (ref($d) eq 'ARRAY') {
#Overwrite default value.
                @$d = @$value;
            }
        }

        return $value;
    }

#Set value specified by configure option.
    sub setValue{
        my $me = shift;
        my($value) = @_;

        my $check = $me->attribute('check');
        foreach my $val(split( / \s + /, $value))
        {
#Invoke check method if needed.
            $val = $me->$check($val) if ($check);

#Set new value.
                    $me->value($val);
            }
    }
}

#Option spec class which takes a string map argument.
{
    package StrMapOptSpec;

            use base qw(StrOptSpec);

            sub new{
        my $this = shift;
        my $class = ref($this) || $this;

        my $me = $class->SUPER::new(@_);
        $me-> {
            _MAP
        }
        =
        {};

        return bless($me, $class);
    }

    sub getSpecKeys{
        my $me = shift;

        my $key = $me->getKey();

        return [$me->getKeyPrefix() . $key . '=s%'];
    }

    sub getSpecFunc{
        my $me = shift;

        my $key = $me->getKey();
        my $map = $me-> {
            _MAP
        };
        my $func = sub
        {
            my($k, $name, $value) = @_;

            $map-> {
                $name
            }
            = $value;
        };

        return $func;
    }

    sub getMap{
        my $me = shift;

        return $me-> {
            _MAP
        };
    }

#Disable value() and setValue() methods because this option keeps
#pairs of name and value.
    sub value;
            sub setValue;
}

#Option spec class which takes an user name.
{
    package UserOptSpec;

            use base qw(StrOptSpec);

            sub new{
        my $this = shift;
        my $class = ref($this) || $this;

        my $me = $class->SUPER::new(@_);
        $me = bless($me, $class);

        $me->attribute('arg', '<user-or-uid>');
        $me->attribute('check', 'checkUser');

        my $def = $me->getDefault();
        $me->setValue($def) if (defined($def));

            return $me;
        }

#Ensure that the specified user name is valid, and return user ID.
    sub checkUser{
        my $me = shift;
        my($value) = @_;

        my $uid;
        if ($value = ~ / ^\d + $ /) {
            $uid = $value;
        } else {
            $uid = getpwnam($value);
                    &main::error($me->getOption(),
                    ": Unknown user name: ", $value)
                    unless(defined($uid));
        }

        return $uid;
    }
}

#Option spec class which determines installation directory.
{
    package InstOptSpec;

            use base qw(StrOptSpec);

            use constant PREFIX_USR_DIRS = >{
        sysconfdir = > '/etc',
        localstatedir = > '/var',
    };

    sub new{
        my $this = shift;
        my $class = ref($this) || $this;

        my $me = $class->SUPER::new(@_);
        $me = bless($me, $class);

#This option always takes directory path.
        $me->attribute('arg', $me->OPTARG_DIR);

        my $dname = $me->attribute('dname');
        my $relative = $me->attribute('relative');
        my $default = main::DEFAULT_PREFIX();
        if ($relative) {
            $default = uc($relative) . '/' . $dname;
        }
        elsif($dname)
        {
            $default . = '/' . $dname;
        }

#Set default value.
        $me-> {
            _DEFAULT
        }
        = $default;

#Set default value as value.
        $me->value($default) unless($dname);

#Set check method for a new value.
        $me->attribute('check', 'checkInstDir');

        return $me;
    }

    sub getDescription{
        my $me = shift;
        my($detailed) = @_;

        my $desc = $me->SUPER::getDescription($detailed);
        if ($detailed and $me->getKey() eq 'includedir') {
            $desc . = "\nHeader files are not installed if \"" .
                    main::VALUE_NONE() . "\" is specified.";
        }

        return $desc;
    }

#Ensure that the specified value is suitable for installation
#directory path.
    sub checkInstDir{
        my $me = shift;
        my($value) = @_;

        my $key = $me->getKey();
        return $value if ($key eq 'includedir' and
                $value eq main::VALUE_NONE);

                &main::error($me->getOption(),
                ": Value must be an absolute path.")
                unless(substr($value, 0, 1) eq '/');

            return &main::canonicalize_path($value);
        }

    sub resolvePath{
        my $me = shift;
        my($conf, $prefix) = @_;

        my $key = $me->getKey();
        my $dir = $me->value();
        return if ($dir);

                my $dname = $me->attribute('dname');
            return unless($dname);

                my $relative = $me->attribute('relative');
            if ($relative) {
                $dir = $conf->get($relative) . '/' . $dname;
            } else {
                if ($prefix eq '/usr' and PREFIX_USR_DIRS-> {
                        $key
                    }) {
                $dir = PREFIX_USR_DIRS-> {
                    $key
                };
            } else {
                    $dir = $prefix . '/' . $dname;
                }
            }

        $me->value($dir);
    }

    sub getOption{
        my $me = shift;

        return $me->OPT_PREFIX . $me->getKey();
    }

    sub getSpecKeys{
        my $me = shift;

        my $key = $me->getKey();

        return [$key . '=s'];
    }
}

{
    package Configure;

            use Getopt::Long;
            use Config;
            use Cwd;
            use vars qw( % Config @SPECS @SPEC_LIST);

#Prefix of OptSpec class.
            use constant OPTTYPE_INST = > 'Inst';
            use constant OPTTYPE_BOOL = > 'Bool';
            use constant OPTTYPE_STRING = > 'Str';
            use constant OPTTYPE_STRING_LIST = > 'StrList';
            use constant OPTTYPE_STRING_MAP = > 'StrMap';
            use constant OPTTYPE_USER = > 'User';

            use constant OPTCLASS_SUFFIX = > 'OptSpec';

#Template for supported configure options.
#- The first element is prefix of OptSpec prefix.
#- The second element is option key.
#- The third element is description of the option.
#- The rest of elements are HASH table contents which contains
#optional attributes.
            @SPECS = ([OPTTYPE_INST, 'prefix',
            "Installation directory prefix."],
            [OPTTYPE_INST, 'bindir',
            "Installation directory for user executables.",
            dname = > 'bin'],
            [OPTTYPE_INST, 'sbindir',
            "Installation directory for system admin executables.",
            dname = > 'sbin'],
            [OPTTYPE_INST, 'libexecdir',
            "Installation directory for program executables.",
            dname = > 'libexec'],
            [OPTTYPE_INST, 'datadir',
            "Installation directory for read-only data.",
            dname = > 'share'],
            [OPTTYPE_INST, 'sysconfdir',
            "Installation directory for read-only system " .
            "configuration.",
            dname = > 'etc'],
            [OPTTYPE_INST, 'localstatedir',
            "Installation directory for modifiable system data.",
            dname = > 'var'],
            [OPTTYPE_INST, 'libdir',
            "Installation directory for library files.",
            dname = > 'lib'],
            [OPTTYPE_INST, 'docdir',
            "Installation directory for documents.",
            dname = > 'doc', relative = > 'datadir'],
            [OPTTYPE_INST, 'moduledir',
            "Installation directory for PFC modules.",
            dname = > 'modules'],
            [OPTTYPE_INST, 'modconfdir',
            "Installation directory for public configuration files " .
            "for PFC modules.",
            dname = > 'modconf'],
            [OPTTYPE_INST, 'includedir',
            "Installation directory for C/C++ header files.",
            dname = > 'include'],
            [OPTTYPE_INST, 'sysscriptdir',
            "Installation directory for system admin shell scripts.",
            dname = > 'sbin'],
            [OPTTYPE_INST, 'ipcworkdir',
            "Installation directory for IPC framework workspace.",
            dname = > 'run/ipc', relative = > 'localstatedir'],
            [OPTTYPE_INST, 'javadir',
            "Installation directory for Java programs.",
            dname = > 'java', relative = > 'libdir'],
            [OPTTYPE_INST, 'certsdir',
            "Installation directory for Certification Authorities.",
            dname = > 'certs', relative = > 'sysconfdir'],

            [OPTTYPE_STRING, 'c-optimize',
            "Specify C compiler option for optimization.",
            arg = > OptSpec::OPTARG_CC_OPT,
    default = > \$main::C_OPT, prefix = > 'enable-',
            check = > 'checkNone'],
            [OPTTYPE_STRING, 'cxx-optimize',
            "Specify C++ compiler option for optimization.",
            arg = > OptSpec::OPTARG_CXX_OPT,
    default = > \$main::CXX_OPT, prefix = > 'enable-',
            check = > 'checkNone'],
            [OPTTYPE_STRING, 'c-debug',
            "Specify C compiler option for debugging.",
            arg = > OptSpec::OPTARG_CC_OPT,
    default = > \$main::C_DEBUG, prefix = > 'enable-',
            check = > 'checkNone'],
            [OPTTYPE_STRING, 'cxx-debug',
            "Specify C++ compiler option for debugging.",
            arg = > OptSpec::OPTARG_CXX_OPT,
    default = > \$main::CXX_DEBUG, prefix = > 'enable-',
            check = > 'checkNone'],
            [OPTTYPE_STRING_LIST, 'prep-includes',
            'Specify C/C++ header search path prior to default.',
            arg = > OptSpec::OPTARG_DIR,
            check = > 'checkTargetAbsPath'],
            [OPTTYPE_STRING_LIST, 'prep-libraries',
            'Specify C/C++ library search path prior to default.',
            arg = > OptSpec::OPTARG_DIR,
            check = > 'checkTargetAbsPath'],
            [OPTTYPE_BOOL, 'debug',
            "Enable or disable debugging build option.",
    default = > $main::DEBUG_BUILD],
            [OPTTYPE_BOOL, 'errwarn',
            "Determine whether compilation warning should be treated " .
            "as error or not.", default = > 1],
            [OPTTYPE_BOOL, 'strict-aliasing',
            'Determine whether to allow the compiler to assume ' .
            'strict aliasing rules.', default = > 1],
            [OPTTYPE_STRING_LIST, 'cppflags',
            "Specify additional C/C++ preprocessor flags.\n" .
            "This option can be specified multiple times.",
            arg = > OptSpec::OPTARG_CPP_OPT],

            [OPTTYPE_STRING, 'objdir',
            "Specify directory path to store build outputs.",
            arg = > OptSpec::OPTARG_DIR,
    default = > getcwd() . '/' . main::OBJDIR_NAME(),
            check = > 'checkTargetAbsPath'],
            [OPTTYPE_STRING, 'ldlibdir',
            "Specify intermediate directory path to store library " .
            "files.", auto = > 1, check = > 'checkTargetAbsPath'],
            [OPTTYPE_STRING, 'gcc',
            "Specify gcc path.",
            arg = > OptSpec::OPTARG_FILE, auto = > 1,
            check = > 'checkAbsCommandPath'],
            [OPTTYPE_STRING, 'gxx',
            "Specify g++ path.",
            arg = > OptSpec::OPTARG_FILE, auto = > 1,
            check = > 'checkAbsCommandPath'],
            [OPTTYPE_STRING, 'ar',
            "Specify ar path.",
            arg = > OptSpec::OPTARG_FILE, auto = > 1,
            check = > 'checkAbsCommandPath'],
            [OPTTYPE_STRING, 'os',
            "Specify OS type.",
            arg = > OptSpec::OPTARG_OSTYPE, auto = > 1],
            [OPTTYPE_STRING, 'arch',
            "Specify CPU architecture.",
            arg = > OptSpec::OPTARG_ARCH, auto = > 1],
            [OPTTYPE_STRING, 'perl',
            "Specify perl path.",
            arg = > OptSpec::OPTARG_FILE, default = > $Config{perlpath},
    check = > 'checkAbsPath'],
            [OPTTYPE_STRING, 'boost',
            "Specify installation prefix of boost.",
            arg = > OptSpec::OPTARG_DIR, auto = > 1],
            [OPTTYPE_STRING, 'openssl',
            "Specify installation prefix of OpenSSL.",
            arg = > OptSpec::OPTARG_DIR, auto = > 1],
            [OPTTYPE_STRING, 'openssl-runpath',
            'Specify runtime library path for OpenSSL libraries.',
            arg = > OptSpec::OPTARG_PATH],
            [OPTTYPE_STRING, 'shell',
            "Specify absolute path to bourne shell on the target " .
            "system.",
            arg = > OptSpec::OPTARG_FILE, default = > \$main::SHELL_PATH,
            check = > 'checkTargetAbsPath'],
            [OPTTYPE_STRING, 'path',
            "Specify PATH environment variable for scripts.",
            arg = > OptSpec::OPTARG_PATH, default = > \$main::PATH_SCRIPT,
            check = > 'checkTargetPathEnv'],
            [OPTTYPE_STRING, 'extcmd-dir',
            "Specify additional command path which contains external " .
            "commands.",
            arg = > OptSpec::OPTARG_DIR,
            check = > 'checkTargetAbsPath'],
            [OPTTYPE_STRING, 'version-file',
            "Specify file path which defines software version.",
            arg = > OptSpec::OPTARG_FILE],
            [OPTTYPE_STRING, 'build-id',
            'Specify package build identifier.',
            arg = > OptSpec::OPTARG_BUILD_ID],
            [OPTTYPE_STRING, 'scm-revision',
            'Specify SCM revision number.',
            arg = > OptSpec::OPTARG_SCM_REV],
            [OPTTYPE_STRING_LIST, 'runpath',
            "Specify additional runtime shared library search path.\n" .
            "This option can be specified multiple times.",
            arg = > OptSpec::OPTARG_PATH],
            [OPTTYPE_STRING, 'openflow-includes',
            "Specify directory which contains OpenFlow header files." .
            "\n" .
            "OpenFlow header files in the source tree are never used " .
            "if this option is specified.",
            arg = > OptSpec::OPTARG_DIR],
            [OPTTYPE_STRING, 'pf_nec-h',
            "Specify path to pf_nec.h.\n" .
            "pf_nec.h in the source tree is never used if this " .
            "option is specified.",
            arg = > OptSpec::OPTARG_FILE],

            [OPTTYPE_BOOL, 'java',
            "Build Java bindings.", default = > 0],
            [OPTTYPE_STRING, 'java-objdir',
            "Specify directory path to store Java binary files.",
            arg = > OptSpec::OPTARG_DIR, auto = > 1,
            check = > 'checkTargetAbsPath'],
            [OPTTYPE_STRING, 'ant',
            "Specify path to Apache Ant script.",
            arg = > OptSpec::OPTARG_FILE, auto = > 1,
            check = > 'checkAbsPath'],
            [OPTTYPE_STRING, 'ant-home',
            "Specify path to Apache Ant home directory.",
            arg = > OptSpec::OPTARG_DIR, default = > '/usr/share/ant',
            check = > 'checkAbsPath'],
            [OPTTYPE_STRING, 'java-home',
            "Specify path to JDK home directory. " .
            "If omitted or an invalid path is specified, " .
            "this command tries to find the location of the JDK.",
            arg = > OptSpec::OPTARG_DIR, auto = > 1,
            check = > 'checkAbsPath'],
            [OPTTYPE_STRING, 'java-version',
            'Specify required version of JDK. (Default: ' .
            main::JDK_VERSION_MAJOR() . '.' .
            main::JDK_VERSION_MINOR() . ')'],
            [OPTTYPE_BOOL, 'java-debug',
            "Specify whether to compile Java programs with debugging " .
            "option.", default = > 1],
            [OPTTYPE_STRING, 'java-encoding',
            "Specify encoding of Java source.", default = > 'utf-8'],
            [OPTTYPE_STRING, 'junit',
            "Specify absolute path to junit.jar, which is used for " .
            "unit test of Java programs.", auto = > 1,
            arg = > OptSpec::OPTARG_FILE, check = > 'checkAbsPath'],
            [OPTTYPE_STRING_LIST, 'java-libdir',
            "Specify absolute path to Java library directory.\n" .
            "This option can be specified multiple times.",
            arg = > OptSpec::OPTARG_DIR, auto = > 1,
            check = > 'checkAbsPath'],

            [OPTTYPE_STRING_MAP, 'make-variable',
            "Specify additional pair of Makefile variable name and " .
            "value.\n<name> will be defined in " .
            main::CONFIG_MK() . ".",
            arg = > OptSpec::OPTARG_MAKEVAR],

            [OPTTYPE_BOOL, 'refptr-debug',
            "Enable or disable refptr debugging.",
    default = > 1],
            [OPTTYPE_BOOL, 'tidlog',
            "Enable or disable thread ID logging in the PFC log.",
    default = > 1],

            [OPTTYPE_STRING, 'build-type-suffix',
            "Specify an arbitrary string to be added to build type " .
            "suffix."],
            );

            sub new{
        my $this = shift;
        my $class = ref($this) || $this;

        Getopt::Long::Configure(qw(no_ignore_case bundling
        require_order));
        my $me =
        {_OPTS = >
            {}, _INSTOPTS = >
            {}, _INSTRELOPTS = >
            {}};
        $me = bless($me, $class);
        $me->parse();

        return $me;
    }

    sub parse{
        my $me = shift;

        my $spec = $me->createSpec();
        $me->usage() unless(GetOptions( % $spec));
        $me->fixup();
    }

    sub createSpec{
        my $me = shift;

        @SPEC_LIST = ();
        my( % map, @list);
        my $opts = $me-> {
            _OPTS
        };
        foreach my $t(@SPECS)
        {
            my(@tmpl) = (@$t);
            my $prefix = shift(@tmpl);
            my $key = shift(@tmpl);
            my $desc = shift(@tmpl);

            my $class = $prefix . OPTCLASS_SUFFIX;
            my $spec = $class->new($key, $desc, @tmpl);
            push(@list, $spec);

            if ($spec->isa('InstOptSpec') and $key ne 'prefix') {
                my $instkey = ($spec->attribute('relative'))
                        ? '_INSTRELOPTS' : '_INSTOPTS';

                        $me-> {
                            $instkey
                        }
                        -> {
                            $key
                        }
                        = $spec;
            }
            &main::fatal("Option key conflicts: $key")
            if ($opts-> {
                    $key
                });
            $opts-> {
                $key
            }
            = $spec;
            $spec->setSpec(\ % map);
        }

        (@SPEC_LIST) = (@list);

        $map
        {help}
        = sub
        { $me->printHelp();};
        $map
        {'recheck=s'}
        = sub
        {
            my($k, $v) = @_;
            $me-> {
                _RECHECK
            }
            = $v;
        };

#Special hack for UNC build environment.
        $map
        {'enable-unc'}
        = sub
        {
            $me-> {
                _UNC
            }
            = 1;
        };
        $map
        {'with-product-name=s'}
        = sub
        {
            my($k, $v) = @_;

            $main::PFC_PRODUCT_NAME = $v;
        };
        $map
        {listspec}
        = sub
        {
            print join("\n", keys( % map));
            exit 0;
        };
        $map
        {'scan-jdk'}
        = sub
        {
            $me-> {
                _SCAN_JDK
            }
            = 1;
        };

        return \ % map;
    }

    sub getSpec{
        my $me = shift;
        my($key) = @_;

        return $me-> {
            _OPTS
        }
        -> {
            $key
        };
    }

    sub usage{
        print << OUT;

        Usage : $0 [options]
        OUT

        exit 1;
    }

    sub printHelp{
        print << OUT;
        Usage : $0 [options]

        Options :
        OUT
        my $lbrk = LineBreak->new(\*STDOUT);
        foreach my $spec(@SPEC_LIST)
        {
            $spec->printHelp($lbrk);
            $lbrk->newLine(0, 2);
        }

        print "\nSupported OS types:\n    ";
        my $comma;
        foreach my $os(sort(keys( % main::OSTYPE)))
        {
            print $comma, $os;
            $comma = ', ';
        }

        print "\n\nSupported CPU architectures:\n    ";
        undef $comma;
        foreach my $arch(sort(keys( % main::ARCH)))
        {
            print $comma, $arch;
            $comma = ', ';
        }

        print "\n";

        exit 0;
    }

    sub fixup{
        my $me = shift;

        my $prefix = $me->get('prefix');
        while (my($key, $spec) = each( %{$me-> {
                    _INSTOPTS
                }})) {
            $spec->resolvePath($me, $prefix);
        }
        while (my($key, $spec) = each( %{$me-> {
                    _INSTRELOPTS
                }})) {
            $spec->resolvePath($me, $prefix);
        }
    }

    sub get{
        my $me = shift;
        my($key) = @_;

        my $spec = $me->getSpec($key);

        return ($spec) ? $spec->value() : undef;
    }

    sub set{
        my $me = shift;
        my($key, $value) = @_;

        $me->getSpec($key)->value($value);
    }

    sub getInstDirKeys{
        my $me = shift;

        my(@list) = (keys( %
        {$me-> {
                _INSTOPTS
            }}),
        keys( %
        {$me-> {
                _INSTRELOPTS
            }}));
        return (wantarray) ? @list : \@list;
    }

    sub getDescription{
        my $me = shift;
        my($key) = @_;

        return $me->getSpec($key)->getDescription();
    }

    sub getMap{
        my $me = shift;
        my($key) = @_;

        my $spec = $me->getSpec($key);
        if ($spec and $spec->can('getMap')) {
            return $spec->getMap();
        }

        return undef;
    }

    sub recheck{
        my $me = shift;

        return $me-> {
            _RECHECK
        };
    }

    sub isUNC{
        my $me = shift;

        return $me-> {
            _UNC
        };
    }

    sub isScanJDK{
        my $me = shift;

        return $me-> {
            _SCAN_JDK
        };
    }
}

MAIN :{
    my $intr = sub
    {
        print "\n\nInterrupted.\n";
        exit 3;
    };
    $SIG
    {INT}
    = $intr;
    $SIG
    {TERM}
    = $intr;

#Update umask if it masks permission for current user.
    my $um = umask;
    if (($um & 0700) != 0) {
        $um &= ~0700;
                umask($um);
    }

#Preserve arguments.
    my(@argv_saved) = (@ARGV);

#Parse options.
    my $conf = Configure->new;

    my $recheck = $conf->recheck();
    if ($recheck) {
#Use arguments preserved in config.status.
        parse_config_status($recheck);

        if (@ARGV) {
            print "-- rechecking with arguments:\n";
            for (my $i = 0; $i <= $#ARGV; $i++) {
                    print "   $i: $ARGV[$i]\n";
            }
    } else {
        print "-- rechecking with default configuration.\n";
    }

    (@argv_saved) = (@ARGV);
    $conf = Configure->new;
}

my $os = $conf->get('os');
        my $arch = $conf->get('arch');
        error("Invalid OS type: $os") if (defined($os) and !$OSTYPE{$os});
    error("Invalid CPU architecture: $arch")
    if (defined($arch) and !$ARCH{$arch});

        if ($conf->isScanJDK()) {
            my $jhome = scan_jdk($conf, $arch);
                    print $jhome, "\n"

                                       if ($jhome);

                    exit 0;
            }

#Set up additional Java library search path.
setup_java_libdir($conf);

        my $root = check_srcroot();
        my $prefix = $conf->get('prefix');

#Check whether mandatory commands are installed or not.
        foreach my $key(sort(keys( % COMMAND_SPEC))) {
    my $spec = $COMMAND_SPEC{$key};

    eval{
        verify_command($key, % $spec);
    };
    if ($@) {
        my $err = "$@";
                my $handler = $spec-> {
                    IGNORE_ERROR
                };
        if ($handler) {
            &$handler() if (ref($handler) eq 'CODE');
            } else {
            chomp($err);
                    error($err);
        }
    }
}

my $gcc = $COMMAND_SPEC{gcc}
-> {
    PATH
};
my $gxx = $COMMAND_SPEC{gxx}
-> {
PATH
};

my($multiarch);
        ($os, $arch, $multiarch) = check_os_arch($os, $arch, $gcc, $gxx);
        my($config_os, $config_arch) = ($os, $arch);

        check_cc_search_path($conf);

        undef $CXX_STD unless(check_gcc_option($gxx, $CXX_STD,
        $CXX_MODE, undef, []));

#Determine compiler options for strict source check.
        my $errwarn = ($conf->get('errwarn')) ? '-Werror' : undef;
        foreach my $opts(GCC_WARN_OPTS, $errwarn) {
    next unless($opts);
            push(@CC_WARN, $opts)
    if (check_gcc_option($gcc, $opts, $CC_MODE, undef,
            \@CC_WARN));
            push(@CXX_WARN, $opts)
        if (check_gcc_option($gxx, $opts, $CXX_MODE, undef,
                \@CXX_WARN));
        }

my($gcc_alias, $gxx_alias);
        $gcc_alias = GCC_NO_STRICT_ALIAS
if (check_gcc_option($gcc, GCC_NO_STRICT_ALIAS, $CC_MODE,
        undef, []));
        $gxx_alias = GCC_NO_STRICT_ALIAS
    if (check_gcc_option($gxx, GCC_NO_STRICT_ALIAS, $CXX_MODE,
            undef, []));
            check_strict_aliasing($conf, $gcc, $gcc_alias, @CC_WARN)
        if ($gcc_alias);
                check_strict_aliasing($conf, $gxx, $gxx_alias, @CXX_WARN)
            if ($gxx_alias);

                    check_gcc_ld($gcc);
                    check_gcc_ld($gxx);

                    my $perl = $conf->get('perl');
                    check_perl($perl);

                    my(@hdefs);
                    my $boost_var = check_boost($conf);
                    my $openssl_var = check_openssl($conf, @hdefs);
                    my $runpath = check_runpath($conf);

                    my $cfdef_arch = $arch;
                    my $carch = COMMON_ARCH-> {
                        $arch
                    };
my $subarch;
if ($carch) {

    $subarch = $arch;
            $arch = $carch;
}

my $ldlibdir = $conf->get('ldlibdir');
        unless($ldlibdir) {
    $ldlibdir = $conf->get('objdir') . '/ldlibs';
}

my $make = $COMMAND_SPEC{make}
-> {
    PATH
};
my( % vars) = (PFC_PRODUCT_NAME = > $PFC_PRODUCT_NAME,
        PREFIX = > $prefix,
        CC = > $gcc,
        CC_OPT = > $C_OPT,
        CC_DEBUG = > $C_DEBUG,
        CC_WARN = > join(' ', @CC_WARN),
        CC_MODE = > join(' ', @$CC_MODE),
        CC_NO_ALIAS = > $gcc_alias,
        CXX = > $gxx,
        CXX_OPT = > $CXX_OPT,
        CXX_DEBUG = > $CXX_DEBUG,
        CXX_WARN = > join(' ', @CXX_WARN),
        CXX_MODE = > join(' ', @$CXX_MODE),
        CXX_NO_ALIAS = > $gxx_alias,
        CONFIG_OS = > $config_os,
        CONFIG_ARCH = > $config_arch,
        CPPFLAGS_ALL = > join(' ', @{$conf->get('cppflags')}),
DEFAULT_LIBPATH = > join(' ', @DEFAULT_LIBPATH),
        PATH_MAKE = > $make,
        PERL = > $perl,
        SRCROOT = > $root,
        OBJROOT = > $conf->get('objdir'),
        LINK_LIBDIR = > $ldlibdir,
        BLDDIR = > $root . '/' . BUILD_DIR,
        TOOLS_DIR = > $root . '/' . TOOLS_DIR,
        TOOLBIN = > $root . '/' . TOOLBIN,
        SHELL_PATH = > $SHELL_PATH,
        PATH_SCRIPT = > $PATH_SCRIPT,
        OSTYPE = > $os,
        ARCH = > $arch,
        MULTIARCH = > $multiarch);

        $vars{CXX_STD}
= $CXX_STD if (defined($CXX_STD));

        $vars{CC_INCDIRS_FIRST}
    = join(' ', @CC_INCDIRS_FIRST)
    if (@CC_INCDIRS_FIRST);
            $vars{LD_LIBDIRS_FIRST}
        = join(' ', @LD_LIBDIRS_FIRST)
        if (@LD_LIBDIRS_FIRST);

                $PFC_BUILD_ID = $conf->get('build-id');
            if (length($PFC_BUILD_ID) != 0) {
                $vars{PFC_BUILD_ID}
                = $PFC_BUILD_ID;
            } else {
                undef $PFC_BUILD_ID;
            }

$PFC_SCM_REVISION = $conf->get('scm-revision');
if (length($PFC_SCM_REVISION) != 0) {
    $vars{PFC_SCM_REVISION}
    = $PFC_SCM_REVISION;
} else {
    undef $PFC_SCM_REVISION;
}

my @modsrcdir = ($root . '/' . MODULES_DIR);
        my $pl_srcroot = $root;
        my $pl_objroot = $vars{OBJROOT};
if ($conf->isUNC()) {
    $vars{UNC_CORE}
    = 1;
            $vars{IPC_TMPL_SRCDIR}
    = abs_path('../ipc');

            my $objdir = $vars{OBJROOT};
    my $pat = qr, / [^ / ] + $,;
            $pl_srcroot = ~s, $pat,,;
            $pl_objroot = ~s, $pat,,;
            $vars{UNC_CORE_INCLUDE}
    = $pl_objroot . '/core_include';
            $vars{UNC_SRCROOT}
    = $pl_srcroot;
            $vars{UNC_OBJROOT}
    = $pl_objroot;
            $vars{UNC_INCDIRS}
    = $pl_objroot . '/include ' .
            $pl_srcroot . '/include';

            my $moddir = abs_path($root . '/../' . MODULES_DIR);
            push(@modsrcdir, $moddir);
}

$vars{SUBARCH}
= $subarch if (defined($subarch));
        $vars{DEBUG_BUILD}
    = 1 if ($conf->get('debug'));
        if (isLP64($cfdef_arch)) {
            $vars{CFDEF_MODE}
            = '-L';
                    $vars{PFC_LP64}
            = 1;
        }
$vars{EXTERNAL_RUNPATH}
= $runpath if ($runpath);

        my(@newdirs) = ($ldlibdir);

        my $jenv = check_java( % vars, $conf);
    if ($jenv) {
        $vars{JAVA_CONFIG_MK}
        = $vars{OBJROOT}
        . '/' . JAVA_CONFIG_MK;
                my $jobjs = $jenv-> {
                    JAVA_OBJDIR
                };
        push(@newdirs, $jobjs, "$jobjs/ext");

                my $jnidir = $ldlibdir . '/jni';
                $vars{LINK_JNILIBDIR}
        = $jnidir;
                push(@newdirs, $jnidir);
    }

while (my($k, $v) = each( % $openssl_var)) {

    $vars{$k}
    = $v unless(ref($v));
}

#Check system configuration.
check_cflags_tests();
        check_config_tests(@hdefs, $os, % vars);

#Check for Google Test.
        unless(check_gtest($conf)) {
    warning("Unit test may be disabled");
            delete($COMMAND_SPEC{gtest_config}
    -> {
                PATH
            });
}

#Call OS specific configuration subroutine.
my $osconf = $OS_CONF{$os};
&$osconf($conf, \ % vars) if ($osconf);

#Check for additional command search path.
        check_extcmd_dirs($conf, @hdefs);

#Define make macros and CPP constants that represent additional
#features.
        feature_setup( % vars, @hdefs, $conf);

#Define OS-specific make macros.
        if (defined(my $osdep = $OSDEP{$os})) {
        while (my($k, $v) = each( % $osdep)) {
            $v = join(' ', @$v) if (ref($v) eq 'ARRAY');
                    $vars{$k}
                = $v;
            }
    }

while (my($k, $v) = each( % $boost_var)) {

    $vars{$k}
    = $v;
}

check_additional_vars($conf, % vars);
        check_pfc_version($conf);

        print "\n";
        prepare( % vars, @newdirs);
        foreach my $d(@newdirs) {

    unless(-d $d) {
        print "== creating $d\n";
                mkpath($d, undef, 0755);
    }
}
create_config_mk($conf, % vars);

        my( % plvars) = (SRCROOT = > $pl_srcroot,
        OBJROOT = > $pl_objroot,
        MODULE_SRCDIR = > \@modsrcdir);
        create_config_pl( % plvars);
        create_config_h($conf, $prefix, @hdefs);
        create_openssl_const_h($conf, % $openssl_var);
        create_java_config_mk($vars{JAVA_CONFIG_MK}, % $jenv)

                                                              if ($jenv);
        create_gtest_config();
        create_config_status(@argv_saved, % vars);
        setup($make);

        print "\nType \"$make\" to build.\n" unless($conf->isUNC());
}

sub fatal(@) {

    STDOUT->flush();
            print STDERR "\n*** FATAL ERROR: ", @_, "\n";
            exit 2;
}

sub error(@) {

    STDOUT->flush();
            print STDERR "\n*** ERROR: ", @_, "\n";
            exit 1;
}

sub warning(@) {

    STDOUT->flush();
            print STDERR "\n*** WARNING: ", @_, "\n\n";
}

sub dupfd($$) {

    my($oldfd, $newfd) = @_;

            POSIX::dup2($oldfd, $newfd) or fatal("dup2() failed: $!");
}

sub checking(@) {

    print "-- checking ", @_, "... ";
            STDOUT->flush();
}

sub setup_java_libdir($) {
    my($conf) = @_;

            my $dirs = $conf->get('java-libdir');
    if ($dirs) {

        foreach my $d(@$dirs) {
            push(@JAVA_LIBRARY_PATH, $d)

                                         if (-d $d);
            }
    }

    foreach my $dir(qw( / usr / share / java / usr / lib / java
            / usr / local / lib / java)) {
        push(@JAVA_LIBRARY_PATH, $dir)

                                       if (-d $dir);
        }
}

sub exec_command(\ % @) {
    my($opts, @args) = @_;

            my($rh, $wh);
            my $filter = $opts-> {
                FILTER
            };
    if ($filter) {
        ($rh, $wh) = FileHandle::pipe() or fatal("pipe() failed: $!");
    }

    my $pid = fork();
    if (!defined($pid)) {

        fatal("fork() failed: $!");
    }
    elsif($pid == 0) {
        if ($filter) {
#Create pipe to send output to filter.
            undef $rh;
                    $wh->autoflush(1);
                    my $fd = $opts-> {
                        FILTER_FD
                    };
            $fd = STDOUT->fileno() unless(defined($fd));
                    dupfd($wh->fileno(), $fd);
        }
        elsif(my $redirect = $opts-> {
REDIRECT
        }) {

            my $dir = dirname($redirect);
                    unless(-d $dir) {
                mkpath($dir, undef, 0755);
            }

            my $omode = O_CREAT | O_TRUNC | O_WRONLY;
                    my $out = FileHandle->new($redirect, $omode, 0644) or
                    fatal("open($redirect) failed: $!");
                    dupfd($out->fileno(), STDOUT->fileno());
        }
        if ($opts-> {
                ERR2OUT
            }) {
        dupfd(STDOUT->fileno(), STDERR->fileno());
    }
        elsif(my $errout = $opts-> {
ERR_REDIRECT
        }) {
            my $err;
            if (ref($errout)) {
                $err = $errout;
            } else {
                my $omode = O_CREAT | O_TRUNC | O_WRONLY;
                        $err = FileHandle->new($errout, $omode, 0644)
                        or fatal("open($errout) failed: $!");
            }
            dupfd($err->fileno(), STDERR->fileno());
        }

        exec(@args);
                exit 1;
    }

    if ($filter) {
#Call filter function.
        undef $wh;
                &$filter($rh);
                undef $rh;
    }

    my $cpid;
    do {
        $cpid = waitpid($pid, 0);
    } while ($cpid == -1 and $! == EINTR);
            fatal("waitpid($pid) failed: $!") if ($cpid == -1);

                my $status = $ ?;
            if (WIFEXITED($status)) {

                return WEXITSTATUS($status);
            }
    elsif(WIFSIGNALED($status)) {
        my $sig = WTERMSIG($status);
        return -$sig;
    } else {

        return -1;
    }
}

sub verify_command($\ %) {
    my($key, $spec) = @_;

            my $desc = $spec-> {
                DESC
            }
            || $key;
            checking("for $desc");

            my $names = $spec-> {
                NAMES
            };
    my $list = ($names) ? $names : [$key];
            my $pattern = [];
            foreach my $pat(@{$spec-> {
PATTERN
            }}) {
        push(@$pattern, qr, $pat,);
    }
    my $npatterns = scalar(@$pattern);

            my $found;
            my $checkfd = $spec-> {
                CHECKFD
            };
    my $pathlist = $spec-> {
        PATH
    }
    || \@PATH;

    if (my $abspath = $spec-> {
            ABSPATH
        }) {

    my($name, $dir) = fileparse($abspath);
            $dir = ~s, / +$,, go;
            $list = [$name];
            $pathlist = [$dir];
}
    foreach my $c(@$list) {

        my $cmd;
                foreach my $dir(@$pathlist) {
            my $cpath = "$dir/$c";
            if (-x $cpath) {

                $cmd = $cpath;
                        last;
            }
        }
        next unless($cmd);
                unless($npatterns) {
            $found = $cmd;
                    last;
        }

        my $matched = 0;
                my $filter = sub{
            my($rh) = @_;

            while (my $line = $rh->getline()) {
                next unless($matched < $npatterns);

                        my @newpat;
                while (my $pat = shift(@$pattern)) {
                    if ($line = ~$pat) {
                        $matched++;
                                push(@newpat, @$pattern);
                                last;
                    } else {
                        push(@newpat, $pat);
                    }
                }
                $pattern = \@newpat;
            }
        };

        my( % opts) = (FILTER = > $filter, FILTER_FD = > $checkfd);
                my $ret = exec_command( % opts, $cmd, @{$spec-> {
                    ARG
                }});
        next unless($ret == 0);

        if ($matched == $npatterns) {

            $found = $cmd;
                    last;
        }
    }

    unless($found) {
        print "not found\n";
                my $msg;
        if (my $p = $spec-> {
                ABSPATH
            }) {
        $msg = "$desc ($p) is not found.";
    } else {
            $msg = "$desc is not found.";
        }
        die "$msg\n";
    }

    print $found, "\n";
            my $verify = $spec-> {
                VERIFY
            };
    &$verify($found, $spec) if ($verify);
            $spec-> {
PATH
            }
            = $found;
}

sub canonicalize_path($) {
    my($path) = @_;

            $path = ~s, / +, /, go;
            $path = ~s, / +$,, o;

            my(@comp) = split('/', $path);
            my(@newcomp);
            my $root;
    if ($comp[0] eq '') {

        shift(@comp);
                $root = '/';
    }

    foreach my $c(@comp) {
        next if ($c eq '.');
            if ($c eq '..') {
                pop(@newcomp);
            } else {

                push(@newcomp, $c);
            }
    }

    return $root . join('/', @newcomp);
}

sub stringify($) {
    my($path) = @_;

            $path = ~s, \x22, \\\x22, g;

    return '"' . $path . '"';
}

sub isLP64($) {
    my($arch) = @_;

    return ($ARCH{$arch}
    eq MODEL_LP64());
}

sub create_year_range() {

    unless(defined($YEAR_RANGE)) {
        my(@tm) = localtime(time);
                $YEAR_RANGE = $tm[5] + 1900;
                $YEAR_RANGE = OLDEST_YEAR . '-' . $YEAR_RANGE

        if ($YEAR_RANGE > OLDEST_YEAR);

        }

    return $YEAR_RANGE;
}

sub create_dynamic_header($$) {

    my($conf, $relpath) = @_;

            my $yrange = create_year_range();
            my $path = $conf->get('objdir') . '/' . $relpath;
            my $parent = dirname($path);
            unless(-d $parent) {
        mkpath($parent, undef, 0755);
    }

    print "== creating $path\n";
            my $fh = FileHandle->new($path, O_CREAT | O_TRUNC | O_WRONLY, 0644) or
            fatal("open($path) failed: $!");
            push(@UNLINK_ON_ERROR, $path);

            $fh->print( << OUT);
            /*
             * Copyright (c) $yrange NEC Corporation
             * All rights reserved.
             * 
             * This program and the accompanying materials are made available under the
             * terms of the Eclipse Public License v1.0 which accompanies this
             * distribution, and is available at http://www.eclipse.org/legal/epl-v10.html
             */

            OUT

    return $fh;
}

sub check_runpath($) {

    my($conf) = @_;

            checking("for RUNPATH");

            my $spec = $conf->getSpec('runpath');
            my $list = $spec->value();
            unless(@$list) {
        print "none specified\n";

        return undef;
    }

    my @runpath;
            foreach my $p(@$list) {

        my(@rpath) = split( / : /, $p);
                foreach my $path(@rpath) {
            next

                 if ($path eq VALUE_NONE);

                    unless(substr($path, 0, 1) eq '/') {
                    print "error\n";
                            error($spec->getOption(),
                            ': Value must be an absolute path: ',
                            $path);
                }
            push(@runpath, canonicalize_path($path));
        }
    }

    my $ret = join(' ', @runpath);
            print "$ret\n";

    return $ret;
}

sub check_additional_vars($\ %) {

    my($conf, $vars) = @_;

            my $spec = $conf->getSpec('make-variable');
            my $map = $spec->getMap();
            my $keyword = qr, % % ([A - Za - z\d_]+) % %,;
            foreach my $v(sort keys( % $map)) {
        checking("for $v");

#Ensure that the variable name is valid.
                if ($MAKEVAR_RESV{$v}
            or $vars-> {
                $v
            }) {
        print "failed\n";
                my $opt = $spec->getOption();
                error("$opt: Variable \"$v\" is reserved.");
    }

#Eliminates leading and trailing whitespaces in value.
        my $value = $map-> {
            $v
        };
        $value = ~s, ^\s * (.* ?)\s*$, $1, o;

#Treat "%%VAR%%" as make variable dereference.
                $value = ~s, $keyword, $vars-> {
            $1
        }, g;

                print $value, "\n";
                $vars-> {
$v
                }
                = $value;
    }
}

sub check_extcmd_dirs($\@) {

    my($conf, $hdefs) = @_;

            checking("for additional external command path");
            my $value = $conf->get('extcmd-dir');
            unless($value) {
        print "none\n";

        return;
    }

    print $value, "\n";
            push(@$hdefs, ConfDef->new('PFC_EXTCMD_BINDIR', stringify($value),
            'Additional command search path which ' .
            'contains external commands.'));
}

sub check_pfc_version($) {
    my($conf) = @_;

            checking("for PFC software version");
            my $value = $conf->get('version-file');
    if (defined($value)) {

        my $fh = FileHandle->new($value);
                unless($fh) {
            print "failed\n";
                    error("Failed to open \"$value\": $!");
        }

        my $pat = qr, ^(\d +)\.(\d +)\.(\d +)(\.(\d +)) ? (-.+) ? $,;
                my($maj, $min, $rev, $patch, $vstr);
        while (my $line = $fh->getline()) {
            if ($line = ~$pat) {
                (
$maj, $min, $rev, $patch) =
                        ($1, $2, $3, $5);
                        chomp($line);
                        $vstr = $line;
                        last;
            }
        }

        $patch = 0 unless(defined($patch));
                unless(defined($maj) and defined($min) and defined($rev)) {

            print "failed\n";
                    error("Failed to determine software version from ",
                    "\"$value\".");
        }

        $PFC_VERSION = join('.', $maj, $min, $rev, $patch);
                $PFC_VERSION_MAJOR = $maj;
                $PFC_VERSION_MINOR = $min;
                $PFC_VERSION_REVISION = $rev;
                $PFC_VERSION_PATCHLEVEL = $patch;
                $PFC_VERSION_STRING = $vstr;
    }

    my $bid = $PFC_BUILD_ID;
            my $scm = $PFC_SCM_REVISION;
            $bid = '(none)' unless(defined($bid));
            $scm = '(none)' unless(defined($scm));

            print $PFC_VERSION, "\n";
            print << OUT;
            PFC_VERSION = $PFC_VERSION
            PFC_VERSION_MAJOR = $PFC_VERSION_MAJOR
            PFC_VERSION_MINOR = $PFC_VERSION_MINOR
            PFC_VERSION_REVISION = $PFC_VERSION_REVISION
            PFC_VERSION_PATCHLEVEL = $PFC_VERSION_PATCHLEVEL
            PFC_VERSION_STRING = $PFC_VERSION_STRING
            PFC_BUILD_ID = $bid
            PFC_SCM_REVISION = $scm
            OUT
}

sub check_srcroot() {
    my $dir = dirname($0);

            my $dirst = stat($dir) or fatal("stat($dir) failed: $!");
            my $curst = stat('.') or fatal("stat(.) failed: $!");

            fatal("\"configure\" script must be executed under ",
            "source tree root.")
            unless($dirst->dev == $curst->dev and
            $dirst->ino == $curst->ino);

            my $root = abs_path($dir) or
            fatal("Failed to determine source tree root: $dir");

    return $root;
}

sub check_make($$) {
    my($make, $spec) = @_;

            checking("for GNU make version");

            my $version;
            my $filter = sub{
        my($rh) = @_;

        my $pattern = qr, GNU Make\s + ([\d.]+),;
        while (my $line = $rh->getline()) {
            if (!defined($version) and $line = ~$pattern) {

                $version = $1;
            }
        }
    };

    my( % opts) = (FILTER = > $filter);
            my $ret = exec_command( % opts, $make, '--version');
            unless($ret == 0) {

        print "unknown error\n";

                die "\"$make --version\" failed.\n";
    }

    unless(defined($version)) {
        print "unknown\n";

                die "Failed to detect GNU make version.\n";
    }

    print "$version\n";
            die "GNU make 3.81 or later is required.\n"

                                                        if ($version < 3.81);
    }

sub check_gcc($$) {
    my($gcc, $spec) = @_;

            my($gcc_arg, @c_opts);
    {

        my(@args) = ($gcc);
                foreach my $o($C_DEBUG, $C_OPT) {
            if ($o) {

                push(@args, $o);
                        push(@c_opts, split( / \s + /, $o));
            }
        }
        $gcc_arg = join(' ', @args);
    }
    checking("whether $gcc_arg works");
            my $src = 'conftest.c';
            my $fh = TempFile->new($src, O_CREAT | O_TRUNC | O_WRONLY, 0644) or
            fatal("open($src) failed: $!");

            $fh->print( << 'OUT');
#include <stdio.h>

            int
            main(int argc, char **argv) {
        printf("Hello World!\n");

        return 0;
    }
    OUT

    $fh->close();

            my $output = 'conftest';
            my % opts;
            my(@args) = ($gcc, @c_opts, '-o', $output);
            my $ret = exec_command( % opts, @args, $src);
            unless($ret == 0) {

        print "no\n";
                error("Compilation failed.");
    }

    unless(-f $output) {

        print "no\n";
                error("$gcc_arg didn't generate object file: $output");
    }

    print "yes\n";
            unlink($output);
}

sub check_gxx($$) {
    my($gxx, $spec) = @_;

            my($gxx_arg, @cxx_opts);
    {

        my(@args) = ($gxx);
                foreach my $o($CXX_DEBUG, $CXX_OPT) {
            if ($o) {

                push(@args, $o);
                        push(@cxx_opts, split( / \s + /, $o));
            }
        }
        $gxx_arg = join(' ', @args);
    }
    checking("whether $gxx_arg works");
            my $src = 'conftest.cc';
            my $fh = TempFile->new($src, O_CREAT | O_TRUNC | O_WRONLY, 0644) or
            fatal("open($src) failed: $!");

            $fh->print( << 'OUT');
#include <iostream>

            using namespace std;

            int
            main(int argc, char **argv) {
        cout << "Hello World!" << endl;

        return 0;
    }
    OUT

    $fh->close();

            my $output = 'conftest';
            my % opts;
            my(@args) = ($gxx, @cxx_opts, '-o', $output);
            my $ret = exec_command( % opts, @args, $src);
            unless($ret == 0) {

        print "no\n";
                error("Compilation failed.");
    }

    unless(-f $output) {

        print "no\n";
                error("$gxx_arg didn't generate object file: $output");
    }

    print "yes\n";
            unlink($output);
}

sub check_gtest($) {
    my($conf) = @_;

            my $gtspec = $COMMAND_SPEC{gtest_config};
    my $gtconfig = $gtspec-> {
        PATH
    };
    my(@gtconfig_args, $stub);
    if ($gtconfig) {
        return undef unless(check_gtest_config($gtconfig));
                (@gtconfig_args) = ($gtconfig);
    } else {
#Use gtest-config stub which assumes that the Google Test is
#installed in standard location.
        (@gtconfig_args) = ('/bin/sh', STUB_GTEST_CONFIG);
                $stub = $conf->get('objdir') . '/gtest-config';
    }

    checking("whether Google Test works");
            my( % opts, $gtest_inc, $gtest_cxxflags, $gtest_libdir, $gtest_libs);
            eval{

        $gtest_inc = read_command_oneline( % opts, @gtconfig_args,
        '--includedir');
        $gtest_cxxflags = read_command_oneline( % opts, @gtconfig_args,
        '--cxxflags');
        $gtest_libdir = read_command_oneline( % opts, @gtconfig_args,
        '--libdir');
        $gtest_libs = read_command_oneline( % opts, @gtconfig_args,
        '--libs');
    };
    if ($@) {
        my $err = "$@";
                chomp($err);
                print "no ($err)\n";
                my $out = $opts{ERR_OUTPUT};
        print "\n", @$out, "\n" if ($out);

            return undef;
        }

#Construct compiler options.
    my(@cppflags, @ldflags);
            my $space = qr, \s +,;

            foreach my $dir(split($space, $gtest_inc)) {

        push(@cppflags, '-I' . $dir)
        unless(is_default_header_path($dir));
    }

    foreach my $dir(split($space, $gtest_libdir)) {

        unless(is_default_library_path($dir)) {

            push(@ldflags, '-L' . $dir, '-Wl,-rpath,' . $dir);
        }
    }

    my(@cxxflags) = (@$CXX_MODE, split($space, $gtest_cxxflags));
            my(@libs) = (split($space, $gtest_libs), '-lgtest_main');

            my $code = << 'OUT';
#include <unistd.h>
#include <gtest/gtest.h>

            TEST(foo, test1) {

        ASSERT_NE(static_cast<pid_t> (-1), getpid());
    }

    TEST(foo, test2) {
        EXPECT_NE(static_cast<gid_t> (-1), getgid());
    }
    OUT

    my @errout;
            my $gxx = $COMMAND_SPEC{gxx}
    -> {
                PATH
            };
    ( % opts) = (CC = > $gxx, LIBS = > \@libs);
            $opts{CFLAGS}
    = \@cxxflags if (@cxxflags);
            $opts{CPPFLAGS}
        = \@cppflags if (@cppflags);
                $opts{LDFLAGS}
            = \@ldflags if (@ldflags);
                    $opts{CC_ERROR}
                = \@errout;

                    my $out;
                    eval{
                    $out = compile_and_exec($code, % opts);
                };
                if ($@) {
                    print "no (compilation failed)\n";
                            print "\n", @errout, "\n" if (@errout);

                        return undef;
                    }

    my $pattern = qr, \[\s * PASSED\s*\]\s * 2\s + tests\.,;
            foreach my $line(@$out) {
        if ($line = ~$pattern) {
            print "yes\n";
            if ($stub) {
                $gtspec-> {
                    STUB
                }
                = 1;
                        $gtspec-> {
                            PATH
                        }
                        = $stub;
            }
            return 1;
        }
    }

    print "no (test did not work)\n";

    return undef;
}

sub check_gtest_config($) {

    my($gtconfig) = (@_);

            checking("for Google Test version");

            my $version;
            my $filter = sub{
        my($rh) = @_;

        $version = $rh->getline();
        chomp($version);
    };

    my( % opts) = (FILTER = > $filter);
            my $ret = exec_command( % opts, $gtconfig, '--version');
            unless($ret == 0 and defined($version)) {
        print "unknown version\n";

        return undef;
    }
    print $version, "\n";

            ( % opts) = ();
            $ret = exec_command( % opts, $gtconfig,
            '--min-version=' . GTEST_VERSION);
            unless($ret == 0) {
        warning("Google Test is too old.");

        return undef;
    }

    return 1;
}

sub compile_and_exec($ %) {

    my($source, % args) = @_;

            my $cc = $args{CC};
    my $cppflags = $args{CPPFLAGS};
    my $cflags = $args{CFLAGS};
    my $ldflags = $args{LDFLAGS};
    my $libs = $args{LIBS};
    my $src = 'conftest.c';
            my $fh = TempFile->new($src, O_CREAT | O_TRUNC | O_WRONLY, 0644) or
            fatal("open($src) failed: $!");

            $fh->print($source);
            $fh->close();

            my $output = 'conftest';
            my % opts;
            my(@args) = ($cc, '-o', $output);
            foreach my $a($cppflags, $cflags, $src, $ldflags, $libs) {
        if ($a) {
            if (ref($a) eq 'ARRAY') {
                push(@args, @$a);
            } else {
                push(@args, $a);
            }
        }
    }

    my $ccerr = $args{CC_ERROR};
    if ($ccerr) {
        push(@$ccerr, "Command: ", join(" ", @args), "\nOutput:\n");
                my $filter = sub{
            my($rh) = @_;

            while (<$rh>) {
                push(@$ccerr, $_);
            }
        };
        ( % opts) = (FILTER = > $filter, ERR2OUT = > 1);
    } else {
        (
% opts) = (REDIRECT = > '/dev/null', ERR2OUT = > 1);
    }
    my $ret = exec_command( % opts, @args);
            die "Compilation failed.\n" unless($ret == 0);
            die "$cc didn't generate object file: $output\n" unless(-f $output);

            my(@out);
            unless($args{CC_ONLY}) {
        my $filter = sub{
            my($rh) = @_;

            while (<$rh>) {

                push(@out, $_);
            }
        };
        ( % opts) = (FILTER = > $filter, ERR2OUT = > 1);
                my $ret = exec_command( % opts, './' . $output);
                unlink($output);

                die "Command failed: $ret\n" unless($ret == 0);
    }

    return \@out;
}

sub check_perl($) {

    my($perl) = @_;

            checking("whether perl ($perl) works");

#Perl v5.8 or later is required.
            my(@err);
            my $filter = sub{
        my($rh) = @_;

        foreach my $line(<$rh>)
        {
            push(@err, "PERL OUTPUT: " . $line);
        }
    };

    my( % opts) = (ERR2OUT = > 1, FILTER = > $filter);
            my $ret = exec_command( % opts, $perl, '-e', 'use 5.008;');

            unless($ret == 0) {

        print "no\n\n";
                print STDERR @err, "\n";
                error("$perl didn't work.");
    }

    print "yes\n";
}

sub check_boost($) {
    my($conf) = @_;

            my $spec = $conf->getSpec('boost');
            my $boost = $spec->value();

            my(@pfx, $user);
    if ($boost) {
        error($spec->getOption(), ": Directory not found: ", $boost)
        unless(-d $boost);
                $boost = canonicalize_path($boost);
                (@pfx) = ($boost);
                $user = $boost;
    } else {
        (
@pfx) = qw( / usr / usr / local / usr / local / boost);
    }

    my $incdir = check_header('boost/version.hpp', @pfx);
            unless($incdir) {
        if ($user) {
            error($spec->getOption(),
                    ": boost is not found under $user.");
        } else {
            error("boost is not found.");
        }
    }

    undef $incdir if (is_default_header_path($incdir));

            my $gxx = $COMMAND_SPEC{gxx}
        -> {
                PATH
            };
    my( % opts) = (CC = > $gxx, CFLAGS = > $CXX_MODE, PREFIX = > \@pfx);

            my(@cppflags);
            push(@cppflags, '-I' . $incdir)

                                            if ($incdir);

            checking("for boost version");
            my $source = << OUT;
#include <iostream>
#include <boost/version.hpp>

            using namespace std;

            int
            main(int argc, char **argv) {
            cout << BOOST_VERSION << endl;
            return 0;
        }
    OUT

            ( % opts) = (CC = > $gxx, CFLAGS = > $CXX_MODE);
            $opts{CPPFLAGS}
    = \@cppflags;

            my @errout;
            $opts{CC_ERROR}
    = \@errout;
            my $out;
            eval{
        $out = compile_and_exec($source, % opts);
    };
    if ($@) {
        print "unknown\n";
        if (@errout) {
            print "\n", @errout, "\n";
        }
        error("Failed to determine boost version.");
    }

    my $line = $out->[0];
            chomp($line);
    if ($line = ~m, ^(\d +)$,) {
        my $version = $1;
                my $ver = format_boost_version($version);
                print "$ver\n";

        if (BOOST_VERSION != 0 and $version < BOOST_VERSION) {
            my $rver = format_boost_version(BOOST_VERSION);
                    error("Boost $rver or later is required.");
        }
    }

    undef $incdir if ($incdir and $INCDIRS_FIRST_MAP{$incdir});

        my( % vars) = (BOOST_INCDIR = > $incdir);
            $vars{BOOST_CPPFLAGS}
        = '-I$(BOOST_INCDIR)' if ($incdir);

            return \ % vars;
        }

sub check_openssl($\@) {
    my($conf, $hdefs) = @_;

            my $spec = $conf->getSpec('openssl');
            my $prefix = $spec->value();

            my(@pfx, $user);
    if ($prefix) {
        error($spec->getOption(), ": Directory not found: ", $prefix)
        unless(-d $prefix);
                $prefix = canonicalize_path($prefix);
                (@pfx) = ($prefix);
                $user = $prefix;
    } else {
        (
@pfx) = qw( / usr / usr / local / ssl / usr / local);
    }

    my $incdir;
            ($incdir, $prefix) = check_header('openssl/opensslv.h', @pfx,
            need_prefix = > 1);
            unless($incdir) {
        if ($user) {
            error($spec->getOption(),
                    ": OpenSSL header is not found under $user.");
        } else {

            error("OpenSSL header is not found.");
        }
    }

    foreach my $head(qw(openssl / crypto.h openssl / evp.h openssl / err.h)) {
        my(@p) = ($incdir);
                my $d = check_file($head, @p);
                error("$head is not found under $incdir.") unless($d);
    }

    undef $incdir if (is_default_header_path($incdir));

            my( % opts) = (PREFIX = > [$prefix]);
            my(@cppflags);
            push(@cppflags, '-I' . $incdir)

                                            if ($incdir);
                $opts{CPPFLAGS}
            = \@cppflags;
                $opts{CODE}
            = << 'OUT';
#include <openssl/crypto.h>

                int
                main(int argc, char **argv) {
                SSLeay();
                return;
            }
    OUT
    my($libname, $libdir) = check_library('crypto', % opts);
            error("OpenSSL libcrypto library is not found.") unless($libdir);
            undef $libdir if (is_default_library_path($libdir));

            my $gcc = $COMMAND_SPEC{gcc}
        -> {
                PATH
            };
    my(@ldflags);
            push(@ldflags, '-L' . $libdir, '-Wl,-rpath,' . $libdir)

                                                                    if ($libdir);
            ( % opts) = (CC = > $gcc, CFLAGS = > $CC_MODE);
            $opts{CPPFLAGS}
        = \@cppflags;
            $opts{LDFLAGS}
        = \@ldflags;
            $opts{LIBS}
        = '-lcrypto';

            foreach my $test(@OPENSSL_TESTS) {
            my @errout;
                    $opts{CC_ERROR}
            = \@errout;
                    my $out;
                    my($checking, $source) = ($test-> {
                        CHECK
                    }, $test-> {
                        CODE
                    });
            my $libs = $test-> {
                LIBS
            }
            || '-lcrypto';
            $opts{LIBS}
            = $libs;

                    checking($checking);
                    eval{
                $out = compile_and_exec($source, % opts);
            };
            if ($@) {
                if ($test-> {
                        IGNORE_ERROR
                    }) {
                $out = ["NO:Compilation failed\n"];
            } else {
                    print "unknown\n";
                    if (@errout) {
                        print "\n", @errout, "\n";
                    }
                    error("OpenSSL test failed: $@");
                }
            }

            my $line = $out->[0];
                    chomp($line);
            if ($line = ~m, ^ERROR : (.+)$,) {
                print "failed\n";

                        error($1);
            }

            my $value;
            if ($line = ~m, ^NO( : (.+)) ? $, o) {
                my $msg = ($2) ? " ($2)" : '';

                        print "no$msg\n";
                        my $mandatory = $test-> {
                            MANDATORY
                        };
                error($mandatory)

                                  if ($mandatory);
                }
            elsif($line = ~m, ^VALUE : (.+)$, o) {

                $value = $1;
                        print $value, "\n";
            }
            elsif($line = ~m, ^OK : (.+)$, o) {
                print $1, "\n";
                        $value = 1;
            } else {
                error("Unexpected output: $line");
            }

            my($name, $desc) = ($test-> {
                DEFINE
            }, $test-> {
                DESC
            });
            push(@$hdefs, ConfDef->new($name, $value, $desc)) if ($name);
            }

    my( % vars) = (OPENSSL_INCDIR = > $incdir, OPENSSL_LIBDIR = > $libdir,
            OPENSSL_PREFIX = > $prefix,
            OPENSSL_CONST = > check_openssl_const( % opts));
            my $runpath = $conf->get('openssl-runpath');
            $runpath = '$(OPENSSL_LIBDIR)' unless($runpath);
            $vars{OPENSSL_RUNPATH}
    = $runpath;
            $MAKEVAR_OP{OPENSSL_RUNPATH}
    = '=';

    return \ % vars;
}

sub check_openssl_const( %) {

    my( % opts) = @_;

            checking("for OpenSSL constants");
            my $source = << 'OUT';
#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#define DEFCONST(name)  printf("%s\t0x%x\n", #name, name)

            int
            main(int argc, char **argv) {
        DEFCONST(SSLEAY_VERSION);

                DEFCONST(BIO_CLOSE);
                DEFCONST(BIO_NOCLOSE);
                DEFCONST(BIO_C_GET_BUF_MEM_PTR);
                DEFCONST(BIO_C_SET_BUF_MEM_EOF_RETURN);
                DEFCONST(BIO_CTRL_EOF);
                DEFCONST(BIO_CTRL_FLUSH);
                DEFCONST(BIO_CTRL_SET_CLOSE);
                DEFCONST(BIO_FLAGS_SHOULD_RETRY);

                DEFCONST(EVP_MAX_KEY_LENGTH);
                DEFCONST(EVP_MAX_IV_LENGTH);

                DEFCONST(CRYPTO_LOCK);
                DEFCONST(CRYPTO_UNLOCK);

        return 0;
    }
    OUT

    my($out, @errout);
            $opts{CC_ERROR}
    = \@errout;
            delete($opts{LIBS});
    delete($opts{LDFLAGS});

    eval{
        $out = compile_and_exec($source, % opts);
    };
    if ($@) {
        print "failed\n";
        if (@errout) {

            print "\n", @errout, "\n";
        }
        error("Failed to determine OpenSSL constants: $@");
    }

    my % const;
            foreach my $line(@$out) {
        my($name, $value) = split( / \s + / o, $line, 2);
                chomp($value);
                $const{$name}
        = $value;
    }

    print "ok\n";

    return \ % const;
}

sub check_cflags_tests() {
    my $gcc = $COMMAND_SPEC{gcc}
    -> {
PATH
    };
    my( % opts) = (CC = > $gcc);
            my(@cflags) = (@$CC_MODE);

            foreach my $test(@CFLAGS_TESTS) {
        my @errout;
                $opts{CC_ERROR}
        = \@errout;
                my($name, $source, $cflags, $ldflags, $libs) =
                ($test-> {
                    NAME
                }, $test-> {
                    CODE
                }, $test-> {
                    CFLAGS
                },
        $test-> {
            LDFLAGS
        }, $test-> {
            LIBS
        });
        my(@cf) = (@cflags);
                push(@cf, @$cflags) if ($cflags);
                $opts{LDFLAGS}
            = $ldflags if ($ldflags);
                    $opts{LIBS}
                = $libs if ($libs);
                        $opts{CC_ONLY}
                    = 1;
                        my($reqflag, $reqopt);

                        checking("whether $name is defined");
                        foreach my $curflag(undef, @{$test-> {
CANDIDATES
                        }}) {
                        @errout = ();
                                my $curopt;
                        if ($curflag) {
                            my($n, $v) = ($curflag-> {
                                NAME
                            },
                            $curflag-> {
                                VALUE
                            });
                            $curopt = "-D$n";
                                    $curopt . = "=$v" if (defined($v));
                                    my(@c) = (@cf, $curopt);
                                    $opts{CFLAGS}
                                = \@c;
                            } else {

                            $opts{CFLAGS}
                            = \@cf;
                        }
                        eval{
                            compile_and_exec($source, % opts);
                        };
                        unless($@) {
                            if ($curopt) {
                                $reqopt = $curopt;
                                        $reqflag = $curflag;
                            } else {
                                $reqopt = '';
                                        $reqflag = {};
                            }
                            last;
                        }
                    }

        if (defined($reqflag)) {
            if ( %{$reqflag}) {
                print "yes ($reqopt)\n";
            } else {
                print "yes\n";
            }
            $REQUIRED_CFLAGS{$name}
            = $reqflag;
        } else {
            print "not defined\n";

                    unless($test-> {
IGNORE_ERROR
                    }) {

                STDERR->print("\nLast compilation error: \n",
                        @errout);
                        error("$name is not defined.");
            }
        }
    }
}

sub check_config_tests(\@$\ %) {
    my($hdefs, $os, $vars) = @_;

            my $gcc = $COMMAND_SPEC{gcc}
    -> {
                PATH
            };
    my( % opts) = (CC = > $gcc);
            my(@cflags) = (@$CC_MODE);
            my $osdefs = $OSDEP{$os};
    if ($osdefs) {
        my $defs = $osdefs-> {
            CC_FEATURE_DEFS
        };
        if ($defs) {
            if (ref($defs) eq 'ARRAY') {
                push(@cflags, @$defs)
            } else {

                push(@cflags, $defs)
            }
        }
    }

    foreach my $test(@CONFIG_TESTS) {
        my($name, $desc, $skip, $required) =
                ($test-> {
                    DEFINE
                }, $test-> {
                    DESC
                }, $test-> {
                    SKIP
                },
        $test-> {
            REQUIRED
        });
        if (ref($skip) eq 'CODE') {
            if (&$skip()) {
                push(@$hdefs, ConfDef->new($name, undef, $desc,
                        $required));
                        next;
            }
        }
        my $setup = $test-> {
            SETUP
        };
        if (ref($setup) eq 'CODE') {
            &$setup($test, $vars);
        }

        my @errout;
                $opts{CC_ERROR}
        = \@errout;
                my $out;
                my($checking, $source, $cflags, $ldflags, $libs) =
                ($test-> {
                    CHECK
                }, $test-> {
                    CODE
                }, $test-> {
                    CFLAGS
                },
        $test-> {
            LDFLAGS
        }, $test-> {
            LIBS
        });
        my(@cf) = (@cflags);
                push(@cf, @$cflags) if ($cflags);
                $opts{CFLAGS}
            = \@cf;
                $opts{LDFLAGS}
            = $ldflags if ($ldflags);
                    $opts{LIBS}
                = $libs if ($libs);

                        checking($checking);
                        eval{
                        $out = compile_and_exec($source, % opts);
                    };
                    if ($@) {
                        if ($test-> {
                                IGNORE_ERROR
                            }) {
                        $out = ["NO:Compilation failed\n"];
                    } else {
                            print "unknown\n";
                            if (@errout) {
                                print "\n", @errout, "\n";
                            }
                            error("Configuration test failed: $@");
                        }
                    }

        my $line = $out->[0];
                chomp($line);
        if ($line = ~m, ^ERROR : (.+)$,) {
            print "failed\n";

                    error($1);
        }

        my $value;
        if ($line = ~m, ^NO( : (.+)) ? $, o) {
            my $msg = ($2) ? " ($2)" : '';

                    print "no$msg\n";
                    my $mandatory = $test-> {
                        MANDATORY
                    };
            error($mandatory)

                              if ($mandatory);
            }
        elsif($line = ~m, ^VALUE : (.+)$, o) {

            $value = $1;
                    print $value, "\n";
        }
        elsif($line = ~m, ^OK : (.+)$, o) {
            print $1, "\n";
                    $value = 1;
        } else {
            error("Unexpected output: $line");
        }
        push(@$hdefs, ConfDef->new($name, $value, $desc, $required))
        if ($name);
                my $callback = $test-> {
                    CALLBACK
                };
        &$callback($vars, $value)

                                  if ($callback);
        }
}

sub format_boost_version($) {
    my($version) = @_;

            use integer;
            my $patch = $version % 100;
            my $minor = ($version / 100) % 1000;
            my $major = $version / 100000;

    return "$major.$minor.$patch";
}

sub check_java(\ % $) {

    my($vars, $conf) = @_;

            checking('whether to build Java bindings');
            unless($conf->get('java')) {
        print "no\n";
        return undef;
    }
    print "yes\n";

            checking('for required Java version');
            my($major, $minor) = eval{
        return get_required_jdk_version($conf);
    };
    if ($@) {

        my $err = "$@";
                chomp($err);
                print "error\n";
                error($err);
    }
    print "${major}.${minor}\n";

            my $jobjdir = $conf->get('java-objdir');
            unless(defined($jobjdir)) {
        $jobjdir = $conf->get('objdir') . '/jobjs';
    }

    checking('for JDK home directory');
            my( % args) = (major = > $major, minor = > $minor);
            my $arch = $vars-> {
                CONFIG_ARCH
            };
    my $jmode = JAVA_ARCHFLAG-> {
        $arch
    };
    $args{mode}
    = $jmode

             if ($jmode);
            my($jhome, $jver) = get_java_home($conf, % args);
            unless($jhome) {
            print "not found\n";
                    error("JDK is not found. Use --with-java-home.");
        }
    print "$jhome ($jver)\n";

            checking('whether to compile Java source with debugging option');
            my $debug = $conf->get('java-debug');
            my $sdebug = ($debug) ? "yes" : "no";
            print $sdebug, "\n";

            checking('for encoding of Java source');
            my $enc = $conf->get('java-encoding');
            print $enc, "\n";

            my( % jenv) = (JAVA_OBJDIR = > $jobjdir, JAVA_HOME = > $jhome,
            JAVA_DEBUG_DEF = > $debug, JAVA_ENCODING_DEF = > $enc,
            INST_JARDIR = > '$(INST_JAVADIR)/jar');
            $jenv{JAVA_MODE}
    = $jmode

             if ($jmode);

            checking('for JNI header file path');
            my $inc = $jhome . '/include';
            my $jni_h = $inc . '/jni.h';
            unless(-f $jni_h) {
            print "no\n";
                    error("jni.h is not found under $jhome.");
        }

    my $os = $vars-> {
OSTYPE
    };
    my $mdinc = $inc . '/' . $os;
            my $jni_md_h = $mdinc . '/jni_md.h';
            unless(-f $jni_md_h) {
        print "no\n";
                error("jni_md.h is not found under $jhome.");
    }

    my $incdir = "$mdinc $inc";
            print "$incdir\n";
            $jenv{JNI_INCDIR}
    = $incdir;

#Set JAVA_HOME environment variable for succeeding tests.
            $ENV{JAVA_HOME}
    = $jhome;

            check_junit( % jenv, $conf);
            check_ant( % jenv, $conf);

    return \ % jenv;
}

sub scan_jdk($$) {

    my($conf, $arch) = @_;

            my($major, $minor) = eval{
        return get_required_jdk_version($conf);
    };

    $major = JDK_VERSION_MAJOR unless(defined($major));
            $minor = JDK_VERSION_MINOR unless(defined($minor));
            my( % args) = (major = > $major, minor = > $minor);

            unless($arch) {
        my $archname = $Config{myarchname};
        $arch = (split( / - /, $archname, 2))[0] if ($archname);
        }

    my $jmode = JAVA_ARCHFLAG-> {
        $arch
    };
    $args{mode}
    = $jmode if ($jmode);

            my($jhome, $jver) = get_java_home($conf, % args);

        return $jhome;
    }

sub get_required_jdk_version($) {
    my($conf) = @_;

            my($major, $minor);
            my $ver = $conf->get('java-version');
    if (defined($ver)) {
        die "Unexpected Java version format: $ver\n"
                unless($ver = ~ / ^\s * (\d +)\.(\d +)(\.\d +) ? \s * $ /);

                $major = $1;
                $minor = $2;
    } else {

        $major = JDK_VERSION_MAJOR;
                $minor = JDK_VERSION_MINOR;
    }

    return ($major, $minor);
}

sub get_java_home($ %) {
    my($conf, % args) = @_;

#Try the specified JAVA_HOME first.
            my $jhome = $conf->get('java-home');
            my $ver;
    if ($jhome) {
        $ver = check_java_home($jhome, % args);
        return ($jhome, $ver) if ($ver);
        }

#Try OpenJDK at the standard location.
    $jhome = '/usr/lib/jvm/java-openjdk';
            $ver = check_java_home($jhome, % args);
    return ($jhome, $ver)

                          if ($ver);

#Try possible directories.
            my $dirpat = qr, ^(jdk | java),;
            foreach my $dir(qw( / usr / lib / jvm / usr / java / usr / usr / local / java
            / usr / local)) {
            ($jhome, $ver) = search_java_home($dir, $dirpat, % args);
            return ($jhome, $ver)

                                  if ($jhome);
            }

#Try to derive JDK home from javac command.
    my $javac_pat = qr, / bin / javac$,;
            foreach my $dir(@PATH) {
        my $javac = "$dir/javac";
                next unless(-x $javac);
                my $path = abs_path($javac);
                next unless($path);
        if ($path = ~s, $javac_pat,,) {
            $ver = check_java_home($path, % args);
            return ($path, $ver)

                                 if ($ver);
            }
    }

    return undef;
}

sub search_java_home($$ %) {
    my($basedir, $dirpat, % args) = @_;

            my $dirp = DirHandle->new($basedir);
    return undef unless($dirp);

    while (my $dp = $dirp->read()) {
        next unless($dp = ~$dirpat);
                my $jhome = "$basedir/$dp";
                my $javac = "$jhome/bin/javac";
                next unless(-x $javac);

                my $ver = check_java_home($jhome, % args);
        return ($jhome, $ver)

                              if ($ver);
        }

    return undef;
}

sub check_java_home($ %) {
    my($jhome, % args) = @_;

            my( % option) = (major = > $args{major}, minor = > $args{minor});
    return undef unless(check_java_version($jhome, 'javac', % option));
    return undef unless(check_java_version($jhome, 'javah', % option));

            my $mode = $args{mode};
    $option{option}
    = [$args{mode}
    ] if ($mode);
            my $ver = check_java_version($jhome, 'java', % option);
        return undef unless($ver);

#Check jar command.
            my $jar = $jhome . '/bin/jar';

        return undef unless(-x $jar);

        return $ver;
    }

sub check_java_version($$ %) {
    my($jhome, $cmd, % args) = @_;

            my $path = $jhome . '/bin/' . $cmd;
            my($ver, $major, $minor, @output);
            my($filter) = sub{
        my($rh) = @_;

        my $pat = qr, ^($cmd | openjdk).*\x22 ? ((\d +)\.(\d +)\.[\d_]+)
        (-.+) ? \x22 ? $, x;
        while (my $line = $rh->getline()) {
            if ($line = ~$pat) {
                $ver = $2;
                        $major = $3;
                        $minor = $4;
                        last;
            }

            push(@output, $line);
        }

        while (my $line = $rh->getline()) {
        }
    };

    my( % opts) = (FILTER = > $filter, ERR2OUT = > 1);
            my $cmdopts = $args{option};
    my(@commands) = ($path, '-version');
            push(@commands, @$cmdopts) if ($cmdopts);
            my $ret = exec_command( % opts, @commands);
        return undef unless($ret == 0);

            my $req_major = $args{major};
        my $req_minor = $args{minor};

        return undef if ($major < $req_major or
                ($major == $req_major and $minor < $req_minor));

            return $ver;
        }

sub check_jarfile($$$$) {
    my($msg, $jenv, $path, $class) = @_;

            checking($msg);
            my $found;
            my $filter = sub{
        my($rh) = @_;

        while (my $line = $rh->getline()) {
            chomp($line);
            if ($class eq $line) {
                $found = 1;
                        last;
            }
        }

        while ($rh->getline()) {
        }
    };

    my $errfile = 'conferr.out';
            my $errout = TempFile->new($errfile, O_CREAT | O_TRUNC | O_RDWR, 0644) or
            fatal("open($errfile) failed: $!");
            my $jar = $jenv-> {
JAVA_HOME
            }
            . '/bin/jar';
    my( % opts) = (FILTER = > $filter, ERR_REDIRECT = > $errout);
            my $ret = exec_command( % opts, $jar, 'tf', $path);
            unless($ret == 0) {
        print "unknown\n\n";

                $errout->seek(0, SEEK_SET);
        while (my $line = $errout->getline()) {
            print 'STDERR: ', $line;
        }

        error('Failed to test JAR file.');
    }

    my $res = ($found) ? 'yes' : 'no';
            print $res, "\n";

    return $found;
}

sub check_junit(\ % $) {

    my($jenv, $conf) = @_;

            checking('for JUnit JAR file');
            my $junit = $conf->get('junit');
            unless($junit) {
        my $filepat = qr, ^junit.*\.jar$,;
                my $wanted = sub{
            my $fname = basename($_);
            return unless($fname = ~$filepat);

            my $st = stat($_);
            return unless($st and S_ISREG($st->mode()));
            $junit = $File::Find::name;
            die "JAR file found\n";
        };

        eval{
            find(
            {wanted = > $wanted, no_chdir = > 1},
            @JAVA_LIBRARY_PATH);
        };
        if ($@) {

            my $err = "$@";
                    chomp($err);
                    die "$err\n" unless($err eq 'JAR file found');
        }
    }

    unless($junit) {
        print "no\n";

        return;
    }

    print $junit, "\n";

            my $swing = check_jarfile('whether JUnit provides swing UI', $jenv,
            $junit, 'junit/swingui/TestRunner.class');
            $jenv-> {
                JUNIT_SWINGUI
            }
            = 'junit.swingui.TestRunner' if ($swing);
            $jenv-> {
JAVA_JUNIT
            }
            = $junit;
}

sub check_junit_jar($) {

    my($junit) = @_;

            my $fh = FileHandle->new($junit);
            die "Unable to open $junit: $!\n" unless($fh);

            my $st = stat($fh);
            die "stat($junit) failed: $!\n" unless($st);
            die "$junit is not a regular file.\n" unless(S_ISREG($st->mode()));
}

sub check_ant(\ % $) {

    my($jenv, $conf) = @_;

            delete $ENV{ANT_HOME};

    checking('for Ant home directory');
            my $ahome = $conf->get('ant-home');
            my $antjar = $ahome . '/lib/ant.jar';
            unless(-f $antjar and -r _) {

        print "no\n";
                error("Ant library is not found. Use --with-ant-home.");
    }
    print "$ahome\n";
            $ENV{ANT_HOME}
    = $ahome;

            my $ant = $conf->get('ant');
            unless($ant) {
        my $spec = {};
        eval{
            verify_command('ant', % $spec);
        };
        if ($@) {
            my $err = "$@";
                    chomp($err);
                    print "error\n";
                    error($err);
        }
        $ant = $spec-> {
            PATH
        };
    }

    checking('for Apache Ant version');
            my($major, $minor, $rev);
            my $filter = sub{
        my($rh) = @_;

        my $pat = qr, Apache Ant.* version\s + (\d +)\.(\d +)(\.(\d +)) ? \s +,;
        while (my $line = $rh->getline()) {
            if ($line = ~$pat) {
                $major = $1;
                        $minor = $2;
                        $rev = $4 || 0;
                while ($line = $rh->getline()) {
                }

                last;
            }
        }
    };

    my $errfile = 'conferr.out';
            my $errout = TempFile->new($errfile, O_CREAT | O_TRUNC | O_RDWR, 0644) or
            fatal("open($errfile) failed: $!");
            my( % opts) = (FILTER = > $filter, ERR_REDIRECT = > $errout);
            my $ret = exec_command( % opts, $ant, '-version');
            unless($ret == 0) {
        print "unknown\n\n";

                $errout->seek(0, SEEK_SET);
        while (my $line = $errout->getline()) {

            print "STDERR: ", $line;
        }

        error("$ant didn't work.");
    }

    unless(defined($major) and defined($minor) and defined($rev)) {
        print "unknown\n";
                error("Unable to determine $ant version.");
    }

    print $major, '.', $minor, '.', $rev, "\n";
    if ($major < ANT_VERSION_MAJOR or
            ($major == ANT_VERSION_MAJOR and
            ($minor < ANT_VERSION_MINOR or
            ($minor == ANT_VERSION_MINOR and $rev < ANT_VERSION_REVISION)))) {
        error("Apache Ant ", ANT_VERSION_MAJOR, '.', ANT_VERSION_MINOR,
                '.', ANT_VERSION_REVISION, " or later is required.");
    }

    if ($jenv-> {
            JAVA_JUNIT
        }) {
    checking('whether Ant supports <junit> task');
            my $junit = $ahome . '/lib/ant-junit.jar';
    if (-f $junit) {
        print "yes\n";
                $jenv-> {
                    ANT_JUNIT
                }
                = 1;
    } else {
        print "no\n";
    }
}

    $jenv-> {
        ANT
    }
    = $ant;
            $jenv-> {
ANT_HOME
            }
            = $ahome;
}

sub check_file($\@ %) {

    my($file, $prefix, % args) = @_;

            checking("for $file");
            my $dirlist = $args{dirlist}
    || ['.'];

    foreach my $pfx(@$prefix) {

        foreach my $dir(@$dirlist) {
            my $parent = "$pfx/$dir";
                    my $path = "$parent/$file";
            if (-e $path) {
                $parent = canonicalize_path($parent);
                        print "$parent/$file\n";

                return $parent unless($args{need_prefix});
                return ($parent, $pfx);
            }
        }
    }

    print "not found\n";

    return undef;
}

sub check_header($\@ %) {
    my($file, $prefix, % args) = @_;

            $args{dirlist}
    = ['include'];

    return check_file($file, @$prefix, % args);
}

sub check_library($\ %) {
    my($libname, $args) = @_;

            checking("for lib$libname library");

            my $code = $args-> {
CODE
            };
    unless($code) {
        $args-> {
CODE
        }
        = << OUT;
                int
                main() {
            return 0;
        }
        OUT
    }

#Set up compiler options.
    my $cc = $args-> {
        CC
    };
    $args-> {
        CC
    }
    = $COMMAND_SPEC{gcc}
    -> {
        PATH
    }
    unless($cc);
            my $cflags = $args-> {
                CFLAGS
            };
    $args-> {
        CFLAGS
    }
    = $CC_MODE unless($cflags);
            $args-> {
                CC_ONLY
            }
            = 1;

            my $libdirs = $args-> {
LIBDIRS
            };
    unless($libdirs) {
        my $prefix = $args-> {
PREFIX
        }
        || [];
        my(@dirs);
                foreach my $pfx(@$prefix, '/') {

            foreach my $dir(@LIBDIR_NAMES) {
                my $path = canonicalize_path("$pfx/$dir");
                        push(@dirs, $path)

                                           if (-d $path);
                }
        }
        $libdirs = \@dirs;
    }

    foreach my $path(@$libdirs) {
        my $libname = check_library_impl($libname, $path, % $args);
        if ($libname) {
            print "$path\n";
            return ($libname, $path);
        }
    }

    print "not found\n";

    return undef;
}

sub check_library_impl($$\ %) {
    my($libname, $path, $args) = @_;

            my( % opts) = ( % $args);
            my(@ldflags) = ('-L' . $path, '-Wl,-rpath,' . $path);
            $opts{LDFLAGS}
    = \@ldflags;
            my $code = $args-> {
                CODE
            };

    $opts{LIBS}
    = '-l' . $libname;

            eval{
        compile_and_exec($code, % opts);
    };

    return $libname unless($@);

    return undef;
}

sub is_default_header_path($) {
    my($path) = @_;

    return ($path eq DEFAULT_INCDIR);
}

sub is_default_library_path($) {

    my($path) = @_;

            foreach my $dir(@DEFAULT_LIBPATH) {
        return 1

                 if ($path eq $dir);
        }

    return undef;
}

sub check_gcc_arch($@) {
    my($gcc, @langopts) = @_;

            checking("for $gcc target architecture");

            my $pattern = qr, ^#define\s+(\S+)\s+(\S+)\s*$,;
            my($os, $arch);
            my $found = 0;
            my $filter = sub{
        my($rh) = @_;

        while (<$rh>) {
            next

                 if ($found >= 2);
                    next unless( / $pattern /);
                    my($key, $value) = ($1, $2);
                    unless(defined($os)) {
                    $os = $GCC_OSTYPE{$key};
                    $found++

                             if ($os);
                    }
            unless(defined($arch)) {
                $arch = $GCC_ARCH{$key};
                $found++

                         if ($arch);
                }
        }
    };

    my $errfile = 'conferr.out';
            my $errout = TempFile->new($errfile, O_CREAT | O_TRUNC | O_RDWR, 0644) or
            fatal("open($errfile) failed: $!");
            my( % opts) = (FILTER = > $filter, ERR_REDIRECT = > $errout);
            my $ret = exec_command( % opts, $gcc, '-E', '-dM', @langopts, DEV_NULL);
            unless($ret == 0) {
        print "unknown\n\n";

                $errout->seek(0, SEEK_SET);
        while (my $line = $errout->getline()) {

            print "STDERR: ", $line;
        }

        error("$gcc didn't work.");
    }

    unless($os) {

        print "unknown\n";
                error("Failed to detect target OS for $gcc.");
    }
    unless($arch) {
        print "unknown\n";
                error("Failed to detect target architecture for $gcc.");
    }

    print "$os $arch\n";

    return ($os, $arch);
}

sub check_gcc_multiarch($) {
    my($gcc) = @_;

            checking("for multiarch library directory");

            my $multiarch;
            my $filter = sub{
        my($rh) = @_;

        while (<$rh>) {

            unless($multiarch) {
                chomp;
                        $multiarch = $_;
            }
        }
    };

    my( % opts) = (FILTER = > $filter, ERR2OUT = > 1);
            my $ret = exec_command( % opts, $gcc, @$CC_MODE, '-print-multiarch');
    if ($ret == 0 and $multiarch) {
        print $multiarch, "\n";
    } else {

        undef $multiarch;
                print "none\n";
    }

    return $multiarch;
}

sub check_gcc_option($$$$$) {
    my($gcc, $option, $mode, $mandatory, $warn) = @_;

            my($optstr, $optlist);
    if (ref($option) eq 'ARRAY') {
        $optstr = join(' ', @$option);
                $optlist = $option;
    } else {

        $optstr = $option;
                $optlist = [$option];
    }

    checking("whether $gcc supports $optstr");

            my $source = << OUT;
#include <stdio.h>

            int
            main(int argc, char **argv) {
        return 0;
    }
    OUT

    my(@wopts) = (@$warn);
            push(@wopts, @$optlist);
    if ($mode) {
        if (ref($mode) eq 'ARRAY') {
            push(@wopts, @$mode);
        } else {
            push(@wopts, $mode);
        }
    }

    my( % opts) = (CC = > $gcc, CFLAGS = > \@wopts);
    if ($mandatory) {
        $opts{CC_ERROR}
        = [];
    }
    eval{
        compile_and_exec($source, % opts);
    };
    if ($@) {
        print "no\n";

                my $out = $opts{CC_ERROR};
        error("$gcc doesn't support $optstr.\n\n", @$out)
        if ($mandatory);

            return undef;
        }

    print "yes\n";

    return 1;
}

sub check_strict_aliasing($$$\@) {
    my($conf, $gcc, $opt, $warn) = @_;

            checking("whether to allow $gcc to assume strict aliasing rules");
    if ($conf->get('strict-aliasing')) {
        print "yes\n";
    } else {

        print "no\n";
                push(@$warn, $opt);
    }
}

sub check_gcc_ld($) {
    my($gcc) = @_;

            checking("whether $gcc invokes GNU ld");

            my $found;
            my(@output);
            my $errfile = 'conferr.out';
            my $errout = TempFile->new($errfile, O_CREAT | O_TRUNC | O_RDWR, 0644) or
            fatal("open($errfile) failed: $!");
            my $filter = sub{
        my($rh) = @_;

        while (<$rh>) {

            push(@output, $_);
                    unless($found) {
                $found = 1

                           if ( / ^GNU ld /);
                }
        }
    };
    my( % opts) = (FILTER = > $filter, ERR_REDIRECT = > $errout);
            my $ret = exec_command( % opts, $gcc, '-Wl,--version');
            unless($ret == 0 and $found) {

        print "no\n\n";

                foreach my $line(@output) {
            print "STDOUT: ", $line;
        }

        $errout->seek(0, SEEK_SET);
        while (my $line = $errout->getline()) {

            print "STDERR: ", $line;
        }

        error("GNU ld is required.");
    }

    print "yes\n";
}

sub check_os_arch($$$$) {
    my($os, $arch, $gcc, $gxx) = @_;

            my(@cc_opts) = qw(-xc);
            my(@cxx_opts) = qw(-xc++);
    if ($arch) {
        $CC_MODE = $ARCH_CC_MODEFLAGS{$arch};
        push(@cc_opts, @$CC_MODE) if ($CC_MODE);

                $CXX_MODE = $ARCH_CXX_MODEFLAGS{$arch};
            push(@cxx_opts, @$CXX_MODE) if ($CXX_MODE);
            }
    my($os_gcc, $arch_gcc) = check_gcc_arch($gcc, @cc_opts);
            my($os_gxx, $arch_gxx) = check_gcc_arch($gxx, @cxx_opts);

            error("Target OS type doesn't match.\n") unless($os_gcc eq $os_gxx);
            error("Target CPU architecture doesn't match.\n")
            unless($arch_gcc eq $arch_gxx);

    if (defined($os)) {
        error("$gcc doesn't support the given OS type: $os")
        unless($os eq $os_gcc);
    } else {
        $os = $os_gcc;
    }

    if (defined($arch)) {
        error("$gcc doesn't support the given CPU architecture: $arch")
        unless($arch eq $arch_gcc);
    } else {

        $arch = $arch_gcc;
                $CC_MODE = $ARCH_CC_MODEFLAGS{$arch}
        || [];
        $CXX_MODE = $ARCH_CXX_MODEFLAGS{$arch}
        || [];
    }

    foreach my $elem([$gcc, $CC_MODE], [$gxx, $CXX_MODE]) {
        my($cc, $mode) = (@$elem);
                check_gcc_option($cc, $mode, undef, 1, []);
    }

    my $multiarch = check_gcc_multiarch($gcc);

#Set up default library search path.
            my $libspec = $DEFAULT_OSARCH_LIBPATH{$os}
    -> {
        $ARCH{$arch}
    };
    if ($multiarch) {
        (
@DEFAULT_LIBPATH) = ('/lib/' . $multiarch,
                '/usr/lib/' . $multiarch);
                (@LIBDIR_NAMES) = qw(lib);
    }
    elsif($libspec) {
        (@DEFAULT_LIBPATH) = @{$libspec-> {
                PATH
            }};
        (@LIBDIR_NAMES) = @{$libspec-> {
                NAME
            }};
    } else {
        (
@LIBDIR_NAMES) = qw(lib);
    }

    my($has_lib, $has_usrlib);
            foreach my $dir(@DEFAULT_LIBPATH) {
        $has_lib = 1 if ($dir eq '/lib');
                $has_usrlib = 1 if ($dir eq '/usr/lib');
            }

    push(@DEFAULT_LIBPATH, '/lib') unless($has_lib);
            push(@DEFAULT_LIBPATH, '/usr/lib') unless($has_usrlib);

    return ($os, $arch, $multiarch);
}

sub check_cc_search_path($) {
    my($conf) = @_;

            checking('for C/C++ header search path prior to default');
            my $list = $conf->get('prep-includes');
    if ($list and @$list) {

        print join(' ', @$list), "\n";
                foreach my $dir(@$list) {
            push(@CC_INCDIRS_FIRST, $dir);
                    $INCDIRS_FIRST_MAP{$dir}
            = 1;
        }
    } else {
        print "none specified\n";
    }

    checking('for C/C++ library search path prior to default');
            $list = $conf->get('prep-libraries');
    if ($list and @$list) {

        print join(' ', @$list), "\n";
                foreach my $dir(@$list) {
            push(@LD_LIBDIRS_FIRST, $dir);
                    $LIBDIRS_FIRST_MAP{$dir}
            = 1;
        }
    } else {
        print "none specified\n";

        return;
    }
}

sub write_makevar($$$) {

    my($fh, $name, $value) = @_;

            my $op = $MAKEVAR_OP{$name}
    || ':=';
    $fh->printf("%s\t\t%s %s\n", $name, $op, $value);
}

sub create_config_mk($\ %) {
    my($conf, $vars) = @_;

            my $fname = CONFIG_MK;
            print "== creating $fname\n";
            my $fh = FileHandle->new($fname, O_CREAT | O_TRUNC | O_WRONLY, 0644) or
            fatal("open($fname) failed: $!");
            push(@UNLINK_ON_ERROR, $fname);

            $fh->print( << OUT);
##
## Auto-generated build configuration.
##

            ifndef CONFIG_MK_INCLUDED

            CONFIG_MK_INCLUDED : = 1

#PFC version.
            PFC_VERSION : = $PFC_VERSION
            PFC_VERSION_MAJOR : = $PFC_VERSION_MAJOR
            PFC_VERSION_MINOR : = $PFC_VERSION_MINOR
            PFC_VERSION_REVISION : = $PFC_VERSION_REVISION
            PFC_VERSION_PATCHLEVEL : = $PFC_VERSION_PATCHLEVEL
            PFC_VERSION_STRING : = $PFC_VERSION_STRING

            OUT

            foreach my $vname(sort(keys( % $vars))){
        my $value = $vars-> {
$vname
        };
        write_makevar($fh, $vname, $value);
    }
    $fh->print("\n");

            foreach my $key(sort(@{$conf->getInstDirKeys()})) {

        my $dir = $conf->get($key);
                my $vname = make_symbol('INST_', $key);
                write_makevar($fh, $vname, $dir);
    }
    $fh->print("\n");

            foreach my $key(sort(keys( % COMMAND_SPEC))) {
        my $spec = $COMMAND_SPEC{$key};
        if ($spec-> {
                EXPORT
            }) {
        my $var = uc($key);
                my $path = $spec-> {
                    PATH
                };
        next unless($path);
                write_makevar($fh, $var, $path);
    }
    }

    my $root = $vars-> {
SRCROOT
    };
    my $defs_mk = $root . '/' . DEFS_MK;

            $fh->print( << OUT);

            include $defs_mk

            endif#!CONFIG_MK_INCLUDED
            OUT
}

sub create_config_pl(\ %) {

    my($vars) = @_;

            my $fname = CONFIG_PL;
            print "== creating $fname\n";
            my $fh = FileHandle->new($fname, O_CREAT | O_TRUNC | O_WRONLY, 0644) or
            fatal("open($fname) failed: $!");
            push(@UNLINK_ON_ERROR, $fname);

            $fh->print( << OUT);
##
## Auto-generated build configuration.
##

            OUT
            foreach my $vname(sort(keys( % $vars))) {
        my $value = $vars-> {
            $vname
        };
        my($type, $def);
        if (ref($value) eq 'ARRAY') {
            $type = '@';
                    $def = '(' .
                    join(', ', map{stringify($_)}
            @$value) .
                    ')';
        } else {

            $type = '$';
                    $def = stringify($value);
        }
        $fh->print( << OUT);
                $type$vname = $def;
                OUT
    }
}

sub create_java_config_mk($\ %) {

    my($path, $jenv) = @_;

            print "== creating $path\n";

            my $fh = FileHandle->new($path, O_CREAT | O_TRUNC | O_WRONLY, 0644) or
            fatal("open($path) failed: $!");

            $fh->print( << OUT);
##
## Build configuration for Java.
## This file must be included after config.mk.
##

            OUT

            foreach my $key(sort(keys( % $jenv))) {
        my $value = $jenv-> {
            $key
        };
        if ($key eq 'ANT') {
            $key = 'ANT_PATH';
        }
        write_makevar($fh, $key, $value);
    }

    my $junit = $jenv-> {
        JAVA_JUNIT
    };
    if ($junit) {
        my $dpath = $jenv-> {
JAVA_OBJDIR
        }
        . '/ext';
        my $jpath = $dpath . '/junit.jar';
                print "== creating $jpath\n";

                unless(-d $dpath) {

            mkpath($dpath, undef, 0755);
        }

        symlink($junit, $jpath) or
                fatal("Failed to symlink to JUnit JAR file: $!");
    }
}

sub create_config_h($$\@) {
    my($conf, $prefix, $hdefs) = @_;

            my $fh = create_dynamic_header($conf, CONFIG_H);
            my $product = stringify($PFC_PRODUCT_NAME);
            my $sprefix = stringify($prefix);
            my $vstr = stringify($PFC_VERSION_STRING);
            $fh->print( << OUT);
            /*
             * PFC build configuration.
             */

#ifndef _PFC_CONFIG_H
#define _PFC_CONFIG_H

            /* Product name. */
#define PFC_PRODUCT_NAME $product

            /* PFC major version. */
#define PFC_VERSION_MAJOR $PFC_VERSION_MAJOR

            /* PFC minor version. */
#define PFC_VERSION_MINOR $PFC_VERSION_MINOR

            /* PFC software revision. */
#define PFC_VERSION_REVISION $PFC_VERSION_REVISION

            /* PFC software patch level. */
#define PFC_VERSION_PATCHLEVEL $PFC_VERSION_PATCHLEVEL

            /* PFC software version string. */
#define PFC_VERSION_STRING $vstr

            OUT

    if (defined($PFC_BUILD_ID)) {
        my $bid = stringify($PFC_BUILD_ID);
                $fh->print( << OUT);
                /* PFC package build identifier. */
#define PFC_BUILD_ID  $bid

                OUT
    }
    if (defined($PFC_SCM_REVISION)) {
        my $scm = stringify($PFC_SCM_REVISION);
                $fh->print( << OUT);
                /* Source revision number derived from SCM. */
#define PFC_SCM_REVISION $scm

                OUT
    }

    my $ssuffix;
            my $suff = $conf->get('build-type-suffix');
    if ($suff) {

        $ssuffix = ' ' . stringify('-' . $suff);
    }

    $fh->print( << OUT);
            /* Version suffix which represents build type. */
#ifdef PFC_VERBOSE_DEBUG
#define PFC_DEBUG_BUILD_TYPE_SUFFIX "-debug"
#else /* !PFC_VERBOSE_DEBUG */
#define PFC_DEBUG_BUILD_TYPE_SUFFIX ""
#endif /* PFC_VERBOSE_DEBUG */

#define PFC_BUILD_TYPE_SUFFIX PFC_DEBUG_BUILD_TYPE_SUFFIX$ssuffix

            /* PFC installation root. */
#define PFC_ROOTDIR $sprefix

            OUT

            foreach my $key(sort(@{$conf->getInstDirKeys()})) {

        my $dir = $conf->get($key);
                my $value = stringify($dir);
                my $name = make_symbol('PFC_', $key);
                my $desc = $conf->getDescription($key);
                $fh->print( << OUT);
                /* $desc */
#define $name $value

                OUT
    }

#Append additional lines.
    foreach my $cdef(@$hdefs) {

        $cdef->print($fh);
    }

    $fh->print( << OUT);
#endif /* _PFC_CONFIG_H */
            OUT
}

sub create_openssl_const_h($\ %) {
    my($conf, $vars) = @_;

    return unless( % $vars);

            my $const = $vars-> {
OPENSSL_CONST
            };
    my $fh = create_dynamic_header($conf, OPENSSL_CONST_H);
            $fh->print( << OUT);
            /*
             * Constants defined by OpenSSL.
             */

#ifndef _PFC_OPENSSL_CONST_H
#define _PFC_OPENSSL_CONST_H

            OUT

            foreach my $name(sort(keys( % $const))) {
        my $value = $const-> {
$name
        };
        $fh->print( << OUT);
#define $name  $value
                OUT
    }

    $fh->print( << OUT);

#endif /* !_PFC_OPENSSL_CONST_H */
            OUT
}

sub create_config_status(\@\ %) {
    my($argv, $vars) = @_;

            my $objroot = $vars-> {
OBJROOT
            };
    my $path = $objroot . '/' . CONFIG_STATUS;
            print "== creating $path\n";

            unless(-d $objroot) {

        mkpath($objroot, undef, 0755);
    }

    my $fh = FileHandle->new($path, O_CREAT | O_TRUNC | O_WRONLY, 0644) or
            fatal("open($path) failed: $!");

            foreach my $arg(@$argv) {

        $fh->print(CSTATUS_ARGV, ':', $arg, "\n");
    }
}

sub create_gtest_config() {
    my $gtspec = $COMMAND_SPEC{gtest_config};
    return unless($gtspec-> {
        STUB
    });

    my $path = $gtspec-> {
        PATH
    };
    print "== creating $path\n";
            my $src = FileHandle->new(STUB_GTEST_CONFIG) or
            fatal("open(", STUB_GTEST_CONFIG, ") failed: $!");
            my $dst = FileHandle->new($path, O_CREAT | O_TRUNC | O_WRONLY, 0755) or
            fatal("open($path) failed: $!");

    while (my $line = $src->getline()) {

        $dst->print($line);
    }
}

sub parse_config_status($) {
    my($file) = @_;

            my $fh = FileHandle->new($file) or fatal("open($file) failed: $!");

            @ARGV = ();
    while (my $line = $fh->getline()) {
        my($key, $value) = split( / : /, $line, 2);
        if ($key eq CSTATUS_ARGV) {

            chomp($value);
                    push(@ARGV, $value);
        }
    }
}

sub prepare(\ % @) {
    my($vars, @newdirs) = @_;

            my $objroot = $vars-> {
                OBJROOT
            };
    if (-d $objroot) {

        print "== cleaning up old object directory.\n";
                rmtree($objroot);
    }

    foreach my $d(@newdirs) {
        if (-d $d) {
            print "== cleaning up $d.\n";
                    rmtree($d);
        }
    }

    if (-f CONFIG_H()) {

        print "== removing config.h generated by old version.\n";
                rmtree(CONFIG_H());
    }
}

sub setup($) {

    my($make) = @_;

            print "== preparing build tools.\n";

#Force to rebuild build tools.
            my % opts;
            my $ret = exec_command( % opts, $make, '-j1', 'tools-rebuild');
            error("Failed to build tools.") unless($ret == 0);
}

sub make_symbol($$) {
    my($prefix, $key) = @_;

            my $sym = $prefix . uc($key);
            $sym = ~s, -, _, go;

    return $sym;
}

sub cpp_feature_key($) {

    return 'PFC_' . make_feature_key($_[0]);
}

sub make_feature_key($) {
    my $key = uc($_[0]) . '_ENABLED';
            $key = ~s, -, _, go;

    return $key;
}

sub feature_setup(\ % \@$) {

    my($vars, $hdefs, $conf) = @_;

            my $unc = $conf->isUNC();
            foreach my $key(FEATURE_KEYS) {
        my $fdesc = FEATURE_DESC-> {
            $key
        }
        || "$key support";
        checking("whether to enable $fdesc");
                my $en;
        if ($conf->get($key)) {
            print "yes\n";
                    $en = 1;
        } else {
            print "no\n";
        }

        my($val);
                my $cppname = cpp_feature_key($key);
                my $desc = $conf->getDescription($key);

        if ($en) {
            my $name = make_feature_key($key);
                    $vars-> {
$name
                    }
                    = 1;
                    $val = 1;
        }

        push(@$hdefs, ConfDef->new($cppname, $val, $desc));
    }
}

sub check_linux($\ %) {
    my($conf, $vars) = @_;

            checking("for Linux distribution");
            my($dist, $abbrev) = check_linux_dist();
            $dist = 'unknown' unless(defined($dist));
            $abbrev = 'unknown' unless(defined($abbrev));

            print $dist, "\n";
            $vars-> {
LINUX_DIST
            }
            = $abbrev;
}

sub check_linux_dist() {

    foreach my $spec(@LINUX_DIST) {
        my($file, $func) = ($spec-> {
            FILE
        }, $spec-> {
            CODE
        });
        my $fh = FileHandle->new($file);
        if ($fh) {
            my($dist, $abbrev) = &$func($fh);
            return ($dist, $abbrev)

            if (defined($dist) and defined($abbrev));
            }
    }

#Try to determine distribution using lsb_release command.
    return check_lsb_release();
}

sub check_lsb_release() {
    my $path = $COMMAND_SPEC{lsb_release}
    -> {
        PATH
    };
    return undef unless($path);

            my($id, $rel, $desc);
            my $filter = sub{
        my($rh) = @_;

        my $sep = qr, \s* : \s*,;
        while (<$rh>) {
            my($key, $value) = split($sep, $_, 2);
                    next unless($value);
            if ($key eq 'Distributor ID') {

                chomp($value);
                        $id = $value unless($value eq 'n/a');
            }
            elsif($key eq 'Release') {

                chomp($value);
                        $rel = $value unless($value eq 'n/a');
            }
            elsif($key eq 'Description') {
                chomp($value);
                        $desc = $value unless($value eq 'n/a');
            }
        }
    };

    my( % opts) = (FILTER = > $filter, ERR2OUT = > 1);
            my $ret = exec_command( % opts, $path, '-a');
    return undef unless($ret == 0);

    if ($id and defined($rel)) {
        my $abbrev;
        if ($id = ~ / ^Fedora / i) {

            $abbrev = 'fc';
        }
        elsif($id = ~ / ^RedHatEnterprise / i) {

            $abbrev = 'el';
        }
        elsif($id = ~ / SUSE LINUX / i) {
            $abbrev = 'SLES';
        } else {
            $abbrev = lc($id);
        }

        my $name = ($desc) ? $desc : "$id $rel";
        if ($rel = ~ / ^(\d +) /) {
            $abbrev . = $1;
        }

        return ($name, $abbrev);
    }

    if ($desc) {
        if ($desc = ~ / ^Fedora release(\d +) /) {
            my $v = $1;
                    my $ver = "Fedora $v";
                    my $abbrev = "fc$v";

            return ($ver, $abbrev);
        }
        if ($desc = ~
                / ^Red Hat Enterprise Linux .* release(\d +)\..*$ /) {
            my $v = $1;
                    my $ver = "Red Hat Enterprise Linux $v";
                    my $abbrev = "el$v";

            return ($ver, $abbrev);
        }
        if ($desc = ~ / ^SUSE Linux Enterprise Server(\d +) /) {
            my $v = $1;
                    my $ver = "SUSE Linux Enterprise Server $v";
                    my $abbrev = "SLES$v";

            return ($ver, $abbrev);
        }
    }

    return undef;
}

sub fedora_version($) {
    my($fh) = @_;

            my $line = $fh->getline();
    if ($line = ~ / ^Fedora release(\d +) /) {
        my $v = $1;
                my $ver = "Fedora $v";
                my $abbrev = "fc$v";

        return ($ver, $abbrev);
    }

    return ('Fedora', 'fc');
}

sub redhat_version($) {
    my($fh) = @_;

            my $line = $fh->getline();
    if ($line = ~ / ^Red Hat Enterprise Linux .* release(\d +)\..*$ /) {
        my $v = $1;
                my $ver = "Red Hat Enterprise Linux $v";
                my $abbrev = "el$v";

        return ($ver, $abbrev);
    }

#Fedora may have /etc/redhat-release.
    $fh->seek(0, SEEK_SET);
            my($ver, $abbrev) = fedora_version($fh);
            ($ver, $abbrev) = ('RHEL', 'el') if ($abbrev eq 'fc');

        return ($ver, $abbrev);
    }

sub suse_version($) {
    my($fh) = @_;

            my($sles, $v);
    while (my $line = $fh->getline()) {
        if (!$sles and $line = ~ / ^SUSE Linux Enterprise Server /) {

            $sles = 1;
        }
        elsif(!defined($v) and $line = ~ / ^VERSION\s *= \s * (\d +)\s * $ /) {
            $v = $1;
        }

        if ($sles and defined($v)) {
            my $ver = "SUSE Linux Enterprise Server $v";
                    my $abbrev = "SLES$v";

            return ($ver, $abbrev);
        }
    }

    return ('SUSE Linux', 'SUSE');
}

sub os_release_version($) {
    my($fh) = @_;

            my $eqpat = qr, =,;
            my($name, $version, $id);
            my $eval_str = sub{
        my $str = $_[0];
        my $s = eval "$str";
        return (defined($s)) ? $s : $str;
    };

    while (my $line = $fh->getline()) {
        my($key, $value) = split($eqpat, $line, 2);
                next unless(defined($key) and defined($value));
        if ($key eq 'NAME') {

            chomp($value);
                    $name = &$eval_str($value);
        }
        elsif($key eq 'VERSION_ID') {

            chomp($value);
                    $version = &$eval_str($value);
        }
        elsif($key eq 'ID') {
            chomp($value);
                    $id = &$eval_str($value);
        }
    }

    if ($name and defined($version) and $id) {
        my $abbrev = $id;
        if ($version = ~ / ^(\d +) /) {
            $abbrev . = $1;
        }

        $name . = " $version";

        return ($name, $abbrev);
    }

    return undef;
}

sub lsb_version($) {
    my($fh) = @_;

            my $eqpat = qr, =,;
            my($id, $rel, $desc);
            my $eval_str = sub{
        my $str = $_[0];
        my $s = eval "$str";
        return (defined($s)) ? $s : $str;
    };

    while (my $line = $fh->getline()) {
        my($key, $value) = split($eqpat, $line, 2);
                next unless(defined($key) and defined($value));
        if ($key eq 'DISTRIB_ID') {

            chomp($value);
                    $id = &$eval_str($value);
        }
        elsif($key eq 'DISTRIB_RELEASE') {

            chomp($value);
                    $rel = &$eval_str($value);
        }
        elsif($key eq 'DISTRIB_DESCRIPTION') {
            chomp($value);
                    $desc = &$eval_str($value);
        }
    }

    if ($id and defined($rel)) {
        my $abbrev = lc($id);
                my $name = ($desc) ? $desc : "$id $rel";
        if ($rel = ~ / ^(\d +) /) {

            $abbrev . = $1;
        }

        return ($name, $abbrev);
    }

    return undef;
}

sub read_command_output(\ % @) {
    my($opts, @args) = @_;

            my @out;
            my $filter = sub{
        my($rh) = @_;

        while (<$rh>) {

            push(@out, $_);
        }
    };

    my( % copts) = ( % $opts);
            $copts{FILTER}
    = $filter;
            $copts{ERR2OUT}
    = 1;

            my $ret = exec_command( % copts, @args);
            unless($ret == 0) {
        my $cmd = join(' ', @args);
                $opts-> {
                    ERR_OUTPUT
                }
                = \@out

                        if (@out);
                die "\"$cmd\" failed.\n";
        }

    return \@out;
}

sub read_command_oneline(\ % @) {
    my($opts, @args) = @_;

            my $out = read_command_output( % $opts, @args);

            CHECK :{
        last CHECK unless(@$out == 1);
        my $ret = $out->[0] || '';
        chomp($ret);

        my $pat = $opts-> {
            pattern
        };
        last CHECK if ($pat and $ret !~$pat);

            return $ret;
        }

    my $cmd = join(' ', @args);
            $opts-> {
                ERR_OUTPUT
            }
            = $out if (@$out);

            die "\"$cmd\" exited with unexpected output.\n";
    }
