obj-$(CONFIG_HMA_UIDFAKE_BUILTIN) += hma_uidfake.o
hma_uidfake-y := core.o policy.o netlink.o
# Older Kbuild uses a relative src; newer Kbuild supplies an absolute path.
ccflags-y += -I$(if $(filter /%,$(src)),$(src),$(srctree)/$(src))/include
