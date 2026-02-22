PROJECT_NAME = ERRO_BELA

# Add u8g2 include directories with -iquote for quoted includes
CPPFLAGS += -iquote $(PROJECT_DIR)/u8g2/csrc
CPPFLAGS += -iquote $(PROJECT_DIR)/u8g2/cppsrc
CPPFLAGS += -iquote $(PROJECT_DIR)/u8g2/common
CPPFLAGS += -I$(PROJECT_DIR)/u8g2/csrc
CPPFLAGS += -I$(PROJECT_DIR)/u8g2/cppsrc
CPPFLAGS += -I$(PROJECT_DIR)/u8g2/common

# Add C flags too
CFLAGS += -I$(PROJECT_DIR)/u8g2/csrc
CFLAGS += -I$(PROJECT_DIR)/u8g2/common

# Add all u8g2 C source files to the project
PROJECT_FILES += $(wildcard u8g2/csrc/*.c)
PROJECT_FILES += $(wildcard u8g2/common/*.c)

include $(BELA_DIR)/Makefile
