obj-$(CONFIG_HMA_UIDFAKE_BUILTIN) += hma_uidfake.o
hma_uidfake-y := core.o policy.o netlink.o
# $(src) is relative in an out-of-tree O= build; anchor to srctree.
ccflags-y += -I$(srctree)/$(src)/include
