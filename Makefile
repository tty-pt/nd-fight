all := libnd-fight

LDLIBS-libnd-fight := -lxylem

# Sibling -I for a dev build. In CI the siblings do not exist and all headers
# come from the installed packages named in .github/workflows/ci.yml.
CFLAGS += -I$(shell cd .. && pwd)/axil-nd/include
CFLAGS += -I$(shell cd .. && pwd)/axil-nd-attr/include
CFLAGS += -I$(shell cd .. && pwd)/axil-nd-level/include
CFLAGS += -I$(shell cd .. && pwd)/axil-nd-mortal/include
CFLAGS += -I$(shell cd .. && pwd)/axil-nd-core/include

FOLDER := nd

-include ./../mk/include.mk
