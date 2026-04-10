# ============================================================================
# IMS Project Makefile
# ============================================================================

# Compiler Settings
CC_QNX = qcc
CC_LINUX = gcc

# Include Paths
INCLUDES = -I./apps \
           -I./common \
           -I./drivers \
           -I./protocol

# Flags
CFLAGS_COMMON = -Wall -Wextra $(INCLUDES)
CFLAGS_QNX    = -V gcc_ntoaarch64le -D_QNX_SOURCE
CFLAGS_LINUX  = -D_GNU_SOURCE

# Libraries
# FIX: QNX provides pthreads natively in libc (no -lpthread needed).
# Linux requires the explicit -lpthread link.
LIBS_COMMON = -lssl -lcrypto
LIBS_QNX    = $(LIBS_COMMON) -lsocket
LIBS_LINUX  = $(LIBS_COMMON) -lpthread

# Source Files
SRC_SERVER_DEPS = common/authorization.c \
                  drivers/sensors.c \
                  drivers/sensor_manager.c \
                  protocol/protocol.c

SRC_SERVER = apps/server.c
SRC_CLIENT = apps/client.c
SRC_TEST   = tests/sensor_test.c

QNX_BENCH_SOURCES = $(wildcard tests/bench_*_qnx.c)
QNX_BENCH_BINS    = $(patsubst tests/%.c,%,$(QNX_BENCH_SOURCES))
QNX_TEST_BINS     = $(TARGET_TEST) $(QNX_BENCH_BINS)

# Binaries
TARGET_SERVER = ims_server
TARGET_CLIENT = ims_client
TARGET_TEST   = sensor_test

# ============================================================================
# Build Targets
# ============================================================================

all: server_qnx client_linux tests_qnx

# 1. QNX Server
server_qnx:
	@echo "[INFO] Building QNX Server..."
	$(CC_QNX) $(CFLAGS_QNX) $(CFLAGS_COMMON) -o $(TARGET_SERVER) \
		$(SRC_SERVER) $(SRC_SERVER_DEPS) $(LIBS_QNX)

# 2. Linux Client
client_linux:
	@echo "[INFO] Building Linux Client..."
	$(CC_LINUX) $(CFLAGS_LINUX) $(CFLAGS_COMMON) -o $(TARGET_CLIENT) \
		$(SRC_CLIENT) $(LIBS_LINUX)

# 3. QNX Sensor Test
sensor_test_qnx:
	@echo "[INFO] Building QNX Sensor Test..."
	$(CC_QNX) $(CFLAGS_QNX) $(CFLAGS_COMMON) -o $(TARGET_TEST) \
		$(SRC_TEST) drivers/sensors.c drivers/sensor_manager.c $(LIBS_QNX)

qnx_benchmarks: $(QNX_BENCH_BINS)
	@echo "[OK] Built QNX benchmark tests."

$(QNX_BENCH_BINS): %: tests/%.c
	@echo "[INFO] Building QNX benchmark $@..."
	$(CC_QNX) $(CFLAGS_QNX) -Wall -Wextra -O2 -o $@ $<

tests_qnx: sensor_test_qnx qnx_benchmarks

# FIX: Removed 'rm -rf certs/' to prevent quick_start.sh from deleting 
# newly generated keys during the build phase.
clean:
	@echo "[INFO] Cleaning up binaries and logs..."
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT) $(QNX_TEST_BINS) *.o 
	rm -f blackbox.log

deploy: server_qnx tests_qnx
	@echo "[INFO] Deploying to QNX..."
	scp $(TARGET_SERVER) $(QNX_TEST_BINS) qnxuser@10.42.0.171:/home/qnxuser/ims/
