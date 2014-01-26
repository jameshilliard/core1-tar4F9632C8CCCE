
EXTRA_CFLAGS+=-I. -DLINUX -DEXPORT_SYMTAB -fvolatile
# EXTRA_CFLAGS+=-I. -DLINUX -DEXPORT_SYMTAB

ifeq ($(CONFIG_BOARD_TWINPASS_E),y)
export-objs     :=	mps_tpe_linux.o
obj-$(CONFIG_DANUBE_MPS) += mps_tpe_linux.o mps_tpe_device.o mps_tpe_buffer.o

else
export-objs     :=	mps_linux.o
obj-$(CONFIG_DANUBE_MPS) += mps_linux.o mps_device.o 

endif

#export-objs     :=	mps_linux.o
#obj-$(CONFIG_DANUBE_MPS) += mps_linux.o mps_device.o 

#obj-$(CONFIG_DANUBE_MPS) += mps.o

#mps-objs := mps_linux.o mps_device.o

#mps.o: $(mps-objs)
#	$(LD) -r -o $@ $(mps-objs)

#prochao,to separate the output for core0 and core1

ifeq ($(CONFIG_BOARD_TWINPASS_E),y)
ifeq ($(CONFIG_DANUBE_CORE1),y) ## MPS driver for the TP-E core1
O_TARGET:= mps1.o
else    ## MPS driver for the TP-E core0
O_TARGET:= mps0.o
endif
else    ## normal MPS driver as for Danube or TwinPass-VE
O_TARGET:= mps.o
endif

include $(TOPDIR)/Rules.make

