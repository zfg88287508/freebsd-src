#
# $FreeBSD$
#
# The user-driven targets are:
#
# universe            - *Really* build *everything* (buildworld and
#                       all kernels on all architectures).
# buildworld          - Rebuild *everything*, including glue to help do
#                       upgrades.
# installworld        - Install everything built by "buildworld".
# world               - buildworld + installworld.
# buildkernel         - Rebuild the kernel and the kernel-modules.
# installkernel       - Install the kernel and the kernel-modules.
# reinstallkernel     - Reinstall the kernel and the kernel-modules.
# kernel              - buildkernel + installkernel.
# update              - Convenient way to update your source tree (cvs).
# upgrade             - Upgrade a.out (2.2.x/3.0) system to the new ELF way
# most                - Build user commands, no libraries or include files.
# installmost         - Install user commands, no libraries or include files.
# aout-to-elf         - Upgrade a system from a.out to elf format (see below).
# aout-to-elf-build   - Build everything required to upgrade a system from
#                       a.out to elf format (see below).
# aout-to-elf-install - Install everything built by aout-to-elf-build (see
#                       below).
# move-aout-libs      - Move the a.out libraries into an aout sub-directory
#                       of each elf library sub-directory.
#
# This makefile is simple by design. The FreeBSD make automatically reads
# the /usr/share/mk/sys.mk unless the -m argument is specified on the
# command line. By keeping this makefile simple, it doesn't matter too
# much how different the installed mk files are from those in the source
# tree. This makefile executes a child make process, forcing it to use
# the mk files from the source tree which are supposed to DTRT.
#
# The user-driven targets (as listed above) are implemented in Makefile.inc1.
#
# If you want to build your system from source be sure that /usr/obj has
# at least 400MB of diskspace available.
#
# For individuals wanting to build from the sources currently on their
# system, the simple instructions are:
#
# 1.  `cd /usr/src'  (or to the directory containing your source tree).
# 2.  `make world'
#
# For individuals wanting to upgrade their sources (even if only a
# delta of a few days):
#
# 1.  `cd /usr/src'       (or to the directory containing your source tree).
# 2.  `make buildworld'
# 3.  `make buildkernel KERNCONF=YOUR_KERNEL_HERE'     (default is GENERIC).
# 4.  `make installkernel KERNCONF=YOUR_KERNEL_HERE'   (default is GENERIC).
# 5.  `reboot'        (in single user mode: boot -s from the loader prompt).
# 6.  `mergemaster -p'
# 7.  `make installworld'
# 8.  `mergemaster'
# 9.  `reboot'
#
# See src/UPDATING `COMMON ITEMS' for more complete information.
#
# If TARGET_ARCH=arch (e.g. ia64, sparc64, ...) is specified you can
# cross build world for other architectures using the buildworld target,
# and once the world is built you can cross build a kernel using the
# buildkernel target.
#
# ----------------------------------------------------------------------------
#
#           Upgrading an i386 system from a.out to elf format
#
#
# The aout->elf transition build is performed by doing a `make upgrade' (or
# `make aout-to-elf') or in two steps by a `make aout-to-elf-build' followed
# by a `make aout-to-elf-install', depending on user preference.
# You need to have at least 320 MB of free space for the object tree.
#
# The upgrade process checks the installed release. If this is 3.0-CURRENT,
# it is assumed that your kernel contains all the syscalls required by the
# current sources.
#
# The upgrade procedure will stop and ask for confirmation to proceed
# several times. On each occasion, you can type Ctrl-C to abort the
# upgrade.  Optionally, you can also start it with NOCONFIRM=yes and skip
# the confirmation steps.
#
# ----------------------------------------------------------------------------
#
#
# Define the user-driven targets. These are listed here in alphabetical
# order, but that's not important.
#
TGTS=	all all-man buildkernel buildworld checkdpadd clean \
	cleandepend cleandir depend distribute distributeworld everything \
	hierarchy install installcheck installkernel \
	reinstallkernel installmost installworld libraries lint maninstall \
	mk most obj objlink regress rerelease tags update

BITGTS=	files includes
BITGTS:=${BITGTS} ${BITGTS:S/^/build/} ${BITGTS:S/^/install/}

.ORDER: buildworld installworld
.ORDER: buildworld distributeworld
.ORDER: buildkernel installkernel
.ORDER: buildkernel reinstallkernel

PATH=	/sbin:/bin:/usr/sbin:/usr/bin
MAKEOBJDIRPREFIX?=	/usr/obj
MAKEPATH=	${MAKEOBJDIRPREFIX}${.CURDIR}/make.${MACHINE}
_MAKE=	PATH=${PATH} \
	`if [ -x ${MAKEPATH}/make ]; then echo ${MAKEPATH}/make; else echo ${MAKE}; fi` \
	-m ${.CURDIR}/share/mk -f Makefile.inc1

