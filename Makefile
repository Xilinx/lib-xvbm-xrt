# SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

# Copyright (C) 2019-2021 Xilinx Inc - All rights reserved
# Xilinx Video Buffer Manager (Xvbm)
#
# This file is dual-licensed; you may select either the GNU
# Lesser General Public License version 3 or
# Apache License, Version 2.0.

DEVDIR := Debug
DEBDIR := DEB_Release
RPMDIR := RPM_Release

export LD_LIBRARY_PATH=/opt/xilinx/xrt/lib

dev: | $(DEVDIR)
	cd $(DEVDIR); \
	cmake -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS_DEBUG="-O0 -g" \
          -DCMAKE_INSTALL_PREFIX=$(CURDIR)/Debug ..; \
	cmake --build . -- -j 8; \
	make install; \
	make coverage

DEB: | $(DEBDIR)
	cd $(DEBDIR); \
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/xilinx/xvbm -DXVBM_PC_INSTALL_PATH=/usr/lib/pkgconfig ..; \
	cmake --build . -- -j 8; \
	cpack -G DEB

RPM: | $(RPMDIR)
	cd $(RPMDIR); \
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/xilinx/xvbm -DXVBM_PC_INSTALL_PATH=/usr/lib64/pkgconfig ..; \
	cmake --build . -- -j 8; \
	cpack -G RPM

$(DEVDIR):
	mkdir $(DEVDIR);

$(DEBDIR):
	mkdir $(DEBDIR);

$(RPMDIR):
	mkdir $(RPMDIR);

clean:
	rm -rf Debug DEB_Release RPM_Release
