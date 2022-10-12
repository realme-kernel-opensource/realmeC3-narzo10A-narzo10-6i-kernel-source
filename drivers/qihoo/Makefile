srcpath=$(kbuild-dir)
dstpath=$(O)/$(obj)
file_is_exit=$(wildcard $(srcpath)/*.c)
func_file=$(srcpath)/../../include/linux/vmalloc.h
func_start=$(shell grep -n __vmalloc_node_range $(func_file) | awk -F ':' '{print $$1}')
func_end=$(shell expr $(func_start) + 3)
sed_str=$(func_start),$(func_end)p
func_str=$(shell sed -n "$(sed_str)" $(func_file))
parentheses=")"
para_num=$(shell echo "$(func_str)" | awk -F $(parentheses) '{print $$1}' | awk -F ',' '{print NF}')

FORCE:
ifeq ($(file_is_exit),)
	mkdir -p $(dstpath);
  ifeq ($(CONFIG_64BIT),y)
     ifeq ($(srcpath)/preqihoo_64.a, $(wildcard $(srcpath)/preqihoo_64.a))
	tar xf $(srcpath)/preqihoo_64.a  -C $(dstpath);
     else
	echo "error, not found 64 bit library";
     endif
  else
     ifeq ($(srcpath)/preqihoo_32.a, $(wildcard $(srcpath)/preqihoo_32.a))
	tar xf $(srcpath)/preqihoo_32.a  -C $(dstpath);
     else
	echo "error, not found 32 bit library";
     endif
  endif
else
obj-$(CONFIG_QIHOO) += qihoo.o
qihoo-objs :=core.o elfload.o split.o patch_entry.o
obj-$(CONFIG_QIHOO) += qh_sign/
EXTRA_CFLAGS += -DDEBUG
ifeq ($(para_num),8)
EXTRA_CFLAGS += -DVMALLOC_NODE_RANGE_PARA_8
endif
endif