#
# Handle the user-driven targets, using the source relative mk files.
#
${TGTS} ${BITGTS}: upgrade_checks
	@cd ${.CURDIR}; \
		${_MAKE} ${.TARGET}

# Set a reasonable default
.MAIN:	all

STARTTIME!= LC_ALL=C date
#
# world
#
# Attempt to rebuild and reinstall *everything*, with reasonable chance of
# success, regardless of how old your existing system is.
#
world: upgrade_checks
	@echo "--------------------------------------------------------------"
	@echo ">>> elf make world started on ${STARTTIME}"
	@echo "--------------------------------------------------------------"
.if target(pre-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Making 'pre-world' target"
	@echo "--------------------------------------------------------------"
	@cd ${.CURDIR}; ${_MAKE} pre-world
.endif
	@cd ${.CURDIR}; ${_MAKE} buildworld
	@cd ${.CURDIR}; ${_MAKE} -B installworld
.if target(post-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo ">>> Making 'post-world' target"
	@echo "--------------------------------------------------------------"
	@cd ${.CURDIR}; ${_MAKE} post-world
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@printf ">>> elf make world completed on `LC_ALL=C date`\n                       (started ${STARTTIME})\n"
	@echo "--------------------------------------------------------------"

#
# kernel
#
# Short hand for `make buildkernel installkernel'
#
kernel: buildkernel installkernel

#
# Perform a few tests to determine if the installed tools are adequate
# for building the world.
#
upgrade_checks:
	@if ! (cd ${.CURDIR}/tools/regression/usr.bin/make && \
	    PATH=${PATH} ${MAKE} 2>/dev/null); \
	then \
	    (cd ${.CURDIR} && make make); \
	fi

#
# Upgrade make(1) to the current version using the installed
# headers, libraries and tools.
#
MMAKEENV=	MAKEOBJDIRPREFIX=${MAKEPATH} \
		DESTDIR= \
		INSTALL="sh ${.CURDIR}/tools/install.sh"
MMAKE=		${MMAKEENV} make \
		-D_UPGRADING \
		-DNOMAN -DNOSHARED \
		-DNO_CPU_CFLAGS -DNO_WERROR

make:
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Building an up-to-date make(1)"
	@echo "--------------------------------------------------------------"
	@cd ${.CURDIR}/usr.bin/make; \
		${MMAKE} obj && \
		${MMAKE} depend && \
		${MMAKE} all && \
		${MMAKE} install DESTDIR=${MAKEPATH} BINDIR=

#
# Define the upgrade targets. These are listed here in alphabetical
# order, but that's not important.
#
UPGRADE=	aout-to-elf aout-to-elf-build aout-to-elf-install \
		move-aout-libs

#
# Handle the upgrade targets, using the source relative mk files.
#

upgrade:	aout-to-elf

${UPGRADE} : upgrade_checks
	@cd ${.CURDIR}; \
		${_MAKE} -f Makefile.upgrade -m ${.CURDIR}/share/mk ${.TARGET}

#
# universe
#
# Attempt to rebuild *everything* for all supported architectures,
# with reasonable chance of success, regardless of how old your
# existing system is.
#
i386_mach=	pc98
universe:
	@echo "--------------------------------------------------------------"
	@echo ">>> make universe started on ${STARTTIME}"
	@echo "--------------------------------------------------------------"
.for arch in i386 sparc64 alpha ia64
.for mach in ${arch} ${${arch}_mach}
	@echo ">> ${mach} started on `LC_ALL=C date`"
	-cd ${.CURDIR} && ${MAKE} buildworld \
	    TARGET_ARCH=${arch} TARGET=${mach} \
	    __MAKE_CONF=/dev/null \
	    > _.${mach}.buildworld 2>&1
	@echo ">> ${mach} buildworld completed on `LC_ALL=C date`"
.if exists(${.CURDIR}/sys/${mach}/conf/NOTES)
	-cd ${.CURDIR}/sys/${mach}/conf && ${MAKE} LINT \
	    > ${.CURDIR}/_.${mach}.makeLINT 2>&1
.endif
	cd ${.CURDIR} && ${MAKE} buildkernels TARGET_ARCH=${arch} TARGET=${mach}
	@echo ">> ${mach} completed on `LC_ALL=C date`"
.endfor
.endfor
	@echo "--------------------------------------------------------------"
	@printf ">>> make universe completed on `LC_ALL=C date`\n                      (started ${STARTTIME})\n"
	@echo "--------------------------------------------------------------"

KERNCONFS!=	cd ${.CURDIR}/sys/${TARGET}/conf && \
		find [A-Z]*[A-Z] -type f -maxdepth 0 ! -name NOTES

buildkernels:
.for kernel in ${KERNCONFS}
	-cd ${.CURDIR} && ${MAKE} buildkernel \
	    KERNCONF=${kernel} \
	    __MAKE_CONF=/dev/null \
	    > _.${TARGET}.${kernel} 2>&1
.endfor
